
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
#include "j9cp.h"
#include "ModronAssertions.h"

#include "ConstantPoolObjectSlotIterator.hpp"


/**
 * @return the next object slot in the constant pool
 * @return NULL if there are no more object references
 */
j9object_t *
GC_ConstantPoolObjectSlotIterator::nextSlot()
{
	U_32 slotType;
	j9object_t *slotPtr;
	j9object_t *result = NULL;

	while(_cpEntryCount) {
		if(0 == _cpDescriptionIndex) {
			_cpDescription = *_cpDescriptionSlots;
			_cpDescriptionSlots += 1;
			_cpDescriptionIndex = J9_CP_DESCRIPTIONS_PER_U32;
		}

		slotType = _cpDescription & J9_CP_DESCRIPTION_MASK;
		slotPtr = _cpEntry;

		/* Adjust the CP slot and description information */
		_cpEntry = (j9object_t *)( ((U_8 *)_cpEntry) + sizeof(J9RAMConstantPoolItem) );
		_cpEntryCount -= 1;

		_cpDescription >>= J9_CP_BITS_PER_DESCRIPTION;
		_cpDescriptionIndex -= 1;

		/* Determine if the slot should be processed */
		switch (slotType) {
			case J9CPTYPE_STRING: /* fall through */
			case J9CPTYPE_ANNOTATION_UTF8:
				result = &(((J9RAMStringRef *) slotPtr)->stringObject);
				break;
			case J9CPTYPE_METHOD_TYPE:
				result = &(((J9RAMMethodTypeRef *) slotPtr)->type);
				break;
			case J9CPTYPE_METHODHANDLE:
				result = &(((J9RAMMethodHandleRef *) slotPtr)->methodHandle);
				break;
			case J9CPTYPE_CONSTANT_DYNAMIC:
				if (NULL != ((J9RAMConstantDynamicRef *) slotPtr)->value) {
					result = &(((J9RAMConstantDynamicRef *) slotPtr)->value);
				} else {
					result = &(((J9RAMConstantDynamicRef *) slotPtr)->exception);
				}
				break;
			default:
				continue;
		}

		return result;
	}
	return NULL;
}

bool
GC_ConstantPoolObjectSlotIterator::isSlotInConstantPool(j9object_t *slot)
{
	/* we can only call this if nextSlot has not yet been called */
	/* TODO (JAZZ 47325): Fix how we link GCCheck such that it can find these trace symbols (otherwise, uncommenting these will cause link failures on AIX and z/OS)
	Assert_MM_true(_cpEntryTotal == _cpEntryCount);
	Assert_MM_true(0 == _cpDescriptionIndex);
	*/

	J9RAMConstantPoolItem *newPointer = (J9RAMConstantPoolItem *)slot;
	J9RAMConstantPoolItem *oldPointer = (J9RAMConstantPoolItem *)_cpEntry;
	bool isInConstantPool = false;
	if (newPointer >= oldPointer) {
		UDATA entriesToSkip = (newPointer - oldPointer);
		isInConstantPool = (entriesToSkip <= _cpEntryCount);
	}
	return isInConstantPool;
}

void
GC_ConstantPoolObjectSlotIterator::setNextSlot(j9object_t *slot)
{
	/* we can only call this if nextSlot has not yet been called */
	/* TODO (JAZZ 47325): Fix how we link GCCheck such that it can find these trace symbols (otherwise, uncommenting these will cause link failures on AIX and z/OS)
	Assert_MM_true(_cpEntryTotal == _cpEntryCount);
	Assert_MM_true(0 == _cpDescriptionIndex);
	*/

	J9RAMConstantPoolItem *newPointer = (J9RAMConstantPoolItem *)slot;
	J9RAMConstantPoolItem *oldPointer = (J9RAMConstantPoolItem *)_cpEntry;
	/* TODO (JAZZ 47325): Fix how we link GCCheck such that it can find these trace symbols (otherwise, uncommenting these will cause link failures on AIX and z/OS)
	Assert_MM_true(newPointer >= oldPointer);
	*/

	UDATA entriesToSkip = (newPointer - oldPointer);
	UDATA descriptionSlotsToAdvance = entriesToSkip / J9_CP_DESCRIPTIONS_PER_U32;
	UDATA offsetIntoDescription = entriesToSkip % J9_CP_DESCRIPTIONS_PER_U32;

	_cpDescriptionSlots += descriptionSlotsToAdvance;
	_cpDescription = *_cpDescriptionSlots;
	_cpDescriptionSlots += 1;
	_cpDescription >>= (offsetIntoDescription * J9_CP_BITS_PER_DESCRIPTION);
	_cpDescriptionIndex = J9_CP_DESCRIPTIONS_PER_U32 - offsetIntoDescription;
	_cpEntryCount -= (U_32)entriesToSkip;
	_cpEntry = slot;
}
