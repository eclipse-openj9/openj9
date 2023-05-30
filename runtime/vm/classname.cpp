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

j9object_t
getClassNameString(J9VMThread *currentThread, j9object_t classObject, jboolean internAndAssign)
{
	j9object_t classNameObject = J9VMJAVALANGCLASS_CLASSNAMESTRING(currentThread, classObject);
	if (NULL == classNameObject) {
		PORT_ACCESS_FROM_VMC(currentThread);
		U_8 *utfData = NULL;
		UDATA utfLength = 0;
		bool freeUTFData = false;
		U_8 onStackBuffer[64];
		bool anonClassName = false;

		J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
		J9ROMClass *romClass = clazz->romClass;
		if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
			J9ArrayClass *arrayClazz = (J9ArrayClass*)clazz;
			UDATA arity = arrayClazz->arity;
			J9Class *leafComponentType = arrayClazz->leafComponentType;
			J9ROMClass * leafROMClass = leafComponentType->romClass;
			J9UTF8 *leafName = J9ROMCLASS_CLASSNAME(leafROMClass);
			UDATA isPrimitive = J9ROMCLASS_IS_PRIMITIVE_TYPE(leafROMClass);

			/* Compute the length of the class name.
			 *
			 * Primitive arrays are one [ per level plus the primitive type code
			 * 		e.g. [[[B
			 * Object arrays are one [ per level plus L plus the leaf type name plus ;
			 * 		e.g. [[[[Lpackage.name.Class;
			 */
			utfLength = arity;
			if (isPrimitive) {
				utfLength += 1;
			} else {
				utfLength += (J9UTF8_LENGTH(leafName) + 2);
			}

			/* Create the name in UTF8, using an on-stack buffer if possible */

			if (utfLength <= sizeof(onStackBuffer)) {
				utfData = onStackBuffer;
			} else {
				utfData = (U_8*)j9mem_allocate_memory(utfLength, J9MEM_CATEGORY_VM_JCL);
				freeUTFData = true;
			}
			if (NULL == utfData) {
				setNativeOutOfMemoryError(currentThread, 0, 0);
			} else {
				memset(utfData, '[', arity);
				if (isPrimitive) {
					utfData[arity] = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafComponentType->arrayClass->romClass))[1];
				} else {
					/* The / to . conversion is done later, so just copy the RAW class name here */
					utfData[arity] = 'L';
					memcpy(utfData + arity + 1, J9UTF8_DATA(leafName), J9UTF8_LENGTH(leafName));
					utfData[utfLength - 1] = ';';
				}
			}
			anonClassName = J9_ARE_ANY_BITS_SET(leafROMClass->extraModifiers, J9AccClassAnonClass | J9AccClassHidden);
		} else {
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
			utfLength = J9UTF8_LENGTH(className);
			utfData = J9UTF8_DATA(className);
			anonClassName = J9_ARE_ANY_BITS_SET(romClass->extraModifiers, J9AccClassAnonClass | J9AccClassHidden);
		}

		if (NULL != utfData) {
			UDATA flags = J9_STR_XLAT;
			if (internAndAssign) {
				flags |= J9_STR_INTERN;
			}
			if (anonClassName) {
				flags |= J9_STR_ANON_CLASS_NAME;
			}
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, classObject);
			classNameObject = currentThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, utfData, utfLength, flags);
			classObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			if (internAndAssign && (NULL != classNameObject)) {
				J9VMJAVALANGCLASS_SET_CLASSNAMESTRING(currentThread, classObject, classNameObject);
			}
			if (freeUTFData) {
				j9mem_free_memory(utfData);
			}
		}
	}

	return classNameObject;
}

} /* extern "C" */
