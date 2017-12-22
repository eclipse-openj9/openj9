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
#include "j9port.h"
#include "modronopt.h"
#include "mmhook.h"

#include "ConcurrentGCStats.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapMemoryPoolIterator.hpp"
#include "HeapMemorySubSpaceIterator.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ScanClassesMode.hpp"
#include "TgcExtensions.hpp"
#include "TgcLargeAllocation.hpp"

static void
tgcFreeMemoryPrintStatsForMemoryPool(OMR_VMThread* omrVMThread, MM_MemoryPool *memoryPool)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();

	if (NULL !=	stats) {

		tgcExtensions->printf("    -------------------------------------\n");
		tgcExtensions->printf("     %llx (%s) pool: \n", memoryPool, memoryPool->getPoolName());
		tgcExtensions->printf("    Index  SizeClass    Count FreqAlloc     MBytes (   Pct) Cumulative (   Pct)\n");

		UDATA totalCount = 0;
		UDATA approxTotalFreeMemory = 0;
		for (IDATA sizeClassIndex = (IDATA)stats->getMaxSizeClasses() - 1; sizeClassIndex >=0 ; sizeClassIndex--) {
			UDATA count = stats->getFreeEntrySizeClassStats()->getCount(sizeClassIndex);
			UDATA frequentAllocCount = stats->getFreeEntrySizeClassStats()->getFrequentAllocCount(sizeClassIndex);
			count += frequentAllocCount;
			if (0 != count) {
				totalCount += count;
				UDATA approxFreeMemory = (count - frequentAllocCount) * stats->getSizeClassSizes(sizeClassIndex);
				
				MM_FreeEntrySizeClassStats::FrequentAllocation *curr = stats->getFreeEntrySizeClassStats()->getFrequentAllocationHead(sizeClassIndex);
				UDATA veryLargeEntrySizeClass = stats->getFreeEntrySizeClassStats()->getVeryLargeEntrySizeClass();
				while (NULL != curr) {
					if (sizeClassIndex >= (IDATA)veryLargeEntrySizeClass) {
						tgcExtensions->printf("    VeryLarge size %8zu count %8zu\n", curr->_size, curr->_count);
					} else {
						tgcExtensions->printf("    Frequent  size %8zu count %8zu\n", curr->_size, curr->_count);
					}
					approxFreeMemory += (curr->_size * curr->_count);
					curr = curr->_nextInSizeClass;
				}
				approxTotalFreeMemory += approxFreeMemory;

				Assert_MM_true(frequentAllocCount <= count);

				tgcExtensions->printf("    %4zu %11zu %8zu %9zu %9zuM (%5.2f%%) %9zuM (%5.2f%%)\n",
						sizeClassIndex, stats->getSizeClassSizes(sizeClassIndex), count, frequentAllocCount,
						approxFreeMemory >> 20, 100 * (float)approxFreeMemory/(float)memoryPool->getActualFreeMemorySize(),
						approxTotalFreeMemory >> 20, 100 * (float)approxTotalFreeMemory/(float)memoryPool->getActualFreeMemorySize());
			}
		}
		tgcExtensions->printf("    Total Count: %zu (%zu). Approx total free memory: %zuMB (%zuMB)\n",
				totalCount, memoryPool->getActualFreeEntryCount(), approxTotalFreeMemory >> 20, memoryPool->getActualFreeMemorySize() >> 20);
		Assert_MM_true(totalCount == memoryPool->getActualFreeEntryCount());
	}
}

static void
tgcTlhAllocPrintStatsForMemoryPool(OMR_VMThread* omrVMThread, MM_MemoryPool *memoryPool)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();

	if (NULL !=	stats) {

		tgcExtensions->printf("    -------------------------------------\n");
		tgcExtensions->printf("    %llx (%s) pool - TLH alloc: \n", memoryPool, memoryPool->getPoolName());
		tgcExtensions->printf("    Index  SizeClass tlhCount  tlhKBytes\n");

		for (IDATA sizeClassIndex = (IDATA)stats->getMaxTLHSizeClasses() - 1; sizeClassIndex >=0 ; sizeClassIndex--) {
			UDATA countTlhAlloc = stats->getTlhAllocSizeClassCount(sizeClassIndex);
			if (0 != countTlhAlloc) {
				UDATA approxTlhAllocMemory = countTlhAlloc * stats->getSizeClassSizes(sizeClassIndex);
				tgcExtensions->printf("    %4zu %11zu %8zu %9zuK\n",
						sizeClassIndex, stats->getSizeClassSizes(sizeClassIndex), countTlhAlloc, approxTlhAllocMemory >> 10);
			}
		}
	}

}

