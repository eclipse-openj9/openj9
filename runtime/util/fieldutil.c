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
#if defined(J9VM_OUT_OF_PROCESS)
#include "j9dbgext.h"
#include "dbggen.h"
#endif /* defined(J9VM_OUT_OF_PROCESS) */

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "util_internal.h"
#include "ut_j9vmutil.h"

static UDATA getFieldAnnotationsDataOffset(J9ROMFieldShape * field);
static UDATA annotationAttributeSize(U_8 *sectionStart);

/* Field signature translation table
 * NOTE: this table is NOT used in this file, but declared as an
 * external in util_api.h and used elsewhere in the code.
 *
 * Shift left by 16 to use as J9FieldType* */

const U_8 fieldModifiersLookupTable[] = {
	0x00												/* A */,
	(U_8)(J9FieldTypeByte >> 16)						/* B */,
	(U_8)(J9FieldTypeChar >> 16)						/* C */,
	(U_8)((J9FieldSizeDouble + J9FieldTypeDouble) >> 16)/* D */,
	0x00												/* E */,
	(U_8)(J9FieldTypeFloat >> 16)						/* F */,
	0x00												/* G */,
	0x00												/* H */,
	(U_8)(J9FieldTypeInt >> 16)							/* I */,
	(U_8)((J9FieldSizeDouble + J9FieldTypeLong) >> 16)	/* J */,
	0x00												/* K */,
	(U_8)(J9FieldFlagObject >> 16)						/* L */,
	0x00												/* M */,
	0x00												/* N */,
	0x00												/* O */,
	0x00												/* P */,
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	(U_8)(J9FieldFlagObject >> 16)						/* Q */,
#else
	0x00												/* Q */,
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
	0x00												/* R */,
	(U_8)(J9FieldTypeShort >> 16)						/* S */,
	0x00												/* T */,
	0x00												/* U */,
	0x00												/* V */,
	0x00												/* W */,
	0x00												/* X */,
	0x00												/* Y */,
	(U_8)(J9FieldTypeBoolean >> 16)						/* Z */,
	(U_8)(J9FieldFlagObject >> 16)						/* [ */
};


J9ROMFieldShape *
romFieldsNextDo(J9ROMFieldWalkState* state)
{
	if (state->fieldsLeft == 0) {
		return NULL;
	}

	state->field = (J9ROMFieldShape *) (((U_8 *) state->field) + romFieldSize(state->field));
	--(state->fieldsLeft);
	return state->field;
}

J9ROMFieldShape *
romFieldsStartDo(J9ROMClass * romClass, J9ROMFieldWalkState* state)
{
	state->fieldsLeft = romClass->romFieldCount;
	if (state->fieldsLeft == 0) {
		return NULL;
	}
	state->field = J9ROMCLASS_ROMFIELDS(romClass);
	--(state->fieldsLeft);
	return state->field;
}

void
walkFieldHierarchyDo(J9Class *clazz, J9WalkFieldHierarchyState *state)
{
	J9ROMFieldShape *romField = NULL;
	J9ROMFieldWalkState walkState;

	/* walk class and superclasses */
	if (FALSE == J9ROMCLASS_IS_INTERFACE(clazz->romClass)) {
		UDATA classDepth = J9CLASS_DEPTH(clazz);
		J9Class *currentClass = clazz;

		while (NULL != currentClass) {
			/* walk currentClass */
			memset(&walkState, 0, sizeof(walkState));
			romField = romFieldsStartDo(currentClass->romClass, &walkState);
			while (NULL != romField) {
				if (J9WalkFieldActionStop == state->fieldCallback(romField, currentClass, state->userData)) {
					return;
				}
				romField = romFieldsNextDo(&walkState);
			}

			/* get the superclass */
			if (classDepth >= 1) {
				classDepth -= 1;
				currentClass = clazz->superclasses[classDepth];
			} else {
				currentClass = NULL;
			}
		}
	}

	/* walk interfaces */
	{
		J9ITable *currentITable = (J9ITable *)clazz->iTable;

		while (NULL != currentITable) {
			memset(&walkState, 0, sizeof(walkState));
			romField = romFieldsStartDo(currentITable->interfaceClass->romClass, &walkState);
			while (NULL != romField) {
				if (J9WalkFieldActionStop == state->fieldCallback(romField, currentITable->interfaceClass, state->userData)) {
					return;
				}
				romField = romFieldsNextDo(&walkState);
			}
			currentITable = currentITable->next;
		}
	}
}

/**
 * Calculate the size of an annotation attribute, including padding to alignment at the end.
 * Assumes the sectionStart starts on a 4-byte boundary.
 * @param sectionStart pointer to the 4-byte length field preceding the actual annotation data
 * @return number of bytes in the attribute, including the length field and trailing padding.
 */
static UDATA
annotationAttributeSize(U_8 *sectionStart)
{
	Assert_VMUtil_true(((UDATA)sectionStart % sizeof(U_32)) == 0);
	/* skip over the field annotations length bytes, data, and padding */
	return ROUND_UP_TO_POWEROF2(sizeof(U_32) + *((U_32 *)sectionStart), sizeof(U_32)); /* length + data + padding */
}

