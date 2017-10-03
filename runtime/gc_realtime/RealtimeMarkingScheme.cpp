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

#include "j9.h"
#include "j2sever.h"
#include "modron.h"

#include "ArrayletObjectModel.hpp"
#include "CycleState.hpp"
#include "EnvironmentRealtime.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizeListManager.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "HeapRegionIterator.hpp"
#include "MarkMap.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectAllocationInterface.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeRootScanner.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "SlotObject.hpp"
#include "StackSlotValidator.hpp"
#include "WorkPacketsRealtime.hpp"

/**
 * This scanner will mark objects that pass through its doSlot.
 */
class MM_RealtimeMarkingSchemeRootMarker : public MM_RealtimeRootScanner
{
/* Data memebers / types */
public:
protected:
private:
	
/* Methods */
public:

	/**
	 * Simple chained constructor.
	 */
	MM_RealtimeMarkingSchemeRootMarker(MM_EnvironmentRealtime *env, MM_RealtimeGC *realtimeGC) :
		MM_RealtimeRootScanner(env, realtimeGC)
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * This scanner can be instantiated so we must give it a name.
	 */
	virtual const char*
	scannerName()
	{
		return "Mark";
	}
	
	/**
	 * Wraps the scanning of one thread to only happen if it hasn't already occured in this phase of this GC,
	 * also sets the thread up for the upcoming forwarding phase.
	 * @return true if the thread was scanned (the caller should offer to yield), false otherwise.
	 * @see MM_RootScanner::scanOneThread()
	 */
	virtual void
	scanOneThreadImpl(MM_EnvironmentRealtime *env, J9VMThread* walkThread, void* localData)
	{
		MM_EnvironmentRealtime* walkThreadEnv = MM_EnvironmentRealtime::getEnvironment(walkThread);
		MM_RealtimeGC* realtimeGC = (MM_RealtimeGC*)_realtimeGC;
		/* Scan the thread by invoking superclass */
		MM_RootScanner::scanOneThread(env, walkThread, localData);

		/*
		 * TODO CRGTMP we should be able to premark the cache instead of flushing the cache
		 * but this causes problems in overflow.  When we have some time we should look into
		 * this again.
		 */
		/*((MM_SegregatedAllocationInterface *)walkThreadEnv->_objectAllocationInterface)->preMarkCache(walkThreadEnv);*/
		walkThreadEnv->_objectAllocationInterface->flushCache(walkThreadEnv);
		/* Disable the double barrier on the scanned thread. */
		realtimeGC->disableDoubleBarrierOnThread(env, walkThread);
	}
	
#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t object) {
		_markingScheme->markObject(_env, object);
	}
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * Simply pass the call on to the RealtimeGC.
	 * @see MM_Metronome::markObject()
	 */
	virtual void
	doSlot(J9Object** slot)
	{
		_markingScheme->markObject(_env, *slot);
	}
	
	virtual void
	doStackSlot(J9Object **slotPtr, void *walkState, const void* stackLocation)
	{
		J9Object *object = *slotPtr;
		if (_markingScheme->isHeapObject(object)) {
			/* heap object - validate and mark */
			Assert_MM_validStackSlot(MM_StackSlotValidator(0, object, stackLocation, walkState).validate(_env));
			_markingScheme->markObject(_env, object);
		} else if (NULL != object) {
			/* stack object - just validate */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, object, stackLocation, walkState).validate(_env));
		}
	}

	virtual void 
	doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator) {
		J9Object *object = *slotPtr;
		if (_markingScheme->isHeapObject(object)) {
			_markingScheme->markObject(_env, object);
		} else if (NULL != object) {
			Assert_MM_true(vmthreaditerator_state_monitor_records == vmThreadIterator->getState());
		}
	}

	/**
	 * Scans non-collectable internal objects (immortal) and classes
	 */
	virtual void
	scanIncrementalRoots(MM_EnvironmentRealtime *env)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (_classDataAsRoots) {
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
			scanClasses(env);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		} else {
			scanPermanentClasses(env);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		condYield(_realtimeGC->_sched->beatNanos);
		CompletePhaseCode status = scanClassesComplete(env);

		if (complete_phase_ABORT == status) {
			return ;
		}
	}
protected:
private:
};

/**
 * This scanner will mark objects that pass through its doSlot.
 */
class MM_RealtimeMarkingSchemeRootClearer : public MM_RealtimeRootScanner
{
/* Data memebers / types */
public:
protected:
private:
	
/* Methods */
public:

	/**
	 * Simple chained constructor.
	 */
	MM_RealtimeMarkingSchemeRootClearer(MM_EnvironmentRealtime *env, MM_RealtimeGC *realtimeGC) :
		MM_RealtimeRootScanner(env, realtimeGC)
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * This should not be called
	 */
	virtual void
	doSlot(J9Object** slot)
	{
		PORT_ACCESS_FROM_ENVIRONMENT(_env);
		j9tty_printf(PORTLIB, "MM_RealtimeMarkingSchemeRootClearer::doSlot should not be called\n");
		assert(false);
	}

	/**
	 * This scanner can be instantiated so we must give it a name.
	 */
	virtual const char*
	scannerName()
	{
		return "Clearable";
	}
	
