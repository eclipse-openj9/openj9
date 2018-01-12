/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#if !defined(MIXEDOBJECTSCANNER_HPP_)
#define MIXEDOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "objectdescription.h"
#include "GCExtensions.hpp"
#include "ObjectScanner.hpp"

/**
 * This class is used to iterate over the slots of a Java object.
 */
class GC_MixedObjectScanner : public GC_ObjectScanner
{
	/* Data Members */
private:
	fomrobject_t * const _endPtr;	/**< end scan pointer */
	fomrobject_t *_mapPtr;			/**< pointer to first slot in current scan segment */
	uintptr_t *_descriptionPtr;		/**< current description pointer */
#if defined(J9VM_GC_LEAF_BITS)
	uintptr_t *_leafPtr;			/**< current leaf description pointer */
#endif /* J9VM_GC_LEAF_BITS */

protected:

public:

	/* Member Functions */
private:

protected:
	/**
	 * @param env The scanning thread environment
	 * @param[in] objectPtr the object to be processed
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_MixedObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, uintptr_t flags)
		: GC_ObjectScanner(env, objectPtr, (fomrobject_t *)(objectPtr + 1), 0, flags, J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceHotFieldDescription)
		, _endPtr((fomrobject_t *)((uint8_t*)_scanPtr + J9GC_J9OBJECT_CLAZZ(objectPtr)->totalInstanceSize))
		, _mapPtr(_scanPtr)
		, _descriptionPtr(NULL)
#if defined(J9VM_GC_LEAF_BITS)
		, _leafPtr(NULL)
#endif /* J9VM_GC_LEAF_BITS */
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Subclasses must call this method to set up the instance description bits and description pointer.
	 * @param[in] env The scanning thread environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		GC_ObjectScanner::initialize(env);

		/* Initialize the slot map from description bits */
		J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(_parentObjectPtr);
		_scanMap = (uintptr_t)clazzPtr->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
		_leafMap = (uintptr_t)clazzPtr->instanceLeafDescription;
#endif /* J9VM_GC_LEAF_BITS */
		if (_scanMap & 1) {
			_scanMap >>= 1;
			_descriptionPtr = NULL;
#if defined(J9VM_GC_LEAF_BITS)
			_leafMap >>= 1;
			_leafPtr = NULL;
#endif /* J9VM_GC_LEAF_BITS */
			setNoMoreSlots();
		} else {
			_descriptionPtr = (uintptr_t *)_scanMap;
			_scanMap = *_descriptionPtr;
			_descriptionPtr += 1;
#if defined(J9VM_GC_LEAF_BITS)
			_leafPtr = (uintptr_t *)_leafMap;
			_leafMap = *_leafPtr;
			_leafPtr += 1;
#endif /* J9VM_GC_LEAF_BITS */
		}
	}

public:
	/**
	 * In-place instantiation and initialization for mixed obect scanner.
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr The object to scan
	 * @param[in] allocSpace Pointer to space for in-place instantiation (at least sizeof(GC_MixedObjectScanner) bytes)
	 * @param[in] flags Scanning context flags
	 * @return Pointer to GC_MixedObjectScanner instance in allocSpace
	 */
	MMINLINE static GC_MixedObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
	{
		GC_MixedObjectScanner *objectScanner = (GC_MixedObjectScanner *)allocSpace;
		new(objectScanner) GC_MixedObjectScanner(env, objectPtr, flags);
		objectScanner->initialize(env);
		return objectScanner;
	}
	
	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }

	/**
	 * @see GC_ObjectScanner::getNextSlotMap(uintptr_t&, bool&)
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t &slotMap, bool &hasNextSlotMap)
	{
		slotMap = 0;
		hasNextSlotMap = false;
		_mapPtr += _bitsPerScanMap;
		while (_endPtr > _mapPtr) {
			slotMap = *_descriptionPtr;
			_descriptionPtr += 1;
			if (0 != slotMap) {
				hasNextSlotMap = _bitsPerScanMap < (_endPtr - _mapPtr);
				return _mapPtr;
			}
			_mapPtr += _bitsPerScanMap;
		}
		return NULL;
	}

#if defined(J9VM_GC_LEAF_BITS)
	/**
	 * @see GC_ObjectScanner::getNextSlotMap(uintptr_t&, uintptr_t&, bool&)
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t &slotMap, uintptr_t &leafMap, bool &hasNextSlotMap)
	{
		slotMap = 0;
		hasNextSlotMap = false;
		_mapPtr += _bitsPerScanMap;
		while (_endPtr > _mapPtr) {
			slotMap = *_descriptionPtr;
			_descriptionPtr += 1;
			leafMap = *_leafPtr;
			_leafPtr += 1;
			if (0 != slotMap) {
				hasNextSlotMap = _bitsPerScanMap < (_endPtr - _mapPtr);
				return _mapPtr;
			}
			_mapPtr += _bitsPerScanMap;
		}
		return NULL;
	}
#endif /* J9VM_GC_LEAF_BITS */
};
#endif /* MIXEDOBJECTSCANNER_HPP_ */
