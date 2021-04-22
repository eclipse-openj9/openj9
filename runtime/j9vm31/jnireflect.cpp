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

jmethodID JNICALL
FromReflectedMethod(JNIEnv *env, jobject method)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), method };
	jmethodID returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, FromReflectedMethod);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jmethodID, &returnValue);
	return returnValue;
}

jfieldID JNICALL
FromReflectedField(JNIEnv *env, jobject field)
{
	const jint NUM_ARGS = 2;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jobject };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), field };
	jfieldID returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, FromReflectedField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jfieldID, &returnValue);
	return returnValue;
}

jobject JNICALL
ToReflectedMethod(JNIEnv *env, jclass cls, jmethodID methodID, jboolean isStatic)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_jmethodID, CEL4RO64_type_jboolean };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), cls, methodID, (uint64_t)isStatic };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ToReflectedMethod);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}

jobject JNICALL
ToReflectedField(JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic)
{
	const jint NUM_ARGS = 4;
	J9_CEL4RO64_ArgType argTypes[NUM_ARGS] = { CEL4RO64_type_JNIEnv64, CEL4RO64_type_jclass, CEL4RO64_type_jfieldID, CEL4RO64_type_jboolean };
	uint64_t argValues[NUM_ARGS] = { JNIENV64_FROM_JNIENV31(env), cls, fieldID, (uint64_t)isStatic };
	jobject returnValue = NULL;
	FUNCTION_DESCRIPTOR_FROM_JNIENV31(env, ToReflectedField);
	j9_cel4ro64_call_function(functionDescriptor, argTypes, argValues, NUM_ARGS, CEL4RO64_type_jobject, &returnValue);
	return returnValue;
}
