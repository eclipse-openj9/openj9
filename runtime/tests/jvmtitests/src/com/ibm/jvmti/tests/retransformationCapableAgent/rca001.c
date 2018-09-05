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

#include "jvmti_test.h"

#define CLASS_LOADING 1
#define CLASS_RETRANSFORMING 2
#define CLASS_REDEFINING 3

#define CLASS_PACKAGE "com/ibm/jvmti/tests/classModificationAgent"
#define AGENT_CLASS_NAME "cma001"
#define TEST_CLASS_NAME "cma001_TestClass_V1"

static agentEnv * env;

static char *agentName;
static int expectedVersion;
static int newClassVersion;

/* Variables to cache JNI types */
static jclass agentJavaClass;				/* cma001.class */
static jfieldID currentVersionFieldId;		/* cma001.currentClassVersion */
static jfieldID stageFieldId;				/* cma001.stage */
static jfieldID cbResultFieldId;			/* cma001.classFileLoadHookCallbackResult */
static jfieldID riHookCalledFieldId;		/* cma001.retransformationIncapableHookCalled */
static jmethodID getClassBytesMethodId;		/* cma001.getClassBytesForVersion() */
static jclass jlClass;						/* java.lang.Class */
static jmethodID getNameMethodId;			/* java.lang.Class.getCanonicalName() */

static void setResultValue(JNIEnv* jni_env, jboolean value) {
	(*jni_env)->SetStaticBooleanField(jni_env, agentJavaClass, cbResultFieldId, value);
}

/*
 * ClassFileLoadHook event callback performs following function:
 * 		- if the stage is CLASS_LOADING or CLASS_REDEFINING sets its expected version to cma001.currentClassVersion.
 * 		- verifies bytes received correspond to expected version,
 *  	- modifies the class bytes to Vn where Vn is the version passed to the agent as argument.
 *  	- sets cma001.currentClassVersion to Vn.
 */
