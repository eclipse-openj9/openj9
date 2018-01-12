
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

/**
 * @file
 * @ingroup GC_Modron_Tarok
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"
#include "gcutils.h"
#include "ModronAssertions.h"

#include "AllocationContextTarok.hpp"
#include "CollectionSetDelegate.hpp"
#include "CompactGroupManager.hpp"
#include "CompactGroupPersistentStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GlobalAllocationManagerTarok.hpp"
#include "MemorySubSpace.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "HeapRegionManagerTarok.hpp"
#include "MarkMap.hpp"
#include "MemoryPoolBumpPointer.hpp"
#include "RegionValidator.hpp"

MM_CollectionSetDelegate::MM_CollectionSetDelegate(MM_EnvironmentBase *env, MM_HeapRegionManager *manager)
	: MM_BaseNonVirtual()
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _regionManager(manager)
	, _setSelectionDataTable(NULL)
	, _dynamicSelectionList(NULL)
{
	_typeId = __FUNCTION__;
}

bool
MM_CollectionSetDelegate::initialize(MM_EnvironmentVLHGC *env)
{
	if(_extensions->tarokEnableDynamicCollectionSetSelection) {
		UDATA setSelectionEntryCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
		UDATA tableAllocationSizeInBytes = sizeof(SetSelectionData) * setSelectionEntryCount;
		_setSelectionDataTable = (SetSelectionData *)env->getForge()->allocate(tableAllocationSizeInBytes, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
		if(NULL == _setSelectionDataTable) {
			goto error_no_memory;
		}
		memset(_setSelectionDataTable, 0, tableAllocationSizeInBytes);
		for(UDATA index = 0; index < setSelectionEntryCount; index++) {
			_setSelectionDataTable[index]._compactGroup = index;
		}

		/* Publish table for TGC purposes */
		_extensions->tarokTgcSetSelectionDataTable = (void *)_setSelectionDataTable;

		_dynamicSelectionList = (SetSelectionData **)env->getForge()->allocate(sizeof(SetSelectionData *) * setSelectionEntryCount, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
		if(NULL == _dynamicSelectionList) {
			goto error_no_memory;
		}
	}

	return true;

error_no_memory:
	return false;
}

void
MM_CollectionSetDelegate::tearDown(MM_EnvironmentVLHGC *env)
{
	if(NULL != _setSelectionDataTable) {
		env->getForge()->free(_setSelectionDataTable);
		_setSelectionDataTable = NULL;
	}

	if(NULL != _dynamicSelectionList) {
		env->getForge()->free(_dynamicSelectionList);
		_dynamicSelectionList = NULL;
	}
}

/**
 * Helper function used by J9_SORT to sort rate of return element lists.
 */
static int
compareRateOfReturnScoreFunc(const void *element1, const void *element2)
{
	MM_CollectionSetDelegate::SetSelectionData *ror1 = *(MM_CollectionSetDelegate::SetSelectionData **)element1;
	MM_CollectionSetDelegate::SetSelectionData *ror2 = *(MM_CollectionSetDelegate::SetSelectionData **)element2;

	/* We actually do the checks instead of simple math (w/ multiplier) to catch slight differences.  Perhaps if we multiply by 1000 then the differences are
	 * so minor it doesn't matter?
	 */
	if(ror1->_rateOfReturn == ror2->_rateOfReturn) {
		return 0;
	} else if(ror1->_rateOfReturn < ror2->_rateOfReturn ) {
		return 1;
	} else {
		return -1;
	}
}

/**
 * Helper function used by J9_SORT to sort core sample element lists.
 */
static int
compareCoreSampleScoreFunc(const void *element1, const void *element2)
{
	MM_CollectionSetDelegate::SetSelectionData *coreSample1 = *(MM_CollectionSetDelegate::SetSelectionData **)element1;
	MM_CollectionSetDelegate::SetSelectionData *coreSample2 = *(MM_CollectionSetDelegate::SetSelectionData **)element2;

	if(coreSample1->_regionCount == coreSample2->_regionCount) {
		return 0;
	} else if(coreSample1->_regionCount < coreSample2->_regionCount) {
		return 1;
	} else {
		return -1;
	}
}

