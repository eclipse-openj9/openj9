
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "jni.h"
#include "jnichk_internal.h"
#include "jnichknls.h"
#include "jnicheck.h"

static jint JNICALL checkGetVersion(JNIEnv *env);
static jclass JNICALL checkDefineClass(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len);
static jclass JNICALL checkFindClass(JNIEnv *env, const char* name);
static jmethodID JNICALL checkFromReflectedMethod(JNIEnv *env, jobject method);
static jfieldID JNICALL checkFromReflectedField(JNIEnv *env, jobject field);
static jobject JNICALL checkToReflectedMethod(JNIEnv *env, jclass clazz, jmethodID methodID, jboolean isStatic);
static jclass JNICALL checkGetSuperclass(JNIEnv *env, jclass sub);
static jboolean JNICALL checkIsAssignableFrom(JNIEnv *env, jclass sub, jclass sup);
static jobject JNICALL checkToReflectedField(JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean isStatic);
static jint JNICALL checkThrow(JNIEnv *env, jthrowable obj);
static jint JNICALL checkThrowNew(JNIEnv *env, jclass clazz, const char *msg);
static jthrowable JNICALL checkExceptionOccurred(JNIEnv *env);
static void JNICALL checkExceptionDescribe(JNIEnv *env);
static void JNICALL checkExceptionClear(JNIEnv *env);
static void JNICALL checkFatalError(JNIEnv *env, const char *msg);
static jint JNICALL checkPushLocalFrame(JNIEnv *env, jint capacity);
static jobject JNICALL checkPopLocalFrame(JNIEnv *env, jobject result);
static jobject JNICALL checkNewGlobalRef(JNIEnv *env, jobject lobj);
static void JNICALL checkDeleteGlobalRef(JNIEnv *env, jobject gref);
static void JNICALL checkDeleteLocalRef(JNIEnv *env, jobject obj);
static jboolean JNICALL checkIsSameObject(JNIEnv *env, jobject obj1, jobject obj2);
static jobject JNICALL checkNewLocalRef(JNIEnv *env, jobject ref);
static jint JNICALL checkEnsureLocalCapacity(JNIEnv *env, jint capacity);
static jobject JNICALL checkAllocObject(JNIEnv *env, jclass clazz);
static jobject JNICALL checkNewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jobject JNICALL checkNewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jobject JNICALL checkNewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jclass JNICALL checkGetObjectClass(JNIEnv *env, jobject obj);
static jboolean JNICALL checkIsInstanceOf(JNIEnv *env, jobject obj, jclass clazz);
static jmethodID JNICALL checkGetMethodID(JNIEnv *env, jclass clazz, const char* name, const char* sig);
static jobject JNICALL checkCallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jobject JNICALL checkCallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jobject JNICALL checkCallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
static jboolean JNICALL checkCallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jboolean JNICALL checkCallBooleanMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jboolean JNICALL checkCallBooleanMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
static jbyte JNICALL checkCallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jbyte JNICALL checkCallByteMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jbyte JNICALL checkCallByteMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static jchar JNICALL checkCallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jchar JNICALL checkCallCharMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jchar JNICALL checkCallCharMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static jshort JNICALL checkCallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jshort JNICALL checkCallShortMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jshort JNICALL checkCallShortMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static jint JNICALL checkCallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jint JNICALL checkCallIntMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jint JNICALL checkCallIntMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static jlong JNICALL checkCallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jlong JNICALL checkCallLongMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jlong JNICALL checkCallLongMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static jfloat JNICALL checkCallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jfloat JNICALL checkCallFloatMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jfloat JNICALL checkCallFloatMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static jdouble JNICALL checkCallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static jdouble JNICALL checkCallDoubleMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static jdouble JNICALL checkCallDoubleMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
static void JNICALL checkCallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
static void JNICALL checkCallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
static void JNICALL checkCallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
static jobject JNICALL checkCallNonvirtualObjectMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jobject JNICALL checkCallNonvirtualObjectMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jobject JNICALL checkCallNonvirtualObjectMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
static jboolean JNICALL checkCallNonvirtualBooleanMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jboolean JNICALL checkCallNonvirtualBooleanMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jboolean JNICALL checkCallNonvirtualBooleanMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
static jbyte JNICALL checkCallNonvirtualByteMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jbyte JNICALL checkCallNonvirtualByteMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jbyte JNICALL checkCallNonvirtualByteMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static jchar JNICALL checkCallNonvirtualCharMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jchar JNICALL checkCallNonvirtualCharMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jchar JNICALL checkCallNonvirtualCharMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static jshort JNICALL checkCallNonvirtualShortMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jshort JNICALL checkCallNonvirtualShortMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jshort JNICALL checkCallNonvirtualShortMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static jint JNICALL checkCallNonvirtualIntMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jint JNICALL checkCallNonvirtualIntMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jint JNICALL checkCallNonvirtualIntMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static jlong JNICALL checkCallNonvirtualLongMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jlong JNICALL checkCallNonvirtualLongMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jlong JNICALL checkCallNonvirtualLongMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static jfloat JNICALL checkCallNonvirtualFloatMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jfloat JNICALL checkCallNonvirtualFloatMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jfloat JNICALL checkCallNonvirtualFloatMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static jdouble JNICALL checkCallNonvirtualDoubleMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static jdouble JNICALL checkCallNonvirtualDoubleMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static jdouble JNICALL checkCallNonvirtualDoubleMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
static void JNICALL checkCallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
static void JNICALL checkCallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
static void JNICALL checkCallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
static jfieldID JNICALL checkGetFieldID(JNIEnv *env, jclass clazz, const char* name, const char* sig);
static jobject JNICALL checkGetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jboolean JNICALL checkGetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jbyte JNICALL checkGetByteField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jchar JNICALL checkGetCharField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jshort JNICALL checkGetShortField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jint JNICALL checkGetIntField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jlong JNICALL checkGetLongField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jfloat JNICALL checkGetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID);
static jdouble JNICALL checkGetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID);
static void JNICALL checkSetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject val);
static void JNICALL checkSetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean val);
static void JNICALL checkSetByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte val);
static void JNICALL checkSetCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar val);
static void JNICALL checkSetShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort val);
static void JNICALL checkSetIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint val);
static void JNICALL checkSetLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong val);
static void JNICALL checkSetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat val);
static void JNICALL checkSetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble val);
static jmethodID JNICALL checkGetStaticMethodID(JNIEnv *env, jclass clazz, const char* name, const char* sig);
static jobject JNICALL checkCallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jobject JNICALL checkCallStaticObjectMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jobject JNICALL checkCallStaticObjectMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jboolean JNICALL checkCallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jboolean JNICALL checkCallStaticBooleanMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jboolean JNICALL checkCallStaticBooleanMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jbyte JNICALL checkCallStaticByteMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jbyte JNICALL checkCallStaticByteMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jbyte JNICALL checkCallStaticByteMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jchar JNICALL checkCallStaticCharMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jchar JNICALL checkCallStaticCharMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jchar JNICALL checkCallStaticCharMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jshort JNICALL checkCallStaticShortMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jshort JNICALL checkCallStaticShortMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jshort JNICALL checkCallStaticShortMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jint JNICALL checkCallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jint JNICALL checkCallStaticIntMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jint JNICALL checkCallStaticIntMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jlong JNICALL checkCallStaticLongMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jlong JNICALL checkCallStaticLongMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jlong JNICALL checkCallStaticLongMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jfloat JNICALL checkCallStaticFloatMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jfloat JNICALL checkCallStaticFloatMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jfloat JNICALL checkCallStaticFloatMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static jdouble JNICALL checkCallStaticDoubleMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static jdouble JNICALL checkCallStaticDoubleMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static jdouble JNICALL checkCallStaticDoubleMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
static void JNICALL checkCallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
static void JNICALL checkCallStaticVoidMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
static void JNICALL checkCallStaticVoidMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args);
static jfieldID JNICALL checkGetStaticFieldID(JNIEnv *env, jclass clazz, const char* name, const char* sig);
static jobject JNICALL checkGetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jboolean JNICALL checkGetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jbyte JNICALL checkGetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jchar JNICALL checkGetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jshort JNICALL checkGetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jint JNICALL checkGetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jlong JNICALL checkGetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jfloat JNICALL checkGetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static jdouble JNICALL checkGetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID);
static void JNICALL checkSetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value);
static void JNICALL checkSetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean value);
static void JNICALL checkSetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID, jbyte value);
static void JNICALL checkSetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID, jchar value);
static void JNICALL checkSetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID, jshort value);
static void JNICALL checkSetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID, jint value);
static void JNICALL checkSetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID, jlong value);
static void JNICALL checkSetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID, jfloat value);
static void JNICALL checkSetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID, jdouble value);
static jstring JNICALL checkNewString(JNIEnv *env, const jchar *unicode, jsize len);
static jsize JNICALL checkGetStringLength(JNIEnv *env, jstring str);
static const jchar * JNICALL checkGetStringChars(JNIEnv *env, jstring str, jboolean *isCopy);
static void JNICALL checkReleaseStringChars(JNIEnv *env, jstring str, const jchar *chars);
static jstring JNICALL checkNewStringUTF(JNIEnv *env, const char *utf);
static jsize JNICALL checkGetStringUTFLength(JNIEnv *env, jstring str);
static const char* JNICALL checkGetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy);
static void JNICALL checkReleaseStringUTFChars(JNIEnv *env, jstring str, const char* chars);
static jsize JNICALL checkGetArrayLength(JNIEnv *env, jarray array);
static jobjectArray JNICALL checkNewObjectArray(JNIEnv *env, jsize len, jclass clazz, jobject init);
static jobject JNICALL checkGetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index);
static void JNICALL checkSetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject val);
static jbooleanArray JNICALL checkNewBooleanArray(JNIEnv *env, jsize len);
static jbyteArray JNICALL checkNewByteArray(JNIEnv *env, jsize len);
static jcharArray JNICALL checkNewCharArray(JNIEnv *env, jsize len);
static jshortArray JNICALL checkNewShortArray(JNIEnv *env, jsize len);
static jintArray JNICALL checkNewIntArray(JNIEnv *env, jsize len);
static jlongArray JNICALL checkNewLongArray(JNIEnv *env, jsize len);
static jfloatArray JNICALL checkNewFloatArray(JNIEnv *env, jsize len);
static jdoubleArray JNICALL checkNewDoubleArray(JNIEnv *env, jsize len);
static jboolean * JNICALL checkGetBooleanArrayElements(JNIEnv *env, jbooleanArray array, jboolean *isCopy);
static jbyte * JNICALL checkGetByteArrayElements(JNIEnv *env, jbyteArray array, jboolean *isCopy);
static jchar * JNICALL checkGetCharArrayElements(JNIEnv *env, jcharArray array, jboolean *isCopy);
static jshort * JNICALL checkGetShortArrayElements(JNIEnv *env, jshortArray array, jboolean *isCopy);
static jint * JNICALL checkGetIntArrayElements(JNIEnv *env, jintArray array, jboolean *isCopy);
static jlong * JNICALL checkGetLongArrayElements(JNIEnv *env, jlongArray array, jboolean *isCopy);
static jfloat * JNICALL checkGetFloatArrayElements(JNIEnv *env, jfloatArray array, jboolean *isCopy);
static jdouble * JNICALL checkGetDoubleArrayElements(JNIEnv *env, jdoubleArray array, jboolean *isCopy);
static void JNICALL checkReleaseBooleanArrayElements(JNIEnv *env, jbooleanArray array, jboolean *elems, jint mode);
static void JNICALL checkReleaseByteArrayElements(JNIEnv *env, jbyteArray array, jbyte *elems, jint mode);
static void JNICALL checkReleaseCharArrayElements(JNIEnv *env, jcharArray array, jchar *elems, jint mode);
static void JNICALL checkReleaseShortArrayElements(JNIEnv *env, jshortArray array, jshort *elems, jint mode);
static void JNICALL checkReleaseIntArrayElements(JNIEnv *env, jintArray array, jint *elems, jint mode);
static void JNICALL checkReleaseLongArrayElements(JNIEnv *env, jlongArray array, jlong *elems, jint mode);
static void JNICALL checkReleaseFloatArrayElements(JNIEnv *env, jfloatArray array, jfloat *elems, jint mode);
static void JNICALL checkReleaseDoubleArrayElements(JNIEnv *env, jdoubleArray array, jdouble *elems, jint mode);
static void JNICALL checkGetBooleanArrayRegion(JNIEnv *env, jbooleanArray array, jsize start, jsize len, jboolean *buf);
static void JNICALL checkGetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf);
static void JNICALL checkGetCharArrayRegion(JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf);
static void JNICALL checkGetShortArrayRegion(JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf);
static void JNICALL checkGetIntArrayRegion(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf);
static void JNICALL checkGetLongArrayRegion(JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf);
static void JNICALL checkGetFloatArrayRegion(JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf);
static void JNICALL checkGetDoubleArrayRegion(JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf);
static void JNICALL checkSetBooleanArrayRegion(JNIEnv *env, jbooleanArray array, jsize start, jsize len, jboolean *buf);
static void JNICALL checkSetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf);
static void JNICALL checkSetCharArrayRegion(JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf);
static void JNICALL checkSetShortArrayRegion(JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf);
static void JNICALL checkSetIntArrayRegion(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf);
static void JNICALL checkSetLongArrayRegion(JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf);
static void JNICALL checkSetFloatArrayRegion(JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf);
static void JNICALL checkSetDoubleArrayRegion(JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf);
static jint JNICALL checkRegisterNatives(JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods);
static jint JNICALL checkUnregisterNatives(JNIEnv *env, jclass clazz);
static jint JNICALL checkMonitorEnter(JNIEnv *env, jobject obj);
static jint JNICALL checkMonitorExit(JNIEnv *env, jobject obj);
static jint JNICALL checkGetJavaVM(JNIEnv *env, JavaVM **vm);
static void JNICALL checkGetStringRegion(JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf);
static void JNICALL checkGetStringUTFRegion(JNIEnv *env, jstring str, jsize start, jsize len, char *buf);
static void * JNICALL checkGetPrimitiveArrayCritical(JNIEnv *env, jarray array, jboolean *isCopy);
static void JNICALL checkReleasePrimitiveArrayCritical(JNIEnv *env, jarray array, void *carray, jint mode);
static const jchar * JNICALL checkGetStringCritical(JNIEnv *env, jstring string, jboolean *isCopy);
static void JNICALL checkReleaseStringCritical(JNIEnv *env, jstring string, const jchar *carray);
static jweak JNICALL checkNewWeakGlobalRef(JNIEnv *env, jobject obj);
static void JNICALL checkDeleteWeakGlobalRef(JNIEnv *env, jweak obj);
static jboolean JNICALL checkExceptionCheck(JNIEnv *env);
static jobject JNICALL checkNewDirectByteBuffer(JNIEnv *env, void *address, jlong capacity);
static void* JNICALL checkGetDirectBufferAddress(JNIEnv *env, jobject buf);
static jlong JNICALL checkGetDirectBufferCapacity(JNIEnv *env, jobject buf);
static jobjectRefType JNICALL checkGetObjectRefType(JNIEnv *env, jobject obj);

