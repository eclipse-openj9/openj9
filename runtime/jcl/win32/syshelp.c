/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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


char* getPlatformFileEncoding(JNIEnv *env, char *codepage, int size, int encodingType);
void mapLibraryToPlatformName(const char *inPath, char *outPath);


char* getPlatformFileEncoding(JNIEnv *env, char *codepage, int size, int encodingType) {
	PORT_ACCESS_FROM_ENV(env);
	LCID threadLocale;
	CPINFO cpInfo;
	int cp;
#ifdef UNICODE
	int i;
#endif
	int length;

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
		codepage[1] = 'p';
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
