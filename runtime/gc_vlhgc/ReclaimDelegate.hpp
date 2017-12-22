
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

#if !defined(RECLAIMDELEGATE_HPP_)
#define RECLAIMDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"

class MM_EnvironmentVLHGC;
class MM_AllocateDescription;
class MM_MemorySubSpace;
class MM_GCCode;
class MM_HeapRegionManager;
class MM_HeapRegionDescriptorVLHGC;
class MM_ParallelSweepSchemeVLHGC;
class MM_MemoryPool;
class MM_WriteOnceCompactor;
class MM_MarkMap;
class MM_CollectionSetDelegate;

class MM_ReclaimDelegate : public MM_BaseNonVirtual
{
	/* Data Members */
public:
protected:
private:
	MM_HeapRegionManager *_regionManager; /**< A cached pointer to the global heap region manager */	
	MM_Dispatcher *_dispatcher;  /**< A cached pointer to the global dispatcher used for tasks */
	MM_ParallelSweepSchemeVLHGC *_sweepScheme;	/**< The sweep scheme component which this delegate owns and uses to reclaim memory */
#if defined(J9VM_GC_MODRON_COMPACTION)
	MM_WriteOnceCompactor *_writeOnceCompactor;	/**< The compactor used when reclaiming memory */
#endif /* J9VM_GC_MODRON_COMPACTION */
	MM_CollectionSetDelegate *_collectionSetDelegate; /**< Cached pointer to the collection set delegate */

	/* This constant reflects typical number of regions in a vlh setting.
	 * TODO: _regionSortedByCompactScore can be allocated dynamically based on region count determined
	 * during startup.
	 */
	enum { MAX_SORTED_REGIONS = 1024 };
	UDATA _currentSortedRegionCount; /**< The current size of the array */
	MM_HeapRegionDescriptorVLHGC *_regionSortedByCompactScore[MAX_SORTED_REGIONS]; /**< descending sorted list */

	MM_HeapRegionDescriptorVLHGC ** _regionsSortedByEmptinessArray; /**< descending list of regions sorted by their emptiness (amount of free and dark matter bytes in their memory pool) */
	UDATA _regionsSortedByEmptinessArraySize;	/**< The populated size of _regionsSortedByEmptinessArray */
	
	double _compactRateOfReturn; /**< The historical rate of return of compact (essentially, the accuracy of the compact region selection) */
	
	typedef struct {
		UDATA freeBytes;
		UDATA recoverableBytes;
		UDATA defragmentRecoverableBytes;
		UDATA regionCount;
		UDATA bytesToBeMoved;
	} MM_ReclaimDelegate_ScoreBaseCompactTable;
	MM_ReclaimDelegate_ScoreBaseCompactTable *_compactGroups;  /**< Score based compaction recovered details per compactGroup */
	const UDATA _compactGroupMaxCount;	/**< The number of compact groups in _compactGroups - used for walking the _compactGroups table and validating calculated compaction group numbers */

	/* Member Functions */
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
	 * Adjust internal structures to reflect the change in heap size.
	 *
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @return true if operation completes with success
	 */
	bool heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 *
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
	 * @param highValidAddress The first valid address following the highest in the heap range being removed
	 * @return true if operation completes with success
	 */
	bool heapRemoveRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	
	/**
	 * Adjust internal structures to reflect the change in heap size.
	 */
	void heapReconfigured(MM_EnvironmentVLHGC *env);
	
