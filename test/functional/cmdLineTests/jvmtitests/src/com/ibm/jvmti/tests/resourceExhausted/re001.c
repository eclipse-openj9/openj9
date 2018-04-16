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

static agentEnv * env;

static jboolean callbackPassed = JNI_FALSE;
static jint     expectedEventBitmask = 0;

static void JNICALL re001_callback(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jint flags, const void* reserved, const char* description);


static jint JNICALL
resourceExhaustedSetup(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env; 

	if (!ensureVersion(agent_env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}   	

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_resource_exhaustion_heap_events = 1;
	capabilities.can_generate_resource_exhaustion_threads_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}						


	/* Set the ResourceExhausted event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ResourceExhausted = re001_callback;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for Resource Exhaust events");
		return JNI_ERR;
	}


	/* Enable the ResourceExhausted callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_RESOURCE_EXHAUSTED, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable Resource Exhausted event");
		return JNI_ERR;
	} 



	return JNI_OK;
}

jint JNICALL
re001(agentEnv * agent_env, char * args)
{
	jint ret;
	
	ret = resourceExhaustedSetup(agent_env, args);
	
	expectedEventBitmask = JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP;
	
	return ret;	
}


jint JNICALL
re002(agentEnv * agent_env, char * args)
{
	jint ret;
	
	ret = resourceExhaustedSetup(agent_env, args);
	
	expectedEventBitmask = JVMTI_RESOURCE_EXHAUSTED_THREADS;
	
	return ret;	
}



static void JNICALL 
re001_callback(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jint flags, const void* reserved, const char* description)
{
	/* printf("in callback flags = 0x%x [%s]\n", flags, description); */
	
	if (flags != expectedEventBitmask) { 
		callbackPassed = JNI_FALSE;
		error(env, JVMTI_ERROR_INTERNAL, "ResourceExhausted Callback with flags [0x%x] instead of expected [0x%x]", 
			  flags, expectedEventBitmask);  
	} else {
		callbackPassed = JNI_TRUE;
	}
}



static jboolean
hasBeenCalledBack(void)
{
	jboolean ret;
	
	ret = callbackPassed;
	
	callbackPassed = JNI_FALSE;
		
	return ret;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_resourceExhausted_re001_hasBeenCalledBack(JNIEnv * jni_env, jclass c)
{
	return hasBeenCalledBack();
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_resourceExhausted_re002_hasBeenCalledBack(JNIEnv * jni_env, jclass c)
{
	return hasBeenCalledBack();
}

