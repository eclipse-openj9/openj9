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
#include "ibmjvmti.h"

static void JNICALL cbGCCycleStart(jvmtiEnv *jvmti_env, ...);
static void JNICALL cbGCCycleFinish(jvmtiEnv *jvmti_env, ...);

static agentEnv* env;

static jlong gcStartNs = 0;
jvmtiExtensionFunction   getHeapFreeMemory = NULL;
jvmtiExtensionFunction   getHeapTotalMemory = NULL;

jint cycleStartCount = 0;
jint cycleEndCount = 0;

jint JNICALL
ghftm001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jint extensionCount;
	jint extensionStartEventIndex = -1;
	jint extensionFinishEventIndex = -1;
	jvmtiExtensionEventInfo *extensionEvents;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	jvmtiError err;
	int i;

	env = agent_env;

	if (!ensureVersion(agent_env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}						

	/* Hook the CYCLE extension events */
	err = (*jvmti_env)->GetExtensionEvents(jvmti_env, &extensionCount, &extensionEvents);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionEvents");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
#ifdef COM_IBM_GARBAGE_COLLECTION_CYCLE_START	
		if (strcmp(extensionEvents[i].id, COM_IBM_GARBAGE_COLLECTION_CYCLE_START) == 0) {
			extensionStartEventIndex = extensionEvents[i].extension_event_index;
		}
#endif		
		
#ifdef COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH		
		if (strcmp(extensionEvents[i].id, COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH) == 0) {
			extensionFinishEventIndex = extensionEvents[i].extension_event_index;
		}
#endif		
	}

	if (extensionStartEventIndex == -1) {
		error(env, err, "COM_IBM_GARBAGE_COLLECTION_CYCLE_START extension event not found");
		goto bailOut;
	}
	
	if (extensionFinishEventIndex == -1) {
		error(env, err, "COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH extension event not found");
		goto bailOut;
	}
	
	err = (*jvmti_env)->SetExtensionEventCallback(jvmti_env, extensionStartEventIndex, (jvmtiExtensionEvent) cbGCCycleStart); 
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed SetExtensionEventCallback");
		goto bailOut;
	}
	
	err = (*jvmti_env)->SetExtensionEventCallback(jvmti_env, extensionFinishEventIndex, (jvmtiExtensionEvent) cbGCCycleFinish); 
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed SetExtensionEventCallback");
		goto bailOut;
	}
	
	/* Cache the extension heap query functions */
	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionEvents");
		return JNI_ERR;
	}
	
	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_GET_HEAP_FREE_MEMORY) == 0) {
			getHeapFreeMemory = extensionFunctions[i].func;
		}
		
		if (strcmp(extensionFunctions[i].id, COM_IBM_GET_HEAP_TOTAL_MEMORY) == 0) {
			getHeapTotalMemory = extensionFunctions[i].func;
		}
	}
	
	if (getHeapFreeMemory == NULL) {
		error(env, err, "COM_IBM_GET_HEAP_FREE_MEMORY extension event not found");
		goto bailOut;
	}
	
	if (getHeapTotalMemory == NULL) {
		error(env, err, "COM_IBM_GET_HEAP_TOTAL_MEMORY extension event not found");
		goto bailOut;
	}
	
	return JNI_OK;
	
bailOut:
	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) extensionEvents);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension events");
	}
	
	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
	}
	
	return JNI_ERR;
}

static void JNICALL
cbGCCycleStart(jvmtiEnv *jvmti_env, ...)
{
	tprintf(env, 0, "Starting GC\n");
	cycleStartCount += 1;
}

static void JNICALL
cbGCCycleFinish(jvmtiEnv *jvmti_env, ...)
{
	jlong heapFree;
	jlong heapTotal;
	jvmtiError err;
	
	err = getHeapFreeMemory(jvmti_env, &heapFree);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Call to getHeapFreeMemory failed");
	}
	
	err = getHeapTotalMemory(jvmti_env, &heapTotal);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Call to getHeapTotalMemory failed");
	}
	tprintf(env, 0, "Finishing GC heapFree = %d MB, heapTotal = %d MB\n", (jint)heapFree/1024/1024, (jint)heapTotal/1024/1024);
	cycleEndCount += 1;
}

jlong JNICALL
Java_com_ibm_jvmti_tests_getHeapFreeTotalMemory_ghftm001_getHeapFreeMemory(JNIEnv *jni_env, jclass klass)
{
	jlong heapFree;
	jvmtiError err = getHeapFreeMemory(env->jvmtiEnv, &heapFree);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Call to getHeapFreeMemory failed");
		return -1;
	}
	
	return heapFree;
}

jlong JNICALL
Java_com_ibm_jvmti_tests_getHeapFreeTotalMemory_ghftm001_getHeapTotalMemory(JNIEnv *jni_env, jclass klass)
{
	jlong heapTotal;
	jvmtiError err = getHeapTotalMemory(env->jvmtiEnv, &heapTotal);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Call to getHeapTotalMemory failed");
		return -1;
	}
	
	return heapTotal;
}

jint JNICALL
Java_com_ibm_jvmti_tests_getHeapFreeTotalMemory_ghftm001_getCycleStartCount(JNIEnv *jni_env, jclass klass)
{
	return cycleStartCount;
}

jint JNICALL
Java_com_ibm_jvmti_tests_getHeapFreeTotalMemory_ghftm001_getCycleEndCount(JNIEnv *jni_env, jclass klass)
{
	return cycleEndCount;
}