UDATA
MM_CollectionSetDelegate::createNurseryCollectionSet(MM_EnvironmentVLHGC *env)
{
	bool dynamicCollectionSet = _extensions->tarokEnableDynamicCollectionSetSelection;
	Trc_MM_CollectionSetDelegate_createNurseryCollectionSet_Entry(env->getLanguageVMThread(), dynamicCollectionSet ? "true" : "false");
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	UDATA nurseryRegionCount = 0;

	/* Calculate the core "required" collection set for the Partial GC, which constitutes all regions that are equal to
	 * or below the nursery age.
	 */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::MANAGED);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		Assert_MM_true(MM_RegionValidator(region).validate(env));
		Assert_MM_false(region->_markData._shouldMark);
		Assert_MM_false(region->_reclaimData._shouldReclaim);
		if (region->containsObjects()) {
			bool regionHasCriticalRegions = (0 != region->_criticalRegionsInUse);
			bool isSelectionForCopyForward = env->_cycleState->_shouldRunCopyForward;

			if (region->getRememberedSetCardList()->isAccurate() && (!isSelectionForCopyForward || !regionHasCriticalRegions)) {
				if(MM_CompactGroupManager::isRegionInNursery(env, region)) {
					UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
					/* on collection phase, mark all non-overflowed regions and those that RSCL is not being rebuilt */
					/* sweep/compact flags are set in ReclaimDelegate */
					region->_markData._shouldMark = true;
					region->_reclaimData._shouldReclaim = true;
					region->_compactData._shouldCompact = false;
					/* Collected regions are no longer target for defragmentation until next GMP */
					region->_defragmentationTarget = false;
					_extensions->compactGroupPersistentStats[compactGroup]._regionsInRegionCollectionSetForPGC += 1;
					nurseryRegionCount += 1;
				} else {
					Assert_MM_true(!region->isEden());
				}
	
				/* Add the region to appropriate dynamic collection set data age group (building up information for later use) */
				if(dynamicCollectionSet) {
					UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
					_setSelectionDataTable[compactGroup].addRegion(region);
				}
			} else {
				Assert_MM_true(!region->isEden());
			}
		}
	}
	
	Trc_MM_CollectionSetDelegate_createNurseryCollectionSet_Exit(env->getLanguageVMThread(), nurseryRegionCount);
	return nurseryRegionCount;
}

UDATA
MM_CollectionSetDelegate::selectRegionsForBudget(MM_EnvironmentVLHGC *env, UDATA ageGroupBudget, SetSelectionData *setSelectionData)
{
	Trc_MM_CollectionSetDelegate_selectRegionsForBudget_Entry(env->getLanguageVMThread(), ageGroupBudget);
	UDATA ageGroupBudgetRemaining = ageGroupBudget;
	UDATA regionSize = _regionManager->getRegionSize();

	/* Walk the rate of return age group using a remainder skip-system to select the appropriate % of regions to collect (based on budget) */
	UDATA regionSelectionIndex = 0;
	UDATA regionSelectionIncrement = ageGroupBudget;
	UDATA regionSelectionThreshold = setSelectionData->_regionCount;
	MM_HeapRegionDescriptorVLHGC *regionSelectionPtr = setSelectionData->_regionList;
	while((0 != ageGroupBudgetRemaining) && (NULL != regionSelectionPtr)) {
		regionSelectionIndex += regionSelectionIncrement;
		if(regionSelectionIndex >= regionSelectionThreshold) {
			/* The region is to be selected as part of the dynamic set */
			regionSelectionPtr->_markData._shouldMark = true;
			regionSelectionPtr->_reclaimData._shouldReclaim = true;
			regionSelectionPtr->_compactData._shouldCompact = false;
			/* Collected regions are no longer target for defragmentation until the next GMP */
			regionSelectionPtr->_defragmentationTarget = false;
			ageGroupBudgetRemaining -= 1;

			UDATA tableIndex = _regionManager->mapDescriptorToRegionTableIndex(regionSelectionPtr);
			UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, regionSelectionPtr);
			MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)regionSelectionPtr->getMemoryPool();
			UDATA freeMemory = memoryPool->getFreeMemoryAndDarkMatterBytes();
			_extensions->compactGroupPersistentStats[compactGroup]._regionsInRegionCollectionSetForPGC += 1;

			Trc_MM_CollectionSetDelegate_selectRegionsForBudget(env->getLanguageVMThread(), tableIndex, compactGroup, (100 * freeMemory)/regionSize, (UDATA) 0, (UDATA) 0);
		}
		regionSelectionIndex %= regionSelectionThreshold;

		regionSelectionPtr = regionSelectionPtr->getDynamicSelectionNext();
	}

	Assert_MM_true(ageGroupBudgetRemaining <= ageGroupBudget);
	Trc_MM_CollectionSetDelegate_selectRegionsForBudget_Exit(env->getLanguageVMThread(), ageGroupBudget - ageGroupBudgetRemaining);
	return ageGroupBudgetRemaining;
}

