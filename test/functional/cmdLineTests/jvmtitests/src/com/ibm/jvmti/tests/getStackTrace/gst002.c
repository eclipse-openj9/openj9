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

jint JNICALL
gst002(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiCapabilities capabilities;
	jvmtiError err;

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getStackTrace_gst002_check(JNIEnv *jni_env, jclass cls, jthread t)
{
	char pad1[256];
	jvmtiFrameInfo frames[2];  /* keep on top of stack */
	char pad0[256];
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err;
    jint count = 0;
    jint  i;

    memset(pad0, 0x00, 256);
	memset(frames, 0x00, sizeof(jvmtiFrameInfo) * 2);
	memset(pad1, 0x00, 256);

    err = (*jvmti_env)->GetStackTrace(jvmti_env, t, 0, 2, frames, &count);

    if (err != JVMTI_ERROR_THREAD_NOT_ALIVE) {
    	error(env, err, "GetStackTrace did not return JVMTI_ERROR_THREAD_NOT_ALIVE as expected");
        return JNI_FALSE;
    }

    err = JVMTI_ERROR_NONE;

    for (i = 0; i < 256; i++) {
    	if (pad0[i] != 0) {
    		error(env, err, "GetStackTrace, frame structure overflow onto pad0 at offset %d.", i);
    		return JNI_FALSE;
    	}
    	if (pad1[i] != 0) {
    		error(env, err, "GetStackTrace, frame structure overflow onto pad1 at offset %d.", i);
    		return JNI_FALSE;
    	}
    }

    if (count != 0) {
    	error(env, JVMTI_ERROR_INTERNAL, "Expected 0 frames, returned %d frames", count);
    	return JNI_FALSE;
    }

    return JNI_TRUE;
}
