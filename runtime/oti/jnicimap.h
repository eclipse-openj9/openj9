/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef JNICIMAP_H
#define JNICIMAP_H

#ifdef __cplusplus
extern "C" {
#endif


/* Prototypes for terminal functions */
void JNICALL callVirtualVoidMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...);
void JNICALL callVirtualVoidMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va);
void JNICALL callVirtualVoidMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args);
jobject JNICALL callVirtualObjectMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...);
jobject JNICALL callVirtualObjectMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va);
jobject JNICALL callVirtualObjectMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args);
jint JNICALL callVirtualIntMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...);
jint JNICALL callVirtualIntMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va);
jint JNICALL callVirtualIntMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args);
jlong JNICALL callVirtualLongMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...);
jlong JNICALL callVirtualLongMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va);
jlong JNICALL callVirtualLongMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args);
jfloat JNICALL callVirtualFloatMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...);
jfloat JNICALL callVirtualFloatMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va);
jfloat JNICALL callVirtualFloatMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args);
jdouble JNICALL callVirtualDoubleMethod (JNIEnv *env, jobject receiver, jmethodID methodID, ...);
jdouble JNICALL callVirtualDoubleMethodV (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va);
jdouble JNICALL callVirtualDoubleMethodA (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args);
void JNICALL callNonvirtualVoidMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...);
void JNICALL callNonvirtualVoidMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va);
void JNICALL callNonvirtualVoidMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args);
jobject JNICALL callNonvirtualObjectMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...);
jobject JNICALL callNonvirtualObjectMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va);
jobject JNICALL callNonvirtualObjectMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args);
jint JNICALL callNonvirtualIntMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...);
jint JNICALL callNonvirtualIntMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va);
jint JNICALL callNonvirtualIntMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args);
jlong JNICALL callNonvirtualLongMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...);
jlong JNICALL callNonvirtualLongMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va);
jlong JNICALL callNonvirtualLongMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args);
jfloat JNICALL callNonvirtualFloatMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...);
jfloat JNICALL callNonvirtualFloatMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va);
jfloat JNICALL callNonvirtualFloatMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args);
jdouble JNICALL callNonvirtualDoubleMethod (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...);
jdouble JNICALL callNonvirtualDoubleMethodV (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va);
jdouble JNICALL callNonvirtualDoubleMethodA (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args);
void JNICALL callStaticVoidMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...);
void JNICALL callStaticVoidMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va);
void JNICALL callStaticVoidMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args);
jobject JNICALL callStaticObjectMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...);
jobject JNICALL callStaticObjectMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va);
jobject JNICALL callStaticObjectMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args);
jint JNICALL callStaticIntMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...);
jint JNICALL callStaticIntMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va);
jint JNICALL callStaticIntMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args);
jlong JNICALL callStaticLongMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...);
jlong JNICALL callStaticLongMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va);
jlong JNICALL callStaticLongMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args);
jfloat JNICALL callStaticFloatMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...);
jfloat JNICALL callStaticFloatMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va);
jfloat JNICALL callStaticFloatMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args);
jdouble JNICALL callStaticDoubleMethod (JNIEnv *env, jclass cls, jmethodID methodID, ...);
jdouble JNICALL callStaticDoubleMethodV (JNIEnv *env, jclass cls, jmethodID methodID, va_list va);
jdouble JNICALL callStaticDoubleMethodA (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args);

