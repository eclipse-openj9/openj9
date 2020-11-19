/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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

#if !defined(POINTERARRAYOBJECTSCANNER_HPP_)
#define POINTERARRAYOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "ArrayObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "IndexableObjectScanner.hpp"

/**
 * This class is used to iterate over the slots of a Java pointer array.
 */
class GC_PointerArrayObjectScanner : public GC_IndexableObjectScanner
{
	/* Data Members */
private:
	fomrobject_t *_mapPtr;	/**< pointer to first array element in current scan segment */

protected:

public:

	/* Methods */
private:

protected:
	/**
	 * @param env The scanning thread environment
	 * @param[in] arrayPtr pointer to the array to be processed
	 * @param[in] basePtr pointer to the first contiguous array cell
	 * @param[in] limitPtr pointer to end of last contiguous array cell
	 * @param[in] scanPtr pointer to the array cell where scanning will start
	 * @param[in] endPtr pointer to the array cell where scanning will stop
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_PointerArrayObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t arrayPtr, fomrobject_t *basePtr, fomrobject_t *limitPtr, fomrobject_t *scanPtr, fomrobject_t *endPtr, uintptr_t flags)
		: GC_IndexableObjectScanner(env, arrayPtr, basePtr, limitPtr, scanPtr, endPtr
			, (GC_SlotObject::subtractSlotAddresses(endPtr, scanPtr, env->compressObjectReferences()) < _bitsPerScanMap)
				? ((uintptr_t)1 << GC_SlotObject::subtractSlotAddresses(endPtr, scanPtr, env->compressObjectReferences())) - 1
				: UDATA_MAX
			, env->compressObjectReferences() ? sizeof(uint32_t) : sizeof(uintptr_t)
			, flags)
		, _mapPtr(_scanPtr)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Set up the scanner.
	 * @param[in] env Current environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		GC_IndexableObjectScanner::initialize(env);
	}

public:
	/**
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr pointer to the array to be processed
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param[in] flags Scanning context flags
	 * @param[in] sizeInElements Number of elements to be scanned
	 * @param[in] startIndex The index of the first element to scan
	 */
	MMINLINE static GC_PointerArrayObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, fomrobject_t *basePtr, void *allocSpace, uintptr_t flags, uintptr_t sizeInElements,  uintptr_t startIndex = 0)
	{
		GC_PointerArrayObjectScanner *objectScanner = (GC_PointerArrayObjectScanner *)allocSpace;
		if (NULL != objectScanner) {
			bool const compressed = env->compressObjectReferences();
			/* If splitting is used, sizeInElements is the total size of the array. Otherwise, its the rest to be scanned. */
			fomrobject_t *scanPtr = GC_SlotObject::addToSlotAddress(basePtr, startIndex, compressed);
			fomrobject_t *limitPtr = GC_SlotObject::addToSlotAddress(basePtr, sizeInElements, compressed);
			fomrobject_t *endPtr = limitPtr;

			new(objectScanner) GC_PointerArrayObjectScanner(env, objectPtr, basePtr, limitPtr, scanPtr, endPtr, flags);
			objectScanner->initialize(env);
			if (0 != startIndex) {
				objectScanner->clearHeadObjectScanner();
			}
		}
		return objectScanner;
	}

	MMINLINE uintptr_t getBytesRemaining() { return (uintptr_t)_endPtr - (uintptr_t)_scanPtr; }

	/**
	 * @param env The scanning thread environment
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param splitAmount The maximum number of array elements to include
	 * @return Pointer to split scanner in allocSpace
	 */
	virtual GC_IndexableObjectScanner *
	splitTo(MM_EnvironmentBase *env, void *allocSpace, uintptr_t splitAmount)
	{
		GC_PointerArrayObjectScanner *splitScanner = NULL;

		Assert_MM_true(_limitPtr >= _endPtr);
		/* Downsize splitAmount if larger than the tail of this scanner */
		uintptr_t remainder = GC_SlotObject::subtractSlotAddresses(_limitPtr, _endPtr, compressObjectReferences());
		if (splitAmount > remainder) {
			splitAmount = remainder;
		}

		Assert_MM_true(NULL != allocSpace);
		/* If splitAmount is 0 the new scanner will return NULL on the first call to getNextSlot(). */
		splitScanner = (GC_PointerArrayObjectScanner *)allocSpace;
		/* Create new scanner for next chunk of array starting at the end of current chunk size splitAmount elements */
		new(splitScanner) GC_PointerArrayObjectScanner(env, getArrayObject(), _basePtr, _limitPtr, _endPtr, GC_SlotObject::addToSlotAddress(_endPtr, splitAmount, compressObjectReferences()), _flags);
		splitScanner->initialize(env);

		return splitScanner;
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
		bool const compressed = compressObjectReferences();
		fomrobject_t *result = NULL;
		_mapPtr = GC_SlotObject::addToSlotAddress(_mapPtr, _bitsPerScanMap, compressed);
		if (_endPtr > _mapPtr) {
			intptr_t remainder = GC_SlotObject::subtractSlotAddresses(_endPtr, _mapPtr, compressed);
			if (remainder >= _bitsPerScanMap) {
				*slotMap = UDATA_MAX;
			} else {
				*slotMap = ((uintptr_t)1 << remainder) - 1;
			}
			*hasNextSlotMap = remainder > _bitsPerScanMap;
			result = _mapPtr;
		} else {
			*slotMap = 0;
			*hasNextSlotMap = false;
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
	getNextSlotMap(uintptr_t *scanMap, uintptr_t *leafMap, bool *hasNextSlotMap)
	{
		*leafMap = 0;
		return getNextSlotMap(scanMap, hasNextSlotMap);
	}
#endif /* defined(OMR_GC_LEAF_BITS) */
};

#endif /* POINTERARRAYOBJECTSCANNER_HPP_ */
