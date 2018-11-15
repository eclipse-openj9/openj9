/*******************************************************************************
 * Copyright (c) 2002, 2018 IBM Corp. and others
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


#ifndef SUNVMI_API_H
#define SUNVMI_API_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This module exposes the vm-centric glue to attach J9 to the Sun
 * class library.  
 * 
 * The primary interface to this module is a table of function pointers 
 * motivated by the fact that most consumers (classic/jvm.dll and proxy
 * services) are simply forwarders.
 */

#include "jni.h"
#include "j9.h"

/* 
 * Typedefs for all members of the SunVMI table.
 */
typedef jobject (JNICALL *JVM_AllocateNewArray_Type)(JNIEnv *env, jclass caller, jclass current, jint length);
typedef jobject (JNICALL *JVM_AllocateNewObject_Type)(JNIEnv *env, jclass caller, jclass current, jclass init);
typedef jlong (JNICALL *JVM_FreeMemory_Type)(void);
typedef void (JNICALL *JVM_GC_Type)(void);
typedef void (JNICALL *JVM_GCNoCompact_Type)(void);
#if JAVA_SPEC_VERSION >= 11
typedef jobject (JNICALL *JVM_GetCallerClass_Type)(JNIEnv *env);
#else /* JAVA_SPEC_VERSION >= 11 */
typedef jobject (JNICALL *JVM_GetCallerClass_Type)(JNIEnv *env, jint depth);
#endif /* JAVA_SPEC_VERSION >= 11 */
typedef jint (JNICALL *JVM_GetClassAccessFlags_Type)(JNIEnv * env, jclass clazzRef);
typedef jobject (JNICALL *JVM_GetClassContext_Type)(JNIEnv *env);
typedef jobject (JNICALL *JVM_GetClassLoader_Type)(JNIEnv *env, jobject obj);


/**
 * Returns an array of strings for the currently loaded system packages. 
 * The name returned for each package is to be '/' separated and end with 
 * a '/' character.
 *
 * @param env
 * @param pkdName *
 * @return a jarray of jstrings
 */
typedef jstring (JNICALL *JVM_GetSystemPackage_Type)(JNIEnv* env, jstring pkgName);

typedef jobject (JNICALL *JVM_GetSystemPackages_Type)(JNIEnv* env);
typedef void* (JNICALL *JVM_GetThreadInterruptEvent_Type)(void);
typedef void (JNICALL *JVM_Halt_Type)(jint exitCode);
typedef jobject (JNICALL *JVM_InvokeMethod_Type)(JNIEnv * env, jobject method, jobject obj, jobjectArray args);
typedef jobject (JNICALL *JVM_LatestUserDefinedLoader_Type)(JNIEnv *env);
typedef jlong (JNICALL *JVM_MaxObjectInspectionAge_Type)(void);
typedef jobject (JNICALL *JVM_NewInstanceFromConstructor_Type)(JNIEnv * env, jobject c, jobjectArray args);
typedef jlong (JNICALL *JVM_TotalMemory_Type)(void);
typedef jlong (JNICALL *JVM_MaxMemory_Type)(void);
typedef jobject (JNICALL *JVM_FindClassFromClassLoader_Type)(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError);
typedef void (JNICALL *JVM_ExtendBootClassPath_Type)(JNIEnv* env, const char * pathSegment);
typedef jbyteArray (JNICALL *JVM_GetClassTypeAnnotations_Type)(JNIEnv *env, jclass jlClass);
typedef jbyteArray (JNICALL *JVM_GetFieldTypeAnnotations_Type)(JNIEnv *env, jobject jlrField);
typedef jbyteArray (JNICALL *JVM_GetMethodTypeAnnotations_Type)(JNIEnv *env, jobject jlrMethod);
typedef jobjectArray (JNICALL *JVM_GetMethodParameters_Type)(JNIEnv *env, jobject jlrExecutable);

