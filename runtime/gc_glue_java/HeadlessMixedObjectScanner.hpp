/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#if !defined(HEADLESSMIXEDOBJECTSCANNER_HPP_)
#define HEADLESSMIXEDOBJECTSCANNER_HPP_

#include "j9.h"

#include "ObjectScanner.hpp"

class GC_HeadlessMixedObjectScanner : public GC_ObjectScanner
{
	/* Data Members */
private:
	fomrobject_t *_slotPtr; /**< pointer to first slot in current scan segment */
	fomrobject_t * const _endPtr; /**< end scan pointer */
	uintptr_t *_slotMapPtr; /**< current description pointer */
#if defined(J9VM_GC_LEAF_BITS)
	uintptr_t *_leafMapPtr; /**< current leaf description pointer */
#endif /* J9VM_GC_LEAF_BITS */

protected:

public:

	/* Member Functions */
private:

	MMINLINE bool
	scanForwards()
	{
		while (_slotPtr < _endPtr) {
			if (*_slotMapPtr != 0) {
				return true;
			}
			_slotPtr += _bitsPerScanMap;
			_slotMapPtr += 1;
#if defined(J9VM_GC_LEAF_BITS)
			_leafMapPtr += 1;
#endif /* J9VM_GC_LEAF_BITS */
		}
		return false;
	}

protected:

public:

	/**
	 * @param env The scanning thread environment
	 * @param[in] clazzPtr Pointer to the object's class
	 * @param[in] slotPtr Pointer to the first field of the object
	 * @param[in] objectPtr The object to be processed
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_HeadlessMixedObjectScanner(MM_EnvironmentBase *env, J9Class *clazzPtr, fomrobject_t *slotPtr, uintptr_t flags)
		: GC_ObjectScanner(env, slotPtr, 0, flags)
		, _slotPtr(slotPtr)
		, _endPtr((fomrobject_t *)((uint8_t*)_scanPtr + env->getExtensions()->mixedObjectModel.getSizeInBytesWithoutHeader(clazzPtr)))
		, _slotMapPtr(NULL)
#if defined(J9VM_GC_LEAF_BITS)
		, _leafMapPtr(NULL)
#endif /* J9VM_GC_LEAF_BITS */
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Subclasses must call this method to set up the instance description bits and description pointer.
	 * @param[in] env The scanning thread environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env, J9Class *clazzPtr)
	{
		GC_ObjectScanner::initialize(env);

		/* Initialize the slot map from description bits */
		_scanMap = (uintptr_t)clazzPtr->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
		_leafMap = (uintptr_t)clazzPtr->instanceLeafDescription;
#endif /* J9VM_GC_LEAF_BITS */

		/* If the instanceDescription is low tagged, then there is only 1 slot map, and it is
		 * encoded directly in the instance description.
		 */
		if (_scanMap & 1) {
			_scanMap >>= 1;
			_slotMapPtr = NULL;
#if defined(J9VM_GC_LEAF_BITS)
			_leafMap >>= 1;
			_leafMapPtr = NULL;
#endif /* J9VM_GC_LEAF_BITS */
			setNoMoreSlots();
		} else {
			_slotMapPtr = (uintptr_t *)_scanMap;
			_scanMap = *_slotMapPtr;
#if defined(J9VM_GC_LEAF_BITS)
			_leafMapPtr = (uintptr_t *)_leafMap;
			_leafMap = *_leafMapPtr;
#endif /* J9VM_GC_LEAF_BITS */
		}
	}
	
	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }

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
		_slotPtr += _bitsPerScanMap;
		_slotMapPtr += 1;
		
		if (scanForwards()) {
			*slotMap = *_slotMapPtr;
			*hasNextSlotMap = _bitsPerScanMap < (_endPtr - _slotPtr);
			return _slotPtr;
		}

		*slotMap = 0;
		*hasNextSlotMap = false;
		return NULL;
	}

#if defined(J9VM_GC_LEAF_BITS)
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
		_slotPtr += _bitsPerScanMap;
		_slotMapPtr += 1;
		_leafMapPtr += 1;
		
		if (scanForwards()) {
			*slotMap = *_slotMapPtr;
			*leafMap = *_leafMapPtr;
			*hasNextSlotMap = _bitsPerScanMap < (_endPtr - _slotPtr);
			return _slotPtr;
		}

		*slotMap = 0;
		*leafMap = 0;
		*hasNextSlotMap = false;
		return NULL;
	}
#endif /* J9VM_GC_LEAF_BITS */
};

#endif /* HEADLESSMIXEDOBJECTSCANNER_HPP_ */