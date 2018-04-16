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

/* the standard agent test context which lives for the duration of the test - this is supposed to be held for error logging */
static agentEnv * env;
/* set the first time we run the test since we only need one sample */
static jboolean testDidRun;
/* set while the test is running to ensure that we don't fall into any sort of recursion */
static jboolean alreadyInTest;

static void JNICALL objectWasAllocated(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jobject object, jclass object_klass, jlong size);
static jint JNICALL followReference(jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* reference_info, jlong class_tag, jlong referrer_class_tag, jlong size, jlong* tag_ptr, jlong* referrer_tag_ptr, jint length, void* user_data);

#define JAVA_LANG_CLASS "Ljava/lang/Class;"


jint JNICALL
evmoa001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;

	env = agent_env;

	/* Set the CompiledMethodLoad event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.VMObjectAlloc = objectWasAllocated;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(agent_env, err, "Failed to set callback for CompiledMethodLoad events");
		return JNI_ERR;
	}

	/* we require the can_generate_vm_object_alloc_events capability if we want to enable allocation callbacks */
	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_vm_object_alloc_events = 1;
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(agent_env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	/* Enable the JVMTI_EVENT_VM_OBJECT_ALLOC callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(agent_env, err, "Failed to enable VM Object Alloc event");
		return JNI_ERR;
	}

	testDidRun = JNI_FALSE;
	alreadyInTest = JNI_FALSE;
	
	return JNI_OK;
}

static void JNICALL
objectWasAllocated(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jobject object, jclass object_klass, jlong size)
{
	if (!alreadyInTest) {
		/* we are not in the recursive case so run the test */
		jvmtiError err;
		jvmtiHeapCallbacks heapCallbacks;
		char *declaringClassName = NULL;
		
		/* protect from recursive callback */
		alreadyInTest = JNI_TRUE;
		/* setup the callback structure and then follow all references from this object */
		memset(&heapCallbacks, 0, sizeof(jvmtiHeapCallbacks));
		heapCallbacks.heap_reference_callback = followReference;
			
		err = (*jvmti_env)->FollowReferences(jvmti_env, 0, NULL, object, &heapCallbacks, NULL);
		if (JVMTI_ERROR_NONE != err) {
			error(env, err, "Failed to follow references from new object instance");
		}
		/* set the flag that this test ran at least once so that the Java code will know that this was a valid test run */
		testDidRun = JNI_TRUE;
		/* test complete so undo protection */
		alreadyInTest = JNI_FALSE;
	}
}

static jint JNICALL
followReference(jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* reference_info, jlong class_tag, jlong referrer_class_tag, jlong size, jlong* tag_ptr, jlong* referrer_tag_ptr, jint length, void* user_data)
{
	/* don't actually do anything.  If we don't crash by the time all these callbacks have been made, we passed the test.  Just return the "continue" bit */
	return JVMTI_VISIT_OBJECTS;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_eventVMObjectAllocation_evmoa001_didTestRun(JNIEnv *jni_env, jclass cls)
{
	return testDidRun;
}
