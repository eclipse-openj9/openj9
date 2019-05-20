/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#if 0
#define DEBUG
#endif

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#include <io.h>
#endif /* WIN32 */

#include "j9.h"
#include "jni.h"
#include "exelib_api.h"
#include "j9exelibnls.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if !defined(WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(J9ZOS39064)
#include "omriarv64.h"
#endif /* defined(J9ZOS39064) */

extern void lookupJVMFunctions(void *vmdll);

typedef jint (JNICALL *CreateVM)(JavaVM**, void**, void*);
typedef jint (JNICALL *InitArgs)(void*);
typedef jint (JNICALL *GetVMs)(JavaVM**, jsize, jsize*);
typedef jint (JNICALL *DetachThread)(JavaVM *);
typedef jint (JNICALL *DestroyVM)(JavaVM *);

static JavaVMInitArgs *args = NULL;

static CreateVM globalCreateVM=NULL;
static InitArgs globalInitArgs=NULL;
static GetVMs globalGetVMs=NULL;
static DestroyVM globalDestroyVM=NULL;

static JavaVM * globalVM = NULL;

#if defined(AIXPPC)
/* Used to keep track of whether or not opening of the "master redirector" has been attempted. 
 * Avoiding an infinite loop when libjvm.a is soft linked to libjvm.so
 */
static int attempted_to_open_master = 0;

int openLibraries(const char *libraryDir);
#else /* defined(AIXPPC) */
static int openLibraries(const char *libraryDir);
#endif /* defined(AIXPPC) */
static const char *isPackagedWithCompressedRefs(void);
static BOOLEAN isPackagedWithSubdir(const char *subdir);
static void showVMChoices(void);

#ifdef WIN32
#define J9_MAX_PATH _MAX_PATH
static HINSTANCE j9vm_dllHandle = (HINSTANCE) 0;
#else
#define J9_MAX_PATH PATH_MAX
static void *j9vm_dllHandle = NULL;
#endif
/* define a size for the buffer which will hold the directory name containing the libjvm.so */
#define J9_VM_DIR_LENGTH 32

#if defined(J9ZOS39064)
#pragma linkage (GETTTT,OS)
#pragma map (getUserExtendedPrivateAreaMemoryType,"GETTTT")
UDATA getUserExtendedPrivateAreaMemoryType();
#endif /* defined(J9ZOS39064) */

/*
 * Keep this structure synchronized with gc_policy_name table in parseGCPolicy()
 */
typedef enum gc_policy{
	GC_POLICY_OPTTHRUPUT,
	GC_POLICY_OPTAVGPAUSE,
	GC_POLICY_GENCON,
	GC_POLICY_BALANCED,
	GC_POLICY_METRONOME,
	GC_POLICY_NOGC
} gc_policy;

#if defined(LINUX) || defined(OSX)
/* defining _GNU_SOURCE allows the use of dladdr() in dlfcn.h */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#define __USE_GNU 1
#include <dlfcn.h>
#endif

#ifdef AIXPPC
#include <stdlib.h>
#include <sys/ldr.h>
#include <load.h>
#include <dlfcn.h>
#endif /* defined(AIXPPC) */

#if defined(J9ZOS390)
#include <dll.h>
#include "atoe.h"
#include <stdlib.h>
#include <errno.h>
#define dlsym   dllqueryfn
#define dlopen(a,b)     dllload(a)
#define dlclose dllfree
#define getj9bin        getj9binZOS
#define J9FSTAT fstat
#endif /* defined(J9ZOS390) */

#ifndef PATH_MAX
#define PATH_MAX 1023
#endif
#define ENVVAR_OPENJ9_JAVA_OPTIONS "OPENJ9_JAVA_OPTIONS"
#define ENVVAR_IBM_JAVA_OPTIONS "IBM_JAVA_OPTIONS"

#if defined(AIXPPC)
static J9StringBuffer* getLibraryNameWithPath(J9StringBuffer *buffer);
#endif /* defined(AIXPPC) */
static J9StringBuffer* getjvmBin(BOOLEAN removeSubdir);
static void chooseJVM(JavaVMInitArgs *args, char *retBuffer, size_t bufferLength);
static void addToLibpath(const char *dir);
static J9StringBuffer *findDir(const char *libraryDir);

static J9StringBuffer* jvmBufferCat(J9StringBuffer* buffer, const char* string);
static J9StringBuffer* jvmBufferEnsure(J9StringBuffer* buffer, UDATA len);
static char* jvmBufferData(J9StringBuffer* buffer);
static void jvmBufferFree(J9StringBuffer* buffer);
static BOOLEAN parseGCPolicy(char *buffer, int *value);
#define MIN_GROWTH 128

#if defined(RS6000) || defined(LINUXPPC)
#ifdef PPC64
#ifdef J9VM_ENV_LITTLE_ENDIAN
#define JVM_ARCH_DIR "ppc64le"
#else /* J9VM_ENV_LITTLE_ENDIAN */
#define JVM_ARCH_DIR "ppc64"
#endif /* J9VM_ENV_LITTLE_ENDIAN */
#else
#define JVM_ARCH_DIR "ppc"
#endif /* PPC64*/
#elif defined(J9X86) || defined(WIN32)
#define JVM_ARCH_DIR "i386"
#elif defined(S390) || defined(J9ZOS390)
#if defined(S39064) || defined(J9ZOS39064)
#define JVM_ARCH_DIR "s390x"
#else
#define JVM_ARCH_DIR "s390"
#endif /* S39064 || J9ZOS39064 */
#elif defined(J9HAMMER)
#define JVM_ARCH_DIR "amd64"
#elif defined(J9ARM)
#define JVM_ARCH_DIR "arm"
#elif defined(J9AARCH64)
#define JVM_ARCH_DIR "aarch64"
#else
#error "Must define an architecture"
#endif

#define COMPRESSEDREFS_DIR "compressedrefs"
#define DEFAULT_DIR "default"

#define XMX	"-Xmx"

/* We use forward slashes here because J9VM_LIB_ARCH_DIR is not used on Windows. */
#if (JAVA_SPEC_VERSION >= 9) || defined(OSX)
/* On OSX, <arch> doesn't exist, so JVM_ARCH_DIR shouldn't be included in J9VM_LIB_ARCH_DIR. */
#define J9VM_LIB_ARCH_DIR "/lib/"
#else /* (JAVA_SPEC_VERSION >= 9) || defined(OSX) */
#define J9VM_LIB_ARCH_DIR "/lib/" JVM_ARCH_DIR "/"
#endif /* (JAVA_SPEC_VERSION >= 9) || defined(OSX) */

#if defined(WIN32)
#define DIR_SLASH_CHAR '\\'
#else
#define DIR_SLASH_CHAR '/'
#endif

#if defined(DEBUG)
#define DBG_MSG(x) printf x
#else
#define DBG_MSG(x)
#endif

