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

static agentEnv * _agentEnv;

#define MAX_TAGS 1024

static int  queuedTagNextFree; 
static jlong queuedTagList[MAX_TAGS];
static int  setTagNextFree;
static jlong setTagList[MAX_TAGS];

void 
tagManager_initialize(agentEnv * env)
{
	_agentEnv = env;
	
	setTagNextFree = 0;
	queuedTagNextFree = 0;
	
	memset(&queuedTagList, 0x00, sizeof(jlong) * MAX_TAGS);
	memset(&setTagList, 0x00, sizeof(jlong) * MAX_TAGS);
}


void
queueTag(jlong tag)
{
	queuedTagList[queuedTagNextFree++] = tag;
}


int
isQueued(jlong tag)
{
	int i = 0;
	
	for (i = 0; i < queuedTagNextFree; i++) {
		
		if (queuedTagList[i] == tag) {
			/*printf("%llx -> %llx \n", tag,queuedTagList[i] );*/
			return 1;
		}
	}
	
	return 0;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_TagManager_isTagQueued(JNIEnv * jni_env, jclass klass, jlong tag)
{
	return isQueued(tag) ? JNI_TRUE : JNI_FALSE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_TagManager_checkTags(JNIEnv * jni_env, jclass klass)
{
	int i, j;
	jboolean err = JNI_TRUE; 
	jlong origSetTagList[MAX_TAGS];
		
	memcpy(origSetTagList, setTagList, sizeof(jlong) * MAX_TAGS);
	
	for (i = 0; i < setTagNextFree; i++) {
		for (j = 0; j < queuedTagNextFree; j++) {
			if (origSetTagList[i] == queuedTagList[j]) {
				origSetTagList[j] = 0L;	
			}
		}
	}
	
	for (j = 0; j < setTagNextFree; j++) {
		if (origSetTagList[j] != 0L) {
			error(_agentEnv, JVMTI_ERROR_INTERNAL, "Received no callback for Tag [%x]", origSetTagList[j]);
			err = JNI_FALSE;
		}			
	}

	return err;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_TagManager_setTag(JNIEnv * jni_env, jclass klass, jobject obj, jlong tag, jint flag)
{
	jvmtiError err;
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
	
    err = (*jvmti_env)->SetTag(jvmti_env, obj, tag);
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "TagManager SetTag failed for tag %p", tag);
		return JNI_FALSE;
	}
	
	setTagList[setTagNextFree++] = tag;
	 
	return JNI_TRUE;
}



jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_TagManager_clearTags(JNIEnv * jni_env, jclass klass, jint flags)
{
	int i;
	jvmtiError err;
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
	jlong tags[MAX_TAGS];
	jint tag_count = setTagNextFree - 1;
	jint count_ptr = 0;
	jobject * object_result_ptr;
	jlong * tag_result_ptr;

	if (setTagNextFree == 0) {
		return JNI_TRUE;	
	}
	
	for (i = 0; i < tag_count; i++) {
		tags[i] = setTagList[i];
	}
	
	err = (*jvmti_env)->GetObjectsWithTags(jvmti_env,
			tag_count,
			tags,
			&count_ptr,
			&object_result_ptr,
			&tag_result_ptr);
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "TagManager GetObjectsWithTags failed");
		return JNI_FALSE;
	} 
	
	for (i = 0; i < count_ptr; i++) {
	    err = (*jvmti_env)->SetTag(jvmti_env, object_result_ptr[i], 0L);
		if (err != JVMTI_ERROR_NONE) {
			error(_agentEnv, err, "TagManager SetTag failed for tag %p", tag_result_ptr[i]);
			return JNI_FALSE;
		}		
	}
	
	tagManager_initialize(_agentEnv);
	
	return JNI_TRUE;
}
