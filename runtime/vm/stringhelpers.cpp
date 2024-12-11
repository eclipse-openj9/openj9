/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "objhelp.h"
#include "j9cp.h"
#include "vm_internal.h"
#include "ut_j9vm.h"

#include "VMHelpers.hpp"

extern "C" {

/**
 * Compare a uncompressed java unicode array to another uncompressed java unicode array for equality.
 * @param *vmThread
 * @param *unicodeBytes1
 * @param *unicodeBytes2
 * @param length length for both unicode arrays
 * @returns 1 if the two arrays are equal, 0 otherwise
 */
UDATA
compareUncompressedUnicode(J9VMThread *vmThread, j9object_t unicodeBytes1, j9object_t unicodeBytes2, UDATA length)
{
	UDATA result = 1;
	if (unicodeBytes1 != unicodeBytes2) {
		UDATA i = 0;
		while (0 != length) {
			U_16 unicodeChar1 = J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes1, i);
			U_16 unicodeChar2 = J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes2, i);
			if (unicodeChar1 != unicodeChar2) {
				result = 0;
				break;
			}
			length -= 1;
			i += 1;
		}
	}
	return result;
}

/**
 * Compare a compressed java unicode array to another compressed java unicode array for equality.
 * @param *vmThread
 * @param *unicodeBytes1
 * @param *unicodeBytes2
 * @param length length for both unicode arrays
 * @returns 1 if the two arrays are equal, 0 otherwise
 */
UDATA
compareCompressedUnicode(J9VMThread *vmThread, j9object_t unicodeBytes1, j9object_t unicodeBytes2, UDATA length)
{
	UDATA result = 1;
	if (unicodeBytes1 != unicodeBytes2) {
		UDATA i = 0;
		while (0 != length) {
			U_16 unicodeChar1 = (U_8)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes1, i);
			U_16 unicodeChar2 = (U_8)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes2, i);
			if (unicodeChar1 != unicodeChar2) {
				result = 0;
				break;
			}
			length -= 1;
			i += 1;
		}
	}
	return result;
}

/**
 * Compare a compressed java unicode array to uncompressed java unicode array for equality.
 * @param *vmThread
 * @param *unicodeBytes1 compressed unicode array
 * @param *unicodeBytes2 uncompressed unicode array
 * @param length for both unicode arrays
 * @returns 1 if the two arrays are equal, 0 otherwise
 */
UDATA
compareCompressedUnicodeToUncompressedUnicode(J9VMThread *vmThread, j9object_t unicodeBytes1, j9object_t unicodeBytes2, UDATA length)
{
	UDATA result = 1;
	UDATA i = 0;
	while (0 != length) {
		U_16 unicodeChar1 = J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes1, i);
		U_16 unicodeChar2 = (U_8)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes2, i);
		if (unicodeChar1 != unicodeChar2) {
			result = 0;
			break;
		}
		length -=  1;
		i += 1;
	}
	return result;
}

/**
 * Compare a java string to another java string for character equality.
 * @param *vmThread
 * @param *string1
 * @param *string2
 * @returns 1 if the strings are equal, 0 otherwise
 */
UDATA
compareStrings(J9VMThread *vmThread, j9object_t string1, j9object_t string2)
{
	UDATA result = 1;
	if (string1 != string2) {
		UDATA stringLength1 = J9VMJAVALANGSTRING_LENGTH(vmThread, string1);
		UDATA stringLength2 = J9VMJAVALANGSTRING_LENGTH(vmThread, string2);

		if (stringLength1 != stringLength2) {
			result = 0;
		} else {
			j9object_t unicodeBytes1 = J9VMJAVALANGSTRING_VALUE(vmThread, string1);
			j9object_t unicodeBytes2 = J9VMJAVALANGSTRING_VALUE(vmThread, string2);

			bool isString1Compressed = IS_STRING_COMPRESSED(vmThread, string1);
			bool isString2Compressed = IS_STRING_COMPRESSED(vmThread, string2);
			if (isString1Compressed) {
				if (isString2Compressed) {
					result = compareCompressedUnicode(vmThread, unicodeBytes1, unicodeBytes2, stringLength1);
				} else {
					result = compareCompressedUnicodeToUncompressedUnicode(vmThread, unicodeBytes1, unicodeBytes2, stringLength1);
				}
			} else {
				if (isString2Compressed) {
					result = compareCompressedUnicodeToUncompressedUnicode(vmThread, unicodeBytes2, unicodeBytes1, stringLength1);
				} else {
					result = compareUncompressedUnicode(vmThread, unicodeBytes1, unicodeBytes2, stringLength1);
				}
			}
		}
	}
	return result;
}