/*
 * Remove one segment of a path, optionally keeping a trailing DIR_SLASH_CHAR.
 */
static void
truncatePath(char *inputPath, BOOLEAN keepSlashChar) {
	char *lastOccurence = strrchr(inputPath, DIR_SLASH_CHAR);
	/* strrchr() returns NULL if it cannot find the character */
	if (NULL != lastOccurence) {
		lastOccurence[keepSlashChar ? 1 : 0] = '\0';
	}
}

#if (JAVA_SPEC_VERSION == 8) || defined(AIXPPC)
/*
 * Remove the suffix from string if present.
 */
static void
removeSuffix(char *string, const char *suffix)
{
	size_t stringLength = strlen(string);
	size_t suffixLength = strlen(suffix);

	if (stringLength >= suffixLength) {
		char *tail = &string[stringLength - suffixLength];

		if (0 == strcmp(tail, suffix)) {
			*tail = '\0';
		}
	}
}
#endif /* (JAVA_SPEC_VERSION == 8) || defined(AIXPPC) */

static void
addToLibpath(const char *dir)
{
#if defined(J9ZOS390)
	char *oldPath, *newPath;
	int rc, newSize;
	char *putenvPath;
	int putenvSize;
	int putenvErrno;

	if (!dir) {
		return;
	}

	if (dir[0] == '\0') {
		return;
	}

	oldPath = getenv("LIBPATH");
	DBG_MSG(("\nLIBPATH before = %s\n", oldPath ? oldPath : "<empty>"));

	newSize = (oldPath ? strlen(oldPath) : 0) + strlen(dir) + 2;  /* 1 for :, 1 for \0 terminator */
	newPath = malloc(newSize);

	if(!newPath) {
		fprintf(stderr, "addToLibpath malloc(%d) 1 failed, aborting\n", newSize);
		abort();
	}

	/* prepend the new path */
	strcpy(newPath, dir);
	if (oldPath) {
		strcat(newPath, ":");
		strcat(newPath, oldPath);
	}

	putenvSize = newSize + strlen("LIBPATH=");
	putenvPath = malloc(putenvSize);
	if(!putenvPath) {
		fprintf(stderr, "addToLibpath malloc(%d) 2 failed, aborting\n", putenvSize);
		abort();
	}

	strcpy(putenvPath,"LIBPATH=");
	strcat(putenvPath, newPath);
	rc = putenv(putenvPath);
	putenvErrno = errno;
	free(putenvPath);

#ifdef DEBUG
	printf("\nLIBPATH after = %s\n", getenv("LIBPATH"));
#endif
	free(newPath);

	if (rc != 0) {
		fprintf(stderr, "addToLibpath putenv(%s) failed: %s\n", putenvPath, strerror(putenvErrno));
		abort();
	}
#endif
}

void
freeGlobals(void)
{
#if defined(WIN32)
	if (NULL != j9vm_dllHandle) {
		FreeLibrary(j9vm_dllHandle);
	}
#else
	int rc = 0;
	if (NULL != j9vm_dllHandle) {
		rc = dlclose(j9vm_dllHandle);
		if (0 != rc) {
			printf("Error closing jvm library: \"%s\"\n", dlerror());
		}
		j9vm_dllHandle = NULL;
	}
#endif
	if (NULL != args) {
		free(args);
		args = NULL;
	}
}

static void
showVMChoices(void)
{
	if (isPackagedWithCompressedRefs()) {
		fprintf(stdout, "\nThe following options control global VM configuration:\n\n");
		fprintf(stdout, "  -Xcompressedrefs              use compressed heap references\n");
	} 
}

static int xcompressed = -1;
static int xnocompressed = -1;

/**
 * Checks if specified option is part of the envOptions string.
 * Verifies that it is surrounded by whitespace (or is at the
 * beginning or end of the envOptions string).
 *
 * Note: This is not as robust as parseOptionsFileText() in
 * jvminit.c which accounts for quoted strings and such.
 *
 * @param envOptions    Null-terminated string of options.
 * @param option        Null-terminated option to find.
 *
 * @return TRUE if option was found, FALSE otherwise.
 */
static BOOLEAN
hasEnvOption(const char *envOptions, const char *option)
{
	BOOLEAN success = FALSE;
	const char *start = strstr(envOptions, option);
	UDATA optionLength = strlen(option);

	while (NULL != start) {
		if ((start == envOptions) || isspace(start[-1])) {
			const char *end = start + optionLength;

			if ((*end == '\0') || isspace(*end)) {
				success = TRUE;
				break;
			}
		}
		start = strstr(start + 1, option);
	}

	return success;
}

/**
 * Searching for most right occurrence of option in the options line
 * The option should be first in the command line or has a space before
 * There is no check what is behind the discovered option
 *
 * @param envOptions    Null-terminated string of options
 * @param option        Null-terminated option to find
 * @return pointer to discovered option on options line (NULL means not found)
 */
static char *
findStartOfMostRightOption(const char *envOptions, const char *option)
{
	char *result = strstr(envOptions, option);
	UDATA optionSize = strlen(option);
	if (NULL != result) {
		if ((result == envOptions) || isspace(result[-1])) {
			char *cursor = result;
			char *next = NULL;
			while (NULL != (next = strstr(cursor + optionSize, option))) {
				if (isspace(next[-1])) {
					result = next;
				}
				cursor = next;
			}
		}
	}
	return result;
}

/* Scan the next unsigned number off of the argument string.
 * Store the result in *result
 * Answer 0 on success
 */
UDATA
scan_u64(char **scan_start, U_64* result)
{
	/* supporting 0x prefix might be nice (or octal) */
	U_64 total = 0;
	UDATA rc = 1;
	char *c = *scan_start;

	/* isdigit isn't properly supported everywhere */
	while ( *c >= '0' && *c <= '9' ) {
		UDATA digitValue = *c - '0';

		if (total > ((U_64)-1) / 10 ) {
			return 2;
		}

		total *= 10;

		if ( total > ((U_64)-1) - digitValue ) {
			return 2;
		}

		total += digitValue;

		rc = 0;	/* we found at least one digit */

		c++;
	}

	*scan_start = c;
	*result = total;
	return rc;
}

/**
 * Parse memory size value where it is a decimal number possibly followed by postfix
 * postfix must be 'k', 'm', 'g' in capitals or lower case
 * Note: this function does not check what located behind parsed option.
 * This is done for compatibility with parsing inside GC
 *
 * @param option pointer to start point to scan
 * @return parsed memory size (0 if parsing was not successful)
 */
