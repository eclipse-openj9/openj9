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

static void JNICALL cbMethodExit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value);

static jint javaMethodEntryCount = 0;
static jint nativeMethodEntryCount = 0;

jint JNICALL
emex001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;
                              
	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_method_exit_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}	
	
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.MethodExit = cbMethodExit;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for MethodExit events");
		return JNI_ERR;
	}					
	
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable MethodExit event");
		return JNI_ERR;
	} 
	
	return JNI_OK;
}

static void JNICALL
cbMethodExit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value)
{
	jvmtiError err;
	char *name_ptr, *signature_ptr, *generic_ptr;
				
	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, &signature_ptr, &generic_ptr);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetMethodName");
		return;	
    }       
    
	if (!strcmp(name_ptr, "sampleJavaMethod") && !strcmp(signature_ptr, "(I)I")) {
    	javaMethodEntryCount++;	
    }
	
	if (!strcmp(name_ptr, "sampleNativeMethod") && !strcmp(signature_ptr, "(I)I")) {
    	nativeMethodEntryCount++;	
    }
		
	return;	
}


jint JNICALL
Java_com_ibm_jvmti_tests_eventMethodExit_emex001_sampleNativeMethod(JNIEnv *jni_env, jclass cls, jint foo)
{
	return 100 + foo;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_eventMethodExit_emex001_checkJavaMethodExit(JNIEnv *jni_env, jclass cls)
{
	if (javaMethodEntryCount > 0) {
		return JNI_TRUE;
	}
	
	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_eventMethodExit_emex001_checkNativeMethodExit(JNIEnv *jni_env, jclass cls)
{
	if (nativeMethodEntryCount > 0) {
		return JNI_TRUE;
	}
	
	return JNI_FALSE;
}

