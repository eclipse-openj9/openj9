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

#if !defined(ARRAYLETOBJECTMODELBASE_)
#define ARRAYLETOBJECTMODELBASE_

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modron.h"
#include "modronopt.h"

#if defined(J9VM_GC_ARRAYLETS)

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
protected:
	OMR_VM *_omrVM; 	/**< used so that we can pull the arrayletLeafSize and arrayletLeafLogSize for arraylet sizing calculations */
	void * _arrayletRangeBase; /**< The base heap range of where discontiguous arraylets are allowed. */
	void * _arrayletRangeTop; /**< The top heap range of where discontiguous arraylets are allowed. */
	MM_MemorySubSpace * _arrayletSubSpace; /**< The only subspace that is allowed to have discontiguous arraylets. */
	UDATA _largestDesirableArraySpineSize; /**< A cached copy of the subspace's _largestDesirableArraySpineSize to be used when we don't have access to a subspace. */
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
	UDATA
	getSpineSizeWithoutHeader(ArrayLayout layout, UDATA numberArraylets, UDATA dataSize, bool alignData);

public:
	/**
	 * Returns the size of an indexable object in elements.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Size of object in elements
	 */
	MMINLINE UDATA
	getSizeInElements(J9IndexableObject *arrayPtr)
	{
		UDATA size = ((J9IndexableObjectContiguous *)arrayPtr)->size;
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (0 == size) {
			/* Discontiguous */
			size =((J9IndexableObjectDiscontiguous *)arrayPtr)->size;
		}
#endif /* defined(J9VM_GC_HYBRID_ARRAYLETS) */
		return size;
	}

	/**
	 * Sets size in elements of a contiguous indexable object .
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @param size Size in elements to set.
	 */
	MMINLINE void
	setSizeInElementsForContiguous(J9IndexableObject *arrayPtr, UDATA size)
	{
		((J9IndexableObjectContiguous *)arrayPtr)->size = (U_32)size;
	}

	/**
	 * Sets size in elements of a discontiguous indexable object .
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @param size Size in elements to set.
	 */
	MMINLINE void
	setSizeInElementsForDiscontiguous(J9IndexableObject *arrayPtr, UDATA size)
	{
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		((J9IndexableObjectContiguous *)arrayPtr)->size = 0;
#endif
		((J9IndexableObjectDiscontiguous *)arrayPtr)->size = (U_32)size;
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

#if defined(OMR_GC_COMPRESSED_POINTERS)
		/* Compressed pointers require that each leaf starts at 8 aligned address.
		 * Otherwise compressed leaf pointers will not work with shift value of 3.
		 */
		needAlignment = true;
#else
		/* The alignment padding is only required when the size of the spine pointers are
		 * not 8 byte aligned.  For example, on a 64-bit non-compressed platform, a single
		 * spine pointer would be 8 byte aligned, whereas on a 32-bit platform, a single
		 * spine pointer would not be 8 byte aligned, and would require additional padding.
		 */
		if (sizeof(fj9object_t) < sizeof(double)) {
			if (OBJECT_HEADER_SHAPE_DOUBLES == J9GC_CLASS_SHAPE(clazz)) {
				needAlignment = true;
			}
		}
#endif
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
	MMINLINE UDATA
	arrayletSize(J9IndexableObject *objPtr, UDATA index, UDATA dataSizeInBytes, UDATA numArraylets)
	{
		UDATA arrayletLeafSize = _omrVM->_arrayletLeafSize;
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
	MMINLINE UDATA
	numArraylets(UDATA unadjustedDataSizeInBytes)
	{
		UDATA leafSize = _omrVM->_arrayletLeafSize;
		UDATA numberOfArraylets = 1;
		if (UDATA_MAX != leafSize) {
			UDATA leafSizeMask = leafSize - 1;
			UDATA leafLogSize = _omrVM->_arrayletLeafLogSize;

			/* We add one to unadjustedDataSizeInBytes to ensure that it's always possible to determine the address
			 * of the after-last element without crashing. Handle the case of UDATA_MAX specially, since we use that
			 * for any object whose size overflows the address space.
			 */
			UDATA dataSizeInBytes = (UDATA_MAX == unadjustedDataSizeInBytes) ? UDATA_MAX : (unadjustedDataSizeInBytes + 1);

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
	expandArrayletSubSpaceRange(MM_MemorySubSpace* subSpace, void* rangeBase, void* rangeTop, UDATA largestDesirableArraySpineSize);
};

#endif /*J9VM_GC_ARRAYLETS */
#endif /* ARRAYLETOBJECTMODELBASE_ */
