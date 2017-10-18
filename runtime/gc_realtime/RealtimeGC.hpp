
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Metronome
 */

#if !defined(REALTIMEGC_HPP_)
#define REALTIMEGC_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "CycleState.hpp"
#include "GCExtensions.hpp"
#include "GlobalCollector.hpp"
#include "HeapRegionList.hpp"
#include "IncrementalOverflow.hpp"
#include "MemorySubSpace.hpp"
#include "MemoryPoolSegregated.hpp"
#include "OMRVMInterface.hpp"
#include "Scheduler.hpp"
#include "WorkPacketsRealtime.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;
class MM_MemorySubSpaceMetronome;
class MM_RealtimeAccessBarrier;
class MM_RealtimeMarkingScheme;
class MM_SweepSchemeRealtime;
class MM_FreeHeapRegionList;

#define ITEM_IS_ARRAYLET 0x1
#define IS_ITEM_OBJECT(item) ((item & ITEM_IS_ARRAYLET) == 0x0)
#define IS_ITEM_ARRAYLET(item) ((item & ITEM_IS_ARRAYLET) == ITEM_IS_ARRAYLET)
#define ITEM_TO_OBJECT(item) ((J9Object*)(((UDATA)item) & (~ITEM_IS_ARRAYLET)))
#define ITEM_TO_ARRAYLET(item) ((fj9object_t*)(((UDATA)item) & (~ITEM_IS_ARRAYLET)))
#define ITEM_TO_UDATAP(item) ((UDATA *)(((UDATA)item) & (~ITEM_IS_ARRAYLET)))
#define OBJECT_TO_ITEM(obj) ((UDATA) obj)
#define ARRAYLET_TO_ITEM(arraylet) (((UDATA) arraylet) | ITEM_IS_ARRAYLET)

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
	J9JavaVM *_javaVM;
	MM_GCExtensions *_extensions;

private:
	IDATA _currentGCThreadPriority;
	bool _previousCycleBelowTrigger;
	bool _sweepingArraylets;	

	UDATA _gcPhase; /**< What gc phase are we currently in? */
	
	MM_CycleState _cycleState;  /**< Embedded cycle state to be used as the master cycle state for GC activity */

protected:
	MM_RealtimeMarkingScheme *_markingScheme; /**< The marking scheme used to mark objects. */
	MM_SweepSchemeRealtime *_sweepScheme; /**< The sweep scheme used to sweep objects for MemoryPoolSegregated. */

public:
	MM_MemoryPoolSegregated *_memoryPool;
	MM_MemorySubSpaceMetronome *_memorySubSpace;
	bool _allowGrowth;
	MM_OSInterface *_osInterface;
	MM_Scheduler *_sched;

 	bool _unmarkedImpliesCleared;
 	bool _unmarkedImpliesStringsCleared; /**< If true, we can assume that unmarked strings in the string table will be cleared */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
 	bool _unmarkedImpliesClasses; /**< if true the mark bit can be used to check is class alive or not */
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	bool _fixHeapForWalk;
	float _avgPercentFreeHeapAfterCollect;
	MM_WorkPacketsRealtime *_workPackets;
	
#if defined(J9VM_GC_FINALIZATION)	
	bool _finalizationRequired;