static U_64
parseMemorySizeValue(char *option)
{
	U_64 result = 0;
	char *cursor = option;

	if (NULL != option) {
		if (0 == scan_u64(&cursor, &result)) {
			UDATA shiftFactor = 0;
			BOOLEAN parsingError = FALSE;

			switch (*cursor) {
			case 'k':
			case 'K':
				shiftFactor = 10;
				break;
			case 'm':
			case 'M':
				shiftFactor = 20;
				break;
			case 'g':
			case 'G':
				shiftFactor = 30;
				break;
			case ' ':
			case '\0':
				break;
			default:
				parsingError = TRUE;
				result = 0;
				break;
			}

			if (!parsingError && (0 != shiftFactor)) {
				if (result <= (((U_64)-1) >> shiftFactor)) {
					result <<= shiftFactor;
				} else {
					result = 0;
				}
			}
		}
	}
	return result;
}

/**
 * @param args The VM command line arguments
 * @param retBuffer The buffer which will be populated with the directory name (must be big enough to contain the name and the NULL byte)
 * @param bufferLength The side of the retBuffer
 *
 * Returns the name of the directory containing the libjvm.so VM library  in the caller-provided retBuffer (with a terminating NULL byte)
 *
 * Exits and prints an error message on error. Returns on success.
 */
static void
chooseJVM(JavaVMInitArgs *args, char *retBuffer, size_t bufferLength)
{
	char *envOptions = NULL;
	int i;
	int xjvm = -1;

	char *namedVM = NULL;
	const char *basePointer = NULL;
	char *optionUsed = NULL;
	size_t nameLength = 0;

	char *xjvmstr = NULL;
	char *xcompressedstr = NULL;
	char *xnocompressedstr = NULL;

	int ignoreUnrecognizedEnabled = 0;

	int gcPolicy = GC_POLICY_GENCON;
	const char *gcPolicyOption = "-Xgcpolicy:";
	size_t gcPolicyOptionLength = strlen(gcPolicyOption);
	char *gcPolicyString = NULL;

	char *xmxstr = NULL;
	U_64 requestedHeapSize = 0;

	/* the command line is handled below but look into the ENVVAR_OPENJ9_JAVA_OPTIONS here, since it is a special case */
	envOptions = getenv(ENVVAR_OPENJ9_JAVA_OPTIONS);
	if (NULL == envOptions) {
		envOptions = getenv(ENVVAR_IBM_JAVA_OPTIONS);
	}
	if (NULL != envOptions) {
		/* we need a non-zero index to point to where this occurs to use the first index we don't have - the number of arguments in the list */
		int i = 0;

		gcPolicyString = findStartOfMostRightOption(envOptions, gcPolicyOption);
		if (NULL == gcPolicyString) {
			if (hasEnvOption(envOptions, "-XX:+UseNoGC")) {
				gcPolicyString = "nogc";
			}
		}

		if (NULL != gcPolicyString) {
			parseGCPolicy(gcPolicyString + gcPolicyOptionLength, &gcPolicy);
		}

		if (hasEnvOption(envOptions, "-Xcompressedrefs")) {
			xcompressed = i;
			xcompressedstr = "-Xcompressedrefs";
		}
		if (hasEnvOption(envOptions, "-XX:+UseCompressedOops")) {
			xcompressed = i;
			xcompressedstr = "-XX:+UseCompressedOops";
		}
		if (hasEnvOption(envOptions, "-Xnocompressedrefs")) {
			xnocompressed = i;
			xnocompressedstr = "-Xnocompressedrefs";
		}
		if (hasEnvOption(envOptions, "-XX:-UseCompressedOops")) {
			xnocompressed = i;
			xnocompressedstr = "-XX:-UseCompressedOops";
		}
		
		xjvmstr = strstr(envOptions, "-Xjvm:");
		if (NULL != xjvmstr) {
			char *space = NULL;

			xjvm = i;
			namedVM = strstr(envOptions, "-Xjvm:") + 6;
			/* make sure that we don't include the rest of the env var by saving the length until the next space */
			space = strstr(namedVM, " ");
			if (NULL == space) {
				nameLength = strlen(namedVM);
			} else {
				nameLength = (size_t)(space - namedVM);
			}
		}

		xmxstr = findStartOfMostRightOption(envOptions, XMX);
		if (NULL != xmxstr) {
			xmxstr += sizeof(XMX)-1;
		}
	}

	for( i=0; i < args->nOptions; i++ ) {
		if ( 0 == strcmp(args->options[i].optionString, "-Xcompressedrefs") || 0 == strcmp(args->options[i].optionString, "-XX:+UseCompressedOops") ) {
			xcompressed = i+1;
			xcompressedstr = args->options[i].optionString;
		} else if( 0 == strcmp(args->options[i].optionString, "-Xnocompressedrefs")  || 0 == strcmp(args->options[i].optionString, "-XX:-UseCompressedOops") ) {
			xnocompressed = i+1;
			xnocompressedstr = args->options[i].optionString;
		} else if( 0 == strncmp(args->options[i].optionString, "-Xjvm:", 6) ) {
			if ( (NULL != xjvmstr) && (0 != strcmp(xjvmstr, args->options[i].optionString)) ) {
				fprintf( stdout, "incompatible options specified: %s %s\n", xjvmstr, args->options[i].optionString );
				exit(-1);
			}
			xjvm = i+1;
			namedVM = args->options[i].optionString + 6;
			xjvmstr = args->options[i].optionString;
		} else if (0 == strncmp(args->options[i].optionString, XMX, sizeof(XMX)-1)) {
			xmxstr = args->options[i].optionString + sizeof(XMX)-1;
		} else if ((0 == strcmp(args->options[i].optionString, "-XXvm:ignoreUnrecognized")) || (JNI_TRUE == args->ignoreUnrecognized)) {
			ignoreUnrecognizedEnabled = 1;
		} else if (0 == strncmp(args->options[i].optionString, gcPolicyOption, gcPolicyOptionLength)) {
			parseGCPolicy(args->options[i].optionString + gcPolicyOptionLength, &gcPolicy);
		}
	}

	requestedHeapSize = parseMemorySizeValue(xmxstr);

	/* check for conflicts.
	 * The check is based on increasing 'optEnabled' counter for every mutually exclusive sidecar related
	 * arguments. If optEnabled is found to be bigger than 1, we have a problem.
	 */
	if ( xjvm != -1 ) {
		/*
		 * xjvm overrides most other options. If the user specified -Xjvm: trust that they know what they're doing.
		 * 1) redirector invokes the chosen sidecar (through -Xjvm:[sidecar]) without raising any conflict.
		 * 2) the chosen sidecar is invoked and re-parses the command arguments.
		 * 3) sidecar init will check and raise any feature conflict.
		 */
		xcompressed = -1;
		xcompressedstr = NULL;

		xnocompressed = -1;
		xnocompressedstr = NULL;

	}

	/* decode which VM directory to use */
	basePointer = DEFAULT_DIR;
	if ((xnocompressed != -1) && (xcompressed < xnocompressed)) {
		basePointer = DEFAULT_DIR;
		optionUsed = xnocompressedstr;
	} else if ((xcompressed != -1) && (xnocompressed < xcompressed)) {
		basePointer = COMPRESSEDREFS_DIR;
		optionUsed = xcompressedstr;
	} else if (xjvm != -1) {
		optionUsed = xjvmstr;
		basePointer = namedVM;
	} else {

		/*
		 * If compressedrefs VM is included to the package
		 * and requested heap size is smaller then maximum heap size recommended for compressed refs VM
		 * set VM to be launched to Compressed References VM
		 */
		if (isPackagedWithSubdir(COMPRESSEDREFS_DIR)) {
			U_64 maxHeapForCR = 0;
#if defined(J9ZOS39064)
			switch (getUserExtendedPrivateAreaMemoryType()) {
			case ZOS64_VMEM_ABOVE_BAR_GENERAL:
			default:
				break;
			case ZOS64_VMEM_2_TO_32G:
				maxHeapForCR = MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_3BIT_SHIFT_COMPRESSEDREFS;
				break;
			case ZOS64_VMEM_2_TO_64G:
				maxHeapForCR = MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_COMPRESSEDREFS;
				break;
			}
#else /* defined(J9ZOS39064) */
			maxHeapForCR = MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_COMPRESSEDREFS;
#endif /* defined(J9ZOS39064) */

			/* Sizes Table for Segregated heap does not support 4-bit shift so do not use it for Metronome */
			if ((0 != maxHeapForCR) && (GC_POLICY_METRONOME == gcPolicy)) {
				maxHeapForCR = MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_3BIT_SHIFT_COMPRESSEDREFS;
			}

			if (requestedHeapSize <= maxHeapForCR) {
				basePointer = COMPRESSEDREFS_DIR;
			}
		}
		}

	/*
	 * Jazz 31002 : if -XXvm:ignoreUnrecognized is specified and that the targeted VM
	 * (eg. compressed ref vm) can't be found, revert to default VM.
	 */
	if (!isPackagedWithSubdir(basePointer) && (1 == ignoreUnrecognizedEnabled)) {
		basePointer = DEFAULT_DIR;
	}

	/* if we didn't set the string length already, do it now for the comparison and copy */
	if (0 == nameLength) {
		nameLength = strlen(basePointer);
	}

	/* now, make sure that it will fit in the buffer */
	if (nameLength < bufferLength) {
		/* it will fit, so do the copy */
		memcpy(retBuffer, basePointer, nameLength);
		retBuffer[nameLength] = '\0';
	} else {
		/* won't fit, so set the error */
		fprintf(stdout, "Failed to choose VM (buffer too small) - aborting\n");
		exit(-1);
	}

	/* check that the chosen VM exists */
	if (!isPackagedWithSubdir(retBuffer) ) {
		fprintf(stdout, "Selected VM [%s] ", retBuffer);
		if ( NULL != optionUsed ) {
			fprintf(stdout, "by option %s ", optionUsed);
		}
		fprintf(stdout, "does not exist.\n");

		/* direct user to OpenJ9 build configurations to properly generate the requested build. */
		if (DEFAULT_DIR == basePointer) {
			fprintf(stdout,
					"This JVM package only includes the '-Xcompressedrefs' configuration. Please run "
					"the VM without specifying the '-Xnocompressedrefs' option or by specifying the "
					"'-Xcompressedrefs' option.\nTo compile the other configuration, please run configure "
					"with '--with-noncompressedrefs.\n"
			);
		}
		if (COMPRESSEDREFS_DIR == basePointer) {
			fprintf(stdout,
					"This JVM package only includes the '-Xnocompressedrefs' configuration. Please run "
					"the VM without specifying the '-Xcompressedrefs' option or by specifying the "
					"'-Xnocompressedrefs' option.\nTo compile the other configuration, please run configure "
					"without '--with-noncompressedrefs.\n"
			);
		}
		exit(-1);
	}
}

