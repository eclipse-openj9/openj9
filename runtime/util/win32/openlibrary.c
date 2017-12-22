/*******************************************************************************
 * Copyright (c) 2011, 2014 IBM Corp. and others
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

/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#include "j9protos.h"
#include "ut_j9util.h"
static I_32
util_convertToUTF8(const wchar_t* unicodeString, char* utf8Buffer, UDATA size);

/**
 * @internal
 * Converts the UTF8 encoded string to Unicode. Use the provided buffer if possible
 * or allocate memory as necessary. Return the new string.
 *
 * @param[in] string The UTF8 encoded string
 * @param[in] unicodeBuffer The unicode buffer to attempt to use
 * @param[in] unicodeBufferSize The number of bytes available in the unicode buffer
 *
 * @return	the pointer to the Unicode string, or NULL on failure.
 */
static wchar_t*
util_convertFromUTF8(const char* string, wchar_t* unicodeBuffer, UDATA unicodeBufferSize)
{
	wchar_t* unicodeString = NULL;
	UDATA length = 0;

	if (NULL == string) {
		return NULL;
	}
	length = (UDATA)strlen(string);
	if (length < unicodeBufferSize) {
		unicodeString = unicodeBuffer;
	} else {
		unicodeString = (wchar_t*) malloc ((length + 1) * 2);
		if (NULL == unicodeString) {
			return NULL;
		}
	}

	if (0 == MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, string, -1, unicodeString, (int)length + 1)) {
		if (unicodeString != unicodeBuffer) {
			free(unicodeString);
		}
		return NULL;
	}

	return unicodeString;
}

/**
 * @internal
 * Converts the Unicode string to UTF8 encoded data in the provided buffer.
 *
 * @param[in] unicodeString The unicode buffer to convert
 * @param[in] utf8Buffer The buffer to store the UTF8 encoded bytes into
 * @param[in] size The size of utf8Buffer
 *
 * @return 0 on success, -1 on failure.
 */
static I_32
util_convertToUTF8(const wchar_t* unicodeString, char* utf8Buffer, UDATA size)
{
	if (0 == WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeString, -1, utf8Buffer, (int)size, NULL, NULL)) {
		return -1;
	}
	return 0;
}

/* Error codes used by checkLibraryType() for incompatible library architecture */
enum {
	LIB_UNSUPPORTED_MAGIC = 1,
	LIB_UNSUPPORTED_SIGNATURE,
	LIB_UNSUPPORTED_MACHINE
};

/* Reads raw data from library file and verify the library architecture.
 * Library files on Windows follow PE (Portable Executable) format.
 * To verify library architecture, we check three fields:
 * 1. IMAGE_DOS_HEADER.e_magic
 * 2. Signature
 * 3. IMAGE_FILE_HEADER.machine
 *
 * @return TRUE on success, FALSE on failure.
*/
static BOOLEAN
checkLibraryType(wchar_t *unicodeLibPath) {
	HANDLE libHandle = 0;
	USHORT magic = 0;
	IMAGE_DOS_HEADER imageDosHeader;
	IMAGE_FILE_HEADER *imageFileHeader = NULL;
 	DWORD signature = 0;
 	DWORD error = 0;
 	DWORD bytesRead = 0;
 	WORD machine = 0;
 	/* For Windows NT, signature is 4 bytes */
 	BYTE signatureAndFileHeader[sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)];
 	BOOLEAN rc = FALSE;

 	Trc_Util_checkLibraryType_Enter();
	libHandle = CreateFileW(unicodeLibPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == libHandle) {
		error = GetLastError();
		Trc_Util_checkLibraryType_FailedCreateFile(unicodeLibPath, error);
		goto _end;
	}

	/* Read IMAGE_DOS_HEADER to get e.magic */
	rc = ReadFile(libHandle, (void *)&imageDosHeader, (DWORD) sizeof(IMAGE_DOS_HEADER), &bytesRead, NULL);
	if ((0 == rc) || (sizeof(IMAGE_DOS_HEADER) != bytesRead)) {
		error = GetLastError();
		Trc_Util_checkLibraryType_FailedRead(unicodeLibPath, error);
 	 	goto _end;
 	}

 	magic = imageDosHeader.e_magic;
 	if (IMAGE_DOS_SIGNATURE != magic) {
 		Trc_Util_checkLibraryType_FailedUnsupported(LIB_UNSUPPORTED_MAGIC);
 		goto _end;
	}

 	/* IMAGE_DOS_HEADER.e_lfanew gives the offset to signature field from top of the file */
	if (INVALID_SET_FILE_POINTER == SetFilePointer(libHandle, imageDosHeader.e_lfanew, NULL, FILE_BEGIN)) {
		error = GetLastError();
		if (NO_ERROR != error) {
			Trc_Util_checkLibraryType_FailedSetFP(unicodeLibPath, error);
			goto _end;
		}
	}

	rc = ReadFile(libHandle, (void *)signatureAndFileHeader, sizeof(signatureAndFileHeader), &bytesRead, NULL);
	if ((0 == rc) || (sizeof(signatureAndFileHeader) != bytesRead)) {
		error = GetLastError();
		Trc_Util_checkLibraryType_FailedRead(unicodeLibPath, error);
		goto _end;
	}

	signature = *(DWORD *)signatureAndFileHeader;
	if (IMAGE_NT_SIGNATURE != signature) {
		Trc_Util_checkLibraryType_FailedUnsupported(LIB_UNSUPPORTED_SIGNATURE);
		goto _end;
	}

	imageFileHeader = (IMAGE_FILE_HEADER *)(signatureAndFileHeader + sizeof(DWORD));
	machine = imageFileHeader->Machine;
