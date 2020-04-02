/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include <errno.h>
#include "jvmti_test.h"
#include <stdio.h>
#include <string.h>

jvmtiEnv *globalEnv;
jfieldID currentFieldBeingWatched;
const char * testClassName;

/* Find the correct API to increment the number of reports we have seen and return the new value. */
jint
incAndGetNumReports(JNIEnv* jni_env)
{
	jmethodID mID;
	jclass clazz = (*jni_env)->FindClass(jni_env, testClassName);

	if (NULL == clazz) {
		printf("Unable to find class: %s\n", testClassName);
		return JNI_ERR;
	}

	mID = (*jni_env)->GetStaticMethodID(jni_env, clazz, "incrementNumReports", "()I");
	if (NULL == mID) {
		printf("Unable to find methodID for class: %s and method: %s\n", testClassName, "incrementNumReports");
		return JNI_ERR;
	}

	return (*jni_env)->CallStaticIntMethod(jni_env, clazz, mID);
}

/* Callback routine invoked by the VM to report a field read event. */
void JNICALL
fieldAccessCallback(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jclass field_klass, jobject object, jfieldID field)
{
	/* Only increment the count if the field ID matches with the same field we added the watch for. */
	if (currentFieldBeingWatched == field) {
		jint numReports = incAndGetNumReports(jni_env);
		if (JNI_ERR != numReports) {
			printf("Field Access Report #%d: method: %p location %d. Class: %p, fieldID: %p\n", numReports, method, (jint)location, field_klass, field);
		} else {
			printf("**Error**: Invalid value for numReports. Return Code: %d\n", numReports);
		}
	}
}

/* Callback routine invoked by the VM to report a field write event. */
void JNICALL
fieldModificationCallback(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jclass field_klass, jobject object, jfieldID field, char signature_type, jvalue new_value)
{
	/* Only increment the count if the field ID matches with the same field we added the watch for. */
	if (currentFieldBeingWatched == field) {
		jint numReports = incAndGetNumReports(jni_env);
		if (JNI_ERR != numReports) {
			printf("Field Modification Report #%d: method: %p location: %d, fieldID: %p, signature: %c\n", numReports, method, (jint)location, field, signature_type);
		} else {
			printf("**ERROR**: Invalid value for numReports. Return Code: %d\n", numReports);
		}
	}
}

jint JNICALL
fw001(agentEnv * agent_env, char * args)
{
	jvmtiCapabilities capabilities;
	jvmtiEventCallbacks callbacks;
	jvmtiError err;
	JavaVM *vm = agent_env->vm;
	jint rc = (*vm)->GetEnv(vm, (void **) &globalEnv, JVMTI_VERSION_1_2);

	if (JNI_OK != rc) {
		printf("**ERROR**: Failed to get agent env. Return code: %d.\n", rc);
		return JNI_ERR;
	}

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_field_access_events = 1;  
	capabilities.can_generate_field_modification_events = 1;
	err = (*globalEnv)->AddCapabilities(globalEnv, &capabilities);
	if (JVMTI_ERROR_NONE != err) {
		printf("**ERROR**: Failed to AddCapabilities. JVMTI_ERROR code: %d.\n", err);
		return JNI_ERR;
	}

	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.FieldAccess = fieldAccessCallback;
	callbacks.FieldModification = fieldModificationCallback;
	err = (*globalEnv)->SetEventCallbacks(globalEnv, &callbacks, sizeof(jvmtiEventCallbacks));
	if (JVMTI_ERROR_NONE != err) {
		printf("**ERROR**: Failed to set callbacks. JVMTI_ERROR code:  %d.\n", err);
		return JNI_ERR;
	}

	err = (*globalEnv)->SetEventNotificationMode(globalEnv, JVMTI_ENABLE, JVMTI_EVENT_FIELD_ACCESS, NULL);
	if (JVMTI_ERROR_NONE != err) {
		printf("**ERROR**: Failed to enable field access event. JVMTI_ERROR code: %d.\n", err);
		return JNI_ERR;
	}

	err = (*globalEnv)->SetEventNotificationMode(globalEnv, JVMTI_ENABLE, JVMTI_EVENT_FIELD_MODIFICATION, NULL);
	if (JVMTI_ERROR_NONE != err) {
		printf("**ERROR**: Failed to enable field modification event. JVMTI_ERROR code: %d.\n", err);
		return JNI_ERR;
	}

	return JNI_OK;
}

