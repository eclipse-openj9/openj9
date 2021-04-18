/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#if !defined(FLATTENEDCONTIGUOUSARRAYITERATOR_HPP_)
#define FLATTENEDCONTIGUOUSARRAYITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "ArrayObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectIteratorState.hpp"
#include "SlotObject.hpp"
#include "MixedObjectIterator.hpp"

class GC_FlattenedContiguousArrayIterator
{
private:
	J9IndexableObject *_arrayPtr; /**< pointer to the array object being iterated */
	GC_MixedObjectIterator _mixedObjectIterator; /**< Object iterator which iterates over field of each element */
	uintptr_t _elementStride; /**< Size of each element, including padding */
	fj9object_t *_basePtr; /**< pointer to the first array slot, where element 0 is */
	fj9object_t *_scanPtr; /**< pointer to the current array element's first slot */
	fj9object_t *_endPtr; /**< pointer to the array slot where the iteration will terminate */
	J9Class *_elementClass; /**< Pointer to class of the elements in the flattened array */
protected:
	OMR_VM *_omrVM;
public:
	MMINLINE GC_SlotObject *nextSlot()
	{
		/* If no more object slots to scan, returns NULL */
		GC_SlotObject *result = NULL;
		if (_scanPtr < _endPtr) {
			result = _mixedObjectIterator.nextSlot();
			if (NULL == result) {
				_scanPtr = (fj9object_t *)(_elementStride + (uintptr_t)_scanPtr);
				if (_scanPtr < _endPtr) {
					/* mixedObjectIterator reached end of element. Move the iterator to the beginning of the next element */
					_mixedObjectIterator.initialize(_omrVM, _elementClass, _scanPtr);
					result = _mixedObjectIterator.nextSlot();
				}
			}
		}
		return result;
	}

	MMINLINE void initialize(J9Object *objectPtr)
	{
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
		GC_ArrayObjectModel *arrayObjectModel = &(extensions->indexableObjectModel);
		J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(objectPtr, extensions);

		_elementStride = J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		_basePtr = (fomrobject_t *)arrayObjectModel->getDataPointerForContiguous(_arrayPtr);
		_scanPtr = _basePtr;
		_endPtr = (fomrobject_t *)((uintptr_t)_basePtr + (arrayObjectModel->getSizeInElements(_arrayPtr) * _elementStride));
		_elementClass = ((J9ArrayClass *) clazzPtr)->componentType;

		if (_scanPtr < _endPtr) {
			_mixedObjectIterator.initialize(_omrVM, _elementClass, _scanPtr);
		}
	}

	/**
	 * Gets the current slot's array index.
	 * @return slot number (or zero based array index) of the last call to nextSlot.
	 */
	MMINLINE UDATA getIndex()
	{
		return ((uintptr_t)_scanPtr - (uintptr_t)_basePtr) / _elementStride;
	}

	/**
	 * Sets the current slot's array index
	 * @param[in] index index to set scan index to
	 */
	MMINLINE void setIndex(UDATA index) {
		_scanPtr = (fj9object_t *)((uintptr_t)_basePtr + (index * _elementStride));
	}

	/**
	 * Restores the iterator state into this class
	 * @param[in] objectIteratorState partially scanned object iterator state
	 */
	MMINLINE void restore(GC_ObjectIteratorState *objectIteratorState)
	{
		_scanPtr = objectIteratorState->_scanPtr;
		_endPtr = objectIteratorState->_endPtr;
	}

	/**
	 * Saves the partially scanned state of this class
	 * @param[in] objectIteratorState where to store the state
	 */
	MMINLINE void save(GC_ObjectIteratorState *objectIteratorState)
	{
		objectIteratorState->_scanPtr = _scanPtr;
		objectIteratorState->_endPtr  = _endPtr;
	}

	MMINLINE J9Object *getObject()
	{
		return (J9Object *)_arrayPtr;
	}

	GC_FlattenedContiguousArrayIterator(OMR_VM *omrVM, J9Object *objectPtr)
		: _arrayPtr((J9IndexableObject *)objectPtr)
		, _mixedObjectIterator(omrVM)
		, _elementStride(0)
		, _basePtr(NULL)
		, _scanPtr(NULL)
		, _endPtr(NULL)
		, _elementClass(NULL)
		, _omrVM(omrVM)
	{
		initialize(objectPtr);
	}
};

#endif /* FLATTENEDCONTIGUOUSARRAYITERATOR_HPP_ */