#if defined(WIN64)
	if (IMAGE_FILE_MACHINE_AMD64 != machine) {
#else
	if (IMAGE_FILE_MACHINE_I386 != machine) {
#endif
		Trc_Util_checkLibraryType_FailedUnsupported(LIB_UNSUPPORTED_MACHINE);
		goto _end;
	}
	/* Successfully verified that library is for correct architecture */
	rc = TRUE;

_end:
	if (INVALID_HANDLE_VALUE != libHandle) {
		CloseHandle(libHandle);
	}
	Trc_Util_checkLibraryType_Exit(rc);
	return rc;
}

/**
 * Opens the given library. Lookup in System directory to load the library.
 * If not found, look up in Windows directory.
 *
 * @param[in] name Null-terminated string containing the shared library.
 * @param[in] descriptor Pointer to memory which is filled in with shared-library handle on success.
 * @param[in] flags bitfield comprised of J9PORT_SLOPEN_* constants.
 *
 * @return 0 on success, any other value on failure.
 */
UDATA
j9util_open_system_library(char *name, UDATA *descriptor, UDATA flags)
{
		UDATA result = 1;

		wchar_t libAbsPath[EsMaxPath];
		HINSTANCE dllHandle = NULL;
		UDATA decorate = flags & J9PORT_SLOPEN_DECORATE;
		U_32 error = 0;
		UINT prevMode = 0;
		int i = 0;

		Trc_Util_sl_open_system_library_Entry(name, flags);
		prevMode = SetErrorMode( SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS );

		if (NULL == descriptor) {
			Trc_Util_sl_open_system_library_Null();
			result = -1;
			goto _end;
		}

		for (i = 0; i < 2; i++) {
			wchar_t searchDir[EsMaxPath];
			wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE];
			wchar_t *unicodeName = NULL;
			char appendLibName[EsMaxPath];

			/* Search for dll in System directory and on failure, in Windows directory. */
			if (0 == i) {
				UINT rc = 0;
				BOOL isWow64 = FALSE;
				memset(searchDir, 0, EsMaxPath);
				IsWow64Process(GetCurrentProcess(), &isWow64);
				if (TRUE == isWow64) {
					rc = GetSystemWow64DirectoryW(searchDir, EsMaxPath);
				} else {
					rc = GetSystemDirectoryW(searchDir, EsMaxPath);
				}
				if (0 == rc) {
					/* Failed to get System directory. Try to get Windows directory. */
					error = GetLastError();
					Trc_Util_sl_open_system_library_FailedGetSystemDirectory(error);
					continue;
				}
			} else {
				memset(searchDir, 0, EsMaxPath);
				if (0 == GetWindowsDirectoryW(searchDir, EsMaxPath)) {
					error = GetLastError();
					Trc_Util_sl_open_system_library_FailedGetWindowsDirectory(error);
					goto _error;
				}
			}

			/* appendLibName contains "\\" + libName */
			memset(appendLibName, 0, EsMaxPath);
			appendLibName[0] = '\\';
			/* subtract 6 to leave room for the starting backslash, ".dll" and the trailing NULL */
			memcpy(&appendLibName[1], name, OMR_MIN(strlen(name) + 1, EsMaxPath - 6));
			if (decorate) {
				strcat(appendLibName, ".dll");
			}

			unicodeName = util_convertFromUTF8(appendLibName, unicodeBuffer, UNICODE_BUFFER_SIZE);
			if (NULL == unicodeName) {
				Trc_Util_sl_open_system_library_FailedUnicodeConversion();
				result = -1;
				goto _end;
			}

			memset(libAbsPath, 0, EsMaxPath);
			_snwprintf(libAbsPath, EsMaxPath, L"%s%s", searchDir, unicodeName);
			libAbsPath[EsMaxPath-1] = '\0';
			dllHandle = LoadLibraryExW(libAbsPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
			if (NULL == dllHandle) {
				char unicodeLibPath[EsMaxPath];

				error = GetLastError();
				util_convertToUTF8(libAbsPath, unicodeLibPath, EsMaxPath);
				Trc_Util_sl_open_system_library_FailedLoadLibrary(unicodeLibPath, error);

				if (0 == i) {
					/* Failed to load library from System directory. Try to load from Windows directory. */
					continue;
				} else {
					goto _error;
				}
			}
			if (unicodeName != unicodeBuffer) {
				free(unicodeName);
			}
			break;
		}

		if (FALSE == checkLibraryType(libAbsPath)) {
			result = -1;
			goto _end;
		}

		*descriptor = (UDATA)dllHandle;
		result = 0;

		_error:
		if (0 != result) {
			result = error;
		}

		_end:
		SetErrorMode(prevMode);


		Trc_Util_sl_open_system_library_Exit(dllHandle);
		return result;

}

