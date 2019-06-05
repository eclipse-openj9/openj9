/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#if defined(WIN32)
#include <windows.h>
#endif /* defined(WIN32) */

#include "jni.h"
#include "j9port.h"
#include "jclprots.h"

/*
 * Invoked immediately after this shared library has been loaded.
 */
jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
	return JNI_VERSION_1_8;
}

/* Convert a modified UTF8 path to the given encoding.
 *
 * The caller must free the returned memory.
 *
 * @return NULL on error, otherwise return converted string
 */
static char *
convertPath(JNIEnv *env, const char *pathUTF, int32_t toCode)
{
	PORT_ACCESS_FROM_ENV(env);
	char *result = NULL;
	const uintptr_t pathUTFSize = strlen(pathUTF);
	const int32_t resultSize = j9str_convert(J9STR_CODE_MUTF8, toCode, pathUTF, pathUTFSize, NULL, 0);

	if (resultSize >= 0) {
		const int32_t spaceRequired = resultSize + MAX_STRING_TERMINATOR_LENGTH;
		result = (char *)j9mem_allocate_memory(spaceRequired, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != result) {
			if (resultSize != j9str_convert(J9STR_CODE_MUTF8, toCode,
					pathUTF, pathUTFSize, result, spaceRequired)) {
				j9mem_free_memory(result);
				result = NULL; /* error occured */
			}
		}
	}

	return result;
}

/*
 * Determines whether file path has user permissions only.
 *
 * @param path file path
 * @return Returns true if the file has only user permissions,
 * false otherwise. Always returns false on Windows.
 *
 * Note: Windows permission bits are not set to differentiate between users.
 */
jboolean JNICALL
Java_sun_management_FileSystemImpl_isAccessUserOnly0(JNIEnv *env, jclass c, jstring path)
{
	PORT_ACCESS_FROM_ENV(env);
	jboolean result = JNI_FALSE;
	const char *pathUTF = (*env)->GetStringUTFChars(env, path, NULL);

	if (NULL == pathUTF) {
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
	} else {
		struct J9FileStat buf;
		I_32 status = -1;
#if defined(WIN32)
		const char *pathConvert = pathUTF;
#else
		char *pathConvert = convertPath(env, pathUTF, J9STR_CODE_PLATFORM_OMR_INTERNAL);
		if (NULL == pathConvert) {
			((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
			goto done;
		}
#endif /* !defined(WIN32) */
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

#if !defined(WIN32)
		j9mem_free_memory(pathConvert);
done:
#endif /* !defined(WIN32) */
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	}

	return result;
}

#if defined(WIN32)

void JNICALL
Java_sun_management_FileSystemImpl_init0(JNIEnv *env, jclass c)
{
	/* stub method, no implementation required */
}

/*
 * Determines whether the file system addressed by the given absolute path supports security.
 *
 * @param path file path
 * @return Returns true if the referenced file system supports security, false otherwise.
 */
jboolean JNICALL
Java_sun_management_FileSystemImpl_isSecuritySupported0(JNIEnv *env, jclass c, jstring path)
{
	jboolean result = JNI_FALSE;
	const char *pathUTF = (*env)->GetStringUTFChars(env, path, NULL);

	if (NULL == pathUTF) {
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
	} else {
		PORT_ACCESS_FROM_ENV(env);
		void *pathWide = convertPath(env, pathUTF, J9STR_CODE_WIDE);

		if (NULL == pathWide) {
			((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
		} else {
			DWORD fileSystemFlags = 0;
			wchar_t volumePath[EsMaxPath];

			if (!GetVolumePathNameW((wchar_t *)pathWide, volumePath, EsMaxPath) ||
				!GetVolumeInformationW(volumePath, NULL, 0, NULL, NULL, &fileSystemFlags, NULL, 0)) {
				/* failed to retrieve information */
				((J9VMThread *)env)->javaVM->internalVMFunctions->throwNewJavaIoIOException(env, NULL);
			} else if (0 != (fileSystemFlags & FILE_PERSISTENT_ACLS)) {
				result = JNI_TRUE;
			}

			j9mem_free_memory(pathWide);
		}

		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	}

	return result;
}

#endif /* defined(WIN32) */
