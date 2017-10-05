/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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

#if !defined (J9VM_OUT_OF_PROCESS)

/**
 * Copy certain number of Unicode characters from an offset into a Unicode character array to a UTF8 data buffer.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] compressed if the Unicode character array is compressed
 * @param[in] nullTermination if the utf8 data is going to be NULL terminated
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] unicodeBytes a Unicode character array
 * @param[in] unicodeOffset an offset into the Unicode character array
 * @param[in] unicodeLength the number of Unicode characters to be copied
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8Length the size of the utf8 data buffer
 *
 * @return UDATA_MAX if a failure occurred, otherwise the number of utf8 data copied excluding null termination
 */
UDATA
copyCharsIntoUTF8Helper(
	J9VMThread *vmThread, BOOLEAN compressed, BOOLEAN nullTermination, UDATA stringFlags, j9object_t unicodeBytes, UDATA unicodeOffset, UDATA unicodeLength, U_8 *utf8Data, UDATA utf8Length)
{
	UDATA remaining = utf8Length;
	U_8 *data = utf8Data;
	UDATA result = 0;
	UDATA i = 0;

	if (compressed) {
		/* following implementation is expected to be different from non-compressed case when String compressing is enabled */
		for (i = unicodeOffset; i < unicodeOffset + unicodeLength; i++) {
			result = VM_VMHelpers::encodeUTF8CharN(J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i), data, (U_32)remaining);
			if (0 == result) {
				return UDATA_MAX;
			} else {
				if ((stringFlags & J9_STR_XLAT) && ('.' == *data)) {
					*data = '/';
				}
				remaining -= result;
				data += result;
			}
		}
	} else {
		for (i = unicodeOffset; i < (unicodeOffset + unicodeLength); i++) {
			result = VM_VMHelpers::encodeUTF8CharN(J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i), data, (U_32)remaining);
			if (0 == result) {
				return UDATA_MAX;
			} else {
				if ((stringFlags & J9_STR_XLAT) && ('.' == *data)) {
					*data = '/';
				}
				remaining -= result;
				data += result;
			}
		}
	}
	if (nullTermination && (0 == remaining)) {
		return UDATA_MAX;
	}
	if (nullTermination) {
		*data = '\0';
	}

	return data - utf8Data;
}

/**
 * Copy a Unicode String to a UTF8 data buffer with NULL termination.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] nullTermination if the utf8 data is going to be NULL terminated
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8Length the size of the utf8 data buffer
 *
 * @return UDATA_MAX if a failure occurred, otherwise the number of utf8 data copied excluding null termination
 */
UDATA
copyStringToUTF8Helper(J9VMThread *vmThread, j9object_t string, BOOLEAN nullTermination, UDATA stringFlags, U_8 *utf8Data, UDATA utf8Length)
{
	Assert_VM_notNull(string);

	UDATA unicodeLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);

	return copyCharsIntoUTF8Helper(vmThread, IS_STRING_COMPRESSED(vmThread, string), nullTermination, stringFlags, unicodeBytes, 0, unicodeLength, utf8Data, utf8Length);
}


/**
 * !!! this method is for backwards compatibility with JIT usage !!!
 * Copy a Unicode String to a UTF8 data buffer with NULL termination.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8Length the size of the utf8 data buffer
 *
 * @return 1 if a failure occurred, otherwise 0 for success
 */
UDATA
copyStringToUTF8(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, U_8 *utf8Data, UDATA utf8Length)
{
	if (UDATA_MAX == copyStringToUTF8Helper(vmThread, string, TRUE, stringFlags, utf8Data, utf8Length)) {
		return 1;
	} else {
		return 0;
	}
}


/**
 * !!! this method is for backwards compatibility with JIT usage !!!
 * Copy a Unicode String to a UTF8 data buffer without NULL termination.
 * dest is assumed to have enough length - the easiest way to ensure this is to pass a buffer with 1.5 * string length in it
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] dest a utf8 data buffer
 *
 * @return UDATA_MAX if a failure occurred, otherwise the number of utf8 data copied excluding null termination
 */
UDATA
copyFromStringIntoUTF8(J9VMThread * vmThread, j9object_t string, char * dest)
{
	return copyStringToUTF8Helper(vmThread, string, FALSE, J9_STR_NONE, (U_8*)dest, UDATA_MAX);
}


/**
 * Copy a string object to a UTF8 data buffer, optionally to prepend a string before it
 *
 * ***The caller must free the memory from this pointer if the return value is NOT the buffer argument ***
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] prependStr the string to be prepended before the string object to be copied
 * 				it can't be NULL but can be an empty string ""
 * @param[in] buffer the buffer for the string
 * @param[in] bufferLength the buffer length, not expected to larger than 64K
 *
 * @return a char pointer to the string (with NULL termination)
 */
char*
copyStringToUTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, char *buffer, UDATA bufferLength)
{
	Assert_VM_notNull(prependStr);
	Assert_VM_notNull(string);

	char *strUTF = NULL;
	UDATA stringLen = getStringUTF8Length(vmThread, string);
	UDATA prependStrLen = strlen(prependStr);
	UDATA length = stringLen + prependStrLen + 1;

	PORT_ACCESS_FROM_VMC(vmThread);

	if (length > bufferLength) {
		strUTF = (char *)j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);
	} else {
		strUTF = buffer;
		length = bufferLength;
	}
	if (NULL != strUTF) {
		if (0 < prependStrLen) {
			memcpy(strUTF, prependStr, prependStrLen);
		}
		if (UDATA_MAX == copyStringToUTF8Helper(
			vmThread, string, TRUE, stringFlags, (U_8 *)(strUTF + prependStrLen), length - prependStrLen)
		) {
			if (buffer != strUTF) {
				j9mem_free_memory(strUTF);
			}
			return NULL;
		}
	}
	return strUTF;
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
			utf8Length += VM_VMHelpers::encodedUTF8Length(J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i));
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
		for (i = 1; i < unicodeLength; i++) {
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
#endif

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

}
