/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include "ArrayletLeafIterator.hpp"
#include "ArrayletObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"
#include "ObjectModel.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"

bool
GC_ArrayletObjectModel::initialize(MM_GCExtensionsBase *extensions)
{
	return GC_ArrayletObjectModelBase::initialize(extensions);
}

void
GC_ArrayletObjectModel::tearDown(MM_GCExtensionsBase *extensions)
{
	GC_ArrayletObjectModelBase::tearDown(extensions);
}

void
GC_ArrayletObjectModel::AssertBadElementSize()
{
	Assert_MM_unreachable();
}

void
GC_ArrayletObjectModel::AssertContiguousArrayDataUnreachable()
{
	Assert_MM_unreachable();
}

void
GC_ArrayletObjectModel::AssertArrayletIsDiscontiguous(J9IndexableObject *objPtr)
{
	if (!isVirtualLargeObjectHeapEnabled()) {
		uintptr_t arrayletLeafSize = _omrVM->_arrayletLeafSize;
		uintptr_t remainderBytes = getDataSizeInBytes(objPtr) % arrayletLeafSize;
		if (0 != remainderBytes) {
			MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
			Assert_MM_true((getSpineSize(objPtr) + remainderBytes + extensions->getObjectAlignmentInBytes()) > arrayletLeafSize);
		}
	} else if (0 != getSizeInElements(objPtr)) {
		Assert_MM_unreachable();
	}
}

void
GC_ArrayletObjectModel::AssertContiguousArrayletLayout(J9IndexableObject *objPtr)
{
	Assert_MM_true(InlineContiguous == getArrayLayout(objPtr));
}

void
GC_ArrayletObjectModel::AssertVirtualLargeObjectHeapEnabled()
{
	Assert_MM_true(isVirtualLargeObjectHeapEnabled());
}

void
GC_ArrayletObjectModel::AssertDiscontiguousArrayletLayout(J9IndexableObject *objPtr)
{
	ArrayLayout layout = getArrayLayout(objPtr);
	Assert_MM_true((Discontiguous == layout) || (Hybrid == layout));
}

GC_ArrayletObjectModel::ArrayLayout
GC_ArrayletObjectModel::getArrayletLayout(J9Class* clazz, uintptr_t numberOfElements, uintptr_t largestDesirableSpine)
{
	ArrayLayout layout = Illegal;
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	uintptr_t objectAlignmentInBytes = extensions->getObjectAlignmentInBytes();
	uintptr_t dataSizeInBytes = getDataSizeInBytes(clazz, numberOfElements);

	/* the spine need not contain a pointer to the data */
	const uintptr_t minimumSpineSize = 0;
	uintptr_t minimumSpineSizeAfterGrowing = minimumSpineSize;
	if (extensions->isVLHGC()) {
		/* CMVC 170688:  Ensure that we don't try to allocate an inline contiguous array of a size which will overflow the region if it ever grows
		 * (easier to handle this case in the allocator than to special-case the collectors to know how to avoid this case)
		 * (currently, we only grow by a hashcode slot which is 4-bytes but will increase our size by the granule of alignment)
		 */
		minimumSpineSizeAfterGrowing += objectAlignmentInBytes;
	}

	/* CMVC 135307 : when checking for InlineContiguous layout, perform subtraction as adding to dataSizeInBytes could trigger overflow. */
	if ((largestDesirableSpine == UDATA_MAX)
		|| (dataSizeInBytes <= (largestDesirableSpine - minimumSpineSizeAfterGrowing - contiguousIndexableHeaderSize()))) {
		layout = InlineContiguous;
		if (0 == numberOfElements) {
			/* Zero sized NUA uses the discontiguous shape */
			layout = Discontiguous;
		}
	} else {
		uintptr_t arrayletLeafSize = _omrVM->_arrayletLeafSize;
		uintptr_t lastArrayletBytes = dataSizeInBytes & (arrayletLeafSize - 1);

		if (isVirtualLargeObjectHeapEnabled() && (0 < dataSizeInBytes)) {
			layout = InlineContiguous;
		} else if (lastArrayletBytes > 0) {
			Assert_MM_false(isVirtualLargeObjectHeapEnabled());
			/* determine how large the spine would be if this were a hybrid array */
			uintptr_t numberArraylets = numArraylets(dataSizeInBytes);
			bool align = shouldAlignSpineDataSection(clazz);
			uintptr_t hybridSpineBytes = getSpineSize(clazz, Hybrid, numberArraylets, dataSizeInBytes, align);
			uintptr_t adjustedHybridSpineBytes = extensions->objectModel.adjustSizeInBytes(hybridSpineBytes);
			uintptr_t adjustedHybridSpineBytesAfterMove = adjustedHybridSpineBytes;
			if (extensions->isVLHGC()) {
				adjustedHybridSpineBytesAfterMove += objectAlignmentInBytes;
			}
			/* if remainder data can fit in spine, make it hybrid */
			if (adjustedHybridSpineBytesAfterMove <= largestDesirableSpine) {
				/* remainder data can fit in spine, last arrayoid pointer points to empty data section in spine */
				layout = Hybrid;
			} else {
				/* remainder data will go into an arraylet, last arrayoid pointer points to it */
				layout = Discontiguous;
			}
		} else {
			/* remainder is empty, so no arraylet allocated; last arrayoid pointer is set to MULL */
			layout = Discontiguous;
		}
	}
	return layout;
}

