
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

#if !defined(MIXEDOBJECTDECLARATIONORDERITERATOR_HPP_)
#define MIXEDOBJECTDECLARATIONORDERITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "MixedObjectModel.hpp"
#include "SlotObject.hpp"

#ifdef J9VM_THR_LOCK_NURSERY
#include "locknursery.h"
#endif

/**
 * Iterate over all slots in a mixed object which contain an object reference.
 * @note This iterator relies on a VM iterator which is not out of process safe, and consequently
 * it is also not out of process safe.
 * 
 * @ingroup GC_Structs
 */
class GC_MixedObjectDeclarationOrderIterator
{
protected:
	J9ROMFullTraversalFieldOffsetWalkState _walkState;
	J9ROMFieldShape *_fieldShape;
	J9JavaVM *_javaVM;
	J9Object *_objectPtr;
	GC_SlotObject _slotObject;
	IDATA _index;

public:
	GC_MixedObjectDeclarationOrderIterator(J9JavaVM *jvm, J9Object *objectPtr, bool shouldPreindexInterfaceFields) :
		_javaVM(jvm),
		_objectPtr(objectPtr),
		_slotObject(jvm->omrVM, NULL),
		_index(-1)
	{
		U_32 flags = J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS;
		if (shouldPreindexInterfaceFields) {
			flags |= J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS;
		}

		J9Class *clazz = J9GC_J9OBJECT_CLAZZ(objectPtr);
		_fieldShape = _javaVM->internalVMFunctions->fullTraversalFieldOffsetsStartDo(_javaVM, clazz, &_walkState, flags);
	}

	GC_SlotObject *nextSlot();

	/**
	 * Gets the current slot's declaration order index.
	 * The current slot's declaration order index is based on the indices of its superclass and superinterfaces.
	 * @return slot's declaration order index of the entry returned by the last call of nextSlot.
	 * @return -1 if nextSlot has yet to be called.
	 */
	MMINLINE IDATA getIndex() {
		return _index;
	}
};

#endif /* MIXEDOBJECTDECLARATIONORDERITERATOR_HPP_ */

