/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "j9protos.h"
#include "j9consts.h"
#include "j9vmconstantpool.h"

#define TYPE_COUNT 9

#define OBJECT_ARRAY_NAME "[L"
#define BOOLEAN_ARRAY_NAME "[Z"
#define CHAR_ARRAY_NAME "[C"
#define FLOAT_ARRAY_NAME "[F"
#define DOUBLE_ARRAY_NAME "[D"
#define BYTE_ARRAY_NAME "[B"
#define SHORT_ARRAY_NAME "[S"
#define INT_ARRAY_NAME "[I"
#define LONG_ARRAY_NAME "[J"
#define VOID_CLASS_NAME "void"
#define BOOLEAN_CLASS_NAME "boolean"
#define CHAR_CLASS_NAME "char"
#define FLOAT_CLASS_NAME "float"
#define DOUBLE_CLASS_NAME "double"
#define BYTE_CLASS_NAME "byte"
#define SHORT_CLASS_NAME "short"
#define INT_CLASS_NAME "int"
#define LONG_CLASS_NAME "long"
#define OBJECT_CLASS_NAME "java/lang/Object"
#define CLONEABLE_CLASS_NAME "java/lang/Cloneable"
#define SERIALIZEABLE_CLASS_NAME "java/io/Serializable"

#define INLINE_UTF(name, value) struct { U_16 length; U_8 data[sizeof(value) - 1]; } name

typedef struct {
	J9ROMImageHeader header;
	J9ROMArrayClass objectArrayROMClass;
	J9ROMArrayClass booleanArrayROMClass;
	J9ROMArrayClass charArrayROMClass;
	J9ROMArrayClass floatArrayROMClass;
	J9ROMArrayClass doubleArrayROMClass;
	J9ROMArrayClass byteArrayROMClass;
	J9ROMArrayClass shortArrayROMClass;
	J9ROMArrayClass intArrayROMClass;
	J9ROMArrayClass longArrayROMClass;
	struct {
		J9SRP cloneable;
		J9SRP serializeable;
	} interfaceClasses;
	INLINE_UTF(objectArrayClassName, OBJECT_ARRAY_NAME);
	INLINE_UTF(booleanArrayClassName, BOOLEAN_ARRAY_NAME);
	INLINE_UTF(charArrayClassName, CHAR_ARRAY_NAME);
	INLINE_UTF(floatArrayClassName, FLOAT_ARRAY_NAME);
	INLINE_UTF(doubleArrayClassName, DOUBLE_ARRAY_NAME);
	INLINE_UTF(byteArrayClassName, BYTE_ARRAY_NAME);
	INLINE_UTF(shortArrayClassName, SHORT_ARRAY_NAME);
	INLINE_UTF(intArrayClassName, INT_ARRAY_NAME);
	INLINE_UTF(longArrayClassName, LONG_ARRAY_NAME);
	INLINE_UTF(objectClassName, OBJECT_CLASS_NAME);
	INLINE_UTF(cloneableClassName, CLONEABLE_CLASS_NAME);
	INLINE_UTF(serializeableClassName, SERIALIZEABLE_CLASS_NAME);
} J9ArrayROMClasses;

typedef struct {
	J9ROMImageHeader header;
	J9ROMReflectClass voidReflectROMClass;
	J9ROMReflectClass booleanReflectROMClass;
	J9ROMReflectClass charReflectROMClass;
	J9ROMReflectClass floatReflectROMClass;
	J9ROMReflectClass doubleReflectROMClass;
	J9ROMReflectClass byteReflectROMClass;
	J9ROMReflectClass shortReflectROMClass;
	J9ROMReflectClass intReflectROMClass;
	J9ROMReflectClass longReflectROMClass;
	INLINE_UTF(voidClassName, VOID_CLASS_NAME);
	INLINE_UTF(booleanClassName, BOOLEAN_CLASS_NAME);
	INLINE_UTF(charClassName, CHAR_CLASS_NAME);
	INLINE_UTF(floatClassName, FLOAT_CLASS_NAME);
	INLINE_UTF(doubleClassName, DOUBLE_CLASS_NAME);
	INLINE_UTF(byteClassName, BYTE_CLASS_NAME);
	INLINE_UTF(shortClassName, SHORT_CLASS_NAME);
	INLINE_UTF(intClassName, INT_CLASS_NAME);
	INLINE_UTF(longClassName, LONG_CLASS_NAME);
} J9BaseTypePrimitiveROMClasses;