static jint JNICALL
checkGetVersion(JNIEnv *env)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { 0 };
	static const char function[] = "GetVersion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env);
	actualResult = globalJavaVM->EsJNIFunctions->GetVersion(env);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jclass JNICALL
checkDefineClass(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len)
{
	jclass actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_CLASSNAME, JNIC_CLASSLOADER, JNIC_POINTER, JNIC_JSIZE, 0 };
	static const char function[] = "DefineClass";
	U_32 nameCRC, bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, name, loader, buf, len);
	nameCRC = computeStringCRC(name);
	bufCRC = computeDataCRC(buf, len);
	actualResult = globalJavaVM->EsJNIFunctions->DefineClass(env, name, loader, buf, len);
	checkStringCRC(function, 2, name, nameCRC);
	checkDataCRC(function, 4, buf, len, bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jclass JNICALL
checkFindClass(JNIEnv *env, const char* name)
{ 	
	jclass actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_CLASSNAME, 0 };
	static const char function[] = "FindClass";
	U_32 nameCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, name);
	nameCRC = computeStringCRC(name);
	jniVerboseFindClass(function, env, name);
	actualResult = globalJavaVM->EsJNIFunctions->FindClass(env, name);
	checkStringCRC(function, 2, name, nameCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jmethodID JNICALL
checkFromReflectedMethod(JNIEnv *env, jobject method)
{ 	
	jmethodID actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_REFLECTMETHOD, 0 };
	static const char function[] = "FromReflectedMethod";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, method);
	actualResult = globalJavaVM->EsJNIFunctions->FromReflectedMethod(env, method);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jfieldID JNICALL
checkFromReflectedField(JNIEnv *env, jobject field)
{ 	
	jfieldID actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_REFLECTFIELD, 0 };
	static const char function[] = "FromReflectedField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, field);
	actualResult = globalJavaVM->EsJNIFunctions->FromReflectedField(env, field);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkToReflectedMethod(JNIEnv *env, jclass clazz, jmethodID methodID, jboolean isStatic)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JBOOLEAN, 0 };
	static const char function[] = "ToReflectedMethod";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, isStatic);
	actualResult = globalJavaVM->EsJNIFunctions->ToReflectedMethod(env, clazz, methodID, isStatic);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jclass JNICALL
checkGetSuperclass(JNIEnv *env, jclass sub)
{ 	
	jclass actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, 0 };
	static const char function[] = "GetSuperclass";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, sub);
	actualResult = globalJavaVM->EsJNIFunctions->GetSuperclass(env, sub);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jboolean JNICALL
