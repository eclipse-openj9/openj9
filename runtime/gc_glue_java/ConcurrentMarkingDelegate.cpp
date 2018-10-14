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

#include "ConcurrentMarkingDelegate.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "AsyncCallbackHandler.hpp"
#include "ClassLoaderIterator.hpp"
#include "ConfigurationDelegate.hpp"
#include "FinalizeListManager.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "ReferenceObjectList.hpp"
#include "StackSlotValidator.hpp"
#include "VMInterface.hpp"
#include "VMThreadListIterator.hpp"

/**
 * Concurrents stack slot iterator.
 * Called for each slot in a threads active stack frames which contains a object reference.
 *
 * @param objectInidrect
 * @param localdata
 * @param isDerivedPointer
 * @param objectIndirectBase
 */
void
concurrentStackSlotIterator(J9JavaVM *javaVM, omrobjectptr_t *objectIndirect, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	MM_ConcurrentMarkingDelegate::markSchemeStackIteratorData *data = (MM_ConcurrentMarkingDelegate::markSchemeStackIteratorData *)localData;

	omrobjectptr_t object = *objectIndirect;
	if (data->env->getExtensions()->heap->objectIsInGap(object)) {
		/* CMVC 136483:  Ensure that the object is not in the gap of a split heap (stack-allocated object) since we can't mark that part of the address space */
		Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, object, stackLocation, walkState).validate(data->env));
	} else if (data->markingScheme->isHeapObject(object)) {
		/* heap object - validate and mark */
		Assert_MM_validStackSlot(MM_StackSlotValidator(0, object, stackLocation, walkState).validate(data->env));
		data->markingScheme->markObject(data->env, object);
	} else if (NULL != object) {
		/* stack object - just validate */
		Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, object, stackLocation, walkState).validate(data->env));
	}
}

bool
MM_ConcurrentMarkingDelegate::initialize(MM_EnvironmentBase *env, MM_ConcurrentGC *collector)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	_javaVM = (J9JavaVM *)extensions->getOmrVM()->_language_vm;
	_objectModel = &(extensions->objectModel);
	_markingScheme = collector->getMarkingScheme();
	_collector = collector;
	return true;
}

void
MM_ConcurrentMarkingDelegate::signalThreadsToTraceStacks(MM_EnvironmentBase *env)
{
	uintptr_t threadCount = 0;

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_VMInterface::lockVMThreadList(extensions);

	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	J9VMThread *walkThread;

	while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		MM_AsyncCallbackHandler::signalThreadForCallback(walkThread);
		threadCount += 1;
	}
	GC_VMInterface::unlockVMThreadList(extensions);

	_collector->getConcurrentGCStats()->setThreadsToScanCount(threadCount);
}

void
MM_ConcurrentMarkingDelegate::signalThreadsToActivateWriteBarrier(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_VMInterface::lockVMThreadList(extensions);

	J9VMThread *walkThread;
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	while ((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		walkThread->privateFlags |= J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
	}
	GC_VMInterface::unlockVMThreadList(extensions);
}

void
MM_ConcurrentMarkingDelegate::signalThreadsToDeactivateWriteBarrier(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_javaVM);
	if (extensions->optimizeConcurrentWB) {
		GC_VMInterface::lockVMThreadList(extensions);
		GC_VMThreadListIterator vmThreadListIterator(_javaVM);
		J9VMThread *walkThread;

		/* Reset vmThread flag so mutators don't dirty cards or run write barriers until next concurrent KO */
		while ((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
			walkThread->privateFlags &= ~J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
		}
		GC_VMInterface::unlockVMThreadList(extensions);
	}
}

void
MM_ConcurrentMarkingDelegate::signalThreadsToDirtyCards(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_VMInterface::lockVMThreadList(extensions);

	J9VMThread *walkThread;
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
		walkThread->privateFlags |= J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
	}
	GC_VMInterface::unlockVMThreadList(extensions);
}

