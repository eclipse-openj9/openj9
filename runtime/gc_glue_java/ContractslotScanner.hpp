
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "Base.hpp"
#include "ClassIterator.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizeListManager.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "ModronAssertions.h"
#include "ObjectAccessBarrier.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "RootScanner.hpp"
#include "RootScannerTypes.h"
#include "UnfinalizedObjectBuffer.hpp"
#include "UnfinalizedObjectList.hpp"

/* All slot scanner for full VM slot walk.
 * @copydoc MM_RootScanner
 * @ingroup GC_Modron_Base
 */
class MM_ContractSlotScanner : public MM_RootScanner
{
private:
	void *_srcBase;
	void *_srcTop;
	void *_dstBase;
protected:
public:

private:
protected:
public:
	MM_ContractSlotScanner(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase) :
		MM_RootScanner(env, true)
		,_srcBase(srcBase)
		,_srcTop(srcTop)
		,_dstBase(dstBase)
	{}

	virtual void
	doSlot(J9Object **slotPtr)
	{
		J9Object *objectPtr = *slotPtr;
		if(NULL != objectPtr) {
			if((objectPtr >= (J9Object *)_srcBase) && (objectPtr < (J9Object *)_srcTop)) {
				objectPtr = (J9Object *)((((UDATA)objectPtr) - ((UDATA)_srcBase)) + ((UDATA)_dstBase));
				*slotPtr = objectPtr;
			}
		}
	}

	virtual void
	doClass(J9Class *clazz)
	{
		volatile j9object_t *objectSlotPtr = NULL;

		/* walk all object slots */
		GC_ClassIterator classIterator(_env, clazz);
		while((objectSlotPtr = classIterator.nextSlot()) != NULL) {
			/* discard volatile since we must be in stop-the-world mode */
			doSlot((j9object_t*)objectSlotPtr);
		}
		/* There is no need to walk the J9Class slots since no values within this struct could
		 * move during a nursery contract.
		 */
	}

	virtual void
	scanUnfinalizedObjects(MM_EnvironmentBase *env)
	{
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);

		/* Only walk MEMORY_TYPE_NEW regions since MEMORY_TYPE_OLD regions would not contain
		 * any objects that would move during a nursery contract.
		 */
		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(env->getExtensions()->heap->getHeapRegionManager());
		while(NULL != (region = regionIterator.nextRegion())) {
			if ((MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
				for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
					MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
					list->startUnfinalizedProcessing();
				}
			}
		}

		GC_HeapRegionIteratorStandard regionIterator2(env->getExtensions()->heap->getHeapRegionManager());
		while(NULL != (region = regionIterator2.nextRegion())) {
			if ((MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
				for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
					MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[i];
					if (!list->wasEmpty()) {
						J9Object *object = list->getPriorList();
						while (NULL != object) {
							J9Object *movePtr = object;
							if((movePtr >= (J9Object *)_srcBase) && (movePtr < (J9Object *)_srcTop)) {
								movePtr = (J9Object *)((((UDATA)movePtr) - ((UDATA)_srcBase)) + ((UDATA)_dstBase));
							}
							/* read the next link out of the moved copy of the object before we add it to the buffer */
							object = _extensions->accessBarrier->getFinalizeLink(movePtr);
							/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
							env->getGCEnvironment()->_unfinalizedObjectBuffer->add(env, movePtr);
						}
					}
				}
			}
		}

		/* restore everything to a flushed state before exiting */
		env->getGCEnvironment()->_unfinalizedObjectBuffer->flush(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}