static J9ArrayROMClasses arrayROMClasses;
static J9BaseTypePrimitiveROMClasses baseTypePrimitiveROMClasses;

#define INIT_UTF8(field, str) \
		do { \
			field.length = sizeof(str) - 1; \
			memcpy(&field.data, str, sizeof(str) - 1); \
		} while(0)

/**
 * Initialize a ROM array class.
 *
 * @param romClass[in] the ROM class to initialize
 * @param className[in] the class name
 * @param arrayShape[in] the array shape
 * @param instanceShape[in] the instance shape
 * @param size[in] the size for the ROM class
 */
static void
initializeArrayROMClass(J9ROMArrayClass *romClass, J9UTF8 *className, U_32 arrayShape, U_32 instanceShape, U_32 size)
{
	romClass->romSize = size;
	NNSRP_SET(romClass->className, className);
	NNSRP_SET(romClass->superclassName, &arrayROMClasses.objectClassName);
	romClass->modifiers = J9AccFinal | J9AccPublic | J9AccClassArray | J9AccAbstract;
	romClass->extraModifiers = J9AccClassCloneable | J9AccClassIsUnmodifiable;
	romClass->interfaceCount = sizeof(arrayROMClasses.interfaceClasses) / sizeof(J9SRP);
	NNSRP_SET(romClass->interfaces, &arrayROMClasses.interfaceClasses);
	romClass->arrayShape = arrayShape;
	romClass->instanceShape = J9_OBJECT_HEADER_INDEXABLE | instanceShape;
}

/**
 * Initialize a base type ROM class.
 *
 * @param romClass[in] the ROM class to initialize
 * @param className[in] the class name
 * @param typeCode[in] the reflect type code
 * @param instanceShape[in] the instance shape
 * @param elementSize[in] the element size
 * @param size[in] the size for the ROM class
 */
static void
initializeBaseTypeROMClass(J9ROMReflectClass *romClass, J9UTF8 *className, U_32 typeCode, U_32 instanceShape, U_32 elementSize, U_32 size)
{
	romClass->romSize = size;
	NNSRP_SET(romClass->className, className);
	romClass->modifiers = J9AccFinal | J9AccPublic | J9AccClassInternalPrimitiveType | J9AccAbstract;
	romClass->extraModifiers = J9AccClassIsUnmodifiable;
	romClass->reflectTypeCode = typeCode;
	romClass->instanceShape = instanceShape;
	romClass->elementSize = elementSize;
}

