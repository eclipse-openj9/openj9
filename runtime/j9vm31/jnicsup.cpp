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

#include "j9vm31.h"
#include "omrcomp.h"

extern "C" {

#define GET_BOOLEAN_ARRAY_ELEMENTS ((jboolean * (JNICALL *)(JNIEnv *env, jbooleanArray array, jboolean * isCopy))getArrayElements)
#define GET_BYTE_ARRAY_ELEMENTS ((jbyte * (JNICALL *)(JNIEnv *env, jbyteArray array, jboolean * isCopy))getArrayElements)
#define GET_CHAR_ARRAY_ELEMENTS ((jchar * (JNICALL *)(JNIEnv *env, jcharArray array, jboolean * isCopy))getArrayElements)
#define GET_SHORT_ARRAY_ELEMENTS ((jshort * (JNICALL *)(JNIEnv *env, jshortArray array, jboolean * isCopy))getArrayElements)
#define GET_INT_ARRAY_ELEMENTS ((jint * (JNICALL *)(JNIEnv *env, jintArray array, jboolean * isCopy))getArrayElements)
#define GET_LONG_ARRAY_ELEMENTS ((jlong * (JNICALL *)(JNIEnv *env, jlongArray array, jboolean * isCopy))getArrayElements)
#define GET_FLOAT_ARRAY_ELEMENTS ((jfloat * (JNICALL *)(JNIEnv *env, jfloatArray array, jboolean * isCopy))getArrayElements)
#define GET_DOUBLE_ARRAY_ELEMENTS ((jdouble * (JNICALL *)(JNIEnv *env, jdoubleArray array, jboolean * isCopy))getArrayElements)
#define GET_PRIMITIVE_ARRAY_CRITICAL ((void * (JNICALL *)(JNIEnv *env, jarray array, jboolean * isCopy))getArrayElements)

#define RELEASE_BOOLEAN_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jbooleanArray array, jboolean * elems, jint mode))releaseArrayElements)
#define RELEASE_BYTE_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jbyteArray array, jbyte * elems, jint mode))releaseArrayElements)
#define RELEASE_CHAR_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jcharArray array, jchar * elems, jint mode))releaseArrayElements)
#define RELEASE_SHORT_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jshortArray array, jshort * elems, jint mode))releaseArrayElements)
#define RELEASE_INT_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jintArray array, jint * elems, jint mode))releaseArrayElements)
#define RELEASE_LONG_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jlongArray array, jlong * elems, jint mode))releaseArrayElements)
#define RELEASE_FLOAT_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jfloatArray array, jfloat * elems, jint mode))releaseArrayElements)
#define RELEASE_DOUBLE_ARRAY_ELEMENTS ((void (JNICALL *)(JNIEnv *env, jdoubleArray array, jdouble * elems, jint mode))releaseArrayElements)
#define RELEASE_PRIMITIVE_ARRAY_CRITICAL ((void (JNICALL *)(JNIEnv *env, jarray carray, void * elems, jint mode))releaseArrayElements)

#define GET_BOOLEAN_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jbooleanArray array, jsize start, jsize len, jboolean *buf))getArrayRegion)
#define GET_BYTE_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf))getArrayRegion)
#define GET_CHAR_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf))getArrayRegion)
#define GET_SHORT_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf))getArrayRegion)
#define GET_INT_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf))getArrayRegion)
#define GET_LONG_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf))getArrayRegion)
#define GET_FLOAT_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf))getArrayRegion)
#define GET_DOUBLE_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf))getArrayRegion)

#define SET_BOOLEAN_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jbooleanArray array, jsize start, jsize len, jboolean *buf))setArrayRegion)
#define SET_BYTE_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf))setArrayRegion)
#define SET_CHAR_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf))setArrayRegion)
#define SET_SHORT_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf))setArrayRegion)
#define SET_INT_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf))setArrayRegion)
#define SET_LONG_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf))setArrayRegion)
#define SET_FLOAT_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf))setArrayRegion)
#define SET_DOUBLE_ARRAY_REGION ((void (JNICALL *)(JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf))setArrayRegion)

#define GET_STRING_CRITICAL ((const jchar * (JNICALL *)(JNIEnv *env, jstring string, jboolean *isCopy))GetStringChars)
#define RELEASE_STRING_CRITICAL ((void (JNICALL *)(JNIEnv *env, jstring string, const jchar *carray))ReleaseStringChars)

/* Given LE supports only one TCB that can cross AMODE boundary, most likely
 * we will only have 1 JNIEnv31 in the address space. Use a static to
 * track the live JNIEnv31 instances in case this gets expanded in the future.
 */
static JNIEnv31* globalRootJNIEnv31 = NULL;

/* A handle on the global table from 64-bit JVM with set of function pointers. */
static const jlong * global64BitJNIInterface_ = NULL;

/* This is the function table of the 31-bit shim JNINativeInterface functions. */
struct JNINativeInterface_ EsJNIFunctions = {
	NULL,
	NULL,
	NULL,
	NULL,
	GetVersion,
	DefineClass,
	FindClass,
	FromReflectedMethod,
	FromReflectedField,
	ToReflectedMethod,
	GetSuperclass,
	IsAssignableFrom,
	ToReflectedField,
	Throw,
	ThrowNew,
	ExceptionOccurred,
	ExceptionDescribe,
	ExceptionClear,
	FatalError,
	PushLocalFrame,
	PopLocalFrame,
	NewGlobalRef,
	DeleteGlobalRef,
	DeleteLocalRef,
	IsSameObject,
	NewLocalRef,
	EnsureLocalCapacity,
	AllocObject,
	NewObject,
	NewObjectV,
	NewObjectA,
	GetObjectClass,
	IsInstanceOf,
	GetMethodID,
	CallObjectMethod,
	CallObjectMethodV,
	CallObjectMethodA,
	CallBooleanMethod,
	CallBooleanMethodV,
	CallBooleanMethodA,
	CallByteMethod,
	CallByteMethodV,
	CallByteMethodA,
	CallCharMethod,
	CallCharMethodV,
	CallCharMethodA,
	CallShortMethod,
	CallShortMethodV,
	CallShortMethodA,
	CallIntMethod,
	CallIntMethodV,
	CallIntMethodA,
	CallLongMethod,
	CallLongMethodV,
	CallLongMethodA,
	CallFloatMethod,
	CallFloatMethodV,
	CallFloatMethodA,
	CallDoubleMethod,
	CallDoubleMethodV,
	CallDoubleMethodA,
	CallVoidMethod,
	CallVoidMethodV,
	CallVoidMethodA,
	CallNonvirtualObjectMethod,
	CallNonvirtualObjectMethodV,
	CallNonvirtualObjectMethodA,
	CallNonvirtualBooleanMethod,
	CallNonvirtualBooleanMethodV,
	CallNonvirtualBooleanMethodA,
	CallNonvirtualByteMethod,
	CallNonvirtualByteMethodV,
	CallNonvirtualByteMethodA,
	CallNonvirtualCharMethod,
	CallNonvirtualCharMethodV,
	CallNonvirtualCharMethodA,
	CallNonvirtualShortMethod,
	CallNonvirtualShortMethodV,
	CallNonvirtualShortMethodA,
	CallNonvirtualIntMethod,
	CallNonvirtualIntMethodV,
	CallNonvirtualIntMethodA,
	CallNonvirtualLongMethod,
	CallNonvirtualLongMethodV,
	CallNonvirtualLongMethodA,
	CallNonvirtualFloatMethod,
	CallNonvirtualFloatMethodV,
	CallNonvirtualFloatMethodA,
	CallNonvirtualDoubleMethod,
	CallNonvirtualDoubleMethodV,
	CallNonvirtualDoubleMethodA,
	CallNonvirtualVoidMethod,
	CallNonvirtualVoidMethodV,
	CallNonvirtualVoidMethodA,
	GetFieldID,
	GetObjectField,
	GetBooleanField,
	GetByteField,
	GetCharField,
	GetShortField,
	GetIntField,
	GetLongField,
	GetFloatField,
	GetDoubleField,
	SetObjectField,
	SetBooleanField,
	SetByteField,
	SetCharField,
	SetShortField,
	SetIntField,
	SetLongField,
	SetFloatField,
	SetDoubleField,
	GetStaticMethodID,
	CallStaticObjectMethod,
	CallStaticObjectMethodV,
	CallStaticObjectMethodA,
	CallStaticBooleanMethod,
	CallStaticBooleanMethodV,
	CallStaticBooleanMethodA,
	CallStaticByteMethod,
	CallStaticByteMethodV,
	CallStaticByteMethodA,
	CallStaticCharMethod,
	CallStaticCharMethodV,
	CallStaticCharMethodA,
	CallStaticShortMethod,
	CallStaticShortMethodV,
	CallStaticShortMethodA,
	CallStaticIntMethod,
	CallStaticIntMethodV,
	CallStaticIntMethodA,
	CallStaticLongMethod,
	CallStaticLongMethodV,
	CallStaticLongMethodA,
	CallStaticFloatMethod,
	CallStaticFloatMethodV,
	CallStaticFloatMethodA,
	CallStaticDoubleMethod,
	CallStaticDoubleMethodV,
	CallStaticDoubleMethodA,
	CallStaticVoidMethod,
	CallStaticVoidMethodV,
	CallStaticVoidMethodA,
	GetStaticFieldID,
	GetStaticObjectField,
	GetStaticBooleanField,
	GetStaticByteField,
	GetStaticCharField,
	GetStaticShortField,
	GetStaticIntField,
	GetStaticLongField,
	GetStaticFloatField,
	GetStaticDoubleField,
	SetStaticObjectField,
	SetStaticBooleanField,
	SetStaticByteField,
	SetStaticCharField,
	SetStaticShortField,
	SetStaticIntField,
	SetStaticLongField,
	SetStaticFloatField,
	SetStaticDoubleField,
	NewString,
	GetStringLength,
	GetStringChars,
	ReleaseStringChars,
	NewStringUTF,
	GetStringUTFLength,
	GetStringUTFChars,
	ReleaseStringUTFChars,
	GetArrayLength,
	NewObjectArray,
	GetObjectArrayElement,
	SetObjectArrayElement,
	NewBooleanArray,
	NewByteArray,
	NewCharArray,
	NewShortArray,
	NewIntArray,
	NewLongArray,
	NewFloatArray,
	NewDoubleArray,
	GET_BOOLEAN_ARRAY_ELEMENTS,
	GET_BYTE_ARRAY_ELEMENTS,
	GET_CHAR_ARRAY_ELEMENTS,
	GET_SHORT_ARRAY_ELEMENTS,
	GET_INT_ARRAY_ELEMENTS,
	GET_LONG_ARRAY_ELEMENTS,
	GET_FLOAT_ARRAY_ELEMENTS,
	GET_DOUBLE_ARRAY_ELEMENTS,
	RELEASE_BOOLEAN_ARRAY_ELEMENTS,
	RELEASE_BYTE_ARRAY_ELEMENTS,
	RELEASE_CHAR_ARRAY_ELEMENTS,
	RELEASE_SHORT_ARRAY_ELEMENTS,
	RELEASE_INT_ARRAY_ELEMENTS,
	RELEASE_LONG_ARRAY_ELEMENTS,
	RELEASE_FLOAT_ARRAY_ELEMENTS,
	RELEASE_DOUBLE_ARRAY_ELEMENTS,
	GET_BOOLEAN_ARRAY_REGION,
	GET_BYTE_ARRAY_REGION,
	GET_CHAR_ARRAY_REGION,
	GET_SHORT_ARRAY_REGION,
	GET_INT_ARRAY_REGION,
	GET_LONG_ARRAY_REGION,
	GET_FLOAT_ARRAY_REGION,
	GET_DOUBLE_ARRAY_REGION,
	SET_BOOLEAN_ARRAY_REGION,
	SET_BYTE_ARRAY_REGION,
	SET_CHAR_ARRAY_REGION,
	SET_SHORT_ARRAY_REGION,
	SET_INT_ARRAY_REGION,
	SET_LONG_ARRAY_REGION,
	SET_FLOAT_ARRAY_REGION,
	SET_DOUBLE_ARRAY_REGION,
	RegisterNatives,
	UnregisterNatives,
	MonitorEnter,
	MonitorExit,
	GetJavaVM,
	GetStringRegion,
	GetStringUTFRegion,
	GET_PRIMITIVE_ARRAY_CRITICAL,
	RELEASE_PRIMITIVE_ARRAY_CRITICAL,
	GET_STRING_CRITICAL,
	RELEASE_STRING_CRITICAL,
	NewWeakGlobalRef,
	DeleteWeakGlobalRef,
	ExceptionCheck,
	NewDirectByteBuffer,
	GetDirectBufferAddress,
	GetDirectBufferCapacity,
	GetObjectRefType,
#if JAVA_SPEC_VERSION >= 9
	GetModule,
#endif /* JAVA_SPEC_VERSION >= 9 */
};

static void initializeJNIEnv31(JNIEnv31 * jniEnv31, jlong jniEnv64);

jint JNICALL
GetVersion(JNIEnv *env)
{
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64 };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env) };
	jint returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetVersion);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jclass JNICALL
