

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

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modronopt.h"
#include "ModronAssertions.h"
#include "ut_j9mm.h"
#include "j9comp.h"

#include <string.h>

#include "ParallelSweepSchemeVLHGC.hpp"

#include "AllocateDescription.hpp"
#include "Bits.hpp"
#include "CardTable.hpp"
#include "Debug.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapMapIterator.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "Math.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectModel.hpp"
#include "ParallelDispatcher.hpp"
#include "ParallelSweepChunk.hpp"
#include "ParallelTask.hpp"
#include "SweepHeapSectioningVLHGC.hpp"
#include "SweepPoolManagerVLHGC.hpp"
#include "SweepPoolManagerAddressOrderedList.hpp"
#include "SweepPoolState.hpp"


#if defined(J9VM_ENV_DATA64)
#define J9MODRON_OBM_SLOT_EMPTY ((UDATA)0x0000000000000000)
#define J9MODRON_OBM_SLOT_FIRST_SLOT ((UDATA)0x0000000000000001)
#define J9MODRON_OBM_SLOT_LAST_SLOT ((UDATA)0x8000000000000000)
#else /* J9VM_ENV_DATA64 */
#define J9MODRON_OBM_SLOT_EMPTY ((UDATA)0x00000000)
#define J9MODRON_OBM_SLOT_FIRST_SLOT ((UDATA)0x00000001)
#define J9MODRON_OBM_SLOT_LAST_SLOT ((UDATA)0x80000000)
#endif /* J9VM_ENV_DATA64 */

/**
 * Run the sweep task.
 * Skeletal code to run the sweep task per work thread.  No actual work done.
 */
void
MM_ParallelSweepVLHGCTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	_sweepScheme->internalSweep(env);
}

/**
 * Initialize sweep statistics per work thread at the beginning of the sweep task.
 */
void
MM_ParallelSweepVLHGCTask::setup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	if (!env->isMainThread()) {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	} else {
		Assert_MM_true(_cycleState == env->_cycleState);
	}

	env->_sweepVLHGCStats.clear();
	
	/* record that this thread is participating in this cycle */
	env->_sweepVLHGCStats._gcCount = MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount;

	env->_freeEntrySizeClassStats.resetCounts();
}

/**
 * Gather sweep statistics into the global statistics counter at the end of the sweep task.
 */
void
MM_ParallelSweepVLHGCTask::cleanup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._sweepStats.merge(&env->_sweepVLHGCStats);
	if(!env->isMainThread()) {
		env->_cycleState = NULL;
	}
	
	Trc_MM_ParallelSweepVLHGCTask_parallelStats(
		env->getLanguageVMThread(),
		(U_32)env->getWorkerID(), 
		(U_32)j9time_hires_delta(0, env->_sweepVLHGCStats.idleTime, J9PORT_TIME_DELTA_IN_MILLISECONDS), 
		env->_sweepVLHGCStats.sweepChunksProcessed, 
		(U_32)j9time_hires_delta(0, env->_sweepVLHGCStats.mergeTime, J9PORT_TIME_DELTA_IN_MILLISECONDS));
}

void
MM_ParallelSweepVLHGCTask::mainSetup(MM_EnvironmentBase *env)
{
	_sweepScheme->setCycleState(_cycleState);
}

void
MM_ParallelSweepVLHGCTask::mainCleanup(MM_EnvironmentBase *envBase)
{
	/* now that sweep is finished, walk the list of regions and recycle any which are completely empty */
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	_sweepScheme->recycleFreeRegions(env);
	_cycleState->_noCompactionAfterSweep = false;
	_sweepScheme->clearCycleState();
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
/**
 * Stats gathering for synchronizing threads during sweep.
 */
void
MM_ParallelSweepVLHGCTask::synchronizeGCThreads(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 startTime = j9time_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_sweepVLHGCStats.addToIdleTime(startTime, endTime);
}

/**
 * Stats gathering for synchronizing threads during sweep.
 */
bool
MM_ParallelSweepVLHGCTask::synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 startTime = j9time_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_sweepVLHGCStats.addToIdleTime(startTime, endTime);
	
	return result;
}

bool
MM_ParallelSweepVLHGCTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 startTime = j9time_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_sweepVLHGCStats.addToIdleTime(startTime, endTime);

	return result;
}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

