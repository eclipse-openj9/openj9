/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @brief  This file contains implementations of the public JVM interface (JVM_ functions)
 * which simply forward to a concrete implementation located either in the JCL library
 * or proxy forwarder.
 */

#include <stdlib.h>
#include <assert.h>

#include "j9.h"
#include "omrgcconsts.h"
#include "j9modifiers_api.h"
#include "j9vm_internal.h"
#include "j9vmconstantpool.h"
#include "rommeth.h"
#include "sunvmi_api.h"
#include "util_api.h"
#include "ut_j9scar.h"
#include "j9vmnls.h"

jbyteArray JNICALL
JVM_GetClassTypeAnnotations(JNIEnv *env, jclass jlClass) {
	ENSURE_VMI();
	return g_VMI->JVM_GetClassTypeAnnotations(env, jlClass);
}

jbyteArray JNICALL
JVM_GetFieldTypeAnnotations(JNIEnv *env, jobject jlrField) {
	ENSURE_VMI();
	return g_VMI->JVM_GetFieldTypeAnnotations(env, jlrField);
}

jobjectArray JNICALL
JVM_GetMethodParameters(JNIEnv *env, jobject jlrExecutable) {
	ENSURE_VMI();
	return g_VMI->JVM_GetMethodParameters(env, jlrExecutable);
}

jbyteArray JNICALL
JVM_GetMethodTypeAnnotations(JNIEnv *env, jobject jlrMethod) {
	ENSURE_VMI();
	return g_VMI->JVM_GetMethodTypeAnnotations(env, jlrMethod);
}

jboolean JNICALL
JVM_IsVMGeneratedMethodIx(JNIEnv *env, jclass cb, jint index) {
	assert(!"JVM_IsVMGeneratedMethodIx unimplemented"); /* Jazz 63527: Stub in APIs for Java 8 */
	return FALSE;
}

/**
 * Returns platform specific temporary directory used by the system.
 * Same as getTmpDir() defined in jcl/unix/syshelp.c and jcl/win32/syshelp.c.
 *
 * @param [in] env Pointer to JNI environment.
 *
 * @return String object representing the platform specific temporary directory.
 */
jstring JNICALL
JVM_GetTemporaryDirectory(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	jstring result = NULL;
	IDATA size = j9sysinfo_get_tmp(NULL, 0, TRUE);
	if (0 <= size) {
		char *buffer = (char *)j9mem_allocate_memory(size, OMRMEM_CATEGORY_VM);
		if (NULL == buffer) {
			return NULL;
		}
		if (0 == j9sysinfo_get_tmp(buffer, size, TRUE)) {
			result = (*env)->NewStringUTF(env, buffer);
		}

		j9mem_free_memory(buffer);
	}

	return result;
}

