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

/**
 * @file
 * @ingroup GC_Modron_Tarok
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "ModronAssertions.h"

#include <math.h>
#include <string.h>

#include "SchedulingDelegate.hpp"

#include "CompactGroupPersistentStats.hpp"
#include "CycleState.hpp"
#include "CompactGroupManager.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "GlobalAllocationManagerTarok.hpp"
#include "Heap.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "MemoryPoolBumpPointer.hpp"

/* NOTE: old logic for determining incremental thresholds has been deleted. Please 
 * see CVS history, version 1.14, if you need to find this logic
 */

/* Arbitrarily given historical averaging weight for scan rate measurement.
 * We want to give much more weight to GMP info than PGC,
 * since scan rate is used for GMP duration estimation
 */
const double measureScanRateHistoricWeightForGMP = 0.50;
const double measureScanRateHistoricWeightForPGC = 0.95;
const double partialGCTimeHistoricWeight = 0.80;
const double incrementalScanTimePerGMPHistoricWeight = 0.50;
const double bytesScannedConcurrentlyPerGMPHistoricWeight = 0.50;

MM_SchedulingDelegate::MM_SchedulingDelegate (MM_EnvironmentVLHGC *env, MM_HeapRegionManager *manager)
	: MM_BaseNonVirtual()
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _regionManager(manager)
	, _taxationIndex(0)
	, _remainingGMPIntermissionIntervals(0)
	, _nextIncrementWillDoPartialGarbageCollection(false)
	, _nextIncrementWillDoGlobalMarkPhase(false)
	, _nextPGCShouldCopyForward(_extensions->tarokPGCShouldCopyForward)
	, _globalSweepRequired(false)
	, _disableCopyForwardDuringCurrentGlobalMarkPhase(false)
	, _idealEdenRegionCount(0)
	, _minimumEdenRegionCount(0)
	, _edenRegionCount(0)
	, _edenSurvivalRateCopyForward(1.0)
	, _nonEdenSurvivalCountCopyForward(0)
	, _previousReclaimableRegions(0)
	, _previousDefragmentReclaimableRegions(0)
	, _regionConsumptionRate(0.0)
	, _defragmentRegionConsumptionRate(0.0)
	, _bytesCompactedToFreeBytesRatio(0.0)
	, _averageCopyForwardBytesCopied(0.0)
	, _averageCopyForwardBytesDiscarded(0.0)
	, _averageSurvivorSetRegionCount(0.0)
	, _averageCopyForwardRate(1.0)
	, _averageMacroDefragmentationWork(0.0)
	, _currentMacroDefragmentationWork(0)
	, _didGMPCompleteSinceLastReclaim(false)
	, _liveSetBytesAfterPartialCollect(0)
	, _heapOccupancyTrend(1.0)
	, _liveSetBytesBeforeGlobalSweep(0)
	, _liveSetBytesAfterGlobalSweep(0)
	, _previousLiveSetBytesAfterGlobalSweep(0)
	, _scannableBytesRatio(1.0)
	, _historicTotalIncrementalScanTimePerGMP(0)
	, _historicBytesScannedConcurrentlyPerGMP(0)
	, _partialGcStartTime(0)
	, _historicalPartialGCTime(0)
	, _dynamicGlobalMarkIncrementTimeMillis(50)
	, _scanRateStats()
{
	_typeId = __FUNCTION__;
}



UDATA
MM_SchedulingDelegate::getInitialTaxationThreshold(MM_EnvironmentVLHGC *env)
{
	/* reset all stored state and call getNextTaxationThreshold() */
	_nextIncrementWillDoGlobalMarkPhase = false;
	_nextIncrementWillDoPartialGarbageCollection = false;
	_taxationIndex = 0;
	_remainingGMPIntermissionIntervals = _extensions->tarokGMPIntermission;
	calculateEdenSize(env);

	/* initial value for _averageSurvivorSetRegionCount is arbitrarily chosen as 30% of Eden size (after first Eden is selected) */
	_averageSurvivorSetRegionCount = 0.3 * (double)getCurrentEdenSizeInBytes(env) / _regionManager->getRegionSize();

	return getNextTaxationThreshold(env);
}

void 
MM_SchedulingDelegate::globalMarkPhaseCompleted(MM_EnvironmentVLHGC *env)
{
	/* Taking a snapshot of _liveSetBytesAfterPartialCollect from the last PGC.
	 * This is slightly incorrect. We should take liveSetBytes at the beginning of next PGC
	 * (just before sweep is done)
	 */
	_liveSetBytesBeforeGlobalSweep = _liveSetBytesAfterPartialCollect;

	_remainingGMPIntermissionIntervals = _extensions->tarokGMPIntermission;
	
	/* reset the reclaimable estimate, since we just created more reclaimable data */
	_previousReclaimableRegions = 0;

	_didGMPCompleteSinceLastReclaim = true;

	_globalSweepRequired = true;

	_disableCopyForwardDuringCurrentGlobalMarkPhase = false;

	updateGMPStats(env);

}

void 
MM_SchedulingDelegate::globalMarkIncrementCompleted(MM_EnvironmentVLHGC *env)
{
	measureScanRate(env, measureScanRateHistoricWeightForGMP);
}

void 
MM_SchedulingDelegate::globalGarbageCollectCompleted(MM_EnvironmentVLHGC *env, UDATA reclaimableRegions, UDATA defragmentReclaimableRegions)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	/* Re-estimate the reclaimable region set but don't measure region consumption, since this wasn't a PGC */
	_previousReclaimableRegions = reclaimableRegions;
	_previousDefragmentReclaimableRegions = defragmentReclaimableRegions;

	/* Global GC will do full compact of the heap. No work is left for PGCs */
	_bytesCompactedToFreeBytesRatio = 0.0;

	/* since we did full sweep, there is no need for next PGC to do it again */
	_globalSweepRequired = false;

	/* if GMP ended up with AF, we need to clear this flag as if GMP normally completed */
	_disableCopyForwardDuringCurrentGlobalMarkPhase = false;

	Trc_MM_SchedulingDelegate_globalGarbageCollectCompleted(env->getLanguageVMThread(), _bytesCompactedToFreeBytesRatio);

	TRIGGER_J9HOOK_MM_PRIVATE_VLHGC_GARBAGE_COLLECT_COMPLETED(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock()
	);
}

void
MM_SchedulingDelegate::partialGarbageCollectStarted(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(0 == _partialGcStartTime);

	PORT_ACCESS_FROM_ENVIRONMENT(env);

	/* Record the GC start time in order to track Partial GC times (and averages) over the course of the application lifetime */
	_partialGcStartTime = j9time_hires_clock();
}

void
MM_SchedulingDelegate::determineNextPGCType(MM_EnvironmentVLHGC *env)
{
	/* if we have no historic scan rate info, we want to force M/S/C collect */
	if (0.0 == _scanRateStats.microSecondsPerByteScanned) {
		env->_cycleState->_reasonForMarkCompactPGC = MM_CycleState::reason_calibration;
		_nextPGCShouldCopyForward = false;
	}

	/* Aborted CopyForward happened in near past. The rest of PGCs until GMP completes, should not try CopyForward. */
	/* try only mark partial nursery regions instead of mark all of collectionSet in CopyForwardHybrid mode */
	if (_disableCopyForwardDuringCurrentGlobalMarkPhase && !_extensions->tarokEnableCopyForwardHybrid) {
		env->_cycleState->_reasonForMarkCompactPGC = MM_CycleState::reason_recent_abort;
		_nextPGCShouldCopyForward = false;
	}

	env->_cycleState->_shouldRunCopyForward = _nextPGCShouldCopyForward;
	if (_nextPGCShouldCopyForward && _extensions->tarokPGCShouldMarkCompact) {
		/* we are going to perform a copy-forward and are allowed to compact so the next cycle should compact */
		_nextPGCShouldCopyForward = false;
	} else if (!_nextPGCShouldCopyForward && _extensions->tarokPGCShouldCopyForward) {
		/* we are going to perform a compact and are allowed to copy-forward so the next cycle should copy-forward */
		_nextPGCShouldCopyForward = true;
	} else {
		/* we aren't allowed to change from our current mode so leave it as is */
	}
}

