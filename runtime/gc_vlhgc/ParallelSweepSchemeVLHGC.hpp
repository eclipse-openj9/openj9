
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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(PARALLELSWEEPSCHEMEVLHGC_HPP_)
#define PARALLELSWEEPSCHEMEVLHGC_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"
#include "modronopt.h"
#include "modronbase.h"

#include "Base.hpp"

#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "MemoryPool.hpp"
#include "ParallelTask.hpp"

class MM_AllocateDescription;
class MM_Dispatcher;
class MM_MarkMap;
class MM_MemoryPool;
class MM_MemorySubSpace;
class MM_ParallelSweepChunk;
class MM_ParallelSweepSchemeVLHGC;
class MM_SweepHeapSectioning;
class MM_SweepPoolManager;
class MM_SweepPoolState;

/**
 * Task to perform sweep.
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelSweepVLHGCTask : public MM_ParallelTask
{
private:
protected:
	MM_ParallelSweepSchemeVLHGC *_sweepScheme;
	MM_CycleState *_cycleState;  /**< Current cycle state used for tasks dispatched */

public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_SWEEP; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	
	void masterSetup(MM_EnvironmentBase *env);
	void masterCleanup(MM_EnvironmentBase *env);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/**
	 * Create a ParallelSweepTaskVLHGC object.
	 */
	MM_ParallelSweepVLHGCTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ParallelSweepSchemeVLHGC *sweepScheme, MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher),
		_sweepScheme(sweepScheme),
		_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	}
};

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelSweepSchemeVLHGC : public MM_BaseVirtual
{
/*
 * Data members
 */
private:
	UDATA _chunksPrepared; 
	MM_GCExtensions *_extensions;
	MM_Dispatcher *_dispatcher;
	MM_CycleState _cycleState;  /**< Current cycle state information used to formulate receiver state for any operations  */
	U_8 *_currentSweepBits;	/**< The base address of the raw bits used by the _currentMarkMap (sweep knows about this in order to perform some optimized types of map walking) */
	MM_HeapRegionManager *_regionManager; /**< A cached pointer to the global heap region manager */

	void *_heapBase;

	MM_SweepHeapSectioning *_sweepHeapSectioning;	/**< pointer to Sweep Heap Sectioning */

	J9Pool *_poolSweepPoolState;				/**< Memory pools for SweepPoolState*/ 
	omrthread_monitor_t _mutexSweepPoolState;	/**< Monitor to protect memory pool operations for sweepPoolState*/
	
protected:
public:
	
/*
 * Function members
 */
private:
protected:
	
	virtual bool initialize(MM_EnvironmentVLHGC *env);
	virtual void tearDown(MM_EnvironmentVLHGC *env);

	void initializeSweepChunkTable(MM_EnvironmentVLHGC *env);

	void sweepMarkMapBody(UDATA * &markMapCurrent, UDATA * &markMapChunkTop, UDATA * &markMapFreeHead, UDATA &heapSlotFreeCount, UDATA * &heapSlotFreeCurrent, UDATA * &heapSlotFreeHead);
	void sweepMarkMapHead(UDATA *markMapFreeHead, UDATA *markMapChunkBase, UDATA * &heapSlotFreeHead, UDATA &heapSlotFreeCount);
	void sweepMarkMapTail(UDATA *markMapCurrent, UDATA *markMapChunkTop, UDATA &heapSlotFreeCount);

	bool sweepChunk(MM_EnvironmentVLHGC *env, MM_ParallelSweepChunk *sweepChunk);
	void sweepAllChunks(MM_EnvironmentVLHGC *env, UDATA totalChunkCount);
	UDATA prepareAllChunks(MM_EnvironmentVLHGC *env);

	/**
	 * Accurately measure the dark matter within the mark map UDATA beginning at heapSlotFreeCurrent.
	 * Leading dark matter (before the first marked object) is not included.
	 * Trailing dark matter (after the last marked object) is included, even if it falls outside of the
	 * mark map range.
	 * 
	 * For each heap visit, collect information whether the object is scannable.
	 *
	 * @param sweepChunk[in] the sweepChunk to be measured
	 * @param markMapCurrent[in] the address of the mark map word to be measured 
	 * @param heapSlotFreeCurrent[in] the first slot in the section of heap to be measured
	 * 
	 * @return the dark matter, measured in bytes, in the range 
	 */
	UDATA performSamplingCalculations(MM_ParallelSweepChunk *sweepChunk, UDATA* markMapCurrent, UDATA* heapSlotFreeCurrent);

	/**
	 * Accurately measure the dark matter within the specified sweepChunk.
	 *
	 * @param env[in] the current thread 
	 * @param sweepChunk[in] the sweepChunk to be measured
	 * 
	 * @return the dark matter, measured in bytes, in the chunk 
	 */
	UDATA measureAllDarkMatter(MM_EnvironmentVLHGC *env, MM_ParallelSweepChunk *sweepChunk);
	
	virtual void connectChunk(MM_EnvironmentVLHGC *env, MM_ParallelSweepChunk *chunk);
	void connectAllChunks(MM_EnvironmentVLHGC *env, UDATA totalChunkCount);

	void initializeSweepStates(MM_EnvironmentBase *env);

	void flushFinalChunk(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);
	/**
	 * Flushes and connects the final chunk of each region.
	 * If a given region is empty, this call will also clear its card table.
	 * @note Called in parallel
	 * @param env[in] A GC thread
	 */
	void flushAllFinalChunks(MM_EnvironmentBase *env);

	MMINLINE MM_Heap *getHeap() { return _extensions->heap; };

	void internalSweep(MM_EnvironmentVLHGC *env);
	
	virtual void setupForSweep(MM_EnvironmentVLHGC *env);

	void recycleFreeRegions(MM_EnvironmentVLHGC *env);

	static void hookMemoryPoolNew(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
	static void hookMemoryPoolKill(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

	void updateProjectedLiveBytesAfterSweep(MM_EnvironmentVLHGC *env);

public:
	static MM_ParallelSweepSchemeVLHGC *newInstance(MM_EnvironmentVLHGC *env); 
	virtual void kill(MM_EnvironmentVLHGC *env);
	
	/**
 	* Request to create sweepPoolState class for pool
 	* @param  memoryPool memory pool to attach sweep state to
 	* @return pointer to created class
 	*/
	virtual void *createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool);

	/**
 	* Request to destroy sweepPoolState class for pool
 	* @param  sweepPoolState class to destroy
 	*/
	virtual void deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState);

	virtual void sweep(MM_EnvironmentVLHGC *env);
	virtual void completeSweep(MM_EnvironmentBase *env, SweepCompletionReason reason);
	virtual bool sweepForMinimumSize(MM_EnvironmentBase *env, MM_MemorySubSpace *baseMemorySubSpace, MM_AllocateDescription *allocateDescription);

	MM_SweepPoolState *getPoolState(MM_MemoryPool *memoryPool);

	/**
	 * Set the current cycle state information.
	 */
	void setCycleState(MM_CycleState *cycleState) {
		_cycleState = *cycleState;
	}

	/**
	 * Clear the current cycle state information.
	 */
	void clearCycleState() {
		_cycleState = MM_CycleStateVLHGC();
	}

	bool heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(
		MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress,
		void *lowValidAddress, void *highValidAddress);
	/**
	 * Called after the heap geometry changes to allow any data structures dependent on this to be updated.
	 * This call could be triggered by memory ranges being added to or removed from the heap or memory being
	 * moved from one subspace to another.
	 * @param env[in] The thread which performed the change in heap geometry 
	 */
	void heapReconfigured(MM_EnvironmentVLHGC *env);

#if defined(J9VM_GC_CONCURRENT_SWEEP)
	virtual bool replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, UDATA size);
#endif /* J9VM_GC_CONCURRENT_SWEEP */

	/**
	 * Create a ParallelSweepSchemeVLHGC object.
	 */
	MM_ParallelSweepSchemeVLHGC(MM_EnvironmentVLHGC *env);

	friend class MM_ParallelSweepVLHGCTask;
};

#endif /* PARALLELSWEEPSCHEMEVLHGC_HPP_ */