MM_ParallelSweepSchemeVLHGC::MM_ParallelSweepSchemeVLHGC(MM_EnvironmentVLHGC *env)
	: _chunksPrepared(0)
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _dispatcher(_extensions->dispatcher)
	, _cycleState()
	, _currentSweepBits(NULL)
	, _regionManager(_extensions->getHeap()->getHeapRegionManager())
	, _heapBase(NULL)
	, _sweepHeapSectioning(NULL)
	, _poolSweepPoolState(NULL)
	, _mutexSweepPoolState(NULL)
{
	_typeId = __FUNCTION__;
}

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_ParallelSweepSchemeVLHGC *
MM_ParallelSweepSchemeVLHGC::newInstance(MM_EnvironmentVLHGC *env)
{
	MM_ParallelSweepSchemeVLHGC *sweepScheme;
	
	sweepScheme = (MM_ParallelSweepSchemeVLHGC *)env->getForge()->allocate(sizeof(MM_ParallelSweepSchemeVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (sweepScheme) {
		new(sweepScheme) MM_ParallelSweepSchemeVLHGC(env);
		if (!sweepScheme->initialize(env)) { 
        	sweepScheme->kill(env);        
        	sweepScheme = NULL;            
		}                                       
	}

	return sweepScheme;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_ParallelSweepSchemeVLHGC::kill(MM_EnvironmentVLHGC *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize any internal structures.
 * @return true if initialization is successful, false otherwise.
 */
bool
MM_ParallelSweepSchemeVLHGC::initialize(MM_EnvironmentVLHGC *env)
{
	_sweepHeapSectioning = MM_SweepHeapSectioningVLHGC::newInstance(env);

	if(NULL == _sweepHeapSectioning) {
		return false;
	}

	if (0 != omrthread_monitor_init_with_name(&_mutexSweepPoolState, 0, "SweepPoolState Monitor")) {
		return false;
	}

	return true;
}

/**
 * Tear down internal structures.
 */
void
MM_ParallelSweepSchemeVLHGC::tearDown(MM_EnvironmentVLHGC *env)
{
	if(NULL != _sweepHeapSectioning) {
		_sweepHeapSectioning->kill(env);
		_sweepHeapSectioning = NULL;
	}

	if (NULL != _poolSweepPoolState) {
		pool_kill(_poolSweepPoolState);
		_poolSweepPoolState = NULL;
	}

	if (0 != _mutexSweepPoolState) {
		omrthread_monitor_destroy(_mutexSweepPoolState);
	}
}

/**
 * Request to create sweepPoolState class for pool
 * @param  memoryPool memory pool to attach sweep state to
 * @return pointer to created class
 */
void *
MM_ParallelSweepSchemeVLHGC::createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	omrthread_monitor_enter(_mutexSweepPoolState);
	if (NULL == _poolSweepPoolState) {
		_poolSweepPoolState = pool_new(sizeof(MM_SweepPoolState), 0, 2 * sizeof(UDATA), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(PORTLIB));
		if (NULL == _poolSweepPoolState) {
			omrthread_monitor_exit(_mutexSweepPoolState);
			return NULL;
		}
	}
	omrthread_monitor_exit(_mutexSweepPoolState);
	
	return MM_SweepPoolState::newInstance(env, _poolSweepPoolState, _mutexSweepPoolState, memoryPool);
}

/**
 * Request to destroy sweepPoolState class for pool
 * @param  sweepPoolState class to destroy
 */
void
MM_ParallelSweepSchemeVLHGC::deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState)
{
	((MM_SweepPoolState *)sweepPoolState)->kill(env, _poolSweepPoolState, _mutexSweepPoolState);
}

/**
 * Get the sweep scheme state for the given memory pool.
 */
MM_SweepPoolState *
MM_ParallelSweepSchemeVLHGC::getPoolState(MM_MemoryPool *memoryPool)
{
	MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
	return sweepPoolManager->getPoolState(memoryPool);
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * 
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @return true if operation completes with success (always)
 */
bool
MM_ParallelSweepSchemeVLHGC::heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress)
{
	/* this method is called too often in some configurations (ie: Tarok) so we update the sectioning table in heapReconfigured */
	return true;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * 
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * @return true if operation completes with success (always)
 */
bool
MM_ParallelSweepSchemeVLHGC::heapRemoveRange(
	MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress,
	void *lowValidAddress, void *highValidAddress)
{
	/* this method is called too often in some configurations (ie: Tarok) so we update the sectioning table in heapReconfigured */
	return true;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * No new memory has been added to a heap reconfiguration.  This call typically is the result
 * of having segment range changes (memory redistributed between segments) or the meaning of
 * memory changed.
 * 
 */
void
MM_ParallelSweepSchemeVLHGC::heapReconfigured(MM_EnvironmentVLHGC *env)
{
	_sweepHeapSectioning->update(env);
}

/**
 * Initialize all internal structures in order to perform a parallel sweep.
 * 
 * @note called by the main thread only
 */
void
MM_ParallelSweepSchemeVLHGC::setupForSweep(MM_EnvironmentVLHGC *env)
{
	/* the markMap uses the heapBase() (not the activeHeapBase()) for its calculations. We must use the same base. */
	_heapBase = _extensions->heap->getHeapBase();
}


/****************************************
 * Core sweep functionality
 ****************************************
 */

MMINLINE void
MM_ParallelSweepSchemeVLHGC::sweepMarkMapBody(
	UDATA * &markMapCurrent,
	UDATA * &markMapChunkTop,
	UDATA * &markMapFreeHead,
	UDATA &heapSlotFreeCount,
	UDATA * &heapSlotFreeCurrent,
	UDATA * &heapSlotFreeHead )
{
	if(*markMapCurrent == J9MODRON_OBM_SLOT_EMPTY) {
		markMapFreeHead = markMapCurrent;
		heapSlotFreeHead = heapSlotFreeCurrent;

		markMapCurrent += 1;

		while(markMapCurrent < markMapChunkTop) {
			if(*markMapCurrent != J9MODRON_OBM_SLOT_EMPTY) {
				break;
			}
			markMapCurrent += 1;
		}

		/* Find the number of slots we've walked
		 * (pointer math makes this the number of slots)
		 */
		UDATA count = markMapCurrent - markMapFreeHead;
		heapSlotFreeCount = J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * count;
		heapSlotFreeCurrent += J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * count;
	}
}

MMINLINE void
MM_ParallelSweepSchemeVLHGC::sweepMarkMapHead(
	UDATA *markMapFreeHead,
	UDATA *markMapChunkBase,
	UDATA * &heapSlotFreeHead,
	UDATA &heapSlotFreeCount )
{
	UDATA markMapHeadValue;
	UDATA markMapFreeBitIndexHead;

	if(markMapFreeHead > markMapChunkBase) {
		markMapHeadValue = *(markMapFreeHead - 1);
		markMapFreeBitIndexHead = J9MODRON_HEAP_SLOTS_PER_MARK_BIT * MM_Bits::trailingZeroes(markMapHeadValue);
		if(markMapFreeBitIndexHead) {
			heapSlotFreeHead -= markMapFreeBitIndexHead;
			heapSlotFreeCount += markMapFreeBitIndexHead;
		}
	}
}

MMINLINE void
MM_ParallelSweepSchemeVLHGC::sweepMarkMapTail(
	UDATA *markMapCurrent,
	UDATA *markMapChunkTop,
	UDATA &heapSlotFreeCount )
{
	UDATA markMapTailValue;
	UDATA markMapFreeBitIndexTail;

	if(markMapCurrent < markMapChunkTop) {
		markMapTailValue = *markMapCurrent;
		markMapFreeBitIndexTail = J9MODRON_HEAP_SLOTS_PER_MARK_BIT * MM_Bits::leadingZeroes(markMapTailValue);

		if(markMapFreeBitIndexTail) {
			heapSlotFreeCount += markMapFreeBitIndexTail;
		}
	}
}

UDATA
MM_ParallelSweepSchemeVLHGC::measureAllDarkMatter(MM_EnvironmentVLHGC *env, MM_ParallelSweepChunk *sweepChunk)
{
	const UDATA minimumFreeEntrySize = sweepChunk->memoryPool->getMinimumFreeEntrySize();
	UDATA * const startAddress = (UDATA *)sweepChunk->chunkBase;
	UDATA * const endAddress = (UDATA *)sweepChunk->chunkTop;
		
	MM_HeapMapIterator markedObjectIterator(_extensions, _cycleState._markMap, startAddress, endAddress);
	
	/* Hole at the beginning of the chunk is not considered, since we do not know 
	 * if that's part of the object from previous chunk or a part of a real hole.
	 * Similarly, the hole at the end of the sampled heap is not considered, since we do not
	 * know if it will be part of a real free entry or not
	 */
	UDATA sumOfHoleSizes = 0;
	J9Object *prevObject = markedObjectIterator.nextObject();
	if (NULL != prevObject) {
		UDATA prevObjectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(prevObject);
		
		J9Object *object = NULL;
		while (NULL != (object = markedObjectIterator.nextObject())) {
			UDATA holeSize = (UDATA)object - ((UDATA)prevObject + prevObjectSize);
			if (holeSize < minimumFreeEntrySize) {
				sumOfHoleSizes += holeSize;
			}
			prevObject = object;
			prevObjectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(prevObject);
		}			
	}

	Assert_MM_true(sumOfHoleSizes < ((UDATA)endAddress - (UDATA)startAddress));
	
	return sumOfHoleSizes;
}

UDATA
MM_ParallelSweepSchemeVLHGC::performSamplingCalculations(MM_ParallelSweepChunk *sweepChunk, UDATA* markMapCurrent, UDATA* heapSlotFreeCurrent)
{
	const UDATA minimumFreeEntrySize = sweepChunk->memoryPool->getMinimumFreeEntrySize();
	UDATA darkMatter = 0;
	
	/* this word has objects in it. Sample them for dark matter */
	MM_HeapMapWordIterator markedObjectIterator(*markMapCurrent, heapSlotFreeCurrent);
	
	/* Hole at the beginning of the sample is not considered, since we do not know 
	 * if that's part of a preceding object or part of hole.
	 */
	J9Object *prevObject = markedObjectIterator.nextObject();
	Assert_MM_true(NULL != prevObject);
	UDATA prevObjectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(prevObject);
	/* Any object we visit for dark matter estimate, we'll consider for other estimates too (for example scannable vs non-scannable ratio) */
	if (GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT == _extensions->objectModel.getScanType(prevObject)) {
		sweepChunk->_nonScannableBytes += prevObjectSize;
	} else {
		sweepChunk->_scannableBytes += prevObjectSize;
	}

	J9Object *object = NULL;
	while (NULL != (object = markedObjectIterator.nextObject())) {
		UDATA holeSize = (UDATA)object - ((UDATA)prevObject + prevObjectSize);
		Assert_MM_true(holeSize < minimumFreeEntrySize);
		darkMatter += holeSize;
		prevObject = object;
		prevObjectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(prevObject);
		if (GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT == _extensions->objectModel.getScanType(prevObject)) {
			sweepChunk->_nonScannableBytes += prevObjectSize;
		} else {
			sweepChunk->_scannableBytes += prevObjectSize;
		}
	}
	
	/* find the trailing dark matter */
	UDATA * endOfPrevObject = (UDATA*)((UDATA)prevObject + prevObjectSize);
	UDATA * startSearchAt = (UDATA*)MM_Math::roundToFloor(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(UDATA), (UDATA)endOfPrevObject);
	UDATA * endSearchAt = (UDATA*)MM_Math::roundToCeiling(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(UDATA), (UDATA)endOfPrevObject + minimumFreeEntrySize);
	startSearchAt = OMR_MAX(startSearchAt, heapSlotFreeCurrent + J9MODRON_HEAP_SLOTS_PER_MARK_SLOT);
	endSearchAt = OMR_MIN(endSearchAt, (UDATA*)sweepChunk->chunkTop);
	if (startSearchAt < endSearchAt) {
		while ( startSearchAt < endSearchAt ) {
			MM_HeapMapWordIterator nextMarkedObjectIterator(_cycleState._markMap, startSearchAt);
			J9Object *nextObject = nextMarkedObjectIterator.nextObject();
			if (NULL != nextObject) {
				UDATA holeSize = (UDATA)nextObject - (UDATA)endOfPrevObject;
				if (holeSize < minimumFreeEntrySize) {
					darkMatter += holeSize;
				}
				break;
			}
			startSearchAt += J9MODRON_HEAP_SLOTS_PER_MARK_SLOT;
		}
	} else if (endSearchAt > endOfPrevObject) {
		darkMatter += (UDATA)endSearchAt - (UDATA)endOfPrevObject;
	}

	return darkMatter;
}

/**
 * Sweep the range of memory described by the chunk.
 * Given a properly initialized chunk (zeroed + subspace and range information) sweep the area to include
 * inner free list information as well as surrounding details (leading/trailing, projection, etc).  The routine
 * is self contained to run on an individual chunk, and does not rely on surrounding chunks having been processed.
 * @note a chunk can only be processed by a single thread (single threaded routine)
 * @return return TRUE if live objects found in chunk; FALSE otherwise
 */
bool
MM_ParallelSweepSchemeVLHGC::sweepChunk(MM_EnvironmentVLHGC *env, MM_ParallelSweepChunk *sweepChunk)
{
	UDATA *heapSlotFreeCurrent;  /* Current heap slot in chunk being processed */
	UDATA *markMapChunkBase;  /* Mark map base pointer for chunk */
	UDATA *markMapChunkTop;  /* Mark map top pointer for chunk */
	UDATA *markMapCurrent;  /* Current mark map slot being processed in chunk */
	UDATA *heapSlotFreeHead;  /* Heap free slot pointer for head */
	UDATA heapSlotFreeCount;  /* Size in slots of heap free range */
	UDATA *markMapFreeHead;  /* Mark map free slots for head and tail */
	bool liveObjectFound = false; /* Set if we find at least one live object in chunk */
	MM_SweepPoolManager *sweepPoolManager = sweepChunk->memoryPool->getSweepPoolManager();
	const UDATA darkMatterSampleRate = (0 == _extensions->darkMatterSampleRate)?UDATA_MAX:_extensions->darkMatterSampleRate;

	/* Set up range limits */
	heapSlotFreeCurrent = (UDATA *)sweepChunk->chunkBase;

	markMapChunkBase = (UDATA *) (_currentSweepBits 
		+ (MM_Math::roundToFloor(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(UDATA), (UDATA)sweepChunk->chunkBase - (UDATA)_heapBase) / J9MODRON_HEAP_SLOTS_PER_MARK_SLOT) );
	markMapChunkTop = (UDATA *) (_currentSweepBits 
		+ (MM_Math::roundToFloor(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(UDATA), ((UDATA)sweepChunk->chunkTop) - ((UDATA)_heapBase)) / J9MODRON_HEAP_SLOTS_PER_MARK_SLOT) );
	markMapCurrent = markMapChunkBase;

	/* Be sure that chunk is just initialized */
	Assert_MM_true(NULL == sweepChunk->freeListTail);

	/* Process the leading free entry */
	heapSlotFreeHead = NULL;
	heapSlotFreeCount = 0;
	sweepMarkMapBody(markMapCurrent, markMapChunkTop, markMapFreeHead, heapSlotFreeCount, heapSlotFreeCurrent, heapSlotFreeHead);
	sweepMarkMapTail(markMapCurrent, markMapChunkTop, heapSlotFreeCount);
	if(0 != heapSlotFreeCount) {
		/* Quick fixup if there was no full free map entries at the head */
		if(NULL == heapSlotFreeHead) {
			heapSlotFreeHead = heapSlotFreeCurrent;
		}

		/* Chunk borders must be multiple of J9MODRON_HEAP_SLOTS_PER_MARK_SLOT */
		Assert_MM_true ((UDATA*)sweepChunk->chunkBase == heapSlotFreeHead);
		
		sweepPoolManager->addFreeMemory(env, sweepChunk, heapSlotFreeHead, heapSlotFreeCount);
	}

	/* If we get here and we have not processed the whole chunk then
	 * there must be at least one live object in the chunk.
	 */	
	if (markMapCurrent < markMapChunkTop) {
		liveObjectFound = true;
	}	

	UDATA darkMatterBytes = 0;
	UDATA darkMatterCandidates = 0;
	UDATA darkMatterSamples = 0;
	
	/* Process inner chunks */
	heapSlotFreeHead = NULL;
	heapSlotFreeCount = 0;
	while(markMapCurrent < markMapChunkTop) {
		/* Check if the map slot is part of a candidate free list entry */
		sweepMarkMapBody(markMapCurrent, markMapChunkTop, markMapFreeHead, heapSlotFreeCount, heapSlotFreeCurrent, heapSlotFreeHead);
		if(0 == heapSlotFreeCount) {
			darkMatterCandidates += 1;
			if (0 == (darkMatterCandidates % darkMatterSampleRate) ) {
				darkMatterBytes += performSamplingCalculations(sweepChunk, markMapCurrent, heapSlotFreeCurrent);
				darkMatterSamples += 1;
			}
		} else {
			/* There is at least a single free slot in the mark map - check the head and tail */
			sweepMarkMapHead(markMapFreeHead, markMapChunkBase, heapSlotFreeHead, heapSlotFreeCount);
			sweepMarkMapTail(markMapCurrent, markMapChunkTop, heapSlotFreeCount);

			if (!sweepPoolManager->addFreeMemory(env, sweepChunk, heapSlotFreeHead, heapSlotFreeCount)) {
				break;
			}

			/* Reset the free entries for the next body */
			heapSlotFreeHead = NULL;
			heapSlotFreeCount = 0;
		}

		/* Proceed to the next map slot */
		heapSlotFreeCurrent += J9MODRON_HEAP_SLOTS_PER_MARK_SLOT;
		markMapCurrent += 1;
	}

	/* Process the trailing free entry - The body processing will handle trailing entries that cover a map slot or more */
	if(*(markMapCurrent - 1) != J9MODRON_OBM_SLOT_EMPTY) {
		heapSlotFreeCount = 0;
		heapSlotFreeHead = heapSlotFreeCurrent;
		sweepMarkMapHead(markMapCurrent, markMapChunkBase, heapSlotFreeHead, heapSlotFreeCount);

		/* Update the sweep chunk table entry with the trailing free information */
		sweepPoolManager->updateTrailingFreeMemory(env, sweepChunk, heapSlotFreeHead, heapSlotFreeCount);
	}
	
	if (darkMatterSamples == 0) {
		/* No samples were taken, so no dark matter was found (avoid division by zero) */
		sweepChunk->_darkMatterBytes = 0;
		sweepChunk->_darkMatterSamples = 0;
	} else {
		Assert_MM_true(darkMatterCandidates >= darkMatterSamples);
		double projectionFactor = (double)darkMatterCandidates / (double)darkMatterSamples;
		UDATA projectedDarkMatter = (UDATA)((double)darkMatterBytes * projectionFactor);
		UDATA chunkSize = (UDATA)sweepChunk->chunkTop - (UDATA)sweepChunk->chunkBase;
		UDATA freeSpace = sweepChunk->freeBytes + sweepChunk->leadingFreeCandidateSize + sweepChunk->trailingFreeCandidateSize;
		Assert_MM_true(freeSpace <= chunkSize);
		sweepChunk->_darkMatterSamples = darkMatterSamples;

		if (projectedDarkMatter >= (chunkSize - freeSpace)) {
			/* We measured more than 100% dark matter in the sample.
			 * This can happen due to trailing dark matter and due to holes in some of the candidates.
			 * It usually only happens for very small samples, and will cause us to estimate more dark 
			 * matter than is possible. In this case, don't apply the projection. Just record what we 
			 * actually measured, since this is an accurate lower bound.
			 */
			sweepChunk->_darkMatterBytes = darkMatterBytes;
		} else {
			sweepChunk->_darkMatterBytes = projectedDarkMatter;
		}
	}
	
	/* This is a level 9 tracepoint because it is very expensive. Enable using -Xtrace:iprint=j9mm{darkMatterComparison} */
	Trc_MM_ParallelSweepScheme_sweepChunk_darkMatterComparison(env->getLanguageVMThread(),
			sweepChunk->chunkBase, 
			sweepChunk->chunkTop,
			darkMatterSamples,
			darkMatterCandidates,
			sweepChunk->_darkMatterBytes,
			measureAllDarkMatter(env, sweepChunk));
	
	return liveObjectFound;
}

/**
 * Prepare the chunk list for sweeping.
 * Given the current state of the heap, prepare the chunk list such that all entries are assign
 * a range of memory (and all memory has a chunk associated to it), sectioning the heap based on
 * the configuration, as well as configuration rules.
 * @return the total number of chunks in the system.
 */
UDATA
MM_ParallelSweepSchemeVLHGC::prepareAllChunks(MM_EnvironmentVLHGC *env)
{
	return _sweepHeapSectioning->reassignChunks(env);
}

/**
 * Sweep all chunks.
 * 
 * @param totalChunkCount total number of chunks to be swept
 */
void
MM_ParallelSweepSchemeVLHGC::sweepAllChunks(MM_EnvironmentVLHGC *env, UDATA totalChunkCount)
{
	
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	UDATA chunksProcessed = 0; /* Chunks processed by this thread */
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MM_ParallelSweepChunk *chunk;
	MM_ParallelSweepChunk *prevChunk = NULL;
	MM_SweepHeapSectioningIterator sectioningIterator(_sweepHeapSectioning);

	for (UDATA chunkNum = 0; chunkNum < totalChunkCount; chunkNum++) {
		
		chunk = sectioningIterator.nextChunk();
			
		Assert_MM_true (chunk != NULL);  /* Should never return NULL */
		
if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)                           
			chunksProcessed += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

 			/* if we are changing memory pool, flush the thread local stats to appropriate (previous) pool */
			if ((NULL != prevChunk) && (prevChunk->memoryPool != chunk->memoryPool)) {
				prevChunk->memoryPool->getLargeObjectAllocateStats()->getFreeEntrySizeClassStats()->mergeLocked(&env->_freeEntrySizeClassStats);
			}

 			/* if we are starting or changing memory pool, setup frequent allocation sizes in free entry stats for the pool we are about to sweep */
			if ((NULL == prevChunk) || (prevChunk->memoryPool != chunk->memoryPool)) {
				env->_freeEntrySizeClassStats.initializeFrequentAllocation(chunk->memoryPool->getLargeObjectAllocateStats());
			}

			/* Sweep the chunk */
			sweepChunk(env, chunk);
			prevChunk = chunk;
		}	
	}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	env->_sweepVLHGCStats.sweepChunksProcessed = chunksProcessed;
	env->_sweepVLHGCStats.sweepChunksTotal = totalChunkCount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/* flush the remaining stats (since the the last pool switch) */
	if (NULL != prevChunk) {
		prevChunk->memoryPool->getLargeObjectAllocateStats()->getFreeEntrySizeClassStats()->mergeLocked(&env->_freeEntrySizeClassStats);
	}
	
}

/**
 * Connect a chunk into the free list.
 * Given a previously swept chunk, connect its data to the free list of the associated memory subspace.
 * This requires that all previous chunks in the memory subspace have been swept.
 * @note It is expected that the sweep state free information is updated properly.
 * @note the free list is not guaranteed to be in a consistent state at the end of the routine
 * @note a chunk can only be connected by a single thread (single threaded routine)
 * @todo add the general algorithm to the comment header.
 */
void
MM_ParallelSweepSchemeVLHGC::connectChunk(MM_EnvironmentVLHGC *env, MM_ParallelSweepChunk *chunk)
{
	MM_SweepPoolManager *sweepPoolManager = chunk->memoryPool->getSweepPoolManager();
	sweepPoolManager->connectChunk(env, chunk);
}

void
MM_ParallelSweepSchemeVLHGC::connectAllChunks(MM_EnvironmentVLHGC *env, UDATA totalChunkCount)
{
	/* Initialize all sweep states for sweeping */
	initializeSweepStates(env);
	
	/* Walk the sweep chunk table connecting free lists */
	MM_ParallelSweepChunk *sweepChunk;
	MM_SweepHeapSectioningIterator sectioningIterator(_sweepHeapSectioning);

	for (UDATA chunkNum = 0; chunkNum < totalChunkCount; chunkNum++) {
		sweepChunk = sectioningIterator.nextChunk();
		Assert_MM_true(sweepChunk != NULL);  /* Should never return NULL */

		connectChunk(env, sweepChunk);
	}
}

/**
 * Initialize all global garbage collect sweep information in all memory pools
 * @note This routine should only be called once for every sweep cycle.
 */
void
MM_ParallelSweepSchemeVLHGC::initializeSweepStates(MM_EnvironmentBase *envModron)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (!region->_sweepData._alreadySwept && region->hasValidMarkMap()) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);

			MM_SweepPoolState *sweepState = getPoolState(memoryPool);
			Assert_MM_true(NULL != sweepState);

			sweepState->initializeForSweep(envModron);
		}
	}
}

