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

#include "LayoutFFITypeHelpers.hpp"

#if JAVA_SPEC_VERSION >= 16

extern "C" void
freeAllStructFFITypes(J9VMThread *currentThread, void *cifNode)
{
	LayoutFFITypeHelpers ffiTypeHelpers(currentThread);
	J9JavaVM * vm = currentThread->javaVM;
	ffi_cif *cif = (ffi_cif *)cifNode;
	ffi_type **argTypes = cif->arg_types;
	ffi_type *retType = cif->rtype;

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL != argTypes) {
		for (U_32 argIndex = 0; argTypes[argIndex] != NULL; argIndex++) {
			ffiTypeHelpers.freeStructFFIType(argTypes[argIndex]);
		}
		j9mem_free_memory(argTypes);
		argTypes = NULL;
	}

	if (NULL != retType) {
		ffiTypeHelpers.freeStructFFIType(retType);
		retType = NULL;
	}
}

ffi_type *
LayoutFFITypeHelpers::getArrayFFIType(const char **layout, UDATA nElements)
{
	ffi_type *typeFFI = NULL;
	ffi_type *elementType = NULL;
	PORT_ACCESS_FROM_JAVAVM(_vm);

	/* A struct starts with "#" plus the count of struct elements,
	 * followed by the types of the struct elements.
	 * e.g. the preprocessed layout of
	 *     struct int_struct {
	 *         int elem1;
	 *         int elem2;
	 *     };
	 * is "#2[II]".
	 */
	if ('#' == **layout) {
		elementType = getStructFFIType(layout);
		if (NULL == elementType) {
			goto done;
		}
	} else {
		elementType = getPrimitiveFFIType(**layout);
	}

	if (elementType != NULL) {
		typeFFI = (ffi_type *)j9mem_allocate_memory(sizeof(ffi_type), J9MEM_CATEGORY_VM_FFI);
		if (NULL == typeFFI) {
			freeStructFFIType(elementType);
			goto done;
		}

		typeFFI->size = 0;
		typeFFI->alignment = 0;
		typeFFI->type = FFI_TYPE_STRUCT;
		typeFFI->elements = (ffi_type **)j9mem_allocate_memory(sizeof(ffi_type *) * (nElements + 1), J9MEM_CATEGORY_VM_FFI);
		if (NULL == typeFFI->elements) {
			freeStructFFIType(elementType);
			j9mem_free_memory(typeFFI);
			typeFFI = NULL;
			goto done;
		}

		for (U_32 elemIndex = 0; elemIndex < nElements; elemIndex++) {
			typeFFI->elements[elemIndex] = elementType;
		}
		typeFFI->elements[nElements] = NULL;
	}
done:
	return typeFFI;
}

ffi_type *
LayoutFFITypeHelpers::getStructFFIType(const char **layout)
{
	const char *currentLayout = *layout;
	J9LayoutStrFFITypeEntry *layoutStrFFITypeEntry = NULL;
	J9LayoutStrFFITypeEntry *resultEntry = NULL;
	ffi_type **structElements = NULL;
	ffi_type *typeFFI = NULL;
	PORT_ACCESS_FROM_JAVAVM(_vm);

	if (NULL == _vm->layoutStrFFITypeTable) {
		_vm->layoutStrFFITypeTable = createLayoutStrFFITypeTable(_vm);
		if (NULL == _vm->layoutStrFFITypeTable) {
			goto freeAllMemoryThenExit;
		}
	}

	layoutStrFFITypeEntry = (J9LayoutStrFFITypeEntry *)j9mem_allocate_memory(sizeof(J9LayoutStrFFITypeEntry), J9MEM_CATEGORY_VM_FFI);
	if (NULL == layoutStrFFITypeEntry) {
		goto freeAllMemoryThenExit;
	}

	layoutStrFFITypeEntry->layoutStrLength = getLengthOfStructLayout(currentLayout);
	if (0 == layoutStrFFITypeEntry->layoutStrLength) {
		/* Malformed input. */
		goto freeAllMemoryThenExit;
	}
	layoutStrFFITypeEntry->layoutStr = (U_8 *)j9mem_allocate_memory(layoutStrFFITypeEntry->layoutStrLength + 1, J9MEM_CATEGORY_VM_FFI);
	if (NULL == layoutStrFFITypeEntry->layoutStr) {
		goto freeAllMemoryThenExit;
	}

	memcpy(layoutStrFFITypeEntry->layoutStr, currentLayout, layoutStrFFITypeEntry->layoutStrLength);
	layoutStrFFITypeEntry->layoutStr[layoutStrFFITypeEntry->layoutStrLength] = '\0';

	/* Search the hashtable for the ffi_type if the same struct layout string exists. */
	resultEntry = findLayoutStrFFIType(_vm->layoutStrFFITypeTable, layoutStrFFITypeEntry);
	if (NULL != resultEntry) {
		j9mem_free_memory(layoutStrFFITypeEntry->layoutStr);
		j9mem_free_memory(layoutStrFFITypeEntry);
		/* Return the duplicate ffi_type for the same struct if found in the hashtable. */
		typeFFI = (ffi_type *)(resultEntry->structFFIType);
		/* Reach the end of the nested struct layout string so as to move to
		 * the next element in getStructFFITypeElements() if exists.
		 */
		(*layout) += resultEntry->layoutStrLength - 1;
		goto done;
	}

	/* Skip over the '#' case. */
	(*layout) = currentLayout + 1;

	/* The struct layout is incremented for the '[' case in getStructFFITypeElements(), in which case
	 * the remaining layout string is then traversed when getStructFFITypeElements() is called recursively.
	 */
	structElements = getStructFFITypeElements(layout);
	if (NULL == structElements) {
		goto freeAllMemoryThenExit;
	}

	typeFFI = (ffi_type *)j9mem_allocate_memory(sizeof(ffi_type), J9MEM_CATEGORY_VM_FFI);
	if (NULL == typeFFI) {
		goto freeAllMemoryThenExit;
	}
	typeFFI->size = 0;
	typeFFI->alignment = 0;
	typeFFI->type = FFI_TYPE_STRUCT;
	typeFFI->elements = structElements;

	/* Add the created ffi_type for the struct to the hashtable
	 * if the corresponding layout string doesn't exist.
	 */
	layoutStrFFITypeEntry->structFFIType = (void *)typeFFI;
	resultEntry = addLayoutStrFFIType(_vm->layoutStrFFITypeTable, layoutStrFFITypeEntry);
	if (NULL == resultEntry) {
		freeStructFFIType(typeFFI);
		typeFFI = NULL;
		goto freeAllMemoryThenExit;
	}

done:
	return typeFFI;

freeAllMemoryThenExit:
	if (NULL != layoutStrFFITypeEntry) {
		j9mem_free_memory(layoutStrFFITypeEntry->layoutStr);
		j9mem_free_memory(layoutStrFFITypeEntry);
	}
	freeStructFFITypeElements(structElements);
	setNativeOutOfMemoryError(_currentThread, 0, 0);
	goto done;
}

