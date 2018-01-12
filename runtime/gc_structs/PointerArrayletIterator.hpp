
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

#if !defined(POINTERARRAYLETITERATOR_HPP_)
#define POINTERARRAYLETITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ArrayObjectModel.hpp"
#include "Bits.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectIteratorState.hpp"
#include "SlotObject.hpp"
#include "ArrayletObjectModel.hpp"

#if defined(J9VM_GC_ARRAYLETS)


/**
 * Iterate over all slots in a pointer array which contain an object reference
 * @ingroup GC_Structs
 */
class GC_PointerArrayletIterator
{
private:
	J9IndexableObject *_arrayPtr;	/**< pointer to the array object being iterated */
	GC_SlotObject _slotObject;		/**< Create own SlotObject class to provide output */

	UDATA const _arrayletLeafSize; 		/* The size of an arraylet leaf */
	UDATA const _fobjectsPerLeaf; 		/* The number of fj9object_t's per leaf */
	UDATA _index; 					/* The current index of the array */
	fj9object_t * _currentArrayletBaseAddress; /* The base address of the current arraylet */
	UDATA _currentArrayletIndex; 	/* The index of the current arraylet */
	UDATA _currentArrayletOffset; 	/* The offset to the _index'ed item from the currentArrayletBaseAddress */

	MMINLINE void recalculateArrayletOffsets()
	{
		if ( _index > 0 ) {
			MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_javaVM->omrVM);
			_currentArrayletIndex = ((U_32)_index-1) / _fobjectsPerLeaf; /* The index of the arraylet containing _index */
			_currentArrayletOffset = ((U_32)_index-1) % _fobjectsPerLeaf; /* The offset of _index in the current arraylet */

			fj9object_t *arrayoidPointer = extensions->indexableObjectModel.getArrayoidPointer(_arrayPtr);
			GC_SlotObject arrayletBaseSlotObject(_javaVM->omrVM, arrayoidPointer + _currentArrayletIndex);
			_currentArrayletBaseAddress = (fj9object_t *)arrayletBaseSlotObject.readReferenceFromSlot();

			/* note that we need to check the arraylet base address before reading it since we might be walking
			 * a partially allocated arraylet which still has some NULL leaf pointers.  We return NULL, in those
			 * cases, which will cause the caller to stop iterating.  This is correct behaviour if we can assume
			 * that arraylet leaves are always allocated in-order.  This is currently true.
			 * Moreover, we can safely assume that an incomplete arraylet contains no non-null object references
			 * since it hasn't yet been returned from its allocation routine so it isn't yet visible to the mutator.
			 */
			if (NULL == _currentArrayletBaseAddress) {
				_index = 0;
			}
		}
	}

protected:
	J9JavaVM *const _javaVM;	/**< A cached pointer to the shared JavaVM instance */

public:
	MMINLINE void initialize(J9Object *objectPtr) {
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_javaVM->omrVM);

#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		/* if we are using hybrid arraylets, we need to ensure that we don't initialize the discontiguous iterator since it will try to read off the end of the header
		 * when reading the non-existent arrayoid.  In the case of small arrays near the end of a region, this could cause us to read an uncommitted page.
		 */
		if (extensions->indexableObjectModel.isInlineContiguousArraylet((J9IndexableObject *)objectPtr)) {
			_arrayPtr = NULL;
			_index = 0;
		} else
#endif /* defined(J9VM_GC_HYBRID_ARRAYLETS) */
		{
			_arrayPtr = (J9IndexableObject *)objectPtr;

			/* Set current and end scan pointers */
			_index = extensions->indexableObjectModel.getSizeInElements(_arrayPtr);
			recalculateArrayletOffsets();
		}
	}

	MMINLINE GC_SlotObject *nextSlot()
	{
		if (_index > 0) {
			_slotObject.writeAddressToSlot(_currentArrayletBaseAddress + _currentArrayletOffset);

			_index -= 1;
			/* If we reach the end of the arraylet leaf, we need to recalculate our arrayletBaseAddress for the next _index */
			if ( 0 == _currentArrayletOffset) {
				recalculateArrayletOffsets();
			} else {
				_currentArrayletOffset -= 1;
			}

			return &_slotObject;
		}

		/* No more object slots to scan */
		return NULL;
}

	GC_PointerArrayletIterator(J9JavaVM *javaVM)
		: _arrayPtr(NULL)
		, _slotObject(GC_SlotObject(javaVM->omrVM, NULL))
		, _arrayletLeafSize(javaVM->arrayletLeafSize)
		, _fobjectsPerLeaf(_arrayletLeafSize/sizeof(fj9object_t))
		, _index(0)
		, _currentArrayletBaseAddress(NULL)
		, _currentArrayletIndex(0)
		, _currentArrayletOffset(0)
		, _javaVM(javaVM)
	{
	}

	/**
	 * @param objectPtr the array object to be processed
	 */
	GC_PointerArrayletIterator(J9JavaVM *javaVM, J9Object *objectPtr)
		: _arrayPtr(NULL)
		, _slotObject(GC_SlotObject(javaVM->omrVM, NULL))
		, _arrayletLeafSize(javaVM->arrayletLeafSize)
		, _fobjectsPerLeaf(_arrayletLeafSize/sizeof(fj9object_t))
		, _index(0)
		, _currentArrayletBaseAddress(NULL)
		, _currentArrayletIndex(0)
		, _currentArrayletOffset(0)
		, _javaVM(javaVM)
	{
		initialize(objectPtr);
	}

	MMINLINE J9Object *getObject()
	{
		return (J9Object *)_arrayPtr;
	}

	/**
	 * Gets the current slot's array index.
	 * @return slot number (or zero based array index) of the last call to nextSlot.
	 * @return undefined if nextSlot has yet to be called.
	 */
	MMINLINE UDATA getIndex() {
		return _index;
	}


	MMINLINE void setIndex(UDATA index) {
		_index = index;
		recalculateArrayletOffsets();
	}

	/**
	 * Restores the iterator state into this class
	 * @param[in] objectIteratorState partially scanned object iterator state
	 */
	MMINLINE void restore(GC_ObjectIteratorState *objectIteratorState) {
		_arrayPtr = (J9IndexableObject *)objectIteratorState->_objectPtr;
		setIndex(objectIteratorState->_index);
	}


	/**
	 * Saves the partially scanned state of this class
	 * @param[in] objectIteratorState where to store the state
	 */
	MMINLINE void save(GC_ObjectIteratorState *objectIteratorState) {
		objectIteratorState->_objectPtr = (J9Object *)_arrayPtr;
		objectIteratorState->_index = _index;
	}
};

#endif /* defined(J9VM_GC_ARRAYLETS) */


#endif /* POINTERARRAYLETITERATOR_HPP_ */