/* Macros to populate tables */
#define CALL_VIRTUAL_VOID_METHOD callVirtualVoidMethod
#define CALL_VIRTUAL_VOID_METHOD_V callVirtualVoidMethodV
#define CALL_VIRTUAL_VOID_METHOD_A callVirtualVoidMethodA
#define CALL_VIRTUAL_OBJECT_METHOD callVirtualObjectMethod
#define CALL_VIRTUAL_OBJECT_METHOD_V callVirtualObjectMethodV
#define CALL_VIRTUAL_OBJECT_METHOD_A callVirtualObjectMethodA
#define CALL_VIRTUAL_INT_METHOD callVirtualIntMethod
#define CALL_VIRTUAL_INT_METHOD_V callVirtualIntMethodV
#define CALL_VIRTUAL_INT_METHOD_A callVirtualIntMethodA
#define CALL_VIRTUAL_LONG_METHOD callVirtualLongMethod
#define CALL_VIRTUAL_LONG_METHOD_V callVirtualLongMethodV
#define CALL_VIRTUAL_LONG_METHOD_A callVirtualLongMethodA
#define CALL_VIRTUAL_FLOAT_METHOD callVirtualFloatMethod
#define CALL_VIRTUAL_FLOAT_METHOD_V callVirtualFloatMethodV
#define CALL_VIRTUAL_FLOAT_METHOD_A callVirtualFloatMethodA
#define CALL_VIRTUAL_DOUBLE_METHOD callVirtualDoubleMethod
#define CALL_VIRTUAL_DOUBLE_METHOD_V callVirtualDoubleMethodV
#define CALL_VIRTUAL_DOUBLE_METHOD_A callVirtualDoubleMethodA
#define CALL_VIRTUAL_BOOLEAN_METHOD (jboolean (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, ...)) callVirtualIntMethod
#define CALL_VIRTUAL_BOOLEAN_METHOD_V (jboolean (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)) callVirtualIntMethodV
#define CALL_VIRTUAL_BOOLEAN_METHOD_A (jboolean (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)) callVirtualIntMethodA
#define CALL_VIRTUAL_BYTE_METHOD (jbyte (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, ...)) callVirtualIntMethod
#define CALL_VIRTUAL_BYTE_METHOD_V (jbyte (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)) callVirtualIntMethodV
#define CALL_VIRTUAL_BYTE_METHOD_A (jbyte (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)) callVirtualIntMethodA
#define CALL_VIRTUAL_CHAR_METHOD (jchar (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, ...)) callVirtualIntMethod
#define CALL_VIRTUAL_CHAR_METHOD_V (jchar (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)) callVirtualIntMethodV
#define CALL_VIRTUAL_CHAR_METHOD_A (jchar (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)) callVirtualIntMethodA
#define CALL_VIRTUAL_SHORT_METHOD (jshort (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, ...)) callVirtualIntMethod
#define CALL_VIRTUAL_SHORT_METHOD_V (jshort (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, va_list va)) callVirtualIntMethodV
#define CALL_VIRTUAL_SHORT_METHOD_A (jshort (JNICALL *) (JNIEnv *env, jobject receiver, jmethodID methodID, jvalue *args)) callVirtualIntMethodA
#define CALL_NONVIRTUAL_VOID_METHOD callNonvirtualVoidMethod
#define CALL_NONVIRTUAL_VOID_METHOD_V callNonvirtualVoidMethodV
#define CALL_NONVIRTUAL_VOID_METHOD_A callNonvirtualVoidMethodA
#define CALL_NONVIRTUAL_OBJECT_METHOD callNonvirtualObjectMethod
#define CALL_NONVIRTUAL_OBJECT_METHOD_V callNonvirtualObjectMethodV
#define CALL_NONVIRTUAL_OBJECT_METHOD_A callNonvirtualObjectMethodA
#define CALL_NONVIRTUAL_INT_METHOD callNonvirtualIntMethod
#define CALL_NONVIRTUAL_INT_METHOD_V callNonvirtualIntMethodV
#define CALL_NONVIRTUAL_INT_METHOD_A callNonvirtualIntMethodA
#define CALL_NONVIRTUAL_LONG_METHOD callNonvirtualLongMethod
#define CALL_NONVIRTUAL_LONG_METHOD_V callNonvirtualLongMethodV
#define CALL_NONVIRTUAL_LONG_METHOD_A callNonvirtualLongMethodA
#define CALL_NONVIRTUAL_FLOAT_METHOD callNonvirtualFloatMethod
#define CALL_NONVIRTUAL_FLOAT_METHOD_V callNonvirtualFloatMethodV
#define CALL_NONVIRTUAL_FLOAT_METHOD_A callNonvirtualFloatMethodA
#define CALL_NONVIRTUAL_DOUBLE_METHOD callNonvirtualDoubleMethod
#define CALL_NONVIRTUAL_DOUBLE_METHOD_V callNonvirtualDoubleMethodV
#define CALL_NONVIRTUAL_DOUBLE_METHOD_A callNonvirtualDoubleMethodA
#define CALL_NONVIRTUAL_BOOLEAN_METHOD (jboolean (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)) callNonvirtualIntMethod
#define CALL_NONVIRTUAL_BOOLEAN_METHOD_V (jboolean (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)) callNonvirtualIntMethodV
#define CALL_NONVIRTUAL_BOOLEAN_METHOD_A (jboolean (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)) callNonvirtualIntMethodA
#define CALL_NONVIRTUAL_BYTE_METHOD (jbyte (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)) callNonvirtualIntMethod
#define CALL_NONVIRTUAL_BYTE_METHOD_V (jbyte (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)) callNonvirtualIntMethodV
#define CALL_NONVIRTUAL_BYTE_METHOD_A (jbyte (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)) callNonvirtualIntMethodA
#define CALL_NONVIRTUAL_CHAR_METHOD (jchar (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)) callNonvirtualIntMethod
#define CALL_NONVIRTUAL_CHAR_METHOD_V (jchar (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)) callNonvirtualIntMethodV
#define CALL_NONVIRTUAL_CHAR_METHOD_A (jchar (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)) callNonvirtualIntMethodA
#define CALL_NONVIRTUAL_SHORT_METHOD (jshort (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, ...)) callNonvirtualIntMethod
#define CALL_NONVIRTUAL_SHORT_METHOD_V (jshort (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, va_list va)) callNonvirtualIntMethodV
#define CALL_NONVIRTUAL_SHORT_METHOD_A (jshort (JNICALL *) (JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, jvalue *args)) callNonvirtualIntMethodA
#define CALL_STATIC_VOID_METHOD callStaticVoidMethod
#define CALL_STATIC_VOID_METHOD_V callStaticVoidMethodV
#define CALL_STATIC_VOID_METHOD_A callStaticVoidMethodA
#define CALL_STATIC_OBJECT_METHOD callStaticObjectMethod
#define CALL_STATIC_OBJECT_METHOD_V callStaticObjectMethodV
#define CALL_STATIC_OBJECT_METHOD_A callStaticObjectMethodA
#define CALL_STATIC_INT_METHOD callStaticIntMethod
#define CALL_STATIC_INT_METHOD_V callStaticIntMethodV
#define CALL_STATIC_INT_METHOD_A callStaticIntMethodA
#define CALL_STATIC_LONG_METHOD callStaticLongMethod
#define CALL_STATIC_LONG_METHOD_V callStaticLongMethodV
#define CALL_STATIC_LONG_METHOD_A callStaticLongMethodA
#define CALL_STATIC_FLOAT_METHOD callStaticFloatMethod
#define CALL_STATIC_FLOAT_METHOD_V callStaticFloatMethodV
#define CALL_STATIC_FLOAT_METHOD_A callStaticFloatMethodA
#define CALL_STATIC_DOUBLE_METHOD callStaticDoubleMethod
#define CALL_STATIC_DOUBLE_METHOD_V callStaticDoubleMethodV
#define CALL_STATIC_DOUBLE_METHOD_A callStaticDoubleMethodA
#define CALL_STATIC_BOOLEAN_METHOD (jboolean (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, ...)) callStaticIntMethod
#define CALL_STATIC_BOOLEAN_METHOD_V (jboolean (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)) callStaticIntMethodV
#define CALL_STATIC_BOOLEAN_METHOD_A (jboolean (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)) callStaticIntMethodA
#define CALL_STATIC_BYTE_METHOD (jbyte (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, ...)) callStaticIntMethod
#define CALL_STATIC_BYTE_METHOD_V (jbyte (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)) callStaticIntMethodV
#define CALL_STATIC_BYTE_METHOD_A (jbyte (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)) callStaticIntMethodA
#define CALL_STATIC_CHAR_METHOD (jchar (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, ...)) callStaticIntMethod
#define CALL_STATIC_CHAR_METHOD_V (jchar (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)) callStaticIntMethodV
#define CALL_STATIC_CHAR_METHOD_A (jchar (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)) callStaticIntMethodA
#define CALL_STATIC_SHORT_METHOD (jshort (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, ...)) callStaticIntMethod
#define CALL_STATIC_SHORT_METHOD_V (jshort (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, va_list va)) callStaticIntMethodV
#define CALL_STATIC_SHORT_METHOD_A (jshort (JNICALL *) (JNIEnv *env, jclass cls, jmethodID methodID, jvalue *args)) callStaticIntMethodA
#ifdef __cplusplus
}
#endif

#endif /* JNICIMAP_H */