	virtual void
	scanOwnableSynchronizerObjects(MM_EnvironmentBase *env)
	{
		reportScanningStarted(RootScannerEntity_OwnableSynchronizerObjects);

		/* Only walk MEMORY_TYPE_NEW regions since MEMORY_TYPE_OLD regions would not contain
		 * any objects that would move during a nursery contract.
		 */
		MM_HeapRegionDescriptorStandard *region = NULL;
		GC_HeapRegionIteratorStandard regionIterator(env->getExtensions()->heap->getHeapRegionManager());
		while(NULL != (region = regionIterator.nextRegion())) {
			if ((MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
				for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
					MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[i];
					list->startOwnableSynchronizerProcessing();
				}
			}
		}

		GC_HeapRegionIteratorStandard regionIterator2(env->getExtensions()->heap->getHeapRegionManager());
		while(NULL != (region = regionIterator2.nextRegion())) {
			if ((MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
				for (UDATA i = 0; i < regionExtension->_maxListIndex; i++) {
					MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[i];
					if (!list->wasEmpty()) {
						J9Object *object = list->getPriorList();
						while (NULL != object) {
							J9Object *movePtr = object;
							if ((movePtr >= (J9Object *)_srcBase) && (movePtr < (J9Object *)_srcTop)) {
								movePtr = (J9Object *)((((UDATA)movePtr) - ((UDATA)_srcBase)) + ((UDATA)_dstBase));
							}
							/* read the next link out of the moved copy of the object before we add it to the buffer */
							J9Object *next = _extensions->accessBarrier->getOwnableSynchronizerLink(movePtr);
							/* the last object in the list pointing itself, after the object moved, the link still points to old object address */
							if (object != next) {
								object = next;
							} else {
								/* reach the end of the list */
								object = NULL;
							}
							/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
							env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->add(env, movePtr);
						}
					}
				}
			}
		}

		/* restore everything to a flushed state before exiting */
		env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->flush(env);
		reportScanningEnded(RootScannerEntity_OwnableSynchronizerObjects);
	}

#if defined(J9VM_GC_FINALIZATION)
	void
	doFinalizableObject(J9Object *objectPtr)
	{
		Assert_MM_unreachable();
	}

	virtual void
	scanFinalizableObjects(MM_EnvironmentBase *env)
	{
		reportScanningStarted(RootScannerEntity_FinalizableObjects);

		GC_FinalizeListManager * finalizeListManager = _extensions->finalizeListManager;
		{
			GC_FinalizableObjectBuffer objectBuffer(_extensions);
			/* walk finalizable objects loaded by the system class loader */
			j9object_t systemObject = finalizeListManager->resetSystemFinalizableObjects();
			while (NULL != systemObject) {
				J9Object *movePtr = systemObject;
				if((movePtr >= (J9Object *)_srcBase) && (movePtr < (J9Object *)_srcTop)) {
					movePtr = (J9Object *)((((UDATA)movePtr) - ((UDATA)_srcBase)) + ((UDATA)_dstBase));
				}
				/* read the next link out of the moved copy of the object before we add it to the buffer */
				systemObject = _extensions->accessBarrier->getFinalizeLink(movePtr);
				/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
				objectBuffer.add(env, movePtr);
			}
			objectBuffer.flush(env);
		}

		{
			GC_FinalizableObjectBuffer objectBuffer(_extensions);
			/* walk finalizable objects loaded by the all other class loaders */
			j9object_t defaultObject = finalizeListManager->resetDefaultFinalizableObjects();
			while (NULL != defaultObject) {
				J9Object *movePtr = defaultObject;
				if((movePtr >= (J9Object *)_srcBase) && (movePtr < (J9Object *)_srcTop)) {
					movePtr = (J9Object *)((((UDATA)movePtr) - ((UDATA)_srcBase)) + ((UDATA)_dstBase));
				}
				/* read the next link out of the moved copy of the object before we add it to the buffer */
				defaultObject = _extensions->accessBarrier->getFinalizeLink(movePtr);
				/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
				objectBuffer.add(env, movePtr);
			}
			objectBuffer.flush(env);
		}

		{
			/* walk reference objects */
			GC_FinalizableReferenceBuffer referenceBuffer(_extensions);
			j9object_t referenceObject = finalizeListManager->resetReferenceObjects();
			while (NULL != referenceObject) {
				J9Object *movePtr = referenceObject;
				if((movePtr >= (J9Object *)_srcBase) && (movePtr < (J9Object *)_srcTop)) {
					movePtr = (J9Object *)((((UDATA)movePtr) - ((UDATA)_srcBase)) + ((UDATA)_dstBase));
				}
				/* read the next link out of the moved copy of the object before we add it to the buffer */
				referenceObject = _extensions->accessBarrier->getReferenceLink(movePtr);
				/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
				referenceBuffer.add(env, movePtr);
			}
			referenceBuffer.flush(env);
		}

		reportScanningEnded(RootScannerEntity_FinalizableObjects);
	}
#endif /* J9VM_GC_FINALIZATION */
};
