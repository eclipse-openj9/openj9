/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

/* the standard agent test context which lives for the duration of the test - this is supposed to be held for error logging */
static agentEnv *env;
/* the callback method of the event JVMTI_EVENT_SAMPLED_OBJECT_ALLOC */
static void JNICALL sampledObjectAlloc(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jobject object, jclass object_klass, jlong size); 
/* the number of time the callback sampledObjectAlloc invoked */
static jint soaeResult = 0;

#if JAVA_SPEC_VERSION >= 11

jint JNICALL
soae001(agentEnv *agent_env, char *args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err = JVMTI_ERROR_NONE;
	jint result = JNI_OK;

	env = agent_env;

	/* Set the JVMTI_EVENT_SAMPLED_OBJECT_ALLOC event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.SampledObjectAlloc = sampledObjectAlloc;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (JVMTI_ERROR_NONE != err) {
		error(agent_env, err, "Failed to set callback for JVMTI_EVENT_SAMPLED_OBJECT_ALLOC event");
		result = JNI_ERR;
	} else {
		/* we require the can_generate_sampled_object_alloc_events capability if we want to enable allocation callbacks */
		memset(&capabilities, 0, sizeof(jvmtiCapabilities));
		capabilities.can_generate_sampled_object_alloc_events = 1;
		err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
		if (JVMTI_ERROR_NONE != err) {
			error(agent_env, err, "Failed to add capabilities can_generate_sampled_object_alloc_events");
			result = JNI_ERR;
		}
	}
	
	return JNI_OK;
}

static void JNICALL
sampledObjectAlloc(jvmtiEnv *jvmti_env,
	JNIEnv *jni_env,
	jthread thread,
	jobject object,
	jclass object_klass,
	jlong size) 
{
	soaeResult++;
}

void JNICALL
Java_com_ibm_jvmti_tests_samplingObjectAllocation_soae001_reset(JNIEnv *jni_env, jclass cls)
{
	soaeResult = 0;
}

jint JNICALL
Java_com_ibm_jvmti_tests_samplingObjectAllocation_soae001_enable(JNIEnv *jni_env, jclass cls)
{
	jint result = JNI_OK;
	jvmtiError err = JVMTI_ERROR_NONE;
	JVMTI_ACCESS_FROM_AGENT(env);
	
	/* Enable the JVMTI_EVENT_SAMPLED_OBJECT_ALLOC callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable JVMTI_EVENT_SAMPLED_OBJECT_ALLOC event");
		result = JNI_ERR;
	}
	
	return result;
}

jint JNICALL
Java_com_ibm_jvmti_tests_samplingObjectAllocation_soae001_disable(JNIEnv *jni_env, jclass cls)
{
	jint result = JNI_OK;
	jvmtiError err = JVMTI_ERROR_NONE;
	JVMTI_ACCESS_FROM_AGENT(env);
	
	/* Enable the JVMTI_EVENT_SAMPLED_OBJECT_ALLOC callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to disable JVMTI_EVENT_SAMPLED_OBJECT_ALLOC event");
		result = JNI_ERR;
	}
	
	return result;
}

jint JNICALL
Java_com_ibm_jvmti_tests_samplingObjectAllocation_soae001_check(JNIEnv *jni_env, jclass cls)
{
	return soaeResult;
}
#endif /* JAVA_SPEC_VERSION >= 11 */