static void JNICALL
classFileLoadHook(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jclass class_being_redefined,
            jobject loader,
            const char* name,
            jobject protection_domain,
            jint class_data_len,
            const unsigned char* class_data,
            jint* new_class_data_len,
            unsigned char** new_class_data)
{
	jvmtiError err;
	jint currentVersion, stage;
	jbyteArray classBytesArray;
	jbyte *classBytes;
	jsize classByteSize;

	if ((0 == strcmp(name, CLASS_PACKAGE "/" TEST_CLASS_NAME))
		&& (NULL != agentJavaClass)) {
		jboolean riHookCalled;

		stage = (*jni_env)->GetStaticIntField(jni_env, agentJavaClass, stageFieldId);
		currentVersion = (*jni_env)->GetStaticIntField(jni_env, agentJavaClass, currentVersionFieldId);
		riHookCalled = (*jni_env)->GetStaticIntField(jni_env, agentJavaClass, riHookCalledFieldId);

		if ((CLASS_LOADING == stage) || (CLASS_REDEFINING == stage)) {
			/* ClassFileLoadHook event for Re-transformation Incapable agent should be called
			 * before Re-transformation Capable agent when loading or re-defining
			 */
			if (JNI_FALSE == riHookCalled) {
				fprintf(stderr, "%s: ClassFileLoadHook event for Re-transformation Incapable agent is not called\n", agentName);
				setResultValue(jni_env, JNI_FALSE);
				return;
			}
			expectedVersion = (int)currentVersion;
			(*jni_env)->SetStaticIntField(jni_env, agentJavaClass, currentVersionFieldId, (jint)newClassVersion);
		}

		if (CLASS_RETRANSFORMING == stage) {
			/* ClassFileLoadHook event for Re-transformation Incapable agent should not be called when re-transforming */
			if (JNI_TRUE == riHookCalled) {
				fprintf(stderr, "%s: ClassFileLoadHook event for Re-transformation Incapable agent is incorrectly called\n", agentName);
				setResultValue(jni_env, JNI_FALSE);
				return;
			}
		}

		classBytesArray = (*jni_env)->CallStaticObjectMethod(jni_env, agentJavaClass, getClassBytesMethodId, (jint)expectedVersion);
		if (NULL != classBytesArray) {
			classByteSize = (*jni_env)->GetArrayLength(jni_env, classBytesArray);
			if (classByteSize != class_data_len) {
				/* set error */
				fprintf(stderr, "%s: Size of class bytes does not match. Expected: %d, received: %d\n", agentName, classByteSize, class_data_len);
				setResultValue(jni_env, JNI_FALSE);
				return;
			}
			classBytes = (*jni_env)->GetByteArrayElements(jni_env, classBytesArray, NULL);
			if (NULL == classBytes) {
				fprintf(stderr, "%s: Failed to get array elements\n", agentName);
				setResultValue(jni_env, JNI_FALSE);
				return;
			}
			if (0 != memcmp(classBytes, class_data, class_data_len)) {
				/* set error */
				fprintf(stderr, "%s: Class byte do not match\n", agentName);
				setResultValue(jni_env, JNI_FALSE);
				return;
			} else {
				fprintf(stdout, "%s: Expected class bytes received\n", agentName);
			}
		} else {
			fprintf(stderr, "%s: Failed to get class bytes for expected class version %d\n", agentName, expectedVersion);
			setResultValue(jni_env, JNI_FALSE);
			return;
		}

		classBytesArray = (*jni_env)->CallStaticObjectMethod(jni_env, agentJavaClass, getClassBytesMethodId, (jint)newClassVersion);
		if (NULL != classBytesArray) {
			unsigned char *newClassData;

			classByteSize = (*jni_env)->GetArrayLength(jni_env, classBytesArray);
			err = (*jvmti_env)->Allocate(jvmti_env, classByteSize, &newClassData);
			if (JVMTI_ERROR_NONE != err) {
				error(env, err, "Failed to allocate memory for new class data");
				setResultValue(jni_env, JNI_FALSE);
				return;
			}
			(*jni_env)->GetByteArrayRegion(jni_env, classBytesArray, 0, classByteSize, (jbyte *)newClassData);
			*new_class_data = newClassData;
			*new_class_data_len = classByteSize;
		} else {
			fprintf(stderr, "%s: Failed to get class bytes for new class version %d\n", agentName, newClassVersion);
			setResultValue(jni_env, JNI_FALSE);
			return;
		}
		setResultValue(jni_env, JNI_TRUE);
	}
}

static jint getJNIIDs(JNIEnv* jni_env)
{
	if (NULL != agentJavaClass) {
		currentVersionFieldId = (*jni_env)->GetStaticFieldID(jni_env, agentJavaClass, "currentClassVersion", "I");
		if (NULL == currentVersionFieldId) {
			return JNI_ERR;
		}
		cbResultFieldId = (*jni_env)->GetStaticFieldID(jni_env, agentJavaClass, "classFileLoadHookCallbackResult", "Z");
		if (NULL == cbResultFieldId) {
			return JNI_ERR;
		}
		stageFieldId = (*jni_env)->GetStaticFieldID(jni_env, agentJavaClass, "stage", "I");
		if (NULL == stageFieldId) {
			return JNI_ERR;
		}
		riHookCalledFieldId = (*jni_env)->GetStaticFieldID(jni_env, agentJavaClass, "retransformationIncapableHookCalled", "Z");
		if (NULL == riHookCalledFieldId) {
			return JNI_ERR;
		}
		getClassBytesMethodId = (*jni_env)->GetStaticMethodID(jni_env, agentJavaClass, "getClassBytesForVersion", "(I)[B");
		if (NULL == getClassBytesMethodId) {
			return JNI_ERR;
		}
		return JNI_OK;
	} else {
		return JNI_ERR;
	}
}