DefineClass(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize bufLen)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr, CEL4RO64_type_jobject, CEL4RO64_type_uint32_ptr, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint64_t)name, loader, (uint64_t) buf, (uint64_t) bufLen };
	jclass returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, DefineClass);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jclass, &returnValue);
	return returnValue;
}

jint JNICALL
ThrowNew(JNIEnv *env, jclass clazz, const char *message)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, (uint64_t)message };
	jint returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ThrowNew);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jthrowable JNICALL
ExceptionOccurred(JNIEnv *env)
{
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64 };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env) };
	jthrowable returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ExceptionOccurred);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jthrowable, &returnValue);
	return returnValue;
}

void JNICALL
ExceptionDescribe(JNIEnv *env)
{
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64 };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env) };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ExceptionDescribe);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
ExceptionClear(JNIEnv *env)
{
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64 };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env) };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ExceptionClear);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
FatalError(JNIEnv *env, const char *msg)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint64_t)msg };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, FatalError);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jint JNICALL
PushLocalFrame(JNIEnv *env, jint capacity)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), capacity };
	jint returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, PushLocalFrame);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jobject JNICALL
PopLocalFrame(JNIEnv *env, jobject result)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), result };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, PopLocalFrame);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jobject JNICALL
NewGlobalRef(JNIEnv *env, jobject obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewGlobalRef);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

