/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "ArrayletObjectModelBase.hpp"
#include "GCExtensionsBase.hpp"

#if defined(J9VM_GC_ARRAYLETS)

bool
GC_ArrayletObjectModelBase::initialize(MM_GCExtensionsBase * extensions)
{
	_omrVM = extensions->getOmrVM();
	_arrayletRangeBase = NULL;
	_arrayletRangeTop = (void *)UDATA_MAX;
	_arrayletSubSpace = NULL;
	_largestDesirableArraySpineSize = UDATA_MAX;

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
GC_ArrayletObjectModelBase::expandArrayletSubSpaceRange(MM_MemorySubSpace * subSpace, void * rangeBase, void * rangeTop, UDATA largestDesirableArraySpineSize)
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

UDATA
GC_ArrayletObjectModelBase::getSpineSizeWithoutHeader(ArrayLayout layout, UDATA numberArraylets, UDATA dataSize, bool alignData)
{
	/* The spine consists of three (possibly empty) sections, not including the header:
	 * 1. the alignment word - padding between arrayoid and inline-data
	 * 2. the arrayoid - an array of pointers to the leaves
	 * 3. in-line data
	 * In hybrid specs, the spine may also include padding for a secondary size field in empty arrays
	 */
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	UDATA spineArrayoidSize = 0;
	UDATA spinePaddingSize = 0;
	if (InlineContiguous != layout) {
		if (0 != dataSize) {
			/* not in-line, so there in an arrayoid */
			spinePaddingSize = alignData ? (extensions->getObjectAlignmentInBytes() - sizeof(fj9object_t)) : 0;
			spineArrayoidSize = numberArraylets * sizeof(fj9object_t);
		}
	}
#else
	UDATA spinePaddingSize = alignData ? sizeof(fj9object_t) : 0;
	UDATA spineArrayoidSize = numberArraylets * sizeof(fj9object_t);
#endif
	UDATA spineDataSize = 0;
	if (InlineContiguous == layout) {
		spineDataSize = dataSize; // All data in spine
	} else if (Hybrid == layout) {
		spineDataSize = (dataSize & (_omrVM->_arrayletLeafSize - 1)); // Last arraylet in spine.
	}

	return spinePaddingSize + spineArrayoidSize + spineDataSize;
}

#endif /* defined(J9VM_GC_ARRAYLETS) */
