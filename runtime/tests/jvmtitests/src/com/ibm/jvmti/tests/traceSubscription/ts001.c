/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include <stdio.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

#if 0
#define PRINTF_DEBUG
#endif

static agentEnv * env;
static jvmtiExtensionFunction subscribe  = NULL;
static jvmtiExtensionFunction unsubscribe  = NULL;
static jvmtiExtensionFunction flush  = NULL;
static jvmtiExtensionFunction metadata = NULL;
void *subscriptionID;
volatile static jint bufferCount = 0;
volatile static jint bufferCountFinal = 0;
volatile static int completed = 0;

jvmtiError devNull(jvmtiEnv *env, void *data, jlong length, void *userData) {

#ifdef PRINTF_DEBUG
	fprintf(stderr, "discarding buffer\n");
	fflush(stderr);
#endif

	bufferCount++;
	return JVMTI_ERROR_NONE;
}


jvmtiError ts001Alarm(jvmtiEnv *env, void *subscriptionID, void *userData) {
#ifdef PRINTF_DEBUG
	fprintf(stderr, "ts001Alarm\n");
	fflush(stderr);
#endif
	bufferCountFinal = bufferCount;
	completed = 1;

	return JVMTI_ERROR_NONE;
}

jint JNICALL
ts001(agentEnv * agent_env, char * args)
{
	jvmtiError         err;
	jint                         extensionCount;
	jvmtiExtensionFunctionInfo	*extensionFunctions;
	int                          i;

	JVMTI_ACCESS_FROM_AGENT(agent_env);
	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_FALSE;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_REGISTER_TRACE_SUBSCRIBER) == 0) {
			subscribe = extensionFunctions[i].func;
		} else if (strcmp(extensionFunctions[i].id, COM_IBM_FLUSH_TRACE_DATA) == 0) {
			flush = extensionFunctions[i].func;
		} else if (strcmp(extensionFunctions[i].id, COM_IBM_DEREGISTER_TRACE_SUBSCRIBER) == 0) {
			unsubscribe = extensionFunctions[i].func;
		} else if (strcmp(extensionFunctions[i].id, COM_IBM_GET_TRACE_METADATA) == 0) {
			metadata = extensionFunctions[i].func;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return JNI_FALSE;
	}

	if (subscribe == NULL || flush == NULL || unsubscribe == NULL || metadata == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "trace subscription extension functions were not found");
		return JNI_FALSE;
	}

	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts001_tryRegisterTraceSubscriber(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError					 err;
	jvmtiEnv					*jvmti_env = env->jvmtiEnv;


	/* Let's register a subscriber */
#ifdef PRINTF_DEBUG
	fprintf(stderr, "registering\n");
	fflush(stderr);
#endif

	err = subscribe(jvmti_env, "ts001 subscriber", devNull, ts001Alarm, NULL, &subscriptionID);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RegisterTraceSubscriber failed");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts001_tryFlushTraceData(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError					 err;
	jvmtiEnv					*jvmti_env = env->jvmtiEnv;

#ifdef PRINTF_DEBUG
	fprintf(stderr, "flushing data\n");
	fflush(stderr);
#endif

	err = flush(jvmti_env);
	if (err != JVMTI_ERROR_NONE) {
		fprintf(stderr, "warning: FlushTraceData failed: %i\n", err);
		return FALSE;
	}

	return TRUE;
}

jint JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts001_tryDeregisterTraceSubscriber(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError					 err = JVMTI_ERROR_NONE;
	jvmtiEnv					*jvmti_env = env->jvmtiEnv;

	/* flushing should have caused us to be passed a buffer
	 * Might not be true if external trace is enabled as we could have no trace
	 * data in memory
	 */
#ifdef PRINTF_DEBUG
	fprintf(stderr, "deregistering\n");
	fflush(stderr);
#endif

	err = unsubscribe(jvmti_env, subscriptionID);
	if (err != JVMTI_ERROR_NONE && err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(env, err, "DeregisterTraceSubscriber failed");
		return -1;
	}

	return bufferCount;
}

jint JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts001_getBufferCount(JNIEnv * jni_env, jclass clazz)
{
	return bufferCount;
}

jint JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts001_getFinalBufferCount(JNIEnv * jni_env, jclass clazz)
{
	return bufferCountFinal;
}

jbyte JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts001_tryGetTraceMetadata(JNIEnv * jni_env, jclass clazz, jint index) {
	jvmtiError					 err = JVMTI_ERROR_NONE;
	jvmtiEnv					*jvmti_env = env->jvmtiEnv;
	void						*data;
	char						*chars;
	jint						length;

#ifdef PRINTF_DEBUG
	fprintf(stderr, "getting metadata\n");
	fflush(stderr);
#endif

	err = metadata(jvmti_env, &data, &length);
	chars = (char*)data;

	if (index > length) {
		error(env, err, "Requested out of bounds data");
		return 0;
	}
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "GetTraceMetadata failed");
		return -1;
	}

	return *(chars + index);
}