/**
 * Compare a java string to a UTF8 string for equality.
 * @param *vm
 * @param *string
 * @param translateDots
 * @param *utfData
 * @param utfLength
 * @returns 1 if the strings are equal, 0 otherwise
 */
UDATA
compareStringToUTF8(J9VMThread *vmThread, j9object_t string, UDATA translateDots, const U_8 * utfData, UDATA utfLength)
{
	const U_8 *tmpUtfData = utfData;
	UDATA tmpUtfLength = utfLength;
	UDATA i = 0;

	UDATA stringLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	UDATA tmpStringLength = stringLength;
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		while ((tmpUtfLength != 0) && (tmpStringLength != 0)) {
			U_16 unicodeChar = (U_8)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i);
			U_16 utfChar = 0;
			UDATA consumed = decodeUTF8Char(tmpUtfData, &utfChar);

			if (translateDots) {
				if ('.' == unicodeChar) {
					unicodeChar = '/';
				}
			}
			if (unicodeChar != utfChar) {
				return 0;
			}
			tmpStringLength--;
			tmpUtfData += consumed;
			tmpUtfLength -= consumed;
			i++;
		}
	} else {
		while ((tmpUtfLength != 0) && (tmpStringLength != 0)) {
			U_16 unicodeChar = J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i);
			U_16 utfChar;
			UDATA ate = decodeUTF8Char(tmpUtfData, &utfChar);
			if (translateDots) {
				if ('.' == unicodeChar) {
					unicodeChar = '/';
				}
			}
			if (unicodeChar != utfChar) {
				return 0;
			}
			tmpStringLength--;
			tmpUtfData += ate;
			tmpUtfLength -= ate;
			i++;
		}
	}
	return ((tmpUtfLength == 0) && (tmpStringLength == 0));
}

