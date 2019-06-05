/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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
/* Undefine the winsockapi because winsock2 defines it.  Removes warnings. */
#if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <stdlib.h>
#include <LMCONS.H>
#include <direct.h>
#include "jcl.h"
#include "jclprots.h"
#include "jclglob.h"
#include "util_api.h"


#include "portsock.h"

#include <WinSDKVer.h>

#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
#include <VersionHelpers.h>
#endif

/* JCL_J2SE */
#define JCL_J2SE


char* getPlatformFileEncoding(JNIEnv *env, char *codepage, int size, int encodingType);
I_32 convertToUTF8(J9PortLibrary* portLibrary, const wchar_t* unicodeString, char* utf8Buffer, UDATA size);
char * getTmpDir(JNIEnv *env, char **tempdir);
jobject getPlatformPropertyList(JNIEnv *env, const char *strings[], int propIndex);
void mapLibraryToPlatformName(const char *inPath, char *outPath);


jobject getPlatformPropertyList(JNIEnv *env, const char *strings[], int propIndex) {
	PORT_ACCESS_FROM_ENV(env);
#if !defined(_WIN32_WINNT_WINBLUE) || (_WIN32_WINNT_MAXVER < _WIN32_WINNT_WINBLUE)
	OSVERSIONINFO versionInfo;
#endif /* !defined(_WIN32_WINNT_WINBLUE) || (_WIN32_WINNT_MAXVER < _WIN32_WINNT_WINBLUE) */
	I_32 envSize;
	char *envSpace = NULL, *tempdir = NULL;
	jobject result;
	char userhome[EsMaxPath];
	wchar_t unicodeTemp[EsMaxPath];
	int i;
	char userdir[EsMaxPath];
	wchar_t unicodeHome[EsMaxPath];
	HANDLE process, token;
	UDATA handle;
	BOOL (WINAPI *func)(HANDLE hToken, LPWSTR lpProfileDir, LPDWORD lpcchSize);
#if !defined(JCL_J2SE)
	UINT codePage;
	char codePageBuf[32];
	CPINFO cpInfo;
#endif

	/* Hard coded file/path separators and other values */

	strings[propIndex++] = "file.separator";
	strings[propIndex++] = "\\";

	strings[propIndex++] = "line.separator";
	strings[propIndex++] = "\r\n";

	/* Get the Temp Dir name */
	strings[propIndex++] = "java.io.tmpdir";
	strings[propIndex++] = getTmpDir(env, &tempdir);

	strings[propIndex++] = "user.home";
	i = propIndex;
	envSize = (I_32)j9sysinfo_get_env("USERPROFILE", NULL, 0);
	if (-1 != envSize) {
			envSpace = jclmem_allocate_memory(env, envSize); /* trailing null taken into account */
			if (NULL == envSpace) {
				strings[propIndex++] = "\\";
			} else {
				j9sysinfo_get_env("USERPROFILE", envSpace, envSize);
				strings[propIndex++] = envSpace;
			}
	}
#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
	/* dwPlatformId, VER_PLATFORM_WIN32_NT = https://msdn.microsoft.com/en-us/library/windows/desktop/ms724834(v=vs.85).aspx */
	if ((i == propIndex) && IsWindowsVersionOrGreater( 5, 0, 0))
#else /* defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE) */
	versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if ((i == propIndex) && GetVersionEx(&versionInfo) && (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT))
#endif /* defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE) */
	{
		process = GetCurrentProcess();
		if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
			envSize = 0;
			if (j9util_open_system_library("userenv", &handle, TRUE) == 0) {
				if (i == propIndex) {
					if (j9sl_lookup_name(handle, "GetUserProfileDirectoryW", (UDATA *)&func, "ZPLP") == 0) {
						envSize = EsMaxPath;
						if (func(token, unicodeHome, &envSize)) {
							/* When the SystemDrive environment variable isn't set, such as when j9 is exec'ed
							 * running JCK tests, we get %SystemDrive%/Documents and Settings/...
							 */
							if (!wcsncmp(unicodeHome, L"%SystemDrive%", 13)) {
								/* Borrow userdir variable, which is used for real below */
								if (GetSystemDirectoryW(unicodeTemp, EsMaxPath) > 1) {
									unicodeHome[0] = unicodeTemp[0];
									unicodeHome[1] = unicodeTemp[1];
									wcsncpy(&unicodeHome[2], &unicodeHome[13], envSize - 13);
								}
							}
							convertToUTF8(PORTLIB, unicodeHome, userhome, EsMaxPath);
							strings[propIndex++] = userhome;
						}
					}
				}
			}
		}
	}

	if (i == propIndex) {
		/* Fallback to Windows Directory */
		envSize = (I_32)j9sysinfo_get_env("WINDIR", NULL, 0);
		if (-1 == envSize) {
			strings[propIndex++] = "\\";
		} else {
			envSpace = jclmem_allocate_memory(env,envSize);	/* trailing null taken into account */
			if(!envSpace) {
				strings[propIndex++] = "\\";
			} else {
				j9sysinfo_get_env("WINDIR", envSpace, envSize);
				strings[propIndex++] = envSpace;
			}
		}
	}

	/* Get the directory where the executable was started */
	strings[propIndex++] = "user.dir";
	if (GetCurrentDirectoryW(EsMaxPath, unicodeTemp) == 0) {
		strings[propIndex++] = "\\";
	} else {
		convertToUTF8(PORTLIB, unicodeTemp, userdir, EsMaxPath);
		strings[propIndex++] = userdir;
	}

	if (JAVA_SPEC_VERSION < J2SE_V12) {
		/* Get the timezone */
		strings[propIndex++] = "user.timezone";
		strings[propIndex++] = "";
	}

	/* Jazz 52075 JCL_J2SE is always true */

	result = createSystemPropertyList(env, strings, propIndex);
	if (tempdir) jclmem_free_memory(env,tempdir);
	if (envSpace) jclmem_free_memory(env,envSpace);
	return result;
}