static void
tgcFreeMemoryPrintStatsForMemorySubSpace(OMR_VMThread* omrVMThread, MM_MemorySubSpace *memorySubSpace, bool globalGC)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_LargeObjectAllocateStats *stats = memorySubSpace->getLargeObjectAllocateStats();

	if (NULL !=	stats) {
		MM_MemoryPool *memoryPool = memorySubSpace->getMemoryPool();

		tgcExtensions->printf("-------------------------------------\n");
		tgcExtensions->printf("Index  SizeClass    Count FreqAlloc     MBytes (   Pct) Cumulative (   Pct)\n");

		UDATA totalCount = 0;
		UDATA approxTotalFreeMemory = 0;
		for (IDATA sizeClassIndex = (IDATA)stats->getMaxSizeClasses() - 1; sizeClassIndex >=0 ; sizeClassIndex--) {
			UDATA count = stats->getFreeEntrySizeClassStats()->getCount(sizeClassIndex);
			UDATA frequentAllocCount = stats->getFreeEntrySizeClassStats()->getFrequentAllocCount(sizeClassIndex);
			count += frequentAllocCount;
			if (0 != count) {
				totalCount += count;
				UDATA approxFreeMemory = (count - frequentAllocCount) * stats->getSizeClassSizes(sizeClassIndex);
				
				Assert_MM_true(frequentAllocCount <= count);

				MM_FreeEntrySizeClassStats::FrequentAllocation *curr = stats->getFreeEntrySizeClassStats()->getFrequentAllocationHead(sizeClassIndex);
				UDATA veryLargeEntrySizeClass = stats->getFreeEntrySizeClassStats()->getVeryLargeEntrySizeClass();
				while (NULL != curr) {
					if (sizeClassIndex >= (IDATA)veryLargeEntrySizeClass) {
						tgcExtensions->printf("    VeryLarge size %8zu count %8zu\n", curr->_size, curr->_count);
					} else {
						tgcExtensions->printf("    Frequent  size %8zu count %8zu\n", curr->_size, curr->_count);
					}
					approxFreeMemory += (curr->_size * curr->_count);
					curr = curr->_nextInSizeClass;
				}
				approxTotalFreeMemory += approxFreeMemory;
				
				tgcExtensions->printf("%4zu %11zu %8zu %9zu %9zuM (%5.2f%%) %9zuM (%5.2f%%)\n",
						sizeClassIndex, stats->getSizeClassSizes(sizeClassIndex), count, frequentAllocCount,
						approxFreeMemory >> 20, 100 * (float)approxFreeMemory/(float)memoryPool->getActualFreeMemorySize(),
						approxTotalFreeMemory >> 20, 100 * (float)approxTotalFreeMemory/(float)memoryPool->getActualFreeMemorySize());
			}
		}
		tgcExtensions->printf("Total Count: %zu (%zu). Approx total free memory: %zuMB (%zuMB)\n",
				totalCount, memoryPool->getActualFreeEntryCount(), approxTotalFreeMemory >> 20, memoryPool->getActualFreeMemorySize() >> 20);
		Assert_MM_true(totalCount == memoryPool->getActualFreeEntryCount());

		if ((globalGC && (GLOBALGC_ESTIMATE_FRAGMENTATION == (extensions->estimateFragmentation & GLOBALGC_ESTIMATE_FRAGMENTATION))) ||
			(!globalGC && (LOCALGC_ESTIMATE_FRAGMENTATION == (extensions->estimateFragmentation & LOCALGC_ESTIMATE_FRAGMENTATION)))) {
			tgcExtensions->printf("------------- Fragmented Remainder ------------------------\n");
			tgcExtensions->printf("Index  SizeClass    Count FreqAlloc     MBytes (   Pct) Cumulative (   Pct)\n");

			totalCount = 0;
			approxTotalFreeMemory = 0;
			for (IDATA sizeClassIndex = (IDATA)stats->getMaxSizeClasses() - 1; sizeClassIndex >=0 ; sizeClassIndex--) {
				UDATA count = extensions->freeEntrySizeClassStatsSimulated.getCount(sizeClassIndex);
				UDATA frequentAllocCount = extensions->freeEntrySizeClassStatsSimulated.getFrequentAllocCount(sizeClassIndex);
				count += frequentAllocCount;
				if (0 != count) {
					totalCount += count;
					UDATA approxFreeMemory = (count - frequentAllocCount) * stats->getSizeClassSizes(sizeClassIndex);

					MM_FreeEntrySizeClassStats::FrequentAllocation *curr = extensions->freeEntrySizeClassStatsSimulated.getFrequentAllocationHead(sizeClassIndex);
					UDATA veryLargeEntrySizeClass = stats->getFreeEntrySizeClassStats()->getVeryLargeEntrySizeClass();
					while (NULL != curr) {
						if (sizeClassIndex >= (IDATA)veryLargeEntrySizeClass) {
							tgcExtensions->printf("    VeryLarge size %8zu count %8zu\n", curr->_size, curr->_count);
						} else {
							tgcExtensions->printf("    Frequent  size %8zu count %8zu\n", curr->_size, curr->_count);
						}
						approxFreeMemory += (curr->_size * curr->_count);
						Assert_MM_true(stats->getSizeClassIndex(curr->_size) == (UDATA)sizeClassIndex);
						curr = curr->_nextInSizeClass;
					}
					approxTotalFreeMemory += approxFreeMemory;

					tgcExtensions->printf("%4zu %11zu %8zu %9zu %9zuM (%5.2f%%) %9zuM (%5.2f%%)\n",
						sizeClassIndex, stats->getSizeClassSizes(sizeClassIndex), count, frequentAllocCount,
						approxFreeMemory >> 20, 100 * (float)approxFreeMemory/(float)memoryPool->getActualFreeMemorySize(),
						approxTotalFreeMemory >> 20, 100 * (float)approxTotalFreeMemory/(float)memoryPool->getActualFreeMemorySize());

					Assert_MM_true(frequentAllocCount <= count);
				}
			}
			tgcExtensions->printf("Total Count: %zu (%zu). Approx total free memory: %zuMB (%zuMB)\n",
					totalCount, memoryPool->getActualFreeEntryCount(), approxTotalFreeMemory >> 20, memoryPool->getActualFreeMemorySize() >> 20);
		}
	}
}

