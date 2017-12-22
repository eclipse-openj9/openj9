
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(LEAFAWAREMIXEDOBJECTITERATOR_HPP_)
#define LEAFAWAREMIXEDOBJECTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

#include "Bits.hpp"
#include "ObjectModel.hpp"
#include "MixedObjectModel.hpp"
#include "ObjectIteratorState.hpp"
#include "SlotObject.hpp"

class GC_LeafAwareMixedObjectIterator
{
private:
	GC_SlotObject _slotObject;	/**< Create own SlotObject class to provide output */

	J9Object *_objectPtr;		/**< save address of object this class initiated with */
	fj9object_t *_scanPtr;		/**< current scan pointer */
	fj9object_t *_endPtr;		/**< end scan pointer */
	UDATA *_descriptionPtr;		/**< current description pointer */
	UDATA _description;			/**< current description word */
	UDATA _descriptionIndex;	/**< current bit number in description word */
	UDATA *_leafDescriptionPtr;	/**< current leaf-bit description pointer */
	UDATA _leafDescription;		/**< current leaf-bit description word */

protected:

public:
	/**
	 * Return SlotObject for next leaf slot (or NULL if there are no more leaf slots)
	 * @return SlotObject
	 */
	MMINLINE GC_SlotObject *nextLeafSlot()
	{
		while(_scanPtr < _endPtr) {
			/* Record the slot information */
			if ((1 == (_description & 1)) && (1 == (_leafDescription & 1))) {
				_slotObject.writeAddressToSlot(_scanPtr);
				_scanPtr += 1;

				/* Advance the description pointer information */
				if(--_descriptionIndex) {
					_description >>= 1;
					_leafDescription >>= 1;
				} else {
					_description = *_descriptionPtr;
					_descriptionPtr += 1;
					_descriptionIndex = J9BITS_BITS_IN_SLOT;
					_leafDescription = *_leafDescriptionPtr;
					_leafDescriptionPtr += 1;
				}
				return &_slotObject;
			} else {
				_scanPtr += 1;

				/* Advance the description pointer information */
				if(--_descriptionIndex) {
					_description >>= 1;
					_leafDescription >>= 1;
				} else {
					_description = *_descriptionPtr;
					_descriptionPtr += 1;
					_descriptionIndex = J9BITS_BITS_IN_SLOT;
					_leafDescription = *_leafDescriptionPtr;
					_leafDescriptionPtr += 1;
				}
			}
		}
		/* No more object slots to scan */
		return NULL;
	}

	/**
	 * Return SlotObject for next non-leaf slot (or NULL if there are no more non-leaf slots)
	 * @return SlotObject
	 */
	MMINLINE GC_SlotObject *nextNonLeafSlot()
	{
		while(_scanPtr < _endPtr) {
			/* Record the slot information */
			if ((1 == (_description & 1)) && (0 == (_leafDescription & 1))) {
				_slotObject.writeAddressToSlot(_scanPtr);
				_scanPtr += 1;

				/* Advance the description pointer information */
				if(--_descriptionIndex) {
					_description >>= 1;
					_leafDescription >>= 1;
				} else {
					_description = *_descriptionPtr;
					_descriptionPtr += 1;
					_descriptionIndex = J9BITS_BITS_IN_SLOT;
					_leafDescription = *_leafDescriptionPtr;
					_leafDescriptionPtr += 1;
				}
				return &_slotObject;
			} else {
				_scanPtr += 1;

				/* Advance the description pointer information */
				if(--_descriptionIndex) {
					_description >>= 1;
					_leafDescription >>= 1;
				} else {
					_description = *_descriptionPtr;
					_descriptionPtr += 1;
					_descriptionIndex = J9BITS_BITS_IN_SLOT;
					_leafDescription = *_leafDescriptionPtr;
					_leafDescriptionPtr += 1;
				}
			}
		}
		/* No more object slots to scan */
		return NULL;
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
			_leafDescriptionPtr += slotsToMove;
			/* _descriptionPtr ALWAYS points at the NEXT slot of index bits so read the CURRENT by subtracting 1 */
			UDATA *currentDescriptionPointer = _descriptionPtr - 1;
			_description = *currentDescriptionPointer;
			/* use J9BITS_BITS_IN_SLOT as the "0" for _descriptionIndex since it uses backward math */
			_descriptionIndex = J9BITS_BITS_IN_SLOT;
			/* now, advance the number of bits required into this slot */
			_descriptionIndex -= newBitIndexAfterSlotMove;
			_description >>= newBitIndexAfterSlotMove;
			_leafDescription >>= newBitIndexAfterSlotMove;
		} else {
			Assert_MM_true(_descriptionIndex >= bitsToTravel);
			_descriptionIndex -= bitsToTravel;
			_description >>= bitsToTravel;
			_leafDescription >>= bitsToTravel;
		}
		/* save back the new slot ptr */
		_scanPtr = slotPtr;
		Assert_MM_true (_description & 1);
	}

	GC_LeafAwareMixedObjectIterator (J9JavaVM *vm, J9Object *objectPtr)
		: _slotObject(GC_SlotObject(vm->omrVM, NULL))
		, _objectPtr(NULL)
		, _scanPtr(NULL)
		, _endPtr(NULL)
		, _descriptionPtr(NULL)
		, _description(0)
		, _descriptionIndex(0)
		, _leafDescriptionPtr(NULL)
		, _leafDescription(0)
	  {
		_objectPtr = objectPtr;
		J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(objectPtr);
		/* Set up the description bits */
		UDATA tempDescription = (UDATA)clazzPtr->instanceDescription;
		UDATA tempLeafDescription = (UDATA)clazzPtr->instanceLeafDescription;
		if(tempDescription & 1) {
			_description = (tempDescription >> 1);
			Assert_MM_true(1 == (tempLeafDescription & 1));
			_leafDescription = (tempLeafDescription >> 1);
		} else {
			_descriptionPtr = (UDATA *)tempDescription;
			_description = *_descriptionPtr;
			_descriptionPtr += 1;
			Assert_MM_true(0 == (tempLeafDescription & 1));
			_leafDescriptionPtr = (UDATA *)tempLeafDescription;
			_leafDescription = *_leafDescriptionPtr;
			_leafDescriptionPtr += 1;
		}
		_descriptionIndex = J9BITS_BITS_IN_SLOT;

		/* Set current and end scan pointers */
		_scanPtr = (fj9object_t *)(objectPtr + 1);
		_endPtr = (fj9object_t *)((U_8*)_scanPtr +clazzPtr->totalInstanceSize);
	  }
};

#endif /* LEAFAWAREMIXEDOBJECTITERATOR_HPP_ */