void
MM_SchedulingDelegate::calculateGlobalMarkIncrementTimeMillis(MM_EnvironmentVLHGC *env, U_64 pgcTime)
{
	if(U_32_MAX < pgcTime) {
		/* Time likely traveled backwards due to a clock adjustment - just ignore this round */
	} else {
		/* Prime or calculate the running weighted average for PGC times */
		if (0 == _historicalPartialGCTime) {
			_historicalPartialGCTime = pgcTime;
		} else {
			_historicalPartialGCTime = (U_64) ((_historicalPartialGCTime * partialGCTimeHistoricWeight) + (pgcTime * (1-partialGCTimeHistoricWeight)));
		}

		Assert_MM_true(U_32_MAX >= _historicalPartialGCTime);
		/* we just take a fraction (1/3) of the recent average, so that we do not impede mutator utilization significantly */
		/* (note that we need to assume a mark increment took at least 1 millisecond or else we will divide by zero in later calculations) */
		_dynamicGlobalMarkIncrementTimeMillis = OMR_MAX((UDATA)(_historicalPartialGCTime / 3), 1);
	}

}

void
MM_SchedulingDelegate::partialGarbageCollectCompleted(MM_EnvironmentVLHGC *env, UDATA reclaimableRegions, UDATA defragmentReclaimableRegions)
{
	Trc_MM_SchedulingDelegate_partialGarbageCollectCompleted_Entry(env->getLanguageVMThread(), reclaimableRegions, defragmentReclaimableRegions);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CopyForwardStats *copyForwardStats = &static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats;
	
	_globalSweepRequired = false;
	/* copy out the Eden size of the previous interval (between the last PGC and this one) before we recalculate the next one */
	UDATA edenCountBeforeCollect = getCurrentEdenSizeInRegions(env);
	
	Trc_MM_SchedulingDelegate_partialGarbageCollectCompleted_stats(env->getLanguageVMThread(),
			copyForwardStats->_edenEvacuateRegionCount,
			copyForwardStats->_nonEdenEvacuateRegionCount,
			copyForwardStats->_edenSurvivorRegionCount,
			copyForwardStats->_nonEdenSurvivorRegionCount,
			edenCountBeforeCollect);

	if (env->_cycleState->_shouldRunCopyForward) {
		UDATA regionSize = _regionManager->getRegionSize();
		
		/* count the number of survivor regions allocated specifically to support Eden survivors */
		UDATA edenSurvivorCount = copyForwardStats->_edenSurvivorRegionCount;
		UDATA nonEdenSurvivorCount = copyForwardStats->_nonEdenSurvivorRegionCount;
		
		/* estimate how many more regions we would have needed to avoid abort */
		Assert_MM_true( (0 == copyForwardStats->_scanBytesEden) || copyForwardStats->_aborted || (0 != copyForwardStats->_nonEvacuateRegionCount));
		Assert_MM_true( (0 == copyForwardStats->_scanBytesNonEden) || copyForwardStats->_aborted || (0 != copyForwardStats->_nonEvacuateRegionCount));
		edenSurvivorCount += (copyForwardStats->_scanBytesEden + regionSize - 1) / regionSize;
		nonEdenSurvivorCount += (copyForwardStats->_scanBytesNonEden + regionSize - 1) / regionSize;

		/* Eden count could be 0 in special case, after compaction if there is still no free region for scheduling eden(eden count = 0),
		   will skip update Survival Rate */
		if (0 != edenCountBeforeCollect) {
			double thisSurvivalRate = (double)edenSurvivorCount / (double)edenCountBeforeCollect;
			updateSurvivalRatesAfterCopyForward(thisSurvivalRate, nonEdenSurvivorCount);
		}

		if (copyForwardStats->_aborted && (0 ==_remainingGMPIntermissionIntervals)) {
			_disableCopyForwardDuringCurrentGlobalMarkPhase = true;
		}
	} else {
		/* measure scan rate in PGC, only if we did M/S/C collect */
		measureScanRate(env, measureScanRateHistoricWeightForPGC);
	}

	measureConsumptionForPartialGC(env, reclaimableRegions, defragmentReclaimableRegions);
	calculateAutomaticGMPIntermission(env);
	calculateEdenSize(env);
	estimateMacroDefragmentationWork(env);
	
	/* Calculate the time spent in the current Partial GC */
	U_64 partialGcEndTime = j9time_hires_clock();
	U_64 pgcTime = j9time_hires_delta(_partialGcStartTime, partialGcEndTime, J9PORT_TIME_DELTA_IN_MILLISECONDS);
	/* Clear the start time to be clear that we've used it */
	_partialGcStartTime = 0;
	calculateGlobalMarkIncrementTimeMillis(env, pgcTime);

	TRIGGER_J9HOOK_MM_PRIVATE_VLHGC_GARBAGE_COLLECT_COMPLETED(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		partialGcEndTime
	);
	
	Trc_MM_SchedulingDelegate_partialGarbageCollectCompleted_Exit(env->getLanguageVMThread());
}

UDATA
MM_SchedulingDelegate::getNextTaxationThresholdInternal(MM_EnvironmentVLHGC *env)
{
	/* these must be in their initial invalid state (both false) when this is called */
	Assert_MM_false(_nextIncrementWillDoPartialGarbageCollection);
	Assert_MM_false(_nextIncrementWillDoGlobalMarkPhase);
	 
	UDATA threshold = (_edenRegionCount * _regionManager->getRegionSize());
	UDATA nextTaxationIndex = _taxationIndex;
	
	if(_extensions->tarokEnableIncrementalGMP) {
		UDATA numerator = _extensions->tarokPGCtoGMPNumerator;
		UDATA denominator = _extensions->tarokPGCtoGMPDenominator;
		if (1 == numerator) {
			/* the PGC:GMP ratio is 1:n. Therefore every (n+1)th taxation point is a PGC, and the remainder are GMPs.
			 * e.g. --GMP--PGC--GMP--GMP--GMP--PGC--GMP--GMP--GMP--PGC--
			 */
			if (0 == (nextTaxationIndex % (denominator + 1))) {
				_nextIncrementWillDoGlobalMarkPhase = true;
			} else {
				_nextIncrementWillDoPartialGarbageCollection = true;
			}
			/* divide the gap between PGCs up into n+1 taxation points */
			threshold /= (denominator + 1);
		} else if (1 == denominator) {
			/* The PGC:GMP ratio is n:1. Therefore every (n+1)th taxation point is a GMP, and the remainder are PGCs.
			 * The GMP should occur half way between two PGCs.
			 * e.g. ------PGC------PGC---GMP---PGC------PGC---GMP---PGC------ 
			 */
			if (0 == (nextTaxationIndex % (numerator + 1))) {
				/* we just completed a PGC, and the next increment is a GMP */
				_nextIncrementWillDoGlobalMarkPhase = true;
				threshold /= 2;
			} else if (0 == ((nextTaxationIndex - 1) % (numerator + 1))) {
				/* we just completed a GMP, and the next increment is a PGC */
				_nextIncrementWillDoPartialGarbageCollection = true;
				threshold /= 2;
			} else {
				/* we just completed a PGC, and the next increment is also a PGC */
				_nextIncrementWillDoPartialGarbageCollection = true;
			}
		} else {
			/* the ratio must be 1:n or n:1 */
			Assert_MM_unreachable();
		}
	} else {
		/* Incremental GMP is disabled, so every increment just does a PGC.
		 * e.g. ------PGC------PGC------PGC------PGC------PGC------
		 */ 
		_nextIncrementWillDoPartialGarbageCollection = true;
	}
	
	_taxationIndex += 1;
		
	return threshold;
}

