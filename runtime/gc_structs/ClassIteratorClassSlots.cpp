
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Structs
 */

#include "j9.h"
#include "j9cfg.h"

#include "ClassIteratorClassSlots.hpp"

/**
 * @return the next non-NULL class reference
 * @return NULL if there are no more such references
 */
J9Class *
GC_ClassIteratorClassSlots::nextSlot()
{
	J9Class *classPtr;

	switch(_state) {
	case classiteratorclassslots_state_start:
		_state += 1;

	case classiteratorclassslots_state_constant_pool:
		classPtr = _constantPoolClassSlotIterator.nextSlot();
		if (NULL != classPtr) {
			return classPtr;
		}
		_state += 1;

	case classiteratorclassslots_state_superclasses:
		classPtr = _classSuperclassesIterator.nextSlot();
		if (NULL != classPtr) {
			return classPtr;
		}
		_state += 1;

	case classiteratorclassslots_state_interfaces:
		/*
		 * Checking sharedITable is an optimization that only checks booleanArrayClass Interfaces
		 * since all array claseses share the same ITable.
		 */
		if (_shouldScanInterfaces) {
			classPtr = _classLocalInterfaceIterator.nextSlot();
			if (NULL != classPtr) {
				return classPtr;
			}
		}
		_state += 1;

	case classiteratorclassslots_state_array_class_slots:
		classPtr = _classArrayClassSlotIterator.nextSlot();
		if (NULL != classPtr) {
			return classPtr;
		}
		_state += 1;

	case classiteratorclassslots_state_flattened_class_cache_slots:
		classPtr = _classFCCSlotIterator.nextSlot();
		if (NULL != classPtr) {
			return classPtr;
		}
		_state += 1;

	case classiteratorclassslots_state_end:
	default:
		break;
	}

	return NULL;
}

