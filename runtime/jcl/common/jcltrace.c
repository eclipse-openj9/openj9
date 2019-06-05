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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "jclprots.h"
#include "jclglob.h"
#include "j9protos.h"

#define UT_TRACE_VERSION 7
#define MAX_APPLICATION_NAME_CHARS 256 /*ibm@94275*/

/* ibm@94077 starts */
#define TRACEDOTCGLOBAL(x) JCL_CACHE_GET(env, traceGlobals).x

/* Argument type codes. These are combined to build a word describing the
 * expected argument for a particular trace point.
 */

#define JWORD 0x01
#define JDOUBLEWORD 0x02
#define JFLOATPOINT 0x04
#define JCHAR 0x08
#define JSTRING 0x10
#define JOBJECT 0x20
/*Pointers can be passed in as objects or double words*/
#define JPOINTER (JOBJECT | JDOUBLEWORD)

static I_32 CompareAndSwap32(volatile U_32 *target, U_32 old, U_32 new32);

static I_32 CompareAndSwapPtr(volatile UDATA *target, UDATA old, UDATA newptr);

/**
 * Thread-safe array list used for mapping handles to module info structures
 */
struct ArrayList {
	UDATA slabSize;
	void ** header;
};

/*Data type function prototypes*/
/**
 * Allocates new array list
 *
 * @param slabSize Size of each array slab
 * @return Pointer to a new array list or NULL if there was a problem
 */
static struct ArrayList *allocArrayList(JNIEnv *const env, const UDATA slabSize);

/**
 * Inserts an element into the array list
 *
 * @param list Array list to insert into
 * @param index Location to put value
 * @param value Value to insert
 * @return 0 on success, non-zero on failure
 *
 */
static IDATA arrayListPut(JNIEnv *const env, const struct ArrayList *const list, const UDATA index, void *const value);

/**
 * Gets an element from the array list
 *
 * @param list Array list to get from
 * @param index Location to get
 * @return Value at that position or NULL if that position is not filled
 */
static void *arrayListGet(JNIEnv *const env, const struct ArrayList *const list, const UDATA index);

/* Deep frees a modInfo structure */
static void freeModInfo(JNIEnv *const env, UtModuleInfo *toFree);

/*Size of the slabs used to build array lists*/
#define SLAB_SIZE 10

#define ERROR_MESSAGE_BUFFER_LENGTH 256

/* Macros for building argument descriptors (magic values describing a sequence of arguments) for the application trace methods.
 * This has been done as macros rather than as a function so the work can be done at compile time.
 */
#define ARGUMENT_DESCRIPTOR() 0
#define ARGUMENT_DESCRIPTOR_1(x) x
#define ARGUMENT_DESCRIPTOR_2(x, y) ((x << 8) | y)
#define ARGUMENT_DESCRIPTOR_3(x, y, z) ((x << 16) | (y << 8) | z)

static void
throwNewThrowable(JNIEnv *const env, const char *const exceptionClassName, const char *const message)
{
	jclass exceptionClass = (*env)->FindClass(env, exceptionClassName);

	if (NULL == exceptionClass) {
		return;
	}

	(*env)->ThrowNew(env, exceptionClass, message);
}

static void
throwIllegalArgumentException(JNIEnv *const env, const char *const message)
{
	throwNewThrowable(env, "java/lang/IllegalArgumentException", message);
}

static void
throwRuntimeException(JNIEnv *const env, const char *const message)
{
	throwNewThrowable(env, "java/lang/RuntimeException", message);
}

