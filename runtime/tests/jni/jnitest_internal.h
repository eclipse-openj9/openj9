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

#ifndef jnitest_internal_h
#define jnitest_internal_h

/**
* @file jnitest_internal.h
* @brief Internal prototypes used within the JNITEST module.
*
* This file contains implementation-private function prototypes and
* type definitions for the JNITEST module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "jnitest_api.h"
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief
* @param *env
* @param clazz
* @return jboolean
*/
jboolean JNICALL
Java_j9vm_test_jni_GetObjectRefTypeTest_getObjectRefTypeTest(JNIEnv *env, jclass clazz, jobject stackArg);

/**
* @brief
* @param *env
* @param clazz
* @return jint
*/
jint JNICALL
Java_jvmti_test_nativeMethodPrefixes_UnwrappedNative_nat(JNIEnv *env, jclass clazz);

/**
* @brief
* @param *env
* @param clazz
* @return jint
*/
jint JNICALL
Java_jvmti_test_nativeMethodPrefixes_DirectNative_gac4gac3gac2gac1nat(JNIEnv *env, jclass clazz);

/**
* @brief
* @param *env
* @param clazz
* @return jint
*/
jint JNICALL
Java_jvmti_test_nativeMethodPrefixes_WrappedNative_nat(JNIEnv *env, jclass clazz);

/* ---------------- memoryCheckTest.c ---------------- */
void JNICALL Java_j9vm_test_memchk_NoFree_test(JNIEnv * env, jclass clazz);

void JNICALL Java_j9vm_test_memchk_BlockOverrun_test(JNIEnv * env, jclass clazz);

void JNICALL Java_j9vm_test_memchk_BlockUnderrun_test(JNIEnv * env, jclass clazz);

void JNICALL Java_j9vm_test_memchk_Generic_test(JNIEnv * env, jclass clazz);


/* ---------------- jnierrors.c ---------------- */
void JNICALL Java_j9vm_test_jnichk_BufferOverrun_test(JNIEnv * env, jclass clazz);

void JNICALL Java_j9vm_test_jnichk_ModifiedBuffer_test(JNIEnv* env, jclass clazz, jint phase);

void JNICALL Java_j9vm_test_jnichk_DeleteGlobalRefTwice_test(JNIEnv* env, jclass clazz);

jboolean JNICALL Java_j9vm_test_jnichk_ModifyArrayData_test(JNIEnv* env, jclass clazz, jobject array, jchar type, jint mod1, jint mod2, jint mode);

void JNICALL Java_j9vm_test_jnichk_ConcurrentGlobalReferenceModification_test(JNIEnv * env, jclass clazz);

jboolean JNICALL Java_j9vm_test_jnichk_CriticalAlwaysCopy_testArray(JNIEnv* env, jclass clazz, jobjectArray array);
jboolean JNICALL Java_j9vm_test_jnichk_CriticalAlwaysCopy_testString(JNIEnv* env, jclass clazz, jstring str);

/* ---------------- montest.c ---------------- */

/**
* @brief
* @param *env
* @param clazz
* @return jint
*/
jint JNICALL
Java_j9vm_test_monitor_Helpers_getLastReturnCode(JNIEnv * env, jclass clazz);

/**
* @brief
* @param *env
* @param clazz
* @param obj
* @return jint
*/
jint JNICALL 
Java_j9vm_test_monitor_Helpers_monitorEnter(JNIEnv * env, jclass clazz, jobject obj);

/**
* @brief
* @param *env
* @param clazz
* @param obj
* @return jint
*/
jint JNICALL 
Java_j9vm_test_monitor_Helpers_monitorExit(JNIEnv * env, jclass clazz, jobject obj);

/**
* @brief
* @param *env
* @param clazz
* @param obj
* @param throwable
* @return jint
*/
jint JNICALL
Java_j9vm_test_monitor_Helpers_monitorExitWithException(JNIEnv * env, jclass clazz, jobject obj, jobject throwable);

/**
* @brief
* @param *env
* @param clazz
* @param obj
* @return void
*/
void JNICALL 
Java_j9vm_test_monitor_Helpers_monitorReserve(JNIEnv * env, jclass clazz, jobject obj);


/* ---------------- jnibench.c ---------------- */