/**
 * Flush any unaccounted for free entries to the free list.
 */
void
MM_ParallelSweepSchemeVLHGC::flushFinalChunk(
	MM_EnvironmentBase *envModron,
	MM_MemoryPool *memoryPool)
{
	MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
	sweepPoolManager->flushFinalChunk(envModron, memoryPool);
}

/**
 * Flush out any remaining free list data from the last chunk processed for every memory subspace.
 */
void
MM_ParallelSweepSchemeVLHGC::flushAllFinalChunks(MM_EnvironmentBase *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (!region->_sweepData._alreadySwept && region->hasValidMarkMap()) {
			if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_MemoryPool *memoryPool = region->getMemoryPool();
				Assert_MM_true(NULL != memoryPool);
	
				MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
				Assert_MM_true(NULL != sweepPoolManager);
				
				/* Find any unaccounted for free entries and flush them to the free list */
				sweepPoolManager->flushFinalChunk(env, memoryPool);
				sweepPoolManager->connectFinalChunk(env, memoryPool);

				/* clear card table if empty */
				if (region->getSize() == memoryPool->getActualFreeMemorySize()) {
					void *low = region->getLowAddress();
					void *high = region->getHighAddress();
					MM_CardTable *cardTable = _extensions->cardTable;
					Card *card = cardTable->heapAddrToCardAddr(env, low);
					Card *toCard = cardTable->heapAddrToCardAddr(env, high);
					memset(card, CARD_CLEAN, (UDATA)toCard - (UDATA)card);
				}
			}
		}
	}
}

