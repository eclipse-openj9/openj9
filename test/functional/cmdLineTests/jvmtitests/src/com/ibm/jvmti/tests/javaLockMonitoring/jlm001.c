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

#define INTERNAL_ERROR						-1
#define FAILED_TO_GET_EXTENSION_FUNCTIONS 	-2
#define FAILED_TO_FIND_EXTENSION 			-3

#define JLM001_DUMP                          0
#define JLM001_DUMP_STATS                    1

#define JLM001_TAG_VALUE                  1234

static agentEnv * env;
static jvmtiExtensionFunction jlmSet       = NULL;
static jvmtiExtensionFunction jlmDump      = NULL;
static jvmtiExtensionFunction jlmDumpStats = NULL;

static void JNICALL
cbMonitorContendedEntered(jvmtiEnv * jvmti, JNIEnv * jni_env, jthread thread, jobject object);

jint JNICALL
jlm001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks          callbacks;
	jvmtiCapabilities            capabilities;
	jvmtiError                   err;
	jint                         extensionCount;
	jvmtiExtensionFunctionInfo * extensionFunctions;
	int                          i;

	env = agent_env;

	/* Set capabilities and event callbacks */

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	capabilities.can_generate_monitor_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.MonitorContendedEntered = cbMonitorContendedEntered;

    err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set event callbacks");
		return JNI_ERR;
	}

	/* Find the extended functions */

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_JLM) == 0) {
			jlmSet = extensionFunctions[i].func;
		}
		if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_JLM_DUMP) == 0) {
			jlmDump = extensionFunctions[i].func;
		}
		if (strcmp(extensionFunctions[i].id, COM_IBM_JLM_DUMP_STATS) == 0) {
			jlmDumpStats = extensionFunctions[i].func;
		}
	}


	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return JNI_ERR;
	}

	if (jlmSet == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "COM_IBM_SET_VM_JLM extension was not found");         
		return JNI_ERR;
	}

	if (jlmDump == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "COM_IBM_SET_VM_JLM_DUMP extension was not found");         
		return JNI_ERR;
	}

	if (jlmDumpStats == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "COM_IBM_JLM_DUMP_STATS was not found");         
		return JNI_ERR;
	}

	return JNI_OK;
}


jint JNICALL
Java_com_ibm_jvmti_tests_javaLockMonitoring_jlm001_jvmtiJlmSet(JNIEnv * jni_env, jclass klazz, jint mode)							      
{	
	jvmtiError error;
	
	JVMTI_ACCESS_FROM_AGENT(env); 
			
	/* call the method */
	error = (jlmSet)(jvmti_env, mode);
		
	return error;
}


jobject JNICALL
Java_com_ibm_jvmti_tests_javaLockMonitoring_jlm001_jvmtiJlmDump(JNIEnv * jni_env, jclass klazz, jint dumpFunction, jint format)
{
	jvmtiError    error;
	jclass        cls;
	jmethodID     method;
	jobject       result;
	jfieldID      resultFieldID;
	jfieldID      dumpDataFieldID;
	jlm_dump * dump;
	jbyteArray    dumpByteArray;
	int           dumpSize;
	int           dumpOffset;
	
	JVMTI_ACCESS_FROM_AGENT(env);
	
	/* create the object that we will return the result with */
	cls = (*jni_env)->FindClass(jni_env, "com/ibm/jvmti/tests/javaLockMonitoring/jlmresult001");
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}
	
	method = (*jni_env)->GetMethodID(jni_env, cls, "<init>", "()V");
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}
	
	result = (*jni_env)->NewObject(jni_env, cls, method);
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}
	
	resultFieldID = (*jni_env)->GetFieldID(jni_env, cls, "result", "I");
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}
	
	dumpDataFieldID = (*jni_env)->GetFieldID(jni_env, cls, "dumpData", "[B");
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}
					
	/* call the method */

	dump = NULL;
    if (dumpFunction == JLM001_DUMP) {
		/* JlmDump */
		error = (jlmDump)(jvmti_env, &dump);
	} else {
		/* JlmDumpStats */
		error = (jlmDumpStats)(jvmti_env, &dump, format);
	}

	(*jni_env)->SetIntField(jni_env, result, resultFieldID, error);
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}

	if ( error == JVMTI_ERROR_NONE ) { 

		/**
		* put the dump information in to the java object
		*/

		if ( (dumpFunction == JLM001_DUMP_STATS) && (format == COM_IBM_JLM_DUMP_FORMAT_TAGS) ) {
			/* Offset for version and format fields */
			dumpOffset = 8;
			tprintf(env, 300, "JLM dump version %x \n", *(unsigned int *)dump->begin);
			tprintf(env, 300, "format %x \n", *(unsigned int *)(dump->begin + 4));
		} else {
			dumpOffset = 0;
		}

		dumpSize = (int)(dump->end - dump->begin) - dumpOffset;
		tprintf(env, 300, "dumpSize %d\n", dumpSize);

		dumpByteArray = (*jni_env)->NewByteArray(jni_env, dumpSize);
		(*jni_env)->SetObjectField(jni_env, result, dumpDataFieldID, dumpByteArray);
		if ((*jni_env)->ExceptionCheck(jni_env)) {
			return NULL;
		}
		(*jni_env)->SetByteArrayRegion(jni_env, dumpByteArray, 0, dumpSize, (jbyte *)(dump->begin) + dumpOffset);
		if ((*jni_env)->ExceptionCheck(jni_env)) {
			return NULL;
		}	
	
		/**
		* free the dump data that was returned
		*/
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)dump);
	}
	
	return result;
}


static void JNICALL
cbMonitorContendedEntered(jvmtiEnv * jvmti, JNIEnv * jni_env, jthread thread, jobject object )
{
	jvmtiError err;
	jlong      tag;

	err = (*jvmti)->GetTag(jvmti, object, &tag);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to get tag");
	}

	tprintf(env, 300, "tag %lld \n", (unsigned long long)tag); 

    if (tag == 0) {
		err = (*jvmti)->SetTag(jvmti, object, JLM001_TAG_VALUE);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to Set tag");
		}
	}
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_javaLockMonitoring_jlm001_enableMonitoringEvent(JNIEnv * jni_env, jclass klazz)
{

	jvmtiError    err;
	
	JVMTI_ACCESS_FROM_AGENT(env);
	
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable JVMTI_EVENT_MONITOR_CONTENDED_ENTERED");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_javaLockMonitoring_jlm001_disableMonitoringEvent(JNIEnv * jni_env, jclass klazz)
{

	jvmtiError    err;
	
	JVMTI_ACCESS_FROM_AGENT(env);
	
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to disable JVMTI_EVENT_MONITOR_CONTENDED_ENTERED");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

