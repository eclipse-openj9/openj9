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

static jobject
CallObjectMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallObjectMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jobject JNICALL
CallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallObjectMethodV_64(env, obj, methodID, parms);
}

jobject JNICALL
CallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallObjectMethodV_64(env, obj, methodID, parms);
}

jobject JNICALL
CallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallObjectMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

static jboolean
CallBooleanMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallBooleanMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jboolean JNICALL
CallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallBooleanMethodV_64(env, obj, methodID, parms);
}

jboolean JNICALL
CallBooleanMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallBooleanMethodV_64(env, obj, methodID, parms);
}

jboolean JNICALL
CallBooleanMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallBooleanMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

static jbyte
CallByteMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jbyte returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallByteMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

jbyte JNICALL
CallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallByteMethodV_64(env, obj, methodID, parms);
}

jbyte JNICALL
CallByteMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallByteMethodV_64(env, obj, methodID, parms);
}

jbyte JNICALL
CallByteMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jbyte returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallByteMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

static jchar
CallCharMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jchar returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallCharMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

jchar JNICALL
CallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallCharMethodV_64(env, obj, methodID, parms);
}

jchar JNICALL
CallCharMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallCharMethodV_64(env, obj, methodID, parms);
}

jchar JNICALL
CallCharMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jchar returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallCharMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

static jshort
CallShortMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jshort returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallShortMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

jshort JNICALL
CallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallShortMethodV_64(env, obj, methodID, parms);
}

jshort JNICALL
CallShortMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallShortMethodV_64(env, obj, methodID, parms);
}

jshort JNICALL
CallShortMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jshort returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallShortMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

static jint
CallIntMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallIntMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jint JNICALL
CallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallIntMethodV_64(env, obj, methodID, parms);
}

jint JNICALL
CallIntMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallIntMethodV_64(env, obj, methodID, parms);
}

jint JNICALL
CallIntMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallIntMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

static jlong
CallLongMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallLongMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

jlong JNICALL
CallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallLongMethodV_64(env, obj, methodID, parms);
}

jlong JNICALL
CallLongMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallLongMethodV_64(env, obj, methodID, parms);
}

jlong JNICALL
CallLongMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallLongMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

static jfloat
CallFloatMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jfloat returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallFloatMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

jfloat JNICALL
CallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallFloatMethodV_64(env, obj, methodID, parms);
}

jfloat JNICALL
CallFloatMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallFloatMethodV_64(env, obj, methodID, parms);
}

jfloat JNICALL
CallFloatMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jfloat returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallFloatMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

static jdouble
CallDoubleMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	jdouble returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallDoubleMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

jdouble JNICALL
CallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallDoubleMethodV_64(env, obj, methodID, parms);
}

jdouble JNICALL
CallDoubleMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallDoubleMethodV_64(env, obj, methodID, parms);
}

jdouble JNICALL
CallDoubleMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };
	jdouble returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallDoubleMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

static void
CallVoidMethodV_64(JNIEnv *env, jobject obj, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)parms };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallVoidMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	CallVoidMethodV_64(env, obj, methodID, parms);
	return;
}

void JNICALL
CallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	CallVoidMethodV_64(env, obj, methodID, parms);
	return;
}

void JNICALL
CallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, methodID, (uint64_t)args };

	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallVoidMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

static jobject
CallNonvirtualObjectMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualObjectMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jobject JNICALL
CallNonvirtualObjectMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualObjectMethodV_64(env, obj, clazz, methodID, parms);
}

jobject JNICALL
CallNonvirtualObjectMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualObjectMethodV_64(env, obj, clazz, methodID, parms);
}

jobject JNICALL
CallNonvirtualObjectMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualObjectMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

static jboolean
CallNonvirtualBooleanMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualBooleanMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jboolean JNICALL
CallNonvirtualBooleanMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualBooleanMethodV_64(env, obj, clazz, methodID, parms);
}

