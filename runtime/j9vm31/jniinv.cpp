/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "j9vm31.h"
#include "omrcomp.h"

extern "C" {

/* Given LE supports only one TCB that can cross AMODE boundary, most likely
 * we will only have 1 JavaVM in the address space. Simply use a static to
 * track the live JavaVM31 instances.
 */
static JavaVM31* globalRootJavaVM31 = NULL;

static void initializeJavaVM31(JavaVM31 * JavaVM31, jlong JavaVM64);
static void freeJavaVM31(JavaVM31 * JavaVM31);

/* This is the function table of the 31-bit shim JNIInvokeInterface functions. */
struct JNIInvokeInterface_ EsJNIInvokeFunctions = {
	NULL,
	NULL,
	NULL,
	DestroyJavaVM,
	AttachCurrentThread,
	DetachCurrentThread,
	GetEnv,
	AttachCurrentThreadAsDaemon,
};

jint JNICALL
JNI_GetDefaultJavaVMInitArgs(void * vm_args)
{
	/* Zero out the JavaVMInitArgs reserved / padding fields. */
	JavaVMInitArgs *javaInitArgs = ((JavaVMInitArgs *) vm_args);
	if (NULL != javaInitArgs) {
		javaInitArgs->reservedOptionsPadding = NULL;
		javaInitArgs->reservedAlignmentPadding = NULL;

		for (int32_t i = 0; i < javaInitArgs->nOptions; i++) {
			javaInitArgs->options[i].reservedOptionStringPadding = NULL;
			javaInitArgs->options[i].reservedExtraInfoPadding = NULL;
		}
	}

	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { (uint64_t)vm_args };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJVM_NAME, "JNI_GetDefaultJavaVMInitArgs",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "JNI_GetDefaultJavaVMInitArgs failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		return JNI_ERR;
	}
	return returnValue;
}

jint JNICALL
JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args)
{
	uint64_t JavaVM64Result = 0;
	uint64_t JNIEnv64Result = 0;

	/* Zero out the JavaVMInitArgs reserved / padding fields. */
	JavaVMInitArgs *javaInitArgs = ((JavaVMInitArgs *) vm_args);
	if (NULL != javaInitArgs) {
		javaInitArgs->reservedOptionsPadding = NULL;
		javaInitArgs->reservedAlignmentPadding = NULL;

		for (int32_t i = 0; i < javaInitArgs->nOptions; i++) {
			javaInitArgs->options[i].reservedOptionStringPadding = NULL;
			javaInitArgs->options[i].reservedExtraInfoPadding = NULL;

			/* Abort, Exit and vsprintf hooks may be specified, in which
			 * case, we need to high tag the AMODE31 functions.
			 */
			if (NULL != javaInitArgs->options[i].extraInfo) {
				/* extraInfo may have been stale garbage, so need to compare string. */
				char *optionString = javaInitArgs->options[i].optionString;
				if (NULL != optionString)
				/* Have to do the string comparisons in CCSID 1208 - UTF-8. */
#pragma convert(1208)
				if (0 == strncmp(optionString, "abort", strlen("abort"))) {
					javaInitArgs->options[i].reservedExtraInfoPadding = (void *)HANDLE31_HIGHTAG;
				} else if (0 == strncmp(optionString, "exit", strlen("exit"))) {
					javaInitArgs->options[i].reservedExtraInfoPadding = (void *)HANDLE31_HIGHTAG;
				}  else if (0 == strncmp(optionString, "vfprintf", strlen("vfprintf"))) {
					javaInitArgs->options[i].reservedExtraInfoPadding = (void *)HANDLE31_HIGHTAG;
				}
#pragma convert(pop)
			}
		}
	}

	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JavaVM64, CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { (uint64_t)&JavaVM64Result, (uint64_t)&JNIEnv64Result, (uint64_t)vm_args };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJVM_NAME, "JNI_CreateJavaVM",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "JNI_CreateJavaVM failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		return JNI_ERR;
	}

	if (JNI_OK == returnValue) {
		/* Create and return the corresponding 31-bit instance of JavaVM and JNIEnv. */
		JavaVM31* javaVM31 = getJavaVM31(JavaVM64Result);
		*pvm = (JavaVM*) javaVM31;
		JNIEnv31* jniEnv31 = getJNIEnv31(JNIEnv64Result);
		*penv = (JNIEnv*) jniEnv31;
		jniEnv31->javaVM31 = javaVM31;
	}
	return returnValue;
}

