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

jclass JNICALL
FindClass(JNIEnv *env, const char *name)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint64_t)name};
	jclass returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, FindClass);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jclass, &returnValue);
	return returnValue;
}

jclass JNICALL
GetSuperclass(JNIEnv *env, jclass clazz)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz };
	jclass returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetSuperclass);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jclass, &returnValue);
	return returnValue;
}

jboolean JNICALL
IsAssignableFrom(JNIEnv *env, jclass clazz1, jclass clazz2)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_jclass };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz1, clazz2 };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, IsAssignableFrom);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jint JNICALL
Throw(JNIEnv *env, jthrowable obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jthrowable };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, Throw);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jboolean JNICALL
IsSameObject(JNIEnv *env, jobject ref1, jobject ref2)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), ref1, ref2 };
	jboolean returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, IsSameObject);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jobject JNICALL
AllocObject(JNIEnv *env, jclass clazz)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, AllocObject);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jclass JNICALL
GetObjectClass(JNIEnv *env, jobject obj)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj };
	jclass returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetObjectClass);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jclass, &returnValue);
	return returnValue;
}

jstring JNICALL
NewString(JNIEnv *env, const jchar *unicodeChars, jsize len)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint64_t)unicodeChars, len };
	jstring returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewString);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jstring, &returnValue);
	return returnValue;
}

jsize JNICALL
GetStringLength(JNIEnv *env, jstring string)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), string };
	jsize returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStringLength);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jsize, &returnValue);
	return returnValue;
}

const jchar *JNICALL
GetStringChars(JNIEnv *env, jstring string, jboolean *isCopy)
{
	/* This routine handles both GetStringChars and GetStringCritical, as the Java Heap is not accessible in AMODE31. */
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), string, (uint64_t)isCopy };
	const jchar *returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStringChars);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_uint32_ptr, &returnValue);
	return returnValue;
}

void JNICALL
ReleaseStringChars(JNIEnv *env, jstring string, const jchar *utf)
{
	/* This routine handles both GetStringChars and ReleaseStringCritical, GetStringCritical invokes GetStringChars. */
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), string, (uint64_t)utf };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ReleaseStringChars);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jstring JNICALL
NewStringUTF(JNIEnv *env, const char *bytes)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), (uint64_t)bytes };
	jstring returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewStringUTF);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jstring, &returnValue);
	return returnValue;
}

jsize JNICALL
GetStringUTFLength(JNIEnv *env, jstring string)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), string };
	jsize returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStringUTFLength);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jsize, &returnValue);
	return returnValue;
}

const char* JNICALL
GetStringUTFChars(JNIEnv *env, jstring string, jboolean *isCopy)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), string, (uint64_t)isCopy };
	const char* returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStringUTFChars);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_uint32_ptr, &returnValue);
	return returnValue;
}

void JNICALL
ReleaseStringUTFChars(JNIEnv *env, jstring string, const char* utf)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), string, (uint64_t)utf };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ReleaseStringUTFChars);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jsize JNICALL
GetArrayLength(JNIEnv *env, jarray array)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array };
	jsize returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetArrayLength);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jsize, &returnValue);
	return returnValue;
}

jobjectArray JNICALL
NewObjectArray(JNIEnv *env, jsize length, jclass clazz, jobject initialElement) {
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jsize, CEL4RO64_type_jclass, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), length, clazz, initialElement };
	jobjectArray returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, NewObjectArray);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jarray, &returnValue);
	return returnValue;
}

void* JNICALL
getArrayElements(JNIEnv *env, jarray array, jboolean *isCopy)
{
	/* This routine is used for all Get*ArrayElements and GetPrimitiveArrayCritical. The latter because
	 * the Java heap is not addressible in AMODE 31, as such, will always return a copy.
	 */
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array, (uint64_t)isCopy };
	void* returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetBooleanArrayElements);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_uint32_ptr, &returnValue);
	return returnValue;
}

void JNICALL
releaseArrayElements(JNIEnv *env, jarray array, void *elems, jint mode)
{
	/* This routine is used for all Release*ArrayElements and ReleasePrimitiveArrayCritical. The latter because
	 * GetPrimitiveArrayCritical actually calls getArrayElements, so we need to release the array elements
	 * the same way.
	 */
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray, CEL4RO64_type_uint32_ptr, CEL4RO64_type_jint };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array, (uint64_t) elems, mode };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ReleaseBooleanArrayElements);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
getArrayRegion(JNIEnv *env, jarray array, jsize start, jsize len, void *buf)
{
	/* This routine is used for all Get*ArrayRegion. */
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray, CEL4RO64_type_jsize, CEL4RO64_type_jsize, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array, start, len, (uint64_t)buf };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetBooleanArrayRegion);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
setArrayRegion(JNIEnv *env, jarray array, jsize start, jsize len, void *buf)
{
	/* This routine is used for all Get*ArrayRegion. */
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray, CEL4RO64_type_jsize, CEL4RO64_type_jsize, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array, start, len, (uint64_t)buf };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetBooleanArrayRegion);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

/**
 * A 64-bit representation of JNINativeMethod struct.
 */
typedef struct {
	void *namePadding;        /**< Padding for upper 32-bits of name member. */
	char *name;               /**< Method name. */
	void *signaturePadding;   /**< Padding for upper 32-bits of signature member. */
	char *signature;          /**< Method signature. */
	jint fnPtrPadding;        /**< Upper 32-bits of function pointer -- to be tagged HANDLE31_HIGHTAG. */
	void *fnPtr;              /**< Function pointer. */
} JNINativeMethod64;

jint JNICALL
RegisterNatives(JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods)
{
	JNINativeMethod64* methods64 = (JNINativeMethod64 *)malloc(nMethods * sizeof(JNINativeMethod64));
	if (NULL == methods64)
		return JNI_ERR;

	/* Zero initialize and copy the JNINativeMethod parameters, and tag the function pointers
	* with HANDLE31_HIGHTAG to signify they are from AMODE 31.
	*/
	memset(methods64, 0, nMethods * sizeof(JNINativeMethod64));

	for (int i = 0; i < nMethods; i++) {
		methods64[i].fnPtr = methods[i].fnPtr;
		methods64[i].name = methods[i].name;
		methods64[i].signature = methods[i].signature;
		methods64[i].fnPtrPadding = HANDLE31_HIGHTAG;
	}

	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_uint32_ptr, CEL4RO64_type_jint };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, (uint64_t)methods64, nMethods };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, RegisterNatives);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);

	free(methods64);
	return returnValue;
}

jint JNICALL
UnregisterNatives(JNIEnv *env, jclass clazz)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, UnregisterNatives);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

void JNICALL
GetStringRegion(JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring, CEL4RO64_type_jsize, CEL4RO64_type_jsize, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), str, start, len, (uint64_t)buf };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStringRegion);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
GetStringUTFRegion(JNIEnv *env, jstring str, jsize start, jsize len, char *buf)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jstring, CEL4RO64_type_jsize, CEL4RO64_type_jsize, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), str, start, len, (uint64_t)buf };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStringUTFRegion);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}