static void
tgcLargeAllocationPrintCurrentStatsForMemoryPool(OMR_VMThread* omrVMThread, MM_MemoryPool *memoryPool)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();

	if (NULL !=	stats) {
		tgcExtensions->printf("    --------------------------------------\n");
		tgcExtensions->printf("    %llx (%s) pool - CURRENT:\n", memoryPool, memoryPool->getPoolName());
		tgcExtensions->printf("    Rank |      Size      KBytes  Count | SizeClass      KBytes  Count\n");

		for(U_16 i = 0; i < spaceSavingGetCurSize(stats->getSpaceSavingSizes()) && i < stats->getMaxAllocateSizes(); i++) {
			UDATA size = 0;
			UDATA sizeCount = 0;
			size = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizes(), i + 1);
			if (0 != size) {
				sizeCount = spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizes(), i + 1) / size;
			}

			UDATA sizeClass = 0;
			UDATA sizeClassCount = 0;
			if (i < spaceSavingGetCurSize(stats->getSpaceSavingSizeClasses())) {
				sizeClass = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizeClasses(), i + 1);
				if (0 != sizeClass) {
					sizeClassCount = spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizeClasses(), i + 1) / sizeClass;
				}
			}

			tgcExtensions->printf("    %4zu | %9zu %10zuK %6zu | %9zu %10zuK %6zu\n",
					i, size, sizeCount * size >> 10, sizeCount, sizeClass, sizeClass * sizeClassCount >> 10, sizeClassCount);
		}
	}
}

