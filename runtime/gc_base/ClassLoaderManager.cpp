
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
#include "segment.h"
#include "ModronAssertions.h"
#include "vmhook_internal.h" /* this file triggers a VM hook, so we need the internal version */

#include "ClassLoaderManager.hpp"

#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "ClassUnloadStats.hpp"
#include "EnvironmentBase.hpp"
#include "FinalizableClassLoaderBuffer.hpp"
#include "GCExtensions.hpp"
#include "GlobalCollector.hpp"
#include "HeapMap.hpp"
#include "ClassLoaderRememberedSet.hpp"

#if defined(J9VM_GC_REALTIME)
extern "C" {
void classLoaderLoadHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
}
#endif /* defined(J9VM_GC_REALTIME) */

MM_ClassLoaderManager *
MM_ClassLoaderManager::newInstance(MM_EnvironmentBase *env, MM_GlobalCollector *globalCollector)
{	
	MM_ClassLoaderManager *classLoaderManager = (MM_ClassLoaderManager *)env->getForge()->allocate(sizeof(MM_ClassLoaderManager), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (classLoaderManager) {
		new(classLoaderManager) MM_ClassLoaderManager(env, globalCollector);
		if (!classLoaderManager->initialize(env)) {
			classLoaderManager->kill(env);
			classLoaderManager = NULL;   			
		}
	}
	return classLoaderManager;
}

void
MM_ClassLoaderManager::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_ClassLoaderManager::tearDown(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (_undeadSegmentListMonitor) {		
		omrthread_monitor_destroy(_undeadSegmentListMonitor);
		_undeadSegmentListMonitor = NULL;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
	if (_classLoaderListMonitor) {
		omrthread_monitor_destroy(_classLoaderListMonitor);
		_classLoaderListMonitor = NULL;
	}
	
#if defined(J9VM_GC_REALTIME)
	if (MM_GCExtensions::getExtensions(env)->isMetronomeGC()) {
		J9HookInterface **vmHookInterface = _javaVM->internalVMFunctions->getVMHookInterface(_javaVM);
		if (NULL != vmHookInterface) {
			(*vmHookInterface)->J9HookUnregister(vmHookInterface, J9HOOK_VM_CLASS_LOADER_INITIALIZED, classLoaderLoadHook, this);
		}
	}
#endif /* defined(J9VM_GC_REALTIME) */
}

/**
 * Initialize the class unload manager structure.  This just means that the monitor and the pointers are initialized.
 *
 * @return true if the initialization was successful, false otherwise.
 */
bool
MM_ClassLoaderManager::initialize(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_firstUndeadSegment = NULL;
	_undeadSegmentsTotalSize = 0;
	
	if (0 != omrthread_monitor_init_with_name(&_undeadSegmentListMonitor, 0, "Undead Segment List Monitor")) {		
		return false;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
	if (0 != omrthread_monitor_init_with_name(&_classLoaderListMonitor, 0, "Class Loader List Monitor")) {		
		return false;
	}
	
	J9HookInterface **vmHookInterface = _javaVM->internalVMFunctions->getVMHookInterface(_javaVM);
	if (NULL == vmHookInterface) {
		return false;
	}
	
#if defined(J9VM_GC_REALTIME)
	/* TODO CRGTMP Remove if once non-realtime collectors use classLoaderManager during
	 * unloadDeadClassLoaders.  This was added to fix CMVC 127599 until stability week is over
	 */
	if (MM_GCExtensions::getExtensions(env)->isMetronomeGC()) {
		if ((*vmHookInterface)->J9HookRegisterWithCallSite(vmHookInterface, J9HOOK_VM_CLASS_LOADER_INITIALIZED, classLoaderLoadHook, OMR_GET_CALLSITE(), this)) {
			return false;
		}
	}
#endif /* defined(J9VM_GC_REALTIME) */
	
	return true;
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_ClassLoaderManager::enqueueUndeadClassSegments(J9MemorySegment *listRoot)
{
	if (NULL != listRoot) {
		omrthread_monitor_enter(_undeadSegmentListMonitor);
		
		while (NULL != listRoot) {
			_undeadSegmentsTotalSize += listRoot->size;
			J9MemorySegment *nextSegment = listRoot->nextSegmentInClassLoader;
			listRoot->nextSegmentInClassLoader = _firstUndeadSegment;
			_firstUndeadSegment = listRoot;
			listRoot = nextSegment;
		}
		omrthread_monitor_exit(_undeadSegmentListMonitor);
	}
}

void
MM_ClassLoaderManager::flushUndeadSegments(MM_EnvironmentBase *env)
{
	omrthread_monitor_enter(_undeadSegmentListMonitor);
	/* now free all the segments */
	J9MemorySegment *walker = _firstUndeadSegment;
	_firstUndeadSegment = NULL;
	_undeadSegmentsTotalSize = 0;
	omrthread_monitor_exit(_undeadSegmentListMonitor);
	
	while (NULL != walker) {
		J9MemorySegment *thisWalk = walker;
		walker = thisWalk->nextSegmentInClassLoader;
		_javaVM->internalVMFunctions->freeMemorySegment(_javaVM, thisWalk, TRUE);
		_globalCollector->condYield(env, 0);
	}
}

void
MM_ClassLoaderManager::setLastUnloadNumOfClassLoaders() 
{
	_lastUnloadNumOfClassLoaders =  (UDATA)pool_numElements(_javaVM->classLoaderBlocks); 
}

void
MM_ClassLoaderManager::setLastUnloadNumOfAnonymousClasses()
{
	_lastUnloadNumOfAnonymousClasses = _javaVM->anonClassCount;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

void
MM_ClassLoaderManager::unlinkClassLoader(J9ClassLoader *classLoader)
{
	omrthread_monitor_enter(_classLoaderListMonitor);
	J9_LINEAR_LINKED_LIST_REMOVE(gcLinkNext, gcLinkPrevious, _classLoaders, classLoader);
	omrthread_monitor_exit(_classLoaderListMonitor);
	
}

void
MM_ClassLoaderManager::linkClassLoader(J9ClassLoader *classLoader)
{
	omrthread_monitor_enter(_classLoaderListMonitor);
	J9_LINEAR_LINKED_LIST_ADD(gcLinkNext, gcLinkPrevious, _classLoaders, classLoader);
	omrthread_monitor_exit(_classLoaderListMonitor);
}

bool
MM_ClassLoaderManager::isTimeForClassUnloading(MM_EnvironmentBase *env)
{
	bool result = false;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)

	UDATA numClassLoaderBlocks = pool_numElements(_javaVM->classLoaderBlocks);
	UDATA numAnonymousClasses = _javaVM->anonClassCount;

	Trc_MM_GlobalCollector_isTimeForClassUnloading_Entry(
			_extensions->dynamicClassUnloading,
			numClassLoaderBlocks,
			_extensions->dynamicClassUnloadingThreshold,
			_lastUnloadNumOfClassLoaders
	);

	Trc_MM_GlobalCollector_isTimeForClassUnloading_anonClasses(
		numAnonymousClasses,
		_lastUnloadNumOfAnonymousClasses,
		 _extensions->classUnloadingAnonymousClassWeight
	);

	Assert_MM_true(numAnonymousClasses >= _lastUnloadNumOfAnonymousClasses);

	if ( _extensions->dynamicClassUnloading != MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER ) {
		UDATA recentlyLoaded = (UDATA) ((numAnonymousClasses - _lastUnloadNumOfAnonymousClasses) *  _extensions->classUnloadingAnonymousClassWeight);
		/* todo aryoung: _lastUnloadNumOfClassLoaders includes the class loaders which
		 * were unloaded but still required finalization when the last classUnloading occured.
		 * This means that the threshold check is wrong when there are classes which require finalization.
		 * Temporarily make sure that we do not create a negative recently loaded.
		 */
		if (numClassLoaderBlocks >= _lastUnloadNumOfClassLoaders) {
			recentlyLoaded += (numClassLoaderBlocks - _lastUnloadNumOfClassLoaders);
		}
		result = recentlyLoaded >= _extensions->dynamicClassUnloadingThreshold;
	}
	
	Trc_MM_GlobalCollector_isTimeForClassUnloading_Exit(result ? "true" : "false");

#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	return result;
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
J9ClassLoader *
MM_ClassLoaderManager::identifyClassLoadersToUnload(MM_EnvironmentBase *env, MM_HeapMap *markMap, MM_ClassUnloadStats* classUnloadStats)
{
	Trc_MM_identifyClassLoadersToUnload_Entry(env->getLanguageVMThread());
	
	Assert_MM_true(NULL != markMap);
	J9ClassLoader *unloadLink = NULL;
	classUnloadStats->_classLoaderCandidates = 0;

	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	J9ClassLoader * classLoader = NULL;
	while( NULL != (classLoader = classLoaderIterator.nextSlot()) ) {
		classUnloadStats->_classLoaderCandidates += 1;
		/* Check if the class loader is already DEAD - ignore if it is */
		if( J9_GC_CLASS_LOADER_DEAD == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) ) {
			/* If the class loader is already dead, it should be enqueued or unloading by now */  
			Assert_MM_true( 0 != (classLoader->gcFlags & (J9_GC_CLASS_LOADER_UNLOADING | J9_GC_CLASS_LOADER_ENQ_UNLOAD)) );
			Assert_MM_true( 0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED) ); 
		} else {
			/* If the class loader isn't already dead, it must not be enqueued or unloading */  
			Assert_MM_true( 0 == (classLoader->gcFlags & (J9_GC_CLASS_LOADER_UNLOADING | J9_GC_CLASS_LOADER_ENQ_UNLOAD)) );
			Assert_MM_true(NULL == classLoader->unloadLink);

			/* Is the class loader still alive? (object may be NULL while the loader is being initialized) */
			J9Object *classLoaderObject = classLoader->classLoaderObject;
			if( (NULL != classLoaderObject) && (!markMap->isBitSet(classLoaderObject)) ) {
				/* Anonymous classloader should not be unloaded */
				Assert_MM_true(0 == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER));
				Assert_MM_true( 0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED) ); 

				/* add this loader to the linked list of loaders being unloaded in this cycle */
				classLoader->unloadLink = unloadLink;
				unloadLink = classLoader;
			} else {
				if (MM_GCExtensions::getExtensions(env)->isVLHGC()) {
					/* we don't use the SCANNED flag in VLHGC */
					Assert_MM_true(0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED)); 
				} else {
					/* TODO: Once SE stops using the SCANNED flag this path can be removed */
					/* Anonymous classloader might not have SCANNED flag set */
					if (0 == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER)) {
						Assert_MM_true(J9_GC_CLASS_LOADER_SCANNED == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED));
					}
					classLoader->gcFlags &= ~J9_GC_CLASS_LOADER_SCANNED;
				}
			}
		}
	}

	Trc_MM_identifyClassLoadersToUnload_Exit(env->getLanguageVMThread());
	
	return unloadLink;
}

