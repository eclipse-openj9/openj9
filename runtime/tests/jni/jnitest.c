/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "vmi.h"
/*
 * jvm.h is required by MemoryAllocator_allocateMemory:
 * https://github.com/eclipse/openj9/issues/1377
 */
#include "../j9vm/jvm.h"
#ifdef WIN32
#include <stdlib.h>
#include <malloc.h>
#else
#include <pthread.h>
#include <stdlib.h>
#endif

jboolean JNICALL
Java_j9vm_test_jni_GetObjectRefTypeTest_getObjectRefTypeTest(JNIEnv *env, jclass clazz, jobject stackArg)
{
	jobject stackBased = (*env)->AllocObject(env, clazz);
	jobject global = (*env)->NewGlobalRef(env, stackArg);
	jobject weak = (*env)->NewWeakGlobalRef(env, stackArg);
	jobject poolBased;
	jboolean rc = JNI_TRUE;

	(*env)->PushLocalFrame(env, 100);
	poolBased = (*env)->AllocObject(env, clazz);
	
	if ((*env)->GetObjectRefType(env, NULL) != JNIInvalidRefType) {
		rc = JNI_FALSE;
	}
	if ((*env)->GetObjectRefType(env, clazz) != JNILocalRefType) {
		rc = JNI_FALSE;
	}
	if ((*env)->GetObjectRefType(env, stackArg) != JNILocalRefType) {
		rc = JNI_FALSE;
	}
	if ((*env)->GetObjectRefType(env, stackBased) != JNILocalRefType) {
		rc = JNI_FALSE;
	}
	if ((*env)->GetObjectRefType(env, global) != JNIGlobalRefType) {
		rc = JNI_FALSE;
	}
	if ((*env)->GetObjectRefType(env, weak) != JNIWeakGlobalRefType) {
		rc = JNI_FALSE;
	}

	(*env)->DeleteGlobalRef(env, global);
	(*env)->DeleteWeakGlobalRef(env, weak);

	return rc;
}

jint JNICALL
Java_jvmti_test_nativeMethodPrefixes_UnwrappedNative_nat(JNIEnv *env, jclass clazz)
{
	return 33;
}

jint JNICALL
Java_jvmti_test_nativeMethodPrefixes_DirectNative_gac4gac3gac2gac1nat(JNIEnv *env, jclass clazz)
{
	return 22;
}

jint JNICALL
Java_jvmti_test_nativeMethodPrefixes_WrappedNative_nat(JNIEnv *env, jclass clazz)
{
	return 11;
}

jboolean JNICALL 
Java_j9vm_test_jni_JNIFloatTest_floatJNITest(JNIEnv * env, jobject rcv)
{
	jclass jFclass;
	jmethodID initMid;
	jobject jFo;
	jmethodID mid;
	float org = 0.7f;
	float f;

	jFclass = (*env)->FindClass (env, "java/lang/Float");
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	initMid = (*env)->GetMethodID (env, jFclass, "<init>", "(F)V");
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	jFo = (*env)->NewObject (env, jFclass, initMid, org);
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	mid = (*env)->GetMethodID (env, jFclass, "floatValue", "()F");
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	f = (*env)->CallFloatMethod (env, jFo, mid);
	if (f == org) return JNI_TRUE;
	return JNI_FALSE;
}


jint JNICALL 
Java_j9vm_test_libraryhandle_LibHandleTest_libraryHandleTest(JNIEnv * env, jclass clazz, jstring libName)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );
	const char * cString;
	jint result;
	UDATA handle1;
	UDATA handle2;

	cString = (const char *) (*env)->GetStringUTFChars(env, libName, NULL);
	if (!cString) {
		result = 1;
		goto done0;
	}

	if (j9sl_open_shared_library((char *)cString, &handle1, TRUE)) {
		result = 101;
		goto done1;
	}
	if (j9sl_open_shared_library((char *)cString, &handle2, TRUE)) {
		result = 102;
		goto done2;
	}
	result = ((handle1 == handle2) ? 0 : 500);

	j9sl_close_shared_library(handle2);
done2:
	j9sl_close_shared_library(handle1);
done1:
	(*env)->ReleaseStringUTFChars(env, libName, cString);
