/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
 * @ingroup GC_Metronome
 */

#if !defined(REALTIMEGC_HPP_)
#define REALTIMEGC_HPP_

#include "CycleState.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "HeapRegionList.hpp"
#include "IncrementalOverflow.hpp"
#include "MemorySubSpace.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MetronomeDelegate.hpp"
#include "omrmodroncore.h"
#include "OMRVMInterface.hpp"
#include "Scheduler.hpp"
#include "WorkPacketsRealtime.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;
class MM_MemorySubSpaceMetronome;
class MM_RealtimeMarkingScheme;
class MM_SweepSchemeRealtime;
class MM_FreeHeapRegionList;

/**
 * @todo Provide class documentation
 * @ingroup GC_Metronome
 */
class MM_RealtimeGC : public MM_GlobalCollector
{
	/*
	 * Data members
	 */
protected:
	OMR_VM *_vm;
	MM_GCExtensionsBase *_extensions;

private:
	intptr_t _currentGCThreadPriority;
	bool _previousCycleBelowTrigger;
	bool _sweepingArraylets;

	uintptr_t _gcPhase; /**< What gc phase are we currently in? */
	
	MM_CycleState _cycleState;  /**< Embedded cycle state to be used as the master cycle state for GC activity */

	bool _moreTracingRequired; /**< Is used to decide if there needs to be another pass of the tracing loop. */

protected:
	MM_RealtimeMarkingScheme *_markingScheme; /**< The marking scheme used to mark objects. */
	MM_SweepSchemeRealtime *_sweepScheme; /**< The sweep scheme used to sweep objects for MemoryPoolSegregated. */

public:
	MM_MemoryPoolSegregated *_memoryPool;
	MM_MemorySubSpaceMetronome *_memorySubSpace;
	bool _allowGrowth;
	MM_OSInterface *_osInterface;
	MM_Scheduler *_sched;

	bool _fixHeapForWalk;
	float _avgPercentFreeHeapAfterCollect;
	MM_WorkPacketsRealtime *_workPackets;

	bool _stopTracing;
	MM_MetronomeDelegate _realtimeDelegate;

	/*
	 * Function members
	 */
private:
protected:
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, U_32 gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);
	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);
	
	void reportMarkStart(MM_EnvironmentBase *env);
	void reportMarkEnd(MM_EnvironmentBase *env);
	void reportSweepStart(MM_EnvironmentBase *env);
	void reportSweepEnd(MM_EnvironmentBase *env);
	void reportGCStart(MM_EnvironmentBase *env);
	void reportGCEnd(MM_EnvironmentBase *env);
	
