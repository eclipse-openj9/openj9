/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include <string.h>

void JNICALL 
Java_j9vm_test_jnichk_BufferOverrun_test(JNIEnv * env, jclass clazz)
{
	jobject array;
	jbyte* buffer;

	array = (*env)->NewByteArray(env, 0);
	if (array == NULL) {
		return;
	}

	buffer = (*env)->GetByteArrayElements(env, array, NULL);
	if (buffer == NULL) {
		return;
	}

	/* Illegally write into the buffer. Memorycheck should detect this. */
	buffer[0] = 0;

	/* We expect the error to be detected here: */
	(*env)->ReleaseByteArrayElements(env, array, buffer, JNI_ABORT);

	return;
}


/**
 * Call FindClass with a character buffer containing the class name.
 * During the call the buffer will be modified. The class's static initializer
 * calls a second native method which zeros the buffer.
 * Modifying an in-use buffer during a JNI call invokes undefined behaviour
 * and should be detected by -Xcheck:jni
 */
void JNICALL 
Java_j9vm_test_jnichk_ModifiedBuffer_test(JNIEnv* env, jclass clazz, jint phase)
{
	static char bufferToModify[] = "j9vm/test/jnichk/ModifiedBuffer$ClassToFind";

	if (phase == 1) {
		(*env)->FindClass(env, bufferToModify);
	} else {
		memset(bufferToModify, 0, sizeof(bufferToModify));
	}
}

/**
 * Call DeleteGlobalRef twice on the same reference
 *
 * In rare situations, another thread might create a global reference
 * at the same address just before second deletion starts. In such cases,
 * this test fails since it will be able to delete the global reference since
 * it is created by another thread. To avoid the possibility of this happening,
 * this test should be run with attach API disabled.
 * See JTC_JAT 43777: CMVC 198801: [MT]XcheckJNI deleteglobalreftwice failed
 * for details.
 *
 */
void JNICALL 
Java_j9vm_test_jnichk_DeleteGlobalRefTwice_test(JNIEnv* env, jclass clazz)
{
	jobject gref = (*env)->NewGlobalRef(env, clazz);

	(*env)->DeleteGlobalRef(env, gref);
	(*env)->DeleteGlobalRef(env, gref);
}

static void* 
getArrayElements(JNIEnv* env, jobject array, jchar type, jboolean* isCopy)
{
	switch (type) {
	case 'B':
		return (*env)->GetByteArrayElements(env, array, isCopy);
	case 'C':
		return (*env)->GetCharArrayElements(env, array, isCopy);
	case 'D':
		return (*env)->GetDoubleArrayElements(env, array, isCopy);
	case 'F':
		return (*env)->GetFloatArrayElements(env, array, isCopy);
	case 'I':
		return (*env)->GetIntArrayElements(env, array, isCopy);
	case 'J':
		return (*env)->GetLongArrayElements(env, array, isCopy);
	case 'S':
		return (*env)->GetShortArrayElements(env, array, isCopy);
	case 'Z':
		return (*env)->GetBooleanArrayElements(env, array, isCopy);
	}

	return NULL;
}

static void
releaseArrayElements(JNIEnv* env, jobject array, jchar type, void* data, jint mode)
{
	switch (type) {
	case 'B':
		(*env)->ReleaseByteArrayElements(env, array, data, mode);
		return;
	case 'C':
		(*env)->ReleaseCharArrayElements(env, array, data, mode);
		return;
	case 'D':
		(*env)->ReleaseDoubleArrayElements(env, array, data, mode);
		return;
	case 'F':
		(*env)->ReleaseFloatArrayElements(env, array, data, mode);
		return;
	case 'I':
		(*env)->ReleaseIntArrayElements(env, array, data, mode);
		return;
	case 'J':
		(*env)->ReleaseLongArrayElements(env, array, data, mode);
		return;
	case 'S':
		(*env)->ReleaseShortArrayElements(env, array, data, mode);
		return;
	case 'Z':
		(*env)->ReleaseBooleanArrayElements(env, array, data, mode);
		return;
	}
}

