/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
 * @ingroup GC_Structs
 */

#if !defined(REFERENCEOBJECTSCANNER_HPP_)
#define REFERENCEOBJECTSCANNER_HPP_

#include "MixedObjectScanner.hpp"

/**
 * This class is used to iterate over the slots of a Java reference object.
 */
class GC_ReferenceObjectScanner : public GC_MixedObjectScanner
{
	/* Data Members */
private:
	fomrobject_t * _referentSlotAddress;

protected:

public:

	/* Member Functions */
private:
	/**
	 * The referent slot must be skipped unless the referent must be marked. So clear
	 * the corresponding bit in the scan map when the scan pointer is reset to contain
	 * the referent slot within its mapped range.
	 */
	MMINLINE uintptr_t skipReferentSlot(fomrobject_t *mapPtr, uintptr_t scanMap)
	{
		if (_referentSlotAddress > mapPtr) {
			/* Skip over referent slot */
			intptr_t referentSlotDistance = _referentSlotAddress - mapPtr;
			if (referentSlotDistance < _bitsPerScanMap) {
				scanMap &= ~((uintptr_t)1 << referentSlotDistance);
			}
		}
		return scanMap;
	}

protected:
	/**
	 * Instantiation constructor.
	 *
	 * @param[in] env pointer to the environment for the current thread
	 * @param[in] objectPtr pointer to the object to be processed
	 * @param[in] referentSlotAddress pointer to referent slot, if this slot is to be skipped; otherwise, specify NULL
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_ReferenceObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, fomrobject_t *referentSlotAddress, uintptr_t flags)
		: GC_MixedObjectScanner(env, objectPtr, flags)
		, _referentSlotAddress(referentSlotAddress)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Subclasses must call this method to set up the instance description bits and description pointer.
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		GC_MixedObjectScanner::initialize(env);

		/* Skip over referent slot if required */
		_scanMap = skipReferentSlot(_scanPtr, _scanMap);
	}

public:
	/**
	 * In-place instantiation and initialization for reference object scanner.
	 *
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr The object to scan
	 * @param[in] referentSlotAddress pointer to referent slot, if this slot is to be skipped; otherwise, specify NULL
	 * @param[in] allocSpace Pointer to space for in-place instantiation (at least sizeof(GC_ReferenceObjectScanner) bytes)
	 * @param[in] flags Scanning context flags
	 * @return Pointer to GC_ReferenceObjectScanner instance in allocSpace
	 */
	MMINLINE static GC_ReferenceObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, fomrobject_t *referentSlotAddress, void *allocSpace, uintptr_t flags)
	{
		GC_ReferenceObjectScanner *objectScanner = (GC_ReferenceObjectScanner *)allocSpace;
		new(objectScanner) GC_ReferenceObjectScanner(env, objectPtr, referentSlotAddress, flags);
		objectScanner->initialize(env);
		return objectScanner;
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
		fomrobject_t *mapPtr = GC_MixedObjectScanner::getNextSlotMap(slotMap, hasNextSlotMap);

		/* Skip over referent slot */
		*slotMap = skipReferentSlot(mapPtr, *slotMap);

		return mapPtr;
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
		fomrobject_t *mapPtr = GC_MixedObjectScanner::getNextSlotMap(slotMap, leafMap, hasNextSlotMap);

		/* Skip over referent slot */
		*slotMap = skipReferentSlot(mapPtr, *slotMap);

		return mapPtr;
	}
#endif /* defined(OMR_GC_LEAF_BITS) */
};
#endif /* REFERENCEOBJECTSCANNER_HPP_ */
