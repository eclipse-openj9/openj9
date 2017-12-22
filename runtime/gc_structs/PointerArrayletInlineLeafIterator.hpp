
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

#if !defined(POINTERARRAYLETINLINELEAFITERATOR_HPP_)
#define POINTERARRAYLETINLINELEAFITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ArrayObjectModel.hpp"
#include "Bits.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectIteratorState.hpp"
#include "SlotObject.hpp"

#if defined(J9VM_GC_ARRAYLETS)


/**
 * Iterate over all slots in a pointer array's inline leaf which contain an object reference (will iterate over nothing if the object is fully discontiguous)
 * @ingroup GC_Structs
 */
class GC_PointerArrayletInlineLeafIterator
{
	/* Data Members */
private:
	GC_SlotObject _slotObject;		/**< Create own SlotObject class to provide output */

	UDATA _arrayletLeafSize; 		/* The size of an arraylet leaf */
	UDATA _fobjectsPerLeaf; 		/* The number of fj9object_t's per leaf */
	fj9object_t * _currentArrayletBaseAddress; /* The base address of the current arraylet */
	UDATA _currentArrayletOffset; 	/* The offset to the _index'ed item from the currentArrayletBaseAddress */
	UDATA _elementsStillToRead;		/**< The number of elements this iterator is still expecting to return */
	J9JavaVM *_javaVM;				/**< The JavaVM */
protected:
public:

	/* Member Functions */
private:
protected:
public:
	MMINLINE void initialize(J9Object *objectPtr) {
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_javaVM->omrVM);

		J9IndexableObject *arrayPtr = (J9IndexableObject *)objectPtr;

		/* Set current and end scan pointers */
		UDATA index = extensions->indexableObjectModel.getSizeInElements(arrayPtr);
		if(0 == index) {
			_currentArrayletOffset = 0;
			_currentArrayletBaseAddress = NULL;
			_elementsStillToRead = 0;
		} else {
			UDATA currentArrayletIndex = ((U_32)index-1) / _fobjectsPerLeaf; /* The index of the arraylet containing _index */
			_currentArrayletOffset = ((U_32)index-1) % _fobjectsPerLeaf; /* The offset of _index in the current arraylet */
	
			fj9object_t *arrayoidPointer = extensions->indexableObjectModel.getArrayoidPointer(arrayPtr);
			GC_SlotObject arrayletBaseSlotObject(_javaVM->omrVM, arrayoidPointer + currentArrayletIndex);
			_currentArrayletBaseAddress = (fj9object_t *)arrayletBaseSlotObject.readReferenceFromSlot();
			_elementsStillToRead = index % _fobjectsPerLeaf;
		}
	}

	MMINLINE GC_SlotObject *nextSlot()
	{
		GC_SlotObject *slotObject = NULL;
		
		if (_elementsStillToRead > 0) {
			_slotObject.writeAddressToSlot(_currentArrayletBaseAddress + _currentArrayletOffset);

			_elementsStillToRead -= 1;
			_currentArrayletOffset -= 1;

			slotObject = &_slotObject;
		}
		return slotObject;
}

	GC_PointerArrayletInlineLeafIterator(J9JavaVM *javaVM)
		: _slotObject(GC_SlotObject(javaVM->omrVM, NULL))
		, _javaVM(javaVM)
	{
		_arrayletLeafSize = _javaVM->arrayletLeafSize;
		_fobjectsPerLeaf = _arrayletLeafSize/sizeof(fj9object_t);
	}

	/**
	 * @param objectPtr the array object to be processed
	 */
	GC_PointerArrayletInlineLeafIterator(J9JavaVM *javaVM, J9Object *objectPtr)
		: _slotObject(GC_SlotObject(javaVM->omrVM, NULL))
		, _javaVM(javaVM)
	{
		_arrayletLeafSize = _javaVM->arrayletLeafSize;
		_fobjectsPerLeaf = _arrayletLeafSize/sizeof(fj9object_t);
		initialize(objectPtr);
	}
};

#endif /* defined(J9VM_GC_ARRAYLETS) */


#endif /* POINTERARRAYLETINLINELEAFITERATOR_HPP_ */
