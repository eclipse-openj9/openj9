
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if defined(J9VM_GC_ARRAYLETS)

#include "ArrayletObjectModelBase.hpp"
#include "Bits.hpp"
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
	MMINLINE UDATA
	getSpineSizeWithoutHeader(ArrayLayout layout, UDATA numberArraylets, UDATA dataSize, bool alignData)
	{
		return GC_ArrayletObjectModelBase::getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
	}
public:
	/* forward declare methods from parent class to avoid namespace issues */
	MMINLINE UDATA
	arrayletSize(J9IndexableObject *objPtr, UDATA index, UDATA dataSizeInBytes, UDATA numberOfArraylets)
	{
		return GC_ArrayletObjectModelBase::arrayletSize(objPtr, index, dataSizeInBytes, numberOfArraylets);
	}
	MMINLINE UDATA
	numArraylets(UDATA unadjustedDataSizeInBytes)
	{
		return GC_ArrayletObjectModelBase::numArraylets(unadjustedDataSizeInBytes);
	}

	/**
	 * Get the spine size for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return The total size in bytes of objPtr's array spine;
	 * 			includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	MMINLINE UDATA
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
	MMINLINE UDATA
	getSpineSize(J9Class *clazzPtr, ArrayLayout layout, UDATA numberArraylets, UDATA dataSize, bool alignData)
	{
		UDATA result = getHeaderSize(clazzPtr, layout) + getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
		return result;
	}

	/**
	 * Return the total number of arraylets for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return the number of arraylets used for an array of dataSizeInBytes bytes
	 */
	MMINLINE UDATA
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
	MMINLINE UDATA
	numExternalArraylets(J9IndexableObject *objPtr)
	{
		ArrayLayout layout = getArrayLayout(objPtr);
		if (layout == InlineContiguous) {
			return 0;
		}

		UDATA numberArraylets = numArraylets(objPtr);
		if (layout == Hybrid) {
			/* last arrayoid pointer points into spine (remainder data is contiguous with header) */
			numberArraylets -= 1;
		} else if (layout == Discontiguous) {
			/* last arrayoid pointer is NULL if data fits exactly within a whole number of arraylets */
			if (0 == (getDataSizeInBytes(objPtr) & (_omrVM->_arrayletLeafSize - 1))) {
				numberArraylets -= 1;
			}
		}

		return numberArraylets;
	}

	/**
	 * Get the total number of bytes consumed by arraylets external to the
	 * given indexable object.
	 * @param objPtr Pointer to an array object
	 * @return the number of bytes consumed external to the spine
	 */
	MMINLINE UDATA
	externalArrayletsSize(J9IndexableObject *objPtr)
	{
		UDATA numberArraylets = numExternalArraylets(objPtr);

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
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		/* Contiguous arraylet has no implicit leaf pointer */
		return !isInlineContiguousArraylet(objPtr);
#else /* J9VM_GC_HYBRID_ARRAYLETS */
		return true;
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
	}


	/**
	 * Get the size in bytes for the arraylet at index in indexable object objPtr
	 * @param objPtr Pointer to an array object
	 * @param index the index in question.  0<=index<numArraylets(objPtr)
	 * @return the size of the indexth arraylet
	 */
	MMINLINE UDATA
	arrayletSize(J9IndexableObject *objPtr, UDATA index)
	{
		UDATA dataSizeInBytes = getDataSizeInBytes(objPtr);
		UDATA numberOfArraylets = numArraylets(dataSizeInBytes);
		return arrayletSize(objPtr, index, dataSizeInBytes, numberOfArraylets);
	}
	
	/**
	 * Get the spine size for the given indexable object
	 * @param objPtr Pointer to an array object
 	 * @param layout layout for array object
 	 * @return The total size in bytes of objPtr's array spine;
	 * 			includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	MMINLINE UDATA
	getSpineSize(J9IndexableObject* objPtr, ArrayLayout layout)
	{
		return getSpineSize(J9GC_J9OBJECT_CLAZZ(objPtr), layout, getSizeInElements(objPtr));
	}

	/**
	 * Get the spine size for the given indexable object
	 * @param clazzPtr Pointer to the preserved J9Class, used in scavenger
 	 * @param layout layout for array object
 	 * @param size Size of indexable object
	 * @return The total size in bytes of objPtr's array spine;
	 * 			includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	MMINLINE UDATA
	getSpineSize(J9Class *clazzPtr, ArrayLayout layout, UDATA size)
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
	MMINLINE UDATA
	getSpineSizeWithoutHeader(J9Class *clazzPtr, ArrayLayout layout, UDATA size)
	{
		UDATA dataSize = getDataSizeInBytes(clazzPtr, size);
		UDATA numberArraylets = numArraylets(dataSize);
		bool alignData = shouldAlignSpineDataSection(clazzPtr);
		return getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
	}

	/**
	 * We can't use memcpy because it may be not atomic for pointers, use this function instead
	 * Copy data in UDATA words
	 * If length is not times of UDATA one more word is copied
	 * @param destAddr address copy to
	 * @param sourceAddr address copy from
	 * @param lengthInBytes requested size in bytes
	 */
	MMINLINE void
	copyInWords (UDATA *destAddr, UDATA *sourceAddr, UDATA lengthInBytes)
	{
		UDATA lengthInWords = lengthInBytes / sizeof(UDATA);
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
	MMINLINE UDATA
	getHeaderSize(J9Class *clazzPtr, ArrayLayout layout)
	{
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		UDATA headerSize = sizeof(J9IndexableObjectContiguous);
		if (layout != InlineContiguous) {
			headerSize = sizeof(J9IndexableObjectDiscontiguous);
		}
#else
		UDATA headerSize = sizeof(J9IndexableObjectDiscontiguous);
#endif
		return headerSize;
	}

	/**
	 * Returns the size of data in an indexable object, in bytes, including leaves and alignment
	 * padding, excluding the header, or UDATA_MAX if an overflow occurs.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Aligned size in bytes excluding the header, or UDATA_MAX if an overflow occurs
	 */
	MMINLINE UDATA
	getDataSizeInBytes(J9IndexableObject *arrayPtr)
	{
		return getDataSizeInBytes(J9GC_J9OBJECT_CLAZZ(arrayPtr), getSizeInElements(arrayPtr));
	}

	/**
	 * Returns the size of data in an indexable object, in bytes, including leaves and alignment
	 * padding, excluding the header, or UDATA_MAX if an overflow occurs.
	 * @param clazzPtr Pointer to the class of the object
	 * @param numberOfElements size from indexable object
	 * @return Aligned size in bytes excluding the header, or UDATA_MAX if an overflow occurs
	 */
	MMINLINE UDATA
	getDataSizeInBytes(J9Class *clazzPtr, UDATA numberOfElements)
	{	
		UDATA stride = J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		UDATA size = numberOfElements * stride;
		UDATA alignedSize = UDATA_MAX;
		if ((size / stride) == numberOfElements) {
			alignedSize = MM_Math::roundToSizeofUDATA(size);
			if (alignedSize < size) {
				alignedSize = UDATA_MAX;
			}
		}
		return alignedSize;
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
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		/* Trivial check for InlineContiguous. */
		if (0 != ((J9IndexableObjectContiguous*)objPtr)->size) {
			return GC_ArrayletObjectModel::InlineContiguous;
		}
#endif /* J9VM_GC_HYBRID_ARRAYLETS */

		/* Check if the objPtr is in the allowed arraylet range. */
		if (((UDATA)objPtr >= (UDATA)_arrayletRangeBase) && ((UDATA)objPtr < (UDATA)_arrayletRangeTop)) {
			UDATA dataSizeInBytes = getDataSizeInBytes(objPtr);
			J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objPtr);
			layout = getArrayletLayout(clazz, dataSizeInBytes);
		}
		return layout;
	}

	MMINLINE ArrayLayout
	getArrayletLayout(J9Class* clazz, UDATA dataSizeInBytes)
	{
		return getArrayletLayout(clazz, dataSizeInBytes, _largestDesirableArraySpineSize);
	}

	/**
	 * Get the layout of an indexable object given it's class, data size in bytes and the subspace's largestDesirableSpine.
	 * @param clazz The class of the object stored in the array.
	 * @param dataSizeInBytes the size in bytes of the data of the array.
	 * @param largestDesirableSpine The largest desirable spine of the arraylet.
	 */
	ArrayLayout getArrayletLayout(J9Class* clazz, UDATA dataSizeInBytes, UDATA largestDesirableSpine);

	/**
	 * Perform a safe memcpy of one array to another.
	 * Assumes that destObject and srcObject have the same shape and size.
	 */
	MMINLINE void
	memcpyArray(J9IndexableObject *destObject, J9IndexableObject *srcObject)
	{
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (InlineContiguous == getArrayLayout(srcObject)) {
			/* assume that destObject must have the same shape! */
			UDATA sizeInBytes = getSizeInBytesWithoutHeader(srcObject);
			UDATA* srcData = (UDATA*)getDataPointerForContiguous(srcObject);
			UDATA* destData = (UDATA*)getDataPointerForContiguous(destObject);
			copyInWords(destData, srcData, sizeInBytes);
		} else
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
		{
			UDATA arrayletCount = numArraylets(srcObject);
			fj9object_t *srcArraylets = getArrayoidPointer(srcObject);
			fj9object_t *destArraylets = getArrayoidPointer(destObject);
			for (UDATA i = 0; i < arrayletCount; i++) {
				GC_SlotObject srcSlotObject(_omrVM, &srcArraylets[i]);
				GC_SlotObject destSlotObject(_omrVM, &destArraylets[i]);
				void* srcLeafAddress = srcSlotObject.readReferenceFromSlot();
				void* destLeafAddress = destSlotObject.readReferenceFromSlot();
				

				UDATA copySize = _omrVM->_arrayletLeafSize;
				
				if (i == arrayletCount - 1) {
					copySize = arrayletSize(srcObject, i);
				}
				copyInWords((UDATA *)destLeafAddress, (UDATA *)srcLeafAddress, copySize);
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
	memcpyFromArray(void *destData, J9IndexableObject *srcObject, I_32 elementIndex, I_32 elementCount)
	{
		UDATA elementSize = J9ARRAYCLASS_GET_STRIDE(J9GC_J9OBJECT_CLAZZ(srcObject));
		if (isInlineContiguousArraylet(srcObject)) {
			// If the data is stored contiguously, then a simple copy is sufficient.
			void* srcData = getDataPointerForContiguous(srcObject);
			switch(elementSize)
			{
			case 1:
				// byte/boolean
				{
					U_8* srcCursor = ((U_8*)srcData) + elementIndex;
					U_8* destCursor = (U_8*)destData;
					I_32 count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 2:
				// short/char
				{
					U_16* srcCursor = ((U_16*)srcData) + elementIndex;
					U_16* destCursor = (U_16*)destData;
					I_32 count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 4:
				// int/float
				{
					U_32* srcCursor = ((U_32*)srcData) + elementIndex;
					U_32* destCursor = (U_32*)destData;
					U_32 count = elementCount;
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
					U_32 count = elementCount;
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
			fj9object_t *srcArraylets = getArrayoidPointer(srcObject);
			void *outerDestCursor = destData;
			U_32 outerCount = elementCount;
			UDATA arrayletLeafElements = _omrVM->_arrayletLeafSize / elementSize;
			UDATA arrayletIndex = elementIndex / arrayletLeafElements;
			UDATA arrayletElementOffset = elementIndex % arrayletLeafElements;
			while(outerCount > 0) {
				GC_SlotObject srcSlotObject(_omrVM, &srcArraylets[arrayletIndex++]);
				void* srcLeafAddress = srcSlotObject.readReferenceFromSlot();
				U_32 innerCount = outerCount;
				// Can we fulfill the remainder of the copy from this page?
				if (innerCount + arrayletElementOffset > arrayletLeafElements) {
					// Copy as much as we can
					innerCount = (U_32)(arrayletLeafElements - arrayletElementOffset);
				}
				switch(elementSize)
				{
				case 1:
					// byte/boolean
					{
						U_8* srcCursor = ((U_8*)srcLeafAddress) + arrayletElementOffset;
						U_8* destCursor = (U_8*)outerDestCursor;
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
						U_16* srcCursor = ((U_16*)srcLeafAddress) + arrayletElementOffset;
						U_16* destCursor = (U_16*)outerDestCursor;
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
						U_32* srcCursor = ((U_32*)srcLeafAddress) + arrayletElementOffset;
						U_32* destCursor = (U_32*)outerDestCursor;
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
	memcpyToArray(J9IndexableObject *destObject, I_32 elementIndex, I_32 elementCount, void *srcData)
	{
		UDATA elementSize = J9ARRAYCLASS_GET_STRIDE(J9GC_J9OBJECT_CLAZZ(destObject));
		if (isInlineContiguousArraylet(destObject)) {
			// If the data is stored contiguously, then a simple copy is sufficient.
			void* destData = getDataPointerForContiguous(destObject);
			switch(elementSize)
			{
			case 1:
				// byte/boolean
				{
					U_8* srcCursor = (U_8*)srcData;
					U_8* destCursor = ((U_8*)destData) + elementIndex;
					I_32 count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 2:
				// short/char
				{
					U_16* srcCursor = (U_16*)srcData;
					U_16* destCursor = ((U_16*)destData) + elementIndex;
					I_32 count = elementCount;
					while(count--) {
						*destCursor++ = *srcCursor++;
					}
				}
				break;

			case 4:
				// int/float
				{
					U_32* srcCursor = (U_32*)srcData;
					U_32* destCursor = ((U_32*)destData) + elementIndex;
					U_32 count = elementCount;
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
					U_32 count = elementCount;
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
			fj9object_t *destArraylets = getArrayoidPointer(destObject);
			void *outerSrcCursor = srcData;
			U_32 outerCount = elementCount;
			UDATA arrayletLeafElements = _omrVM->_arrayletLeafSize / elementSize;
			UDATA arrayletIndex = elementIndex / arrayletLeafElements;
			UDATA arrayletElementOffset = elementIndex % arrayletLeafElements;
			while(outerCount > 0) {
				GC_SlotObject destSlotObject(_omrVM, &destArraylets[arrayletIndex++]);
				void* destLeafAddress = destSlotObject.readReferenceFromSlot();
				U_32 innerCount = outerCount;
				// Can we fulfill the remainder of the copy from this page?
				if (innerCount + arrayletElementOffset > arrayletLeafElements) {
					// Copy as much as we can
					innerCount = (U_32)(arrayletLeafElements - arrayletElementOffset);
				}
				switch(elementSize)
				{
				case 1:
					// byte/boolean
					{
						U_8* srcCursor = (U_8*)outerSrcCursor;
						U_8* destCursor = ((U_8*)destLeafAddress) + arrayletElementOffset;
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
						U_16* srcCursor = (U_16*)outerSrcCursor;
						U_16* destCursor = ((U_16*)destLeafAddress) + arrayletElementOffset;
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
						U_32* srcCursor = (U_32*)outerSrcCursor;
						U_32* destCursor = ((U_32*)destLeafAddress) + arrayletElementOffset;
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
	MMINLINE UDATA
	getSizeInBytesWithoutHeader(J9IndexableObject *arrayPtr)
	{
		ArrayLayout layout = getArrayLayout(arrayPtr);
		return getSpineSizeWithoutHeader(J9GC_J9OBJECT_CLAZZ(arrayPtr), layout, getSizeInElements(arrayPtr));
	}

	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * WARNING: This implementation assumes the arraylet layout is InlineContiguous.
	 * @param clazz Pointer to the preserved indexable object class which may not be intact
	 * @param numberOfElements size from indexable object
	 * @return Size of object in bytes including the header
	 */
	MMINLINE UDATA
	getSizeInBytesWithHeader(J9Class *clazz, UDATA numberOfElements)
	{
		ArrayLayout layout = InlineContiguous; 
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		if(0 == numberOfElements) {
			layout = Discontiguous;
		}
#endif /* defined(J9VM_GC_HYBRID_ARRAYLETS) */
		return getSizeInBytesWithHeader(clazz, layout, numberOfElements);
	}

	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * @param clazz Pointer to the preserved indexable object class which may not be intact
	 * @param flags Indexable object flags
	 * @param numberOfElements size from indexable object
	 * @return Size of object in bytes including the header
	 */
	MMINLINE UDATA
	getSizeInBytesWithHeader(J9Class *clazz, ArrayLayout layout, UDATA numberOfElements)
	{
		return getSpineSize(clazz, layout, numberOfElements);
	}

	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Size of object in bytes including the header
	 */
	MMINLINE UDATA
	getSizeInBytesWithHeader(J9IndexableObject *arrayPtr)
	{
		ArrayLayout layout = getArrayLayout(arrayPtr);
		return getSpineSize(J9GC_J9OBJECT_CLAZZ(arrayPtr), layout, getSizeInElements(arrayPtr));
	}

	/**
	 * Returns the header size of a given indexable object. The arraylet layout is determined base on "small" size.
	 * @param arrayPtr Ptr to an array for which header size will be returned
	 * @return Size of header in bytes
	 */
	MMINLINE UDATA
	getHeaderSize(J9IndexableObject *arrayPtr)
	{
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		UDATA size = ((J9IndexableObjectContiguous *)arrayPtr)->size;
		UDATA headerSize = sizeof(J9IndexableObjectContiguous);
		if (0 == size) {
			headerSize = sizeof(J9IndexableObjectDiscontiguous);
		}
#else
		UDATA headerSize = sizeof(J9IndexableObjectDiscontiguous);
#endif
		return headerSize;
	}

	/**
	 * Returns the address of first arraylet leaf slot in the spine
	 * @param arrayPtr Ptr to an array
	 * @return Arrayoid ptr
	 */
	MMINLINE fj9object_t *
	getArrayoidPointer(J9IndexableObject *arrayPtr)
	{
		return (fj9object_t *)((UDATA)arrayPtr + sizeof(J9IndexableObjectDiscontiguous));
	}

	/**
	 * Returns the address of first data slot in the array
	 * @param arrayPtr Ptr to an array
	 * @return Address of first data slot in the array
	 */
	MMINLINE void *
	getDataPointerForContiguous(J9IndexableObject *arrayPtr)
	{
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		return (void *)((UDATA)arrayPtr + sizeof(J9IndexableObjectContiguous));
#else /* J9VM_GC_HYBRID_ARRAYLETS */
		fj9object_t* arrayoidPointer = getArrayoidPointer(arrayPtr);
		fj9object_t firstArrayletLeaf = arrayoidPointer[0];
		return mmPointerFromToken((J9JavaVM*)_omrVM->_language_vm, firstArrayletLeaf);
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
	}


	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * WARNING: This implementation assumes the arraylet layout is InlineContiguous.
	 * @param clazzPtr Pointer to the class of the object
	 * @param numberOfElements size from indexable object
	 * @return offset of the hashcode slot
	 */
	MMINLINE UDATA
	getHashcodeOffset(J9Class *clazzPtr, UDATA numberOfElements)
	{
		ArrayLayout layout = InlineContiguous; 
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		if(0 == numberOfElements) {
			layout = Discontiguous;
		}
#endif /* defined(J9VM_GC_HYBRID_ARRAYLETS) */
		return getHashcodeOffset(clazzPtr, layout, numberOfElements);
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param clazzPtr Pointer to the class of the object
	 * @param layout The layout of the indexable object
	 * @param numberOfElements size from indexable object
	 * @return offset of the hashcode slot
	 */
	MMINLINE UDATA
	getHashcodeOffset(J9Class *clazzPtr, ArrayLayout layout, UDATA numberOfElements)
	{
		/* Don't call getDataSizeInBytes() since that rounds up to UDATA. */
		UDATA dataSize = numberOfElements * J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		UDATA numberOfArraylets = numArraylets(dataSize);
		bool alignData = shouldAlignSpineDataSection(clazzPtr);
		UDATA spineSize = getSpineSize(clazzPtr, layout, numberOfArraylets, dataSize, alignData);
		return MM_Math::roundToSizeofU32(spineSize);
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param arrayPtr Pointer to the array
	 * @return offset of the hashcode slot
	 */
	MMINLINE UDATA
	getHashcodeOffset(J9IndexableObject *arrayPtr)
	{
		ArrayLayout layout = getArrayLayout(arrayPtr);
		return getHashcodeOffset(J9GC_J9OBJECT_CLAZZ(arrayPtr), layout, getSizeInElements(arrayPtr));
	}
	
	MMINLINE void
	expandArrayletSubSpaceRange(MM_MemorySubSpace * subSpace, void * rangeBase, void * rangeTop, UDATA largestDesirableArraySpineSize)
	{
		GC_ArrayletObjectModelBase::expandArrayletSubSpaceRange(subSpace, rangeBase, rangeTop, largestDesirableArraySpineSize);
	}

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
};

#endif /*J9VM_GC_ARRAYLETS */
#endif /* ARRAYLETOBJECTMODEL_ */
