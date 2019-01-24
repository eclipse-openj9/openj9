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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "modronopt.h"
#include "gcutils.h"

#include "mmhook_internal.h"


#include <string.h>
#include "ModronAssertions.h"

#include "RealtimeGC.hpp"

#include "modronapi.hpp"

#include "AllocateDescription.hpp"
#include "BarrierSynchronization.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderLinkedListIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassModel.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentRealtime.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "FinalizableClassLoaderBuffer.hpp"
#include "FinalizeListManager.hpp"
#include "FinalizerSupport.hpp"
#endif /* J9VM_GC_FINALIZATION */
#include "GlobalAllocationManagerSegregated.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceMetronome.hpp"
#include "RealtimeAccessBarrier.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RealtimeMarkTask.hpp"
#include "RealtimeRootScanner.hpp"
#include "ReferenceObjectList.hpp"
#include "ObjectModel.hpp"
#include "OMRVMInterface.hpp"
#include "OSInterface.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "RealtimeSweepTask.hpp"
#include "ReferenceChainWalker.hpp"
#include "RegionPoolSegregated.hpp"
#include "RootScanner.hpp"
#include "Scheduler.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SublistFragment.hpp"
#include "SweepSchemeRealtime.hpp"
#include "Task.hpp"
#include "UnfinalizedObjectList.hpp"
#include "WorkPacketsRealtime.hpp"

void
MM_RealtimeGC::setGCThreadPriority(OMR_VMThread *vmThread, UDATA newGCThreadPriority)
{
	if(newGCThreadPriority == (UDATA) _currentGCThreadPriority) {
		return;
	}
	
	Trc_MM_GcThreadPriorityChanged(vmThread->_language_vmthread, newGCThreadPriority);
	
	/* Walk through all GC threads and set the priority */
	omrthread_t* gcThreadTable = _sched->getThreadTable();
	for (UDATA i = 0; i < _sched->threadCount(); i++) {
		omrthread_set_priority(gcThreadTable[i], newGCThreadPriority);
	}
	_currentGCThreadPriority = (IDATA) newGCThreadPriority;
}

/**
 * Initialization.
 */
bool
MM_RealtimeGC::initialize(MM_EnvironmentBase *env)
{
	_gcPhase = GC_PHASE_IDLE;
	_extensions->realtimeGC = this;
	_allowGrowth = false;
	_unmarkedImpliesCleared = false;
	_unmarkedImpliesStringsCleared = false;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_unmarkedImpliesClasses = false;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	
	if (_extensions->gcTrigger == 0) {
		_extensions->gcTrigger = (_extensions->memoryMax / 2);
		_extensions->gcInitialTrigger = (_extensions->memoryMax / 2);
	}

	_extensions->distanceToYieldTimeCheck = 0;

	/* Only SRT passes this check as the commandline option to specify beatMicro is only enabled on SRT */
	if (METRONOME_DEFAULT_BEAT_MICRO != _extensions->beatMicro) {
		/* User-specified quanta time, adjust related parameters */
		_extensions->timeWindowMicro = 20 * _extensions->beatMicro;
		/* Currently all supported SRT platforms - AIX and Linux, can only use HRT for alarm thread implementation.
		 * The default value for HRT period is 1/3 of the default quanta: 1 msec for HRT period and 3 msec quanta,
		 * we will attempt to adjust the HRT period to 1/3 of the specified quanta.
		 */
		UDATA hrtPeriodMicro = _extensions->beatMicro / 3;
		if ((hrtPeriodMicro < METRONOME_DEFAULT_HRT_PERIOD_MICRO) && (METRONOME_DEFAULT_HRT_PERIOD_MICRO < _extensions->beatMicro)) {
			/* If the adjusted value is too small for the hires clock resolution, we will use the default HRT period provided that
			 * the default period is smaller than the quanta time specified.
			 * Otherwise we fail to initialize the alarm thread with an error message.
			 */
			hrtPeriodMicro = METRONOME_DEFAULT_HRT_PERIOD_MICRO;
		}
		Assert_MM_true(0 != hrtPeriodMicro);
		_extensions->hrtPeriodMicro = hrtPeriodMicro;

		/* On Windows SRT we still use interrupt-based alarm. Set the interrupt period the same as hires timer period.
		 * We will fail to init the alarm if this is too small a resolution for Windows.
		 */
		_extensions->itPeriodMicro = _extensions->hrtPeriodMicro;

		/* if the pause time user specified is larger than the default value, calculate if there is opportunity
		 * for the GC to do time checking less often inside condYieldFromGC.
		 */
		if (METRONOME_DEFAULT_BEAT_MICRO < _extensions->beatMicro) {
			UDATA intervalToSkipYieldCheckMicro = _extensions->beatMicro - METRONOME_DEFAULT_BEAT_MICRO;
			UDATA maxInterYieldTimeMicro = INTER_YIELD_MAX_NS / 1000;
			_extensions->distanceToYieldTimeCheck = (U_32)(intervalToSkipYieldCheckMicro / maxInterYieldTimeMicro);
		}
	}

	_osInterface = MM_OSInterface::newInstance(env);
	if (_osInterface == NULL) {
		return false;
	}
	
	_sched = (MM_Scheduler *)_extensions->dispatcher;

	_workPackets = allocateWorkPackets(env);	
	if (_workPackets == NULL) {
		return false;
	}
	
	/* allocate and initialize the global reference object lists */
	if (!allocateAndInitializeReferenceObjectLists(env)){
		return false;
	}

	/* allocate and initialize the global unfinalized object lists */
	if (!allocateAndInitializeUnfinalizedObjectLists(env)) {
		return false;
	}

	/* allocate and initialize the global ownable synchronizer object lists */
	if (!allocateAndInitializeOwnableSynchronizerObjectLists(env)) {
		return false;
	}

	_markingScheme = MM_RealtimeMarkingScheme::newInstance(env, this);
	if (NULL == _markingScheme) {
		return false;
	}
	
	if (!_delegate.initialize(env, NULL, NULL)) {
		return false;
	}

	_sweepScheme = MM_SweepSchemeRealtime::newInstance(env, this, _sched, _markingScheme->getMarkMap());
	if(NULL == _sweepScheme) {
		return false;
	}

	_stopTracing = false;

	/* create the appropriate access barrier for this configuration of Metronome */
	MM_RealtimeAccessBarrier *accessBarrier = NULL;
	accessBarrier = allocateAccessBarrier(env);
	if (NULL == accessBarrier) {
		return false;
	}
	_extensions->accessBarrier = accessBarrier;

	_sched->collectorInitialized(this);

	return true;
}

