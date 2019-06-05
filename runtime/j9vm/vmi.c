/*******************************************************************************
 * Copyright (c) 2002, 2019 IBM Corp. and others
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

/**
 * @brief  This file contains implementations of the Sun VM interface (JVM_ functions)
 * which simply forward to a concrete implementation located either in the JCL library
 * or proxy forwarder.
 */

#include <stdlib.h>
#include "j9.h"
#include "sunvmi_api.h"

#include "../util/ut_module.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#define _UTE_STATIC_
#include "ut_j9scar.h"

extern J9JavaVM *BFUjavaVM;
SunVMI* g_VMI;

/**
 * Initializes the VM-interface from the supplied JNIEnv.
 * 
 */
void 
initializeVMI(void)
{
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
	jint result;
	
	/* Register this module with trace */
	UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(BFUjavaVM));
	Trc_SC_VMInitStages_Event1(BFUjavaVM->mainThread);
	result = (BFUjavaVM)->internalVMFunctions->GetEnv((JavaVM*)BFUjavaVM, (void**)&g_VMI, SUNVMI_VERSION_1_1);
	if (result != JNI_OK) {
		j9tty_printf(PORTLIB, "FATAL ERROR: Could not obtain SUNVMI from VM.\n");
		exit(-1);
	}
}

jobject JNICALL
JVM_LatestUserDefinedLoader(JNIEnv *env)
{
	ENSURE_VMI();
	return g_VMI->JVM_LatestUserDefinedLoader(env);
}


jobject JNICALL
#if JAVA_SPEC_VERSION >= 11
JVM_GetCallerClass(JNIEnv *env)
#else /* JAVA_SPEC_VERSION >= 11 */
JVM_GetCallerClass(JNIEnv *env, jint depth)
#endif /* JAVA_SPEC_VERSION >= 11 */
{
	ENSURE_VMI();
#if JAVA_SPEC_VERSION >= 11
	return g_VMI->JVM_GetCallerClass(env);
#else /* JAVA_SPEC_VERSION >= 11 */
	return g_VMI->JVM_GetCallerClass(env, depth);
#endif /* JAVA_SPEC_VERSION >= 11 */
}


jobject JNICALL
JVM_NewInstanceFromConstructor(JNIEnv * env, jobject c, jobjectArray args)
{
	ENSURE_VMI();
	return g_VMI->JVM_NewInstanceFromConstructor(env, c, args);
}


jobject JNICALL
JVM_InvokeMethod(JNIEnv * env, jobject method, jobject obj, jobjectArray args)
{
	ENSURE_VMI();
	return g_VMI->JVM_InvokeMethod(env, method, obj, args);
}


jint JNICALL
JVM_GetClassAccessFlags(JNIEnv * env, jclass clazzRef)
{
	ENSURE_VMI();
	return g_VMI->JVM_GetClassAccessFlags(env, clazzRef);
}


jobject JNICALL
JVM_GetClassContext(JNIEnv *env)
{
	ENSURE_VMI();
	return g_VMI->JVM_GetClassContext(env);
}


void JNICALL
JVM_Halt(jint exitCode)
{
	ENSURE_VMI();
	g_VMI->JVM_Halt(exitCode);
}


void JNICALL
JVM_GCNoCompact(void)
{
	ENSURE_VMI();
	g_VMI->JVM_GCNoCompact();
}


void JNICALL
JVM_GC(void)
{
	ENSURE_VMI();
	g_VMI->JVM_GC();
}


/**
 * JVM_TotalMemory
 */
jlong JNICALL
JVM_TotalMemory(void)
{
	ENSURE_VMI();
	return g_VMI->JVM_TotalMemory();
}


jlong JNICALL
JVM_FreeMemory(void)
{
	ENSURE_VMI();
	return g_VMI->JVM_FreeMemory();
}


jobject JNICALL
JVM_GetSystemPackages(JNIEnv* env)
{
	ENSURE_VMI();
	return g_VMI->JVM_GetSystemPackages(env);
}


/**
 * Returns the package information for the specified package name.  Package information is the directory or
 * jar file name from where the package was loaded (separator is to be '/' and for a directory the return string is
 * to end with a '/' character). If the package is not loaded then null is to be returned.
 *
 * @arg[in] env          - JNI environment.
 * @arg[in] pkgName -  A Java string for the name of a package. The package must be separated with '/' and optionally end with a '/' character.
 *
 * @return Package information as a string.
 *
 * @note In the current implementation, the separator is not guaranteed to be '/', not is a directory guaranteed to be
 * terminated with a slash. It is also unclear what the expected implementation is for UNC paths.
 *
 * @note see CMVC defects 81175 and 92979
 */
jstring JNICALL
JVM_GetSystemPackage(JNIEnv* env, jstring pkgName)
{
	ENSURE_VMI();
	return g_VMI->JVM_GetSystemPackage(env, pkgName);
}


jobject JNICALL
JVM_AllocateNewObject(JNIEnv *env, jclass caller, jclass current, jclass init) 
{
	ENSURE_VMI();
	return g_VMI->JVM_AllocateNewObject(env, caller, current, init);
}


jobject JNICALL
JVM_AllocateNewArray(JNIEnv *env, jclass caller, jclass current, jint length)
{
	ENSURE_VMI();
	return g_VMI->JVM_AllocateNewArray(env, caller, current, length);
}


jobject JNICALL
JVM_GetClassLoader(JNIEnv *env, jobject obj)
{
	ENSURE_VMI();
	return g_VMI->JVM_GetClassLoader(env, obj);
}


void* JNICALL
JVM_GetThreadInterruptEvent(void)
{
	ENSURE_VMI();
	return g_VMI->JVM_GetThreadInterruptEvent();
}


jlong JNICALL
JVM_MaxObjectInspectionAge(void)
{
	ENSURE_VMI();
	return g_VMI->JVM_MaxObjectInspectionAge();
}

jlong JNICALL
JVM_MaxMemory(void) {
	ENSURE_VMI();
	return g_VMI->JVM_MaxMemory();
}