done0:
	return result;
}


jobject JNICALL 
Java_j9vm_test_libraryhandle_MultipleLibraryLoadTest_vmVersion(JNIEnv * env, jobject rcv)
{
	return (*env)->NewStringUTF(env, J9_DLL_VERSION_STRING);
}



jint JNICALL 
Java_j9vm_test_jni_JNIMultiFloatTest_floatJNITest2(JNIEnv * env, jobject rcv)
{
	jclass jFclass;
	jmethodID initMid;
	jobject jFo;
	jmethodID mid;

#define CHECK_FOR_EXCEPTIONS do{ if ((*env)->ExceptionOccurred(env)) return 256; }while(0)
	
	jfloat FLOAT_A = 0.7f;
	jint INT_B = 375;
	jfloat FLOAT_C = 0.55f;
	jdouble DOUBLE_D = 2.1;
	jfloat FLOAT_E = 2.5f;
	jdouble DOUBLE_F = 4.6;
	jfloat FLOAT_G = 6.1f;
	jfloat f;
	jdouble d;
	jint i;
	jint result = 0;

	jFclass = (*env)->FindClass (env, "j9vm/test/jni/FloatContainer");
	CHECK_FOR_EXCEPTIONS;

	initMid = (*env)->GetMethodID (env, jFclass, "<init>", "(FIF)V");
	CHECK_FOR_EXCEPTIONS;
	jFo = (*env)->NewObject (env, jFclass, initMid, FLOAT_A, INT_B, FLOAT_C);
	CHECK_FOR_EXCEPTIONS;

	mid = (*env)->GetMethodID (env, jFclass, "setValues", "(DFDF)V");
	CHECK_FOR_EXCEPTIONS;
	(*env)->CallVoidMethod (env, jFo, mid, DOUBLE_D, FLOAT_E, DOUBLE_F, FLOAT_G);
	CHECK_FOR_EXCEPTIONS;

#define CHECK_VALUE(callFn, mtdName, mtdSig, varName, constName, resultFlag) do {\
	mid = (*env)->GetMethodID (env, jFclass, mtdName, mtdSig);\
	CHECK_FOR_EXCEPTIONS;\
	varName = (*env)->callFn (env, jFo, mid);\
	if (varName != constName)  result |= resultFlag;\
	CHECK_FOR_EXCEPTIONS; }while(0)

	CHECK_VALUE(CallFloatMethod, "getA", "()F", f, FLOAT_A, 1);
	CHECK_VALUE(CallIntMethod, "getB", "()I", i, INT_B, 2);
	CHECK_VALUE(CallFloatMethod, "getC", "()F", f, FLOAT_C, 4);
	CHECK_VALUE(CallDoubleMethod, "getD", "()D", d, DOUBLE_D, 8);
	CHECK_VALUE(CallFloatMethod, "getE", "()F", f, FLOAT_E, 16);
	CHECK_VALUE(CallDoubleMethod, "getF", "()D", d, DOUBLE_F, 32);
	CHECK_VALUE(CallFloatMethod, "getG", "()F", f, FLOAT_G, 64);

	return result;
}



/* test that PushLocalFrame and PopLocalFrame really do manage frames. If they don't this
 * test should run out of memory. Note that an implementation which collapses duplicate 
 * references could defeat this test
 */
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame1(JNIEnv * env, jobject rcv)
{
	jweak weakRef;
	jclass objClass, sysClass;
	jobject test;
	jmethodID gcmid;
	jint i;

	objClass  = (*env)->FindClass(env, "java/lang/Object");
	if (objClass == NULL) return JNI_FALSE;

	sysClass = (*env)->FindClass(env, "java/lang/System");
	if (sysClass == NULL) return JNI_FALSE;

	gcmid = (*env)->GetStaticMethodID(env, sysClass, "gc", "()V");
	if (gcmid == NULL) return JNI_FALSE;

	if ((*env)->PushLocalFrame(env, 1024)) return JNI_FALSE;

	test = (*env)->AllocObject(env, objClass);
	if (test == NULL) return JNI_FALSE;

	weakRef = (*env)->NewWeakGlobalRef(env, test);
	if (weakRef == NULL) return JNI_FALSE;

	if ((*env)->IsSameObject(env, weakRef, NULL)) {
		/* object still has a strong reference! It shouldn't be NULL yet */
		return JNI_FALSE;
	}

	(*env)->PopLocalFrame(env, NULL);

	/* at this point there are no strong references to the test object */
	for (i = 0; i < 1024; i++) {
		(*env)->CallStaticVoidMethod(env, sysClass, gcmid);
		if ((*env)->ExceptionCheck(env)) return JNI_FALSE;

		if ((*env)->IsSameObject(env, weakRef, NULL)) {
			(*env)->DeleteWeakGlobalRef(env, weakRef);
			return JNI_TRUE;
		}
	}

	/* after 1024 GCs the object still hasn't been collected. That's not good */
	(*env)->DeleteWeakGlobalRef(env, weakRef);
	return JNI_FALSE;
}