void
MM_CollectionSetDelegate::createRateOfReturnCollectionSet(MM_EnvironmentVLHGC *env, UDATA nurseryRegionCount)
{
	/* Build and sort the rate of return list into budget consumption priority order */
	UDATA sortListSize = 0;
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	
	for (UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
		if (MM_CompactGroupManager::isCompactGroupDCSSCandidate(env, compactGroup)) {
			SetSelectionData *tableEntry = &_setSelectionDataTable[compactGroup];
			if ((0.0 != tableEntry->_rateOfReturn) && (0 != tableEntry->_regionCount)) {
				_dynamicSelectionList[sortListSize] =  tableEntry;
				sortListSize += 1;
			}
		}
	}

	J9_SORT(_dynamicSelectionList, sortListSize, sizeof(SetSelectionData *), compareRateOfReturnScoreFunc);

	/* Walk the ROR priority list and select regions based on remaining available budget */
	UDATA regionBudget = 0;
	if(0 != _extensions->tarokDynamicCollectionSetSelectionAbsoluteBudget) {
		regionBudget = _extensions->tarokDynamicCollectionSetSelectionAbsoluteBudget;
	} else {
		regionBudget = (UDATA)(nurseryRegionCount * _extensions->tarokDynamicCollectionSetSelectionPercentageBudget);
	}

	Trc_MM_CollectionSetDelegate_createRegionCollectionSetForPartialGC_dynamicRegionSelectionBudget(
		env->getLanguageVMThread(),
		nurseryRegionCount,
		regionBudget
	);

	UDATA rorIndex = 0;
	while((0 != regionBudget) && (rorIndex < sortListSize)) {
		SetSelectionData *rorEntry = _dynamicSelectionList[rorIndex];

		/* Determine the budget available for collection in the current compact group */
		UDATA compactGroupBudget = (UDATA)( ((double)regionBudget) * rorEntry->_rateOfReturn );
		Assert_MM_true(compactGroupBudget <= regionBudget);
		compactGroupBudget = OMR_MIN(compactGroupBudget, rorEntry->_regionCount);

		UDATA compactGroupBudgetRemaining = 0;

		if(compactGroupBudget > 0) {
			/* The age group is participating in dynamic set selection */
			rorEntry->_dynamicSelectionThisCycle = true;

			compactGroupBudgetRemaining = selectRegionsForBudget(env, compactGroupBudget, rorEntry);
		}

		/* Deduct the age group budget used from the overall budget */
		Assert_MM_true(compactGroupBudget >= compactGroupBudgetRemaining);
		UDATA budgetConsumed = compactGroupBudget - compactGroupBudgetRemaining;
		Assert_MM_true(regionBudget >= budgetConsumed);
		regionBudget -= budgetConsumed;

		Trc_MM_CollectionSetDelegate_createRegionCollectionSetForPartialGC_dynamicRegionSelection(
			env->getLanguageVMThread(),
			rorEntry->_compactGroup,
			rorEntry->_regionCount,
			1000.0 * rorEntry->_rateOfReturn,
			compactGroupBudget,
			budgetConsumed
		);

		/* Move to the next ROR entry */
		rorIndex += 1;
	}
	Trc_MM_CollectionSetDelegate_createRegionCollectionSetForPartialGC_dynamicRegionSelectionBudgetRemaining(
		env->getLanguageVMThread(),
		regionBudget
	);
}