checkIsAssignableFrom(JNIEnv *env, jclass sub, jclass sup)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JCLASS, 0 };
	static const char function[] = "IsAssignableFrom";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, sub, sup);
	actualResult = globalJavaVM->EsJNIFunctions->IsAssignableFrom(env, sub, sup);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkToReflectedField(JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean isStatic)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDID, JNIC_JBOOLEAN, 0 };
	static const char function[] = "ToReflectedField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, isStatic);
	actualResult = globalJavaVM->EsJNIFunctions->ToReflectedField(env, clazz, fieldID, isStatic);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkThrow(JNIEnv *env, jthrowable obj)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JTHROWABLE, 0 };
	static const char function[] = "Throw";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	actualResult = globalJavaVM->EsJNIFunctions->Throw(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkThrowNew(JNIEnv *env, jclass clazz, const char *msg)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_CLASSTHROWABLE, JNIC_STRING, 0 };
	static const char function[] = "ThrowNew";
	U_32 msgCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, msg);
	msgCRC = computeStringCRC(msg);
	actualResult = globalJavaVM->EsJNIFunctions->ThrowNew(env, clazz, msg);
	checkStringCRC(function, 3, msg, msgCRC);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jthrowable JNICALL
checkExceptionOccurred(JNIEnv *env)
{ 	
	jthrowable actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { 0 };
	static const char function[] = "ExceptionOccurred";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env);
	actualResult = globalJavaVM->EsJNIFunctions->ExceptionOccurred(env);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	/* the caller can now tell that an exception is pending */
	jniCheckSetPotentialPendingException(NULL);

	return actualResult;
}

static void JNICALL
checkExceptionDescribe(JNIEnv *env)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { 0 };
	static const char function[] = "ExceptionDescribe";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env);
	globalJavaVM->EsJNIFunctions->ExceptionDescribe(env);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	/* the caller can now tell that an exception is pending */
	jniCheckSetPotentialPendingException(NULL);
}

static void JNICALL
checkExceptionClear(JNIEnv *env)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { 0 };
	static const char function[] = "ExceptionClear";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env);
	globalJavaVM->EsJNIFunctions->ExceptionClear(env);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	/* the caller can now tell that an exception is pending */
	jniCheckSetPotentialPendingException(NULL);
}

static void JNICALL
checkFatalError(JNIEnv *env, const char *msg)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_STRING, 0 };
	static const char function[] = "FatalError";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, msg);
	globalJavaVM->EsJNIFunctions->FatalError(env, msg);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jint JNICALL
checkPushLocalFrame(JNIEnv *env, jint capacity)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "PushLocalFrame";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, capacity);
	jniCheckRange(function, "jint", capacity, 2, 1, INT32_MAX);
	actualResult = globalJavaVM->EsJNIFunctions->PushLocalFrame(env, capacity);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkPopLocalFrame(JNIEnv *env, jobject result)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, 0 };
	static const char function[] = "PopLocalFrame";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, result);
	jniCheckPopLocalFrame(env, function);
	actualResult = globalJavaVM->EsJNIFunctions->PopLocalFrame(env, result);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkNewGlobalRef(JNIEnv *env, jobject lobj)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, 0 };
	static const char function[] = "NewGlobalRef";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, lobj);
	actualResult = globalJavaVM->EsJNIFunctions->NewGlobalRef(env, lobj);
	if (NULL != actualResult) {
		/* search for the reference in the hashtable */
		JNICHK_GREF_HASHENTRY searchEntry;
		JNICHK_GREF_HASHENTRY* foundEntry;
			
		searchEntry.reference = (UDATA)actualResult;
		searchEntry.alive = TRUE;
			
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_enter(globalJavaVM->jniFrameMutex);
#endif
		foundEntry = hashTableFind(globalJavaVM->checkJNIData.jniGlobalRefHashTab, &searchEntry);
		if (NULL == foundEntry) {
			/* write a new entry into the hashtable */
			hashTableAdd(globalJavaVM->checkJNIData.jniGlobalRefHashTab, &searchEntry);
		} else {
			/* found an entry already in the table, so make it alive */
			foundEntry->alive = TRUE;
		}
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalJavaVM->jniFrameMutex);
#endif
	}
	
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkDeleteGlobalRef(JNIEnv *env, jobject gref)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_GLOBALREF, 0 };
	static const char function[] = "DeleteGlobalRef";
	JNICHK_GREF_HASHENTRY entry;
	JNICHK_GREF_HASHENTRY* actualResult;

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, gref);
	globalJavaVM->EsJNIFunctions->DeleteGlobalRef(env, gref);

	/* search for the reference in the hashtable */
	entry.reference = (UDATA)gref;
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_enter(globalJavaVM->jniFrameMutex);
#endif
	actualResult = hashTableFind(globalJavaVM->checkJNIData.jniGlobalRefHashTab, &entry);
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalJavaVM->jniFrameMutex);
#endif
	if (NULL != actualResult) {
		/* mark the entry as dead */
		actualResult->alive = FALSE;
	}
		
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkDeleteLocalRef(JNIEnv *env, jobject obj)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_LOCALREF, 0 };
	static const char function[] = "DeleteLocalRef";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	globalJavaVM->EsJNIFunctions->DeleteLocalRef(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jboolean JNICALL
checkIsSameObject(JNIEnv *env, jobject obj1, jobject obj2)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, JNIC_JOBJECT, 0 };
	static const char function[] = "IsSameObject";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj1, obj2);
	actualResult = globalJavaVM->EsJNIFunctions->IsSameObject(env, obj1, obj2);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkNewLocalRef(JNIEnv *env, jobject ref)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, 0 };
	static const char function[] = "NewLocalRef";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, ref);
	actualResult = globalJavaVM->EsJNIFunctions->NewLocalRef(env, ref);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkEnsureLocalCapacity(JNIEnv *env, jint capacity)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "EnsureLocalCapacity";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, capacity);
	actualResult = globalJavaVM->EsJNIFunctions->EnsureLocalCapacity(env, capacity);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkAllocObject(JNIEnv *env, jclass clazz)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, 0 };
	static const char function[] = "AllocObject";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz);
	actualResult = globalJavaVM->EsJNIFunctions->AllocObject(env, clazz);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL checkNewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jobject result;
	va_list va;

	va_start(va, methodID);
	result = checkNewObjectV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jobject JNICALL
checkNewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "NewObject/NewObjectV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_CONSTRUCTOR, 'V', methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->NewObjectV(env, clazz, methodID, args);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, VA_PTR(args), actualResult);

	return actualResult;
}

static jobject JNICALL
checkNewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "NewObjectA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_CONSTRUCTOR, 'V', methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->NewObjectA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, NULL, actualResult);

	return actualResult;
}

static jclass JNICALL
checkGetObjectClass(JNIEnv *env, jobject obj)
{ 	
	jclass actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, 0 };
	static const char function[] = "GetObjectClass";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	actualResult = globalJavaVM->EsJNIFunctions->GetObjectClass(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jboolean JNICALL
checkIsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, JNIC_JCLASS, 0 };
	static const char function[] = "IsInstanceOf";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz);
	actualResult = globalJavaVM->EsJNIFunctions->IsInstanceOf(env, obj, clazz);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jmethodID JNICALL