/**
 * Perform a basic sweep operation.
 * Called by all work threads from a task, performs a full sweep on all memory subspaces.
 * 
 * @note Do not call directly - used by the dispatcher for work threads.
 */
void
MM_ParallelSweepSchemeVLHGC::internalSweep(MM_EnvironmentVLHGC *env)
{
	/* main thread does initialization */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMain(env, UNIQUE_ID)) {
		/* Reset all memory pools within the sweep set as a precursor to sweeping. */
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			if (!region->_sweepData._alreadySwept && region->hasValidMarkMap()) {
				region->getMemoryPool()->reset(MM_MemoryPool::forSweep);
			}
		}

		/* Reset largestFreeEntry of all subSpaces at beginning of sweep */
		_extensions->heap->resetLargestFreeEntry();
		_currentSweepBits = (U_8 *)_cycleState._markMap->getMarkBits();

		_chunksPrepared = prepareAllChunks(env);
		
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	/* ..all threads now join in to do actual sweep */
	sweepAllChunks(env, _chunksPrepared);
	
	/* ..and then main thread finishes off by connecting all the chunks */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMain(env, UNIQUE_ID)) {
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		U_64 mergeStartTime, mergeEndTime;
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		
		mergeStartTime = j9time_hires_clock();
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

		connectAllChunks(env, _chunksPrepared);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		mergeEndTime = j9time_hires_clock();
		env->_sweepVLHGCStats.addToMergeTime(mergeStartTime, mergeEndTime);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	/* Walk all memory spaces flushing the previous free entry and clearing cards for every fully-empty region */
	flushAllFinalChunks(env);
}

