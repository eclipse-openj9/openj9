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

#include <string.h>
#include <math.h>


#include "CompactGroupPersistentStats.hpp"

#include "AllocationContextTarok.hpp"
#include "CompactGroupManager.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "GlobalAllocationManagerTarok.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "Math.hpp"

MM_CompactGroupPersistentStats * 
MM_CompactGroupPersistentStats::allocateCompactGroupPersistentStats(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	UDATA persistentStatsAllocationSize = sizeof(MM_CompactGroupPersistentStats) * compactGroupCount;

	MM_CompactGroupPersistentStats * result = (MM_CompactGroupPersistentStats *)extensions->getForge()->allocate(persistentStatsAllocationSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != result) {
		memset(result, 0x0, persistentStatsAllocationSize);
		for (UDATA i = 0; i < compactGroupCount; i++) {
			result[i]._historicalSurvivalRate = 1.0;
			result[i]._weightedSurvivalRate = 1.0;
			result[i]._projectedInstantaneousSurvivalRate = 1.0;
//			result[i]._projectedInstantaneousSurvivalRatePerAgeUnit = 1.0;
			result[i]._projectedInstantaneousSurvivalRateThisPGC = 1.0;
			result[i]._projectedLiveBytes = 0;
			result[i]._liveBytesAbsoluteDeviation = 0;
			result[i]._regionCount = 0;
			result[i]._statsHaveBeenUpdatedThisCycle = false;
		}
	}
	return result;
}

void 
MM_CompactGroupPersistentStats::killCompactGroupPersistentStats(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions::getExtensions(env)->getForge()->free(persistentStats);
}


void
MM_CompactGroupPersistentStats::deriveWeightedSurvivalRates(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	/* this weight is completely arbitrary */
	const double olderWeight = 0.7;
	const double thisWeight = 1.0 - olderWeight;
	Trc_MM_CompactGroupPersistentStats_deriveWeightedSurvivalRates_Entry(env->getLanguageVMThread(), olderWeight);
	
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_GlobalAllocationManagerTarok *allocationManager = (MM_GlobalAllocationManagerTarok*)extensions->globalAllocationManager;
	
	UDATA regionMaxAge = extensions->tarokRegionMaxAge;
	UDATA managedAllocationContextCount = allocationManager->getManagedAllocationContextCount();
	for (UDATA contextIndex = 0; contextIndex < managedAllocationContextCount; contextIndex++) {
		MM_AllocationContextTarok *context = allocationManager->getAllocationContextByIndex(contextIndex);
		double olderSurvivalRate = 1.0;
		
		/* process the compact groups from oldest to youngest (note that the for loop relies on integer-underflow) */
		for (UDATA age = regionMaxAge; age <= regionMaxAge; age--) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumberForAge(env, age, context);
			
			double thisSurvivalRate = persistentStats[compactGroup]._historicalSurvivalRate;
			double weightedSurvivalRate = (olderWeight * olderSurvivalRate) + (thisWeight * thisSurvivalRate);
			/* the historical rate may decrease the apparent survival rate but isn't permitted to increase it */
			weightedSurvivalRate = OMR_MIN(weightedSurvivalRate, thisSurvivalRate);
		
			Assert_MM_true(0.0 <= weightedSurvivalRate);
			Assert_MM_true(1.0 >= weightedSurvivalRate);
		
			persistentStats[compactGroup]._weightedSurvivalRate = weightedSurvivalRate;
			
			Trc_MM_CompactGroupPersistentStats_deriveWeightedSurvivalRates_group(env->getLanguageVMThread(), contextIndex, age, thisSurvivalRate, weightedSurvivalRate);
			
			olderSurvivalRate = weightedSurvivalRate;
		}
	}
	Trc_MM_CompactGroupPersistentStats_deriveWeightedSurvivalRates_Exit(env->getLanguageVMThread());
}


