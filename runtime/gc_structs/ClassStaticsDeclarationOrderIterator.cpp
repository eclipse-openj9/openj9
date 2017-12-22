
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

#include "j9.h"
#include "j9cfg.h"
#include "j9cp.h"

#include "ClassStaticsDeclarationOrderIterator.hpp"

/**
 * Gets the next static slot in the class containing an object reference.
 * @return the next static slot in the class containing an object reference
 * @return NULL if there are no more such slots
 */
j9object_t *
GC_ClassStaticsDeclarationOrderIterator::nextSlot()
{
	j9object_t *slot;

	while (NULL != _fieldShape) {
		/* In order to maintain the correct index of the static fields for this class we need to walk all the
		 * super classes (which fullTraversalFieldOffsetsNextDo does) however we also should only be reporting
		 * slots which come from this class, so we do the check here.
		 */
		if (_clazz == _walkState.currentClass) {
			slot = (j9object_t *) ((U_8 *)_clazz->ramStatics + _walkState.fieldOffsetWalkState.result.offset);
			_index = _walkState.referenceIndexOffset + _walkState.classIndexAdjust + _walkState.fieldOffsetWalkState.result.index - 1;
			_fieldShape = _javaVM->internalVMFunctions->fullTraversalFieldOffsetsNextDo(&_walkState);
			return slot;
		}
		_fieldShape = _javaVM->internalVMFunctions->fullTraversalFieldOffsetsNextDo(&_walkState);
	}
	
	return NULL;
}