/**
 *  jint JNICALL JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args)
 *  Load and initialize a virtual machine instance.
 *	This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @param pvm pointer to the location where the JavaVM interface
 *			pointer will be placed
 *  @param penv pointer to the location where the JNIEnv interface
 *			pointer for the main thread will be placed
 *  @param vm_args java virtual machine initialization arguments
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *	DLL: jvm
 */
jint JNICALL
JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args)
{
	char *envOptions = NULL;
	jint result;
	int i;
	jint openedLibraries = 0;
	char namedVM[J9_VM_DIR_LENGTH];

#if defined(J9ZOS390)
	/* since we need to perform some output and look up env vars, we will require the a2e library to be initialized.  This will happen again, when the VM is
		actually initialized, but the iconv_init call uses a static flag to make sure that it is only initialized once, so this is safe */
	iconv_init();
#endif

	/* no tracing for this function, since it's unlikely to be used once the VM is running and the trace engine is initialized */
	args = (JavaVMInitArgs *)malloc( sizeof( JavaVMInitArgs) );
	args->version = ((JavaVMInitArgs *)vm_args)->version;
	args->nOptions = ((JavaVMInitArgs *)vm_args)->nOptions;
	args->options = ((JavaVMInitArgs *)vm_args)->options;
	args->ignoreUnrecognized = ((JavaVMInitArgs *)vm_args)->ignoreUnrecognized;

	memset(namedVM, 0, J9_VM_DIR_LENGTH);
	chooseJVM(args, namedVM, J9_VM_DIR_LENGTH);

	envOptions = getenv(ENVVAR_OPENJ9_JAVA_OPTIONS);
	if (NULL == envOptions) {
		envOptions = getenv(ENVVAR_IBM_JAVA_OPTIONS);
	}
	if ((NULL != envOptions) && hasEnvOption(envOptions, "-locateVM")) {
		J9StringBuffer *buffer = findDir(namedVM);
		fprintf(stdout, "%s\n", jvmBufferData(buffer));
		free(buffer);
		exit(0);
	} else {
		for (i = 0; i < args->nOptions; i++) {
			if (0 == strcmp(args->options[i].optionString, "-locateVM")) {
				J9StringBuffer *buffer = findDir(namedVM);
				fprintf(stdout, "%s\n", jvmBufferData(buffer));
				free(buffer);
				exit(0);
			}
		}
	}

	openedLibraries  = openLibraries(namedVM);

	if(openedLibraries == JNI_ERR) {
		fprintf(stdout, "Failed to find VM - aborting\n");
		exit(-1);
	}

#if defined(LINUXPPC) && !defined(LINUXPPC64)
	/*
		This is a work-around for a segfault on shut-down on old LinuxPPC GlibC variants (ie:  2.2.5 - where this was observed)
		The core problem may be a VM bug since this library is finalized 3 times when once is expected but it looks like a GlibC
			bug after first-pass investigation.  More investigation will be required when time permits.
		Refer to CMVC 103003 for the background of this work-around.
	*/
	dlopen("libjava.so", RTLD_NOW);
#endif

#ifdef DEBUG
	fprintf(stdout, "Calling... args=%d, pvm=%p(%p), penv=%p(%p)\n", args, pvm, *pvm, penv, *penv);
	fflush(stdout);
#endif
	result = globalCreateVM(pvm, penv, args);
#ifdef DEBUG
	fprintf(stdout, "Finished, result=%d args=%d, pvm=%p(%p), penv=%p(%p)\n", result, pvm, *pvm, penv, *penv);
	fflush(stdout);
#endif

	if (result == JNI_OK) {
		globalVM = *pvm;
/* TODO - intercept shutdown
		JavaVM * vm = (JavaVM*)BFUjavaVM;
		*pvm = vm;

		memcpy(&globalInvokeInterface, *vm, sizeof(J9InternalVMFunctions));
		globalDestroyVM = globalInvokeInterface.DestroyJavaVM;
		globalInvokeInterface.DestroyJavaVM = DestroyJavaVM;
		*vm = (struct JNIInvokeInterface_ *) &globalInvokeInterface;
*/
	} else {
		freeGlobals();
	}

	return result;
}

