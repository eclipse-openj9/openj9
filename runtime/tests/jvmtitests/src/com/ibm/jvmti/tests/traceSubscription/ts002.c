/*******************************************************************************
 * Copyright (c) 2013, 2013 IBM Corp. and others
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

static agentEnv * env;
static jvmtiExtensionFunction tracePointRegisterFunction  = NULL;
static jvmtiExtensionFunction tracePointDeregisterFunction = NULL;
void *subscription1;
void *subscription2;
volatile static jint tracePointCount1 = 0;
volatile static jint tracePointCount2 = 0;
volatile static jboolean alarmTriggered1 = JNI_FALSE;
volatile static jboolean alarmTriggered2 = JNI_FALSE;

/* JVMTI tracepoint subscriber tracepoint callbacks */
jvmtiError ts002DataCallback1(jvmtiEnv *jvmti_env, void *data, jlong length, void *userData) {
	/* printf("ts002.c: JVMTI tracepoint callback 1 triggered, with data %s\n", data); */
	tracePointCount1++;
	if (tracePointCount1++ >= 50) {
		/* printf("tiSample: JVMTI tracepoint callback 1, reached 50 tracepoints, returning error now\n", data); */
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	return JVMTI_ERROR_NONE;
}
jvmtiError ts002DataCallback2(jvmtiEnv *jvmti_env, void *data, jlong length, void *userData) {
	/* printf("ts002.c: JVMTI tracepoint callback 2 triggered, with data %s\n", data); */
	tracePointCount2++;
	if (tracePointCount2++ >= 50) {
		/* printf("tiSample: JVMTI tracepoint callback 2, reached 50 tracepoints, returning error now\n", data); */
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	return JVMTI_ERROR_NONE;
}
/* JVMTI tracepoint subscriber alarm callbacks */
void ts002AlarmCallback1(jvmtiEnv *env, void *subscriptionID, void *userData) {
	/* printf("ts002.c: JVMTI tracepoint alarm callback triggered\n"); */
	alarmTriggered1 = JNI_TRUE;
}
void ts002AlarmCallback2(jvmtiEnv *env, void *subscriptionID, void *userData) {
	/* printf("ts002.c: JVMTI tracepoint alarm callback triggered\n"); */
	alarmTriggered2 = JNI_TRUE;
}

jint JNICALL
ts002(agentEnv * agent_env, char * args)
{
	jvmtiError result;
	jint extensionCount;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	int i;

	JVMTI_ACCESS_FROM_AGENT(agent_env);
	env = agent_env;

	result = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( result != JVMTI_ERROR_NONE ) {
		error(env, result, "Failed GetExtensionFunctions()");
		return JNI_FALSE;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_REGISTER_TRACEPOINT_SUBSCRIBER) == 0) {
			tracePointRegisterFunction = extensionFunctions[i].func;
		} else if (strcmp(extensionFunctions[i].id, COM_IBM_DEREGISTER_TRACEPOINT_SUBSCRIBER) == 0) {
			tracePointDeregisterFunction = extensionFunctions[i].func;
		}
	}

	result = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (result != JVMTI_ERROR_NONE) {
		error(env, result, "Failed to deallocate extension functions");
		return JNI_FALSE;
	}

	if (tracePointRegisterFunction == NULL || tracePointDeregisterFunction == NULL ) {
		error(env, JVMTI_ERROR_NOT_FOUND, "Trace subscription extension functions were not found");
		return JNI_FALSE;
	}

	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts002_tryRegisterTracePointSubscribers(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError result;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	/* printf("ts002.c: registering tracepoint subscribers\n"); */
	result = tracePointRegisterFunction(jvmti_env, "ts002 subscriber 1", ts002DataCallback1, ts002AlarmCallback1, NULL, &subscription1);
	if (result != JVMTI_ERROR_NONE) {
		error(env, result, "RegisterTraceSubscriber failed");
		return FALSE;
	}
	result = tracePointRegisterFunction(jvmti_env, "ts002 subscriber 2", ts002DataCallback2, ts002AlarmCallback2, NULL, &subscription2);
	if (result != JVMTI_ERROR_NONE) {
		error(env, result, "RegisterTraceSubscriber failed");
		return FALSE;
	}
	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts002_tryDeregisterTracePointSubscribers(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError result;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	result = tracePointDeregisterFunction(jvmti_env, subscription1);
	if (result != JVMTI_ERROR_NONE && result != JVMTI_ERROR_NOT_AVAILABLE) {
		error(env, result, "DeregisterTraceSubscriber failed");
		return FALSE;
	}
	result = tracePointDeregisterFunction(jvmti_env, subscription2);
	if (result != JVMTI_ERROR_NONE && result != JVMTI_ERROR_NOT_AVAILABLE) {
		error(env, result, "DeregisterTraceSubscriber failed");
		return FALSE;
	}
	return TRUE;
}

jint JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts002_getTracePointCount1(JNIEnv * jni_env, jclass clazz)
{
	return tracePointCount1;
}
jint JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts002_getTracePointCount2(JNIEnv * jni_env, jclass clazz)
{
	return tracePointCount2;
}
jboolean JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts002_hasAlarmTriggered1(JNIEnv * jni_env, jclass clazz)
{
	return alarmTriggered1;
}
jboolean JNICALL
Java_com_ibm_jvmti_tests_traceSubscription_ts002_hasAlarmTriggered2(JNIEnv * jni_env, jclass clazz)
{
	return alarmTriggered2;
}
