/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH
 *Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include <string.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

static agentEnv *env;

static void JNICALL testCallback(jvmtiEnv *jvmti_env, char *className, jboolean *test_result);

jint JNICALL ptjb001(agentEnv *agent_env, char *args) {
    jint extensionCount;
    jint patchToJavaBaseIndex = -1;
    jvmtiExtensionEventInfo *extensionEvents;
    int err;
    jvmtiError tiErr;
    int i;

    JVMTI_ACCESS_FROM_AGENT(agent_env);
    env = agent_env;

    err = (*jvmti_env)->GetExtensionEvents(jvmti_env, &extensionCount, &extensionEvents);
    if (JVMTI_ERROR_NONE != err) {
        return JNI_ERR;
    }

    for (i = 0; i < extensionCount; i++) {
        if (strcmp(extensionEvents[i].id, COM_OPENJ9_PATCH_TO_JAVABASE) == 0) {
            patchToJavaBaseIndex = extensionEvents[i].extension_event_index;
        }
    }

    if (-1 == patchToJavaBaseIndex) {
        error(env, JVMTI_ERROR_NOT_FOUND,
              "ptjb001:patchToJavaBase extension event was not found");
        return JNI_ERR;
    }

    /* set extensions callback to run tests */
    tiErr = (*jvmti_env)
                ->SetExtensionEventCallback(jvmti_env, patchToJavaBaseIndex,
                                            (jvmtiExtensionEvent)testCallback);
    if (JVMTI_ERROR_NONE != tiErr) {
        error(env, tiErr,
              "ptjb001:patchToJavaBase extension callback could not be set");
        return JNI_ERR;
    }

    err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensionEvents);
    if (err != JVMTI_ERROR_NONE) {
        error(env, err, "ptjb001:Failed to Deallocate extension functions");
        return JNI_ERR;
    }

    return JNI_OK;
}

/*
 * Test callback - assign test classes to correct module
 */
static void JNICALL testCallback(jvmtiEnv *jvmti_env, char *className, jboolean *test_result) {
    jboolean result = JNI_FALSE; // (unnamed)

    tprintf(env, 0, "ptjb001 jvmti callback: Callback class name is: %s\n", className);

	/* Assign BootstrapClassJavaBase to load into java.base */
    if (0 == strcmp(className,
                    "com/ibm/jvmti/tests/patchToJavaBase/bootstrapClass/BootstrapClassJavaBase")) {
        tprintf(env, 0, "ptjb001 jvmti callback: Assign %s to java.base module\n", className);
		result = JNI_TRUE; // (java.base)
	/* For all other classes, load into unnamed module */
    } else {
        tprintf(env, 0, "ptjb001 jvmti callback: Default: set %s to unnamed module\n", className);
    }

    /* pass result back to vm */
	*test_result = result;
}