	/**
	 * Request to create sweepPoolState class for pool
	 * @param  memoryPool memory pool to attach sweep state to
	 * @return pointer to created class
	 */
	void *createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool);

	/**
	 * Request to destroy sweepPoolState class for pool
	 * @param  sweepPoolState class to destroy
	 */
	void deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState);

	/**
	 * Performs sweep.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param activeSubSpace[in] The active subspace which ran out of memory
	 * @param gcCode[in] The description of the nature of the collection
	 */
	void runReclaimCompleteSweep(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode);

	/**
	 * Performs compact and cleans up our data structures used to define a compact.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param activeSubSpace[in] The active subspace which ran out of memory
	 * @param gcCode[in] The description of the nature of the collection
	 * @param nextMarkMap[in] The next mark map (since the compactor uses it for fixup data storage)
	 * @param compactSelectionGoalInBytes[in] The minimum number of free bytes or  The minimum amount of compact work to be done
	 */
	void runReclaimCompleteCompact(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode, MM_MarkMap *nextMarkMap, UDATA compactSelectionGoalInBytes);

	/**
	 * Performs sweep followed by compact and cleans up our data structures used to define a compact.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param activeSubSpace[in] The active subspace which ran out of memory
	 * @param gcCode[in] The description of the nature of the collection
	 * @param nextMarkMap[in] The next mark map (since the compactor uses it for fixup data storage)
	 * @param skippedRegionCountRequiringSweep[out] The number of regions which should be compacted but couldn't be (due to inaccurate RSCL or active critical regions) and also haven't yet been swept
	 */
	void runReclaimForAbortedCopyForward(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode, MM_MarkMap *nextMarkMap, UDATA *skippedRegionCountRequiringSweep);
	
	/**
	 * Estimate the number of reclaimable regions. If score based compact is enabled, assume that all
	 * regions are selected. If it is not enabled, only consider free regions.
	 * 
	 * @param env[in] the master GC thread
	 * @param reclaimableRegions[out] estimated total number of reclaimable regions 
	 * @param defragmentReclaimableRegions[out] estimated number of reclaimable old regions
	 */
	void estimateReclaimableRegions(MM_EnvironmentVLHGC *env, double averageEmptinessOfCopyForwardedRegions, UDATA *reclaimableRegions, UDATA *defragmentReclaimableRegions);

	/**
	 * Called to invoke a global sweep to update free region data and dark matter estimates following the completion of a GMP.
	 * This is intended to be called before the PGC runs either its Copy-Forward or Mark-Sweep-Compact so that it can use the
	 * most up-to-date statistics when selecting its collection set.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param activeSubSpace[in] The active subspace which ran out of memory
	 * @param gcCode[in] The description of the nature of the collection
	 */
	void runGlobalSweepBeforePGC(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode);

	/**
	 * Selects regions with the goal of amount of data (in bytes) being compacted.
	 * Used both for Copy-Forward or Mark-Sweep-Compact
	 * 
	 * @param env[in] The current thread
	 * @param isCopyForward true if called in a context of Copy-Forward, and false in a context of Mark-Compact
	 * @param desiredWorkToDo Desired amount od data (in bytes) to be compacted 
	 * @param skippedRegionCountRequiringSweep[out] The number of regions which should be compacted but couldn't be (due to inaccurate RSCL or active critical regions) and also haven't yet been swept
	 * @return The number of regions selected for compaction.
	 */	
	UDATA tagRegionsBeforeCompactWithWorkGoal(MM_EnvironmentVLHGC *env, bool isCopyForward, UDATA desiredWorkToDo, UDATA *skippedRegionCountRequiringSweep);
	
	/**
	 * Untag all regions after sweep/compact is completed, .
	 */	
	void untagRegionsAfterSweep();

	/**
	 * Build the internal representation of the set of regions that are to be collected for this cycle.
	 * This should only be called during a partial garbage collect.
	 * @param env[in] The master GC thread
	 */
	void createRegionCollectionSetForPartialGC(MM_EnvironmentVLHGC *env, UDATA desiredWorkToDo);

	/**
	 * Compute the compact score based on various stats collected and store them in the regions and in a sorted list
	 * for later consideration when selecting regions to compact. Used only for compactWorkGoal selection for both CopyForward and MarkSweepCompact
	 * @param env[in] The master GC thread
	 */
	void deriveCompactScore(MM_EnvironmentVLHGC *env);
	
	/**
	 * A helper to call into the collector to perform the compact.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param nextMarkMap[in] The next mark map (since the compactor uses it for fixup data storage)
	 */
	void compactAndCorrectStats(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MarkMap *nextMarkMap);

	/**
	 *	Main call for Compact operation
	 * @param nextMarkMap[in] The next mark map (since the compactor uses it for fixup data storage)
	 */
	void masterThreadCompact(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MarkMap *nextMarkMap);

	void masterThreadRestartAllocationCaches(MM_EnvironmentVLHGC *env);
	
	void reportSweepStart(MM_EnvironmentBase *env);
	void reportSweepEnd(MM_EnvironmentBase *env);
	void reportGlobalGCCollectComplete(MM_EnvironmentBase *env);