void
MM_ConcurrentMarkingDelegate::signalThreadsToStopDirtyingCards(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_javaVM);
	if (extensions->optimizeConcurrentWB) {
		GC_VMInterface::lockVMThreadList(extensions);
		GC_VMThreadListIterator vmThreadListIterator(_javaVM);
		J9VMThread *walkThread;

		/* Reset vmThread flag so mutators don't dirty cards until next concurrent KO */
		while((walkThread = vmThreadListIterator.nextVMThread()) != NULL) {
			walkThread->privateFlags &= ~J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
		}
		GC_VMInterface::unlockVMThreadList(extensions);
	}
}

bool
MM_ConcurrentMarkingDelegate::scanThreadRoots(MM_EnvironmentBase *env)
{
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	Assert_GC_true_with_message(env, vmThread->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, "MM_ConcurrentStats::_executionMode = %zu\n",
			_collector->getConcurrentGCStats()->getExecutionMode());

	GC_VMThreadIterator vmThreadIterator(vmThread);
	omrobjectptr_t *slotPtr;
	uintptr_t slotNum = 0;
	while (NULL != (slotPtr = vmThreadIterator.nextSlot())) {
		slotNum +=1;
		if (!_collector->isExclusiveAccessRequestWaitingSparseSample(env, slotNum)) {
			omrobjectptr_t objectPtr = *slotPtr;
			if (_markingScheme->isHeapObject(objectPtr) && !env->getExtensions()->heap->objectIsInGap(objectPtr)) {
				_markingScheme->markObject(env, objectPtr);
			} else if (NULL != objectPtr) {
				Assert_MM_true(vmthreaditerator_state_monitor_records == vmThreadIterator.getState());
			}
		} else {
			break;
		}
	}

	markSchemeStackIteratorData localData;
	localData.markingScheme = _markingScheme;
	localData.env = env;
	GC_VMThreadStackSlotIterator::scanSlots(vmThread, vmThread, (void *)&localData, concurrentStackSlotIterator, true, false);

	return true;
}

/**
 * Collect any JNI global references.
 * Iterate over all JNI global references; mark and push any found.
 *
 */
