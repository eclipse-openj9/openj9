/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "jvmti_test.h"
#include "ibmjvmti.h"

static agentEnv * _agent_env;
static jint       count[4] = {0};

static jvmtiExtensionFunction setMethodSelectiveEntryExitNotification   = NULL;
static jvmtiExtensionFunction clearMethodSelectiveEntryExitNotification = NULL;

static void JNICALL cbMethodExit(jvmtiEnv * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value);
static void JNICALL cbMethodEntry(jvmtiEnv * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method);
static void JNICALL cbMethodEntryExtended(jvmtiEnv  * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method, jint mtype, ...);
static void JNICALL cbMethodExitNoRc(jvmtiEnv * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, ...);

jint JNICALL
smee002(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks          callbacks;
	jvmtiCapabilities            capabilities;
	jint                         extensionCount;
	jvmtiExtensionFunctionInfo * extensionFunctions;
	jint                         extensionEventIndex = -1;
	jvmtiExtensionEventInfo    * extensions;
	jvmtiError                   err;
	int                          i;
	jvmtiExtensionFunction       setVmAndCompilingControlOptions = NULL;

	_agent_env = agent_env;

	/* Find extended functions */

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(_agent_env, err, "Failed GetExtensionFunctions");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_AND_COMPILING_CONTROL_OPTIONS) == 0) {
			setVmAndCompilingControlOptions = extensionFunctions[i].func;
		}
		if (strcmp(extensionFunctions[i].id, COM_IBM_SET_METHOD_SELECTIVE_ENTRY_EXIT_NOTIFY) == 0) {
			setMethodSelectiveEntryExitNotification = extensionFunctions[i].func;
		}
#ifdef COM_IBM_CLEAR_METHOD_SELECTIVE_ENTRY_EXIT_NOTIFY
		if (strcmp(extensionFunctions[i].id, COM_IBM_CLEAR_METHOD_SELECTIVE_ENTRY_EXIT_NOTIFY) == 0) {
			clearMethodSelectiveEntryExitNotification = extensionFunctions[i].func;
		}
#endif		
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to Deallocate extension functions");
		return JNI_ERR;
	}

	if (setVmAndCompilingControlOptions == NULL) {
		error(_agent_env, JVMTI_ERROR_NOT_FOUND, "SetVmAndCompilingControlOptions extension was not found");         
		return JNI_ERR;
	}

	if (setMethodSelectiveEntryExitNotification == NULL) {
		error(_agent_env, JVMTI_ERROR_NOT_FOUND, "setMethodSelectiveEntryExitNotification extension was not found");         
		return JNI_ERR;
	}

	if (clearMethodSelectiveEntryExitNotification == NULL) {
		error(_agent_env, JVMTI_ERROR_NOT_FOUND, "clearMethodSelectiveEntryExitNotification extension was not found");         
		return JNI_ERR;
	}

	/* Enable events */

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_method_entry_events = 1;
	capabilities.can_generate_method_exit_events  = 1;    
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.MethodEntry  = cbMethodEntry; 
	callbacks.MethodExit   = cbMethodExit;   
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to set callbacks for MethodEntry/Exit events");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to enable MethodEntry event");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to enable MethodExit event");
		return JNI_ERR;
	}

	/* Enable extended events  */

	err = (*jvmti_env)->GetExtensionEvents(jvmti_env, &extensionCount, &extensions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(_agent_env, err, "Failed GetExtensionEvents");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensions[i].id, COM_IBM_METHOD_ENTRY_EXTENDED) == 0) {
			extensionEventIndex = extensions[i].extension_event_index;
		}
	}

	if (extensionEventIndex == -1) {
		error(_agent_env, err, "COM_IBM_METHOD_ENTRY_EXTENDED extension event not found");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetExtensionEventCallback(jvmti_env, extensionEventIndex, (jvmtiExtensionEvent)cbMethodEntryExtended); 
	if ( JVMTI_ERROR_NONE != err ) {
		error(_agent_env, err, "Failed SetExtensionEventCallback");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensions[i].id, COM_IBM_METHOD_EXIT_NO_RC) == 0) {
			extensionEventIndex = extensions[i].extension_event_index;
		}
	}

	if (extensionEventIndex == -1) {
		error(_agent_env, err, "COM_IBM_METHOD_EXIT_NO_RC extension event not found");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetExtensionEventCallback(jvmti_env, extensionEventIndex, (jvmtiExtensionEvent)cbMethodExitNoRc); 
	if ( JVMTI_ERROR_NONE != err ) {
		error(_agent_env, err, "Failed SetExtensionEventCallback");
		return JNI_ERR;

	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) extensions);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to Deallocate extension events");
		return JNI_ERR;
	}

	/* Set the selective mode - should fail with JVMTI_ERROR_NOT_AVAILABLE */
	err = (setVmAndCompilingControlOptions)(jvmti_env,
											COM_IBM_ENABLE_SELECTIVE_METHOD_ENTRY_EXIT_NOTIFICATION);

	if (err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(_agent_env, err, "setVmAndCompilingControlOptions did not fail with JVMTI_ERROR_NOT_AVAILABLE as expected");
		return JNI_ERR;
	}

	return JNI_OK;
}


