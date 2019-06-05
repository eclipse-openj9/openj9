
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(COMPACTGROUPPERSISTENTSTATS_HPP_)
#define COMPACTGROUPPERSISTENTSTATS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

class MM_EnvironmentVLHGC;
class MM_HeapRegionDescriptorVLHGC;

/**
 * Stores globally accessible data on a per-compact-group basis.
 * @ingroup GC_Modron_Standard
 */
class MM_CompactGroupPersistentStats
{


/* data members */
private:
protected:
public:

	double _historicalSurvivalRate;	/**< Historical survival rate of bytes in the corresponding compact group expressed as a percentage in the range [0..1] */
	double _weightedSurvivalRate; /**< The historical survival rate in the corresponding compact group, weighted to take into consideration older groups in the same context */

	bool _statsHaveBeenUpdatedThisCycle; /** < Indicates whether we have updated the stats shown below yet this cycle.  They will be updated only once per cycle */
	UDATA _measuredLiveBytesBeforeCollectInCollectedSet;	/**< The number of bytes allocated in the collection set subset in this group at the beginning of the collect (both live and dead objects) */
	UDATA _projectedLiveBytesBeforeCollectInCollectedSet;	/**< The projected number of bytes allocated in the collection set subset in this group at the beginning of the collect (the reason for the projection is that this attempts to account for only live objects) */
	UDATA _projectedLiveBytesAfterPreviousPGCInCollectedSetForNonEdenFraction; /**< Non-Eden Fraction (in case age group spans over Eden boundary) of _projectedLiveBytesAfterPreviousPGCInCollectedSet. This could be approximation! */
	UDATA _projectedLiveBytesAfterPreviousPGCInCollectedSet; /**< The projected number of bytes live at the end of previous PGC, for regions selected in this PGC for this compact group */
	UDATA _projectedLiveBytesAfterPreviousPGCInCollectedSetForEdenFraction; /**< Eden Fraction (in case age group spans over Eden boundary) of _projectedLiveBytesAfterPreviousPGCInCollectedSet */
	UDATA _measuredLiveBytesBeforeCollectInGroup;	/**< The total number of bytes consumed by all the regions in this compact group before the collect, whether they are in the collection set or not */
	UDATA _projectedLiveBytesBeforeCollectInGroup;	/**< The projected number of bytes consumed by all the regions in this compact group before the collect, whether they are in the collection set or not (the reason for the projection is that this attempts to account for only live objects) */
	UDATA _measuredLiveBytesAfterCollectInGroup;	/**< The total number of bytes in a compact group, after a collect, determined by summing the measured bytes not in the collected set and the number of bytes in the collected set, as measured by this collect */
	UDATA _measuredLiveBytesAfterCollectInCollectedSet;	/**< The number of bytes allocated in the collection set subset in this group at the    */
	volatile UDATA _measuredBytesCopiedFromGroupDuringCopyForward;	/**< The total number of bytes measured to be live during a copy-forward which were copied from this compact group (the destination could be this group or another) */
	volatile UDATA _measuredBytesCopiedToGroupDuringCopyForward; /**< The total number of bytes measured to be copied to group during a copy-forward */
	volatile U_64 _measuredAllocationAgeToGroupDuringCopyForward; /**< The total allocation age measured on objects to be copied to group during copy-forward */
	U_64 _averageAllocationAgeToGroup;	/**< Average allocation age for group */
	U_64 _maxAllocationAge;  /**< max allocation age (of an object or region) in this compact group) */
	/* TODO: lpnguyen group everything into anonymous structs */

	double _projectedInstantaneousSurvivalRate; /**< survivor rate between this and previous age group */
	double _projectedInstantaneousSurvivalRatePerAgeUnit; /**< fraction of _projectedInstantaneousSurvivalRate, in case age group is a multiple of age units. this is an average of _projectedInstantaneousSurvivalRateThisPGCPerAgeUnit */
	double _projectedInstantaneousSurvivalRateThisPGCPerAgeUnit; /**< same as _projectedInstantaneousSurvivalRatePerAgeUnit, but for this PGC (not an average over time) */

	
	UDATA _projectedLiveBytes; /** < The sum of projected live bytes of all regions in the compact group */
	UDATA _liveBytesAbsoluteDeviation; /** < sum of the absolute value _projectedLiveBytesDeviation of every region in the compact group */
	UDATA _regionCount; /** < count of the number of regions in the compact group */

