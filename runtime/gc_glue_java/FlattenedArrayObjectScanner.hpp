/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#if !defined(FLATTENEDARRAYOBJECTSCANNER_HPP_)
#define FLATTENEDARRAYOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "objectdescription.h"
#include "ArrayObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "HeadlessMixedObjectScanner.hpp"
#include "IndexableObjectScanner.hpp"

class GC_FlattenedArrayObjectScanner : public GC_HeadlessMixedObjectScanner
{
	/* Data Members */
private:

	MM_EnvironmentBase *_env;
	uintptr_t _elementSizeWithoutPadding; /**< Size of the flattened element, without padding */
	uintptr_t *_descriptionBasePtr; /**< Pointer to the description base */
#if defined(OMR_GC_LEAF_BITS)
	uintptr_t *_leafBasePtr; /**< Pointer to the leaf description base */
#endif /* defined(OMR_GC_LEAF_BITS) */
	GC_IndexableObjectScanner _indexableScanner; /**< Used to iterate the array by element */

protected:

public:

	/* Methods */
private:

protected:

	/**
	 * @param env The scanning thread environment
	 * @param arrayPtr pointer to the array to be processed
	 * @param basePtr pointer to the first contiguous array cell
	 * @param limitPtr pointer to end of last contiguous array cell
	 * @param scanPtr pointer to the array cell where scanning will start
	 * @param endPtr pointer to the array cell where scanning will stop
	 * @param scanMap The first scan map
	 * @param elementSize The size of each element, without padding
	 * @param elementStride The stride of each element, including element padding
	 * @param flags Scanning context flags
	 */
	MMINLINE GC_FlattenedArrayObjectScanner(
		MM_EnvironmentBase *env
		, omrobjectptr_t arrayPtr
		, fomrobject_t *basePtr
		, fomrobject_t *limitPtr
		, fomrobject_t *scanPtr
		, fomrobject_t *endPtr
		, uintptr_t elementSize
		, uintptr_t elementStride
		, uintptr_t *descriptionBasePtr
#if defined(OMR_GC_LEAF_BITS)
		, uintptr_t *leafBasePtr
#endif /* defined(OMR_GC_LEAF_BITS) */
		, uintptr_t flags)
	: GC_HeadlessMixedObjectScanner(env, scanPtr, elementSize, flags | GC_ObjectScanner::indexableObject)
	, _env(env)
	, _elementSizeWithoutPadding(elementSize)
	, _descriptionBasePtr(descriptionBasePtr)
#if defined(OMR_GC_LEAF_BITS)
	, _leafBasePtr(leafBasePtr)
#endif /* defined(OMR_GC_LEAF_BITS) */
	/* Pass 0 for scanMap, as the indexable iterator does not use the scanMap */
	, _indexableScanner(env, arrayPtr, basePtr, limitPtr, scanPtr, endPtr, 0, elementStride, flags)
	{
		_typeId = __FUNCTION__;
	}

	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
#if defined(OMR_GC_LEAF_BITS)
		GC_HeadlessMixedObjectScanner::initialize(env, _descriptionBasePtr, _leafBasePtr);
#else /* defined(OMR_GC_LEAF_BITS) */
		GC_HeadlessMixedObjectScanner::initialize(env, _descriptionBasePtr);
#endif /* defined(OMR_GC_LEAF_BITS) */

		_indexableScanner.initialize(env);

		/* The HeadlessMixedObjectScanner will setNoMoreSlots() causing us
		 * to miss other elements of the array
		 */
		setMoreSlots();
	}

