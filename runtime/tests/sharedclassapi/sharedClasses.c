/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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

/*
 * A JVMTI agent that tests the JVMTI extension functions to delete/query
 * shared classes caches.
 */

#include <string.h>
#include <time.h>
#include <stdlib.h>
#if defined(ZOS)
#include <unistd.h>
#endif /* defined(ZOS) */

#include "jvmti.h"
#include "ibmjvmti.h"

#define INVALID_COM_IBM_ITERATE_SHARED_CACHES_VERSION (COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 + 999)
#define INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS (COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS + 100)

/* Arguments to pass to the destroy cache function. */
#define CACHE_TYPE_PERSISTENT 1
#define CACHE_TYPE_NONPERSISTENT 2
#define CACHE_TYPE_INVALID 3

/* Forward declaration of the iterator callback function. */
static jint JNICALL validateSharedCacheInfo(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data);

/* The JVMTI extension functions that will be used. */
static jvmtiExtensionFunction iterateSharedCacheFunction = NULL;
static jvmtiExtensionFunction destroySharedCacheFunction = NULL;

static int foundCacheCount = 0;

/* Options parsed from the command line. */
static int expectedCacheCount = -1;
static jboolean useCommandLineValues = JNI_FALSE;
static jboolean deleteCaches = JNI_FALSE;
static char *cachePrefix = NULL; /* All caches created by the test should start with this. */
static const jbyte *cacheDir = NULL;
static char *cacheDirPrintable = NULL; /* The name of the cache dir in appropriate ASCII/EBCDIC for printing. */

#if defined(ZOS)
#pragma convlit(suspend)
#endif /* defined(ZOS) */

/* All message strings are defined here, as on z/OS they need to
 * be kept as EBCDIC, rather than converted to ASCII, as all
 * other strings are.
 */
static char *msg1  = "Iterating over cache number %d";
static char *msg2  = "Cache name is: %s";
static char *msg3  = "Cache is persistent";
static char *msg4  = "Cache is non-persistent";
static char *msg5  = "Delete caches was specified";
static char *msg6  = "No prefix is specified, will attempt to delete cache";
static char *msg7  = "Prefix was specified and cache name matches. Cache will be deleted";
static char *msg8  = "Prefix was specified and cache name doesn't match. Cache will not be deleted";
static char *msg9  = "Attempting to delete cache";
static char *msg10 = "Cache appears to have been deleted successfully";
static char *msg11 = "Cache was not deleted. return code was %d";
static char *msg12 = "Finshed with cache %d\n";
static char *msg13 = "Querying all caches in directory specified via -Xshareclasses option";
static char *msg14 = "cacheDir option passed to the agent should be ignored: %s";
static char *msg15 = "Querying all caches in directory %s";
static char *msg16 = "Failed to get extension functions";
static char *msg17 = "Failed to deallocate extensionFunctions structure";
static char *msg18 = "ERROR: Failed to find jvmti extension function to iterate over caches";
static char *msg19 = "ERROR: Failed to find jvmti extension function to delete caches";
static char *msg20 = "iterateSharedCacheFunction: invalid version number test failed \n";
static char *msg21 = "iterateSharedCacheFunction: invalid flags field test failed \n";
static char *msg22 = "iterateSharedCacheFunction: iteration of shared caches failed \n";
static char *msg23 = "Finshed iterating over the caches, total found was %d";
static char *msg24 = "Correct number of caches found. Expected at least %d caches, found %d. ";
static char *msg25 = "Incorrect number of caches found. Expected at least %d, found %d";
static char *msg26 = "Starting iterating over caches from VM Init event\n";
static char *msg27 = "Iterating over caches: ERROR";
static char *msg28 = "Iterating over caches: SUCCESS";
static char *msg29 = "No value found for option '%s'";
static char *msg30 = "Parsing option string '%s'";
static char *msg31 = "The default cache directory will be used";
static char *msg32 = "cache Directory to be searched is '%s'";
static char *msg33 = "Invalid value specified for expectedCacheCount. Expected an integer, got '%s'";
static char *msg34 = "Number of caches found will not be checked";
static char *msg35 = "Number of caches found will be checked. Expected value is %d";
static char *msg36 = "Command line values for the shared classes cache directory will be used";
static char *msg37 = "Command line values for the shared classes cache directory will be ignored";
static char *msg38 = "Will attempt to delete the caches after querying them";
static char *msg39 = "Caches will not be deleted after querying them";
static char *msg40 = "Cache prefix was specified as: '%s'. Only caches with this prefix will be deleted";
static char *msg41 = "No Cache prefix was specified. All caches will be deleted";
static char *msg42 = "onload processing complete\n\n";
static char *msg43 = "Unexpected return code '%d' from '%s'\n\n";

