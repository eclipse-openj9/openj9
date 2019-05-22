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
#include "j9comp.h"

#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapStats.hpp"
#include "TgcExtensions.hpp"
#include "TgcHeap.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemoryPool.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepHeapSectioning.hpp"
#include "ModronAssertions.h"

#include "SweepStats.hpp"
#include "GlobalGCStats.hpp"


#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "j9port.h"
#include "modronopt.h"
#include "j9comp.h"

#include "FreeHeapRegionList.hpp"
#include "GCExtensions.hpp"
#include "GlobalAllocationManagerSegregated.hpp"
#include "GlobalGCStats.hpp"
#include "Heap.hpp"
#include "HeapRegionQueue.hpp"
#include "LockingHeapRegionQueue.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ParallelSweepChunk.hpp"
#include "RegionPoolSegregated.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SweepHeapSectioning.hpp"
#include "SweepStats.hpp"
#include "TgcExtensions.hpp"
#include "TgcHeap.hpp"

#include "MemoryPool.hpp"

static void
tgcHeapPrintStats(OMR_VMThread* omrVMThread)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(omrVMThread);

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_HeapStats stats;
	MM_Heap *heap = extensions->heap;
	/* Gather statistics on all allocations since last collection from memory pools */
	heap->mergeHeapStats(&stats); /* Assume active */
	
	tgcExtensions->printf("------------- Heap Statistics -------------\n");
	tgcExtensions->printf("Heap Allocated Count:          %12zu\n", stats._allocCount);
	tgcExtensions->printf("Heap Allocated Bytes:          %12zu\n", stats._allocBytes);
	tgcExtensions->printf("Heap Discarded Bytes:          %12zu\n", stats._allocDiscardedBytes);
	tgcExtensions->printf("Heap Search Count:             %12zu\n", stats._allocSearchCount);
	tgcExtensions->printf("Heap Free After Last GC:       %12zu\n", stats._lastFreeBytes);
	tgcExtensions->printf("Freelist Size:                 %12zu\n", stats._activeFreeEntryCount);
	tgcExtensions->printf("Deferred Size:                 %12zu\n", stats._inactiveFreeEntryCount);
}

static void
tgcHeapMicroFragmentPrintStats(OMR_VMThread* omrVMThread)
{
	void *tgcRegionDarkmatter_address_start = NULL;
	void *tgcRegionDarkmatter_address_end = NULL;
	UDATA tgcRegionDarkmatter_bytes = 0;
	UDATA tgcRegionFree_bytes = 0;
	UDATA tgcRegionDarkmatter_count = 0;
	const UDATA tgcRegionMaxcount = 20;

	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(omrVMThread);

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();

	UDATA totalChunkCount = extensions->splitFreeListNumberChunksPrepared;
	UDATA tgcRegionDarkmatter_size = totalChunkCount/tgcRegionMaxcount;	/**> number of chunks */

	if (0 == tgcRegionDarkmatter_size) {
		tgcRegionDarkmatter_size = 1;
	}

	MM_ParallelSweepChunk* sweepChunk = NULL;
	MM_ParallelSweepChunk* lastSweepChunk = NULL;
	MM_SweepHeapSectioningIterator sectioningIterator(extensions->sweepHeapSectioning);
	UDATA regionSize = 0;

	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	char timestamp[32];
	omrstr_ftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", omrtime_current_time_millis());
	tgcExtensions->printf("------- Micro Fragment Statistics timestamp=\"%s\" ---------\n", timestamp);
	tgcExtensions->printf("   address start - address end     ( size)     free KBytes(   Pct) fragment KBytes(   Pct)\n");

	for (uintptr_t chunkNum = 0; chunkNum < totalChunkCount; chunkNum++) {
		sweepChunk = sectioningIterator.nextChunk();
		if (MEMORY_TYPE_OLD == sweepChunk->memoryPool->getSubSpace()->getTypeFlags()) {

			if (0 == sweepChunk->chunkTop) {
				continue;
			}
			tgcRegionDarkmatter_count += 1;
			tgcRegionDarkmatter_bytes += sweepChunk->_darkMatterBytes;
			tgcRegionFree_bytes += sweepChunk->freeBytes;
			lastSweepChunk = sweepChunk;
			if (1 == tgcRegionDarkmatter_count) {
				tgcRegionDarkmatter_address_start = sweepChunk->chunkBase;
			}
			if (tgcRegionDarkmatter_size == tgcRegionDarkmatter_count) {
				tgcRegionDarkmatter_address_end = sweepChunk->chunkTop;
				if (tgcRegionDarkmatter_address_end == tgcRegionDarkmatter_address_start) {
					break;
				}
				regionSize = (UDATA)tgcRegionDarkmatter_address_end-(UDATA)tgcRegionDarkmatter_address_start;

				tgcExtensions->printf("%p - %p(%3zuMB)%14zuKB(%5.2f%%)%14zuKB(%5.2f%%)\n",
						tgcRegionDarkmatter_address_start, tgcRegionDarkmatter_address_end, regionSize>>20,
						tgcRegionFree_bytes >> 10, 100 * (float)tgcRegionFree_bytes/(float)regionSize,
						tgcRegionDarkmatter_bytes >> 10, 100 * (float)tgcRegionDarkmatter_bytes/(float)regionSize);

				tgcRegionDarkmatter_count = 0;
				tgcRegionDarkmatter_bytes = 0;
				tgcRegionFree_bytes = 0;
			}
		}
	}

	if (0 != tgcRegionDarkmatter_count) {
		tgcRegionDarkmatter_address_end = lastSweepChunk->chunkTop;
		regionSize = (UDATA)tgcRegionDarkmatter_address_end-(UDATA)tgcRegionDarkmatter_address_start;

		tgcExtensions->printf("%p - %p(%3zuMB)%14zuKB(%5.2f%%)%14zuKB(%5.2f%%)\n",
				tgcRegionDarkmatter_address_start, tgcRegionDarkmatter_address_end, regionSize>>20,
				tgcRegionFree_bytes >> 10, 100 * (float)tgcRegionFree_bytes/(float)regionSize,
				tgcRegionDarkmatter_bytes >> 10, 100 * (float)tgcRegionDarkmatter_bytes/(float)regionSize);

		tgcRegionDarkmatter_count = 0;
		tgcRegionDarkmatter_bytes = 0;
		tgcRegionFree_bytes = 0;
	}
	UDATA tenureDarkMatterBytes = tenureMemorySubspace->getMemoryPool()->getDarkMatterBytes();
	UDATA tenureFreeBytes = tenureMemorySubspace->getMemoryPool()->getApproximateFreeMemorySize();

	tgcExtensions->printf("Tenure(%zuMB) Free Size:		 %12zu(%zuMB, %5.2f%%), Micro Fragment Size:	%12zu(%zuMB, %5.2f%%)\n",
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD)>>20, tenureFreeBytes, tenureFreeBytes>>20, 100 * (float)tenureFreeBytes / (float)extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
			tenureDarkMatterBytes, tenureDarkMatterBytes>>20, 100 * (float)tenureDarkMatterBytes / (float)extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD));

	MM_SweepStats *sweepStats = &extensions->globalGCStats.sweepStats;

	U_64 duration = omrtime_hires_delta(sweepStats->_startTime, sweepStats->_endTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	tgcExtensions->printf("Sweep Time(ms): total=\"%llu.%03.3llu\", DarkMatter Samples: %zu\n",
			duration / 1000, duration % 1000, tenureMemorySubspace->getMemoryPool()->getDarkMatterSamples());

}

