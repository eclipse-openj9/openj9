
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
			result[i]._projectedInstantaneousSurvivalRatePerAgeUnit = 1.0;
			result[i]._projectedInstantaneousSurvivalRateThisPGCPerAgeUnit = 1.0;
			result[i]._projectedLiveBytes = 0;
			result[i]._liveBytesAbsoluteDeviation = 0;
			result[i]._regionCount = 0;
			result[i]._statsHaveBeenUpdatedThisCycle = false;
			/* this is not really stats, but a constant; calculate only if unit is set */
			if (0 != extensions->tarokAllocationAgeUnit) {
				UDATA ageGroup = MM_CompactGroupManager::getRegionAgeFromGroup(env, i);
				if (ageGroup != extensions->tarokRegionMaxAge) {
					result[i]._maxAllocationAge = MM_CompactGroupManager::calculateMaximumAllocationAge(env, ageGroup + 1);
				} else {
					result[i]._maxAllocationAge = UDATA_MAX;
				}
			}
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
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	const double newObservationWeight = 0.2;
	 MM_CompactGroupPersistentStats *persistentStats = &persistentStatsArray[compactGroup];

	/* calculate the old historical survival rate */
	UDATA liveBeforeCollect = persistentStats->_measuredLiveBytesBeforeCollectInCollectedSet;
	
	if (liveBeforeCollect > 0) {
		UDATA totalBytesBeforeCollect = persistentStats->_measuredLiveBytesBeforeCollectInGroup;
		UDATA liveBytesInCollectedSetAfterCollect = persistentStats->_measuredLiveBytesAfterCollectInCollectedSet;
		
		/* Note that this calculation doesn't try to limit the maximum impact of our new survival rate on the historic score
		 * which is consistent with the existing _rorStats.  Since we always collect all of the nursery, this means that we
		 * will always replace the survival rate data of the nursery after each PGC
		 */
		double weightOfNewStats = 1.0;
		if ((totalBytesBeforeCollect > 0) && (liveBeforeCollect < totalBytesBeforeCollect)) {
			weightOfNewStats = (double)liveBeforeCollect / (double)totalBytesBeforeCollect;
			Assert_MM_true(weightOfNewStats >= 0.0);
			Assert_MM_true(weightOfNewStats <= 1.0);
		}
		double weightOfOldStats = 1.0 - weightOfNewStats;

		/* Due to dark matter estimates the survival rate could appear to exceed 1.0 by a small amount, so cap it at 1.0. */
		double thisSurvivalRate = OMR_MIN(1.0, (double)liveBytesInCollectedSetAfterCollect / (double)liveBeforeCollect);
		Assert_MM_true(thisSurvivalRate >= 0.0);

		double newSurvivalRate = (weightOfOldStats * persistentStats->_historicalSurvivalRate) + (weightOfNewStats * thisSurvivalRate);
		Assert_MM_true(newSurvivalRate >= 0.0);
		Assert_MM_true(newSurvivalRate <= 1.0);

		persistentStats->_historicalSurvivalRate = newSurvivalRate;
	}

	/* Calculate the new projected survival rate */
	UDATA projectedLiveBytesSelected = persistentStats->_projectedLiveBytesBeforeCollectInCollectedSet;
	U_64 allocatedSinceLastPGC = ((MM_IncrementalGenerationalGC *)extensions->getGlobalCollector())->getAllocatedSinceLastPGC();
	if ((projectedLiveBytesSelected > 0) && (allocatedSinceLastPGC > 0)) {
		UDATA projectedLiveBytesBefore = persistentStats->_projectedLiveBytesBeforeCollectInGroup;
		UDATA measuredLiveBytesNotParticipating = (persistentStats->_measuredLiveBytesBeforeCollectInGroup - persistentStats->_measuredLiveBytesBeforeCollectInCollectedSet);
		UDATA measuredLiveBytesAfter = (persistentStats->_measuredLiveBytesAfterCollectInCollectedSet + measuredLiveBytesNotParticipating);
		double oldSurvivalRate = persistentStats->_projectedInstantaneousSurvivalRatePerAgeUnit;
		/* By definition Instantaneous Survival Rate = (live bytes at the end of the current age group)/(live bytes at the end of the previous age group) per tarokAllocationAgeUnit
		 * (tarokAllocationAgeUnit defaults to Eden size, but could be far less than Eden size)
		 */

		/* We'll initially find survivorRate since last PGC and take out survivor rate of previous age groups to get survivor rate of current age group */
		U_64 bytesRemaining = allocatedSinceLastPGC;
		/* For Eden age groups, we'll take out "only" age groups since the moment they were created (they may not exist at previous PGC) */
		/* TODO: currentAge should be average age (after GC) of collection set in this group */
		U_64 currentAge = persistentStats->_maxAllocationAge;
		U_64 maxAgeInThisAgeGroup = extensions->tarokAllocationAgeUnit;
		if (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup) > 0) {
			maxAgeInThisAgeGroup = currentAge - persistentStatsArray[compactGroup - 1]._maxAllocationAge;
		}

		/* normalize for large age groups, larger than Eden */
		U_64 ageInThisAgeGroup = OMR_MIN(maxAgeInThisAgeGroup, allocatedSinceLastPGC);
		/* ageInThisCompactGroup essentially represents the amount of bytes allocated in this compact group out of this age group */
		U_64 ageInThisCompactGroup = 0;

		UDATA currentCompactGroup = compactGroup;

		U_64 edenFractionOfCompactGroup = persistentStats->_projectedLiveBytesAfterPreviousPGCInCollectedSetForEdenFraction;
		U_64 nonEdenFractionOfCompactGroup = 0;		
		calculateAgeGroupFractionsAtEdenBoundary(env, ageInThisAgeGroup, &ageInThisCompactGroup, currentAge, allocatedSinceLastPGC, &edenFractionOfCompactGroup, &nonEdenFractionOfCompactGroup);

		/* Calculate separately survivorRate for Eden and non Eden fractions of the age group */
		double thisSurvivalRateForEdenFraction = (double)(measuredLiveBytesAfter - measuredLiveBytesNotParticipating) / ageInThisCompactGroup;
		/* TODO: check cases when thisSurvivalRateForEdenFraction > 1.0. Should be completely or almost non-existent */
		if (thisSurvivalRateForEdenFraction > 1.0){
			thisSurvivalRateForEdenFraction = 1.0;
		}

		double thisSurvivalRateForNonEdenFraction = thisSurvivalRateForEdenFraction;
		double projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction = (double)(persistentStats->_projectedLiveBytesAfterPreviousPGCInCollectedSet - edenFractionOfCompactGroup);
		Trc_MM_CompactGroupPersistentStats_updateProjectedSurvivalRate_Entry(env->getLanguageVMThread(),
			compactGroup,
			(double)persistentStats->_projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction / (1024*1024),
			(double)persistentStats->_projectedLiveBytesAfterPreviousPGCInCollectedSet / (1024*1024),
			(double)(measuredLiveBytesAfter - measuredLiveBytesNotParticipating)/ (1024*1024),
			(double)measuredLiveBytesAfter/ (1024*1024),
			(double) measuredLiveBytesNotParticipating/ (1024*1024),
			projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction/ (1024*1024));
		Trc_MM_CompactGroupPersistentStats_calculateAgeGroupFractions(env->getLanguageVMThread(), persistentStats->_maxAllocationAge, maxAgeInThisAgeGroup, ageInThisAgeGroup, edenFractionOfCompactGroup, nonEdenFractionOfCompactGroup);

		/* TODO: check cases when projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction < 0. Should be completely or almost non-existent */
		if (projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction > 0.0) {
			thisSurvivalRateForNonEdenFraction = thisSurvivalRateForEdenFraction * nonEdenFractionOfCompactGroup / projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction;
		}
		if (thisSurvivalRateForNonEdenFraction > 1.0){
			thisSurvivalRateForNonEdenFraction = 1.0;
		}

		/* Skip all ageUnits in current compact group, to the maximum of what we allocated since last GC - this is where we want to calculate survivorRate */
		bytesRemaining -= ageInThisAgeGroup;
		currentAge -= ageInThisAgeGroup;

		while (((bytesRemaining > 0) || (0 != edenFractionOfCompactGroup)) && (currentAge > 0)) {
			/* check if we moved in next age group */
			if (MM_CompactGroupManager::getRegionAgeFromGroup(env, currentCompactGroup) != 0) {
				if (currentAge <= persistentStatsArray[currentCompactGroup - 1]._maxAllocationAge) {
					currentCompactGroup -= 1;
				}
			}

			double baseSurvivalRate = persistentStatsArray[currentCompactGroup]._projectedInstantaneousSurvivalRateThisPGCPerAgeUnit;

			U_64 minAllocationAgeForThisCompactGroup = 0;

			if (MM_CompactGroupManager::getRegionAgeFromGroup(env, currentCompactGroup) > 0) {
				minAllocationAgeForThisCompactGroup = persistentStatsArray[currentCompactGroup - 1]._maxAllocationAge;
			}

			U_64 ageInCurrentGroup = currentAge - minAllocationAgeForThisCompactGroup;
			U_64 ageInCurrentGroupForEden = ageInCurrentGroup;
			double ageUnitsInCurrentGroup = (double)ageInCurrentGroupForEden / extensions->tarokAllocationAgeUnit;
			double survivalRate = pow(baseSurvivalRate, ageUnitsInCurrentGroup);

			Trc_MM_CompactGroupPersistentStats_updateProjectedSurvivalRate_eden(env->getLanguageVMThread(), thisSurvivalRateForEdenFraction, (double)currentAge/(1024*1024),
			 (double)bytesRemaining/(1024*1024), (double)ageInCurrentGroupForEden/(1024*1024), survivalRate, baseSurvivalRate, ageUnitsInCurrentGroup, currentCompactGroup);
			thisSurvivalRateForEdenFraction = thisSurvivalRateForEdenFraction / survivalRate;

			U_64 ageInCurrentGroupForNonEden  = OMR_MIN(ageInCurrentGroup, bytesRemaining);
			ageUnitsInCurrentGroup = (double)ageInCurrentGroupForNonEden / extensions->tarokAllocationAgeUnit;
			survivalRate = pow(baseSurvivalRate, ageUnitsInCurrentGroup);

			Assert_MM_true(0.0 < survivalRate);
			Trc_MM_CompactGroupPersistentStats_updateProjectedSurvivalRate_nonEden(env->getLanguageVMThread(), thisSurvivalRateForNonEdenFraction, (double)currentAge/(1024*1024),
			 (double)bytesRemaining/(1024*1024), (double)ageInCurrentGroupForNonEden/(1024*1024), survivalRate, baseSurvivalRate, ageUnitsInCurrentGroup, currentCompactGroup);
			thisSurvivalRateForNonEdenFraction = thisSurvivalRateForNonEdenFraction / survivalRate;

			Assert_MM_true(bytesRemaining >= ageInCurrentGroupForNonEden);
			bytesRemaining -= ageInCurrentGroupForNonEden;
			Assert_MM_true(currentAge >= ageInCurrentGroup);
			currentAge -= ageInCurrentGroup;
		}

		if (thisSurvivalRateForEdenFraction > 1.0){
			thisSurvivalRateForEdenFraction = 1.0;
		}
		if (thisSurvivalRateForNonEdenFraction > 1.0){
			thisSurvivalRateForNonEdenFraction = 1.0;
		}

		/* Combine Eden and non-Eden survivor rate */
		double weightedMeanSurvivalRate = 0.5;
		if (0 != ageInThisCompactGroup) {
			weightedMeanSurvivalRate = (thisSurvivalRateForEdenFraction * edenFractionOfCompactGroup + thisSurvivalRateForNonEdenFraction * nonEdenFractionOfCompactGroup) / ageInThisCompactGroup;
		}
		double ageUnitsInThisAgeGroup = (double)ageInThisAgeGroup / extensions->tarokAllocationAgeUnit;
		double maxAgeUnitsInThisGroup = (double)maxAgeInThisAgeGroup / extensions->tarokAllocationAgeUnit;

		double thisSurvivalRate = pow(weightedMeanSurvivalRate, 1/ageUnitsInThisAgeGroup);

		Assert_MM_true(thisSurvivalRate >= 0.0);
		Assert_MM_true(thisSurvivalRate <= 1.0);

		/* Approximate zero survivor rate with a low non-zero number to avoid division with zero in future calculations */
		if (0.0 == thisSurvivalRate) {
			thisSurvivalRate = 0.01;
		}
		persistentStats->_projectedInstantaneousSurvivalRateThisPGCPerAgeUnit = thisSurvivalRate;

		double observedWeight = 1.0;
		if (projectedLiveBytesBefore > 0) {
			observedWeight = newObservationWeight * (double)projectedLiveBytesSelected / (double)projectedLiveBytesBefore;
		}
		persistentStats->_projectedInstantaneousSurvivalRatePerAgeUnit = (observedWeight * thisSurvivalRate) + ((1.0 - observedWeight) * oldSurvivalRate);
		persistentStats->_projectedInstantaneousSurvivalRate = pow(persistentStats->_projectedInstantaneousSurvivalRatePerAgeUnit, maxAgeUnitsInThisGroup);

		Trc_MM_CompactGroupPersistentStats_updateProjectedSurvivalRate_Exit(env->getLanguageVMThread(), compactGroup, thisSurvivalRateForEdenFraction, thisSurvivalRateForNonEdenFraction,
		 weightedMeanSurvivalRate, ageUnitsInThisAgeGroup, thisSurvivalRate, persistentStats->_projectedInstantaneousSurvivalRate, persistentStats->_projectedInstantaneousSurvivalRatePerAgeUnit);
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
		persistentStats[compactGroup]._measuredAllocationAgeToGroupDuringCopyForward = 0;
		persistentStats[compactGroup]._averageAllocationAgeToGroup = 0;

		/* TODO: lpnguyen move this or rename this function (not a live bytes stat */
		persistentStats[compactGroup]._regionsInRegionCollectionSetForPGC = 0;
	}
}