bool
MM_RealtimeGC::allocateAndInitializeReferenceObjectLists(MM_EnvironmentBase *env)
{
	const UDATA listCount = MM_HeapRegionDescriptorRealtime::getReferenceObjectListCount(env);
	Assert_MM_true(0 < listCount);
	_extensions->referenceObjectLists = (MM_ReferenceObjectList *)env->getForge()->allocate((sizeof(MM_ReferenceObjectList) * listCount), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _extensions->referenceObjectLists) {
		return false;
	}
	for (UDATA index = 0; index < listCount; index++) {
		new(&_extensions->referenceObjectLists[index]) MM_ReferenceObjectList();
	}
	return true;
}

bool
MM_RealtimeGC::allocateAndInitializeUnfinalizedObjectLists(MM_EnvironmentBase *env)
{
	const UDATA listCount = MM_HeapRegionDescriptorRealtime::getUnfinalizedObjectListCount(env);
	Assert_MM_true(0 < listCount);
	MM_UnfinalizedObjectList *unfinalizedObjectLists = (MM_UnfinalizedObjectList *)env->getForge()->allocate((sizeof(MM_UnfinalizedObjectList) * listCount), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == unfinalizedObjectLists) {
		return false;
	}
	for (UDATA index = 0; index < listCount; index++) {
		new(&unfinalizedObjectLists[index]) MM_UnfinalizedObjectList();
		/* add each list to the global list. we need to maintain the doubly linked list
		 * to ensure uniformity with SE/Balanced.
		 */
		MM_UnfinalizedObjectList *previousUnfinalizedObjectList = (0 == index) ? NULL : &unfinalizedObjectLists[index-1];
		MM_UnfinalizedObjectList *nextUnfinalizedObjectList = ((listCount - 1) == index) ? NULL : &unfinalizedObjectLists[index+1];

		unfinalizedObjectLists[index].setNextList(nextUnfinalizedObjectList);
		unfinalizedObjectLists[index].setPreviousList(previousUnfinalizedObjectList);
	}
	_extensions->unfinalizedObjectLists = unfinalizedObjectLists;
	return true;
}

bool
MM_RealtimeGC::allocateAndInitializeOwnableSynchronizerObjectLists(MM_EnvironmentBase *env)
{
	const UDATA listCount = MM_HeapRegionDescriptorRealtime::getOwnableSynchronizerObjectListCount(env);
	Assert_MM_true(0 < listCount);
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectLists = (MM_OwnableSynchronizerObjectList *)env->getForge()->allocate((sizeof(MM_OwnableSynchronizerObjectList) * listCount), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == ownableSynchronizerObjectLists) {
		return false;
	}
	for (UDATA index = 0; index < listCount; index++) {
		new(&ownableSynchronizerObjectLists[index]) MM_OwnableSynchronizerObjectList();
		/* add each list to the global list. we need to maintain the doubly linked list
		 * to ensure uniformity with SE/Balanced.
		 */
		MM_OwnableSynchronizerObjectList *previousOwnableSynchronizerObjectList = (0 == index) ? NULL : &ownableSynchronizerObjectLists[index-1];
		MM_OwnableSynchronizerObjectList *nextOwnableSynchronizerObjectList = ((listCount - 1) == index) ? NULL : &ownableSynchronizerObjectLists[index+1];

		ownableSynchronizerObjectLists[index].setNextList(nextOwnableSynchronizerObjectList);
		ownableSynchronizerObjectLists[index].setPreviousList(previousOwnableSynchronizerObjectList);
	}
	_extensions->ownableSynchronizerObjectLists = ownableSynchronizerObjectLists;
	return true;
}

/**
 * Initialization.
 */
void
MM_RealtimeGC::tearDown(MM_EnvironmentBase *env)
{
	_delegate.tearDown(env);

	if(NULL != _sched) {
		_sched->kill(env);
		_sched = NULL;
	}
	
	if(NULL != _osInterface) {
		_osInterface->kill(env);
		_osInterface = NULL;
	}
	
	if(NULL != _workPackets) {
		_workPackets->kill(env);
		_workPackets = NULL;
	}
	
	if (NULL != _extensions->referenceObjectLists) {
		env->getForge()->free(_extensions->referenceObjectLists);
		_extensions->referenceObjectLists = NULL;
	}

	if (NULL != _extensions->unfinalizedObjectLists) {
		env->getForge()->free(_extensions->unfinalizedObjectLists);
		_extensions->unfinalizedObjectLists = NULL;
	}

	if (NULL != _extensions->ownableSynchronizerObjectLists) {
		env->getForge()->free(_extensions->ownableSynchronizerObjectLists);
		_extensions->ownableSynchronizerObjectLists = NULL;
	}

	if (NULL != _markingScheme) {
		_markingScheme->kill(env);
		_markingScheme = NULL;
 	}
	
	if (NULL != _sweepScheme) {
		_sweepScheme->kill(env);
		_sweepScheme = NULL;
 	}
	
	if (NULL != _extensions->accessBarrier) {
		_extensions->accessBarrier->kill(env);
		_extensions->accessBarrier = NULL;
	}

}

/**
 * @copydoc MM_GlobalCollector::masterSetupForGC()
 */