/* test that PushLocalFrame and PopLocalFrame really do manage frames. If they don't this
 * test should run out of memory. Note that an implementation which collapses duplicate 
 * references could defeat this test
 */
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame2(JNIEnv * env, jobject rcv)
{
	jweak weakRef;
	jclass objClass, sysClass;
	jobject test;
	jmethodID gcmid;
	jint i;

	objClass  = (*env)->FindClass(env, "java/lang/Object");
	if (objClass == NULL) return JNI_FALSE;

	sysClass = (*env)->FindClass(env, "java/lang/System");
	if (sysClass == NULL) return JNI_FALSE;

	gcmid = (*env)->GetStaticMethodID(env, sysClass, "gc", "()V");
	if (gcmid == NULL) return JNI_FALSE;

	if ((*env)->PushLocalFrame(env, 0)) return JNI_FALSE;
	if ((*env)->PushLocalFrame(env, 1024)) return JNI_FALSE;

	test = (*env)->AllocObject(env, objClass);
	if (test == NULL) return JNI_FALSE;

	weakRef = (*env)->NewWeakGlobalRef(env, test);
	if (weakRef == NULL) return JNI_FALSE;

	if ((*env)->IsSameObject(env, weakRef, NULL)) {
		/* object still has a strong reference! It shouldn't be NULL yet */
		return JNI_FALSE;
	}

	(*env)->PopLocalFrame(env, NULL);
	(*env)->PopLocalFrame(env, NULL);

	/* at this point there are no strong references to the test object */
	for (i = 0; i < 1024; i++) {
		(*env)->CallStaticVoidMethod(env, sysClass, gcmid);
		if ((*env)->ExceptionCheck(env)) return JNI_FALSE;

		if ((*env)->IsSameObject(env, weakRef, NULL)) {
			(*env)->DeleteWeakGlobalRef(env, weakRef);
			return JNI_TRUE;
		}
	}

	/* after 1024 GCs the object still hasn't been collected. That's not good */
	(*env)->DeleteWeakGlobalRef(env, weakRef);
	return JNI_FALSE;
}



/* test that PushLocalFrame and PopLocalFrame really do manage frames. If they don't this
 * test should run out of memory. Note that an implementation which collapses duplicate 
 * references could defeat this test
 */
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame3(JNIEnv * env, jobject rcv)
{
	jweak weakRef;
	jclass objClass, sysClass;
	jobject test;
	jmethodID gcmid;
	jint i;

	objClass  = (*env)->FindClass(env, "java/lang/Object");
	if (objClass == NULL) return JNI_FALSE;

	sysClass = (*env)->FindClass(env, "java/lang/System");
	if (sysClass == NULL) return JNI_FALSE;

	gcmid = (*env)->GetStaticMethodID(env, sysClass, "gc", "()V");
	if (gcmid == NULL) return JNI_FALSE;

	if ((*env)->PushLocalFrame(env, 0)) return JNI_FALSE;
	if ((*env)->PushLocalFrame(env, 64)) return JNI_FALSE;
	if ((*env)->PushLocalFrame(env, 64)) return JNI_FALSE;

	test = (*env)->AllocObject(env, objClass);
	if (test == NULL) return JNI_FALSE;

	test = (*env)->PopLocalFrame(env, test);
	if (test == NULL) return JNI_FALSE;

	weakRef = (*env)->NewWeakGlobalRef(env, test);
	if (weakRef == NULL) return JNI_FALSE;

	if ((*env)->IsSameObject(env, weakRef, NULL)) {
		/* object still has a strong reference! It shouldn't be NULL yet */
		return JNI_FALSE;
	}

	(*env)->PopLocalFrame(env, NULL);
	(*env)->PopLocalFrame(env, NULL);

	/* at this point there are no strong references to the test object */
	for (i = 0; i < 1024; i++) {
		(*env)->CallStaticVoidMethod(env, sysClass, gcmid);
		if ((*env)->ExceptionCheck(env)) return JNI_FALSE;

		if ((*env)->IsSameObject(env, weakRef, NULL)) {
			(*env)->DeleteWeakGlobalRef(env, weakRef);
			return JNI_TRUE;
		}
	}

	/* after 1024 GCs the object still hasn't been collected. That's not good */
	(*env)->DeleteWeakGlobalRef(env, weakRef);
	return JNI_FALSE;
}