UDATA
copyStringToUTF8Helper(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, UDATA stringOffset, UDATA stringLength, U_8 *utf8Data, UDATA utf8DataLength)
{
	Assert_VM_notNull(string);

	j9object_t stringValue = J9VMJAVALANGSTRING_VALUE(vmThread, string);
	U_8 *data = utf8Data;

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		/* Manually version J9_STR_XLAT flag checking from the loop for performance as the compiler does not do it */
		if ((stringFlags & J9_STR_XLAT) == 0) {
			for (UDATA i = stringOffset; i < stringOffset + stringLength; i++) {
				I_8 unicode = J9JAVAARRAYOFBYTE_LOAD(vmThread, stringValue, i);
				UDATA encodedLength = VM_VMHelpers::encodedUTF8LengthI8(unicode);

				/* Stop writing to utf8Data if utf8DataLength will be exceeded. */
				if (encodedLength > (utf8DataLength - (UDATA)(data - utf8Data))) {
					break;
				}

				VM_VMHelpers::encodeUTF8CharI8(unicode, data);
				data += encodedLength;
			}
		} else {
			for (UDATA i = stringOffset; i < stringOffset + stringLength; i++) {
				I_8 unicode = J9JAVAARRAYOFBYTE_LOAD(vmThread, stringValue, i);
				UDATA encodedLength = VM_VMHelpers::encodedUTF8LengthI8(unicode);

				/* Stop writing to utf8Data if utf8DataLength will be exceeded. */
				if (encodedLength > (utf8DataLength - (UDATA)(data - utf8Data))) {
					break;
				}

				VM_VMHelpers::encodeUTF8CharI8(unicode, data);
				if ('.' == *data) {
					*data = '/';
				}
				data += encodedLength;
			}
		}
	} else {
		/* Manually version J9_STR_XLAT flag checking from the loop for performance as the compiler does not do it */
		if ((stringFlags & J9_STR_XLAT) == 0) {
			for (UDATA i = stringOffset; i < stringOffset + stringLength; i++) {
				U_16 unicode = J9JAVAARRAYOFCHAR_LOAD(vmThread, stringValue, i);
				UDATA encodedLength = VM_VMHelpers::encodedUTF8Length(unicode);

				/* Stop writing to utf8Data if utf8DataLength will be exceeded. */
				if (encodedLength > (utf8DataLength - (UDATA)(data - utf8Data))) {
					break;
				}

				VM_VMHelpers::encodeUTF8Char(unicode, data);
				data += encodedLength;
			}
		} else {
			for (UDATA i = stringOffset; i < stringOffset + stringLength; i++) {
				U_16 unicode = J9JAVAARRAYOFCHAR_LOAD(vmThread, stringValue, i);
				UDATA encodedLength = VM_VMHelpers::encodedUTF8Length(unicode);

				/* Stop writing to utf8Data if utf8DataLength will be exceeded. */
				if (encodedLength > (utf8DataLength - (UDATA)(data - utf8Data))) {
					break;
				}

				VM_VMHelpers::encodeUTF8Char(unicode, data);
				if ('.' == *data) {
					*data = '/';
				}
				data += encodedLength;
			}
		}
	}

	UDATA returnLength = (UDATA)(data - utf8Data);

	if ((stringFlags & J9_STR_NULL_TERMINATE_RESULT) != 0) {
		Assert_VM_true(returnLength < utf8DataLength);
		*data = '\0';
	} else {
		Assert_VM_true(returnLength <= utf8DataLength);
	}

	return returnLength;
}

char*
copyStringToUTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength, UDATA *utf8Length)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	U_8 *result = NULL;
	UDATA stringLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	U_64 length = prependStrLength + ((U_64)stringLength * 3);

	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
		++length;
	}

#if UDATA_MAX < (3 * INT32_MAX)
	/* Memory allocation functions take a UDATA as the input parameter for length.
	 * On 32-bit systems, the length needs to be restricted to UDATA_MAX since the
	 * maximum UTF-8 length of a Java string can be 3 * Integer.MAX_VALUE. Otherwise,
	 * there will be overflow. On 64-bit platforms, this is not an issue since UDATA
	 * and U_64 are the same size.
	 */
	if (length > UDATA_MAX) {
		length = UDATA_MAX;
	}
#endif /* UDATA_MAX < (3 * INT32_MAX) */

	PORT_ACCESS_FROM_VMC(vmThread);

	if (length > bufferLength) {
		result = (U_8 *)j9mem_allocate_memory((UDATA)length, OMRMEM_CATEGORY_VM);
	} else {
		result = (U_8 *)buffer;
	}

	if (NULL != result) {
		UDATA computedUtf8Length = 0;

		if (0 < prependStrLength) {
			memcpy(result, prependStr, prependStrLength);
		}

		computedUtf8Length = copyStringToUTF8Helper(
				vmThread, string, stringFlags, 0,
				stringLength, result + prependStrLength,
				(UDATA)(length - prependStrLength));

		if (NULL != utf8Length) {
			*utf8Length = computedUtf8Length + prependStrLength;
		}
	}

	return (char *)result;
}

J9UTF8*
copyStringToJ9UTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	U_8 *result = NULL;
	UDATA stringLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	U_64 length = sizeof(J9UTF8) + prependStrLength + ((U_64)stringLength * 3);

	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
		++length;
	}

