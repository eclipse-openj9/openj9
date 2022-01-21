/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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

#if !defined(LAYOUTFFITYPEHELPERS_HPP_)
#define LAYOUTFFITYPEHELPERS_HPP_

#include "ut_j9vm.h"
#include "vm_internal.h"
#if JAVA_SPEC_VERSION >= 16
#include "ffi.h"
#endif /* JAVA_SPEC_VERSION >= 16 */

#define J9VM_LAYOUT_STRING_ON_STACK_LIMIT 128

class LayoutFFITypeHelpers
{
#if JAVA_SPEC_VERSION >= 16
	/* Data members */
private:
	J9VMThread *const _currentThread;
	J9JavaVM *const _vm;

	/* Function members */
public:
	LayoutFFITypeHelpers(J9VMThread *currentThread)
			: _currentThread(currentThread)
			, _vm(_currentThread->javaVM)
	{ };

	/**
	 * @brief Convert the preceding integer of a layout string from string to UDATA,
	 * and put layout at the position after the integer
	 *
	 * @param layout[in] A pointer to a c string describing the types of the struct elements. For example:
	 * 		If layout is "16#2[#2[II]#2[II]]", it returns 16 in byte
	 * @return The preceding integer converted from string to UDATA
	 */
	static VMINLINE UDATA
	getIntFromLayout(char **layout)
	{
		char *currentLayout = *layout;
		UDATA sumBytes = 0;
		while ('\0' != *currentLayout) {
			char symb = *currentLayout;
			if ((symb >= '0') && (symb <= '9')) {
				sumBytes = (sumBytes * 10) + (symb - '0');
			} else {
				break;
			}
			currentLayout += 1;
		}
		*layout = currentLayout;
		return sumBytes;
	}

	/**
	 * @brief Convert argument or return type from the type of ffi_type to J9NativeTypeCode
	 * @param ffiType[in] The pointer to ff_type
	 * @return The J9NativeTypeCode corresponding to the J9Class
	 */
	static VMINLINE U_8
	getJ9NativeTypeCodeFromFFIType(ffi_type *ffiType)
	{
		U_8 typeCode = 0;

		switch (ffiType->type) {
		case FFI_TYPE_VOID:
			typeCode = J9NtcVoid;
			break;
		case FFI_TYPE_UINT8:
			typeCode = J9NtcBoolean;
			break;
		case FFI_TYPE_SINT8:
			typeCode = J9NtcByte;
			break;
		case FFI_TYPE_UINT16:
			typeCode = J9NtcChar;
			break;
		case FFI_TYPE_SINT16:
			typeCode = J9NtcShort;
			break;
		case FFI_TYPE_SINT32:
			typeCode = J9NtcInt;
			break;
		case FFI_TYPE_SINT64:
			typeCode = J9NtcLong;
			break;
		case FFI_TYPE_FLOAT:
			typeCode = J9NtcFloat;
			break;
		case FFI_TYPE_DOUBLE:
			typeCode = J9NtcDouble;
			break;
		case FFI_TYPE_POINTER:
			typeCode = J9NtcPointer;
			break;
		case FFI_TYPE_STRUCT:
			typeCode = J9NtcStruct;
			break;
		default:
			Assert_VM_unreachable();
			break;
		}

		return typeCode;
	}

	/**
	 * @brief obtain the ffi_type from the layout symbol (the preceding letter of the layout type. e.g. I for INT)
	 *
	 * @param layoutSymb[in] The layout symbol describing the type of the layout
	 * @return The pointer to the ffi_type corresponding to the layout symbol
	 */
	static VMINLINE ffi_type*
	getPrimitiveFFIType(char layoutSymb)
	{
		ffi_type* typeFFI = NULL;

		switch (layoutSymb) {
		case 'V': /* VOID */
			typeFFI = &ffi_type_void;
			break;
#if JAVA_SPEC_VERSION >= 18
		case 'B': /* C_BOOL */
			typeFFI = &ffi_type_uint8;
			break;
#endif /* JAVA_SPEC_VERSION >= 18 */
		case 'C': /* C_CHAR */
			typeFFI = &ffi_type_sint8;
			break;
		case 'S': /* C_SHORT */
			typeFFI = &ffi_type_sint16;
			break;
		case 'I': /* C_INT */
			typeFFI = &ffi_type_sint32;
			break;
		case 'J': /* 8 bytes for either C_LONG (Linux) or C_LONG_LONG(maps to long long specific to Windows/AIX 64-bit) */
			typeFFI = &ffi_type_sint64;
			break;
		case 'F': /* C_FLOAT */
			typeFFI = &ffi_type_float;
			break;
		case 'D': /* C_DOUBLE */
			typeFFI = &ffi_type_double;
			break;
		case 'P': /* C_POINTER */
			typeFFI = &ffi_type_pointer;
			break;
		default:
			Assert_VM_unreachable();
		}

		return typeFFI;
	}

