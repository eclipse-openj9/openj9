
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "Tgc.hpp"
#include "mmhook.h"

#include <math.h>

#if defined(J9VM_GC_MODRON_COMPACTION)

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"

static void
processCompactDataForTGC(J9JavaVM *javaVM, MM_CompactStartEvent* event, bool forCompactRegionOnly)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	/* Calculate the average scores for regions */
	double totalRegionCount = 0;
#if 0
	double sumRegionAgeNormalized = 0;
	double sumDarkMatterNormalized = 0;
	double sumTotalFreeNormalized = 0;
	double sumAverageFreeNormalized = 0;
	double sumAbilityToSatisfyAllocateNormalized = 0;
#endif
	double scoreBucket90_100 = 0;
	double scoreBucket80_90 = 0;
	double scoreBucket70_80 = 0;
	double scoreBucket60_70 = 0;
	double scoreBucket50_60 = 0;
	double scoreBucket40_50 = 0;
	double scoreBucket20_40 = 0;
	double scoreBucket0_20 = 0;
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	/* Calculate average and std deviation for individual compact scores */
	GC_HeapRegionIteratorVLHGC regionIterator1(extensions->heap->getHeapRegionManager(), MM_HeapRegionDescriptor::MANAGED);
	while(NULL != (region = regionIterator1.nextRegion())) {
		if (region->containsObjects() && (!forCompactRegionOnly || region->_compactData._shouldCompact)) {
			totalRegionCount+=1;
#if 0
			sumRegionAgeNormalized += region->_compactData._regionAgeNormalized;
			sumDarkMatterNormalized += region->_compactData._darkMatterNormalized;
			sumTotalFreeNormalized += region->_compactData._totalFreeNormalized;
			sumAverageFreeNormalized += region->_compactData._averageFreeNormalized;
			sumAbilityToSatisfyAllocateNormalized += region->_compactData._abilityToSatisfyAllocateNormalized;
#endif
			double currentScore = region->_compactData._compactScore;
			if (currentScore > 90.0) {
				scoreBucket90_100+=1;
			} else if (currentScore > 80.0) {
				scoreBucket80_90+=1;
			} else if (currentScore > 70.0) {
				scoreBucket70_80+=1;
			} else if (currentScore > 60.0) {
				scoreBucket60_70+=1;
			} else if (currentScore > 50.0) {
				scoreBucket50_60+=1;
			} else if (currentScore > 40.0) {
				scoreBucket40_50+=1;
			} else if (currentScore > 20.0) {
				scoreBucket20_40+=1;
			} else {
				scoreBucket0_20+=1;
			}
		}
	}
#if 0
	double avgRegionAgeNormalized = sumRegionAgeNormalized / totalRegionCount;
	double avgDarkMatterNormalized = sumDarkMatterNormalized / totalRegionCount;
	double avgTotalFreeNormalized = sumTotalFreeNormalized / totalRegionCount;
	double avgAverageFreeNormalized = sumAverageFreeNormalized / totalRegionCount;
	double avgAbilityToSatisfyAllocateNormalized = sumAbilityToSatisfyAllocateNormalized / totalRegionCount;
	
	/* Use the averages from previous operation to calculate the standard deviation */
	double std_deviation_sqr_avgRegionAgeNormalized = 0;
	double std_deviation_sqr_avgDarkMatterNormalized = 0;
	double std_deviation_sqr_avgTotalFreeNormalized = 0;
	double std_deviation_sqr_avgAverageFreeNormalized = 0;
	double std_deviation_sqr_avgAbilityToSatisfyAllocateNormalized = 0;
	GC_HeapRegionIteratorVLHGC regionIterator2(extensions->heap->getHeapRegionManager(), MM_HeapRegionDescriptor::MANAGED);
	region = NULL;
	while (NULL != (region = regionIterator2.nextRegion())) {
		if (!forCompactRegionOnly || region->_compactData._shouldCompact) {
			double diff_avgRegionAgeNormalized = avgRegionAgeNormalized - region->_compactData._regionAgeNormalized;
			std_deviation_sqr_avgRegionAgeNormalized += (diff_avgRegionAgeNormalized * diff_avgRegionAgeNormalized);
			
			double diff_avgDarkMatterNormalized = (avgDarkMatterNormalized - region->_compactData._darkMatterNormalized);
			std_deviation_sqr_avgDarkMatterNormalized += (diff_avgDarkMatterNormalized * diff_avgDarkMatterNormalized);
			
			double diff_avgTotalFreeNormalized = (avgTotalFreeNormalized - region->_compactData._totalFreeNormalized);
			std_deviation_sqr_avgTotalFreeNormalized += (diff_avgTotalFreeNormalized * diff_avgTotalFreeNormalized);
			
			double diff_avgAverageFreeNormalized = (avgAverageFreeNormalized - region->_compactData._averageFreeNormalized);
			std_deviation_sqr_avgAverageFreeNormalized += (diff_avgAverageFreeNormalized * diff_avgAverageFreeNormalized);
			
			double diff_avgAbilityToSatisfyAllocateNormalized = (avgAbilityToSatisfyAllocateNormalized - region->_compactData._abilityToSatisfyAllocateNormalized);
			std_deviation_sqr_avgAbilityToSatisfyAllocateNormalized += (diff_avgAbilityToSatisfyAllocateNormalized * diff_avgAbilityToSatisfyAllocateNormalized);
		}
	}
	double std_dev_avgRegionAgeNormalized = sqrt(std_deviation_sqr_avgRegionAgeNormalized / totalRegionCount);
	double std_dev_avgDarkMatterNormalized = sqrt(std_deviation_sqr_avgDarkMatterNormalized / totalRegionCount);
	double std_dev_avgTotalFreeNormalized = sqrt(std_deviation_sqr_avgTotalFreeNormalized / totalRegionCount);
	double std_dev_avgAverageFreeNormalized = sqrt(std_deviation_sqr_avgAverageFreeNormalized / totalRegionCount);
	double std_dev_avgAbilityToSatisfyAllocateNormalized = sqrt(std_deviation_sqr_avgAbilityToSatisfyAllocateNormalized / totalRegionCount);
