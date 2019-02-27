/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include "j9comp.h"
#include "j9.h"

#include "testHelpers.h"
#include "vm_api.h"
#include "util_api.h"


/* Set VMTEST_DEBUG to 1 for test debug output. */
#define VMTEST_DEBUG 0

typedef struct testFieldDef {
	const char *fieldName;
	const char *fieldSignature;
} testFieldDef;

/* Function prototypes. */
static UDATA calculateJ9UTFSize(UDATA stringLength);
static UDATA calculateFieldSize(U_32 modifiers);
static U_32 calculateFieldModifiersForSignature(const U_8 *signature);
static J9ROMClass *createFakeROMClass(U_8 *buffer, UDATA bufferSize, const char *className, testFieldDef *regularFields);
static IDATA checkForFieldOverlaps(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClass, UDATA *badOffset);
static IDATA checkForPresenceOfField(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClass,
	U_8 *fieldNameData, UDATA fieldNameLength, UDATA fieldSize, UDATA expectedIndex);
static IDATA testAddHiddenInstanceFields(J9PortLibrary *portLib, const char *testName,
	testFieldDef *regularFields, testFieldDef *hiddenFields);
static IDATA testAddHiddenInstanceFields1(J9PortLibrary *portLib);
static IDATA testAddHiddenInstanceFields2(J9PortLibrary *portLib);
static IDATA testAddHiddenInstanceFields3(J9PortLibrary *portLib);
static IDATA testAddHiddenInstanceFields4(J9PortLibrary *portLib);
static IDATA testAddHiddenInstanceFields5(J9PortLibrary *portLib);
static IDATA testAddHiddenInstanceFields6(J9PortLibrary *portLib);


static UDATA
calculateJ9UTFSize(UDATA stringLength)
{
	if (0 != (stringLength & 1)) {
		stringLength++;
	}

	return offsetof(J9UTF8, data) + stringLength;
}

static UDATA
calculateFieldSize(U_32 modifiers)
{
	UDATA size;

	if (J9FieldFlagObject == (modifiers & J9FieldFlagObject)) {
		size = sizeof(fj9object_t);
	} else if (J9FieldSizeDouble == (modifiers & J9FieldSizeDouble)) {
		size = sizeof(U_64);
	} else {
		size = sizeof(U_32);
	}

	return size;
}

static U_32
calculateFieldModifiersForSignature(const U_8 *signature)
{
	return (U_32)(fieldModifiersLookupTable[signature[0] - 'A'] << 16);
}

static J9ROMClass *
createFakeROMClass(U_8 *buffer, UDATA bufferSize, const char *className, testFieldDef *regularFields)
{
#define BUF_ALLOC(size) (void *)((bufferIndex + (size) <= bufferSize) ? &buffer[(bufferIndex += (size)) - (size)] : NULL)
	UDATA fieldIndex;
	UDATA bufferIndex = 0;

	J9ROMClass *romClass;
	UDATA classNameLength = strlen(className);
	J9UTF8 *classNameUTF8;
	J9ROMFieldShape *romFields;

	romClass = BUF_ALLOC(sizeof(J9ROMClass));
	if (NULL == romClass) {
		return NULL;
	}

	memset(romClass, 0, sizeof(J9ROMClass));
	for (fieldIndex = 0; NULL != regularFields[fieldIndex].fieldName; fieldIndex++) {
		romClass->romFieldCount++;
	}

	classNameUTF8 = BUF_ALLOC(calculateJ9UTFSize(classNameLength));
	if (NULL == classNameUTF8) {
		return NULL;
	}
	J9UTF8_SET_LENGTH(classNameUTF8, (U_16) classNameLength);
	memcpy(J9UTF8_DATA(classNameUTF8), className, classNameLength);

	romFields = BUF_ALLOC(sizeof(J9ROMFieldShape) * romClass->romFieldCount);
	if (NULL == romFields) {
		return NULL;
	}

	for (fieldIndex = 0; NULL != regularFields[fieldIndex].fieldName; fieldIndex++) {
		UDATA nameLength = strlen(regularFields[fieldIndex].fieldName);
		UDATA signatureLength = strlen(regularFields[fieldIndex].fieldSignature);
		J9UTF8 *nameUTF8 = BUF_ALLOC(calculateJ9UTFSize(nameLength));
		J9UTF8 *signatureUTF8 = BUF_ALLOC(calculateJ9UTFSize(signatureLength));

		if ((NULL == nameUTF8) || (NULL == signatureUTF8)) {
			return NULL;
		}

		J9UTF8_SET_LENGTH(nameUTF8, (U_16)nameLength);
		memcpy(J9UTF8_DATA(nameUTF8), regularFields[fieldIndex].fieldName, nameLength);
		J9UTF8_SET_LENGTH(signatureUTF8, (U_16)signatureLength);
		memcpy(J9UTF8_DATA(signatureUTF8), regularFields[fieldIndex].fieldSignature, signatureLength);

		NNSRP_SET(romFields[fieldIndex].nameAndSignature.name, nameUTF8);
		NNSRP_SET(romFields[fieldIndex].nameAndSignature.signature, signatureUTF8);

		romFields[fieldIndex].modifiers = calculateFieldModifiersForSignature(J9UTF8_DATA(signatureUTF8));
	}

	NNSRP_SET(romClass->className, classNameUTF8);
	NNSRP_SET(romClass->romFields, romFields);
	romClass->romSize = (U_32)bufferIndex;

	return romClass;
#undef BUF_ALLOC
}