/**
 *	jint JNICALL JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
 *  Return pointers to all the virtual machine instances that have been
 *  created.
 *	This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @param vmBuf pointer to the buffer where the pointer to virtual
 *			machine instances will be placed
 *  @param bufLen the length of the buffer
 *  @param nVMs a pointer to an integer
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *	DLL: jvm
 */
jint JNICALL
JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
{
	jint result = JNI_OK;

#if defined(J9ZOS390)
	/**
	 * Since any code that looks at strings will fail without iconv_init and testing on z/OS is somewhat inconvenient, this
	 * call to initialize the a2e library has been added to the beginning of all the exposed entry points in the file.  A static
	 * in iconv_init ensures that only the first init call actually is honoured.
	 */
	iconv_init();
#endif

	if(NULL != globalGetVMs) {
		result = globalGetVMs(vmBuf, bufLen, nVMs);
	} else {
		/* if this is NULL, then no VM has been started yet.  This implies we should return 0 */
		/* below logic pulled from jniinv.c JNI_GetCreatedJavaVMs */
#if defined (LINUXPPC64) || (defined (AIXPPC) && defined (PPC64)) || defined (J9ZOS39064)
		/* there was a bug in Sovereign VMs on these platforms where jsize was defined to
		 * be 64-bits, rather than the 32-bits required by the JNI spec. Provide backwards
		 * compatibility if the JAVA_JSIZE_COMPAT environment variable is set
		 */
		if (getenv("JAVA_JSIZE_COMPAT")) {
			*(jlong*)nVMs = (jlong)0;
		} else {
			*nVMs = 0;
		}
#else
		*nVMs = 0;
#endif
	}

	return result;
}

/**
 *	jint JNICALL JNI_GetDefaultJavaVMInitArgs(void *vm_args)
 *  Return a default configuration for the java virtual machine
 *  implementation.
 *	This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @param vm_args pointer to a vm-specific initialization structure
 *			into which the default arguments are filled.
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *	DLL: jvm
 */
jint JNICALL
JNI_GetDefaultJavaVMInitArgs(void *vm_args)
{
	/* problem: this function cannot be resolved until the command line
	 * is parsed, but must return an answer anyway - fake what normal
	 * J9 does in this case.
	 */

#if defined(J9ZOS390)
	/**
	 * Since any code that looks at strings will fail without iconv_init and testing on z/OS is somewhat inconvenient, this
	 * call to initialize the a2e library has been added to the beginning of all the exposed entry points in the file.  A static
	 * in iconv_init ensures that only the first init call actually is honoured.
	 */
	iconv_init();
#endif

	if(globalInitArgs) {
		return globalInitArgs(vm_args);
	} else {
		UDATA jniVersion = (UDATA)((JDK1_1InitArgs *)vm_args)->version;

		if ((jniVersion == JNI_VERSION_1_2)
			|| (jniVersion == JNI_VERSION_1_4)
			|| (jniVersion == JNI_VERSION_1_6)
			|| (jniVersion == JNI_VERSION_1_8)
#if JAVA_SPEC_VERSION >= 9
			|| (jniVersion == JNI_VERSION_9)
#endif /* JAVA_SPEC_VERSION >= 9 */
#if JAVA_SPEC_VERSION >= 10
			|| (jniVersion == JNI_VERSION_10)
#endif /* JAVA_SPEC_VERSION >= 10 */
		) {
			return JNI_OK;
		} else {
			return JNI_EVERSION;
		}
	}
}

#define strsubdir(subdir) (isPackagedWithSubdir((subdir)) ? (subdir) : NULL)

/*
 * Returns the subdirectory name if a compressed references VM is included in the package containing the redirector
 */
static const char *
isPackagedWithCompressedRefs(void)
{
	return strsubdir(COMPRESSEDREFS_DIR);
}

static BOOLEAN
isPackagedWithSubdir(const char *subdir)
{
	J9StringBuffer *buffer = NULL;
#if defined(WIN32)
	wchar_t unicodeDLLName[J9_MAX_PATH];
#else /* WIN32 */
	size_t jvmBinLength = 0;
	struct stat statBuf;
#endif /* WIN32 */
	BOOLEAN rc = FALSE;

	/* find root directory for this install (e.g. jre/bin/ or jre/lib/<arch>/ dir WITH trailing slash) */
	/* Beginning with Java 9 b150, this will be sdk/bin/ or sdk/lib/ */
	buffer = getjvmBin(TRUE);
	if (NULL == buffer) {
		return FALSE;
	}

#if ! defined(WIN32)
	/* remember the length before appending subdir */
	jvmBinLength = strlen(jvmBufferData(buffer));
#endif /* ! WIN32 */
	buffer = jvmBufferCat(buffer, subdir);

#if defined(WIN32)
	MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, jvmBufferData(buffer), -1, unicodeDLLName, (int)strlen(jvmBufferData(buffer)) + 1);
	rc = (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(unicodeDLLName));
#else /* WIN32 */
	rc = (-1 != stat(jvmBufferData(buffer), &statBuf));

	if ((FALSE == rc) && (jvmBinLength > 0)) {
		/* Failed to find the subdir in .../bin, look in .../lib (or .../lib/<arch>). */
		/* overwrite the trailing slash in jvmBin, removing /subdir */
		jvmBufferData(buffer)[jvmBinLength - 1] = '\0';
		/* remove /bin, /lib or /<arch> */
		truncatePath(jvmBufferData(buffer), FALSE);
#if JAVA_SPEC_VERSION == 8
		/* if the path ended with /lib/<arch> we need to remove another segment */
		removeSuffix(jvmBufferData(buffer), "/lib");
#endif /* JAVA_SPEC_VERSION == 8 */
		buffer = jvmBufferCat(buffer, J9VM_LIB_ARCH_DIR);
		buffer = jvmBufferCat(buffer, subdir);

		rc = (-1 != stat(jvmBufferData(buffer), &statBuf));
	}
#endif /* WIN32 */

	free (buffer);
	return rc;
}

/*
 * finds the jvm.dll in directory named "libraryDir" at the same depth
 * as this directory.
 */