#if defined(ZOS)
#pragma convlit(resume)
#endif /* defined(ZOS) */

/* Returns a newly allocated copy of original. If length is
 * -1, then, copies the whole of original. If length is > 0
 * and less then length of original, then only that number
 * of chars will be copied. The length of the returned string
 * will be 'length' + the null terminator.
 *
 * Returns NULL if malloc fails.
 */
char *
copyString(char *original, int length)
{
	char *newCopy = NULL;
	if (-1 == length) {
		newCopy = malloc(strlen(original) + 1);
		if (NULL == newCopy) {
			return NULL;
		}
		strcpy(newCopy, original);
		return newCopy;
	}

	newCopy = malloc(length + 1);
	if (NULL == newCopy) {
		return NULL;
	}

	strncpy(newCopy, original, length);
	newCopy[length] = '\0';
	return newCopy;
}

/* This should be called as a pair with freeConvertedString.
 *
 * Takes an ASCII string.
 * On Z/OS, copies the string, converts to EBCDIC, and returns
 * the new, copied string.
 *
 * On other platforms, returns the passed in string.
 */
char *
copyAndConvert(char *asciiString)
{
#if defined(ZOS)
	char *copiedString = copyString(asciiString, -1);
	if (NULL != copiedString) {
		__atoe(copiedString);
	}
	return copiedString;
#else
	return asciiString;
#endif /* defined(ZOS) */
}

/* This should be called after copyAndConvert to free the copied
 * string.
 *
 * On Z/OS will free the passed string.
 * On other platforms, this is a no-op.
 */
void
freeConvertedString(char *ebcdicString)
{
#if defined(ZOS)
	free(ebcdicString);
#endif /* defined(ZOS) */
}

/* Prints messages with a timestamp.
 */
void
iteratorLogger(const char *msg, ...)
{
	va_list args;
	time_t now;
	char *formattedTime = NULL;

	time(&now);
	formattedTime = ctime(&now);

#if defined(ZOS)
#pragma convlit(suspend) /* Need this for the printf format strings below. */
#endif /* defined(ZOS) */

	va_start(args, msg);
	printf("%s", formattedTime);
	vprintf(msg, args);
	printf("\n");
	va_end(args);

#if defined(ZOS)
#pragma convlit(resume)
#endif /* defined(ZOS) */
}

/* Call back for iterating over the shared classes caches.
 */