jboolean JNICALL
CallNonvirtualBooleanMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualBooleanMethodV_64(env, obj, clazz, methodID, parms);
}

jboolean JNICALL
CallNonvirtualBooleanMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualBooleanMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

static jbyte
CallNonvirtualByteMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jbyte returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualByteMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

jbyte JNICALL
CallNonvirtualByteMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualByteMethodV_64(env, obj, clazz, methodID, parms);
}

jbyte JNICALL
CallNonvirtualByteMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualByteMethodV_64(env, obj, clazz, methodID, parms);
}

jbyte JNICALL
CallNonvirtualByteMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jbyte returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualByteMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

static jchar
CallNonvirtualCharMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jchar returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualCharMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

jchar JNICALL
CallNonvirtualCharMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualCharMethodV_64(env, obj, clazz, methodID, parms);
}

jchar JNICALL
CallNonvirtualCharMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualCharMethodV_64(env, obj, clazz, methodID, parms);
}

jchar JNICALL
CallNonvirtualCharMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jchar returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualCharMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

static jshort
CallNonvirtualShortMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jshort returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualShortMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

jshort JNICALL
CallNonvirtualShortMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualShortMethodV_64(env, obj, clazz, methodID, parms);
}

jshort JNICALL
CallNonvirtualShortMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualShortMethodV_64(env, obj, clazz, methodID, parms);
}

jshort JNICALL
CallNonvirtualShortMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jshort returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualShortMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

static jint
CallNonvirtualIntMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualIntMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jint JNICALL
CallNonvirtualIntMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualIntMethodV_64(env, obj, clazz, methodID, parms);
}

jint JNICALL
CallNonvirtualIntMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualIntMethodV_64(env, obj, clazz, methodID, parms);
}

jint JNICALL
CallNonvirtualIntMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualIntMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

static jlong
CallNonvirtualLongMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualLongMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

jlong JNICALL
CallNonvirtualLongMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualLongMethodV_64(env, obj, clazz, methodID, parms);
}

jlong JNICALL
CallNonvirtualLongMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualLongMethodV_64(env, obj, clazz, methodID, parms);
}

jlong JNICALL
CallNonvirtualLongMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualLongMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

static jfloat
CallNonvirtualFloatMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jfloat returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualFloatMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

jfloat JNICALL
CallNonvirtualFloatMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualFloatMethodV_64(env, obj, clazz, methodID, parms);
}

jfloat JNICALL
CallNonvirtualFloatMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualFloatMethodV_64(env, obj, clazz, methodID, parms);
}

jfloat JNICALL
CallNonvirtualFloatMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jfloat returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualFloatMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

static jdouble
CallNonvirtualDoubleMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	jdouble returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualDoubleMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

jdouble JNICALL
CallNonvirtualDoubleMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualDoubleMethodV_64(env, obj, clazz, methodID, parms);
}

jdouble JNICALL
CallNonvirtualDoubleMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallNonvirtualDoubleMethodV_64(env, obj, clazz, methodID, parms);
}

jdouble JNICALL
CallNonvirtualDoubleMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };
	jdouble returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualDoubleMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

static void
CallNonvirtualVoidMethodV_64(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)parms };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualVoidMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
CallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	CallNonvirtualVoidMethodV_64(env, obj, clazz, methodID, parms);
}

void JNICALL
CallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	CallNonvirtualVoidMethodV_64(env, obj, clazz, methodID, parms);
}

void JNICALL
CallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 5;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, clazz, methodID, (uint64_t)args };

	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallNonvirtualVoidMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

static jobject
CallStaticObjectMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticObjectMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jobject JNICALL
CallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticObjectMethodV_64(env, clazz, methodID, parms);
}

jobject JNICALL
CallStaticObjectMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticObjectMethodV_64(env, clazz, methodID, parms);
}

jobject JNICALL
CallStaticObjectMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticObjectMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