#if defined(J9VM_GC_MODRON_COMPACTION)
	void reportCompactStart(MM_EnvironmentBase *env);
	void reportCompactEnd(MM_EnvironmentBase *env);
#endif /* J9VM_GC_MODRON_COMPACTION */
	
	/**
	 * Performs a compact and cleans up our data structures used to define a compact.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param activeSubSpace[in] The active subspace which ran out of memory
	 * @param compactSelectionGoalInBytes[in] The minimum number of free bytes or  The minimum amount of compact work to be done
	 * @param gcCode[in] The description of the nature of the collection
	 * @param nextMarkMap[in] The next mark map (since the compactor uses it for fixup data storage)
	 * @param skippedRegionCountRequiringSweep[out] The number of regions which should be compacted but couldn't be (due to inaccurate RSCL or active critical regions) and also haven't yet been swept
	 */
	void runCompact(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, UDATA compactSelectionGoalInBytes, MM_GCCode gcCode, MM_MarkMap *nextMarkMap, UDATA *skippedRegionCountRequiringSweep);
	
	/**
	 * Performs an atomic sweep.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The description of the allocation which triggered the collection
	 * @param activeSubSpace[in] The active subspace which ran out of memory
	 * @param gcCode[in] The description of the nature of the collection
	 */
	void performAtomicSweep(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode);
		
	/**
	 * TODO currently _regionManager is used by tagRegionBeforeCollectPhase and untagRegionAfterCollectPhase.
	 * Both functions will be commonized when markDelegate and reclaim delegate are fully implemented, then we can remove the _regionManager field.
	 */
	MM_ReclaimDelegate(MM_EnvironmentBase *env, MM_HeapRegionManager *manager, MM_CollectionSetDelegate *collectionSetDelegate);

	/**
	 * Return optimal region threshold at where a GMP will be kicked at the right moment to minimize the amount of work
	 * (avgGMPScanTimeCostPerPGC + avgDefragmentCopyForwardDurationPerPGC) we have to do.
	 */
	double calculateOptimalEmptinessRegionThreshold(MM_EnvironmentVLHGC *env, double regionConsumptionRate, double avgSurvivorRegions, double avgCopyForwardRate, U_64 scanTimeCostPerGMP);

protected:
	
private:
	/**
	 *  main method to provide sweep
	 */
	void doSweep(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode);

	void postCompactCleanup(MM_EnvironmentVLHGC *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *activeSubSpace, MM_GCCode gcCode);

	/**
	 * Selects regions for sweep in an atomic collection and reports how many regions were selected.
	 * In a Partial Garbage Collect, all regions in the collection set are selected.
	 * In a Global Garbage Collect, all eligible regions are selected.
	 * 
	 * NOTE:  This method currently does nothing and only exists to provide symmetry when compared to compact (it may be used to incrementalize sweep, in the future)
	 * 
	 * @param env[in] The current thread
	 */
	void tagRegionsBeforeSweep(MM_EnvironmentVLHGC *env);

	/**
	 * Selects regions for compaction in an atomic collection and reports how many regions were selected.
	 * In a Partial Garbage Collect, all regions in the collection set are selected.
	 * In a Global Garbage Collect, all eligible regions are selected.
	 * 
	 * @param env[in] The current thread
	 * @param skippedRegionCountRequiringSweep[out] The number of regions which should be compacted but couldn't be (due to inaccurate RSCL or active critical regions) and also haven't yet been swept
	 * @return The number of regions tagged
	 */
	UDATA tagRegionsBeforeCompact(MM_EnvironmentVLHGC *env, UDATA *skippedRegionCountRequiringSweep);

	/**
	 * Rebuilds a table of regions sorted by their emptiness (freeAndDarkMatterBytes from their memory pool).
	 *
	 * @param env[in] The current thread
	 */
	void rebuildRegionsSortedByEmptinessArray(MM_EnvironmentVLHGC *env);

	/**
	 * Rebuilds a table of regions sorted by their emptiness (freeAndDarkMatterBytes from their memory pool).
	 *
	 * @param env[in] The current thread
	 */
	void calculateOptimalEmptinessRegionThreshold(MM_EnvironmentVLHGC *env);

};

#endif /* RECLAIMDELEGATE_HPP_ */
