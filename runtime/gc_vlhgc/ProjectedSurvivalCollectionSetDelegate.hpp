
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

#if !defined(PROJECTEDSURVIVALCOLLECTIONSETDELEGATE_HPP_)
#define PROJECTEDSURVIVALCOLLECTIONSETDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"

#include <string.h>

#include "BaseNonVirtual.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"

class MM_HeapRegionManager;

class MM_ProjectedSurvivalCollectionSetDelegate : public MM_BaseNonVirtual
{
public:
	class SetSelectionData;

	/**
	 * Trace reclaim rate table element.
	 * Contains details about the rate of reclaim based on tracing for a particular region age group.
	 */
	class RegionReclaimableStats {
	public:
		UDATA _regionCountBefore;  /**< Number of regions in the age group before a sweep event */
		UDATA _regionCountAfter;  /**< Number of regions after trace and sweep have concluded */
		UDATA _regionCountArrayletLeafBefore;  /**< Number of regions that are arraylet leafs in the age group before a sweep */
		UDATA _regionCountArrayletLeafAfter;  /**< Number of regions that are arraylet leafs in the age group after a sweep */
		UDATA _regionCountOverflow;  /**< Number of regions which contain objects but are RSCL overflowed */
		UDATA _regionCountArrayletLeafOverflow;  /**< Number of regions which are arraylet leaves but are RSCL overflowed */

		UDATA _regionBytesFreeBefore;  /**< Number of free bytes (fragmented) in the age group before a sweep event */
		UDATA _regionDarkMatterBefore;  /**< Number of free bytes (fragmented) in the age group after a trace and sweep have concluded */
		UDATA _regionBytesFreeAfter;  /**< Number of dark matter bytes in the age group before a sweep event */
		UDATA _regionDarkMatterAfter;  /**< Number of dark matter bytes in the age group after a trace and sweep have concluded */

		UDATA _reclaimableRegionCountBefore;  /**< Number of regions to be included in the sweep set (includes tracing and recently traced but unswept due to GMP) */
		UDATA _reclaimableRegionCountAfter;  /**< Number of regions in the reclaimable set after having been swept */
		UDATA _reclaimableRegionCountArrayletLeafBefore;  /**< Number of regions that are arraylet leafs in the age group before a sweep */
		UDATA _reclaimableRegionCountArrayletLeafAfter;  /**< Number of regions that are arraylet leafs in the age group after a sweep */

		UDATA _reclaimableBytesConsumedBefore;  /**< Number of bytes consumed before a sweep in the set of regions marked as part of the reclaimable set */
		UDATA _reclaimableBytesConsumedAfter;  /**< Number of bytes consumed after a sweep in the set of regions marked as part of the reclaimable set */

	protected:
	private:

	public:
		RegionReclaimableStats() {};

		void reset() { memset(this, 0, sizeof(RegionReclaimableStats)); }

	protected:
	private:
	};

	/**
	 * Top level class containing all data related to dynamic set selection and core sampling.
	 */
	class SetSelectionData {
	public:
		UDATA _compactGroup;  /**< The compact group that the set represents */
		MM_HeapRegionDescriptorVLHGC *_regionList;  /**< List of regions associated to the compact group (NOTE: valid only during collection set building) */
		UDATA _regionCount;  /**< The number of regions that appear in the region list (NOTE: valid only during collection set building) */

		RegionReclaimableStats _reclaimStats;  /**< general reclaim statistics for the age group that are used for calculations and diagnostics */
		double _rateOfReturn;  /**< The fractional percentage of the expected ROR when tracing the entire set of regions at the corresponding age */
		bool _dynamicSelectionThisCycle;  /**< Flag indicating if the age group has regions that were dynamically selected for collection */
	protected:
	private:

	public:
		SetSelectionData()
			: _compactGroup(0)
			, _regionList(NULL)
			, _regionCount(0)
			, _reclaimStats()
			, _rateOfReturn(0.0)
			, _dynamicSelectionThisCycle(false)
		  {}

		/**
		 * Add a region to the age group region list.
		 * Used for fast tracking to regions without having to do a full region search.
		 */
		void addRegion(MM_HeapRegionDescriptorVLHGC *region) {
			region->setDynamicSelectionNext(_regionList);
			_regionList = region;
			_regionCount += 1;
		}

	protected:
	private:
	};

	/* Data Members */
private:
	MM_GCExtensions *_extensions;  /**< A cached pointer to the global extensions */
	MM_HeapRegionManager *_regionManager; /**< A cached pointer to the global heap region manager */

	SetSelectionData *_setSelectionDataTable;  /**< Storage table for set selection statistics and variables (grouped by age) */
	SetSelectionData **_dynamicSelectionList;  /**< Pointer table used for sorting or iterating over candidate dynamic selection elements */

	MM_HeapRegionDescriptorVLHGC **_dynamicSelectionRegionList;  /**< Pointer table used for sorting or iterating over regions */

protected:
public:

	/* Member Functions */
private:
	/**
	 * Include the core set of regions that comprise the nursery into a collection set for a PartialGC.
	 * Find all regions whose age allow them to be counted as part of the nursery and, if the region is collectable (contains objects
	 * and isn't in an RSCL overflow state) add it to the collection set.
	 * @param env[in] The master GC thread
	 * @return The number of regions in the nursery collection set
	 */
	UDATA createNurseryCollectionSet(MM_EnvironmentVLHGC *env);