ffi_type **
LayoutFFITypeHelpers::getStructFFITypeElements(const char **layout)
{
	PORT_ACCESS_FROM_JAVAVM(_vm);
	const char *currentLayout = *layout;
	UDATA nElements = 0;
	UDATA elementCount = getIntFromLayout(&currentLayout);
	ffi_type **elements = (ffi_type **)j9mem_allocate_memory(sizeof(ffi_type *) * (elementCount + 1), J9MEM_CATEGORY_VM_FFI);
	if (NULL == elements) {
		goto done;
	}
	elements[elementCount] = NULL;
	currentLayout += 1; /* Skip over the '[' case to the struct elements. */

	while ('\0' != *currentLayout) {
		char symb = *currentLayout;
		switch (symb) {
		case '#': /* Start of nested struct. e.g. #5[...] */
		{
			ffi_type *result = getStructFFIType(&currentLayout);
			if (NULL == result) {
				freeStructFFITypeElements(elements);
				elements = NULL;
				goto done;
			}
			elements[nElements] = result;
			nElements += 1;
			break;
		}
		case ']': /* End of struct. */
			*layout = currentLayout;
			/* The last element of struct needs to be NULL for FFI_TYPE_STRUCT. */
			elements[nElements] = NULL;
			goto done;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		{
			/* Get the count of the array elements, followed by the elements's type.
			 * e.g.
			 * 1) "5:I" for an int array with 5 int elements (int[5])
			 * 2) "5:#2[II]" for a struct array with 5 struct elements (each stuct contains 2 integers).
			 */
			UDATA nArray = getIntFromLayout(&currentLayout);
			currentLayout += 1; /* Skip over the separator of an array (":") to the elements. */
			ffi_type *result = getArrayFFIType(&currentLayout, nArray);
			if (NULL == result) {
				freeStructFFITypeElements(elements);
				elements = NULL;
				goto done;
			}
			elements[nElements] = result;
			nElements += 1;
			break;
		}
		default:
			elements[nElements] = getPrimitiveFFIType(*currentLayout);
			nElements += 1;
		}
		currentLayout += 1;
	}

done:
	return elements;
}

void
LayoutFFITypeHelpers::freeStructFFIType(ffi_type *ffiType)
{
	if ((NULL != ffiType)
		&& ((FFI_TYPE_STRUCT == ffiType->type)
#if defined(J9ZOS390) && defined(J9VM_ENV_DATA64)
			|| (FFI_TYPE_STRUCT_FF == ffiType->type)
			|| (FFI_TYPE_STRUCT_DD == ffiType->type)
#endif /* defined(J9ZOS390) && defined(J9VM_ENV_DATA64) */
	)) {
		if (NULL != ffiType->elements) {
			PORT_ACCESS_FROM_JAVAVM(_vm);
			freeStructFFITypeElements(ffiType->elements);
			ffiType->elements = NULL;
			j9mem_free_memory(ffiType);
			ffiType = NULL;
		}
	}
}

void
LayoutFFITypeHelpers::freeStructFFITypeElements(ffi_type **elements)
{
	if (NULL != elements) {
		PORT_ACCESS_FROM_JAVAVM(_vm);
		for (U_32 elemIndex = 0; elements[elemIndex] != NULL; elemIndex++) {
			freeStructFFIType(elements[elemIndex]);
		}
		j9mem_free_memory(elements);
	}
}

#endif /* JAVA_SPEC_VERSION >= 16 */