void
MM_CompactGroupPersistentStats::calculateAgeGroupFractionsAtEdenBoundary(MM_EnvironmentVLHGC *env, U_64 ageInThisAgeGroup, U_64 *ageInThisCompactGroup, U_64 currentAge, U_64 allocatedSinceLastPGC, U_64 *edenFractionOfCompactGroup, U_64 *nonEdenFractionOfCompactGroup)
{
	/* Find if age group is split by Eden boundary.  */
	U_64 nonEdenFractionOfAgeGroup = 0;

	if (allocatedSinceLastPGC < currentAge) {
		/* At least some part is out of eden */
		if (currentAge - allocatedSinceLastPGC > ageInThisAgeGroup) {
			/* whole group is out eden */
			nonEdenFractionOfAgeGroup = ageInThisAgeGroup;
		} else {
			/* Less then full group is out of eden */
			nonEdenFractionOfAgeGroup = currentAge - allocatedSinceLastPGC;
		}
	}

	MM_GlobalAllocationManagerTarok *allocationManager = (MM_GlobalAllocationManagerTarok*)MM_GCExtensions::getExtensions(env)->globalAllocationManager;
	UDATA managedAllocationContextCount = allocationManager->getManagedAllocationContextCount();
	UDATA nonCommonAllocationContextCount = 1;
	if (managedAllocationContextCount > 1) {
		nonCommonAllocationContextCount = managedAllocationContextCount - 1;
	}

	/* we do not have historic info which fraction of this age group was allocated in this compact group; just take a fair share */
	*nonEdenFractionOfCompactGroup = nonEdenFractionOfAgeGroup / nonCommonAllocationContextCount;
	*ageInThisCompactGroup = *edenFractionOfCompactGroup + *nonEdenFractionOfCompactGroup;

}