static jint JNICALL
validateSharedCacheInfo(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data)
{
	jboolean deleteThis = JNI_FALSE;
	foundCacheCount++;

	iteratorLogger(msg1, foundCacheCount);
	{
		char *convertedName = copyAndConvert((char *)cache_info->name);
		iteratorLogger(msg2, convertedName);
		freeConvertedString(convertedName);
	}

	if (cache_info->isPersistent) {
		iteratorLogger(msg3);
	} else {
		iteratorLogger(msg4);
	}

	/* Work out if the cache should be deleted. */
	if (deleteCaches) {
		iteratorLogger(msg5);
		/* This cache should be deleted if no prefix is specified or the cache name matches the prefix. */
		if (NULL == cachePrefix) {
			iteratorLogger(msg6);
			deleteThis = JNI_TRUE;
		} else {
			if (strstr(cache_info->name, cachePrefix) == cache_info->name) {
				iteratorLogger(msg7);
				deleteThis = JNI_TRUE;
			} else {
				iteratorLogger(msg8);
			}
		}
	}

	if (deleteThis) {

		jvmtiError err = JVMTI_ERROR_NONE;
		jint cacheType = 0;
		jint errorCode = 0;

		iteratorLogger(msg9);
		if (cache_info->isPersistent) {
			cacheType = (jint)CACHE_TYPE_PERSISTENT;
		} else {
			cacheType = (jint)CACHE_TYPE_NONPERSISTENT;
		}
		err = destroySharedCacheFunction(jvmti,
				cacheDir,
				cache_info->name,
				cacheType,
				useCommandLineValues,
				&errorCode);
		if (JVMTI_ERROR_NONE == err) {
			iteratorLogger(msg10);
		} else {
			/* This is not an error, as caches that are in use can't always be deleted. */
			iteratorLogger(msg11, errorCode);
		}
	}

	iteratorLogger(msg12, foundCacheCount);
	return JNI_OK;
}

/* Iterate over all the shared classes caches using the JVMTI
 * extension function. Uses the cacheDir in the static cacheDir
 * variable.
 */
int
iterateOverCaches(jvmtiEnv *jvmti_env)
{
	jint extensionCount = 0;
	jvmtiExtensionFunctionInfo *extensionFunctions = NULL;
	jvmtiError err = JVMTI_ERROR_NONE;
	int i = 0;
	jint flags = 0;

	if (useCommandLineValues) {
		iteratorLogger(msg13);
		iteratorLogger(msg14, cacheDirPrintable);
	} else {
		iteratorLogger(msg15, cacheDirPrintable);
	}

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if (JVMTI_ERROR_NONE != err) {
		iteratorLogger(msg16);
		return -1;
	}

	for (i = 0; i < extensionCount; i++) {
		if (0 == strcmp(extensionFunctions[i].id, COM_IBM_ITERATE_SHARED_CACHES)) {
			iterateSharedCacheFunction = extensionFunctions[i].func;
		}
		if (0 == strcmp(extensionFunctions[i].id, COM_IBM_DESTROY_SHARED_CACHE)) {
			destroySharedCacheFunction = extensionFunctions[i].func;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensionFunctions);
	if (JVMTI_ERROR_NONE != err) {
		iteratorLogger(msg17);
		return -1;
	}

	if (NULL == iterateSharedCacheFunction) {
		iteratorLogger(msg18);
		return -1;
	}

	if (NULL == destroySharedCacheFunction) {
		iteratorLogger(msg19);
		return -1;
	}

	/* Check that iterate fails if an invalid version is supplied. */
	err = iterateSharedCacheFunction(jvmti_env,
			INVALID_COM_IBM_ITERATE_SHARED_CACHES_VERSION,
			cacheDir,
			flags,
			useCommandLineValues,
			validateSharedCacheInfo,
			NULL);
	if (JVMTI_ERROR_UNSUPPORTED_VERSION != err) {
		iteratorLogger(msg20);
		return -1;
	}

	/* Check that iterate fails if an invalid flags field is supplied. */
	err = iterateSharedCacheFunction(jvmti_env,
			COM_IBM_ITERATE_SHARED_CACHES_VERSION_1,
			cacheDir,
			INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS,
			useCommandLineValues,
			validateSharedCacheInfo,
			NULL);
	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		iteratorLogger(msg21);
		return -1;
	}

	/* Run the real iteration step with valid arguments. */
	err = iterateSharedCacheFunction(jvmti_env,
			COM_IBM_ITERATE_SHARED_CACHES_VERSION_1,
			cacheDir,
			flags,
			useCommandLineValues,
			validateSharedCacheInfo,
			NULL);
	if (JVMTI_ERROR_NONE != err) {
		iteratorLogger(msg22);
		return -1;
	}

	iteratorLogger(msg23, foundCacheCount);
	if (-1 != expectedCacheCount) {
		if (foundCacheCount >= expectedCacheCount) {
			iteratorLogger(msg24, expectedCacheCount, foundCacheCount);
		} else {
			iteratorLogger(msg25, expectedCacheCount, foundCacheCount);
			return -1;
		}
	}

	return 0;
}

