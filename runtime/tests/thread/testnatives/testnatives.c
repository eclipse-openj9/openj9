/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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
#if ! (defined(WIN32) || defined(J9ZOS390))
#include <pthread.h>
#include <sched.h>
#endif /* ! (defined(WIN32) || defined(J9ZOS390)) */
#include "omrcfg.h"
#include "jni.h"
#include "j9port.h"
#include "testnatives.h"
#include "vmi.h"
#include "ibmjvmti.h"
#include "jlm.h"

#ifdef J9VM_THR_LOCK_NURSERY
#include "lockNurseryUtil.h"
#endif

#include "thrdsup.h"

#define INTERNAL_ERROR						-1
#define FAILED_TO_GET_EXTENSION_FUNCTIONS 	-2
#define FAILED_TO_FIND_EXTENSION 			-3

static jvmtiEnv * jvmti_env = NULL;

static jint getSchedulingPolicy(J9VMThread *targetVMThread);
static jint getSchedulingPriority(J9VMThread *targetVMThread);
#if defined(LINUX)
static J9VMThread *getGCAlarmThread(J9VMThread *vmThread, J9JavaVM *javaVM);
#endif /* defined(LINUX) */

void JNICALL
Agent_OnUnload(JavaVM * vm)
{

}


JNIEXPORT jint JNICALL
Agent_Prepare(JavaVM * vm, char *phase, char * options, void * reserved)
{
	jint rc;
	jvmtiError err;
	jvmtiCapabilities capabilities;

	rc = (*vm)->GetEnv(vm, (void **) &jvmti_env, JVMTI_VERSION_1_1);
	if (rc != JNI_OK) {
		if ((rc != JNI_EVERSION) || (((*vm)->GetEnv(vm, (void **) &jvmti_env, JVMTI_VERSION_1_0)) != JNI_OK)) {
			return JNI_ERR;
		}
	}

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		return JNI_ERR;
	}

	return JNI_OK;


}

jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	return Agent_Prepare(vm, "Agent_OnLoad", options, reserved);
}

jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	return Agent_Prepare(vm, "Agent_OnAttach", options, reserved);
}


/**
 * Native used to set the default spin counts
 *
 * @param env the JNIEnv
 * @param obj the object on which the method was invoked (this will be a dummy value for the tests in which this method is invoked)
 * @param value the value to return
 * @returns the value passed in
 */
