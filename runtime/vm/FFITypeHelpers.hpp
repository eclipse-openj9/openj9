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

#if !defined(FFITYPEHELPERS_HPP_)
#define FFITYPEHELPERS_HPP_

#include "ut_j9vm.h"
#include "vm_internal.h"
#ifdef J9VM_OPT_PANAMA
#include "ffi.h"
#endif /* J9VM_OPT_PANAMA */

#define J9VM_LAYOUT_STRING_ON_STACK_LIMIT 128

#ifdef J9VM_OPT_PANAMA
typedef struct J9NativeCalloutData {
	ffi_type **arguments;
	ffi_cif *cif;
} J9NativeCalloutData;
#endif /* J9VM_OPT_PANAMA */

class FFITypeHelpers
{
/*
 * Data members
 */
private:
#ifdef J9VM_OPT_PANAMA
	J9VMThread *const _currentThread;
	J9JavaVM *const _vm;
#endif /* J9VM_OPT_PANAMA */

protected:

public:

/*
 * Function members
 */
private:

protected:

public:

#ifdef J9VM_OPT_PANAMA
	/**
	 * @brief Convert argument or return type from J9Class to ffi_type.
	 * Only primitive types are currently supported. Objects will likely be passed as a pointer, so they are type long.
	 * @param type[in] The pointer to the J9Class of the type
	 * @return The pointer to the ffi_type corresponding to the J9Class
	 */
	const VMINLINE ffi_type*
	getFFIType(J9Class *type){
		ffi_type* typeFFI = NULL;

		if (type == _vm->voidReflectClass) {
			typeFFI = &ffi_type_void;
		} else if (type == _vm->booleanReflectClass) {
			typeFFI = &ffi_type_uint8;
		} else if (type == _vm->byteReflectClass) {
			typeFFI = &ffi_type_sint8;
		} else if (type == _vm->charReflectClass) {
			typeFFI = &ffi_type_uint16;
		} else if (type == _vm->shortReflectClass) {
			typeFFI = &ffi_type_sint16;
		} else if (type == _vm->intReflectClass) {
			typeFFI = &ffi_type_sint32;
		} else if (type == _vm->floatReflectClass) {
			typeFFI = &ffi_type_float;
		} else if (type == _vm->longReflectClass) {
			typeFFI = &ffi_type_sint64;
		} else if (type == _vm->doubleReflectClass) {
			typeFFI = &ffi_type_double;
		} else if (J9CLASS_IS_ARRAY(type) && ((J9ArrayClass*)type)->leafComponentType == J9VMJAVALANGOBJECT_OR_NULL(_vm)) {
			typeFFI = &ffi_type_pointer;
		} else {
			Assert_VM_unreachable();
		}

		return typeFFI;
	}

	/**
	 * @brief Create a custom FFI type from a layout string object
	 * @param typeFFI[in] The custom FFI type to be created
	 * @param layoutStringObject[in] An object containing the layout string describing the field types
	 * @return The size of the struct, extracted from the layout string, UDATA_MAX if a failure occurs
	 */
	VMINLINE UDATA
	getCustomFFIType(ffi_type** typeFFI, j9object_t layoutStringObject) {
		PORT_ACCESS_FROM_JAVAVM(_vm);

		*typeFFI = NULL;
		UDATA structSize = UDATA_MAX;
		char layoutBuffer[J9VM_LAYOUT_STRING_ON_STACK_LIMIT];
		char* layout = copyStringToUTF8WithMemAlloc(_currentThread, layoutStringObject, J9_STR_NULL_TERMINATE_RESULT, "", 0, layoutBuffer, sizeof(layoutBuffer), NULL);

		/* Subsequent calls will modify the contents of this pointer, so preserve the original pointer for j9mem_free_memory */
		char* layoutTemp = layout;

		if (NULL == layout) {
			goto doneGetCustomFFIType;
		}

		structSize = getIntFromLayout(&layoutTemp);
		*typeFFI = getStructFFIType(&layoutTemp, false);
doneGetCustomFFIType:
		if (layout != layoutBuffer) {
			j9mem_free_memory(layout);
		}
		return structSize;
	}