static void
tgcLargeAllocationPrintAverageStatsForMemoryPool(OMR_VMThread* omrVMThread, MM_MemoryPool *memoryPool)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();

	if (NULL !=	stats) {
		tgcExtensions->printf("    -------------------------------------\n");
		tgcExtensions->printf("    %llx (%s) pool - AVERAGE:\n", memoryPool, memoryPool->getPoolName());
		tgcExtensions->printf("    Rank |      Size BytesPct | SizeClass BytesPct\n");

		for(U_16 i = 0; i < spaceSavingGetCurSize(stats->getSpaceSavingSizesAveragePercent()) && i < stats->getMaxAllocateSizes(); i++) {

			UDATA sizeForAveragePercent = 0;
			float averagePercent = 0;
			if (i < spaceSavingGetCurSize(stats->getSpaceSavingSizesAveragePercent())) {
				sizeForAveragePercent = (UDATA)spaceSavingGetKthMostFreq(stats->getSpaceSavingSizesAveragePercent(), i + 1);
				averagePercent = stats->convertPercentUDATAToFloat(spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizesAveragePercent(), i + 1));
			}

			UDATA sizeClassForAveragePercent = 0;
			float sizeClassAveragePercent = 0;
			if (i < spaceSavingGetCurSize(stats->getSpaceSavingSizeClassesAveragePercent())) {
				sizeClassForAveragePercent = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizeClassesAveragePercent(), i + 1);
				sizeClassAveragePercent = stats->convertPercentUDATAToFloat(spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizeClassesAveragePercent(), i + 1));
			}

			tgcExtensions->printf("    %4zu | %9zu %7.4f%% | %9zu %7.4f%%\n",
					i, sizeForAveragePercent, averagePercent, sizeClassForAveragePercent, sizeClassAveragePercent);
		}


	}
}

static void
tgcLargeAllocationPrintCurrentStatsForMemorySubSpace(OMR_VMThread* omrVMThread, MM_MemorySubSpace *memorySubSpace)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	/* we fetch directly the stats from the SubSpace. It will know if it needs to merge stats from child SubSpaces/Pools, or fetch directly from child Pool */
	MM_LargeObjectAllocateStats *stats = memorySubSpace->getLargeObjectAllocateStats();

	if (NULL !=	stats) {

		tgcExtensions->printf("---------------- CURRENT ----------------\n");
		tgcExtensions->printf("Rank |      Size      KBytes  Count | SizeClass      KBytes  Count\n");

		for(U_16 i = 0; i < spaceSavingGetCurSize(stats->getSpaceSavingSizes()) && i < stats->getMaxAllocateSizes(); i++){
			UDATA size = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizes(), i + 1);
			UDATA sizeCount = 0;
			if (0 != size) {
				sizeCount = spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizes() ,i + 1) / size;
			}
			UDATA sizeClass = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizeClasses(), i + 1);
			UDATA sizeClassCount = 0;
			if (0 != sizeClass) {
				sizeClassCount = spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizeClasses(), i + 1) / sizeClass;
			}

			tgcExtensions->printf("%4zu | %9zu %10zuK %6zu | %9zu %10zuK %6zu\n",
					i, size, sizeCount * size >> 10, sizeCount, sizeClass, sizeClass * sizeClassCount >> 10, sizeClassCount);
		}
	}
}

static void
tgcLargeAllocationPrintAverageStatsForMemorySubSpace(OMR_VMThread* omrVMThread, MM_MemorySubSpace *memorySubSpace)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	/* we fetch directly the stats from the SubSpace. It will know if it needs to merge stats from child SubSpaces/Pools, or fetch directly from child Pool */
	MM_LargeObjectAllocateStats *stats = memorySubSpace->getLargeObjectAllocateStats();

	if (NULL !=	stats) {
		tgcExtensions->printf("---------------- AVERAGE ----------------\n");
		tgcExtensions->printf("Rank |      Size BytesPct | SizeClass BytesPct\n");

		for(U_16 i = 0; i < spaceSavingGetCurSize(stats->getSpaceSavingSizesAveragePercent()) && i < stats->getMaxAllocateSizes(); i++){
			UDATA sizeForAveragePercent = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizesAveragePercent(), i + 1);
			float averagePercent = stats->convertPercentUDATAToFloat(spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizesAveragePercent(), i + 1));

			UDATA sizeClassForAveragePercent = (UDATA) spaceSavingGetKthMostFreq(stats->getSpaceSavingSizeClassesAveragePercent(), i + 1);
			float sizeClassAveragePercent = stats->convertPercentUDATAToFloat(spaceSavingGetKthMostFreqCount(stats->getSpaceSavingSizeClassesAveragePercent(), i + 1));

			tgcExtensions->printf("%4zu | %9zu %7.4f%% | %9zu %7.4f%%\n",
					i, sizeForAveragePercent, averagePercent, sizeClassForAveragePercent, sizeClassAveragePercent);
		}
	}
}

