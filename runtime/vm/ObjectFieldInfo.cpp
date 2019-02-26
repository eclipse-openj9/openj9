/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "j9.h"
#include "j9comp.h"
#include "j9modifiers_api.h"
#include "ObjectFieldInfo.hpp"
#include "util_api.h"
#include "vm_api.h"

/**
 * Calculate the number of various types of fields.
 * Make a copy of the hidden fields.  The first element in the output array is the last added.
 * Only single fields and 32-bit object references are candidates for backfill.
 */
void
ObjectFieldInfo::countInstanceFields(void)
{
	struct J9ROMFieldWalkState fieldWalkState;

	memset(&fieldWalkState,0, sizeof(fieldWalkState));
	/* iterate over fields to count instance fields by size */

	J9ROMFieldShape *field = romFieldsStartDo(_romClass, &fieldWalkState);
	while (NULL != field) {
		const U_32 modifiers = field->modifiers;
		if (J9_ARE_NO_BITS_SET(modifiers, J9AccStatic) ) {
			if (modifiers & J9FieldFlagObject) {
#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
				J9UTF8 *fieldSig = J9ROMFIELDSHAPE_SIGNATURE(field);
				U_8 *fieldSigBytes = J9UTF8_DATA(J9ROMFIELDSHAPE_SIGNATURE(field));
				if ('Q' == *fieldSigBytes) {
					J9Class *fieldClass = NULL;
					fieldClass = findJ9ClassInFlattenedClassCache(_flattenedClassCache, fieldSigBytes + 1, J9UTF8_LENGTH(fieldSig) - 2);
					if (J9_ARE_NO_BITS_SET(fieldClass->classFlags, J9ClassIsFlattened)) {
						_instanceObjectCount += 1;
						_totalObjectCount += 1;
					} else if (J9_ARE_ALL_BITS_SET(fieldClass->classFlags, J9ClassLargestAlignmentConstraintDouble)) {
						_totalFlatFieldDoubleBytes += (U_32) ROUND_UP_TO_POWEROF2(fieldClass->totalInstanceSize - fieldClass->backfillOffset, sizeof(U_64));
					} else if (J9_ARE_ALL_BITS_SET(fieldClass->classFlags, J9ClassLargestAlignmentConstraintReference)) {
						_totalFlatFieldRefBytes += (U_32) ROUND_UP_TO_POWEROF2(fieldClass->totalInstanceSize - fieldClass->backfillOffset, sizeof(fj9object_t));
					} else {
						_totalFlatFieldSingleBytes += (U_32) (fieldClass->totalInstanceSize - fieldClass->backfillOffset);
					}
				} else
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
				{
					_instanceObjectCount += 1;
					_totalObjectCount += 1;
				}
			} else if (modifiers & J9FieldSizeDouble) {
				_instanceDoubleCount += 1;
				_totalDoubleCount += 1;
			} else {
				_instanceSingleCount += 1;
				_totalSingleCount += 1;
			}
		}
		field = romFieldsNextDo(&fieldWalkState);
	}
	if (isContendedClassLayout()) { /* all instance fields are contended. Reassign them and fix the total counts */
		_contendedObjectCount = _instanceObjectCount;
		_totalObjectCount -= _instanceObjectCount;
		_instanceObjectCount = 0;
		_contendedDoubleCount = _instanceDoubleCount;
		_totalDoubleCount -= _instanceDoubleCount;
		_instanceDoubleCount = 0;
		_contendedSingleCount = _instanceSingleCount;
		_totalSingleCount -= _instanceSingleCount;
		_instanceSingleCount = 0;
	} else {
		_instanceFieldBackfillEligible = (_instanceSingleCount > 0) || (_objectCanUseBackfill && (_instanceSingleCount > 0));
	}
}