static void JNICALL
classPrepare(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jclass klass)
{
	jvmtiError err;

	if (NULL == jlClass) {
		jclass localRef;

		localRef = (*jni_env)->FindClass(jni_env, "Ljava/lang/Class;");
		if (NULL == localRef) {
			return;
		}
		jlClass = (*jni_env)->NewGlobalRef(jni_env, localRef);
		if (NULL == jlClass) {
			return;
		}
		getNameMethodId = (*jni_env)->GetMethodID(jni_env, localRef, "getCanonicalName", "()Ljava/lang/String;");
		if (NULL == getNameMethodId) {
			return;
		}
	} else if (NULL != getNameMethodId) {
		const jbyte *utfName;
		char *tempName;
		unsigned int i = 0;

		jstring stringName = (*jni_env)->CallObjectMethod(jni_env, (jobject)klass, getNameMethodId);
		if (NULL == stringName) {
			return;
		}
		utfName = (const jbyte*) (*jni_env)->GetStringUTFChars(jni_env, stringName, NULL);
		if (NULL == utfName) {
			return;
		}
		err = (*jvmti_env)->Allocate(jvmti_env, strlen((const char *) utfName)+1, (unsigned char **) &tempName);
		if (JVMTI_ERROR_NONE != err) {
			error(env, err, "Failed to allocate memory for temporary buffer for the class name");
			return;
		}
		strcpy(tempName, (const char *) utfName);
		while (i < strlen(tempName)) {
			if (tempName[i] == '.') {
				tempName[i] = '/';
			}
			i++;
		}
		if (!strcmp(tempName, CLASS_PACKAGE "/" AGENT_CLASS_NAME)) {
			agentJavaClass = (*jni_env)->NewGlobalRef(jni_env, klass);
			if (JNI_ERR == getJNIIDs(jni_env)) {
				(*jni_env)->DeleteGlobalRef(jni_env, agentJavaClass);
				agentJavaClass = NULL;
				fprintf(stderr, "Failed to get JNI IDs\n");
			}
		}
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)tempName);
		(*jni_env)->ReleaseStringUTFChars(jni_env, stringName, NULL);
	}
}

static jint parseArgs(char *args)
{
	if (NULL != args) {
		if ((strlen(args) > 2) || ((args[0] != 'V') && (args[0] != 'v'))) {
			fprintf(stderr, "%s: Incorrect version argument\n", agentName);
			return JNI_ERR;
		}
		newClassVersion = args[1] - '0';
		if (newClassVersion <= 0) {
			fprintf(stderr, "%s: Invalid version in version argument\n", agentName);
			return JNI_ERR;
		}
	} else {
		fprintf(stderr, "%s: No argument specified\n", agentName);
		return JNI_ERR;
	}
	return JNI_OK;
}

/*
 * This agent is to be used by cma001.
 * This agent accepts argument in following format:
 * args:Vn where n = 1, 2, 3...
 * 	Vn indicates the version of class file to be used during transformation.
 *
 * It is a re-transformation capable agent.
 * It has two global variables:
 * 	newClassVersion: set to the version passed to it as argument. ClassFileLoadHook event callback will transform the class to this version.
 * 	expectedVersion: set in its ClassFileLoadHook event callback when the stage is CLASS_LOADING or CLASS_REDEFINING.
 * 					 Used by ClassFileLoadHook event callback to verify incoming class bytes are as expected.
 *
 * See classFileLoadHook() for more details.
 */
jint JNICALL
rca001(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	char *testArgs;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env;
	testArgs = env->testArgs;

	if (JNI_ERR == parseArgs(testArgs)) {
		return JNI_ERR;
	}

	agentName = malloc(strlen(env->testName)+1);
	if (NULL == agentName) {
		fprintf(stderr, "Failed to allocate memory for agent name\n");
		return JNI_ERR;
	}
	strcpy(agentName, env->testName);

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_redefine_classes = 1;
	capabilities.can_retransform_classes = 1;

	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}

	/* Set the ClassFileLoadHook event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ClassFileLoadHook = classFileLoadHook;
	callbacks.ClassPrepare = classPrepare;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to set callback for ClassFileLoadHook events");
		return JNI_ERR;
	}

	/* Enable the JVMTI_EVENT_CLASS_FILE_LOAD_HOOK callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable ClassFileLoadHook event");
		return JNI_ERR;
	}

	/* Enable the JVMTI_EVENT_CLASS_PREPARE callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to enable ClassPrepare event");
		return JNI_ERR;
	}

	return JNI_OK;
}
