
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

#if !defined(CLASSSTATICSDECLARATIONORDERITERATOR_HPP_)
#define CLASSSTATICSDECLARATIONORDERITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#ifdef J9VM_THR_LOCK_NURSERY
#include "locknursery.h"
#endif

#include "GCExtensions.hpp"

/**
 * Iterate over all static slots in a class which contain an object reference.
 * @note This iterator relies on a VM iterator which is not out of process safe, and consequently
 * it is also not out of process safe.
 * @ingroup GC_Structs
 */
class GC_ClassStaticsDeclarationOrderIterator
{
	J9ROMFullTraversalFieldOffsetWalkState _walkState;
	J9ROMFieldShape *_fieldShape;
	J9JavaVM *_javaVM;
	J9Class *_clazz;
	IDATA _index;

public:
	GC_ClassStaticsDeclarationOrderIterator(J9JavaVM *jvm, J9Class *clazz, bool shouldPreindexInterfaceFields) 
		: _javaVM(jvm)
		, _clazz(clazz)
		, _index(-1)
	{
		U_32 flags = J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS;
		if (shouldPreindexInterfaceFields) {
			flags |= J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS;
		}

		_fieldShape = _javaVM->internalVMFunctions->fullTraversalFieldOffsetsStartDo(_javaVM, _clazz, &_walkState, flags);
	};

	j9object_t *nextSlot();

	/**
	 * Gets the current static slot index.
	 * The current static slot index is based of the last static slot index of the superclass.
	 * @return static slot index of the entry returned by the last call of nextSlot.
	 * @return -1 if nextSlot has yet to be called.
	 */
	MMINLINE IDATA getIndex() {
		return _index;
	}
};

#endif /* CLASSSTATICSDECLARATIONORDERITERATOR_HPP_ */