static IDATA
checkForFieldOverlaps(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClass, UDATA *badOffset)
{
	J9ROMFieldOffsetWalkResult *walkResult;
	J9ROMFieldOffsetWalkState walkState;
	U_8 buf[1024];
	UDATA i, fieldSize;

	memset(buf, 0, sizeof(buf));
	for (i = 0; i < sizeof(J9Object); i++) {
		buf[i] = 0xff;
	}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	walkResult = fieldOffsetsStartDo(vm, romClass, superClass, &walkState,
			J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN, NULL);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	walkResult = fieldOffsetsStartDo(vm, romClass, superClass, &walkState,
			J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	while (NULL != walkResult->field) {

		fieldSize = calculateFieldSize(walkResult->field->modifiers);
		for (i = 0; i < fieldSize; i++) {
			UDATA offset = i + walkResult->offset + sizeof(J9Object);

#if VMTEST_DEBUG
			J9UTF8 *fieldName = SRP_GET(walkResult->field->nameAndSignature.name, J9UTF8*);

			printf("Mark offset %d as used by field %.*s.\n", offset, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
#endif

			if (offset > sizeof(buf)) {
				/* Error - buffer to small? */
				*badOffset = offset;
				return -1;
			}

			if (offset - sizeof(J9Object) > walkResult->totalInstanceSize) {
				/* Error - offset outside of reported instance size? */
				*badOffset = offset;
				return -1;
			}

			if (0 != buf[offset]) {
				/* Error - fields overlap? */
				*badOffset = offset;
				return -1;
			}
			buf[offset] = 0xff;
		}

		walkResult = fieldOffsetsNextDo(&walkState);
	}

	return 0;
}

static IDATA
checkForPresenceOfField(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClass,
	U_8 *fieldNameData, UDATA fieldNameLength, UDATA fieldSize, UDATA expectedIndex)
{
	J9ROMFieldOffsetWalkResult *walkResult;
	J9ROMFieldOffsetWalkState walkState;

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	walkResult = fieldOffsetsStartDo(vm, romClass, superClass, &walkState,
			J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN, NULL);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	walkResult = fieldOffsetsStartDo(vm, romClass, superClass, &walkState,
			J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

	while (NULL != walkResult->field) {
		J9UTF8 *resultFieldName = SRP_GET(walkResult->field->nameAndSignature.name, J9UTF8*);
		UDATA resultFieldSize = calculateFieldSize(walkResult->field->modifiers);

#if VMTEST_DEBUG
		printf("Got field %.*s with index %d and size %d.\n",
			J9UTF8_LENGTH(resultFieldName), J9UTF8_DATA(resultFieldName), walkResult->index, resultFieldSize);
#endif

		if (expectedIndex == walkResult->index) {
	 		if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(resultFieldName), J9UTF8_LENGTH(resultFieldName), fieldNameData, fieldNameLength)) {
	 			return (fieldSize - resultFieldSize);
	 		} else if ((UDATA)-1 != walkResult->index) {
	 			/* We matched index for non-hidden field, but name was different - fail! */
	 			return -1;
	 		}
		}

		walkResult = fieldOffsetsNextDo(&walkState);
	}

	return -1;
}

static IDATA
testAddHiddenInstanceFields(J9PortLibrary *portLib, const char *testName,
	testFieldDef *regularFields, testFieldDef *hiddenFields)
{
	PORT_ACCESS_FROM_PORT(portLib);

	const char *className = "java/lang/Object";
	J9JavaVM javaVM;
	OMR_VM omrVM;
	UDATA fieldOffset, fieldIndex;
	J9ROMFieldOffsetWalkResult *walkResult;
	J9ROMFieldOffsetWalkState walkState;
	UDATA hiddenFieldCount;
	U_8 buffer[4096];
	J9ROMClass *fakeROMClass;
	UDATA badOffset;

	reportTestEntry(PORTLIB, testName);

	memset(&javaVM, 0, sizeof(J9JavaVM));
	javaVM.javaVM = &javaVM;
	javaVM.portLibrary = portLib;

	/*
	 * ObjectFieldInfo required run-time object alignment stored on omrVM
	 * Add dummy value to support
	 */
	javaVM.omrVM = &omrVM;
	omrVM._objectAlignmentInBytes = 8;
	omrVM._objectAlignmentShift = 3;

	if (0 != initializeVMThreading(&javaVM)) {
		outputErrorMessage(TEST_ERROR_ARGS, "initializeVMThreading() failed!\n");
		goto _exit_test;
	}

	if (0 != initializeHiddenInstanceFieldsList(&javaVM)) {
		outputErrorMessage(TEST_ERROR_ARGS, "initializeHiddenInstanceFieldsList() failed!\n");
		goto _exit_test;
	}

	fakeROMClass = createFakeROMClass(buffer, sizeof(buffer), className, regularFields);
	if (NULL == fakeROMClass) {
		outputErrorMessage(TEST_ERROR_ARGS, "createFakeROMClass() failed!\n");
		goto _exit_test;
	}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	walkResult = fieldOffsetsStartDo(&javaVM, fakeROMClass, NULL, &walkState, J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE, NULL);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	walkResult = fieldOffsetsStartDo(&javaVM, fakeROMClass, NULL, &walkState, J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	if (NULL == walkResult) {
		outputErrorMessage(TEST_ERROR_ARGS, "fieldOffsetsStartDo() returned NULL!\n");
		goto _exit_test;
	}

	hiddenFieldCount = 0;
	for (fieldIndex = 0; NULL != hiddenFields[fieldIndex].fieldName; fieldIndex++) {
		UDATA *offsetReturnPtr = NULL;

		if (NULL == hiddenFields[fieldIndex + 1].fieldName) {
			/* Check offsetReturnPtr for last hidden field added. */
			offsetReturnPtr = &fieldOffset;
			fieldOffset = (UDATA)-1;
		}

		if (0 != addHiddenInstanceField(&javaVM, className, hiddenFields[fieldIndex].fieldName, hiddenFields[fieldIndex].fieldSignature, offsetReturnPtr)) {
			outputErrorMessage(TEST_ERROR_ARGS, "addHiddenInstanceField() failed!\n");
			goto _exit_test;
		}
		hiddenFieldCount++;
	}

	if (hiddenFieldCount > 0) {
		if (javaVM.hiddenInstanceFields->offsetReturnPtr != &fieldOffset) {
			outputErrorMessage(TEST_ERROR_ARGS, "field->offsetReturnPtr (%p) != &fieldOffset (%p) after addHiddenInstanceField()!\n",
				javaVM.hiddenInstanceFields->offsetReturnPtr != &fieldOffset);
			goto _exit_test;
		}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		walkResult = fieldOffsetsStartDo(&javaVM, fakeROMClass, NULL, &walkState, J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE, NULL);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		walkResult = fieldOffsetsStartDo(&javaVM, fakeROMClass, NULL, &walkState, J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		if (NULL == walkResult) {
			outputErrorMessage(TEST_ERROR_ARGS, "fieldOffsetsStartDo() returned NULL!\n");
			goto _exit_test;
		}

		if ((UDATA)-1 == fieldOffset) {
			outputErrorMessage(TEST_ERROR_ARGS, "fieldOffset was not set after fieldOffsetsStartDo()!\n");
			goto _exit_test;
		}

		if (javaVM.hiddenInstanceFields->fieldOffset != fieldOffset) {
			outputErrorMessage(TEST_ERROR_ARGS, "field->fieldOffset (%d) != fieldOffset (%d) after fieldOffsetsStartDo()!\n",
				javaVM.hiddenInstanceFields->fieldOffset, fieldOffset);
			goto _exit_test;
		}
	}

	/* Check that there are no field overlaps. */
	if (0 != checkForFieldOverlaps(&javaVM, fakeROMClass, NULL, &badOffset)) {
		outputErrorMessage(TEST_ERROR_ARGS, "overlapping fields detected at offset %d!\n", badOffset);
		goto _exit_test;
	}

	/* Check that all regular fields are there. */
	for (fieldIndex = 0; NULL != regularFields[fieldIndex].fieldName; fieldIndex++) {
		const char *fieldName = regularFields[fieldIndex].fieldName;
		const char *fieldSignature = regularFields[fieldIndex].fieldSignature;
		UDATA fieldSize = calculateFieldSize(calculateFieldModifiersForSignature((U_8*)fieldSignature));

		if (0 != checkForPresenceOfField(&javaVM, fakeROMClass, NULL, (U_8*)fieldName, strlen(fieldName), fieldSize, fieldIndex + 1)) {
			outputErrorMessage(TEST_ERROR_ARGS, "regular field '%s' not found!\n", fieldName);
			goto _exit_test;
		}
	}

	/* Check that all hidden fields are there. */
	for (fieldIndex = 0; NULL != hiddenFields[fieldIndex].fieldName; fieldIndex++) {
		const char *fieldName = hiddenFields[fieldIndex].fieldName;
		const char *fieldSignature = hiddenFields[fieldIndex].fieldSignature;
		UDATA fieldSize = calculateFieldSize(calculateFieldModifiersForSignature((U_8*)fieldSignature));
		UDATA hiddenIndex = (UDATA)-1;

		if (0 != checkForPresenceOfField(&javaVM, fakeROMClass, NULL, (U_8*)fieldName, strlen(fieldName), fieldSize, hiddenIndex)) {
			outputErrorMessage(TEST_ERROR_ARGS, "hidden field '%s' not found!\n", fieldName);
			goto _exit_test;
		}
	}

	freeHiddenInstanceFieldsList(&javaVM);
	terminateVMThreading(&javaVM);

_exit_test:
	return reportTestExit(PORTLIB, testName);
}

static IDATA
testAddHiddenInstanceFields1(J9PortLibrary *portLib)
{
	const char *testName = "testAddHiddenInstanceFields1";
	testFieldDef regularFields[] = {
		{"a", "I"}, {"b", "J"}, {"c", "I"}, {NULL, NULL}
	};
	testFieldDef hiddenFields[] = {
		{"x", "J"}, {"y", "I"}, {NULL, NULL}
	};

	return testAddHiddenInstanceFields(portLib, testName, regularFields, hiddenFields);
}

static IDATA
testAddHiddenInstanceFields2(J9PortLibrary *portLib)
{
	const char *testName = "testAddHiddenInstanceFields2";
	testFieldDef regularFields[] = {
		{"a", "I"}, {"b", "[J"}, {NULL, NULL}
	};
	testFieldDef hiddenFields[] = {
		{"x", "I"}, {"y", "I"}, {NULL, NULL}
	};

	return testAddHiddenInstanceFields(portLib, testName, regularFields, hiddenFields);
}

static IDATA
testAddHiddenInstanceFields3(J9PortLibrary *portLib)
{
	const char *testName = "testAddHiddenInstanceFields3";
	testFieldDef regularFields[] = {
		{NULL, NULL}
	};
	testFieldDef hiddenFields[] = {
		{"x", "I"}, {NULL, NULL}
	};

	return testAddHiddenInstanceFields(portLib, testName, regularFields, hiddenFields);
}

static IDATA
testAddHiddenInstanceFields4(J9PortLibrary *portLib)
{
	const char *testName = "testAddHiddenInstanceFields4";
	testFieldDef regularFields[] = {
		{NULL, NULL}
	};
	testFieldDef hiddenFields[] = {
		{"x", "J"}, {NULL, NULL}
	};

	return testAddHiddenInstanceFields(portLib, testName, regularFields, hiddenFields);
}


static IDATA
testAddHiddenInstanceFields5(J9PortLibrary *portLib)
{
	const char *testName = "testAddHiddenInstanceFields5";
	testFieldDef regularFields[] = {
		{"x", "I"}, {NULL, NULL}
	};
	testFieldDef hiddenFields[] = {
		{NULL, NULL}
	};

	return testAddHiddenInstanceFields(portLib, testName, regularFields, hiddenFields);
}

static IDATA
testAddHiddenInstanceFields6(J9PortLibrary *portLib)
{
	const char *testName = "testAddHiddenInstanceFields6";
	testFieldDef regularFields[] = {
		{"x", "J"}, {NULL, NULL}
	};
	testFieldDef hiddenFields[] = {
		{NULL, NULL}
	};

	return testAddHiddenInstanceFields(portLib, testName, regularFields, hiddenFields);
}

IDATA
testResolveField(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;

	HEADING(PORTLIB, "testResolveField");
	rc |= testAddHiddenInstanceFields1(PORTLIB);
	rc |= testAddHiddenInstanceFields2(PORTLIB);
	rc |= testAddHiddenInstanceFields3(PORTLIB);
	rc |= testAddHiddenInstanceFields4(PORTLIB);
	rc |= testAddHiddenInstanceFields5(PORTLIB);
	rc |= testAddHiddenInstanceFields6(PORTLIB);
	return rc;
}