void
initializeROMClasses(J9JavaVM *vm)
{
    memset(&arrayROMClasses, 0, sizeof(arrayROMClasses));
    memset(&baseTypePrimitiveROMClasses, 0, sizeof(baseTypePrimitiveROMClasses));
	/* Initialize UTF data for the array classes */
	INIT_UTF8(arrayROMClasses.objectArrayClassName, OBJECT_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.booleanArrayClassName, BOOLEAN_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.charArrayClassName, CHAR_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.floatArrayClassName, FLOAT_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.doubleArrayClassName, DOUBLE_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.byteArrayClassName, BYTE_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.shortArrayClassName, SHORT_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.intArrayClassName, INT_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.longArrayClassName, LONG_ARRAY_NAME);
	INIT_UTF8(arrayROMClasses.objectClassName, OBJECT_CLASS_NAME);
	INIT_UTF8(arrayROMClasses.cloneableClassName, CLONEABLE_CLASS_NAME);
	INIT_UTF8(arrayROMClasses.serializeableClassName, SERIALIZEABLE_CLASS_NAME);
	/* Initialize the required fields in the array class ROM image header */
	arrayROMClasses.header.romSize = sizeof(arrayROMClasses) - sizeof(arrayROMClasses.header);
	NNSRP_SET(arrayROMClasses.header.firstClass, &arrayROMClasses.objectArrayROMClass);
	/* Set up the SRPs in the interface array */
	NNSRP_SET(arrayROMClasses.interfaceClasses.cloneable, &arrayROMClasses.cloneableClassName);
	NNSRP_SET(arrayROMClasses.interfaceClasses.serializeable, &arrayROMClasses.serializeableClassName);
	/* Initialize the array classes */
	initializeArrayROMClass(&arrayROMClasses.objectArrayROMClass, (J9UTF8*)&arrayROMClasses.objectArrayClassName, sizeof(fj9object_t) == 4 ? J9ArraySizeLongs : J9ArraySizeDoubles, OBJECT_HEADER_SHAPE_POINTERS, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.booleanArrayROMClass, (J9UTF8*)&arrayROMClasses.booleanArrayClassName, J9ArraySizeBytes, OBJECT_HEADER_SHAPE_BYTES, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.charArrayROMClass, (J9UTF8*)&arrayROMClasses.charArrayClassName, J9ArraySizeWords, OBJECT_HEADER_SHAPE_WORDS, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.floatArrayROMClass, (J9UTF8*)&arrayROMClasses.floatArrayClassName, J9ArraySizeLongs, OBJECT_HEADER_SHAPE_LONGS, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.doubleArrayROMClass, (J9UTF8*)&arrayROMClasses.doubleArrayClassName, J9ArraySizeDoubles, OBJECT_HEADER_SHAPE_DOUBLES, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.byteArrayROMClass, (J9UTF8*)&arrayROMClasses.byteArrayClassName, J9ArraySizeBytes, OBJECT_HEADER_SHAPE_BYTES, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.shortArrayROMClass, (J9UTF8*)&arrayROMClasses.shortArrayClassName, J9ArraySizeWords, OBJECT_HEADER_SHAPE_WORDS, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.intArrayROMClass, (J9UTF8*)&arrayROMClasses.intArrayClassName, J9ArraySizeLongs, OBJECT_HEADER_SHAPE_LONGS, sizeof(J9ROMArrayClass));
	initializeArrayROMClass(&arrayROMClasses.longArrayROMClass, (J9UTF8*)&arrayROMClasses.longArrayClassName, J9ArraySizeDoubles, OBJECT_HEADER_SHAPE_DOUBLES, sizeof(J9ArrayROMClasses) - offsetof(J9ArrayROMClasses, longArrayROMClass));
	vm->arrayROMClasses = &arrayROMClasses.header;
	/* Initialize UTF data for the primitive classes */
	INIT_UTF8(baseTypePrimitiveROMClasses.voidClassName, VOID_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.booleanClassName, BOOLEAN_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.charClassName, CHAR_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.floatClassName, FLOAT_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.doubleClassName, DOUBLE_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.byteClassName, BYTE_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.shortClassName, SHORT_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.intClassName, INT_CLASS_NAME);
	INIT_UTF8(baseTypePrimitiveROMClasses.longClassName, LONG_CLASS_NAME);
	/* Initialize the required fields in the primitive class ROM image header */
	baseTypePrimitiveROMClasses.header.romSize = sizeof(baseTypePrimitiveROMClasses) - sizeof(baseTypePrimitiveROMClasses.header);
	NNSRP_SET(baseTypePrimitiveROMClasses.header.firstClass, &baseTypePrimitiveROMClasses.voidReflectROMClass);
	/* Initialize the primitive classes */
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.voidReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.voidClassName, J9VMCONSTANTPOOL_JAVALANGOBJECT, OBJECT_HEADER_SHAPE_MIXED, 0, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.booleanReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.booleanClassName, J9VMCONSTANTPOOL_JAVALANGBOOLEAN, OBJECT_HEADER_SHAPE_BYTES, 1, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.charReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.charClassName, J9VMCONSTANTPOOL_JAVALANGCHARACTER, OBJECT_HEADER_SHAPE_WORDS, 2, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.floatReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.floatClassName, J9VMCONSTANTPOOL_JAVALANGFLOAT, OBJECT_HEADER_SHAPE_LONGS, 4, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.doubleReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.doubleClassName, J9VMCONSTANTPOOL_JAVALANGDOUBLE, OBJECT_HEADER_SHAPE_DOUBLES, 8, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.byteReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.byteClassName, J9VMCONSTANTPOOL_JAVALANGBYTE, OBJECT_HEADER_SHAPE_BYTES, 1, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.shortReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.shortClassName, J9VMCONSTANTPOOL_JAVALANGSHORT, OBJECT_HEADER_SHAPE_WORDS, 2, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.intReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.intClassName, J9VMCONSTANTPOOL_JAVALANGINTEGER, OBJECT_HEADER_SHAPE_LONGS, 4, sizeof(J9ROMReflectClass));
	initializeBaseTypeROMClass(&baseTypePrimitiveROMClasses.longReflectROMClass, (J9UTF8*)&baseTypePrimitiveROMClasses.longClassName, J9VMCONSTANTPOOL_JAVALANGLONG, OBJECT_HEADER_SHAPE_DOUBLES, 8, sizeof(J9BaseTypePrimitiveROMClasses) - offsetof(J9BaseTypePrimitiveROMClasses, longReflectROMClass));
}

