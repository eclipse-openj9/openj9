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
#include "j9port.h"
#include "jvmti.h"
#include "jvmtinls.h"
#include "jclprots.h"

#define LOG_OPTION_DATA_SIZE 256

static void raiseException(JNIEnv *env, const char *type, U_32 moduleNumber, U_32 messageNumber, const char *message);

/*
 * QueryImpl
 *
 * Returns a string representation of the log flags.
 */
jstring JNICALL
Java_com_ibm_jvm_Log_QueryOptionsImpl(JNIEnv *env, jclass clazz)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	I_32 bufferSize = 0;
	jstring options = NULL;
	char *nativeOptions = NULL;
	UDATA rc = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* allocate enough memory for the options string */
	nativeOptions = j9mem_allocate_memory(LOG_OPTION_DATA_SIZE, J9MEM_CATEGORY_VM_JCL);
	if (NULL == nativeOptions) {
		vm->internalVMFunctions->throwNativeOOMError(env, 0, 0);
		return NULL;
	}

	/* retrieve options string */
	rc = vm->internalVMFunctions->queryLogOptions(vm, LOG_OPTION_DATA_SIZE, (void *)nativeOptions, &bufferSize);
	if (JVMTI_ERROR_NONE != rc) {
		raiseException(env, "java/lang/RuntimeException", J9NLS_JVMTI_COM_IBM_LOG_QUERY_OPT_ERROR,
				"Could not query JVM log options");
		j9mem_free_memory(nativeOptions);
		return NULL;
	}

	/* convert native string to Unicode jstring */
	options = (*env)->NewStringUTF(env, nativeOptions);

	/* free now, as we always need to independent of return value of previous call */
	j9mem_free_memory(nativeOptions);

	if (NULL == options) {
		raiseException(env, "java/lang/RuntimeException", J9NLS_JVMTI_COM_IBM_LOG_NATIVE_STRING_ERROR,
				"Could not convert JVM log options native string");
	}

	return options;
}

/*
 * SetImpl
 *
 * Sets the log flags.
 */
jint JNICALL
Java_com_ibm_jvm_Log_SetOptionsImpl(JNIEnv *env, jclass clazz, jstring options)
{
	J9VMThread *thr = (J9VMThread *)env;
	J9JavaVM *vm = thr->javaVM;
	const char *nativeOptions = NULL;
	UDATA rc = 0;

	/* convert options into a char * */
	nativeOptions = (*env)->GetStringUTFChars(env, options, 0);
	if (NULL == nativeOptions) {
		return JNI_ERR;
	}

	rc = vm->internalVMFunctions->setLogOptions(vm, (char *)nativeOptions);
	(*env)->ReleaseStringUTFChars(env, options, nativeOptions);

	if (JVMTI_ERROR_NONE != rc) {
		raiseException(env, "java/lang/RuntimeException", J9NLS_JVMTI_COM_IBM_LOG_SET_OPT_ERROR,
				"Could not set JVM log options");
		return JNI_ERR;
	}

	return JNI_OK;
}

/* Wrapper for throwing an error */
static void
raiseException(JNIEnv *env, const char *type, U_32 moduleNumber, U_32 messageNumber, const char *message)
{
	const char *nlsMessage = NULL;
	jclass exceptionClass = (*env)->FindClass(env, type);
	PORT_ACCESS_FROM_ENV(env);

	nlsMessage = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_ERROR, moduleNumber, messageNumber, message);
	if (NULL == exceptionClass) {
		/* Did what we could */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, nlsMessage);
}