jint JNICALL
JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_uint32_ptr, CEL4RO64_type_jsize, CEL4RO64_type_uint32_ptr };

	/* Allocate a temporary buffer to store the returned JavaVM64 instances.
	 * Will copy the corresponding 31-bit JavaVM instanes to vmBuf if successful below.
	 */
	uint64_t *tempJavaVM64Buffer = (uint64_t *)malloc(sizeof(jlong) * bufLen);
	if (NULL == tempJavaVM64Buffer) {
		return JNI_ERR;
	}

	uint64_t argValues[NUM_ARGS] = { (uint64_t)tempJavaVM64Buffer, bufLen, (uint64_t)nVMs };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJVM_NAME, "JNI_GetCreatedJavaVMs",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "JNI_GetCreatedJavaVMs failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		returnValue = JNI_ERR;
	}

	/* On success, copy the corresponding 31-bit JavaVM instances to vmBuf. */
	if (JNI_OK == returnValue) {
		for (int32_t i = 0; i < *nVMs; i++) {
			vmBuf[i] = (JavaVM*)getJavaVM31(tempJavaVM64Buffer[i]);
		}
	}
	free(tempJavaVM64Buffer);
	return returnValue;
}

jint JNICALL
DestroyJavaVM(JavaVM *vm)
{
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JavaVM64 };
	uint64_t argValues[NUM_ARGS] = { JAVAVM64_FROM_JAVAVM31(vm)};
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJ9VM29_NAME, "DestroyJavaVM",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "DestroyJavaVM failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		returnValue = JNI_ERR;
	}
	/* If successful, we will free our corresponding 31-bit JavaVM instance and matching 31-bit JNIEnv. */
	if (JNI_OK == returnValue) {
		freeJavaVM31((JavaVM31 *)vm);
	}
	return returnValue;
}

jint JNICALL
AttachCurrentThread(JavaVM *vm, void **penv, void *args)
{
	/* The current LE CEL4RO31/64 support will only allow 1 TCB to cross the AMODE boundary. As such, we need to
	 * check the result of CEL4RO64 for error codes, and handle accordingly.
	 */
	const jint NUM_ARGS = 3;
	uint64_t JNIEnv64Result = 0;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JavaVM64, CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JAVAVM64_FROM_JAVAVM31(vm), (uint64_t)&JNIEnv64Result, (uint64_t)args };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = J9_CEL4RO64_RETCODE_OK;

	/* Zero initialize reservedNamePadding in JavaVMAttachArgs. */
	JavaVMAttachArgs *vmAttachArgs = ((JavaVMAttachArgs *) args);
	if (NULL != vmAttachArgs) {
		vmAttachArgs->reservedNamePadding = NULL;
	}

	cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJ9VM29_NAME, "AttachCurrentThread",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "AttachCurrentThread failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		return JNI_ERR;
	}
	/* If successful, we will return the correponding 31-bit JNIEnv. */
	if (JNI_OK == returnValue) {
		JNIEnv31* jniEnv31 = getJNIEnv31(JNIEnv64Result);
		jniEnv31->javaVM31 = (JavaVM31 *)vm;
		*penv = (JavaVM *)jniEnv31;
	}
	return returnValue;
}

jint JNICALL
DetachCurrentThread(JavaVM *vm) {
	/* The current LE CEL4RO31/64 support will only allow 1 TCB to cross the AMODE boundary. In theory,
	 * we should have attached before calling DetachCurrentThread, but will confirm CEL4RO64 return code.
	 */
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JavaVM64 };
	uint64_t argValues[NUM_ARGS] = { JAVAVM64_FROM_JAVAVM31(vm) };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJ9VM29_NAME, "DetachCurrentThread",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "DetachCurrentThread failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		return JNI_ERR;
	}
	return returnValue;
}

jint JNICALL
GetEnv(JavaVM *vm, void **penv, jint version)
{
	const jint NUM_ARGS = 3;
	uint64_t JNIEnv64Result = 0;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JavaVM64, CEL4RO64_type_JNIEnv64, CEL4RO64_type_jint };
	uint64_t argValues[NUM_ARGS] = { JAVAVM64_FROM_JAVAVM31(vm), (uint64_t)&JNIEnv64Result, version };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJ9VM29_NAME, "GetEnv",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "GetEnv failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		return JNI_ERR;
	}
	/* If successful, we will return the correponding 31-bit JNIEnv. */
	if (JNI_OK == returnValue) {
		JNIEnv31* jniEnv31 = getJNIEnv31(JNIEnv64Result);
		jniEnv31->javaVM31 = (JavaVM31 *)vm;
		*penv = (JavaVM *)jniEnv31;
	}
	return returnValue;
}