void
MM_ClassLoaderManager::cleanUpClassLoadersStart(MM_EnvironmentBase *env, J9ClassLoader* classLoaderUnloadList, MM_HeapMap *markMap, MM_ClassUnloadStats *classUnloadStats)
{
	UDATA classUnloadCount = 0;
	UDATA anonymousClassUnloadCount = 0;
	UDATA classLoaderUnloadCount = 0;
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();

	J9Class *classUnloadList = NULL;
	J9Class *anonymousClassUnloadList = NULL;
	
	Trc_MM_cleanUpClassLoadersStart_Entry(env->getLanguageVMThread());
	
	/*
	 * Walk anonymous classes and set unmarked as dying
	 *
	 * Do this walk before classloaders to be unloaded walk to create list of anonymous classes to be unloaded and use it
	 * as sublist to continue to build general list of classes to be unloaded
	 *
	 * Anonymous classes suppose to be allocated one per segment
	 * This is not relevant here however becomes important at segment removal time
	 */
	anonymousClassUnloadList = addDyingClassesToList(env, _javaVM->anonClassLoader, markMap, false, anonymousClassUnloadList, &anonymousClassUnloadCount);

	/* class unload list includes anonymous class unload list */
	classUnloadList = anonymousClassUnloadList;
	classUnloadCount += anonymousClassUnloadCount;

	/* Count all classes loaded by dying class loaders */
	J9ClassLoader * classLoader = classLoaderUnloadList;
	while (NULL != classLoader) {
		Assert_MM_true( 0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED) );
		classLoaderUnloadCount += 1;
		classLoader->gcFlags |= J9_GC_CLASS_LOADER_DEAD;

		/* mark all of its classes as dying */
		classUnloadList = addDyingClassesToList(env, classLoader, markMap, true, classUnloadList, &classUnloadCount);

		classLoader = classLoader->unloadLink;
	}

	if (0 != classUnloadCount) {
		/* Call classes unload hook */
		Trc_MM_cleanUpClassLoadersStart_triggerClassesUnload(env->getLanguageVMThread(), classUnloadCount);
		TRIGGER_J9HOOK_VM_CLASSES_UNLOAD(_javaVM->hookInterface, vmThread, classUnloadCount, classUnloadList);
	}

	if (0 != anonymousClassUnloadCount) {
		/* Call anonymous classes unload hook */
		Trc_MM_cleanUpClassLoadersStart_triggerAnonymousClassesUnload(env->getLanguageVMThread(), anonymousClassUnloadCount);
		TRIGGER_J9HOOK_VM_ANON_CLASSES_UNLOAD(_javaVM->hookInterface, vmThread, anonymousClassUnloadCount, anonymousClassUnloadList);
	}

	if (0 != classLoaderUnloadCount) {
		/* Call classloader unload hook */
		Trc_MM_cleanUpClassLoadersStart_triggerClassLoadersUnload(env->getLanguageVMThread(), classLoaderUnloadCount);
		TRIGGER_J9HOOK_VM_CLASS_LOADERS_UNLOAD(_javaVM->hookInterface, vmThread, classLoaderUnloadList);
	}

	classUnloadStats->_classesUnloadedCount = classUnloadCount;
	classUnloadStats->_classLoaderUnloadedCount = classLoaderUnloadCount;
	classUnloadStats->_anonymousClassesUnloadedCount = anonymousClassUnloadCount;

	/* Ensure that the vm has an accurate number of currently loaded anonymous classes  */
	_javaVM->anonClassCount -= anonymousClassUnloadCount;

	Trc_MM_cleanUpClassLoadersStart_Exit(env->getLanguageVMThread());
}

