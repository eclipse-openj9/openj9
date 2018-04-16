/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include <string.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

#define INVALID_COM_IBM_ITERATE_SHARED_CACHES_VERSION (COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 + 1)
#define INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS (COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS + 100)
#define INVALID_CACHE_TYPE 3

static jint JNICALL validateSharedCacheInfo(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data);
static jint JNICALL validateSharedCacheInfoVersion2(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data);
static jint JNICALL validateSharedCacheInfoVersion3(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data);
static jint JNICALL validateSharedCacheInfoVersion4(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data);
static int checkAddrMode(int addrMode, int version);

static agentEnv * env;
static jvmtiExtensionFunction iterateSharedCacheFunction = NULL;
static jvmtiExtensionFunction destroySharedCacheFunction = NULL;
static int cacheCount = 0;
static int cacheCountVersion2 = 0;
static int cacheCountVersion3 = 0;
static int cacheCountVersion4 = 0;

jint JNICALL
sca001(agentEnv * agent_env, char * args)
{
	env = agent_env;
	return JNI_OK;
}

/* helper function that tests the address mode */
static int
checkAddrMode(int addrMode, int version)
{
	int ret = -1;

	if (version < COM_IBM_ITERATE_SHARED_CACHES_VERSION_3) {
		if ((COM_IBM_SHARED_CACHE_ADDRMODE_32 == addrMode) || (COM_IBM_SHARED_CACHE_ADDRMODE_64 == addrMode)) {
			ret = 0;
		}
	} else {
		if ((COM_IBM_SHARED_CACHE_ADDRMODE_32 == addrMode) || (COM_IBM_SHARED_CACHE_ADDRMODE_64 == addrMode)) {
			/* Existing cache that does not have the compressedRef info in the addrMode */
			ret = 0;
		} else {
			int validValue1 = COM_IBM_SHARED_CACHE_ADDRMODE_32;
			int validValue2 = COM_IBM_SHARED_CACHE_ADDRMODE_64;
			int validValue3 = COM_IBM_SHARED_CACHE_ADDRMODE_64;

			validValue1 |= COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE;
			validValue2 |= COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE;
			validValue3 |= COM_IBM_ITERATE_SHARED_CACHES_COMPRESSED_POINTERS_MODE;

			if ((validValue1 == addrMode) || (validValue2 == addrMode) || (validValue3 == addrMode)) {
				ret = 0;
			}
		}
	}

	return ret;
}

static jint JNICALL
validateSharedCacheInfo(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data)
{
	if ((NULL != cache_info)
		&& ((jint)0 != cache_info->cacheType)
	) {
		error(env, JVMTI_ERROR_INVALID_TYPESTATE, "Field cacheType of jvmtiSharedCacheInfo has been modified when passing COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 to iterateSharedCacheFunction \n");
		return JNI_EINVAL;
	}
	if (0 != checkAddrMode((int)cache_info->addrMode, COM_IBM_ITERATE_SHARED_CACHES_VERSION_1)) {
		error(env, JVMTI_ERROR_INVALID_TYPESTATE, "Field addrMode of jvmtiSharedCacheInfo is wrong when passing COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 to iterateSharedCacheFunction \n");
		return JNI_EINVAL;
	}

	cacheCount++;
	return JNI_OK;
}

static jint JNICALL
validateSharedCacheInfoVersion2(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data)
{

	if (0 != checkAddrMode((int)cache_info->addrMode, COM_IBM_ITERATE_SHARED_CACHES_VERSION_2)) {
		error(env, JVMTI_ERROR_INVALID_TYPESTATE, "Field addrMode of jvmtiSharedCacheInfo is wrong when passing COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 to iterateSharedCacheFunction \n");
		return JNI_EINVAL;
	}

	cacheCountVersion2++;
	return JNI_OK;
}

