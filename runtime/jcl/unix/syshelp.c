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

#include <dirent.h>
#include <langinfo.h>
#include <locale.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "j2sever.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif

/* defineCodepageTable */
/* NULL separated list of code page aliases. The first name is */
/* the name of the System property, the names following before */
/* the NULL are the aliases. */
char* CodepageTable[] = {
	"ISO8859_1",
	/*[PR 96285]*/
	NULL,
#if defined(AIXPPC)
	/*[PR105613]*/
	"IBM-943C",
	"IBM-943",
	NULL,
	"IBM-950",
	"big5",
	NULL
#endif
};

/**
 * Turns a platform independent DLL name into a platform specific one.
 */	
void mapLibraryToPlatformName(const char *inPath, char *outPath) {
#ifdef J9OS_I5
	strcpy(outPath,inPath);
	strcat(outPath, ".srvpgm");
#else
	strcpy(outPath, "lib");
	strcat(outPath,inPath);
#if defined(AIXPPC) && (JAVA_SPEC_VERSION <= 14)
	strcat(outPath, ".a");
#else /* defined(AIXPPC) && (JAVA_SPEC_VERSION == 8) */
	strcat(outPath, J9PORT_LIBRARY_SUFFIX);
#endif /* defined(AIXPPC) && (JAVA_SPEC_VERSION == 8) */
#endif
}


char *getPlatformFileEncoding(JNIEnv * env, char *codepageProp, int propSize, int encodingType)
{
#if defined(J9ZTPF)
	return "ISO8859_1";
#endif /* defined(J9ZTPF) */
	char *codepage = NULL;
	int i = 0;
	int nameIndex = 0;
#if defined(LINUX) || defined(OSX)
	IDATA result = 0;
	char langProp[24] = {0};
#if defined(LINUX)
	char *ctype = NULL;
#endif /* defined(LINUX) */
	PORT_ACCESS_FROM_ENV(env);
#endif /* defined(LINUX) || defined(OSX) */

#if defined(LINUX)
	/*[PR 104520] Return EUC_JP when LC_CTYPE is not set, and the LANG environment variable is "ja" */
	ctype = setlocale(LC_CTYPE, NULL);
	if ((NULL == ctype) 
		|| (0 == strcmp(ctype, "C"))
		|| (0 == strcmp(ctype, "POSIX"))
	) {
		result = j9sysinfo_get_env("LANG", langProp, sizeof(langProp));
		if ((0 == result) && (0 == strcmp(langProp, "ja"))) {
			return "EUC-JP-LINUX";
		}
	}
	codepage = nl_langinfo(_NL_CTYPE_CODESET_NAME);
#elif defined(OSX)
	/* LC_ALL overwrites LC_CTYPE or LANG;
	 * LC_CTYPE applies to classification and conversion of characters, and to multibyte and wide characters;
	 * LANG specifies the locale to use except as overridden by LC_CTYPE
	 */
	codepage = "UTF-8";
	result = j9sysinfo_get_env("LC_ALL", langProp, sizeof(langProp));
	if (result >= 0) {
		if ((0 == result) && (0 == strcmp(langProp, "C"))) {
			/* LC_ALL is C */
			codepage = "US-ASCII";
		}
	} else {
		/* LC_ALL is not set */
		result = j9sysinfo_get_env("LC_CTYPE", langProp, sizeof(langProp));
		if (result >= 0) {
			if ((0 == result) && (0 == strcmp(langProp, "C"))) {
				/* LC_CTYPE is C */
				codepage = "US-ASCII";
			}
		} else {
			/* LC_CTYPE is not set */
			result = j9sysinfo_get_env("LANG", langProp, sizeof(langProp));
			if ((0 == result) && (0 == strcmp(langProp, "C"))) {
				/* LANG is C */
				codepage = "US-ASCII";
			}
		}
	}
	return codepage;
#else
	codepage = nl_langinfo(CODESET);
#endif /* defined(LINUX) */

	if (codepage == NULL || codepage[0] == '\0') {
		codepage = "ISO8859_1";
	} else {
		if (!strcmp(codepage, "EUC-JP")) {
			/*[PR CMVC 175994] file.encoding system property on ja_JP.eucjp */
			codepage = "EUC-JP-LINUX";
		} else {
			strncpy(codepageProp, codepage, propSize);
			codepageProp[propSize - 1] = '\0';
			/*[PR 96285]*/
			codepage = codepageProp;
			nameIndex = 0;
			for (i = 1; i < sizeof(CodepageTable) / sizeof(char *); i++) {
				if (CodepageTable[i] == NULL) {
					nameIndex = ++i;
					continue;
				}
				if (!strcmp(CodepageTable[i], codepageProp)) {
					codepage = CodepageTable[nameIndex];
					break;
				}
			}
		}
	}
	return codepage;
}