	/**
	 * @brief Create a FFI type from a layout string object for both primitive and struct
	 *
	 * @param typeFFI[in] The custom FFI type to be created
	 * @param layoutStringObject[in] An object containing the layout string describing the field types.
	 * @return The size of the layout, extracted from the layout string, UDATA_MAX if a failure occurs
	 */
	VMINLINE UDATA
	getLayoutFFIType(ffi_type** typeFFI, j9object_t layoutStringObject)
	{
		PORT_ACCESS_FROM_JAVAVM(_vm);
		*typeFFI = NULL;
		UDATA layoutSize = 0;
		char layoutBuffer[J9VM_LAYOUT_STRING_ON_STACK_LIMIT];
		char *layout = copyStringToUTF8WithMemAlloc(_currentThread, layoutStringObject,
				J9_STR_NULL_TERMINATE_RESULT, "", 0, layoutBuffer, sizeof(layoutBuffer), NULL);
		/* Preserve the original pointer for j9mem_free_memory() as
		 * subsequent calls will modify the contents of this pointer.
		 */
		char *layoutTemp = layout;
		if (NULL == layout) {
			goto done;
		}

		/* Check the byte size of the layout's size which is prefixed to the layout string */
		layoutSize = getIntFromLayout(&layoutTemp);
		if (layoutSize >= UDATA_MAX) {
			return layoutSize;
		}

		/* There are 2 kinds of layout: ValueLayout for primitive and GroupLayout for struct.
		 * e.g.
		 * 1) int: 4I  (4 represents the int layout's size in bytes)
		 * 2) struct:
		 *    struct struct_III{
		 *                       int elem1;
		 *                       int elem2;
		 *                       int elem3;
		 *                     };
		 *    with the layout (12#3[III]) as follows:
		 *    12#3[   (12 represents the struct layout's size in bytes while 3 represents the counts of elements)
		 *         I  (C_INT)
		 *         I  (C_INT)
		 *         I  (C_INT)
		 *        ]
		 *    where the header of the struct layout is "12#3".
		 * Thus, a struct layout string starts with the layout's size, a '#' as separator
		 * plus the count of struct elements as the header.
		 *
		 * Note: the layout string is preprocessed and prefixed with the header in
		 * ProgrammableInvoker.preprocessLayoutString() for easier handling in native.
		 */
		if ('#' == *layoutTemp) {
			*typeFFI = getStructFFIType(&layoutTemp);
		} else {
			*typeFFI = getPrimitiveFFIType(*layoutTemp);
		}

		if (layout != layoutBuffer) {
			j9mem_free_memory(layout);
		}

done:
		return layoutSize;
	}

	/**
	 * @brief Create an array of elements for a construct FFI type
	 *
	 * @param layout[in] A pointer to a c string describing the types of the struct elements
	 * @return An array of ffi_type* which are the elements of the struct
	 */
	ffi_type**
	getStructFFITypeElements(char **layout);

	/**
	 * @brief Create a struct FFI type
	 * @param layout[in] A pointer to a c string describing the types of the struct elements.
	 * For example:
	 * 1) "36#8[ (the padding bits (omitted here) are specified initially for each short-sized primitives to align with the longest one)
	 *         C  (C_BOOL) (introduced since Java 18)
	 *         C  (C_CHAR)
	 *         S  (C_SHORT)
	 *         I  (C_INT)
	 *         L  (C_LONG)
	 *         F  (C_FLOAT)
	 *         D  (C_DOUBLE)
	 *         P  (C_POINTER)
	 *     ]" is a struct with primitives & pointer (boolean, byte, short, int, float, long, double, pointer)
	 *
	 * 2) "16#2[
	 *        #2[
	 *           I  (C_INT)
	 *           I  (C_INT)
	 *          ]
	 *        #2[
	 *           I  (C_INT)
	 *           I  (C_INT)
	 *          ]
	 *        ]" is a struct with two nested structs
	 *
	 * 3) "12#2[
	 *        [4:   (an int array where ":" is the sign for array)
	 *           I  (C_INT)
	 *        ]
	 *        [2:  (a strcut arrays where ":" is the sign for array)
	 *          #2[
	 *             I  (C_INT)
	 *             I  (C_INT)
	 *            ] (Point)
	 *        ]
	 *       ]" is a struct with 2 arrays: int[4] and Point[2], where Point is a struct with 2 ints
	 *
	 * Note:
	 * 1) All the descriptions in the layout string are removed with preprocessLayoutString()
	 *    in ProgrammableInvoker in advance for easier parsing the layout string.
	 * 2) A struct pointer is treated as a generic pointer (C_POINTER in C corresponds to MemoryAddress in Java)
	 *    which is the same as a primitive pointer given there is no difference in terms of the layout string.
	 *
	 * @return The ffi_type of type FFI_TYPE_STRUCT
	 */
	ffi_type*
	getStructFFIType(char **layout);

	/**
	 * @brief Create an array FFI type describing an array as a field in a struct
	 *
	 * @param layout[in] A c string describing the type of the array elements
	 * @param nElements[in] The number of elements in the array
	 * @return A ffi_type* describing the array
	 */
	ffi_type*
	getArrayFFIType(char **layout, UDATA nElements);

	/**
	 * @brief Free a struct FFI type
	 *
	 * @param ffi[in] A pointer to a ffi type
	 */
	void
	freeStructFFIType(ffi_type *ffi);

	/**
	 * @brief Free the elements of a struct FFI type
	 *
	 * @param elements[in] The elements of a struct FFI type
	 */
	void
	freeStructFFITypeElements(ffi_type **elements);

#endif /* JAVA_SPEC_VERSION >= 16 */
};

#endif /* LAYOUTFFITYPEHELPERS_HPP_ */
