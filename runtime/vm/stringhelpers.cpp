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
#include "objhelp.h"
#include "j9cp.h"
#include "vm_internal.h"
#include "ut_j9vm.h"

#include "VMHelpers.hpp"

extern "C" {

/**
 * Compress the UTF8 string into compressed format into byteArray at offset
 * @param *vmThread
 * @param *data
 * @param length
 * @param stringFlags
 * @param byteArray
 * @param offset
 */
void
copyUTF8ToCompressedUnicode(J9VMThread *vmThread, U_8 *data, UDATA length, UDATA stringFlags, j9object_t charArray, UDATA startIndex)
{
	UDATA writeIndex = startIndex;
	UDATA originalLength = length;
	while (length > 0) {
		U_16 unicode = 0;
		UDATA consumed = VM_VMHelpers::decodeUTF8Char(data, &unicode);
		if (J9_ARE_ANY_BITS_SET(stringFlags, J9_STR_XLAT)) {
			if ((U_16)'/' == unicode) {
				unicode = (U_16)'.';
			}
		}
		J9JAVAARRAYOFBYTE_STORE(vmThread, charArray, writeIndex, (U_8)unicode);
		writeIndex += 1;
		data += consumed;
		length -= consumed;
	}

	/* anonClasses have the following name format [className]/[ROMADDRESS], we have to
	 * to fix up the name because the previous functions automatically convert '/' to '.'
	 */
	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_ANON_CLASS_NAME)) {
		for (IDATA i = (IDATA) originalLength - 1; i >= 0; i--) {
			if ((U_8)'.' == J9JAVAARRAYOFBYTE_LOAD(vmThread, charArray, i)) {
				J9JAVAARRAYOFBYTE_STORE(vmThread, charArray, i, (U_8) '/');
				break;
			}
		}
	}
}

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
			U_16 unicodeChar1 = (U_16)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes1, i);
			U_16 unicodeChar2 = (U_16)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes2, i);
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
		U_16 unicodeChar2 = (U_16)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes2, i);
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
			U_16 unicodeChar = (U_16)J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i);
			U_16 utfChar;
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

#if !defined(J9VM_OUT_OF_PROCESS)
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
				data += VM_VMHelpers::encodeUTF8CharI8(J9JAVAARRAYOFBYTE_LOAD(vmThread, stringValue, i), data);
			}
		} else {
			for (UDATA i = stringOffset; i < stringOffset + stringLength; i++) {
				UDATA encodedLength = VM_VMHelpers::encodeUTF8CharI8(J9JAVAARRAYOFBYTE_LOAD(vmThread, stringValue, i), data);

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
				data += VM_VMHelpers::encodeUTF8Char(J9JAVAARRAYOFCHAR_LOAD(vmThread, stringValue, i), data);
			}
		} else {
			for (UDATA i = stringOffset; i < stringOffset + stringLength; i++) {
				UDATA encodedLength = VM_VMHelpers::encodeUTF8Char(J9JAVAARRAYOFCHAR_LOAD(vmThread, stringValue, i), data);

				if ('.' == *data) {
					*data = '/';
				}

				data += encodedLength;
			}
		}
	}

	UDATA returnLength = (UDATA)(data - utf8Data);

	if ((stringFlags & J9_STR_NULL_TERMINATE_RESULT) != 0) {
		*data = '\0';

		Assert_VM_true(utf8DataLength >= returnLength + 1);
	} else {
		Assert_VM_true(utf8DataLength >= returnLength);
	}

	return returnLength;
}

char*
copyStringToUTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength, UDATA *utf8Length)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	U_8* result = NULL;
	UDATA stringLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	UDATA length = prependStrLength + (stringLength * 3);

	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
		++length;
	}

	PORT_ACCESS_FROM_VMC(vmThread);

	if (length > bufferLength) {
		result = (U_8*)j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);
	} else {
		result = (U_8*)buffer;
	}

	if (NULL != result) {
		UDATA computedUtf8Length = 0;

		if (0 < prependStrLength) {
			memcpy(result, prependStr, prependStrLength);
		}

		computedUtf8Length = copyStringToUTF8Helper(vmThread, string, stringFlags, 0, stringLength, result + prependStrLength, length - prependStrLength);

		if (NULL != utf8Length) {
			*utf8Length = computedUtf8Length + prependStrLength;
		}
	}

	return (char*)result;
}

