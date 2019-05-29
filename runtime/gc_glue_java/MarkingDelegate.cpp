
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
#include "j9class.h"
#include "j9consts.h"
#include "j9cp.h"
#include "j9modron.h"
#include "j9nongenerated.h"
#include "j2sever.h"
#include "omrcomp.h"
#include "omrsrp.h"

#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "ClassModel.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "CollectorLanguageInterfaceImpl.hpp"
#endif /* defined(J9VM_GC_FINALIZATION) */
#include "ConfigurationDelegate.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentDelegate.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "MarkingScheme.hpp"
#include "MarkingSchemeRootMarker.hpp"
#include "MarkingSchemeRootClearer.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "RootScanner.hpp"
#include "StackSlotValidator.hpp"
#include "Task.hpp"
#include "UnfinalizedObjectList.hpp"
#include "WorkPackets.hpp"

#include "MarkingDelegate.hpp"

/* Verify that leaf bit optimization build flags are defined identically for j9 and omr */
#if defined(J9VM_GC_LEAF_BITS) != defined(OMR_GC_LEAF_BITS)
#error "Build flags J9VM_GC_LEAF_BITS and OMR_GC_LEAF_BITS must enabled/disabled identically"
#endif /* defined(J9VM_GC_LEAF_BITS) */

bool
MM_MarkingDelegate::initialize(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme)
{
	_omrVM = env->getOmrVM();
	_extensions = MM_GCExtensions::getExtensions(env);
	_markingScheme = markingScheme;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_markMap = (_extensions->dynamicClassUnloading != MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER) ? markingScheme->getMarkMap() : NULL;
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	return true;
}