/* Terminates the trace structures. Must not be called while it's still possible for an application to call register */
void
terminateTrace(JNIEnv *env)
{
	int lastIndex = 0;
	void **slab = NULL;

	PORT_ACCESS_FROM_ENV(env);

	if ((NULL == TRACEDOTCGLOBAL(utIntf)) || (NULL == TRACEDOTCGLOBAL(utIntf)->server)) {
		/* nothing to be done without the interfaces, so bail */
		return;
	}

	/* null the app trace handle count so that any attempts to trace will fail and save the tail index
	 * to iterate with
	 */
	do {
		lastIndex = TRACEDOTCGLOBAL(numberOfAppTraceApplications);
	} while (! CompareAndSwap32(&TRACEDOTCGLOBAL(numberOfAppTraceApplications), lastIndex, 0));

	/* free the modInfo and argument structures */
	for (;lastIndex > 0; lastIndex--) {
		UtModuleInfo *modInfo = (UtModuleInfo *)arrayListGet(env, TRACEDOTCGLOBAL(modInfoList), lastIndex);
		UDATA **callPatternsArray = (UDATA **)arrayListGet(env, TRACEDOTCGLOBAL(argumentStructureList), lastIndex);

		assert((NULL != modInfo) && (NULL != callPatternsArray));

		freeModInfo(env, modInfo);
		j9mem_free_memory(callPatternsArray);
	}

	/* free the modInfo list */
	slab = TRACEDOTCGLOBAL(modInfoList)->header;
	while (NULL != slab) {
		void **nextSlab = slab[TRACEDOTCGLOBAL(modInfoList)->slabSize];
		j9mem_free_memory(slab);
		slab = nextSlab;
	}
	j9mem_free_memory(TRACEDOTCGLOBAL(modInfoList));

	/* free the argument structures list */
	slab = TRACEDOTCGLOBAL(argumentStructureList)->header;
	while (NULL != slab) {
		void **nextSlab = slab[TRACEDOTCGLOBAL(argumentStructureList)->slabSize];
		j9mem_free_memory(slab);
		slab = nextSlab;
	}
	j9mem_free_memory(TRACEDOTCGLOBAL(argumentStructureList));
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_initTrace
 * description - Obtain system properties and initialize trace for JAVA.DLL
 * parameters  - JNIEnv, this
 * returns     - none
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_initTraceImpl(JNIEnv *env, jobject this)
{
	int rc = 0;
	JavaVM *vm = NULL;

	TRACEDOTCGLOBAL(rasIntf) = NULL;
	TRACEDOTCGLOBAL(utIntf) = NULL;
	TRACEDOTCGLOBAL(numberOfAppTraceApplications) = 0;

	rc = (*env)->GetJavaVM(env, &vm);
	if (JNI_OK == rc) {
		/* Get RAS interface (Might not be available if trace not loaded) */
		if ((JNI_OK == (*vm)->GetEnv(vm, (void **)&TRACEDOTCGLOBAL(rasIntf), JVMRAS_VERSION_1_3)) &&
			(JNI_OK == (*vm)->GetEnv(vm, (void **)&TRACEDOTCGLOBAL(utIntf), UTE_VERSION_1_1))) {
			TRACEDOTCGLOBAL(modInfoList) = allocArrayList(env, SLAB_SIZE);
			TRACEDOTCGLOBAL(argumentStructureList) = allocArrayList(env, SLAB_SIZE);

			if ((NULL == TRACEDOTCGLOBAL(modInfoList)) || (NULL == TRACEDOTCGLOBAL(argumentStructureList))) {
				/*Exception will have been thrown*/
				TRACEDOTCGLOBAL(utIntf) = NULL;
				TRACEDOTCGLOBAL(rasIntf) = NULL;
			}
		} else {
			TRACEDOTCGLOBAL(utIntf) = NULL;
			TRACEDOTCGLOBAL(rasIntf) = NULL;
			/* No option but to return silently */
			return;
		}
	}
	return;
}

/*
 * Extracts UTF chars from string object and checks length against MAX_APPLICATION_NAME_CHARS
 *
 * @param *env [in] JNI environment
 * @param name [in] String object
 * @param **applicationName [out] Pointer to pointer that will be assigned with GetStringUTFChars
 */
static UDATA
processAndCheckNameString(JNIEnv *env, jobject name, const char **applicationName) {
	*applicationName = (*env)->GetStringUTFChars(env, name, NULL);

	if (NULL == *applicationName) {
		return 1;
	}

	/* Check the application name does not exceed MAX_APPLICATION_NAME_CHARS */
	if (strlen(*applicationName) > MAX_APPLICATION_NAME_CHARS) {
		char messageBuffer[ERROR_MESSAGE_BUFFER_LENGTH + 1];
		PORT_ACCESS_FROM_ENV(env);

		memset(messageBuffer, 0, sizeof(messageBuffer));
		j9str_printf(PORTLIB, messageBuffer, ERROR_MESSAGE_BUFFER_LENGTH, "Application name is too long. Maximum length %d characters, supplied string was %d characters.\n", MAX_APPLICATION_NAME_CHARS, strlen(*applicationName));
		throwIllegalArgumentException(env, messageBuffer);
		return 2;
	}

	return 0;
}

/*
 * A strcpy that won't copy beyond the end pointer
 *
 * @param **p [inout] Pointer to buffer to write out in
 * @param *from [in] Buffer to copy from (must be NULL terminated
 * @param end [in] Pointer to end of buffer *p
 */
static void
guardedStrcpy(char **p, const char *const from, const char *const end)
{
	const char *fromCursor = from;

	while ((*p < end) && (0 != ((**p) = *fromCursor))) {
		(*p)++;
		fromCursor++;
	}
}

/*
 * Formats a human readable version of a call pattern (as created by buildCallPattern).
 *
 * @param p [in] Pointer to buffer to hold the call pattern
 * @param end [in] Pointer to end of buffer (we will not write past it)
 * @param pattern [in] The pattern to format
 */
static void
formatCallPattern(char *p, char *const end, const UDATA pattern)
{
	IDATA args = 0;
	IDATA shift = 3 * 8;

	for (; shift >= 0; shift -= 8) {
		UDATA thisPattern = (pattern >> shift) & 0xFF;
		const char *humanReadablePattern = "unknown";

		switch (thisPattern) {
		case (0) :
			continue;
		case (JWORD) :
			humanReadablePattern = "word(byte/short/int)";
			break;
		case (JDOUBLEWORD) :
			humanReadablePattern = "doubleword(long)";
			break;
		case (JOBJECT) :
			humanReadablePattern = "object";
			break;
		case (JPOINTER) :
			humanReadablePattern = "pointer(object/doubleword)";
			break;
		case (JFLOATPOINT) :
			humanReadablePattern = "float/double";
			break;
		case (JCHAR) :
			humanReadablePattern = "char";
			break;
		case (JSTRING) :
			humanReadablePattern = "string";
			break;
		default:
			/*Should be impossible*/
			assert(0);
			break;
		}

		if (end <= p) {
			/* no space left */
			break;
		}

		if (args != 0) {
			/* a comma separator from the previous pattern */
			*p = ',';
			p++;
		}

		guardedStrcpy(&p, humanReadablePattern, end);
		args += 1;
	}

	/* nul-terminate the string */
	*p = '\0';
}

/**
 * State type for printf parsing (see buildCallPattern).
 */
enum PatternParseState { FINDING_PERCENT, SKIPPING_PRECISION, READING_MODIFIER, READING_TYPE };

/*
 * Reads a printf-style template (of up to 4 placeholders) and stores a value
 * representing the argument sequence to *result.
 *
 * @param templateString [in] printf template string
 * @param result [out] Pointer to output word
 */
static void
buildCallPattern(const char *const templateString, UDATA *const result)
{
	int longModifierCount = 0;
	const char *p = templateString;
	enum PatternParseState state = FINDING_PERCENT;

	*result = 0;

	while (0 != *p) {
		switch (state) {
		case FINDING_PERCENT:
			if ('%' == *p) {
				longModifierCount = 0;
				state = SKIPPING_PRECISION;
			}
			break;
		case SKIPPING_PRECISION:
			/*In this state we skip any 0-9 or .*/
			if ('l' == *p) {
				/*Reparse this letter in the READING_MODIFIER state*/
				state = READING_MODIFIER;
				continue;
			} else if (('.' != *p) && !isdigit(*p)) {
				state = READING_TYPE;
				continue;
			}
			break;
		case READING_MODIFIER:
			if ('l' == *p) {
				longModifierCount++;
			} else {
				state = READING_TYPE;
				continue;
			}
			break;
		case READING_TYPE:
		{
			unsigned char typeCode = 0;
			switch (tolower(*p)) {
			case 'c':
				typeCode = JCHAR;
				break;
			case 'd':
			case 'x':
			case 'i':
			case 'u':
				if (longModifierCount > 1) {
					typeCode = JDOUBLEWORD;
				} else {
					typeCode = JWORD;
				}
				break;
			case 'p':
				typeCode = JPOINTER;
				break;
			case 's':
				typeCode = JSTRING;
				break;
			case 'f':
			case 'g':
			case 'e':
				typeCode = JFLOATPOINT;
				break;
			default:
				/*Unknown placeholder. Ignored by printf*/
				break;
			}

			if (0 != typeCode) {
				*result = (*result << 8) | typeCode;
			}

			state = FINDING_PERCENT;
			break;
		}
		default:
			/*This should be impossible*/
			assert(0);
			break;
		}

		p++;
	}
}

/*
 * Checks whether a character is a valid type character (one of ENTRY, EXIT, EVENT, EXCEPTION, EXIT_EXCEPTION)
 */
static BOOLEAN
isValidTypeChar(char character)
{
	return ('0' == character)
		|| ('1' == character)
		|| ('2' == character)
		|| ('3' == character)
		|| ('4' == character)
		|| ('5' == character);
}

/**
 * Processes the template strings provided to registerApplication()
 *
 * @param *env [in] JNI environment
 * @param templates [in] Array containing templates
 * @param formatStringsArray [out] Pointer to array of strings. Assigned by this method and populated with the UTF version of templates
 * @param callPatternsArray [out] Pointer to array of UDATA. Assigned by this method and populated with the call patterns for the templates.
 * @param tracePointCount [out] Pointer to UDATA to be set to number of trace points (size of templates array)
 */
static UDATA
extractAndProcessFormatStrings(JNIEnv *env, jarray templates, char ***const formatStringsArray, UDATA **const callPatternsArray, UDATA *const tracePointCount)
{
	UDATA i = 0;
	UDATA rc = 0;
	J9InternalVMFunctions *vmFuncs = ((J9VMThread *)env)->javaVM->internalVMFunctions;

	PORT_ACCESS_FROM_ENV(env);

	*tracePointCount = (*env)->GetArrayLength(env, templates);

	if (NULL != (*env)->ExceptionOccurred(env)) {
		return 1;
	}

	/*Allocate the two arrays. The formatStringsArray is passed to the trace engine which expects an extra NULL field at the end*/
	*formatStringsArray = (char **)j9mem_allocate_memory(sizeof(char *) * (*tracePointCount + 1), J9MEM_CATEGORY_VM_JCL);

	if (NULL == *formatStringsArray) {
		vmFuncs->throwNativeOOMError(env, 0, 0);
		rc = 2;
		goto fail;
	}

	memset(*formatStringsArray, 0, sizeof(char *) * (*tracePointCount + 1));

	*callPatternsArray = (UDATA *) j9mem_allocate_memory(sizeof(UDATA) * *tracePointCount, J9MEM_CATEGORY_VM_JCL);

	if (NULL == *callPatternsArray) {
		vmFuncs->throwNativeOOMError(env, 0, 0);
		rc = 2;
		goto fail;
	}

	memset(*callPatternsArray, 0, sizeof(UDATA) * *tracePointCount);

	for (i = 0; i < *tracePointCount; i++) {
		jobject thisString = (*env)->GetObjectArrayElement(env, templates, (jsize)i);
		const char *jniStringCharacters = NULL;

		if (NULL != (*env)->ExceptionOccurred(env)) {
			rc = 3;
			goto fail;
		}

		if (NULL == thisString) {
			throwIllegalArgumentException(env, "Null string passed as trace template");
			rc = 4;
			goto fail;
		}

		jniStringCharacters = (*env)->GetStringUTFChars(env, thisString, NULL);

		if (NULL == jniStringCharacters) {
			rc = 5;
			goto fail;
		}

		if (!isValidTypeChar(jniStringCharacters[0])) {
			char messageBuffer[ERROR_MESSAGE_BUFFER_LENGTH + 1];

			memset(messageBuffer, 0, sizeof(messageBuffer));
			j9str_printf(PORTLIB, messageBuffer, ERROR_MESSAGE_BUFFER_LENGTH, "Error: template %d does not have a valid trace prefix. Trace templates should start with one of Trace.EVENT, Trace.EXIT, Trace.ENTRY, Trace.EXCEPTION or Trace.EXCEPTION_EXIT", i);
			throwIllegalArgumentException(env, messageBuffer);
		}

		/*Dupe the string into the array*/
		(*formatStringsArray)[i] = (char *)j9mem_allocate_memory(strlen(jniStringCharacters) + 1, J9MEM_CATEGORY_VM_JCL);

		if (NULL == (*formatStringsArray)[i]) {
			vmFuncs->throwNativeOOMError(env, 0, 0);
			(*env)->ReleaseStringUTFChars(env, thisString, jniStringCharacters);
			rc = 6;
			goto fail;
		}

		strcpy((*formatStringsArray)[i], jniStringCharacters);

		buildCallPattern((*formatStringsArray)[i], &((*callPatternsArray)[i]));

		(*env)->ReleaseStringUTFChars(env, thisString, jniStringCharacters);
		(*env)->DeleteLocalRef(env, thisString);
	}

	return 0;

fail:

	if (NULL != *formatStringsArray) {
		for (i = 0; i < *tracePointCount; i++) {
			char *formatString = (*formatStringsArray)[i];

			if (NULL != formatString) {
				j9mem_free_memory(formatString);
			}
		}

		j9mem_free_memory(*formatStringsArray);
	}

	if (NULL != *callPatternsArray) {
		j9mem_free_memory(*callPatternsArray);
	}

	return rc;
}

/*
 * Frees a modinfo structure.
 */
static void
freeModInfo(JNIEnv *const env, UtModuleInfo *toFree)
{
	PORT_ACCESS_FROM_ENV(env);

	if (NULL == toFree) {
		return;
	}

	if (NULL != toFree->name) {
		j9mem_free_memory(toFree->name);
	}

	if (NULL != toFree->active) {
		j9mem_free_memory(toFree->active);
	}

	if (NULL != toFree->traceVersionInfo) {
		j9mem_free_memory(toFree->traceVersionInfo);
	}

	if (NULL != toFree->levels) {
		j9mem_free_memory(toFree->levels);
	}

	j9mem_free_memory(toFree);
}

/**
 * Builds and populates the UtModuleInfo structure to register with the trace engine.
 *
 * @param env [in] JNI environment
 * @param japplicationName [in] UTF version of application name, from GetStringUTFChars
 * @param tracePointCount [in] Number of trace points
 * @return Populated UTModuleInfo structure or NULL if there was a problem.
 */
static UtModuleInfo *
buildModInfo(JNIEnv *const env, const char *const applicationName, const UDATA tracePointCount)
{
	UtModuleInfo *toReturn = NULL;
	UDATA arraySize = sizeof(unsigned char) * tracePointCount;
	J9InternalVMFunctions *vmFuncs = ((J9VMThread *)env)->javaVM->internalVMFunctions;

	PORT_ACCESS_FROM_ENV(env);

	toReturn = (UtModuleInfo*) j9mem_allocate_memory( sizeof(UtModuleInfo), J9MEM_CATEGORY_VM_JCL);
	if (NULL == toReturn) {
		goto error;
	}

	memset(toReturn, 0, sizeof(UtModuleInfo));

	/*Duplicate the application name*/
	toReturn->namelength = (I_32)strlen(applicationName);
	toReturn->name = j9mem_allocate_memory( toReturn->namelength + 1, J9MEM_CATEGORY_VM_JCL);
	if (NULL == toReturn->name) {
		vmFuncs->throwNativeOOMError(env, 0, 0);
		goto error;
	}
	strcpy(toReturn->name, applicationName);

	toReturn->count = (I_32)tracePointCount;

	toReturn->moduleId = UT_APPLICATION_TRACE_MODULE;

	toReturn->active = (unsigned char *)j9mem_allocate_memory( arraySize, J9MEM_CATEGORY_VM_JCL);
	if (NULL == toReturn->active) {
		vmFuncs->throwNativeOOMError(env, 0, 0);
		goto error;
	}

	memset(toReturn->active, 0, arraySize);

	toReturn->traceVersionInfo = (UtTraceVersionInfo *)j9mem_allocate_memory( sizeof(UtTraceVersionInfo), J9MEM_CATEGORY_VM_JCL);
	if (NULL == toReturn->traceVersionInfo) {
		vmFuncs->throwNativeOOMError(env, 0, 0);
		goto error;
	}

	toReturn->traceVersionInfo->traceVersion = UT_TRACE_VERSION;

	toReturn->levels = (unsigned char *)j9mem_allocate_memory( arraySize, J9MEM_CATEGORY_VM_JCL);
	if (NULL == toReturn->levels) {
		vmFuncs->throwNativeOOMError(env, 0, 0);
		goto error;
	}
	memset(toReturn->levels, 3, arraySize);

	return toReturn;

error:

	if (NULL != toReturn) {
		freeModInfo(env, toReturn);
	}

	return NULL;
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_registerApplication
 * description - Set trace options
 * parameters  - JNIEnv, this, trace configuration command
 * returns     - return code
 *************************************************************************/
int JNICALL
Java_com_ibm_jvm_Trace_registerApplicationImpl(JNIEnv *env, jobject this, jobject name, jarray templates)
{
	const char *applicationName = NULL;
	char **formatStringsArray = NULL;
	UDATA *callPatternsArray = NULL;
	UDATA tracePointCount = 0;
	int toReturn = JNI_ERR;
	int err = 0;
	int index = 0;
	int tempIndex = 0;
	UtModuleInfo *modInfo = NULL;

	PORT_ACCESS_FROM_ENV(env);

	/* If we don't have the trace engine, we have to exit. Pass back a valid handle so
	 * application trace becomes a silent no-op.
	 */
	if (NULL == TRACEDOTCGLOBAL(utIntf)) {
		toReturn = 0;
		goto error;
	}

	err = (int)processAndCheckNameString(env, name, &applicationName);

	if (0 != err) {
		/*An exception will have been thrown - we just need to free resources and return*/
		toReturn = JNI_ERR;
		goto error;
	}

	err = (int)extractAndProcessFormatStrings(env, templates, &formatStringsArray, &callPatternsArray, &tracePointCount);

	if (0 != err) {
		/*Exception will have been thrown*/
		toReturn = JNI_ERR;
		goto error;
	}

	modInfo = buildModInfo(env, applicationName, tracePointCount);

	if (NULL == modInfo) {
		toReturn = JNI_ERR;
		goto error;
	}

	/*Register the application with the trace engine*/
	err = TRACEDOTCGLOBAL(utIntf)->server->AddComponent(modInfo, (const char **)formatStringsArray);

	if (0 != err) {
		char messageBuffer[ERROR_MESSAGE_BUFFER_LENGTH + 1];

		memset(messageBuffer, 0, sizeof(messageBuffer));
		j9str_printf(PORTLIB, messageBuffer, ERROR_MESSAGE_BUFFER_LENGTH, "Failed to register application with trace engine: %d", err);
		throwRuntimeException(env, messageBuffer);
		toReturn = JNI_ERR;
		goto error;
	}

	/*Get an index and store the modinfo and callPatternsArray to use later*/
	do {
		tempIndex = TRACEDOTCGLOBAL(numberOfAppTraceApplications);
		index = tempIndex + 1;
	} while (! CompareAndSwap32(&TRACEDOTCGLOBAL(numberOfAppTraceApplications), tempIndex, index));

	toReturn = index;

	err = (int)arrayListPut(env, TRACEDOTCGLOBAL(modInfoList), index, (void*)modInfo);

	if (0 != err) {
		/*Exception will have been thrown*/
		toReturn = JNI_ERR;
		goto error;
	}

	err = (int)arrayListPut(env, TRACEDOTCGLOBAL(argumentStructureList), index, (void*)callPatternsArray);

	if (0 != err) {
		/*Exception will have been thrown*/
		toReturn = JNI_ERR;
		goto error;
	}

error:

	if (0 != err) {
		if (NULL != formatStringsArray) {
			UDATA i = 0;

			for (i = 0; i < tracePointCount; i++) {
				if (NULL != formatStringsArray[i]) {
					j9mem_free_memory(formatStringsArray[i]);
				}
			}

			j9mem_free_memory(formatStringsArray);
		}

		if (NULL != callPatternsArray) {
			j9mem_free_memory(callPatternsArray);
		}

		if (NULL != modInfo) {
			freeModInfo(env, modInfo);
		}
	}


	if (NULL != applicationName) {
		(*env)->ReleaseStringUTFChars(env, name, applicationName);
	}

	return toReturn;
}

/**
 * Counts the number of arguments in a callPattern
 * @param pattern [in] Call pattern
 * @return Number of arguments
 */
static UDATA
countArguments(const UDATA pattern)
{
	return (((pattern >> (3 * 8)) & 0xFF) != 0) ? 4
		 : (((pattern >> (2 * 8)) & 0xFF) != 0) ? 3
		 : (((pattern >> (1 * 8)) & 0xFF) != 0) ? 2
		 : (((pattern >> (0 * 8)) & 0xFF) != 0) ? 1
		 : 0;
}

/**
 * Trace method that all other com.ibm.jvm.Trace.trace() methods call through.
 *
 * @param env JNI environment
 * @param handle Application trace handle (returned from registerApplication)
 * @param traceId Id of trace point to trace
 * @param methodSignature the signature of the method that supplied the arguments
 *
 * Trace arguments are passed by varargs
 */
static void
trace(JNIEnv *const env, const jint handle, const jint traceId, const UDATA methodSignature, ...)
{
	UtModuleInfo *modInfo = NULL;
	UDATA *callPatternsArray = NULL;
	va_list args;
	UDATA numberOfArgumentsExpected = 0;
	UDATA numberOfArgumentsSupplied = 0;

	/*If trace is disabled, bail out*/
	if (NULL == TRACEDOTCGLOBAL(utIntf)) {
		return;
	}

	/*Is the handle valid?*/
	if (handle <= 0 || (U_32)handle > TRACEDOTCGLOBAL(numberOfAppTraceApplications)) {
		throwIllegalArgumentException(env, "Invalid application trace handle. Check return code from registerApplication().");
		return;
	}

	modInfo = (UtModuleInfo *) arrayListGet(env, TRACEDOTCGLOBAL(modInfoList), handle);
	assert(NULL != modInfo);

	callPatternsArray = (UDATA *) arrayListGet(env, TRACEDOTCGLOBAL(argumentStructureList), handle);
	assert(NULL != callPatternsArray);

	/*Is the trace id in the right range?*/
	if (traceId < 0 || traceId >= modInfo->count) {
		char messageBuffer[ERROR_MESSAGE_BUFFER_LENGTH + 1];
		PORT_ACCESS_FROM_ENV(env);

		memset(messageBuffer, 0, sizeof(messageBuffer));
		j9str_printf(PORTLIB, messageBuffer, ERROR_MESSAGE_BUFFER_LENGTH, "Specified tracepoint: %d outside of valid range 0<=x<%d\n", traceId, modInfo->count);
		throwIllegalArgumentException(env, messageBuffer);
		return;
	}

	/*Do we have the expected number of arguments?*/
	numberOfArgumentsExpected = countArguments(callPatternsArray[traceId]);
	numberOfArgumentsSupplied = countArguments(methodSignature);

	if (numberOfArgumentsExpected != numberOfArgumentsSupplied) {
		char messageBuffer[ERROR_MESSAGE_BUFFER_LENGTH + 1];
		PORT_ACCESS_FROM_ENV(env);

		memset(messageBuffer, 0, sizeof(messageBuffer));
		j9str_printf(PORTLIB, messageBuffer, ERROR_MESSAGE_BUFFER_LENGTH, "Wrong number of arguments passed to tracepoint %s.%d expected %d received %d.", modInfo->name, traceId, numberOfArgumentsExpected, numberOfArgumentsSupplied);
		throwIllegalArgumentException(env, messageBuffer);
		return;
	}

	/*Do the arguments match the trace point?*/
	if ((methodSignature & callPatternsArray[traceId]) != methodSignature) {
		char messageBuffer[ERROR_MESSAGE_BUFFER_LENGTH + 1];
		char *p = NULL;
		char *const end = messageBuffer + ERROR_MESSAGE_BUFFER_LENGTH;
		PORT_ACCESS_FROM_ENV(env);

		memset(messageBuffer, 0, sizeof(messageBuffer));
		j9str_printf(PORTLIB, messageBuffer, ERROR_MESSAGE_BUFFER_LENGTH, "Invalid arguments passed to tracepoint %s.%d. Tracepoint expects: ", modInfo->name, traceId);

		p = messageBuffer + strlen(messageBuffer);
		formatCallPattern(p, end, callPatternsArray[traceId]);

		strncat(messageBuffer, " received: ", ERROR_MESSAGE_BUFFER_LENGTH - strlen(messageBuffer));

		p = messageBuffer + strlen(messageBuffer);
		formatCallPattern(p, end, methodSignature);

		strncat(messageBuffer, ".", ERROR_MESSAGE_BUFFER_LENGTH - strlen(messageBuffer));

		throwIllegalArgumentException(env, messageBuffer);
		return;
	}

	va_start(args, methodSignature);

	TRACEDOTCGLOBAL(utIntf)->server->TraceVNoThread(modInfo, (traceId << 8) | modInfo->active[traceId], "", args);

	va_end(args);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_traceImpl
 * description - Make tracepoint
 * parameters  - JNIEnv, this, handle, tracepoint id, optional trace data
 * returns     - Nothing
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__II(JNIEnv *env, jclass this, jint handle, jint traceId)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR());
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JSTRING), utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2Ljava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JSTRING), utfS1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2Ljava_lang_String_2Ljava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jstring s2, jstring s3)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;
	const char *utfS3 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	utfS3 = (*env)->GetStringUTFChars(env, s3, NULL);

	if (NULL == utfS3) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JSTRING, JSTRING), utfS1, utfS2, utfS3);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}

	if (NULL != utfS3) {
		(*env)->ReleaseStringUTFChars(env, s3, utfS3);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2Ljava_lang_Object_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jobject o1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JOBJECT), utfS1, o1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_Object_2Ljava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jobject o1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JOBJECT, JSTRING), o1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2I(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jint i1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JWORD), utfS1, i1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIILjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jint i1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JWORD, JSTRING), i1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2J(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jlong l1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JDOUBLEWORD), utfS1, l1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}
