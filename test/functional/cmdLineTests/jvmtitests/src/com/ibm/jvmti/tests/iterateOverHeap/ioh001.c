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

static agentEnv * env;                                                    

jint JNICALL
ioh001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}						

	return JNI_OK;
}


#define JVMTI_TEST_IOH001_JAVALANGCLASS_TAG    0xc0fe
#define JVMTI_TEST_IOH001_CLASS_TAG            0xc0de

typedef struct ioh001_data {
	int foundJavaLangClass;
	int foundIoh001Class;
} ioh001_data;


static jvmtiIterationControl JNICALL
iohHeapObjectCallback(jlong class_tag, jlong size, jlong * tag_ptr, void * user_data)
{
	ioh001_data * data = (ioh001_data *) user_data;

	if (*tag_ptr == JVMTI_TEST_IOH001_JAVALANGCLASS_TAG) {
         	data->foundJavaLangClass = 1;
	}

 	if (*tag_ptr == JVMTI_TEST_IOH001_CLASS_TAG) {
         	data->foundIoh001Class = 1;
	} 

	return JVMTI_ITERATION_CONTINUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateOverHeap_ioh001_iterate(JNIEnv * jni_env, jclass klazz)							      
{
	jvmtiError rc;
	jvmtiEnv * jvmti_env = env->jvmtiEnv;
	jclass clazz;
	ioh001_data data;

	data.foundJavaLangClass = 0;
	
	
	clazz = (*jni_env)->FindClass(jni_env, "java/lang/Class");
	if (clazz == NULL) {
		error(env, JVMTI_ERROR_NONE, "Unable to FindClass java/lang/Class in preparation for tagging");
		return JNI_FALSE;
	}
	
	rc = (*jvmti_env)->SetTag(jvmti_env, clazz, JVMTI_TEST_IOH001_JAVALANGCLASS_TAG);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to SetTag [JVMTI_TEST_IOH001_JAVALANGCLASS_TAG]");
		return JNI_FALSE;
	}

 	rc = (*jvmti_env)->SetTag(jvmti_env, klazz, JVMTI_TEST_IOH001_CLASS_TAG);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to SetTag [JVMTI_TEST_IOH001_JAVALANGCLASS_TAG]");
		return JNI_FALSE;
	}


	rc =  (*jvmti_env)->IterateOverHeap(jvmti_env, JVMTI_HEAP_OBJECT_TAGGED, iohHeapObjectCallback, &data);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to jvmtiIterateOverInstancesOfClass");
		return JNI_FALSE;
	}

       
 	/* Did we find ioh001 Class ? */
	if (data.foundIoh001Class == 0) {
		error(env, JVMTI_ERROR_NONE, "klass tagged with JVMTI_TEST_IOH001_CLASS_TAG not found");
		return JNI_FALSE;
	}
 
	/* Did we find java/lang/Class ? */
	if (data.foundJavaLangClass == 0) {
		error(env, JVMTI_ERROR_NONE, "klass tagged with JVMTI_TEST_IOH001_JAVALANGCLASS_TAG not found");
		return JNI_FALSE;
	}
 
	
	return JNI_TRUE;
}