void
MM_RealtimeGC::masterSetupForGC(MM_EnvironmentBase *env)
{
	/* Reset memory pools of associated memory spaces */
	env->_cycleState->_activeSubSpace->reset();
	
	_workPackets->reset(env);	
	
	/* Clear the gc stats structure */
	clearGCStats(env);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/* Set the dynamic class unloading flag based on command line and runtime state */
	switch (_extensions->dynamicClassUnloading) {
		case MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER:
			_extensions->runtimeCheckDynamicClassUnloading = false;
			break;
		case MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_ALWAYS:
			_extensions->runtimeCheckDynamicClassUnloading = true;
			break;
		case MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_ON_CLASS_LOADER_CHANGES:
			 _extensions->runtimeCheckDynamicClassUnloading = (_extensions->aggressive || _extensions->classLoaderManager->isTimeForClassUnloading(env));
			break;
		default:
			break;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

#if defined(J9VM_GC_FINALIZATION)
	_finalizationRequired = false;
#endif /* J9VM_GC_FINALIZATION */
}

/**
 * @copydoc MM_GlobalCollector::masterCleanupAfterGC()
 */
void
MM_RealtimeGC::masterCleanupAfterGC(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/* flush the dead class segments if their size exceeds the CacheSize mark. 
	 * Heap fixup should have been completed in this cycle.
	 */
	if (_extensions->classLoaderManager->reclaimableMemory() > _extensions->deadClassLoaderCacheSize) {
		Trc_MM_FlushUndeadSegments_Entry(env->getLanguageVMThread(), "Non-zero reclaimable memory available");
		_extensions->classLoaderManager->flushUndeadSegments(env);
		Trc_MM_FlushUndeadSegments_Exit(env->getLanguageVMThread());
	}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
}

/**
 * Thread initialization.
 */
void
MM_RealtimeGC::workerSetupForGC(MM_EnvironmentBase *env)
{	
}

/**
 */
void
MM_RealtimeGC::clearGCStats(MM_EnvironmentBase *env)
{
	_extensions->globalGCStats.clear();
	_extensions->markJavaStats.clear();
}

/**
 */
void
MM_RealtimeGC::mergeGCStats(MM_EnvironmentBase *env)
{
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_RealtimeGC::processDyingClasses(MM_EnvironmentRealtime *env, UDATA* classUnloadCountResult, UDATA* anonymousClassUnloadCountResult, UDATA* classLoaderUnloadCountResult, J9ClassLoader** classLoaderUnloadListResult)
{
	J9ClassLoader *classLoader = NULL;
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	UDATA classUnloadCount = 0;
	UDATA anonymousClassUnloadCount = 0;
	UDATA classLoaderUnloadCount = 0;
	J9ClassLoader *unloadLink = NULL;
	J9Class *classUnloadList = NULL;
	J9Class *anonymousClassUnloadList = NULL;

	/*
	 * Walk anonymous classes and set unmarked as dying
	 *
	 * Do this walk before classloaders to be unloaded walk to create list of anonymous classes to be unloaded and use it
	 * as sublist to continue to build general list of classes to be unloaded
	 *
	 * Anonymous classes suppose to be allocated one per segment
	 * This is not relevant here however becomes important at segment removal time
	 */
	anonymousClassUnloadList = addDyingClassesToList(env, _javaVM->anonClassLoader, false, anonymousClassUnloadList, &anonymousClassUnloadCount);

	/* class unload list includes anonymous class unload list */
	classUnloadList = anonymousClassUnloadList;
	classUnloadCount += anonymousClassUnloadCount;
	
	GC_ClassLoaderLinkedListIterator classLoaderIterator(env, _extensions->classLoaderManager);
	while(NULL != (classLoader = (J9ClassLoader *)classLoaderIterator.nextSlot())) {
		if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			Assert_MM_true(NULL == classLoader->unloadLink);
			if(_markingScheme->isMarked(classLoader->classLoaderObject) ) {
				classLoader->gcFlags &= ~J9_GC_CLASS_LOADER_SCANNED;
			} else {
				/* Anonymous classloader should not be unloaded */
				Assert_MM_true(0 == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER));

				classLoaderUnloadCount += 1;
				classLoader->gcFlags |= J9_GC_CLASS_LOADER_DEAD;
				
				/* add this loader to the linked list of loaders being unloaded in this cycle */
				classLoader->unloadLink = unloadLink;
				unloadLink = classLoader;

				classUnloadList = addDyingClassesToList(env, classLoader, true, classUnloadList, &classUnloadCount);
			}
		}
		yieldFromClassUnloading(env);
	}
	
	if (0 != classUnloadCount) {
		/* Call classes unload hook */
		TRIGGER_J9HOOK_VM_CLASSES_UNLOAD(_javaVM->hookInterface, vmThread, classUnloadCount, classUnloadList);
		yieldFromClassUnloading(env);
	}
	
	if (0 != anonymousClassUnloadCount) {
		/* Call anonymous classes unload hook */
		TRIGGER_J9HOOK_VM_ANON_CLASSES_UNLOAD(_javaVM->hookInterface, vmThread, anonymousClassUnloadCount, anonymousClassUnloadList);
		yieldFromClassUnloading(env);
	}

	if (0 != classLoaderUnloadCount) {
		/* Call classloader unload hook */
		TRIGGER_J9HOOK_VM_CLASS_LOADERS_UNLOAD(_javaVM->hookInterface, vmThread, unloadLink);
		yieldFromClassUnloading(env);
	}

	/* Ensure that the VM has an accurate anonymous class count */
	_javaVM->anonClassCount -= anonymousClassUnloadCount;

	*classUnloadCountResult = classUnloadCount;
	*anonymousClassUnloadCountResult = anonymousClassUnloadCount;
	*classLoaderUnloadCountResult = classLoaderUnloadCount;
	*classLoaderUnloadListResult = unloadLink;
}

J9Class *
MM_RealtimeGC::addDyingClassesToList(MM_EnvironmentRealtime *env, J9ClassLoader * classLoader, bool setAll, J9Class *classUnloadListStart, UDATA *classUnloadCountResult)
{
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	J9Class *classUnloadList = classUnloadListStart;
	UDATA classUnloadCount = 0;

	if (NULL != classLoader) {
		GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
		J9MemorySegment *segment = NULL;
		while(NULL != (segment = segmentIterator.nextSegment())) {
			GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
			J9Class *clazz = NULL;
			while(NULL != (clazz = classHeapIterator.nextClass())) {

				J9CLASS_EXTENDED_FLAGS_CLEAR(clazz, J9ClassGCScanned);

				J9Object *classObject = clazz->classObject;
				if (setAll || !_markingScheme->isMarked(classObject)) {

					/* with setAll all classes must be unmarked */
					Assert_MM_true(!_markingScheme->isMarked(classObject));

					classUnloadCount += 1;

					/* Remove the class from the subclass traversal list */
					_extensions->classLoaderManager->removeFromSubclassHierarchy(env, clazz);
					/* Mark class as dying */
					clazz->classDepthAndFlags |= J9_JAVA_CLASS_DYING;

					/* Call class unload hook */
					Trc_MM_cleanUpClassLoadersStart_triggerClassUnload(env->getLanguageVMThread(),clazz,
								(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
								J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)));
					TRIGGER_J9HOOK_VM_CLASS_UNLOAD(_javaVM->hookInterface, vmThread, clazz);
						
					/* add class to dying anonymous classes link list */
					clazz->gcLink = classUnloadList;
					classUnloadList = clazz;
				}
			}
		}
	}

	*classUnloadCountResult += classUnloadCount;
	return classUnloadList;
}