#endif /* J9VM_GC_FINALIZATION */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool _dynamicClassUnloadingEnabled;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	bool _stopTracing;

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

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	void reportClassUnloadingStart(MM_EnvironmentBase *env);
	void reportClassUnloadingEnd(MM_EnvironmentBase *env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Perform initial cleanup for classloader unloading.  The current thread has exclusive access.
	 * The J9_JAVA_CLASS_DYING bit is set and J9HOOK_VM_CLASS_UNLOAD is triggered for each class that will be unloaded.
	 * The J9_GC_CLASS_LOADER_DEAD bit is set for each class loader that will be unloaded.
	 * J9HOOK_VM_CLASSES_UNLOAD is triggered if any classes will be unloaded.
	 * 
	 * @param env[in] the master GC thread
	 * @param classUnloadCountResult[out] returns the number of classes about to be unloaded
	 * @param anonymousClassUnloadCount[out] returns the number of anonymous classes about to be unloaded
	 * @param classLoaderUnloadCountResult[out] returns the number of class loaders about to be unloaded
	 * @param classLoaderUnloadListResult[out] returns a linked list of class loaders about to be unloaded
	 */
	void processDyingClasses(MM_EnvironmentRealtime *env, UDATA* classUnloadCountResult, UDATA* anonymousClassUnloadCount, UDATA* classLoaderUnloadCountResult, J9ClassLoader** classLoaderUnloadListResult);
	void processUnlinkedClassLoaders(MM_EnvironmentBase *env, J9ClassLoader *deadClassLoaders);
	void updateClassUnloadStats(MM_EnvironmentBase *env, UDATA classUnloadCount, UDATA anonymousClassUnloadCount, UDATA classLoaderUnloadCount);

	/**
	 * Scan classloader for dying classes and add them to the list
	 * @param env[in] the current thread
	 * @param classLoader[in] the list of class loaders to clean up
	 * @param setAll[in] bool if true if all classes must be set dying, if false unmarked classes only
	 * @param classUnloadListStart[in] root of list dying classes should be added to
	 * @param classUnloadCountOut[out] number of classes dying added to the list
	 * @return new root to list of dying classes
	 */
	J9Class *addDyingClassesToList(MM_EnvironmentRealtime *env, J9ClassLoader * classLoader, bool setAll, J9Class *classUnloadListStart, UDATA *classUnloadCountResult);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
public:


#if defined(J9VM_GC_FINALIZATION)	
	bool isFinalizationRequired() { return _finalizationRequired; }
#endif /* J9VM_GC_FINALIZATION */

	void masterSetupForGC(MM_EnvironmentBase *env);
	void masterCleanupAfterGC(MM_EnvironmentBase *env);
	void doAuxilaryGCWork(MM_EnvironmentBase *env); /* Wake up finalizer thread. Discard segments freed by class unloading. */
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

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MMINLINE bool isDynamicClassUnloadingEnabled() { return _dynamicClassUnloadingEnabled; };
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	void traceAll(MM_EnvironmentBase *env);

	void clearGCStats(MM_EnvironmentBase *env);
	void mergeGCStats(MM_EnvironmentBase *env);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	void unloadDeadClassLoaders(MM_EnvironmentBase *env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

    void reportSyncGCStart(MM_EnvironmentBase *env, GCReason reason, UDATA reasonParameter);
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

	virtual UDATA getVMStateID() { return J9VMSTATE_GC_COLLECTOR_METRONOME; };
	
	virtual void kill(MM_EnvironmentBase *env) = 0;
	bool initialize(MM_EnvironmentBase *env);
	bool allocateAndInitializeReferenceObjectLists(MM_EnvironmentBase *env);
	bool allocateAndInitializeUnfinalizedObjectLists(MM_EnvironmentBase *env);
	bool allocateAndInitializeOwnableSynchronizerObjectLists(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
	UDATA verbose(MM_EnvironmentBase *env);
	UDATA gcCount() { return  _extensions->globalGCStats.gcCount; }

	MMINLINE bool isBarrierEnabled()			{	return (_gcPhase == GC_PHASE_ROOT || _gcPhase == GC_PHASE_TRACE  || _gcPhase == GC_PHASE_CONCURRENT_TRACE); }

	MMINLINE bool isCollectorIdle()				{	return (_gcPhase == GC_PHASE_IDLE);	 }
	MMINLINE bool isCollectorRootMarking()		{	return (_gcPhase == GC_PHASE_ROOT);	 }
	MMINLINE bool isCollectorTracing()			{	return (_gcPhase == GC_PHASE_TRACE); }
	MMINLINE bool isCollectorUnloadingClassLoaders() {	return (_gcPhase == GC_PHASE_UNLOADING_CLASS_LOADERS); }
	MMINLINE bool isCollectorSweeping()			{	return (_gcPhase == GC_PHASE_SWEEP); }
	MMINLINE bool isCollectorConcurrentTracing() {	return (_gcPhase == GC_PHASE_CONCURRENT_TRACE); }
	MMINLINE bool isCollectorConcurrentSweeping() {	return (_gcPhase == GC_PHASE_CONCURRENT_SWEEP); }	
	MMINLINE bool isCollectorSweepingArraylets() { return _sweepingArraylets; } 
	MMINLINE bool isFixHeapForWalk() { return _fixHeapForWalk; }	

	MMINLINE void setCollectorIdle()			{	_gcPhase = GC_PHASE_IDLE; _sched->_gcPhaseSet |= GC_PHASE_IDLE;	}
	MMINLINE void setCollectorRootMarking()		{	_gcPhase = GC_PHASE_ROOT; _sched->_gcPhaseSet |= GC_PHASE_ROOT;	}
	MMINLINE void setCollectorTracing()			{	_gcPhase = GC_PHASE_TRACE; _sched->_gcPhaseSet |= GC_PHASE_TRACE;	}
	MMINLINE void setCollectorUnloadingClassLoaders()			{	_gcPhase = GC_PHASE_UNLOADING_CLASS_LOADERS; _sched->_gcPhaseSet |= GC_PHASE_UNLOADING_CLASS_LOADERS;	}
	MMINLINE void setCollectorSweeping()		{	_gcPhase = GC_PHASE_SWEEP; _sched->_gcPhaseSet |= GC_PHASE_SWEEP;	}
	MMINLINE void setCollectorConcurrentTracing()		{	_gcPhase = GC_PHASE_CONCURRENT_TRACE; _sched->_gcPhaseSet |= GC_PHASE_CONCURRENT_TRACE;	}
	MMINLINE void setCollectorConcurrentSweeping()		{	_gcPhase = GC_PHASE_CONCURRENT_SWEEP; _sched->_gcPhaseSet |= GC_PHASE_CONCURRENT_SWEEP;	}
	MMINLINE void setCollectorSweepingArraylets(bool sweepingArraylets) { _sweepingArraylets = sweepingArraylets; } 
	MMINLINE void setFixHeapForWalk(bool fixHeapForWalk) { _fixHeapForWalk = fixHeapForWalk; }
	MMINLINE bool isPreviousCycleBelowTrigger() { return _previousCycleBelowTrigger; }
	MMINLINE void setPreviousCycleBelowTrigger(bool previousCycleBelowTrigger) { _previousCycleBelowTrigger = previousCycleBelowTrigger ; }
		
	void enqueuePointerArraylet(MM_EnvironmentRealtime *env, fj9object_t*arraylet);
	
	
	virtual void setupForGC(MM_EnvironmentBase *env);
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase *extensions);
	void setMemoryPool(MM_MemoryPoolSegregated *memoryPool) { _memoryPool = memoryPool; }
	void setMemorySubSpace(MM_MemorySubSpaceMetronome *memorySubSpace) { _memorySubSpace = memorySubSpace; }

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(
		MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress,
		void *lowValidAddress, void *highValidAddress);
	virtual void heapReconfigured(MM_EnvironmentBase *env);

	void workerSetupForGC(MM_EnvironmentBase *env);
	void allThreadsAllocateUnmarked(MM_EnvironmentBase *env);
	/**
	 * Helper function that flushes cached per-context full regions to the global region pool.
	 * Currently called before debug function showPages for calculating region pool stats.
	 */
	void flushCachedFullRegions(MM_EnvironmentBase *env);
	virtual void setGCThreadPriority(OMR_VMThread *vmThread, UDATA priority);
	/* Create the access barrier */
	virtual MM_RealtimeAccessBarrier* allocateAccessBarrier(MM_EnvironmentBase *env) = 0;
	MM_WorkPacketsRealtime* allocateWorkPackets(MM_EnvironmentBase *env);
	/* Create an EventTypeSpaceVersion object for TuningFork tracing */
	virtual void doTracing(MM_EnvironmentRealtime* env) = 0;
	virtual bool condYield(MM_EnvironmentBase *env, U_64 timeSlackNanoSec);
	virtual bool shouldYield(MM_EnvironmentBase *env);
	virtual void yield(MM_EnvironmentBase *env);
	void yieldFromClassUnloading(MM_EnvironmentRealtime *env);
	void lockClassUnloadMonitor(MM_EnvironmentRealtime *env);
	void unlockClassUnloadMonitor(MM_EnvironmentRealtime *env);
	virtual void enableWriteBarrier(MM_EnvironmentBase* env) = 0;
	virtual void disableWriteBarrier(MM_EnvironmentBase* env) = 0;
	virtual void enableDoubleBarrier(MM_EnvironmentBase* env) = 0;
	virtual void disableDoubleBarrierOnThread(MM_EnvironmentBase* env, J9VMThread* vmThread) = 0;
	virtual void disableDoubleBarrier(MM_EnvironmentBase* env) = 0;
	MM_RealtimeMarkingScheme *getMarkingScheme() { return _markingScheme; }
	MM_SweepSchemeRealtime *getSweepScheme() { return _sweepScheme; }

	virtual bool isMarked(void *objectPtr);

	MM_RealtimeGC(MM_EnvironmentBase *env)
		: MM_GlobalCollector()
		, _javaVM((J9JavaVM*)env->getOmrVM()->_language_vm)
		, _extensions(MM_GCExtensions::getExtensions(env))
		, _currentGCThreadPriority(-1)
		, _previousCycleBelowTrigger(true)
		, _sweepingArraylets(false)
		, _cycleState()
		, _markingScheme(NULL)
		, _sweepScheme(NULL)
		, _memoryPool(NULL)
		, _memorySubSpace(NULL)
		, _osInterface(NULL)
		, _sched(NULL)
		, _fixHeapForWalk(false)
		, _avgPercentFreeHeapAfterCollect(0)	
		, _workPackets(NULL)
#if defined(J9VM_GC_FINALIZATION)
		, _finalizationRequired(false)
#endif /* J9VM_GC_FINALIZATION */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		, _dynamicClassUnloadingEnabled(false)
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	{
		_typeId = __FUNCTION__;
	}

	/*
	 * Friends
	 */
	friend class MM_MemoryPoolSegregated;
	friend class MM_Scheduler;
};

#endif /* REALTIMEGC_HPP_ */
