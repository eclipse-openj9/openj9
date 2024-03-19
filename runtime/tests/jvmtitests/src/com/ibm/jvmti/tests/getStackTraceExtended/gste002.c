/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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

#if JAVA_SPEC_VERSION >= 21
#define JVMTI_TEST_GSTE002_MAX_FRAMES 50

static agentEnv *env = NULL;

jint JNICALL
gste002(agentEnv *agent_env, char *args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	env = agent_env;
	return JNI_OK;
}

static jvmtiExtensionFunction getStackTraceExtended = NULL;

jboolean JNICALL
Java_com_ibm_jvmti_tests_getStackTraceExtended_gste002_anyJittedFrame(JNIEnv *jniEnv, jclass clazz, jthread t)
{
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	jvmtiFrameInfoExtended *frameBuffer = NULL;
	jint extensionCount = 0;
	jvmtiExtensionFunctionInfo *extensionFunctions = NULL;
	int err = JVMTI_ERROR_NONE;
	int numFrames = 0;
	int i = 0;
	int numJitted = 0;
	jboolean rc = JNI_TRUE;

	if (NULL == getStackTraceExtended) {
		err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);

		if (JVMTI_ERROR_NONE != err) {
			error(env, err, "Failed GetExtensionFunctions");
			return 0;
		}

		for (int i = 0; i < extensionCount; i++) {
			if (strcmp(extensionFunctions[i].id, COM_IBM_GET_STACK_TRACE_EXTENDED) == 0) {
				getStackTraceExtended = extensionFunctions[i].func;
			}
		}

		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensionFunctions);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to Deallocate extension functions");
			return 0;
		}

		if (NULL == getStackTraceExtended) {
			error(env, JVMTI_ERROR_NOT_FOUND, "GetStackTraceExtended extension was not found");
			return 0;
		}
	}

	frameBuffer = (jvmtiFrameInfoExtended *)malloc(JVMTI_TEST_GSTE002_MAX_FRAMES * sizeof(jvmtiFrameInfoExtended));

	if (NULL == frameBuffer) {
		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Failed to allocate array of jvmtiFrameInfo");
		return 0;
	}

	if (NULL == t) {
		err = (*jvmti_env)->GetCurrentThread(jvmti_env, &t);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "GetCurrentThread failed");
			return 0;
		}
	}

	err = (getStackTraceExtended)(
			jvmti_env,
			COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO,
			t,
			0,
			JVMTI_TEST_GSTE002_MAX_FRAMES,
			(void *)frameBuffer,
			&numFrames);

	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "getStackTraceExtended failed");
		return 0;
	}


	for (i = 0; i < numFrames; ++i) {
		if (frameBuffer[i].type != COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED) {
			++numJitted;
		}
	}

	if (0 == numJitted) {
		rc = JNI_FALSE;
		error(
			env,
			JVMTI_ERROR_INTERNAL,
			"getStackTraceExtended: Couldn't find jitted frames");
	}

	free(frameBuffer);
	return rc;
}
#endif /* JAVA_SPEC_VERSION >= 21 */