static void
tgcLargeAllocationPrintStatsForAllocateMemory(OMR_VMThread* omrVMThread)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *defaultMemorySubspace = defaultMemorySpace->getDefaultMemorySubSpace();

	if (defaultMemorySubspace->isPartOfSemiSpace()) {
		tgcExtensions->printf("======== Large Allocation Statistics ========\n");

		/* We are in generational configuration. defaultMemorySubspace points to one of two allocate Subspaces, but we want both. Thus we access the parent (SemiSpace subspace) */
		/* SemiSpace stats include only Mutator stats (no Collector stats during flipping) */
		MM_MemorySubSpace *topLevelMemorySubSpaceNew = defaultMemorySubspace->getTopLevelMemorySubSpace(MEMORY_TYPE_NEW);
		tgcExtensions->printf("Allocate subspace: %llx (%s)\n", topLevelMemorySubSpaceNew, topLevelMemorySubSpaceNew->getName());
		tgcLargeAllocationPrintCurrentStatsForMemorySubSpace(omrVMThread, topLevelMemorySubSpaceNew);
		tgcExtensions->printf("=============================================\n");
	}
}

static void
tgcLargeAllocationPrintCurrentStatsForTenureMemory(OMR_VMThread* omrVMThread)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	tgcExtensions->printf("==== Large Allocation Current Statistics ====\n");

	MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();

	/* Note that tenure stats:
	 * for Generational: will include stats both Collector and Mutator allocates
	 * for Flat: will include stats only Mutator allocates
	 */
	tgcExtensions->printf("Tenure subspace: %llx (%s) - CURRENT \n", tenureMemorySubspace, tenureMemorySubspace->getName());
	tgcLargeAllocationPrintCurrentStatsForMemorySubSpace(omrVMThread, tenureMemorySubspace);

	MM_MemoryPool *memoryPool = NULL;
	MM_HeapMemoryPoolIterator poolIterator(env, extensions->heap, tenureMemorySubspace);

	while (NULL != (memoryPool = poolIterator.nextPoolInSubSpace())) {
		tgcTlhAllocPrintStatsForMemoryPool(omrVMThread, memoryPool);
		tgcLargeAllocationPrintCurrentStatsForMemoryPool(omrVMThread, memoryPool);
	}

	tgcExtensions->printf("=============================================\n");
}

static void
tgcLargeAllocationPrintAverageStatsForTenureMemory(OMR_VMThread* omrVMThread, UDATA eventNum)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	tgcExtensions->printf("==== Large Allocation Average Statistics ====\n");

	MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();

	/* Note that tenure stats:
	 * for Generational: will include stats both Collector and Mutator allocates
	 * for Flat: will include stats only Mutator allocates
	 */
	MM_MemoryPool *memoryPool = NULL;
	MM_HeapMemoryPoolIterator poolIterator(env, extensions->heap, tenureMemorySubspace);

	while (NULL != (memoryPool = poolIterator.nextPoolInSubSpace())) {

		tgcLargeAllocationPrintAverageStatsForMemoryPool(omrVMThread, memoryPool);
	}

	tgcExtensions->printf("Tenure subspace: %llx (%s) - AVERAGE\n", tenureMemorySubspace, tenureMemorySubspace->getName());
	tgcLargeAllocationPrintAverageStatsForMemorySubSpace(omrVMThread, tenureMemorySubspace);

	tgcExtensions->printf("=============================================\n");
}