/* test that PushLocalFrame and PopLocalFrame really do manage frames. If they don't this
 * test should run out of memory. Note that an implementation which collapses duplicate 
 * references could defeat this test
 */
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame4(JNIEnv * env, jobject rcv)
{
	jweak weakRef;
	jclass objClass, sysClass;
	jobject test;
	jmethodID gcmid;
	jint i;

	objClass  = (*env)->FindClass(env, "java/lang/Object");
	if (objClass == NULL) return JNI_FALSE;

	sysClass = (*env)->FindClass(env, "java/lang/System");
	if (sysClass == NULL) return JNI_FALSE;

	gcmid = (*env)->GetStaticMethodID(env, sysClass, "gc", "()V");
	if (gcmid == NULL) return JNI_FALSE;

	if ((*env)->PushLocalFrame(env, 0)) return JNI_FALSE;
	if ((*env)->PushLocalFrame(env, 64)) return JNI_FALSE;

	test = (*env)->AllocObject(env, objClass);
	if (test == NULL) return JNI_FALSE;

	/* create an extra frame just to make sure that it doesn't interfere with test */
	if ((*env)->PushLocalFrame(env, 64)) return JNI_FALSE;
	(*env)->PopLocalFrame(env, NULL);

	weakRef = (*env)->NewWeakGlobalRef(env, test);
	if (weakRef == NULL) return JNI_FALSE;

	if ((*env)->IsSameObject(env, weakRef, NULL)) {
		/* object still has a strong reference! It shouldn't be NULL yet */
		return JNI_FALSE;
	}

	(*env)->PopLocalFrame(env, NULL);
	(*env)->PopLocalFrame(env, NULL);

	/* at this point there are no strong references to the test object */
	for (i = 0; i < 1024; i++) {
		(*env)->CallStaticVoidMethod(env, sysClass, gcmid);
		if ((*env)->ExceptionCheck(env)) return JNI_FALSE;

		if ((*env)->IsSameObject(env, weakRef, NULL)) {
			(*env)->DeleteWeakGlobalRef(env, weakRef);
			return JNI_TRUE;
		}
	}

	/* after 1024 GCs the object still hasn't been collected. That's not good */
	(*env)->DeleteWeakGlobalRef(env, weakRef);
	return JNI_FALSE;
}



/* leave pushed frames on the stack when we return */
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrameNeverPop(JNIEnv * env, jobject rcv)
{
	jclass objClass;
	jobject test;

	objClass  = (*env)->FindClass(env, "java/lang/Object");
	if (objClass == NULL) return JNI_FALSE;

	if ((*env)->PushLocalFrame(env, 64)) return JNI_FALSE;
	if ((*env)->PushLocalFrame(env, 64)) return JNI_FALSE;

	test = (*env)->AllocObject(env, objClass);
	if (test == NULL) return JNI_FALSE;

	return JNI_TRUE;
}



jint JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileInt(JNIEnv * env, jobject rcv)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileInt", "I");	

	return (*env)->GetIntField(env, rcv, fid);
}



jfloat JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileFloat(JNIEnv * env, jobject rcv)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileFloat", "F");

	return (*env)->GetFloatField(env, rcv, fid);
}