	/**
	 * Destroys unmarked monitors.
	 */
	virtual void
	doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
	{
		J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
		if(!_markingScheme->isMarked((J9Object *)monitor->userData)) {
			monitorReferenceIterator->removeSlot();
			/* We must call objectMonitorDestroy (as opposed to omrthread_monitor_destroy) when the
			 * monitor is not internal to the GC */
			static_cast<J9JavaVM*>(_omrVM->_language_vm)->internalVMFunctions->objectMonitorDestroy(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9VMThread *)_env->getLanguageVMThread(), (omrthread_monitor_t)monitor);
		}
	}
	
	/**
	 * Wraps the MM_RootScanner::scanMonitorReferences method to disable yielding during the scan.
	 * @see MM_RootScanner::scanMonitorReferences()
	 */
	virtual void
	scanMonitorReferences(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentRealtime *env = (MM_EnvironmentRealtime *)envBase;
		
		/* @NOTE For SRT and MT to play together this needs to be investigated */
		if(_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			reportScanningStarted(RootScannerEntity_MonitorReferences);
		
			J9ObjectMonitor *objectMonitor = NULL;
			J9MonitorTableListEntry *monitorTableList = static_cast<J9JavaVM*>(_omrVM->_language_vm)->monitorTableList;
			while (NULL != monitorTableList) {
				J9HashTable *table = monitorTableList->monitorTable;
				if (NULL != table) {
					GC_HashTableIterator iterator(table);
					iterator.disableTableGrowth();
					while (NULL != (objectMonitor = (J9ObjectMonitor*)iterator.nextSlot())) {
						doMonitorReference(objectMonitor, &iterator);
						if (shouldYieldFromMonitorScan()) {
							yield();
						}
					}
					iterator.enableTableGrowth();
				}
				monitorTableList = monitorTableList->next;
			}

			reportScanningEnded(RootScannerEntity_MonitorReferences);			
		}
	}

	virtual CompletePhaseCode scanMonitorReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_MonitorReferenceObjectsComplete);
		((J9JavaVM *)env->getLanguageVM())->internalVMFunctions->objectMonitorDestroyComplete((J9JavaVM *)env->getLanguageVM(), (J9VMThread *)env->getLanguageVMThread());
		reportScanningEnded(RootScannerEntity_MonitorReferenceObjectsComplete);
		return complete_phase_OK;
	}
	
	virtual void scanWeakReferenceObjects(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_WeakReferenceObjects);

		_markingScheme->scanWeakReferenceObjects(env);

		reportScanningEnded(RootScannerEntity_WeakReferenceObjects);
	}

	virtual CompletePhaseCode scanWeakReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_weak;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		return complete_phase_OK;
	}

	virtual void scanSoftReferenceObjects(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_SoftReferenceObjects);

		_markingScheme->scanSoftReferenceObjects(env);

		reportScanningEnded(RootScannerEntity_SoftReferenceObjects);
	}

	virtual CompletePhaseCode scanSoftReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_SoftReferenceObjectsComplete);
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_soft;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		reportScanningEnded(RootScannerEntity_SoftReferenceObjectsComplete);
		return complete_phase_OK;
	}

	virtual void scanPhantomReferenceObjects(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_PhantomReferenceObjects);

		_markingScheme->scanPhantomReferenceObjects(env);

		reportScanningEnded(RootScannerEntity_PhantomReferenceObjects);
	}

	virtual CompletePhaseCode scanPhantomReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_PhantomReferenceObjectsComplete);
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_phantom;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		/* phantom reference processing may resurrect objects - scan them now */
		_realtimeGC->doTracing(MM_EnvironmentRealtime::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_PhantomReferenceObjectsComplete);
		return complete_phase_OK;
	}

#if defined(J9VM_GC_FINALIZATION)
	/**
	 * Wraps the MM_RootScanner::scanUnfinalizedObjects method to disable yielding during the scan
	 * then yield after scanning.
	 * @see MM_RootScanner::scanUnfinalizedObjects()
	 */
	virtual void
	scanUnfinalizedObjects(MM_EnvironmentBase *envBase)	{
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
		/* allow the marking scheme to handle this */
		_markingScheme->scanUnfinalizedObjects(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}

	virtual CompletePhaseCode scanUnfinalizedObjectsComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_UnfinalizedObjectsComplete);
		/* ensure that all unfinalized processing is complete before we start marking additional objects */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		_realtimeGC->doTracing(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjectsComplete);
		return complete_phase_OK;
	}

	virtual void doFinalizableObject(j9object_t object) {
		Assert_MM_unreachable();
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual void
	scanOwnableSynchronizerObjects(MM_EnvironmentBase *envBase)	{
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);

		reportScanningStarted(RootScannerEntity_OwnableSynchronizerObjects);
		/* allow the marking scheme to handle this */
		_markingScheme->scanOwnableSynchronizerObjects(env);
		reportScanningEnded(RootScannerEntity_OwnableSynchronizerObjects);
	}

	/**
	 * Wraps the MM_RootScanner::scanJNIWeakGlobalReferences method to disable yielding during the scan.
	 * @see MM_RootScanner::scanJNIWeakGlobalReferences()
	 */
	virtual void
	scanJNIWeakGlobalReferences(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		
		env->disableYield();
		MM_RootScanner::scanJNIWeakGlobalReferences(env);
		env->enableYield();
	}
	
	/**
	 * Clears slots holding unmarked objects.
	 */
	virtual void
	doJNIWeakGlobalReference(J9Object **slotPtr)
	{
		J9Object *objectPtr = *slotPtr;
		if(objectPtr && !_markingScheme->isMarked(objectPtr)) {
			*slotPtr = NULL;
		}
	}