/**
* @brief
* @param *env
* @param recv
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__(JNIEnv *env, jobject recv);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__I(JNIEnv *env, jobject recv, int arg1);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__II(JNIEnv *env, jobject recv, int arg1, int arg2);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__III(JNIEnv *env, jobject recv, int arg1, int arg2, int arg3);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__IIII(JNIEnv *env, jobject recv, int arg1, int arg2, int arg3, int arg4);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__IIIII(JNIEnv *env, jobject recv, int arg1, int arg2, int arg3, int arg4, int arg5);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @param arg6
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__IIIIII(JNIEnv *env, jobject recv, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @param arg6
* @param arg7
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__IIIIIII(JNIEnv *env, jobject recv, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @param arg6
* @param arg7
* @param arg8
* @return void
*/
void JNICALL Java_jit_test_vich_JNI_nativeJNI__IIIIIIII(JNIEnv *env, jobject recv, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2, jobject arg3);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2, jobject arg3, jobject arg4);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @return void
*/
void JNICALL Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2, jobject arg3, jobject arg4, jobject arg5);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @param arg6
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2, jobject arg3, jobject arg4, jobject arg5, jobject arg6);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @param arg6
* @param arg7
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2, jobject arg3, jobject arg4, jobject arg5, jobject arg6, jobject arg7);


