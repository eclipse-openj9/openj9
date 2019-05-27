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
#include <string.h>

#include "jvmti_test.h"

#define EXCEPTION_METHOD "generateException"

static agentEnv * env;

static void JNICALL exceptionThrowCB(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception, jmethodID catch_method, jlocation catch_location);

static jboolean exceptionEventFired;

jint JNICALL
ee001(agentEnv * agent_env, char * args)
{
    JVMTI_ACCESS_FROM_AGENT(agent_env);
    jvmtiEventCallbacks callbacks;
    jvmtiCapabilities capabilities;
    jvmtiError err;

    env = agent_env;

    exceptionEventFired = JNI_FALSE;

    memset(&capabilities, 0, sizeof(jvmtiCapabilities));
    capabilities.can_generate_exception_events = 1;
    err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
    if (err != JVMTI_ERROR_NONE) {
        error(env, err, "Failed to add capabilities");
        return JNI_ERR;
    }

    /* Set the Exception event callback */
    memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
    callbacks.Exception = exceptionThrowCB;
    err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
    if (err != JVMTI_ERROR_NONE) {
        error(env, err, "Failed to set callback for Exception events");
        return JNI_ERR;
    }

    /* Enable the JVMTI_EVENT_EXCEPTION callback */
    err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, NULL);
    if (err != JVMTI_ERROR_NONE) {
        error(env, err, "Failed to enable Exception event");
        return JNI_ERR;
    } 

    return JNI_OK;
}

static void JNICALL
exceptionThrowCB(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception, jmethodID catch_method, jlocation catch_location)
{
    char *name_ptr;
    jvmtiError err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, NULL, NULL);
    if (err != JVMTI_ERROR_NONE) {
        error(env, err, "Failed to GetMethodName");
        return;
    }
    if (0 == strcmp(name_ptr, EXCEPTION_METHOD)) {
        if (JNI_FALSE == exceptionEventFired) {
            exceptionEventFired = JNI_TRUE;
        } else {
            error(env, JVMTI_ERROR_INTERNAL, "Failed to avoid duplicate event callback for Exception event");
        }
    }
    
    return;	
}


void JNICALL
Java_com_ibm_jvmti_tests_eventException_ee001_invoke(JNIEnv *jni_env, jclass cls, jobject method)
{
    jmethodID mid = (*jni_env)->FromReflectedMethod(jni_env, method);
    (*jni_env)->CallStaticVoidMethod(jni_env, cls, mid);
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_eventException_ee001_check(JNIEnv *jni_env, jclass cls)
{
    return exceptionEventFired;
}