void JNICALL
DeleteGlobalRef(JNIEnv *env, jobject gref)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), gref };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, DeleteGlobalRef);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
DeleteLocalRef(JNIEnv *env, jobject localRef)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), localRef };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, DeleteLocalRef);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jobject JNICALL
NewLocalRef(JNIEnv *env, jobject ref)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), ref };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewLocalRef);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jint JNICALL
EnsureLocalCapacity(JNIEnv *env, jint capacity)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jint };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), capacity };
	jint returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, EnsureLocalCapacity);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

static jobject
NewObjectV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewObjectV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jobject JNICALL
NewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;

	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return NewObjectV_64(env, clazz, methodID, parms);
}

jobject JNICALL
NewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
	va_list_64 parms;

	long long value = *((int *)(((int *)(args))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return NewObjectV_64(env, clazz, methodID, parms);
}

jobject JNICALL
NewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewObjectA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jboolean JNICALL
IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz };
	jboolean returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, IsInstanceOf);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jmethodID JNICALL
GetMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_uint32_ptr, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, (uint64_t)name, (uint64_t)sig };
	jmethodID returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetMethodID);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jmethodID, &returnValue);
	return returnValue;
}

jfieldID JNICALL
GetFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_uint32_ptr, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, (uint64_t)name, (uint64_t)sig };
	jfieldID returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetFieldID);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfieldID, &returnValue);
	return returnValue;
}