void
MM_CollectionSetDelegate::createCoreSamplingCollectionSet(MM_EnvironmentVLHGC *env, UDATA nurseryRegionCount)
{
	/* Collect and sort all regions into the core sample buckets so that we can find them quickly (this is an optimization) */
	UDATA totalCoreSampleRegions = 0;
	UDATA sortListSize = 0;
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	
	for (UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
		if (MM_CompactGroupManager::isCompactGroupDCSSCandidate(env, compactGroup)) {
			SetSelectionData *tableEntry = &_setSelectionDataTable[compactGroup];
			if (!tableEntry->_dynamicSelectionThisCycle && (0 != tableEntry->_regionCount)) {
				/* The region is a collection candidate and eligible for dynamic selection - count it as part of the total possible core sampling group */
				totalCoreSampleRegions += tableEntry->_regionCount;
				_dynamicSelectionList[sortListSize] =  tableEntry;
				sortListSize += 1;
			}
		}
	}

	J9_SORT(_dynamicSelectionList, sortListSize, sizeof(SetSelectionData *), compareCoreSampleScoreFunc);

	/* Given a region budget, select regions for core sampling according to population and whether the age group has participated in other
	 * dynamically added activities.
	 */
	UDATA regionBudget = 0;
	if(0 != _extensions->tarokCoreSamplingAbsoluteBudget) {
		regionBudget = _extensions->tarokCoreSamplingAbsoluteBudget;
	} else {
		regionBudget = (UDATA)(nurseryRegionCount * _extensions->tarokCoreSamplingPercentageBudget);
	}

	Trc_MM_CollectionSetDelegate_createRegionCollectionSetForPartialGC_coreSamplingBudget(
		env->getLanguageVMThread(),
		totalCoreSampleRegions,
		regionBudget
	);

	UDATA coreSampleIndex = 0;
	while((regionBudget != 0) && (coreSampleIndex < sortListSize)) {
		SetSelectionData *coreSample = _dynamicSelectionList[coreSampleIndex];
		UDATA compactGroup = coreSample->_compactGroup;

		Assert_MM_true(!_setSelectionDataTable[compactGroup]._dynamicSelectionThisCycle);

		/* Determine the budget available for collection in the current compact group */
		Assert_MM_true(totalCoreSampleRegions > 0);
		UDATA compactGroupBudget = (UDATA) (((double)regionBudget) * ((double)coreSample->_regionCount) / ((double)totalCoreSampleRegions));
		Assert_MM_true(compactGroupBudget <= regionBudget);
		compactGroupBudget = OMR_MIN(compactGroupBudget, coreSample->_regionCount);
		/* Want at least one region out of this */
		compactGroupBudget = OMR_MAX(compactGroupBudget, 1);

		UDATA compactGroupBudgetRemaining = 0;

		if(compactGroupBudget > 0) {
			compactGroupBudgetRemaining = selectRegionsForBudget(env, compactGroupBudget, coreSample);
		}

		/* Deduct the compact group budget used from the overall budget */
		Assert_MM_true(compactGroupBudget >= compactGroupBudgetRemaining);
		UDATA budgetConsumed = compactGroupBudget - compactGroupBudgetRemaining;
		Assert_MM_true(regionBudget >= budgetConsumed);
		regionBudget -= budgetConsumed;

		Trc_MM_CollectionSetDelegate_createRegionCollectionSetForPartialGC_coreSamplingSelection(
			env->getLanguageVMThread(),
			compactGroup,
			coreSample->_regionCount,
			compactGroupBudget,
			budgetConsumed
		);

		coreSampleIndex += 1;
	}

	Trc_MM_CollectionSetDelegate_createRegionCollectionSetForPartialGC_coreSamplingBudgetRemaining(
		env->getLanguageVMThread(),
		regionBudget
	);
}


void
MM_CollectionSetDelegate::createRegionCollectionSetForPartialGC(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	bool dynamicCollectionSet = _extensions->tarokEnableDynamicCollectionSetSelection;

	/* If dynamic collection sets are enabled, reset all related data structures that are used for selection */
	if(dynamicCollectionSet) {
		MM_CompactGroupPersistentStats *persistentStats = _extensions->compactGroupPersistentStats;
		UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);

		for(UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
			Assert_MM_true(compactGroup == _setSelectionDataTable[compactGroup]._compactGroup);
			_setSelectionDataTable[compactGroup]._regionCount = 0;
			_setSelectionDataTable[compactGroup]._regionList = NULL;
			/* cache the survival rate in this table since we sort based on rate of return (1-survival) */
			/* survival rate can be more than 100% if the compact group is being used as a migration target due to NUMA proximity */
			double survivalRate = OMR_MIN(1.0, persistentStats[compactGroup]._historicalSurvivalRate);
			_setSelectionDataTable[compactGroup]._rateOfReturn = (1.0 - survivalRate);
			_setSelectionDataTable[compactGroup]._dynamicSelectionThisCycle = false;
		}
	}

	UDATA nurseryRegionCount = createNurseryCollectionSet(env);


	/* Add any non-nursery regions to the collection set as the rate-of-return and region budget dictates */
	if(dynamicCollectionSet) {
		createRateOfReturnCollectionSet(env, nurseryRegionCount);
		createCoreSamplingCollectionSet(env, nurseryRegionCount);

		/* Clean up any linkage data that was computed during set selection but will potentially become stale over the course of the run */

		/* Reset base region counts and lists */
		UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);

		for(UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
			_setSelectionDataTable[compactGroup]._regionCount = 0;
			_setSelectionDataTable[compactGroup]._regionList = NULL;
		}

		/* Clean list linkage between all regions (age group lists) */
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::MANAGED);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			region->setDynamicSelectionNext(NULL);
		}
	}
}