static void JNICALL
cbMethodExit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value)
{
	jvmtiError err;
	char *name_ptr, *signature_ptr, *generic_ptr;

	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, &signature_ptr, &generic_ptr);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to GetMethodName");
		return; 
	}

	if ( !strcmp(name_ptr, "sampleJavaMethod") ) {
		count[0]++; 
	}

	return; 
}


static void JNICALL
cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jmethodID method)
{
	jvmtiError err;
	char *name_ptr, *signature_ptr, *generic_ptr;

	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, &signature_ptr, &generic_ptr);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to GetMethodName");
		return; 
	}

	if (!strcmp(name_ptr, "sampleJavaMethod")) {
		count[1]++; 
	}

	return; 
}


static void JNICALL cbMethodEntryExtended(jvmtiEnv  * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method, jint mtype, ...)
{
	jvmtiError err;
	char *name_ptr, *signature_ptr, *generic_ptr;

	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, &signature_ptr, &generic_ptr);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to GetMethodName");
		return; 
	}

	if (!strcmp(name_ptr, "sampleJavaMethod")) {
		count[2]++; 
	}

	return; 
}


static void JNICALL cbMethodExitNoRc( jvmtiEnv * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, ...)
{
	jvmtiError err;
	char *name_ptr, *signature_ptr, *generic_ptr;

	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, &signature_ptr, &generic_ptr);
	if (err != JVMTI_ERROR_NONE) {
		error(_agent_env, err, "Failed to GetMethodName");
		return; 
	}

	if (!strcmp(name_ptr, "sampleJavaMethod")) {
		count[3]++; 
	}

	return; 
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_selectiveMethodEntryExit_smee002_checkNonSelectiveMethodEntryExit(JNIEnv *jni_env, jclass cls)
{
	if (count[0] == 0) {
        error(_agent_env, JVMTI_ERROR_NONE, "No MethodExit events");
        return JNI_FALSE;
    }

	if (count[1] == 0) {
        error(_agent_env, JVMTI_ERROR_NONE, "No MethodEntry events");
        return JNI_FALSE;
    }

    if (count[2] == 0) {
        error(_agent_env, JVMTI_ERROR_NONE, "No MethodEntryExtended events");
        return JNI_FALSE;
    }

    if (count[3] == 0) {
        error(_agent_env, JVMTI_ERROR_NONE, "No MethodExitNoRc events");
        return JNI_FALSE;
    }

	return JNI_TRUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_selectiveMethodEntryExit_smee002_trySetSelectiveMethodEntryExit(JNIEnv *jni_env, jclass cls)
{
	jmethodID   mid;
	jvmtiError  err;
	JVMTI_ACCESS_FROM_AGENT(_agent_env);

	mid = (*jni_env)->GetStaticMethodID(jni_env, cls, "sampleJavaMethod", "(I)I");
	if ( (*jni_env)->ExceptionOccurred(jni_env) ) {
		(*jni_env)->ExceptionClear(jni_env);
		error(_agent_env, JVMTI_ERROR_NONE, "GetStaticMethodID failed");
		return JNI_FALSE;
	}
	if (mid == NULL) {
		error(_agent_env, JVMTI_ERROR_NONE, "GetStaticMethodID failed");
		return JNI_FALSE;
	}

	err = (setMethodSelectiveEntryExitNotification)(jvmti_env, mid);

	if (err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(_agent_env, err, "setMethodSelectiveEntryExitNotification did not fail as expected");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_selectiveMethodEntryExit_smee002_tryClearSelectiveMethodEntryExit(JNIEnv *jni_env, jclass cls)
{
	jmethodID   mid;
	jvmtiError  err;
	JVMTI_ACCESS_FROM_AGENT(_agent_env);

	mid = (*jni_env)->GetStaticMethodID(jni_env, cls, "sampleJavaMethod", "(I)I");
	if ( (*jni_env)->ExceptionOccurred(jni_env) ) {
		(*jni_env)->ExceptionClear(jni_env);
		error(_agent_env, JVMTI_ERROR_NONE, "GetStaticMethodID failed");
		return JNI_FALSE;
	}
	if (mid == NULL) {
		error(_agent_env, JVMTI_ERROR_NONE, "GetStaticMethodID failed");
		return JNI_FALSE;
	}

	err = (clearMethodSelectiveEntryExitNotification)(jvmti_env, mid);

	if (err != JVMTI_ERROR_NOT_AVAILABLE) {
		error(_agent_env, err, "clearMethodSelectiveEntryExitNotification did not fail as expected");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}