J9Class *
MM_ClassLoaderManager::addDyingClassesToList(MM_EnvironmentBase *env, J9ClassLoader * classLoader, MM_HeapMap *markMap, bool setAll, J9Class *classUnloadListStart, UDATA *classUnloadCountResult)
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
				J9Object *classObject = clazz->classObject;
				if (setAll || !markMap->isBitSet(classObject)) {

					/* with setAll all classes must be unmarked */
					Assert_MM_true(!markMap->isBitSet(classObject));

					classUnloadCount += 1;

					/* Remove the class from the subclass traversal list */
					removeFromSubclassHierarchy(env, clazz);

					/* Mark class as dying */
					clazz->classDepthAndFlags |= J9AccClassDying;

					/* For CMVC 137275. For all dying classes we poison the classObject
					 * field to J9_INVALID_OBJECT to investigate the origin of a class object
					 * reference whose class has been unloaded.
					 */
					clazz->classObject = (j9object_t) J9_INVALID_OBJECT;

					/* Call class unload hook */
					Trc_MM_cleanUpClassLoadersStart_triggerClassUnload(env->getLanguageVMThread(),clazz,
								(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
								J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)));
					TRIGGER_J9HOOK_VM_CLASS_UNLOAD(_javaVM->hookInterface, vmThread, clazz);

					/* add class to dying classes link list */
					clazz->gcLink = classUnloadList;
					classUnloadList = clazz;
				}
			}
		}
	}

	*classUnloadCountResult += classUnloadCount;
	return classUnloadList;
}

