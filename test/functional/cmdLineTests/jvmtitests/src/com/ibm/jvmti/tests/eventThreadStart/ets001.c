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

static void JNICALL threadStartCB(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread);

static jthread receivedThread;
static jboolean threadStartEventFired;

jint JNICALL
ets001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiError err;                                

	env = agent_env;

	threadStartEventFired = JNI_FALSE;

	/* Set the CompiledMethodLoad event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ThreadStart = threadStartCB;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for CompiledMethodLoad events");
		return JNI_ERR;
	}

	/* Enable the JVMTI_EVENT_THREAD_START callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable ThreadStart event");
		return JNI_ERR;
	} 

	return JNI_OK;
}

static void JNICALL
threadStartCB(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread)
{
	jvmtiThreadInfo info;
	jvmtiError err;
	
	err = (*jvmti_env)->GetThreadInfo(jvmti_env, thread, &info);
	if (err != JVMTI_ERROR_NONE) {
		/* callback can fire during start phase, where GetThreadInfo is not allowed */
		if (err != JVMTI_ERROR_WRONG_PHASE) {
			error(env, err, "Failed to GetThreadInfo");
		}
		return;
	} 
	
	if (strcmp(info.name, "ets001 thread")) {
		threadStartEventFired = JNI_TRUE;
	}
	
	return;	
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_eventThreadStart_ets001_check(JNIEnv *jni_env, jclass cls, jthread t)
{
	return threadStartEventFired;
}
