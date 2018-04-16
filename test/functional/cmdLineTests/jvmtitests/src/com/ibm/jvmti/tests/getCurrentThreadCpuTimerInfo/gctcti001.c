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

jint JNICALL
gctcti001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiCapabilities capabilities;
	jvmtiError err;
	
	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_get_current_thread_cpu_time = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		if (err == JVMTI_ERROR_NOT_AVAILABLE) { 
			softError(env, err, "Failed to add capabilities. Expected to fail on Linux PPC32 and Linux S390.");
			return JNI_OK;
		} else {
			error(env, err, "Failed to add capabilities.");
			return JNI_ERR;				
		}
	}						

	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_getCurrentThreadCpuTimerInfo_gctcti001_verifyTimerInfo(JNIEnv *jni_env, jclass cls)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiTimerInfo info;
	jvmtiError err;
	
	memset(&info, 0x00, sizeof(info));
	
	err = (*jvmti_env)->GetCurrentThreadCpuTimerInfo(jvmti_env, &info);
	
	if (info.max_value != ((jlong) - 1)) {
		error(env, err, "incorrect info.max_value");
		return JNI_FALSE; 	
	}
	
	if (info.may_skip_forward != JNI_FALSE) {
		error(env, err, "incorrect info.may_skip_forward");
		return JNI_FALSE;	
	}
	
	if (info.may_skip_backward != JNI_FALSE) {
		error(env, err, "incorrect info.may_skip_backward");
		return JNI_FALSE;	
	}
	
	if (info.kind != JVMTI_TIMER_TOTAL_CPU) {
		error(env, err, "incorrect info.kind");
		return JNI_FALSE;	
	}
	
	
	
	return JNI_TRUE;
}