void
MM_ClassLoaderManager::cleanUpClassLoadersEnd(MM_EnvironmentBase *env, J9ClassLoader* unloadLink) 
{
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	J9MemorySegment *reclaimedSegments = NULL;
	
	Trc_MM_cleanUpClassLoadersEnd_Entry(vmThread);

	/* Unload classes in the unload link and pass back any RAM Classes that we encounter so the caller can decide when to free them */
	Trc_MM_cleanUpClassLoadersEnd_deleteDeadClassLoaderClassSegmentsStart(env->getLanguageVMThread());
	Trc_MM_cleanUpClassLoadersEnd_unloadClassLoadersNotRequiringFinalizerStart(env->getLanguageVMThread());
	while (NULL != unloadLink) {
		J9ClassLoader *nextUnloadLink = unloadLink->unloadLink;
		J9MemorySegment *segment = unloadLink->classSegments;
		/* now clean up all the segments for this class loader */
		cleanUpSegmentsAlongClassLoaderLink(_javaVM, segment, &reclaimedSegments);
		_javaVM->internalVMFunctions->freeClassLoader(unloadLink, _javaVM, vmThread, 1);
		unloadLink = nextUnloadLink;
	}

	/* we should have already cleaned up the segments attached to this class so it should return none reclaimed */
	Assert_MM_true(NULL == reclaimedSegments);

	Trc_MM_cleanUpClassLoadersEnd_Exit(env->getLanguageVMThread());
}