jmethodID JNICALL
GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_uint32_ptr, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, (uint64_t)name, (uint64_t)sig };
	jmethodID returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticMethodID);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jmethodID, &returnValue);
	return returnValue;
}

jfieldID JNICALL
GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_uint32_ptr, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, (uint64_t)name, (uint64_t)sig };
	jfieldID returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticFieldID);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfieldID, &returnValue);
	return returnValue;
}

jbooleanArray JNICALL
NewBooleanArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jbooleanArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewBooleanArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jbyteArray JNICALL
NewByteArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jbyteArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewByteArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jcharArray JNICALL
NewCharArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jcharArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewCharArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jshortArray JNICALL
NewShortArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jshortArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewShortArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jintArray JNICALL
NewIntArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jintArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewIntArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jlongArray JNICALL
NewLongArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jlongArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewLongArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jfloatArray JNICALL
NewFloatArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jfloatArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewFloatArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jdoubleArray JNICALL
NewDoubleArray(JNIEnv *env, jsize length)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length };
	jdoubleArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewDoubleArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

jint JNICALL
MonitorEnter(JNIEnv *env, jobject obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jint returnValue = JNI_ERR;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, MonitorEnter);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jint JNICALL
MonitorExit(JNIEnv *env, jobject obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jint returnValue = JNI_ERR;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, MonitorExit);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jint JNICALL
GetJavaVM(JNIEnv *env, JavaVM **vm)
{
	if (NULL != ((JNIEnv31 *)env)->javaVM31) {
		/* Return cached instance of JavaVM31, if available. */
		*vm = (JavaVM *)((JNIEnv31 *)env)->javaVM31;
		return JNI_OK;
	}

	/* Query the JVM to get the 64-bit JavaVM pointer first, and we will return the corresponding 31-bit
	 * JavaVM instance
	 */
	uint64_t JavaVM64Result = 0;
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint64_t)&JavaVM64Result };
	jint returnValue = JNI_ERR;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetJavaVM);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	if (JNI_OK == returnValue) {
		/* Find or create a matching 31-bit JavaVM instance. */
		JavaVM31 *javaVM31 = getJavaVM31(JavaVM64Result);
		((JNIEnv31 *)env)->javaVM31 = javaVM31;
		*vm = (JavaVM *)javaVM31;
	}
	return returnValue;
}