/* Function prototypes for implementations */
JNIEXPORT jobject JNICALL JVM_LatestUserDefinedLoader_Impl(JNIEnv *env);
#if JAVA_SPEC_VERSION >= 11
JNIEXPORT jobject JNICALL JVM_GetCallerClass_Impl(JNIEnv *env);
#else /* JAVA_SPEC_VERSION >= 11 */
JNIEXPORT jobject JNICALL JVM_GetCallerClass_Impl(JNIEnv *env, jint depth);
#endif /* JAVA_SPEC_VERSION >= 11 */
JNIEXPORT jobject JNICALL JVM_NewInstanceFromConstructor_Impl(JNIEnv * env, jobject c, jobjectArray args);
JNIEXPORT jobject JNICALL JVM_InvokeMethod_Impl(JNIEnv * env, jobject method, jobject obj, jobjectArray args);
JNIEXPORT jint JNICALL JVM_GetClassAccessFlags_Impl(JNIEnv * env, jclass clazzRef);
JNIEXPORT jobject JNICALL JVM_GetClassContext_Impl(JNIEnv *env);
JNIEXPORT void JNICALL JVM_Halt_Impl(jint exitCode);
JNIEXPORT void JNICALL JVM_GCNoCompact_Impl(void);
JNIEXPORT void JNICALL JVM_GC_Impl(void);
JNIEXPORT jlong JNICALL JVM_TotalMemory_Impl(void);
JNIEXPORT jlong JNICALL JVM_FreeMemory_Impl(void);
JNIEXPORT jobject JNICALL JVM_GetSystemPackages_Impl(JNIEnv* env);
JNIEXPORT jstring JNICALL JVM_GetSystemPackage_Impl(JNIEnv* env, jstring pkgName);
JNIEXPORT jobject JNICALL JVM_AllocateNewObject_Impl(JNIEnv *env, jclass caller, jclass current, jclass init);
JNIEXPORT jobject JNICALL JVM_AllocateNewArray_Impl(JNIEnv *env, jclass caller, jclass current, jint length);
JNIEXPORT jobject JNICALL JVM_GetClassLoader_Impl(JNIEnv *env, jobject obj);
JNIEXPORT void* JNICALL JVM_GetThreadInterruptEvent_Impl(void);
JNIEXPORT jlong JNICALL JVM_MaxObjectInspectionAge_Impl(void);
JNIEXPORT jobject JNICALL JVM_FindClassFromClassLoader_Impl(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError);
JNIEXPORT jlong JNICALL JVM_MaxMemory_Impl(void);
JNIEXPORT void JNICALL JVM_ExtendBootClassPath_Impl(JNIEnv* env, const char * pathSegment);
JNIEXPORT jbyteArray JNICALL JVM_GetClassTypeAnnotations_Impl(JNIEnv *env, jclass jlClass);
JNIEXPORT jbyteArray JNICALL JVM_GetFieldTypeAnnotations_Impl(JNIEnv *env, jobject jlrField);
JNIEXPORT jbyteArray JNICALL JVM_GetMethodTypeAnnotations_Impl(JNIEnv *env, jobject jlrMethod);
JNIEXPORT jobjectArray JNICALL JVM_GetMethodParameters_Impl(JNIEnv *env, jobject jlrExecutable);



/* 
 * Structure which contains all of the VMI functions.
 */
typedef struct SunVMI {
	UDATA reserved;
	JVM_AllocateNewArray_Type JVM_AllocateNewArray;
	JVM_AllocateNewObject_Type JVM_AllocateNewObject;
	JVM_FreeMemory_Type JVM_FreeMemory;
	JVM_GC_Type JVM_GC;
	JVM_GCNoCompact_Type JVM_GCNoCompact;
	JVM_GetCallerClass_Type JVM_GetCallerClass;
	JVM_GetClassAccessFlags_Type JVM_GetClassAccessFlags;
	JVM_GetClassContext_Type JVM_GetClassContext;
	JVM_GetClassLoader_Type JVM_GetClassLoader;
	JVM_GetSystemPackage_Type JVM_GetSystemPackage;
	JVM_GetSystemPackages_Type JVM_GetSystemPackages;
	JVM_GetThreadInterruptEvent_Type JVM_GetThreadInterruptEvent;
	JVM_Halt_Type JVM_Halt;
	JVM_InvokeMethod_Type JVM_InvokeMethod;
	JVM_LatestUserDefinedLoader_Type JVM_LatestUserDefinedLoader;
	JVM_MaxObjectInspectionAge_Type JVM_MaxObjectInspectionAge;
	JVM_NewInstanceFromConstructor_Type JVM_NewInstanceFromConstructor;
	JVM_TotalMemory_Type JVM_TotalMemory;
	JVM_MaxMemory_Type JVM_MaxMemory;
	JVM_FindClassFromClassLoader_Type JVM_FindClassFromClassLoader;
	JVM_ExtendBootClassPath_Type JVM_ExtendBootClassPath;
	JVM_GetClassTypeAnnotations_Type JVM_GetClassTypeAnnotations;
	JVM_GetFieldTypeAnnotations_Type JVM_GetFieldTypeAnnotations;
	JVM_GetMethodTypeAnnotations_Type JVM_GetMethodTypeAnnotations;
	JVM_GetMethodParameters_Type JVM_GetMethodParameters;
} SunVMI;


/* 
 * Lifecycle function used to push the SunVMI implementation through the
 * proper initialization stages.  This entrypoint should be called from
 * J9VMDllMain() as the JVM is bootstrapping.
 * 
 */
IDATA SunVMI_LifecycleEvent(J9JavaVM* vm, IDATA stage, void* reserved);

/* Interface constants used with JNI GetEnv() call to obtain the VMI. */
#define SUNVMI_VERSION_1_1 0x7D010001

extern SunVMI* g_VMI;

#define ENSURE_VMI() \
	if (NULL == g_VMI) { \
		initializeVMI(); \
	}

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* SUNVMI_API_H */
