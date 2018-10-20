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

#include "jvmti_test.h"
#include <string.h>
#if defined(_MSC_VER)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif /* _MSC_VER */


static agentEnv * env;
static jmethodID catchMethod = NULL;
static jlocation catchLocation = 0;

static int singleStep = 1;
static int stepInCatch = 0;

static void JNICALL
cbSingleStep(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location)
{
}

static void JNICALL
cbException(jvmtiEnv *jvmti_env,
			JNIEnv* jni_env,
			jthread thread,
			jmethodID method,
			jlocation location,
			jobject exception,
			jmethodID catch_method,
			jlocation catch_location)
{
	jclass clazz = NULL;
	jvmtiError err = JVMTI_ERROR_NONE;
	catchMethod = catch_method;
	catchLocation = catch_location;
	err = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, catch_method, &clazz);
	if (JVMTI_ERROR_NONE == err) {
		jobject catcher = (*jni_env)->ToReflectedMethod(jni_env, clazz, catch_method, JNI_TRUE);
		if (NULL != catcher) {
			jfieldID fid = (*jni_env)->GetStaticFieldID(jni_env, clazz, "catcher", "Ljava/lang/reflect/Method;");
			if (NULL != fid) {
				(*jni_env)->SetStaticObjectField(jni_env, clazz, fid, catcher);
			} else {
				error(env, err, "Failed to get field id");		
			}
		} else {
			error(env, err, "Failed to get reflect method");			
		}
	} else {
		error(env, err, "Failed to get method class");
	}
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_EXCEPTION, thread);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to disable exception event");
	}
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION_CATCH, thread);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable exception catch event");
	}
}

static void JNICALL
cbExceptionCatch(jvmtiEnv *jvmti_env,
				JNIEnv* jni_env,
				jthread thread,
				jmethodID method,
				jlocation location,
				jobject exception)
{
	jvmtiError err = JVMTI_ERROR_NONE;
	if ((method != catchMethod) || (location != catchLocation)) {
		jclass clazz = NULL;
		jfieldID fid = NULL;
		error(env, err, "FAIL: Catching method or location inconsistent at catch point");
		(*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &clazz);
		fid = (*jni_env)->GetStaticFieldID(jni_env, clazz, "catcher", "Ljava/lang/reflect/Method;");
		(*jni_env)->SetStaticObjectField(jni_env, clazz, fid, NULL);
	}
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_EXCEPTION_CATCH, thread);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to disable exception catch event");
	}
	if (stepInCatch) {
		err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_SINGLE_STEP, NULL);
		if ( err != JVMTI_ERROR_NONE ) {
			error(env, err, "Failed to enable step event");
		}
	}
}


jint JNICALL
decomp003(agentEnv * _env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(_env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;

	env = _env;

	if (!ensureVersion(env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}

	if (NULL != args) {
		if (0 == strcasecmp(args, "nostep")) {
			singleStep = 0;
		} else if (0 == strcasecmp(args, "stepincatch")) {
			stepInCatch = 1;
		} else {
			error(env, err, "Unknown option");
			return JNI_ERR;		
		}
	}

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	if (singleStep) {
		capabilities.can_generate_single_step_events = 1;
	}
	capabilities.can_generate_exception_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}	
	
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	if (singleStep) {
		callbacks.SingleStep = cbSingleStep;
	}
	callbacks.Exception = cbException;
	callbacks.ExceptionCatch = cbExceptionCatch;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callbacks");
		return JNI_ERR;
	}	

	return JNI_OK;
}



jboolean JNICALL
Java_com_ibm_jvmti_tests_decompResolveFrame_decomp003_startStepping(JNIEnv *jni_env, jclass cls, jthread thread)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;

	if (singleStep && !stepInCatch) {
		err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_SINGLE_STEP, NULL);
		if ( err != JVMTI_ERROR_NONE ) {
			error(env, err, "Failed to enable step event");
			return JNI_FALSE;
		}
	}
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, thread);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable exception event");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_decompResolveFrame_decomp003_stopStepping(JNIEnv *jni_env, jclass cls, jthread thread)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;

	if (singleStep) {
		err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_SINGLE_STEP, NULL);
		if ( err != JVMTI_ERROR_NONE ) {
			error(env, err, "Failed to disable step event failed");
			return JNI_FALSE;
		}
	}
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_EXCEPTION, thread);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to disable exception event");
		return JNI_FALSE;
	}
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_EXCEPTION_CATCH, thread);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to disable exception catch event");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