static void
tgcFreeMemoryPrintStats(OMR_VMThread* omrVMThread, bool globalGC)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	tgcExtensions->printf("=========== Free Memory Statistics ==========\n");
	tgcExtensions->printf("=========== Size Class Distribution =========\n");

	MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();

	MM_MemoryPool *memoryPool = NULL;
	MM_HeapMemoryPoolIterator poolIterator(env, extensions->heap, tenureMemorySubspace);

	while (NULL != (memoryPool = poolIterator.nextPoolInSubSpace())) {
		tgcFreeMemoryPrintStatsForMemoryPool(omrVMThread, memoryPool);
	}

	tgcExtensions->printf("Tenure subspace: %llx (%s)\n", tenureMemorySubspace, tenureMemorySubspace->getName());
	tgcFreeMemoryPrintStatsForMemorySubSpace(omrVMThread, tenureMemorySubspace, globalGC);

	tgcExtensions->printf("=============================================\n");
}

static void
tgcEstimateFragmentationPrintStats(OMR_VMThread* omrVMThread)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
	MM_LargeObjectAllocateStats* stats = tenureMemorySubspace->getLargeObjectAllocateStats();

	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	U_64 time = omrtime_hires_delta(0, stats->getTimeEstimateFragmentation(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
	U_64 cpuTime = stats->getCPUTimeEstimateFragmentation();
	if (cpuTime > time) {
		cpuTime = 0;
	}
	char timestamp[32];
	omrstr_ftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", omrtime_current_time_millis());
	tgcExtensions->printf("<tgcLargeAllocation op=\"estimateFragmentation\" timems=\"%llu.%03.3llu\" cputimes=\"%llu.%03.3llu\" remainingFreeMemory=\"%zu\" initialFreeMemory=\"%zu\" timestamp=\"%s\" />\n",
			time/1000, time%1000, cpuTime/1000, cpuTime%1000, stats->getRemainingFreeMemoryAfterEstimate(), stats->getFreeMemoryBeforeEstimate(), timestamp);

}

/**
 * print time for mergeLargeAllocateStats, mergeTLH, mergeFreeEntry and averageLargeAllocateStats
 */
static void
tgcMergeAveragePrintStats(OMR_VMThread* omrVMThread)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
		MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
		MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
		MM_LargeObjectAllocateStats* stats = tenureMemorySubspace->getLargeObjectAllocateStats();

		OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
		U_64 time = omrtime_hires_delta(0, stats->getTimeMergeAverage(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
		char timestamp[32];
		omrstr_ftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", omrtime_current_time_millis());
		tgcExtensions->printf("<tgcLargeAllocation op=\"mergeAndAverage\" timems=\"%llu.%03.3llu\" timestamp=\"%s\" />\n",
				time/1000, time%1000, timestamp);

}

static void
tgcVerifyBackoutInScavenger(J9VMThread* vmThread)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_ScavengerStats *scavengerStats = (MM_ScavengerStats *)&extensions->scavengerStats;
	if(scavengerStats->_backout) {
		tgcExtensions->printf("<tgcLargeAllocation op=\"gc\" type=\"scavenge\" details=\"aborted collection due to insufficient free space\" />\n");
	}
}

static void
tgcHookLargeAllocationGlobalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent* event = (MM_GlobalGCEndEvent*)eventData;
	tgcLargeAllocationPrintCurrentStatsForTenureMemory(event->currentThread);
	tgcLargeAllocationPrintAverageStatsForTenureMemory(event->currentThread, eventNum);
	tgcLargeAllocationPrintStatsForAllocateMemory(event->currentThread);
}

static void
tgcHookLargeAllocationLocalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	if (J9HOOK_MM_OMR_LOCAL_GC_START == eventNum) {
		MM_LocalGCStartEvent* event = (MM_LocalGCStartEvent*)eventData;
		tgcLargeAllocationPrintStatsForAllocateMemory(event->currentThread);
	} else if (J9HOOK_MM_OMR_LOCAL_GC_END == eventNum) {
		MM_LocalGCEndEvent* event = (MM_LocalGCEndEvent*)eventData;
		tgcLargeAllocationPrintCurrentStatsForTenureMemory(event->currentThread);
		tgcLargeAllocationPrintAverageStatsForTenureMemory(event->currentThread, eventNum);
	} else {
		Assert_MM_unreachable();
	}

}

