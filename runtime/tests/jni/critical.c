/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
#include "j9port.h"
#include "j9.h"
#include "vm_api.h"

void
reportError(JNIEnv* env, jclass clazz)
{
	jmethodID mid = (*env)->GetStaticMethodID(env, clazz, "reportError", "()V");
	if(NULL != mid) {
		(*env)->CallStaticVoidMethod(env, clazz, mid);
	}
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease___3B(JNIEnv * env, jclass clazz, jbyteArray array)
{
	void* elems;
	jboolean isCopy;

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify___3BZ(JNIEnv * env, jclass clazz, jbyteArray array, jboolean commitChanges)
{
	void* elems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetArrayLength(env, array);
	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((U_8*)elems)[i]++;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, commitChanges == JNI_TRUE ? 0 : JNI_ABORT);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy___3B_3B(JNIEnv * env, jclass clazz, jbyteArray srcArray, jbyteArray destArray)
{
	void* srcElems;
	void* destElems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetArrayLength(env, srcArray);
	srcElems = (*env)->GetPrimitiveArrayCritical(env, srcArray, NULL);
	if(NULL == srcElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	destElems = (*env)->GetPrimitiveArrayCritical(env, destArray, &isCopy);
	if(NULL == destElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((U_8*)destElems)[i] = ((U_8*)srcElems)[i];
	}
	(*env)->ReleasePrimitiveArrayCritical(env, srcArray, srcElems, 0);
	(*env)->ReleasePrimitiveArrayCritical(env, destArray, destElems, 0);
	return isCopy;
}


jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease___3I(JNIEnv * env, jclass clazz, jintArray array)
{
	void* elems;
	jboolean isCopy;

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify___3IZ(JNIEnv * env, jclass clazz, jintArray array, jboolean commitChanges)
{
	void* elems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetArrayLength(env, array);
	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((U_32*)elems)[i]++;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, commitChanges == JNI_TRUE ? 0 : JNI_ABORT);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy___3I_3I(JNIEnv * env, jclass clazz, jintArray srcArray, jintArray destArray)
{
	void* srcElems;
	void* destElems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetArrayLength(env, srcArray);
	srcElems = (*env)->GetPrimitiveArrayCritical(env, srcArray, NULL);
	if(NULL == srcElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	destElems = (*env)->GetPrimitiveArrayCritical(env, destArray, &isCopy);
	if(NULL == destElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((U_32*)destElems)[i] = ((U_32*)srcElems)[i];
	}
	(*env)->ReleasePrimitiveArrayCritical(env, srcArray, srcElems, 0);
	(*env)->ReleasePrimitiveArrayCritical(env, destArray, destElems, 0);
	return isCopy;
}



jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease___3D(JNIEnv * env, jclass clazz, jdoubleArray array)
{
	void* elems;
	jboolean isCopy;

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify___3DZ(JNIEnv * env, jclass clazz, jdoubleArray array, jboolean commitChanges)
{
	void* elems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetArrayLength(env, array);
	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((jdouble*)elems)[i] += 1.0;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, commitChanges == JNI_TRUE ? 0 : JNI_ABORT);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy___3D_3D(JNIEnv * env, jclass clazz, jdoubleArray srcArray, jdoubleArray destArray)
{
	void* srcElems;
	void* destElems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetArrayLength(env, srcArray);
	srcElems = (*env)->GetPrimitiveArrayCritical(env, srcArray, NULL);
	if(NULL == srcElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	destElems = (*env)->GetPrimitiveArrayCritical(env, destArray, &isCopy);
	if(NULL == destElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((U_64*)destElems)[i] = ((U_64*)srcElems)[i];
	}
	(*env)->ReleasePrimitiveArrayCritical(env, srcArray, srcElems, 0);
	(*env)->ReleasePrimitiveArrayCritical(env, destArray, destElems, 0);
	return isCopy;
}



jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease__Ljava_lang_String_2(JNIEnv * env, jclass clazz, jstring string)
{
	const jchar* elems;
	jboolean isCopy;

	elems = (*env)->GetStringCritical(env, string, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	(*env)->ReleaseStringCritical(env, string, elems);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify__Ljava_lang_String_2(JNIEnv * env, jclass clazz, jstring string)
{
	const jchar* elems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetStringLength (env, string);
	elems = (*env)->GetStringCritical(env, string, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		((jchar*)elems)[i] += ('a' - 'A');
	}
	(*env)->ReleaseStringCritical(env, string, elems);
	return isCopy;
}


jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy__Ljava_lang_String_2Ljava_lang_String_2(JNIEnv * env, jclass clazz, jstring srcString, jstring destString)
{
	const jchar* srcElems;
	jchar* destElems;
	jboolean isCopy;
	jint elementCount;
	jint i;

	elementCount = (*env)->GetStringLength(env, srcString);
	srcElems = (*env)->GetStringCritical(env, srcString, NULL);
	if(NULL == srcElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	destElems = (jchar*)(*env)->GetStringCritical(env, destString, &isCopy);
	if(NULL == destElems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	for(i = 0; i < elementCount; i++) {
		destElems[i] = srcElems[i];
	}
	(*env)->ReleaseStringCritical(env, srcString, srcElems);
	(*env)->ReleaseStringCritical(env, destString, destElems);
	return isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_holdsVMAccess(JNIEnv * env, jclass clazz, jbyteArray array)
{
	void* elems;
	jboolean isCopy;
	jboolean result;

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}

	result = ((J9VMThread*)env)->publicFlags &  J9_PUBLIC_FLAGS_VM_ACCESS ? JNI_TRUE : JNI_FALSE;

	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return result;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_returnsDirectPointer(JNIEnv * env, jclass clazz, jbyteArray array)
{
	void* elems;
	jboolean isCopy;

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return !isCopy;
}

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_acquireAndSleep(JNIEnv * env, jclass clazz, jbyteArray array, jlong millis)
{
	void* elems;
	jboolean isCopy;
	JavaVM* jniVM;
    J9ThreadEnv* threadEnv;

	/* Get the thread functions */
	(*env)->GetJavaVM(env, &jniVM);
	(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}

	threadEnv->sleep(millis);

	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return isCopy;
}


jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_acquireAndCallIn(JNIEnv * env, jclass clazz, jbyteArray array, jobject thread)
{
	void* elems;
	jboolean isCopy;
	jclass acquireAndCallInThread;
	jmethodID reportCallInMID;

	acquireAndCallInThread = (*env)->GetObjectClass(env, thread);
	if(NULL == acquireAndCallInThread) {
		return JNI_FALSE;
	}
	reportCallInMID = (*env)->GetMethodID(env, acquireAndCallInThread, "reportCallIn", "()V");
	if(NULL == reportCallInMID) {
		return JNI_FALSE;
	}

	elems = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems) {
		reportError(env, clazz);
		return JNI_FALSE;
	}

	(*env)->CallVoidMethod(env, thread, reportCallInMID);

	(*env)->ReleasePrimitiveArrayCritical(env, array, elems, 0);
	return JNI_TRUE;
}


jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_acquireDiscardAndGC(JNIEnv * env, jclass clazz, jbyteArray array, jlongArray addresses)
{
	void* elems1;
	void* elems2;
	jlong* addressesElems;
	jmethodID discardAndGCMID;
	jboolean isCopy;
	jboolean result = JNI_FALSE;

	discardAndGCMID = (*env)->GetStaticMethodID(env, clazz, "discardAndGC", "()V");
	if(NULL == discardAndGCMID) {
		return JNI_FALSE;
	}

	elems1 = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems1 || isCopy == JNI_TRUE) {
		reportError(env, clazz);
		return JNI_FALSE;
	}

	(*env)->CallStaticVoidMethod(env, clazz, discardAndGCMID);

	elems2 = (*env)->GetPrimitiveArrayCritical(env, array, &isCopy);
	if(NULL == elems2 || isCopy == JNI_TRUE) {
		(*env)->ReleasePrimitiveArrayCritical(env, array, elems1, 0);
		reportError(env, clazz);
		return JNI_FALSE;
	}

	if(elems1 == elems2) {
		result = JNI_TRUE;
	}

	addressesElems = (jlong*)(*env)->GetPrimitiveArrayCritical(env, addresses, &isCopy);
	if(NULL != addressesElems) {
		addressesElems[0] = (jlong)(UDATA)elems1;
		addressesElems[1] = (jlong)(UDATA)elems2;
		(*env)->ReleasePrimitiveArrayCritical(env, addresses, (void*)addressesElems, 0);
	}

	(*env)->ReleasePrimitiveArrayCritical(env, array, elems2, 0);
	(*env)->ReleasePrimitiveArrayCritical(env, array, elems1, 0);
	return result;
}
