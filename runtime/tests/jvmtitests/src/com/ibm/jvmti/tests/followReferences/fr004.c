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

#include "fr.h"

static agentEnv * _agentEnv;

static int res_c1 = 0;
static int res_c2 = 0;
static int res_c3 = 0;
static int res_count = 0;

static jvmtiTestFollowRefsUserData userData;

jint JNICALL
fr004(agentEnv * env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	if (!ensureVersion(env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}   
       
	_agentEnv = env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}
	
	tagManager_initialize(env);
	
	return JNI_OK;
}


static jint JNICALL 
testFollowReferences_primitiveFieldCallback(jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* referrer_info, jlong class_tag, jlong* tag_ptr, jvalue value, jvmtiPrimitiveType value_type, void* user_data)
{
	
	if (*tag_ptr != 0L) {
		queueTag(*tag_ptr);
		queueTag(class_tag);
		printf("testFollowReferences_primitiveFieldCallback tag %llx:%llx\n", class_tag, *tag_ptr);
				
		if (value_type == JVMTI_PRIMITIVE_TYPE_INT) {
			if (value.i == 100)
				res_c1 = 100;
			if (value.i == 200)
				res_c2 = 200;
			if (value.i == 300)
				res_c3 = 300;			
		}
		
		res_count++;
		
	} else {
		if (class_tag != 0L) {
			printf("testFollowReferences_primitiveFieldCallback Class tag %llx\n", class_tag);
		}
	}
	
	return JVMTI_VISIT_OBJECTS;
}



jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_fr004_followObjectPrimitiveFields(JNIEnv * jni_env, jclass klass, jobject initialObject, jclass filterClass)
{
	jvmtiError err;
	jvmtiHeapCallbacks callbacks;	
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
	jint errorCount;
	jint filterTag = JVMTI_HEAP_FILTER_UNTAGGED | JVMTI_HEAP_FILTER_CLASS_UNTAGGED;
	
	errorCount = getErrorCount();
	
	/* Test Follow References  Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	memset(&userData, 0x00, sizeof(jvmtiTestFollowRefsUserData));	
	callbacks.primitive_field_callback = testFollowReferences_primitiveFieldCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, filterTag, filterClass, initialObject, &callbacks, (void *) &userData);
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "FollowReferences failed");
		return JNI_FALSE;		
	}

	if (errorCount != getErrorCount()) {
		return JNI_FALSE;
	} 

	if (res_c1 != 100 || res_c2 != 200 || res_c3 != 300) {
		error(_agentEnv, JVMTI_ERROR_INTERNAL, "Received no primitive field callback for one or more of the expected fields");
		return JNI_FALSE;
	}
		
	if (res_count != 3) {
		error(_agentEnv, JVMTI_ERROR_INTERNAL, "Expected 3 primitive field callbacks, got %d", res_count);
		return JNI_FALSE;
	}
	
	return JNI_TRUE;
	 
}
