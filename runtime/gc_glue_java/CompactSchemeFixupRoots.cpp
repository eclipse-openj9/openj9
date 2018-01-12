/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "CompactSchemeFixupRoots.hpp"

#include "objectdescription.h"

#include "Base.hpp"
#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizeListManager.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "ObjectAccessBarrier.hpp"
#include "Task.hpp"
#include "UnfinalizedObjectBuffer.hpp"
#include "UnfinalizedObjectList.hpp"


#if defined(J9VM_GC_FINALIZATION)
void
MM_CompactSchemeFixupRoots::fixupFinalizableObjects(MM_EnvironmentBase *env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	GC_FinalizeListManager * finalizeListManager = extensions->finalizeListManager;

	{
		GC_FinalizableObjectBuffer objectBuffer(extensions);
		/* walk finalizable objects loaded by the system class loader */
		omrobjectptr_t systemObject = finalizeListManager->resetSystemFinalizableObjects();
		while (NULL != systemObject) {
			omrobjectptr_t forwardedPtr = _compactScheme->getForwardingPtr(systemObject);
			/* read the next link out of the moved copy of the object before we add it to the buffer */
			systemObject = extensions->accessBarrier->getFinalizeLink(forwardedPtr);
			/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
			objectBuffer.add(env, forwardedPtr);
		}
		objectBuffer.flush(env);
	}

	{
		GC_FinalizableObjectBuffer objectBuffer(extensions);
		/* walk finalizable objects loaded by the all other class loaders */
		omrobjectptr_t defaultObject = finalizeListManager->resetDefaultFinalizableObjects();
		while (NULL != defaultObject) {
			omrobjectptr_t forwardedPtr = _compactScheme->getForwardingPtr(defaultObject);
			/* read the next link out of the moved copy of the object before we add it to the buffer */
			defaultObject = extensions->accessBarrier->getFinalizeLink(forwardedPtr);
			/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
			objectBuffer.add(env, forwardedPtr);
		}
		objectBuffer.flush(env);
	}

	{
		/* walk reference objects */
		GC_FinalizableReferenceBuffer referenceBuffer(_extensions);
		omrobjectptr_t referenceObject = finalizeListManager->resetReferenceObjects();
		while (NULL != referenceObject) {
			omrobjectptr_t forwardedPtr = _compactScheme->getForwardingPtr(referenceObject);
			/* read the next link out of the moved copy of the object before we add it to the buffer */
			referenceObject = _extensions->accessBarrier->getReferenceLink(forwardedPtr);
			/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
			referenceBuffer.add(env, forwardedPtr);
		}
		referenceBuffer.flush(env);
	}
}

void
MM_CompactSchemeFixupRoots::fixupUnfinalizedObjects(MM_EnvironmentBase *env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(extensions->getHeap()->getHeapRegionManager());
		while(NULL != (region = regionIterator.nextRegion())) {
			MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
			for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
				MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
				list->startUnfinalizedProcessing();
			}
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(extensions->getHeap()->getHeapRegionManager());
	while(NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
			MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
			if (!list->wasEmpty()) {
				if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					omrobjectptr_t object = list->getPriorList();
					while (NULL != object) {
						omrobjectptr_t forwardedPtr = _compactScheme->getForwardingPtr(object);
						/* read the next link out of the moved copy of the object before we add it to the buffer */
						object = extensions->accessBarrier->getFinalizeLink(forwardedPtr);
						/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
						env->getGCEnvironment()->_unfinalizedObjectBuffer->add(env, forwardedPtr);
					}
				}
			}
		}
	}

	/* restore everything to a flushed state before exiting */
	env->getGCEnvironment()->_unfinalizedObjectBuffer->flush(env);
}
#endif /* J9VM_GC_FINALIZATION */