void
MM_CompactGroupPersistentStats::updateProjectedSurvivalRate(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStatsArray, UDATA compactGroup)
{
//	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	const double newObservationWeight = 0.2;
	MM_CompactGroupPersistentStats *persistentStats = &persistentStatsArray[compactGroup];

	/* Calculate the historical survival rate (unchanged from original) */
	UDATA liveBeforeCollect = persistentStats->_measuredLiveBytesBeforeCollectInCollectedSet;
	
	if (liveBeforeCollect > 0) {
		UDATA totalBytesBeforeCollect = persistentStats->_measuredLiveBytesBeforeCollectInGroup;
		UDATA liveBytesInCollectedSetAfterCollect = persistentStats->_measuredLiveBytesAfterCollectInCollectedSet;
		
		/* Calculate weight based on how much of the group was collected */
		double weightOfNewStats = 1.0;
		if ((totalBytesBeforeCollect > 0) && (liveBeforeCollect < totalBytesBeforeCollect)) {
			weightOfNewStats = (double)liveBeforeCollect / (double)totalBytesBeforeCollect;
			Assert_MM_true(weightOfNewStats >= 0.0);
			Assert_MM_true(weightOfNewStats <= 1.0);
		}
		double weightOfOldStats = 1.0 - weightOfNewStats;

		/* Calculate observed survival rate for this collection */
		double thisSurvivalRate = OMR_MIN(1.0, (double)liveBytesInCollectedSetAfterCollect / (double)liveBeforeCollect);
		Assert_MM_true(thisSurvivalRate >= 0.0);

		/* Update historical survival rate with exponential moving average */
		double newSurvivalRate = (weightOfOldStats * persistentStats->_historicalSurvivalRate) + (weightOfNewStats * thisSurvivalRate);
		Assert_MM_true(newSurvivalRate >= 0.0);
		Assert_MM_true(newSurvivalRate <= 1.0);

		persistentStats->_historicalSurvivalRate = newSurvivalRate;
	}

	/* Calculate the projected instantaneous survival rate */
	UDATA projectedLiveBytesSelected = persistentStats->_projectedLiveBytesBeforeCollectInCollectedSet;

	if (projectedLiveBytesSelected > 0) {
		UDATA projectedLiveBytesBefore = persistentStats->_projectedLiveBytesBeforeCollectInGroup;
		UDATA measuredLiveBytesNotParticipating = (persistentStats->_measuredLiveBytesBeforeCollectInGroup - persistentStats->_measuredLiveBytesBeforeCollectInCollectedSet);
		UDATA measuredLiveBytesAfter = (persistentStats->_measuredLiveBytesAfterCollectInCollectedSet + measuredLiveBytesNotParticipating);
		double oldSurvivalRate = persistentStats->_projectedInstantaneousSurvivalRate;

		/* Get Eden and non-Eden fractions from the collection set */
		UDATA edenFraction = persistentStats->_projectedLiveBytesAfterPreviousPGCInCollectedSetForEdenFraction;
		UDATA nonEdenFraction = persistentStats->_projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction;
		UDATA totalFraction = edenFraction + nonEdenFraction;

		/* Calculate separate survival rates for Eden and non-Eden fractions */
		double thisSurvivalRateForEdenFraction = 0.5;
		double thisSurvivalRateForNonEdenFraction = 0.5;

		if (totalFraction > 0) {
			/* Base survival rate from measured bytes */
			double baseSurvivalRate = (double)(measuredLiveBytesAfter - measuredLiveBytesNotParticipating) / (double)totalFraction;
			if (baseSurvivalRate > 1.0) {
				baseSurvivalRate = 1.0;
			}

			thisSurvivalRateForEdenFraction = baseSurvivalRate;

			/* For non-Eden fraction, the observed rate already includes effects from previous collections
			 * In the original code, it walked backward dividing by previous survival rates
			 * to isolate this age group's contribution. We approximate this by dividing
			 * the observed rate by the previous age group's rate.
			 */
//			if ((nonEdenFraction > 0) && (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup) > 0)) {
//				/* Get the previous age group's survival rate */
//				UDATA currentAge = MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup);
//				if (currentAge > 0) {
//					/* Find the compact group for the previous age in the same context */
//					UDATA prevAge = currentAge - 1;
//					UDATA contextNumber = MM_CompactGroupManager::getAllocationContextNumberFromGroup(env, compactGroup);
//					MM_AllocationContextTarok *context = (MM_AllocationContextTarok *)extensions->globalAllocationManager->getAllocationContextByIndex(contextNumber);
//					if (NULL != context) {
//						UDATA prevGroup = MM_CompactGroupManager::getCompactGroupNumberForAge(env, prevAge, context);
//						double prevRate = persistentStatsArray[prevGroup]._projectedInstantaneousSurvivalRateThisPGC;
//
//						/* Divide by previous rate to isolate this group's contribution */
//						if (prevRate > 0.0) {
//							thisSurvivalRateForNonEdenFraction = baseSurvivalRate / prevRate;
//							if (thisSurvivalRateForNonEdenFraction > 1.0) {
//								thisSurvivalRateForNonEdenFraction = 1.0;
//							}
//						}
//					}
//				}
//			} else {
				thisSurvivalRateForNonEdenFraction = baseSurvivalRate;
//			}
		}

		/* Combine Eden and non-Eden survival rates with weighted average */
		double weightedMeanSurvivalRate = 0.5;
		if (totalFraction > 0) {
			weightedMeanSurvivalRate = (thisSurvivalRateForEdenFraction * edenFraction + thisSurvivalRateForNonEdenFraction * nonEdenFraction) / totalFraction;
		}

		/* Ensure survival rate is in valid range */
		Assert_MM_true(weightedMeanSurvivalRate >= 0.0);
		Assert_MM_true(weightedMeanSurvivalRate <= 1.0);

		/* Approximate zero survivor rate with a low non-zero number to avoid division by zero */
		if (0.0 == weightedMeanSurvivalRate) {
			weightedMeanSurvivalRate = 0.01;
		}

		/* Store survival rate */
		persistentStats->_projectedInstantaneousSurvivalRateThisPGC = weightedMeanSurvivalRate;

		/* Calculate weighted average with previous observations */
		double observedWeight = 1.0;
		if (projectedLiveBytesBefore > 0) {
			observedWeight = newObservationWeight * (double)projectedLiveBytesSelected / (double)projectedLiveBytesBefore;
		}

		persistentStats->_projectedInstantaneousSurvivalRate = (observedWeight * weightedMeanSurvivalRate) + ((1.0 - observedWeight) * oldSurvivalRate);

		Trc_MM_CompactGroupPersistentStats_updateProjectedSurvivalRate_Exit(
			env->getLanguageVMThread(),
			compactGroup,
			thisSurvivalRateForEdenFraction,
			thisSurvivalRateForNonEdenFraction,
			weightedMeanSurvivalRate,
			1.0, /* ageUnitsInThisAgeGroup - always 1 logical age unit */
			weightedMeanSurvivalRate,
			persistentStats->_projectedInstantaneousSurvivalRate,
			persistentStats->_projectedInstantaneousSurvivalRate);
	}
}