void
MM_ClassLoaderManager::cleanUpSegmentsAlongClassLoaderLink(J9JavaVM *javaVM, J9MemorySegment *segment, J9MemorySegment **reclaimedSegments)
{
	while (NULL != segment) {
		J9MemorySegment *nextSegment = segment->nextSegmentInClassLoader;
		if (segment->type & MEMORY_TYPE_RAM_CLASS) {
			segment->type |= MEMORY_TYPE_UNDEAD_CLASS;
			/* we also need to unset the fact that this is a RAM CLASS since some code which walks the segment list is looking for still-valid ones */
			segment->type &= ~MEMORY_TYPE_RAM_CLASS;
			segment->nextSegmentInClassLoader = *reclaimedSegments;
			*reclaimedSegments = segment;
			segment->classLoader = NULL;
		} else if (!(segment->type & MEMORY_TYPE_UNDEAD_CLASS)) {
			javaVM->internalVMFunctions->freeMemorySegment(javaVM, segment, 1);
		}
		segment = nextSegment;
	}
}

void
MM_ClassLoaderManager::cleanUpSegmentsInAnonymousClassLoader(MM_EnvironmentBase *env, J9MemorySegment **reclaimedSegments)
{
	if (NULL != _javaVM->anonClassLoader) {
		J9MemorySegment **previousSegmentPointer = &_javaVM->anonClassLoader->classSegments;
		J9MemorySegment *segment = *previousSegmentPointer;

		while (NULL != segment) {
			J9MemorySegment *nextSegment = segment->nextSegmentInClassLoader;
			bool removed = false;
			if (MEMORY_TYPE_RAM_CLASS == (segment->type & MEMORY_TYPE_RAM_CLASS)) {
				GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
				/* Get anonymous class from this segment */
				J9Class *clazz = classHeapIterator.nextClass();
				/* Anonymous classes expected to be allocated one per segment */
				Assert_MM_true(NULL == classHeapIterator.nextClass());

				if (J9AccClassDying == (J9CLASS_FLAGS(clazz) & J9AccClassDying)) {
					/* TODO replace this to better algorithm */
					/* Try to find ROM class for unloading anonymous RAM class if it is not an array */
					if (!_extensions->objectModel.isIndexable(clazz)) {
						J9ROMClass *romClass = clazz->romClass;
						if (NULL != romClass) {
							J9MemorySegment **previousSegmentPointerROM = &_javaVM->anonClassLoader->classSegments;
							J9MemorySegment *segmentROM = *previousSegmentPointerROM;

							/*
							 *  Walk all anonymous classloader's ROM memory segments
							 *  If ROM class is allocated there it would be one per segment
							 */
							while (NULL != segmentROM) {
								J9MemorySegment *nextSegmentROM = segmentROM->nextSegmentInClassLoader;
								if ((MEMORY_TYPE_ROM_CLASS == (segmentROM->type & MEMORY_TYPE_ROM_CLASS)) && ((J9ROMClass *)segmentROM->heapBase == romClass)) {
									/* found! */
									/* remove memory segment from list */
									*previousSegmentPointerROM = nextSegmentROM;
									/* correct pre-cached next for RAM iteration cycle if necessary */
									if (nextSegment == segmentROM) {
										nextSegment = nextSegmentROM;
									}
									/* correct pre-cached previous for RAM iteration cycle if necessary */
									if (segmentROM->nextSegmentInClassLoader == segment) {
										previousSegmentPointer = previousSegmentPointerROM;
									}
									/* ...and free memory segment */
									_javaVM->internalVMFunctions->freeMemorySegment(_javaVM, segmentROM, 1);
									break;
								}
								previousSegmentPointerROM = &segmentROM->nextSegmentInClassLoader;
								segmentROM = nextSegmentROM;
							}
						}
					}

					segment->type |= MEMORY_TYPE_UNDEAD_CLASS;
					/* we also need to unset the fact that this is a RAM CLASS since some code which walks the segment list is looking for still-valid ones */
					segment->type &= ~MEMORY_TYPE_RAM_CLASS;
					segment->nextSegmentInClassLoader = *reclaimedSegments;
					*reclaimedSegments = segment;
					segment->classLoader = NULL;
					/* remove RAM memory segment from classloader segments list */
					*previousSegmentPointer = nextSegment;
					removed = true;
				}
			}
			if (!removed) {
				previousSegmentPointer = &segment->nextSegmentInClassLoader;
			}
			segment = nextSegment;
		}
	}
}