/**
* @brief
* @param *env
* @param recv
* @param arg1
* @param arg2
* @param arg3
* @param arg4
* @param arg5
* @param arg6
* @param arg7
* @param arg8
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNI_nativeJNI__Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv *env, jobject recv, jobject arg1, jobject arg2, jobject arg3, jobject arg4, jobject arg5, jobject arg6, jobject arg7, jobject arg8);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIArray_getByteArrayElements(JNIEnv *env, jobject obj, jbyteArray array, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIArray_getDoubleArrayElements(JNIEnv *env, jobject obj, jdoubleArray array, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIArray_getFloatArrayElements(JNIEnv *env, jobject obj, jfloatArray array, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param loopCount
* @return void
*/
void JNICALL Java_jit_test_vich_JNIArray_getIntArrayElements(JNIEnv *env, jobject obj, jintArray array, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIArray_getLongArrayElements(JNIEnv *env, jobject obj, jlongArray array, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIArray_getPrimitiveArrayCritical(JNIEnv *env, jobject obj, jintArray array, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualBoolean(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualBooleanA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualByte(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualByteA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL Java_jit_test_vich_JNICallIn_callInNonvirtualChar(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualCharA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualDouble(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualDoubleA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualFloat(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualFloatA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualInt(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualIntA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualLong(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualLongA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualObject(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualObjectA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualShort(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualShortA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualVoid(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInNonvirtualVoidA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticBoolean(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticBooleanA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticByte(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticByteA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticChar(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticCharA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticDouble(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticDoubleA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticFloat(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticFloatA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticInt(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticIntA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticLong(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticLongA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticObject(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticObjectA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticShort(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticShortA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticVoid(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInStaticVoidA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualBoolean(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualBooleanA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualByte(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualByteA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualChar(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualCharA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualDouble(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualDoubleA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualFloat(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualFloatA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualInt(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualIntA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualLong(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualLongA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualObject(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualObjectA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualShort(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL Java_jit_test_vich_JNICallIn_callInVirtualShortA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualVoid(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param objArray
* @param intArray
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNICallIn_callInVirtualVoidA(JNIEnv *env, jobject recv, jint loopCount, jobject objArray, jobject intArray);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jboolean
*/
jboolean JNICALL 
Java_jit_test_vich_JNIFields_getInstanceBoolean(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jbyte
*/
jbyte JNICALL 
Java_jit_test_vich_JNIFields_getInstanceByte(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jchar
*/
jchar JNICALL 
Java_jit_test_vich_JNIFields_getInstanceChar(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jdouble
*/
jdouble JNICALL 
Java_jit_test_vich_JNIFields_getInstanceDouble(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jfloat
*/
jfloat JNICALL 
Java_jit_test_vich_JNIFields_getInstanceFloat(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jint
*/
jint JNICALL 
Java_jit_test_vich_JNIFields_getInstanceInt(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jlong
*/
jlong JNICALL 
Java_jit_test_vich_JNIFields_getInstanceLong(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jobject
*/
jobject JNICALL 
Java_jit_test_vich_JNIFields_getInstanceObject(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jshort
*/
jshort JNICALL 
Java_jit_test_vich_JNIFields_getInstanceShort(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jboolean
*/
jboolean JNICALL 
Java_jit_test_vich_JNIFields_getStaticBoolean(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jbyte
*/
jbyte JNICALL 
Java_jit_test_vich_JNIFields_getStaticByte(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jchar
*/
jchar JNICALL 
Java_jit_test_vich_JNIFields_getStaticChar(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jdouble
*/
jdouble JNICALL 
Java_jit_test_vich_JNIFields_getStaticDouble(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jfloat
*/
jfloat JNICALL 
Java_jit_test_vich_JNIFields_getStaticFloat(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jint
*/
jint JNICALL 
Java_jit_test_vich_JNIFields_getStaticInt(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jlong
*/
jlong JNICALL 
Java_jit_test_vich_JNIFields_getStaticLong(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jobject
*/
jobject JNICALL 
Java_jit_test_vich_JNIFields_getStaticObject(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @return jshort
*/
jshort JNICALL 
Java_jit_test_vich_JNIFields_getStaticShort(JNIEnv *env, jobject recv, jint loopCount);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceBoolean(JNIEnv *env, jobject recv, jint loopCount, jboolean value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceByte(JNIEnv *env, jobject recv, jint loopCount, jbyte value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL Java_jit_test_vich_JNIFields_setInstanceChar(JNIEnv *env, jobject recv, jint loopCount, jchar value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceDouble(JNIEnv *env, jobject recv, jint loopCount, jdouble value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceFloat(JNIEnv *env, jobject recv, jint loopCount, jfloat value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceInt(JNIEnv *env, jobject recv, jint loopCount, jint value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceLong(JNIEnv *env, jobject recv, jint loopCount, jlong value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceObject(JNIEnv *env, jobject recv, jint loopCount, jobject value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setInstanceShort(JNIEnv *env, jobject recv, jint loopCount, jshort value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticBoolean(JNIEnv *env, jobject recv, jint loopCount, jboolean value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticByte(JNIEnv *env, jobject recv, jint loopCount, jbyte value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticChar(JNIEnv *env, jobject recv, jint loopCount, jchar value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticDouble(JNIEnv *env, jobject recv, jint loopCount, jdouble value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticFloat(JNIEnv *env, jobject recv, jint loopCount, jfloat value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticInt(JNIEnv *env, jobject recv, jint loopCount, jint value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticLong(JNIEnv *env, jobject recv, jint loopCount, jlong value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIFields_setStaticObject(JNIEnv *env, jobject recv, jint loopCount, jobject value);


/**
* @brief
* @param *env
* @param recv
* @param loopCount
* @param value
* @return void
*/
void JNICALL Java_jit_test_vich_JNIFields_setStaticShort(JNIEnv *env, jobject recv, jint loopCount, jshort value);


/**
* @brief
* @param *env
* @param obj
* @param o1
* @param o2
* @param o3
* @param o4
* @param o5
* @param o6
* @param o7
* @param o8
* @param o9
* @param o10
* @param o11
* @param o12
* @param o13
* @param o14
* @param o15
* @param o16
* @param o17
* @param o18
* @param o19
* @param o20
* @param o21
* @param o22
* @param o23
* @param o24
* @param o25
* @param o26
* @param o27
* @param o28
* @param o29
* @param o30
* @param o31
* @param o32
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNILocalRef_localReference32(JNIEnv *env, jobject obj, jobject o1, jobject o2, jobject o3, jobject o4, jobject o5, jobject o6, jobject o7, jobject o8, jobject o9, jobject o10, jobject o11, jobject o12, jobject o13, jobject o14, jobject o15, jobject o16, jobject o17, jobject o18, jobject o19, jobject o20, jobject o21, jobject o22, jobject o23, jobject o24, jobject o25, jobject o26, jobject o27, jobject o28, jobject o29, jobject o30, jobject o31, jobject o32, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param o1
* @param o2
* @param o3
* @param o4
* @param o5
* @param o6
* @param o7
* @param o8
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNILocalRef_localReference8(JNIEnv *env, jobject obj, jobject o1, jobject o2, jobject o3, jobject o4, jobject o5, jobject o6, jobject o7, jobject o8, jint loopCount);


/**
* @brief
* @param *env
* @param obj
* @param array
* @param blankArray
* @param arraySize
* @param loopCount
* @return void
*/
void JNICALL 
Java_jit_test_vich_JNIObjectArray_getObjectArrayElement(JNIEnv *env, jobject obj, jobjectArray array, jobjectArray blankArray, jint arraySize, jint loopCount);


/* ---------------- jnitest.c ---------------- */

/**
* @brief
* @param env
* @param rcv
* @return jboolean
*/
jboolean JNICALL 
Java_j9vm_test_jni_JNIFloatTest_floatJNITest(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jint
*/
jint JNICALL 
Java_j9vm_test_jni_JNIMultiFloatTest_floatJNITest2(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jboolean
*/
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame1(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jboolean
*/
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame2(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jboolean
*/
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame3(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jboolean
*/
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrame4(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jboolean
*/
jboolean JNICALL 
Java_j9vm_test_jni_LocalRefTest_testPushLocalFrameNeverPop(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jdouble
*/
jdouble JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileDouble(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jfloat
*/
jfloat JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileFloat(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jint
*/
jint JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileInt(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param rcv
* @return jlong
*/
jlong JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileLong(JNIEnv * env, jobject rcv);


/**
* @brief
* @param env
* @param cls
* @return jdouble
*/
jdouble JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticDouble(JNIEnv * env, jclass cls);


/**
* @brief
* @param env
* @param cls
* @return jfloat
*/
jfloat JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticFloat(JNIEnv * env, jobject cls);


/**
* @brief
* @param env
* @param cls
* @return jint
*/
jint JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticInt(JNIEnv * env, jclass cls);


/**
* @brief
* @param env
* @param cls
* @return jlong
*/
jlong JNICALL 
Java_j9vm_test_jni_VolatileTest_getVolatileStaticLong(JNIEnv * env, jclass cls);


/**
* @brief
* @param env
* @param rcv
* @param aDouble
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileDouble(JNIEnv * env, jobject rcv, jdouble aDouble);


/**
* @brief
* @param env
* @param rcv
* @param aFloat
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileFloat(JNIEnv * env, jobject rcv, jfloat aFloat);


/**
* @brief
* @param env
* @param rcv
* @param anInt
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileInt(JNIEnv * env, jobject rcv, jint anInt);


/**
* @brief
* @param env
* @param rcv
* @param aLong
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileLong(JNIEnv * env, jobject rcv, jlong aLong);


/**
* @brief
* @param env
* @param cls
* @param aDouble
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticDouble(JNIEnv * env, jclass cls, jdouble aDouble);


/**
* @brief
* @param env
* @param cls
* @param aFloat
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticFloat(JNIEnv * env, jclass cls, jfloat aFloat);


/**
* @brief
* @param env
* @param cls
* @param anInt
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticInt(JNIEnv * env, jclass cls, jint anInt);


/**
* @brief
* @param env
* @param cls
* @param aLong
* @return void
*/
void JNICALL 
Java_j9vm_test_jni_VolatileTest_setVolatileStaticLong(JNIEnv * env, jclass cls, jlong aLong);


/**
* @brief
* @param env
* @param clazz
* @param libName
* @return jint
*/
jint JNICALL 
Java_j9vm_test_libraryhandle_LibHandleTest_libraryHandleTest(JNIEnv * env, jclass clazz, jstring libName);


/**
* @brief
* @param env
* @param rcv
* @return jobject
*/
jobject JNICALL 
Java_j9vm_test_libraryhandle_MultipleLibraryLoadTest_vmVersion(JNIEnv * env, jobject rcv);

/**
* @brief
* @param env
* @param clazz
* @return void
*/
void JNICALL
Java_j9vm_test_jni_NullRefTest_test(JNIEnv * env, jclass clazz);

/**
* @brief
* @param vm
* @param *reserved
* @return jint
*/
jint JNICALL 
JNI_OnLoad(JavaVM * vm, void *reserved);


/**
* @brief
* @param vm
* @param *reserved
* @return void
*/
void JNICALL 
JNI_OnUnload(JavaVM * vm, void *reserved);

/* ---------------- threadtest.c ---------------- */

/**
* @brief
* @param env
* @param cls
* @return jobjectArray
*/
jobjectArray JNICALL 
Java_j9vm_test_thread_NativeHelpers_findDeadlockedThreads(JNIEnv *env, jclass cls);

/**
* @brief
* @param env
* @param cls
* @param list
* @return void
*/
void JNICALL 
Java_j9vm_test_thread_NativeHelpers_findDeadlockedThreadsAndObjects(JNIEnv *env, jclass cls, jobject list);

/**
* @brief
* @param env
* @param cls
* @param thread
* @return void
*/
void JNICALL 
Java_j9vm_test_thread_NativeHelpers_abort(JNIEnv *env, jclass cls, jobject thread);

/**
* @brief
* @param env
* @param cls
* @param thread
* @return void
*/
void JNICALL 
Java_j9vm_test_thread_NativeHelpers_priorityInterrupt(JNIEnv *env, jclass cls, jobject thread);


/* ---------------- jniReturnInvalidReference.c ---------------- */
jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_deletedLocalRef(JNIEnv * env, jobject caller);

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_validLocalRef(JNIEnv * env, jobject caller);

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_localRefFromPoppedFrame(JNIEnv * env, jobject caller);

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_explicitReturnOfNull(JNIEnv * env, jobject caller);

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_deletedGlobalRef(JNIEnv * env, jobject caller);

jobject JNICALL 
Java_j9vm_test_jnichk_ReturnInvalidReference_validGlobalRef(JNIEnv * env, jobject caller);

/* ---------------- reattach.c ---------------- */
jboolean JNICALL 
Java_org_openj9_test_osthread_ReattachAfterExit_createTLSKeyDestructor(JNIEnv *env, jobject obj);

jint JNICALL
Java_j9vm_test_classloading_VMAccess_getNumberOfNodes(JNIEnv * env, jobject this);

jint JNICALL
Java_com_ibm_jvmti_tests_util_TestRunner_callLoadAgentLibraryOnAttach(JNIEnv * env, jclass clazz, jstring libraryName, jstring libraryOptions);

/* ---------------- critical.c ---------------- */

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease___3B(JNIEnv * env, jclass clazz, jbyteArray array);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify___3BZ(JNIEnv * env, jclass clazz, jbyteArray array, jboolean commitChanges);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy___3B_3B(JNIEnv * env, jclass clazz, jbyteArray srcArray, jbyteArray destArray);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease___3I(JNIEnv * env, jclass clazz, jintArray array);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify___3IZ(JNIEnv * env, jclass clazz, jintArray array, jboolean commitChanges);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy___3I_3I(JNIEnv * env, jclass clazz, jintArray srcArray, jintArray destArray);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease___3D(JNIEnv * env, jclass clazz, jdoubleArray array);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify___3DZ(JNIEnv * env, jclass clazz, jdoubleArray array, jboolean commitChanges);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy___3D_3D(JNIEnv * env, jclass clazz, jdoubleArray srcArray, jdoubleArray destArray);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testGetRelease__Ljava_lang_String_2(JNIEnv * env, jclass clazz, jstring string);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testModify__Ljava_lang_String_2(JNIEnv * env, jclass clazz, jstring string);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_testMemcpy__Ljava_lang_String_2Ljava_lang_String_2(JNIEnv * env, jclass clazz, jstring srcString, jstring destString);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_holdsVMAccess(JNIEnv * env, jclass clazz, jbyteArray array);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_returnsDirectPointer(JNIEnv * env, jclass clazz, jbyteArray array);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_acquireAndSleep(JNIEnv * env, jclass clazz, jbyteArray array, jlong millis);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_acquireAndCallIn(JNIEnv * env, jclass clazz, jbyteArray array, jobject thread);

jboolean JNICALL
Java_j9vm_test_jni_CriticalRegionTest_acquireDiscardAndGC(JNIEnv * env, jclass clazz, jbyteArray array, jlongArray addresses);


#ifdef __cplusplus
}
#endif


#endif /* jnitest_internal_h */

