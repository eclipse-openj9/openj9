/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

/* The VMStart event callback method */
static void JNICALL vmStart(jvmtiEnv *jvmti_env, JNIEnv *jni_env);
/* The number of times that the VMStart event callback has been invoked */
static jint numberVMStartCallbackInvoked = 0;

jint JNICALL
vmstart001(agentEnv *agent_env, char *args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jint result = JNI_OK;
	jvmtiCapabilities initialCapabilities;

	memset(&initialCapabilities, 0, sizeof(jvmtiCapabilities));
	err = (*jvmti_env)->GetPotentialCapabilities(jvmti_env, &initialCapabilities);
	if (JVMTI_ERROR_NONE != err) {
		error(agent_env, err, "Failed to GetPotentialCapabilities");
		result = JNI_ERR;
#if JAVA_SPEC_VERSION >= 11
	} else if (0 == initialCapabilities.can_generate_early_vmstart) {
		error(agent_env, JVMTI_ERROR_UNSUPPORTED_VERSION, "can_generate_early_vmstart should be available in onload phase");
		result = JNI_ERR;
#endif /* JAVA_SPEC_VERSION >= 11 */
	} else {
		jvmtiEventCallbacks callbacks;

		/* Set the VMStart event callback */
		memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
		callbacks.VMStart = vmStart;
		err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
		if (JVMTI_ERROR_NONE != err) {
			error(agent_env, err, "Failed to set callback for VMStart event");
			result = JNI_ERR;
#if JAVA_SPEC_VERSION >= 11
		} else {
			const char* enableEarlyVMStart = agent_env->testArgs;

			if (NULL != enableEarlyVMStart) {
				if (0 == strcmp(enableEarlyVMStart, "can_generate_early_vmstart")) {
					jvmtiCapabilities capabilities;
					/* Require the can_generate_early_vmstart capability to enable early vmstart callback */
					memset(&capabilities, 0, sizeof(jvmtiCapabilities));
					capabilities.can_generate_early_vmstart = 1;
					err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
					if (JVMTI_ERROR_NONE != err) {
						error(agent_env, err, "Failed to add capability can_generate_early_vmstart");
						result = JNI_ERR;
					}
				} else {
					error(agent_env, JVMTI_ERROR_UNSUPPORTED_VERSION, "Unsupported arguments");
					result = JNI_ERR;
				}
			} else {
				/* This is vmstart001 test run w/o enabling can_generate_early_vmstart */
			}
#endif /* JAVA_SPEC_VERSION >= 11 */
		}

		if (JVMTI_ERROR_NONE == err) {
			/* Enable the JVMTI_EVENT_VM_START callback */
			err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_VM_START, NULL);
			if (JVMTI_ERROR_NONE != err) {
				error(agent_env, err, "Failed to enable JVMTI_EVENT_VM_START event");
				result = JNI_ERR;
			}
		}
	}

	return result;
}

static void JNICALL
vmStart(jvmtiEnv *jvmti_env, JNIEnv *jni_env)
{
	numberVMStartCallbackInvoked += 1;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_eventVMStart_vmstart001_check(JNIEnv *jni_env, jclass cls)
{
	jboolean result = JNI_FALSE;
	if (1 == numberVMStartCallbackInvoked) {
		/* The VMStart callback should be invoked just once. */
		result = JNI_TRUE;
	}

	return result;
}
