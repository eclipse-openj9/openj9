/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#include "jni.h"
#include "j9port.h"
#include "jclprots.h"

/*
 * Invoked immediately after this shared library has been loaded.
 * The shared library is currenly only a stub: simply return.
 */
jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
	return JNI_VERSION_1_8;
}

/* Convert path to platform encoding.
 * It is the callers responsibility for freeing the returned memory.
 *
 * @return NULL on error, otherwise return converted string
 */
static const char*
translatePathToPlatform(JNIEnv *env, const char* pathUTF)
{
	char *result = NULL;
	int32_t bufferLength = 0;
	uintptr_t pathUTFSize = strlen(pathUTF);

	PORT_ACCESS_FROM_ENV(env);

	bufferLength = 	j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_OMR_INTERNAL, pathUTF, pathUTFSize, NULL, 0);

	if (bufferLength >= 0) {
		bufferLength += MAX_STRING_TERMINATOR_LENGTH;

		result = (char*)j9mem_allocate_memory(bufferLength, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != result)  {
			int32_t resultLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_OMR_INTERNAL, pathUTF, pathUTFSize, result, bufferLength);
			if ((resultLength < 0) ||  (resultLength + MAX_STRING_TERMINATOR_LENGTH != bufferLength)) {
				j9mem_free_memory(result);
				result = NULL; /* error occured */
			} else {
				/* add terminator */
				memset(result + resultLength, 0, MAX_STRING_TERMINATOR_LENGTH);
			}
		}
	}

	return (const char*)result;

}

/**
 * Determines whether file path has user permissions only.
 *
 * @param path file path
 * @return Returns true if the file has only user permissions,
 * false otherwise. Always returns false on Windows.
 *
 * Note: Windows permission bits are not set to differentiate between users.
 */
JNIEXPORT jboolean JNICALL 
Java_sun_management_FileSystemImpl_isAccessUserOnly0(JNIEnv *env, jclass c, jstring path)
{
	const char* pathUTF = NULL;
	jboolean result = JNI_FALSE;

	PORT_ACCESS_FROM_ENV(env);

	pathUTF = (*env)->GetStringUTFChars(env, path, NULL);
	if (NULL == pathUTF) {
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
	} else {
		struct J9FileStat buf;
		I_32 status = -1;
		const char* pathConvert = pathUTF;
#if !defined(WIN32) && !defined(WIN64)
		pathConvert = (char*)translatePathToPlatform(env, pathUTF);
		if (NULL == pathConvert) {
			((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
			(*env)->ReleaseStringUTFChars(env, path, pathUTF);
			return result;
		}
#endif /* skip on windows */
		/* retrieve file information */
		status = j9file_stat(pathConvert, 0, &buf);

		if (0 == status) {
			/* if any permissions are set other than user, fail */
			if (!buf.perm.isGroupWriteable && !buf.perm.isGroupReadable && !buf.perm.isOtherWriteable && !buf.perm.isOtherReadable) {
				result = JNI_TRUE;
			}
		} else {
			/* failed to retrieve statistics */
			((J9VMThread *)env)->javaVM->internalVMFunctions->throwNewJavaIoIOException(env, NULL);
		}

		if (pathConvert != pathUTF) {
			j9mem_free_memory((void*)pathConvert);
		}
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	}

	return result;
}

#if defined(WIN32) || defined(WIN64)
JNIEXPORT void JNICALL 
Java_sun_management_FileSystemImpl_init0(JNIEnv *env, jclass c)
{
	/* stub method, no implementation required */ 
}
#endif /* defined(WIN32) || defined(WIN64) */
