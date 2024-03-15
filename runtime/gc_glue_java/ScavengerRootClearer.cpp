/*******************************************************************************
 * Copyright IBM Corp. and others 2015
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9cfg.h"
#include "j2sever.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationDelegate.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ParallelDispatcher.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "ReferenceObjectList.hpp"
#include "ReferenceStats.hpp"
#include "SlotObject.hpp"
#include "UnfinalizedObjectBuffer.hpp"
#include "UnfinalizedObjectList.hpp"
#include "ContinuationObjectBufferStandard.hpp"
#include "ContinuationObjectList.hpp"
#if JAVA_SPEC_VERSION >= 19
#include "ContinuationHelpers.hpp"
#endif /* JAVA_SPEC_VERSION >= 19 */

#include "ScavengerRootClearer.hpp"

void
MM_ScavengerRootClearer::processReferenceList(MM_EnvironmentStandard *env, MM_HeapRegionDescriptorStandard *region, omrobjectptr_t headOfList, MM_ReferenceStats *referenceStats)
{
	/* no list can possibly contain more reference objects than there are bytes in a region. */
	const uintptr_t maxObjects = region->getSize();
	uintptr_t objectsVisited = 0;
	GC_FinalizableReferenceBuffer buffer(_extensions);
	bool const compressed = _extensions->compressObjectReferences();

	omrobjectptr_t referenceObj = headOfList;
	while (NULL != referenceObj) {
		objectsVisited += 1;
		referenceStats->_candidates += 1;

		Assert_MM_true(objectsVisited < maxObjects);
		Assert_GC_true_with_message(env, _scavenger->isObjectInNewSpace(referenceObj), "Scavenged reference object not in new space: %p\n", referenceObj);

		omrobjectptr_t nextReferenceObj = _extensions->accessBarrier->getReferenceLink(referenceObj);
		GC_SlotObject referentSlotObject(_extensions->getOmrVM(), J9GC_J9VMJAVALANGREFERENCE_REFERENT_ADDRESS(env, referenceObj));
		omrobjectptr_t referent = referentSlotObject.readReferenceFromSlot();
		if (NULL != referent) {
			/* update the referent if it's been forwarded */
			MM_ForwardedHeader forwardedReferent(referent, compressed);
			if (forwardedReferent.isForwardedPointer()) {
				referent = forwardedReferent.getForwardedObject();
				referentSlotObject.writeReferenceToSlot(referent);
			}

			if (_scavenger->isObjectInEvacuateMemory(referent)) {
				/* transition the state to cleared */
				Assert_MM_true(GC_ObjectModel::REF_STATE_INITIAL == J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj));
				J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj) = GC_ObjectModel::REF_STATE_CLEARED;

				referenceStats->_cleared += 1;
				referentSlotObject.writeReferenceToSlot(NULL);

				/* Check if the reference has a queue */
				if (0 != J9GC_J9VMJAVALANGREFERENCE_QUEUE(env, referenceObj)) {
					/* Reference object can be enqueued onto the finalizable list */
					buffer.add(env, referenceObj);
					referenceStats->_enqueued += 1;
					_scavenger->getDelegate()->setFinalizationRequired(true);
				}
			}
		}

		referenceObj = nextReferenceObj;
	}
	buffer.flush(env);
}

void
MM_ScavengerRootClearer::scavengeReferenceObjects(MM_EnvironmentStandard *env, uintptr_t referenceObjectType)
{
	Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());

	/* Disable dynamicBreadthFirstScanOrdering depth copying before scavenging reference objects to avoid immediate copying of hot children of reference objects */
	env->disableHotFieldDepthCopy();

	MM_ScavengerJavaStats *javaStats = &env->getGCEnvironment()->_scavengerJavaStats;
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heapRegionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW)) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
				/* NOTE: we can't look at the list to determine if there's work to do since another thread may have already processed it and deleted everything */
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[i];
					MM_ReferenceStats *stats = NULL;
					j9object_t head = NULL;
					switch (referenceObjectType) {
						case J9AccClassReferenceWeak:
						list->startWeakReferenceProcessing();
						if (!list->wasWeakListEmpty()) {
							head = list->getPriorWeakList();
							stats = &javaStats->_weakReferenceStats;
						}
						break;
						case J9AccClassReferenceSoft:
						list->startSoftReferenceProcessing();
						if (!list->wasSoftListEmpty()) {
							head = list->getPriorSoftList();
							stats = &javaStats->_softReferenceStats;
						}
						break;
						case J9AccClassReferencePhantom:
						list->startPhantomReferenceProcessing();
						if (!list->wasPhantomListEmpty()) {
							head = list->getPriorPhantomList();
							stats = &javaStats->_phantomReferenceStats;
						}
						break;
						default:
						Assert_MM_unreachable();
						break;
					}
					if (NULL != head) {
						processReferenceList(env, region, head, stats);
					}
				}
			}
		}
	}
	/* Re-enable dynamicBreadthFirstScanOrdering depth copying after scavenging reference objects */
	env->enableHotFieldDepthCopy();
	
	Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());
}