static J9StringBuffer *
findDir(const char *libraryDir) {
	/* find directory this DLL is in - including trailing DIR_SLASH_CHAR */
	J9StringBuffer *buffer = getjvmBin(FALSE);
	BOOLEAN isClassic = FALSE;
#if !defined(WIN32)
	struct stat statBuf;
#endif
#if defined(AIXPPC)
	J9StringBuffer *libraryNameWithPath = NULL;
#endif

	DBG_MSG(("trying dir: %s with base: %s\n", libraryDir, jvmBufferData(buffer)));

	/* check for Java 6 UNIX shape and adjust as necessary */
#if !defined(WIN32)
	isClassic = isPackagedWithSubdir("classic");
	if (isClassic) {
		J9StringBuffer *tmpBuffer = jvmBufferCat(NULL, jvmBufferData(buffer));
		char *tmpBufferData = jvmBufferData(tmpBuffer);
		truncatePath(tmpBufferData, FALSE); /* at jre/bin/classic or jre/lib/<arch>/classic */
		truncatePath(tmpBufferData, FALSE); /* at jre/bin         or jre/lib/<arch>         */
		truncatePath(tmpBufferData, FALSE); /* at jre             or jre/lib                */
#if JAVA_SPEC_VERSION == 8
		/* if the path ends with .../lib we need to remove another segment */
		removeSuffix(jvmBufferData(buffer), "/lib");
#endif /* JAVA_SPEC_VERSION == 8 */
		tmpBuffer = jvmBufferCat(tmpBuffer, J9VM_LIB_ARCH_DIR "j9vm/");
		DBG_MSG(("classic mode - trying J6 UNIX shape: %s\n", jvmBufferData(tmpBuffer)));

		if (-1 != stat(jvmBufferData(tmpBuffer), &statBuf)) {
			/* does exist, carry on */
			jvmBufferFree(buffer);
			buffer = tmpBuffer;
			DBG_MSG(("is J6 classic mode\n"));
		} else {
			/* does not exist, dump the new possible dir (likely a 5.0 build) */
			jvmBufferFree(tmpBuffer);
		}
	}
#endif /* ! WIN32 */

#if defined(AIXPPC)
	/* It is possible to open multiple redirectors on AIX.
	 * This leads to issues with global function static being properly initialized.
	 * To avoid those problems, designate the redirector in jre/lib/<arch>/j9vm/libjvm.so
	 * as the master redirector.  If a redirector is not this one, it will try and open
	 * the master and redirect to it instead of trying to open the target libjvm.so.
	 *
	 * NOTE: it is possible that jre/lib/<arch>/j9vm/libjvm.a is a soft link to
	 * jre/lib/<arch>/j9vm/libjvm.so.  If this happens and libjvm.a is opened before libjvm.so
	 * we will try to open libjvm.so and the OS will simply return the already open copy of libjvm.a.
	 * We need to detect this case and avoid it.
	 * We can't simply check for libjvm.a and not try libjvm.so since if they are not soft linked there
	 * would be uninitialized function pointers in the function table.
	 * So, try to get to the master at least once.  If we detect that we have tried to open the master
	 * then we are the master so don't try again.*/
	libraryNameWithPath = getLibraryNameWithPath(libraryNameWithPath);
	if ((0 == attempted_to_open_master) && (NULL == strstr(jvmBufferData(libraryNameWithPath), J9VM_LIB_ARCH_DIR "j9vm/libjvm.so"))) {
		J9StringBuffer *tmpBuffer = jvmBufferCat(NULL, jvmBufferData(buffer));
		char *tmpBufferData = jvmBufferData(tmpBuffer);

		/* mark that we tried to open the master redirector */
		attempted_to_open_master = 1;

		/* strip back the path */
		truncatePath(tmpBufferData, FALSE); /* at jre/bin/classic -or- jre/lib/<arch>/classic */
		truncatePath(tmpBufferData, FALSE); /* at jre/bin -or- jre/lib/<arch> */
		truncatePath(tmpBufferData, FALSE); /* at jre -or- jre/lib */

		/* build up the path to the location of the one true redirector */
		removeSuffix(tmpBufferData, "/lib");
		tmpBuffer = jvmBufferCat(tmpBuffer, J9VM_LIB_ARCH_DIR "j9vm");
		jvmBufferFree(buffer);
		buffer = tmpBuffer;
		DBG_MSG(("redirector to one true redirector: %s\n", jvmBufferData(buffer)));
	} else
#endif /* AIXPPC */
	{
		/* remove trailing directory */
		truncatePath(jvmBufferData(buffer), FALSE); /* at ../<dir> */
		truncatePath(jvmBufferData(buffer), TRUE);  /* at ../ */

		DBG_MSG(("trying after dir: %s with base: %s\n", libraryDir, jvmBufferData(buffer)));

		/* add the libraryDir */
		buffer = jvmBufferCat(buffer, libraryDir);
	}

	return buffer;
}


/*
 * opens the jvm.dll in directory named "libraryDir" at the same depth
 * as this directory.  Sets the global function pointers up for
 * passthrough.
 * 
 * on AIX this function is used in the generated.c functions to find the 'master' 
 * redirector (see details in findDir()).
 */
#if defined(AIXPPC)
int
#else
static int
#endif
openLibraries(const char *libraryDir)
{
	J9StringBuffer *buffer = NULL;
#if defined(WIN32)
	wchar_t unicodePath[J9_MAX_PATH];
#endif

	buffer = findDir(libraryDir);

	/* Polluting LIBPATH causes grief for exec()ed child processes. Don't do it unless necessary. */
#if defined(J9ZOS390)
	addToLibpath(jvmBufferData(buffer));
#endif

	/* add the DLL name */
#ifdef WIN32
	buffer = jvmBufferCat(buffer, "\\");
	buffer = jvmBufferCat(buffer, "jvm.dll");
	/* open the DLL and look up the symbols */
	DBG_MSG(("trying %s\n", jvmBufferData(buffer)));
	MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, jvmBufferData(buffer), -1, unicodePath, (int)strlen(jvmBufferData(buffer)) + 1);
	j9vm_dllHandle = LoadLibraryW(unicodePath);
	if(!j9vm_dllHandle) {
		return JNI_ERR;
	}

	globalCreateVM = (CreateVM) GetProcAddress (j9vm_dllHandle, (LPCSTR) "JNI_CreateJavaVM");
	globalInitArgs = (InitArgs) GetProcAddress (j9vm_dllHandle, (LPCSTR) "JNI_GetDefaultJavaVMInitArgs");
	globalGetVMs = (GetVMs) GetProcAddress (j9vm_dllHandle, (LPCSTR) "JNI_GetCreatedJavaVMs");
	if (!globalCreateVM || !globalInitArgs || !globalGetVMs) {
		FreeLibrary(j9vm_dllHandle);
		fprintf(stderr,"jvm.dll failed to load: global entrypoints not found\n");
		return JNI_ERR;
	}
