
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "j9port.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include "TgcFreeListSummary.hpp"

#include "GCExtensions.hpp"
#include "HeapStats.hpp"
#include "TgcExtensions.hpp"

#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapMemoryPoolIterator.hpp"
#include "MemoryPool.hpp"

#define STAT_ELEMENTS			22
#define FIRST_ELEMENT_THRESHOLD	1024

/*
 * Free List Summary is organized as a array of counters. Each counter represents number of holes in pool with size in particular range.
 * First element represents number of holes smaller then FIRST_ELEMENT_THRESHOLD bytes (but larger then dark matter threshold - 512 bytes default)
 * Second element represents number of holes smaller then 2 * FIRST_ELEMENT_THRESHOLD but larger then FIRST_ELEMENT_THRESHOLD.
 * Each next element has a doubled threshold, except last one which represents number of holes larger then previous threshold.
 * The table of thresholds for FIRST_ELEMENT_THRESHOLD = 1024 and number of elements = 22 is below: 
 * 
 * 1	<1024
 * 2	<2048
 * 3	<4096
 * 4	<8192
 * 5	<16384
 * 6	<32768
 * 7	<65536
 * 8	<131072
 * 9	<262144
 * 10	<524288
 * 11	<1048576
 * 12	<2097152
 * 13	<4194304
 * 14	<8388608
 * 15	<16777216
 * 16	<33554432
 * 17	<67108864
 * 18	<134217728
 * 19	<268435456
 * 20	<536870912
 * 21	<1073741824
 * 22	>=1073741824
 *
 */
 
 /*
  * Increment the counter responsible for size in range
  * @param size Size of hole we want to add to count
  * @param array Pointer to Free List Summary array
  * @param elements Number of elements in Free List Summary array
  */
static void
addSizeToSummary(UDATA size, UDATA *array, UDATA elements)
{
	UDATA i;
	UDATA n = FIRST_ELEMENT_THRESHOLD;
	for (i = 0; i < elements - 1; i++) {
		if (size < n) {
			array[i] += 1;
			return;
		}
		n = n * 2;
	}
	array[elements - 1] += 1;
}

/*
 * Calculate Free List Summary for Memory Pool. Return size of Largest hole discovered in pool
 * @param env thread information
 * @param pool Memory pool to calculate summary
 * @param statsArray Pointer to Free List Summary array
 * @param elements Number of elements in Free List Summary array
 * @return Size of largest hole discovered in pool
 */
static UDATA
calcSummaryForMemoryPool(MM_EnvironmentBase *env, MM_MemoryPool *pool, UDATA *statsArray, UDATA elements)
{
	UDATA i;
	UDATA largest = 0;

	for (i = 0; i < elements; i++) {
		statsArray[i] = 0;
	}

	MM_HeapLinkedFreeHeader *currentFreeEntry = (MM_HeapLinkedFreeHeader *)pool->getFirstFreeStartingAddr(env);
	while(currentFreeEntry) {
		UDATA size = currentFreeEntry->getSize();
		if (size > largest) {
			largest = size;
		}
		addSizeToSummary(size, statsArray, elements);
		currentFreeEntry = (MM_HeapLinkedFreeHeader *)pool->getNextFreeStartingAddr(env, currentFreeEntry);
	}

	return largest;
}

/*
 * Walk all MPAOLs in heap, calculate Free List Summary and print it.
 * @param env thread information
 * @param verboseEvent Event description iv verbose form
 */
static void
calcAndPrintFreeListSummary(MM_EnvironmentBase *env, const char *verboseEvent)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_MemoryPool *memoryPool = NULL;
	MM_HeapMemoryPoolIterator poolIterator(env, extensions->heap);

	UDATA statsArray[STAT_ELEMENTS];
	UDATA statLargest = 0;
	UDATA i;

	tgcExtensions->printf("\n<free_list_summary reason=\"%s\">\n", verboseEvent);
	while(NULL != (memoryPool = (MM_MemoryPool *)poolIterator.nextPool())) {
		statLargest = calcSummaryForMemoryPool(env, memoryPool, statsArray, STAT_ELEMENTS);
		tgcExtensions->printf("<memory_pool address=\"%p\" name=\"%s\" largest=\"%d\">", memoryPool, memoryPool->getPoolName(), statLargest);
		for (i = 0; i < STAT_ELEMENTS; i++) {
			tgcExtensions->printf(" %d", statsArray[i]);
		}
		tgcExtensions->printf(" </memory_pool>\n");
	}
	tgcExtensions->printf("</free_list_summary>\n");
}

/*
 * Handle for J9HOOK_MM_GLOBAL_GC_START event
 */
static void
tgcHookFreeListSummaryStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent* event = (MM_GlobalGCEndEvent*)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	calcAndPrintFreeListSummary(env, "Global GC Start");
}

/*
 * Handle for J9HOOK_MM_GLOBAL_GC_END event
 */
static void
tgcHookFreeListSummaryEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent* event = (MM_GlobalGCEndEvent*)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	calcAndPrintFreeListSummary(env, "Global GC End");
}

/*
 * Initialization for Free List Summary request
 */
bool
tgcFreeListSummaryInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** hooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookFreeListSummaryStart, OMR_GET_CALLSITE(), javaVM);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_GLOBAL_GC_END, tgcHookFreeListSummaryEnd, OMR_GET_CALLSITE(), javaVM);

	return result;
}