void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIJLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jlong l1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JDOUBLEWORD, JSTRING), l1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}
void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2B(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jbyte b1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JWORD), utfS1, b1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIBLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jbyte b1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JWORD, JSTRING), b1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2C(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jchar c1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JCHAR), utfS1, c1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IICLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jchar c1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JCHAR, JSTRING), c1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2F(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jfloat f1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JFLOATPOINT), utfS1, f1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIFLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jfloat f1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JFLOATPOINT, JSTRING), f1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2D(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jdouble d1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JSTRING, JFLOATPOINT), utfS1, d1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIDLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jdouble d1, jstring s1)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JFLOATPOINT, JSTRING), d1, utfS1);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_Object_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jobject o1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JOBJECT), o1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jobject o1, jobject o2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JOBJECT, JOBJECT), o1, o2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__III(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jint i1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JWORD), i1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIII(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jint i1 , jint i2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JWORD, JWORD), i1, i2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIIII(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jint i1, jint i2, jint i3)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JWORD, JWORD, JWORD), i1, i2, i3);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIJ(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jlong l1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JDOUBLEWORD), l1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIJJ(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jlong l1, jlong l2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JDOUBLEWORD, JDOUBLEWORD), l1, l2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIJJJ(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jlong l1, jlong l2, jlong l3)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JDOUBLEWORD, JDOUBLEWORD, JDOUBLEWORD), l1, l2, l3);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIB(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jbyte b1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JWORD), b1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIBB(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jbyte b1, jbyte b2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JWORD, JWORD), b1, b2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIBBB(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jbyte b1, jbyte b2, jbyte b3)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JWORD, JWORD, JWORD), b1, b2, b3);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIC(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jchar c1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JCHAR), c1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IICC(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jchar c1, jchar c2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JCHAR, JCHAR), c1, c2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IICCC(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jchar c1, jchar c2, jchar c3)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JCHAR, JCHAR, JCHAR), c1, c2, c3);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIF(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jfloat f1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JFLOATPOINT), f1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIFF(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jfloat f1, jfloat f2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JFLOATPOINT, JFLOATPOINT), f1, f2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIFFF(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jfloat f1, jfloat f2, jfloat f3)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JFLOATPOINT, JFLOATPOINT, JFLOATPOINT), f1, f2, f3);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IID(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jdouble d1)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_1(JFLOATPOINT), d1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIDD(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jdouble d1, jdouble d2)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_2(JFLOATPOINT, JFLOATPOINT), d1, d2);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIDDD(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jdouble d1, jdouble d2, jdouble d3)
{
	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JFLOATPOINT, JFLOATPOINT, JFLOATPOINT), d1, d2, d3);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2Ljava_lang_Object_2Ljava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jobject o1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JOBJECT, JSTRING), utfS1, o1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_Object_2Ljava_lang_String_2Ljava_lang_Object_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jobject o1, jstring s1, jobject o2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JOBJECT, JSTRING, JOBJECT), o1, utfS1, o2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2ILjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jint i1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JWORD, JSTRING), utfS1, i1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIILjava_lang_String_2I(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jint i1, jstring s1, jint i2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JWORD, JSTRING, JWORD), i1, utfS1, i2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2JLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jlong l1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JDOUBLEWORD, JSTRING), utfS1, l1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIJLjava_lang_String_2J(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jlong l1, jstring s1, jlong l2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JDOUBLEWORD, JSTRING, JDOUBLEWORD), l1, utfS1, l2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2BLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jbyte b1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JWORD, JSTRING), utfS1, b1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIBLjava_lang_String_2B(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jbyte b1, jstring s1, jbyte b2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JWORD, JSTRING, JWORD), b1, utfS1, b2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2CLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jchar c1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JCHAR, JSTRING), utfS1, c1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IICLjava_lang_String_2C(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jchar c1, jstring s1, jchar c2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JCHAR, JSTRING, JCHAR), c1, utfS1, c2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2FLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jfloat f1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JFLOATPOINT, JSTRING), utfS1, f1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIFLjava_lang_String_2F(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jfloat f1, jstring s1, jfloat f2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JFLOATPOINT, JSTRING, JFLOATPOINT), f1, utfS1, f2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IILjava_lang_String_2DLjava_lang_String_2(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jstring s1, jdouble d1, jstring s2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);
	const char *utfS2 = NULL;

	if (NULL == utfS1) {
		goto end;
	}

	utfS2 = (*env)->GetStringUTFChars(env, s2, NULL);

	if (NULL == utfS2) {
		goto end;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JSTRING, JFLOATPOINT, JSTRING), utfS1, d1, utfS2);

