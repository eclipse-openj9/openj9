
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
 * @ingroup GC_Structs
 */

#if !defined(MIXEDOBJECTITERATOR_HPP_)
#define MIXEDOBJECTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"
#include "GCExtensionsBase.hpp"

#include "Bits.hpp"
#include "ObjectModel.hpp"
#include "MixedObjectModel.hpp"
#include "ObjectIteratorState.hpp"
#include "SlotObject.hpp"

class GC_MixedObjectIterator
{
/* Data Members */
private:
protected:
	GC_SlotObject _slotObject;	/**< Create own SlotObject class to provide output */

	J9Object *_objectPtr;		/**< save address of object this class initiated with */
	fj9object_t *_scanPtr;		/**< current scan pointer */
	fj9object_t *_endPtr;		/**< end scan pointer */
	UDATA *_descriptionPtr;		/**< current description pointer */
	UDATA _description;			/**< current description word */
	UDATA _descriptionIndex;	/**< current bit number in description word */
public:

/* Member Functions */
private:
protected:
	/**
	 * Set up the description bits
	 * @param clazzPtr[in] Class that from which to load description bits
	 */
	MMINLINE void 
	initializeDescriptionBits(J9Class *clazzPtr)
	{
		UDATA tempDescription = (UDATA)clazzPtr->instanceDescription;
		if (tempDescription & 1) {
			_description = (tempDescription >> 1);
		} else {
			_descriptionPtr = (UDATA *)tempDescription;
			_description = *_descriptionPtr;
			_descriptionPtr += 1;
		}
		_descriptionIndex = J9BITS_BITS_IN_SLOT;
	}

public:
	/**
	 * Initialize the internal walk description for the given object.
	 * @param objectPtr[in] Mixed object to be scanned
	 */
	MMINLINE void 
	initialize(OMR_VM *omrVM, J9Object *objectPtr)
	{
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
		_objectPtr = objectPtr;
		J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(objectPtr);
		initializeDescriptionBits(clazzPtr);

		/* Set current and end scan pointers */
		_scanPtr = extensions->mixedObjectModel.getHeadlessObject(objectPtr);
		_endPtr = (fj9object_t *)((U_8*)_scanPtr + extensions->mixedObjectModel.getSizeInBytesWithoutHeader(objectPtr));
	}

	/**
	 * Initialize the internal walk description for the given class and address.
	 * @param clazzPtr[in] Class of object need to be scanned
	 * @param fj9object_t*[in] Address within an object to scan
	 */
	MMINLINE void 
	initialize(OMR_VM *omrVM, J9Class *clazzPtr, fj9object_t *addr)
	{
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
		_objectPtr = NULL;
		initializeDescriptionBits(clazzPtr);

		/* Set current and end scan pointers */
		_scanPtr = addr;
		_endPtr = (fj9object_t *)((U_8*)_scanPtr + extensions->mixedObjectModel.getSizeInBytesWithoutHeader(clazzPtr));
	}


	/**
	 * Return back SlotObject for next reference
	 * @return SlotObject
	 */
	MMINLINE GC_SlotObject *nextSlot()
	{
		while (_scanPtr < _endPtr) {
			/* Record the slot information */
			if (_description & 1) {
				_slotObject.writeAddressToSlot(_scanPtr);
				_scanPtr += 1;

				/* Advance the description pointer information */
				if (--_descriptionIndex) {
					_description >>= 1;
				} else {
					_description = *_descriptionPtr;
					_descriptionPtr += 1;
					_descriptionIndex = J9BITS_BITS_IN_SLOT;
				}
				return &_slotObject;
			} else {
				_scanPtr += 1;

				/* Advance the description pointer information */
				if (--_descriptionIndex) {
					_description >>= 1;
				} else {
					_description = *_descriptionPtr;
					_descriptionPtr += 1;
					_descriptionIndex = J9BITS_BITS_IN_SLOT;
				}
			}
		}
		/* No more object slots to scan */
		return NULL;
	}

