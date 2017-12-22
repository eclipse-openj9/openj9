
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

#if !defined(POINTERARRAYITERATOR_HPP_)
#define POINTERARRAYITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ArrayObjectModel.hpp"
#include "Bits.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectIteratorState.hpp"
#if defined(J9VM_GC_ARRAYLETS)
#include "PointerArrayletIterator.hpp"
#endif
#if !defined(J9VM_GC_ARRAYLETS) || defined(J9VM_GC_HYBRID_ARRAYLETS)
#include "PointerContiguousArrayIterator.hpp"
#endif /* !defined(J9VM_GC_ARRAYLETS) || defined(J9VM_GC_HYBRID_ARRAYLETS) */
#include "SlotObject.hpp"

/**
 * Iterate over all slots in a pointer array which contain an object reference
 * @ingroup GC_Structs
 */
class GC_PointerArrayIterator
{
private:
#if !defined(J9VM_GC_ARRAYLETS)
	GC_PointerContiguousArrayIterator _contiguousArrayIterator;	/**< Iterator instance specific for Contiguous (standard) array */
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
	bool _contiguous;
	GC_PointerContiguousArrayIterator _contiguousArrayIterator;
	GC_PointerArrayletIterator _pointerArrayletIterator;		/**< Iterator instance specific for Contiguous (standard) array */
#else
	GC_PointerArrayletIterator _pointerArrayletIterator;		/**< Iterator instance specific for Discontiguous array (arraylet)*/
#endif

protected:
public:
	/**
	 * @param objectPtr the array object to be processed
	 */
	GC_PointerArrayIterator(J9JavaVM *javaVM, J9Object *objectPtr)
#if !defined(J9VM_GC_ARRAYLETS)
	  :_contiguousArrayIterator(javaVM->omrVM)
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
	:_contiguousArrayIterator(javaVM->omrVM)
	,_pointerArrayletIterator(javaVM)
#else
	  :_pointerArrayletIterator(javaVM)
#endif
	{
		initialize(javaVM, objectPtr);
	}

	GC_PointerArrayIterator(J9JavaVM *javaVM)
#if !defined(J9VM_GC_ARRAYLETS)
	  :_contiguousArrayIterator(javaVM->omrVM)
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
	/* It is unnecessary to initialize one of those iterators */
	:_contiguousArrayIterator(javaVM->omrVM)
	,_pointerArrayletIterator(javaVM)
#else
	  :_pointerArrayletIterator(javaVM)
#endif
	{
	}

	MMINLINE void initialize(J9JavaVM *javaVM, J9Object *objectPtr)
	{
#if defined(J9VM_GC_ARRAYLETS)
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(javaVM->omrVM);
		_contiguous = extensions->indexableObjectModel.isInlineContiguousArraylet((J9IndexableObject *)objectPtr);
		if (_contiguous) {
			_contiguousArrayIterator.initialize(objectPtr);
		} else {
			_pointerArrayletIterator.initialize(objectPtr);
		}
#else
		_pointerArrayletIterator.initialize(objectPtr);
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
#else
		_contiguousArrayIterator.initialize(objectPtr);
#endif /* J9VM_GC_ARRAYLETS */
	}

	/**
	 * Restores the iterator state into this class
	 * @param[in] objectIteratorState partially scanned object iterator state
	 */
	MMINLINE void restore(GC_ObjectIteratorState *objectIteratorState)
	{
#if defined(J9VM_GC_ARRAYLETS)
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		_contiguous = objectIteratorState->_contiguous;
		if (_contiguous) {
			_contiguousArrayIterator.restore(objectIteratorState);
		} else {
			_pointerArrayletIterator.restore(objectIteratorState);
		}
#else
		_pointerArrayletIterator.restore(objectIteratorState);
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
#else
		_contiguousArrayIterator.restore(objectIteratorState);
#endif /* J9VM_GC_ARRAYLETS */
	}

	/**
	 * Saves the partially scanned state of this class
	 * @param[in] objectIteratorState where to store the state
	 */
	MMINLINE void save(GC_ObjectIteratorState *objectIteratorState)
	{
#if defined(J9VM_GC_ARRAYLETS)
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (_contiguous) {
			_contiguousArrayIterator.save(objectIteratorState);
		} else {
			_pointerArrayletIterator.save(objectIteratorState);
		}
		objectIteratorState->_contiguous = _contiguous;
#else
		_pointerArrayletIterator.save(objectIteratorState);
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
#else
		_contiguousArrayIterator.save(objectIteratorState);
#endif /* J9VM_GC_ARRAYLETS */
	}

	MMINLINE GC_SlotObject *nextSlot()
	{
		GC_SlotObject *slotObject = NULL;

#if !defined(J9VM_GC_ARRAYLETS)
		slotObject = _contiguousArrayIterator.nextSlot();
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (_contiguous) {
			slotObject = _contiguousArrayIterator.nextSlot();
		} else {
			slotObject = _pointerArrayletIterator.nextSlot();
		}
#else
		slotObject = _pointerArrayletIterator.nextSlot();
#endif

		return slotObject;
	}

	MMINLINE J9Object *getObject()
	{
		J9Object *objectPtr = NULL;

#if !defined(J9VM_GC_ARRAYLETS)
		objectPtr = _contiguousArrayIterator.getObject();
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (_contiguous) {
			objectPtr = _contiguousArrayIterator.getObject();
		} else {
			objectPtr = _pointerArrayletIterator.getObject();
		}
#else
		objectPtr = _pointerArrayletIterator.getObject();
#endif

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

#if !defined(J9VM_GC_ARRAYLETS)
		index = _contiguousArrayIterator.getIndex();
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (_contiguous) {
			index = _contiguousArrayIterator.getIndex();
		} else {
			index = _pointerArrayletIterator.getIndex();
		}
#else
		index = _pointerArrayletIterator.getIndex();
#endif

		return index;
	}


	MMINLINE void setIndex(UDATA index)
	{
#if !defined(J9VM_GC_ARRAYLETS)
		_contiguousArrayIterator.setIndex(index);
#elif defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (_contiguous) {
			_contiguousArrayIterator.setIndex(index);
		} else {
			_pointerArrayletIterator.setIndex(index);
		}
#else
		_pointerArrayletIterator.setIndex(index);
#endif
	}

};

#endif /* POINTERARRAYITERATOR_HPP_ */