void
MM_CompactGroupPersistentStats::deriveProjectedLiveBytesStats(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_HeapRegionIteratorVLHGC regionIterator(extensions->heapRegionManager, MM_HeapRegionDescriptor::ALL);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	for (UDATA compactGroup= 0; compactGroup < compactGroupCount; compactGroup++) {
		persistentStats[compactGroup]._projectedLiveBytes = 0;
		persistentStats[compactGroup]._liveBytesAbsoluteDeviation = 0;
		persistentStats[compactGroup]._regionCount = 0;
	}

	while (NULL != (region = regionIterator.nextRegion())) {
		if(region->containsObjects()) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
			UDATA projectedLiveBytes = region->_projectedLiveBytes;
			IDATA liveBytesDeviation = region->_projectedLiveBytesDeviation;
			persistentStats[compactGroup]._projectedLiveBytes += projectedLiveBytes;
			persistentStats[compactGroup]._liveBytesAbsoluteDeviation += MM_Math::abs(liveBytesDeviation);
			persistentStats[compactGroup]._regionCount += 1;
		}
	}
}

void
MM_CompactGroupPersistentStats::resetLiveBytesStats(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	for (UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
		/* clear the data we use to determine projected liveness, etc */
		persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle = false;
		persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInCollectedSet = 0;
		persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInCollectedSet = 0;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSet = 0;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction = 0;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSetForEdenFraction = 0;
		persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInGroup = 0;
		persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInGroup = 0;
		persistentStats[compactGroup]._measuredLiveBytesAfterCollectInGroup = 0;
		persistentStats[compactGroup]._measuredLiveBytesAfterCollectInCollectedSet = 0;
		persistentStats[compactGroup]._measuredBytesCopiedFromGroupDuringCopyForward = 0;
		persistentStats[compactGroup]._measuredBytesCopiedToGroupDuringCopyForward = 0;

		/* TODO: lpnguyen move this or rename this function (not a live bytes stat */
		persistentStats[compactGroup]._regionsInRegionCollectionSetForPGC = 0;
	}
}

void
MM_CompactGroupPersistentStats::calculateLiveBytesForRegion(
		MM_EnvironmentVLHGC *env,
		MM_CompactGroupPersistentStats *persistentStats,
		UDATA compactGroup, MM_HeapRegionDescriptorVLHGC *region,
		UDATA measuredLiveBytes,
		UDATA projectedLiveBytes)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInCollectedSet += measuredLiveBytes;
	persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInCollectedSet += projectedLiveBytes;

	if (region->isEden()) {
		/* Eden regions: all live bytes are from previous PGC */
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSetForEdenFraction += region->_projectedLiveBytesPreviousPGC;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSet += region->_projectedLiveBytesPreviousPGC;
	} else {
		/* Non-Eden regions: use exponential decay based on logical age
		 * to better approximate the original allocation-age boundary calculation
		 */
		uintptr_t regionAge = region->getAge();
		uintptr_t maxAge = extensions->tarokRegionMaxAge;

		/* Calculate recency factor using exponential decay
		 * This better approximates the original behavior where older objects
		 * had exponentially lower contribution to the non-Eden fraction
		 */
		double recencyFactor = 1.0;

		if (maxAge > 0) {
			/* Use exponential decay: factor = exp(-k * age / maxAge)
			 * where k controls the decay rate (higher k = faster decay)
			 * k=2.0 provides a good balance similar to the original behavior
			 */
			double normalizedAge = (double)regionAge / (double)maxAge;
			recencyFactor = exp(-2.0 * normalizedAge);
		}

		MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
		uintptr_t regionSize = regionManager->getRegionSize();
		uintptr_t occupiedBytes = regionSize - region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
		double occupancyFactor = (double)occupiedBytes / (double)regionSize;

		/* Apply recency factor to projected live bytes from previous PGC */
		uintptr_t recentLiveBytes = (UDATA)(region->_projectedLiveBytesPreviousPGC * recencyFactor * occupancyFactor);

		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction += recentLiveBytes;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSet += recentLiveBytes;
	}
}