static jint JNICALL
validateSharedCacheInfoVersion3(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data)
{
	if (0 != checkAddrMode((int)cache_info->addrMode, COM_IBM_ITERATE_SHARED_CACHES_VERSION_3)) {
		error(env, JVMTI_ERROR_INVALID_TYPESTATE, "Field addrMode of jvmtiSharedCacheInfo is wrong when passing COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 to iterateSharedCacheFunction \n");
		return JNI_EINVAL;
	}

	cacheCountVersion3++;

	return JNI_OK;
}

static jint JNICALL
validateSharedCacheInfoVersion4(jvmtiEnv *jvmti, jvmtiSharedCacheInfo *cache_info, jobject user_data)
{
	if (0 != checkAddrMode((int)cache_info->addrMode, COM_IBM_ITERATE_SHARED_CACHES_VERSION_4)) {
		error(env, JVMTI_ERROR_INVALID_TYPESTATE, "Field addrMode of jvmtiSharedCacheInfo is wrong when passing COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 to iterateSharedCacheFunction \n");
		return JNI_EINVAL;
	}

	cacheCountVersion4++;
	return JNI_OK;
}

jint JNICALL
Java_tests_sharedclasses_options_TestSharedCacheJvmtiAPI_iterateSharedCache(JNIEnv * jni_env, jclass clazz, jstring cacheDir, jint flags, jboolean useCommandLineValues)
{
	jint extensionCount;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	jvmtiError err;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;
	int i;
	const jbyte *dir = NULL;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return (jint)-1;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_ITERATE_SHARED_CACHES) == 0) {
			 iterateSharedCacheFunction = extensionFunctions[i].func;
		}
	}

    err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate id");
		return (jint)-1;
	}

    if (iterateSharedCacheFunction == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "IterateSharedCaches extension was not found");
		return (jint)-1;
	}

    if (cacheDir != NULL) {
    	dir = (const jbyte *)(*jni_env)->GetStringUTFChars(jni_env, cacheDir, NULL);
    	if (dir == NULL) {
    		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Unable to allocate memory for cache directory");
    		return (jint)-1;
    	}
    }

    cacheCount = 0;

    /* check for invalid version */
    err = (iterateSharedCacheFunction)(jvmti_env, INVALID_COM_IBM_ITERATE_SHARED_CACHES_VERSION, dir, flags, useCommandLineValues, validateSharedCacheInfo, NULL);
    if (JVMTI_ERROR_UNSUPPORTED_VERSION != err) {
    	if (cacheDir != NULL) {
    		(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
    	}
    	error(env, err, "iterateSharedCacheFunction: invalid version number test failed \n");
    	return (jint)-1;
    }

    /* check for invalid flags field */
    err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_1, dir, INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS, useCommandLineValues, validateSharedCacheInfo, NULL);
    if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: invalid flags field test failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 \n");
		return (jint)-1;
	}

    cacheCount = 0;
    err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_1, dir, flags, useCommandLineValues, validateSharedCacheInfo, NULL);

	if (err != JVMTI_ERROR_NONE) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: iteration of shared caches failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_1\n");
		return (jint)-1;
	}
	
	/* check for invalid flags field when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 */
	err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_2, dir, INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS, useCommandLineValues, validateSharedCacheInfoVersion2, NULL);
	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: invalid flags field test failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_2\n");
		return (jint)-1;
	}

	cacheCountVersion2 = 0;
	err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_2, dir, flags, useCommandLineValues, validateSharedCacheInfoVersion2, NULL);

	if (err != JVMTI_ERROR_NONE) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: iteration of shared caches failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_2\n");
		return (jint)-1;
	}

	if (cacheCountVersion2 != cacheCount) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, JVMTI_ERROR_INTERNAL, "iterateSharedCacheFunction: Number of caches found using COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 and COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 are different \n");
		return (jint)-1;
	}

	/* check for invalid flags field when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 */
	err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_3, dir, INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS, useCommandLineValues, validateSharedCacheInfoVersion3, NULL);
	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: invalid flags field test failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_3\n");
		return (jint)-1;
	}

	cacheCountVersion3 = 0;
	err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_3, dir, flags, useCommandLineValues, validateSharedCacheInfoVersion3, NULL);

	if (err != JVMTI_ERROR_NONE) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: iteration of shared caches failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_3\n");
		return (jint)-1;
	}

	if (cacheCountVersion3 != cacheCount) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, JVMTI_ERROR_INTERNAL, "iterateSharedCacheFunction: Number of caches found using COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 and COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 are different \n");
		return (jint)-1;
	}


	/* check for invalid flags field when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 */
	err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_4, dir, INVALID_COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS, useCommandLineValues, validateSharedCacheInfoVersion4, NULL);
	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: invalid flags field test failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_4\n");
		return (jint)-1;
	}

	cacheCountVersion4 = 0;
	err = (iterateSharedCacheFunction)(jvmti_env, COM_IBM_ITERATE_SHARED_CACHES_VERSION_4, dir, flags, useCommandLineValues, validateSharedCacheInfoVersion4, NULL);

	if (err != JVMTI_ERROR_NONE) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, err, "iterateSharedCacheFunction: iteration of shared caches failed when passing in COM_IBM_ITERATE_SHARED_CACHES_VERSION_4\n");
		return (jint)-1;
	}

	if (cacheCountVersion4 != cacheCount) {
		if (cacheDir != NULL) {
			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
		}
		error(env, JVMTI_ERROR_INTERNAL, "iterateSharedCacheFunction: Number of caches found using COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 and COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 are different \n");
		return (jint)-1;
	}

	if (cacheDir != NULL) {
		(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
	}

	return cacheCount;
}