/* Add class name to a global so we can invoke the appropriate Java API to update the number of reports seen. */
JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_fieldwatch_fw001_startTest(JNIEnv *jni_env, jobject cls, jstring name)
{
	testClassName = (*jni_env)->GetStringUTFChars(jni_env, name, NULL);
	if (NULL == testClassName) {
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_fieldwatch_fw001_endTest(JNIEnv *jni_env, jobject cls, jstring name)
{
	(*jni_env)->ReleaseStringUTFChars(jni_env, name, testClassName);
	return JNI_TRUE;
}

/* Add or remove field watches. */
JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_fieldwatch_fw001_modifyWatches(JNIEnv *env, jclass rcv, jstring className, jstring fieldName, jstring fieldSignature, jboolean isStatic, jboolean isRead, jboolean isWrite, jboolean isAdd)
{
	const char * _fieldName = (*env)->GetStringUTFChars(env, fieldName, NULL);
	const char * _fieldSignature = (*env)->GetStringUTFChars(env, fieldSignature, NULL);
	const char * _className = (*env)->GetStringUTFChars(env, className, NULL);
	jfieldID fid;

	if (NULL == _className) {
		printf("_className string is NULL\n");
		return JNI_FALSE;
	}

	if (NULL == _fieldName) {
		printf("_fieldName string is NULL\n");
		return JNI_FALSE;
	}

	if (NULL == _fieldSignature) {
		printf("_fieldSignature string is NULL\n");
		return JNI_FALSE;
	}

	rcv = (*env)->FindClass(env, _className);
	if (NULL == rcv) {
		printf("Unable to find class with name: %s\n", _className);
		return JNI_FALSE;
	}

	if (isStatic) {
		fid = (*env)->GetStaticFieldID(env, rcv, _fieldName, _fieldSignature);
	} else {
		fid = (*env)->GetFieldID(env, rcv, _fieldName, _fieldSignature);
	}

	if (NULL != fid) {
		if (isRead) {
			jvmtiError err = JVMTI_ERROR_NONE;
			if (isAdd) {
				err = (*globalEnv)->SetFieldAccessWatch(globalEnv, rcv, fid);
			} else {
				err = (*globalEnv)->ClearFieldAccessWatch(globalEnv, rcv, fid);
			}

			if (JVMTI_ERROR_NONE != err) {
				printf( "**Error**: Failed to %s field access watch with JVMTI Error: %d\n", isAdd ? "add" : "remove",  err);
				return JNI_FALSE;
			}
		}

		if (isWrite) {
			jvmtiError err = JVMTI_ERROR_NONE;
			if (isAdd) {
				err = (*globalEnv)->SetFieldModificationWatch(globalEnv, rcv, fid);
			} else {
				err = (*globalEnv)->ClearFieldModificationWatch(globalEnv, rcv, fid);
			}

			if (JVMTI_ERROR_NONE != err) {
				printf( "**Error**: Failed to %s field modification watch with JVMTI Error Code: %d\n", isAdd ? "add" : "remove",  err);
				return JNI_FALSE;
			}
		}
	} else {
		printf("**ERROR**: Unable to modify fieldwatch(%s%s) for %s. Couldn't find a valid fieldID associated with class: %s, field: %s, signature: %s, isStatic: %s, modificationType: %s\n",
			isRead ? "access" : "",
			isWrite ? ", modification" : "",
			_fieldName,
			_className,
			_fieldName,
			_fieldSignature,
			isStatic ? "Yes" : "No",
			isAdd ? "add" : "remove"
		);
		return JNI_FALSE;
	}

	printf("**NOTE**: Modified fieldwatch(%s%s) for %s. The fieldID is: %p and it is associated with class: %s, field: %s, signature: %s, isStatic: %s, modificationType: %s\n",
		isRead ? "access" : "",
		isWrite ? ", modification" : "",
		_fieldName,
		fid,
		_className,
		_fieldName,
		_fieldSignature,
		isStatic ? "Yes" : "No",
		isAdd ? "add" : "remove"
	);

	currentFieldBeingWatched = isAdd ? fid : 0;

	(*env)->ReleaseStringUTFChars(env, className, _className);
	(*env)->ReleaseStringUTFChars(env, fieldName, _fieldName);
	(*env)->ReleaseStringUTFChars(env, fieldSignature, _fieldSignature);
	return JNI_TRUE;
}