	/**
	 * Restores the iterator state into this class
	 * @param[in] objectIteratorState partially scanned object iterator state
	 */
	MMINLINE void restore(GC_ObjectIteratorState *objectIteratorState)
	{
		_objectPtr        = objectIteratorState->_objectPtr;
		_scanPtr          = objectIteratorState->_scanPtr;
		_endPtr           = objectIteratorState->_endPtr;
		_descriptionPtr   = objectIteratorState->_descriptionPtr;
		_description      = objectIteratorState->_description;
		_descriptionIndex = objectIteratorState->_descriptionIndex;
	}

	/**
	 * Saves the partially scanned state of this class
	 * @param[in] objectIteratorState where to store the state
	 */
	MMINLINE void save(GC_ObjectIteratorState *objectIteratorState)
	{
		objectIteratorState->_objectPtr        = _objectPtr;
		objectIteratorState->_scanPtr          = _scanPtr;
		objectIteratorState->_endPtr           = _endPtr;
		objectIteratorState->_descriptionPtr   = _descriptionPtr;
		objectIteratorState->_description      = _description;
		objectIteratorState->_descriptionIndex = _descriptionIndex;
	}

	MMINLINE J9Object *getObject()
	{
		return _objectPtr;
	}

	/**
	 * A much lighter-weight way to restore iterator state:  pass in the slot pointer and the iterator will align itself
	 * to return that as its next slot.
	 * @param slotPtr[in] The iterator slot which should be next returned by the iterator
	 */
	MMINLINE void advanceToSlot(fj9object_t *slotPtr)
	{
		Assert_MM_true(slotPtr >= _scanPtr);
		UDATA bitsToTravel = slotPtr - _scanPtr;
		if (NULL != _descriptionPtr) {
			Assert_MM_true(J9BITS_BITS_IN_SLOT >= _descriptionIndex);
			/* _descriptionIndex uses backward math so put it in forward-facing bit counts */
			UDATA currentBitIndexInCurrentSlot = J9BITS_BITS_IN_SLOT - _descriptionIndex;
			UDATA newBitIndexRelativeToCurrentSlot = currentBitIndexInCurrentSlot + bitsToTravel;
			UDATA slotsToMove = newBitIndexRelativeToCurrentSlot / J9BITS_BITS_IN_SLOT;
			UDATA newBitIndexAfterSlotMove = newBitIndexRelativeToCurrentSlot % J9BITS_BITS_IN_SLOT;
			/* bump the _descriptionPtr by the appropriate number of slots */
			_descriptionPtr += slotsToMove;
			/* _descriptionPtr ALWAYS points at the NEXT slot of index bits so read the CURRENT by subtracting 1 */
			UDATA *currentDescriptionPointer = _descriptionPtr - 1;
			_description = *currentDescriptionPointer;
			/* use J9BITS_BITS_IN_SLOT as the "0" for _descriptionIndex since it uses backward math */
			_descriptionIndex = J9BITS_BITS_IN_SLOT;
			/* now, advance the number of bits required into this slot */
			_descriptionIndex -= newBitIndexAfterSlotMove;
			_description >>= newBitIndexAfterSlotMove;
		} else {
			Assert_MM_true(_descriptionIndex >= bitsToTravel);
			_descriptionIndex -= bitsToTravel;
			_description >>= bitsToTravel;
		}
		/* save back the new slot ptr */
		_scanPtr = slotPtr;
		Assert_MM_true (_description & 1);
	}

	/**
	 * @param vm[in] pointer to the JVM
	 */
	GC_MixedObjectIterator (OMR_VM *omrVM)
		: _slotObject(GC_SlotObject(omrVM, NULL))
		, _objectPtr(NULL)
		, _scanPtr(NULL)
		, _endPtr(NULL)
		, _descriptionPtr(NULL)
		, _description(0)
		, _descriptionIndex(0)
	{}

	/**
	 * @param vm[in] pointer to the JVM
	 * @param objectPtr[in] the object to be processed
	 */
	GC_MixedObjectIterator (OMR_VM *omrVM, J9Object *objectPtr)
		: _slotObject(GC_SlotObject(omrVM, NULL))
		, _objectPtr(NULL)
		, _scanPtr(NULL)
		, _endPtr(NULL)
		, _descriptionPtr(NULL)
		, _description(0)
		, _descriptionIndex(0)
	  {
		initialize(omrVM, objectPtr);
	  }
};

#endif /* MIXEDOBJECTITERATOR_HPP_ */
