/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include <stdlib.h>
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "objhelp.h"

extern "C" {

J9UTF8 *
buildClassNameJ9UTF8(J9VMThread *currentThread, U_32 memCategory, J9Class *clazz, U_8 *buffer, UDATA bufferSize, BOOLEAN *freeResult)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9ROMClass *romClass = clazz->romClass;

	*freeResult = FALSE;

	if (!J9ROMCLASS_IS_ARRAY(romClass)) {
		/* Non-array: the ROM class already has the correct complete name. */
		return J9ROMCLASS_CLASSNAME(romClass);
	}

	J9ArrayClass *arrayClazz = (J9ArrayClass *)clazz;
	UDATA arity = arrayClazz->arity;
	J9Class *leafComponentType = arrayClazz->leafComponentType;
	J9ROMClass *leafROMClass = leafComponentType->romClass;
	J9UTF8 *leafName = J9ROMCLASS_CLASSNAME(leafROMClass);
	bool isPrimitive = J9ROMCLASS_IS_PRIMITIVE_TYPE(leafROMClass);

	/* Compute the length of the class name.
	 *
	 * Primitive arrays are one '[' per level, plus the primitive type code
	 *  e.g. [[[B
	 * Object arrays are one '[' per level, plus 'L', plus the leaf type name, plus ';'
	 *  e.g. [[[[Lpackage.name.Class;
	 */
	UDATA nameLen = arity;
	if (isPrimitive) {
		nameLen += 1;
	} else {
		nameLen += (J9UTF8_LENGTH(leafName) + 2);
	}

	/* Use the supplied buffer when it is large enough, otherwise heap-allocate. */
	const UDATA allocLen = sizeof(J9UTF8) + nameLen;
	J9UTF8 *result = NULL;
	if ((NULL != buffer) && (allocLen <= bufferSize)) {
		result = (J9UTF8 *)buffer;
	} else {
		result = (J9UTF8 *)j9mem_allocate_memory(allocLen, memCategory);
		if (NULL == result) {
			return NULL;
		}
		*freeResult = TRUE;
	}

	J9UTF8_SET_LENGTH(result, (U_16)nameLen);
	U_8 *data = J9UTF8_DATA(result);
	memset(data, '[', arity);
	if (isPrimitive) {
		/* The type letter is at index [1] of the 1D primitive array ROM class name (e.g. "[I" -> 'I'). */
		data[arity] = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafComponentType->arrayClass->romClass))[1];
	} else {
		/* The / to . conversion is done later, so just copy the RAW class name here. */
		data[arity] = 'L';
		memcpy(data + arity + 1, J9UTF8_DATA(leafName), J9UTF8_LENGTH(leafName));
		data[nameLen - 1] = ';';
	}

	return result;
}

j9object_t
getClassNameString(J9VMThread *currentThread, j9object_t classObject, jboolean internAndAssign)
{
	j9object_t classNameObject = J9VMJAVALANGCLASS_CLASSNAMESTRING(currentThread, classObject);
	if (NULL == classNameObject) {
		U_8 onStackBuffer[64];
		BOOLEAN freeUTF8 = FALSE;

		J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
		J9UTF8 *utf8 = buildClassNameJ9UTF8(
				currentThread, J9MEM_CATEGORY_VM_JCL, clazz,
				onStackBuffer, sizeof(onStackBuffer), &freeUTF8);

		if (NULL == utf8) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
		} else {
			J9ROMClass *romClass = clazz->romClass;
			if (J9ROMCLASS_IS_ARRAY(romClass)) {
				J9ArrayClass *arrayClazz = (J9ArrayClass *)clazz;
				romClass = arrayClazz->leafComponentType->romClass;
			}
			bool anonClassName = J9_ARE_ANY_BITS_SET(
					romClass->extraModifiers,
					J9AccClassAnonClass | J9AccClassHidden);

			UDATA flags = J9_STR_XLAT;
			if (internAndAssign) {
				flags |= J9_STR_INTERN;
			}
			if (anonClassName) {
				flags |= J9_STR_ANON_CLASS_NAME;
			}
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, classObject);
			classNameObject = currentThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(
					currentThread,
					J9UTF8_DATA(utf8),
					J9UTF8_LENGTH(utf8),
					flags);
			classObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			if (internAndAssign && (NULL != classNameObject)) {
				J9VMJAVALANGCLASS_SET_CLASSNAMESTRING(currentThread, classObject, classNameObject);
			}
			if (freeUTF8) {
				PORT_ACCESS_FROM_VMC(currentThread);
				j9mem_free_memory(utf8);
			}
		}
	}

	return classNameObject;
}

} /* extern "C" */
