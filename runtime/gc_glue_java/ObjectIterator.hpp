/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

#if !defined(OBJECTITERATOR_HPP_)
#define OBJECTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"
#include "modronbase.h"
#include "objectdescription.h"

#include "Base.hpp"
#include "GCExtensions.hpp"
#include "MixedObjectIterator.hpp"
#include "ObjectModel.hpp"
#include "PointerContiguousArrayIterator.hpp"
#include "SlotObject.hpp"

class GC_ObjectIterator {
	/* Data Members */
private:
	OMR_VM* _omrVM;

	GC_ObjectModel::ScanType _type;

	GC_SlotObject _slotObject;

	GC_MixedObjectIterator _mixedObjectIterator;
	GC_PointerContiguousArrayIterator _pointerContiguousArrayIterator;

protected:
public:
	/* Member Functions */
private:
protected:
public:
	/**
	 * Initialize the internal walk description for the given object.
	 * @param objectPtr[in] object to be scanned
	 */
	MMINLINE void initialize(omrobjectptr_t objectPtr)
	{
		/* check scan type and initialize the proper iterator */
		_type = MM_GCExtensions::getExtensions(_omrVM)->objectModel.getScanType(objectPtr);
		switch (_type) {
		case GC_ObjectModel::SCAN_INVALID_OBJECT:
			return;
		case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
		case GC_ObjectModel::SCAN_MIXED_OBJECT:
		case GC_ObjectModel::SCAN_CLASS_OBJECT:
		case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
			return _mixedObjectIterator.initialize(_omrVM, objectPtr);
		case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
			return _pointerContiguousArrayIterator.initialize(objectPtr);
		case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
			return;
		default:
			Assert_MM_unreachable();
		}
	}

	/**
	 * Return back SlotObject for next reference
	 * @return SlotObject
	 */
	MMINLINE GC_SlotObject* nextSlot()
	{
		/* check scan type and call nextSlot on the proper iterator */
		switch (_type) {
		case GC_ObjectModel::SCAN_INVALID_OBJECT:
			return NULL;
		case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
		case GC_ObjectModel::SCAN_MIXED_OBJECT:
		case GC_ObjectModel::SCAN_CLASS_OBJECT:
		case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
			return _mixedObjectIterator.nextSlot();
		case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
			return _pointerContiguousArrayIterator.nextSlot();
		case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
			return NULL;
		default:
			Assert_MM_unreachable();
		}
		return NULL;
	}

	/**
	 * Restores the iterator state into this class
	 * @param[in] objectIteratorState partially scanned object iterator state
	 */
	MMINLINE void restore(GC_ObjectIteratorState* objectIteratorState)
	{
		/* check scan type and call restore on the proper iterator */
		switch (_type) {
		case GC_ObjectModel::SCAN_INVALID_OBJECT:
			return;
		case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
		case GC_ObjectModel::SCAN_MIXED_OBJECT:
		case GC_ObjectModel::SCAN_CLASS_OBJECT:
		case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
			return _mixedObjectIterator.restore(objectIteratorState);
		case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
			return _pointerContiguousArrayIterator.restore(objectIteratorState);
		case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
			return;
		}
	}

	/**
	 * Saves the partially scanned state of this class
	 * @param[in] objectIteratorState where to store the state
	 */
	MMINLINE void save(GC_ObjectIteratorState* objectIteratorState)
	{
		/* check scan type and call nextSlot on the proper iterator */
		switch (_type) {
		case GC_ObjectModel::SCAN_INVALID_OBJECT:
			return;
		case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
		case GC_ObjectModel::SCAN_MIXED_OBJECT:
		case GC_ObjectModel::SCAN_CLASS_OBJECT:
		case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
			return _mixedObjectIterator.save(objectIteratorState);
		case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
			return _pointerContiguousArrayIterator.save(objectIteratorState);
		case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
			return;
		}
	}

	/**
	 * @param omrVM[in] pointer to the OMR VM
	 */
	GC_ObjectIterator(OMR_VM* omrVM)
		: _omrVM(omrVM)
		, _type(GC_ObjectModel::SCAN_INVALID_OBJECT)
		, _slotObject(GC_SlotObject(omrVM, NULL))
		, _mixedObjectIterator(omrVM)
		, _pointerContiguousArrayIterator(omrVM)
	{
	}

	/**
	 * @param omrVM[in] pointer to the OMR VM
	 * @param objectPtr[in] the object to be processed
	 */
	GC_ObjectIterator(OMR_VM* omrVM, omrobjectptr_t objectPtr)
		: _omrVM(omrVM)
		, _type(GC_ObjectModel::SCAN_INVALID_OBJECT)
		, _slotObject(GC_SlotObject(omrVM, NULL))
		, _mixedObjectIterator(omrVM)
		, _pointerContiguousArrayIterator(omrVM)
	{
		initialize(objectPtr);
	}
};

#endif /* OBJECTITERATOR_HPP_ */