jlong JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileLong(JNIEnv * env, jobject rcv)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileLong", "J");

	return (*env)->GetLongField(env, rcv, fid);
}



jdouble JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileDouble(JNIEnv * env, jobject rcv)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileDouble", "D");

	return (*env)->GetDoubleField(env, rcv, fid);
}



jdouble JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticDouble(JNIEnv * env, jclass cls)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticDouble", "D");

	return (*env)->GetStaticDoubleField(env, cls, fid);
}



jlong JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticLong(JNIEnv * env, jclass cls)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticLong", "J");

	return (*env)->GetStaticLongField(env, cls, fid);
}



jint JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticInt(JNIEnv * env, jclass cls)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticInt", "I");

	return (*env)->GetStaticIntField(env, cls, fid);
}



jfloat JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticFloat(JNIEnv * env, jobject cls)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticFloat", "F");

	return (*env)->GetStaticFloatField(env, cls, fid);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileDouble(JNIEnv * env, jobject rcv, jdouble aDouble)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileDouble", "D");

	(*env)->SetDoubleField(env, rcv, fid, aDouble);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileFloat(JNIEnv * env, jobject rcv, jfloat aFloat)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileFloat", "F");

	(*env)->SetFloatField(env, rcv, fid, aFloat);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileInt(JNIEnv * env, jobject rcv, jint anInt)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileInt", "I");	

	(*env)->SetIntField(env, rcv, fid, anInt);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileLong(JNIEnv * env, jobject rcv, jlong aLong)
{
	jclass cls = (*env)->GetObjectClass(env, rcv);
	jfieldID fid = NULL;
	fid = (*env)->GetFieldID(env, cls, "volatileLong", "J");

	(*env)->SetLongField(env, rcv, fid, aLong);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticDouble(JNIEnv * env, jclass cls, jdouble aDouble)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticDouble", "D");

	(*env)->SetStaticDoubleField(env, cls, fid, aDouble);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticFloat(JNIEnv * env, jclass cls, jfloat aFloat)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticFloat", "F");

	(*env)->SetStaticFloatField(env, cls, fid, aFloat);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticInt(JNIEnv * env, jclass cls, jint anInt)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticInt", "I");

	(*env)->SetStaticIntField(env, cls, fid, anInt);
}



void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticLong(JNIEnv * env, jclass cls, jlong aLong)
{
	jfieldID fid = NULL;
	fid = (*env)->GetStaticFieldID(env, cls, "volatileStaticLong", "J");

	(*env)->SetStaticLongField(env, cls, fid, aLong);
}