#endif
	
	UDATA gcCount = event->gcCount;
	tgcExtensions->printf("Compact(%zu): region count: %.0f\n", gcCount, totalRegionCount);
#if 0
	tgcExtensions->printf("Compact(%zu):               %10s %10s %10s %12s %25s\n", gcCount, "RegionAge", "DarkMatter", "TotalFree", "AverageFree", "AbilityToSatisfyAllocate");
	tgcExtensions->printf("Compact(%zu): avg           %10.0f %10.0f %10.0f %12.0f %25.0f\n", 
			gcCount, avgRegionAgeNormalized, avgDarkMatterNormalized, avgTotalFreeNormalized, avgAverageFreeNormalized, avgAbilityToSatisfyAllocateNormalized);
	tgcExtensions->printf("Compact(%zu): std deviation %10.0f %10.0f %10.0f %12.0f %25.0f\n",
			gcCount,
			std_dev_avgRegionAgeNormalized,
			std_dev_avgDarkMatterNormalized,
			std_dev_avgTotalFreeNormalized,
			std_dev_avgAverageFreeNormalized,
			std_dev_avgAbilityToSatisfyAllocateNormalized);
#endif
	
	tgcExtensions->printf("Compact(%zu): Score distribution:\n", gcCount);
	tgcExtensions->printf("Compact(%zu): Range:       %6s %6s %6s %6s %6s %6s %6s %6s\n", 
			gcCount, "<= 20", "<= 40", "<= 50", "<= 60", "<= 70", "<= 80", "<= 90", "<= 100");
	tgcExtensions->printf("Compact(%zu): Region Count:%6.0f %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f\n", 
			gcCount,
			scoreBucket0_20,
			scoreBucket20_40,
			scoreBucket40_50,
			scoreBucket50_60,
			scoreBucket60_70,
			scoreBucket70_80,
			scoreBucket80_90,
			scoreBucket90_100);
}
	
/**
 * Report NUMA statistics prior to a collection
 */
static void
tgcHookReportIntelligentCompactStatistics(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_CompactStartEvent* event = (MM_CompactStartEvent*)eventData;
	UDATA gcCount = event->gcCount;
	J9JavaVM *javaVM = static_cast<J9VMThread*>(event->currentThread->_language_vmthread)->javaVM;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(javaVM);
	
	/* report TGC stats for all regoins */
	tgcExtensions->printf("Compact(%zu): For All Regions:\n", gcCount);
	processCompactDataForTGC(javaVM, event, false);
	
	/* report TGC stats for compact regoins */
	tgcExtensions->printf("Compact(%zu): For Compact Regions:\n", gcCount);
	processCompactDataForTGC(javaVM, event, true);
}


/**
 * Initialize NUMA tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by NUMA tgc tracing.
 */
bool
tgcIntelligentCompactInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;
	
	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_COMPACT_START, tgcHookReportIntelligentCompactStatistics, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* J9VM_GC_MODRON_COMPACTION */
