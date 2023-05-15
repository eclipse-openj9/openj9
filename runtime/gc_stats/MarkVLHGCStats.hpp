
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(MARKVLHGCSTATS_HPP_)
#define MARKVLHGCSTATS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "j9consts.h"
#include "modron.h"
#include "modronopt.h"

#if defined(J9VM_GC_VLHGC)

#include "MarkVLHGCStatsCore.hpp"
#include "Base.hpp"
#include "AtomicOperations.hpp" 
#include "ReferenceStats.hpp"

/**
 * Storage for statistics relevant to the mark phase of a global collection.
 * @ingroup GC_Stats
 */
class MM_MarkVLHGCStats : public MM_MarkVLHGCStatsCore
{
/* data members */
private:
protected:
public:

	uintptr_t _unfinalizedCandidates;  /**< unfinalized objects that are candidates to be finalized visited this cycle */
	uintptr_t _unfinalizedEnqueued;  /**< unfinalized objects that are enqueued during this cycle (MUST be less than or equal _unfinalizedCandidates) */

	uintptr_t _ownableSynchronizerCandidates;  /**< number of ownable synchronizer objects visited this cycle, used by both MarkingScheme */
	uintptr_t _ownableSynchronizerSurvived;  /**< number of ownable synchronizer objects survived this cycle, used only by PMS */
	uintptr_t _ownableSynchronizerCleared;  /**< number of ownable synchronizer objects cleared this cycle, used only by GMP */

	uintptr_t _continuationCandidates;  /**< number of continuation objects visited this cycle, used by both MarkingScheme */
	uintptr_t _continuationCleared;  /**< number of continuation objects cleared this cycle, used only by GMP */

	MM_ReferenceStats _weakReferenceStats;  /**< Weak reference stats for the cycle */
	MM_ReferenceStats _softReferenceStats;  /**< Soft reference stats for the cycle */
	MM_ReferenceStats _phantomReferenceStats;  /**< Phantom reference stats for the cycle */

	uintptr_t _stringConstantsCleared;  /**< The number of string constants that have been cleared during marking */
	uintptr_t _stringConstantsCandidates; /**< The number of string constants that have been visited in string table during marking */

	uintptr_t _monitorReferenceCleared; /**< The number of monitor references that have been cleared during marking */
	uintptr_t _monitorReferenceCandidates; /**< The number of monitor references that have been visited in monitor table during marking */

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	uintptr_t _offHeapRegionsCleared; /**< The number of double mapped or sparse heap allocated regions that have been cleared during marking */
	uintptr_t _offHeapRegionCandidates; /**< The number of double mapped or sparse heap allocated regions that have been visited during marking */
#endif /* defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t _splitArraysProcessed; /**< The number of array chunks (not counting parts smaller than the split size) processed by this thread */
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	U_64 _concurrentGCThreadsCPUStartTimeSum; /**< The sum of all gc cpu thread times when concurrent gc work began */
	U_64 _concurrentGCThreadsCPUEndTimeSum; /**< The sum of all gc cpu thread times when concurrent gc work ended */
	U_64 _concurrentMarkGCThreadsTotalWorkTime; /**< The slowdown attributed to concurrent GC work */
/* function members */
private:
protected:
public:

	void clear()
	{
		MM_MarkVLHGCStatsCore::clear();
		_unfinalizedCandidates = 0;
		_unfinalizedEnqueued = 0;

		_ownableSynchronizerCandidates = 0;
		_ownableSynchronizerSurvived = 0;
		_ownableSynchronizerCleared = 0;

		_continuationCandidates = 0;
		_continuationCleared = 0;

		_weakReferenceStats.clear();
		_softReferenceStats.clear();
		_phantomReferenceStats.clear();

		_stringConstantsCleared = 0;
		_stringConstantsCandidates = 0;

		_monitorReferenceCleared = 0;
		_monitorReferenceCandidates = 0;

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
		_offHeapRegionsCleared = 0;
		_offHeapRegionCandidates = 0;
#endif /* defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		_splitArraysProcessed = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

		_concurrentGCThreadsCPUStartTimeSum = 0;
		_concurrentGCThreadsCPUEndTimeSum = 0;
		_concurrentMarkGCThreadsTotalWorkTime = 0;

	}

	void merge(MM_MarkVLHGCStats *statsToMerge)
	{
		MM_MarkVLHGCStatsCore::merge(statsToMerge);
		_unfinalizedCandidates += statsToMerge->_unfinalizedCandidates;
		_unfinalizedEnqueued += statsToMerge->_unfinalizedEnqueued;

		_ownableSynchronizerCandidates += statsToMerge->_ownableSynchronizerCandidates;
		_ownableSynchronizerSurvived += statsToMerge->_ownableSynchronizerSurvived;
		_ownableSynchronizerCleared += statsToMerge->_ownableSynchronizerCleared;

		_continuationCandidates += statsToMerge->_continuationCandidates;
		_continuationCleared += statsToMerge->_continuationCleared;

		_weakReferenceStats.merge(&statsToMerge->_weakReferenceStats);
		_softReferenceStats.merge(&statsToMerge->_softReferenceStats);
		_phantomReferenceStats.merge(&statsToMerge->_phantomReferenceStats);

		_stringConstantsCleared += statsToMerge->_stringConstantsCleared;
		_stringConstantsCandidates += statsToMerge->_stringConstantsCandidates;

		_monitorReferenceCleared += statsToMerge->_monitorReferenceCleared;
		_monitorReferenceCandidates += statsToMerge->_monitorReferenceCandidates;

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
		_offHeapRegionsCleared += statsToMerge->_offHeapRegionsCleared;
		_offHeapRegionCandidates += statsToMerge->_offHeapRegionCandidates;
#endif /* defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

		_concurrentGCThreadsCPUStartTimeSum += statsToMerge->_concurrentGCThreadsCPUStartTimeSum;
		_concurrentGCThreadsCPUEndTimeSum += statsToMerge->_concurrentGCThreadsCPUEndTimeSum;
		_concurrentMarkGCThreadsTotalWorkTime += statsToMerge->_concurrentMarkGCThreadsTotalWorkTime;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		/* It may not ever be useful to merge these stats, but do it anyways */
		_splitArraysProcessed += statsToMerge->_splitArraysProcessed;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}
	
	MM_MarkVLHGCStats() :
		MM_MarkVLHGCStatsCore()
		,_unfinalizedCandidates(0)
		,_unfinalizedEnqueued(0)
		,_ownableSynchronizerCandidates(0)
		,_ownableSynchronizerSurvived(0)
		,_ownableSynchronizerCleared(0)
		,_continuationCandidates(0)
		,_continuationCleared(0)
		,_weakReferenceStats()
		,_softReferenceStats()
		,_phantomReferenceStats()
		,_stringConstantsCleared(0)
		,_stringConstantsCandidates(0)
		,_monitorReferenceCleared(0)
		,_monitorReferenceCandidates(0)
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
		,_offHeapRegionsCleared(0)
		,_offHeapRegionCandidates(0)
#endif /* defined(J9VM_GC_ENABLE_DOUBLE_MAP) || defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		,_splitArraysProcessed(0)
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
		,_concurrentGCThreadsCPUStartTimeSum(0)
		,_concurrentGCThreadsCPUEndTimeSum(0)
		,_concurrentMarkGCThreadsTotalWorkTime(0)
	{
	}
	
}; 

#endif /* J9VM_GC_VLHGC */
#endif /* MARKVLHGCSTATS_HPP_ */