void
MM_CompactGroupPersistentStats::calculateLiveBytesForRegion(MM_EnvironmentVLHGC *env,  MM_CompactGroupPersistentStats *persistentStats, UDATA compactGroup, MM_HeapRegionDescriptorVLHGC *region, UDATA measuredLiveBytes, UDATA projectedLiveBytes)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	U_64 allocatedSinceLastPGC = ((MM_IncrementalGenerationalGC *)extensions->getGlobalCollector())->getAllocatedSinceLastPGC();

	persistentStats[compactGroup]._measuredLiveBytesBeforeCollectInCollectedSet += measuredLiveBytes;
	persistentStats[compactGroup]._projectedLiveBytesBeforeCollectInCollectedSet += projectedLiveBytes;

	if (region->isEden()) {
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSetForEdenFraction += region->_projectedLiveBytesPreviousPGC;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSet += region->_projectedLiveBytesPreviousPGC;
	}  else	{

		U_64 boundary = 0;
		U_64 maxAllocationAge = extensions->compactGroupPersistentStats[compactGroup]._maxAllocationAge;
		U_64 minAllocationAge = 0;

		if (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup) > 0) {
			minAllocationAge = extensions->compactGroupPersistentStats[compactGroup - 1]._maxAllocationAge;
		}
		U_64 ageSpan = maxAllocationAge - minAllocationAge;
		/* while the region is non-Eden, the compact group it belongs to might cross Eden boundary */
		U_64 nonEdenAgeSpan = 0;
		/* Only the upper portion of the non-Eden span is a good representative. About to calculate the size of the sample */
		U_64 sampleSpan = 0;

		/* We use a premise: given a point of time m (expressed in MB allocated), liveness of objects are constant between m/expBase and m.
		 * For non, linear aging this range (m/expBase,m) can span multiple age groups, which may have different liveness.
		 */

		if (maxAllocationAge > allocatedSinceLastPGC) {
			U_64 oldestAgePointObjectsCameFrom = maxAllocationAge - allocatedSinceLastPGC;
			sampleSpan = (U_64) ((double)(oldestAgePointObjectsCameFrom) / extensions->tarokAllocationAgeExponentBase);
			/* between oldestAgePointObjectsCameFrom - sampleSpan and oldestAgePointObjectsCameFrom, we consider objects had same liveness. */
			/* however, object are already forward-aged at the end of previous PGC, so we are talking about (maxAllocationAge - sampleSpan, maxAllocationAge) span */
			boundary = maxAllocationAge - sampleSpan;
			nonEdenAgeSpan = OMR_MIN(ageSpan, maxAllocationAge - allocatedSinceLastPGC);
		}

		double nonEdenSpanExpandRatio = 1.0;
		if (maxAllocationAge > allocatedSinceLastPGC + sampleSpan) {
			nonEdenSpanExpandRatio = (double)nonEdenAgeSpan / (maxAllocationAge - allocatedSinceLastPGC - sampleSpan);
		}

		UDATA liveBytesAboveBoundary = 0;
		if (region->getAllocationAge() >= boundary) {
			liveBytesAboveBoundary = region->_projectedLiveBytesPreviousPGC;

			if (region->getLowerAgeBound() < boundary) {
				/* not a whole region is above boundary */

				/* first calculated the the fraction of objects below average age */
				UDATA liveBytesBelowAverageAge = (UDATA)(region->_projectedLiveBytesPreviousPGC
														* (region->getUpperAgeBound() - region->getAllocationAge())
														/ (region->getUpperAgeBound() - region->getLowerAgeBound()));

				/* now, find the fraction of below boundary */
				UDATA liveBytesBelowBoundary = (UDATA)(liveBytesBelowAverageAge
														* (boundary - region->getLowerAgeBound())
														/ (region->getAllocationAge() - region->getLowerAgeBound()));

				liveBytesAboveBoundary -= liveBytesBelowBoundary;

			}

		} else {
			if (region->getUpperAgeBound() > boundary) {
				/* at least a fraction of the region is above boundary */

				/* first calculated the the fraction of objects above average age */
				UDATA liveBytesAboveAverageAge = (UDATA)(region->_projectedLiveBytesPreviousPGC
														* (region->getAllocationAge() - region->getLowerAgeBound())
														/ (region->getUpperAgeBound() - region->getLowerAgeBound()));

				/* now, find the fraction of above boundary */
				 liveBytesAboveBoundary = (UDATA)(liveBytesAboveAverageAge
														* (region->getUpperAgeBound() - boundary)
														/ (region->getUpperAgeBound() - region->getAllocationAge()));
			}

		}

		liveBytesAboveBoundary = (UDATA)(nonEdenSpanExpandRatio * liveBytesAboveBoundary);
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction += liveBytesAboveBoundary;
		persistentStats[compactGroup]._projectedLiveBytesAfterPreviousPGCInCollectedSet += liveBytesAboveBoundary;
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
				MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
				UDATA completeFreeMemory = pool->getFreeMemoryAndDarkMatterBytes();
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
				MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
				UDATA completeFreeMemory = pool->getFreeMemoryAndDarkMatterBytes();
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
				MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
				UDATA completeFreeMemory = pool->getFreeMemoryAndDarkMatterBytes();
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
				MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
				UDATA completeFreeMemory = pool->getFreeMemoryAndDarkMatterBytes();
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
				MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
				UDATA completeFreeMemory = pool->getFreeMemoryAndDarkMatterBytes();
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
			MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
			UDATA completeFreeMemory = memoryPool->getFreeMemoryAndDarkMatterBytes();
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
	MM_CompactGroupPersistentStats * persistentStats = extensions->compactGroupPersistentStats;

	while (NULL != (region = regionIterator.nextRegion())) {
		if(region->containsObjects()) {
			/* before applying decay, take a snapshot of projectedLiveBytes used later for survivor rate calculation */
			region->_projectedLiveBytesPreviousPGC = region->_projectedLiveBytes;
			// TODO amicic: do we need to do on regions in eden (projectedLiveBytes is measuredLiveBytes)?

			I_64 allocatedSinceLastPGC = ((MM_IncrementalGenerationalGC *)extensions->getGlobalCollector())->getAllocatedSinceLastPGC();
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);

			I_64 currentAge = (I_64)region->getAllocationAge();
			I_64 bytesRemaining = allocatedSinceLastPGC;
			UDATA currentCompactGroup = compactGroup;

			while ((bytesRemaining > 0) && (currentAge > 0)) {
				if (MM_CompactGroupManager::getRegionAgeFromGroup(env, currentCompactGroup) > 0) {
					if ((currentAge <= (IDATA)persistentStats[currentCompactGroup - 1]._maxAllocationAge)) {
						currentCompactGroup -= 1;
					}
				}

				double baseSurvivalRate = persistentStats[currentCompactGroup]._projectedInstantaneousSurvivalRatePerAgeUnit;

				U_64 minAllocationAgeForThisCompactGroup = 0;

				if (MM_CompactGroupManager::getRegionAgeFromGroup(env, currentCompactGroup) > 0) {
					minAllocationAgeForThisCompactGroup = persistentStats[currentCompactGroup - 1]._maxAllocationAge;
				}

				U_64 ageInThisGroup = OMR_MIN((U_64)bytesRemaining, (U_64)currentAge - minAllocationAgeForThisCompactGroup);
				double ageUnitsInThisGroup = (double)ageInThisGroup / extensions->tarokAllocationAgeUnit;
				double survivalRate = pow(baseSurvivalRate, ageUnitsInThisGroup);

				UDATA oldProjectedLiveBytes =  region->_projectedLiveBytes;
				region->_projectedLiveBytes = (UDATA) ((double)oldProjectedLiveBytes * survivalRate);

				Trc_MM_CompactGroupPersistentStats_decayProjectedLiveBytesForRegions(env->getLanguageVMThread(), extensions->heapRegionManager->mapDescriptorToRegionTableIndex(region),
						(double)oldProjectedLiveBytes / (1024*1024), (double)region->_projectedLiveBytes / (1024*1024), compactGroup,
						(double)bytesRemaining / (1024*1024), (double)currentAge / (1024*1024), survivalRate, baseSurvivalRate, ageUnitsInThisGroup, currentCompactGroup);

				bytesRemaining -= ageInThisGroup;
				currentAge -= ageInThisGroup;
			}
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