UDATA
MM_SchedulingDelegate::getNextTaxationThreshold(MM_EnvironmentVLHGC *env)
{
	/* TODO: eventually this should be some adaptive number which the 
	 * delegate calculates based on survival rates, collection times, ... 
	 */
	
	Trc_MM_SchedulingDelegate_getNextTaxationThreshold_Entry(env->getLanguageVMThread());
	
	UDATA nextTaxationIndex = _taxationIndex;
	UDATA threshold = 0;
	
	/* consume thresholds until we complete the GMP intermission or we encounter a PGC. */
	/* TODO: this could be time consuming if the intermission were very large */
	do {
		threshold += getNextTaxationThresholdInternal(env);
		
		/* skip the next GMP interval if necessary */
		if ( (0 < _remainingGMPIntermissionIntervals) && _nextIncrementWillDoGlobalMarkPhase ) {
			_remainingGMPIntermissionIntervals -= 1;
			_nextIncrementWillDoGlobalMarkPhase = false;
		}
	} while (!_nextIncrementWillDoGlobalMarkPhase && !_nextIncrementWillDoPartialGarbageCollection);

	UDATA regionSize = _regionManager->getRegionSize();
	threshold = OMR_MAX(regionSize, MM_Math::roundToFloor(regionSize, threshold));
	
	Trc_MM_SchedulingDelegate_getNextTaxationThreshold_Exit(env->getLanguageVMThread(),
			nextTaxationIndex, 
			(_edenRegionCount * regionSize),
			threshold,
			_nextIncrementWillDoGlobalMarkPhase ? 1 : 0,
			_nextIncrementWillDoPartialGarbageCollection ? 1 : 0);
	
	return threshold;
}

void 
MM_SchedulingDelegate::getIncrementWork(MM_EnvironmentVLHGC *env, bool* doPartialGarbageCollection, bool* doGlobalMarkPhase)
{
	*doPartialGarbageCollection = _nextIncrementWillDoPartialGarbageCollection;
	*doGlobalMarkPhase = _nextIncrementWillDoGlobalMarkPhase;
	
	/* invalidate the remembered values */
	_nextIncrementWillDoPartialGarbageCollection = false;
	_nextIncrementWillDoGlobalMarkPhase = false;
}

void 
MM_SchedulingDelegate::measureScanRate(MM_EnvironmentVLHGC *env, double historicWeight)
{
	Trc_MM_SchedulingDelegate_measureScanRate_Entry(env->getLanguageVMThread(), env->_cycleState->_collectionType);
	MM_MarkVLHGCStats *markStats = &static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._markStats;
	
	UDATA currentBytesScanned = markStats->_bytesScanned + markStats->_bytesCardClean;

	if (0 != currentBytesScanned) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		UDATA historicalBytesScanned = _scanRateStats.historicalBytesScanned;
		U_64 historicalScanMicroseconds = _scanRateStats.historicalScanMicroseconds;
		/* NOTE: scan time is the total time all threads spent scanning */
		U_64 currentScanMicroseconds = j9time_hires_delta(0, markStats->getScanTime(), J9PORT_TIME_DELTA_IN_MICROSECONDS);

		if (0 != historicalBytesScanned) {
			/* Keep a historical count of bytes scanned and scan times and re-derive microsecondsperBytes every time we receive new data */
			_scanRateStats.historicalBytesScanned = (UDATA) ((historicalBytesScanned * historicWeight) + (currentBytesScanned * (1.0 - historicWeight)));
			_scanRateStats.historicalScanMicroseconds = (U_64) ((historicalScanMicroseconds * historicWeight) + (currentScanMicroseconds * (1.0 - historicWeight)));
		} else {
			/* if we have no historic data, do not use averaging */
			_scanRateStats.historicalBytesScanned = currentBytesScanned;
			_scanRateStats.historicalScanMicroseconds = currentScanMicroseconds;
		}

		if (0 != _scanRateStats.historicalBytesScanned) {
			double microSecondsPerByte = (double)_scanRateStats.historicalScanMicroseconds / (double)_scanRateStats.historicalBytesScanned;
			_scanRateStats.microSecondsPerByteScanned = microSecondsPerByte;
		}

		Trc_MM_SchedulingDelegate_measureScanRate_summary(env->getLanguageVMThread(), _extensions->gcThreadCount, currentBytesScanned, currentScanMicroseconds, _scanRateStats.historicalBytesScanned, _scanRateStats.historicalScanMicroseconds, _scanRateStats.microSecondsPerByteScanned);
	}

	Trc_MM_SchedulingDelegate_measureScanRate_Exit(env->getLanguageVMThread(), _scanRateStats.microSecondsPerByteScanned );
}

void
MM_SchedulingDelegate::estimateMacroDefragmentationWork(MM_EnvironmentVLHGC *env)
{
	const double historicWeight = 0.80; /* arbitrarily give 80% weight to historical result, 20% to newest result */
	_averageMacroDefragmentationWork = (_averageMacroDefragmentationWork * historicWeight) + (_currentMacroDefragmentationWork * (1.0 - historicWeight));
	Trc_MM_SchedulingDelegate_estimateMacroDefragmentationWork(env->getLanguageVMThread(), _currentMacroDefragmentationWork, _averageMacroDefragmentationWork);

	_currentMacroDefragmentationWork = 0;
}

void
MM_SchedulingDelegate::updateCurrentMacroDefragmentationWork(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
{
	MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
	UDATA freeMemory = memoryPool->getFreeMemoryAndDarkMatterBytes();
	UDATA liveData = _regionManager->getRegionSize() - freeMemory;

	double bytesDiscardedPerByteCopied = (_averageCopyForwardBytesCopied > 0.0) ? (_averageCopyForwardBytesDiscarded / _averageCopyForwardBytesCopied) : 0.0;
	UDATA estimatedFreeMemoryDiscarded = (UDATA)(liveData * bytesDiscardedPerByteCopied);
	UDATA recoverableFreeMemory = MM_Math::saturatingSubtract(freeMemory, estimatedFreeMemoryDiscarded);

	/* take a min out of free memory and live data.
	 * However, this is an overestimate, since the work will often be calculated twice (both as source and as destination).
	 * More correct estimate requires knowledge of all regions in oldest age group (knapsack problem) .
	 */
	_currentMacroDefragmentationWork += OMR_MIN(recoverableFreeMemory , liveData);
}

void
MM_SchedulingDelegate::updateLiveBytesAfterPartialCollect()
{
	/* Measure the amount of data to be scanned.
	 * This is an approximate upper bound. The actual amount will be lower, since:
	 * a) not everything measured is actually live
	 * b) the measured data includes primitive arrays, which don't need to be scanned
	 */
	_liveSetBytesAfterPartialCollect = 0;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			_liveSetBytesAfterPartialCollect += region->getSize();
			_liveSetBytesAfterPartialCollect -= memoryPool->getActualFreeMemorySize();
			_liveSetBytesAfterPartialCollect -= memoryPool->getDarkMatterBytes();
		} else if (region->isArrayletLeaf()) {
			if (_extensions->objectModel.isObjectArray(region->_allocateData.getSpine())) {
				_liveSetBytesAfterPartialCollect += region->getSize();
			}
		} 
	}
}

double
MM_SchedulingDelegate::calculateEstimatedGlobalBytesToScan() const
{
	/* If historic occupancy trend is negative (due to high death rate),
	 * liveSetAdjustedByOccupancyTrend should still be no less than current _liveSetBytesAfterGlobalSweep
	 * (we do not want to extrapolate negative trend)
	 */
	double heapOccupancyTrendAdjusted = OMR_MAX(0.0, _heapOccupancyTrend);
	/* If current occupancy trend is negative (due to strong DCS effect), liveSetAdjustedByOccupancyTrend should equal current liveSetBytesAfterPartialCollect */
	double liveSetBytesAfterPartialDeltaSinceLastGlobalSweep = OMR_MAX(0.0, (double)_liveSetBytesAfterPartialCollect - (double)_liveSetBytesAfterGlobalSweep);
	double liveSetAdjustedForOccupancyTrend = _liveSetBytesAfterPartialCollect - (liveSetBytesAfterPartialDeltaSinceLastGlobalSweep * (1.0 - heapOccupancyTrendAdjusted));

	double liveSetAdjustedForScannableBytesRatio = liveSetAdjustedForOccupancyTrend * _scannableBytesRatio;
	return liveSetAdjustedForScannableBytesRatio;
}

