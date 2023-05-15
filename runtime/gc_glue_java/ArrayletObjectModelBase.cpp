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

#include "ArrayletObjectModelBase.hpp"
#include "GCExtensionsBase.hpp"

bool
GC_ArrayletObjectModelBase::initialize(MM_GCExtensionsBase * extensions)
{
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	_compressObjectReferences = extensions->compressObjectReferences();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	_omrVM = extensions->getOmrVM();
	_arrayletRangeBase = NULL;
	_arrayletRangeTop = (void *)UDATA_MAX;
	_arrayletSubSpace = NULL;
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	_enableDoubleMapping = false;
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
	_largestDesirableArraySpineSize = UDATA_MAX;
#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	_enableVirtualLargeObjectHeap = false;
	_isIndexableDataAddrPresent = false;
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
	_contiguousIndexableHeaderSize = 0;
	_discontiguousIndexableHeaderSize = 0;

	return true;
}

void
GC_ArrayletObjectModelBase::tearDown(MM_GCExtensionsBase * extensions)
{

	/* ensure that we catch any invalid uses during shutdown */
	_omrVM = NULL;
	_arrayletRangeTop = NULL;
	_arrayletSubSpace = NULL;
	_arrayletRangeBase = 0;
	_largestDesirableArraySpineSize = 0;
}


void
GC_ArrayletObjectModelBase::expandArrayletSubSpaceRange(MM_MemorySubSpace * subSpace, void * rangeBase, void * rangeTop, uintptr_t largestDesirableArraySpineSize)
{
	/* Is this the first expand? */
	if(NULL == _arrayletSubSpace) {
		_arrayletRangeBase = rangeBase;
		_arrayletRangeTop = rangeTop;
		_arrayletSubSpace = subSpace;
		_largestDesirableArraySpineSize = largestDesirableArraySpineSize;
	} else {
		/* Expand the valid range for arraylets. */
		if (rangeBase < _arrayletRangeBase) {
			_arrayletRangeBase = rangeBase;
		}
		if (rangeTop > _arrayletRangeTop) {
			_arrayletRangeTop = rangeTop;
		}
	}
}

uintptr_t
GC_ArrayletObjectModelBase::getSpineSizeWithoutHeader(ArrayLayout layout, uintptr_t numberArraylets, uintptr_t dataSize, bool alignData)
{
	/* The spine consists of three (possibly empty) sections, not including the header:
	 * 1. the alignment word - padding between arrayoid and inline-data
	 * 2. the arrayoid - an array of pointers to the leaves
	 * 3. in-line data
	 * In hybrid specs, the spine may also include padding for a secondary size field in empty arrays
	 */
	uintptr_t const slotSize = J9GC_REFERENCE_SIZE(this);
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	uintptr_t spineArrayoidSize = 0;
	uintptr_t spinePaddingSize = 0;
	if (InlineContiguous != layout) {
		if (0 != dataSize) {
			/* not in-line, so there in an arrayoid */
			spinePaddingSize = alignData ? (extensions->getObjectAlignmentInBytes() - slotSize) : 0;
			spineArrayoidSize = numberArraylets * slotSize;
		}
	}
	bool isAllIndexableDataContiguousEnabled = extensions->indexableObjectModel.isVirtualLargeObjectHeapEnabled();
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	isAllIndexableDataContiguousEnabled = (isAllIndexableDataContiguousEnabled || extensions->indexableObjectModel.isDoubleMappingEnabled());
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */

	uintptr_t spineDataSize = 0;
	if (InlineContiguous == layout) {
		spineDataSize = dataSize; // All data in spine
		if (isAllIndexableDataContiguousEnabled && (!extensions->indexableObjectModel.isArrayletDataAdjacentToHeader(dataSize))) {
			spineDataSize = 0;
		}
	} else if (Hybrid == layout) {
		if (isAllIndexableDataContiguousEnabled) {
			extensions->indexableObjectModel.AssertContiguousArrayDataUnreachable();
		}
		/* Last arraylet in spine */
		spineDataSize = (dataSize & (_omrVM->_arrayletLeafSize - 1));
	}

	return spinePaddingSize + spineArrayoidSize + spineDataSize;
}