/**
 * Perform a basic sweep operation.
 * There is no expectation for amount of work done for this routine.  The receiver is entitled to do as much or as little
 * work towards completing the sweep as it wants.  In this case, a full sweep of all memory spaces will be performed.
 * 
 * @param cycleState Cycle state used for any operation relative to the current collection cycle.
 *
 * @note Expect to have the dispatcher and worker threads available for work
 * @note Expect to have exclusive access
 * @note Expect to have a valid mark map for all live objects
 */
void
MM_ParallelSweepSchemeVLHGC::sweep(MM_EnvironmentVLHGC *env)
{
	setupForSweep(env);
	
	Assert_MM_true(NULL != env->_cycleState->_markMap);
	MM_ParallelSweepVLHGCTask sweepTask(env, _extensions->dispatcher, this, env->_cycleState);
	_extensions->dispatcher->run(env, &sweepTask);
	updateProjectedLiveBytesAfterSweep(env);
}

/**
 * Complete any sweep work after a basic sweep operation.
 * Completing the sweep is a noop - the basic sweep operation consists of a full sweep.
 * 
 * @note Expect to have the dispatcher and worker threads available for work
 * @note Expect to have exclusive access
 * @note Expect to have a valid mark map for all live objects
 * @note Expect basic sweep work to have been completed
 * 
 * @param Reason code to identify why completion of concurrent sweep is required.
 */