	UDATA _regionsInRegionCollectionSetForPGC; /** < number of regions in the region collection set for a partial global collection*/

/* member functions */
private:
	/* Checks if a compactGroup has been collected and set _statsHaveBeenUpdatedThisCycle to true if that is the case.
	 * Also updates some stats (projectedInstantaneousSurvivalRate, historicalSurvivalRate, weightedSurvivalRate) that are dependant on the 
	 * live bytes stats if they have not been updated yet.
	 *
	 * @param env[in] the current thread
	 * @param persistentStats[in] an array of MM_CompactGroupPersistentStats with as many elements as there are compact groups
	 */
	static void updateStatsAfterCollectionOperation(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/**
	 * Update survival rate stats at the end of a PGC.  This method depends on the before/after data for the given persistent stats
	 * structure being populated by the component which performed the PGC but then acts on those, generically.
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The persistent stats array
	 * @param compactGroup[in] compactGroup for which instantaneousSurvivalRate is calculated
	 */
	static void updateProjectedSurvivalRate(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats, UDATA compactGroup);
	
	/**
	 * Auxiliary function used by updateProjectedSurvivalRate. If an age group is at the boundary of Eden, it returns the size fractions on either sides.
 	 * @param env[in] The Master GC thread
	 * @param ageInThisAgeGroup[in] Age-size of the age group
	 * @param ageInThisCompactGroup[in] Age-size of the compact group
	 * @param currentAge[in] Current age of objects in this group
	 * @param edenFractionOfCompactGroup[out]
	 * @param nonEdenFractionOfCompactGroup[out]
	 */
	static void calculateAgeGroupFractionsAtEdenBoundary(MM_EnvironmentVLHGC *env, U_64 ageInThisAgeGroup, U_64 *ageInThisCompactGroup, U_64 currentAge, U_64 allocatedSinceLastPGC, U_64 *edenFractionOfCompactGroup, U_64 *nonEdenFractionOfCompactGroup);

	/* Reset live bytes stats.  Should be done at the start of every gc cycle.
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void resetLiveBytesStats(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Init projected live bytes of regions that have been allocated from since previous PGC.
	 * The init value is set to current consumed space (according to memory pool).
	 *
	 * TODO: Create a new class for this as it's not really tied to compact groups
 	 * @param env[in] The Master GC thread
	 */
	static void initProjectedLiveBytes(MM_EnvironmentVLHGC *env);

	/*
	 * Updates the projectedLiveBytes for all regions by multiplying by their compact group's instantaneous survivor rate
	 *
	 * TODO: Create a new class for this as it's not really tied to compact groups
 	 * @param env[in] The Master GC thread
	 */
	static void decayProjectedLiveBytesForRegions(MM_EnvironmentVLHGC *env);
	
	/**
	 * Given the input list of compact group stats with initialized _historicalSurvivalRate fields, initialize the _weightedSurvivalRate fields.
	 * The survival rate is weighted using survival data from older groups to project the future survival rate of the group.
	 * This is used to favour compacting more stable compact groups.
	 * 
	 * @param env[in] the current thread
	 * @param persistentStats[in] an array of MM_CompactGroupPersistentStats with as many elements as there are compact groups
	 */
	static void deriveWeightedSurvivalRates(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/**
	 * Auxiliary function to calculation various values for measured/projected live bytes before current/after previous PGC, invoked for each region in Collection Set
  	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 * @param region[in] The region for which we take measured/projected live bytes values
	 * @param measuredLiveBytes Passing in the actual value for measuredLiveBytes (already calculated by the caller for its own purpose)
	 * @param projectedLiveBytes Passing in the actual value for projectedLiveBytes (already calculated by the caller for its own purpose)
	 */
	static void calculateLiveBytesForRegion(MM_EnvironmentVLHGC *env,  MM_CompactGroupPersistentStats *persistentStats, UDATA compactGroup, MM_HeapRegionDescriptorVLHGC *region, UDATA measuredLiveBytes, UDATA projectedLiveBytes);

protected:
public:
	/**
	 * Allocate a list of compact group stats
	 * @param env[in] the current thread
	 * @return an array of MM_CompactGroupPersistentStats, or NULL 
	 */
	static MM_CompactGroupPersistentStats * allocateCompactGroupPersistentStats(MM_EnvironmentVLHGC *env);
	
	/**
	 * Free the list of compact group stats
	 * @param env[in] the current thread
	 * @param persistentStats[in] the list to free 
	 */
	static void killCompactGroupPersistentStats(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Gather information related to bytes in and out of collection set.  Run before a copy forward
	 * Stats are only updated on the first collection operation (copy forward,sweep...).  This side-steps annoying double-counting of live bytes
	 * between different collection operations (though it possibly lowers our sample set).
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsBeforeCopyForward(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Gathers information related to bytes collected by copy forward.  Run after a copy forward
	 * Stats are only updated on the first collection operation (copy forward,sweep...).  This side-steps annoying double-counting of live bytes
	 * between different collection operations (though it possibly lowers our sample set).
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsAfterCopyForward(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Gather information related to bytes in and out of collection set.  Run before a sweep
	 * Stats are only updated on the first collection operation (copy forward,sweep,compact).  This side-steps annoying double-counting of live bytes
	 * between different collection operations (though it possibly lowers our sample set).
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsBeforeSweep(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Gathers information related to bytes collected by copy forward.  Run after a sweep
	 * Stats are only updated on the first collection operation (copy forward,sweep,compact).  This side-steps annoying double-counting of live bytes
	 * between different collection operations (though it possibly lowers our sample set).
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsAfterSweep(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Gather information related to bytes in and out of collection set.  Run before a compact
	 * Stats are only updated on the first collection operation (copy forward,sweep,compact).  This side-steps annoying double-counting of live bytes
	 * between different collection operations (though it possibly lowers our sample set).
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsBeforeCompact(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Gathers information related to bytes collected by copy forward.  Run after a compact
	 * Stats are only updated on the first collection operation (copy forward,sweep,compact).  This side-steps annoying double-counting of live bytes
	 * between different collection operations (though it possibly lowers our sample set).
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsAfterCompact(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/*
	 * Does any book-keeping/updating of stats before any gc operations happen
	 *
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void updateStatsBeforeCollect(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

	/**
	 * Updates projectedLiveBytes, projectedLiveBytesDeviation and regionCount fields by iterating through all regions and updating
	 * their compact group's stats.
	 * @param env[in] The Master GC thread
	 * @param persistentStats[in] The list of per-compact group persistent stats
	 */
	static void deriveProjectedLiveBytesStats(MM_EnvironmentVLHGC *env, MM_CompactGroupPersistentStats *persistentStats);

};

#endif /* COMPACTGROUPPERSISTENTSTATS_HPP_ */