void
GC_ArrayletObjectModel::fixupInternalLeafPointersAfterCopy(J9IndexableObject *destinationPtr, J9IndexableObject *sourcePtr)
{
	if (hasArrayletLeafPointers(destinationPtr)) {
		GC_ArrayletLeafIterator leafIterator((J9JavaVM *)_omrVM->_language_vm, destinationPtr);
		GC_SlotObject *leafSlotObject = NULL;
		uintptr_t sourceStartAddress = (uintptr_t) sourcePtr;
		uintptr_t sourceEndAddress = sourceStartAddress + getSizeInBytesWithHeader(destinationPtr);

		while (NULL != (leafSlotObject = leafIterator.nextLeafPointer())) {
			uintptr_t leafAddress = (uintptr_t)leafSlotObject->readReferenceFromSlot();

			if ((sourceStartAddress < leafAddress) && (leafAddress < sourceEndAddress)) {
				leafSlotObject->writeReferenceToSlot((J9Object *)((uintptr_t)destinationPtr + (leafAddress - sourceStartAddress)));
			}
		}
	}
}

bool
GC_ArrayletObjectModel::shouldFixupDataAddrForContiguous(J9IndexableObject *arrayPtr)
{
#if defined(J9VM_ENV_DATA64)
	return ((void *)((uintptr_t)arrayPtr + contiguousIndexableHeaderSize()) == getDataAddrForContiguous(arrayPtr));
#else /* defined(J9VM_ENV_DATA64) */
	return false;
#endif /* defined(J9VM_ENV_DATA64) */
}

bool
GC_ArrayletObjectModel::shouldFixupDataAddrForContiguous(MM_ForwardedHeader *forwardedHeader, void *dataAddr)
{
#if defined(J9VM_ENV_DATA64)
	return ((void *)((uintptr_t)forwardedHeader->getObject() + contiguousIndexableHeaderSize()) == dataAddr);
#else /* defined(J9VM_ENV_DATA64) */
	return false;
#endif /* defined(J9VM_ENV_DATA64) */
}

bool
GC_ArrayletObjectModel::isDataAdjacentToHeader(J9IndexableObject *arrayPtr)
{
	uintptr_t dataSizeInBytes = getDataSizeInBytes(arrayPtr);
	return isDataAdjacentToHeader(dataSizeInBytes);
}

bool
GC_ArrayletObjectModel::isDataAdjacentToHeader(uintptr_t dataSizeInBytes)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	uintptr_t minimumSpineSizeAfterGrowing = extensions->getObjectAlignmentInBytes();
	return ((UDATA_MAX == _largestDesirableArraySpineSize)
			|| (dataSizeInBytes <= (_largestDesirableArraySpineSize - minimumSpineSizeAfterGrowing - contiguousIndexableHeaderSize())));
}

#if defined(J9VM_ENV_DATA64)
void
GC_ArrayletObjectModel::AssertArrayPtrIsIndexable(J9IndexableObject *arrayPtr)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	Assert_MM_true(extensions->objectModel.isIndexable(J9GC_J9OBJECT_CLAZZ(arrayPtr, this)));
}
#endif /* defined(J9VM_ENV_DATA64) */