#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_MarkingDelegate::clearClassLoadersScannedFlag(MM_EnvironmentBase *env)
{
	J9JavaVM *javaVM = (J9JavaVM*)env->getLanguageVM();

	/**
	 *	ClassLoaders might be scanned already at concurrent stage.
	 *  Clear the "scanned" flags of all classLoaders to scan them again
	 **/
	GC_ClassLoaderIterator classLoaderIterator(javaVM->classLoaderBlocks);
	J9ClassLoader *classLoader;
	while ((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		classLoader->gcFlags &= ~J9_GC_CLASS_LOADER_SCANNED;
	}

	/*
	 * Clear "scanned" flag for all classes in anonymous classloader
	 */
	classLoader = javaVM->anonClassLoader;
	if (NULL != classLoader) {
		GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
		J9MemorySegment *segment = NULL;
		while (NULL != (segment = segmentIterator.nextSegment())) {
			GC_ClassHeapIterator classHeapIterator(javaVM, segment);
			J9Class *clazz = NULL;
			while (NULL != (clazz = classHeapIterator.nextClass())) {
				J9CLASS_EXTENDED_FLAGS_CLEAR(clazz, J9ClassGCScanned);
			}
		}
	}
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

void
MM_MarkingDelegate::masterSetupForWalk(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_markMap = NULL;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	/* treat all interned strings as roots for the purposes of a heap walk */
	_collectStringConstantsEnabled = false;
}

void
MM_MarkingDelegate::workerSetupForGC(MM_EnvironmentBase *env)
{
	GC_Environment *gcEnv = env->getGCEnvironment();
	gcEnv->_markJavaStats.clear();
#if defined(J9VM_GC_MODRON_SCAVENGER)
	if (_extensions->scavengerEnabled) {
		/* clear scavenger stats for correcting the ownableSynchronizerObjects stats, only in generational gc */
		gcEnv->_scavengerJavaStats.clearOwnableSynchronizerCounts();
	}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
		/* record that this thread is participating in this cycle */
		env->_markStats._gcCount = env->_workPacketStats._gcCount = _extensions->globalGCStats.gcCount;
#endif /* defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME) */
}

void
MM_MarkingDelegate::workerCompleteGC(MM_EnvironmentBase *env)
{
	/* ensure that all buffers have been flushed before we start reference processing */
	GC_Environment *gcEnv = env->getGCEnvironment();
	gcEnv->_referenceObjectBuffer->flush(env);

	if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_soft;
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_weak;
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
	MM_MarkingSchemeRootClearer rootClearer(env, _markingScheme, this);
	rootClearer.setStringTableAsRoot(!_collectStringConstantsEnabled);
	rootClearer.scanClearable(env);
}

void
MM_MarkingDelegate::workerCleanupAfterGC(MM_EnvironmentBase *env)
{
	GC_Environment *gcEnv = env->getGCEnvironment();
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());

	_extensions->markJavaStats.merge(&gcEnv->_markJavaStats);
#if defined(J9VM_GC_MODRON_SCAVENGER)
	if (_extensions->scavengerEnabled) {
		/* merge scavenger ownableSynchronizerObjects stats, only in generational gc */
		_extensions->scavengerJavaStats.mergeOwnableSynchronizerCounts(&gcEnv->_scavengerJavaStats);
	}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
}

void
MM_MarkingDelegate::masterSetupForGC(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	clearClassLoadersScannedFlag(env);
	_markMap = (0 != _extensions->runtimeCheckDynamicClassUnloading) ? _markingScheme->getMarkMap() : NULL;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	_collectStringConstantsEnabled = _extensions->collectStringConstants;
}

void
MM_MarkingDelegate::masterCleanupAfterGC(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_markMap = (_extensions->dynamicClassUnloading != MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER) ? _markingScheme->getMarkMap() : NULL;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
}

void
MM_MarkingDelegate::scanRoots(MM_EnvironmentBase *env)
{
	/* Start unfinalized object and ownable synchronizer processing */
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		_shouldScanUnfinalizedObjects = false;
		_shouldScanOwnableSynchronizerObjects = false;
		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
		while (NULL != (region = regionIterator.nextRegion())) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
				/* Start unfinalized object processing for region */
				MM_UnfinalizedObjectList *unfinalizedObjectList = &(regionExtension->_unfinalizedObjectLists[i]);
				unfinalizedObjectList->startUnfinalizedProcessing();
				if (!unfinalizedObjectList->wasEmpty()) {
					_shouldScanUnfinalizedObjects = true;
				}
				/* Start ownable synchronizer processing for region */
				MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = &(regionExtension->_ownableSynchronizerObjectLists[i]);
				ownableSynchronizerObjectList->startOwnableSynchronizerProcessing();
				if (!ownableSynchronizerObjectList->wasEmpty()) {
					_shouldScanOwnableSynchronizerObjects = true;
				}
			}
		}
	}

	/* Reset MM_RootScanner base class for scanning */
	MM_MarkingSchemeRootMarker rootMarker(env, _markingScheme, this);
	rootMarker.setStringTableAsRoot(!_collectStringConstantsEnabled);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/* Mark root set classes */
	rootMarker.setClassDataAsRoots(!isDynamicClassUnloadingEnabled());
	if (isDynamicClassUnloadingEnabled()) {
		/* Setting the permanent class loaders to scanned without a locked operation is safe
		 * Class loaders will not be rescanned until a thread synchronize is executed
		 */
		if (env->isMasterThread()) {
			J9JavaVM * javaVM = (J9JavaVM*)env->getLanguageVM();
			((J9ClassLoader *)javaVM->systemClassLoader)->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;
			_markingScheme->markObject(env, (omrobjectptr_t )((J9ClassLoader *)javaVM->systemClassLoader)->classLoaderObject);
			if (javaVM->applicationClassLoader) {
				((J9ClassLoader *)javaVM->applicationClassLoader)->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;
				_markingScheme->markObject(env, (omrobjectptr_t )((J9ClassLoader *)javaVM->applicationClassLoader)->classLoaderObject);
			}
		}
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	/* Scan roots */
	rootMarker.scanRoots(env);
}