static void
tgcHookFreeMemoryGlobalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent* event = (MM_GlobalGCEndEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	if (tgcExtensions->_largeAllocationVerboseRequested) {
		tgcFreeMemoryPrintStats(event->currentThread, true);
	}
	tgcMergeAveragePrintStats(event->currentThread);
	if (GLOBALGC_ESTIMATE_FRAGMENTATION == (extensions->estimateFragmentation & GLOBALGC_ESTIMATE_FRAGMENTATION)) {
		tgcEstimateFragmentationPrintStats(event->currentThread);
	}
}

static void
tgcHookFreeMemoryLocalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCEndEvent* event = (MM_LocalGCEndEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	if (tgcExtensions->_largeAllocationVerboseRequested) {
		tgcFreeMemoryPrintStats(event->currentThread, false);
	}

	tgcVerifyBackoutInScavenger((J9VMThread *) event->currentThread->_language_vmthread);
	tgcMergeAveragePrintStats(event->currentThread);
	if (LOCALGC_ESTIMATE_FRAGMENTATION == (extensions->estimateFragmentation & LOCALGC_ESTIMATE_FRAGMENTATION)) {
		tgcEstimateFragmentationPrintStats(event->currentThread);
	}
}

static void
tgcVerifyHaltedInConcurrentGC(J9VMThread* vmThread, const char* state, const char* status)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	tgcExtensions->printf("<tgcLargeAllocation op=\"gc\" type=\"concurrent\" details=\"concurrent-halted\" state=\"%s\" status=\"%s\" />\n", state, status);
}

static void
tgcHookVerifyHaltedInConcurrentGC(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ConcurrentHaltedEvent* event = (MM_ConcurrentHaltedEvent*)eventData;
#define CONCURRENT_STATUS_BUFFER_LENGTH 32
	char statusBuffer[CONCURRENT_STATUS_BUFFER_LENGTH];
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	const char* statusString = MM_ConcurrentGCStats::getConcurrentStatusString(env, event->executionMode, statusBuffer, CONCURRENT_STATUS_BUFFER_LENGTH);
#undef CONCURRENT_STATUS_BUFFER_LENGTH

	const char* stateString = "Complete";
	if (0 == event->isTracingExhausted) {
		stateString = "Tracing incomplete";
	}
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	switch (event->scanClassesMode) {
		case MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED:
		case MM_ScanClassesMode::SCAN_CLASSES_CURRENTLY_ACTIVE:
			stateString = "Class scanning incomplete";
			break;
		case MM_ScanClassesMode::SCAN_CLASSES_COMPLETE:
		case MM_ScanClassesMode::SCAN_CLASSES_DISABLED:
			break;
		default:
			stateString = "Class scanning bad state";
			break;
	}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	if (0 == event->isCardCleaningComplete) {
		stateString = "Card cleaning incomplete";
	}

	tgcVerifyHaltedInConcurrentGC(static_cast<J9VMThread*>(event->currentThread->_language_vmthread), stateString, statusString);
}

bool
tgcLargeAllocationInitialize(J9JavaVM *javaVM)
{
	bool result = true;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	/*TODO: remove condition check for concurrentSweep, currently tgc:largeAllocation can not be enabled in concurrentSweep case   */
	if (extensions->processLargeAllocateStats && extensions->isStandardGC()) {
		if (!extensions->isConcurrentSweepEnabled()) {
			J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
			if (tgcExtensions->_largeAllocationVerboseRequested) {
				(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookLargeAllocationGlobalPrintStats, OMR_GET_CALLSITE(), NULL);
				(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookLargeAllocationLocalPrintStats, OMR_GET_CALLSITE(), NULL);
				(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, tgcHookLargeAllocationLocalPrintStats, OMR_GET_CALLSITE(), NULL);
			}

			(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, tgcHookFreeMemoryGlobalPrintStats, OMR_GET_CALLSITE(), NULL);
			(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, tgcHookFreeMemoryLocalPrintStats, OMR_GET_CALLSITE(), NULL);

			J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
			(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_HALTED, tgcHookVerifyHaltedInConcurrentGC, OMR_GET_CALLSITE(), NULL);
		}
	}
	return result;
}