jweak JNICALL
NewWeakGlobalRef(JNIEnv *env, jobject obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jweak returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewWeakGlobalRef);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jweak, &returnValue);
	return returnValue;
}

void JNICALL
DeleteWeakGlobalRef(JNIEnv *env, jweak obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jweak };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, DeleteWeakGlobalRef);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jboolean JNICALL
ExceptionCheck(JNIEnv *env)
{
	const jint NUM_ARGS = 1;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64 };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env) };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ExceptionCheck);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jobject JNICALL
NewDirectByteBuffer(JNIEnv *env, void *address, jlong capacity)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr, CEL4RO64_type_jlong};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint32_t)address, capacity};
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewDirectByteBuffer);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

void * JNICALL
GetDirectBufferAddress(JNIEnv *env, jobject buf)
{
	/* It is the user's responsibility to invoke GetDirectBufferAddress on DBB objects that are backed by memory
	 * below-the-bar in order for this to be accessible. However, we will check the final address and return NULL,
	 * which should fail more gracefully.
	 */
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), buf };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetDirectBufferAddress);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);

	if (0 != returnValue) {
		/* If the upper 33-bits are non-zero, then the backing memory is not accessible in AMODE31.
		 * Return NULL if that's the case.
		 */
		if (returnValue != (returnValue & 0x7FFFFFFF)) {
			return NULL;
		}
	}
	return (void *)returnValue;
}