void
MM_MarkingDelegate::completeMarking(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (isDynamicClassUnloadingEnabled()) {
		J9ClassLoader *classLoader;
		J9JavaVM * javaVM = (J9JavaVM*)env->getLanguageVM();

		if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
			_anotherClassMarkPass = false;
			_anotherClassMarkLoopIteration = true;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}

		while (_anotherClassMarkLoopIteration) {
			GC_ClassLoaderIterator classLoaderIterator(javaVM->classLoaderBlocks);
			while ((classLoader = classLoaderIterator.nextSlot()) != NULL) {
				/* We cannot go more granular (for example per class segment) since the class loader flag is changed */
				/* Several threads might contend setting the value of the flags */
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
						if (J9CLASSLOADER_ANON_CLASS_LOADER == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER)) {
							/* Anonymous classloader should be scanned on level of classes every time */
							GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
							J9MemorySegment *segment = NULL;
							while (NULL != (segment = segmentIterator.nextSegment())) {
								GC_ClassHeapIterator classHeapIterator(javaVM, segment);
								J9Class *clazz = NULL;
								while (NULL != (clazz = classHeapIterator.nextClass())) {
									Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));
									if ((0 == (J9CLASS_EXTENDED_FLAGS(clazz) & J9ClassGCScanned)) && _markingScheme->isMarked(clazz->classObject)) {
										J9CLASS_EXTENDED_FLAGS_SET(clazz, J9ClassGCScanned);

										scanClass(env, clazz);
										/* This may result in other class loaders being marked,
										 * so we have to do another pass
										 */
										_anotherClassMarkPass = true;
									}
								}
							}
						} else {
							/* Check if the class loader has not been scanned but the class loader is live */
							if ((0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED)) && _markingScheme->isMarked(classLoader->classLoaderObject)) {
								/* Flag the class loader as being scanned */
								classLoader->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;

								GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
								J9MemorySegment *segment = NULL;
								J9Class *clazz = NULL;
								while (NULL != (segment = segmentIterator.nextSegment())) {
									GC_ClassHeapIterator classHeapIterator(javaVM, segment);
									while (NULL != (clazz = classHeapIterator.nextClass())) {
										scanClass(env, clazz);
										/* This may result in other class loaders being marked,
										 * so we have to do another pass
										 */
										_anotherClassMarkPass = true;
									}
								}

								/* CMVC 131487 */
								J9HashTableState walkState;
								/*
								 * We believe that (NULL == classLoader->classHashTable) is set ONLY for DEAD class loader
								 * so, if this pointer happened to be NULL at this point let it crash here
								 */
								Assert_MM_true(NULL != classLoader->classHashTable);
								clazz = javaVM->internalVMFunctions->hashClassTableStartDo(classLoader, &walkState);
								while (NULL != clazz) {
									_markingScheme->markObjectNoCheck(env, (omrobjectptr_t )clazz->classObject);
									_anotherClassMarkPass = true;
									clazz = javaVM->internalVMFunctions->hashClassTableNextDo(&walkState);
								}

								if (NULL != classLoader->moduleHashTable) {
									J9HashTableState moduleWalkState;
									J9Module **modulePtr = (J9Module**)hashTableStartDo(classLoader->moduleHashTable, &moduleWalkState);
									while (NULL != modulePtr) {
										J9Module * const module = *modulePtr;

										_markingScheme->markObjectNoCheck(env, (omrobjectptr_t )module->moduleObject);
										if (NULL != module->moduleName) {
											_markingScheme->markObjectNoCheck(env, (omrobjectptr_t )module->moduleName);
										}
										if (NULL != module->version) {
											_markingScheme->markObjectNoCheck(env, (omrobjectptr_t )module->version);
										}
										modulePtr = (J9Module**)hashTableNextDo(&moduleWalkState);
									}
								}
							}
						}
					}
				}
			}

			/* In case some GC threads don't find a classLoader to work with (or are quick to finish with it)
			 * let them help empty out the generic work stack.
			 */
			_markingScheme->completeScan(env);

			/* have to stop the threads while resetting the flag, to prevent them rushing through another pass and
			 * losing an early "set flag"
			 */
			if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
				/* if work is complete, end loop */
				_anotherClassMarkLoopIteration = _anotherClassMarkPass;
				_anotherClassMarkPass = false;
				env->_currentTask->releaseSynchronizedGCThreads(env);
			}
		}
	}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
}