static jboolean
CallStaticBooleanMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticBooleanMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jboolean JNICALL
CallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticBooleanMethodV_64(env, clazz, methodID, parms);
}

jboolean JNICALL
CallStaticBooleanMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticBooleanMethodV_64(env, clazz, methodID, parms);
}

jboolean JNICALL
CallStaticBooleanMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jboolean returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticBooleanMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

static jbyte
CallStaticByteMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jbyte returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticByteMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

jbyte JNICALL
CallStaticByteMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticByteMethodV_64(env, clazz, methodID, parms);
}

jbyte JNICALL
CallStaticByteMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticByteMethodV_64(env, clazz, methodID, parms);
}

jbyte JNICALL
CallStaticByteMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jbyte returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticByteMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

static jchar
CallStaticCharMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jchar returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticCharMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

jchar JNICALL
CallStaticCharMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticCharMethodV_64(env, clazz, methodID, parms);
}

jchar JNICALL
CallStaticCharMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticCharMethodV_64(env, clazz, methodID, parms);
}

jchar JNICALL
CallStaticCharMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jchar returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticCharMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

static jshort
CallStaticShortMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jshort returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticShortMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

jshort JNICALL
CallStaticShortMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticShortMethodV_64(env, clazz, methodID, parms);
}

jshort JNICALL
CallStaticShortMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticShortMethodV_64(env, clazz, methodID, parms);
}

jshort JNICALL
CallStaticShortMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jshort returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticShortMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

static jint
CallStaticIntMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticIntMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jint JNICALL
CallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticIntMethodV_64(env, clazz, methodID, parms);
}

jint JNICALL
CallStaticIntMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticIntMethodV_64(env, clazz, methodID, parms);
}

jint JNICALL
CallStaticIntMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jint returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticIntMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

static jlong
CallStaticLongMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticLongMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

jlong JNICALL
CallStaticLongMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticLongMethodV_64(env, clazz, methodID, parms);
}

jlong JNICALL
CallStaticLongMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticLongMethodV_64(env, clazz, methodID, parms);
}

jlong JNICALL
CallStaticLongMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jlong returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticLongMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

static jfloat
CallStaticFloatMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jfloat returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticFloatMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

jfloat JNICALL
CallStaticFloatMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticFloatMethodV_64(env, clazz, methodID, parms);
}

jfloat JNICALL
CallStaticFloatMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticFloatMethodV_64(env, clazz, methodID, parms);
}

jfloat JNICALL
CallStaticFloatMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jfloat returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticFloatMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

static jdouble
CallStaticDoubleMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	jdouble returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticDoubleMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

jdouble JNICALL
CallStaticDoubleMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticDoubleMethodV_64(env, clazz, methodID, parms);
}

jdouble JNICALL
CallStaticDoubleMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	return CallStaticDoubleMethodV_64(env, clazz, methodID, parms);
}

jdouble JNICALL
CallStaticDoubleMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };
	jdouble returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticDoubleMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

static void
CallStaticVoidMethodV_64(JNIEnv *env, jclass clazz, jmethodID methodID, va_list_64 parms)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)parms };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticVoidMethodV);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
CallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	va_list_64 parms;
	long long value = *((int *)((char*)&(methodID) + sizeof(jmethodID)));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	CallStaticVoidMethodV_64(env, clazz, methodID, parms);
}

void JNICALL
CallStaticVoidMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list arg)
{
	va_list_64 parms;
	long long value = *((int *)(((int *)(arg))[1]));
	(void)( (parms)[0] = 0, (parms)[1] =  (char *)&(methodID), (parms)[2] = 0, (parms)[3] = (parms)[1] + sizeof(jmethodID) );

	CallStaticVoidMethodV_64(env, clazz, methodID, parms);
}

void JNICALL
CallStaticVoidMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jmethodID, CEL4RO64_type_uint32_ptr };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, methodID, (uint64_t)args };

	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, CallStaticVoidMethodA);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}
