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
			, _vm(currentThread->javaVM)
	{
	}

	/**
	 * @brief Convert the preceding integer of a layout string from string to UDATA,
	 * and put layout at the position after the integer.
	 *
	 * @param layout[in/out] A pointer to a C string describing the types of the struct elements,
	 * which points to the symbol of the struct/array upon return after computing the digits
	 *
	 * For example, if layout is "16#2[#2[II]#2[II]]", it returns 16 which represents
	 * the size of the native struct in bytes.
	 *
	 * Note: it is also used to obtain the array count e.g. 5:I for an int array with 5 elements.
	 * @return The preceding integer converted from string to UDATA
	 */
	static VMINLINE UDATA
	getIntFromLayout(const char **layout)
	{
		const char *currentLayout = *layout;
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
	 * @brief Get the length of the specified struct layout string by counting the bracket symbols
	 * for the struct to compute the length.
	 *
	 * For example, if a struct layout string is "#2[II]", it returns 6.
	 *
	 * @param layout[in] A pointer to a C string describing the types of the struct elements
	 * @return The length of the struct layout string
	 */
	static VMINLINE UDATA
	getLengthOfStructLayout(const char *layout)
	{
		UDATA bracketCounter = 0;

		for (const char *cursor = layout;; ++cursor) {
			switch (*cursor) {
			case '\0':
				/* Malformed input. */
				return 0;
			case '[':
				bracketCounter += 1;
				break;
			case ']':
				if (0 == bracketCounter) {
					/* Malformed input. */
					return 0;
				}
				bracketCounter -= 1;
				if (0 == bracketCounter) {
					/* Return as we reach the boundary of the specified struct layout string. */
					return cursor - layout + 1;
				}
				break;
			default:
				/* Do nothing. */
				break;
			}
		}
	}

	/**
	 * @brief Convert argument or return type from the type of ffi_type to J9NativeTypeCode.
	 *
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
#if defined(J9ZOS390) && defined(J9VM_ENV_DATA64)
		case FFI_TYPE_STRUCT_FF:
		case FFI_TYPE_STRUCT_DD:
#endif /* defined(J9ZOS390) && defined(J9VM_ENV_DATA64) */
			typeCode = J9NtcStruct;
			break;
		default:
			Assert_VM_unreachable();
			break;
		}

		return typeCode;
	}

	/**
	 * @brief Obtain the ffi_type from the layout symbol (the preceding letter of the layout type. e.g. I for INT).
	 *
	 * @param layoutSymb the layout symbol describing the type of the layout
	 * @return The pointer to the ffi_type corresponding to the layout symbol
	 */
	static VMINLINE ffi_type *
	getPrimitiveFFIType(char layoutSymb)
	{
		ffi_type *typeFFI = NULL;

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
	 * @brief Create a FFI type from a layout string object for both primitive and struct.
	 *
	 * @param typeFFI[out] The custom FFI type to be created
	 * @param layoutStringObject[in] An object containing the layout string describing the field types
	 * @return The size of the layout, extracted from the layout string, UDATA_MAX if a failure occurs
	 */
	VMINLINE UDATA
	getLayoutFFIType(ffi_type **typeFFI, j9object_t layoutStringObject)
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
		const char *layoutTemp = layout;
		if (NULL == layout) {
			goto done;
		}

		/* Check the byte size of the layout's size which is prefixed to the layout string. */
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
		 * Note: the layout string is preprocessed and prefixed with the header in java
		 * for easier handling in native.
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
	 * @brief Create an array of elements for a construct FFI type.
	 *
	 * @param layout[in/out] A pointer to a C string describing the types of the struct elements,
	 * which points to the end of the struct layout string upon return
	 * @return An array of ffi_type * which are the elements of the struct
	 */
	ffi_type **
	getStructFFITypeElements(const char **layout);

	/**
	 * @brief Create a struct FFI type.
	 *
	 * @param layout[in/out] A pointer to a C string describing the types of the struct elements,
	 * which points to the end of the struct layout string upon return
	 * @return The ffi_type of type FFI_TYPE_STRUCT
	 *
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
	 *        [2:  (a struct array where ":" is the sign for array)
	 *          #2[
	 *             I  (C_INT)
	 *             I  (C_INT)
	 *            ] (Point)
	 *        ]
	 *       ]" is a struct with 2 arrays: int[4] and Point[2], where Point is a struct with 2 ints
	 *
	 * Note:
	 * 1) All the descriptions in the layout string are removed with preprocessLayoutString()
	 *    in ProgrammableInvoker/DowncallLinker in advance for easier parsing the layout string.
	 * 2) A struct pointer is treated as a generic pointer (C_POINTER in C corresponds to MemoryAddress in Java)
	 *    which is the same as a primitive pointer given there is no difference in terms of the layout string.
	 */
	ffi_type *
	getStructFFIType(const char **layout);

	/**
	 * @brief Create an array FFI type describing an array as a field in a struct.
	 *
	 * @param layout[in/out] A C string describing the type of the array elements,
	 * which points to the end of the array layout string upon return
	 * @param nElements[in] The number of elements in the array
	 * @return A ffi_type * describing the array
	 */
	ffi_type *
	getArrayFFIType(const char **layout, UDATA nElements);

	/**
	 * @brief Free a struct FFI type.
	 *
	 * @param ffi[in] A pointer to a ffi type
	 */
	void
	freeStructFFIType(ffi_type *ffi);

	/**
	 * @brief Free the elements of a struct FFI type.
	 *
	 * @param elements[in] The elements of a struct FFI type
	 */
	void
	freeStructFFITypeElements(ffi_type **elements);

	/* @brief Encode a signature string to a struct representing the signature to be used
	 * to handle the argument or return value in the upcall.
	 *
	 * @param cSignature[in] A pointer to a preprocessed signature string
	 * @param sigType[in] A pointer to the J9UpcallSigType struct
	 */
	static void
	encodeUpcallSignature(const char *cSignature, J9UpcallSigType *sigType)
	{
		if ((*cSignature >= '0') && (*cSignature <= '9')) {
			sigType->sizeInByte = (U_32)getIntFromLayout(&cSignature);
			cSignature += 1; /* Skip over '#' to the signature. */

			/* The start of a struct/union signature string. */
			if (('[' == *cSignature) || ('{' == *cSignature)) {
				sigType->type = encodeOuterStruct(cSignature, sigType->sizeInByte);
			} else {
				sigType->type = encodeUpcallPrimitive(cSignature);
			}
		} else {
			Assert_VM_unreachable();
		}
	}

	/* @brief Encode a primitive signature string to a predefined type which
	 * is set in a struct representing the signature in the upcall.
	 *
	 * @param cSignature[in] A pointer to a preprocessed signature string
	 * @return An encoded type of the primitive signature
	 */
	static U_8
	encodeUpcallPrimitive(const char *cSignature)
	{
		U_8 primSigType = 0;

		switch (*cSignature) {
		case 'V': /* The void type on return */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_VOID;
			break;
#if JAVA_SPEC_VERSION >= 18
		case 'B': /* C_BOOL in 1 byte */
#endif /* JAVA_SPEC_VERSION >= 18 */
		case 'C': /* C_CHAR in 1 byte */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_CHAR;
			break;
		case 'S': /* C_SHORT in 2 bytes */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_SHORT;
			break;
		case 'I': /* C_INT in 4 bytes */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_INT32;
			break;
		case 'J': /* C_LONG or C_LONG_LONG(Windows/AIX 64bit) in 8 bytes */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_INT64;
			break;
		case 'F': /* C_FLOAT in 4 bytes */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_FLOAT;
			break;
		case 'D': /* C_DOUBLE in 8 bytes */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_DOUBLE;
			break;
		case 'P': /* C_POINTER in 8 bytes */
			primSigType = J9_FFI_UPCALL_SIG_TYPE_POINTER;
			break;
		default:
			Assert_VM_unreachable();
			break;
		}

		return primSigType;
	}

	/* @brief Compare the struct (rather than the union) size and the specified layout ([FF]/[DD]) size
	 * in bytes to determine whether they are identical on z/OS or not in the case of ALL_SP/ALL_DP.
	 *
	 * @param structSig[in] A pointer to the specified struct signature string
	 * @param structSize[in] the struct size in bytes
	 * @param layoutSize[in] the layout size in bytes
	 * @return true if they are identical on z/OS; false otherwise
	 */
	static bool
	isStructWithFFOrDD(const char *structSig, U_32 structSize, U_32 layoutSize)
	{
#if defined(J9ZOS390)
		/* Check whether the specified signature is a union (starting from '{')
		 * which must be excluded given a union is treated as a non-complex
		 * type according to the z/OS ABI.
		 *
		 * Note:
		 * [1] The following float/double related structs ('T' stands for float or double) are complex:
		 * 1) struct {T a[1]; T b[1];} which is"[1:F1:F]" or "[1:D1:D]" in the signature string.
		 * 2) struct {T a[1]; T b;} which is "[1:FF]" or "[1:DD]" in the signature string.
		 * 3) struct {T a; T b[1];} which is "[F1:F]" or "[D1:D]" in the signature string.
		 * [2] The following float/double related union/structs are non-complex even though they are
		 * ALL_SP/ALL_DP with 8/16 bytes in size:
		 * 1) union {T a[2]; T b;} which is "{2:F}" or "{2:D}" in the signature string.
		 * 2) struct {T a[2];} which is "[2:F]" or "[2:D]" in the signature string.
		 * 3) struct {struct {T a; T b;} c;} which is "[[FF]]" or "[[DD]]" in the signature string.
		 */
		return ((structSize == layoutSize) /* The struct must be 8-byte or 16-byte in size. */
				&& '{' != structSig[0]) /* A union starts from '{'. */
				&& !strstr(structSig, "[[FF]]") /* A struct nested by "[[FF]]" or its variant. */
				&& !strstr(structSig, "[[DD]]") /* A struct nested by "[[DD]]" or its variant. */
				&& !(('2' == structSig[1]) && (':' == structSig[2])); /* "[2:F]" or "[2:D]" contains "2:" from index 1. */
#else /* defined(J9ZOS390) */
		return true;
#endif /* defined(J9ZOS390) */
	}

	/* @brief Determine whether the current platform is z/OS or not.
	 *
	 * @return true for z/OS; false otherwise
	 */
	static bool
	isZos()
	{
#if defined(J9ZOS390)
		return true;
#else /* defined(J9ZOS390) */
		return false;
#endif /* defined(J9ZOS390) */
	}

	/* @brief This wrapper function invokes parseStruct() to determine
	 * the AGGREGATE subtype of the specified struct.
	 *
	 * @param structSig[in] A pointer to the specified struct signature string
	 * @param sizeInByte[in] the struct size in bytes
	 * @return An encoded type of the struct signature
	 */
	static U_8
	encodeOuterStruct(const char *structSig, U_32 sizeInByte)
	{
		const char *initSig = structSig;
		bool isAllSP = true;
		bool isAllDP = true;
		U_8 structSigType = 0;
		U_8 first16ByteComposTypes[J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH] = {0};
		UDATA curIndex = 0;

		/* Analyze the specified native signature to fill up a 16-byte composition type
		 * array so as to determine the aggregate subtype.
		 */
		parseStruct(&structSig, &isAllSP, &isAllDP, first16ByteComposTypes, &curIndex);

		/* As per the z/OS ABI, ALL_SP/ALL_DP are only intended for struct {float, float} and
		 * struct {double, double} or their variants as the complex type placed on FPRs given
		 * the struct size is 8 bytes for [FF]/variant or 16 bytes for [DD]/variant; otherwise,
		 * the non-complex types are categorized into AGGREGATE_OTHER placed on GPRs.
		 *
		 * e.g.
		 * 1) struct {float, union {float, float}} belong to ALL_SP while
		 *    struct {double, union {double, double}} belongs to ALL_DP.
		 * 2) struct {float a[2]} ([2:F] in terms of the layout string)
		 *    and struct {double b[2]} ([2:D] in terms of the layout string)
		 *    belong to AGGREGATE_OTHER placed on GPRs.
		 */
		if (isAllSP && isStructWithFFOrDD(initSig, sizeInByte, J9_FFI_UPCALL_STRUCT_FF_SIZE)) {
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP;
		} else if (isAllDP && isStructWithFFOrDD(initSig, sizeInByte, J9_FFI_UPCALL_STRUCT_DD_SIZE)) {
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP;
		} else if (isZos() || (sizeInByte > J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH)) {
			/* AGGREGATE_OTHER (mix of different types without pure float/double) is
			 * intended for the native signature greater than 16 bytes in size
			 */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER;
		} else {
			/* Analyze the 16-byte composition type array to determine the aggregate subtype
			 * of the native signature which is equal to or less than 16 bytes in size).
			 */
			structSigType = getStructSigTypeFrom16ByteComposTypes(first16ByteComposTypes);
		}

		return structSigType;
	}

	/* @brief This function is invoked recursively to parse each element in a struct signature
	 * which is used to determine the AGGREGATE subtype of the struct.
	 *
	 * To help analyze the aggregate subtype, we use a 16-byte composition type array
	 * in which each element represents a 1-byte of composition types:
	 * 'U' --- this 1-byte cell is 0 as it is undefined or unused
	 * 'E' --- this 1-byte cell is filled with padding
	 * 'M' --- this 1-byte cell is part of any integer byte without pure float/double
	 * 'F' --- this 1-byte cell is part of a single-precision floating point
	 * 'D' --- this 1-byte cell is part of a double-precision floating point
	 *
	 * Note:
	 * This array is unused if the native signature is bigger than 16 bytes in size.
	 *
	 * @param currentStructSig[in/out] A pointer to the current location of a struct signature string,
	 * which points to the end of the struct signature string upon return
	 * @param isAllSP[in] A pointer to boolean indicating whether the struct only contains floats
	 * @param isAllDP[in] A pointer to boolean indicating whether the struct only contains doubles
	 * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
	 * @param currentIndex[in] A pointer to the current index to the 16-byte composition type array
	 */
	static void
	parseStruct(const char **currentStructSig, bool *isAllSP, bool *isAllDP, U_8 *first16ByteComposTypes, UDATA *currentIndex)
	{
		UDATA arrayLength = 0;
		UDATA paddingBytes = 0;
		const char *curStruSig = *currentStructSig;

		curStruSig += 1; /* Skip over '[' to the 1st element type of struct. */
		while ('\0' != *curStruSig) {
			switch (*curStruSig) {
#if JAVA_SPEC_VERSION >= 18
			case 'B': /* C_BOOL in 1 byte */
#endif /* JAVA_SPEC_VERSION >= 18 */
			case 'C': /* C_CHAR in 1 byte */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_M, 1, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case 'S': /* C_SHORT in 2 bytes */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_M, 2, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case 'I': /* C_INT in 4 bytes */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_M, 4, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case 'J': /* C_LONG or C_LONG_LONG(Windows 64bit) in 8 bytes. */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_M, 8, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case 'P': /* C_POINTER in 8 bytes */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_M, 8, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case 'F': /* C_FLOAT in 4 bytes */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_F, 4, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case 'D': /* C_DOUBLE in 8 bytes */
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_D, 8, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case '(': /* The start of the padding bytes explicitly specified by users. */
				curStruSig += 1; /* Skip over '(' to the padding bytes explicitly specified in java. */
				paddingBytes = getIntFromLayout(&curStruSig);
				setByteCellforPrimitive(isAllSP, isAllDP, first16ByteComposTypes, currentIndex, J9_FFI_UPCALL_COMPOSITION_TYPE_E, paddingBytes, 0);
				break;
			case '[': /* The start of a nested struct signature. */
			case '{': /* The start of a nested union signature. */
				setByteCellforStruct(&curStruSig, isAllSP, isAllDP, first16ByteComposTypes, currentIndex, arrayLength);
				arrayLength = 0; /* Reset for the next array if exists. */
				break;
			case ']': /* The end of a struct signature. */
			case '}': /* The end of a union signature. */
				*currentStructSig = curStruSig;
				return;
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
				/* Get the array count followed by the array type. */
				arrayLength = getIntFromLayout(&curStruSig);
				break;
			}
			default:
				Assert_VM_unreachable();
				break;
			}

			/* Move to the next element of the struct
			 * or skip over ':' to the array element
			 * in the case of array.
			 */
			curStruSig += 1;
		}
	}

	/* @brief Fill in a U_8[16] array with the specified composition type for primitives and set the sign
	 * for the float/double type which helps determine the AGGREGATE subtype of struct.
	 *
	 * @param isAllSP[in] A pointer to boolean indicating whether the struct only contains floats
	 * @param isAllDP[in] A pointer to boolean indicating whether the struct only contains doubles
	 * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
	 * @param currentIndex[in] A pointer to the current index to the 16-byte composition type array
	 * @param composType[in] The composition type for the primitive
	 * @param primTypeSize[in] The primitive size in bytes
	 * @param arrayLength[in] The length of the primitive array if exists
	 */
	static void
	setByteCellforPrimitive(bool *isAllSP, bool *isAllDP, U_8 *first16ByteComposTypes, UDATA *currentIndex, U_8 composType, UDATA primTypeSize, UDATA arrayLength)
	{
		UDATA arrLen = (arrayLength > 0) ? arrayLength : 1; /* Set 1 for non-array by default. */

		switch (composType) {
		case J9_FFI_UPCALL_COMPOSITION_TYPE_E: /* Part of padding bytes. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_M: /* Part of any integer byte. */
			/* It is neither  ALL_SP nor ALL_DP in the case of padding or any integer type. */
			*isAllSP = false;
			*isAllDP = false;
			break;
		case J9_FFI_UPCALL_COMPOSITION_TYPE_F: /* Part of a single-precision floating point. */
			*isAllDP = false;
			break;
		case J9_FFI_UPCALL_COMPOSITION_TYPE_D: /* Part of a double-precision floating point. */
			*isAllSP = false;
			break;
		default:
			Assert_VM_unreachable();
			break;
		}

		/* Only set the 16-byte composition type array with the first 16 bytes of the native signature. */
		while ((*currentIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH) && (arrLen > 0)) {
			for (UDATA typeSize = primTypeSize; typeSize > 0; typeSize--) {
				if (*currentIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH) {
					first16ByteComposTypes[*currentIndex] = composType;
					*currentIndex += 1;
				}
			}
			arrLen -= 1;
		}
	}

	/* @brief Fill in the 16-byte composition type array with the specified composition type for struct and copy
	 * the filled bytes of struct to the rest of the array in the case of a nested struct array.
	 *
	 * @param currentStructSig[in/out] A pointer to the current location of a struct signature string,
	 * which points to the end of the struct signature string upon return
	 * @param isAllSP[in] A pointer to boolean indicating whether the struct only contains floats
	 * @param isAllDP[in] A pointer to boolean indicating whether the struct only contains doubles
	 * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
	 * @param currentIndex[in] A pointer to the current index to the 16-byte composition type array
	 * @param arrayLength[in] The length of the struct array if exists
	 */
	static void
	setByteCellforStruct(const char **currentStructSig, bool *isAllSP, bool *isAllDP, U_8 *first16ByteComposTypes, UDATA *currentIndex, UDATA arrayLength)
	{
		/* Set 1 for non-array by default. */
		UDATA arrLen = (arrayLength > 0) ? arrayLength : 1;
		/* The start of the struct bytes to be filled in the composition type array. */
		UDATA startIndex = *currentIndex;
		UDATA composTypesSize = 0;

		parseStruct(currentStructSig, isAllSP, isAllDP, first16ByteComposTypes, currentIndex);
		/* The length of the filled bytes of struct in the composition type array. */
		composTypesSize = *currentIndex - startIndex;
		arrLen -= 1;

		/* Copy the filled bytes of struct to the rest of the array based on the length
		 * of the struct array till it reaches the end of the composition type array.
		 */
		while ((*currentIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH) && (arrLen > 0)) {
			for (UDATA offset = 0; offset < composTypesSize; offset++) {
				if (*currentIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH) {
					first16ByteComposTypes[*currentIndex] = first16ByteComposTypes[startIndex + offset];
					*currentIndex += 1;
				}
			}
			arrLen -= 1;
		}
	}

	/* @brief Check the merged composition types of both the first 8 bytes and the next 8 bytes
	 * of the 16-byte composition type array so as to determine the aggregate subtype of
	 * a struct equal to or less than 16 bytes in size).
	 *
	 * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
	 * @return an encoded AGGREGATE subtype for the struct signature
	 */
	static U_8
	getStructSigTypeFrom16ByteComposTypes(U_8 *first16ByteComposTypes)
	{
		U_8 structSigType = 0;
		U_8 first8ByteComposType = getComposTypeFrom8Bytes(first16ByteComposTypes, 0);
		U_8 second8ByteComposType = getComposTypeFrom8Bytes(first16ByteComposTypes, 8);
		U_8 structSigComposType = (first8ByteComposType << 4) | second8ByteComposType;

		switch (structSigComposType) {
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_E_D:
			/* The aggregate subtype is set for the struct {float, padding, double}. */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_F_D:
			/* The aggregate subtype is set for the struct {float, float, double}. */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_D_F_E:
			/* The aggregate subtype is set for the struct {double, float, padding}. */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_D_F_F:
			/* The aggregate subtype is set for the struct {double, float, float}. */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_M_F_E:
			/* The aggregate subtype is set for structs starting with the mix of any integer type/float
			 * in the first 8 bytes followed by one float in the second 8 bytes.
			 * e.g. {int, float, float} or {float, int, float}.
			 */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_M_F_F: /* Fall through */
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_M_D:
			/* The aggregate subtype is set for a struct starting with the mix of any integer type/float in the
			 * first 8 bytes followed by a double or two floats(treated as a double) in the second 8 bytes.
			 * e.g. {int, float, double}, {float, int, double}, {long, double}, {int, float, float, float}
			 * or {long, float, float}.
			 */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_E_M:
			/* The aggregate subtype is set for a struct starting with a float in the first 8 bytes
			 * followed by the mix of any integer type/float in the second 8 bytes.
			 * e.g. {float, padding, long}.
			 */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC;
			break;
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_F_M: /* Fall through */
		case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_D_M:
			/* The aggregate subtype is set for a struct starting with a double or two floats in the
			 * first 8 bytes, followed by the mix of any integer type/float in the second 8 bytes.
			 * e.g. {double, float, int}, {double, long} or  {float, float, float, int}
			 * or {float, float, long}.
			 */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC;
			break;
		default:
			/* The aggregate subtype is set for a struct mixed with any integer type/float
			 * without pure float/double in the first/second 8 bytes.
			 * e.g. {short a[3], char b} or {int, float, int, float}.
			 */
			structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC;
			break;
		}

		return structSigType;
	}

	/* @brief Merge 8 bytes of the 16-byte composition type array from the specified index
	 * to determine the composition types.
	 *
	 * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
	 * @param arrayIndex[in] An index to the 16-byte composition type array
	 * @return a composition type for an 8-byte of the 16-byte composition type array
	 */
	static U_8
	getComposTypeFrom8Bytes(U_8 *first16ByteComposTypes, UDATA arrayIndex)
	{
		U_8 composType = 0;
		U_8 low4ByteComposType = 0;
		U_8 high4ByteComposType = 0;

		/* The specified index to the U_8[16] composition type array must be 0 or 8. */
		Assert_VM_true(arrayIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH);
		Assert_VM_true(0 == (arrayIndex % J9_FFI_UPCALL_COMPOSITION_TYPE_DWORD_SIZE));

		/* Merge the low 4 bytes of 8 bytes from the specified index to the composition type array. */
		low4ByteComposType = getComposTypeFrom4Bytes(first16ByteComposTypes, arrayIndex);
		/* The first byte and the low 4 bytes of 8 bytes can't be padding bytes.
		 * e.g. a struct {padding, int, ...} is invalid if specified in java code.
		 */
		Assert_VM_true(J9_FFI_UPCALL_COMPOSITION_TYPE_E != low4ByteComposType);
		Assert_VM_true(J9_FFI_UPCALL_COMPOSITION_TYPE_E != first16ByteComposTypes[arrayIndex]);

		/* Merge the high 4 bytes of 8 bytes from the specified index to the composition type array. */
		high4ByteComposType = getComposTypeFrom4Bytes(first16ByteComposTypes, arrayIndex + J9_FFI_UPCALL_COMPOSITION_TYPE_WORD_SIZE);

		if ((J9_FFI_UPCALL_COMPOSITION_TYPE_F == low4ByteComposType)
		&& (J9_FFI_UPCALL_COMPOSITION_TYPE_U == high4ByteComposType)
		) {
			composType = J9_FFI_UPCALL_COMPOSITION_TYPE_F_E; /* Unused cell slots are treated as padding. */
		} else {
			composType = low4ByteComposType | high4ByteComposType;
		}

		/* 'D' and 'E' can't coexist in 8 bytes */
		Assert_VM_true(!J9_ARE_ALL_BITS_SET(composType, J9_FFI_UPCALL_COMPOSITION_TYPE_D_E));

		switch (composType) {
		case J9_FFI_UPCALL_COMPOSITION_TYPE_U: /* Undefined or unused. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_F: /* 8 bytes for single-precision floating point. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_F_E: /* 8 bytes for single-precision floating point (4 bytes) plus the padding bytes. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_D: /* 8 bytes for a double-precision floating point. */
			break;
		default:
			composType = J9_FFI_UPCALL_COMPOSITION_TYPE_M; /* 8 bytes for the mix of integer types without pure float/double. */
			break;
		}

		return composType;
	}

	/* @brief Merge 4 bytes of the 16-byte composition type array from the specified index
	 * to determine the composition types.
	 *
	 * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
	 * @param arrayIndex[in] An index to the 16-byte composition type array
	 * @return a composition type for a 4-byte of the 16-byte composition type array
	 */
	static U_8
	getComposTypeFrom4Bytes(U_8 *first16ByteComposTypes, UDATA arrayIndex)
	{
		U_8 composType = 0;

		/* The specified index to the 16-byte composition type array must be one of 0, 4, 8 and 12. */
		Assert_VM_true(arrayIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_ARRAY_LENGTH);
		Assert_VM_true(0 == (arrayIndex % J9_FFI_UPCALL_COMPOSITION_TYPE_WORD_SIZE));

		for (UDATA byteIndex = 0; byteIndex < J9_FFI_UPCALL_COMPOSITION_TYPE_WORD_SIZE; byteIndex++) {
			composType |= first16ByteComposTypes[arrayIndex + byteIndex];
		}

		switch (composType) {
		case J9_FFI_UPCALL_COMPOSITION_TYPE_U: /* Undefined or unused. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_E: /* 4 padding bytes. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_F: /* 4 bytes for single-precision floating point. */
		case J9_FFI_UPCALL_COMPOSITION_TYPE_D: /* 4 bytes of a double-precision floating point. */
			break;
		default:
			/* It is impossible that 'F' or 'D' partially exits in 4 bytes. */
			Assert_VM_true(J9_ARE_NO_BITS_SET(composType, (J9_FFI_UPCALL_COMPOSITION_TYPE_F | J9_FFI_UPCALL_COMPOSITION_TYPE_D)));
			composType = J9_FFI_UPCALL_COMPOSITION_TYPE_M; /* 4 bytes for the mix of integer types. */
			break;
		}

		return composType;
	}

#endif /* JAVA_SPEC_VERSION >= 16 */
};

#endif /* LAYOUTFFITYPEHELPERS_HPP_ */