void JNICALL
Java_j9vm_test_jni_NullRefTest_test(JNIEnv * env, jclass clazz) 
{
	jobject obj;
	jobject weak;
	jobject weak2;
	jobject strong;
	jmethodID mid;
	jclass error;

	error = (*env)->FindClass(env, "java/lang/Error");
	if (error == NULL) {
		return;
	}

	obj = (*env)->NewLocalRef(env, NULL);
	if (obj != NULL) {
		(*env)->DeleteLocalRef(env, obj);
		(*env)->ThrowNew(env, error, "(*env)->NewLocalRef(env, NULL) failed to return NULL");
		return;
	}

	strong = (*env)->NewGlobalRef(env, NULL);
	if (strong != NULL) {
		(*env)->DeleteGlobalRef(env, strong);
		(*env)->ThrowNew(env, error, "(*env)->NewGlobalRef(env, NULL) failed to return NULL");
		return;
	}

	weak = (*env)->NewWeakGlobalRef(env, NULL);
	if (weak != NULL) {
		(*env)->DeleteWeakGlobalRef(env, weak);
		(*env)->ThrowNew(env, error, "(*env)->NewWeakGlobalRef(env, NULL) failed to return NULL");
		return;
	}

	obj = (*env)->AllocObject(env, clazz);
	if (obj == NULL) {
		return;
	}

	weak = (*env)->NewWeakGlobalRef(env, obj);
	if (weak == NULL) {
		(*env)->ThrowNew(env, error, "(*env)->NewWeakGlobalRef(env, obj) returned NULL");
		return;
	}

	/* Delete obj, call GC */
	(*env)->DeleteLocalRef(env, obj);
	clazz = (*env)->FindClass(env, "java/lang/System");
	if (clazz == NULL) {
		(*env)->DeleteWeakGlobalRef(env, weak);
		return;
	}
	mid = (*env)->GetStaticMethodID(env, clazz, "gc", "()V");
	if (mid == NULL) {
		(*env)->DeleteWeakGlobalRef(env, weak);
		return;
	}
	while ( JNI_FALSE == (*env)->IsSameObject(env, weak, NULL) ) {
		(*env)->CallStaticVoidMethod(env, clazz, mid);
		if ( (*env)->ExceptionCheck(env) ) {
			(*env)->DeleteWeakGlobalRef(env, weak);
			return;
		}
	}

	obj = (*env)->NewLocalRef(env, weak);
	if (obj != NULL) {
		(*env)->DeleteWeakGlobalRef(env, weak);
		(*env)->DeleteLocalRef(env, obj);
		(*env)->ThrowNew(env, error, "(*env)->NewLocalRef(env, weak) failed to return NULL");
		return;
	}

	strong = (*env)->NewGlobalRef(env, weak);
	if (strong != NULL) {
		(*env)->DeleteWeakGlobalRef(env, weak);
		(*env)->DeleteGlobalRef(env, strong);
		(*env)->ThrowNew(env, error, "(*env)->NewGlobalRef(env, weak) failed to return NULL");
		return;
	}

	weak2 = (*env)->NewWeakGlobalRef(env, weak);
	if (weak2 != NULL) {
		(*env)->DeleteWeakGlobalRef(env, weak);
		(*env)->DeleteWeakGlobalRef(env, weak2);
		(*env)->ThrowNew(env, error, "(*env)->NewWeakGlobalRef(env, weak) failed to return NULL");
		return;
	}

	(*env)->DeleteWeakGlobalRef(env, weak);
}
 
jint JNICALL
Java_j9vm_test_classloading_VMAccess_getNumberOfNodes(JNIEnv * env, jobject this) 
{
	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;
	jint result;
	result = javaVM->contendedLoadTable->numberOfNodes;
	return result;
}

#if defined(J9VM_OPT_JVMTI)
jint JNICALL

Java_com_ibm_jvmti_tests_util_TestRunner_callLoadAgentLibraryOnAttach(JNIEnv * env, jclass clazz, jstring libraryName, jstring libraryOptions)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	const char *libraryNameUTF;
	const char *libraryOptionsUTF;
	jboolean isCopy;
	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;

	libraryNameUTF = (*env)->GetStringUTFChars(env, libraryName, &isCopy);
	libraryOptionsUTF = (*env)->GetStringUTFChars(env, libraryOptions, &isCopy);
	return (javaVM->loadAgentLibraryOnAttach)(javaVM, libraryNameUTF, libraryOptionsUTF, TRUE) ;
}
#endif

