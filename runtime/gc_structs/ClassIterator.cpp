
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "ClassIterator.hpp"

/**
 * Offsets of the slots in J9Class that contain object references.
 * NOTE: This can probably be simplified since we shouldn't be adding any more object
 * references to the J9Class struct
 */
static const UDATA slotOffsets[] = 
{
	offsetof(J9Class, classObject),
	0
};

/**
 * @return the next slot in the class containing an object reference
 * @return NULL if there are no more such slots
 */
volatile j9object_t *
GC_ClassIterator::nextSlot()
{
	volatile j9object_t *slotPtr;

	switch(_state) {
	case classiterator_state_start:
		_state += 1;

	case classiterator_state_statics:
		slotPtr = _classStaticsIterator.nextSlot();
		if (NULL != slotPtr) {
			return slotPtr;
		}
		_state += 1;

	case classiterator_state_constant_pool:
		slotPtr = _constantPoolObjectSlotIterator.nextSlot();
		if (NULL != slotPtr) {
			return slotPtr;
		}
		_state += 1;

	case classiterator_state_slots:
		while (slotOffsets[_scanIndex] != 0) {
			/* shouldScanClassObject is true by default, in the case of balanced GC check if object is ClassObject */
			if (_shouldScanClassObject || (slotOffsets[_scanIndex] != offsetof(J9Class, classObject))) {
				return (j9object_t *)((U_8 *)_clazzPtr + (UDATA)slotOffsets[_scanIndex++]);
			} else {
				_scanIndex += 1;
			}
		}
		_state += 1;

	case classiterator_state_callsites:
		slotPtr = _callSitesIterator.nextSlot();
		if (NULL != slotPtr) {
			return slotPtr;
		}
		_state += 1;

	case classiterator_state_methodtypes:
		slotPtr = _methodTypesIterator.nextSlot();
		if (NULL != slotPtr) {
			return slotPtr;
		}
		_state += 1;

	case classiterator_state_varhandlemethodtypes:
		slotPtr = _varHandlesMethodTypesIterator.nextSlot();
		if (NULL != slotPtr) {
			return slotPtr;
		}
		_state += 1;

	default:
		break;
	}

	return NULL;
}