static void
tgcHookHeapGlobalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent* event = (MM_GlobalGCStartEvent*)eventData;
	tgcHeapPrintStats(event->currentThread);
}

static void
tgcHookHeapLocalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCStartEvent* event = (MM_LocalGCStartEvent*)eventData;
	tgcHeapPrintStats(event->currentThread);
}

static void
tgcHookGlobalGcSweepEndPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*)eventData;
	tgcHeapMicroFragmentPrintStats(event->currentThread);
}

#if defined(J9VM_GC_SEGREGATED_HEAP)
static void
tgcShowRegions(OMR_VMThread *omrVMThread, const char *comment)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(omrVMThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	MM_GlobalAllocationManagerSegregated *globalAllocationManager = (MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager;
	MM_RegionPoolSegregated *regionPool = globalAllocationManager->getRegionPool();

	globalAllocationManager->flushCachedFullRegions(env);

	uintptr_t countSmall, countLarge, countArraylet, countFree, countCoalesce, countMultiFree;
	uintptr_t countTotal = 0;
	uintptr_t countFullSmallTotal = 0;
	uintptr_t countAvailableSmallTotal = 0;
	uintptr_t darkMatterBytesTotal = 0;
	uintptr_t allocCacheBytesTotal = 0;

	tgcExtensions->printf(">>> %32s \n", comment);
	tgcExtensions->printf(">>> sizeClass | full | available               | total | dark    | cache\n");
	tgcExtensions->printf(">>> ====================================================================\n");
	uintptr_t regionSize = extensions->heap->getHeapRegionManager()->getRegionSize();

	for (int32_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
		uintptr_t cellSize = env->getExtensions()->defaultSizeClasses->getCellSize(sizeClass);
		countSmall= regionPool->getSmallFullRegions(sizeClass)->getTotalRegions();
		countFullSmallTotal += countSmall;
		tgcExtensions->printf(">>> %2d: %5d | %4d | ", sizeClass, cellSize, countSmall);

		for (int32_t i = 0; i < NUM_DEFRAG_BUCKETS; i++) {
			uintptr_t count = 0;
			for (uintptr_t j = 0; j < regionPool->getSplitAvailableListSplitCount(); j++) {
				count += regionPool->getSmallAvailableRegions(sizeClass, i, j)->getTotalRegions();
			}
			countSmall += count;
			countAvailableSmallTotal += count;
			tgcExtensions->printf(" %4d ", count);
		}
		countTotal += countSmall;
		tgcExtensions->printf("| %5d | ", countSmall);

		uintptr_t darkMatterBytes = regionPool->getDarkMatterCellCount(sizeClass) * cellSize;
		darkMatterBytesTotal += darkMatterBytes;
		tgcExtensions->printf("%6.2f%% | ", (countSmall == 0)? 0: (darkMatterBytes / (countSmall * regionSize)));

		uintptr_t allocCacheSize = 0;
		GC_OMRVMThreadListIterator vmThreadListIterator(env->getOmrVM());
		while(OMR_VMThread* thread = vmThreadListIterator.nextOMRVMThread()) {
			MM_EnvironmentBase *envThread = MM_EnvironmentBase::getEnvironment(thread);
			allocCacheSize += MM_SegregatedAllocationInterface::getObjectAllocationInterface(envThread)->getAllocatableSize(sizeClass);
		}

		allocCacheBytesTotal += allocCacheSize;
		tgcExtensions->printf("%5d\n", allocCacheSize);
	}

	tgcExtensions->printf(">>> region size %d\n", regionSize);

	uintptr_t arrayletLeafSize = extensions->getOmrVM()->_arrayletLeafSize;
	tgcExtensions->printf(">>> arraylet leaf size %d\n", arrayletLeafSize);

	tgcExtensions->printf(">>> small total (full, available) region count %d (%d, %d)\n", countTotal, countFullSmallTotal, countAvailableSmallTotal);

	countTotal += countLarge = regionPool->getLargeFullRegions()->getTotalRegions();
	tgcExtensions->printf(">>> large full region count %d\n", countLarge);

	countTotal += countArraylet = regionPool->getArrayletFullRegions()->getTotalRegions();
	tgcExtensions->printf(">>> arraylet full region count %d\n", countArraylet);

	countTotal += (countArraylet = regionPool->getArrayletAvailableRegions()->getTotalRegions());
	tgcExtensions->printf(">>> arraylet available region count %d\n", countArraylet);

	countTotal += countFree = regionPool->getSingleFreeList()->getTotalRegions();
	tgcExtensions->printf(">>> free region count %d\n", countFree);

	countTotal += countMultiFree = regionPool->getMultiFreeList()->getTotalRegions();
	tgcExtensions->printf(">>> mutliFree region count %d\n", countMultiFree);

	countTotal += countCoalesce = regionPool->getCoalesceFreeList()->getTotalRegions();
	tgcExtensions->printf(">>> coalesce region count %d\n", countCoalesce);

	uintptr_t heapSize = countTotal * env->getExtensions()->getHeap()->getHeapRegionManager()->getRegionSize();

	tgcExtensions->printf(">>> total region count %d\n", countTotal);
	tgcExtensions->printf(">>> dark matter total bytes %d (%2.2f%% of heap)\n", darkMatterBytesTotal, 100.0 * darkMatterBytesTotal / heapSize);
	tgcExtensions->printf(">>> allocation cache total bytes %d (%2.2f%% of heap)\n", allocCacheBytesTotal, 100.0 * allocCacheBytesTotal / heapSize);
	tgcExtensions->printf(">>> -------------------------------------------------------\n");
}

static void
tgcShowHeapStatistics(OMR_VMThread *omrVMThread)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(omrVMThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_MemoryPool *memoryPool = extensions->getHeap()->getDefaultMemorySpace()->getDefaultMemorySubSpace()->getMemoryPool();

	tgcExtensions->printf(">>> minimum free entry size: %d\n", memoryPool->getMinimumFreeEntrySize());
}

static void
tgcHookSegregatedGlobalGcSweepStartPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepStartEvent* event = (MM_SweepStartEvent *) eventData;
	tgcShowRegions(event->currentThread, "Before Sweep");
}