BOOLEAN isSystemPropertyEqual(JNIEnv* env, const char* utfPropName,  const char* expectedPropValue)
{
	jclass clazz;
	jstring prop, propname;
	jmethodID meth = NULL;
	BOOLEAN result = FALSE;

	clazz = (*env)->FindClass(env, "java/lang/System");
	meth = (*env)->GetStaticMethodID(env, clazz, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
	if (NULL != meth) {
		char *propValue;

		propname = (*env)->NewStringUTF(env, utfPropName);
		prop = (jstring) (*env)->CallStaticObjectMethod(env, clazz, meth, propname);
		if (NULL != prop) {
			propValue = (char*) (*env)->GetStringUTFChars(env, prop, NULL);
			if (NULL != propValue) {
				if (!strcmp(propValue, expectedPropValue)) {
					result = TRUE;
				}
				(*env)->ReleaseStringChars(env, prop, (jchar *) propValue);
			}
		}
	}
	return result;
}

/*
 * Class:     j9vm_utils_JNI
 * Method:    NewDirectByteBuffer
 * Signature: (JJ)Ljava/nio/ByteBuffer;
 */
jobject JNICALL
Java_j9vm_utils_JNI_NewDirectByteBuffer(JNIEnv *env, jclass clazz, jlong address, jlong capacity)
{
	return (*env)->NewDirectByteBuffer(env, (void*)(UDATA)address, capacity);
}

/*
 * Class:     j9vm_utils_JNI
 * Method:    GetDirectBufferAddress
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
jlong JNICALL
Java_j9vm_utils_JNI_GetDirectBufferAddress(JNIEnv *env, jclass clazz, jobject buffer)
{
	return (jlong)(UDATA)((*env)->GetDirectBufferAddress(env, buffer));
}

/*
 * Class:     j9vm_utils_JNI
 * Method:    GetDirectBufferCapacity
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
jlong JNICALL
Java_j9vm_utils_JNI_GetDirectBufferCapacity(JNIEnv *env, jclass clazz, jobject buffer)
{
	return (*env)->GetDirectBufferCapacity(env, buffer);
}

jboolean JNICALL
Java_j9vm_test_memory_MemoryAllocator_allocateMemory(JNIEnv * env, jclass clazz, jlong byteAmount, jint category)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;
	void *result;

/* malloc() is overridden in jvm.h and takes a parameter CURRENT_MEMORY_CATEGORY. */
#undef CURRENT_MEMORY_CATEGORY
/*
 * force the category to the parameter passed in by the test.  Normally, the calling source file will
 * set the CURRENT_MEMORY_CATEGORY to the appropriate value.
 */
#define CURRENT_MEMORY_CATEGORY category
	result = malloc((size_t) byteAmount);
#undef CURRENT_MEMORY_CATEGORY
#define CURRENT_MEMORY_CATEGORY J9MEM_CATEGORY_SUN_JCL
	return (NULL != result);
}

jboolean JNICALL
Java_j9vm_test_memory_MemoryAllocator_allocateMemory32(JNIEnv * env, jclass clazz, jlong byteAmount, jint category)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;
	void *result;
	result = j9mem_allocate_memory32((UDATA) byteAmount, OMRMEM_CATEGORY_VM);
	return (NULL != result);
}