/**
 * Free classloaders which are being unloaded during this GC cycle.  Also remove all
 * dead classes from the traversal list.  
 * @note the traversal code belongs in its own function or possibly processDyingClasses
 * or possible processDyingClasses.  It is currently here for historic reasons.
 * 
 * @param deadClassLoaders Linked list of classloaders dying during this GC cycle
 */
void
MM_RealtimeGC::processUnlinkedClassLoaders(MM_EnvironmentBase *envModron, J9ClassLoader *deadClassLoaders)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envModron);
	J9ClassLoader *unloadLink = deadClassLoaders;
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();

	/* Remove dead classes from the traversal list (if necessary) */
	J9Class *jlObject = J9VMJAVALANGOBJECT_OR_NULL(javaVM);
	J9Class *previousClass = jlObject;
	J9Class *nextClass = (NULL != jlObject) ? jlObject->subclassTraversalLink : jlObject;
	while ((NULL != nextClass) && (jlObject != nextClass)) {
		if (J9CLASS_FLAGS(nextClass) & J9_JAVA_CLASS_DYING) {
			while ((NULL != nextClass->subclassTraversalLink) && (jlObject != nextClass) && (J9CLASS_FLAGS(nextClass) & 0x08000000)) {
				nextClass = nextClass->subclassTraversalLink;
			}
			previousClass->subclassTraversalLink = nextClass;
		}
		previousClass = nextClass;
		nextClass = nextClass->subclassTraversalLink;
		/* TODO CRGTMP Do we need to yield here?  Is yielding safe? */
	}

	/* Free memory for dead classloaders */
	while (NULL != unloadLink) {
		J9ClassLoader *nextUnloadLink = unloadLink->unloadLink;
		_javaVM->internalVMFunctions->freeClassLoader(unloadLink, _javaVM, vmThread, 1);
		unloadLink = nextUnloadLink;
		yieldFromClassUnloading(env);
	}
}

void
MM_RealtimeGC::updateClassUnloadStats(MM_EnvironmentBase *env, UDATA classUnloadCount, UDATA anonymousClassUnloadCount, UDATA classLoaderUnloadCount)
{
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;

	/* TODO CRGTMP move global stats into super class implementation once it is created */
	classUnloadStats->_classesUnloadedCount = classUnloadCount;
	classUnloadStats->_anonymousClassesUnloadedCount = anonymousClassUnloadCount;
	classUnloadStats->_classLoaderUnloadedCount = classLoaderUnloadCount;

	/* Record increment stats */
	_extensions->globalGCStats.metronomeStats.classesUnloadedCount = classUnloadCount;
	_extensions->globalGCStats.metronomeStats.anonymousClassesUnloadedCount = anonymousClassUnloadCount;
	_extensions->globalGCStats.metronomeStats.classLoaderUnloadedCount = classLoaderUnloadCount;
}

/**
 * Unload classcloaders that are no longer referenced.  If the classloader has shared
 * libraries open place it on the finalize queue instead of freeing it.
 * 
 */
void
MM_RealtimeGC::unloadDeadClassLoaders(MM_EnvironmentBase *envModron)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envModron);
	J9ClassLoader *unloadLink = NULL;
	UDATA classUnloadCount = 0;
	UDATA anonymousClassUnloadCount = 0;
	UDATA classLoaderUnloadCount = 0;
	J9ClassLoader *classLoadersUnloadedList = NULL;
	J9MemorySegment *reclaimedSegments = NULL;

	/* set the vmState whilst we're unloading classes */
	UDATA vmState = env->pushVMstate(OMRVMSTATE_GC_CLEANING_METADATA);
	
	lockClassUnloadMonitor(env);
	
	processDyingClasses(env, &classUnloadCount, &anonymousClassUnloadCount, &classLoaderUnloadCount, &classLoadersUnloadedList);

	/* cleanup segments in anonymous classloader */
	_extensions->classLoaderManager->cleanUpSegmentsInAnonymousClassLoader(env, &reclaimedSegments);
	
	/* enqueue all the segments we just salvaged from the dead class loaders for delayed free (this work was historically attributed in the unload end operation so it goes after the timer start) */
	_extensions->classLoaderManager->enqueueUndeadClassSegments(reclaimedSegments);

	yieldFromClassUnloading(env);

	GC_FinalizableClassLoaderBuffer buffer(_extensions);
	
	while (NULL != classLoadersUnloadedList) {
		/* fetch the next loader immediately, since we will re-use the unloadLink in this loop */
		J9ClassLoader* classLoader = classLoadersUnloadedList;
		classLoadersUnloadedList = classLoader->unloadLink;
		
		Assert_MM_true(0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED));
		Assert_MM_true(J9_GC_CLASS_LOADER_DEAD == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD));
		Assert_MM_true(0 == (classLoader->gcFlags & (J9_GC_CLASS_LOADER_UNLOADING | J9_GC_CLASS_LOADER_ENQ_UNLOAD)));

		/* Class loader died this collection, so do cleanup work */
		reclaimedSegments = NULL;
		
		/* Perform classLoader-specific clean up work, including freeing the classLoader's class hash table and
		 * class path entries.
		 */
		 _javaVM->internalVMFunctions->cleanUpClassLoader((J9VMThread *)env->getLanguageVMThread(), classLoader);
		 /* free any ROM classes now and enqueue any RAM classes */
		 _extensions->classLoaderManager->cleanUpSegmentsAlongClassLoaderLink(_javaVM, classLoader->classSegments, &reclaimedSegments);
		 /* we are taking responsibility for cleaning these here so free them */
		 classLoader->classSegments = NULL;
		 /* enqueue all the segments we just salvaged from the dead class loaders for delayed free (this work was historically attributed in the unload end operation so it goes after the timer start) */
		 _extensions->classLoaderManager->enqueueUndeadClassSegments(reclaimedSegments);
		 
		 /* Remove this classloader slot */
		 _extensions->classLoaderManager->unlinkClassLoader(classLoader);

#if defined(J9VM_GC_FINALIZATION)
		/* Determine if the classLoader needs to be enqueued for finalization (for shared library unloading),
		 * otherwise add it to the list of classLoaders to be unloaded by cleanUpClassLoadersEnd.
		 */
		if(((NULL != classLoader->sharedLibraries)
		&& (0 != pool_numElements(classLoader->sharedLibraries)))
		|| (_extensions->fvtest_forceFinalizeClassLoaders)) {
			/* Attempt to enqueue the class loader for the finalizer */
			buffer.add(env, classLoader);
			classLoader->gcFlags |= J9_GC_CLASS_LOADER_ENQ_UNLOAD;
			_finalizationRequired = true;
		} else 
