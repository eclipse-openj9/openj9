/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "j9port.h"
#include "j9.h"

#define LENGTH 20000
/*
 * Class:     com.ibm.j9.zostest.ZosNativesTest
 * Method:    ifaSwitchTest1
 * Signature: ()I
 *
 * Testing: GetByteArrayElements, ReleaseByteArrayElements
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest1(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	jboolean isCopy; /* dummy variable */
	jbyteArray array_b;
	jbyte * buffer_b;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest1 is starting\n");

	array_b = (*env)->NewByteArray(env, LENGTH);
	if (NULL == array_b) {
		j9tty_printf( PORTLIB, "NewByteArray should not return NULL\n");
		return 1;
	}

	buffer_b = (jbyte *)(*env)->GetByteArrayElements(env, array_b, &isCopy);
	if (NULL == buffer_b) {
		j9tty_printf( PORTLIB, "GetByteArrayElements should not return NULL\n");
		return 1;
	}

	/* Free the buffer */
	(*env)->ReleaseByteArrayElements(env, array_b, buffer_b, 0);
	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in ReleaseByteArrayElements\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest1 has finished\n");

	return 0;
}

/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest2
 * Signature: ()I
 *
 * Testing: NewStringUTF
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest2(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	char * buffer_str;
	jstring testStr;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest2 is starting\n");

	buffer_str = (char *) j9mem_allocate_memory(LENGTH, OMRMEM_CATEGORY_VM);
	if (NULL == buffer_str) {
		j9tty_printf( PORTLIB, "j9mem_allocate_memory should not return NULL\n");
		return 1;
	}

	memset((void *)buffer_str, 's', LENGTH - 1);
	buffer_str[LENGTH-1] = '\0';

	testStr = (*env)->NewStringUTF(env, buffer_str);
	j9mem_free_memory((void *)buffer_str);

	if (NULL == testStr) {
		j9tty_printf( PORTLIB, "NewStringUTF should not return NULL\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest2 has finished\n");
	return 0;
}


/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest3
 * Signature: ()I
 *
 * Testing: GetByteArrayRegion
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest3(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	jbyteArray array_b;
	jbyte *buffer_b;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest3 is starting\n");

	array_b = (*env)->NewByteArray(env, LENGTH);
	if (NULL == array_b) {
		j9tty_printf( PORTLIB,"NewByteArray should not return NULL\n");
		return 1;
	}

	buffer_b = (jbyte*) j9mem_allocate_memory(LENGTH*sizeof(jbyte), OMRMEM_CATEGORY_VM);
	if (NULL == buffer_b) {
		j9tty_printf( PORTLIB,"j9mem_allocate_memory should not return NULL\n");
		return 1;
	}

	(*env)->GetByteArrayRegion(env, array_b, 0, LENGTH, buffer_b);
	j9mem_free_memory((void*)buffer_b);

	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in GetByteArrayRegion\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest3 has finished\n");

	return 0;
}


/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest4
 * Signature: ()I
 *
 * Testing: SetIntArrayRegion
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest4(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	jint *buffer_i;
	jintArray array_i;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest4 is starting\n");

	array_i = (*env)->NewIntArray(env, LENGTH);

	if (NULL == array_i) {
		j9tty_printf( PORTLIB,"NewIntArray should not return NULL\n");
		return 1;
	}

	/* Allocate buffer */
	buffer_i = (jint*) j9mem_allocate_memory(LENGTH*sizeof(jint), OMRMEM_CATEGORY_VM);
	if (NULL == buffer_i) {
		j9tty_printf( PORTLIB,"j9mem_allocate_memory should not return NULL\n");
		return JNI_FALSE;
	}

	(*env)->SetIntArrayRegion(env, array_i, 0, LENGTH, buffer_i);

	/* Free buffer */
	j9mem_free_memory(buffer_i);

	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in SetIntArrayRegion\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest4 has finished\n");

	return 0;
}


/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest5
 * Signature: ()I
 *
 * Testing: GetStringRegion
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest5(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	const jchar * buffer_str;
	jstring testStr;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest5 is starting\n");

	buffer_str = (const jchar *) j9mem_allocate_memory(LENGTH*sizeof(jchar), OMRMEM_CATEGORY_VM);
	if (NULL == buffer_str) {
		j9tty_printf( PORTLIB,"j9mem_allocate_memory should not return NULL\n");
		return 1;
	}

	memset((void *)buffer_str, 0, LENGTH*sizeof(jchar));

	testStr = (*env)->NewString(env, buffer_str, LENGTH);
	if (NULL == testStr) {
		j9tty_printf( PORTLIB, "NewString should not return NULL\n");
		j9mem_free_memory((void*)buffer_str);
		return 1;
	}

	(*env)->GetStringRegion(env, testStr, 0, (*env)->GetStringLength(env, testStr), buffer_str);

	/* Free buffer */
	j9mem_free_memory((void *)buffer_str);

	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in GetStringRegion\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest5 has finished\n");
	return 0;
}


/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest6
 * Signature: ()I
 *
 *
 * Testing: GetStringChars, ReleaseStringChars
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest6(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	jboolean isCopy; /* dummy variable */
	jthrowable ex = NULL;
	const jchar * buffer_str;
	const jchar * string_chars;
	jstring testStr;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest6 is starting\n");

	buffer_str = (const jchar *) j9mem_allocate_memory(LENGTH*sizeof(jchar), OMRMEM_CATEGORY_VM);
	if (NULL == buffer_str) {
		j9tty_printf( PORTLIB,"j9mem_allocate_memory should not return NULL\n");
		return 1;
	}

	memset((void *)buffer_str, 0, LENGTH*sizeof(jchar));

	testStr = (*env)->NewString(env, buffer_str, LENGTH);
	j9mem_free_memory((void *)buffer_str);

	if (NULL == testStr) {
		j9tty_printf( PORTLIB, "NewString should not return NULL\n");
		return 1;
	}

	string_chars = (*env)->GetStringChars(env, testStr, &isCopy);

	if (NULL == string_chars) {
		j9tty_printf( PORTLIB,"GetStringChars should not return NULL\n");
		return 1;
	}

	(*env)->ReleaseStringChars(env, testStr, string_chars);
	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in ReleaseStringChars\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest6 has finished\n");

	return 0;
}


/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest7
 * Signature: ()I
 *
 * Testing: GetStringUTFChars
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest7(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	jboolean isCopy; /* dummy variable */
	jthrowable ex = NULL;
	const char * utfChars;
	char * buffer_str;
	jstring testStr;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest7 is starting\n");

	buffer_str = (char *) j9mem_allocate_memory(LENGTH, OMRMEM_CATEGORY_VM);
	if (NULL == buffer_str) {
		j9tty_printf( PORTLIB,"j9mem_allocate_memory should not return NULL\n");
		return 1;
	}
	memset((void *)buffer_str, 's', LENGTH - 1);
	buffer_str[LENGTH-1] = '\0';

	testStr = (*env)->NewStringUTF(env, buffer_str);
	j9mem_free_memory((void *)buffer_str);
	if (NULL == testStr) {
		j9tty_printf( PORTLIB, "NewStringUTF should not return NULL\n");
		return 1;
	}

	utfChars = (*env)->GetStringUTFChars(env, testStr, &isCopy);

	if (NULL == utfChars) {
		j9tty_printf( PORTLIB,"GetStringUTFChars should not return NULL\n");
		return 1;
	}

	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in ReleaseStringUTFChars\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest7 has finished\n");

	return 0;

}


/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    ifaSwitchTest8
 * Signature: ()I
 *
 * Testing: GetStringUTFRegion
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_ifaSwitchTest8(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_ENV(env);
	char * buffer_str;
	jstring testStr;

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest8 is starting\n");

	buffer_str = (char *) j9mem_allocate_memory(LENGTH, OMRMEM_CATEGORY_VM);
	if (NULL == buffer_str) {
		j9tty_printf( PORTLIB,"j9mem_allocate_memory should not return NULL\n");
		return 1;
	}

	memset((void *)buffer_str, 's', LENGTH - 1);
	buffer_str[LENGTH-1] = '\0';

	testStr = (*env)->NewStringUTF(env, buffer_str);

	if (NULL == testStr) {
		j9mem_free_memory((void *)buffer_str);
		j9tty_printf( PORTLIB, "NewStringUTF should not return NULL\n");
		return 1;
	}

	(*env)->GetStringUTFRegion(env, testStr, 0, (*env)->GetStringUTFLength(env, testStr), buffer_str);
	j9mem_free_memory((void *)buffer_str);
	if ((*env)->ExceptionCheck(env)) {
		j9tty_printf( PORTLIB, "Exception occurred in GetStringUTFRegion\n");
		return 1;
	}

	j9tty_printf( PORTLIB, "Java_com_ibm_j9_zostest_TestZosNatives_ifaSwitchTest8 has finished\n");

	return 0;
}

jint JNICALL registeredNativeMethod()
{
	return 100;
}

/*
 * Class:     com.ibm.j9.zostest.TestZosNatives
 * Method:    registerNative
 * Signature: ()I
 *
 * Testing: RegisterNatives
 */
jint JNICALL
Java_com_ibm_j9_zostest_ZosNativesTest_registerNative(JNIEnv * env, jobject obj, jclass klass)
{
	PORT_ACCESS_FROM_ENV(env);
	JNINativeMethod method;
	int rc;

	j9tty_printf(PORTLIB, "Java_com_ibm_j9_zostest_ZosNativesTest_registerNative is starting\n");

	method.name = "registeredNativeMethod";
	method.signature = "()I";
	method.fnPtr = (void *)registeredNativeMethod;
	rc = (*env)->RegisterNatives(env, klass, &method, 1);
	if (0 != rc) {
		j9tty_printf(PORTLIB, "Failed to register native method!\n");
		return JNI_FALSE;
	}

	j9tty_printf(PORTLIB, "Java_com_ibm_j9_zostest_ZosNativesTest_registerNative has finished\n");

	return JNI_TRUE;
}