void
MM_ParallelSweepSchemeVLHGC::completeSweep(MM_EnvironmentBase *env, SweepCompletionReason reason)
{
	/* No work - basic sweep will have done all the work */
}

/**
 * Sweep and connect the heap until a free entry of the specified size is found.
 * Sweeping for a minimum size involves completing a full sweep (there is no incremental operation) then
 * evaluating whether the minimum free size was found.
 * 
 * @note Expects to have exclusive access
 * @note Expects to have control over the parallel GC threads (ie: able to dispatch tasks)
 * @note This only sweeps spaces that are concurrent sweepable
 * @return true if a free entry of at least the requested size was found, false otherwise.
 */
bool
MM_ParallelSweepSchemeVLHGC::sweepForMinimumSize(
	MM_EnvironmentBase *env, 
	MM_MemorySubSpace *baseMemorySubSpace, 
	MM_AllocateDescription *allocateDescription)
{
	sweep(MM_EnvironmentVLHGC::getEnvironment(env));
	if (allocateDescription) {
		UDATA minimumFreeSize =  allocateDescription->getBytesRequested();
		return minimumFreeSize <= baseMemorySubSpace->findLargestFreeEntry(env, allocateDescription);
	} else { 
		return true;
	}	
}

#if defined(J9VM_GC_CONCURRENT_SWEEP)
/**
 * Replenish a pools free lists to satisfy a given allocate.
 * The given pool was unable to satisfy an allocation request of (at least) the given size.  See if there is work
 * that can be done to increase the free stores of the pool so that the request can be met.
 * @note This call is made under the pools allocation lock (or equivalent)
 * @note The base implementation has completed all work, nothing further can be done.
 * @return True if the pool was replenished with a free entry that can satisfy the size, false otherwise.
 */