#endif /* J9VM_GC_FINALIZATION */
		{
			/* Add the classLoader to the list of classLoaders to unloaded by cleanUpClassLoadersEnd */
			classLoader->unloadLink = unloadLink;
			unloadLink = classLoader;
		}
		yieldFromClassUnloading(env);
	}
	
	buffer.flush(env);

	updateClassUnloadStats(env, classUnloadCount, anonymousClassUnloadCount, classLoaderUnloadCount);
	
	processUnlinkedClassLoaders(env, unloadLink);
	
	unlockClassUnloadMonitor(env);
	
	env->popVMstate(vmState);
}

/**
 * Check to see if it is time to yield.  If it is time to yield the GC
 * must release the classUnloadMonitor before yielding.  Once the GC
 * comes back from the yield it is required to acquire the classUnloadMonitor
 * again.
 */
void
MM_RealtimeGC::yieldFromClassUnloading(MM_EnvironmentRealtime *env)
{
	if (shouldYield(env)) {
		unlockClassUnloadMonitor(env);
		yield(env);
		lockClassUnloadMonitor(env);
	}
}

/**
 * The GC is required to hold the classUnloadMonitor while it is unloading classes.
 * This will ensure that the JIT will abort and ongoing compilations
 */
void
MM_RealtimeGC::lockClassUnloadMonitor(MM_EnvironmentRealtime *env)
{
	/* Grab the classUnloadMonitor so that the JIT and the GC will not interfere with each other */
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	if (0 != omrthread_rwmutex_try_enter_write(_javaVM->classUnloadMutex)) {
#else
	if (0 != omrthread_monitor_try_enter(_javaVM->classUnloadMutex)) {
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
		/* Failed acquire the monitor so interrupt the JIT.  This will allow the GC
		 * to continue unloading classes.
		 */
		TRIGGER_J9HOOK_MM_INTERRUPT_COMPILATION(_extensions->hookInterface, (J9VMThread *)env->getLanguageVMThread());
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
		omrthread_rwmutex_enter_write(_javaVM->classUnloadMutex);
#else
		omrthread_monitor_enter(_javaVM->classUnloadMutex);
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
	}
}

/**
 * Release the classUnloadMonitor.  This will allow the JIT to compile new methods.
 */
void
MM_RealtimeGC::unlockClassUnloadMonitor(MM_EnvironmentRealtime *env)
{
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	omrthread_rwmutex_exit_write(_javaVM->classUnloadMutex);
#else
	omrthread_monitor_exit(_javaVM->classUnloadMutex);
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

void
MM_RealtimeGC::enqueuePointerArraylet(MM_EnvironmentRealtime *env, fj9object_t *arraylet)
{
	env->getWorkStack()->push(env, (void *)ARRAYLET_TO_ITEM(arraylet));
}

UDATA
MM_RealtimeGC::verbose(MM_EnvironmentBase *env) {
	return _sched->verbose();
}

/**
 * @note only called by master thread.
 */
void 
MM_RealtimeGC::doAuxilaryGCWork(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_FINALIZATION)
	if(isFinalizationRequired()) {
		omrthread_monitor_enter(_javaVM->finalizeMasterMonitor);
		_javaVM->finalizeMasterFlags |= J9_FINALIZE_FLAGS_MASTER_WAKE_UP;
		omrthread_monitor_notify_all(_javaVM->finalizeMasterMonitor);
		omrthread_monitor_exit(_javaVM->finalizeMasterMonitor);
	}
#endif /* J9VM_GC_FINALIZATION */
	
	/* Restart the caches for all threads. */
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	J9VMThread *walkThread;
	while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread->omrVMThread);
		((MM_SegregatedAllocationInterface *)(walkEnv->_objectAllocationInterface))->restartCache(walkEnv);
	}
	
	mergeGCStats(env);
}

/**
 * Incremental Collector.
 * Employs a double write barrier that saves overwriting (new) values from unscanned threads and
 * also the first (old) value overwritten by all threads (the latter as in a Yuasa barrier).
 * @note only called by master thread.
 */
void 
MM_RealtimeGC::incrementalCollect(MM_EnvironmentRealtime *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	masterSetupForGC(env);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_dynamicClassUnloadingEnabled = ((_extensions->runtimeCheckDynamicClassUnloading != 0) ? true : false);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	/* Make sure all threads notice GC is ongoing with a barrier. */
	_extensions->globalGCStats.gcCount++;
	if (verbose(env) >= 2) {
		j9tty_printf(PORTLIB, "RealtimeGC::incrementalCollect\n");
	}
	if (verbose(env) >= 3) {
		j9tty_printf(PORTLIB, "RealtimeGC::incrementalCollect   setup and root phase\n");
	}
	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_soft_as_weak;
	}

	setCollectorRootMarking();
	
	reportMarkStart(env);
	MM_RealtimeMarkTask markTask(env, _sched, this, _markingScheme, env->_cycleState);
	_sched->run(env, &markTask);
	reportMarkEnd(env);
	
	/* 
	 * We have to do class unloading before we turn on GC_UNMARK allocation collor.
	 */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (_extensions->runtimeCheckDynamicClassUnloading != 0) {
		MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
		setCollectorUnloadingClassLoaders();
		reportClassUnloadingStart(env);
		classUnloadStats->_startTime = j9time_hires_clock();
		unloadDeadClassLoaders(env);
		classUnloadStats->_endTime = j9time_hires_clock();
		reportClassUnloadingEnd(env);
		
		/* If there was dynamic class unloading checks during the run, record the new number of class
		 * loaders last seen during a DCU pass
		 */
		_extensions->classLoaderManager->setLastUnloadNumOfClassLoaders();		
		_extensions->classLoaderManager->setLastUnloadNumOfAnonymousClasses();
	}
	
	/* Handling of classes done. Return back to "mark if necessary" mode */
	_unmarkedImpliesClasses = false;

	/* Clear the appropriate flags of all classLoaders */
	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	J9ClassLoader *classLoader;
	while((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		classLoader->gcFlags &= ~J9_GC_CLASS_LOADER_SCANNED;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
	/* If the J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK flag is set,
	 * or if we are about to unload classes and free class memory segments
	 * then fix the heap so that it can be walked by debugging tools
	 */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool fixupForClassUnload = (_extensions->classLoaderManager->reclaimableMemory() > _extensions->deadClassLoaderCacheSize);
#else /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	bool fixupForClassUnload = false;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	if (J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK == (((J9JavaVM *)env->getLanguageVM())->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK)
			|| fixupForClassUnload) {
		_fixHeapForWalk = true;
	}	

	/*
	 * Sweeping.
	 */
	reportSweepStart(env);
	MM_RealtimeSweepTask sweepTask(env, _sched, _sweepScheme);
	_sched->run(env, &sweepTask);
	reportSweepEnd(env);

	doAuxilaryGCWork(env);
	
	/* Get all components to clean up after themselves at the end of a collect */
	masterCleanupAfterGC(env);

	_sched->condYieldFromGC(env);
	setCollectorIdle();

	if (verbose(env) >= 3) {
		j9tty_printf(PORTLIB, "RealtimeGC::incrementalCollect   gc complete  %d  MB in use\n", _memoryPool->getBytesInUse() >> 20); 
	}
}

