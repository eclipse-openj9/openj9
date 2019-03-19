/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "RealtimeGCDelegate.hpp"

#if defined(J9VM_GC_REALTIME)

#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderLinkedListIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "FinalizableClassLoaderBuffer.hpp"
#include "FinalizerSupport.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "ReferenceObjectList.hpp"
#include "UnfinalizedObjectList.hpp"


void
MM_RealtimeGCDelegate::clearGCStats(MM_EnvironmentBase *env)
{
	_extensions->markJavaStats.clear();
}

bool
MM_RealtimeGCDelegate::initialize(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_unmarkedImpliesClasses = false;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

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

	/* create the appropriate access barrier for this configuration of Metronome */
	MM_RealtimeAccessBarrier *accessBarrier = NULL;
	accessBarrier = _realtimeGC->allocateAccessBarrier(env);
	if (NULL == accessBarrier) {
		return false;
	}
	_extensions->accessBarrier = (MM_ObjectAccessBarrier *)accessBarrier;

	J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();
	javaVM->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_USER_REALTIME_ACCESS_BARRIER;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (!_extensions->dynamicClassUnloadingThresholdForced) {
		_extensions->dynamicClassUnloadingThreshold = 1;
	}
	if (!_extensions->dynamicClassUnloadingKickoffThresholdForced) {
		_extensions->dynamicClassUnloadingKickoffThreshold = 0;
	}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	return true;
}

