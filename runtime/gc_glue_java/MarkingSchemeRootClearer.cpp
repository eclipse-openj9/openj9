
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
#include "j9consts.h"

#if defined(J9VM_GC_FINALIZATION)
#include "CollectorLanguageInterfaceImpl.hpp"
#endif /* defined(J9VM_GC_FINALIZATION) */
#include "ConfigurationDelegate.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "HeapRegionManager.hpp"
#include "MarkingScheme.hpp"
#include "MarkingSchemeRootClearer.hpp"
#include "ModronAssertions.h"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "ReferenceStats.hpp"
#include "RootScanner.hpp"
#include "StackSlotValidator.hpp"
#include "UnfinalizedObjectBuffer.hpp"

void
MM_MarkingSchemeRootClearer::doSlot(omrobjectptr_t *slotPtr)
{
	Assert_MM_unreachable();
}

void
MM_MarkingSchemeRootClearer::doClass(J9Class *clazz)
{
	Assert_MM_unreachable();
}

void
MM_MarkingSchemeRootClearer::doFinalizableObject(omrobjectptr_t object)
{
	Assert_MM_unreachable();
}

void
MM_MarkingSchemeRootClearer::scanWeakReferenceObjects(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_WeakReferenceObjects);
	GC_Environment *gcEnv = env->getGCEnvironment();
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());

	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		/* NOTE: we can't look at the list to determine if there's work to do since another thread may have already processed it and deleted everything */
		for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[i];
				list->startWeakReferenceProcessing();
				if (!list->wasWeakListEmpty()) {
					_markingDelegate->processReferenceList(env, region, list->getPriorWeakList(), &gcEnv->_markJavaStats._weakReferenceStats);
				}
			}
		}
	}

	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
	reportScanningEnded(RootScannerEntity_WeakReferenceObjects);
}

MM_RootScanner::CompletePhaseCode
MM_MarkingSchemeRootClearer::scanWeakReferencesComplete(MM_EnvironmentBase *env)
{
	/* No new objects could have been discovered by soft / weak reference processing,
	 * but we must complete this phase prior to unfinalized processing to ensure that
	 * finalizable referents get cleared */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
	return complete_phase_OK;
}

void
MM_MarkingSchemeRootClearer::scanSoftReferenceObjects(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_SoftReferenceObjects);
	GC_Environment *gcEnv = env->getGCEnvironment();
	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());

	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		/* NOTE: we can't look at the list to determine if there's work to do since another thread may have already processed it and deleted everything */
		for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[i];
				list->startSoftReferenceProcessing();
				if (!list->wasSoftListEmpty()) {
					_markingDelegate->processReferenceList(env, region, list->getPriorSoftList(), &gcEnv->_markJavaStats._softReferenceStats);
				}
			}
		}
	}

	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
	reportScanningEnded(RootScannerEntity_SoftReferenceObjects);
}

MM_RootScanner::CompletePhaseCode
MM_MarkingSchemeRootClearer::scanSoftReferencesComplete(MM_EnvironmentBase *env)
{
	/* do nothing -- no new objects could have been discovered by soft reference processing */
	return complete_phase_OK;
}

void
MM_MarkingSchemeRootClearer::scanPhantomReferenceObjects(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_PhantomReferenceObjects);

	/* ensure that all _referenceObjectBuffers are flushed before phantom references
	 * are processed since scanning unfinalizedObjects may resurrect a phantom reference
	 */
	GC_Environment *gcEnv = env->getGCEnvironment();
	gcEnv->_referenceObjectBuffer->flush(env);
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		/* NOTE: we can't look at the list to determine if there's work to do since another thread may have already processed it and deleted everything */
		for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[i];
				list->startPhantomReferenceProcessing();
				if (!list->wasPhantomListEmpty()) {
					_markingDelegate->processReferenceList(env, region, list->getPriorPhantomList(), &gcEnv->_markJavaStats._phantomReferenceStats);
				}
			}
		}
	}

	Assert_MM_true(gcEnv->_referenceObjectBuffer->isEmpty());
	reportScanningEnded(RootScannerEntity_PhantomReferenceObjects);
}