#if defined(J9VM_OPT_JVMTI)
	/**
	 * Clears slots holding unmarked objects.
	 */
	virtual void
	doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
	{
		J9Object *objectPtr = *slotPtr;
		if(objectPtr && !_markingScheme->isMarked(objectPtr)) {
			*slotPtr = NULL;
		}
	}
#endif /* J9VM_OPT_JVMTI */
	
protected:
private:
};

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_RealtimeMarkingScheme *
MM_RealtimeMarkingScheme::newInstance(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC)
{
	MM_RealtimeMarkingScheme *instance;
	
	instance = (MM_RealtimeMarkingScheme *)env->getForge()->allocate(sizeof(MM_RealtimeMarkingScheme), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (instance) {
		new(instance) MM_RealtimeMarkingScheme(env, realtimeGC);
		if (!instance->initialize(env)) { 
			instance->kill(env);
			instance = NULL;
		}
	}

	return instance;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_RealtimeMarkingScheme::kill(MM_EnvironmentBase *env)
{
	tearDown(env); 
	env->getForge()->free(this);
}

/**
 * Intialize the RealtimeMarkingScheme instance.
 * 
 */
bool
MM_RealtimeMarkingScheme::initialize(MM_EnvironmentBase *env)
{
	if (!MM_SegregatedMarkingScheme::initialize(env)) {
		return false;
	}

	_javaVM = (J9JavaVM *)env->getOmrVM()->_language_vm;
	_scheduler = _realtimeGC->_sched;
	_gcExtensions = (MM_GCExtensions *)_extensions;
	
	_javaVM->realtimeHeapMapBasePageRounded = _markMap->getHeapMapBaseRegionRounded();
	_javaVM->realtimeHeapMapBits = _markMap->getHeapMapBits();

	return true;
}

/**
 * Teardown the RealtimeMarkingScheme instance.
 * 
 */
void
MM_RealtimeMarkingScheme::tearDown(MM_EnvironmentBase *env)
{
	MM_SegregatedMarkingScheme::tearDown(env);

	_javaVM->realtimeHeapMapBits = NULL;
}

/**
 * Mark all of the roots.  The system and application classloaders need to be set
 * to marked/scanned before root marking begins.  
 * 
 * @note Once the root lists all have barriers this code may change to call rootScanner.scanRoots();
 * 
 */
void 
MM_RealtimeMarkingScheme::markRoots(MM_EnvironmentRealtime *env, MM_RealtimeMarkingSchemeRootMarker *rootScanner)
{
	/* Mark root set classes */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(env->isMasterThread()) {
		/* TODO: This code belongs somewhere else? */
		/* Setting the permanent class loaders to scanned without a locked operation is safe
		 * Class loaders will not be rescanned until a thread synchronize is executed
		 */
		if(_realtimeGC->isDynamicClassUnloadingEnabled()) {
			((J9ClassLoader *)_javaVM->systemClassLoader)->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;
			markObject(env, (J9Object *)((J9ClassLoader *)_javaVM->systemClassLoader)->classLoaderObject);
			if(_javaVM->applicationClassLoader) {
				((J9ClassLoader *)_javaVM->applicationClassLoader)->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;
				markObject(env, (J9Object *)((J9ClassLoader *)_javaVM->applicationClassLoader)->classLoaderObject);
			}
		}
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
	/* Note: it's important to scan the finalizable objects queue (atomically) before the
	 * threads, because the finalizer threads are among the threads and, once any one of
	 * them is scanned and then allowed to execute, any object it takes off the finalizer
	 * queue had better also be scanned.  An alternative would be to put a special read
	 * barrier in the queue-removal action but controlling the order is an easy solution.
	 *
	 * It is also important to scan JNI global references after scanning threads, because
	 * the JNI global reference barrier is executed at deletion time, not creation time.
	 * We could have barriers in both, but controlling the order is an easy solution.
	 *
	 *
	 * The Metronome write barrier ensures that no unscanned thread can expose an object
	 * to other threads without it becoming a root and no scanned thread can make an
	 * object that was once reachable unreachable until it has been traced ("snapshot at
	 * the beginning with a fuzzy snapshot").  This eliminates other order dependencies
	 * between portions of the scan or requirements that multiple phases be done as an
	 * atomic unit.  However, some phases are still done atomically because we have not
	 * yet determined whether the iterators that they use are safe and complete and have
	 * not even analyzed in all cases whether correctness depends on completeness.
	 */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
#if defined(J9VM_GC_FINALIZATION)
		/* Note: if iterators are safe in scanFinalizableObjects, disableYield() could be
		 * removed.
		 */
		env->disableYield();
		rootScanner->scanFinalizableObjects(env);
		env->enableYield();
		_scheduler->condYieldFromGC(env);
#endif /* J9VM_GC_FINALIZATION */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) 
		if (!_realtimeGC->isDynamicClassUnloadingEnabled()) {
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */ 
			/* We are scanning all classes, no need to include stack frame references */
			rootScanner->setIncludeStackFrameClassReferences(false); 
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) 
		} else {
			rootScanner->setIncludeStackFrameClassReferences(true); 
		} 
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	rootScanner->scanThreads(env);

	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		_gcExtensions->newThreadAllocationColor = GC_MARK;
		_realtimeGC->disableDoubleBarrier(env);
		if (_realtimeGC->verbose(env) >= 3) {
			rootScanner->reportThreadCount(env);
		}

		/* Note: if iterators are safe for some or all remaining atomic root categories,
		 * disableYield() could be removed or moved inside scanAtomicRoots.
		 */
		env->disableYield();
		rootScanner->scanAtomicRoots(env);
		env->enableYield();
		rootScanner->scanIncrementalRoots(env);
	
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

/**
 * This function marks all of the live objects on the heap and handles all of the clearable
 * objects.
 * 
 */
void
MM_RealtimeMarkingScheme::markLiveObjects(MM_EnvironmentRealtime *env)
{
	env->getWorkStack()->reset(env, _realtimeGC->_workPackets);
	
	/* These are thread-local stats that should probably be moved
	 * into the MM_MarkStats structure.
	 */
	env->resetScannedCounters();
	
	/* The write barrier must be enabled before any scanning begins. The double barrier will
	 * be enabled for the duration of the thread scans. It gets disabled on a per thread basis
	 * as the threads get scanned. It also gets "disabled" on a global basis once all threads
	 * are scanned.
	 */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		_realtimeGC->enableWriteBarrier(env);
		_realtimeGC->enableDoubleBarrier(env);
		/* BEN TODO: Ragged barrier here */
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
	
	MM_RealtimeMarkingSchemeRootMarker rootMarker(env, _realtimeGC);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	rootMarker.setClassDataAsRoots(!_realtimeGC->isDynamicClassUnloadingEnabled());
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	markRoots(env, &rootMarker);
	_scheduler->condYieldFromGC(env);
	/* Heap Marking and barrier processing.Cannot delay barrier processing until the end.*/	
	_realtimeGC->doTracing(env);
	
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		_realtimeGC->_unmarkedImpliesCleared = true;
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	/* Process reference objects and finalizable objects. */
	MM_RealtimeMarkingSchemeRootClearer rootScanner(env, _realtimeGC);
	rootScanner.scanClearable(env);

	Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());
	_scheduler->condYieldFromGC(env);
	
	/* Do a final tracing phase to complete the marking phase.  It should not be possible for any thread,
	 * including NHRT's, to add elements to the rememberedSet between the end of this doTracing call and when
	 * we disable the write barrier since the entire live set will be completed.
	 */
	_realtimeGC->doTracing(env);
	
	Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());

	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		/* This flag is set during the soft reference scanning just before unmarked references are to be
		 * cleared. It's used to prevent objects that are going to be cleared (e.g. referent that is not marked,
		 * or unmarked string constant) from escaping.
		 */
		_realtimeGC->_unmarkedImpliesCleared = false;
		_realtimeGC->_unmarkedImpliesStringsCleared = false;
		
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		/* enable to use mark information to detect is class dead */
		_realtimeGC->_unmarkedImpliesClasses = true;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

		/* This is the symmetric call to the enabling of the write barrier that happens at the top of this method. */
		_realtimeGC->disableWriteBarrier(env);
		/* BEN TODO: Ragged barrier here */
		
		/* reset flag "overflow happened this GC cycle" */
		_realtimeGC->_workPackets->getIncrementalOverflowHandler()->resetOverflowThisGCCycle();
		
		Assert_MM_true(_realtimeGC->_workPackets->isAllPacketsEmpty());
		
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

MMINLINE UDATA
MM_RealtimeMarkingScheme::scanPointerArraylet(MM_EnvironmentRealtime *env, fj9object_t *arraylet)
{
	fj9object_t *startScanPtr = arraylet;
	fj9object_t *endScanPtr = startScanPtr + (((J9JavaVM *)env->getLanguageVM())->arrayletLeafSize / sizeof(fj9object_t));
	return scanPointerRange(env, startScanPtr, endScanPtr);
}

MMINLINE UDATA
MM_RealtimeMarkingScheme::scanPointerRange(MM_EnvironmentRealtime *env, fj9object_t *startScanPtr, fj9object_t *endScanPtr)
{
	fj9object_t *scanPtr = startScanPtr;
	UDATA pointerFieldBytes = (UDATA)(endScanPtr - scanPtr);
	UDATA pointerField = pointerFieldBytes / sizeof(fj9object_t);
	while(scanPtr < endScanPtr) {
		GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
		markObject(env, slotObject.readReferenceFromSlot());
		scanPtr++;
	}

	env->addScannedBytes(pointerFieldBytes);
	env->addScannedPointerFields(pointerField);

	return pointerField;
}

MMINLINE UDATA
MM_RealtimeMarkingScheme::scanObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
{
	UDATA pointersScanned = 0;
	switch(_gcExtensions->objectModel.getScanType(objectPtr)) {
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		pointersScanned = scanMixedObject(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		pointersScanned = scanPointerArrayObject(env, (J9IndexableObject *)objectPtr);
		break;
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		pointersScanned = scanReferenceMixedObject(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
	   pointersScanned = 0;
	   break;
	default:
		Assert_MM_unreachable();
	}
	
	return pointersScanned;
}

MMINLINE UDATA
MM_RealtimeMarkingScheme::scanMixedObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
{
	/* Object slots */

	fj9object_t *scanPtr = (fj9object_t*)( objectPtr + 1 );
	UDATA objectSize = _gcExtensions->mixedObjectModel.getSizeInBytesWithHeader(objectPtr);
	fj9object_t *endScanPtr = (fj9object_t*)(((U_8 *)objectPtr) + objectSize);
	UDATA *descriptionPtr;
	UDATA descriptionBits;
	UDATA descriptionIndex;
#if defined(J9VM_GC_LEAF_BITS)
	UDATA *leafPtr;
	UDATA leafBits;
#endif /* J9VM_GC_LEAF_BITS */
	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(_realtimeGC->isDynamicClassUnloadingEnabled()) {
		markClassOfObject(env, objectPtr);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	descriptionPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
	leafPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceLeafDescription;
#endif /* J9VM_GC_LEAF_BITS */

	if(((UDATA)descriptionPtr) & 1) {
		descriptionBits = ((UDATA)descriptionPtr) >> 1;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits = ((UDATA)leafPtr) >> 1;
#endif /* J9VM_GC_LEAF_BITS */
	} else {
		descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
	}
	descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;

	UDATA pointerFields = 0;
	while(scanPtr < endScanPtr) {
		/* Determine if the slot should be processed */
		if(descriptionBits & 1) {
			pointerFields++;
			GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
#if defined(J9VM_GC_LEAF_BITS)
			markObject(env, slotObject.readReferenceFromSlot(), 1 == (leafBits & 1));
#else /* J9VM_GC_LEAF_BITS */
			markObject(env, slotObject.readReferenceFromSlot(), false);
#endif /* J9VM_GC_LEAF_BITS */
		}
		descriptionBits >>= 1;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits >>= 1;
#endif /* J9VM_GC_LEAF_BITS */
		if(descriptionIndex-- == 0) {
			descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
			leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
			descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
		}
		scanPtr += 1;
	}

	env->addScannedBytes(objectSize);
	env->addScannedPointerFields(pointerFields);
	env->incScannedObjects();

	return pointerFields;
}

MMINLINE UDATA
MM_RealtimeMarkingScheme::scanReferenceMixedObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
{
	fj9object_t *scanPtr = (fj9object_t*)( objectPtr + 1 );
	UDATA objectSize = _gcExtensions->mixedObjectModel.getSizeInBytesWithHeader(objectPtr);
	fj9object_t *endScanPtr = (fj9object_t*)(((U_8 *)objectPtr) + objectSize);
	UDATA *descriptionPtr;
	UDATA descriptionBits;
	UDATA descriptionIndex;
#if defined(J9VM_GC_LEAF_BITS)
	UDATA *leafPtr;
	UDATA leafBits;
#endif /* J9VM_GC_LEAF_BITS */
	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(_realtimeGC->isDynamicClassUnloadingEnabled()) {
		markClassOfObject(env, objectPtr);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
	descriptionPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
	leafPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceLeafDescription;
#endif /* J9VM_GC_LEAF_BITS */

	if(((UDATA)descriptionPtr) & 1) {
		descriptionBits = ((UDATA)descriptionPtr) >> 1;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits = ((UDATA)leafPtr) >> 1;
#endif /* J9VM_GC_LEAF_BITS */
	} else {
		descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
	}
	descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;

	I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr);
	UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(objectPtr)) & J9_JAVA_CLASS_REFERENCE_MASK;
	UDATA referenceObjectOptions = env->_cycleState->_referenceObjectOptions;
	bool isReferenceCleared = (GC_ObjectModel::REF_STATE_CLEARED == referenceState) || (GC_ObjectModel::REF_STATE_ENQUEUED == referenceState);
	bool referentMustBeMarked = isReferenceCleared;
	bool referentMustBeCleared = false;

	switch (referenceObjectType) {
	case J9_JAVA_CLASS_REFERENCE_WEAK:
		referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_weak));
		break;
	case J9_JAVA_CLASS_REFERENCE_SOFT:
		referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_soft));
		referentMustBeMarked = referentMustBeMarked || (
			((0 == (referenceObjectOptions & MM_CycleState::references_soft_as_weak))
			&& ((UDATA)J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, objectPtr) < _gcExtensions->getDynamicMaxSoftReferenceAge())));
		break;
	case J9_JAVA_CLASS_REFERENCE_PHANTOM:
		referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_phantom));
		break;
	default:
		Assert_MM_unreachable();
	}

	GC_SlotObject referentPtr(_javaVM->omrVM, &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr));

	if (referentMustBeCleared) {
		/* Discovering this object at this stage in the GC indicates that it is being resurrected. Clear its referent slot. */
		referentPtr.writeReferenceToSlot(NULL);
		/* record that the reference has been cleared if it's not already in the cleared or enqueued state */
		if (!isReferenceCleared) {
			J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr) = GC_ObjectModel::REF_STATE_CLEARED;
		}
	} else {
		/* we don't need to process cleared or enqueued references */
		if (!isReferenceCleared) {
			env->getGCEnvironment()->_referenceObjectBuffer->add(env, objectPtr);
		}
	}
	
	UDATA pointerFields = 0;
	while(scanPtr < endScanPtr) {
		/* Determine if the slot should be processed */
		if((descriptionBits & 1) && ((scanPtr != referentPtr.readAddressFromSlot()) || referentMustBeMarked)) {
			pointerFields++;
			GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
#if defined(J9VM_GC_LEAF_BITS)
			markObject(env, slotObject.readReferenceFromSlot(), 1 == (leafBits & 1));
#else /* J9VM_GC_LEAF_BITS */
			markObject(env, slotObject.readReferenceFromSlot(), false);
#endif /* J9VM_GC_LEAF_BITS */
		}
		descriptionBits >>= 1;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits >>= 1;
#endif /* J9VM_GC_LEAF_BITS */
		if(descriptionIndex-- == 0) {
			descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
			leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
			descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
		}
		scanPtr++;
	}
	
	env->addScannedBytes(objectSize);
	env->addScannedPointerFields(pointerFields);
	env->incScannedObjects();
	
	return pointerFields;
}