void JNICALL
Java_com_ibm_thread_TestNatives_setSpinCounts(JNIEnv* env, jobject obj, jlong spin1, jlong spin2, jlong yield,
											  jlong tryEnterSpin1, jlong tryEnterSpin2, jlong tryEnterYield,
											  jlong threeTierSpinCount1, jlong threeTierSpinCount2, jlong threeTierSpinCount3)
{
	J9JavaVM *vm = ((J9VMThread *) env)->javaVM;
	JavaVM* javaVM = (JavaVM*)vm;
	J9ThreadEnv* threadEnv = NULL;
	(*javaVM)->GetEnv(javaVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

#if defined(OMR_THR_THREE_TIER_LOCKING)
	**(UDATA **)threadEnv->global("defaultMonitorSpinCount1") = (UDATA) threeTierSpinCount1;
	**(UDATA **)threadEnv->global("defaultMonitorSpinCount2") = (UDATA) threeTierSpinCount2;
	**(UDATA **)threadEnv->global("defaultMonitorSpinCount3") = (UDATA) threeTierSpinCount3;
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
	
	vm->thrMaxSpins1BeforeBlocking = (UDATA) spin1;
	vm->thrMaxSpins2BeforeBlocking = (UDATA) spin2;
	vm->thrMaxYieldsBeforeBlocking = (UDATA) yield;

	vm->thrMaxTryEnterSpins1BeforeBlocking = (UDATA) tryEnterSpin1;
	vm->thrMaxTryEnterSpins2BeforeBlocking = (UDATA) tryEnterSpin2;
	vm->thrMaxTryEnterYieldsBeforeBlocking = (UDATA) tryEnterYield;

	return;
}

#define TESTNATIVES_SCHED_OTHER ((jint) 1)
#define TESTNATIVES_SCHED_RR ((jint) 2)
#define TESTNATIVES_SCHED_FIFO ((jint) 3)

/**
 * Helper function used by Java_j9vm_test_thread_TestNatives_* JNI functions
 * to get the TESTNATIVES_SCHED_* integer representing the scheduling policy
 * in use by a J9VMThread
 * 
 * @param targetVMThread[in]  A pointer to the J9VMThread for which we'll
 * 							  retrieve the scheduling policy
 * 							  
 * @returns  One of the TESTNATIVES_SCHED_* integers representing the scheduling policy of the
 * 			 targetVMThreads's backing OS thread or -1 if an error occurred
 */
static jint
getSchedulingPolicy(J9VMThread *targetVMThread)
{
#if defined(WIN32) || defined(J9ZOS390)
	/* This function should never be called on this platform */
	return (jint)-1;
#else
	jint schedulingPolicy = -1;
	struct sched_param schedParam;
	omrthread_t targetJ9Thread = targetVMThread->osThread;
	OSTHREAD handle = targetJ9Thread->handle;
	int policy = 0;

	if(0 == pthread_getschedparam(handle, &policy, &schedParam)) {
		switch (policy) {
		case SCHED_OTHER:
			schedulingPolicy = TESTNATIVES_SCHED_OTHER;
			break;
		case SCHED_RR:
			schedulingPolicy = TESTNATIVES_SCHED_RR;
			break;
		case SCHED_FIFO:
			schedulingPolicy = TESTNATIVES_SCHED_FIFO;
			break;
		default:
			break; /* FAIL */
		}
	}

	return schedulingPolicy;
#endif /* defined(WIN32) || defined(J9ZOS390) */
}

/**
 * Helper function used by Java_j9vm_test_thread_TestNatives_* JNI functions
 * to get the the scheduling priority in use by a J9VMThread
 * 
 * @param targetVMThread[in]  A pointer to the J9VMThread for which we'll
 * 							  retrieve the scheduling priority
 * 							  
 * @returns  The scheduling priority of the targetVMThreads's backing
 * 			 OS thread, different platforms return different error values
 */
static jint
getSchedulingPriority(J9VMThread *targetVMThread)
{
#if defined(WIN32)
	omrthread_t targetJ9Thread = targetVMThread->osThread;
	return (jint)GetThreadPriority(targetJ9Thread->handle);
#elif defined(J9ZOS390)
	/* this function should never be called on this platform */
	return (jint) -1; /* error value */
#else
	jint schedulingPriority = -1; /* error value */
	struct sched_param schedParam;
	omrthread_t targetJ9Thread = targetVMThread->osThread;
	OSTHREAD handle = targetJ9Thread->handle;
	int policy = 0;

	if(0 == pthread_getschedparam(handle, &policy, &schedParam)) {
		schedulingPriority = (jint)schedParam.sched_priority;
	}

	return schedulingPriority;
#endif /* defined(WIN32) */
}

/**
 * Native used to retrieve the OS thread's scheduling policy
 *
 * @param env[in]  The JNIEnv
 * @param clazz[in]  The class on which the method was invoked
 * @param javaThread[in]  The java thread for which we want to retrieve the scheduling policy
 * @returns  One of the TESTNATIVES_SCHED_* integers representing the scheduling policy of the 
 * 			 javaThread's backing OS thread or -1 if an error occurred
 */
jint JNICALL
Java_j9vm_test_thread_TestNatives_getSchedulingPolicy(JNIEnv* env, jclass clazz, jobject javaThread)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9Object *javaThreadObject = NULL;
	J9VMThread *targetVMThread = NULL;
	jint schedulingPolicy = -1;

	javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);

	javaThreadObject = J9_JNI_UNWRAP_REFERENCE(javaThread);
	targetVMThread = (J9VMThread *)J9VMJAVALANGTHREAD_THREADREF(vmThread, javaThreadObject);
	
	javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (NULL != targetVMThread) {
		schedulingPolicy = getSchedulingPolicy(targetVMThread);
	}

	return schedulingPolicy;
}