J9UTF8*
copyStringToJ9UTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	U_8* result = NULL;
	UDATA stringLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	UDATA length = sizeof(((J9UTF8*)0)->length) + prependStrLength + (stringLength * 3);

	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_NULL_TERMINATE_RESULT)) {
		++length;
	}

	PORT_ACCESS_FROM_VMC(vmThread);

	if (length > bufferLength) {
		result = (U_8*)j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);
	} else {
		result = (U_8*)buffer;
	}

	if (NULL != result) {
		UDATA computedUtf8Length = 0;

		if (0 < prependStrLength) {
			memcpy(result + sizeof(((J9UTF8*)0)->length), prependStr, prependStrLength);
		}

		computedUtf8Length = copyStringToUTF8Helper(vmThread, string, stringFlags, 0, stringLength, result + sizeof(((J9UTF8*)0)->length) + prependStrLength, length - sizeof(((J9UTF8*)0)->length) - prependStrLength);

		J9UTF8_SET_LENGTH(result, (U_16)computedUtf8Length + (U_16)prependStrLength);
	}

	return (J9UTF8*)result;
}

IDATA
getStringUTF8Length(J9VMThread *vmThread, j9object_t string)
{
	IDATA utf8Length = 0;
	UDATA unicodeLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);
	UDATA i;

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		for (i = 0; i < unicodeLength; i++) {
			utf8Length += VM_VMHelpers::encodedUTF8LengthI8(J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i));
		}
	} else {
		for (i = 0; i < unicodeLength; i++) {
			utf8Length += VM_VMHelpers::encodedUTF8Length(J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i));
		}
	}

	return utf8Length;
}

UDATA 
verifyQualifiedName(J9VMThread *vmThread, j9object_t string)
{
	UDATA unicodeLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);
	UDATA i = 0;
	UDATA rc = CLASSNAME_VALID_NON_ARRARY;

	if (unicodeLength == 0) {
		return CLASSNAME_INVALID;
	}

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		if ('[' == J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, 0)) {
			rc = CLASSNAME_VALID_ARRARY;
		}
		for (i = 0; i < unicodeLength; i++) {
			U_8 c = J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i);
			if (c == '/') {
				rc = CLASSNAME_INVALID;
				break;
			}
		}
	} else {
		if ('[' == J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, 0)) {
			rc = CLASSNAME_VALID_ARRARY;
		}
		for (i = 0; i < unicodeLength; i++) {
			U_16 c = J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i);
			if (c == '/') {
				rc = CLASSNAME_INVALID;
				break;
			}
		}
	}
	return rc;
}
#endif /* !defined(J9VM_OUT_OF_PROCESS) */

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

void
copyUTF8ToUnicode(J9VMThread * vmThread, U_8 * data, UDATA length, UDATA stringFlags, j9object_t charArray, UDATA startIndex)
{
	UDATA writeIndex = startIndex;
	UDATA originalLength = length;
	while (length > 0) {
		U_16 unicode = 0;
		UDATA consumed = VM_VMHelpers::decodeUTF8Char(data, &unicode);
		if (J9_ARE_ANY_BITS_SET(stringFlags, J9_STR_XLAT)) {
			if ((U_16)'/' == unicode) {
				unicode = (U_16)'.';
			}
		}
		J9JAVAARRAYOFCHAR_STORE(vmThread, charArray, writeIndex, unicode);
		writeIndex += 1;
		data += consumed;
		length -= consumed;
	}

	/* anonClasses have the following name format [className]/[ROMADDRESS], we have to
	 * to fix up the name because the previous functions automatically convert '/' to '.'
	 */
	if (J9_ARE_ALL_BITS_SET(stringFlags, J9_STR_ANON_CLASS_NAME)) {
		for (IDATA i = (IDATA) originalLength - 1; i >= 0; i--) {
			if ((U_16)'.' == J9JAVAARRAYOFCHAR_LOAD(vmThread, charArray, i)) {
				J9JAVAARRAYOFCHAR_STORE(vmThread, charArray, i, (U_16) '/');
				break;
			}
		}
	}
}

j9object_t
catUtfToString4(J9VMThread * vmThread, const U_8 *data1, UDATA length1, const U_8 *data2, UDATA length2, const U_8 *data3, UDATA length3, const U_8 *data4, UDATA length4)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	j9object_t result = NULL;
	UDATA totalLength = length1 + length2 + length3 + length4;
	U_8 *buffer = (U_8*)j9mem_allocate_memory(totalLength, OMRMEM_CATEGORY_VM);
	if (NULL != buffer) {
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