public:

	/**
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr pointer to the array to be processed
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param[in] flags Scanning context flags
	 * @param[in] splitAmount If >0, the number of elements to include for this scanner instance; if 0, include all elements
	 * @param[in] startIndex The index of the first element to scan
	 */
	MMINLINE static GC_FlattenedArrayObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags, uintptr_t splitAmount, uintptr_t startIndex = 0)
	{
		GC_FlattenedArrayObjectScanner *objectScanner = (GC_FlattenedArrayObjectScanner *)allocSpace;
		GC_ArrayObjectModel *arrayObjectModel = &(env->getExtensions()->indexableObjectModel);
		J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(objectPtr, env);
		J9ArrayClass *j9ArrayClass = (J9ArrayClass *) clazzPtr;

		/* TODO are these always the same? */
		Assert_MM_true(j9ArrayClass->componentType == j9ArrayClass->leafComponentType);

		J9Class *elementClass = j9ArrayClass->componentType;
		omrarrayptr_t arrayPtr = (omrarrayptr_t)objectPtr;
		
		uintptr_t sizeInElements = arrayObjectModel->getSizeInElements(arrayPtr);
		uintptr_t elementSize = J9_VALUETYPE_FLATTENED_SIZE(elementClass);
		uintptr_t elementStride = J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		fomrobject_t *basePtr = (fomrobject_t *)arrayObjectModel->getDataPointerForContiguous(arrayPtr);
		fomrobject_t *limitPtr = (fomrobject_t *)((uintptr_t)basePtr + (sizeInElements * elementStride));
		fomrobject_t *scanPtr = (fomrobject_t *)((uintptr_t)basePtr + (startIndex * elementStride));
		fomrobject_t *endPtr = limitPtr;
		if (!GC_ObjectScanner::isIndexableObjectNoSplit(flags) && (splitAmount != 0)) {
			Assert_MM_unreachable();
			endPtr = (fomrobject_t *)((uintptr_t)scanPtr + (splitAmount * elementStride));
			if (endPtr > limitPtr) {
				endPtr = limitPtr;
			}
		}
		uintptr_t *instanceDescription = elementClass->instanceDescription;
#if defined(OMR_GC_LEAF_BITS)
		uintptr_t *leafDescription = elementClass->instanceLeafDescription;
		new(objectScanner) GC_FlattenedArrayObjectScanner(env, objectPtr, basePtr, limitPtr, scanPtr, endPtr, elementSize, elementStride, instanceDescription, leafDescription, flags);
#else /* defined(OMR_GC_LEAF_BITS) */
		new(objectScanner) GC_FlattenedArrayObjectScanner(env, objectPtr, basePtr, limitPtr, scanPtr, endPtr, elementSize, elementStride, instanceDescription, flags);
#endif /* defined(OMR_GC_LEAF_BITS) */

		objectScanner->initialize(env);
		if (0 != startIndex) {
			objectScanner->clearHeadObjectScanner();
		}
		return objectScanner;
	}

	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }

	/**
	 * @param env The scanning thread environment
	 * @param allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param splitAmount The maximum number of array elements to include
	 * @return Pointer to split scanner in allocSpace
	 */
	GC_IndexableObjectScanner *
	splitTo(MM_EnvironmentBase *env, void *allocSpace, uintptr_t splitAmount)
	{
		Assert_MM_unimplemented();
		return NULL;
	}

	/**
	 * Return base pointer and slot bit map for next block of contiguous slots to be scanned. The
	 * base pointer must be fomrobject_t-aligned. Bits in the bit map are scanned in order of
	 * increasing significance, and the least significant bit maps to the slot at the returned
	 * base pointer.
	 *
	 * @param[out] scanMap the bit map for the slots contiguous with the returned base pointer
	 * @param[out] hasNextSlotMap set this to true if this method should be called again, false if this map is known to be last
	 * @return a pointer to the first slot mapped by the least significant bit of the map, or NULL if no more slots
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t *slotMap, bool *hasNextSlotMap)
	{
		fomrobject_t *result = GC_HeadlessMixedObjectScanner::getNextSlotMap(slotMap, hasNextSlotMap);
		/* Ignore hasNextSlotMap from HeadLess, we want to always report that there is another element */
		*hasNextSlotMap = true;
		if (result == NULL) {
			/* No more slots in the current element, get the next element of the array */
			result = _indexableScanner.nextIndexableElement();
			if (result == NULL) {
				/* There are no elements in the array */
				*hasNextSlotMap = false;
			} else {
				_mapPtr = result;
				_endPtr = (fomrobject_t *)((uintptr_t)_mapPtr + _elementSizeWithoutPadding);
				GC_HeadlessMixedObjectScanner::initialize(_env, _descriptionBasePtr, _leafBasePtr);
				/* GC_HeadlessMixedObjectScanner::initialize() may setNoMoreSlots(), so set it back to true. 
				 * We must also return (hasNextSlotMap = true) on top of this
				 */
				setMoreSlots();
			}
		}
		return result;
	}

#if defined(OMR_GC_LEAF_BITS)
	/**
	 * Return base pointer and slot bit map for next block of contiguous slots to be scanned. The
	 * base pointer must be fomrobject_t-aligned. Bits in the bit map are scanned in order of
	 * increasing significance, and the least significant bit maps to the slot at the returned
	 * base pointer.
	 *
	 * @param[out] scanMap the bit map for the slots contiguous with the returned base pointer
	 * @param[out] leafMap the leaf bit map for the slots contiguous with the returned base pointer
	 * @param[out] hasNextSlotMap set this to true if this method should be called again, false if this map is known to be last
	 * @return a pointer to the first slot mapped by the least significant bit of the map, or NULL if no more slots
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t *slotMap, uintptr_t *leafMap, bool *hasNextSlotMap)
	{
		fomrobject_t *result = GC_HeadlessMixedObjectScanner::getNextSlotMap(slotMap, leafMap, hasNextSlotMap);
		/* Ignore hasNextSlotMap from HeadLess, we want to always report that there is another element */
		*hasNextSlotMap = true;
		if (result == NULL) {
			/* No more slots in the current element, get the next element of the array */
			result = _indexableScanner.nextIndexableElement();
			if (result == NULL) {
				/* There are no elements in the array */
				*hasNextSlotMap = false;
			} else {
				_mapPtr = result;
				_endPtr = (fomrobject_t *)((uintptr_t)_mapPtr + _elementSizeWithoutPadding);
				GC_HeadlessMixedObjectScanner::initialize(_env, _descriptionBasePtr, _leafBasePtr);
				/* GC_HeadlessMixedObjectScanner::initialize() may setNoMoreSlots(), so set it back to true. 
				 * We must also return (hasNextSlotMap = true) on top of this
				 */
				setMoreSlots();
			}
		}
		return result;
	}
#endif /* defined(OMR_GC_LEAF_BITS) */
};

#endif /* FLATTENEDARRAYOBJECTSCANNER_HPP_ */
