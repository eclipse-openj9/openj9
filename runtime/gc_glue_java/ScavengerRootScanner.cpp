/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "j9cfg.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationDelegate.hpp"
#include "Dispatcher.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizeListManager.hpp"
#include "ForwardedHeader.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "ReferenceObjectList.hpp"
#include "ReferenceStats.hpp"
#include "SlotObject.hpp"
#include "UnfinalizedObjectBuffer.hpp"
#include "UnfinalizedObjectList.hpp"

#include "ScavengerRootScanner.hpp"

#if defined(J9VM_GC_FINALIZATION)
void
MM_ScavengerRootScanner::startUnfinalizedProcessing(MM_EnvironmentBase *env)
{
	if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		_scavengerDelegate->setShouldScavengeUnfinalizedObjects(false);

		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(env->getExtensions()->getHeap()->getHeapRegionManager());
		while(NULL != (region = regionIterator.nextRegion())) {
			if ((MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
				for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
					MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
					list->startUnfinalizedProcessing();
					if (!list->wasEmpty()) {
						_scavengerDelegate->setShouldScavengeUnfinalizedObjects(true);
					}
				}
			}
		}
	}
}

void
MM_ScavengerRootScanner::scavengeFinalizableObjects(MM_EnvironmentStandard *env)
{
	GC_FinalizeListManager * const finalizeListManager = _extensions->finalizeListManager;

	/* this code must be run single-threaded and we should only be here if work is actually required */
	Assert_MM_true(env->_currentTask->isSynchronized());
	Assert_MM_true(_scavengerDelegate->getShouldScavengeFinalizableObjects());
	Assert_MM_true(finalizeListManager->isFinalizableObjectProcessingRequired());

	{
		GC_FinalizableObjectBuffer objectBuffer(_extensions);
		/* walk finalizable objects loaded by the system class loader */
		omrobjectptr_t systemObject = finalizeListManager->resetSystemFinalizableObjects();
		while (NULL != systemObject) {
			omrobjectptr_t next = NULL;
			if(_scavenger->isObjectInEvacuateMemory(systemObject)) {
				MM_ForwardedHeader forwardedHeader(systemObject);
				if (!forwardedHeader.isForwardedPointer()) {
					next = _extensions->accessBarrier->getFinalizeLink(systemObject);
					omrobjectptr_t copiedObject = _scavenger->copyObject(env, &forwardedHeader);
					if (NULL == copiedObject) {
						objectBuffer.add(env, systemObject);
					} else {
						objectBuffer.add(env, copiedObject);
					}
				} else {
					omrobjectptr_t forwardedPtr =  forwardedHeader.getNonStrictForwardedObject();
					Assert_MM_true(NULL != forwardedPtr);
					next = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);
					objectBuffer.add(env, forwardedPtr);
				}
			} else {
				next = _extensions->accessBarrier->getFinalizeLink(systemObject);
				objectBuffer.add(env, systemObject);
			}

			systemObject = next;
		}
		objectBuffer.flush(env);
	}

	{
		GC_FinalizableObjectBuffer objectBuffer(_extensions);
		/* walk finalizable objects loaded by the all other class loaders */
		omrobjectptr_t defaultObject = finalizeListManager->resetDefaultFinalizableObjects();
		while (NULL != defaultObject) {
			omrobjectptr_t next = NULL;
			if(_scavenger->isObjectInEvacuateMemory(defaultObject)) {
				MM_ForwardedHeader forwardedHeader(defaultObject);
				if (!forwardedHeader.isForwardedPointer()) {
					next = _extensions->accessBarrier->getFinalizeLink(defaultObject);
					omrobjectptr_t copiedObject = _scavenger->copyObject(env, &forwardedHeader);
					if (NULL == copiedObject) {
						objectBuffer.add(env, defaultObject);
					} else {
						objectBuffer.add(env, copiedObject);
					}
				} else {
					omrobjectptr_t forwardedPtr = forwardedHeader.getNonStrictForwardedObject();
					Assert_MM_true(NULL != forwardedPtr);
					next = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);
					objectBuffer.add(env, forwardedPtr);
				}
			} else {
				next = _extensions->accessBarrier->getFinalizeLink(defaultObject);
				objectBuffer.add(env, defaultObject);
			}

			defaultObject = next;
		}
		objectBuffer.flush(env);
	}

	{
		/* walk reference objects */
		GC_FinalizableReferenceBuffer referenceBuffer(_extensions);
		omrobjectptr_t referenceObject = finalizeListManager->resetReferenceObjects();
		while (NULL != referenceObject) {
			omrobjectptr_t next = NULL;
			if(_scavenger->isObjectInEvacuateMemory(referenceObject)) {
				MM_ForwardedHeader forwardedHeader(referenceObject);
				if (!forwardedHeader.isForwardedPointer()) {
					next = _extensions->accessBarrier->getReferenceLink(referenceObject);
					omrobjectptr_t copiedObject = _scavenger->copyObject(env, &forwardedHeader);
					if (NULL == copiedObject) {
						referenceBuffer.add(env, referenceObject);
					} else {
						referenceBuffer.add(env, copiedObject);
					}
				} else {
					omrobjectptr_t forwardedPtr =  forwardedHeader.getNonStrictForwardedObject();
					Assert_MM_true(NULL != forwardedPtr);
					next = _extensions->accessBarrier->getReferenceLink(forwardedPtr);
					referenceBuffer.add(env, forwardedPtr);
				}
			} else {
				next = _extensions->accessBarrier->getReferenceLink(referenceObject);
				referenceBuffer.add(env, referenceObject);
			}

			referenceObject = next;
		}
		referenceBuffer.flush(env);
	}
}
#endif /* J9VM_GC_FINALIZATION */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