jboolean JNICALL
Java_j9vm_test_harmonyvmi_Test_testGetEnv(JNIEnv *env, jclass clazz)
{
	JavaVM* javaVM;
	VMInterface* vmi;
	vmiVersion version = VMI_VERSION_2_0;
	vmiError vmiRc;
	jclass java_lang_Exception = (*env)->FindClass(env, "java/lang/Exception");
	jint rc;

	rc = (*env)->GetJavaVM(env, &javaVM);
	if (0 != rc) {
		(*env)->ThrowNew(env, java_lang_Exception, "Could not navigate env->javaVM");
		return JNI_FALSE;
	}

	rc = (*javaVM)->GetEnv(javaVM, (void**)&vmi, HARMONY_VMI_VERSION_2_0);
	if (0 != rc) {
		(*env)->ThrowNew(env, java_lang_Exception, "GetEnv(HARMONY_VMI_VERSION_2_0) failed!");
		return JNI_FALSE;
	}

	vmiRc = (*vmi)->CheckVersion(vmi, &version);
	if (VMI_ERROR_NONE != vmiRc) {
		(*env)->ThrowNew(env, java_lang_Exception, "VMI CheckVersion() failed");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

typedef struct JvmInfo {
	JNIEnv *env;
	jclass clazz;
	char* threadName;
	jint result;
} JvmInfo;

static void* callAttachCurrentThreadAsDaemon(void *arg) {
	JvmInfo *vmInfo = (JvmInfo*) arg;
	JavaVM *javaVM;
	JavaVMAttachArgs attachArgs = {JNI_VERSION_1_2, NULL, NULL};
	jint rc;

	attachArgs.name = vmInfo->threadName; 
	rc = (*vmInfo->env)->GetJavaVM(vmInfo->env, &javaVM);
	if (0 != rc) {
		vmInfo->result = JNI_FALSE;
	}
	vmInfo->result = (*javaVM)->AttachCurrentThreadAsDaemon(javaVM, (void *) &(vmInfo->env), &attachArgs);
	return NULL;
}

jint JNICALL
Java_j9vm_test_jni_Utf8Test_testAttachCurrentThreadAsDaemon(JNIEnv *env, jclass clazz, jbyteArray threadName)
{
#ifdef LINUX
	JvmInfo *vmInfo;
	char *threadNameCopy;
	pthread_t newThread;
	void *result;
	jsize threadNameSize;

	threadNameSize = (*env)->GetArrayLength(env, threadName);
	if (NULL == (vmInfo = malloc(sizeof(JvmInfo)))) {
		return JNI_FALSE;
	}
	if (NULL == (threadNameCopy = malloc(threadNameSize+1))) {
		free(vmInfo);
		return JNI_FALSE;
	}
	(*env)->GetByteArrayRegion(env, threadName,  0, threadNameSize, (jbyte *)threadNameCopy);
	threadNameCopy[threadNameSize] = '\0';

	vmInfo->env = env;
	vmInfo->clazz = clazz;
	vmInfo->threadName = threadNameCopy;
	vmInfo->result = 0;

	pthread_create(&newThread, NULL, callAttachCurrentThreadAsDaemon, (void *) vmInfo);
	pthread_join(newThread, &result);

	free(vmInfo);
	free(threadNameCopy);
	return vmInfo->result;
#else
	/* test runs only on linux */
	return -1;
#endif
}

jint JNICALL 
JNI_OnLoad(JavaVM * vm, void *reserved)
{
	JNIEnv * env;
	jint returnValue = JNI_VERSION_1_8;

	if (JNI_OK == (*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_8)) {
		if (isSystemPropertyEqual(env, "j9vm.test.jnitest.fail", "yes")) {
			returnValue = JNI_ERR;
		}
		if (isSystemPropertyEqual(env, "j9vm.test.jnitest.throw", "yes")) {
			jclass clazz;
			clazz = (*env)->FindClass(env, "java/lang/Exception");
			(*env)->ThrowNew(env, clazz, "JNI_OnLoad");
		}
	}
	return returnValue;
}

void JNICALL 
JNI_OnUnload(JavaVM * vm, void *reserved)
{
	return;
}

#ifndef WIN32

static pthread_key_t key;
static jboolean worked = JNI_FALSE;

static void
detachThread(void *p)
{
	PORT_ACCESS_FROM_VMC((J9VMThread*)p);
	JNIEnv *env = p;
	JavaVM *vm = NULL;
	(*env)->GetJavaVM(env, &vm);
	jint result = (*vm)->DetachCurrentThread(vm);
	if (JNI_OK == result) {
		j9tty_printf(PORTLIB, "thread detached\n");
		worked = JNI_TRUE;
	} else {
		j9tty_printf(PORTLIB, "\n*** failed to detach current thread rc = %d ***\n", result);
	}
}

static void *
threadproc(void *p)
{
	PORT_ACCESS_FROM_JAVAVM((J9JavaVM*)p);
	JNIEnv *env = NULL;
	JavaVM *vm = p;
	jint result = (*vm)->AttachCurrentThread(vm, (void **) &env, NULL);
	if (JNI_OK == result) {
		j9tty_printf(PORTLIB, "thread attached\n");
		pthread_setspecific(key, (void *) env);
	} else {
		j9tty_printf(PORTLIB, "\n*** failed to attach current thread rc = %d ***\n", result);
	}
	j9tty_printf(PORTLIB, "exitting pthread\n");
	pthread_exit(NULL);
}

#endif

jboolean JNICALL
Java_j9vm_test_jni_PthreadTest_attachAndDetach(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_VMC((J9VMThread*)env);
#if defined(WIN32)
    j9tty_printf(PORTLIB, "pthread test does not run on windows - skipping\n");
    return JNI_TRUE;
#else
    pthread_t thread;
    void *status;
    int rc;
    JavaVM *vm = NULL;

    (*env)->GetJavaVM(env, &vm);
    rc = pthread_key_create(&key, detachThread);
    if (0 == rc) {
		rc = pthread_create(&thread, NULL, threadproc, (void *) vm);
		if (0 == rc) {
			j9thread_sleep(5000);
		} else {
			j9tty_printf(PORTLIB, "\n*** failed to create pthread rc = %d ***\n", rc);
		}
		pthread_key_delete(key);
    } else {
		j9tty_printf(PORTLIB, "\n*** failed to create pthread key rc = %d ***\n", rc);
    }
    return worked;
#endif
}

