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

#include "ibmjvmti.h"
#include "jvmti_test.h"


static agentEnv * env;

jint JNICALL
gcf001(agentEnv * agent_env, char * args)
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

JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_getClassFields_gcf001_checkClassFields(JNIEnv *jni_env,
	jclass myklass, jclass klass, jobjectArray expectedFieldNames, jobjectArray expectedFieldSignatures)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jint i;
	jint fieldCount;
	jfieldID *fields;
	jvmtiError err;

	err = (*jvmti_env)->GetClassFields(jvmti_env, klass, &fieldCount, &fields);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "GetClassFields() failed");
		return JNI_FALSE;
	}

	if ((*jni_env)->GetArrayLength(jni_env, expectedFieldNames) != fieldCount) {
		error(env, err, "GetArrayLength() (%d) did not match fieldCount (%d)",
			(*jni_env)->GetArrayLength(jni_env, expectedFieldNames), fieldCount);
		return JNI_FALSE;
	}

	for (i = 0; i < fieldCount; i++) {
		jstring expectedNameString = (*jni_env)->GetObjectArrayElement(jni_env, expectedFieldNames, i);
		jstring expectedSignatureString = (*jni_env)->GetObjectArrayElement(jni_env, expectedFieldSignatures, i);
		const char *expectedName = (*jni_env)->GetStringUTFChars(jni_env, expectedNameString, 0);
		const char *expectedSignature = (*jni_env)->GetStringUTFChars(jni_env, expectedSignatureString, 0);
		char *fieldName;
		char *fieldSignature;

		err = (*jvmti_env)->GetFieldName(jvmti_env, klass, fields[i], &fieldName, &fieldSignature, NULL);
		if (JVMTI_ERROR_NONE != err) {
			error(env, err, "GetFieldName() failed");
			return JNI_FALSE;
		}

		if (0 != strcmp(fieldName, expectedName)) {
			error(env, err, "fieldName (%s) did not match expectedName (%s)", fieldName, expectedName);
			return JNI_FALSE;
		}

		if (0 != strcmp(fieldSignature, expectedSignature)) {
			error(env, err, "fieldSignature (%s) did not match expectedSignature (%s)", fieldSignature, expectedSignature);
			return JNI_FALSE;
		}

		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)fieldName);
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)fieldSignature);
		(*jni_env)->ReleaseStringUTFChars(jni_env, expectedNameString, expectedName);
		(*jni_env)->ReleaseStringUTFChars(jni_env, expectedSignatureString, expectedSignature);
	}
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)fields);

	return JNI_TRUE;
}