void
MM_RealtimeGC::flushCachedFullRegions(MM_EnvironmentBase *env)
{
	/* delegate to the memory pool to perform the flushing of per-context full regions to the region pool */
	_memoryPool->flushCachedFullRegions(env);
}

/**
 * This function is called at the end of tracing when it is safe for threads to stop
 * allocating black and return to allocating white.  It iterates through all the threads
 * and sets their allocationColor to GC_UNMARK.  It also sets the new thread allocation
 * color to GC_UNMARK.
 **/
void 
MM_RealtimeGC::allThreadsAllocateUnmarked(MM_EnvironmentBase *env) {
	GC_OMRVMInterface::flushCachesForGC(env);
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	
	while(J9VMThread *aThread = vmThreadListIterator.nextVMThread()) {
		MM_EnvironmentRealtime *threadEnv = MM_EnvironmentRealtime::getEnvironment(aThread);	
		assume0(threadEnv->getAllocationColor() == GC_MARK);
		threadEnv->setAllocationColor(GC_UNMARK);
		threadEnv->setMonitorCacheCleared(FALSE);
	}
	_extensions->newThreadAllocationColor = GC_UNMARK;
}

/****************************************
 * VM Garbage Collection API
 ****************************************
 */
/**
 */
void
MM_RealtimeGC::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, U_32 gcCode)
{
	/* Setup the master thread cycle state */
	_cycleState = MM_CycleState();
	env->_cycleState = &_cycleState;
	env->_cycleState->_gcCode = MM_GCCode(gcCode);
	env->_cycleState->_type = _cycleType;
	env->_cycleState->_activeSubSpace = subSpace;

	/* If we are in an excessiveGC level beyond normal then an aggressive GC is
	 * conducted to free up as much space as possible
	 */
	if (!env->_cycleState->_gcCode.isExplicitGC()) {
		if(excessive_gc_normal != _extensions->excessiveGCLevel) {
			/* convert the current mode to excessive GC mode */
			env->_cycleState->_gcCode = MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE);
		}
	}

	/* The minimum free entry size is always re-adjusted at the end of a cycle.
	 * But if the current cycle is triggered due to OOM, at the start of the cycle
	 * set the minimum free entry size to the smallest size class - 16 bytes.
	 */
	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
		_memoryPool->setMinimumFreeEntrySize((1 << J9VMGC_SIZECLASSES_LOG_SMALLEST));
	}

	MM_EnvironmentRealtime *rtEnv = MM_EnvironmentRealtime::getEnvironment(env);
	/* Having heap walkable after the end of GC may be explicitly required through command line option or GC Check*/
	if (rtEnv->getExtensions()->fixHeapForWalk) {
		_fixHeapForWalk = true;
	}
	/* we are about to collect so generate the appropriate cycle start and increment start events */
	reportGCCycleStart(rtEnv);
	_sched->reportStartGCIncrement(rtEnv);
}

/**
 */
void 
MM_RealtimeGC::setupForGC(MM_EnvironmentBase *env)
{
}	

/**
 * @note only called by master thread.
 */
bool
MM_RealtimeGC::internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);

	incrementalCollect(envRealtime);

	_extensions->heap->resetHeapStatistics(true);
	
	return true;
}

void
MM_RealtimeGC::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	MM_GlobalCollector::internalPostCollect(env, subSpace);
	
	/* Reset fixHeapForWalk for the next cycle, no matter who set it */
	_fixHeapForWalk = false;
	
	/* Check if user overrode the default minimumFreeEntrySize */
	if (_extensions->minimumFreeEntrySize != (UDATA)-1) {
		_memoryPool->setMinimumFreeEntrySize(_extensions->minimumFreeEntrySize);
	} else {
		/* Set it dynamically based on free heap after the end of collection */
		float percentFreeHeapAfterCollect = _extensions->heap->getApproximateActiveFreeMemorySize() * 100.0f / _extensions->heap->getMaximumMemorySize();
		_avgPercentFreeHeapAfterCollect = _avgPercentFreeHeapAfterCollect * 0.8f + percentFreeHeapAfterCollect * 0.2f;
		/* Has percent range changed? (for example from [80,90] down to [70,80]) */ 
		UDATA minFreeEntrySize = (UDATA)1 << (((UDATA)_avgPercentFreeHeapAfterCollect / 10) + 1);
		if (minFreeEntrySize != _memoryPool->getMinimumFreeEntrySize()) {
			/* Yes, it did => make sure it changed enough (more than 1% up or below the range boundary) to accept it (in the example, 78.9 is ok, but 79.1 is not */
			if ((UDATA)_avgPercentFreeHeapAfterCollect % 10 >= 1 && (UDATA)_avgPercentFreeHeapAfterCollect % 10 < 9) {
				if (minFreeEntrySize < 16) {
					minFreeEntrySize = 0;
				}
				_memoryPool->setMinimumFreeEntrySize(minFreeEntrySize);
			}
		}
	}

	/*
	 * MM_GC_CYCLE_END is hooked by external components (e.g. JIT), which may cause GC to yield while in the
	 * external callback. Yielding introduces additional METRONOME_INCREMENT_STOP/START verbose events, which must be
	 * processed before the very last METRONOME_INCREMENT_STOP event before the PRIVATE_GC_POST_CYCLE_END event. Otherwise
	 * the METRONOME_INCREMENT_START/END events become out of order and verbose GC will fail.
	 */
	reportGCCycleFinalIncrementEnding(env);

	MM_EnvironmentRealtime *rtEnv = MM_EnvironmentRealtime::getEnvironment(env);
	_sched->reportStopGCIncrement(rtEnv, true);
	_sched->setGCCode(MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_DEFAULT));
	reportGCCycleEnd(rtEnv);
	/*
	 * We could potentially yield during reportGCCycleEnd (e.g. due to JIT callbacks) and the scheduler will only wake up the master if _gcOn is true.
	 * Turn off _gcOn flag at the very last, after cycle end has been reported.
	 */
	_sched->stopGC(rtEnv);
	env->_cycleState->_activeSubSpace = NULL;
}

