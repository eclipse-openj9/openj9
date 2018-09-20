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

#include "ibmjvmti.h"
#include "jvmti_test.h"

static agentEnv * env;

static jint getVariableSlotNumber(JNIEnv *jni_env, jclass clazz, char * varName, char * methodName, char * signature);
static jint digForStackDepth(jthread thread, char *methodName, char * methodSignature);

jint JNICALL
gosl001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiCapabilities capabilities;
	jvmtiError err;

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_access_local_variables = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_getOrSetLocal_gosl001_getInt(
		JNIEnv *jni_env,
		jclass clazz,
		jthread thread,
		jint depth,
		jclass methodClass,
		jstring s_methodName, jstring s_methodSignature,
		jstring s_varName, jint expectedInt)
{
	JVMTI_ACCESS_FROM_AGENT(env);
    jint        intValue = 0;
    jvmtiError  err = JVMTI_ERROR_NONE;
    jint 		slot;

    char * methodName, * methodSignature, * varName;


    methodName = (char *)(*jni_env)->GetStringUTFChars(jni_env, s_methodName, NULL);
	if (methodName == NULL) {
		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Unable to allocate methodName");
		return JNI_FALSE;
	}
	methodSignature = (char *)(*jni_env)->GetStringUTFChars(jni_env, s_methodSignature, NULL);
	if (methodSignature == NULL) {
		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Unable to allocate methodSignature");
		return JNI_FALSE;
	}
	varName = (char *)(*jni_env)->GetStringUTFChars(jni_env, s_varName, NULL);
	if (varName == NULL) {
		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Unable to allocate varName");
		return JNI_FALSE;
	}


    slot = getVariableSlotNumber(jni_env, methodClass, varName, methodName, methodSignature);
    if (slot == -1) {
    	error(env, err, "getVariableSlotNumber() failed retrieving slot index");
    	return JNI_FALSE;
    }

    depth = digForStackDepth(thread, methodName, methodSignature);
    if (depth == -1) {
    	error(env, err, "Unable to determine stack depth for method [%s][%s]", methodName, methodSignature);
    	return JNI_FALSE;
    }

	err = (*jvmti_env)->GetLocalInt(jvmti_env, thread, depth, slot, &intValue);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "GetLocalInt() failed");
		return JNI_FALSE;
	}

	tprintf(env, 0, "\tint retrieved=%d\n", intValue);

    return JNI_TRUE;
}

static jint
digForStackDepth(jthread thread, char *methodName, char * methodSignature)
{
	jint i;
	jvmtiError  err;
	jvmtiFrameInfo frames[20];
	JVMTI_ACCESS_FROM_AGENT(env);
    jint count = 0;
    jint depth = -1;
    char *name, *sig;


    err = (*jvmti_env)->GetStackTrace(jvmti_env, thread, 0, 20, frames, &count);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetStackTrace");
        return -1;
    }


    for (i = 0; i < count; i++) {
    	err = (*jvmti_env)->GetMethodName(jvmti_env, frames[i].method, &name, &sig, NULL);
    	if (err != JVMTI_ERROR_NONE) {
    		error(env, err, "Failed to GetMethodName for frame at stack depth %d", i);
    		return -1;
    	}
    	if (!strcmp(name, methodName) && !strcmp(sig, methodSignature)) {
			depth = i;
    		i = count;
    	}
    	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)name);
    	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)sig);
    }

    return depth;
}

static jint
getVariableSlotNumber(JNIEnv *jni_env, jclass clazz, char * varName, char * methodName, char * signature)
{
	jvmtiError  err;
	JVMTI_ACCESS_FROM_AGENT(env);
	jint entryCount;
	jvmtiLocalVariableEntry * table;
	jint slotIndex = -1;
	jmethodID method;
	jint i;

	method = (*jni_env)->GetMethodID(jni_env, clazz, methodName, signature);

	err = (*jvmti_env)->GetLocalVariableTable(jvmti_env, method, &entryCount, &table);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "GetLocalVariableTable() failed");
		return JNI_FALSE;
	}

	for (i = 0; i < entryCount; i++) {
		if (strcmp(table[i].name, varName) == 0) {
			slotIndex = table[i].slot;
			/*printf("got slot at i=%d, slot=%d\n", i, slotIndex);*/
		}
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)table[i].name);
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)table[i].signature);
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)table[i].generic_signature);
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) table);

	return slotIndex;
}