void
MM_ClassLoaderManager::removeFromSubclassHierarchy(MM_EnvironmentBase *env, J9Class *clazzPtr)
{
	J9Class* nextLink = clazzPtr->subclassTraversalLink;
	J9Class* reverseLink = clazzPtr->subclassTraversalReverseLink;
	
	reverseLink->subclassTraversalLink = nextLink;
	nextLink->subclassTraversalReverseLink = reverseLink;
	
	/* link this obsolete class to itself so that it won't have dangling pointers into the subclass traversal list */
	clazzPtr->subclassTraversalLink = clazzPtr;
	clazzPtr->subclassTraversalReverseLink = clazzPtr;
}

void
MM_ClassLoaderManager::cleanUpClassLoaders(MM_EnvironmentBase *env, J9ClassLoader *classLoadersUnloadedList, J9MemorySegment** reclaimedSegments, J9ClassLoader ** unloadLink, volatile bool* finalizationRequired)
{
	*reclaimedSegments = NULL;
	*unloadLink = NULL;

	/*
	 * Cleanup segments in anonymous classloader
	 */
	cleanUpSegmentsInAnonymousClassLoader(env, reclaimedSegments);
	
	/* For each classLoader that is not already unloading, not scanned and not enqueued for finalization:
	 * perform classLoader-specific clean up, if it died on the current collection cycle; and either enqueue it for
	 * finalization, if it needs any shared libraries to be unloaded, or add it to the list of classLoaders to be
	 * unloaded by cleanUpClassLoadersEnd.
	 */
	GC_FinalizableClassLoaderBuffer buffer(_extensions);
	while (NULL != classLoadersUnloadedList) {
		/* fetch the next loader immediately, since we will re-use the unloadLink in this loop */
		J9ClassLoader* classLoader = classLoadersUnloadedList;
		classLoadersUnloadedList = classLoader->unloadLink;
		
		Assert_MM_true(0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED)); /* we don't use the SCANNED flag in VLHGC */
		Assert_MM_true(J9_GC_CLASS_LOADER_DEAD == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD));
		Assert_MM_true(0 == (classLoader->gcFlags & (J9_GC_CLASS_LOADER_UNLOADING | J9_GC_CLASS_LOADER_ENQ_UNLOAD)));

		Trc_MM_ClassLoaderUnload(env->getLanguageVMThread());

		/* Perform classLoader-specific clean up work, including freeing the classLoader's class hash table and
		 * class path entries.
		 */
		_javaVM->internalVMFunctions->cleanUpClassLoader((J9VMThread *)env->getLanguageVMThread(), classLoader);