bool
MM_RealtimeGCDelegate::allocateAndInitializeReferenceObjectLists(MM_EnvironmentBase *env)
{
	const UDATA listCount = getReferenceObjectListCount(env);
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
MM_RealtimeGCDelegate::allocateAndInitializeUnfinalizedObjectLists(MM_EnvironmentBase *env)
{
	const UDATA listCount = getUnfinalizedObjectListCount(env);
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
MM_RealtimeGCDelegate::allocateAndInitializeOwnableSynchronizerObjectLists(MM_EnvironmentBase *env)
{
	const UDATA listCount = getOwnableSynchronizerObjectListCount(env);
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

void
MM_RealtimeGCDelegate::tearDown(MM_EnvironmentBase *env)
{
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
	
	if (NULL != _extensions->accessBarrier) {
		_extensions->accessBarrier->kill(env);
		_extensions->accessBarrier = NULL;
	}
}

void
MM_RealtimeGCDelegate::masterSetupForGC(MM_EnvironmentBase *env)
{
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

void
MM_RealtimeGCDelegate::masterCleanupAfterGC(MM_EnvironmentBase *env)
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

void
MM_RealtimeGCDelegate::incrementalCollectStart(MM_EnvironmentRealtime *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_dynamicClassUnloadingEnabled = ((_extensions->runtimeCheckDynamicClassUnloading != 0) ? true : false);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
}

void
MM_RealtimeGCDelegate::incrementalCollect(MM_EnvironmentRealtime *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	_dynamicClassUnloadingEnabled = ((_extensions->runtimeCheckDynamicClassUnloading != 0) ? true : false);

	if (_extensions->runtimeCheckDynamicClassUnloading != 0) {
		MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
		_realtimeGC->setCollectorUnloadingClassLoaders();
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
		_realtimeGC->_fixHeapForWalk = true;
	}
}

void
MM_RealtimeGCDelegate::doAuxilaryGCWork(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_FINALIZATION)
	if(isFinalizationRequired()) {
		omrthread_monitor_enter(_javaVM->finalizeMasterMonitor);
		_javaVM->finalizeMasterFlags |= J9_FINALIZE_FLAGS_MASTER_WAKE_UP;
		omrthread_monitor_notify_all(_javaVM->finalizeMasterMonitor);
		omrthread_monitor_exit(_javaVM->finalizeMasterMonitor);
	}
#endif /* J9VM_GC_FINALIZATION */
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_RealtimeGCDelegate::processDyingClasses(MM_EnvironmentRealtime *env, UDATA* classUnloadCountResult, UDATA* anonymousClassUnloadCountResult, UDATA* classLoaderUnloadCountResult, J9ClassLoader** classLoaderUnloadListResult)
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
			if(_realtimeGC->_markingScheme->isMarked(classLoader->classLoaderObject) ) {
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
MM_RealtimeGCDelegate::addDyingClassesToList(MM_EnvironmentRealtime *env, J9ClassLoader * classLoader, bool setAll, J9Class *classUnloadListStart, UDATA *classUnloadCountResult)
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
				if (setAll || !_realtimeGC->_markingScheme->isMarked(classObject)) {

					/* with setAll all classes must be unmarked */
					Assert_MM_true(!_realtimeGC->_markingScheme->isMarked(classObject));

					classUnloadCount += 1;

					/* Remove the class from the subclass traversal list */
					_extensions->classLoaderManager->removeFromSubclassHierarchy(env, clazz);
					/* Mark class as dying */
					clazz->classDepthAndFlags |= J9AccClassDying;

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
MM_RealtimeGCDelegate::processUnlinkedClassLoaders(MM_EnvironmentBase *envModron, J9ClassLoader *deadClassLoaders)
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
		if (J9CLASS_FLAGS(nextClass) & J9AccClassDying) {
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
MM_RealtimeGCDelegate::updateClassUnloadStats(MM_EnvironmentBase *env, UDATA classUnloadCount, UDATA anonymousClassUnloadCount, UDATA classLoaderUnloadCount)
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
MM_RealtimeGCDelegate::unloadDeadClassLoaders(MM_EnvironmentBase *envModron)
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
MM_RealtimeGCDelegate::yieldFromClassUnloading(MM_EnvironmentRealtime *env)
{
	if (_realtimeGC->shouldYield(env)) {
		unlockClassUnloadMonitor(env);
		_realtimeGC->yield(env);
		lockClassUnloadMonitor(env);
	}
}

/**
 * The GC is required to hold the classUnloadMonitor while it is unloading classes.
 * This will ensure that the JIT will abort and ongoing compilations
 */
void
MM_RealtimeGCDelegate::lockClassUnloadMonitor(MM_EnvironmentRealtime *env)
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
MM_RealtimeGCDelegate::unlockClassUnloadMonitor(MM_EnvironmentRealtime *env)
{
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	omrthread_rwmutex_exit_write(_javaVM->classUnloadMutex);
#else
	omrthread_monitor_exit(_javaVM->classUnloadMutex);
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
}

void
MM_RealtimeGCDelegate::reportClassUnloadingStart(MM_EnvironmentBase *env)
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
MM_RealtimeGCDelegate::reportClassUnloadingEnd(MM_EnvironmentBase *env)
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
MM_RealtimeGCDelegate::reportSyncGCEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA approximateFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize();
#if defined(OMR_GC_DYNAMIC_CLASS_UNLOADING)
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
	UDATA classLoaderUnloadCount = classUnloadStats->_classLoaderUnloadedCount;
	UDATA classUnloadCount = classUnloadStats->_classesUnloadedCount;
	UDATA anonymousClassUnloadCount = classUnloadStats->_anonymousClassesUnloadedCount;
#else /* defined(OMR_GC_DYNAMIC_CLASS_UNLOADING) */
	UDATA classLoaderUnloadCount = 0;
	UDATA classUnloadCount = 0;
	UDATA anonymousClassUnloadCount = 0;
#endif /* defined(OMR_GC_DYNAMIC_CLASS_UNLOADING) */
	UDATA weakReferenceCount = _extensions->markJavaStats._weakReferenceStats._cleared;
	UDATA softReferenceCount = _extensions->markJavaStats._softReferenceStats._cleared;
	UDATA maxSoftReferenceAge = _extensions->getMaxSoftReferenceAge();
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
		env->getOmrVMThread(), omrtime_hires_clock(),
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

#endif /* defined(J9VM_GC_REALTIME) */

