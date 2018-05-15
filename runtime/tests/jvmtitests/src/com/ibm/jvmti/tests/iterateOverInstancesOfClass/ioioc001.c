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

static agentEnv * _env;                                                    

jint JNICALL
ioioc001(agentEnv * env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	_env = env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}		

	return JNI_OK;
}


#define JVMTI_TEST_IOIOC001_CLASS_TAG		0xc0fe
#define JVMTI_TEST_IOIOC001_SUBCLASS_TAG	0xc0de

typedef struct ioioc001_data {
	int foundClass;
	int foundSubClass;
} ioioc001_data;



static jvmtiIterationControl JNICALL
heapObjectCallback(jlong class_tag, jlong size, jlong * tag_ptr, void * user_data)
{
        ioioc001_data * data = (ioioc001_data *) user_data;

	if (*tag_ptr == JVMTI_TEST_IOIOC001_CLASS_TAG) {
         	data->foundClass = 1;
	}
 
	if (*tag_ptr == JVMTI_TEST_IOIOC001_SUBCLASS_TAG) {
         	data->foundSubClass = 1;
	}
 
	return JVMTI_ITERATION_CONTINUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateOverInstancesOfClass_ioioc001_checkSubclasses(JNIEnv * jni_env,
									      jclass clazz,
									      jclass klass,
									      jobject klassObject, 
									      jobject subKlassObject, 
									      jchar className, 
									      jchar subClassName)
{
	jvmtiError rc;
	jvmtiEnv * jvmti_env = _env->jvmtiEnv;
	ioioc001_data data;

	data.foundClass = 0;
	data.foundSubClass = 0;

	rc = (*jvmti_env)->SetTag(jvmti_env, klassObject, JVMTI_TEST_IOIOC001_CLASS_TAG);
	if (rc != JVMTI_ERROR_NONE) {
		error(_env, rc, "Failed to SetTag [JVMTI_TEST_IOIOC001_CLASS_TAG]");
		return JNI_FALSE;
	}

	rc = (*jvmti_env)->SetTag(jvmti_env, subKlassObject, JVMTI_TEST_IOIOC001_SUBCLASS_TAG);
	if (rc != JVMTI_ERROR_NONE) {
		error(_env, rc, "Failed to SetTag [JVMTI_TEST_IOIOC001_SUBCLASS_TAG]");
		return JNI_FALSE;
	}

	rc =  (*jvmti_env)->IterateOverInstancesOfClass(jvmti_env, klass, JVMTI_HEAP_OBJECT_EITHER, heapObjectCallback, &data);
	if (rc != JVMTI_ERROR_NONE) {
		error(_env, rc, "Failed to jvmtiIterateOverInstancesOfClass");
        	return JNI_FALSE;
	}

	/* Did we find both the class and subclass? */
	if (data.foundClass && data.foundSubClass) {
        	return JNI_TRUE;
	}

        if (data.foundClass == 0) {
         	error(_env, JVMTI_ERROR_NONE, "klass tagged with JVMTI_TEST_IOIOC001_CLASS_TAG not found");
	}
 
	if (data.foundSubClass == 0) {
         	error(_env, JVMTI_ERROR_NONE, "klass tagged with JVMTI_TEST_IOIOC001_SUBCLASS_TAG not found");
	}
 
	return JNI_FALSE;
}