MMINLINE UDATA
MM_RealtimeMarkingScheme::scanPointerArrayObject(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr)
{
	UDATA pointerFields = 0;
	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(_realtimeGC->isDynamicClassUnloadingEnabled()) {
		markClassOfObject(env, (J9Object *)objectPtr);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	bool isContiguous = _gcExtensions->indexableObjectModel.isInlineContiguousArraylet(objectPtr);

	/* Very small arrays cannot be set as scanned (no scanned bit in Mark Map reserved for them) */
	bool canSetAsScanned = (isContiguous
			&& (_gcExtensions->minArraySizeToSetAsScanned <= _gcExtensions->indexableObjectModel.arrayletSize(objectPtr, 0)));

	if (canSetAsScanned && isScanned((J9Object *)objectPtr)) {
		/* Already scanned by ref array copy optimization */
		return pointerFields;
	}

#if defined(J9VM_GC_HYBRID_ARRAYLETS)
	/* if NUA is enabled, separate path for contiguous arrays */
	UDATA sizeInElements = _gcExtensions->indexableObjectModel.getSizeInElements(objectPtr);
	if (isContiguous || (0 == sizeInElements)) {
		fj9object_t *startScanPtr = (fj9object_t *)_gcExtensions->indexableObjectModel.getDataPointerForContiguous(objectPtr);
		fj9object_t *endScanPtr = startScanPtr + sizeInElements;
		pointerFields += scanPointerRange(env, startScanPtr, endScanPtr);
	} else {
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
		fj9object_t *arrayoid = _gcExtensions->indexableObjectModel.getArrayoidPointer(objectPtr);
		UDATA numArraylets = _gcExtensions->indexableObjectModel.numArraylets(objectPtr);
		for (UDATA i=0; i<numArraylets; i++) {
			UDATA arrayletSize = _gcExtensions->indexableObjectModel.arrayletSize(objectPtr, i);
			/* need to check leaf pointer because this can be a partially allocated arraylet (not all leafs are allocated) */
			GC_SlotObject slotObject(_javaVM->omrVM, &arrayoid[i]);
			fj9object_t *startScanPtr = (fj9object_t*) (slotObject.readReferenceFromSlot());
			if (NULL != startScanPtr) {
				fj9object_t *endScanPtr = startScanPtr + arrayletSize / sizeof(fj9object_t);
				if (i == (numArraylets - 1)) {
					pointerFields += scanPointerRange(env, startScanPtr, endScanPtr);
					if (canSetAsScanned) {
						setScanAtomic((J9Object *)objectPtr);
					}
				} else {
					_realtimeGC->enqueuePointerArraylet(env, startScanPtr);
				}
			}
		}
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
	}
#endif /* J9VM_GC_HYBRID_ARRAYLETS */

	/* check for yield if we've actually scanned a leaf */
	if (0 != pointerFields) {
		_scheduler->condYieldFromGC(env);
	}

	env->incScannedObjects();

	return pointerFields;
}

/**
 * Mark this class
 */
bool 
MM_RealtimeMarkingScheme::markClass(MM_EnvironmentRealtime *env, J9Class *clazz)
{
	bool result = false;
	if (clazz != NULL) {
		result = markClassNoCheck(env, clazz);
	}
	return result;
}

/**
 * If maxCount == MAX_UNIT, we run till work stack is empty and we return true, if at least one
 * object is marked.
 * Otherwise, mark up to maxCount of objects. If we reached the limit return false, which means we are
 * not finished yet.
 */
bool
MM_RealtimeMarkingScheme::incrementalConsumeQueue(MM_EnvironmentRealtime *env, UDATA maxCount)
{
	UDATA item;
	UDATA count = 0, countSinceLastYieldCheck = 0;
	UDATA scannedPointersSumSinceLastYieldCheck = 0;
  
	while(0 != (item = (UDATA)env->getWorkStack()->pop(env))) {
		UDATA scannedPointers;
		if (IS_ITEM_ARRAYLET(item)) {
			fj9object_t*arraylet = ITEM_TO_ARRAYLET(item);
			scannedPointers = scanPointerArraylet(env, arraylet);
		} else {
			J9Object *objectPtr = ITEM_TO_OBJECT(item);
			scannedPointers = scanObject(env, objectPtr);
		}

		countSinceLastYieldCheck += 1;
		scannedPointersSumSinceLastYieldCheck += scannedPointers;
		
		if (((countSinceLastYieldCheck * 2) + scannedPointersSumSinceLastYieldCheck) > _gcExtensions->traceCostToCheckYield) {
			_scheduler->condYieldFromGC(env);
			
			scannedPointersSumSinceLastYieldCheck = 0;
			countSinceLastYieldCheck = 0;
		}
		
		if (++count >= maxCount) {
			return false;
		}
	}
	
	if (maxCount == MAX_UINT) {
		return (count != 0);
	} else {
		return true;
	}
}

#if defined(J9VM_GC_FINALIZATION)
void
MM_RealtimeMarkingScheme::scanUnfinalizedObjects(MM_EnvironmentRealtime *env)
{
	const UDATA maxIndex = MM_HeapRegionDescriptorRealtime::getUnfinalizedObjectListCount(env);
	/* first we need to move the current list to the prior list and process the prior list,
	 * because if object has not yet become finalizable, we have to re-insert it back to the current list.
	 */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		GC_OMRVMInterface::flushNonAllocationCaches(env);
		UDATA listIndex;
		for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
			MM_UnfinalizedObjectList *unfinalizedObjectList = &_gcExtensions->unfinalizedObjectLists[listIndex];
			unfinalizedObjectList->startUnfinalizedProcessing();
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	GC_Environment *gcEnv = env->getGCEnvironment();
	GC_FinalizableObjectBuffer buffer(_gcExtensions);
	UDATA listIndex;
	for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			MM_UnfinalizedObjectList *unfinalizedObjectList = &_gcExtensions->unfinalizedObjectLists[listIndex];
			J9Object *object = unfinalizedObjectList->getPriorList();
			UDATA objectsVisited = 0;

			while (NULL != object) {
				objectsVisited += 1;
				gcEnv->_markJavaStats._unfinalizedCandidates += 1;
				J9Object* next = _gcExtensions->accessBarrier->getFinalizeLink(object);
				if (markObject(env, object)) {
					/* object was not previously marked -- it is now finalizable so push it to the local buffer */
					buffer.add(env, object);
					gcEnv->_markJavaStats._unfinalizedEnqueued += 1;
					_realtimeGC->_finalizationRequired = true;
				} else {
					/* object was already marked. It is still unfinalized */
					gcEnv->_unfinalizedObjectBuffer->add(env, object);
				}
				object = next;
				if (UNFINALIZED_OBJECT_YIELD_CHECK_INTERVAL == objectsVisited ) {
					_scheduler->condYieldFromGC(env);
					objectsVisited = 0;
				}
			}
			_scheduler->condYieldFromGC(env);
		}
	}

	/* Flush the local buffer of finalizable objects to the global list */
	buffer.flush(env);

	/* restore everything to a flushed state before exiting */
	gcEnv->_unfinalizedObjectBuffer->flush(env);
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_RealtimeMarkingScheme::scanOwnableSynchronizerObjects(MM_EnvironmentRealtime *env)
{
	const UDATA maxIndex = MM_HeapRegionDescriptorRealtime::getOwnableSynchronizerObjectListCount(env);

	/* first we need to move the current list to the prior list and process the prior list,
	 * because if object has been marked, we have to re-insert it back to the current list.
	 */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		GC_OMRVMInterface::flushNonAllocationCaches(env);
		UDATA listIndex;
		for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
			MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = &_gcExtensions->ownableSynchronizerObjectLists[listIndex];
			ownableSynchronizerObjectList->startOwnableSynchronizerProcessing();
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	GC_Environment *gcEnv = env->getGCEnvironment();
	MM_OwnableSynchronizerObjectBuffer *buffer = gcEnv->_ownableSynchronizerObjectBuffer;
	UDATA listIndex;
	for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
		MM_OwnableSynchronizerObjectList *list = &_gcExtensions->ownableSynchronizerObjectLists[listIndex];
		if (!list->wasEmpty()) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				J9Object *object = list->getPriorList();
				UDATA objectsVisited = 0;
				while (NULL != object) {
					objectsVisited += 1;
					gcEnv->_markJavaStats._ownableSynchronizerCandidates += 1;

					/* Get next before adding it to the buffer, as buffer modifies OwnableSynchronizerLink */
					J9Object* next = _gcExtensions->accessBarrier->getOwnableSynchronizerLink(object);
					if (isMarked(object)) {
						buffer->add(env, object);
					} else {
						gcEnv->_markJavaStats._ownableSynchronizerCleared += 1;
					}
					object = next;

					if (OWNABLE_SYNCHRONIZER_OBJECT_YIELD_CHECK_INTERVAL == objectsVisited ) {
						_scheduler->condYieldFromGC(env);
						objectsVisited = 0;
					}
				}
				_scheduler->condYieldFromGC(env);
			}
		}
	}
	/* restore everything to a flushed state before exiting */
	buffer->flush(env);
}

