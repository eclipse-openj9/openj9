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
#include <stdlib.h>
#include <stdint.h>

#include "jvmti_test.h"

#define TEST_CLASS_NAME "com/ibm/jvmti/tests/redefineBreakpointCombo/rbc001"

static agentEnv *env;
static jclass testClassRef = NULL;
static jfieldID breakpointCounterID = NULL;
void JNICALL
breakpointEvent(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jmethodID method,
            jlocation location);

jint JNICALL
rbc001(agentEnv * agent_env, char * args)
{
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiEventCallbacks callbacks = {0};
	jvmtiCapabilities capabilities = {0};
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_redefine_classes = 1;
	capabilities.can_retransform_classes = 1;
	capabilities.can_generate_breakpoint_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}

	/* Set the Breakpoint event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.Breakpoint = breakpointEvent;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to set callback for Breakpoint events");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_MIN_EVENT_TYPE_VAL, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable min event");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable BreakPoint event");
		return JNI_ERR;
	}

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineBreakpointCombo_rbc001_redefineClass(JNIEnv * jni_env, jobject obj, jclass originalClass, jbyteArray redefinedClassBytes)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiClassDefinition classdef = {0};
	jbyte *redefinedClassData = NULL;
	jsize classDataSize = 0;
	jboolean rc = JNI_TRUE;

	/* Create a new class definition to be used for re-definition */
	classDataSize = (*jni_env)->GetArrayLength(jni_env, redefinedClassBytes);

	err = (*jvmti_env)->Allocate(jvmti_env, classDataSize, (unsigned char **) &redefinedClassData);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Unable to allocate temp buffer for the class file");
		rc = JNI_FALSE;
		goto done;
	}

	(*jni_env)->GetByteArrayRegion(jni_env, redefinedClassBytes, 0, classDataSize, redefinedClassData);

	classdef.class_bytes = (unsigned char *) redefinedClassData;
	classdef.class_byte_count = classDataSize;
	classdef.klass = originalClass;

    err = (*jvmti_env)->RedefineClasses(jvmti_env, 1, &classdef);
    if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "RedefineClasses failed");
    	rc = JNI_FALSE;
    }

    err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) redefinedClassData);

done:
	return rc;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineBreakpointCombo_rbc001_setBreakpoint(JNIEnv * jni_env, jobject obj, jclass clazz, jstring methodName, jstring methodSig, jlocation pos)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	char* utfMethodName = NULL;
	char* utfMethodSig = NULL;
	jmethodID methodID = NULL;
	jboolean rc = JNI_TRUE;

	/* get methodName */
	utfMethodName = (char *)(*jni_env)->GetStringUTFChars(jni_env, methodName, NULL);
	if (NULL == utfMethodName) {
		fprintf(stderr, "Failed to get UTF characters for method name string\n");
		rc = JNI_FALSE;
		goto done0;
	}

	/* get method sig */
	utfMethodSig = (char *)(*jni_env)->GetStringUTFChars(jni_env, methodSig, NULL);
	if (NULL == utfMethodSig) {
		fprintf(stderr, "Failed to get UTF characters for method sig string\n");
		rc = JNI_FALSE;
		goto done1;
	}

	/* get methodID */
	methodID = (*jni_env)->GetMethodID(jni_env, clazz, utfMethodName, utfMethodSig);
	if (NULL == methodID) {
		fprintf(stderr, "Failed to get methodID\n");
		rc = JNI_FALSE;
		goto done2;
	}

	/* set breakpoint */
	err = (*jvmti_env)->SetBreakpoint(jvmti_env, methodID, pos);
	if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "set breakpoint failed");
		rc = JNI_FALSE;
		goto done2;
    }

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable break point event");
		rc = JNI_FALSE;
	}

done2:
	(*jni_env)->ReleaseStringChars(jni_env, methodSig, (jchar *) utfMethodSig);
done1:
	(*jni_env)->ReleaseStringChars(jni_env, methodName, (jchar *) utfMethodName);
done0:

	return rc;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineBreakpointCombo_rbc001_clearBreakpoint(JNIEnv * jni_env, jobject obj, jclass clazz, jstring methodName, jstring methodSig, jlocation pos)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	char* utfMethodName = NULL;
	char* utfMethodSig = NULL;
	jmethodID methodID = NULL;
	jboolean rc = JNI_TRUE;

	/* get methodName */
	utfMethodName = (char *)(*jni_env)->GetStringUTFChars(jni_env, methodName, NULL);
	if (NULL == utfMethodName) {
		fprintf(stderr, "Failed to get UTF characters for method name string\n");
		rc = JNI_FALSE;
		goto done0;
	}

	/* get method sig */
	utfMethodSig = (char *)(*jni_env)->GetStringUTFChars(jni_env, methodSig, NULL);
	if (NULL == utfMethodSig) {
		fprintf(stderr, "Failed to get UTF characters for method sig string\n");
		rc = JNI_FALSE;
		goto done1;
	}

	/* get methodID */
	methodID = (*jni_env)->GetMethodID(jni_env, clazz, utfMethodName, utfMethodSig);
	if (NULL == methodID) {
		fprintf(stderr, "Failed to get methodID\n");
		rc = JNI_FALSE;
		goto done2;
	}

	/* clear breakpoint */
	err = (*jvmti_env)->ClearBreakpoint(jvmti_env, methodID, pos);
	if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "set breakpoint failed");
		rc = JNI_FALSE;
    }