#else /* WIN32 */
	buffer = jvmBufferCat(buffer, "/libjvm" J9PORT_LIBRARY_SUFFIX);
	/* open the DLL and look up the symbols */
#if defined(AIXPPC)
	/* CMVC 137341:
	 * dlopen() searches for libraries using the LIBPATH envvar as it was when the process
	 * was launched.  This causes multiple issues such as:
	 *  - finding 32 bit binaries for libomrsig.so instead of the 64 bit binary needed and vice versa
	 *  - finding compressed reference binaries instead of non-compressed ref binaries
	 *
	 * calling loadAndInit(libname, 0 -> no flags, NULL -> use the currently defined LIBPATH) allows
	 * us to load the library with the current libpath instead of the one at process creation
	 * time. We can then call dlopen() as per normal and the just loaded library will be found.
	 * */
	loadAndInit(jvmBufferData(buffer), L_RTLD_LOCAL, NULL);
#endif /* defined(AIXPPC) */
	j9vm_dllHandle = dlopen(jvmBufferData(buffer), RTLD_LAZY);
	if(!j9vm_dllHandle) {
			fprintf(stderr, "failed to open <%s> - reason: <%s>\n", jvmBufferData(buffer), dlerror());
			return JNI_ERR;
	}

	globalCreateVM = (CreateVM) dlsym (j9vm_dllHandle, "JNI_CreateJavaVM");
	globalInitArgs = (InitArgs) dlsym (j9vm_dllHandle, "JNI_GetDefaultJavaVMInitArgs");
	globalGetVMs = (GetVMs) dlsym (j9vm_dllHandle, "JNI_GetCreatedJavaVMs");
	if (!globalCreateVM || !globalInitArgs || !globalGetVMs) {
		dlclose(j9vm_dllHandle);
		fprintf(stderr,"jvm.dll failed to load: global entrypoints not found\n");
		return JNI_ERR;
	}
#endif /* WIN32 */

	lookupJVMFunctions(j9vm_dllHandle);
	free(buffer);
	return 0;
}

/*
 * The getjvmBin method needs to be implemented for Win32, Linux, AIX, and z/OS.
 * Note that the result buffer is assumed to be J9_MAX_PATH length and the value returned via the buffer.
 * MUST be terminated with a DIR_SLASH_CHAR.
*/

#ifdef WIN32
static J9StringBuffer*
getjvmBin(BOOLEAN removeSubdir)
{
	J9StringBuffer *buffer = NULL;
	wchar_t unicodeDLLName[J9_MAX_PATH + 1] = {0}; /*extra character is added to check if path is truncated by GetModuleFileNameW()*/
	DWORD unicodeDLLNameLength = 0;
	char result[J9_MAX_PATH];

	unicodeDLLNameLength = GetModuleFileNameW(GetModuleHandle("jvm"), unicodeDLLName, (J9_MAX_PATH + 1));
	/* Don't use truncated path */
	if (unicodeDLLNameLength > (DWORD)J9_MAX_PATH) {
		fprintf(stderr, "ERROR: cannot determine JAVA home directory\n");
		abort();
	}

	WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeDLLName, -1, result, J9_MAX_PATH, NULL, NULL);

	buffer = jvmBufferCat(NULL, result);

	if (removeSubdir) {
		/* remove jvm.dll */
		truncatePath(jvmBufferData(buffer), FALSE);
	}
	/* remove jvm.dll or classes */
	truncatePath(jvmBufferData(buffer), TRUE);

	return buffer;
}
#endif

#if (defined(LINUX) || defined(OSX)) && !defined(J9ZTPF)
static J9StringBuffer*
getjvmBin(BOOLEAN removeSubdir)
{
	J9StringBuffer *buffer = NULL;
	Dl_info libraryInfo;
	int rc = dladdr((void *)getjvmBin, &libraryInfo);

	if (0 == rc) {
		fprintf(stderr, "ERROR: cannot determine JAVA home directory\n");
		abort();
	}
	buffer = jvmBufferCat(NULL, libraryInfo.dli_fname);
	if (removeSubdir) {
		/* remove libjvm.so */
		truncatePath(jvmBufferData(buffer), FALSE);
	}
	/* remove libjvm.so or j9vm */
	truncatePath(jvmBufferData(buffer), TRUE);

	return buffer;
}
#endif /* (defined(LINUX) || defined(OSX)) && !defined(J9ZTPF) */

#ifdef AIXPPC
static J9StringBuffer*
getLibraryNameWithPath(J9StringBuffer *buffer)
{
	struct ld_info *linfo, *linfop;
	int             linfoSize, rc;
	char           *myAddress, *filename, *membername;

	/* get loader information */
	linfoSize = 1024;
	linfo = malloc(linfoSize);
	for(;;) {
		rc = loadquery(L_GETINFO, linfo, linfoSize);
		if (rc != -1) {
			break;
		}
		linfoSize *=2; /* insufficient buffer size - increase */
		linfo = realloc(linfo, linfoSize);
	}

	/* find entry for my loaded object */
	myAddress = ((char **)&getjvmBin)[0];
	for (linfop = linfo;;) {
		char *textorg  = (char *)linfop->ldinfo_textorg;
		char *textend  = textorg + (unsigned long)linfop->ldinfo_textsize;
		if (myAddress >=textorg && (myAddress < textend)) {
			break;
		}
		if (!linfop->ldinfo_next) {
			abort();
		}
		linfop = (struct ld_info *)((char *)linfop + linfop->ldinfo_next);
	}

	filename   = linfop->ldinfo_filename;
	membername = filename+strlen(filename)+1;
#ifdef DEBUG
	printf("ldinfo: filename is %s. membername is %s\n", filename, membername);
#endif
	buffer = jvmBufferCat(buffer, filename);
	free(linfo);

#ifdef DEBUG
	printf("getLibraryNameWithPath ret: %s\n", jvmBufferData(buffer));
#endif

	return buffer;
}

static J9StringBuffer*
getjvmBin(BOOLEAN removeSubdir)
{
	J9StringBuffer *buffer = NULL;
	struct ld_info *linfo, *linfop;
	int             linfoSize, rc;
	char           *myAddress, *filename, *membername;

	/* get loader information */
	linfoSize = 1024;
	linfo = malloc(linfoSize);
	for(;;) {
		rc = loadquery(L_GETINFO, linfo, linfoSize);
		if (rc != -1) {
			break;
		}
		linfoSize *=2; /* insufficient buffer size - increase */
		linfo = realloc(linfo, linfoSize);
	}

	/* find entry for my loaded object */
	myAddress = ((char **)&getjvmBin)[0];
	for (linfop = linfo;;) {
		char *textorg  = (char *)linfop->ldinfo_textorg;
		char *textend  = textorg + (unsigned long)linfop->ldinfo_textsize;
		if (myAddress >=textorg && (myAddress < textend)) {
			break;
		}
		if (!linfop->ldinfo_next) {
			abort();
		}
		linfop = (struct ld_info *)((char *)linfop + linfop->ldinfo_next);
	}

	filename   = linfop->ldinfo_filename;
	membername = filename+strlen(filename)+1;
#ifdef DEBUG
	printf("[%p]ldinfo: filename is %s. membername is %s\n",  myAddress, filename, membername);
#endif
	buffer = jvmBufferCat(buffer, filename);
	if (removeSubdir) {
		/* remove libjvm.so */
		truncatePath(jvmBufferData(buffer), FALSE);
	}
	/* remove libjvm.so or j9vm */
	truncatePath(jvmBufferData(buffer), TRUE);

	free(linfo);

#ifdef DEBUG
	printf("[%p] getjvmbin ret: %s\n", myAddress, jvmBufferData(buffer));
#endif

	return buffer;
}

