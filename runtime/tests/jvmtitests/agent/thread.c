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

#include "jvmti_test.h"

/**
 * \brief	Get thread local storage data
 * \ingroup 	jvmti.test.utils
 *
 * @param[in] env            jvmti test environment
 * @param[in] currentThread  jni ref to the thread
 * @return pointer to the user thread local storage data
 *
 */
threadData *
getThreadData(agentEnv *env, jthread currentThread)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	if (currentThread != NULL) {
		threadData * localData = NULL;

		(*jvmti_env)->GetThreadLocalStorage(jvmti_env, currentThread, (void **) &localData);
		if (localData) {
			return localData;
		}
	}
	return &env->globalData;
}


/**
 * \brief	Get the name of a thread
 * \ingroup 	jvmti.test.utils
 *
 * @param[in] env           jvmti test environment
 * @param[in] currentThread jni ref to the current thread
 * @param[in] thread        jni ref to the thread we wish to get a name for
 * @return  dynamically allocated thread name
 *
 *  The caller is responsible for freeing the returned thread name using
 *  <code>(*jvmti_env)->Deallocate</code>
 */
char *
getThreadName(agentEnv *env, jthread currentThread, jthread thread)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	jvmtiThreadInfo threadInfo;
	threadData * data = getThreadData(env, currentThread);
	IDATA  threadNameLen;
	char * threadNameStr;
	char   threadNameBuf[JVMTI_TEST_NAME_MAX];

	jvmtiPhase phase;

	(*jvmti_env)->GetPhase(jvmti_env, &phase);
	if (phase == JVMTI_PHASE_LIVE) {
		err = (*jvmti_env)->GetThreadInfo(jvmti_env, thread, &threadInfo);
		if (err == JVMTI_ERROR_NONE) {
			strcpy(threadNameBuf, threadInfo.name);
			(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) threadInfo.name);
		} else {
			error(env, err, "Failed to get thread info");
		}
	} else {
		strcpy(threadNameBuf, "<not in live phase>");
	}

	threadNameLen = strlen(threadNameBuf) + 1;
	err = (*jvmti_env)->Allocate(jvmti_env, threadNameLen, (unsigned char **) &threadNameStr);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to allocate return buffer for thread name");
		return NULL;
	}

	strcpy(threadNameStr, threadNameBuf);

	return threadNameStr;
}


/**
 * \brief	Get a JNI ref to the current thread
 * \ingroup	jvmti.test.utils
 *
 *
 * @param[in] jni_env JNI environment
 * @return JNI reference to the current thread
 */
jthread
getCurrentThread(JNIEnv * jni_env)
{
	jclass clazz;
	jmethodID mid;

	clazz = (*jni_env)->FindClass(jni_env, "java/lang/Thread");
	if (clazz == NULL) {
		return NULL;
	}

	mid = (*jni_env)->GetStaticMethodID(jni_env, clazz, "currentThread", "()Ljava/lang/Thread;");
	if (mid == NULL) {
		return NULL;
	}

	return (jthread)  (*jni_env)->CallStaticObjectMethod(jni_env, clazz, mid);
}