#if defined(J9VM_GC_FINALIZATION)
void
MM_ScavengerRootClearer::scavengeUnfinalizedObjects(MM_EnvironmentStandard *env)
{
	/* Disable dynamicBreadthFirstScanOrdering depth copying before scavenging finalizable objects to avoid immediate copying of hot children of finalizable objects */
	env->disableHotFieldDepthCopy();

	GC_FinalizableObjectBuffer buffer(_extensions);
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heapRegionManager);
	GC_Environment *gcEnv = env->getGCEnvironment();
	bool const compressed = _extensions->compressObjectReferences();
	while (NULL != (region = regionIterator.nextRegion())) {
		if (MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW)) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
				MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
				if (!list->wasEmpty()) {
					if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
						omrobjectptr_t object = list->getPriorList();
						while (NULL != object) {
							omrobjectptr_t next = NULL;
							gcEnv->_scavengerJavaStats._unfinalizedCandidates += 1;

							MM_ForwardedHeader forwardedHeader(object, compressed);
							if (!forwardedHeader.isForwardedPointer()) {
								Assert_MM_true(_scavenger->isObjectInEvacuateMemory(object));
								next = _extensions->accessBarrier->getFinalizeLink(object);
								omrobjectptr_t finalizableObject = _scavenger->copyObject(env, &forwardedHeader);
								if (NULL == finalizableObject) {
									/* Failure - the scavenger must back out the work it has done. */
									gcEnv->_unfinalizedObjectBuffer->add(env, object);
								} else {
									/* object was not previously forwarded -- it is now finalizable so push it to the local buffer */
									buffer.add(env, finalizableObject);
									gcEnv->_scavengerJavaStats._unfinalizedEnqueued += 1;
									_scavenger->getDelegate()->setFinalizationRequired(true);
								}
							} else {
								omrobjectptr_t forwardedPtr =  forwardedHeader.getForwardedObject();
								Assert_MM_true(NULL != forwardedPtr);
								next = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);
								gcEnv->_unfinalizedObjectBuffer->add(env, forwardedPtr);
							}

							object = next;
						}
					}
				}
			}
		}
	}
	/* Flush the local buffer of finalizable objects to the global list */
	buffer.flush(env);

	/* restore everything to a flushed state before exiting */
	gcEnv->_unfinalizedObjectBuffer->flush(env);

	/* Re-enable dynamicBreadthFirstScanOrdering depth copying after scavenging finalizable objects */
	env->enableHotFieldDepthCopy();
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_ScavengerRootClearer::scavengeContinuationObjects(MM_EnvironmentStandard *env)
{
#if JAVA_SPEC_VERSION >= 19
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heapRegionManager);
	GC_Environment *gcEnv = env->getGCEnvironment();
	bool const compressed = _extensions->compressObjectReferences();
	while (NULL != (region = regionIterator.nextRegion())) {
		if (MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW)) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
				MM_ContinuationObjectList *list = &regionExtension->_continuationObjectLists[i];
				if (!list->wasEmpty()) {
					if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
						omrobjectptr_t object = list->getPriorList();
						while (NULL != object) {
							omrobjectptr_t next = _extensions->accessBarrier->getContinuationLink(object);
							gcEnv->_scavengerJavaStats._continuationCandidates += 1;

							MM_ForwardedHeader forwardedHeader(object, compressed);
							omrobjectptr_t forwardedPtr = object;
							if (forwardedHeader.isForwardedPointer()) {
								forwardedPtr = forwardedHeader.getForwardedObject();
								Assert_GC_true_with_message(env, NULL != forwardedPtr, "Continuation object  %p should be forwarded\n", object);
							}
							if (!forwardedHeader.isForwardedPointer() || VM_ContinuationHelpers::isFinished(*VM_ContinuationHelpers::getContinuationStateAddress((J9VMThread *)env->getLanguageVMThread() , forwardedPtr))) {
								gcEnv->_scavengerJavaStats._continuationCleared += 1;
								_extensions->releaseNativesForContinuationObject(env, forwardedPtr);
							} else {
								gcEnv->_continuationObjectBuffer->add(env, forwardedPtr);
							}
							object = next;
						}
					}
				}
			}
		}
	}

	/* restore everything to a flushed state before exiting */
	gcEnv->_continuationObjectBuffer->flush(env);
#endif /* JAVA_SPEC_VERSION >= 19 */
}

void
MM_ScavengerRootClearer::iterateAllContinuationObjects(MM_EnvironmentBase *env)
{
	if (_scavenger->getDelegate()->getShouldIterateContinuationObjects()) {
		reportScanningStarted(RootScannerEntity_ContinuationObjectsComplete);
		MM_ContinuationObjectBufferStandard::iterateAllContinuationObjects(env);
		reportScanningEnded(RootScannerEntity_ContinuationObjectsComplete);
	}
}

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */



