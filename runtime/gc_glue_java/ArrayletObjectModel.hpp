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

/**
 * @file
 * @ingroup GC_Base
 */
#if !defined(ARRAYLETOBJECTMODEL_)
#define ARRAYLETOBJECTMODEL_

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modron.h"
#include "modronopt.h"

#include "ArrayletObjectModelBase.hpp"
#include "Bits.hpp"
#include "ForwardedHeader.hpp"
#include "Math.hpp"
#include "SlotObject.hpp"

class GC_ArrayletObjectModel : public GC_ArrayletObjectModelBase
{
/*
* Data members
*/
private:
protected:
public:

/*
* Function members
*/
private:
	void AssertBadElementSize();
protected:
	/* forward declare methods from parent class to avoid namespace issues */
	MMINLINE uintptr_t
	getSpineSizeWithoutHeader(ArrayLayout layout, uintptr_t numberArraylets, uintptr_t dataSize, bool alignData)
	{
		return GC_ArrayletObjectModelBase::getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
	}
public:
	/* forward declare methods from parent class to avoid namespace issues */
	MMINLINE uintptr_t
	arrayletSize(J9IndexableObject *objPtr, uintptr_t index, uintptr_t dataSizeInBytes, uintptr_t numberOfArraylets)
	{
		return GC_ArrayletObjectModelBase::arrayletSize(objPtr, index, dataSizeInBytes, numberOfArraylets);
	}
	MMINLINE uintptr_t
	numArraylets(uintptr_t unadjustedDataSizeInBytes)
	{
		return GC_ArrayletObjectModelBase::numArraylets(unadjustedDataSizeInBytes);
	}

	/**
	 * Get the header size for contiguous arrays
	 * @return header size
	 */
	MMINLINE uintptr_t
	contiguousIndexableHeaderSize()
	{
		return _contiguousIndexableHeaderSize;
	}

	/**
	 * Get the header size for discontiguous arrays
	 * @return header size
	 */
	MMINLINE uintptr_t
	discontiguousIndexableHeaderSize()
	{
		return _discontiguousIndexableHeaderSize;
	}

	/**
	 * Get the spine size for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return The total size in bytes of objPtr's array spine;
	 * 			includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	MMINLINE uintptr_t
	getSpineSize(J9IndexableObject* objPtr)
	{
		ArrayLayout layout = getArrayLayout(objPtr);
		return getSpineSize(objPtr, layout);
	}

	/**
	 * Get the spine size for an arraylet with these properties
	 * @param J9Class The class of the array.
	 * @param layout The layout of the indexable object
	 * @param numberArraylets Number of arraylets for this indexable object
	 * @param dataSize How many elements are in the indexable object
	 * @param alignData Should the data section be aligned
	 * @return spineSize The actual size in byte of the spine
	 */
	MMINLINE uintptr_t
	getSpineSize(J9Class *clazzPtr, ArrayLayout layout, uintptr_t numberArraylets, uintptr_t dataSize, bool alignData)
	{
		uintptr_t result = getHeaderSize(clazzPtr, layout) + getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
		return result;
	}

	/**
	 * Return the total number of arraylets for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return the number of arraylets used for an array of dataSizeInBytes bytes
	 */
	MMINLINE uintptr_t
	numArraylets(J9IndexableObject *objPtr)
	{
		return numArraylets(getDataSizeInBytes(objPtr));
	}

	/**
	 * Check the given indexable object is inline contiguous
	 * @param objPtr Pointer to an array object
	 * @return true of array is inline contiguous
	 */
	MMINLINE bool
	isInlineContiguousArraylet(J9IndexableObject *objPtr)
	{
		return (getArrayLayout(objPtr) == InlineContiguous);
	}

	/**
	 * Get the number of discontiguous arraylets for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return the number of arraylets stored external to the spine.
	 */
	MMINLINE uintptr_t
	numExternalArraylets(J9IndexableObject *objPtr)
	{
		ArrayLayout layout = getArrayLayout(objPtr);
		if (layout == InlineContiguous) {
			return 0;
		}

		uintptr_t numberArraylets = numArraylets(objPtr);
		if (layout == Hybrid) {
			/* last arrayoid pointer points into spine (remainder data is contiguous with header) */
			numberArraylets -= 1;
		} else if (layout == Discontiguous) {
			/* Data fits exactly within a whole number of arraylets */
			AssertArrayletIsDiscontiguous(objPtr);
		}

		return numberArraylets;
	}

	/**
	 * Get the total number of bytes consumed by arraylets external to the
	 * given indexable object.
	 * @param objPtr Pointer to an array object
	 * @return the number of bytes consumed external to the spine
	 */
	MMINLINE uintptr_t
	externalArrayletsSize(J9IndexableObject *objPtr)
	{
		uintptr_t numberArraylets = numExternalArraylets(objPtr);

		return numberArraylets * _omrVM->_arrayletLeafSize;
	}

	/**
	 * Determine if the specified array object includes any arraylet leaf pointers.
	 * @param objPtr[in] the object to test
	 * @return true if the array has any internal or external leaf pointers, false otherwise
	 */
	MMINLINE bool
	hasArrayletLeafPointers(J9IndexableObject *objPtr)
	{
		/* Contiguous arraylet has no implicit leaf pointer */
		return !isInlineContiguousArraylet(objPtr);
	}


	/**
	 * Get the size in bytes for the arraylet at index in indexable object objPtr
	 * @param objPtr Pointer to an array object
	 * @param index the index in question.  0<=index<numArraylets(objPtr)
	 * @return the size of the indexth arraylet
	 */
	MMINLINE uintptr_t
	arrayletSize(J9IndexableObject *objPtr, uintptr_t index)
	{
		uintptr_t dataSizeInBytes = getDataSizeInBytes(objPtr);
		uintptr_t numberOfArraylets = numArraylets(dataSizeInBytes);
		return arrayletSize(objPtr, index, dataSizeInBytes, numberOfArraylets);
	}
	
	/**
	 * Get the spine size for the given indexable object
	 * @param objPtr Pointer to an array object
 	 * @param layout layout for array object
 	 * @return The total size in bytes of objPtr's array spine;
	 * 			includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	MMINLINE uintptr_t
	getSpineSize(J9IndexableObject* objPtr, ArrayLayout layout)
	{
		return getSpineSize(J9GC_J9OBJECT_CLAZZ(objPtr, this), layout, getSizeInElements(objPtr));
	}

	/**
	 * Get the spine size for the given indexable object
	 * @param clazzPtr Pointer to the preserved J9Class, used in scavenger
 	 * @param layout layout for array object
 	 * @param size Size of indexable object
	 * @return The total size in bytes of objPtr's array spine;
	 * 			includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	MMINLINE uintptr_t
	getSpineSize(J9Class *clazzPtr, ArrayLayout layout, uintptr_t size)
	{
		return getHeaderSize(clazzPtr, layout) + getSpineSizeWithoutHeader(clazzPtr, layout, size);
	}

	/**
	 * Get the spine size without header for the given indexable object
	 * @param clazzPtr Pointer to the preserved J9Class, used in scavenger
 	 * @param layout layout for array object
 	 * @param size Size of indexable object
	 * @return The size in bytes of objPtr's array spine without header;
	 * 			includes arraylet ptrs, padding and inline data (if any is present)
	 */
	MMINLINE uintptr_t
	getSpineSizeWithoutHeader(J9Class *clazzPtr, ArrayLayout layout, uintptr_t size)
	{
		uintptr_t dataSize = getDataSizeInBytes(clazzPtr, size);
		uintptr_t numberArraylets = numArraylets(dataSize);
		bool alignData = shouldAlignSpineDataSection(clazzPtr);
		return getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
	}

	/**
	 * We can't use memcpy because it may be not atomic for pointers, use this function instead
	 * Copy data in uintptr_t words
	 * If length is not times of uintptr_t one more word is copied
	 * @param destAddr address copy to
	 * @param sourceAddr address copy from
	 * @param lengthInBytes requested size in bytes
	 */
	MMINLINE void
	copyInWords (uintptr_t *destAddr, uintptr_t *sourceAddr, uintptr_t lengthInBytes)
	{
		uintptr_t lengthInWords = lengthInBytes / sizeof(uintptr_t);
		while (lengthInWords--) {
			*destAddr++ = *sourceAddr++;
		}
	}

	/**
	 * Returns the header size of an arraylet for a given layout.
	 * This method is actually private, but since ArrayLayout is public,
	 * it cannot be declared in private section.
	 * @param layout Layout of the arraylet
	 * @return Size of header in bytes
	 */
	MMINLINE uintptr_t
	getHeaderSize(J9Class *clazzPtr, ArrayLayout layout)
	{
		uintptr_t headerSize = contiguousIndexableHeaderSize();
		if (layout != InlineContiguous) {
			headerSize = discontiguousIndexableHeaderSize();
		}
		return headerSize;
	}

	/**
	 * Returns the size of data in an indexable object, in bytes, including leaves and alignment
	 * padding, excluding the header, or UDATA_MAX if an overflow occurs.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Aligned size in bytes excluding the header, or UDATA_MAX if an overflow occurs
	 */
	MMINLINE uintptr_t
	getDataSizeInBytes(J9IndexableObject *arrayPtr)
	{
		return getDataSizeInBytes(J9GC_J9OBJECT_CLAZZ(arrayPtr, this), getSizeInElements(arrayPtr));
	}

	/**
	 * Returns the size of data in an indexable object, in bytes, including leaves and alignment
	 * padding, excluding the header, or UDATA_MAX if an overflow occurs.
	 * @param clazzPtr Pointer to the class of the object
	 * @param numberOfElements size from indexable object
	 * @return Aligned size in bytes excluding the header, or UDATA_MAX if an overflow occurs
	 */
	MMINLINE uintptr_t
	getDataSizeInBytes(J9Class *clazzPtr, uintptr_t numberOfElements)
	{	
		uintptr_t stride = J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		uintptr_t size = numberOfElements * stride;
		uintptr_t alignedSize = UDATA_MAX;
		if ((0 == stride) || ((size / stride) == numberOfElements)) {
			alignedSize = MM_Math::roundToSizeofUDATA(size);
			if (alignedSize < size) {
				alignedSize = UDATA_MAX;
			}
		}
		return alignedSize;
	}

	/**
	 * Get the size from the header for the given indexable object,
	 * assuming it is Contiguous
	 * @param objPtr Pointer to an array object
	 * @return the size
	 */
	MMINLINE uint32_t
	getContiguousArraySize(J9IndexableObject *objPtr)
	{
		return compressObjectReferences()
			? ((J9IndexableObjectContiguousCompressed*)objPtr)->size
			: ((J9IndexableObjectContiguousFull*)objPtr)->size;
	}

	/**
	 * Get the size from the header for the given indexable object,
	 * assuming it is Discontiguous
	 * @param objPtr Pointer to an array object
	 * @return the size
	 */
	MMINLINE uint32_t
	getDiscontiguousArraySize(J9IndexableObject *objPtr)
	{
		return compressObjectReferences()
			? ((J9IndexableObjectDiscontiguousCompressed*)objPtr)->size
			: ((J9IndexableObjectDiscontiguousFull*)objPtr)->size;
	}

	/**
	 * Get the layout for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return the ArrayLayout for objectPtr
	 */
	MMINLINE ArrayLayout
	getArrayLayout(J9IndexableObject *objPtr)
	{
		GC_ArrayletObjectModel::ArrayLayout layout = GC_ArrayletObjectModel::InlineContiguous;
		/* Trivial check for InlineContiguous. */
		if (0 != getContiguousArraySize(objPtr)) {
			return GC_ArrayletObjectModel::InlineContiguous;
		}

		/* Check if the objPtr is in the allowed arraylet range. */
		if (((uintptr_t)objPtr >= (uintptr_t)_arrayletRangeBase) && ((uintptr_t)objPtr < (uintptr_t)_arrayletRangeTop)) {
			J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objPtr, this);
			layout = getArrayletLayout(clazz, getDiscontiguousArraySize(objPtr));
		}
		return layout;
	}

	MMINLINE ArrayLayout
	getArrayletLayout(J9Class* clazz, uintptr_t numberOfElements)
	{
		return getArrayletLayout(clazz, numberOfElements, _largestDesirableArraySpineSize);
	}

	/**
	 * Get the layout of an indexable object given it's class, data size in bytes and the subspace's largestDesirableSpine.
	 * @param clazz The class of the object stored in the array.
	 * @param numberOfElements number of indexed fields
	 * @param largestDesirableSpine The largest desirable spine of the arraylet.
	 */
	ArrayLayout getArrayletLayout(J9Class* clazz, uintptr_t numberOfElements, uintptr_t largestDesirableSpine);

	/**
	 * Perform a safe memcpy of one array to another.
	 * Assumes that destObject and srcObject have the same shape and size.
	 */
	MMINLINE void
	memcpyArray(J9IndexableObject *destObject, J9IndexableObject *srcObject)
	{
		if (InlineContiguous == getArrayLayout(srcObject)) {
			/* assume that destObject must have the same shape! */
			uintptr_t sizeInBytes = getSizeInBytesWithoutHeader(srcObject);
			uintptr_t* srcData = (uintptr_t*)getDataPointerForContiguous(srcObject);
			uintptr_t* destData = (uintptr_t*)getDataPointerForContiguous(destObject);
			copyInWords(destData, srcData, sizeInBytes);
		} else {
			bool const compressed = compressObjectReferences();
			uintptr_t arrayletCount = numArraylets(srcObject);
			fj9object_t *srcArraylets = getArrayoidPointer(srcObject);
			fj9object_t *destArraylets = getArrayoidPointer(destObject);
			for (uintptr_t i = 0; i < arrayletCount; i++) {
				GC_SlotObject srcSlotObject(_omrVM, GC_SlotObject::addToSlotAddress(srcArraylets, i, compressed));
				GC_SlotObject destSlotObject(_omrVM, GC_SlotObject::addToSlotAddress(destArraylets, i, compressed));
				void* srcLeafAddress = srcSlotObject.readReferenceFromSlot();
				void* destLeafAddress = destSlotObject.readReferenceFromSlot();
				

				uintptr_t copySize = _omrVM->_arrayletLeafSize;
				
				if (i == arrayletCount - 1) {
					copySize = arrayletSize(srcObject, i);
				}
				copyInWords((uintptr_t *)destLeafAddress, (uintptr_t *)srcLeafAddress, copySize);
			}
		}
	}

	/**
	 * Perform a memcpy from a primitive array to native memory.
	 * Assumes that the destination is large enough.
	 *
	 * @param destData		the native memory buffer to copy into
	 * @param srcObject		the array to copy from
	 * @param elementIndex	the start index (element-relative) to copy from
	 * @param elementCount	the number of elements of data to copy
	 */
	/*MMINLINE*/ void
	memcpyFromArray(void *destData, J9IndexableObject *srcObject, int32_t elementIndex, int32_t elementCount)
	{
		uintptr_t elementSize = J9ARRAYCLASS_GET_STRIDE(J9GC_J9OBJECT_CLAZZ(srcObject, this));
		if (isInlineContiguousArraylet(srcObject)) {
			// If the data is stored contiguously, then a simple copy is sufficient.
			void* srcData = getDataPointerForContiguous(srcObject);
			switch(elementSize)
			{
			case 0:
				break;

			case 1:
				// byte/boolean
				{
					uint8_t* srcCursor = ((uint8_t*)srcData) + elementIndex;
					uint8_t* destCursor = (uint8_t*)destData;
					int32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 2:
				// short/char
				{
					uint16_t* srcCursor = ((uint16_t*)srcData) + elementIndex;
					uint16_t* destCursor = (uint16_t*)destData;
					int32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 4:
				// int/float
				{
					uint32_t* srcCursor = ((uint32_t*)srcData) + elementIndex;
					uint32_t* destCursor = (uint32_t*)destData;
					uint32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 8:
				// long/double
				{
					U_64* srcCursor = ((U_64*)srcData) + elementIndex;
					U_64* destCursor = (U_64*)destData;
					uint32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;
				
			default :
				// unreachable since we currently do not expect anything other than primitive arrays to be using them.
				{
					AssertBadElementSize();
				}
				break;
			}
		} else {
			bool const compressed = compressObjectReferences();
			fj9object_t *srcArraylets = getArrayoidPointer(srcObject);
			void *outerDestCursor = destData;
			uint32_t outerCount = elementCount;
			uintptr_t arrayletLeafElements = _omrVM->_arrayletLeafSize / elementSize;
			uintptr_t arrayletIndex = elementIndex / arrayletLeafElements;
			uintptr_t arrayletElementOffset = elementIndex % arrayletLeafElements;
			while(outerCount > 0) {
				GC_SlotObject srcSlotObject(_omrVM, GC_SlotObject::addToSlotAddress(srcArraylets, arrayletIndex++, compressed));
				void* srcLeafAddress = srcSlotObject.readReferenceFromSlot();
				uint32_t innerCount = outerCount;
				// Can we fulfill the remainder of the copy from this page?
				if (innerCount + arrayletElementOffset > arrayletLeafElements) {
					// Copy as much as we can
					innerCount = (uint32_t)(arrayletLeafElements - arrayletElementOffset);
				}
				switch(elementSize)
				{
				case 1:
					// byte/boolean
					{
						uint8_t* srcCursor = ((uint8_t*)srcLeafAddress) + arrayletElementOffset;
						uint8_t* destCursor = (uint8_t*)outerDestCursor;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerDestCursor = (void*)destCursor;
						arrayletElementOffset = 0;	// Consumed
					}
					break;

				case 2:
					// short/char
					{
						uint16_t* srcCursor = ((uint16_t*)srcLeafAddress) + arrayletElementOffset;
						uint16_t* destCursor = (uint16_t*)outerDestCursor;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerDestCursor = (void*)destCursor;
						arrayletElementOffset = 0;
					}
					break;

				case 4:
					// int/float
					{
						uint32_t* srcCursor = ((uint32_t*)srcLeafAddress) + arrayletElementOffset;
						uint32_t* destCursor = (uint32_t*)outerDestCursor;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerDestCursor = (void*)destCursor;
						arrayletElementOffset = 0;
					}
					break;

				case 8:
					// long/double
					{
						U_64* srcCursor = ((U_64*)srcLeafAddress) + arrayletElementOffset;
						U_64* destCursor = (U_64*)outerDestCursor;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerDestCursor = (void*)destCursor;
						arrayletElementOffset = 0;
					}
					break;

				default :
					// unreachable since we currently do not expect anything other than primitive arrays to be using them.
					{
						AssertBadElementSize();
					}
					break;
				}
			}
		}
	}

	/**
	 * Perform a memcpy from native memory to a primitive array.
	 * Assumes that the destination is large enough.
	 *
	 * @param destObject	the array to copy to
	 * @param elementIndex	the start index (element-relative) to copy from
	 * @param elementCount	the number of elements of data to copy
	 * @param srcData		the native memory buffer to copy from
	 */
	MMINLINE void
	memcpyToArray(J9IndexableObject *destObject, int32_t elementIndex, int32_t elementCount, void *srcData)
	{
		uintptr_t elementSize = J9ARRAYCLASS_GET_STRIDE(J9GC_J9OBJECT_CLAZZ(destObject, this));
		if (isInlineContiguousArraylet(destObject)) {
			// If the data is stored contiguously, then a simple copy is sufficient.
			void* destData = getDataPointerForContiguous(destObject);
			switch(elementSize)
			{
			case 0:
				break;

			case 1:
				// byte/boolean
				{
					uint8_t* srcCursor = (uint8_t*)srcData;
					uint8_t* destCursor = ((uint8_t*)destData) + elementIndex;
					int32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 2:
				// short/char
				{
					uint16_t* srcCursor = (uint16_t*)srcData;
					uint16_t* destCursor = ((uint16_t*)destData) + elementIndex;
					int32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 4:
				// int/float
				{
					uint32_t* srcCursor = (uint32_t*)srcData;
					uint32_t* destCursor = ((uint32_t*)destData) + elementIndex;
					uint32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 8:
				// long/double
				{
					U_64* srcCursor = (U_64*)srcData;
					U_64* destCursor = ((U_64*)destData) + elementIndex;
					uint32_t count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			default :
				// unreachable since we currently do not expect anything other than primitive arrays to be using them.
				{
					AssertBadElementSize();
				}
				break;
			}
		} else {
			bool const compressed = compressObjectReferences();
			fj9object_t *destArraylets = getArrayoidPointer(destObject);
			void *outerSrcCursor = srcData;
			uint32_t outerCount = elementCount;
			uintptr_t arrayletLeafElements = _omrVM->_arrayletLeafSize / elementSize;
			uintptr_t arrayletIndex = elementIndex / arrayletLeafElements;
			uintptr_t arrayletElementOffset = elementIndex % arrayletLeafElements;
			while(outerCount > 0) {
				GC_SlotObject destSlotObject(_omrVM, GC_SlotObject::addToSlotAddress(destArraylets, arrayletIndex++, compressed));
				void* destLeafAddress = destSlotObject.readReferenceFromSlot();
				uint32_t innerCount = outerCount;
				// Can we fulfill the remainder of the copy from this page?
				if (innerCount + arrayletElementOffset > arrayletLeafElements) {
					// Copy as much as we can
					innerCount = (uint32_t)(arrayletLeafElements - arrayletElementOffset);
				}
				switch(elementSize)
				{
				case 1:
					// byte/boolean
					{
						uint8_t* srcCursor = (uint8_t*)outerSrcCursor;
						uint8_t* destCursor = ((uint8_t*)destLeafAddress) + arrayletElementOffset;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerSrcCursor = (void*)srcCursor;
						arrayletElementOffset = 0;	// Consumed
					}
					break;

				case 2:
					// short/char
					{
						uint16_t* srcCursor = (uint16_t*)outerSrcCursor;
						uint16_t* destCursor = ((uint16_t*)destLeafAddress) + arrayletElementOffset;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerSrcCursor = (void*)srcCursor;
						arrayletElementOffset = 0;
					}
					break;

				case 4:
					// int/float
					{
						uint32_t* srcCursor = (uint32_t*)outerSrcCursor;
						uint32_t* destCursor = ((uint32_t*)destLeafAddress) + arrayletElementOffset;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerSrcCursor = (void*)srcCursor;
						arrayletElementOffset = 0;
					}
					break;

				case 8:
					// long/double
					{
						U_64* srcCursor = (U_64*)outerSrcCursor;
						U_64* destCursor = ((U_64*)destLeafAddress) + arrayletElementOffset;
						outerCount -= innerCount;	// Decrement the outer count before looping
						while(innerCount--) {
							*destCursor++ = *srcCursor++;
						}
						outerSrcCursor = (void*)srcCursor;
						arrayletElementOffset = 0;
					}
					break;
					
				default :
					// unreachable since we currently do not expect anything other than primitive arrays to be using them.
					{
						AssertBadElementSize();
					}
					break;
				}
			}
		}
	}

	/**
	 * Returns the size of an indexable object, in bytes, excluding the header.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Size of object in bytes excluding the header
	 */
	MMINLINE uintptr_t
	getSizeInBytesWithoutHeader(J9IndexableObject *arrayPtr)
	{
		ArrayLayout layout = getArrayLayout(arrayPtr);
		return getSpineSizeWithoutHeader(J9GC_J9OBJECT_CLAZZ(arrayPtr, this), layout, getSizeInElements(arrayPtr));
	}

	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * WARNING: This implementation assumes the arraylet layout is InlineContiguous.
	 * @param clazz Pointer to the preserved indexable object class which may not be intact
	 * @param numberOfElements size from indexable object
	 * @return Size of object in bytes including the header
	 */
	MMINLINE uintptr_t
	getSizeInBytesWithHeader(J9Class *clazz, uintptr_t numberOfElements)
	{
		ArrayLayout layout = InlineContiguous; 
		if(0 == numberOfElements) {
			layout = Discontiguous;
		}
		return getSizeInBytesWithHeader(clazz, layout, numberOfElements);
	}

	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * @param clazz Pointer to the preserved indexable object class which may not be intact
	 * @param flags Indexable object flags
	 * @param numberOfElements size from indexable object
	 * @return Size of object in bytes including the header
	 */
	MMINLINE uintptr_t
	getSizeInBytesWithHeader(J9Class *clazz, ArrayLayout layout, uintptr_t numberOfElements)
	{
		return getSpineSize(clazz, layout, numberOfElements);
	}

	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Size of object in bytes including the header
	 */
	MMINLINE uintptr_t
	getSizeInBytesWithHeader(J9IndexableObject *arrayPtr)
	{
		ArrayLayout layout = getArrayLayout(arrayPtr);
		return getSpineSize(J9GC_J9OBJECT_CLAZZ(arrayPtr, this), layout, getSizeInElements(arrayPtr));
	}

#if defined(J9VM_ENV_DATA64)
	/**
	 * Gets data pointer of a contiguous indexable object.
	 * Helper to get dataAddr field of contiguous indexable objects.
	 *
	 * @return Pointer which points to indexable object data
	 */
	MMINLINE void **
	dataAddrSlotForContiguous(J9IndexableObject *arrayPtr)
	{
		AssertContiguousArrayletLayout(arrayPtr);
		bool const compressed = compressObjectReferences();
		void **dataAddrPtr = NULL;
		if (compressed) {
			dataAddrPtr = &((J9IndexableObjectWithDataAddressContiguousCompressed *)arrayPtr)->dataAddr;
		} else {
			dataAddrPtr = &((J9IndexableObjectWithDataAddressContiguousFull *)arrayPtr)->dataAddr;
		}
		return dataAddrPtr;
	}

	/**
	 * Gets data pointer of a discontiguous indexable object.
	 * Helper to get dataAddr field of discontiguous indexable objects.
	 *
	 * @return Pointer which points to discontiguous indexable object data
	 */
	MMINLINE void **
	dataAddrSlotForDiscontiguous(J9IndexableObject *arrayPtr)
	{
		/* If double mapping is enabled only, arraylet will have a discontiguous layout.
		 * If sparse-heap is enabled, arraylet will have a contiguous layout. For now
		 * we can't simply Assert only the discontiguous case because there could also
		 * exist hybrid arraylets (which will be dicontinued in the future) */
		bool const compressed = compressObjectReferences();
		void **dataAddrPtr = NULL;
		if (compressed) {
			dataAddrPtr = &((J9IndexableObjectWithDataAddressDiscontiguousCompressed *)arrayPtr)->dataAddr;
		} else {
			dataAddrPtr = &((J9IndexableObjectWithDataAddressDiscontiguousFull *)arrayPtr)->dataAddr;
		}
		return dataAddrPtr;
	}

	/**
	 * Sets data pointer of a contiguous indexable object.
	 * Sets the data pointer of a contiguous indexable object; in this case
	 * dataAddr will point directly into the data right after dataAddr field
	 * (data resides in heap).
	 *
	 * @param arrayPtr      Pointer to the indexable object whose size is required
	 */
	MMINLINE void
	setDataAddrForContiguous(J9IndexableObject *arrayPtr)
	{
		void **dataAddrPtr = dataAddrSlotForContiguous(arrayPtr);
		void *dataAddr = (void *)((uintptr_t)arrayPtr + contiguousIndexableHeaderSize());
		*dataAddrPtr = dataAddr;
	}

	/**
	 * Sets the data pointer of a contiguous indexable object.
	 *
	 * @param arrayPtr      Pointer to the indexable object whose size is required
	 * @param address       Pointer which points to indexable object data
	 */
	MMINLINE void
	setDataAddrForContiguous(J9IndexableObject *arrayPtr, void *address)
	{
		void *calculatedDataAddr = address;
		void **dataAddrPtr = dataAddrSlotForContiguous(arrayPtr);

		*dataAddrPtr = calculatedDataAddr;
	}

	/**
	 * Sets data pointer of a discontiguous indexable object.
	 * Sets the data pointer of a discontiguous indexable object; in this case
	 * dataAddr will point to the contiguous representation of the data
	 * which resides outside the heap (assuming double map/sparse-heap is enabled).
	 * In case double map is disabled, the dataAddr will point to the first arrayoid
	 * of the discontiguous indexable object, which also resides right after dataAddr
	 * field.
	 *
	 * @param arrayPtr      Pointer to the indexable object whose size is required
	 * @param address       Pointer which points to indexable object data
	 */
	MMINLINE void
	setDataAddrForDiscontiguous(J9IndexableObject *arrayPtr, void *address)
	{
		/* If double mapping is enabled only, arraylet will have a discontiguous layout.
		 * If sparse-heap is enabled, arraylet will have a contiguous layout. For now
		 * we can't simply Assert only the discontiguous case because there could also
		 * exist hybrid arraylets (which will be dicontinued in the future) */
		void *calculatedDataAddr = address;
		void **dataAddrPtr = dataAddrSlotForDiscontiguous(arrayPtr);

		*dataAddrPtr = calculatedDataAddr;
	}

	/**
	 * Asserts that an indexable object pointer is indeed an indexable object
	 *
	 * @param arrayPtr      Pointer to the indexable object
	 */
	void AssertArrayPtrIsIndexable(J9IndexableObject *arrayPtr);

	/**
	 * Returns data pointer associated with a contiguous Indexable object.
	 * Data pointer will always be pointing at the arraylet data. In this
	 * case the data pointer will be pointing to address immediately after
	 * the header.
	 *
	 * @param arrayPtr      Pointer to the indexable object whose size is required
	 * @return data address associated with the Indexable object
	 */
	MMINLINE void *
	getDataAddrForContiguous(J9IndexableObject *arrayPtr)
	{
		void *dataAddr = NULL;
		if (_isIndexableDataAddrPresent) {
			dataAddr = *dataAddrSlotForContiguous(arrayPtr);
		}
		return dataAddr;
	}

	/**
	 * Returns data pointer associated with the Indexable object.
	 * Data pointer will always be pointing at the arraylet data. In all
	 * cases the data pointer will be pointing to address immediately after
	 * the header,
	 *
	 * @param arrayPtr      Pointer to the indexable object whose size is required
	 * @return data address associated with the Indexable object
	 */
	MMINLINE void *
	getDataAddrForIndexableObject(J9IndexableObject *arrayPtr)
	{
		return (InlineContiguous == getArrayLayout(arrayPtr))
			? getDataAddrForContiguous(arrayPtr)
			/* DataAddr is only used for off-heap enabled case, return NULL for disContiguous layout */
			: NULL;
	}

	/**
	 * Checks that the dataAddr field of the indexable object is correct.
	 * this method is supposed to be called only if offheap is enabled.
	 *
	 * @param arrayPtr      Pointer to the indexable object
	 * @param isValidDataAddrForOffHeapObject	Boolean to determine whether the given indexable object is off heap
	 * @return if the dataAddr field of the indexable object is correct
	 */
	MMINLINE bool
	isValidDataAddr(J9IndexableObject *arrayPtr, bool isValidDataAddrForOffHeapObject)
	{
		bool isValidDataAddress = true;
		if (_isIndexableDataAddrPresent) {
			void *dataAddr = getDataAddrForIndexableObject(arrayPtr);
			isValidDataAddress = isValidDataAddr(arrayPtr, dataAddr, isValidDataAddrForOffHeapObject);
		}
		return isValidDataAddress;
	}

	/**
	 * Checks that the dataAddr field of the indexable object is correct.
	 * this method is supposed to be called only if offheap is enabled
	 *
	 * @param arrayPtr      Pointer to the indexable object
	 * @param isValidDataAddrForOffHeapObject	Boolean to determine whether the given indexable object is off heap
	 * @return if the dataAddr field of the indexable object is correct
	 */
	MMINLINE bool
	isValidDataAddr(J9IndexableObject *arrayPtr, void *dataAddr, bool isValidDataAddrForOffHeapObject)
	{
		bool isValidDataAddress = false;
		uintptr_t dataSizeInBytes = getDataSizeInBytes(arrayPtr);

		if (0 == dataSizeInBytes) {
			isValidDataAddress = ((dataAddr == NULL) || (dataAddr == (void *)((uintptr_t)arrayPtr + discontiguousIndexableHeaderSize())));
		} else if (dataSizeInBytes < _omrVM->_arrayletLeafSize) {
			isValidDataAddress = (dataAddr == (void *)((uintptr_t)arrayPtr + contiguousIndexableHeaderSize()));
		} else {
			isValidDataAddress = isValidDataAddrForOffHeapObject;
		}

		return isValidDataAddress;
	}
#endif /* defined(J9VM_ENV_DATA64) */

	/**
	 * External fixup dataAddr API to update pointer of indexable objects.
	 * Used in concurrent GCs in case of mutator and GC thread races.
	 * Sets the dataAddr of either a contiguous or discomtiguous indexable
	 * object.
	 *
	 * @param forwardedHeader Forwarded header of arrayPtr
	 * @param destinationArray Pointer to the indexable object whose size is required
	 */
	MMINLINE void
	fixupDataAddr(MM_ForwardedHeader *forwardedHeader, J9IndexableObject *destinationArray)
	{
#if defined(J9VM_ENV_DATA64)
		AssertVirtualLargeObjectHeapEnabled();

		if (InlineContiguous == getPreservedArrayLayout(forwardedHeader)) {
			void *dataAddr = getDataAddrForContiguous(destinationArray);
			/* forwardedHeader should not be treated as an object (since it does not have valid class).*/
			if (shouldFixupDataAddrForContiguous(forwardedHeader, dataAddr)) {
				setDataAddrForContiguous(destinationArray);
			}
		}
#endif /* defined(J9VM_ENV_DATA64) */
	}

	/**
	 * Extract the class pointer from an unforwarded object.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param[in] pointer to forwardedHeader the MM_ForwardedHeader instance encapsulating the object
	 * @return pointer to the J9Class from the object encapsulated by forwardedHeader
	 * @see MM_ForwardingHeader::isForwardedObject()
	 */
	MMINLINE J9Class *
	getPreservedClass(MM_ForwardedHeader *forwardedHeader)
	{
		return (J9Class *)((uintptr_t)(forwardedHeader->getPreservedSlot()) & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK);
	}

	/**
	 * Get the preserved hashcode offset of forwarded header.
	 *
	 * @param[in]   Pointer to forwardedHeader the MM_ForwardedHeader instance encapsulating the object
	 */
	MMINLINE uintptr_t
	getPreservedHashcodeOffset(MM_ForwardedHeader *forwardedHeader)
	{
		J9Class *clazz = getPreservedClass(forwardedHeader);
		return getHashcodeOffset(clazz, getPreservedArrayLayout(forwardedHeader), getPreservedIndexableSize(forwardedHeader));
	}

	/**
	 * Extract the size (as getSizeInElements()) from an unforwarded object
	 *
	 * This method will assert if the object is not indexable or has been marked as forwarded.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return the size (#elements) of the array encapsulated by forwardedHeader
	 * @see MM_ForwardingHeader::isForwardedObject()
	 */
	MMINLINE uint32_t
	getPreservedIndexableSize(MM_ForwardedHeader *forwardedHeader)
	{
		/* Asserts if object is indeed indexable */
		ForwardedHeaderAssert(J9GC_CLASS_IS_ARRAY(getPreservedClass(forwardedHeader)));

		/* in compressed headers, the size of the object is stored in the low-order half of the uintptr_t read when we read clazz
		 * so read it from there instead of the heap (since the heap copy would have been over-written by the forwarding
		 * pointer if another thread copied the object underneath us). In non-compressed, this field should still be readable
		 * out of the heap.
		 */
		uint32_t size = 0;
#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			size = forwardedHeader->getPreservedOverlap();
		} else
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
		{
			size = ((J9IndexableObjectContiguousFull *)forwardedHeader->getObject())->size;
		}

		if (0 == size) {
			/* Discontiguous */
			size = getDiscontiguousArraySize((J9IndexableObject *)forwardedHeader->getObject());
		}

		return size;
	}

	/**
	 * Extract the array layout from preserved info in Forwarded header
	 * (this mimics getArrayLayout())
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return the ArrayLayout for the forwarded object
	 */
	MMINLINE ArrayLayout
	getPreservedArrayLayout(MM_ForwardedHeader *forwardedHeader)
	{
		J9Class *clazz = getPreservedClass(forwardedHeader);
		/* Asserts if object is indeed indexable */
		ForwardedHeaderAssert(J9GC_CLASS_IS_ARRAY(clazz));

		ArrayLayout layout = InlineContiguous;
		uint32_t size = 0;
#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			size = forwardedHeader->getPreservedOverlap();
		} else
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
		{
			size = ((J9IndexableObjectContiguousFull *)forwardedHeader->getObject())->size;
		}

		if (0 == size) {
			/* we know we are dealing with heap object, so we don't need to check against _arrayletRangeBase/Top, like getArrayLayout does */
			uintptr_t numberOfElements = (uintptr_t)getPreservedIndexableSize(forwardedHeader);
			layout = getArrayletLayout(clazz, numberOfElements);
		}

		return layout;
	}

	/**
	 * Calculates object size in bytes with header and object's hashcode offset
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[out] hashcodeOffset hashcode offset of object being moved
	 *
	 * @return object size in bytes with header
	 */
	MMINLINE uintptr_t
	calculateObjectSizeAndHashcode(MM_ForwardedHeader *forwardedHeader, uintptr_t *hashcodeOffset)
	{
		J9Class* clazz = getPreservedClass(forwardedHeader);
		uintptr_t numberOfElements = (uintptr_t)getPreservedIndexableSize(forwardedHeader);
		ArrayLayout layout = getArrayletLayout(clazz, numberOfElements);
		*hashcodeOffset = getHashcodeOffset(clazz, layout, numberOfElements);
		return getSizeInBytesWithHeader(clazz, layout, numberOfElements);
	}

	/**
	 * Returns the header size of a given indexable object. The arraylet layout is determined base on "small" size.
	 * @param arrayPtr Ptr to an array for which header size will be returned
	 * @return Size of header in bytes
	 */
	MMINLINE uintptr_t
	getHeaderSize(J9IndexableObject *arrayPtr)
	{
		uintptr_t size = (compressObjectReferences() ? ((J9IndexableObjectContiguousCompressed *)arrayPtr)->size : ((J9IndexableObjectContiguousFull *)arrayPtr)->size);
		return (0 == size) ? _discontiguousIndexableHeaderSize : _contiguousIndexableHeaderSize;
	}

	/**
	 * Returns the address of first arraylet leaf slot in the spine
	 * @param arrayPtr Ptr to an array
	 * @return Arrayoid ptr
	 */
	MMINLINE fj9object_t *
	getArrayoidPointer(J9IndexableObject *arrayPtr)
	{
		return (fj9object_t *)((uintptr_t)arrayPtr + _discontiguousIndexableHeaderSize);
	}

	/**
	 * Returns the address of first data slot in the array.
	 *
	 * @param arrayPtr Pointer to the contigous indexable object whose dataPointer we are trying to access
	 * @return Address of first data slot in the array
	 */
	MMINLINE void *
	getDataPointerForContiguous(J9IndexableObject *arrayPtr)
	{
		void *dataAddr = (void *)((uintptr_t)arrayPtr + contiguousIndexableHeaderSize());
#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
		if (isVirtualLargeObjectHeapEnabled()) {
			dataAddr = *dataAddrSlotForContiguous(arrayPtr);
		}
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
		return dataAddr;
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * WARNING: This implementation assumes the arraylet layout is InlineContiguous.
	 * @param clazzPtr Pointer to the class of the object
	 * @param numberOfElements size from indexable object
	 * @return offset of the hashcode slot
	 */
	MMINLINE uintptr_t
	getHashcodeOffset(J9Class *clazzPtr, uintptr_t numberOfElements)
	{
		ArrayLayout layout = InlineContiguous; 
		if(0 == numberOfElements) {
			layout = Discontiguous;
		}
		return getHashcodeOffset(clazzPtr, layout, numberOfElements);
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param clazzPtr Pointer to the class of the object
	 * @param layout The layout of the indexable object
	 * @param numberOfElements size from indexable object
	 * @return offset of the hashcode slot
	 */
	MMINLINE uintptr_t
	getHashcodeOffset(J9Class *clazzPtr, ArrayLayout layout, uintptr_t numberOfElements)
	{
		/* Don't call getDataSizeInBytes() since that rounds up to uintptr_t. */
		uintptr_t dataSize = numberOfElements * J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		uintptr_t numberOfArraylets = numArraylets(dataSize);
		bool alignData = shouldAlignSpineDataSection(clazzPtr);
		uintptr_t spineSize = getSpineSize(clazzPtr, layout, numberOfArraylets, dataSize, alignData);
		return MM_Math::roundToSizeofU32(spineSize);
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param arrayPtr Pointer to the array
	 * @return offset of the hashcode slot
	 */
	MMINLINE uintptr_t
	getHashcodeOffset(J9IndexableObject *arrayPtr)
	{
		ArrayLayout layout = getArrayLayout(arrayPtr);
		return getHashcodeOffset(J9GC_J9OBJECT_CLAZZ(arrayPtr, this), layout, getSizeInElements(arrayPtr));
	}
	
	MMINLINE void
	expandArrayletSubSpaceRange(MM_MemorySubSpace * subSpace, void * rangeBase, void * rangeTop, uintptr_t largestDesirableArraySpineSize)
	{
		GC_ArrayletObjectModelBase::expandArrayletSubSpaceRange(subSpace, rangeBase, rangeTop, largestDesirableArraySpineSize);
	}

	/**
	 * Updates leaf pointers that point to an address located within the indexable object.  For example,
	 * when the array layout is either inline continuous or hybrid, there will be leaf pointers that point
	 * to data that is contained within the indexable object.  These internal leaf pointers are updated by
	 * calculating their offset within the source arraylet, then updating the destination arraylet pointers
	 * to use the same offset.
	 *
	 * @param destinationPtr Pointer to the new indexable object
	 * @param sourcePtr      Pointer to the original indexable object that was copied
	 */
	void fixupInternalLeafPointersAfterCopy(J9IndexableObject *destinationPtr, J9IndexableObject *sourcePtr);

	/**
	 * Check if the arraylet data is adjacent to the header.
	 * Mostly used for the detection of camouflaged contiguous arrays that exist with virtualLargeObjectHeap enabled.
	 *
	 * @param dataSizeInBytes the size of data in an indexable object, in bytes, including leaves and alignment padding
	 * @return true if the arraylet data is adjacent to the header, false otherwise
	 */
	bool isDataAdjacentToHeader(J9IndexableObject *arrayPtr);

	/**
	 * Check if the arraylet data is adjacent to the header.
	 * Mostly used for the detection of camouflaged contiguous arrays that exist with virtualLargeObjectHeap enabled.
	 *
	 * @param dataSizeInBytes the size of data in an indexable object, in bytes, including leaves and alignment padding
	 * @return true if based on the value of dataSizeInBytes, the arraylet data is adjacent to the header, false otherwise
	 */
	bool isDataAdjacentToHeader(uintptr_t dataSizeInBytes);

	/**
	 * Check if the data address for the contiguous indexable object should be fixed up.
	 *
	 * @param extensions pointer to MM_GCExtensionsBase
	 * @param arrayPtr pointer to J9IndexableObject
	 * @return true if we should fixup the data address of the indexable object
	 */
	bool shouldFixupDataAddrForContiguous(J9IndexableObject *arrayPtr);

	/**
	 * Check if the data address for the contiguous indexable object should be fixed up.
	 *
	 * @param extensions pointer to MM_GCExtensionsBase
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * 		  forwardedHeader should not be treated as an object (since it does not have valid class).
	 * @param dataAddr data address in the indexable object
	 * @return true if we should fixup the data address of the indexable object
	 */
	bool shouldFixupDataAddrForContiguous(void *forwardedHeader, void *dataAddr);

	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel
	 * 
	 * @return true on success, false on failure
	 */
	bool initialize(MM_GCExtensionsBase *extensions);
	
	/**
	 * Tear down the receiver
	 */
	void tearDown(MM_GCExtensionsBase *extensions);

	/**
	 * Asserts that an Arraylet has indeed discontiguous layout
	 */
	void AssertArrayletIsDiscontiguous(J9IndexableObject *objPtr);

	/**
	 * Asserts that an Arraylet has true contiguous layout
	 */
	void AssertContiguousArrayletLayout(J9IndexableObject *objPtr);

	/**
	 * Asserts that an Arraylet has true discontiguous layout
	 */
	void AssertDiscontiguousArrayletLayout(J9IndexableObject *objPtr);

	/**
	 * Asserts unreachable code if either off-heap or double-mapping is enabled.
	 */
	void AssertContiguousArrayDataUnreachable();

	/**
	 * Asserts that off-heap is enabled.
	 */
	void AssertVirtualLargeObjectHeapEnabled();
};
#endif /* ARRAYLETOBJECTMODEL_ */