void
MM_CollectionSetDelegate::deleteRegionCollectionSetForPartialGC(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		/* there shouldn't be any regions that have some occupancy but not marked left if the increment is done */
		Assert_MM_false(MM_HeapRegionDescriptor::BUMP_ALLOCATED == region->getRegionType());
		Assert_MM_true(MM_RegionValidator(region).validate(env));

		region->_markData._shouldMark = false;
		region->_reclaimData._shouldReclaim = false;
	}
}

void
MM_CollectionSetDelegate::createRegionCollectionSetForGlobalGC(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_GLOBAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::MANAGED);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		Assert_MM_true(MM_RegionValidator(region).validate(env));
		Assert_MM_false(region->_reclaimData._shouldReclaim);
		if (region->containsObjects()) {
			region->_reclaimData._shouldReclaim = true;
			region->_compactData._shouldCompact = false;
		}
	}
}

void
MM_CollectionSetDelegate::deleteRegionCollectionSetForGlobalGC(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_GLOBAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		/* there shouldn't be any regions that have some occupancy but not marked left */
		Assert_MM_false(MM_HeapRegionDescriptor::BUMP_ALLOCATED == region->getRegionType());
		Assert_MM_true(MM_RegionValidator(region).validate(env));

		region->_reclaimData._shouldReclaim = false;
	}
}

MM_HeapRegionDescriptorVLHGC*
MM_CollectionSetDelegate::getNextRegion(MM_HeapRegionDescriptorVLHGC* cursor)
{
	MM_HeapRegionDescriptorVLHGC* result = cursor;
	if (NULL != result) {
		result = (MM_HeapRegionDescriptorVLHGC*)_regionManager->getNextTableRegion(result);
	}
	if (NULL == result) {
		result = (MM_HeapRegionDescriptorVLHGC*)_regionManager->getFirstTableRegion();
	}
	Assert_MM_true(NULL != result);
	return result;
}

void
MM_CollectionSetDelegate::rateOfReturnCalculationBeforeSweep(MM_EnvironmentVLHGC *env)
{
	if(_extensions->tarokEnableDynamicCollectionSetSelection) {
		UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
		for (UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
			/* Clear the current trace reclaim rate table information as we'll be building a new set */
			_setSelectionDataTable[compactGroup]._reclaimStats.reset();
		}

		/* Walk and count all regions, and count any region that will be included in the sweep set (those in trace or those that require re-sweeping due to a GMP) */
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::ALL);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			if(region->containsObjects()) {
				SetSelectionData *stats = &_setSelectionDataTable[MM_CompactGroupManager::getCompactGroupNumber(env, region)];
				MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();

				stats->_reclaimStats._regionCountBefore += 1;
				if(!region->_sweepData._alreadySwept) {
					stats->_reclaimStats._reclaimableRegionCountBefore += 1;

					stats->_reclaimStats._regionBytesFreeBefore += memoryPool->getActualFreeMemorySize();
					stats->_reclaimStats._regionDarkMatterBefore +=  memoryPool->getDarkMatterBytes();
				}
				if(!region->getRememberedSetCardList()->isAccurate()) {
					stats->_reclaimStats._regionCountOverflow += 1;
				}
			} else if(region->isArrayletLeaf()) {
				MM_HeapRegionDescriptorVLHGC *parentRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->regionDescriptorForAddress((void *)region->_allocateData.getSpine());
				Assert_MM_true(parentRegion->containsObjects());
				SetSelectionData *stats = &_setSelectionDataTable[MM_CompactGroupManager::getCompactGroupNumber(env, parentRegion)];

				stats->_reclaimStats._regionCountBefore += 1;
				stats->_reclaimStats._regionCountArrayletLeafBefore += 1;

				if(!parentRegion->_sweepData._alreadySwept) {
					stats->_reclaimStats._reclaimableRegionCountBefore += 1;
					stats->_reclaimStats._reclaimableRegionCountArrayletLeafBefore += 1;
				}
				if(!parentRegion->getRememberedSetCardList()->isAccurate()) {
					stats->_reclaimStats._regionCountArrayletLeafOverflow += 1;
				}
			}
		}
	}
}

