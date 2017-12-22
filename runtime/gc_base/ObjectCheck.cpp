
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

#include "ObjectCheck.hpp"

#include "ClassModel.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "ObjectModel.hpp"
#include "SegmentIterator.hpp"

extern "C" {
/**
 * Check if the pointer is a valid class.  Helper function for j9gc_ext_check_is_valid_heap_object.
 * @param javaVM
 * @param ptr the pointer to check if it is a valid class
 * @param flags set J9OBJECTCHECK_FLAGS_* to control checking behaviour.
 * @return one of J9OBJECTCHECK_* depending if the class is valid, invalid or looks like a forwarded pointer
 * @ingroup GC_Base
 */
static UDATA
isValidClass(J9JavaVM *javaVM, J9Class *ptr, UDATA flags) {
	GC_SegmentIterator segmentIterator(javaVM->classMemorySegments, MEMORY_TYPE_RAM_CLASS);
	J9MemorySegment *segment;

	/* class field can't be NULL */
	if (NULL == ptr) {
		return J9OBJECTCHECK_INVALID;
	}

	/* check alignment */
	if (0 != ((UDATA)ptr & (sizeof(UDATA) - 1))) {
		return J9OBJECTCHECK_INVALID;
	}

	/* We are not guarantee to have exclusive access at this point -- we need to mutex
	 * so that we don't pick up a segment while it is being added.
	 */
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(javaVM->classMemorySegments->segmentMutex);
#endif /* J9VM_THR_PREEMPTIVE */

	/* try to find the segment that this class starts in */
	while(NULL != (segment = segmentIterator.nextSegment())) {
		/* is the pointer in this segment? */
		if ((segment->heapBase <= (U_8 *)ptr) && ((U_8 *)ptr < segment->heapAlloc)) {
			break;
		}
	}

#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(javaVM->classMemorySegments->segmentMutex);
#endif /* J9VM_THR_PREEMPTIVE */

	/* pointer is not in class segment */
	if (NULL == segment) {
		return J9OBJECTCHECK_INVALID;
	}

	/* ensure that the class header fits into the segment */
	if ((segment->heapAlloc - (U_8 *)ptr) < (ptrdiff_t)sizeof(J9Class)) {
		return J9OBJECTCHECK_INVALID;
	}

	return J9OBJECTCHECK_CLASS;
}

/**
 * Check if the pointer is a valid object in the object heap.
 * @param javaVM
 * @param ptr the pointer to check if it is a valid object
 * @param flags set J9OBJECTCHECK_FLAGS_* to control checking behaviour.
 * @return one of J9OBJECTCHECK_* depending if the object is valid, invalid or looks like it was forwarded.
 * @ingroup GC_Base
 */
UDATA
j9gc_ext_check_is_valid_heap_object(J9JavaVM *javaVM, J9Object *ptr, UDATA flags)
{
	/* check alignment */
	if (0 != ((UDATA)ptr & (sizeof(UDATA) - 1) )) {
		return J9OBJECTCHECK_INVALID;
	}

	UDATA retVal = 0;
	void *lowAddress = NULL;
	void *highAddress = NULL;
	/* look up the region containing the object for checking that it is correctly contained */
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_Heap *heap = extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;

	/* try to find the region that this class starts in */
	while(NULL != (region = regionIterator.nextRegion())) {
		/* is the pointer in this segment? */
		lowAddress = region->getLowAddress();
		highAddress = region->getHighAddress();
		if ((lowAddress <= ptr) && (ptr < highAddress)) {
			break;
		}
	}

	/* pointer is not in a region, this may really be a pointer to a class */
	if (NULL == region) {
		return J9OBJECTCHECK_INVALID;
	}

	/* ensure that the object header fits into the segment */
	if (((UDATA)highAddress - (UDATA)ptr) < sizeof(J9Object)) {
		return J9OBJECTCHECK_INVALID;
	}
	
	/* NOTE: Individual fields of the header will be queried multiple times
	 * If they change underneath us, we may want to take a snapshot
	 * of the header before querying them
	 */

	/* ensure that its class is valid */
	retVal = isValidClass(javaVM, J9GC_J9OBJECT_CLAZZ(ptr), flags);
	if (J9OBJECTCHECK_CLASS != retVal) {
		return retVal;
	}

	/* ensure that the shape is correct */
	if (!extensions->objectModel.checkIndexableFlag(ptr)) {
		return J9OBJECTCHECK_INVALID;
	}

	if (extensions->objectModel.isObjectArray(ptr)
		|| extensions->objectModel.isPrimitiveArray(ptr)) {
		/* ensure that the array size fits into the segment */
		if (((UDATA)highAddress - (UDATA)ptr) < sizeof(J9IndexableObjectContiguous)) {
			return J9OBJECTCHECK_INVALID;
		}
	}

	/* ensure that the full object fits into the segment
	 * This must be done after knowing the class is valid, as we reach into the class to get the instance size
	 * Note: there are two cases for getSizeInBytesWithHeader:
	 *  IndexableObject: The size is retrieved from the buffered object in the fourth slot which has already been
	 *   cached, and range checked.
	 *  MixedObject: The size is retrieved from the buffered class pointer.  The class pointer won't have changed
	 *   since checking it to be valid, however the size itself within the class memory may have changed. Since
	 *   this is a range check it "does not matter", we know we will be able to dereference the memory.
	 */
	if (((UDATA)highAddress - (UDATA)ptr) < extensions->objectModel.getSizeInBytesWithHeader(ptr)) {
		return J9OBJECTCHECK_INVALID;
	}

	return J9OBJECTCHECK_OBJECT;
}
} /* extern "C" */
