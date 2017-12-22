
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

#if !defined(ARRAYLETLEAFITERATOR_HPP_)
#define ARRAYLETLEAFITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

#include "GCExtensionsBase.hpp"
#include "SlotObject.hpp"
#include "ArrayObjectModel.hpp"
#include "ArrayletObjectModel.hpp"

#if defined(J9VM_GC_ARRAYLETS)
/**
 * Defines the interface for iterating over all slots in an object which contain an object reference
 * @ingroup GC_Structs
 */
class GC_ArrayletLeafIterator
{
protected:
	OMR_VM *const _omrVM;
	GC_SlotObject _slotObject;
	J9IndexableObject *_spinePtr; /**< The pointer to the beginning of the actual indexable object (ie: the spine) */
	GC_ArrayletObjectModel::ArrayLayout _layout; /**< The layout of the arraylet being iterated */
	fj9object_t *_arrayoid; /**< The pointer to the beginning of the arraylet leaf pointers (that is, the first slot after the object header) */
	UDATA _numLeafs; /**< The number of leaf pointers beginning at the _arrayoid.  This includes inline and out-of-line leaf pointers */
	UDATA _numLeafsCounted;
	void *_endOfSpine; /**< Pointer to the first slot AFTER the arraylet spine */

public:
	/**
	 * @return the next leaf reference slot in the arraylet
	 * @return NULL if there are no more reference slots in the object
	 */
	MMINLINE GC_SlotObject *nextLeafPointer()
	{
		if (_numLeafsCounted < _numLeafs) {
			_slotObject.writeAddressToSlot(&_arrayoid[_numLeafsCounted]);
			_numLeafsCounted += 1;
			return &_slotObject;
		} else {
			return NULL;
		}
	}
	
	MMINLINE void 
	initialize(J9IndexableObject *objectPtr)
	{
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
		_spinePtr = objectPtr;
		_layout = extensions->indexableObjectModel.getArrayLayout(objectPtr);
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		/* For a hybrid arraylet spec, this iterator should not be called for a contiguous arraylet */
		Assert_MM_true(GC_ArrayletObjectModel::InlineContiguous != _layout);
#endif
		/* for 0-sized arrays, there is no need to return the fake leaf pointer.
		 * It can potentially be problematic to return this fake leaf pointer as users of
		 * this iterator assume we return valid leaf pointers. 
		 */
		if (0 == extensions->indexableObjectModel.getSizeInElements(objectPtr)) {
			_arrayoid = NULL;
			_numLeafs = 0;
		} else {
			_arrayoid = extensions->indexableObjectModel.getArrayoidPointer(objectPtr);
			_numLeafs = extensions->indexableObjectModel.numArraylets(objectPtr);
		}
		_numLeafsCounted = 0;
		_endOfSpine = ((U_8 *)objectPtr) + extensions->indexableObjectModel.getSizeInBytesWithHeader(objectPtr);
	}
	
	MMINLINE UDATA getNumLeafs() { return _numLeafs ; }
	
	MMINLINE void *getEndOfSpine() { return _endOfSpine ; }

	GC_ArrayletLeafIterator(J9JavaVM *javaVM, J9IndexableObject *objectPtr) :
		_omrVM(javaVM->omrVM)
	,	_slotObject(GC_SlotObject(_omrVM, NULL))
	{
		initialize(objectPtr);
	}
};
#endif /* J9VM_GC_ARRAYLETS */

#endif /* ARRAYLETLEAFITERATOR_HPP_ */