#if UDATA_MAX < (3 * INT32_MAX)
	/* Memory allocation functions take a UDATA as the input parameter for length.
	 * On 32-bit systems, the length needs to be restricted to UDATA_MAX since the
	 * maximum UTF-8 length of a Java string can be 3 * Integer.MAX_VALUE. Otherwise,
	 * there will be overflow. On 64-bit platforms, this is not an issue since UDATA
	 * and U_64 are the same size.
	 */
	if (length > UDATA_MAX) {
		length = UDATA_MAX;
	}
#endif /* UDATA_MAX < (3 * INT32_MAX) */

	PORT_ACCESS_FROM_VMC(vmThread);

	if ((prependStrLength > J9UTF8_MAX_LENGTH) || (length > (J9UTF8_MAX_LENGTH - prependStrLength))) {
		result = NULL;
	} else if (length > bufferLength) {
		result = (U_8 *)j9mem_allocate_memory((UDATA)length, OMRMEM_CATEGORY_VM);
	} else {
		result = (U_8 *)buffer;
	}

	if (NULL != result) {
		UDATA computedUtf8Length = 0;

		if (0 < prependStrLength) {
			memcpy(result + sizeof(J9UTF8), prependStr, prependStrLength);
		}

		computedUtf8Length = copyStringToUTF8Helper(
				vmThread, string, stringFlags, 0, stringLength,
				result + sizeof(J9UTF8) + prependStrLength,
				(UDATA)(length - sizeof(J9UTF8) - prependStrLength));

		J9UTF8_SET_LENGTH(result, (U_16)computedUtf8Length + (U_16)prependStrLength);
	}

	return (J9UTF8 *)result;
}

char *
copyJ9UTF8ToUTF8WithMemAlloc(J9VMThread *vmThread, J9UTF8 *string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	U_8 *result = NULL;
	PORT_ACCESS_FROM_VMC(vmThread);

	const U_8 *stringData = J9UTF8_DATA(string);
	const UDATA stringLength = (UDATA)J9UTF8_LENGTH(string);
	UDATA allocationSize = prependStrLength + stringLength;

	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
		allocationSize += 1;
	}

	if (stringLength < bufferLength) {
		result = (U_8 *)buffer;
	} else {
		result = (U_8 *)j9mem_allocate_memory(allocationSize, OMRMEM_CATEGORY_VM);
	}

	if (NULL != result) {
		if (prependStrLength > 0) {
			memcpy(result, prependStr, prependStrLength);
		}
		memcpy(result + prependStrLength, stringData, stringLength);
		if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
			result[allocationSize - 1] = '\0';
		}
	}

	return (char *)result;
}

J9UTF8 *
copyJ9UTF8WithMemAlloc(J9VMThread *vmThread, J9UTF8 *string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	J9UTF8 *result = NULL;
	PORT_ACCESS_FROM_VMC(vmThread);

	const U_8 *stringData = J9UTF8_DATA(string);
	const UDATA stringLength = (UDATA)J9UTF8_LENGTH(string);
	const UDATA totalStringLength = prependStrLength + stringLength;
	UDATA allocationSize = totalStringLength + sizeof(J9UTF8);

	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
		allocationSize += 1;
	}

	if (totalStringLength < J9UTF8_MAX_LENGTH) {
		if (allocationSize <= bufferLength) {
			result = (J9UTF8 *)buffer;
		} else {
			result = (J9UTF8 *)j9mem_allocate_memory(allocationSize, OMRMEM_CATEGORY_VM);
		}

		if (NULL != result) {
			U_8 *resultString = J9UTF8_DATA(result);
			if (prependStrLength > 0) {
				memcpy(resultString, prependStr, prependStrLength);
			}
			memcpy(resultString + prependStrLength, stringData, stringLength);
			if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
				resultString[totalStringLength] = '\0';
			}
			J9UTF8_SET_LENGTH(result, (U_16)totalStringLength);
		}
	}

	return result;
}

