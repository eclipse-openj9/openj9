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


static agentEnv * env;
static jvmtiExtensionFunction subscribe  = NULL;
static jvmtiExtensionFunction unsubscribe  = NULL;

void *subscriptionID;
volatile static jint bufferCount = 0;
volatile static int alarmed = 0;

void *subscriptionID2;
volatile static jint bufferCount2 = 0;
volatile static int alarmed2 = 0;

void *subscriptionID3;
volatile static jint bufferCount3 = 0;
volatile static int alarmed3 = 0;

jvmtiError vgc001Callback(jvmtiEnv *env, void *data, jlong length, void *userData) 
{
	bufferCount++;
	return JVMTI_ERROR_NONE;
}

jvmtiError vgc001Alarm(jvmtiEnv *env, void *subscriptionID, void *userData) 
{
	alarmed = 1;
	return JVMTI_ERROR_NONE;
}

jvmtiError vgc001Callback2(jvmtiEnv *env, void *data, jlong length, void *userData) 
{
	bufferCount2++;
	return JVMTI_ERROR_NONE;
}

jvmtiError vgc001Alarm2(jvmtiEnv *env, void *subscriptionID, void *userData) 
{
	alarmed2 = 1;
	return JVMTI_ERROR_NONE;
}

jvmtiError vgc001Callback3(jvmtiEnv *env, void *data, jlong length, void *userData) 
{
	bufferCount3++;
	
	/* Let a few events through, then fail */
	if(bufferCount3 > 4) {
		return JVMTI_ERROR_INTERNAL;
	} else {
		return JVMTI_ERROR_NONE;
	}
}

jvmtiError vgc001Alarm3(jvmtiEnv *env, void *subscriptionID, void *userData) 
{
	alarmed3 = 1;
	return JVMTI_ERROR_NONE;
}

jint JNICALL
vgc001(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jint extensionCount;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	int i;

	JVMTI_ACCESS_FROM_AGENT(agent_env);
	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_FALSE;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_REGISTER_VERBOSEGC_SUBSCRIBER) == 0) {
			subscribe = extensionFunctions[i].func;
		} else if (strcmp(extensionFunctions[i].id, COM_IBM_DEREGISTER_VERBOSEGC_SUBSCRIBER) == 0) {
			unsubscribe = extensionFunctions[i].func;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return JNI_FALSE;
	}

	if (subscribe == NULL || unsubscribe == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "verbose GC subscription extension functions were not found");
		return JNI_FALSE;
	}

	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_tryRegisterVerboseGCSubscriber(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	err = subscribe(jvmti_env, "vgc001 subscriber", vgc001Callback, vgc001Alarm, NULL, &subscriptionID);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RegisterVerboseGCSubscriber failed");
		return FALSE;
	}
	bufferCount = 0;
	alarmed = 0;
	return TRUE;
}

jint JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_tryDeregisterVerboseGCSubscriber(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	err = unsubscribe(jvmti_env, subscriptionID, NULL);
	if (err != JVMTI_ERROR_NONE && err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(env, err, "DeregisterVerboseGCSubscriber failed");
		return -1;
	}
	return bufferCount;
}

jint JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_getBufferCount(JNIEnv * jni_env, jclass clazz)
{
	return bufferCount;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_hasAlarmed(JNIEnv * jni_env, jclass clazz)
{
	return alarmed ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_tryRegisterVerboseGCSubscriber2(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	err = subscribe(jvmti_env, "vgc001 subscriber 2", vgc001Callback2, vgc001Alarm2, NULL, &subscriptionID2);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RegisterVerboseGCSubscriber2 failed");
		return FALSE;
	}
	bufferCount2 = 0;
	alarmed2 = 0;
	return TRUE;
}

jint JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_tryDeregisterVerboseGCSubscriber2(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	err = unsubscribe(jvmti_env, subscriptionID2, NULL);
	if (err != JVMTI_ERROR_NONE && err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(env, err, "DeregisterVerboseGCSubscriber2 failed");
		return -1;
	}
	return bufferCount2;
}

jint JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_getBufferCount2(JNIEnv * jni_env, jclass clazz)
{
	return bufferCount2;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_hasAlarmed2(JNIEnv * jni_env, jclass clazz)
{
	return alarmed2 ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_tryRegisterVerboseGCSubscriber3(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	err = subscribe(jvmti_env, "vgc001 subscriber 3", vgc001Callback3, vgc001Alarm3, NULL, &subscriptionID3);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RegisterVerboseGCSubscriber3 failed");
		return FALSE;
	}
	bufferCount3 = 0;
	alarmed3 = 0;
	return TRUE;
}

jint JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_tryDeregisterVerboseGCSubscriber3(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	err = unsubscribe(jvmti_env, subscriptionID3, NULL);
	if (err != JVMTI_ERROR_NONE && err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(env, err, "DeregisterVerboseGCSubscriber3 failed");
		return -1;
	}
	return bufferCount3;
}

jint JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_getBufferCount3(JNIEnv * jni_env, jclass clazz)
{
	return bufferCount3;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_verboseGC_vgc001_hasAlarmed3(JNIEnv * jni_env, jclass clazz)
{
	return alarmed3? JNI_TRUE : JNI_FALSE;
}