UDATA
MM_SchedulingDelegate::estimateGlobalMarkIncrements(MM_EnvironmentVLHGC *env, double liveSetAdjustedForScannableBytesRatio) const
{
	Trc_MM_SchedulingDelegate_estimateGlobalMarkIncrements_Entry(env->getLanguageVMThread());

	/* we can consider liveSetAdjustedForScannableBytesRatio to be the total bytes the GMP needs to scan */
	Assert_MM_true(0 != _extensions->gcThreadCount);
	double estimatedScanMillis = liveSetAdjustedForScannableBytesRatio * _scanRateStats.microSecondsPerByteScanned / _extensions->gcThreadCount / 1000.0;
	UDATA currentMarkIncrementMillis = currentGlobalMarkIncrementTimeMillis(env);
	Assert_MM_true(0 != currentMarkIncrementMillis);
	double estimatedGMPIncrements = estimatedScanMillis / currentMarkIncrementMillis;
	Trc_MM_SchedulingDelegate_estimateGlobalMarkIncrements_liveSetBytes(env->getLanguageVMThread(), _liveSetBytesAfterPartialCollect, (UDATA)0, (UDATA)liveSetAdjustedForScannableBytesRatio);
	Trc_MM_SchedulingDelegate_estimateGlobalMarkIncrements_summary(env->getLanguageVMThread(), estimatedScanMillis, estimatedGMPIncrements);
	
	/* adding 1 increment for final GMP phase (most importantly clearable processing) */
	UDATA result = (UDATA)ceil(estimatedGMPIncrements) + 1;
	Trc_MM_SchedulingDelegate_estimateGlobalMarkIncrements_Exit(env->getLanguageVMThread(), result);
	return result;
}

UDATA
MM_SchedulingDelegate::getBytesToScanInNextGMPIncrement(MM_EnvironmentVLHGC *env) const
{
	UDATA targetPauseTimeMillis = currentGlobalMarkIncrementTimeMillis(env);
	double calculatedWorkTargetDouble = (((double)targetPauseTimeMillis * 1000.0) / _scanRateStats.microSecondsPerByteScanned) * (double)_extensions->gcThreadCount;

	/* minimum to UDATA_MAX in case we overflowed */
	UDATA calculatedWorkTarget = (UDATA) OMR_MIN(calculatedWorkTargetDouble, (double)UDATA_MAX);

	UDATA workTarget = OMR_MAX(calculatedWorkTarget, _extensions->tarokMinimumGMPWorkTargetBytes._valueSpecified);

	Trc_MM_SchedulingDelegate_getBytesToScanInNextGMPIncrement(env->getLanguageVMThread(), targetPauseTimeMillis, _scanRateStats.microSecondsPerByteScanned, _extensions->gcThreadCount, workTarget);

	return workTarget;
}

void 
MM_SchedulingDelegate::measureConsumptionForPartialGC(MM_EnvironmentVLHGC *env, UDATA currentReclaimableRegions, UDATA currentDefragmentReclaimableRegions)
{
	/* check to see if we have a valid previous data point */
	if (0 == _previousReclaimableRegions) {
		/* this must be the first PGC after a GMP. Since the GMP affected reclaimable memory, we have no reliable way to measure consumption for this cycle */ 
		Trc_MM_SchedulingDelegate_measureConsumptionForPartialGC_noPreviousData(env->getLanguageVMThread());
	} else {
		/* Use a signed number. The PGC may have negative consumption if it recovered more than an Eden-worth of memory, or if the estimates are a bit off */
		IDATA regionsConsumed = (IDATA)_previousReclaimableRegions - (IDATA)currentReclaimableRegions;
		const double historicWeight = 0.80; /* arbitrarily give 80% weight to historical result, 20% to newest result */
		_regionConsumptionRate = (_regionConsumptionRate * historicWeight) + (regionsConsumed * (1.0 - historicWeight));
		Trc_MM_SchedulingDelegate_measureConsumptionForPartialGC_consumptionRate(env->getLanguageVMThread(), regionsConsumed, _previousReclaimableRegions, currentReclaimableRegions, _regionConsumptionRate);
	}
	_previousReclaimableRegions = currentReclaimableRegions;

	/* check to see if we have a valid previous data point */
	if (0 == _previousDefragmentReclaimableRegions) {
		/* this must be the first PGC after a GMP. Since the GMP affected reclaimable memory, we have no reliable way to measure consumption for this cycle */
		Trc_MM_SchedulingDelegate_measureConsumptionForPartialGC_noPreviousData(env->getLanguageVMThread());
	} else {
		/* Use a signed number. The PGC may have negative consumption if it recovered more than an Eden-worth of memory, or if the estimates are a bit off */
		IDATA defragmentRegionsConsumed = (IDATA)_previousDefragmentReclaimableRegions - (IDATA)currentDefragmentReclaimableRegions;
		const double historicWeight = 0.80; /* arbitrarily give 80% weight to historical result, 20% to newest result */
		_defragmentRegionConsumptionRate = (_defragmentRegionConsumptionRate * historicWeight) + (defragmentRegionsConsumed * (1.0 - historicWeight));
		Trc_MM_SchedulingDelegate_measureConsumptionForPartialGC_defragmentConsumptionRate(env->getLanguageVMThread(), defragmentRegionsConsumed, _previousDefragmentReclaimableRegions, currentDefragmentReclaimableRegions, _defragmentRegionConsumptionRate);
	}
	_previousDefragmentReclaimableRegions = currentDefragmentReclaimableRegions;
}

UDATA
MM_SchedulingDelegate::estimatePartialGCsRemaining(MM_EnvironmentVLHGC *env) const
{
	Trc_MM_SchedulingDelegate_estimatePartialGCsRemaining_Entry(env->getLanguageVMThread(), _regionConsumptionRate, _previousDefragmentReclaimableRegions);

	UDATA partialCollectsRemaining = UDATA_MAX;
	if (_regionConsumptionRate > 0.0) {
		/* TODO: decide how to reconcile kick-off with dynamic Eden size */
		UDATA edenRegions = _idealEdenRegionCount;

		/* TODO:  This kick-off logic needs to be adapted to work with a dynamic mix of copy-forward and compact PGC increments.  For now, use the cycle state flags since they at least will let us test both code paths here. */
		if (env->_cycleState->_shouldRunCopyForward) {

			/* Calculate the number of regions that we need for copy forward destination */
			double survivorRegions = _averageSurvivorSetRegionCount;
			/* if _extensions->fvtest_forceCopyForwardHybridRatio is set(testing purpose), correct required survivor region count to avoid underestimating the remaining. */
			if ((0 != _extensions->fvtest_forceCopyForwardHybridRatio) && (100 >= _extensions->fvtest_forceCopyForwardHybridRatio)) {
				survivorRegions = survivorRegions * (100 - _extensions->fvtest_forceCopyForwardHybridRatio) / 100;
			}
			Trc_MM_SchedulingDelegate_estimatePartialGCsRemaining_survivorNeeds(env->getLanguageVMThread(), (UDATA)_averageSurvivorSetRegionCount, MM_GCExtensions::getExtensions(env)->tarokKickoffHeadroomInBytes, (UDATA)survivorRegions);

			double freeRegions = (double)((MM_GlobalAllocationManagerTarok *)_extensions->globalAllocationManager)->getFreeRegionCount();

			/* _previousReclaimableRegions has just been calculated (it's still current). It's a total (including free regions) */
			Assert_MM_true(_previousDefragmentReclaimableRegions >= freeRegions);
			double recoverableRegions = (double)_previousDefragmentReclaimableRegions - freeRegions;

			/* Copy PGC has compact selection goal work drive, so it optimistically relies on our projected compact work to indeed recover all reclaimable regions */
			if ((freeRegions + recoverableRegions) > (edenRegions + survivorRegions)) {
				partialCollectsRemaining = (UDATA)((freeRegions + recoverableRegions - edenRegions - survivorRegions) / _regionConsumptionRate);
			} else {
				partialCollectsRemaining = 0;
			}
		} else {
			/* MarkSweepCompact PGC has compact selection driven by free region goal, so it counts on reclaimable regions */
			if (_previousDefragmentReclaimableRegions > edenRegions) {
				partialCollectsRemaining = (UDATA)((double)(_previousDefragmentReclaimableRegions - edenRegions) / _regionConsumptionRate);
			} else {
				partialCollectsRemaining = 0;
			}
		}
	}

	Trc_MM_SchedulingDelegate_estimatePartialGCsRemaining_Exit(env->getLanguageVMThread(), partialCollectsRemaining);
	
	return partialCollectsRemaining;
}

