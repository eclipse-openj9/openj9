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


/**
 * Copies memory from one place to another, endian flipping the data.
 *
 * Implementation of native java.nio.Bits.copySwapMemory0(). The single java caller
 * has ensured all of the parameters are valid.
 *
 * @param [in] env Pointer to JNI environment
 * @param [in] srcObj Source primitive array (NULL means srcOffset represents native memory)
 * @param [in] srcOffset Offset in source array / address in native memory
 * @param [in] dstObj Destination primitive array (NULL means dstOffset represents native memory)
 * @param [in] dstOffset Offset in destination array / address in native memory
 * @param [in] size Number of bytes to copy
 * @param [in] elemSize Size of elements to copy and flip
 *
 * elemSize = 2 means byte order 1,2 becomes 2,1
 * elemSize = 4 means byte order 1,2,3,4 becomes 4,3,2,1
 * elemSize = 8 means byte order 1,2,3,4,5,6,7,8 becomes 8,7,6,5,4,3,2,1
 */
void JNICALL
JVM_CopySwapMemory(JNIEnv *env, jobject srcObj, jlong srcOffset, jobject dstObj, jlong dstOffset, jlong size, jlong elemSize)
{
	U_8 *srcBytes = NULL;
	U_8 *dstBytes = NULL;
	U_8 *dstAddr = NULL;
	if (NULL != srcObj) {
		srcBytes = (*env)->GetPrimitiveArrayCritical(env, srcObj, NULL);
		/* The java caller has added Unsafe.arrayBaseOffset() to the offset. Remove it
		 * here as GetPrimitiveArrayCritical returns a pointer to the first element.
		 */
		srcOffset -= sizeof(J9IndexableObjectContiguous);
	}
	if (NULL != dstObj) {
		dstBytes = (*env)->GetPrimitiveArrayCritical(env, dstObj, NULL);
		dstAddr = dstBytes;
		/* The java caller has added Unsafe.arrayBaseOffset() to the offset. Remove it
		 * here as GetPrimitiveArrayCritical returns a pointer to the first element.
		 */
		dstOffset -= sizeof(J9IndexableObjectContiguous);
	}
	dstAddr += (UDATA)dstOffset;
	/* First copy the bytes unmodified to the new location (memmove handles the overlap case) */
	memmove(dstAddr, srcBytes + (UDATA)srcOffset, (size_t)size);
	/* Now flip each element in the destination */
	switch(elemSize) {
	case 2: {
		jlong elemCount = size / 2;
		while (0 != elemCount) {
			U_8 temp = dstAddr[0];
			dstAddr[0] = dstAddr[1];
			dstAddr[1] = temp;
			dstAddr += 2;
			elemCount -= 1;
		}
		break;
	}
	case 4: {
		jlong elemCount = size / 4;
		while (0 != elemCount) {
			U_8 temp = dstAddr[0];
			dstAddr[0] = dstAddr[3];
			dstAddr[3] = temp;
			temp = dstAddr[1];
			dstAddr[1] = dstAddr[2];
			dstAddr[2] = temp;
			dstAddr += 4;
			elemCount -= 1;
		}
		break;
	}
	default /* 8 */: {
		jlong elemCount = size / 8;
		while (0 != elemCount) {
			U_8 temp = dstAddr[0];
			dstAddr[0] = dstAddr[7];
			dstAddr[7] = temp;
			temp = dstAddr[1];
			dstAddr[1] = dstAddr[6];
			dstAddr[6] = temp;
			temp = dstAddr[2];
			dstAddr[2] = dstAddr[5];
			dstAddr[5] = temp;
			temp = dstAddr[3];
			dstAddr[3] = dstAddr[4];
			dstAddr[4] = temp;
			dstAddr += 8;
			elemCount -= 1;
		}
		break;
	}
	}
	if (NULL != srcObj) {
		(*env)->ReleasePrimitiveArrayCritical(env, srcObj, srcBytes, JNI_ABORT);
	}
	if (NULL != dstObj) {
		(*env)->ReleasePrimitiveArrayCritical(env, dstObj, dstBytes, 0);
	}
}