#if defined(J9VM_GC_FINALIZATION)
		/* Determine if the classLoader needs to be enqueued for finalization (for shared library unloading),
		 * otherwise add it to the list of classLoaders to be unloaded by cleanUpClassLoadersEnd.
		 */
		if(((NULL != classLoader->sharedLibraries)
		&& (0 != pool_numElements(classLoader->sharedLibraries)))
		|| (_extensions->fvtest_forceFinalizeClassLoaders)) {
			/* Enqueue the class loader for the finalizer */
			buffer.add(env, classLoader);
			classLoader->gcFlags |= J9_GC_CLASS_LOADER_ENQ_UNLOAD;
			*finalizationRequired = true;
		} else {
			/* Add the classLoader to the list of classLoaders to be unloaded by cleanUpClassLoadersEnd */
			classLoader->unloadLink = *unloadLink;
			*unloadLink = classLoader;
		}
#endif /* J9VM_GC_FINALIZATION */

		/* free any ROM classes now and enqueue any RAM classes */
		cleanUpSegmentsAlongClassLoaderLink(_javaVM, classLoader->classSegments, reclaimedSegments);

		/* we are taking responsibility for cleaning these here so free them */
		classLoader->classSegments = NULL;
		
		/* perform any configuration specific clean up */
		if (_extensions->isVLHGC()) {
			MM_ClassLoaderRememberedSet *classLoaderRememberedSet = _extensions->classLoaderRememberedSet;
			if (MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType) {
				/* during PGCs we should never unload a class loader which is remembered because it could have instances */
				Assert_MM_false(classLoaderRememberedSet->isRemembered(env, classLoader));
			}
			classLoaderRememberedSet->killRememberedSet(env, classLoader);
		}
	}
	buffer.flush(env);
}

bool
MM_ClassLoaderManager::tryEnterClassUnloadMutex(MM_EnvironmentBase *env)
{
	bool result = true;
	
	/* now, perform any actions that the global collector needs to take before a collection starts */
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	if (0 != omrthread_rwmutex_try_enter_write(_javaVM->classUnloadMutex))
#else
	if (0 != omrthread_monitor_try_enter(_javaVM->classUnloadMutex))
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
	{
		/* The JIT currently is in the monitor */
		/* We can simply skip unloading classes for this GC */
		result = false;
	}
	return result;
}

U_64
MM_ClassLoaderManager::enterClassUnloadMutex(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 quiesceTime = J9CONST64(0);
	
	/* now, perform any actions that the global collector needs to take before a collection starts */
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	if (0 != omrthread_rwmutex_try_enter_write(_javaVM->classUnloadMutex))
#else
	if (0 != omrthread_monitor_try_enter(_javaVM->classUnloadMutex))
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
	{
		/* The JIT currently is in the monitor */
		/* We must interrupt the JIT compilation so the GC can unload classes */
		U_64 startTime = j9time_hires_clock();
		TRIGGER_J9HOOK_MM_INTERRUPT_COMPILATION(_extensions->hookInterface, (J9VMThread *)env->getLanguageVMThread());
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
		omrthread_rwmutex_enter_write(_javaVM->classUnloadMutex);
#else
		omrthread_monitor_enter(_javaVM->classUnloadMutex);
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
		U_64 endTime = j9time_hires_clock();
		quiesceTime = j9time_hires_delta(startTime, endTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	}
	return quiesceTime;
}
	
void
MM_ClassLoaderManager::exitClassUnloadMutex(MM_EnvironmentBase *env)
{
	/* If we allowed class unloading during this gc, we must release the classUnloadMutex */
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	omrthread_rwmutex_exit_write(_javaVM->classUnloadMutex);
#else
	omrthread_monitor_exit(_javaVM->classUnloadMutex);
#endif /* J9VM_JIT_CLASS_UNLOAD_RWMONITOR */
}

#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


#if defined(J9VM_GC_REALTIME)
/* TODO CRGTMP Remove #if defined once non-realtime collectors use classLoaderManager during
 * unloadDeadClassLoaders.  This was added to fix CMVC 127599 until stability week is over
 */
extern "C" {
	
void classLoaderLoadHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassLoaderInitializedEvent* event = (J9VMClassLoaderInitializedEvent*)eventData;
	MM_ClassLoaderManager *manager = (MM_ClassLoaderManager *)userData;
	/* TODO CRGTMP should we be setting the SCANNED bit on the classloader if we are in trace phase? */
	manager->linkClassLoader(event->classLoader);
}

}
#endif /* J9VM_GC_REALTIME */
