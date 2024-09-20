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

/* JCL_J2SE */
#define JCL_J2SE

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
 * Try to find the 'correct' unix temp directory, as taken from the man page for tmpnam.
 * As a last resort, the '.' representing the current directory is returned.
 */
char * getTmpDir(JNIEnv *env, char**envSpace) {
	PORT_ACCESS_FROM_ENV(env);
	I_32 envSize;
	if ((envSize = j9sysinfo_get_env("TMPDIR", NULL, 0))> 0) {
		*envSpace = jclmem_allocate_memory(env,envSize);	/* trailing null taken into account */
		if(*envSpace==NULL) return ".";
		j9sysinfo_get_env("TMPDIR", *envSpace, envSize);
		if (j9file_attr(*envSpace) > -1)
			return *envSpace;
		/* directory was not there, free up memory and continue */
		jclmem_free_memory(env,*envSpace);
		*envSpace = NULL;
		}
	if (j9file_attr(P_tmpdir) > -1)
		return P_tmpdir;
	if (j9file_attr("/tmp") > -1)
		return "/tmp";
	return ".";
}

jobject getPlatformPropertyList(JNIEnv *env, const char *strings[], int propIndex)
{
	PORT_ACCESS_FROM_ENV(env);
	char *charResult = NULL;
	char *envSpace = NULL;
	jobject plist = NULL;
	char userdir[EsMaxPath] = {0};
	char home[EsMaxPath] = {0};
	char *homeAlloc = NULL;
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	/* Hard coded file/path separators and other values */

#if defined(J9ZOS390)
	if (J2SE_VERSION_FROM_ENV(env)) {
		strings[propIndex++] = "platform.notASCII";
		strings[propIndex++] = "true";

		strings[propIndex++] = "os.encoding";
		strings[propIndex++] = "ISO8859_1";
	}
#endif

	strings[propIndex++] = "file.separator";
	strings[propIndex++] = "/";

	strings[propIndex++] = "line.separator";
	strings[propIndex++] = "\n";

	/* Get the directory where the executable was started */
	strings[propIndex++] = "user.dir";
	charResult = getcwd(userdir, EsMaxPath);
	if (charResult == NULL) {
		strings[propIndex++] = ".";
	} else {
		strings[propIndex++] = charResult;
	}

	strings[propIndex++] = "user.home";
	charResult = NULL;
#if defined(J9ZOS390)
	charResult = getenv("HOME");
	if (NULL != charResult) {
		strings[propIndex++] = charResult;
	} else {
		uid_t uid = geteuid();
		if (0 != uid) {
			struct passwd *userDescription = getpwuid(uid);
			if (NULL != userDescription) {
				charResult = userDescription->pw_dir;
				strings[propIndex++] = charResult;
			}
		} else {
			char *loginID = getlogin();
			if (NULL != loginID) {
				struct passwd *userDescription = getpwnam(loginID);
				if (NULL != userDescription) {
					charResult = userDescription->pw_dir;
					strings[propIndex++] = charResult;
				}
			}
		}
	}

	/* there exist situations where one of the above calls will fail.  Fall through to the Unix solution for those cases */
#endif
#if defined(J9VM_OPT_CRIU_SUPPORT)
	/* Skip getpwuid if a checkpoint can be taken.
	 * https://github.com/eclipse-openj9/openj9/issues/15800
	 */
	if (!vmFuncs->isCheckpointAllowed(vm))
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	{
		/*[PR 101939] user.home not set correctly when j9 invoked via execve(x,y,null) */
		if (NULL == charResult) {
			struct passwd *pwentry = getpwuid(getuid());
			if (NULL != pwentry) {
				charResult = pwentry->pw_dir;
				strings[propIndex++] = charResult;
			}
		}
	}

	if (NULL == charResult) {
		IDATA result = j9sysinfo_get_env("HOME", home, sizeof(home));
		strings[propIndex] = ".";
		if (0 == result) {
			if (strlen(home) > 0) {
				strings[propIndex] = home;
			}
		} else if (result > 0) {
			homeAlloc = j9mem_allocate_memory(result, J9MEM_CATEGORY_VM_JCL);
			if (NULL != homeAlloc) {
				result = j9sysinfo_get_env("HOME", homeAlloc, result);
				if (0 == result) {
					strings[propIndex] = homeAlloc;
				}
			} else {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto failed;
			}
		}
		propIndex += 1;
	}

	/* Get the Temp Dir name */
	strings[propIndex++] = "java.io.tmpdir";
	strings[propIndex++] = getTmpDir(env, &envSpace);

	if (JAVA_SPEC_VERSION < 12) {
		/* Get the timezone */
		strings[propIndex++] = "user.timezone";
		strings[propIndex++] = "";
	}

	plist = createSystemPropertyList(env, strings, propIndex);
	if (NULL != envSpace) {
		jclmem_free_memory(env,envSpace);
	}

failed:
	if (NULL != homeAlloc) {
		jclmem_free_memory(env, homeAlloc);
	}
	return plist;
}

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

	/* Called with codepageProp == NULL to initialize the locale */
	if (NULL == codepageProp) {
		return NULL;
	}

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
