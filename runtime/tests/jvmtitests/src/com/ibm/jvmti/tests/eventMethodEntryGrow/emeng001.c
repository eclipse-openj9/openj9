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
#include <string.h>

#include "jvmti_test.h"
#include "ibmjvmti.h"

static agentEnv * env;

static void JNICALL cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method);

jint JNICALL
emeng001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_method_entry_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.MethodEntry = cbMethodEntry;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for MethodEntry callback");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable MethodEntry event");
		return JNI_ERR;
	}

	return JNI_OK;
}

static void JNICALL
cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method)
{
	char *name_ptr = NULL;
	jvmtiError err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, NULL, NULL);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetMethodName");
		return;
    }
	if (0 == strcmp(name_ptr, "emeng001NativeMethod")) {
		jclass c = NULL;
		jmethodID mid = NULL;
		err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to disable MethodEntry event");
			return;
		}
		err = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &c);
	    if (err != JVMTI_ERROR_NONE) {
	    	error(env, err, "Failed to GetMethodDeclaringClass");
			return;
	    }
		mid = (*jni_env)->GetStaticMethodID(jni_env, c, "growStack", "()V");
		if (NULL == mid) {
	    	error(env, err, "Failed to GetStaticMethodID");
			return;
		}
		(*jni_env)->CallStaticVoidMethod(jni_env, c, mid);
		if ((*jni_env)->ExceptionOccurred(jni_env)) {
	    	error(env, err, "Failed during grow");
			return;
		}
	}
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)name_ptr);

	return;
}


void JNICALL
Java_com_ibm_jvmti_tests_eventMethodEntryGrow_emeng001_emeng001NativeMethod(JNIEnv *env, jobject rcv, jobject o, jint i, jlong l, jobject o2)
{
	jclass c = (*env)->GetObjectClass(env, rcv);
	if (NULL != c) {
		jmethodID mid = (*env)->GetMethodID(env, c, "verifyValues", "(Ljava/lang/String;IJLjava/lang/String;)V");
		if (NULL != mid) {
			(*env)->CallVoidMethod(env, rcv, mid, o, i, l, o2);
		}
	}
}