UDATA
romFieldSize(J9ROMFieldShape *romField)
{
	UDATA modifiers = romField->modifiers;
	UDATA size = sizeof(J9ROMFieldShape);

	if (modifiers & J9FieldFlagConstant) {	
		size += ((modifiers & J9FieldSizeDouble) ? sizeof(U_64) : sizeof(U_32));
	}

	if (modifiers & J9FieldFlagHasGenericSignature) {
		size += sizeof(U_32);
	}

	if (modifiers & J9FieldFlagHasFieldAnnotations) {
		size += annotationAttributeSize((U_8 *)romField + size);
	}

	if (J9_ARE_ANY_BITS_SET(modifiers, J9FieldFlagHasTypeAnnotations)) {
		size += annotationAttributeSize((U_8 *)romField + size);
	}


	return size;
}

U_32 *
romFieldInitialValueAddress(J9ROMFieldShape * field)
{
	if (field->modifiers & J9FieldFlagConstant) {
		return (U_32 *) (field + 1);
	}

	return NULL;
}


J9UTF8 * romFieldGenericSignature(J9ROMFieldShape * field)
{
	if (field->modifiers & J9FieldFlagHasGenericSignature) {
		U_32 * ptr = (U_32 *) (field + 1);

		if (field->modifiers & J9FieldFlagConstant) {
			ptr += ((field->modifiers & J9FieldSizeDouble) ? 2 : 1);
		}

		return NNSRP_PTR_GET(ptr, J9UTF8 *);
	}

	return NULL;
}

/**
 * Find the offset from the beginning of the field to the area containing field annotations.
 * If there are no field annotations, this is the start of the field type annotations (if any).
 * Skip over any constant value or generic field signature.
 * @param field pointer to start of the ROM field
 * @return offset in bytes to start of annotations area.
 */
static UDATA 
getFieldAnnotationsDataOffset(J9ROMFieldShape * field)
{
	UDATA modifiers = field->modifiers;
	UDATA size = sizeof(J9ROMFieldShape);

	if (modifiers & J9FieldFlagConstant) {
		size += ((modifiers & J9FieldSizeDouble) ? sizeof(U_64) : sizeof(U_32));
	}

	if (modifiers & J9FieldFlagHasGenericSignature) {
		size += sizeof(U_32);
	}
	return size;
}

U_32 *
getFieldAnnotationsDataFromROMField(J9ROMFieldShape * field)
{
	U_32 *result = NULL;
	if (field->modifiers & J9FieldFlagHasFieldAnnotations) {
		UDATA size = getFieldAnnotationsDataOffset(field);

		result = (U_32 *)((UDATA)field + size);
	}
	return result;
}


U_32 *
getFieldTypeAnnotationsDataFromROMField(J9ROMFieldShape * field)
{
	U_32 *result = NULL;
	if (field->modifiers & J9FieldFlagHasTypeAnnotations) {
		U_8 *fieldAnnotationsData = (U_8 *)getFieldAnnotationsDataFromROMField(field);

		if (NULL != fieldAnnotationsData) { 
			result = (U_32 *)(fieldAnnotationsData + annotationAttributeSize(fieldAnnotationsData));
		} else {
			/* the type annotations are where the field annotations would have been */
			result = (U_32 *)((UDATA)field + getFieldAnnotationsDataOffset(field));
		}
	}
	return result;
}



#if defined(J9VM_OUT_OF_PROCESS)
static UDATA
debugRomFieldSize(J9ROMFieldShape *romField)
{
	UDATA modifiers = (UDATA)dbgReadSlot((UDATA)&((romField)->modifiers), sizeof((romField)->modifiers));
	UDATA size = sizeof(J9ROMFieldShape);

	if (modifiers & J9FieldFlagConstant) {
		size += ((modifiers & J9FieldSizeDouble) ? sizeof(U_64) : sizeof(U_32));
	}

	if (modifiers & J9FieldFlagHasGenericSignature) {
		size += sizeof(U_32);
	}

	if (modifiers & J9FieldFlagHasFieldAnnotations) {
		UDATA bytesToPad = 0;
		U_32 annotationSize = dbgReadU32((U_32 *)((UDATA)romField + size));
		/* add the length of annotation data */
		size += annotationSize;
		/* add the size of the size of the annotation data */
		size += sizeof(U_32);

		bytesToPad = sizeof(U_32) - annotationSize%sizeof(U_32);
		if (sizeof(U_32)==bytesToPad) {
			bytesToPad = 0;
		}
		/* add the padding */
		size += bytesToPad;
	}
	
	return size;
}

J9ROMFieldShape *
debugRomFieldsStartDo(J9ROMClass *romClass, J9ROMFieldWalkState *state)
{
	state->fieldsLeft = (U_32)dbgReadSlot((UDATA)&((romClass)->romFieldCount), sizeof((romClass)->romFieldCount));

	if (state->fieldsLeft == 0) {
		state->field = NULL;
	} else {
		J9ROMClass *localRomClass = dbgRead_J9ROMClass(romClass);
		state->field = TARGET_NNSRP_GET(localRomClass->romFields, struct J9ROMFieldShape*);
		--(state->fieldsLeft);
		dbgFree(localRomClass);
	}

	return state->field;
}


J9ROMFieldShape *
debugRomFieldsNextDo(J9ROMFieldWalkState *state)
{
	if (state->fieldsLeft == 0) {
		return NULL;
	}

	state->field = (J9ROMFieldShape *) (((U_8 *) state->field) + debugRomFieldSize(state->field));
	--(state->fieldsLeft);
	return state->field;
}
#endif /* defined(J9VM_OUT_OF_PROCESS) */
