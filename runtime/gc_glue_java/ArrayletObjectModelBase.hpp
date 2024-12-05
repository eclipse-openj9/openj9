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

#if !defined(ARRAYLETOBJECTMODELBASE_)
#define ARRAYLETOBJECTMODELBASE_

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modron.h"
#include "modronopt.h"

#include "Bits.hpp"
#include "Math.hpp"

class MM_GCExtensionsBase;
class MM_MemorySubSpace;

class GC_ArrayletObjectModelBase 
{
/*
* Data members
*/
private:
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	bool _enableDoubleMapping; /** Allows arraylets to be double mapped */
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
protected:
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	bool _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	OMR_VM *_omrVM; 	/**< used so that we can pull the arrayletLeafSize and arrayletLeafLogSize for arraylet sizing calculations */
	void * _arrayletRangeBase; /**< The base heap range of where discontiguous arraylets are allowed. */
	void * _arrayletRangeTop; /**< The top heap range of where discontiguous arraylets are allowed. */
	MM_MemorySubSpace * _arrayletSubSpace; /**< The only subspace that is allowed to have discontiguous arraylets. */
	uintptr_t _largestDesirableArraySpineSize; /**< A cached copy of the subspace's _largestDesirableArraySpineSize to be used when we don't have access to a subspace. */
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
	bool _enableVirtualLargeObjectHeap;
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
#if defined(J9VM_ENV_DATA64)
	bool _isIndexableDataAddrPresent;
#endif /* defined(J9VM_ENV_DATA64) */
	uintptr_t _contiguousIndexableHeaderSize;
	uintptr_t _discontiguousIndexableHeaderSize;
public:
	typedef enum ArrayLayout {
		Illegal = 0,
		InlineContiguous,
		Discontiguous,
		Hybrid
	} ArrayLayout;

/*
* Function members
*/
private:
protected:
	virtual bool initialize(MM_GCExtensionsBase *extensions);
	virtual void tearDown(MM_GCExtensionsBase *extensions);

	/**
	 * Get the spine size without header for an arraylet with these properties
	 * @param layout The layout of the indexable object
	 * @param numberArraylets Number of arraylets for this indexable object
	 * @param dataSize How many elements are in the indexable object
	 * @param alignData Should the data section be aligned
	 * @return spineSize The actual size in byte of the spine
	 */
	uintptr_t
	getSpineSizeWithoutHeader(ArrayLayout layout, uintptr_t numberArraylets, uintptr_t dataSize, bool alignData);

public:
	/**
	 * Return back true if object references are compressed
	 * @return true, if object references are compressed
	 */
	MMINLINE bool
	compressObjectReferences()
	{
		return OMR_COMPRESS_OBJECT_REFERENCES(_compressObjectReferences);
	}

	/**
	 * Returns the size of an indexable object in elements.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Size of object in elements
	 */
	MMINLINE uintptr_t
	getSizeInElements(J9IndexableObject *arrayPtr)
	{
		uintptr_t size = 0;
		if (compressObjectReferences()) {
			size = ((J9IndexableObjectContiguousCompressed *)arrayPtr)->size;
			if (0 == size) {
				/* Discontiguous */
				size = ((J9IndexableObjectDiscontiguousCompressed *)arrayPtr)->size;
			}
		} else {
			size = ((J9IndexableObjectContiguousFull *)arrayPtr)->size;
			if (0 == size) {
				/* Discontiguous */
				size = ((J9IndexableObjectDiscontiguousFull *)arrayPtr)->size;
			}
		}
		return size;
	}

	/**
	 * Sets size in elements of a contiguous indexable object .
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @param size Size in elements to set.
	 */
	MMINLINE void
	setSizeInElementsForContiguous(J9IndexableObject *arrayPtr, uintptr_t size)
	{
		if (compressObjectReferences()) {
			((J9IndexableObjectContiguousCompressed *)arrayPtr)->size = (uint32_t)size;
		} else {
			((J9IndexableObjectContiguousFull *)arrayPtr)->size = (uint32_t)size;
		}
	}

	/**
	 * Sets the contiguous and discontiguous header size values
	 * @param vm Java VM
	 */
	MMINLINE void
	setHeaderSizes(J9JavaVM *vm)
	{
		_contiguousIndexableHeaderSize = vm->contiguousIndexableHeaderSize;
		_discontiguousIndexableHeaderSize = vm->discontiguousIndexableHeaderSize;
	}

#if defined(J9VM_ENV_DATA64)
	/**
	 * Sets whether the dataAddr field is present in the indexable object header
	 * @param vm Java VM
	 */
	MMINLINE void
	setIsIndexableDataAddrPresent(J9JavaVM *vm)
	{
		_isIndexableDataAddrPresent = vm->isIndexableDataAddrPresent;
	}
#endif /* defined(J9VM_ENV_DATA64) */

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	/**
	 * Returns enable double mapping status
	 * 
	 * @return true if double mapping status is set to true, false otherwise.
	 */
	MMINLINE bool
	isDoubleMappingEnabled()
	{
		return _enableDoubleMapping;
	}
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */

	/**
	 * Sets size in elements of a discontiguous indexable object .
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @param size Size in elements to set.
	 */
	MMINLINE void
	setSizeInElementsForDiscontiguous(J9IndexableObject *arrayPtr, uintptr_t size)
	{
		if (compressObjectReferences()) {
			((J9IndexableObjectDiscontiguousCompressed *)arrayPtr)->mustBeZero = 0;
			((J9IndexableObjectDiscontiguousCompressed *)arrayPtr)->size = (uint32_t)size;
		} else {
			((J9IndexableObjectDiscontiguousFull *)arrayPtr)->mustBeZero = 0;
			((J9IndexableObjectDiscontiguousFull *)arrayPtr)->size = (uint32_t)size;
		}
	}

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
	/**
	 * Set whether the virtual large object heap (off-heap) allocation for large objects is enabled.
	 *
	 * @param enableVirtualLargeObjectHeap[in] if true, off-heap is enabled.
	 */
	MMINLINE void
	setEnableVirtualLargeObjectHeap(bool enableVirtualLargeObjectHeap)
	{
		_enableVirtualLargeObjectHeap = enableVirtualLargeObjectHeap;
	}
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

#if defined(J9VM_ENV_DATA64)
	/**
	 * Set whether the indexable header field dataAddr is present in the header of the indexable object.
	 *
	 * @param isDataAddressPresent[in] if true, dataAddr is present.
	 */
	MMINLINE void
	setIsDataAddressPresent(bool isDataAddressPresent)
	{
		_isIndexableDataAddrPresent = isDataAddressPresent;
	}
#endif /* defined(J9VM_ENV_DATA64) */

	/**
	 * Query if virtual large object heap (off-heap) allocation for large objects is enabled.
	 *
	 * @return true if virtual large object heap (off-heap) allocation for large objects is enabled, 0 otherwise
	 */
	MMINLINE bool
	isVirtualLargeObjectHeapEnabled()
	{
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
		return _enableVirtualLargeObjectHeap;
#else /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
		return false;
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
	}

	/**
	 * Determines whether or not a spine that represents an object of this
	 * class should have its data section aligned.
	 * @param clazz The class to check alignment for
	 * @return needAlignment Should the data section be aligned
	 */
	MMINLINE bool
	shouldAlignSpineDataSection(J9Class *clazz)
	{
		bool needAlignment = false;

		if (compressObjectReferences()) {
			/* Compressed pointers require that each leaf starts at an appropriately-aligned address
			 * (based on the compressed shift value). If this is not done, compressed leaf pointers
			 * would be unable to reach all of the heap.
			 */
			needAlignment = true;
#if !defined(J9VM_ENV_DATA64)
		} else {
			/* The alignment padding is only required when the size of the spine pointers are
			 * not 8 byte aligned.  For example, on a 64-bit non-compressed platform, a single
			 * spine pointer would be 8 byte aligned, whereas on a 32-bit platform, a single
			 * spine pointer would not be 8 byte aligned, and would require additional padding.
			 */
			if (OBJECT_HEADER_SHAPE_DOUBLES == J9GC_CLASS_SHAPE(clazz)) {
				needAlignment = true;
			}
#endif /* !J9VM_ENV_DATA64 */
		}

		return needAlignment;
	}

	/**
	 * Get the size in bytes for the arraylet at index in indexable object objPtr
	 * @param objPtr Pointer to an array object
	 * @param index the index in question.  0<=index<numArraylets(objPtr)
	 * @param dataSizeInBytes size of data in an indexable object, in bytes, including leaves, excluding the header.
	 * @param numArraylets total number of arraylets for the given indexable object
	 * @return the size of the indexth arraylet
	 */
	MMINLINE uintptr_t
	arrayletSize(J9IndexableObject *objPtr, uintptr_t index, uintptr_t dataSizeInBytes, uintptr_t numArraylets)
	{
		uintptr_t arrayletLeafSize = _omrVM->_arrayletLeafSize;
		if (index < numArraylets - 1) {
			return arrayletLeafSize;
		} else {
			return MM_Math::saturatingSubtract(dataSizeInBytes, index * arrayletLeafSize);
		}
	}

	/**
	 * Return the total number of arraylets for an indexable object with a size of dataInSizeByte
	 * @param dataSizeInBytes size of an array in bytes (not elements)
	 * @return the number of arraylets used for an array of dataSizeInBytes bytes
	 */
	MMINLINE uintptr_t
	numArraylets(uintptr_t dataSizeInBytes)
	{
		uintptr_t leafSize = _omrVM->_arrayletLeafSize;
		uintptr_t numberOfArraylets = 1;
		if (UDATA_MAX != leafSize) {
			uintptr_t leafSizeMask = leafSize - 1;
			uintptr_t leafLogSize = _omrVM->_arrayletLeafLogSize;

			/* CMVC 135307 : following logic for calculating the leaf count would not overflow dataSizeInBytes.
			 * the assumption is leaf size is order of 2. It's identical to:
			 * if (dataSizeInBytes % leafSize) is 0
			 * 	leaf count = dataSizeInBytes >> leafLogSize
			 * else
			 * 	leaf count = (dataSizeInBytes >> leafLogSize) + 1
			 */
			numberOfArraylets = ((dataSizeInBytes >> leafLogSize) + (((dataSizeInBytes & leafSizeMask) + leafSizeMask) >> leafLogSize));
		}
		return numberOfArraylets;
	}

	/**
	 * Expands the heap range in which discontiguous arraylets are allowed.
	 * @param subSpace The subspace of the range in which discontiguous arraylets are allowed.
	 * @param rangeBase The base heap range of where discontiguous arraylets are allowed.
	 * @param rangeTop The top heap range of where discontiguous arraylets are allowed.
	 * @param largestDesirableArraySpineSize The subspace's _largestDesirableArraySpineSize to be used when we don't have access to a subspace.
	 */
	void
	expandArrayletSubSpaceRange(MM_MemorySubSpace* subSpace, void* rangeBase, void* rangeTop, uintptr_t largestDesirableArraySpineSize);
};

#endif /* ARRAYLETOBJECTMODELBASE_ */