/*
 * This function performs the following steps:
 *  Use Get*ArrayElements to fetch the data from array
 *  If mod1 >= 0, modify the original array at the specified offset
 *  If mod2 >= 0, modify the copy of the data at the specified offset
 *  Release the data with the specified mode
 */
jboolean JNICALL
Java_j9vm_test_jnichk_ModifyArrayData_test(JNIEnv* env, jclass clazz, jobject array, jchar type, jint mod1, jint mod2, jint mode)
{
	jboolean isCopy;
	U_8 * data;

	data = getArrayElements(env, array, type, &isCopy);

	if (mod1 >= 0) {
		U_8* data2 = getArrayElements(env, array, type, NULL);
		data2[mod1] += 1;
		releaseArrayElements(env, array, type, data2, 0);
	}

	if (mod2 >= 0) {
		data[mod2] += 1;
	}

	releaseArrayElements(env, array, type, data, mode);

	return isCopy;
}

/*
 * Class:     j9vm_test_jnichk_ConcurrentGlobalReferenceModification
 * Method:    test
 * Signature: ()V
 * 
 * This create and destroy a global reference.
 * This method is called concurrently across many threads and stresses the concurrent access/mutation of the jniGlobalReferences pool.
 * (see CMVC 133653)
 */
void JNICALL 
Java_j9vm_test_jnichk_ConcurrentGlobalReferenceModification_test(JNIEnv * env, jclass clazz)
{
#define GREF_COUNT 256
	jobject gref[GREF_COUNT];
	UDATA i = 0;
	
	for (i = 0; i < GREF_COUNT; i++) {
		gref[i] = (*env)->NewGlobalRef(env, clazz);
	}
	for (i = 0; i < GREF_COUNT; i++) {
		(*env)->DeleteGlobalRef(env, gref[i]);
	}
}

/*
 * Class:     j9vm_test_jnichk_PrimitiveAlwaysCopy
 * Method:    testArray
 * Signature: ([Ljava/lang/Object;)Z
 */
jboolean JNICALL
Java_j9vm_test_jnichk_CriticalAlwaysCopy_testArray(JNIEnv* env, jclass clazz, jobjectArray array) 
{
	jboolean isCopy1,isCopy2,result = JNI_TRUE;
	
	/* First, get a pointer to the array */
	void * arrPointer1 = (*env)->GetPrimitiveArrayCritical(env,array,&isCopy1);
	void * arrPointer2 = (*env)->GetPrimitiveArrayCritical(env,array,&isCopy2);
	
	result = ((arrPointer1 != arrPointer2) && (isCopy1 == JNI_TRUE) && (isCopy2 == JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
	
	(*env)->ReleasePrimitiveArrayCritical(env,array,arrPointer1,JNI_ABORT);
	(*env)->ReleasePrimitiveArrayCritical(env,array,arrPointer2,JNI_ABORT);
	
	return result;
}

/*
 * Class:     j9vm_test_jnichk_PrimitiveAlwaysCopy
 * Method:    testString
 * Signature: (Ljava/lang/String;)Z
 */
jboolean JNICALL
Java_j9vm_test_jnichk_CriticalAlwaysCopy_testString(JNIEnv* env, jclass clazz, jstring str)
{
	jboolean isCopy1,isCopy2,result = JNI_TRUE;
	
	const jchar * strPointer1 = (*env)->GetStringCritical(env,str,&isCopy1);
	const jchar * strPointer2 = (*env)->GetStringCritical(env,str,&isCopy2);
	
	result = ((strPointer1 != strPointer2) && (isCopy1 == JNI_TRUE) && (isCopy2 == JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
	
	(*env)->ReleaseStringCritical(env,str,strPointer1);
	(*env)->ReleaseStringCritical(env,str,strPointer2);
	
	return result;
}
