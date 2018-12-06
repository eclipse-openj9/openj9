/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
#include "jnitest_internal.h"

static jclass runtimeException = NULL;

jboolean JNICALL
Java_org_openj9_test_booleans_BooleanTest_setUp(JNIEnv *env, jclass instanceClass, jobject exception)
{
	runtimeException = (*env)->NewGlobalRef(env, exception);

	return (jboolean) (runtimeException != NULL);
}

void JNICALL
Java_org_openj9_test_booleans_BooleanTest_tearDown(JNIEnv *env, jclass instanceClass)
{
	(*env)->DeleteGlobalRef(env, runtimeException);
}


void JNICALL
Java_org_openj9_test_booleans_BooleanTest_putInstanceBoolean(JNIEnv *env, jclass clazz, jclass instanceClass, jobject instance, jobject reflectField, jint value)
{
	jfieldID field = (*env)->FromReflectedField(env, reflectField);
	if (NULL == field) {
		(*env)->ThrowNew(env, runtimeException, "couldn't find field");
	} else {
		(*env)->SetBooleanField(env, instance, field, value);
	}
}

void JNICALL
Java_org_openj9_test_booleans_BooleanTest_putStaticBoolean(JNIEnv *env, jclass clazz, jclass instanceClass, jobject reflectField, jint value)
{
	jfieldID field = (*env)->FromReflectedField(env, reflectField);

	if (NULL == field) {
		(*env)->ThrowNew(env, runtimeException, "couldn't find field");
	} else {
		(*env)->SetStaticBooleanField(env, instanceClass, field, value);
	}

}

jboolean JNICALL
Java_org_openj9_test_booleans_BooleanTest_getInstanceBoolean(JNIEnv *env, jclass clazz, jclass instanceClass, jobject instance, jobject reflectField)
{
	jfieldID field = (*env)->FromReflectedField(env, reflectField);
	jboolean result = JNI_FALSE;

	if (NULL == field) {
		(*env)->ThrowNew(env, runtimeException, "couldn't find field");
	} else {
		result = (*env)->GetBooleanField(env, instance, field);
	}

	return result;
}

jboolean JNICALL
Java_org_openj9_test_booleans_BooleanTest_getStaticBoolean(JNIEnv *env, jclass clazz, jclass instanceClass, jobject reflectField)
{
	jfieldID field = (*env)->FromReflectedField(env, reflectField);
	jboolean result = JNI_FALSE;

	if (NULL == field) {
		(*env)->ThrowNew(env, runtimeException, "couldn't find field");
	} else {
		result = (*env)->GetStaticBooleanField(env, instanceClass, field);
	}

	return result;
}

jboolean JNICALL
Java_org_openj9_test_booleans_BooleanTest_returnBooleanValue(JNIEnv *env, jclass clazz, jint value)
{
	return value;
}