static void
tgcHookSegregatedGlobalGcSweepEndPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent *) eventData;
	tgcShowRegions(event->currentThread, "After Sweep");
	tgcShowHeapStatistics(event->currentThread);
}

static void
tgcHookSegregatedGlobalGcSynchronousGCStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeSynchronousGCStartEvent *event = (MM_MetronomeSynchronousGCStartEvent *) eventData;

	if (OUT_OF_MEMORY_TRIGGER == event->reason) {
		tgcShowRegions(event->currentThread, "Out of Memory");
		tgcShowHeapStatistics(event->currentThread);
	}
}

#endif /*  defined(J9VM_GC_SEGREGATED_HEAP) */


bool
tgcHeapInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	
	bool result = true;

if (extensions->isStandardGC() || extensions->isVLHGC()) {
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookHeapGlobalPrintStats, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookHeapLocalPrintStats, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, tgcHookGlobalGcSweepEndPrintStats, OMR_GET_CALLSITE(), NULL);
} else if (extensions->isSegregatedHeap()) {
#if defined(J9VM_GC_SEGREGATED_HEAP)
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SWEEP_START, tgcHookSegregatedGlobalGcSweepStartPrintStats, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, tgcHookSegregatedGlobalGcSweepEndPrintStats, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, tgcHookSegregatedGlobalGcSynchronousGCStart, OMR_GET_CALLSITE(), NULL);
#endif /* defined(J9VM_GC_SEGREGATED_HEAP) */
	}
	return result;
}