done2:
	(*jni_env)->ReleaseStringChars(jni_env, methodSig, (jchar *) utfMethodSig);
done1:
	(*jni_env)->ReleaseStringChars(jni_env, methodName, (jchar *) utfMethodName);
done0:

	return rc;
}

jlong JNICALL
Java_com_ibm_jvmti_tests_redefineBreakpointCombo_rbc001_getMethodIDFromStack(JNIEnv * jni_env, jobject obj, jint framePosition)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jthread thread = NULL;
	jvmtiFrameInfo *frames = NULL;
	jlong method = 0;
	jint count = 0;

	frames = (jvmtiFrameInfo * ) malloc(sizeof(jvmtiFrameInfo) * (framePosition + 2));

	err = (*jvmti_env)->GetCurrentThread(jvmti_env, &thread);
	if (NULL == thread) {
    	error(env, err, "failed to get thread");
    	goto done;
	}

	err = (*jvmti_env)->GetStackTrace(jvmti_env,thread, 0, framePosition + 1, frames, &count);
	if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "failed to get stacktrace");
    	goto done;
    }

	if (framePosition >= count) {
		fprintf(stderr, "Failed to get methodID from stack\n");
		goto done;
	}

	method = (jlong) (intptr_t) frames[framePosition].method;

done:
	free(frames);
	return method;
}


jlong JNICALL
Java_com_ibm_jvmti_tests_redefineBreakpointCombo_rbc001_getMethodID(JNIEnv * jni_env, jobject obj, jclass clazz, jstring methodName, jstring methodSig)
{
	char* utfMethodName = NULL;
	char* utfMethodSig = NULL;
	jmethodID methodID = 0;

	/* get methodName */
	utfMethodName = (char *)(*jni_env)->GetStringUTFChars(jni_env, methodName, NULL);
	if (NULL == utfMethodName) {
		fprintf(stderr, "Failed to get UTF characters for method name string\n");
		goto done0;
	}

	/* get method sig */
	utfMethodSig = (char *)(*jni_env)->GetStringUTFChars(jni_env, methodSig, NULL);
	if (NULL == utfMethodSig) {
		fprintf(stderr, "Failed to get UTF characters for method sig string\n");
		goto done1;
	}

	/* get methodID */
	methodID = (*jni_env)->GetMethodID(jni_env, clazz, utfMethodName, utfMethodSig);
	if (NULL == methodID) {
		fprintf(stderr, "Failed to get methodID\n");
	}

	(*jni_env)->ReleaseStringChars(jni_env, methodSig, (jchar *) utfMethodSig);
done1:
	(*jni_env)->ReleaseStringChars(jni_env, methodName, (jchar *) utfMethodName);
done0:
	return (jlong) (intptr_t) methodID;
}



void JNICALL
breakpointEvent(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location)
{

	jint breakpointCounter = 0;
	jclass testClass = NULL;

	/* increment test class breakpoint counter */
	if (NULL == testClassRef) {
		testClass = (*jni_env)->FindClass(jni_env, TEST_CLASS_NAME);
		if (NULL == testClass) {
			fprintf(stderr, "Failed to get test class %s\n", TEST_CLASS_NAME);
			return;
		} else {
			testClassRef = (*jni_env)->NewGlobalRef(jni_env, testClass);
			if (NULL == testClassRef) {
				fprintf(stderr, "Failed to get global ref for test class %s\n", TEST_CLASS_NAME);
				return;
			}
		}
	}

	if (NULL == breakpointCounterID) {
		breakpointCounterID = (*jni_env)->GetStaticFieldID(jni_env, testClassRef, "breakpointCounter", "I");
		if (NULL == breakpointCounterID) {
			fprintf(stderr, "Failed to get breakpointCounterID\n");
			return;
		}
	}

	breakpointCounter = (*jni_env)->GetStaticIntField(jni_env, testClassRef, breakpointCounterID);

	breakpointCounter++;

	(*jni_env)->SetStaticIntField(jni_env, testClass, breakpointCounterID, breakpointCounter);

	return;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineBreakpointCombo_rbc001_setBreakpointInMethodID(JNIEnv * jni_env, jobject obj, jmethodID methodID, jlocation pos)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;

	/* set breakpoint */
	err = (*jvmti_env)->SetBreakpoint(jvmti_env, methodID, pos);
	if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "set breakpoint failed");
    	return JNI_FALSE;
    }

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable break point event");
		return JNI_ERR;
	}
	return JNI_TRUE;
}