/**
 * Native used to retrieve the OS thread's scheduling priority
 *
 * @param env[in]  The JNIEnv
 * @param clazz[in]  The class on which the method was invoked
 * @param javaThread[in]  The java thread for which we want to retrieve the scheduling policy
 * 
 * @returns  The scheduling priority of the javaThread's backing OS thread 
 * 			 or -1 if an error occurred (except on Windows where -1 is a valid priority)
 */
jint JNICALL
Java_j9vm_test_thread_TestNatives_getSchedulingPriority(JNIEnv* env, jclass clazz, jobject javaThread)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9Object *javaThreadObject = NULL;
	J9VMThread *targetVMThread = NULL;
	jint schedulingPriority = -1;

	javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);

	javaThreadObject = J9_JNI_UNWRAP_REFERENCE(javaThread);
	targetVMThread = (J9VMThread *)J9VMJAVALANGTHREAD_THREADREF(vmThread, javaThreadObject);

	javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (NULL != targetVMThread) {
		schedulingPriority = getSchedulingPriority(targetVMThread);
	}

	return schedulingPriority;
}

#if defined(LINUX)
/**
 * Helper method used by the Java_j9vm_test_thread_TestNatives_getGCAlarmScheduling*
 * natives to retrieve the GC Alarm thread
 * 
 * @param vmThread[in] The current J9VMThread
 * @param javaVM[in] A pointer to the Java VM
 * 
 * @returns A J9VMThread pointer to the GC Alarm thread or NULL if none was found
 */
static J9VMThread *
getGCAlarmThread(J9VMThread *vmThread, J9JavaVM *javaVM)
{
	J9VMThread *gcAlarmVMThread = NULL;
	J9VMThread *walkThread = NULL;

	/* lock the vmthread list */
	omrthread_monitor_enter(javaVM->vmThreadListMutex);

	/* walk threads until we find the GC Alarm thread */
	walkThread = javaVM->mainThread;
	do {
		/* The GC Alarm thread is a system daemon thread */
		if (0 != (walkThread->privateFlags & (J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_DAEMON_THREAD))) {
			char* name = getOMRVMThreadName(walkThread->omrVMThread);
			if (0 == strcmp(name, "GC Alarm")) {
				/* found the GC Alarm thread */
				gcAlarmVMThread = walkThread;
				releaseOMRVMThreadName(walkThread->omrVMThread);
				break;
			}
			releaseOMRVMThreadName(walkThread->omrVMThread);
		}

		walkThread = walkThread->linkNext;
	} while (walkThread != javaVM->mainThread);

	/* unlock the vmthread list */
	omrthread_monitor_exit(javaVM->vmThreadListMutex);

	return gcAlarmVMThread;
}
#endif /* defined(LINUX) */

/**
 * Native used to retrieve the GC Alarm thread's scheduling policy
 *
 * @param env[in]  The JNIEnv
 * @param clazz[in]  The class on which the method was invoked
 * 
 * @returns  One of the TESTNATIVES_SCHED_* integers representing the scheduling policy of the
 * 			 GC Alarm thread's backing OS thread or -1 if an error occurred
 */
