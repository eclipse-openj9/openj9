/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

#include "jni.h"
#include "j9.h"
#include "util_api.h"
#include "ut_j9jcl.h"
#include "rasdump_api.h"

#define COM_IBM_JVM_DUMP "com.ibm.jvm.Dump."

static void raiseExceptionFor(JNIEnv *env, omr_error_t result);

/*
 * Cause a HeapDump to be caused.
 *
 * Returns the return value of the creation - 0 on success.
 */
jint JNICALL 
Java_com_ibm_jvm_Dump_HeapDumpImpl(JNIEnv *env, jclass clazz)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	omr_error_t rc = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "heap", "com.ibm.jvm.Dump.HeapDump", NULL, 0);

	return omrErrorCodeToJniErrorCode(rc);
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

/*
 * Cause a JavaDump to be caused.
 *
 * Returns the return value of the creation - 0 on success.
 */
jint JNICALL 
Java_com_ibm_jvm_Dump_JavaDumpImpl(JNIEnv *env, jclass clazz)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	omr_error_t rc = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "java", "com.ibm.jvm.Dump.JavaDump", NULL, 0);

	return omrErrorCodeToJniErrorCode(rc);
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

/*
 * Cause a SystemDump to be caused.
 *
 * Returns the return value of the creation - 0 on success.
 */
jint JNICALL 
Java_com_ibm_jvm_Dump_SystemDumpImpl(JNIEnv *env, jclass clazz)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	omr_error_t rc = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "system", "com.ibm.jvm.Dump.SystemDump", NULL, 0);
	
	return omrErrorCodeToJniErrorCode(rc);
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

/*
 * Cause a SnapDump to be caused.
 *
 * Returns the return value of the creation - 0 on success.
 */