MM_RootScanner::CompletePhaseCode
MM_MarkingSchemeRootClearer::scanPhantomReferencesComplete(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_PhantomReferenceObjectsComplete);
	if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_phantom;
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
	/* phantom reference processing may resurrect objects - scan them now */
	_markingScheme->completeMarking(env);
	reportScanningEnded(RootScannerEntity_PhantomReferenceObjectsComplete);
	return complete_phase_OK;
}

void
MM_MarkingSchemeRootClearer::scanUnfinalizedObjects(MM_EnvironmentBase *env)
{
	if (_markingDelegate->shouldScanUnfinalizedObjects()) {
		/* allow the marking scheme to handle this */
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
		GC_Environment *gcEnv = env->getGCEnvironment();
		GC_FinalizableObjectBuffer buffer(_extensions);
#if defined(J9VM_GC_FINALIZATION)
		bool finalizationRequired = false;
#endif /* defined(J9VM_GC_FINALIZATION) */

		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
		while (NULL != (region = regionIterator.nextRegion())) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
				MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
				if (!list->wasEmpty()) {
					if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
						omrobjectptr_t object = list->getPriorList();
						while (NULL != object) {
							gcEnv->_markJavaStats._unfinalizedCandidates += 1;
							omrobjectptr_t next = _extensions->accessBarrier->getFinalizeLink(object);
							if (_markingScheme->inlineMarkObject(env, object)) {
								/* object was not previously marked -- it is now finalizable so push it to the local buffer */
								buffer.add(env, object);
								gcEnv->_markJavaStats._unfinalizedEnqueued += 1;
#if defined(J9VM_GC_FINALIZATION)
								/* inform global GC if finalization is required */
								if (!finalizationRequired) {
									MM_GlobalCollector *globalCollector = (MM_GlobalCollector *)_extensions->getGlobalCollector();
									globalCollector->getGlobalCollectorDelegate()->setFinalizationRequired();
									finalizationRequired = true;
								}
#endif /* defined(J9VM_GC_FINALIZATION) */
							} else {
								/* object was already marked. It is still unfinalized */
								gcEnv->_unfinalizedObjectBuffer->add(env, object);
							}
							object = next;
						}
					}
				}
			}
		}

		/* Flush the local buffer of finalizable objects to the global list */
		buffer.flush(env);

		/* restore everything to a flushed state before exiting */
		gcEnv->_unfinalizedObjectBuffer->flush(env);

		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}
}

MM_RootScanner::CompletePhaseCode
MM_MarkingSchemeRootClearer::scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env)
{
	if (_markingDelegate->shouldScanUnfinalizedObjects()) {
		reportScanningStarted(RootScannerEntity_UnfinalizedObjectsComplete);
		/* ensure that all unfinalized processing is complete before we start marking additional objects */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		/* TODO: consider relaxing completeMarking into completeScan (which will skip over 'complete class marking' step).
		 * This is to avoid potentially unnecessary sync point in class marking step. The class marking step that we do after
		 * phantom processing might be sufficient.
		 */
		_markingScheme->completeMarking(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjectsComplete);
	}
	return complete_phase_OK;
}

void
MM_MarkingSchemeRootClearer::scanOwnableSynchronizerObjects(MM_EnvironmentBase *env)
{
	if (_markingDelegate->shouldScanOwnableSynchronizerObjects()) {
		/* allow the marking scheme to handle this */
		reportScanningStarted(RootScannerEntity_OwnableSynchronizerObjects);
		GC_Environment *gcEnv = env->getGCEnvironment();

		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
		while (NULL != (region = regionIterator.nextRegion())) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
				MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[i];
				if (!list->wasEmpty()) {
					if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
						omrobjectptr_t object = list->getPriorList();
						while (NULL != object) {
							gcEnv->_markJavaStats._ownableSynchronizerCandidates += 1;
							omrobjectptr_t next = _extensions->accessBarrier->getOwnableSynchronizerLink(object);
							if (_markingScheme->isMarked(object)) {
								/* object was already marked. */
								gcEnv->_ownableSynchronizerObjectBuffer->add(env, object);
							} else {
								/* object was not previously marked */
								gcEnv->_markJavaStats._ownableSynchronizerCleared += 1;
							}
							object = next;
						}
					}
				}
			}
