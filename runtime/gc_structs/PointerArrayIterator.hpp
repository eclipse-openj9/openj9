
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

#if !defined(POINTERARRAYITERATOR_HPP_)
#define POINTERARRAYITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ArrayObjectModel.hpp"
#include "Bits.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectIteratorState.hpp"
#include "PointerArrayletIterator.hpp"
#include "PointerContiguousArrayIterator.hpp"
#include "SlotObject.hpp"

/**
 * Iterate over all slots in a pointer array which contain an object reference
 * @ingroup GC_Structs
 */
class GC_PointerArrayIterator
{
private:
	bool _contiguous;
	GC_PointerContiguousArrayIterator _contiguousArrayIterator;
	GC_PointerArrayletIterator _pointerArrayletIterator;  /**< Iterator instance specific for Contiguous (standard) array */

protected:
public:
	/**
	 * @param objectPtr the array object to be processed
	 */
	GC_PointerArrayIterator(J9JavaVM *javaVM, J9Object *objectPtr)
		: _contiguousArrayIterator(javaVM->omrVM)
		, _pointerArrayletIterator(javaVM)
	{
		initialize(javaVM, objectPtr);
	}

	GC_PointerArrayIterator(J9JavaVM *javaVM)
		/* It is unnecessary to initialize one of those iterators */
		: _contiguousArrayIterator(javaVM->omrVM)
		, _pointerArrayletIterator(javaVM)
	{
	}

	MMINLINE void initialize(J9JavaVM *javaVM, J9Object *objectPtr)
	{
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(javaVM->omrVM);
		_contiguous = extensions->indexableObjectModel.isInlineContiguousArraylet((J9IndexableObject *)objectPtr);
		if (_contiguous) {
			_contiguousArrayIterator.initialize(objectPtr);
		} else {
			_pointerArrayletIterator.initialize(objectPtr);
		}
	}

	/**
	 * Restores the iterator state into this class
	 * @param[in] objectIteratorState partially scanned object iterator state
	 */
	MMINLINE void restore(GC_ObjectIteratorState *objectIteratorState)
	{
		_contiguous = objectIteratorState->_contiguous;
		if (_contiguous) {
			_contiguousArrayIterator.restore(objectIteratorState);
		} else {
			_pointerArrayletIterator.restore(objectIteratorState);
		}
	}

	/**
	 * Saves the partially scanned state of this class
	 * @param[in] objectIteratorState where to store the state
	 */
	MMINLINE void save(GC_ObjectIteratorState *objectIteratorState)
	{
		if (_contiguous) {
			_contiguousArrayIterator.save(objectIteratorState);
		} else {
			_pointerArrayletIterator.save(objectIteratorState);
		}
		objectIteratorState->_contiguous = _contiguous;
	}

	MMINLINE GC_SlotObject *nextSlot()
	{
		GC_SlotObject *slotObject = NULL;
		if (_contiguous) {
			slotObject = _contiguousArrayIterator.nextSlot();
		} else {
			slotObject = _pointerArrayletIterator.nextSlot();
		}
		return slotObject;
	}

	MMINLINE J9Object *getObject()
	{
		J9Object *objectPtr = NULL;
		if (_contiguous) {
			objectPtr = _contiguousArrayIterator.getObject();
		} else {
			objectPtr = _pointerArrayletIterator.getObject();
		}
		return (J9Object *)objectPtr;
	}

	/**
	 * Gets the current slot's array index.
	 * @return slot number (or zero based array index) of the last call to nextSlot.
	 * @return undefined if nextSlot has yet to be called.
	 */
	MMINLINE UDATA getIndex()
	{
		UDATA index = 0;
		if (_contiguous) {
			index = _contiguousArrayIterator.getIndex();
		} else {
			index = _pointerArrayletIterator.getIndex();
		}
		return index;
	}

	MMINLINE void setIndex(UDATA index)
	{
		if (_contiguous) {
			_contiguousArrayIterator.setIndex(index);
		} else {
			_pointerArrayletIterator.setIndex(index);
		}
	}
};

#endif /* POINTERARRAYITERATOR_HPP_ */
