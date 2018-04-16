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
gst001(agentEnv * agent_env, char * args)
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
Java_com_ibm_jvmti_tests_getStackTrace_gst001_check(JNIEnv *jni_env, jclass cls)
{
	char pad1[256];
	jvmtiFrameInfo frames[2];  /* keep on top of stack */
	char pad0[256];
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err;
    jmethodID mid_run, mid_check;
    jint count = 0;
    jint i = 0;

    memset(pad0, 0x00, 256);
	memset(frames, 0x00, sizeof(jvmtiFrameInfo) * 2);
	memset(pad1, 0x00, 256);

    err = (*jvmti_env)->GetStackTrace(jvmti_env, NULL, 0, 2, frames, &count);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetStackTrace");
        return JNI_FALSE;
    }

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

    mid_run = (*jni_env)->GetMethodID(jni_env, cls, "testGetStackTrace", "()Z");
    if (mid_run == NULL) {
        error(env, err, "Failed to GetMethodID (testGetStackTrace)");
        return JNI_FALSE;
    }

    mid_check = (*jni_env)->GetStaticMethodID(jni_env, cls, "check", "()Z");
    if (mid_check == NULL) {
    	error(env, err, "Failed to GetStaticMethodID (check)");
    	return JNI_FALSE;
    }

    if (count < 2) {
    	error(env, err, "Expected 2 frames, returned %d frames", count);
    	return JNI_FALSE;
    }

    if (frames[0].method != mid_check) {
		error(env, err, "Invalid first frame. frames[0].method=%p != %p", frames[0].method, mid_check);
        return JNI_FALSE;
    }

    if (frames[1].method != mid_run) {
		error(env, err, "Invalid second frame. frames[1].method=%p != %p", frames[1].method, mid_run);
        return JNI_FALSE;
    }

    if (frames[1].method == mid_check && frames[0].method == mid_run) {
        error(env, err, "Frames in reverse order! Deepest should be first.");
        return JNI_FALSE;
    }

    return JNI_TRUE;
}