UDATA
getStringUTF8Length(J9VMThread *vmThread, j9object_t string)
{
	U_64 utf8Length = 0;
	UDATA unicodeLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		for (UDATA i = 0; i < unicodeLength; i++) {
			UDATA nextLength = VM_VMHelpers::encodedUTF8LengthI8(J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i));
#if UDATA_MAX < (3 * INT32_MAX)
			/* Truncate on a 32-bit platform since the maximum length of a UTF-8 string is 3 * Integer.MAX_VALUE.
			 * This is to accommodate memory allocation functions which take a UDATA as the input parameter for
			 * length. On 64-bit platforms, truncation is not needed since UDATA and U_64 are of the same size.
			 */
			if (utf8Length > (UDATA_MAX - nextLength)) {
				break;
			}
#endif /* UDATA_MAX < (3 * INT32_MAX) */
			utf8Length += nextLength;
		}
	} else {
		for (UDATA i = 0; i < unicodeLength; i++) {
			UDATA nextLength = VM_VMHelpers::encodedUTF8Length(J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i));
#if UDATA_MAX < (3 * INT32_MAX)
			/* Truncate on a 32-bit platform since the maximum length of a UTF-8 string is 3 * Integer.MAX_VALUE.
			 * This is to accommodate memory allocation functions which take a UDATA as the input parameter for
			 * length. On 64-bit platforms, truncation is not needed since UDATA and U_64 are of the same size.
			 */
			if (utf8Length > (UDATA_MAX - nextLength)) {
				break;
			}
#endif /* UDATA_MAX < (3 * INT32_MAX) */
			utf8Length += nextLength;
		}
	}

	return (UDATA)utf8Length;
}

U_64
getStringUTF8LengthTruncated(J9VMThread *vmThread, j9object_t string, U_64 maxLength)
{
	U_64 utf8Length = 0;
	UDATA unicodeLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		for (UDATA i = 0; i < unicodeLength; i++) {
			UDATA nextLength = VM_VMHelpers::encodedUTF8LengthI8(J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i));
			if (utf8Length > (maxLength - nextLength)) {
				break;
			}
			utf8Length += nextLength;
		}
	} else {
		for (UDATA i = 0; i < unicodeLength; i++) {
			UDATA nextLength = VM_VMHelpers::encodedUTF8Length(J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i));
			if (utf8Length > (maxLength - nextLength)) {
				break;
			}
			utf8Length += nextLength;
		}
	}

	return utf8Length;
}

UDATA 
verifyQualifiedName(J9VMThread *vmThread, U_8 *className, UDATA classNameLength, UDATA allowedBitsForClassName)
{
	UDATA result = CLASSNAME_INVALID;
	UDATA remainingLength = classNameLength;
	BOOLEAN separator = FALSE;
	UDATA arity = 0;
	UDATA i = 0;

	/* strip leading ['s for array classes */
	for (i = 0; i < classNameLength; i++) {
		if ('[' == className[i]) {
			arity += 1;
		} else {
			break;
		}
	}
	remainingLength -= arity;

	if (0 == remainingLength) {
		/* Name must be more than just [s.  Also catches empty string case */
		return CLASSNAME_INVALID;
	}

	/* check invalid characters in the class name */
	for (; i < classNameLength; i++) {
		/* check for illegal characters:  46(.) 47(/) 59(;) 60(<) 70(>) 91([) */
		switch (className[i]) {
		case '.':
			if (separator) {
				return CLASSNAME_INVALID;
			}
			/* convert the characters from '.' to '/' in the case of J9_STR_XLAT for later use in the caller */
			className[i] = '/';
			separator = TRUE;
			break;
		case '/':
			return CLASSNAME_INVALID;
		case ';':
			/* Valid at the end of array classes */
			if (arity && (1 == remainingLength)) {
				break;
			}
			return CLASSNAME_INVALID;
		case '<': /* Fall through */
		case '>':
			separator = FALSE; /* allow /<>/ as a pattern, per test suites */
			break;
		case '[':
			return CLASSNAME_INVALID;
		default:
			/* Do Nothing */
			separator = FALSE;
		}

		remainingLength -= 1;
	}

	/* the separator shouldn't exist at the end of name, regardless of class and method */
	if (separator) {
		return CLASSNAME_INVALID;
	}

	/* check the arity of the class name to see whether it is an array or not
	 * and check whether the results are the allowed values by the caller.
	 */
	result = (0 == arity) ? CLASSNAME_VALID_NON_ARRARY : CLASSNAME_VALID_ARRARY;
	if (J9_ARE_ANY_BITS_SET(result, allowedBitsForClassName)) {
		return result;
	}

	return CLASSNAME_INVALID;
}