#endif

#if defined(J9ZOS390) || defined(J9ZTPF)

/* Moved from jvm.c
 * TODO: Move into a util library
 */
int
isFileInDir(char *dir, char *file)
{
	int   l;
	char *fullpath;
	FILE *f;

	/* Construct 'full' path */
	if (dir[strlen(dir)-1] == DIR_SLASH_CHAR) {
		/* remove trailing DIR_SLASH_CHAR */
		dir[strlen(dir)-1] = '\0';
	}

	l = strlen(dir) + strlen(file) + 2; /* 2= '/' + null char */
	fullpath = malloc(l);
	strcpy(fullpath, dir);
	fullpath[strlen(dir)] = DIR_SLASH_CHAR;
	strcpy(fullpath+strlen(dir)+1, file);

	/* See if file exists - use fopen() for portability */
	f = fopen(fullpath, "rb");
	if (f) {
		fclose(f);
	}
	return f!=0;
}

J9StringBuffer*
findDirContainingFile(J9StringBuffer *buffer, char *paths, char pathSeparator, char *fileToFind)
{
	char *startOfDir, *endOfDir;
	int   isEndOfPaths, foundIt;

	/* Copy input as it is modified */
	paths = strdup(paths);
	if (!paths) {
		return NULL;
	}

	/* Search each dir in the list for fileToFind */
	startOfDir = endOfDir = paths;
	for (isEndOfPaths=FALSE, foundIt=FALSE; !foundIt && !isEndOfPaths; endOfDir++) {
		isEndOfPaths = endOfDir[0] == '\0';
		if (isEndOfPaths || (endOfDir[0] == pathSeparator))  {
			endOfDir[0] = '\0';
			if (strlen(startOfDir) && isFileInDir(startOfDir, fileToFind)) {
				foundIt = TRUE;
				/*strcpy(result, startOfDir);*/
				buffer = jvmBufferCat(buffer, startOfDir);
			}
			startOfDir = endOfDir+1;
		}
	}
	free(paths); /* from strdup() */
	/*return foundIt;*/

	if ( !foundIt) {
		fprintf(stderr, "ERROR: cannot determine JAVA home directory");
		abort();
	}
	return buffer;
}

J9StringBuffer*
findDirUplevelToDirContainingFile(J9StringBuffer *buffer, char *pathEnvar, char pathSeparator, char *fileInPath, int upLevels)
{
	/* Get the list of paths */
	char *paths = getenv(pathEnvar);
	if (NULL == paths) {
		return NULL;
	}

	/* find the directory */
	if (buffer = findDirContainingFile(buffer, paths, pathSeparator, fileInPath)) {
		/* Now move upLevel to it - this may not work for directories of the form
		 * /xxx/yyy/..      ... and so on.
		 * If that is a problem, could always use /.. to move up.
		 */
		for (; upLevels > 0; upLevels--) {
			truncatePath(jvmBufferData(buffer), FALSE);
		}
		/* the above loop destroyed the last DIR_SLASH_CHAR so re-add it */
		buffer = jvmBufferCat(buffer, "/");
	}

	return buffer;
}

static J9StringBuffer*
getjvmBin(BOOLEAN removeSubdir)
{
	J9StringBuffer *buffer = NULL;

	/* assumes LIBPATH points to where libjvm.so can be found */
#if defined(J9ZOS390)
	buffer = findDirUplevelToDirContainingFile(buffer, "LIBPATH", ':', "libjvm.so", 0);
#else
	buffer = findDirUplevelToDirContainingFile(buffer, "LD_LIBRARY_PATH", ':', "libjvm" J9PORT_LIBRARY_SUFFIX, 0);
#endif

	if (NULL != buffer) {
		if (removeSubdir) {
			/* remove libjvm.so */
			truncatePath(jvmBufferData(buffer), FALSE);
		}
		/* remove libjvm.so or j9vm */
		truncatePath(jvmBufferData(buffer), TRUE);
	}

	return buffer;
}
#endif /* defined(J9ZOS390) || defined(J9ZTPF) */

static J9StringBuffer* jvmBufferCat(J9StringBuffer* buffer, const char* string)
{
	UDATA len = strlen(string);

	buffer = jvmBufferEnsure(buffer, len);
	if (buffer) {
		strcat(buffer->data, string);
		buffer->remaining -= len;
	}

	return buffer;
}

static J9StringBuffer* jvmBufferEnsure(J9StringBuffer* buffer, UDATA len) {
	if (buffer == NULL) {
		UDATA newSize = len > MIN_GROWTH ? len : MIN_GROWTH;
		buffer = (J9StringBuffer*) malloc( newSize + 1 + sizeof(UDATA));	/* 1 for null terminator */
		if (buffer != NULL) {
			buffer->remaining = newSize;
			buffer->data[0] = '\0';
		}
		return buffer;
	}

	if (len > buffer->remaining) {
		UDATA newSize = len > MIN_GROWTH ? len : MIN_GROWTH;
		const char* bufData = (const char*)buffer->data;
		J9StringBuffer* new = (J9StringBuffer*) malloc(strlen(bufData) + newSize + sizeof(UDATA) + 1 );
		if (new) {
			new->remaining = newSize;
			strcpy(new->data, bufData);
		}
		free(buffer);
		return new;
	}

	return buffer;
}

static char* jvmBufferData(J9StringBuffer* buffer) {
	return buffer ? buffer->data : NULL;
}

static void jvmBufferFree(J9StringBuffer* buffer) {
	if(buffer) {
		free(buffer);
	}
}

static BOOLEAN parseGCPolicy(char *buffer, int *value)
{
	const char *gc_policy_name[] = {
			"optthruput",
			"optavgpause",
			"gencon",
			"balanced",
			"metronome",
			"nogc",
			NULL,
	};

	int i = 0;
	while (NULL != gc_policy_name[i]) {
		if (0 == strncmp(buffer, gc_policy_name[i], strlen(gc_policy_name[i]))) {
			*value = i;
			return TRUE;
		}
		i += 1;
	}
	return FALSE;
}
