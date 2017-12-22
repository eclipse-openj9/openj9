
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

#include "ClassIteratorDeclarationOrder.hpp"

/**
 * Gets the next slot in the class containing an object reference.
 * @return the next slot in the class containing an object reference
 * @return NULL if there are no more such slots
 */
volatile j9object_t *
GC_ClassIteratorDeclarationOrder::nextSlot()
{
	volatile j9object_t *slotPtr;

	if (classiterator_state_statics != getState()) {
		slotPtr = GC_ClassIterator::nextSlot();
		/* catch the initial switch to the statics state */
		if (classiterator_state_statics != getState()) {
			return slotPtr;
		}
	}

	slotPtr = _classStaticsDeclarationOrderIterator.nextSlot();
	if(NULL != slotPtr) {
		return slotPtr;
	}
	_state += 1;

	return GC_ClassIterator::nextSlot();
}