/**
 * Check that each UTF8 character is well-formed.
 * @param utf8Data pointer to a null-terminated sequence of bytes
 * @param length number of bytes in the string, not including the terminating '\0'
 * @return non-zero if utf8Data points to valid UTF8, 0 otherwise
 */
UDATA
isValidUtf8(const U_8 * utf8Data, size_t length)
{
	while (length > 0) {
		U_16 dummy;
		U_32 consumed =  decodeUTF8CharN(utf8Data, &dummy, length);
		if (0 == consumed) { /* 0 indicates parsing error */
			return 0;
		}
		utf8Data += consumed;
		length -= consumed;
	}
	return 1;
}

/**
 *  copies original to corrected, replacing bad characters with '?'.  Also add a terminating null byte.
 *
 * @param original pointer to a null-terminated sequence of bytes.
 * @param corrected pointer to a buffer to receive the corrected string.
 */
void
fixBadUtf8(const U_8 * original, U_8 *corrected, size_t length)
{
	strcpy((char*)corrected, (const char*)original);
	while (length > 0) {
		U_16 dummy;
		U_32 consumed =  decodeUTF8CharN(corrected, &dummy, length);
		if (0 == consumed) { /* 0 indicates parsing error */
			*corrected = '?';
			/*
			 * next time through the loop, decodeUTF8CharN() will recognize
			 * this as a good character and we will advance the pointer
			 */
		}
		corrected += consumed;
		length -= consumed;
	}
	*corrected = '\0';
}

j9object_t
catUtfToString4(J9VMThread * vmThread, const U_8 *data1, UDATA length1, const U_8 *data2, UDATA length2, const U_8 *data3, UDATA length3, const U_8 *data4, UDATA length4)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	j9object_t result = NULL;
	UDATA totalLength = length1 + length2 + length3 + length4;
	U_8 *buffer = (U_8*)j9mem_allocate_memory(totalLength, OMRMEM_CATEGORY_VM);
	if (NULL == buffer) {
		vmThread->javaVM->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
	} else {
		U_8 *ptr = buffer;
		memcpy(ptr, data1, length1);
		ptr += length1;
		memcpy(ptr, data2, length2);
		ptr += length2;
		memcpy(ptr, data3, length3);
		ptr += length3;
		memcpy(ptr, data4, length4);
		result = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, buffer, totalLength, 0);
		j9mem_free_memory(buffer);
	}
	return result;
}

j9object_t
methodToString(J9VMThread * vmThread, J9Method* method)
{
	J9Class *clazz = J9_CLASS_FROM_METHOD(method);
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(clazz->romClass);
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
	J9UTF8 *sig = J9ROMMETHOD_SIGNATURE(romMethod);
	return catUtfToString4(vmThread,
			J9UTF8_DATA(className), J9UTF8_LENGTH(className),
			(U_8*)".", 1,
			J9UTF8_DATA(name), J9UTF8_LENGTH(name),
			J9UTF8_DATA(sig), J9UTF8_LENGTH(sig));
}

} /* extern "C" */
