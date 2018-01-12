
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "CompactDelegate.hpp"
#include "CompactSchemeCheckMarkRoots.hpp"
#include "CompactSchemeFixupRoots.hpp"
#include "ConfigurationDelegate.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "MarkMap.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "PointerContiguousArrayIterator.hpp"

#if defined(OMR_GC_MODRON_COMPACTION)
bool
MM_CompactDelegate::initialize(MM_EnvironmentBase *env, OMR_VM *omrVM, MM_MarkMap *markMap, MM_CompactScheme *compactScheme)
{
	_omrVM = omrVM;
	_compactScheme = compactScheme;
	_markMap = markMap;
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_CompactDelegate::tearDown(MM_EnvironmentBase *env)
{

}

void
MM_CompactDelegate::verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_CompactSchemeCheckMarkRoots rootChecker(MM_EnvironmentStandard::getEnvironment(env));
	rootChecker.scanAllSlots(env);

	/* Check that the heap alignment is as compaction expects it. Compaction
	 * expects that the heap will split into a whole number of pages where
	 * each pages maps to 2 mark bit slots so heap alignment needs to be a multiple
	 * of the compaction page size
	 */
	assume0(extensions->heapAlignment % (2 * J9BITS_BITS_IN_SLOT * sizeof(UDATA)) == 0);

	MM_HeapRegionManager *regionManager = env->getExtensions()->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		void *lowAddress = region->getLowAddress();
		void *highAddress = region->getHighAddress();
		MM_HeapMapIterator markedObjectIterator(extensions, markMap, (UDATA *)lowAddress, (UDATA *)highAddress);

		omrobjectptr_t objectPtr = NULL;
		while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
			switch(extensions->objectModel.getScanType(objectPtr)) {
			case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
			case GC_ObjectModel::SCAN_MIXED_OBJECT:
			case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
			case GC_ObjectModel::SCAN_CLASS_OBJECT:
			case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
			case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
			{
				GC_MixedObjectIterator it(_omrVM, objectPtr);
				while (GC_SlotObject* slotObject = it.nextSlot()) {
					if ((slotObject->readReferenceFromSlot() >= extensions->getHeap()->getHeapBase()) && (slotObject->readReferenceFromSlot() < extensions->getHeap()->getHeapTop())) {
						Assert_MM_true (markMap->isBitSet(slotObject->readReferenceFromSlot()));
					}
				}
				break;
			}

			case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
			{
				GC_PointerContiguousArrayIterator it(_omrVM, objectPtr);
				while (GC_SlotObject* slotObject = it.nextSlot()) {
					if ((slotObject->readReferenceFromSlot() >= extensions->getHeap()->getHeapBase()) && (slotObject->readReferenceFromSlot() < extensions->getHeap()->getHeapTop())) {
						Assert_MM_true (markMap->isBitSet(slotObject->readReferenceFromSlot()));
					}
				}
				break;
			}

			case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
				/* nothing to do */
				break;

			default:
				Assert_MM_unreachable();
			}
		}
	}
}

void
MM_CompactDelegate::fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme)
{
	MM_CompactSchemeFixupRoots rootScanner(env, compactScheme);
	rootScanner.scanAllSlots(env);
}

void
MM_CompactDelegate::workerCleanupAfterGC(MM_EnvironmentBase *env)
{
	/* flush ownable synchronizer object buffer after rebuild the ownableSynchronizerObjectList during fixupObjects */
	env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->flush(env);
}

void
MM_CompactDelegate::masterSetupForGC(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(extensions->getHeap()->getHeapRegionManager());
	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
			MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[i];
			list->startOwnableSynchronizerProcessing();
		}
	}
}

#endif /* OMR_GC_MODRON_COMPACTION */