void
MM_RealtimeMarkingScheme::scanWeakReferenceObjects(MM_EnvironmentRealtime *env)
{
	GC_Environment *gcEnv = env->getGCEnvironment();
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
	const UDATA maxIndex = MM_HeapRegionDescriptorRealtime::getReferenceObjectListCount(env);
	UDATA listIndex;
	for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			MM_ReferenceObjectList *referenceObjectList = &_gcExtensions->referenceObjectLists[listIndex];
			referenceObjectList->startWeakReferenceProcessing();
			processReferenceList(env, NULL, referenceObjectList->getPriorWeakList(), &gcEnv->_markJavaStats._weakReferenceStats);
			_scheduler->condYieldFromGC(env);
		}
	}
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
}

void
MM_RealtimeMarkingScheme::scanSoftReferenceObjects(MM_EnvironmentRealtime *env)
{
	GC_Environment *gcEnv = env->getGCEnvironment();
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
	const UDATA maxIndex = MM_HeapRegionDescriptorRealtime::getReferenceObjectListCount(env);
	UDATA listIndex;
	for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			MM_ReferenceObjectList *referenceObjectList = &_gcExtensions->referenceObjectLists[listIndex];
			referenceObjectList->startSoftReferenceProcessing();
			processReferenceList(env, NULL, referenceObjectList->getPriorSoftList(), &gcEnv->_markJavaStats._weakReferenceStats);
			_scheduler->condYieldFromGC(env);
		}
	}
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
}