void
MM_SchedulingDelegate::calculateHeapOccupancyTrend(MM_EnvironmentVLHGC *env)
{
	_previousLiveSetBytesAfterGlobalSweep = _liveSetBytesAfterGlobalSweep;
	_liveSetBytesAfterGlobalSweep = _liveSetBytesAfterPartialCollect;
	Trc_MM_SchedulingDelegate_calculateHeapOccupancyTrend_liveSetBytes(env->getLanguageVMThread(), _previousLiveSetBytesAfterGlobalSweep, _liveSetBytesBeforeGlobalSweep, _liveSetBytesAfterGlobalSweep);

	Assert_MM_true(_liveSetBytesAfterGlobalSweep <= _liveSetBytesAfterGlobalSweep);

	_heapOccupancyTrend = 1.0;
	if (0 != (_liveSetBytesBeforeGlobalSweep - _previousLiveSetBytesAfterGlobalSweep)) {
		_heapOccupancyTrend = ((double)_liveSetBytesAfterGlobalSweep - (double)_previousLiveSetBytesAfterGlobalSweep)
										/ ((double)_liveSetBytesBeforeGlobalSweep - (double)_previousLiveSetBytesAfterGlobalSweep);
	}
	
	Trc_MM_SchedulingDelegate_calculateHeapOccupancyTrend_heapOccupancy(env->getLanguageVMThread(), _heapOccupancyTrend);
}

void
MM_SchedulingDelegate::calculateScannableBytesRatio(MM_EnvironmentVLHGC *env)
{
	UDATA scannableBytes = 0;
	UDATA nonScannableBytes = 0;

	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
			scannableBytes += memoryPool->getScannableBytes();
			nonScannableBytes += memoryPool->getNonScannableBytes();
		}
	}

	if (0 == (scannableBytes + nonScannableBytes)) {
		/* assume all is scannable */
		_scannableBytesRatio = 1.0;
	} else {
		_scannableBytesRatio = (double)scannableBytes / (double)(scannableBytes + nonScannableBytes);
	}
}

void
MM_SchedulingDelegate::recalculateRatesOnFirstPGCAfterGMP(MM_EnvironmentVLHGC *env)
{
	if (isFirstPGCAfterGMP()) {
		calculatePGCCompactionRate(env, getCurrentEdenSizeInRegions(env) * _regionManager->getRegionSize());
		calculateHeapOccupancyTrend(env);
		calculateScannableBytesRatio(env);

		firstPGCAfterGMPCompleted();
	}
}

double
MM_SchedulingDelegate::getAverageEmptinessOfCopyForwardedRegions()
{
	return ((_averageCopyForwardBytesCopied + _averageCopyForwardBytesDiscarded) > 0.0)
			? (_averageCopyForwardBytesDiscarded / (_averageCopyForwardBytesCopied + _averageCopyForwardBytesDiscarded))
			: 0.0;
}

double
MM_SchedulingDelegate::getDefragmentEmptinessThreshold(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions * extensions = MM_GCExtensions::getExtensions(env);
	double averageEmptinessofCopyForwardedRegions = getAverageEmptinessOfCopyForwardedRegions();
	double defragmentEmptinessThreshold = 0.0;

	if (extensions->tarokAutomaticDefragmentEmptinessThreshold) {
		defragmentEmptinessThreshold = OMR_MAX(_automaticDefragmentEmptinessThreshold, averageEmptinessofCopyForwardedRegions);
	} else {
		if (0.0 != _extensions->tarokDefragmentEmptinessThreshold) {
			/* Was set on the command line */
			defragmentEmptinessThreshold = _extensions->tarokDefragmentEmptinessThreshold;
		} else {
			defragmentEmptinessThreshold = averageEmptinessofCopyForwardedRegions;
		}
	}

	return defragmentEmptinessThreshold;

}

UDATA
MM_SchedulingDelegate::estimateTotalFreeMemory(MM_EnvironmentVLHGC *env, UDATA freeRegionMemory, UDATA defragmentedMemory, UDATA reservedFreeMemory)
{
	UDATA estimatedFreeMemory = 0;

	/* Adjust estimatedFreeMemory - we are only interested in area that shortfall can be fed from.
	 * Thus exclude reservedFreeMemory(Eden and Survivor size).
	 */
	estimatedFreeMemory = MM_Math::saturatingSubtract(defragmentedMemory + freeRegionMemory, reservedFreeMemory);

	Trc_MM_SchedulingDelegate_estimateTotalFreeMemory(env->getLanguageVMThread(), estimatedFreeMemory, reservedFreeMemory, defragmentedMemory, freeRegionMemory);
	return estimatedFreeMemory;
}

UDATA
MM_SchedulingDelegate::calculateKickoffHeadroom(MM_EnvironmentVLHGC *env, UDATA totalFreeMemory)
{
	if (_extensions->tarokForceKickoffHeadroomInBytes) {
		return _extensions->tarokKickoffHeadroomInBytes;
	}
	UDATA newHeadroom = totalFreeMemory * _extensions->tarokKickoffHeadroomRegionRate / 100;
	Trc_MM_SchedulingDelegate_calculateKickoffHeadroom(env->getLanguageVMThread(), _extensions->tarokKickoffHeadroomInBytes, newHeadroom);
	_extensions->tarokKickoffHeadroomInBytes = newHeadroom;
	return newHeadroom;
}

UDATA
MM_SchedulingDelegate::initializeKickoffHeadroom(MM_EnvironmentVLHGC *env)
{
	/* total free memory = total heap size - eden size */
	UDATA totalFreeMemory = _regionManager->getTotalHeapSize() - getCurrentEdenSizeInBytes(env);
	return calculateKickoffHeadroom(env, totalFreeMemory);
}