void
MM_MarkingDelegate::scanClass(MM_EnvironmentBase *env, J9Class *clazz)
{
	/* Note: Class loader objects are handled separately */
	/*
	 * Scan and mark using GC_ClassIterator:
	 *  - class object
	 *  - class constant pool
	 *  - class statics
	 *  - class method types
	 *  - class call sites
	 *  - class varhandle method types
	 *
	 * As this function can be invoked during concurrent mark the slot is
	 * volatile so we must ensure that the compiler generates the correct
	 * code if markObject() is inlined.
	 */
	GC_ClassIterator classIterator(env, clazz, true);
	while (volatile omrobjectptr_t *slotPtr = classIterator.nextSlot()) {
		_markingScheme->markObject(env, *slotPtr);
	}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (isDynamicClassUnloadingEnabled()) {
		UDATA classDepth = GC_ClassModel::getClassDepth(clazz);
		/* Superclasses - if the depth is 0, don't bother - no superclasses */
		if (0 < classDepth) {
			J9Class** superclassScanPtr = clazz->superclasses;
			J9Class** superclassEndScanPtr = superclassScanPtr + classDepth;
			while (superclassScanPtr < superclassEndScanPtr) {
				J9Class *clazzPtr = *superclassScanPtr++;
				_markingScheme->markObject(env, clazzPtr->classObject);
			}
		}

		if (NULL != clazz->arrayClass) {
			J9Class *clazzPtr = clazz->arrayClass;
			_markingScheme->markObject(env, clazzPtr->classObject);
		}

		/* Component type and Leaf Component type for indexable */
		if (_extensions->objectModel.isIndexable(clazz)) {
			J9ArrayClass *indexableClass = (J9ArrayClass *)clazz;
			J9Class *clazzPtr = indexableClass->componentType;
			_markingScheme->markObject(env, clazzPtr->classObject);

			/*
			 * There is no mandatory need to scan leafComponentType here because this class
			 * would be discovered at the end componentType chain eventually
			 * However it is a performance optimization because an early class discovery
			 * might reduce number of loops to scan
			 */
			clazzPtr = indexableClass->leafComponentType;
			_markingScheme->markObject(env, clazzPtr->classObject);
		}

		/* ITable */
		J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();
		if (!GC_ClassModel::usesSharedITable(javaVM, clazz)) {
			J9ITable *clazzITable = (J9ITable *)clazz->iTable;
			J9ITable *endITable = (0 != classDepth) ? (J9ITable *)(clazz->superclasses[classDepth - 1]->iTable) : NULL;
			while (clazzITable != endITable) {
				_markingScheme->markObject(env, clazzITable->interfaceClass->classObject);
				clazzITable = clazzITable->next;
			}
		}
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
}

void
MM_MarkingDelegate::processReferenceList(MM_EnvironmentBase *env, MM_HeapRegionDescriptorStandard* region, omrobjectptr_t headOfList, MM_ReferenceStats *referenceStats)
{
	/* no list can possibly contain more reference objects than there are bytes in a region. */
	const UDATA maxObjects = region->getSize();
	UDATA objectsVisited = 0;
	GC_FinalizableReferenceBuffer buffer(_extensions);
#if defined(J9VM_GC_FINALIZATION)
	bool finalizationRequired = false;
#endif /* defined(J9VM_GC_FINALIZATION) */

	omrobjectptr_t referenceObj = headOfList;
	while (NULL != referenceObj) {
		objectsVisited += 1;
		referenceStats->_candidates += 1;

		Assert_MM_true(_markingScheme->isMarked(referenceObj));
		Assert_MM_true(objectsVisited < maxObjects);

		omrobjectptr_t nextReferenceObj = _extensions->accessBarrier->getReferenceLink(referenceObj);

		GC_SlotObject referentSlotObject(_extensions->getOmrVM(), &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, referenceObj));

		if (NULL != referentSlotObject.readReferenceFromSlot()) {
			_markingScheme->fixupForwardedSlot(&referentSlotObject);
			omrobjectptr_t referent = referentSlotObject.readReferenceFromSlot();

			UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(referenceObj)) & J9AccClassReferenceMask;
			if (_markingScheme->isMarked(referent)) {
				if (J9AccClassReferenceSoft == referenceObjectType) {
					U_32 age = J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, referenceObj);
					if (age < _extensions->getMaxSoftReferenceAge()) {
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
				J9JavaVM * javaVM = (J9JavaVM*)env->getLanguageVM();
				if ((J9AccClassReferencePhantom == referenceObjectType) && ((J2SE_VERSION(javaVM) & J2SE_VERSION_MASK) <= J2SE_18)) {
					/* Phantom objects keep their referent - scanning will be done after the enqueuing */
					_markingScheme->inlineMarkObject(env, referent);
				} else {
					referentSlotObject.writeReferenceToSlot(NULL);
				}

				/* Check if the reference has a queue */
				if (0 != J9GC_J9VMJAVALANGREFERENCE_QUEUE(env, referenceObj)) {
					/* Reference object can be enqueued onto the finalizable list */
					buffer.add(env, referenceObj);
					referenceStats->_enqueued += 1;
#if defined(J9VM_GC_FINALIZATION)
					/* inform global GC if finalization is required */
					if (!finalizationRequired) {
						MM_GlobalCollector *globalCollector = (MM_GlobalCollector *)_extensions->getGlobalCollector();
						globalCollector->getGlobalCollectorDelegate()->setFinalizationRequired();
						finalizationRequired = true;
					}
#endif /* defined(J9VM_GC_FINALIZATION) */
				}
			}
		}

		referenceObj = nextReferenceObj;
	}

	buffer.flush(env);
}