#if defined(J9VM_GC_MODRON_SCAVENGER)
			/* correct scavenger statistics for ownableSynchronizerObjects, adjust survived ownableSynchronizerObject count in Nursery, only in generational gc */
			if (_extensions->scavengerEnabled && (MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				gcEnv->_scavengerJavaStats.updateOwnableSynchronizerNurseryCounts(gcEnv->_markJavaStats._ownableSynchronizerCandidates - gcEnv->_markJavaStats._ownableSynchronizerCleared);
			}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
		}

		/* restore everything to a flushed state before exiting */
		gcEnv->_ownableSynchronizerObjectBuffer->flush(env);
		reportScanningEnded(RootScannerEntity_OwnableSynchronizerObjects);
	}
}

void
MM_MarkingSchemeRootClearer::doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
{
	J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
	if(!_markingScheme->isMarked((omrobjectptr_t )monitor->userData)) {
		monitorReferenceIterator->removeSlot();
		/* We must call objectMonitorDestroy (as opposed to omrthread_monitor_destroy) when the
		 * monitor is not internal to the GC */
		static_cast<J9JavaVM*>(_omrVM->_language_vm)->internalVMFunctions->objectMonitorDestroy(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9VMThread *)_env->getLanguageVMThread(), (omrthread_monitor_t)monitor);
	}
}

MM_RootScanner::CompletePhaseCode
MM_MarkingSchemeRootClearer::scanMonitorReferencesComplete(MM_EnvironmentBase *env)
{
	J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();
	reportScanningStarted(RootScannerEntity_MonitorReferenceObjectsComplete);
	javaVM->internalVMFunctions->objectMonitorDestroyComplete(javaVM, (J9VMThread *)env->getLanguageVMThread());
	reportScanningEnded(RootScannerEntity_MonitorReferenceObjectsComplete);
	return complete_phase_OK;
}

void
MM_MarkingSchemeRootClearer::doJNIWeakGlobalReference(omrobjectptr_t *slotPtr)
{
	omrobjectptr_t objectPtr = *slotPtr;
	if ((NULL != objectPtr) && !_markingScheme->isMarked(objectPtr)) {
		*slotPtr = NULL;
	}
}

void
MM_MarkingSchemeRootClearer::doRememberedSetSlot(omrobjectptr_t *slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator)
{
	omrobjectptr_t objectPtr = *slotPtr;
	if (objectPtr == NULL) {
		rememberedSetSlotIterator->removeSlot();
	} else if (!_markingScheme->isMarked(objectPtr)) {
		_extensions->objectModel.clearRemembered(objectPtr);
		rememberedSetSlotIterator->removeSlot();
	}
}

void
MM_MarkingSchemeRootClearer::doStringTableSlot(omrobjectptr_t *slotPtr, GC_StringTableIterator *stringTableIterator)
{
	_env->getGCEnvironment()->_markJavaStats._stringConstantsCandidates += 1;
	if(!_markingScheme->isMarked(*slotPtr)) {
		_env->getGCEnvironment()->_markJavaStats._stringConstantsCleared += 1;
		stringTableIterator->removeSlot();
	}
}

/**
 * @Clear the string table cache slot if the object is not marked
 */
void
MM_MarkingSchemeRootClearer::doStringCacheTableSlot(omrobjectptr_t *slotPtr)
{
	omrobjectptr_t objectPtr = *slotPtr;
	if((NULL != objectPtr) && (!_markingScheme->isMarked(*slotPtr))) {
		*slotPtr = NULL;
	}
}

void
MM_MarkingSchemeRootClearer::doJVMTIObjectTagSlot(omrobjectptr_t *slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
{
	if(!_markingScheme->isMarked(*slotPtr)) {
		objectTagTableIterator->removeSlot();
	}
}
