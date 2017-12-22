/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#include "FFITypeHelpers.hpp"

#ifdef J9VM_OPT_PANAMA
ffi_type**
FFITypeHelpers::getStructFFITypeElements(char **layout, bool inPtr)
{
	PORT_ACCESS_FROM_JAVAVM(_vm);
	
	ffi_type **elements = NULL;
	UDATA nElements = 0;
	char *currentLayout = *layout;
	bool localInPtr = inPtr;
	
	if (!inPtr) {
		elements = (ffi_type **) j9mem_allocate_memory(sizeof(ffi_type*) * strlen(*layout), OMRMEM_CATEGORY_VM);
		if (NULL == elements) {
			goto doneGetStructFFITypeElements;
		}
	}

	while (*currentLayout != '\0') {
		char symb = *currentLayout;
		switch (symb) {
		case '*': /* flexible array (eg. int a[];) */
			/* Fall through is intentional */
		case ':': /* start of pointer, skip the next symbol */
			localInPtr = true;
			break;
		case '[': {
			/* start of nested struct */
			ffi_type *result = getStructFFIType(&currentLayout, localInPtr);
			if (!localInPtr) {
				if (NULL == result) {
					freeStructFFITypeElements(elements);
					elements = NULL;
					goto doneGetStructFFITypeElements;
				}
				elements[nElements] = result;
				nElements++;
			}
			if (!inPtr) {
				localInPtr = false;
			}
			break;
		}
		case ']': /* end of struct */
			*layout = currentLayout;
			if (!localInPtr) {
				elements[nElements] = NULL;
			}
			goto doneGetStructFFITypeElements;
		case '0': 
		case '1': 
		case '2': 
		case '3': 
		case '4': 
		case '5': 
		case '6': 
		case '7': 
		case '8': 
		case '9': {
			/* number of elements in the array, followed by the type of the elements */
			UDATA nArray = getIntFromLayout(&currentLayout);
			ffi_type *result = getArrayFFIType(&currentLayout, nArray, inPtr);
			if (!localInPtr) {
				if (NULL == result) {
					freeStructFFITypeElements(elements);
					elements = NULL;
					goto doneGetStructFFITypeElements;
				}
				elements[nElements] = result;
				nElements++;
			}
			break;
		}
		default: /* primitive */
			if (!localInPtr) {
				elements[nElements] = getPrimativeFFITypeElement(symb);
				nElements++;
			} else if (!inPtr) {
				localInPtr = false;
			}
		}
		currentLayout++;
	}
doneGetStructFFITypeElements:
	return elements;
}

ffi_type*
FFITypeHelpers::getStructFFIType(char **layout, bool inPtr)
{
	ffi_type *typeFFI = NULL;
	/** The Layout is incremented for the '[' case in getStructFFITypeElements(char **layout, bool inPtr).
	 * The remaining layout string is then traversed when getStructFFITypeElements(char **layout, bool inPtr) is called again recursively.
	 */
	(*layout)++;
	ffi_type **result = getStructFFITypeElements(layout, inPtr);
	if (!inPtr) {
		PORT_ACCESS_FROM_JAVAVM(_vm);
		typeFFI = (ffi_type *)j9mem_allocate_memory(sizeof(ffi_type), OMRMEM_CATEGORY_VM);
		if (NULL == typeFFI) {
			setNativeOutOfMemoryError(_currentThread, 0, 0);
			goto doneGetStructFFIType;
		}
		typeFFI->size = 0;
		typeFFI->alignment = 0;
		typeFFI->type = FFI_TYPE_STRUCT;
		typeFFI->elements = result;
	} else {
		Assert_VM_Null(result);
	}
doneGetStructFFIType:
	return typeFFI;
}

void
FFITypeHelpers::freeStructFFITypeElements(ffi_type **elements)
{
	PORT_ACCESS_FROM_JAVAVM(_vm);
	if (NULL != elements) {
		for (U_32 i = 0; elements[i] != NULL; i++) {
			freeStructFFIType(elements[i]);
		}
		j9mem_free_memory(elements);
	}
}

void
FFITypeHelpers::freeStructFFIType(ffi_type *ffiType)
{
	if (NULL != ffiType) {
		PORT_ACCESS_FROM_JAVAVM(_vm);
		if (FFI_TYPE_STRUCT == ffiType->type) {
			/* ffiType->elements is NULL weve already seen this a free'd it.
			 * This will occur when dealing with inlined arrays.
			 */
			if (NULL != ffiType->elements) {
				freeStructFFITypeElements(ffiType->elements);
				ffiType->elements = NULL;
				j9mem_free_memory(ffiType);
				/* TODO we need a better way to deal with
				 * inlined arrays, the current solution just makes a
				 * struct with identical types. We need to investigate
				 * whether libffi has support for arrays.
				 */
			}
		}
	}
}
#endif /* J9VM_OPT_PANAMA */