jint JNICALL
Java_j9vm_test_thread_TestNatives_getGCAlarmSchedulingPolicy(JNIEnv* env, jclass clazz)
{
#if defined(LINUX)
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9VMThread *gcAlarmVMThread = NULL;
	jint schedulingPolicy = -1;

	gcAlarmVMThread = getGCAlarmThread(vmThread, javaVM);

	if (NULL != gcAlarmVMThread) {
		schedulingPolicy = getSchedulingPolicy(gcAlarmVMThread);
	}

	return schedulingPolicy;
#else
	/* this function should never be called on non-Linux platforms */
	return (jint) -1;
#endif /* defined(LINUX) */	
}


/**
 * Native used to retrieve the GC Alarm thread's scheduling priority
 *
 * @param env[in]  The JNIEnv
 * @param clazz[in]  The class on which the method was invoked
 * 
 * @returns  The scheduling priority of the GC Alarm thread's backing
 * 			 OS thread or -1 if an error occurred
 */
jint JNICALL
Java_j9vm_test_thread_TestNatives_getGCAlarmSchedulingPriority(JNIEnv* env, jclass clazz)
{
#if defined(LINUX)
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9VMThread *gcAlarmVMThread = NULL;
	jint schedulingPriority = -1;

	gcAlarmVMThread = getGCAlarmThread(vmThread, javaVM);

	if (NULL != gcAlarmVMThread) {
		schedulingPriority = getSchedulingPriority(gcAlarmVMThread);
	}

	return schedulingPriority;
#else
	/* this function should never be called on non-Linux platforms */
	return (jint) -1;
#endif /* defined(LINUX) */	
}

jint JNICALL
Java_com_ibm_tests_perftests_jlm_JLMInfoHelper_jvmtiJlmSet(JNIEnv * jni_env, jclass klazz, jint mode)
{

	jint count;
	jvmtiExtensionFunctionInfo* extens;
	jvmtiError error;
	int methodOffset;
	BOOLEAN found = FALSE;

	if (jvmti_env == NULL){
		printf("Problem jvmti_env is NULL");
		return FAILED_TO_GET_EXTENSION_FUNCTIONS;
	}

	/* get the table of extension functions through which we call the methods for
	 * jlm
	 */
	error = (*jvmti_env)->GetExtensionFunctions(jvmti_env,&count,&extens);
	if (error != JVMTI_ERROR_NONE){
		return FAILED_TO_GET_EXTENSION_FUNCTIONS;
	}

	/* find the method to call */
	for(methodOffset = 0;methodOffset<count;methodOffset++){
		if (strcmp(extens[methodOffset].id,COM_IBM_SET_VM_JLM)==0){
			found = TRUE;
			break;
		}
	}
	if (found != TRUE){
		return FAILED_TO_FIND_EXTENSION;
	}

	/* call the method */
	error = extens[methodOffset].func(jvmti_env, mode);

	/**
	 * free the array of extension functions
	 */
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) extens);

	return error;
}