void
MM_SchedulingDelegate::calculatePGCCompactionRate(MM_EnvironmentVLHGC *env, UDATA edenSizeInBytes)
{
	/* Ideally, copy-forwarded regions should be 100% full (i.e. 0% empty), but there are inefficiencies due to parallelism and compact groups.
	 * We measure this so that we can detect regions which are unlikely to become less empty if we copy-and-forward them.
	 */
	const double defragmentEmptinessThreshold = getDefragmentEmptinessThreshold(env);
	Assert_MM_true( (defragmentEmptinessThreshold >= 0.0) && (defragmentEmptinessThreshold <= 1.0) );
	const UDATA regionSize = _regionManager->getRegionSize();

	UDATA totalLiveDataInCollectableRegions = 0;
	UDATA totalLiveDataInNonCollectibleRegions = 0;
	UDATA fullyCompactedData = 0;

	UDATA freeMemoryInCollectibleRegions = 0;
	UDATA freeMemoryInNonCollectibleRegions = 0;
	UDATA freeMemoryInFullyCompactedRegions = 0;
	UDATA freeRegionMemory = 0;

	UDATA collectibleRegions = 0;
	UDATA nonCollectibleRegions = 0;
	UDATA freeRegions = 0;
	UDATA fullyCompactedRegions = 0;

	UDATA estimatedFreeMemory = 0;
	UDATA defragmentedMemory = 0;

	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::MANAGED);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		region->_defragmentationTarget = false;
		MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
		if (region->containsObjects()) {
			Assert_MM_true(region->_sweepData._alreadySwept);
			UDATA freeMemory = memoryPool->getFreeMemoryAndDarkMatterBytes();
			if (!region->getRememberedSetCardList()->isAccurate()) {
				/* Overflowed regions or those that RSCL is being rebuilt will not be be compacted */
				nonCollectibleRegions += 1;
				freeMemoryInNonCollectibleRegions += freeMemory;
				totalLiveDataInNonCollectibleRegions += (regionSize - freeMemory);
			} else {
				double emptiness = (double)freeMemory / (double)regionSize;
				Assert_MM_true( (emptiness >= 0.0) && (emptiness <= 1.0) );

				/* Only consider regions which are likely to become more dense if we copy-and-forward them */
				if (emptiness > defragmentEmptinessThreshold) {
					collectibleRegions += 1;
					freeMemoryInCollectibleRegions += freeMemory;
					/* see ReclaimDelegate::deriveCompactScore() for an explanation of potentialWastedWork */
					UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
					double weightedSurvivalRate = MM_GCExtensions::getExtensions(env)->compactGroupPersistentStats[compactGroup]._weightedSurvivalRate;
					double potentialWastedWork = (1.0 - weightedSurvivalRate) * (1.0 - emptiness);

					/* the probability that we'll recover the free memory is determined by the potential gainful work, so use that determine how much memory we're likely to actually compact */
					defragmentedMemory += (UDATA)((double)freeMemory * (1.0 - potentialWastedWork));
					totalLiveDataInCollectableRegions += (UDATA)((double)(regionSize - freeMemory) * (1.0 - potentialWastedWork));
					region->_defragmentationTarget = true;

				} else {
					/* if method calculatePGCCompactionRate() is called right after the sweep before PGC(the first PGC after GMP), half of Eden regions were allocated after the final GMP, those Eden regions didn't have been marked, they would be showed as fullyCompacted regions */
					fullyCompactedRegions += 1;
					freeMemoryInFullyCompactedRegions += freeMemory;
					fullyCompactedData += (regionSize - freeMemory);
				}
			}
		} else if (region->isFreeOrIdle()) {
			freeRegions += 1;
			freeRegionMemory += regionSize;
		}
	}

	/* Survivor space needs to accommodate for Nursery set, Dynamic collection set and Compaction set
	 */
	/* estimate totalFreeMemory for recalculating kickoffHeadroomRegionCount */	 
	UDATA surivivorSize = (UDATA)(regionSize * _averageSurvivorSetRegionCount);
	UDATA reservedFreeMemory = edenSizeInBytes + surivivorSize;
	estimatedFreeMemory = estimateTotalFreeMemory(env, freeRegionMemory, defragmentedMemory, reservedFreeMemory);
	calculateKickoffHeadroom(env, estimatedFreeMemory);

	/* estimate totalFreeMemory for recalculating PGCCompactionRate with tarokKickoffHeadroomInBytes */
	reservedFreeMemory += _extensions->tarokKickoffHeadroomInBytes;
	estimatedFreeMemory = estimateTotalFreeMemory(env, freeRegionMemory, defragmentedMemory, reservedFreeMemory);

	double bytesDiscardedPerByteCopied = (_averageCopyForwardBytesCopied > 0.0) ? (_averageCopyForwardBytesDiscarded / _averageCopyForwardBytesCopied) : 0.0;
	double estimatedFreeMemoryDiscarded = (double)totalLiveDataInCollectableRegions * bytesDiscardedPerByteCopied;
	double recoverableFreeMemory = (double)estimatedFreeMemory - estimatedFreeMemoryDiscarded;

	if (0.0 < recoverableFreeMemory) {
		_bytesCompactedToFreeBytesRatio = ((double)totalLiveDataInCollectableRegions)/recoverableFreeMemory;
	} else {
		_bytesCompactedToFreeBytesRatio = (double)(_regionManager->getTableRegionCount() + 1);
	}

	Trc_MM_SchedulingDelegate_calculatePGCCompactionRate_liveToFreeRatio1(env->getLanguageVMThread(), (totalLiveDataInCollectableRegions + totalLiveDataInNonCollectibleRegions + fullyCompactedData), totalLiveDataInCollectableRegions, totalLiveDataInNonCollectibleRegions, fullyCompactedData);
	Trc_MM_SchedulingDelegate_calculatePGCCompactionRate_liveToFreeRatio2(env->getLanguageVMThread(), (freeMemoryInCollectibleRegions + freeMemoryInNonCollectibleRegions + freeRegionMemory), freeMemoryInCollectibleRegions, freeMemoryInNonCollectibleRegions, freeRegionMemory, freeMemoryInFullyCompactedRegions);
	Trc_MM_SchedulingDelegate_calculatePGCCompactionRate_liveToFreeRatio3(env->getLanguageVMThread(), (collectibleRegions + nonCollectibleRegions + fullyCompactedRegions + freeRegions), collectibleRegions, nonCollectibleRegions, fullyCompactedRegions, freeRegions);
	Trc_MM_SchedulingDelegate_calculatePGCCompactionRate_liveToFreeRatio4(env->getLanguageVMThread(), _bytesCompactedToFreeBytesRatio, edenSizeInBytes, surivivorSize, reservedFreeMemory, defragmentEmptinessThreshold, defragmentedMemory, estimatedFreeMemory);
}

UDATA
MM_SchedulingDelegate::getDesiredCompactWork()
{
	/* compact work (mostly) driven by M/S from GMP */
	UDATA desiredCompactWork = (UDATA)(_bytesCompactedToFreeBytesRatio * OMR_MAX(0.0, _regionConsumptionRate) * _regionManager->getRegionSize());
	
	/* defragmentation work (mostly) driven by compact group merging (maxAge - 1 into maxAge) */
	desiredCompactWork += (UDATA)_averageMacroDefragmentationWork;

	return desiredCompactWork;
}

bool
MM_SchedulingDelegate::isFirstPGCAfterGMP()
{
	return _didGMPCompleteSinceLastReclaim;
}

void
MM_SchedulingDelegate::firstPGCAfterGMPCompleted()
{
	_didGMPCompleteSinceLastReclaim = false;
}

void
MM_SchedulingDelegate::copyForwardCompleted(MM_EnvironmentVLHGC *env)
{
	MM_CopyForwardStats * copyForwardStats = &(static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats);
	UDATA bytesCopied = copyForwardStats->_copyBytesTotal;
	UDATA bytesDiscarded = copyForwardStats->_copyDiscardBytesTotal;
	UDATA bytesScanned = copyForwardStats->_scanBytesTotal;
	UDATA bytesCompacted = copyForwardStats->_externalCompactBytes;
	UDATA regionSize = _regionManager->getRegionSize();
	double copyForwardRate = calculateAverageCopyForwardRate(env);
	
	const double historicWeight = 0.70; /* arbitrarily give 70% weight to historical result, 30% to newest result */
	_averageCopyForwardBytesCopied = (_averageCopyForwardBytesCopied * historicWeight) + ((double)bytesCopied * (1.0 - historicWeight));
	_averageCopyForwardBytesDiscarded = (_averageCopyForwardBytesDiscarded * historicWeight) + ((double)bytesDiscarded * (1.0 - historicWeight));

	/* calculate the number of additional regions which would have been required to complete the copy-forward without aborting */
	UDATA failedEvacuateRegionCount = (bytesScanned + regionSize - 1) / regionSize;
	UDATA compactSetSurvivorRegionCount = (bytesCompacted + regionSize - 1) / regionSize;
	UDATA survivorSetRegionCount = env->_cycleState->_pgcData._survivorSetRegionCount + failedEvacuateRegionCount + compactSetSurvivorRegionCount;
	
	_averageSurvivorSetRegionCount = (_averageSurvivorSetRegionCount * historicWeight) + ((double)survivorSetRegionCount * (1.0 - historicWeight));
	_averageCopyForwardRate = (_averageCopyForwardRate * historicWeight) + (copyForwardRate * (1.0 - historicWeight));

	Trc_MM_SchedulingDelegate_copyForwardCompleted_efficiency(
		env->getLanguageVMThread(),
		bytesCopied,
		bytesDiscarded,
		(double)bytesDiscarded / (double)(bytesCopied + bytesDiscarded),
		_averageCopyForwardBytesCopied,
		_averageCopyForwardBytesDiscarded,
		_averageCopyForwardBytesDiscarded / (_averageCopyForwardBytesCopied + _averageCopyForwardBytesDiscarded),
		survivorSetRegionCount,
		failedEvacuateRegionCount,
		compactSetSurvivorRegionCount,
		_averageSurvivorSetRegionCount,
		copyForwardRate,
		_averageCopyForwardRate
		);
}