public:
	void masterSetupForGC(MM_EnvironmentBase *env);
	void masterCleanupAfterGC(MM_EnvironmentBase *env);
	void doAuxiliaryGCWork(MM_EnvironmentBase *env); /* Wake up finalizer thread. Discard segments freed by class unloading. */
	void incrementalCollect(MM_EnvironmentRealtime *env);

	virtual void *createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool)
	{
		/*
		 *  This function does nothing and is designed to have implementation of abstract in GlobalCollector,
		 *  but must return non-NULL value to pass initialization - so, return "this"
		 */
		return this;
	}
	
	virtual void deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState)
	{
		/*
		 *  This function does nothing and is designed to have implementation of abstract in GlobalCollector
		 */
	}

	void traceAll(MM_EnvironmentBase *env);

	void clearGCStats();
	void mergeGCStats(MM_EnvironmentBase *env);

	void reportSyncGCStart(MM_EnvironmentBase *env, GCReason reason, uintptr_t reasonParameter);
	void reportSyncGCEnd(MM_EnvironmentBase *env);
	void reportGCCycleStart(MM_EnvironmentBase *env);
	void reportGCCycleEnd(MM_EnvironmentBase *env);
	void reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env);

	void flushCachesForGC(MM_EnvironmentBase *env) {
	    	/* we do not want to flush caches for each increment, unless there is a reason to walk the heap
	    	 * (for instance, fillFromOverflow requires heap be walkable) */
	    	if (_workPackets->getIncrementalOverflowHandler()->isOverflowThisGCCycle()) {
	    		GC_OMRVMInterface::flushCachesForGC(env);
	    	}
    	}
    
	/**
	 * helper function that determines if non-deterministic sweep is enabled
	 */
	bool shouldPerformNonDeterministicSweep()
	{
		return (_extensions->nonDeterministicSweep && !isCollectorSweepingArraylets());
	}

	virtual uintptr_t getVMStateID() { return OMRVMSTATE_GC_COLLECTOR_METRONOME; };

	static MM_RealtimeGC *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
	uintptr_t verbose(MM_EnvironmentBase *env);
	uintptr_t gcCount() { return  _extensions->globalGCStats.gcCount; }

	MMINLINE bool isBarrierEnabled()			{ return (_gcPhase == GC_PHASE_ROOT || _gcPhase == GC_PHASE_TRACE  || _gcPhase == GC_PHASE_CONCURRENT_TRACE); }

	MMINLINE bool isCollectorIdle()				{ return (_gcPhase == GC_PHASE_IDLE);	 }
	MMINLINE bool isCollectorRootMarking()		{ return (_gcPhase == GC_PHASE_ROOT);	 }
	MMINLINE bool isCollectorTracing()			{ return (_gcPhase == GC_PHASE_TRACE); }
	MMINLINE bool isCollectorUnloadingClassLoaders() { return (_gcPhase == GC_PHASE_UNLOADING_CLASS_LOADERS); }
	MMINLINE bool isCollectorSweeping()			{ return (_gcPhase == GC_PHASE_SWEEP); }
	MMINLINE bool isCollectorConcurrentTracing() { return (_gcPhase == GC_PHASE_CONCURRENT_TRACE); }
	MMINLINE bool isCollectorConcurrentSweeping() { return (_gcPhase == GC_PHASE_CONCURRENT_SWEEP); }	
	MMINLINE bool isCollectorSweepingArraylets() { return _sweepingArraylets; }
	MMINLINE bool isFixHeapForWalk() { return _fixHeapForWalk; }	

	MMINLINE void setCollectorIdle()			{ _gcPhase = GC_PHASE_IDLE; _sched->_gcPhaseSet |= GC_PHASE_IDLE; }
	MMINLINE void setCollectorRootMarking()		{ _gcPhase = GC_PHASE_ROOT; _sched->_gcPhaseSet |= GC_PHASE_ROOT; }
	MMINLINE void setCollectorTracing()			{ _gcPhase = GC_PHASE_TRACE; _sched->_gcPhaseSet |= GC_PHASE_TRACE; }
	MMINLINE void setCollectorUnloadingClassLoaders()			{ _gcPhase = GC_PHASE_UNLOADING_CLASS_LOADERS; _sched->_gcPhaseSet |= GC_PHASE_UNLOADING_CLASS_LOADERS; }
	MMINLINE void setCollectorSweeping()		{ _gcPhase = GC_PHASE_SWEEP; _sched->_gcPhaseSet |= GC_PHASE_SWEEP; }
	MMINLINE void setCollectorConcurrentTracing()		{ _gcPhase = GC_PHASE_CONCURRENT_TRACE; _sched->_gcPhaseSet |= GC_PHASE_CONCURRENT_TRACE; }
	MMINLINE void setCollectorConcurrentSweeping()		{ _gcPhase = GC_PHASE_CONCURRENT_SWEEP; _sched->_gcPhaseSet |= GC_PHASE_CONCURRENT_SWEEP; }
	MMINLINE void setCollectorSweepingArraylets(bool sweepingArraylets) { _sweepingArraylets = sweepingArraylets; }
	MMINLINE void setFixHeapForWalk(bool fixHeapForWalk) { _fixHeapForWalk = fixHeapForWalk; }
	MMINLINE bool isPreviousCycleBelowTrigger() { return _previousCycleBelowTrigger; }
	MMINLINE void setPreviousCycleBelowTrigger(bool previousCycleBelowTrigger) { _previousCycleBelowTrigger = previousCycleBelowTrigger; }

	virtual void setupForGC(MM_EnvironmentBase *env);
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase *extensions);
	void setMemoryPool(MM_MemoryPoolSegregated *memoryPool) { _memoryPool = memoryPool; }
	void setMemorySubSpace(MM_MemorySubSpaceMetronome *memorySubSpace) { _memorySubSpace = memorySubSpace; }

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(
		MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress,
		void *lowValidAddress, void *highValidAddress);

	void workerSetupForGC(MM_EnvironmentBase *env);
	void allThreadsAllocateUnmarked(MM_EnvironmentBase *env);
	/**
	 * Helper function that flushes cached per-context full regions to the global region pool.
	 * Currently called before debug function showPages for calculating region pool stats.
	 */
	void flushCachedFullRegions(MM_EnvironmentBase *env);
	virtual void setGCThreadPriority(OMR_VMThread *vmThread, uintptr_t priority);
	/* Create the access barrier */
	MM_WorkPacketsRealtime* allocateWorkPackets(MM_EnvironmentBase *env);
	/* Create an EventTypeSpaceVersion object for TuningFork tracing */
	void completeMarking(MM_EnvironmentRealtime* env);
	virtual bool condYield(MM_EnvironmentBase *env, U_64 timeSlackNanoSec);
	virtual bool shouldYield(MM_EnvironmentBase *env);
	virtual void yield(MM_EnvironmentBase *env);
	void enableWriteBarrier(MM_EnvironmentBase* env);
	void disableWriteBarrier(MM_EnvironmentBase* env);
	void enableDoubleBarrier(MM_EnvironmentBase* env);
	void disableDoubleBarrierOnThread(MM_EnvironmentBase* env, OMR_VMThread *vmThread);
	void disableDoubleBarrier(MM_EnvironmentBase* env);

	void flushRememberedSet(MM_EnvironmentRealtime *env);

	MM_RealtimeMarkingScheme *getMarkingScheme() { return _markingScheme; }
	MM_SweepSchemeRealtime *getSweepScheme() { return _sweepScheme; }
	MM_MetronomeDelegate *getRealtimeDelegate() { return &_realtimeDelegate; }

	virtual bool isMarked(void *objectPtr);

	MM_RealtimeGC(MM_EnvironmentBase *env)
		: MM_GlobalCollector()
		, _vm(env->getOmrVM())
		, _extensions(env->getExtensions())
		, _currentGCThreadPriority(-1)
		, _previousCycleBelowTrigger(true)
		, _sweepingArraylets(false)
		, _cycleState()
		, _moreTracingRequired(false)
		, _markingScheme(NULL)
		, _sweepScheme(NULL)
		, _memoryPool(NULL)
		, _memorySubSpace(NULL)
		, _osInterface(NULL)
		, _sched(NULL)
		, _fixHeapForWalk(false)
		, _avgPercentFreeHeapAfterCollect(0)	
		, _workPackets(NULL)
		, _stopTracing(false)
		, _realtimeDelegate(env)
	{
		_typeId = __FUNCTION__;
		_realtimeDelegate._realtimeGC = this;
	}

	/*
	 * Friends
	 */
	friend class MM_MemoryPoolSegregated;
	friend class MM_Scheduler;
	friend class MM_MetronomeDelegate;
};

#endif /* REALTIMEGC_HPP_ */
