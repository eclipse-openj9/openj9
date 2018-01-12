/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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



#include "jnicsup.h"
#include "j9consts.h"
#include "vmaccess.h"

extern const struct JNINativeInterface_ EsJNIFunctions;

#define RUN_CALLIN_METHOD(env, receiver, cls, methodID, args) \
		RUN_CALLIN_METHOD_HELPER(env, receiver, cls, methodID, args)

#define RUN_CALLIN_METHOD_HELPER(env, receiver, cls, methodID, args) \
		gpCheckCallin(env, receiver, cls, methodID, args)

void JNICALL callVirtualVoidMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	va_end(va);
	return;
}

void JNICALL callVirtualVoidMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	return;
}

void JNICALL callVirtualVoidMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, (U_8*)args + 1);
	return;
}

jobject JNICALL callVirtualObjectMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	va_end(va);
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jobject JNICALL callVirtualObjectMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jobject JNICALL callVirtualObjectMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, (U_8*)args + 1);
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jint JNICALL callVirtualIntMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	va_end(va);
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jint JNICALL callVirtualIntMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jint JNICALL callVirtualIntMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, (U_8*)args + 1);
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jlong JNICALL callVirtualLongMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	va_end(va);
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jlong JNICALL callVirtualLongMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jlong JNICALL callVirtualLongMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, (U_8*)args + 1);
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jfloat JNICALL callVirtualFloatMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	va_end(va);
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jfloat JNICALL callVirtualFloatMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jfloat JNICALL callVirtualFloatMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, (U_8*)args + 1);
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jdouble JNICALL callVirtualDoubleMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	va_end(va);
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

jdouble JNICALL callVirtualDoubleMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, VA_PTR(va));
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

jdouble JNICALL callVirtualDoubleMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, NULL, methodID, (U_8*)args + 1);
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

void JNICALL callNonvirtualVoidMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	va_end(va);
	return;
}

void JNICALL callNonvirtualVoidMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	return;
}

void JNICALL callNonvirtualVoidMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, (U_8*)args + 1);
	return;
}

jobject JNICALL callNonvirtualObjectMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jobject JNICALL callNonvirtualObjectMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jobject JNICALL callNonvirtualObjectMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, (U_8*)args + 1);
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jint JNICALL callNonvirtualIntMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jint JNICALL callNonvirtualIntMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jint JNICALL callNonvirtualIntMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, (U_8*)args + 1);
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jlong JNICALL callNonvirtualLongMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jlong JNICALL callNonvirtualLongMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jlong JNICALL callNonvirtualLongMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, (U_8*)args + 1);
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jfloat JNICALL callNonvirtualFloatMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jfloat JNICALL callNonvirtualFloatMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jfloat JNICALL callNonvirtualFloatMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, (U_8*)args + 1);
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jdouble JNICALL callNonvirtualDoubleMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

jdouble JNICALL callNonvirtualDoubleMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, VA_PTR(va));
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

jdouble JNICALL callNonvirtualDoubleMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, receiver, cls, methodID, (U_8*)args + 1);
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

void JNICALL callStaticVoidMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	va_end(va);
	return;
}

void JNICALL callStaticVoidMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	return;
}

void JNICALL callStaticVoidMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, (U_8*)args + 1);
	return;
}

jobject JNICALL callStaticObjectMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jobject JNICALL callStaticObjectMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jobject JNICALL callStaticObjectMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, (U_8*)args + 1);
	return *(jobject*)(&((J9VMThread*)env)->returnValue);
}

jint JNICALL callStaticIntMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jint JNICALL callStaticIntMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jint JNICALL callStaticIntMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, (U_8*)args + 1);
	return *(jint*)(&(((J9VMThread*)env)->returnValue));
}

jlong JNICALL callStaticLongMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jlong JNICALL callStaticLongMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jlong JNICALL callStaticLongMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, (U_8*)args + 1);
	return *(jlong*)(&((J9VMThread*)env)->returnValue);
}

jfloat JNICALL callStaticFloatMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jfloat JNICALL callStaticFloatMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jfloat JNICALL callStaticFloatMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, (U_8*)args + 1);
	return *(jfloat*)(&(((J9VMThread*)env)->returnValue));
}

jdouble JNICALL callStaticDoubleMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
	va_list va;
	va_start(va, methodID);
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	va_end(va);
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

jdouble JNICALL callStaticDoubleMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, VA_PTR(va));
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

jdouble JNICALL callStaticDoubleMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)
{
	RUN_CALLIN_METHOD(env, NULL, cls, methodID, (U_8*)args + 1);
	return *(jdouble*)(&((J9VMThread*)env)->returnValue);
}