bool
MM_ParallelSweepSchemeVLHGC::replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, UDATA size)
{
	return false;
}
#endif /* J9VM_GC_CONCURRENT_SWEEP */

void
MM_ParallelSweepSchemeVLHGC::recycleFreeRegions(MM_EnvironmentVLHGC *env)
{
	/* now, see if we can free up any arraylet leaves due to dead spines */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	
	while(NULL != (region = regionIterator.nextRegion())) {
		/* Region must be marked for sweep */
		if (!region->_sweepData._alreadySwept && region->hasValidMarkMap()) {
			MM_MemoryPool *regionPool = region->getMemoryPool();
			Assert_MM_true(NULL != regionPool);
			MM_HeapRegionDescriptorVLHGC *walkRegion = region;
			MM_HeapRegionDescriptorVLHGC *next = walkRegion->_allocateData.getNextArrayletLeafRegion();
			/* Try to walk list from this head */
			while (NULL != (walkRegion = next)) {
				Assert_MM_true(walkRegion->isArrayletLeaf());
				J9Object *spineObject = (J9Object *)walkRegion->_allocateData.getSpine();
				next = walkRegion->_allocateData.getNextArrayletLeafRegion();
				Assert_MM_true( region->isAddressInRegion(spineObject) );
				if (!_cycleState._markMap->isBitSet(spineObject)) {
					/* Arraylet is dead */

					/* remove arraylet leaf from list */
					walkRegion->_allocateData.removeFromArrayletLeafList(env);

					/* recycle */
					walkRegion->_allocateData.setSpine(NULL);

					walkRegion->getSubSpace()->recycleRegion(env, walkRegion);
				}
			}

			/* recycle if empty */
			if (region->getSize() == regionPool->getActualFreeMemorySize()) {
				Assert_MM_true(NULL == region->_allocateData.getSpine());
				Assert_MM_true(NULL == region->_allocateData.getNextArrayletLeafRegion());
				region->getSubSpace()->recycleRegion(env, region);
			}
		}
	}
}

void
MM_ParallelSweepSchemeVLHGC::updateProjectedLiveBytesAfterSweep(MM_EnvironmentVLHGC *env)
{
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::ALL);
	UDATA regionSize = _regionManager->getRegionSize();
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects() && !region->_sweepData._alreadySwept) {
			UDATA actualLiveBytes = regionSize - region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
			region->_projectedLiveBytesDeviation = actualLiveBytes - region->_projectedLiveBytes;
			region->_projectedLiveBytes = actualLiveBytes;
		}
	}
}
