/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
#include <stdlib.h>
#include <string.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

jboolean JNICALL Java_com_ibm_jvmti_tests_getThreadGroupChildren_gtgc002_checkGroupName (JNIEnv *jni_env, jclass klass);

static void getChildInfo(JNIEnv *jni_env, jthreadGroup *group);
static agentEnv *env = NULL;

jint JNICALL
gtgc002(agentEnv *agent_env, char *args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env;

	return JNI_OK;
}

static void
getChildInfo(JNIEnv *jni_env, jthreadGroup *group)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	jvmtiThreadGroupInfo groupInfo;
	jint thread_cnt = 0;
	jthread* threads_p = NULL;
	jint group_cnt = 0;
	jthreadGroup *groups_p = NULL;

	if (group == NULL) {
		return;
	}

	if (JNI_OK == (*jvmti_env)->GetThreadGroupInfo(jvmti_env, *group, &groupInfo)) {
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)groupInfo.name);
	}

	if (JNI_OK == (*jvmti_env)->GetThreadGroupChildren(jvmti_env, *group, &thread_cnt, &threads_p, &group_cnt, &groups_p)) {
		int i = 0;
		jthread * thread_ptr = threads_p;
		jthreadGroup *group_ptr = groups_p;
		jvmtiThreadInfo threadInfo;


		for (i = 1; i <= thread_cnt; i++) {
			if (JNI_OK == (*jvmti_env)->GetThreadInfo(jvmti_env, *thread_ptr, &threadInfo)) {
				(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)threadInfo.name);
			}
		}

		for (i = 1; i <= group_cnt; i++) {
			getChildInfo(jni_env, group_ptr);
			group_ptr++;
		}
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)groups_p);
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)threads_p);

	}
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_getThreadGroupChildren_gtgc002_checkGroupName (JNIEnv *jni_env, jclass klass)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	jint grp_count = 0;
	jthreadGroup *grp = NULL;

	if (JNI_OK == (*jvmti_env)->GetTopThreadGroups(jvmti_env, &grp_count, &grp)) {
		getChildInfo(jni_env, grp);
	}

	return JNI_TRUE;
}