void
MM_RealtimeGC::reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	MM_CommonGCData commonData;
	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		omrgc_condYieldFromGC
	);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportSyncGCStart(MM_EnvironmentBase *env, GCReason reason, UDATA reasonParameter) 
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA approximateFreeFreeMemorySize;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	
	approximateFreeFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize();
	
	Trc_MM_SynchGCStart(env->getLanguageVMThread(),
		reason,
		getGCReasonAsString(reason),
		reasonParameter,
		approximateFreeFreeMemorySize,
		0
	);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	UDATA classLoaderUnloadedCount = isCollectorIdle()?0:classUnloadStats->_classLoaderUnloadedCount;
	UDATA classesUnloadedCount = isCollectorIdle()?0:classUnloadStats->_classesUnloadedCount;
	UDATA anonymousClassesUnloadedCount = isCollectorIdle()?0:classUnloadStats->_anonymousClassesUnloadedCount;
#else /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	UDATA classLoaderUnloadedCount = 0;
	UDATA classesUnloadedCount = 0;
	UDATA anonymousClassesUnloadedCount = 0;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	
	/* If master thread was blocked at end of GC, waiting for a new GC cycle,
	 * globalGCStats are not cleared yet. Thus, if we haven't started GC yet,
	 * just report 0s for classLoaders unloaded count */
	TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START(_extensions->privateHookInterface,
		env->getOmrVMThread(), j9time_hires_clock(), 
		J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, reason, reasonParameter,
		approximateFreeFreeMemorySize,
		0,
		classLoaderUnloadedCount,
		classesUnloadedCount,
		anonymousClassesUnloadedCount
	);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportSyncGCEnd(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA approximateFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize();
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
	UDATA classLoaderUnloadCount = classUnloadStats->_classLoaderUnloadedCount;
	UDATA classUnloadCount = classUnloadStats->_classesUnloadedCount;
	UDATA anonymousClassUnloadCount = classUnloadStats->_anonymousClassesUnloadedCount;
#else /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	UDATA classLoaderUnloadCount = 0;
	UDATA classUnloadCount = 0;
	UDATA anonymousClassUnloadCount = 0;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	UDATA weakReferenceCount = _extensions->markJavaStats._weakReferenceStats._cleared;
	UDATA softReferenceCount = _extensions->markJavaStats._softReferenceStats._cleared;
	UDATA maxSoftReferenceAge =_extensions->getMaxSoftReferenceAge();
	UDATA softReferenceAge = _extensions->getDynamicMaxSoftReferenceAge();
	UDATA phantomReferenceCount = _extensions->markJavaStats._phantomReferenceStats._cleared;
	UDATA finalizerCount = _extensions->globalGCStats.metronomeStats.getWorkPacketOverflowCount();
	UDATA packetOverflowCount = _extensions->globalGCStats.metronomeStats.getWorkPacketOverflowCount();
	UDATA objectOverflowCount = _extensions->globalGCStats.metronomeStats.getObjectOverflowCount();
			
	Trc_MM_SynchGCEnd(env->getLanguageVMThread(),
		approximateFreeMemorySize,
		0,
		classLoaderUnloadCount,
		classUnloadCount,
		weakReferenceCount,
		softReferenceCount,
		maxSoftReferenceAge,
		softReferenceAge,
		phantomReferenceCount,
		finalizerCount,
		packetOverflowCount,
		objectOverflowCount
	);
	
	TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_END(_extensions->privateHookInterface,
		env->getOmrVMThread(), j9time_hires_clock(), 
		J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_END,
		approximateFreeMemorySize,
		0,
		classLoaderUnloadCount,
		classUnloadCount,
		anonymousClassUnloadCount,
		weakReferenceCount,
		softReferenceCount,
		maxSoftReferenceAge,
		softReferenceAge,
		phantomReferenceCount,
		finalizerCount,
		packetOverflowCount,
		objectOverflowCount
	);		
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportGCCycleStart(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	/* Let VM know that GC cycle is about to start. JIT, in particular uses it,
	 * to not compile while GC cycle is on.
	 */
	omrthread_monitor_enter(((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOnMonitor);
	((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOn = 1;

	UDATA approximateFreeMemorySize = _memoryPool->getApproximateFreeMemorySize();

	Trc_MM_CycleStart(env->getLanguageVMThread(), env->_cycleState->_type, approximateFreeMemorySize);

	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_START,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type
	);

	omrthread_monitor_exit(((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOnMonitor);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportGCCycleEnd(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	omrthread_monitor_enter(((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOnMonitor);


	UDATA approximateFreeMemorySize = _memoryPool->getApproximateFreeMemorySize();

	TRIGGER_J9HOOK_MM_PRIVATE_REPORT_MEMORY_USAGE(_extensions->privateHookInterface,
		env->getOmrVMThread(), j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_REPORT_MEMORY_USAGE,
		_extensions->getForge()->getCurrentStatistics()
	);

	Trc_MM_CycleEnd(env->getLanguageVMThread(), env->_cycleState->_type, approximateFreeMemorySize);

	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		_extensions->globalGCStats.fixHeapForWalkReason,
		_extensions->globalGCStats.fixHeapForWalkTime
	);

	/* If GC cycle just finished, and trigger start was previously generated, generate trigger end now */
	if (_memoryPool->getBytesInUse() < _extensions->gcInitialTrigger) {
		_previousCycleBelowTrigger = true;
		TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END(_extensions->privateHookInterface,
			env->getOmrVMThread(), j9time_hires_clock(),
			J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END
		);
	}
	
	/* Let VM (JIT, in particular) GC cycle is finished. Do a monitor notify, to
	 * unblock parties that waited for the cycle to complete
	 */
	((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOn = 0;
	omrthread_monitor_notify_all(((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOnMonitor);

	omrthread_monitor_exit(((J9JavaVM *)env->getLanguageVM())->omrVM->_gcCycleOnMonitor);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
bool
MM_RealtimeGC::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress)
{
	bool result = _markingScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);

	if (result) {
		if(NULL != _extensions->referenceChainWalkerMarkMap) {
			result = _extensions->referenceChainWalkerMarkMap->heapAddRange(env, size, lowAddress, highAddress);
			if (!result) {
				/* Expansion of Reference Chain Walker Mark Map has failed
				 * Marking Scheme expansion must be reversed
				 */
				_markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
			}
		}
	}
	return result;
}

/**
 */
bool
MM_RealtimeGC::heapRemoveRange(
	MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress,
	void *lowValidAddress, void *highValidAddress)
{
	bool result = _markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	if(NULL != _extensions->referenceChainWalkerMarkMap) {
		result = result && _extensions->referenceChainWalkerMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return result;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * No new memory has been added to a heap reconfiguration.  This call typically is the result
 * of having segment range changes (memory redistributed between segments) or the meaning of
 * memory changed.
 */
void
MM_RealtimeGC::heapReconfigured(MM_EnvironmentBase *env)
{
}

/**
 */
bool
MM_RealtimeGC::collectorStartup(MM_GCExtensionsBase* extensions)
{
	((MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager)->setSweepScheme(_sweepScheme);
	((MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager)->setMarkingScheme(_markingScheme);
	return true;
}

/**
 */	
void
MM_RealtimeGC::collectorShutdown(MM_GCExtensionsBase *extensions)
{
}

/** 
 * Factory method for creating the work packets structure .
 * 
 * @return the WorkPackets to be used for this Collector.
 */
MM_WorkPacketsRealtime* 
MM_RealtimeGC::allocateWorkPackets(MM_EnvironmentBase *env)
{
	return MM_WorkPacketsRealtime::newInstance(env);
}

/**
 * Calls the Scheduler's yielding API to determine if the GC should yield.
 * @return true if the GC should yield, false otherwise
 */ 
bool
MM_RealtimeGC::shouldYield(MM_EnvironmentBase *env)
{
	return _sched->shouldGCYield(MM_EnvironmentRealtime::getEnvironment(env), 0);
}

/**
 * Yield from GC by calling the Scheduler's API.
 */
void
MM_RealtimeGC::yield(MM_EnvironmentBase *env)
{
	_sched->yieldFromGC(MM_EnvironmentRealtime::getEnvironment(env));
}

/**
 * Yield only if the Scheduler deems yielding should occur at the time of the
 * call to this method.
 */ 
bool
MM_RealtimeGC::condYield(MM_EnvironmentBase *env, U_64 timeSlackNanoSec)
{
	return _sched->condYieldFromGC(MM_EnvironmentRealtime::getEnvironment(env), timeSlackNanoSec);
}

bool
MM_RealtimeGC::isMarked(void *objectPtr)
{
	return _markingScheme->isMarked(static_cast<J9Object*>(objectPtr));
}

void
MM_RealtimeGC::reportMarkStart(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_START);
}

void
MM_RealtimeGC::reportMarkEnd(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkEnd(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_END);
}

void
MM_RealtimeGC::reportSweepStart(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_START);
}

void
MM_RealtimeGC::reportSweepEnd(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepEnd(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_END);
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_RealtimeGC::reportClassUnloadingStart(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_ClassUnloadingStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START);
}

void
MM_RealtimeGC::reportClassUnloadingEnd(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;

	Trc_MM_ClassUnloadingEnd(env->getLanguageVMThread(),
							classUnloadStats->_classLoaderUnloadedCount,
							classUnloadStats->_classesUnloadedCount);

	TRIGGER_J9HOOK_MM_CLASS_UNLOADING_END(
		_extensions->hookInterface,
		(J9VMThread *)env->getLanguageVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_CLASS_UNLOADING_END,
		classUnloadStats->_endTime - classUnloadStats->_startTime,
		classUnloadStats->_classLoaderUnloadedCount,
		classUnloadStats->_classesUnloadedCount,
		classUnloadStats->_classUnloadMutexQuiesceTime,
		classUnloadStats->_endSetupTime - classUnloadStats->_startSetupTime,
		classUnloadStats->_endScanTime - classUnloadStats->_startScanTime,
		classUnloadStats->_endPostTime - classUnloadStats->_startPostTime);
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

void
MM_RealtimeGC::reportGCStart(MM_EnvironmentBase *env)
{
	UDATA scavengerCount = 0;
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_GlobalGCStart(env->getLanguageVMThread(), _extensions->globalGCStats.gcCount);

	TRIGGER_J9HOOK_MM_OMR_GLOBAL_GC_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_OMR_GLOBAL_GC_START,
		_extensions->globalGCStats.gcCount,
		scavengerCount,
		env->_cycleState->_gcCode.isExplicitGC() ? 1 : 0,
		env->_cycleState->_gcCode.isAggressiveGC() ? 1: 0,
		_bytesRequested);
}

void
MM_RealtimeGC::reportGCEnd(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA approximateNewActiveFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW);
	UDATA newActiveMemorySize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW);
	UDATA approximateOldActiveFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
	UDATA oldActiveMemorySize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	UDATA approximateLoaActiveFreeMemorySize = (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 );
	UDATA loaActiveMemorySize = (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 );

	/* not including LOA in total (already accounted by OLD */
	UDATA approximateTotalActiveFreeMemorySize = approximateNewActiveFreeMemorySize + approximateOldActiveFreeMemorySize;
	UDATA totalActiveMemorySizeTotal = newActiveMemorySize + oldActiveMemorySize;


	Trc_MM_GlobalGCEnd(env->getLanguageVMThread(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		approximateTotalActiveFreeMemorySize,
		totalActiveMemorySizeTotal
	);

	/* these are assigned to temporary variable out-of-line since some preprocessors get confused if you have directives in macros */
	UDATA approximateActiveFreeMemorySize = 0;
	UDATA activeMemorySize = 0;

	TRIGGER_J9HOOK_MM_PRIVATE_REPORT_MEMORY_USAGE(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_PRIVATE_REPORT_MEMORY_USAGE,
		_extensions->getForge()->getCurrentStatistics()
	);

	TRIGGER_J9HOOK_MM_OMR_GLOBAL_GC_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		j9time_hires_clock(),
		J9HOOK_MM_OMR_GLOBAL_GC_END,
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		approximateNewActiveFreeMemorySize,
		newActiveMemorySize,
		approximateOldActiveFreeMemorySize,
		oldActiveMemorySize,
		(_extensions-> largeObjectArea ? 1 : 0),
		approximateLoaActiveFreeMemorySize,
		loaActiveMemorySize,
		/* We can't just ask the heap for everything of type FIXED, because that includes scopes as well */
		approximateActiveFreeMemorySize,
		activeMemorySize,
		_extensions->globalGCStats.fixHeapForWalkReason,
		_extensions->globalGCStats.fixHeapForWalkTime
	);
}