	/**
	 * @brief Create a customFFI type describing an array that is a field in a struct
	 * @param layout[in] A c string describing the type of the array elements
	 * @param nElements[in] The number of elements in the array
	 * @param inPtr[in] Describes whether the current symbols are in a pointer. No ffi_type should be allocated for symbols in a pointer.
	 * @return A ffi_type* describing the array
	 */
	VMINLINE ffi_type*
	getArrayFFIType(char **layout, UDATA nElements, bool inPtr) {
		PORT_ACCESS_FROM_JAVAVM(_vm);

		ffi_type *typeFFI = NULL;
		ffi_type *elementType = NULL;

		if (**layout == '[') {
			ffi_type* result = getStructFFIType(layout, inPtr);
			if (!inPtr) {
				if (NULL == result) {
					goto doneGetArrayFFIType;
				}
				elementType = result;
			}
		} else {
			elementType = getPrimativeFFITypeElement(**layout);
		}

		if (!inPtr && (elementType != NULL)) {
			typeFFI = (ffi_type *)j9mem_allocate_memory(sizeof(ffi_type), OMRMEM_CATEGORY_VM);
			if (NULL == typeFFI) {
				freeStructFFIType(elementType);
				goto doneGetArrayFFIType;
			}
			typeFFI->size = 0;
			typeFFI->alignment = 0;
			typeFFI->type = FFI_TYPE_STRUCT;
			typeFFI->elements = (ffi_type **) j9mem_allocate_memory(sizeof(ffi_type*) * (nElements + 1), OMRMEM_CATEGORY_VM);
			if (NULL == typeFFI->elements) {
				freeStructFFIType(elementType);
				j9mem_free_memory(typeFFI);
				typeFFI = NULL;
				goto doneGetArrayFFIType; }

			for (U_32 i = 0; i < nElements; i++) {
				typeFFI->elements[i] = elementType;
			}
			typeFFI->elements[nElements] = NULL;
		}
doneGetArrayFFIType:
		return typeFFI;
	}

	/**
	 * @brief Convert a symbol in the struct layout to ffi_type.
	 * @param symb[in] The symbol from the struct layout describing the type of the element
	 * @return The pointer to the ffi_type corresponding to the symbol
	 */
	VMINLINE ffi_type*
	getPrimativeFFITypeElement(char symb)
	{
		ffi_type* typeFFI = NULL;

		switch (symb) {
		case 'o':
			typeFFI = &ffi_type_sint8;
			break;
		case 'c':
			typeFFI = &ffi_type_sint8;
			break;
		case 's':
			typeFFI = &ffi_type_sint16;
			break;
		case 'i':
			typeFFI = &ffi_type_sint32;
			break;
		case 'l':
			typeFFI = &ffi_type_sint64;
			break;
		case 'F':
			typeFFI = &ffi_type_float;
			break;
		case 'D':
			typeFFI = &ffi_type_double;
			break;
		case 'p':
			typeFFI = &ffi_type_pointer;
			break;
		default:
			Assert_VM_unreachable();
		}

		return typeFFI;
	}

	/**
	 * @brief Convert the preceding integer of a layout string from string to UDATA, and put layout at the position after the integer
	 * @param layout[in] A pointer to a c string describing the types of the struct elements. For example:
	 * 		If layout is "16[[ii][ii]]", it becomes "[[ii][ii]]" and returns 16
	 * @return The preceding integer converted from string to UDATA
	 */
	VMINLINE UDATA
	getIntFromLayout(char **layout)
	{
		char *currentLayout = *layout;
		UDATA n = 0;
		while (*currentLayout != '\0') {
			char symb = *currentLayout;
			if (symb >= '0' && symb <= '9') {
				n = (n * 10) + (symb - '0');
			} else {
				break;
			}
			currentLayout++;
		}
		*layout = currentLayout;
		return n;
	}

	/**
	 * @brief Create an array of elements for a custom FFI type
	 * @param layout[in] A pointer to a c string describing the types of the struct elements
	 * @param inPtr[in] Describes whether the current symbols are in a pointer
	 * @return An array of ffi_type* which are the elements of the struct
	 */
	ffi_type**
	getStructFFITypeElements(char **layout, bool inPtr);

	/**
	 * @brief Create a custom FFI type
	 * @param layout[in] A pointer to a c string describing the types of the struct elements. For example:
	 * 		"[liFsooD]" is a struct with primitives (long, int, float, short, byte, byte, double)
	 *		"[p:p:cp:cip:[[ii][ii]]D]" is a struct with pointers (char**, char*, int, a struct pointer, double)
	 *		"[[ii][ii]]" is a struct with two nested structs
	 *		"[4i2[ii]*[ii]]" is a struct with 3 arrays: int[4], Point[2], and Point[], where Point is a struct with 2 ints
	 * @param inPtr[in] Describes whether the current symbols are in a pointer. No ffi_type should be allocated for symbols in a pointer.
	 * @return The ffi_type of type FFI_TYPE_STRUCT
	 */
	ffi_type*
	getStructFFIType(char **layout, bool inPtr);

	/**
	 * @brief Frees the elements of a struct FFI type
	 * @param elements[in] The elements of a struct FFI type
	 */
	void
	freeStructFFITypeElements(ffi_type **elements);

	/**
	 * @brief Free a custom FFI type
	 * @param ffi[in] A pointer to a ffi type
	 */
	void
	freeStructFFIType(ffi_type *ffi);

	FFITypeHelpers(J9VMThread *currentThread)
			: _currentThread(currentThread)
			, _vm(_currentThread->javaVM)
	{ };

#endif /* J9VM_OPT_PANAMA */
};

#endif /* FFITYPEHELPERS_HPP_ */