jint JNICALL
Java_com_ibm_jvm_Dump_SnapDumpImpl(JNIEnv *env, jclass clazz) {
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	omr_error_t rc = vm->j9rasDumpFunctions->triggerOneOffDump(vm, "Snap", "com.ibm.jvm.Dump.SnapDump", NULL, 0);
	
	return omrErrorCodeToJniErrorCode(rc);
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

/* Scan the dump type string for "tool". Done in C to make sure we
 * can use the same logic as the dump code and avoid anyone doing
 * anything clever like finding multiple UTF-8 characters that map
 * to the ascii letters for "tool".
 */
static jboolean scanDumpTypeForToolDump(char **typeString)
{
	/* Check for the string "tool" as a dump type. (Appears before + or :) */
	char *endPtr = *typeString + strlen(*typeString);

	if( strchr(*typeString, ':') != NULL ) {
		endPtr = strchr(*typeString, ':');
	}

	do {
		/* Check for a tool dump option. */
		if( try_scan(typeString, "tool") ) {

			/* Check for a well-formed dump option */
			if ( *typeString[0] == '+' ||
				*typeString[0] == ':' ||
				*typeString[0] == '\0' ) {
				return JNI_TRUE;
			}
		} else {
			/* Any more dump types? */
			if( strchr(*typeString, '+') != NULL ) {
				*typeString = strchr(*typeString, '+') + 1;
			} else {
				break; /* No more dump types. */
			}
		}
	} while ( *typeString < endPtr);

	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvm_Dump_isToolDump(JNIEnv *env, jclass clazz, jstring jopts) {
#if defined(J9VM_RAS_DUMP_AGENTS)
	char *optsBuffer = NULL;
	int optsLength = 0;
	jboolean retVal = JNI_FALSE;

	PORT_ACCESS_FROM_ENV(env);

	if( jopts == NULL ) {
		return FALSE;
	}

	optsLength = (*env)->GetStringUTFLength(env, jopts);
	optsBuffer = j9mem_allocate_memory(optsLength+1, J9MEM_CATEGORY_VM_JCL);

	if( optsBuffer != NULL ) {
		char * optsBufferPtr = optsBuffer;
		memset(optsBuffer, 0, optsLength+1);
		(*env)->GetStringUTFRegion(env, jopts, 0, optsLength, optsBuffer);

		retVal = scanDumpTypeForToolDump(&optsBufferPtr);

		j9mem_free_memory(optsBuffer);
	} else {
		jclass exceptionClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		 if (exceptionClass != NULL) {
			 (*env)->ThrowNew(env, exceptionClass, "Out of memory triggering dump");
		 }
		 /* Just return if we can't load the exception class. */
		 retVal = JNI_FALSE;
	}
	return retVal;
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_FALSE;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_FALSE;
#endif
}

jstring JNICALL
Java_com_ibm_jvm_Dump_triggerDumpsImpl (JNIEnv *env, jclass clazz, jstring jopts, jstring jevent)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	char *optsBuffer = NULL;
	char *eventBuffer = NULL;
	char fileName[EsMaxPath+1];
	int optsLength = 0;
	int eventLength = 0;
	omr_error_t result = OMR_ERROR_NONE;
	jstring toReturn = NULL;

	PORT_ACCESS_FROM_ENV(env);

	/* Java code will have checked jopts is not null. */
	optsLength = (*env)->GetStringUTFLength(env, jopts);
	eventLength = (*env)->GetStringUTFLength(env, jevent);

	optsBuffer = j9mem_allocate_memory(optsLength+1, J9MEM_CATEGORY_VM_JCL);
	eventBuffer = j9mem_allocate_memory(strlen(COM_IBM_JVM_DUMP) + eventLength + 1, J9MEM_CATEGORY_VM_JCL);


	/* Copy the file name string, avoid holding locks on things. */
	if( optsBuffer != NULL && eventBuffer != NULL) {

		memset(optsBuffer, 0, optsLength+1);
		memset(eventBuffer, 0, strlen(COM_IBM_JVM_DUMP) + eventLength + 1);
		/* Prefix the dump detail with com.ibm.jvm.Dump so createOneOffDumpAgent can
		 * be sure of the source and prevent tool dumps from being run. (Avoiding
		 * a back door to Runtime.exec() )
		 */
		strcpy(eventBuffer, COM_IBM_JVM_DUMP);
		memset(fileName, 0, sizeof(fileName));

		(*env)->GetStringUTFRegion(env, jopts, 0, optsLength, optsBuffer);
		(*env)->GetStringUTFRegion(env, jevent, 0, eventLength, eventBuffer + strlen(eventBuffer));

		result = vm->j9rasDumpFunctions->triggerOneOffDump(vm, optsBuffer, eventBuffer, fileName, sizeof(fileName));

     	if (OMR_ERROR_NONE == result) {
    		jstring actualFile = NULL;
    		actualFile = (*env)->NewStringUTF(env, fileName);
    		toReturn = actualFile;
    	} else {
    		raiseExceptionFor(env, result);
    	}
	} else {
		jclass exceptionClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		 if (exceptionClass != NULL) {
			 (*env)->ThrowNew(env, exceptionClass, "Out of memory triggering dump");
		 }
		 /* Just return if we can't load the exception class. */
	}
	if( optsBuffer != NULL ) {
		j9mem_free_memory(optsBuffer);
	}
	if( eventBuffer != NULL ) {
		j9mem_free_memory(eventBuffer);
	}
	return toReturn;
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

void JNICALL
Java_com_ibm_jvm_Dump_setDumpOptionsImpl (JNIEnv *env, jclass clazz, jstring jopts)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	char *optsBuffer = NULL;
	int optsLength = 0;
	omr_error_t result = OMR_ERROR_NONE;

#ifdef J9VM_RAS_DUMP_AGENTS

	PORT_ACCESS_FROM_ENV(env);

	/* Java code will have checked jopts is not null. */
	optsLength = (*env)->GetStringUTFLength(env, jopts);

	optsBuffer = j9mem_allocate_memory(optsLength+1, J9MEM_CATEGORY_VM_JCL);

	if( optsBuffer != NULL ) {
		memset(optsBuffer, 0, optsLength+1);

		(*env)->GetStringUTFRegion(env, jopts, 0, optsLength, optsBuffer);
		if (!(*env)->ExceptionCheck(env)) {

			/* Pass option to the dump facade */
			result = vm->j9rasDumpFunctions->setDumpOption(vm, optsBuffer);

			/* Map back to exception */
			if (OMR_ERROR_NONE != result) {
				raiseExceptionFor(env, result);
			}
		}
	} else {
		jclass exceptionClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		 if (exceptionClass != NULL) {
			 (*env)->ThrowNew(env, exceptionClass, "Out of memory setting dump options");
		 }
		 /* Just return if we can't load the exception class as an exception will be pending. */
	}

	if( optsBuffer != NULL ) {
		j9mem_free_memory(optsBuffer);
	}
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class as an exception will be pending. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

jstring JNICALL
Java_com_ibm_jvm_Dump_queryDumpOptionsImpl (JNIEnv *env, jclass clazz) {

#define BUFFER_SIZE 10240

	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	jint buffer_size = BUFFER_SIZE;
	char options_buffer[BUFFER_SIZE];
	char* options_ptr = NULL;
	jint data_size;
	jint* data_size_ptr = &data_size;
	jstring toReturn = NULL;
	omr_error_t result = OMR_ERROR_NONE;
	PORT_ACCESS_FROM_ENV(env);

#ifdef J9VM_RAS_DUMP_AGENTS
	memset(options_buffer, 0, buffer_size);
	result = vm->j9rasDumpFunctions->queryVmDump(vm, buffer_size, options_buffer, data_size_ptr);

	/* Insufficient buffer space, malloc. */
	/* Retry in case someone is updating the agents while we run. */
	while( data_size > buffer_size) {
		buffer_size = data_size;
		if( options_ptr != NULL ) {
			j9mem_free_memory(options_ptr);
			options_ptr = NULL;
		}
		options_ptr = j9mem_allocate_memory(buffer_size, J9MEM_CATEGORY_VM_JCL);
		if( options_ptr != NULL ) {
			memset(options_ptr, 0, buffer_size);
			result = vm->j9rasDumpFunctions->queryVmDump(vm, buffer_size, options_ptr, data_size_ptr);
		} else {
			result = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			break; /* malloc failed */
		}
	}
	if (OMR_ERROR_NONE == result) {
		if( options_ptr == NULL ) {
			toReturn = (*env)->NewStringUTF(env, options_buffer);
		} else {
			toReturn = (*env)->NewStringUTF(env, options_ptr);
		}
	} else {
		/* Map back to exception */
		raiseExceptionFor(env, result);
	}
	if( options_ptr != NULL ) {
		j9mem_free_memory(options_ptr);
	}

	return toReturn;
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

void JNICALL
Java_com_ibm_jvm_Dump_resetDumpOptionsImpl (JNIEnv *env, jclass clazz)
{
	omr_error_t result = OMR_ERROR_NONE;
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;

#ifdef J9VM_RAS_DUMP_AGENTS
	/* request dump reset from dump module */
	result = vm->j9rasDumpFunctions->resetDumpOptions(vm);

	/* Not much error handling we can do but this can fail if the dump configuration
	 * is locked while a dump is in progress. */

	/* Map back to exception */
	if (OMR_ERROR_NONE != result) {
		raiseExceptionFor(env, result);
	}
#else
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return JNI_ERR;
	}
	(*env)->ThrowNew(env, exceptionClass, "Dumps not supported in this configuration");
	return JNI_ERR;
#endif
}

/**
 * Raise exception for OMR error code that is passed in.
 * 
 * Dump.queryDumpOptions() will never return DumpConfigurationUnavailableException according to the published API.
 * Java_com_ibm_jvm_Dump_queryDumpOptionsImpl() calls raiseExceptionFor() only when queryVmDump() returns
 * OMR_ERROR_OUT_OF_NATIVE_MEMORY, the return case of OMR_ERROR_ILLEGAL_ARGUMENT will never happen for raiseException().
 *
 * @param[in] env The JNIEnv* of the current thread.
 * @param[in] result The OMR error code.
 */
static void
raiseExceptionFor(JNIEnv *env, omr_error_t result)
{
	jclass exceptionClass = NULL;

	switch (result) {
	case OMR_ERROR_INTERNAL:
		exceptionClass = (*env)->FindClass(env, "com/ibm/jvm/InvalidDumpOptionException");
		if (exceptionClass != NULL) {
			(*env)->ThrowNew(env, exceptionClass, "Error in dump options.");
		}
		/* Just return if we can't load the exception class. */
		break;
	case OMR_ERROR_OUT_OF_NATIVE_MEMORY:
		exceptionClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		if (exceptionClass != NULL) {
			(*env)->ThrowNew(env, exceptionClass, "Out of memory setting dump option");
		}
		/* Just return if we can't load the exception class. */
		break;
	case OMR_ERROR_NOT_AVAILABLE:
		exceptionClass = (*env)->FindClass(env, "com/ibm/jvm/DumpConfigurationUnavailableException");
		if (exceptionClass != NULL) {
			(*env)->ThrowNew(env, exceptionClass, "Dump configuration cannot be changed while a dump is in progress.");
		}
		/* Just return if we can't load the exception class. */
		break;
	default:
		Assert_JCL_unreachable();
		break;
	}
}
