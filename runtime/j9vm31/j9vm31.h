/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef j9vm31_h
#define j9vm31_h

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "j9cel4ro64.h"
#include "jni.h"

/* High tag bits to signify 31-bit function handles that are passed to 64-bit. */
#define HANDLE31_HIGHTAG 0x31000000

/* Macro to look up 64-bit function descriptors from JNINativeInterface_. */
#define FUNCTION_DESCRIPTOR_FROM_JNIENV31(jniEnv, functionX) \
		jlong functionDescriptor = ((JNIEnv31 *)jniEnv)->functions64[offsetof(struct JNINativeInterface_, functionX) / sizeof(intptr_t)]; \
		j9vm31TrcJNIMethodEntry(jniEnv, #functionX);

/* Utility macros to extract corresponding 64-bit handle. */
#define JNIENV64_FROM_JNIENV31(jniEnv) ((JNIEnv31 *)jniEnv)->jniEnv64
#define JAVAVM64_FROM_JAVAVM31(javaVM) ((JavaVM31 *)javaVM)->javaVM64

/* We need to do DLL query on libjvm.so (sidecar redirector), which will load the appropriate VM module. */
#define LIBJVM_NAME "libjvm.so"
#define LIBJ9VM29_NAME "libj9vm29.so"

#ifdef __cplusplus
extern "C" {
#endif

/* A 31-bit JavaVM struct, instances of which are used by 31-bit native code.
 * Each instance should always have a matching 64-bit JavaVM * isntance from the JVM.
 */
typedef struct JavaVM31 {
	struct JNIInvokeInterface_ *functions;
	void *reserved0;
	void *reserved1;
	void *reserved2;
	JavaVM31 *next;             /**< Next JavaVM31 instance in global list. */
	jlong javaVM64;             /**< Handle to corresponding 64-bit JavaVM* instance from JVM. */
	const jlong *functions64;   /**< Reference to 64-bit JNIInvokeInterface_ function descriptors to invoke. */
} JavaVM31;

/* A 31-bit JNIEnv struct, instances of which are used by 31-bit native code.
 * Each instance should always have a matching 64-bit JNIEnv * isntance from the JVM.
 */
typedef struct JNIEnv31 {
	struct JNINativeInterface_ *functions;
	void *reserved0;
	void *reserved1[6];
	JavaVM31 *javaVM31;         /**< A handle to the matching JavaVM31 instance. */
	JNIEnv31 *next;             /**< Next JNIEnv31 instance in global list. */
	jlong jniEnv64;             /**< Handle to corresponding 64-bit JNIEnv* instance from JVM. */
	const jlong *functions64;   /**< Reference to 64-bit JNINativeInterface_ function descriptors to invoke. */
} JNIEnv31;

/**
 * Typedefs to wrap the arguments of a va_list to the 64-bit representation.
 * The actual parameter data itself is still in 31-bit, and those are interpreted on
 * the 64-bit side with recognition that they are being invoked by 31-bit caller, as
 * the parameters interpretation require understanding of the specifiers from the
 * method signature.
 */
typedef char  *___valist64[4];    // 64-bit uses char *[2].
typedef ___valist64  va_list_64;

/**
 * A helper function that can be also invoked by 64-bit JVM to return the respective 31-bit
 * JavaVM instance for the given 64-bit JavaVM reference. When invoked by the JVM, this
 * allows 64-bit JVM to directly invoke 31-bit target functions passing a valid 31-bit JavaVM*
 * parameter.
 *
 * @param[in] JavaVM64Ptr The 64-bit JavaVM reference.
 *
 * @return The 31-bit corresponding JavaVM instance.
 */
JavaVM31 *
JNICALL getJavaVM31(uint64_t JavaVM64Ptr);

/**
 * A helper function that can be also invoked by 64-bit JVM to return the respective 31-bit
 * JNIEnv instance for the given 64-bit JNIEnv reference. When invoked by the JVM, this
 * allows 64-bit JVM to directly invoke 31-bit target functions passing a valid 31-bit JNIEnv*
 * parameter.
 *
 * @param[in] JNIEnv64Ptr The 64-bit JNIEnv reference.
 *
 * @return The 31-bit corresponding JNIEnv instance.
 */
JNIEnv31 *
JNICALL getJNIEnv31(uint64_t JNIEnv64Ptr);

/**
 * A debugging routine that outputs method entry statements to stderr. While there
 * are trace points in 64-bit JVM for most of the JNI methods, this routine
 * supports diagnostics on 31-bit side.
 * @Todo: Should consider how to improve this RAS support.
 *
 * @param[in] JNIEnv The 31-bit JNIEnv instance.
 * @param[in] functionName The JNI function to invoke.
 */
void
j9vm31TrcJNIMethodEntry(JNIEnv* env, const char *functionName);

/* The Java Invocation API functions that the shim library will implement. */
jint JNICALL JNI_GetDefaultJavaVMInitArgs(void *);
jint JNICALL JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args);
jint JNICALL JNI_GetCreatedJavaVMs(JavaVM **, jsize, jsize *);
jint JNICALL DestroyJavaVM(JavaVM *vm);
jint JNICALL AttachCurrentThread(JavaVM *vm, void **penv, void *args);
jint JNICALL DetachCurrentThread(JavaVM *vm);
jint JNICALL GetEnv(JavaVM *vm, void **penv, jint version);
jint JNICALL AttachCurrentThreadAsDaemon(JavaVM *vm, void **penv, void *args);

/* The Java Native Interface API functions that the shim library will implement. */
jint JNICALL GetVersion(JNIEnv *env);
jclass JNICALL DefineClass(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize bufLen);
jclass JNICALL FindClass(JNIEnv *env, const char *name);
jmethodID JNICALL FromReflectedMethod(JNIEnv *env, jobject method);
jfieldID JNICALL FromReflectedField(JNIEnv *env, jobject field);
jobject JNICALL ToReflectedMethod(JNIEnv *env, jclass cls, jmethodID methodID, jboolean isStatic);
jclass JNICALL GetSuperclass(JNIEnv *env, jclass clazz);
jboolean JNICALL IsAssignableFrom(JNIEnv *env, jclass clazz1, jclass clazz2);
jobject JNICALL ToReflectedField(JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic);
jint JNICALL Throw(JNIEnv *env, jthrowable obj);
jint JNICALL ThrowNew(JNIEnv *env, jclass clazz, const char *message);
jthrowable JNICALL ExceptionOccurred(JNIEnv *env);
void JNICALL ExceptionDescribe(JNIEnv *env);
void JNICALL ExceptionClear(JNIEnv *env);
void JNICALL FatalError(JNIEnv *env, const char *msg);
jint JNICALL PushLocalFrame(JNIEnv *env, jint capacity);
jobject JNICALL PopLocalFrame(JNIEnv *env, jobject result);
jobject JNICALL NewGlobalRef(JNIEnv *env, jobject obj);
void JNICALL DeleteGlobalRef(JNIEnv *env, jobject gref);
void JNICALL DeleteLocalRef(JNIEnv *env, jobject localRef);
jboolean JNICALL IsSameObject(JNIEnv *env, jobject ref1, jobject ref2);
jobject JNICALL NewLocalRef(JNIEnv *env, jobject ref);
jint JNICALL EnsureLocalCapacity(JNIEnv *env, jint capacity);
jobject JNICALL AllocObject(JNIEnv *env, jclass clazz);
jobject JNICALL NewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jobject JNICALL NewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jobject JNICALL NewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jclass JNICALL GetObjectClass(JNIEnv *env, jobject obj);
jboolean JNICALL IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz);
jmethodID JNICALL GetMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
jobject JNICALL CallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jobject JNICALL CallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jobject JNICALL CallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
jboolean JNICALL CallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jboolean JNICALL CallBooleanMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jboolean JNICALL CallBooleanMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
jbyte JNICALL CallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jbyte JNICALL CallByteMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jbyte JNICALL CallByteMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
jchar JNICALL CallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jchar JNICALL CallCharMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jchar JNICALL CallCharMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
jshort JNICALL CallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jshort JNICALL CallShortMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jshort JNICALL CallShortMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
jint JNICALL CallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jint JNICALL CallIntMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jint JNICALL CallIntMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
jlong JNICALL CallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jlong JNICALL CallLongMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jlong JNICALL CallLongMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
jfloat JNICALL CallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jfloat JNICALL CallFloatMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jfloat JNICALL CallFloatMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
jdouble JNICALL CallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
jdouble JNICALL CallDoubleMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
jdouble JNICALL CallDoubleMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
void JNICALL CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...);
void JNICALL CallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
void JNICALL CallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
jobject JNICALL CallNonvirtualObjectMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jobject JNICALL CallNonvirtualObjectMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jobject JNICALL CallNonvirtualObjectMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
jboolean JNICALL CallNonvirtualBooleanMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jboolean JNICALL CallNonvirtualBooleanMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jboolean JNICALL CallNonvirtualBooleanMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
jbyte JNICALL CallNonvirtualByteMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jbyte JNICALL CallNonvirtualByteMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jbyte JNICALL CallNonvirtualByteMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
jchar JNICALL CallNonvirtualCharMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jchar JNICALL CallNonvirtualCharMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jchar JNICALL CallNonvirtualCharMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
jshort JNICALL CallNonvirtualShortMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jshort JNICALL CallNonvirtualShortMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jshort JNICALL CallNonvirtualShortMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
jint JNICALL CallNonvirtualIntMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jint JNICALL CallNonvirtualIntMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jint JNICALL CallNonvirtualIntMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
jlong JNICALL CallNonvirtualLongMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jlong JNICALL CallNonvirtualLongMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jlong JNICALL CallNonvirtualLongMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
jfloat JNICALL CallNonvirtualFloatMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jfloat JNICALL CallNonvirtualFloatMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jfloat JNICALL CallNonvirtualFloatMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
jdouble JNICALL CallNonvirtualDoubleMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
jdouble JNICALL CallNonvirtualDoubleMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
jdouble JNICALL CallNonvirtualDoubleMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
void JNICALL CallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
void JNICALL CallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
void JNICALL CallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
jfieldID JNICALL GetFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
jobject JNICALL GetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID);
jboolean JNICALL GetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID);
jbyte JNICALL GetByteField(JNIEnv *env, jobject obj, jfieldID fieldID);
jchar JNICALL GetCharField(JNIEnv *env, jobject obj, jfieldID fieldID);
jshort JNICALL GetShortField(JNIEnv *env, jobject obj, jfieldID fieldID);
jint JNICALL GetIntField(JNIEnv *env, jobject obj, jfieldID fieldID);
jlong JNICALL GetLongField(JNIEnv *env, jobject obj, jfieldID fieldID);
jfloat JNICALL GetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID);
jdouble JNICALL GetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID);
void JNICALL SetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject value);
void JNICALL SetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean value);
void JNICALL SetByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte value);
void JNICALL SetCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar value);
void JNICALL SetShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort value);
void JNICALL SetIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint value);
void JNICALL SetLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong value);
void JNICALL SetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat value);
void JNICALL SetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble value);
jmethodID JNICALL GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
jobject JNICALL CallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jobject JNICALL CallStaticObjectMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jobject JNICALL CallStaticObjectMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jboolean JNICALL CallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jboolean JNICALL CallStaticBooleanMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jboolean JNICALL CallStaticBooleanMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jbyte JNICALL CallStaticByteMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jbyte JNICALL CallStaticByteMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jbyte JNICALL CallStaticByteMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jchar JNICALL CallStaticCharMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jchar JNICALL CallStaticCharMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jchar JNICALL CallStaticCharMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jshort JNICALL CallStaticShortMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jshort JNICALL CallStaticShortMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jshort JNICALL CallStaticShortMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jint JNICALL CallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jint JNICALL CallStaticIntMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jint JNICALL CallStaticIntMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jlong JNICALL CallStaticLongMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jlong JNICALL CallStaticLongMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jlong JNICALL CallStaticLongMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jfloat JNICALL CallStaticFloatMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jfloat JNICALL CallStaticFloatMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jfloat JNICALL CallStaticFloatMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
jdouble JNICALL CallStaticDoubleMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
jdouble JNICALL CallStaticDoubleMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
jdouble JNICALL CallStaticDoubleMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
void JNICALL CallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
void JNICALL CallStaticVoidMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
void JNICALL CallStaticVoidMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args);
jfieldID JNICALL GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
jobject JNICALL GetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jboolean JNICALL GetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jbyte JNICALL GetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jchar JNICALL GetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jshort JNICALL GetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jint JNICALL GetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jlong JNICALL GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jfloat JNICALL GetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID);
jdouble JNICALL GetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID);
void JNICALL SetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value);
void JNICALL SetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean value);
void JNICALL SetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID, jbyte value);
void JNICALL SetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID, jchar value);
void JNICALL SetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID, jshort value);
void JNICALL SetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID, jint value);
void JNICALL SetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID, jlong value);
void JNICALL SetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID, jfloat value);
void JNICALL SetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID, jdouble value);
jstring JNICALL NewString(JNIEnv *env, const jchar *unicodeChars, jsize len);
jsize JNICALL GetStringLength(JNIEnv *env, jstring string);
const jchar *JNICALL GetStringChars(JNIEnv *env, jstring string, jboolean *isCopy);
void JNICALL ReleaseStringChars(JNIEnv *env, jstring string, const jchar *utf);
jstring JNICALL NewStringUTF(JNIEnv *env, const char *bytes);
jsize JNICALL GetStringUTFLength(JNIEnv *env, jstring string);
const char* JNICALL GetStringUTFChars(JNIEnv *env, jstring string, jboolean *isCopy);
void JNICALL ReleaseStringUTFChars(JNIEnv *env, jstring string, const char* utf);
jsize JNICALL GetArrayLength(JNIEnv *env, jarray array);
jobjectArray JNICALL NewObjectArray(JNIEnv *env, jsize length, jclass clazz, jobject initialElement);
jobject JNICALL GetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index);
void JNICALL SetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject value);
jbooleanArray JNICALL NewBooleanArray(JNIEnv *env, jsize length);
jbyteArray JNICALL NewByteArray(JNIEnv *env, jsize length);
jcharArray JNICALL NewCharArray(JNIEnv *env, jsize length);
jshortArray JNICALL NewShortArray(JNIEnv *env, jsize length);
jintArray JNICALL NewIntArray(JNIEnv *env, jsize length);
jlongArray JNICALL NewLongArray(JNIEnv *env, jsize length);
jfloatArray JNICALL NewFloatArray(JNIEnv *env, jsize length);
jdoubleArray JNICALL NewDoubleArray(JNIEnv *env, jsize length);
void * JNICALL getArrayElements(JNIEnv *env, jdoubleArray array, jboolean *isCopy);
void JNICALL releaseArrayElements(JNIEnv *env, jarray array, void *elems, jint mode);
void JNICALL getArrayRegion(JNIEnv *env, jarray array, jsize start, jsize len, void *buf);
void JNICALL setArrayRegion(JNIEnv *env, jarray array, jsize start, jsize len, void *buf);
jint JNICALL RegisterNatives(JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods);
jint JNICALL UnregisterNatives(JNIEnv *env, jclass clazz);
jint JNICALL MonitorEnter(JNIEnv *env, jobject obj);
jint JNICALL MonitorExit(JNIEnv *env, jobject obj);
jint JNICALL GetJavaVM(JNIEnv *env, JavaVM **vm);
void JNICALL GetStringRegion(JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf);
void JNICALL GetStringUTFRegion(JNIEnv *env, jstring str, jsize start, jsize len, char *buf);
jweak JNICALL NewWeakGlobalRef(JNIEnv *env, jobject obj);
void JNICALL DeleteWeakGlobalRef(JNIEnv *env, jweak obj);
jboolean JNICALL ExceptionCheck(JNIEnv *env);
jobject JNICALL NewDirectByteBuffer(JNIEnv *env, void *address, jlong capacity);
void * JNICALL GetDirectBufferAddress(JNIEnv *env, jobject buf);
jlong JNICALL GetDirectBufferCapacity(JNIEnv *env, jobject buf);
jobjectRefType JNICALL GetObjectRefType(JNIEnv* env, jobject obj);
#if JAVA_SPEC_VERSION >= 9
jobject JNICALL GetModule(JNIEnv *env, jclass clazz);
#endif /* JAVA_SPEC_VERSION >= 9 */

#ifdef __cplusplus
}
#endif

#endif /* j9vm31_h */