U_32
ObjectFieldInfo::countAndCopyHiddenFields(J9HiddenInstanceField *hiddenFieldList, J9HiddenInstanceField *hiddenFieldArray[])
{
	const J9UTF8 *className = J9ROMCLASS_CLASSNAME(_romClass);

	_hiddenFieldCount = 0;
	for (J9HiddenInstanceField *hiddenField = hiddenFieldList; NULL != hiddenField; hiddenField = hiddenField->next) {
		if ((NULL == hiddenField->className) || J9UTF8_EQUALS(hiddenField->className, className)) {
			const U_32 modifiers = hiddenField->shape->modifiers;
			const bool thisFieldUnresolved = ((UDATA)-1 == hiddenField->fieldOffset);

			_hiddenFieldOffsetResolutionRequired |= thisFieldUnresolved;
			if (J9_ARE_ANY_BITS_SET(modifiers, J9FieldFlagObject)) {
				_totalObjectCount += 1;
			} else if (J9_ARE_ANY_BITS_SET(modifiers, J9FieldSizeDouble)) {
				_totalDoubleCount += 1;
			} else {
				_totalSingleCount += 1;
			}
			hiddenFieldArray[_hiddenFieldCount] = hiddenField;
			_hiddenFieldCount += 1;
		}
	}
	return _hiddenFieldCount;
}

U_32
ObjectFieldInfo::calculateTotalFieldsSizeAndBackfill()
{
	U_32 accumulator = 0;
	if (isContendedClassLayout()) {
		_superclassBackfillOffset = NO_BACKFILL_AVAILABLE;
		_myBackfillOffset = NO_BACKFILL_AVAILABLE;
		_subclassBackfillOffset = NO_BACKFILL_AVAILABLE;

		accumulator = getPaddedTrueNonContendedSize(); /* this includes the header */
		accumulator += (_contendedObjectCount * sizeof(J9Object)) + (_contendedSingleCount * sizeof(U_32)) + (_contendedDoubleCount * sizeof(U_64)); /* add the contended fields */
		accumulator = ROUND_UP_TO_POWEROF2((UDATA)accumulator, (UDATA)_cacheLineSize) - sizeof(J9Object); /* Rounding takes care of the odd number of 4-byte fields. Remove the header */
	} else {
		accumulator = _superclassFieldsSize + (_totalObjectCount * sizeof(J9Object)) + (_totalSingleCount * sizeof(U_32)) + (_totalDoubleCount * sizeof(U_64));
#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
		accumulator += _totalFlatFieldDoubleBytes + _totalFlatFieldRefBytes + _totalFlatFieldSingleBytes;

		/* ValueTypes cannot be subtyped and their superClass contains no fields */
		if (!isValue()) {
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
			/* if the superclass is not end aligned but we have doubleword fields, use the space before the first field as the backfill */
			if (
					((getSuperclassObjectSize() % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0) && /* superclass is not end-aligned */
					((_totalDoubleCount > 0) || (!_objectCanUseBackfill && (_totalObjectCount > 0)))
			){ /* our fields start on a 8-byte boundary */
				Assert_VM_equal(_superclassBackfillOffset, NO_BACKFILL_AVAILABLE);
				_superclassBackfillOffset = getSuperclassFieldsSize();
				accumulator += BACKFILL_SIZE;
			}
			if (isSuperclassBackfillSlotAvailable() && isBackfillSuitableFieldAvailable()) {
				accumulator -= BACKFILL_SIZE;
				_myBackfillOffset = _superclassBackfillOffset;
				_superclassBackfillOffset = NO_BACKFILL_AVAILABLE;
			}
			if (((accumulator + sizeof(J9Object)) % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0) {
				/* we have consumed the superclass's backfill (if any), so let our subclass use the residue at the end of this class. */
				_subclassBackfillOffset = accumulator;
				accumulator += BACKFILL_SIZE;
			} else {
				_subclassBackfillOffset = _superclassBackfillOffset;
			}
#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
		} else {
			/* If the first field does not start at zero, this means we added padding to satisfy
			 * alignment requirements of double slot fields. Since values cannot be subclassed we can use
			 * the _subclassBackfillOffset to denote where the first field actually starts.
			 */
			U_32 firstFieldOffset = calculateFieldDataStart();
			accumulator += firstFieldOffset;
			_subclassBackfillOffset = firstFieldOffset;
		}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
	}
	return accumulator;
}

