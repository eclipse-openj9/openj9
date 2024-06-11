/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "modronopt.h"
#include "mmhook.h"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "VMThreadListIterator.hpp"
#include "TLHAllocationInterface.hpp"
#include "TgcExtensions.hpp"
#include "TgcAllocation.hpp"
#include "HeapStats.hpp"

static void
tgcAllocationPrintCumulativeStats(OMR_VMThread *omrVMThread)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(ext);

	MM_AllocationStats *allocStats = &ext->allocationStats;

	tgcExtensions->printf("----- Cumulative Allocation Statistics ----\n");
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	uintptr_t tlhRefreshCountTotal = allocStats->_tlhRefreshCountFresh + allocStats->_tlhRefreshCountReused;
	uintptr_t tlhAllocatedTotal = allocStats->tlhBytesAllocated();
	tgcExtensions->printf("TLH Refresh Count Total:       %12zu\n", tlhRefreshCountTotal);
	tgcExtensions->printf("TLH Refresh Count Fresh:       %12zu\n", allocStats->_tlhRefreshCountFresh);
	tgcExtensions->printf("TLH Refresh Count Reused:      %12zu\n", allocStats->_tlhRefreshCountReused);
	tgcExtensions->printf("TLH Refresh Bytes Total:       %12zu\n", tlhAllocatedTotal);
	tgcExtensions->printf("TLH Refresh Bytes Fresh:       %12zu\n", allocStats->_tlhAllocatedFresh);
	tgcExtensions->printf("TLH Discarded Bytes:           %12zu\n", allocStats->_tlhDiscardedBytes);
	tgcExtensions->printf("TLH Refresh Bytes Reused:      %12zu\n", allocStats->_tlhAllocatedReused);
	tgcExtensions->printf("TLH Requested Bytes:           %12zu\n", allocStats->_tlhRequestedBytes);
	tgcExtensions->printf("TLH Max Abandoned List Length: %12zu\n", allocStats->_tlhMaxAbandonedListSize);
#endif /* defined (J9VM_GC_THREAD_LOCAL_HEAP) */
	tgcExtensions->printf("Normal Allocated Count:        %12zu\n", allocStats->_allocationCount);
	tgcExtensions->printf("Normal Allocated Bytes:        %12zu\n", allocStats->_allocationBytes);
}

static void
tgcAllocationPrintPerThreadStats(OMR_VMThread *omrVMThread)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(ext);

	GC_OMRVMThreadListIterator threadListIterator(omrVMThread->_vm);
	OMR_VMThread *walkOmrVMThread = NULL;

	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	char timestamp[32];
	omrstr_ftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", omrtime_current_time_millis());

	tgcExtensions->printf("----- Per Thread Allocation Statistics %s ----\n", timestamp);

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
#if defined(OMR_GC_NON_ZERO_TLH)
	tgcExtensions->printf("   JVM thread ID |                                                                             name | refresh/remaining cache size | refresh/remaining non-0 cache | obj alloc count/bytes | TLH fresh count/bytes | TLH discards\n");
#else /* defined(OMR_GC_NON_ZERO_TLH) */
	tgcExtensions->printf("   JVM thread ID |                                                                             name | refresh/remaining cache size | obj alloc count/bytes | TLH fresh count/bytes | TLH discards\n");
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
#else /* defined(J9VM_GC_THREAD_LOCAL_HEAP) */
	/* OMR_GC_NON_ZERO_TLH should not be defined, if J9VM_GC_THREAD_LOCAL_HEAP is not defined */
	tgcExtensions->printf("   JVM thread ID |                                                                             name | refresh/remaining cache size | obj alloc count/bytes\n");
#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) */

	while ((walkOmrVMThread = threadListIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkOmrVMThread);
		MM_ObjectAllocationInterface *walkInterface = walkEnv->_objectAllocationInterface;
		MM_AllocationStats *walkStats = walkInterface->getAllocationStats();

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
		/* To reduce output, skip the threads that did not refresh TLH or allocated object directly from heap, since the last GC. */
		if ((0 == walkStats->_tlhRefreshCountFresh) && (0 == walkStats->_allocationCount)) {
			continue;
		}
#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) */

#if defined(OMR_GC_NON_ZERO_TLH)
		tgcExtensions->printf("%8p | %80s | %14zu %13zu | %14zu %14zu | %8zu %12zu",
#else /* defined(OMR_GC_NON_ZERO_TLH) */
		tgcExtensions->printf("%8p | %80s | %14zu %13zu | %8zu %12zu",
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
				walkOmrVMThread->_language_vmthread, getOMRVMThreadName(walkOmrVMThread),
				walkInterface->getRefreshCacheSize(false), walkInterface->getRemainingCacheSize(false),
#if defined(OMR_GC_NON_ZERO_TLH)
				walkInterface->getRefreshCacheSize(true), walkInterface->getRemainingCacheSize(true),
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
				walkStats->_allocationCount,  walkStats->_allocationBytes);
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
		/* If OMR_GC_NON_ZERO_TLH is enabled, the stats are cumulative for both zero and non-zero TLH */
		tgcExtensions->printf(" | %8zu %12zu | %12zu\n", walkStats->_tlhRefreshCountFresh, walkStats->_tlhAllocatedFresh, walkStats->_tlhDiscardedBytes);
#else /* defined(J9VM_GC_THREAD_LOCAL_HEAP) */
		tgcExtensions->printf("\n");
#endif/* defined(J9VM_GC_THREAD_LOCAL_HEAP) */
		releaseOMRVMThreadName(walkOmrVMThread);
	}
}

static void
tgcHookAllocationGlobalPrintStats(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GlobalGCStartEvent *event = (MM_GlobalGCStartEvent *)eventData;
	tgcAllocationPrintCumulativeStats(event->currentThread);
}

static void
tgcHookAllocationLocalPrintStats(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_LocalGCStartEvent *event = (MM_LocalGCStartEvent *)eventData;
	tgcAllocationPrintCumulativeStats(event->currentThread);
}

static void
tgcHookAllocationFlushCachesStats(J9HookInterface** hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_FlushCachesForGCEvent *event = (MM_FlushCachesForGCEvent *)eventData;
	tgcAllocationPrintPerThreadStats(event->currentThread);
}

bool
tgcAllocationInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);

	/* Report per thread allocation stats, before they are cleared by the flush */
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_FLUSH_CACHES_FOR_GC, tgcHookAllocationFlushCachesStats, OMR_GET_CALLSITE(), NULL);

	/* Report cumulative allocation stats, after they are merged by the flush */
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookAllocationGlobalPrintStats, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookAllocationLocalPrintStats, OMR_GET_CALLSITE(), NULL);

	return result;
}