void
MM_RealtimeMarkingScheme::scanPhantomReferenceObjects(MM_EnvironmentRealtime *env)
{
	GC_Environment *gcEnv = env->getGCEnvironment();
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
	const UDATA maxIndex = MM_HeapRegionDescriptorRealtime::getReferenceObjectListCount(env);
	UDATA listIndex;
	for (listIndex = 0; listIndex < maxIndex; ++listIndex) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			MM_ReferenceObjectList *referenceObjectList = &_gcExtensions->referenceObjectLists[listIndex];
			referenceObjectList->startPhantomReferenceProcessing();
			processReferenceList(env, NULL, referenceObjectList->getPriorPhantomList(), &gcEnv->_markJavaStats._weakReferenceStats);
			_scheduler->condYieldFromGC(env);
		}
	}
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
}

void
MM_RealtimeMarkingScheme::processReferenceList(MM_EnvironmentRealtime *env, MM_HeapRegionDescriptorRealtime *region, J9Object* headOfList, MM_ReferenceStats *referenceStats)
{
	UDATA objectsVisited = 0;
#if defined(J9VM_GC_FINALIZATION)
	GC_FinalizableReferenceBuffer buffer(_gcExtensions);
#endif /* J9VM_GC_FINALIZATION */
	J9Object* referenceObj = headOfList;

	while (NULL != referenceObj) {
		objectsVisited += 1;
		referenceStats->_candidates += 1;

		Assert_MM_true(isMarked(referenceObj));

		J9Object* nextReferenceObj = _gcExtensions->accessBarrier->getReferenceLink(referenceObj);

		GC_SlotObject referentSlotObject(_gcExtensions->getOmrVM(), &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, referenceObj));
		J9Object *referent = referentSlotObject.readReferenceFromSlot();
		if (NULL != referent) {
			UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(referenceObj)) & J9_JAVA_CLASS_REFERENCE_MASK;
			if (isMarked(referent)) {
				if (J9_JAVA_CLASS_REFERENCE_SOFT == referenceObjectType) {
					U_32 age = J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, referenceObj);
					if (age < _gcExtensions->getMaxSoftReferenceAge()) {
						/* Soft reference hasn't aged sufficiently yet - increment the age */
						J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, referenceObj) = age + 1;
					}
				}
			} else {
				/* transition the state to cleared */
				Assert_MM_true(GC_ObjectModel::REF_STATE_INITIAL == J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj));
				J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj) = GC_ObjectModel::REF_STATE_CLEARED;

				referenceStats->_cleared += 1;

				/* Phantom references keep it's referent alive in Java 8 and doesn't in Java 9 and later */
				if ((J9_JAVA_CLASS_REFERENCE_PHANTOM == referenceObjectType) && ((J2SE_VERSION(_javaVM) & J2SE_VERSION_MASK) <= J2SE_18)) {
					/* Scanning will be done after the enqueuing */
					markObject(env, referent);
				} else {
					referentSlotObject.writeReferenceToSlot(NULL);
				}
#if defined(J9VM_GC_FINALIZATION)
				/* Check if the reference has a queue */
				if (0 != J9GC_J9VMJAVALANGREFERENCE_QUEUE(env, referenceObj)) {
					/* Reference object can be enqueued onto the finalizable list */
					buffer.add(env, referenceObj);
					referenceStats->_enqueued += 1;
					/* Flag for the finalizer */
					_realtimeGC->_finalizationRequired = true;
				}
#endif /* J9VM_GC_FINALIZATION */
			}
		}
		referenceObj = nextReferenceObj;
		if (REFERENCE_OBJECT_YIELD_CHECK_INTERVAL == objectsVisited) {
			_scheduler->condYieldFromGC(env);
			objectsVisited = 0;
		}
	}
#if defined(J9VM_GC_FINALIZATION)
	buffer.flush(env);
#endif /* J9VM_GC_FINALIZATION */
}