double
MM_SchedulingDelegate::calculateAverageCopyForwardRate(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CopyForwardStats * copyForwardStats = &(static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats);
	UDATA bytesCopied = copyForwardStats->_copyBytesTotal;
	U_64 timeSpentReferenceClearing = static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._irrsStats._clearFromRegionReferencesTimesus;
	U_64 timeSpentInCopyForward = j9time_hires_delta(copyForwardStats->_startTime, copyForwardStats->_endTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);

	double copyForwardRate = 0.0;
	if (timeSpentInCopyForward > timeSpentReferenceClearing) {
		/* theoretically could happen with clock skew */
		copyForwardRate = (double)bytesCopied / ((double)(timeSpentInCopyForward - timeSpentReferenceClearing));
	} else {
		if (0 != timeSpentInCopyForward) {
			/* Ignore time spent in clearing then */
			copyForwardRate = (double)bytesCopied / ((double)timeSpentInCopyForward);
		} else {
			/* Since timeSpentInCopyForward seems to be less than a microsecond, just use the number of bytes we copied as an underestimate */
			copyForwardRate = (double)bytesCopied;
		}
	}

	return copyForwardRate;
}

void
MM_SchedulingDelegate::calculateAutomaticGMPIntermission(MM_EnvironmentVLHGC *env)
{
	Trc_MM_SchedulingDelegate_calculateAutomaticGMPIntermission_Entry(env->getLanguageVMThread(), _extensions->tarokAutomaticGMPIntermission ? "true" : "false", _remainingGMPIntermissionIntervals);
	
	/* call these even if automatic intermissions aren't enabled, so that we get the trace data. This is useful for debugging */
	UDATA partialCollectsRemaining = estimatePartialGCsRemaining(env);
	updateLiveBytesAfterPartialCollect();
	
	if (_extensions->tarokAutomaticGMPIntermission) {
		/* we assume that the default value is MAX when automatic intermissions are enabled */
		Assert_MM_true(UDATA_MAX == _extensions->tarokGMPIntermission);
		
		/* if we haven't kicked off yet, recalculate the intermission until kick-off based on current estimates */
		if (_remainingGMPIntermissionIntervals > 0) {
			double liveSetAdjustedForScannableBytesRatio = calculateEstimatedGlobalBytesToScan();
			UDATA incrementHeadroom = calculateGlobalMarkIncrementHeadroom(env);
			UDATA globalMarkIncrementsRequired = estimateGlobalMarkIncrements(env, liveSetAdjustedForScannableBytesRatio);
			UDATA globalMarkIncrementsRequiredWithHeadroom = globalMarkIncrementsRequired + incrementHeadroom;
			UDATA globalMarkIncrementsRemaining = partialCollectsRemaining * _extensions->tarokPGCtoGMPDenominator / _extensions->tarokPGCtoGMPNumerator;
			_remainingGMPIntermissionIntervals = MM_Math::saturatingSubtract(globalMarkIncrementsRemaining, globalMarkIncrementsRequiredWithHeadroom);
		}
	}

	Trc_MM_SchedulingDelegate_calculateAutomaticGMPIntermission_1_Exit(env->getLanguageVMThread(), _remainingGMPIntermissionIntervals, _extensions->tarokKickoffHeadroomInBytes);
}

void
MM_SchedulingDelegate::updateSurvivalRatesAfterCopyForward(double thisEdenSurvivalRate, UDATA thisNonEdenSurvivorCount)
{
	/* Note that this weight value is currently arbitrary */
	double historicalWeight = 0.5;
	double newWeight = 1.0 - historicalWeight;
	_edenSurvivalRateCopyForward =  (historicalWeight * _edenSurvivalRateCopyForward) + (newWeight * thisEdenSurvivalRate);
	_nonEdenSurvivalCountCopyForward = (UDATA)((historicalWeight * _nonEdenSurvivalCountCopyForward) + (newWeight * thisNonEdenSurvivorCount));
}

void 
MM_SchedulingDelegate::calculateEdenSize(MM_EnvironmentVLHGC *env)
{
	UDATA regionSize = _regionManager->getRegionSize();
	UDATA previousEdenSize = _edenRegionCount * regionSize;
	Trc_MM_SchedulingDelegate_calculateEdenSize_Entry(env->getLanguageVMThread(), previousEdenSize);
	
	MM_GlobalAllocationManagerTarok *globalAllocationManager = (MM_GlobalAllocationManagerTarok *)_extensions->globalAllocationManager;
	UDATA freeRegions = globalAllocationManager->getFreeRegionCount();

	UDATA edenMinimumCount = _minimumEdenRegionCount;
	UDATA edenMaximumCount = _idealEdenRegionCount;
	Assert_MM_true(edenMinimumCount >= 1);
	Assert_MM_true(edenMaximumCount >= 1);
	Assert_MM_true(edenMaximumCount >= edenMinimumCount);
	
	UDATA desiredEdenCount = freeRegions;
	if (desiredEdenCount > edenMaximumCount) {
		desiredEdenCount = edenMaximumCount;
	} else if (desiredEdenCount < edenMinimumCount) {
		desiredEdenCount = edenMinimumCount;
	}
	Trc_MM_SchedulingDelegate_calculateEdenSize_dynamic(env->getLanguageVMThread(), desiredEdenCount, _edenSurvivalRateCopyForward, _nonEdenSurvivalCountCopyForward, freeRegions, edenMinimumCount, edenMaximumCount);
	if (desiredEdenCount <= freeRegions) {
		_edenRegionCount = desiredEdenCount;
	} else {
		/* there isn't enough memory left for a desired Eden. Allow Eden to shrink to free size(could be less than minimum size or 0) before
		 * triggering an allocation failure collection (i.e. a global STW collect)
		 */ 
		_edenRegionCount = freeRegions;
		Trc_MM_SchedulingDelegate_calculateEdenSize_reduceToFreeBytes(env->getLanguageVMThread(), desiredEdenCount, _edenRegionCount);
	}
	Trc_MM_SchedulingDelegate_calculateEdenSize_Exit(env->getLanguageVMThread(), (_edenRegionCount * regionSize));
}

UDATA
MM_SchedulingDelegate::currentGlobalMarkIncrementTimeMillis(MM_EnvironmentVLHGC *env) const
{
	UDATA markIncrementMillis = 0;
	
	if (0 == _extensions->tarokGlobalMarkIncrementTimeMillis) {
		UDATA partialCollectsRemaining = estimatePartialGCsRemaining(env);

		if (0 == partialCollectsRemaining) {
			/* We're going to AF very soon so we need to finish the GMP this increment.  Set current global mark increment time to max */
			markIncrementMillis = UDATA_MAX;
		} else {
			UDATA desiredGlobalMarkIncrementMillis = _dynamicGlobalMarkIncrementTimeMillis;
			double remainingMillisToScan = estimateRemainingTimeMillisToScan();
			UDATA minimumGlobalMarkIncrementMillis = (UDATA) (remainingMillisToScan / (double)partialCollectsRemaining);

			markIncrementMillis = OMR_MAX(desiredGlobalMarkIncrementMillis, minimumGlobalMarkIncrementMillis);
		}
	} else {
		markIncrementMillis = _extensions->tarokGlobalMarkIncrementTimeMillis;
	}
	Trc_MM_SchedulingDelegate_currentGlobalMarkIncrementTimeMillis_summary(env->getLanguageVMThread(), markIncrementMillis);
	
	return markIncrementMillis;
}