void
MM_ConcurrentMarkingDelegate::collectJNIRoots(MM_EnvironmentBase *env, bool *completedJNIRoots)
{
	*completedJNIRoots = false;

	Assert_GC_true_with_message(env, ((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, "MM_ConcurrentStats::_executionMode = %zu\n", _collector->getConcurrentGCStats()->getExecutionMode());
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_VMInterface::lockJNIGlobalReferences(extensions);
	GC_PoolIterator jniGlobalReferenceIterator(_javaVM->jniGlobalReferences);
	omrobjectptr_t *slotPtr;
	uintptr_t slotNum = 0;
	while((slotPtr = (omrobjectptr_t *)jniGlobalReferenceIterator.nextSlot()) != NULL) {
		slotNum += 1;
		if (_collector->isExclusiveAccessRequestWaitingSparseSample(env, slotNum)) {
			goto quitTracingJNIRefs;
		} else {
			_markingScheme->markObject(env, *slotPtr);
		}
	}

	*completedJNIRoots = true;

quitTracingJNIRefs:
	GC_VMInterface::unlockJNIGlobalReferences(extensions);
}

/**
 * Collect any roots in classes
 * Iterate over all classes and mark and push all references
 *
 */
void
MM_ConcurrentMarkingDelegate::collectClassRoots(MM_EnvironmentBase *env, bool *completedClassRoots, bool *classesMarkedAsRoots)
{
	*completedClassRoots = false;
	*classesMarkedAsRoots = false;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_GCExtensions::DynamicClassUnloading dynamicClassUnloadingFlag = (MM_GCExtensions::DynamicClassUnloading )extensions->dynamicClassUnloading;
	if (MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER != dynamicClassUnloadingFlag) {
		_scanClassesMode.setScanClassesMode(MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED);
	} else
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	{
		/* mark classes as roots, scanning classes is disabled by default */
		*classesMarkedAsRoots = true;

		Assert_GC_true_with_message(env, ((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, "MM_ConcurrentStats::_executionMode = %zu\n", _collector->getConcurrentGCStats()->getExecutionMode());

		GC_VMInterface::lockClasses(extensions);

		GC_SegmentIterator segmentIterator(_javaVM->classMemorySegments, MEMORY_TYPE_RAM_CLASS);
		J9MemorySegment *segment;
		J9Class *clazz;

		while((segment = segmentIterator.nextSegment()) != NULL) {
			GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
			while((clazz = classHeapIterator.nextClass()) != NULL) {
				if (env->isExclusiveAccessRequestWaiting()) {
					goto quitMarkClasses;
				} else {
					_markingScheme->getMarkingDelegate()->scanClass(env, clazz);
				}
			}
		}

		*completedClassRoots = true;

quitMarkClasses:
		GC_VMInterface::unlockClasses(extensions);
	}
}

/**
 * Mark all finalizable objects
 * Iterate over finalizable list and mark and push all finalizable objects
 *
 */
void
MM_ConcurrentMarkingDelegate::collectFinalizableRoots(MM_EnvironmentBase *env, bool *completedFinalizableRoots)
{
	*completedFinalizableRoots = false;

	Assert_GC_true_with_message(env, ((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, "MM_ConcurrentStats::_executionMode = %zu\n", _collector->getConcurrentGCStats()->getExecutionMode());

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_VMInterface::lockFinalizeList(extensions);

	GC_FinalizeListManager * finalizeListManager = extensions->finalizeListManager;
	{
		/* walk finalizable objects created by the system class loader */
		j9object_t systemObject = finalizeListManager->peekSystemFinalizableObject();
		while (!env->isExclusiveAccessRequestWaiting() && (NULL != systemObject)) {
			_markingScheme->markObject(env, systemObject);
			systemObject = finalizeListManager->peekNextSystemFinalizableObject(systemObject);
		}
	}

	{
		/* walk finalizable objects created by all other class loaders*/
		j9object_t defaultObject = finalizeListManager->peekDefaultFinalizableObject();
		while (!env->isExclusiveAccessRequestWaiting() && (NULL != defaultObject)) {
			_markingScheme->markObject(env, defaultObject);
			defaultObject = finalizeListManager->peekNextDefaultFinalizableObject(defaultObject);
		}
	}

	{
		/* walk reference objects */
		j9object_t referenceObject = finalizeListManager->peekReferenceObject();
		while (!env->isExclusiveAccessRequestWaiting() && (NULL != referenceObject)) {
			_markingScheme->markObject(env, referenceObject);
			referenceObject = finalizeListManager->peekNextReferenceObject(referenceObject);
		}
	}

	*completedFinalizableRoots = !env->isExclusiveAccessRequestWaiting();

	GC_VMInterface::unlockFinalizeList(extensions);
}

/**
 * Collect all roots in string table
 * Iterate over string table and mark and push all strings
 *
 */
void
MM_ConcurrentMarkingDelegate::collectStringRoots(MM_EnvironmentBase *env, bool *completedStringRoots, bool *collectedStringRoots)
{
	*completedStringRoots = false;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	*collectedStringRoots = !extensions->collectStringConstants;
	if (*collectedStringRoots) {
		/* CMVC 103513 - Do not mark strings as roots if extensions->collectStringConstants is set
		 * since this will not allow them to be collected unless the concurrent mark is aborted
		 * before we hit this point.
		 */
		MM_StringTable *stringTable = extensions->getStringTable();

		Assert_GC_true_with_message(env, ((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, "MM_ConcurrentStats::_executionMode = %zu\n", _collector->getConcurrentGCStats()->getExecutionMode());

		for (uintptr_t tableIndex = 0; tableIndex < stringTable->getTableCount(); tableIndex++) {

			stringTable->lockTable(tableIndex);

			GC_HashTableIterator stringTableIterator(stringTable->getTable(tableIndex));
			omrobjectptr_t* slotPtr;

			while((slotPtr = (omrobjectptr_t *)stringTableIterator.nextSlot()) != NULL) {
				if (env->isExclusiveAccessRequestWaiting()) {
					stringTable->unlockTable(tableIndex);
					goto quitMarkStrings;
				} else {
					_markingScheme->markObject(env, *slotPtr);
				}
			}

			stringTable->unlockTable(tableIndex);
		}

		*completedStringRoots = true;
	}

quitMarkStrings:
	return;
}

void
MM_ConcurrentMarkingDelegate::abortCollection(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(env->getExtensions()->heap->getHeapRegionManager());

	while(NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
			MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[i];
			list->resetLists();
		}
	}
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
uintptr_t
MM_ConcurrentMarkingDelegate::concurrentClassMark(MM_EnvironmentBase *env, bool *completedClassMark)
{
	J9ClassLoader *classLoader;
	uintptr_t sizeTraced = 0;
	*completedClassMark = false;

	Trc_MM_concurrentClassMarkStart(env->getLanguageVMThread());

	Assert_GC_true_with_message(env, ((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, "MM_ConcurrentStats::_executionMode = %zu\n", _collector->getConcurrentGCStats()->getExecutionMode());

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_VMInterface::lockClasses(extensions);
	GC_VMInterface::lockClassLoaders(extensions);

	MM_MarkingDelegate *markingDelegate = _markingScheme->getMarkingDelegate();
	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	while((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		/* skip dead and anonymous classloaders */
		if ((0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) && (0 == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER))) {
			/* Check if the class loader has not been scanned but the class loader is live */
			if( !(classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED) && _markingScheme->isMarkedOutline(classLoader->classLoaderObject)) {
				GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
				J9MemorySegment *segment = NULL;
				J9Class *clazz;

				while(NULL != (segment = segmentIterator.nextSegment())) {
					GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
					while(NULL != (clazz = classHeapIterator.nextClass())) {
						/* TODO CRGTMP investigate proper value here */
						sizeTraced += sizeof(J9Class);
						markingDelegate->scanClass(env, clazz);
						if (env->isExclusiveAccessRequestWaiting()) {	/* interrupt if exclusive access request is waiting */
							goto quitConcurrentClassMark;
						}
					}
				}

				/* CMVC 131487 */
				J9HashTableState walkState;
				/*
				 * We believe that (NULL == classLoader->classHashTable) is set ONLY for DEAD class loader
				 * so, if this pointer happend to be NULL at this point let it crash here
				 */
				Assert_MM_true(NULL != classLoader->classHashTable);
				clazz = _javaVM->internalVMFunctions->hashClassTableStartDo(classLoader, &walkState);
				while (NULL != clazz) {
					sizeTraced += sizeof(uintptr_t);
					_markingScheme->markObject(env, (j9object_t)clazz->classObject);
					if (env->isExclusiveAccessRequestWaiting()) {	/* interrupt if exclusive access request is waiting */
						goto quitConcurrentClassMark;
					}
					clazz = _javaVM->internalVMFunctions->hashClassTableNextDo(&walkState);
				}

				Assert_MM_true(NULL != classLoader->moduleHashTable);
				J9HashTableState moduleWalkState;
				J9Module **modulePtr = (J9Module**)hashTableStartDo(classLoader->moduleHashTable, &moduleWalkState);
				while (NULL != modulePtr) {
					J9Module * const module = *modulePtr;

					_markingScheme->markObject(env, (j9object_t)module->moduleObject);
					if (NULL != module->moduleName) {
						_markingScheme->markObject(env, (j9object_t)module->moduleName);
					}
					if (NULL != module->version) {
						_markingScheme->markObject(env, (j9object_t)module->version);
					}
					if (env->isExclusiveAccessRequestWaiting()) {	/* interrupt if exclusive access request is waiting */
						goto quitConcurrentClassMark;
					}
					modulePtr = (J9Module**)hashTableNextDo(&moduleWalkState);
				}

				classLoader->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;
			}
		}
	}

	*completedClassMark = true;

quitConcurrentClassMark:
	GC_VMInterface::unlockClassLoaders(extensions);
	GC_VMInterface::unlockClasses(extensions);

	return sizeTraced;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