	/**
	 * Marks region for collection.
	 * @param env[in] The master GC thread
	 * @param setSelectionData[in] Age group set selection data element that contains the list of regions to select from
	 */
	void selectRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Support routine to select a number of regions based on a budget to include in the collection set.
	 * Given a set selection age group and a budget, use an form of counting to select the budgeted number of regions available in the age group.
	 * The counting attempts to evenly distribute the selection of regions across all regions in the age group.
	 * @param env[in] The master GC thread
	 * @param budget[in] Number of regions accounted for in the budget
	 * @param setSelectionData[in] Age group set selection data element that contains the list of regions to select from
	 */
	UDATA selectRegionsForBudget(MM_EnvironmentVLHGC *env, UDATA budget, SetSelectionData *setSelectionData);

	/**
	 * Include a set of regions, based on rate of return calculations, outside of the nursery for collection set purposes.
	 * Given the total regions in the nursery, select a number of regions outside the nursery for inclusion in the PartialGC collection set.
	 * The selection will be past on historical rate of return (ROR) percentages, regions with higher ROR values being selected first.
	 * @param env[in] The master GC thread
	 * @param nurseryRegionCount[in] Number of regions selected as the core nursery collection set
	 */
	void createRateOfReturnCollectionSet(MM_EnvironmentVLHGC *env, UDATA nurseryRegionCount);

	/**
	 * Include a set of regions, base on not being selected for collection and having a high age group population count, for collection set purposes.
	 * Find region age groups that have not participated in nursery or ROR collection set selection, and select a set of regions for collection set
	 * sampling purposes.  The aim is to find age groups where collection opportunities might exist (increasing the ROR and being dynamically
	 * selected).
	 * @param env[in] The master GC thread
	 * @param nurseryRegionCount[in] Number of regions selected as the core nursery collection set
	 */
	void createCoreSamplingCollectionSet(MM_EnvironmentVLHGC *env, UDATA nurseryRegionCount);


	/**
	 * Given the specified region, return the next region.
	 * If the region is NULL, or the last region in the table, return the first region.
	 * @param cursor[in] the current region, or NULL
	 * @return the next region (returning to the first region if the end is reached)
	 */
	MM_HeapRegionDescriptorVLHGC* getNextRegion(MM_HeapRegionDescriptorVLHGC* cursor);

	/*
	 * Helper function used by J9_SORT to sort rate of return element lists.
	 *
	 * @param[in] element1 first element to compare
	 * @param[in] element2 second element to compare env
	 */
	static int compareRateOfReturnScoreFunc(const void *element1, const void *element2);

	/**
	 * Helper function used by J9_SORT to sort core sample element lists.
	 *
	 * @param[in] element1 first element to compare
	 * @param[in] element2 second element to compare
	 */
	static int compareCoreSampleScoreFunc(const void *element1, const void *element2);
protected:
public:
	/**
	 * Initialize the receiver.
	 * @param env[in] The thread initializing the collector
	 * @return Whether or not the initialization succeeded
	 */
	bool initialize(MM_EnvironmentVLHGC *env);

	/**
	 * Tear down the receiver.
	 * @param env[in] The thread tearing down the collector
	 */
	void tearDown(MM_EnvironmentVLHGC *env);

	/**
	 * Construct the receiver.
	 */
	MM_ProjectedSurvivalCollectionSetDelegate(MM_EnvironmentBase *env, MM_HeapRegionManager *manager);

	/**
	 * Build the internal representation of the set of regions that are to be collected for this cycle.
	 * This should only be called during a partial garbage collect.
	 * @param env[in] The master GC thread
	 */
	void createRegionCollectionSetForPartialGC(MM_EnvironmentVLHGC *env);

	/**
	 * Delete the internal representation of the set of regions that participated in the collection cycle.
	 * This should only be called during a partial garbage collect.
	 * @param env[in] The master GC thread
	 */
	void deleteRegionCollectionSetForPartialGC(MM_EnvironmentVLHGC *env);

	/**
	 * Build the internal representation of the set of regions that are to be collected for this cycle.
	 * This should only be called during a global garbage collect.
	 * @param env[in] The master GC thread
	 */
	void createRegionCollectionSetForGlobalGC(MM_EnvironmentVLHGC *env);

	/**
	 * Delete the internal representation of the set of regions that participated in the collection cycle.
	 * This should only be called during a global garbage collect.
	 * @param env[in] The master GC thread
	 */
	void deleteRegionCollectionSetForGlobalGC(MM_EnvironmentVLHGC *env);

	/**
	 * Record pre-sweep region information in order to calculate rate of return on tracing for age groups.
	 */
	void rateOfReturnCalculationBeforeSweep(MM_EnvironmentVLHGC *env);

	/**
	 * Record post-sweep region information and calculate rate of return on tracing for age groups.
	 */
	void rateOfReturnCalculationAfterSweep(MM_EnvironmentVLHGC *env);

};

#endif /* PROJECTEDSURVIVALCOLLECTIONSETDELEGATE_HPP_ */
