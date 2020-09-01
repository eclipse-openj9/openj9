/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

static agentEnv * env;

typedef struct TransformerData {
	jint action;
	char *retransformedClassName;
	jsize classDataSize;
	unsigned char *transformedClassData;
} TransformerData;

static TransformerData transformerData;

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
	if ((NULL != transformerData.retransformedClassName) && (0 == strcmp(name, transformerData.retransformedClassName))) {
		if (NULL != transformerData.transformedClassData) {
			*new_class_data = transformerData.transformedClassData;
			*new_class_data_len = (jint)transformerData.classDataSize;
		}
	}
}

jint JNICALL
rtc002(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_retransform_classes = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}

	/* Set the ClassFileLoadHook event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ClassFileLoadHook = classFileLoadHook;
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

	memset(&transformerData, 0, sizeof(TransformerData));
	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_retransformClasses_rtc002_retransformClass(JNIEnv * jni_env, jclass clazz, jclass originalClass, jstring className, jbyteArray transformedClassBytes)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	unsigned char *newClassData = NULL;
	jsize classNameLength;
	char *utfClassName;

	/* Set fields to be used by the transformer */
	classNameLength = (*jni_env)->GetStringLength(jni_env, className);
	utfClassName = (char *)(*jni_env)->GetStringUTFChars(jni_env, className, NULL);
	if (NULL == utfClassName) {
		fprintf(stderr, "Failed to get UTF characters for class name string\n");
		return JNI_FALSE;
	}
	err = (*jvmti_env)->Allocate(jvmti_env, classNameLength+1, (unsigned char **) &transformerData.retransformedClassName);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err,  "Failed to allocate memory for class name\n");
		(*jni_env)->ReleaseStringUTFChars(jni_env, className, utfClassName);
		return JNI_FALSE;
	}
	strcpy(transformerData.retransformedClassName, utfClassName);
	(*jni_env)->ReleaseStringUTFChars(jni_env, className, utfClassName);

	if (NULL != transformedClassBytes) {
		transformerData.classDataSize = (*jni_env)->GetArrayLength(jni_env, (jarray) transformedClassBytes);
		err = (*jvmti_env)->Allocate(jvmti_env, transformerData.classDataSize, &newClassData);
		if (JVMTI_ERROR_NONE != err) {
			error(env, err, "Failed to allocate memory for new class data");
			return JNI_FALSE;
		}
		(*jni_env)->GetByteArrayRegion(jni_env, transformedClassBytes, 0, transformerData.classDataSize, (jbyte *)newClassData);
	} else {
		transformerData.classDataSize = 0;
	}

	transformerData.transformedClassData = newClassData;

	err = (*jvmti_env)->RetransformClasses(jvmti_env, 1, &originalClass);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "RetransformClasses failed");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}
