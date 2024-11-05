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
static uintptr_t
isValidClass(J9JavaVM *javaVM, J9Class *ptr, uintptr_t flags) {
	GC_SegmentIterator segmentIterator(javaVM->classMemorySegments, MEMORY_TYPE_RAM_CLASS);
	J9MemorySegment *segment;

	/* class field can't be NULL */
	if (NULL == ptr) {
		return J9OBJECTCHECK_INVALID;
	}

	/* check alignment - none of flag bits should be set */
	if (0 != ((uintptr_t)ptr & J9GC_J9OBJECT_CLAZZ_FLAGS_MASK)) {
		return J9OBJECTCHECK_INVALID;
	}

	/* We are not guarantee to have exclusive access at this point -- we need to mutex
	 * so that we don't pick up a segment while it is being added.
	 */
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(javaVM->classMemorySegments->segmentMutex);
#endif /* J9VM_THR_PREEMPTIVE */

	/* try to find the segment that this class starts in */
	while (NULL != (segment = segmentIterator.nextSegment())) {
		/* is the pointer in this segment? */
		if ((segment->heapBase <= (uint8_t *)ptr) && ((uint8_t *)ptr < segment->heapAlloc)) {
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
	if ((segment->heapAlloc - (uint8_t *)ptr) < (ptrdiff_t)sizeof(J9Class)) {
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
uintptr_t
j9gc_ext_check_is_valid_heap_object(J9JavaVM *javaVM, J9Object *ptr, uintptr_t flags)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(javaVM);

	/* check heap object alignment */
	if (0 != ((uintptr_t)ptr & (ext->getObjectAlignmentInBytes() - 1))) {
		return J9OBJECTCHECK_INVALID;
	}

	uintptr_t retVal = 0;
	void *lowAddress = NULL;
	void *highAddress = NULL;
	/* look up the region containing the object for checking that it is correctly contained */
	MM_Heap *heap = ext->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;

	/* try to find the region that this class starts in */
	while (NULL != (region = regionIterator.nextRegion())) {
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
	if (((uintptr_t)highAddress - (uintptr_t)ptr) < J9JAVAVM_OBJECT_HEADER_SIZE(javaVM)) {
		return J9OBJECTCHECK_INVALID;
	}
	
	/* NOTE: Individual fields of the header will be queried multiple times
	 * If they change underneath us, we may want to take a snapshot
	 * of the header before querying them
	 */

	/* ensure that its class is valid */
	retVal = isValidClass(javaVM, J9GC_J9OBJECT_CLAZZ_VM(ptr, javaVM), flags);
	if (J9OBJECTCHECK_CLASS != retVal) {
		return retVal;
	}

	/* ensure that the shape is correct */
	if (!ext->objectModel.checkIndexableFlag(ptr)) {
		return J9OBJECTCHECK_INVALID;
	}

	if (ext->objectModel.isObjectArray(ptr)
		|| ext->objectModel.isPrimitiveArray(ptr)) {
		/* ensure that the array size fits into the segment */
		if (((uintptr_t)highAddress - (uintptr_t)ptr) < J9JAVAVM_CONTIGUOUS_INDEXABLE_HEADER_SIZE(javaVM)) {
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
	if (((uintptr_t)highAddress - (uintptr_t)ptr) < ext->objectModel.getSizeInBytesWithHeader(ptr)) {
		return J9OBJECTCHECK_INVALID;
	}

	return J9OBJECTCHECK_OBJECT;
}
} /* extern "C" */