/* Callback for the VM init event.
 * Calls the cache iterator function.
 */
void JNICALL
VMInitCallback(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread)
{
	int rv = 0;

	iteratorLogger(msg26);

	rv = iterateOverCaches(jvmti_env);

	if (0 != rv) {
		iteratorLogger(msg27);
	} else {
		iteratorLogger(msg28);
	}
}

/* Returns a newly malloced NULL terminated string containing the
 * option value. Assumes the option string is in the form:
 * optionName1=value1,optionName2=value2,....
 *
 * The option string is assumed to come from the JVM and hence
 * UTF-8, so optionName should also be UTF-8.
 *
 * Returns NULL if the option is not found or has no value.
 */
char *
findOption(const char *optionString, char *optionName)
{
	char *valueStart = NULL;
	size_t remainingLength = 0;
	size_t optionNameLength = 0;
	char *trailingCommaPos = NULL;
	int valueLength = 0;
	char *optionStart = strstr(optionString, optionName);

	if (NULL == optionStart) {
		char *convertedName = copyAndConvert(optionName);
		iteratorLogger(msg29, convertedName);
		freeConvertedString(convertedName);
		return NULL;
	}

	/* The option name exists, need to check whether it is
	 * in the middle of another string, is really an option
	 * i.e. needs to be ',optionName='.
	 */

	if (optionStart != optionString) {
		/* This is not the first option. We need to check for a preceding comma. */
		char *leadingCommaPosition = optionStart - 1;
		if (',' != *leadingCommaPosition) {
			/* There is no leading comma, carry on looking further into the string. */
			return findOption(optionStart + 1, optionName);
		}
	}

	/* The name is at the start of the option string or is
	 * preceded by a comma. Check that it is followed by '='
	 * and a value.
	 */
	optionNameLength = strlen(optionName);
	remainingLength = strlen(optionStart);
	if (remainingLength <= (optionNameLength + 1)) {

		/* There isn't enough space for an equals sign and a value after the option name.
		 * The value must be blank, or this isn't correctly formatted. Look further
		 * down the option string.
		 */
		return findOption(optionStart + 1, optionName);
	}

	/* We have the start of the option name, and there are at least two characters after it
	 * hopefully '=' and a value.
	 */
	valueStart = optionStart + optionNameLength;
	if ('=' != *valueStart) {
		/* The next char isn't an '='. Carry on looking. */
		return findOption(optionStart + 1, optionName);
	}
	valueStart++;

	/* The start position of the option name is known, and it is preceded by a comma
	 * (or is at the start of the option string). It has an '=' after it and at least one more
	 * char, so there is a valid value for this option.
	 * The value is the rest of the string till the next comma or the end of the string.
	 */
	trailingCommaPos = strchr(valueStart, ',');
	if (NULL == trailingCommaPos) {
		/* No more commas, the value is the whole thing. */
		return copyString(valueStart, -1);
	}

	valueLength = (int)(trailingCommaPos - valueStart);
	return copyString(valueStart, valueLength);
}

/* Parse the options that were passed to this agent on the
 * command line. The options are received via the JVM
 * and hence expected to be a UTF-8 string.
 */