jboolean JNICALL
Java_tests_sharedclasses_options_TestSharedCacheJvmtiAPI_destroySharedCache(JNIEnv * jni_env, jclass clazz, jstring cacheDir, jstring cacheName, jint cacheType, jboolean useCommandLineValues)
{
	jint extensionCount, errorCode;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	jvmtiError err;
	jvmtiEnv *jvmti_env = env->jvmtiEnv;
	int i;
	const jbyte	*dir = NULL;
	const jbyte	*name = NULL;

    err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_FALSE;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_DESTROY_SHARED_CACHE) == 0) {
			 destroySharedCacheFunction = extensionFunctions[i].func;
		}
	}

    err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate ExtensionFunctions");
		return JNI_FALSE;
	}

    if (destroySharedCacheFunction == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "DeleteSharedCaches extension was not found");
		return JNI_FALSE;
	}

    if (NULL != cacheDir) {
    	dir = (const jbyte *)(*jni_env)->GetStringUTFChars(jni_env, cacheDir, NULL);
    	if (dir == NULL) {
    		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Unable to allocate memory for cache directory");
    		return JNI_FALSE;
    	}
    }

    if (NULL != cacheName) {
    	name = (const jbyte *)(*jni_env)->GetStringUTFChars(jni_env, cacheName, NULL);
    	if (NULL == name) {
    		if (NULL != cacheDir) {
    			(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
    		}
    		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Unable to allocate memory for cache name");
    		return JNI_FALSE;
    	}
    }

    err = (destroySharedCacheFunction)(jvmti_env, dir, name, cacheType, useCommandLineValues, &errorCode);

    if (NULL != cacheDir) {
    	(*jni_env)->ReleaseStringUTFChars(jni_env, cacheDir, (const char *)dir);
    }
    if (NULL != cacheName) {
    	(*jni_env)->ReleaseStringUTFChars(jni_env, cacheName, (const char *)name);
    }

    if (INVALID_CACHE_TYPE == cacheType) {
    	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
    		error(env, err, "deleteSharedCacheFunction: invalid cache type test failed \n");
    		return JNI_FALSE;
    	}
    } else if (err != JVMTI_ERROR_NONE) {
		error(env, err, "deleteSharedCacheFunction failed with error code: %d", errorCode);
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