checkGetMethodID(JNIEnv *env, jclass clazz, const char* name, const char* sig)
{ 	
	jmethodID actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_MEMBERNAME, JNIC_METHODSIGNATURE, 0 };
	static const char function[] = "GetMethodID";
	U_32 nameCRC, sigCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, name, sig);
	nameCRC = computeStringCRC(name);
	sigCRC = computeStringCRC(sig);
	jniVerboseGetID(function, env, clazz, name, sig);
	actualResult = globalJavaVM->EsJNIFunctions->GetMethodID(env, clazz, name, sig);
	checkStringCRC(function, 3, name, nameCRC);
	checkStringCRC(function, 4, sig, sigCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL checkCallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jobject result;
	va_list va;

	va_start(va, methodID);
	result = checkCallObjectMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jobject JNICALL
checkCallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallObjectMethod/CallObjectMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JOBJECT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallObjectMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jobject JNICALL
checkCallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallObjectMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JOBJECT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallObjectMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jboolean JNICALL checkCallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jboolean result;
	va_list va;

	va_start(va, methodID);
	result = checkCallBooleanMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jboolean JNICALL
checkCallBooleanMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallBooleanMethod/CallBooleanMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JBOOLEAN, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallBooleanMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jboolean(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jboolean JNICALL
checkCallBooleanMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallBooleanMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JBOOLEAN, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallBooleanMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jboolean(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jbyte JNICALL checkCallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jbyte result;
	va_list va;

	va_start(va, methodID);
	result = checkCallByteMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jbyte JNICALL
checkCallByteMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallByteMethod/CallByteMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JBYTE, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallByteMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jbyte(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jbyte JNICALL
checkCallByteMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallByteMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JBYTE, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallByteMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jbyte(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jchar JNICALL checkCallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jchar result;
	va_list va;

	va_start(va, methodID);
	result = checkCallCharMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jchar JNICALL
checkCallCharMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallCharMethod/CallCharMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JCHAR, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallCharMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jchar(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jchar JNICALL
checkCallCharMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallCharMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JCHAR, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallCharMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jchar(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jshort JNICALL checkCallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jshort result;
	va_list va;

	va_start(va, methodID);
	result = checkCallShortMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jshort JNICALL
checkCallShortMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallShortMethod/CallShortMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JSHORT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallShortMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jshort(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jshort JNICALL
checkCallShortMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallShortMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JSHORT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallShortMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jshort(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jint JNICALL checkCallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jint result;
	va_list va;

	va_start(va, methodID);
	result = checkCallIntMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jint JNICALL
checkCallIntMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallIntMethod/CallIntMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JINT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallIntMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jint(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jint JNICALL
checkCallIntMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallIntMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JINT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallIntMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jint(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jlong JNICALL checkCallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jlong result;
	va_list va;

	va_start(va, methodID);
	result = checkCallLongMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jlong JNICALL
checkCallLongMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallLongMethod/CallLongMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JLONG, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallLongMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jlong(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jlong JNICALL
checkCallLongMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallLongMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JLONG, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallLongMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jlong(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jfloat JNICALL checkCallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jfloat result;
	va_list va;

	va_start(va, methodID);
	result = checkCallFloatMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jfloat JNICALL
checkCallFloatMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallFloatMethod/CallFloatMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JFLOAT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallFloatMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jfloat(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jfloat JNICALL
checkCallFloatMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallFloatMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JFLOAT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallFloatMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jfloat(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jdouble JNICALL checkCallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	jdouble result;
	va_list va;

	va_start(va, methodID);
	result = checkCallDoubleMethodV(env, obj, methodID, va);
	va_end(va);

	return result;
}

static jdouble JNICALL
checkCallDoubleMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallDoubleMethod/CallDoubleMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JDOUBLE, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallDoubleMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jdouble(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jdouble JNICALL
checkCallDoubleMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallDoubleMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JDOUBLE, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallDoubleMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jdouble(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static void JNICALL checkCallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
	va_list va;

	va_start(va, methodID);
	checkCallVoidMethodV(env, obj, methodID, va);
	va_end(va);
}

static void JNICALL
checkCallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallVoidMethod/CallVoidMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_VOID, methodID, args);
	globalJavaVM->EsJNIFunctions->CallVoidMethodV(env, obj, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_void(env, VA_PTR(args));
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);
}

static void JNICALL
checkCallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallVoidMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_VOID, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	globalJavaVM->EsJNIFunctions->CallVoidMethodA(env, obj, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_void(env, NULL);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);
}

static jobject JNICALL checkCallNonvirtualObjectMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jobject result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualObjectMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jobject JNICALL
checkCallNonvirtualObjectMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualObjectMethod/CallNonvirtualObjectMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JOBJECT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualObjectMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jobject JNICALL
checkCallNonvirtualObjectMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualObjectMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JOBJECT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualObjectMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jboolean JNICALL checkCallNonvirtualBooleanMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jboolean result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualBooleanMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jboolean JNICALL
checkCallNonvirtualBooleanMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualBooleanMethod/CallNonvirtualBooleanMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JBOOLEAN, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualBooleanMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jboolean(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jboolean JNICALL
checkCallNonvirtualBooleanMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualBooleanMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JBOOLEAN, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualBooleanMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jboolean(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jbyte JNICALL checkCallNonvirtualByteMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jbyte result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualByteMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jbyte JNICALL
checkCallNonvirtualByteMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualByteMethod/CallNonvirtualByteMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JBYTE, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualByteMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jbyte(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jbyte JNICALL
checkCallNonvirtualByteMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualByteMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JBYTE, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualByteMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jbyte(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jchar JNICALL checkCallNonvirtualCharMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jchar result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualCharMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jchar JNICALL
checkCallNonvirtualCharMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualCharMethod/CallNonvirtualCharMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JCHAR, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualCharMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jchar(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jchar JNICALL
checkCallNonvirtualCharMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualCharMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JCHAR, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualCharMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jchar(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jshort JNICALL checkCallNonvirtualShortMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jshort result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualShortMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jshort JNICALL
checkCallNonvirtualShortMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualShortMethod/CallNonvirtualShortMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JSHORT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualShortMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jshort(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jshort JNICALL
checkCallNonvirtualShortMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualShortMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JSHORT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualShortMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jshort(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jint JNICALL checkCallNonvirtualIntMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jint result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualIntMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jint JNICALL
checkCallNonvirtualIntMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualIntMethod/CallNonvirtualIntMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JINT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualIntMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jint(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jint JNICALL
checkCallNonvirtualIntMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualIntMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JINT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualIntMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jint(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jlong JNICALL checkCallNonvirtualLongMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jlong result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualLongMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jlong JNICALL
checkCallNonvirtualLongMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualLongMethod/CallNonvirtualLongMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JLONG, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualLongMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jlong(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jlong JNICALL
checkCallNonvirtualLongMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualLongMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JLONG, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualLongMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jlong(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jfloat JNICALL checkCallNonvirtualFloatMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jfloat result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualFloatMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jfloat JNICALL
checkCallNonvirtualFloatMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualFloatMethod/CallNonvirtualFloatMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JFLOAT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualFloatMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jfloat(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jfloat JNICALL
checkCallNonvirtualFloatMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualFloatMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JFLOAT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualFloatMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jfloat(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jdouble JNICALL checkCallNonvirtualDoubleMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	jdouble result;
	va_list va;

	va_start(va, methodID);
	result = checkCallNonvirtualDoubleMethodV(env, obj, clazz, methodID, va);
	va_end(va);

	return result;
}

static jdouble JNICALL
checkCallNonvirtualDoubleMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualDoubleMethod/CallNonvirtualDoubleMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_JDOUBLE, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualDoubleMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jdouble(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jdouble JNICALL
checkCallNonvirtualDoubleMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualDoubleMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_JDOUBLE, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallNonvirtualDoubleMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jdouble(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static void JNICALL checkCallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...) {
	va_list va;

	va_start(va, methodID);
	checkCallNonvirtualVoidMethodV(env, obj, clazz, methodID, va);
	va_end(va);
}

static void JNICALL
checkCallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallNonvirtualVoidMethod/CallNonvirtualVoidMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, obj, METHOD_INSTANCE, JNIC_VOID, methodID, args);
	globalJavaVM->EsJNIFunctions->CallNonvirtualVoidMethodV(env, obj, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_void(env, VA_PTR(args));
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);
}

static void JNICALL
checkCallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallNonvirtualVoidMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, clazz, methodID, args);
	jniCheckCallA(function, env, obj, METHOD_INSTANCE, JNIC_VOID, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	globalJavaVM->EsJNIFunctions->CallNonvirtualVoidMethodA(env, obj, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_void(env, NULL);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);
}

static jfieldID JNICALL
checkGetFieldID(JNIEnv *env, jclass clazz, const char* name, const char* sig)
{ 	
	jfieldID actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_MEMBERNAME, JNIC_FIELDSIGNATURE, 0 };
	static const char function[] = "GetFieldID";
	U_32 nameCRC, sigCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, name, sig);
	nameCRC = computeStringCRC(name);
	sigCRC = computeStringCRC(sig);
	jniVerboseGetID(function, env, clazz, name, sig);
	actualResult = globalJavaVM->EsJNIFunctions->GetFieldID(env, clazz, name, sig);
	checkStringCRC(function, 3, name, nameCRC);
	checkStringCRC(function, 4, sig, sigCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkGetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetObjectField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetObjectField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jboolean JNICALL
checkGetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetBooleanField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetBooleanField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jbyte JNICALL
checkGetByteField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetByteField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetByteField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jchar JNICALL
checkGetCharField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetCharField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetCharField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jshort JNICALL
checkGetShortField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetShortField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetShortField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkGetIntField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetIntField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetIntField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jlong JNICALL
checkGetLongField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetLongField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetLongField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jfloat JNICALL
checkGetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetFloatField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetFloatField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jdouble JNICALL
checkGetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, 0 };
	static const char function[] = "GetDoubleField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetDoubleField(env, obj, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkSetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JOBJECT, 0 };
	static const char function[] = "SetObjectField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetObjectField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JBOOLEAN, 0 };
	static const char function[] = "SetBooleanField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetBooleanField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JBYTE, 0 };
	static const char function[] = "SetByteField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetByteField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JCHAR, 0 };
	static const char function[] = "SetCharField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetCharField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JSHORT, 0 };
	static const char function[] = "SetShortField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetShortField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JINT, 0 };
	static const char function[] = "SetIntField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetIntField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JLONG, 0 };
	static const char function[] = "SetLongField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetLongField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JFLOAT, 0 };
	static const char function[] = "SetFloatField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetFloatField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, JNIC_JFIELDINSTANCEID, JNIC_JDOUBLE, 0 };
	static const char function[] = "SetDoubleField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj, fieldID, val);
	globalJavaVM->EsJNIFunctions->SetDoubleField(env, obj, fieldID, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jmethodID JNICALL
checkGetStaticMethodID(JNIEnv *env, jclass clazz, const char* name, const char* sig)
{ 	
	jmethodID actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_MEMBERNAME, JNIC_METHODSIGNATURE, 0 };
	static const char function[] = "GetStaticMethodID";
	U_32 nameCRC, sigCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, name, sig);
	nameCRC = computeStringCRC(name);
	sigCRC = computeStringCRC(sig);
	jniVerboseGetID(function, env, clazz, name, sig);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticMethodID(env, clazz, name, sig);
	checkStringCRC(function, 3, name, nameCRC);
	checkStringCRC(function, 4, sig, sigCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL checkCallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jobject result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticObjectMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jobject JNICALL
checkCallStaticObjectMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticObjectMethod/CallStaticObjectMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JOBJECT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticObjectMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jobject JNICALL
checkCallStaticObjectMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticObjectMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JOBJECT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticObjectMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jobject(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jboolean JNICALL checkCallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jboolean result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticBooleanMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jboolean JNICALL
checkCallStaticBooleanMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticBooleanMethod/CallStaticBooleanMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JBOOLEAN, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticBooleanMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jboolean(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jboolean JNICALL
checkCallStaticBooleanMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticBooleanMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JBOOLEAN, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticBooleanMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jboolean(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jbyte JNICALL checkCallStaticByteMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jbyte result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticByteMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jbyte JNICALL
checkCallStaticByteMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticByteMethod/CallStaticByteMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JBYTE, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticByteMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jbyte(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jbyte JNICALL
checkCallStaticByteMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticByteMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JBYTE, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticByteMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jbyte(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jchar JNICALL checkCallStaticCharMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jchar result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticCharMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jchar JNICALL
checkCallStaticCharMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticCharMethod/CallStaticCharMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JCHAR, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticCharMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jchar(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jchar JNICALL
checkCallStaticCharMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticCharMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JCHAR, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticCharMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jchar(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jshort JNICALL checkCallStaticShortMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jshort result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticShortMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jshort JNICALL
checkCallStaticShortMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticShortMethod/CallStaticShortMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JSHORT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticShortMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jshort(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jshort JNICALL
checkCallStaticShortMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticShortMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JSHORT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticShortMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jshort(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jint JNICALL checkCallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jint result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticIntMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jint JNICALL
checkCallStaticIntMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticIntMethod/CallStaticIntMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JINT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticIntMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jint(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jint JNICALL
checkCallStaticIntMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticIntMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JINT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticIntMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jint(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jlong JNICALL checkCallStaticLongMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jlong result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticLongMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jlong JNICALL
checkCallStaticLongMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticLongMethod/CallStaticLongMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JLONG, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticLongMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jlong(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jlong JNICALL
checkCallStaticLongMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticLongMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JLONG, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticLongMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jlong(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jfloat JNICALL checkCallStaticFloatMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jfloat result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticFloatMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jfloat JNICALL
checkCallStaticFloatMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticFloatMethod/CallStaticFloatMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JFLOAT, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticFloatMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jfloat(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jfloat JNICALL
checkCallStaticFloatMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticFloatMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JFLOAT, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticFloatMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jfloat(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jdouble JNICALL checkCallStaticDoubleMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	jdouble result;
	va_list va;

	va_start(va, methodID);
	result = checkCallStaticDoubleMethodV(env, clazz, methodID, va);
	va_end(va);

	return result;
}

static jdouble JNICALL
checkCallStaticDoubleMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticDoubleMethod/CallStaticDoubleMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_JDOUBLE, methodID, args);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticDoubleMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jdouble(env, VA_PTR(args), actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static jdouble JNICALL
checkCallStaticDoubleMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticDoubleMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_JDOUBLE, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	actualResult = globalJavaVM->EsJNIFunctions->CallStaticDoubleMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_jdouble(env, NULL, actualResult);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);

	return actualResult;
}

static void JNICALL checkCallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {
	va_list va;

	va_start(va, methodID);
	checkCallStaticVoidMethodV(env, clazz, methodID, va);
	va_end(va);
}

static void JNICALL
checkCallStaticVoidMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_VALIST, 0 };
	static const char function[] = "CallStaticVoidMethod/CallStaticVoidMethodV";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, VA_PTR(args));
	jniCheckCallV(function, env, clazz, METHOD_STATIC, JNIC_VOID, methodID, args);
	globalJavaVM->EsJNIFunctions->CallStaticVoidMethodV(env, clazz, methodID, args);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_void(env, VA_PTR(args));
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);
}

static void JNICALL
checkCallStaticVoidMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JMETHODID, JNIC_JVALUE, 0 };
	static const char function[] = "CallStaticVoidMethodA";
	U_32 argsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methodID, args);
	jniCheckCallA(function, env, clazz, METHOD_STATIC, JNIC_VOID, methodID, args);
	argsCRC = computeArgsCRC(args, methodID);
	globalJavaVM->EsJNIFunctions->CallStaticVoidMethodA(env, clazz, methodID, args);
	checkArgsCRC(function, 4, args, methodID, argsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	jniCallInReturn_void(env, NULL);
	/* the caller must now check if an exception is pending */
	jniCheckSetPotentialPendingException(function);
}

static jfieldID JNICALL
checkGetStaticFieldID(JNIEnv *env, jclass clazz, const char* name, const char* sig)
{ 	
	jfieldID actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_MEMBERNAME, JNIC_FIELDSIGNATURE, 0 };
	static const char function[] = "GetStaticFieldID";
	U_32 nameCRC, sigCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, name, sig);
	nameCRC = computeStringCRC(name);
	sigCRC = computeStringCRC(sig);
	jniVerboseGetID(function, env, clazz, name, sig);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticFieldID(env, clazz, name, sig);
	checkStringCRC(function, 3, name, nameCRC);
	checkStringCRC(function, 4, sig, sigCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkGetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticObjectField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticObjectField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jboolean JNICALL
checkGetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticBooleanField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticBooleanField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jbyte JNICALL
checkGetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jbyte actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticByteField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticByteField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jchar JNICALL
checkGetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jchar actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticCharField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticCharField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jshort JNICALL
checkGetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jshort actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticShortField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticShortField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkGetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticIntField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticIntField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jlong JNICALL
checkGetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticLongField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticLongField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jfloat JNICALL
checkGetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jfloat actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticFloatField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticFloatField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jdouble JNICALL
checkGetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{ 	
	jdouble actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, 0 };
	static const char function[] = "GetStaticDoubleField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID);
	actualResult = globalJavaVM->EsJNIFunctions->GetStaticDoubleField(env, clazz, fieldID);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkSetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JOBJECT, 0 };
	static const char function[] = "SetStaticObjectField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticObjectField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JBOOLEAN, 0 };
	static const char function[] = "SetStaticBooleanField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticBooleanField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID, jbyte value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JBYTE, 0 };
	static const char function[] = "SetStaticByteField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticByteField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID, jchar value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JCHAR, 0 };
	static const char function[] = "SetStaticCharField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticCharField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID, jshort value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JSHORT, 0 };
	static const char function[] = "SetStaticShortField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticShortField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID, jint value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JINT, 0 };
	static const char function[] = "SetStaticIntField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticIntField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID, jlong value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JLONG, 0 };
	static const char function[] = "SetStaticLongField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticLongField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID, jfloat value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JFLOAT, 0 };
	static const char function[] = "SetStaticFloatField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticFloatField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID, jdouble value)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_JFIELDSTATICID, JNIC_JDOUBLE, 0 };
	static const char function[] = "SetStaticDoubleField";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, fieldID, value);
	globalJavaVM->EsJNIFunctions->SetStaticDoubleField(env, clazz, fieldID, value);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jstring JNICALL
checkNewString(JNIEnv *env, const jchar *unicode, jsize len)
{ 	
	jstring actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_POINTER, JNIC_JSIZE, 0 };
	static const char function[] = "NewString";
	U_32 unicodeCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, unicode, len);
	unicodeCRC = computeDataCRC(unicode, len * sizeof(unicode[0]));
	actualResult = globalJavaVM->EsJNIFunctions->NewString(env, unicode, len);
	checkDataCRC(function, 2, unicode, len * sizeof(unicode[0]), unicodeCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jsize JNICALL
checkGetStringLength(JNIEnv *env, jstring str)
{ 	
	jsize actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, 0 };
	static const char function[] = "GetStringLength";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, str);
	actualResult = globalJavaVM->EsJNIFunctions->GetStringLength(env, str);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static const jchar * JNICALL
checkGetStringChars(JNIEnv *env, jstring str, jboolean *isCopy)
{ 	
	const jchar * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_POINTER, 0 };
	static const char function[] = "GetStringChars";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, str, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetStringChars(env, str, isCopy);
	jniRecordMemoryAcquire(env, function, str, actualResult, 0);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkReleaseStringChars(JNIEnv *env, jstring str, const jchar *chars)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_POINTER, 0 };
	static const char function[] = "ReleaseStringChars";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, str, chars);
	jniRecordMemoryRelease(env, "GetStringChars", function, str, chars, 0, 0);
	globalJavaVM->EsJNIFunctions->ReleaseStringChars(env, str, chars);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jstring JNICALL
checkNewStringUTF(JNIEnv *env, const char *utf)
{ 	
	jstring actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_STRING, 0 };
	static const char function[] = "NewStringUTF";
	U_32 utfCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, utf);
	utfCRC = computeStringCRC(utf);
	actualResult = globalJavaVM->EsJNIFunctions->NewStringUTF(env, utf);
	checkStringCRC(function, 2, utf, utfCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jsize JNICALL
checkGetStringUTFLength(JNIEnv *env, jstring str)
{ 	
	jsize actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, 0 };
	static const char function[] = "GetStringUTFLength";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, str);
	actualResult = globalJavaVM->EsJNIFunctions->GetStringUTFLength(env, str);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static const char* JNICALL
checkGetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy)
{ 	
	const char* actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_POINTER, 0 };
	static const char function[] = "GetStringUTFChars";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, str, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetStringUTFChars(env, str, isCopy);
	jniRecordMemoryAcquire(env, function, str, actualResult, 0);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkReleaseStringUTFChars(JNIEnv *env, jstring str, const char* chars)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_STRING, 0 };
	static const char function[] = "ReleaseStringUTFChars";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, str, chars);
	jniRecordMemoryRelease(env, "GetStringUTFChars", function, str, chars, 0, 0);
	globalJavaVM->EsJNIFunctions->ReleaseStringUTFChars(env, str, chars);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jsize JNICALL
checkGetArrayLength(JNIEnv *env, jarray array)
{ 	
	jsize actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JARRAY, 0 };
	static const char function[] = "GetArrayLength";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array);
	actualResult = globalJavaVM->EsJNIFunctions->GetArrayLength(env, array);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobjectArray JNICALL
checkNewObjectArray(JNIEnv *env, jsize len, jclass clazz, jobject init)
{ 	
	jobjectArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, JNIC_JCLASS, JNIC_JOBJECT, 0 };
	static const char function[] = "NewObjectArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len, clazz, init);
	actualResult = globalJavaVM->EsJNIFunctions->NewObjectArray(env, len, clazz, init);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobject JNICALL
checkGetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECTARRAY, JNIC_JSIZE, 0 };
	static const char function[] = "GetObjectArrayElement";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, index);
	jniCheckArrayRange(env, function, array, index, 1);
	actualResult = globalJavaVM->EsJNIFunctions->GetObjectArrayElement(env, array, index);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkSetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject val)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECTARRAY, JNIC_JSIZE, JNIC_JOBJECT, 0 };
	static const char function[] = "SetObjectArrayElement";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, index, val);
	jniCheckArrayRange(env, function, array, index, 1);
	globalJavaVM->EsJNIFunctions->SetObjectArrayElement(env, array, index, val);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jbooleanArray JNICALL
checkNewBooleanArray(JNIEnv *env, jsize len)
{ 	
	jbooleanArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewBooleanArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewBooleanArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jbyteArray JNICALL
checkNewByteArray(JNIEnv *env, jsize len)
{ 	
	jbyteArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewByteArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewByteArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jcharArray JNICALL
checkNewCharArray(JNIEnv *env, jsize len)
{ 	
	jcharArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewCharArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewCharArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jshortArray JNICALL
checkNewShortArray(JNIEnv *env, jsize len)
{ 	
	jshortArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewShortArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewShortArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jintArray JNICALL
checkNewIntArray(JNIEnv *env, jsize len)
{ 	
	jintArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewIntArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewIntArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jlongArray JNICALL
checkNewLongArray(JNIEnv *env, jsize len)
{ 	
	jlongArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewLongArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewLongArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jfloatArray JNICALL
checkNewFloatArray(JNIEnv *env, jsize len)
{ 	
	jfloatArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewFloatArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewFloatArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jdoubleArray JNICALL
checkNewDoubleArray(JNIEnv *env, jsize len)
{ 	
	jdoubleArray actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSIZE, 0 };
	static const char function[] = "NewDoubleArray";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, len);
	actualResult = globalJavaVM->EsJNIFunctions->NewDoubleArray(env, len);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jboolean * JNICALL
checkGetBooleanArrayElements(JNIEnv *env, jbooleanArray array, jboolean *isCopy)
{ 	
	jboolean * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBOOLEANARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetBooleanArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetBooleanArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jbyte * JNICALL
checkGetByteArrayElements(JNIEnv *env, jbyteArray array, jboolean *isCopy)
{ 	
	jbyte * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBYTEARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetByteArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetByteArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jchar * JNICALL
checkGetCharArrayElements(JNIEnv *env, jcharArray array, jboolean *isCopy)
{ 	
	jchar * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCHARARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetCharArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetCharArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jshort * JNICALL
checkGetShortArrayElements(JNIEnv *env, jshortArray array, jboolean *isCopy)
{ 	
	jshort * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSHORTARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetShortArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetShortArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint * JNICALL
checkGetIntArrayElements(JNIEnv *env, jintArray array, jboolean *isCopy)
{ 	
	jint * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JINTARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetIntArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetIntArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jlong * JNICALL
checkGetLongArrayElements(JNIEnv *env, jlongArray array, jboolean *isCopy)
{ 	
	jlong * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JLONGARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetLongArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetLongArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jfloat * JNICALL
checkGetFloatArrayElements(JNIEnv *env, jfloatArray array, jboolean *isCopy)
{ 	
	jfloat * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JFLOATARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetFloatArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetFloatArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jdouble * JNICALL
checkGetDoubleArrayElements(JNIEnv *env, jdoubleArray array, jboolean *isCopy)
{ 	
	jdouble * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JDOUBLEARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetDoubleArrayElements";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, isCopy);
	actualResult = globalJavaVM->EsJNIFunctions->GetDoubleArrayElements(env, array, isCopy);
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkReleaseBooleanArrayElements(JNIEnv *env, jbooleanArray array, jboolean *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBOOLEANARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseBooleanArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetBooleanArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseBooleanArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseByteArrayElements(JNIEnv *env, jbyteArray array, jbyte *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBYTEARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseByteArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetByteArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseByteArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseCharArrayElements(JNIEnv *env, jcharArray array, jchar *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCHARARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseCharArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetCharArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseCharArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseShortArrayElements(JNIEnv *env, jshortArray array, jshort *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSHORTARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseShortArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetShortArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseShortArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseIntArrayElements(JNIEnv *env, jintArray array, jint *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JINTARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseIntArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetIntArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseIntArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseLongArrayElements(JNIEnv *env, jlongArray array, jlong *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JLONGARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseLongArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetLongArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseLongArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseFloatArrayElements(JNIEnv *env, jfloatArray array, jfloat *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JFLOATARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseFloatArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetFloatArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseFloatArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkReleaseDoubleArrayElements(JNIEnv *env, jdoubleArray array, jdouble *elems, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JDOUBLEARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleaseDoubleArrayElements";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, array, elems, mode);
	jniRecordMemoryRelease(env, "GetDoubleArrayElements", function, array, elems, 1, mode);
	globalJavaVM->EsJNIFunctions->ReleaseDoubleArrayElements(env, array, elems, mode);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetBooleanArrayRegion(JNIEnv *env, jbooleanArray array, jsize start, jsize len, jboolean *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBOOLEANARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetBooleanArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetBooleanArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBYTEARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetByteArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetByteArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetCharArrayRegion(JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCHARARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetCharArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetCharArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetShortArrayRegion(JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSHORTARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetShortArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetShortArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetIntArrayRegion(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JINTARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetIntArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetIntArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetLongArrayRegion(JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JLONGARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetLongArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetLongArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetFloatArrayRegion(JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JFLOATARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetFloatArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetFloatArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetDoubleArrayRegion(JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JDOUBLEARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetDoubleArrayRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	globalJavaVM->EsJNIFunctions->GetDoubleArrayRegion(env, array, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetBooleanArrayRegion(JNIEnv *env, jbooleanArray array, jsize start, jsize len, jboolean *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBOOLEANARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetBooleanArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetBooleanArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JBYTEARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetByteArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetByteArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetCharArrayRegion(JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCHARARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetCharArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetCharArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetShortArrayRegion(JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSHORTARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetShortArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetShortArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetIntArrayRegion(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JINTARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetIntArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetIntArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetLongArrayRegion(JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JLONGARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetLongArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetLongArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetFloatArrayRegion(JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JFLOATARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetFloatArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetFloatArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkSetDoubleArrayRegion(JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JDOUBLEARRAY, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "SetDoubleArrayRegion";
	U_32 bufCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, array, start, len, buf);
	jniCheckArrayRange(env, function, array, start, len);
	bufCRC = computeDataCRC(buf, len * sizeof(buf[0]));
	globalJavaVM->EsJNIFunctions->SetDoubleArrayRegion(env, array, start, len, buf);
	checkDataCRC(function, 5, buf, len * sizeof(buf[0]), bufCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jint JNICALL
checkRegisterNatives(JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "RegisterNatives";
	U_32 methodsCRC;

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz, methods, nMethods);
	jniCheckRange(function, "jint", nMethods, 4, 1, INT32_MAX);
	methodsCRC = computeDataCRC(methods, nMethods * sizeof(methods[0]));
	actualResult = globalJavaVM->EsJNIFunctions->RegisterNatives(env, clazz, methods, nMethods);
	checkDataCRC(function, 3, methods, nMethods * sizeof(methods[0]), methodsCRC);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkUnregisterNatives(JNIEnv *env, jclass clazz)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JCLASS, 0 };
	static const char function[] = "UnregisterNatives";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, clazz);
	actualResult = globalJavaVM->EsJNIFunctions->UnregisterNatives(env, clazz);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkMonitorEnter(JNIEnv *env, jobject obj)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, 0 };
	static const char function[] = "MonitorEnter";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	actualResult = globalJavaVM->EsJNIFunctions->MonitorEnter(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkMonitorExit(JNIEnv *env, jobject obj)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_NONNULLOBJECT, 0 };
	static const char function[] = "MonitorExit";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	actualResult = globalJavaVM->EsJNIFunctions->MonitorExit(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jint JNICALL
checkGetJavaVM(JNIEnv *env, JavaVM **vm)
{ 	
	jint actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_POINTER, 0 };
	static const char function[] = "GetJavaVM";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, vm);
	actualResult = globalJavaVM->EsJNIFunctions->GetJavaVM(env, vm);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkGetStringRegion(JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetStringRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, str, start, len, buf);
	if ((len > 0) && (NULL == buf)) {
		jniCheckFatalErrorNLS(J9NLS_JNICHK_ARGUMENT_IS_NULL, function, 5);
	}
	jniCheckStringRange(env, function, str, start, len);
	globalJavaVM->EsJNIFunctions->GetStringRegion(env, str, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void JNICALL
checkGetStringUTFRegion(JNIEnv *env, jstring str, jsize start, jsize len, char *buf)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_JSIZE, JNIC_JSIZE, JNIC_POINTER, 0 };
	static const char function[] = "GetStringUTFRegion";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, str, start, len, buf);
	if ((len > 0) && (NULL == buf)) {
		jniCheckFatalErrorNLS(J9NLS_JNICHK_ARGUMENT_IS_NULL, function, 5);
	}
	jniCheckStringUTFRange(env, function, str, start, len);
	globalJavaVM->EsJNIFunctions->GetStringUTFRegion(env, str, start, len, buf);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static void * JNICALL
checkGetPrimitiveArrayCritical(JNIEnv *env, jarray array, jboolean *isCopy)
{ 	
	void * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JARRAY, JNIC_POINTER, 0 };
	static const char function[] = "GetPrimitiveArrayCritical";
	
	jniCheckArgs(function, 0, CRITICAL_SAFE, &refTracking, argDescriptor, env, array, isCopy);
	if ( (globalJavaVM->checkJNIData.options & JNICHK_ALWAYSCOPY) &&
			((globalJavaVM->checkJNIData.options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) ) {
		actualResult = (void*)(globalJavaVM->EsJNIFunctions->GetByteArrayElements(env,array,isCopy));
	} else {
		actualResult = globalJavaVM->EsJNIFunctions->GetPrimitiveArrayCritical(env, array, isCopy);
	}
	jniRecordMemoryAcquire(env, function, array, actualResult, 1);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkReleasePrimitiveArrayCritical(JNIEnv *env, jarray array, void *carray, jint mode)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JARRAY, JNIC_POINTER, JNIC_JINT, 0 };
	static const char function[] = "ReleasePrimitiveArrayCritical";

	jniCheckArgs(function, 1, CRITICAL_SAFE, &refTracking, argDescriptor, env, array, carray, mode);
	jniRecordMemoryRelease(env, "GetPrimitiveArrayCritical", function, array, carray, 1, mode);
	if ( (globalJavaVM->checkJNIData.options & JNICHK_ALWAYSCOPY) &&
			((globalJavaVM->checkJNIData.options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) ) {
		globalJavaVM->EsJNIFunctions->ReleaseByteArrayElements(env, array, carray, mode);
	} else {
		globalJavaVM->EsJNIFunctions->ReleasePrimitiveArrayCritical(env, array, carray, mode);
	}
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static const jchar * JNICALL
checkGetStringCritical(JNIEnv *env, jstring string, jboolean *isCopy)
{ 	
	const jchar * actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_POINTER, 0 };
	static const char function[] = "GetStringCritical";

	jniCheckArgs(function, 0, CRITICAL_SAFE, &refTracking, argDescriptor, env, string, isCopy);
	/* Delegate to GetStringChars if ALWAYSCOPY is set */
	if ( (globalJavaVM->checkJNIData.options & JNICHK_ALWAYSCOPY) &&
			((globalJavaVM->checkJNIData.options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) ) {
		actualResult = globalJavaVM->EsJNIFunctions->GetStringChars(env,string,isCopy);
	} else {
		actualResult = globalJavaVM->EsJNIFunctions->GetStringCritical(env, string, isCopy);
	}
	jniRecordMemoryAcquire(env, function, string, actualResult, 0);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkReleaseStringCritical(JNIEnv *env, jstring string, const jchar *carray)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JSTRING, JNIC_POINTER, 0 };
	static const char function[] = "ReleaseStringCritical";

	jniCheckArgs(function, 1, CRITICAL_SAFE, &refTracking, argDescriptor, env, string, carray);
	jniRecordMemoryRelease(env, "GetStringCritical", function, string, carray, 0, 0);
	/* Delegate to ReleaseStringChars if ALWAYSCOPY is set */
	if ( (globalJavaVM->checkJNIData.options & JNICHK_ALWAYSCOPY) &&
			((globalJavaVM->checkJNIData.options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) ) {
		globalJavaVM->EsJNIFunctions->ReleaseStringChars(env, string, carray);
	} else {
		globalJavaVM->EsJNIFunctions->ReleaseStringCritical(env, string, carray);
	}
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jweak JNICALL
checkNewWeakGlobalRef(JNIEnv *env, jobject obj)
{ 	
	jweak actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, 0 };
	static const char function[] = "NewWeakGlobalRef";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	actualResult = globalJavaVM->EsJNIFunctions->NewWeakGlobalRef(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void JNICALL
checkDeleteWeakGlobalRef(JNIEnv *env, jweak obj)
{ 	
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_WEAKREF, 0 };
	static const char function[] = "DeleteWeakGlobalRef";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	globalJavaVM->EsJNIFunctions->DeleteWeakGlobalRef(env, obj);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
}

static jboolean JNICALL
checkExceptionCheck(JNIEnv *env)
{ 	
	jboolean actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { 0 };
	static const char function[] = "ExceptionCheck";

	jniCheckArgs(function, 1, CRITICAL_WARN, &refTracking, argDescriptor, env);
	actualResult = globalJavaVM->EsJNIFunctions->ExceptionCheck(env);
	jniCheckLocalRefTracking(env, function, &refTracking);
	jniCheckFlushJNICache(env);
	/* the caller can now tell that an exception is pending */
	jniCheckSetPotentialPendingException(NULL);

	return actualResult;
}

static jobject JNICALL
checkNewDirectByteBuffer(JNIEnv *env, void *address, jlong capacity)
{ 	
	jobject actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_POINTER, JNIC_JLONG, 0 };
	static const char function[] = "NewDirectByteBuffer";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, address, capacity);
	actualResult = globalJavaVM->EsJNIFunctions->NewDirectByteBuffer(env, address, capacity);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static void* JNICALL
checkGetDirectBufferAddress(JNIEnv *env, jobject buf)
{ 	
	void* actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_DIRECTBUFFER, 0 };
	static const char function[] = "GetDirectBufferAddress";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, buf);
	actualResult = globalJavaVM->EsJNIFunctions->GetDirectBufferAddress(env, buf);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jlong JNICALL
checkGetDirectBufferCapacity(JNIEnv *env, jobject buf)
{ 	
	jlong actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_DIRECTBUFFER, 0 };
	static const char function[] = "GetDirectBufferCapacity";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, buf);
	actualResult = globalJavaVM->EsJNIFunctions->GetDirectBufferCapacity(env, buf);
	jniCheckFlushJNICache(env);

	return actualResult;
}

static jobjectRefType JNICALL
checkGetObjectRefType(JNIEnv *env, jobject obj)
{ 	
	jobjectRefType actualResult;
	J9JniCheckLocalRefState refTracking;
	static const U_32 argDescriptor[] = { JNIC_JOBJECT, 0 };
	static const char function[] = "GetObjectRefType";

	jniCheckArgs(function, 0, CRITICAL_WARN, &refTracking, argDescriptor, env, obj);
	actualResult = globalJavaVM->EsJNIFunctions->GetObjectRefType(env, obj);
	jniCheckFlushJNICache(env);

	return actualResult;
}


const struct JNINativeInterface_ JNICheckTable = {
	NULL,
	NULL,
	NULL,
	NULL,
	checkGetVersion,
	checkDefineClass,
	checkFindClass,
	checkFromReflectedMethod,
	checkFromReflectedField,
	checkToReflectedMethod,
	checkGetSuperclass,
	checkIsAssignableFrom,
	checkToReflectedField,
	checkThrow,
	checkThrowNew,
	checkExceptionOccurred,
	checkExceptionDescribe,
	checkExceptionClear,
	checkFatalError,
	checkPushLocalFrame,
	checkPopLocalFrame,
	checkNewGlobalRef,
	checkDeleteGlobalRef,
	checkDeleteLocalRef,
	checkIsSameObject,
	checkNewLocalRef,
	checkEnsureLocalCapacity,
	checkAllocObject,
	checkNewObject,
	checkNewObjectV,
	checkNewObjectA,
	checkGetObjectClass,
	checkIsInstanceOf,
	checkGetMethodID,
	checkCallObjectMethod,
	checkCallObjectMethodV,
	checkCallObjectMethodA,
	checkCallBooleanMethod,
	checkCallBooleanMethodV,
	checkCallBooleanMethodA,
	checkCallByteMethod,
	checkCallByteMethodV,
	checkCallByteMethodA,
	checkCallCharMethod,
	checkCallCharMethodV,
	checkCallCharMethodA,
	checkCallShortMethod,
	checkCallShortMethodV,
	checkCallShortMethodA,
	checkCallIntMethod,
	checkCallIntMethodV,
	checkCallIntMethodA,
	checkCallLongMethod,
	checkCallLongMethodV,
	checkCallLongMethodA,
	checkCallFloatMethod,
	checkCallFloatMethodV,
	checkCallFloatMethodA,
	checkCallDoubleMethod,
	checkCallDoubleMethodV,
	checkCallDoubleMethodA,
	checkCallVoidMethod,
	checkCallVoidMethodV,
	checkCallVoidMethodA,
	checkCallNonvirtualObjectMethod,
	checkCallNonvirtualObjectMethodV,
	checkCallNonvirtualObjectMethodA,
	checkCallNonvirtualBooleanMethod,
	checkCallNonvirtualBooleanMethodV,
	checkCallNonvirtualBooleanMethodA,
	checkCallNonvirtualByteMethod,
	checkCallNonvirtualByteMethodV,
	checkCallNonvirtualByteMethodA,
	checkCallNonvirtualCharMethod,
	checkCallNonvirtualCharMethodV,
	checkCallNonvirtualCharMethodA,
	checkCallNonvirtualShortMethod,
	checkCallNonvirtualShortMethodV,
	checkCallNonvirtualShortMethodA,
	checkCallNonvirtualIntMethod,
	checkCallNonvirtualIntMethodV,
	checkCallNonvirtualIntMethodA,
	checkCallNonvirtualLongMethod,
	checkCallNonvirtualLongMethodV,
	checkCallNonvirtualLongMethodA,
	checkCallNonvirtualFloatMethod,
	checkCallNonvirtualFloatMethodV,
	checkCallNonvirtualFloatMethodA,
	checkCallNonvirtualDoubleMethod,
	checkCallNonvirtualDoubleMethodV,
	checkCallNonvirtualDoubleMethodA,
	checkCallNonvirtualVoidMethod,
	checkCallNonvirtualVoidMethodV,
	checkCallNonvirtualVoidMethodA,
	checkGetFieldID,
	checkGetObjectField,
	checkGetBooleanField,
	checkGetByteField,
	checkGetCharField,
	checkGetShortField,
	checkGetIntField,
	checkGetLongField,
	checkGetFloatField,
	checkGetDoubleField,
	checkSetObjectField,
	checkSetBooleanField,
	checkSetByteField,
	checkSetCharField,
	checkSetShortField,
	checkSetIntField,
	checkSetLongField,
	checkSetFloatField,
	checkSetDoubleField,
	checkGetStaticMethodID,
	checkCallStaticObjectMethod,
	checkCallStaticObjectMethodV,
	checkCallStaticObjectMethodA,
	checkCallStaticBooleanMethod,
	checkCallStaticBooleanMethodV,
	checkCallStaticBooleanMethodA,
	checkCallStaticByteMethod,
	checkCallStaticByteMethodV,
	checkCallStaticByteMethodA,
	checkCallStaticCharMethod,
	checkCallStaticCharMethodV,
	checkCallStaticCharMethodA,
	checkCallStaticShortMethod,
	checkCallStaticShortMethodV,
	checkCallStaticShortMethodA,
	checkCallStaticIntMethod,
	checkCallStaticIntMethodV,
	checkCallStaticIntMethodA,
	checkCallStaticLongMethod,
	checkCallStaticLongMethodV,
	checkCallStaticLongMethodA,
	checkCallStaticFloatMethod,
	checkCallStaticFloatMethodV,
	checkCallStaticFloatMethodA,
	checkCallStaticDoubleMethod,
	checkCallStaticDoubleMethodV,
	checkCallStaticDoubleMethodA,
	checkCallStaticVoidMethod,
	checkCallStaticVoidMethodV,
	checkCallStaticVoidMethodA,
	checkGetStaticFieldID,
	checkGetStaticObjectField,
	checkGetStaticBooleanField,
	checkGetStaticByteField,
	checkGetStaticCharField,
	checkGetStaticShortField,
	checkGetStaticIntField,
	checkGetStaticLongField,
	checkGetStaticFloatField,
	checkGetStaticDoubleField,
	checkSetStaticObjectField,
	checkSetStaticBooleanField,
	checkSetStaticByteField,
	checkSetStaticCharField,
	checkSetStaticShortField,
	checkSetStaticIntField,
	checkSetStaticLongField,
	checkSetStaticFloatField,
	checkSetStaticDoubleField,
	checkNewString,
	checkGetStringLength,
	checkGetStringChars,
	checkReleaseStringChars,
	checkNewStringUTF,
	checkGetStringUTFLength,
	checkGetStringUTFChars,
	checkReleaseStringUTFChars,
	checkGetArrayLength,
	checkNewObjectArray,
	checkGetObjectArrayElement,
	checkSetObjectArrayElement,
	checkNewBooleanArray,
	checkNewByteArray,
	checkNewCharArray,
	checkNewShortArray,
	checkNewIntArray,
	checkNewLongArray,
	checkNewFloatArray,
	checkNewDoubleArray,
	checkGetBooleanArrayElements,
	checkGetByteArrayElements,
	checkGetCharArrayElements,
	checkGetShortArrayElements,
	checkGetIntArrayElements,
	checkGetLongArrayElements,
	checkGetFloatArrayElements,
	checkGetDoubleArrayElements,
	checkReleaseBooleanArrayElements,
	checkReleaseByteArrayElements,
	checkReleaseCharArrayElements,
	checkReleaseShortArrayElements,
	checkReleaseIntArrayElements,
	checkReleaseLongArrayElements,
	checkReleaseFloatArrayElements,
	checkReleaseDoubleArrayElements,
	checkGetBooleanArrayRegion,
	checkGetByteArrayRegion,
	checkGetCharArrayRegion,
	checkGetShortArrayRegion,
	checkGetIntArrayRegion,
	checkGetLongArrayRegion,
	checkGetFloatArrayRegion,
	checkGetDoubleArrayRegion,
	checkSetBooleanArrayRegion,
	checkSetByteArrayRegion,
	checkSetCharArrayRegion,
	checkSetShortArrayRegion,
	checkSetIntArrayRegion,
	checkSetLongArrayRegion,
	checkSetFloatArrayRegion,
	checkSetDoubleArrayRegion,
	checkRegisterNatives,
	checkUnregisterNatives,
	checkMonitorEnter,
	checkMonitorExit,
	checkGetJavaVM,
	checkGetStringRegion,
	checkGetStringUTFRegion,
	checkGetPrimitiveArrayCritical,
	checkReleasePrimitiveArrayCritical,
	checkGetStringCritical,
	checkReleaseStringCritical,
	checkNewWeakGlobalRef,
	checkDeleteWeakGlobalRef,
	checkExceptionCheck,
	checkNewDirectByteBuffer,
	checkGetDirectBufferAddress,
	checkGetDirectBufferCapacity,
	checkGetObjectRefType
};