UDATA
internalCreateBaseTypePrimitiveAndArrayClasses(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9ClassLoader *classLoader = vm->systemClassLoader;
	UDATA rc = 1;
	UDATA i = 0;
	J9ROMArrayClass *arrayROMClass = &arrayROMClasses.booleanArrayROMClass;
	J9Class **arrayRAMClass = &vm->booleanArrayClass;
	J9ROMClass *primitiveROMClass = (J9ROMClass*)&baseTypePrimitiveROMClasses.voidReflectROMClass;
	J9Class **primitiveRAMClass = &vm->voidReflectClass;

	/* create a segment for the base type ROM classes */
	if (NULL == romImageNewSegment(vm, &baseTypePrimitiveROMClasses.header, TRUE, classLoader)) {
		goto done;
	}
	/* create a segment for the array ROM classes */
	if (NULL == romImageNewSegment(vm, &arrayROMClasses.header, FALSE, classLoader)) {
		goto done;
	}
	for (i = 0; i < TYPE_COUNT; ++i) {
		J9Class *ramClass = NULL;
		omrthread_monitor_enter(vm->classTableMutex);
		ramClass = internalCreateRAMClassFromROMClass(
				currentThread, /* currentThread */
				classLoader, /* classLoader */
				primitiveROMClass, /* romClass */
				J9_FINDCLASS_FLAG_EXISTING_ONLY, /* options */
				NULL, /* elementClass */
				NULL, /* protectionDomain */
				NULL, /* methodRemapArray */
				J9_CP_INDEX_NONE, /* entryIndex */
				LOAD_LOCATION_UNKNOWN, /* locationType */
				NULL, /* classBeingRedefined */
				NULL /* hostClass */
			);
		if (NULL == ramClass) {
			goto done;
		}
		*primitiveRAMClass = ramClass;
		/* Slot 0 is object array which has no entry in the J9JavaVM and void which
		 * does not get an array class created for it.
		 */
		if (0 != i) {
			J9Class *ramArrayClass = internalCreateArrayClass(currentThread, arrayROMClass, ramClass);
			if (NULL == ramArrayClass) {
				goto done;
			}
			*arrayRAMClass = ramArrayClass;
			arrayROMClass += 1;
			arrayRAMClass += 1;
		}
		primitiveROMClass += 1;
		primitiveRAMClass += 1;
	}
	rc = 0;
done:
	return rc;
}