jint JNICALL
AttachCurrentThreadAsDaemon(JavaVM *vm, void **penv, void *args)
{
	/* The current LE CEL4RO31/64 support will only allow 1 TCB to cross the AMODE boundary. As such, we need to
	 * check the result of CEL4RO64 for error codes, and handle accordingly.
	 */
	const jint NUM_ARGS = 3;
	uint64_t JNIEnv64Result = 0;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JavaVM64, CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JAVAVM64_FROM_JAVAVM31(vm), (uint64_t)&JNIEnv64Result, (uint64_t)args };
	jint returnValue = JNI_ERR;
	uint32_t cel4ro64ReturnCode = J9_CEL4RO64_RETCODE_OK;

	/* Zero initialize reservedNamePadding in JavaVMAttachArgs. */
	JavaVMAttachArgs *vmAttachArgs = ((JavaVMAttachArgs *) args);
	if (NULL != vmAttachArgs) {
		vmAttachArgs->reservedNamePadding = NULL;
	}

	cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
			LIBJ9VM29_NAME, "AttachCurrentThreadAsDaemon",
			argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (J9_CEL4RO64_RETCODE_OK != cel4ro64ReturnCode) {
		fprintf(stderr, "AttachCurrentThreadAsDaemon failed. CEL4RO64 rc: %d - %s\n", cel4ro64ReturnCode, j9_cel4ro64_get_error_message(cel4ro64ReturnCode));
		return JNI_ERR;
	}
	/* If successful, we will return the correponding 31-bit JNIEnv. */
	if (JNI_OK == returnValue) {
		JNIEnv31* jniEnv31 = getJNIEnv31(JNIEnv64Result);
		jniEnv31->javaVM31 = (JavaVM31 *)vm;
		*penv = (JavaVM *)jniEnv31;
	}
	return returnValue;
}

/**
 * Following set of functions are JavaVM31 utility routines.
 */


JavaVM31 * JNICALL
getJavaVM31(uint64_t javaVM64)
{
	/* Traverse list to find matching JavaVM31. */
	for (JavaVM31 *curJavaVM31 = globalRootJavaVM31; curJavaVM31 != NULL; curJavaVM31 = curJavaVM31->next) {
		if (javaVM64 == curJavaVM31->javaVM64) {
			return curJavaVM31;
		}
	}

	/* Create and initialize a new instance. */
	JavaVM31 *newJavaVM31 = (JavaVM31 *)malloc(sizeof(JavaVM31));
	if (NULL != newJavaVM31) {
		initializeJavaVM31(newJavaVM31, javaVM64);
	}
	return newJavaVM31;
}

/**
 * Initialize new JavaVM31 structure, with references to the corresponding 64-bit
 * JavaVM, along with both 31-bit and 64-bit invoke function tables. Add to our
 * list of JavaVM31.
 *
 * @param[in] javaVM31 The 31-bit JavaVM* instance to initialize.
 * @param[in] javaVM64 The matching 64-bit JavaVM* from JVM.
 */
static void
initializeJavaVM31(JavaVM31 * javaVM31, jlong javaVM64)
{
	javaVM31->functions = (JNIInvokeInterface_ *)GLOBAL_TABLE(EsJNIInvokeFunctions);
	javaVM31->javaVM64 = javaVM64;
	javaVM31->next = globalRootJavaVM31;
	globalRootJavaVM31 = javaVM31;
}

/**
 * Free the current 31-bit JavaVM31 instance.
 *
 * @param[in] javaVM31ToFree The 31-bit JavaVM instance to free.
 */
static void
freeJavaVM31(JavaVM31 * javaVM31ToFree)
{
	/* Traverse globalRootJavaVM31 list and remove this instance of JavaVM31. */
	JavaVM31 *currentJavaVM31 = globalRootJavaVM31;

	if (currentJavaVM31 == javaVM31ToFree) {
		globalRootJavaVM31 = currentJavaVM31->next;
	} else {
		/* Traverse list to find the entry to remove. */
		while (NULL != currentJavaVM31) {
			JavaVM31 *next = currentJavaVM31->next;
			if (next == javaVM31ToFree) {
				currentJavaVM31->next = next->next;
				break;
			}
			currentJavaVM31 = next;
		}
	}
	free(javaVM31ToFree);
}

} /* extern "C" */