jlong JNICALL
GetDirectBufferCapacity(JNIEnv *env, jobject buf)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), buf };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetDirectBufferCapacity);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

jobjectRefType JNICALL
GetObjectRefType(JNIEnv* env, jobject obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jobjectRefType returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetObjectRefType);
	/* Note: jobjectRefType is a 4-byte enum -- using CEL4RO64_type_jint as return type. */
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

#if JAVA_SPEC_VERSION >= 9
jobject JNICALL
GetModule(JNIEnv *env, jclass clazz)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetModule);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}
#endif /* JAVA_SPEC_VERSION >= 9 */

/**
 * Following set of functions are JNIEnv31 utility routines.
 */


JNIEnv31 * JNICALL
getJNIEnv31(uint64_t jniEnv64)
{
	/* Traverse list to find matching JavaVM31. */
	for (JNIEnv31 *curJNIEnv31 = globalRootJNIEnv31; curJNIEnv31 != NULL; curJNIEnv31 = curJNIEnv31->next) {
		if (jniEnv64 == curJNIEnv31->jniEnv64) {
			return curJNIEnv31;
		}
	}

	/* Create and initialize a new instance. */
	JNIEnv31 *newJNIEnv31 = (JNIEnv31 *)malloc(sizeof(JNIEnv31));
	if (NULL != newJNIEnv31) {
		initializeJNIEnv31(newJNIEnv31, jniEnv64);
	}
	return newJNIEnv31;
}

/**
 * Initialize new JNIEnv31 structure, with references to the corresponding 64-bit
 * JNIEnv, along with both 31-bit and 64-bit JNI function tables. Add to our
 * list of JNIEnv31.
 *
 * @param[in] javaVM31 The 31-bit JNIEnv* instance to initialize.
 * @param[in] javaVM64 The matching 64-bit JNIEnv* from JVM.
 */
static void
initializeJNIEnv31(JNIEnv31 * jniEnv31, jlong jniEnv64)
{
	jniEnv31->functions = (JNINativeInterface_ *)GLOBAL_TABLE(EsJNIFunctions);
	jniEnv31->jniEnv64 = jniEnv64;

	/* Look up the 64-bit JNIInterface_ function table if necessary. */
	if (NULL == global64BitJNIInterface_) {
		const jint NUM_ARGS = 1;
		J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64 };

		uint64_t argValues[NUM_ARGS] = { (uint64_t)jniEnv64 };
		uintptr_t returnValue = NULL;
		uint32_t cel4ro64ReturnCode = j9_cel4ro64_load_query_call_function(
				LIBJ9VM29_NAME, "J9VM_initializeJNI31Table",
				argTypes, argValues, NUM_ARGS, CEL4RO64_type_uint32_ptr, &returnValue);
		global64BitJNIInterface_ = (jlong *)returnValue;
	}
	jniEnv31->functions64 = global64BitJNIInterface_;
	jniEnv31->next = globalRootJNIEnv31;
	globalRootJNIEnv31 = jniEnv31;
}

void
j9vm31TrcJNIMethodEntry(JNIEnv* env, const char *functionName) {
	const char* isDebug = getenv("TR_3164Debug");
	if (isDebug) {
		fprintf(stderr, "j9vm31Trc - 31:[%p] 64:[%llx] invoking function: %s\n", env, JNIENV64_FROM_JNIENV31(env), functionName);
	}
}

} /* extern "C" */