void
parseOptions(const char *options)
{
	char *expectedCacheString = NULL;
	char *useCommandLineValuesString = NULL;
	char *deleteCachesString = NULL;

	{
		char *convertedOptions = copyAndConvert((char *)options);
		iteratorLogger(msg30, convertedOptions);
		freeConvertedString(convertedOptions);
	}

	/* Option 'cacheDir'. Should be a string path. If missing, then the default will be used. */
	cacheDir = (jbyte *)findOption(options, "cacheDir");
	if (NULL == cacheDir) {
		iteratorLogger(msg31);
	} else {
		cacheDirPrintable = copyAndConvert((char *)cacheDir);
		iteratorLogger(msg32, cacheDirPrintable);
	}

	/* Option 'expectedCacheCount'. Should be an int. If missing, this won't be checked. */
	expectedCacheString = findOption(options, "expectedCacheCount");
	if (NULL != expectedCacheString) {

		char *convertedExpectedCacheCount = copyAndConvert(expectedCacheString);
#if defined(ZOS)
#pragma convlit(suspend) /* Needed for the format string in scanf. */
#endif /* defined(ZOS) */
		int success = sscanf(convertedExpectedCacheCount, "%d", &expectedCacheCount);
#if defined(ZOS)
#pragma convlit(resume)
#endif /* defined(ZOS) */

		if (1 != success) {
			/* Failed to parse an integer. */
			iteratorLogger(msg33, convertedExpectedCacheCount);
			expectedCacheCount = -1;
		}

		freeConvertedString(convertedExpectedCacheCount);
	}

	if (-1 == expectedCacheCount) {
		iteratorLogger(msg34);
	} else {
		iteratorLogger(msg35, expectedCacheCount);
	}

	/* Option 'useCommandLineValues'. Look for the exact string 'true'. Anything else is false.
	 * If set, the cacheDir option will be ignored, and the JVM will be asked to used the value
	 * from the -Xshareclasses option for the cache directory.
	 */
	useCommandLineValuesString = findOption(options, "useCommandLineValues");
	if (NULL != useCommandLineValuesString) {
		if (0 == strcmp(useCommandLineValuesString, "true")) {
			useCommandLineValues = JNI_TRUE;
		}
	}
	if (useCommandLineValues) {
		iteratorLogger(msg36);
	} else {
		iteratorLogger(msg37);
	}

	/* Option 'deleteCaches'. If set to the string 'true', ask the JVM to delete the caches
	 * after the iterator has queried them.
	 */
	deleteCachesString = findOption(options, "deleteCaches");
	if (NULL != deleteCachesString) {
		if (0 == strcmp(deleteCachesString, "true")) {
			deleteCaches = JNI_TRUE;
		}
	}
	if (deleteCaches) {
		iteratorLogger(msg38);
	} else {
		iteratorLogger(msg39);
	}

	/* Option 'cachePrefix'. If set, only caches whose name start with prefix will
	 * be deleted. Necessary when working with the default cache directory so as
	 * other processes caches aren't deleted.
	 */
	cachePrefix = findOption(options, "cachePrefix");
	if (NULL != cachePrefix) {
		char *convertedPrefix = copyAndConvert(cachePrefix);
		iteratorLogger(msg40, convertedPrefix);
		freeConvertedString(convertedPrefix);
	} else {
		iteratorLogger(msg41);
	}
}

/* JVMTI agent initialisation function, invoked as agent is loaded by the JVM.
 * Parse the agent's options, and sets up a callback for the VM
 * init event, where all the work is done.
 */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *jvm, char *options, void *reserved)
{
	jvmtiEnv *jvmti = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
	jvmtiEventCallbacks eventCallbacks;

	parseOptions(options);

	/* Get access to JVMTI. */
	(*jvm)->GetEnv(jvm, (void **)&jvmti, JVMTI_VERSION_1_0);

	/* Setup the callback function for VM init. */
	memset(&eventCallbacks, 0, sizeof(eventCallbacks));
	eventCallbacks.VMInit = &VMInitCallback;
	rc = (*jvmti)->SetEventCallbacks(jvmti, &eventCallbacks, sizeof(eventCallbacks));

	/* Enable callback event for VM init. */
	rc = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);
	if (JVMTI_ERROR_NONE == rc) {
		iteratorLogger(msg42);
		return JNI_OK;
	} else {
		iteratorLogger(msg43, rc, "SetEventNotificationMode");
		return rc;
	}

	return JNI_OK;
}