bool
MM_MarkingDelegate::processReference(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	bool isReferenceCleared = false;
	bool referentMustBeMarked = false;
	bool referentMustBeCleared = getReferenceStatus(env, objectPtr, &referentMustBeMarked, &isReferenceCleared);

	clearReference(env, objectPtr, isReferenceCleared, referentMustBeCleared);

	return referentMustBeMarked;
}

bool
MM_MarkingDelegate::getReferenceStatus(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool *referentMustBeMarked, bool *isReferenceCleared)
{
	/*
	 * the method getReferenceStatus() is shared between STW gc and concurrent gc,
	 * during concurrent gc, the cycleState of the mutator thread might not be set,
	 * but if the cycleState of the thread is not set, we know it is in concurrent mode(not clearable phase).
	 */
	UDATA referenceObjectOptions = MM_CycleState::references_default;
	if (NULL != env->_cycleState) {
		referenceObjectOptions = env->_cycleState->_referenceObjectOptions;
	}

	I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr);
	*isReferenceCleared = (GC_ObjectModel::REF_STATE_CLEARED == referenceState) || (GC_ObjectModel::REF_STATE_ENQUEUED == referenceState);
	*referentMustBeMarked = *isReferenceCleared;
	bool referentMustBeCleared = false;

	UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(objectPtr)) & J9AccClassReferenceMask;
	switch (referenceObjectType) {
	case J9AccClassReferenceWeak:
		referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_weak));
		break;
	case J9AccClassReferenceSoft:
		referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_soft));
		*referentMustBeMarked = *referentMustBeMarked || (
			((0 == (referenceObjectOptions & MM_CycleState::references_soft_as_weak))
			&& ((UDATA)J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, objectPtr) < _extensions->getDynamicMaxSoftReferenceAge())));
		break;
	case J9AccClassReferencePhantom:
		referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_phantom));
		break;
	default:
		Assert_MM_unreachable();
	}

	return referentMustBeCleared;
}

void
MM_MarkingDelegate::clearReference(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool isReferenceCleared, bool referentMustBeCleared)
{
	if (referentMustBeCleared) {
		/* Discovering this object at this stage in the GC indicates that it is being resurrected. Clear its referent slot. */
		GC_SlotObject referentPtr(_omrVM, &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr));
		referentPtr.writeReferenceToSlot(NULL);
		/* record that the reference has been cleared if it's not already in the cleared or enqueued state */
		if (!isReferenceCleared) {
			J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr) = GC_ObjectModel::REF_STATE_CLEARED;
		}
	} else {
		/* we don't need to process cleared or enqueued references. */
		if (!isReferenceCleared) {
			/* for overflow case we assume only 3 active reference states(REF_STATE_INITIAL, REF_STATE_CLEARED, REF_STATE_ENQUEUED),
			   REF_STATE_REMEMBERED is only for balanced case, ReferenceCleared(REF_STATE_CLEARED, REF_STATE_ENQUEUED) notReferenceCleared(REF_STATE_INITIAL),
			   if any new states is added, should reconsider the logic */
			env->getGCEnvironment()->_referenceObjectBuffer->add(env, objectPtr);
		}
	}
}