void
MM_CollectionSetDelegate::rateOfReturnCalculationAfterSweep(MM_EnvironmentVLHGC *env)
{
	if(_extensions->tarokEnableDynamicCollectionSetSelection) {
		/* Walk and count all regions */
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::ALL);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			if(region->containsObjects()) {
				UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
				SetSelectionData *stats = &_setSelectionDataTable[compactGroup];
				MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();

				stats->_reclaimStats._regionCountAfter += 1;

				if(!region->_sweepData._alreadySwept) {
					stats->_reclaimStats._reclaimableRegionCountAfter += 1;

					stats->_reclaimStats._regionBytesFreeAfter += memoryPool->getActualFreeMemorySize();
					stats->_reclaimStats._regionDarkMatterAfter +=  memoryPool->getDarkMatterBytes();
				}
			} else if(region->isArrayletLeaf()) {
				MM_HeapRegionDescriptorVLHGC *parentRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->regionDescriptorForAddress((void *)region->_allocateData.getSpine());
				Assert_MM_true(parentRegion->containsObjects());
				SetSelectionData *stats = &_setSelectionDataTable[MM_CompactGroupManager::getCompactGroupNumber(env, parentRegion)];

				stats->_reclaimStats._regionCountAfter += 1;
				stats->_reclaimStats._regionCountArrayletLeafAfter += 1;

				if(!parentRegion->_sweepData._alreadySwept) {
					stats->_reclaimStats._reclaimableRegionCountAfter += 1;
					stats->_reclaimStats._reclaimableRegionCountArrayletLeafAfter += 1;
				}
			}
		}

		/* We now have an expected change as a result of tracing and sweeping (parts of) the heap.  Calculate the rate-of-return (ROR) on 
		 * tracing for age groups where work was done.
		 * Use a weighted running average to calculate the ROR, where the weight is the % of regions in an age group that we are examining.
		 * NOTE: We leave the ROR for the last age group as 0.
		 */
		UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);

		for(UDATA compactGroup = 0; compactGroup < compactGroupCount; compactGroup++) {
			if (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup) < _extensions->tarokRegionMaxAge) {
				/* We set the compact group for each entry every time through a table.  This is overkill, but allows us to be sure when examining a ROR element whether
				 * in fact it represents the compact group with think it does.
				 */
				SetSelectionData *stats = &_setSelectionDataTable[compactGroup];
				stats->_compactGroup = compactGroup;

				if(0 == stats->_reclaimStats._reclaimableRegionCountBefore) {
					Assert_MM_true(stats->_reclaimStats._regionCountBefore == stats->_reclaimStats._regionCountAfter);
				} else {
					Assert_MM_true(stats->_reclaimStats._regionCountBefore >= stats->_reclaimStats._reclaimableRegionCountBefore);
					Assert_MM_true(stats->_reclaimStats._regionCountBefore >= stats->_reclaimStats._regionCountAfter);
					Assert_MM_true(stats->_reclaimStats._reclaimableRegionCountBefore >= stats->_reclaimStats._reclaimableRegionCountAfter);

					UDATA bytesConsumedBefore =
						(stats->_reclaimStats._reclaimableRegionCountBefore * _extensions->regionSize)
						- stats->_reclaimStats._regionBytesFreeBefore
						- stats->_reclaimStats._regionDarkMatterBefore;
					stats->_reclaimStats._reclaimableBytesConsumedBefore = bytesConsumedBefore;
					UDATA bytesConsumedAfter =
						(stats->_reclaimStats._reclaimableRegionCountAfter * _extensions->regionSize)
						- stats->_reclaimStats._regionBytesFreeAfter
						- stats->_reclaimStats._regionDarkMatterAfter;
					stats->_reclaimStats._reclaimableBytesConsumedAfter = bytesConsumedAfter;
				}
			}
		}
	}
}