void
MM_CompactGroupPersistentStats::updateStatsBeforeCopyForward(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
	UDATA regionSize = regionManager->getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);

			if (!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
				UDATA completeFreeMemory = region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
				Assert_MM_true(completeFreeMemory <= regionSize);
				UDATA measuredLiveBytes = regionSize - completeFreeMemory;
				UDATA projectedLiveBytes = region->_projectedLiveBytes;

				persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInGroup += measuredLiveBytes;
				persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInGroup += projectedLiveBytes;
				if (region->_markData._shouldMark) {
					calculateLiveBytesForRegion(env, persistentStats, compactGroup, region, measuredLiveBytes, projectedLiveBytes);
				}
			}
		}
	}
}

void
MM_CompactGroupPersistentStats::updateStatsAfterCopyForward(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	for (UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
		if (!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
			UDATA liveBeforeCollect = persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInCollectedSet;

			if (liveBeforeCollect > 0) {
				UDATA totalBytesBeforeCollect = persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInGroup;
				UDATA measuredNonParticipatingLiveBytes = (totalBytesBeforeCollect - liveBeforeCollect);
				UDATA totalBytesAfterCollect = (persistentStats[compactGroup]._measuredBytesCopiedFromGroupDuringCopyForward + measuredNonParticipatingLiveBytes);

				Assert_MM_true(totalBytesBeforeCollect >= liveBeforeCollect);

				Assert_MM_true(totalBytesAfterCollect >= measuredNonParticipatingLiveBytes);
				persistentStats[compactGroup]._measuredLiveBytesAfterCollectInGroup = totalBytesAfterCollect;
				persistentStats[compactGroup]._measuredLiveBytesAfterCollectInCollectedSet = totalBytesAfterCollect - measuredNonParticipatingLiveBytes;
			}
		}
	}

	updateStatsAfterCollectionOperation(env, persistentStats);
}

void
MM_CompactGroupPersistentStats::updateStatsBeforeSweep(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
	UDATA regionSize = regionManager->getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);

			if (!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
				UDATA completeFreeMemory = region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
				Assert_MM_true(completeFreeMemory <= regionSize);
				UDATA measuredLiveBytes = regionSize - completeFreeMemory;
				UDATA projectedLiveBytes = region->_projectedLiveBytes;

				persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInGroup += measuredLiveBytes;
				persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInGroup += projectedLiveBytes;
				if (!region->_sweepData._alreadySwept) {
					calculateLiveBytesForRegion(env, persistentStats, compactGroup, region, measuredLiveBytes, projectedLiveBytes);
				}
			}
		}
	}
}

void
MM_CompactGroupPersistentStats::updateStatsAfterSweep(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
	UDATA regionSize = regionManager->getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager, MM_HeapRegionDescriptor::ALL);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);

			if (!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
				UDATA completeFreeMemory = region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
				Assert_MM_true(completeFreeMemory <= regionSize);
				UDATA measuredLiveBytes = regionSize - completeFreeMemory;

				persistentStats[compactGroup]._measuredLiveBytesAfterCollectInGroup += measuredLiveBytes;
				if (!region->_sweepData._alreadySwept) {
					persistentStats[compactGroup]._measuredLiveBytesAfterCollectInCollectedSet += measuredLiveBytes;
				}
			}
		}
	}
	updateStatsAfterCollectionOperation(env, persistentStats);
}

void
MM_CompactGroupPersistentStats::updateStatsBeforeCompact(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
	UDATA regionSize = regionManager->getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);

			if (!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
				UDATA completeFreeMemory = region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
				Assert_MM_true(completeFreeMemory <= regionSize);
				UDATA measuredLiveBytes = regionSize - completeFreeMemory;
				UDATA projectedLiveBytes = region->_projectedLiveBytes;

				persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInGroup += measuredLiveBytes;
				persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInGroup += projectedLiveBytes;
				if (region->_compactData._shouldCompact) {
					calculateLiveBytesForRegion(env, persistentStats, compactGroup, region, measuredLiveBytes, projectedLiveBytes);
				}
			}
		}
	}
}