fomrobject_t *
MM_MarkingDelegate::setupReferenceObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason)
{
	bool isReferenceCleared = false;
	bool referentMustBeMarked = false;
	bool referentMustBeCleared = getReferenceStatus(env, objectPtr, &referentMustBeMarked, &isReferenceCleared);

	GC_SlotObject referentSlotObject(_omrVM, &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr));
	if (SCAN_REASON_PACKET == reason) {
		clearReference(env, objectPtr, isReferenceCleared, referentMustBeCleared);
	}

	fomrobject_t* referentSlotPtr = NULL;
	if (!referentMustBeMarked) {
		referentSlotPtr = referentSlotObject.readAddressFromSlot();
	}
	return referentSlotPtr;
}

uintptr_t
MM_MarkingDelegate::setupPointerArrayScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason, uintptr_t *sizeToDo, uintptr_t *slotsToDo)
{
	uintptr_t startIndex = 0;
	uintptr_t headerBytesToScan = 0;
	uintptr_t workItem = (uintptr_t)env->_workStack.peek(env);
	if (PACKET_ARRAY_SPLIT_TAG == (workItem & PACKET_ARRAY_SPLIT_TAG)) {
		Assert_MM_true(SCAN_REASON_PACKET == reason);
		env->_workStack.pop(env);
		/* since we are putting extra tagged objects on the work stack we are responsible for ensuring that the object scanned
		 * count is correct.  The MM_MarkingScheme::scanObject code will increment _objectScanned by 1 for EVERY object
		 * popped off of the work stack if the scan reason is SCAN_REASON_PACKET. This code is only executed during regular
		 * packet scanning.
		 */
		env->_markStats._objectsScanned -= 1;
		/* only mark the class the first time we scan any array */
		startIndex = workItem >> PACKET_ARRAY_SPLIT_SHIFT;
	} else {
		/* account for header size on first scan */
		headerBytesToScan = _extensions->indexableObjectModel.getHeaderSize((J9IndexableObject *)objectPtr);
	}

	uintptr_t slotsToScan = 0;
	uintptr_t maxSlotsToScan = OMR_MAX(*sizeToDo / sizeof(fomrobject_t), 1);
	Assert_MM_true(maxSlotsToScan > 0);
	uintptr_t sizeInElements = _extensions->indexableObjectModel.getSizeInElements((J9IndexableObject *)objectPtr);
	if (sizeInElements > 0) {
		Assert_MM_true(startIndex < sizeInElements);
		slotsToScan = sizeInElements - startIndex;

		/* pointer arrays are split into segments to improve parallelism. split amount is proportional to array size
		 * and inverse proportional to active thread count.
		 * additionally, the less busy we are, the smaller the split amount, while obeying specified minimum and maximum.
		 */

		uintptr_t arraySplitSize = slotsToScan / (_extensions->dispatcher->activeThreadCount() + 2 * _markingScheme->getWorkPackets()->getThreadWaitCount());
		arraySplitSize = OMR_MAX(arraySplitSize, _extensions->markingArraySplitMinimumAmount);
		arraySplitSize = OMR_MIN(arraySplitSize, _extensions->markingArraySplitMaximumAmount);

		if ((slotsToScan > arraySplitSize) || (slotsToScan > maxSlotsToScan)) {
			slotsToScan = OMR_MIN(arraySplitSize, maxSlotsToScan);

			/* immediately make the next chunk available for another thread to start processing */
			uintptr_t nextIndex = startIndex + slotsToScan;
			Assert_MM_true(nextIndex < sizeInElements);
			void *element1 = (void *)objectPtr;
			void *element2 = (void *)((nextIndex << PACKET_ARRAY_SPLIT_SHIFT) | PACKET_ARRAY_SPLIT_TAG);
			Assert_MM_true(nextIndex == (((uintptr_t)element2) >> PACKET_ARRAY_SPLIT_SHIFT));
			env->_workStack.push(env, element1, element2);
			env->_workStack.flushOutputPacket(env);
			MM_MarkJavaStats *markJavaStats = &(env->getGCEnvironment()->_markJavaStats);
			markJavaStats->splitArraysProcessed += 1;
			markJavaStats->splitArraysAmount += slotsToScan;
		}
	}

	*sizeToDo = headerBytesToScan + (slotsToScan * sizeof(fomrobject_t));
	*slotsToDo = slotsToScan;
	return startIndex;
}