UDATA
MM_SchedulingDelegate::getCurrentEdenSizeInBytes(MM_EnvironmentVLHGC *env)
{
	return (_edenRegionCount * _regionManager->getRegionSize());
}

UDATA
MM_SchedulingDelegate::getCurrentEdenSizeInRegions(MM_EnvironmentVLHGC *env)
{
	return _edenRegionCount;
}

void
MM_SchedulingDelegate::heapReconfigured(MM_EnvironmentVLHGC *env)
{
	UDATA edenMaximumBytes = _extensions->tarokIdealEdenMaximumBytes;
	UDATA edenMinimumBytes = _extensions->tarokIdealEdenMinimumBytes;
	Trc_MM_SchedulingDelegate_heapReconfigured_Entry(env->getLanguageVMThread(), edenMaximumBytes, edenMinimumBytes);
	
	UDATA regionSize = _regionManager->getRegionSize();
	UDATA heapRegions = 0;
	
	/* walk the managed regions (skipping cold area) to determine how large the managed heap is */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::MANAGED);
	while (NULL != regionIterator.nextRegion()) {
		heapRegions += 1;
	}
	UDATA currentHeapSize = heapRegions * regionSize;
	/* since the heap is allowed to be one region less than the size requested (due to "acceptLess" in Virtual Memory), make sure that we consider the "reachable minimum" to be the real minimum heap size */
	UDATA minimumHeap = OMR_MIN(_extensions->initialMemorySize, currentHeapSize);
	UDATA edenIdealBytes = 0;
	UDATA maximumHeap = _extensions->memoryMax;
	if (currentHeapSize == maximumHeap) {
		/* we are fully expanded or mx == ms so just return the maximum ideal eden */
		edenIdealBytes = edenMaximumBytes;
	} else {
		/* interpolate between the maximum and minimum */
		/* This logic follows the formula given in JAZZ 39694
		 * for:  -XmsA -XmxB -XmnsC -XmnxD, "current heap size" W, "current Eden size" Z:
		 * Z := C + ((W-A)/(B-A))(D-C)
		 */
		UDATA heapBytesOverMinimum = currentHeapSize - minimumHeap;
		UDATA maximumHeapVariation = maximumHeap - minimumHeap;
		/* if this is 0, we should have taken the first half of this if */
		Assert_MM_true(0 != maximumHeapVariation);
		double ratioOfHeapExpanded = ((double)heapBytesOverMinimum) / ((double)maximumHeapVariation);
		UDATA maximumEdenVariation = edenMaximumBytes - edenMinimumBytes;
		UDATA edenLinearScale = (UDATA)(ratioOfHeapExpanded * (double)maximumEdenVariation);
		edenIdealBytes = edenMinimumBytes + edenLinearScale;
	}

	_idealEdenRegionCount = (edenIdealBytes + regionSize - 1) / regionSize;
	Assert_MM_true(_idealEdenRegionCount > 0);
	_minimumEdenRegionCount = OMR_MIN(_idealEdenRegionCount, ((MM_GlobalAllocationManagerTarok *)_extensions->globalAllocationManager)->getManagedAllocationContextCount());
	Assert_MM_true(_minimumEdenRegionCount > 0);

	Trc_MM_SchedulingDelegate_heapReconfigured_Exit(env->getLanguageVMThread(), heapRegions, _idealEdenRegionCount, _minimumEdenRegionCount);
	Assert_MM_true(_idealEdenRegionCount >= _minimumEdenRegionCount);
	
	/* recalculate Eden Size after resize heap */
	calculateEdenSize(env);
}

UDATA
MM_SchedulingDelegate::calculateGlobalMarkIncrementHeadroom(MM_EnvironmentVLHGC *env) const
{
	UDATA headroomIncrements = 0;

	if (_regionConsumptionRate > 0.0) {
		double headroomRegions = (double) _extensions->tarokKickoffHeadroomInBytes / _regionManager->getRegionSize();
		double headroomPartialGCs = headroomRegions / _regionConsumptionRate;
		double headroomGlobalMarkIncrements = headroomPartialGCs * (double)_extensions->tarokPGCtoGMPDenominator / (double)_extensions->tarokPGCtoGMPNumerator;
		headroomIncrements = (UDATA) ceil(headroomGlobalMarkIncrements);
	}
	return headroomIncrements;
}

UDATA
MM_SchedulingDelegate::estimateRemainingGlobalBytesToScan() const
{
	UDATA expectedGlobalBytesToScan = (UDATA) calculateEstimatedGlobalBytesToScan();
	UDATA globalBytesScanned = ((MM_IncrementalGenerationalGC *)_extensions->getGlobalCollector())->getBytesScannedInGlobalMarkPhase();
	UDATA remainingGlobalBytesToScan = MM_Math::saturatingSubtract(expectedGlobalBytesToScan, globalBytesScanned);

	return remainingGlobalBytesToScan;
}

double
MM_SchedulingDelegate::estimateRemainingTimeMillisToScan() const
{
	Assert_MM_true(0 != _extensions->gcThreadCount);

	double remainingBytesToScan = (double) estimateRemainingGlobalBytesToScan();
	double estimatedScanMillis = remainingBytesToScan * _scanRateStats.microSecondsPerByteScanned / ((double)_extensions->gcThreadCount) / 1000.0;

	return estimatedScanMillis;
}

void
MM_SchedulingDelegate::updateGMPStats(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	/* We should have just finished the last GMP increment, so persistentGlobalMarkPhaseState should contain
	 * information for the whole GMP cycle.
	 */

	MM_CycleStateVLHGC * persistentGMPState = ((MM_IncrementalGenerationalGC *)_extensions->getGlobalCollector())->getPersistentGlobalMarkPhaseState();
	Assert_MM_true(MM_CycleState::CT_GLOBAL_MARK_PHASE == persistentGMPState->_collectionType);
	Assert_MM_true(0 != _extensions->gcThreadCount);

	MM_MarkVLHGCStats * incrementalMarkStats = &(persistentGMPState->_vlhgcCycleStats._incrementalMarkStats);
	MM_MarkVLHGCStats * concurrentMarkStats = &(persistentGMPState->_vlhgcCycleStats._concurrentMarkStats);

	U_64 incrementalScanTime = (U_64) (((double) j9time_hires_delta(0, incrementalMarkStats->getScanTime(), J9PORT_TIME_DELTA_IN_MICROSECONDS)) / ((double) _extensions->gcThreadCount));
	UDATA concurrentBytesScanned = concurrentMarkStats->_bytesScanned;

	_historicTotalIncrementalScanTimePerGMP = (U_64) ((_historicTotalIncrementalScanTimePerGMP * incrementalScanTimePerGMPHistoricWeight) + (incrementalScanTime * (1 - incrementalScanTimePerGMPHistoricWeight)));
	_historicBytesScannedConcurrentlyPerGMP = (UDATA) ((_historicBytesScannedConcurrentlyPerGMP * bytesScannedConcurrentlyPerGMPHistoricWeight) + (concurrentBytesScanned * (1 - bytesScannedConcurrentlyPerGMPHistoricWeight)));

	Trc_MM_SchedulingDelegate_updateGMPStats(env->getLanguageVMThread(), _historicTotalIncrementalScanTimePerGMP, incrementalScanTime, _historicBytesScannedConcurrentlyPerGMP, concurrentBytesScanned);
}

U_64
MM_SchedulingDelegate::getScanTimeCostPerGMP(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions * extensions = MM_GCExtensions::getExtensions(env);
	double incrementalCost = (double)_historicTotalIncrementalScanTimePerGMP;
	double concurrentCost = 0.0;
	double scanRate = _scanRateStats.microSecondsPerByteScanned / (double)extensions->gcThreadCount;
	
	if (scanRate > 0.0) {
		concurrentCost = extensions->tarokConcurrentMarkingCostWeight * ((double)_historicBytesScannedConcurrentlyPerGMP * scanRate );
	}

	return (U_64) (incrementalCost + concurrentCost);
}