void
MM_CompactGroupPersistentStats::updateStatsAfterCompact(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;
	UDATA regionSize = regionManager->getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager, MM_HeapRegionDescriptor::ALL);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);

			if (!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
				UDATA completeFreeMemory = region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
				Assert_MM_true(completeFreeMemory <= regionSize);
				UDATA measuredLiveBytes = regionSize - completeFreeMemory;

				persistentStats[compactGroup]._measuredLiveBytesAfterCollectInGroup += measuredLiveBytes;
				if (region->_compactData._shouldCompact) {
					persistentStats[compactGroup]._measuredLiveBytesAfterCollectInCollectedSet += measuredLiveBytes;
				}
			}
		}
	}
	updateStatsAfterCollectionOperation(env, persistentStats);
}

void
MM_CompactGroupPersistentStats::initProjectedLiveBytes(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_HeapRegionIteratorVLHGC regionIterator(extensions->heapRegionManager, MM_HeapRegionDescriptor::ALL);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	UDATA regionSize = extensions->heapRegionManager->getRegionSize();

	while (NULL != (region = regionIterator.nextRegion())) {
		/* UDATA_MAX has a special meaning of 'uninitialized' */
		if(region->containsObjects() && (UDATA_MAX == region->_projectedLiveBytes)) {
			UDATA completeFreeMemory = region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
			Assert_MM_true(completeFreeMemory <= regionSize);
			UDATA measuredLiveBytes = regionSize - completeFreeMemory;
			region->_projectedLiveBytes = measuredLiveBytes;
		}
	}
}

void
MM_CompactGroupPersistentStats::updateStatsAfterCollectionOperation(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	bool compactGroupUpdated = false;
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	for (UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
		if (persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInCollectedSet > 0) {
			if(!persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle) {
				persistentStats[compactGroup]._statsHaveBeenUpdatedThisCycle = true;
				updateProjectedSurvivalRate(env, persistentStats, compactGroup);
				compactGroupUpdated = true;
			}
		}
	}

	if (compactGroupUpdated) {
		deriveWeightedSurvivalRates(env, persistentStats);
	}
}

void
MM_CompactGroupPersistentStats::decayProjectedLiveBytesForRegions(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_HeapRegionIteratorVLHGC regionIterator(extensions->heapRegionManager, MM_HeapRegionDescriptor::ALL);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	MM_CompactGroupPersistentStats *persistentStats = extensions->compactGroupPersistentStats;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			/* Save snapshot of projectedLiveBytes before applying decay */
			region->_projectedLiveBytesPreviousPGC = region->_projectedLiveBytes;

			/* Get the compact group for this region based on its logical age */
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
			UDATA regionAge = region->getAge();

			/* Apply survival rate for this region's compact group only
			 * The original code walked backward through age groups, but with logical aging
			 * each region belongs to exactly one compact group based on its logical age
			 */
			double survivalRate = persistentStats[compactGroup]._projectedInstantaneousSurvivalRate;

			UDATA oldProjectedLiveBytes = region->_projectedLiveBytes;
			region->_projectedLiveBytes = (UDATA)((double)oldProjectedLiveBytes * survivalRate);

			Trc_MM_CompactGroupPersistentStats_decayProjectedLiveBytesForRegions(
				env->getLanguageVMThread(),
				extensions->heapRegionManager->mapDescriptorToRegionTableIndex(region),
				(double)oldProjectedLiveBytes / (1024 * 1024),
				(double)region->_projectedLiveBytes / (1024 * 1024),
				compactGroup,
				0.0, /* bytesRemaining - no longer applicable */
				(double)regionAge, /* currentAge - now logical age */
				survivalRate,
				survivalRate, /* baseSurvivalRate same as survivalRate */
				1.0, /*  ageUnitsInThisGroup - 1 logical age unit */
				compactGroup);
		}
	}
}

void
MM_CompactGroupPersistentStats::updateStatsBeforeCollect(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats)
{
	resetLiveBytesStats(env, persistentStats);
	initProjectedLiveBytes(env);
	decayProjectedLiveBytesForRegions(env);
}
