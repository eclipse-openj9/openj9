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

#if 0

static agentEnv * _agentEnv;



typedef struct testFollowReferencesData {
	JavaVM      * vm;
	jvmtiEnv    * jvmti_env;
	JNIEnv      * jni_env;
} testFollowReferencesData;



static jint
testFollowReferences(JavaVM * vm, jvmtiEnv * jvmti_env, char *args)
{
	jvmtiError err;
	jobject objValue;

	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	
	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	capabilities.can_generate_method_entry_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to add capabilities"));
	}						


	/* Set the classLoad event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.MethodEntry = testFollowReferences_methodEnter;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to set callbacks"));
		goto done;
	}

 
	/* Enable the classLoad callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to enable method entry"));
		goto done;
	} 


done:
	printf ("OUT\n");
	j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, ""));

	return err;
}


static void JNICALL
testFollowReferences_methodEnter(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method)
{
	jvmtiError err;
	jclass clazz;
	jclass filterClazz;
	unsigned char *methName = NULL; 
	jvmtiHeapCallbacks callbacks;
	testFollowReferencesData userData;

	methName = testForceEarlyReturn_getMethodName(jvmti_env, method);

	if (strncmp("LTestFollowReferencesSub;.run()I", methName, strlen("LTestFollowReferencesSub;.run()I"))) {
		goto done;
	}

	clazz = (*jni_env)->FindClass(jni_env, "TestFollowReferences");
	if (NULL == clazz) {
		j9tty_printf(PORTLIB, "err: %s\n", errorName(jvmti_env, NULL, err, "\t"));
		goto done;
	}

	/* Drop a tag on this class object so we are able to recognize it within the callback */
	err = (*jvmti_env)->SetTag(jvmti_env, clazz, 0xc0decafe);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "err: %s\n", errorName(jvmti_env, NULL, err, "\t"));
		goto done;
	} 

	/* user data to be passed through to the callbacks */
	userData.jvmti_env = jvmti_env;
	userData.jni_env = jni_env;

	filterClazz = clazz;

	/* Test Follow References  Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.heap_reference_callback = testFollowReferences_callback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, 0, NULL, filterClazz, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "err: %s\n", errorName(jvmti_env, NULL, err, "\t"));
		goto done;
	}

	/* Test Array Primitive Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.array_primitive_value_callback = testFollowReferences_arrayPrimitiveCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, 0, NULL, filterClazz, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "err: %s\n", errorName(jvmti_env, NULL, err, "\t"));
		goto done;
	}


	/* Test Primitive Field Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.primitive_field_callback = testFollowReferences_primitiveFieldCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, 0, NULL, filterClazz, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "err: %s\n", errorName(jvmti_env, NULL, err, "\t"));
		goto done;
	}


	/* Test String Primitive Value Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.string_primitive_value_callback = testFollowReferences_stringPrimitiveCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, 0, NULL, filterClazz, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "err: %s\n", errorName(jvmti_env, NULL, err, "\t"));
		goto done;
	}


done:	
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) methName);

	return;
}


static jint JNICALL
testFollowReferences_callback(jvmtiHeapReferenceKind refKind,
			      const jvmtiHeapReferenceInfo* refInfo,
			      jlong classTag,
			      jlong referrerClassTag,
			      jlong size,
			      jlong* tagPtr,
			      jlong* referrerTagPtr,
			      jint length,
			      void* userData)

{
	if (refKind == JVMTI_HEAP_REFERENCE_STACK_LOCAL) {
		printf("\tdepth=0x%x  method=0x%x  location=0x%x slot=0x%x\n",
		       refInfo->stack_local.depth,
		       refInfo->stack_local.method,
		       refInfo->stack_local.location,
		       refInfo->stack_local.slot);
	}


	return JVMTI_VISIT_OBJECTS;
}



static jint JNICALL 
testFollowReferences_arrayPrimitiveCallback(jlong class_tag, jlong size, jlong* tag_ptr, jint element_count, jvmtiPrimitiveType element_type, const void* elements, void* user_data)
{
}

static jint JNICALL 
testFollowReferences_primitiveFieldCallback(jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* referrer_info, jlong class_tag,jlong* tag_ptr, jvalue value, jvmtiPrimitiveType value_type, void* user_data)
{
}

static jint JNICALL 
testFollowReferences_stringPrimitiveCallback(jlong class_tag, jlong size, jlong* tag_ptr, const jchar* value, jint value_length, void* user_data)
{
}
 
#endif
