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

jobject JNICALL
GetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetObjectField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jboolean JNICALL
GetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jboolean returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetBooleanField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jbyte JNICALL
GetByteField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jbyte returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetByteField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

jchar JNICALL
GetCharField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jchar returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetCharField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

jshort JNICALL
GetShortField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jshort returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetShortField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

jint JNICALL
GetIntField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jint returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetIntField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jlong JNICALL
GetLongField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jlong returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetLongField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

jfloat JNICALL
GetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jfloat returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetFloatField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

jdouble JNICALL
GetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID };
	jdouble returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetDoubleField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

void JNICALL
SetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jobject};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetObjectField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jboolean};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetBooleanField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jbyte};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetByteField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jchar};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetCharField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jshort};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetShortField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jint};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetIntField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jlong};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetLongField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jfloat};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, 0 };

	/* Floating point parameters requires special handling */
	jfloat *floatArgPtr = (jfloat *)(&(argValues[3]));
	*floatArgPtr = value;

	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetFloatField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jdouble};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), obj, fieldID, 0 };

	/* Floating point parameters requires special handling */
	jdouble *doubleArgPtr = (jdouble *)(&(argValues[3]));
	*doubleArgPtr = value;

	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetDoubleField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jobject JNICALL
GetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticObjectField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jboolean JNICALL
GetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jboolean returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticBooleanField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jboolean, &returnValue);
	return returnValue;
}

jbyte JNICALL
GetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jbyte returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticByteField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jbyte, &returnValue);
	return returnValue;
}

jchar JNICALL
GetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jchar returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticCharField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jchar, &returnValue);
	return returnValue;
}

jshort JNICALL
GetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jshort returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticShortField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jshort, &returnValue);
	return returnValue;
}

jint JNICALL
GetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jint returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticIntField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jint, &returnValue);
	return returnValue;
}

jlong JNICALL
GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jlong returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticLongField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jlong, &returnValue);
	return returnValue;
}

jfloat JNICALL
GetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jfloat returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticFloatField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfloat, &returnValue);
	return returnValue;
}

jdouble JNICALL
GetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID };
	jdouble returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetStaticDoubleField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jdouble, &returnValue);
	return returnValue;
}

void JNICALL
SetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jobject};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticObjectField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jboolean};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticBooleanField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID, jbyte value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jbyte};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticByteField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID, jchar value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jchar};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticCharField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID, jshort value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jshort};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticShortField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID, jint value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jint};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticIntField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID, jlong value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jlong};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticLongField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID, jfloat value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jfloat};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, 0 };
	/* Floating point parameters requires special handling */
	jfloat *floatArgPtr = (jfloat *)(&(argValues[3]));
	*floatArgPtr = value;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticFloatField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

void JNICALL
SetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID, jdouble value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject, CEL4RO64_type_jfieldID, CEL4RO64_type_jdouble};
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), clazz, fieldID, 0 };

	/* Floating point parameters requires special handling */
	jdouble *doubleArgPtr = (jdouble *)(&(argValues[3]));
	*doubleArgPtr = value;

	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetStaticDoubleField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}

jobject JNICALL
GetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index)
{
	const jint NUM_ARGS = 3;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray, CEL4RO64_type_jsize };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array, index };
	jobject returnValue;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, GetObjectArrayElement);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

void JNICALL
SetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject value)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jarray, CEL4RO64_type_jsize, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), array, index , value };
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, SetObjectArrayElement);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_void, NULL);
	return;
}
