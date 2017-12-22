/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapStats.hpp"
#include "TgcExtensions.hpp"

/**
 * @todo Provide function documentation
 */
static void
printFreeListStats(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcFreeListExtensions *freeListExtensions = &tgcExtensions->_freeList;
	UDATA freeCount, deferredCount, nonTlhAllocCount;
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	UDATA tlhAllocCount;
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

	MM_AllocationStats *allocStats = &extensions->allocationStats;

	/* Gather statistics on all allocations since last collection from memory pools */
	MM_HeapStats stats;
	MM_Heap *heap = extensions->heap;
	/* Gather statistics on all allocations since last collection from memory pools */
	heap->mergeHeapStats(&stats); /* Assume active */

	freeCount = stats._activeFreeEntryCount;
	deferredCount = stats._inactiveFreeEntryCount;
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	tlhAllocCount = allocStats->_tlhRefreshCountFresh + allocStats->_tlhRefreshCountReused;
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */
	nonTlhAllocCount = allocStats->_allocationCount;

	tgcExtensions->printf("  *%zu* free     %5zu\n", freeListExtensions->gcCount, freeCount);
	tgcExtensions->printf("  *%zu* deferred %5zu\n", freeListExtensions->gcCount, deferredCount);
	tgcExtensions->printf("total            %5zu\n", freeCount + deferredCount);

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	/* Calculate total allocated since last collection */
	UDATA totalAllocatedTLHBytes = allocStats->tlhBytesAllocated();
	UDATA totalAllocatedBytes = totalAllocatedTLHBytes + allocStats->_allocationBytes;
	UDATA tlhPercent = (tlhAllocCount > 0 && totalAllocatedBytes > 0) ?
						(UDATA) (((U_64)totalAllocatedTLHBytes * 100) / (U_64)totalAllocatedBytes) : 0;

	tgcExtensions->printf("<Alloc TLH: count %zu, size %zu, percent %zu, discard %zu >\n",
		tlhAllocCount,
		tlhAllocCount ? (totalAllocatedTLHBytes / tlhAllocCount) : (UDATA)0,
		tlhAllocCount ? tlhPercent : (UDATA)0,
		tlhAllocCount ? allocStats->_tlhDiscardedBytes : (UDATA)0
	);
	tgcExtensions->printf("<  non-TLH: count %zu, search %zu, size %zu, discard %zu>\n",
#else
	tgcExtensions->printf("<non-TLH: count %zu, search %zu, size %zu, discard %zu>\n",
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */
		nonTlhAllocCount,
		nonTlhAllocCount ? (allocStats->_allocationSearchCount / nonTlhAllocCount) : (UDATA)0,
		nonTlhAllocCount ? (allocStats->_allocationBytes / nonTlhAllocCount) : (UDATA)0,
		nonTlhAllocCount ? allocStats->_discardedBytes : (UDATA)0
	);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookGcStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JavaVM* javaVM = (J9JavaVM*)userData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcFreeListExtensions *freeListExtensions = &tgcExtensions->_freeList;

	freeListExtensions->gcCount += 1;
	printFreeListStats(javaVM);
}

/**
 * @todo Provide function documentation
 */
bool
tgcFreeListInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookGcStart, OMR_GET_CALLSITE(), javaVM);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookGcStart, OMR_GET_CALLSITE(), javaVM);

	return result;
}