end:
	if (NULL != utfS1) {
		(*env)->ReleaseStringUTFChars(env, s1, utfS1);
	}

	if (NULL != utfS2) {
		(*env)->ReleaseStringUTFChars(env, s2, utfS2);
	}
}

void JNICALL
Java_com_ibm_jvm_Trace_traceImpl__IIDLjava_lang_String_2D(JNIEnv *env,
			   jclass this, jint handle, jint traceId, jdouble d1, jstring s1, jdouble d2)
{
	const char *utfS1 = (*env)->GetStringUTFChars(env, s1, NULL);

	if (NULL == utfS1) {
		return;
	}

	trace(env, handle, traceId, ARGUMENT_DESCRIPTOR_3(JFLOATPOINT, JSTRING, JFLOATPOINT), d1, utfS1, d2);

	(*env)->ReleaseStringUTFChars(env, s1, utfS1);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_set
 * description - Set trace options
 * parameters  - JNIEnv, this, trace configuration command
 * returns     - return code
 *************************************************************************/
int JNICALL
Java_com_ibm_jvm_Trace_setImpl(JNIEnv *env, jobject this, jstring jcmd)
{
	const char *cmd = NULL;
	int ret = JNI_ERR;

	/*
	 * Check that interface is available
	 */
	if (NULL == TRACEDOTCGLOBAL(rasIntf)) {
		return ret;
	}

	if (NULL == jcmd) {
		return JNI_EINVAL;
	}

	cmd = (*env)->GetStringUTFChars(env, jcmd, JNI_FALSE);

	if (NULL != cmd) {
		ret = TRACEDOTCGLOBAL(rasIntf)->TraceSet(env, cmd);

		(*env)->ReleaseStringUTFChars(env, jcmd, cmd);
	}

	return ret;
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_snap
 * description - Snap internal trace buffers
 * parameters  - JNIEnv, this.
 * returns     - none
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_snapImpl(JNIEnv *env, jobject this)
{
	/*
	 * Check that interface is available
	 */
	if (NULL == TRACEDOTCGLOBAL(rasIntf)) {
		return;
	}

	TRACEDOTCGLOBAL(rasIntf)->TraceSnap(env, NULL);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_suspend
 * description - Suspend tracing activity
 * parameters  - JNIEnv, this.
 * returns     - none
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_suspendImpl(JNIEnv *env, jobject this)
{
	/*
	 * Check that interface is available
	 */
	if (NULL == TRACEDOTCGLOBAL(rasIntf)) {
		return;
	}

	TRACEDOTCGLOBAL(rasIntf)->TraceSuspend(env);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_resume
 * description - Resume tracing activity
 * parameters  - JNIEnv, this.
 * returns     - none
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_resumeImpl(JNIEnv *env, jobject this)
{
	/*
	 * Check that interface is available
	 */
	if (NULL == TRACEDOTCGLOBAL(rasIntf)) {
		return;
	}

	TRACEDOTCGLOBAL(rasIntf)->TraceResume(env);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_suspendThis
 * description - Suspend tracing activity for this thread
 * parameters  - JNIEnv, this.
 * returns     - none
 *
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_suspendThisImpl(JNIEnv *env, jobject this)
{
	/*
	 * Check that interface is available
	 */
	if (NULL == TRACEDOTCGLOBAL(rasIntf)) {
		return;
	}

	TRACEDOTCGLOBAL(rasIntf)->TraceSuspendThis(env);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_resumeThis
 * description - resume tracing activity for this thread
 * parameters  - JNIEnv, this.
 * returns     - none
 *
 *************************************************************************/
void JNICALL
Java_com_ibm_jvm_Trace_resumeThisImpl(JNIEnv *env, jobject this)
{
	/*
	 * Check that interface is available
	 */
	if (NULL == TRACEDOTCGLOBAL(rasIntf)) {
		return;
	}

	TRACEDOTCGLOBAL(rasIntf)->TraceResumeThis(env);
}

/**************************************************************************
 * name        - Java_com_ibm_jvm_Trace_getMicros
 * description - Return the microsecond clock time
 * parameters  - JNIEnv, this.
 * returns     - A long containing the microsecond time
 * initial version for 6.0 - a better version might return the actual time
 * in microseconds.
 *************************************************************************/
jlong JNICALL
Java_com_ibm_jvm_Trace_getMicros(JNIEnv *env, jobject this)
{
	PORT_ACCESS_FROM_ENV(env);
	/* Use j9time_hires_delta instead of deprecated j9time_usec_clock */
	return (jlong)j9time_hires_delta(0, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
}

/*Array list code*/

static struct ArrayList *
allocArrayList(JNIEnv *const env, const UDATA slabSize)
{
	PORT_ACCESS_FROM_ENV(env);

	struct ArrayList *toReturn = j9mem_allocate_memory(sizeof(struct ArrayList), J9MEM_CATEGORY_VM_JCL);

	if (NULL == toReturn) {
		/*Malloc failure*/
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
		return NULL;
	}

	toReturn->slabSize = slabSize;
	toReturn->header = NULL;

	return toReturn;
}

static void **
arrayListAllocateSlab(JNIEnv *const env, const struct ArrayList *const list)
{
	PORT_ACCESS_FROM_ENV(env);

	/*We allocate an extra slot in the slab for the pointer to the next slab*/
	UDATA allocSize = sizeof(void *) * (list->slabSize + 1);
	void **toReturn = j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_VM_JCL);

	if (NULL == toReturn) {
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
		return NULL;
	}

	memset(toReturn, 0, allocSize);

	return toReturn;
}

static void **
arrayListGetSlab(JNIEnv *const env, const struct ArrayList *const list, const UDATA index, const BOOLEAN allocate)
{
	const int targetSlabNumber = (const int)(index / list->slabSize);
	int currentSlabNumber = 0;
	void **currentSlab;
	PORT_ACCESS_FROM_ENV(env);

	/*The first time this is called the list header slab won't have been created.*/
	if (NULL == list->header) {
		void **newSlab = arrayListAllocateSlab(env, list);

		if (NULL == newSlab) {
			/*Malloc failed*/
			return NULL;
		}

		if (! CompareAndSwapPtr((UDATA*)&(list->header), (UDATA)NULL, (UDATA)newSlab)) {
			/*Header is no longer NULL - someone else did the allocation. Free our newSlab and keep going*/
			j9mem_free_memory(newSlab);
		}
	}

	currentSlab = list->header;

	while (currentSlabNumber < targetSlabNumber) {
		/*The last element in the array is the pointer to the next slab*/
		if (currentSlab[list->slabSize]) {
			currentSlab = (void**)currentSlab[list->slabSize];
			currentSlabNumber++;
		} else {
			/*We've hit the end of the chain. At this point we can either allocate the space we need or give up*/
			if (allocate) {
				void **newSlab = arrayListAllocateSlab(env, list);

				if (NULL == newSlab) {
					/*Malloc failed*/
					return NULL;
				}

				/*Atomically swap in the new slab*/
				if (CompareAndSwapPtr((UDATA*)&(currentSlab[list->slabSize]), (UDATA)NULL, (UDATA)newSlab)) {
					currentSlab = newSlab;
					currentSlabNumber++;
				} else {
					/* The compare and swap didn't work - which means another thread must have already allocated and assigned
					 * the slab. Free the memory this thread allocated and go round the loop again to re-evaluate the current slab.
					 */
					j9mem_free_memory(newSlab);
					continue;
				}
			} else {
				/*We don't want to allocate the space for the slot, so we just return*/
				return NULL;
			}
		}
	}

	return currentSlab;
}

static IDATA
arrayListPut(JNIEnv *const env, const struct ArrayList *const list, const UDATA index, void *const value)
{
	void **const slab = arrayListGetSlab(env, list, index, TRUE);
	void *oldValue;
	const UDATA slabIndex = index % list->slabSize;

	if (NULL == slab) {
		return 1;
	}

	do {
		oldValue = slab[slabIndex];
	} while (!CompareAndSwapPtr((UDATA*)&(slab[slabIndex]), (UDATA)oldValue, (UDATA)value));

	return 0;
}

static void *
arrayListGet(JNIEnv *const env, const struct ArrayList *const list, const UDATA index)
{
	void **const slab = arrayListGetSlab(env, list, index, FALSE);
	UDATA slabIndex = index % list->slabSize;

	if (NULL == slab) {
		return NULL;
	}

	return slab[slabIndex];
}

static I_32
CompareAndSwap32(volatile U_32 *target, U_32 old, U_32 new32)
{
	return ( compareAndSwapU32((U_32*)target, old, new32) == old ) ? TRUE : FALSE;
}

static I_32
CompareAndSwapPtr(volatile UDATA *target, UDATA old, UDATA newptr)
{
	return ( compareAndSwapUDATA((UDATA*)target, old, newptr) == old ) ? TRUE : FALSE;
}

/*END OF FILE*/
