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
#define JVMTI_TEST_GTLSTE002_MAX_FRAMES 50

static agentEnv *env = NULL;

jint JNICALL
gtlste002(agentEnv *agent_env, char *args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	env = agent_env;
	return JNI_OK;
}

static jvmtiExtensionFunction getThreadListStackTracesExtended = NULL;

jboolean JNICALL
Java_com_ibm_jvmti_tests_getThreadListStackTracesExtended_gtlste002_anyJittedFrame(JNIEnv * jni_env, jclass clazz, jthread t)
{
	jvmtiEnv *jvmti_env = env->jvmtiEnv;

	jint extensionCount = 0;
	jvmtiExtensionFunctionInfo *extensionFunctions = NULL;
	jvmtiStackInfoExtended *psiExt = NULL;
	int err = JVMTI_ERROR_NONE;
	int numFrames = 0;
	int i = 0;
	int numJitted = 0;
	jboolean rc = JNI_TRUE;

	if (NULL == getThreadListStackTracesExtended ) {
		err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);

		if (JVMTI_ERROR_NONE != err) {
			error(env, err, "Failed GetExtensionFunctions");
			return JNI_FALSE;
		}

		for (int i = 0; i < extensionCount; i++) {
			if (strcmp(extensionFunctions[i].id, COM_IBM_GET_THREAD_LIST_STACK_TRACES_EXTENDED) == 0) {
				getThreadListStackTracesExtended = extensionFunctions[i].func;
			}
		}

		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensionFunctions);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to Deallocate extension functions");
			return JNI_FALSE;
		}

		if (NULL == getThreadListStackTracesExtended) {
			error(env, JVMTI_ERROR_NOT_FOUND, "GetThreadListStackTracesExtended extension was not found");
			return 0;
		}
	}

	if (NULL == t) {
		err = (*jvmti_env)->GetCurrentThread(jvmti_env, &t);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "GetCurrentThread failed");
			return JNI_FALSE;
		}
	}

	jthread threads[1] = {t};

	err = (getThreadListStackTracesExtended)(
			jvmti_env,
			COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO,
			1,
			threads,
			JVMTI_TEST_GTLSTE002_MAX_FRAMES,
			&psiExt);

	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "getThreadListStackTracesExtended failed");
		return JNI_FALSE;
	}

	for (i = 0; i < psiExt[0].frame_count; ++i) {
		if (psiExt[0].frame_buffer[i].type != COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED) {
			++numJitted;
		}
	}

	if (0 == numJitted) {
		rc = JNI_FALSE;
		error(
			env,
			JVMTI_ERROR_INTERNAL,
			"getThreadListStackTracesExtended: Couldn't find jitted frames");
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)psiExt);
	return rc;
}
#endif /* JAVA_SPEC_VERSION >= 21 */