char* getPlatformFileEncoding(JNIEnv *env, char *codepage, int size, int encodingType) {
	PORT_ACCESS_FROM_ENV(env);
	LCID threadLocale;
	CPINFO cpInfo;
	int cp;
#ifdef UNICODE
	int i;
#endif
	int length;

	/* Called with codepage == NULL to initialize the locale */
	if (!codepage) return NULL;

    if (encodingType == 2) {
    	/* file.encoding */
    	threadLocale = GetUserDefaultLCID();
    } else {
    	threadLocale = GetSystemDefaultLCID();
    }
	length = GetLocaleInfo(threadLocale, LOCALE_IDEFAULTANSICODEPAGE, (LPTSTR)&codepage[2], size - 2);

#ifdef UNICODE
	// convert double byte to single byte
	for (i=0; i < length; i++) {
		codepage[i+2] = (char)((short *)&codepage[2])[i];
	}
	codepage[length+2]='\0';
#endif

	/*[PR CMVC 84620] file.encoding is incorrect for foreign locales */ 
	if (length == 2 && codepage[2] == '0') {
		/* no ANSI code page, return UTF8 if its supported, otherwise fall through to return ISO8859_1 */
		if (IsValidCodePage(65001)) {
			return "UTF8";
		}
	}

	if (length == 0 || (cp = atoi(&codepage[2])) == 0)
		return "ISO8859_1";

	/*[PR 94901] Get info about the current code page, not the system code page (CP_ACP) */
	if (GetCPInfo(cp, &cpInfo) && cpInfo.MaxCharSize > 1) {
		if (cp == 936) {
			J9JavaVM* vm = ((J9VMThread *)env)->javaVM;
			if (J2SE_VERSION(vm) < J2SE_V11) {
				if (IsValidCodePage(54936)) {
					return "GB18030";
				}
			}
			return "GBK";
		} else if (cp == 54936) return "GB18030";
		codepage[0] = 'M';
		codepage[1] = 'S';
	} else {
		codepage[0] = 'C';
#if defined(JCL_J2SE)
		codepage[1] = 'p';
#else
		codepage[1] = 'P';
#endif
	}

	return codepage;
}


/**
 * Turns a platform independent DLL name into a platform specific one
 */	
void mapLibraryToPlatformName(const char *inPath, char *outPath) {
	strcpy(outPath,inPath);
	strcat(outPath, ".dll");
}


/**
 * Try to find the 'correct' windows temp directory.
 */
char * getTmpDir(JNIEnv *env, char **tempdir) {
	PORT_ACCESS_FROM_ENV(env);

	DWORD rc;
	wchar_t unicodeBuffer[EsMaxPath];
	char *buffer = NULL;
	char *retVal = ".";

	rc = GetTempPathW(EsMaxPath, unicodeBuffer);

	/* If the function succeeds, the return value is the number of characters stored into 
	the buffer, not including the terminating null character. If the buffer is not large enough, 
	the return value will exceed the length parameter (i.e. the required size)
	*/

	if((rc != 0) && (rc < EsMaxPath)) {
		/* convert */
		rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeBuffer, -1, NULL, 0, NULL, NULL);
		if(rc != 0) {
			buffer = jclmem_allocate_memory(env, rc);
			if(NULL != buffer) {
				rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeBuffer, -1,  buffer, rc, NULL, NULL);
				if(rc == 0) {
					jclmem_free_memory(env, buffer);
					buffer = NULL;
				} else {
					retVal = buffer;
				}
			}
		}
	}
	*tempdir = buffer;
	return retVal;
}

/**
 * @internal
 * Converts the Unicode string to UTF8 encoded data in the provided buffer.
 *
 * @param[in] portLibrary The port library
 * @param[in] unicodeString The unicode buffer to convert
 * @param[in] utf8Buffer The buffer to store the UTF8 encoded bytes into
 * @param[in] size The size of utf8Buffer
 *
 * @return 0 on success, -1 on failure.
 */
I_32
convertToUTF8(J9PortLibrary* portLibrary, const wchar_t* unicodeString, char* utf8Buffer, UDATA size)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if(0 == WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeString, -1, utf8Buffer, (int)size, NULL, NULL)) {
		j9error_set_last_error(GetLastError(), J9PORT_ERROR_OPFAILED); /* continue */
		return -1;
	}
	return 0;
}