jobject JNICALL
Java_com_ibm_tests_perftests_jlm_JLMInfoHelper_jvmtiJlmDump(JNIEnv * jni_env, jclass klazz)
{

	jint count;
	jvmtiExtensionFunctionInfo* extens;
	jvmtiError error;
	int methodOffset;
	BOOLEAN found = FALSE;
	jclass cls;
	jmethodID method;
	jobject result;
	jfieldID resultFieldID;
	jfieldID dumpDataFieldID;
	J9VMJlmDump * dump;
	jbyteArray dumpByteArray;

	/* create the object that we will return the result with */
	cls = (*jni_env)->FindClass(jni_env, "com/ibm/tests/perftests/jlm/JlmData");
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

	/* get the table of extension functions through which we call the methods for
	 * jlm
	 */
	error = (*jvmti_env)->GetExtensionFunctions(jvmti_env,&count,&extens);
	if (error != JVMTI_ERROR_NONE){
		(*jni_env)->SetIntField(jni_env, result, resultFieldID, FAILED_TO_GET_EXTENSION_FUNCTIONS);
		if ((*jni_env)->ExceptionCheck(jni_env)) {
			return NULL;
		}
		return result;
	}

	/* find the method to call */
	for(methodOffset = 0;methodOffset<count;methodOffset++){
		if (strcmp(extens[methodOffset].id,COM_IBM_SET_VM_JLM_DUMP)==0){
			found = TRUE;
			break;
		}
	}
	if (found != TRUE){
		(*jni_env)->SetIntField(jni_env, result, resultFieldID, FAILED_TO_FIND_EXTENSION);
		if ((*jni_env)->ExceptionCheck(jni_env)) {
			return NULL;
		}
		return result;
	}

	/* call the method */
	dump = NULL;
	error = extens[methodOffset].func(jvmti_env,&dump);
	(*jni_env)->SetIntField(jni_env, result, resultFieldID, error);
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}

	/**
	 * put the dump information in to the java object
	 */
	dumpByteArray = (*jni_env)->NewByteArray(jni_env, (jsize)(dump->end - dump->begin));
	(*jni_env)->SetObjectField(jni_env, result, dumpDataFieldID, dumpByteArray);
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}
	(*jni_env)->SetByteArrayRegion(jni_env, dumpByteArray, 0,(jsize)(dump->end - dump->begin),(jbyte*)dump->begin);
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		return NULL;
	}


	/**
	 * free the array of extension functions and the dump data that was returned
	 */
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)dump);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) extens);

	return result;
}

jlong JNICALL
Java_com_ibm_j9_monitor_tests_TestNatives_getLockWordValue(JNIEnv* env, jobject testNativesObject, jobject lockwordObj)
{
	j9object_t obj;
	j9objectmonitor_t *lockEA;
	j9objectmonitor_t lock;
	J9JavaVM *vm = ((J9VMThread *) env)->javaVM;

	vm->internalVMFunctions->internalEnterVMFromJNI((J9VMThread *)env);

	obj = J9_JNI_UNWRAP_REFERENCE(lockwordObj);
#if defined( J9VM_THR_LOCK_NURSERY )
	if (!LN_HAS_LOCKWORD((J9VMThread *)env,obj)) {
		lock = 0 | OBJECT_HEADER_LOCK_INFLATED;
		lockEA = &lock;
	} else
#endif
	{
		lockEA = J9OBJECT_MONITOR_EA((J9VMThread *) env, obj);
		lock = *lockEA;
	}

	vm->internalVMFunctions->internalExitVMToJNI((J9VMThread *)env);

	return (jlong) lock;
}

jlong JNICALL
Java_com_ibm_j9_monitor_tests_TestNatives_getLockWordOffset(JNIEnv* env, jobject testNativesObject, jobject lockwordObj)
{
	j9object_t obj;
	J9JavaVM *vm = ((J9VMThread *) env)->javaVM;
	jlong result = -1;

	vm->internalVMFunctions->internalEnterVMFromJNI((J9VMThread *)env);

	obj = J9_JNI_UNWRAP_REFERENCE(lockwordObj);
	result = (jlong) J9OBJECT_MONITOR_OFFSET((J9VMThread *) env,obj);

	vm->internalVMFunctions->internalExitVMToJNI((J9VMThread *)env);

	return result;
}

jlong JNICALL
Java_com_ibm_j9_monitor_tests_TestNatives_getHeaderSize(JNIEnv* env, jclass clazz)
{
	return sizeof(J9Object);
}

jlong JNICALL
Java_com_ibm_j9_monitor_tests_TestNatives_getLockwordSize(JNIEnv* env, jclass clazz)
{
	return sizeof(j9objectmonitor_t);
}

/**
 * Native used to set aggressive gc policy
 *
 * @param env[in]  The JNIEnv
 * @param clazz[in]  The class on which the method was invoked
 *
 */
void JNICALL
Java_j9vm_test_softmx_TestNatives_setAggressiveGCPolicy(JNIEnv* env, jclass clazz)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;

	javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);

	javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE);

	javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);

}


