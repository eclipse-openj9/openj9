/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

/**
 * @file mgmtthread.c
 * @brief Natives for com.ibm.lang.ThreadMXBeanImpl
 *
 * @par Thread Contention Statistics
 * Thread contention statistics are measured in nanoseconds, which are converted
 * to milliseconds when returning info to the java API. Statistics are measured in
 * MONOTONIC time, not calendar time.
 */

#include "jni.h"
#include "j9.h"
#include "j9comp.h"
#include "j9consts.h"
#include "j9jclnls.h"
#include "objhelp.h"
#include "ut_j9jcl.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "jclexception.h"
#include "j2sever.h"
#include "j9cp.h"
#include "jniidcacheinit.h"

#include <string.h>

#include "vmaccess.h"
#include "jclglob.h"

#include "rommeth.h"
#include "stackwalk.h"
#include "HeapIteratorAPI.h"

#define J9OBJECT_FROM_JOBJECT(jobj) (*(j9object_t*) (jobj))
#define J9_THREADINFO_MAX_STACK_DEPTH (0x7fffffff) /* Integer.MAX_VALUE */

typedef union FlexObjectRef {
	j9object_t unsafe;
	jobject safe;
} FlexObjectRef;

typedef struct MonitorInfo {
	IDATA depth;
	UDATA count;
	jclass clazz;
	I_32 identityHashCode;
} MonitorInfo;

typedef struct SynchronizerInfo {
	struct SynchronizerInfo *next;
	FlexObjectRef obj;
} SynchronizerInfo;

typedef struct ThreadInfo {
	jobject thread;
	jlong nativeTID;
	UDATA vmstate;
	jint jclThreadState;
	jlong blockedCount;
	jlong blockedTime;
	jlong waitedCount;
	jlong waitedTime;
	jobject blocker;
	jobject blockerOwner;
	jobjectArray stackTrace;
	
	struct {
		UDATA len;
		UDATA *pcs;
	} stack;
	
	struct {
		UDATA len;
		J9ObjectMonitorInfo *arr_unsafe;
		MonitorInfo *arr_safe;
	} lockedMonitors;

	struct {
		UDATA len;
		SynchronizerInfo *list;
	} lockedSynchronizers;
} ThreadInfo;

typedef struct SynchronizerIterData {
	ThreadInfo *allinfo;
	UDATA allinfolen;
} SynchronizerIterData;

static void handlerContendedEnter(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void handlerContendedEntered(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void handlerMonitorWait(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void handlerMonitorWaited(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

static jlongArray findDeadlockedThreads(JNIEnv *env, UDATA findFlags);

static jlong getThreadID(J9VMThread *currentThread, j9object_t threadObj);
static J9VMThread *getThread(JNIEnv *env, jlong threadID);
static jlong getThreadUserTime(omrthread_t thread);
static jlong getCurrentThreadUserTime(omrthread_t self);

static jint initIDCache(JNIEnv *env);

static ThreadInfo *getArrayOfThreadInfo(JNIEnv *env, jlong *threadIDs, jint numThreads, jint maxStackDepth, jboolean getLockedMonitors, jboolean getLockedSynchronizers);
static IDATA getThreadInfo(J9VMThread *currentThread, J9VMThread *targetThread, ThreadInfo *info, jint maxStackDepth, jboolean getLockedMonitors);
static void getContentionStats(J9VMThread *currentThread, J9VMThread *vmThread, ThreadInfo *tinfo);
static IDATA getStackFramePCs(J9VMThread *currentThread, J9VMThread *targetThread, ThreadInfo *tinfo, jint maxStackDepth);
static IDATA getMonitors(J9VMThread *currentThread, J9VMThread *targetThread, ThreadInfo *tinfo, UDATA stackLen);
static UDATA getSynchronizers(J9VMThread *currentThread, ThreadInfo *allinfo, UDATA allinfolen);
static jvmtiIterationControl getSynchronizersHeapIterator(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *objectDesc, void *userData);

static void freeThreadInfos(J9VMThread *currentThread, ThreadInfo *allinfo, UDATA allinfolen);
static IDATA saveObjectRefs(JNIEnv *env, ThreadInfo *info);

static jobject createThreadInfo(JNIEnv *env, ThreadInfo *tinfo, jsize maxStackDepth);
static jobjectArray createThreadInfoArray(JNIEnv *env, ThreadInfo *allinfo, UDATA allinfolen, jsize maxStackDepth);
static jobjectArray createLockedMonitors(JNIEnv *env, ThreadInfo *tinfo);
static jobjectArray createLockedSynchronizers(JNIEnv *env, ThreadInfo *tinfo);
static jobjectArray createStackTrace(J9VMThread *currentThread, ThreadInfo *tinfo);
static jobjectArray pruneStackTrace(JNIEnv *env, jobjectArray stackTrace, jsize maxStackDepth);
static jboolean isInNative(JNIEnv *env, jobjectArray stackTrace);

static void throwError(J9VMThread * currentThread, UDATA exc);
static jlong findNativeThreadId(J9VMThread *currentThread, jlong threadID);

static void
throwError(J9VMThread * currentThread, UDATA exc)
{
	J9InternalVMFunctions * vmfns = currentThread->javaVM->internalVMFunctions;

	if (exc == J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR) {
		vmfns->setNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		vmfns->setCurrentException(currentThread, exc, NULL);
	}
}

/**
 * Compute (end - start) time interval.
 *
 * Sanity checks to avoid returning negative or unreasonably large intervals if the timer goes backwards.
 *
 * @param[in]	endNS		end of interval in nanoseconds
 * @param[in]	startNS		start of interval in nanoseconds
 * @return 					(end - start) interval in nanoseconds, or 0 if interval is badly defined
 */
U_64
checkedTimeInterval(U_64 endNS, U_64 startNS)
{
	U_64 intervalNS = endNS - startNS;

	/*
	 * endNS could be less than startNS if the timer wrapped.  However, modular arithmetic would
	 * give us a valid positive interval for (endNS - startNS).
	 *
	 * Excessively large intervals (>292 years approximately) indicate the timer has gone backwards.
	 */
	if (intervalNS > (U_64)I_64_MAX) {
		intervalNS = 0;
	}
	return intervalNS;
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_findMonitorDeadlockedThreadsImpl(JNIEnv *env, jobject beanInstance)
{
	jlongArray result;
	
	result = findDeadlockedThreads(env, 0);
	return result;
}

jlongArray JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_findDeadlockedThreadsImpl(JNIEnv *env, jobject beanInstance)
{
	jlongArray result;
	
	result = findDeadlockedThreads(env, 
			J9VMTHREAD_FINDDEADLOCKFLAG_INCLUDESYNCHRONIZERS
			| J9VMTHREAD_FINDDEADLOCKFLAG_INCLUDEWAITING);
	return result;
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getAllThreadIdsImpl(JNIEnv *env, jobject beanInstance)
{
	PORT_ACCESS_FROM_ENV(env);
	jlongArray resultArray;
	jlong *threadIDs;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	jlong threadCount = 0;
	J9VMThread * currentThread;	
	
	enterVMFromJNI((J9VMThread *) env);

	omrthread_monitor_enter(javaVM->vmThreadListMutex);

	/* Allocate the destination array */
	threadIDs = j9mem_allocate_memory(javaVM->totalThreadCount * sizeof(jlong), J9MEM_CATEGORY_VM_JCL);
	if (NULL == threadIDs) {
		omrthread_monitor_exit(javaVM->vmThreadListMutex);
		exitVMToJNI((J9VMThread *) env);
		return NULL;
	}

	/* Grab any thread (mainThread will do) */
	currentThread = javaVM->mainThread;
	/* Loop over all vmThreads until we get back to the starting thread: Count phase */
	threadCount = 0;
	do {
		{
			if ((currentThread->threadObject != NULL) && (J9VMJAVALANGTHREAD_THREADREF((J9VMThread *)env,currentThread->threadObject) != NULL)) {
				/* CMVC 182865 - exclude threads which have not initialized their ID */
				jlong threadID = getThreadID((J9VMThread *)env, (j9object_t)currentThread->threadObject);
				if (((jlong)0) != threadID) {
					threadIDs[threadCount++] = threadID;
				}
			}
		}
	} while ((currentThread = currentThread->linkNext) != javaVM->mainThread);

	omrthread_monitor_exit(javaVM->vmThreadListMutex);

	exitVMToJNI((J9VMThread *) env);

	resultArray = (*env)->NewLongArray(env, (jsize)threadCount);
	if (resultArray != NULL) {
		(*env)->SetLongArrayRegion(env, resultArray, 0, (jsize)threadCount, threadIDs);
	}
	j9mem_free_memory(threadIDs);

	return resultArray;
}

/*
 * TODO This API should be split for current vs arbitrary thread.
 */
jlong JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getThreadCpuTimeImpl(JNIEnv *env, jobject beanInstance, jlong threadID)
{
	jlong cpuTime = -1;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9VMThread *targetThread;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = javaVM->internalVMFunctions;

	vmfns->internalEnterVMFromJNI(currentThread);
	
	/* shortcut for the current thread */
	if (getThreadID(currentThread, (j9object_t)currentThread->threadObject) == threadID) {
		vmfns->internalExitVMToJNI(currentThread);
		return omrthread_get_self_cpu_time(currentThread->osThread);
	}

	/* walk the vmthreads until we find the matching one */
	omrthread_monitor_enter(javaVM->vmThreadListMutex);
	for (targetThread = currentThread->linkNext;
		targetThread != currentThread;
		targetThread = targetThread->linkNext
	) {
		if (targetThread->threadObject != NULL) {
			/* check that thread's ID matches */
			if (getThreadID(currentThread, (j9object_t)targetThread->threadObject) == threadID) {
				{
					/* check if the thread is alive */
					if (J9VMJAVALANGTHREAD_THREADREF(currentThread, targetThread->threadObject) != NULL) {
						cpuTime = omrthread_get_cpu_time(targetThread->osThread);
					}
					break;
				}
			}
		}
	}
	omrthread_monitor_exit(javaVM->vmThreadListMutex);
	vmfns->internalExitVMToJNI(currentThread);

	return cpuTime;
}

/* 
 * TODO This API should be split for current vs arbitrary thread.
 */
jlong JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getThreadUserTimeImpl(JNIEnv *env, jobject beanInstance, jlong threadID)
{
	jlong userTime = -1;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9VMThread *targetThread;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = javaVM->internalVMFunctions;

	vmfns->internalEnterVMFromJNI(currentThread);
	
	/* shortcut for the current thread */
	if (getThreadID(currentThread, (j9object_t)currentThread->threadObject) == threadID) {
		vmfns->internalExitVMToJNI(currentThread);
		return getCurrentThreadUserTime(currentThread->osThread);
	}
	
	/* walk the vmthreads until we find the matching one */
	omrthread_monitor_enter(javaVM->vmThreadListMutex);
	for (targetThread = currentThread->linkNext;
		targetThread != currentThread;
		targetThread = targetThread->linkNext
	) {
		if (targetThread->threadObject != NULL) {
			/* check that thread's ID matches */
			if (getThreadID(currentThread, (j9object_t)targetThread->threadObject) == threadID) {
				{
					/* check if the thread is alive */
					if (J9VMJAVALANGTHREAD_THREADREF(currentThread, targetThread->threadObject) != NULL) {
						userTime = getThreadUserTime(targetThread->osThread);
					}
					break;
				}
			}
		}
	}
	omrthread_monitor_exit(javaVM->vmThreadListMutex);
	vmfns->internalExitVMToJNI(currentThread);
	
	return userTime;
}


jint JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getDaemonThreadCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	jint result = 0;
	{
		omrthread_rwmutex_enter_read( mgmt->managementDataLock );

		result = (jint)mgmt->liveJavaDaemonThreads;

		omrthread_rwmutex_exit_read( mgmt->managementDataLock );
	}

	return result;
}

jint JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getPeakThreadCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	jint result = 0;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	{
		omrthread_rwmutex_enter_read( mgmt->managementDataLock );

		result = (jint)mgmt->peakLiveJavaThreads;

		omrthread_rwmutex_exit_read( mgmt->managementDataLock );
	}

	return result;
}

jint JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getThreadCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	jint result = 0;
	{
		omrthread_rwmutex_enter_read( mgmt->managementDataLock );
	
		result = (jint)mgmt->liveJavaThreads;

		omrthread_rwmutex_exit_read( mgmt->managementDataLock );
	}
	return result;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getTotalStartedThreadCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	jlong result = 0;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	{
		omrthread_rwmutex_enter_read( mgmt->managementDataLock );

		result = (jlong)mgmt->totalJavaThreadsStarted;

		omrthread_rwmutex_exit_read( mgmt->managementDataLock );
	}

	return result;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isThreadCpuTimeSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	jboolean isSupported = JNI_FALSE;

	if (((J9VMThread *)env)->javaVM->managementData->isThreadCpuTimeSupported == 1) {
		isSupported = JNI_TRUE;
	}

	return isSupported;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isThreadContentionMonitoringEnabledImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	return ( mgmt->threadContentionMonitoringFlag != 0 );
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isThreadContentionMonitoringSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isThreadCpuTimeEnabledImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	return (jboolean)mgmt->threadCpuTimeEnabledFlag;
}

void JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_resetPeakThreadCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	{
		omrthread_rwmutex_enter_write( mgmt->managementDataLock );

		mgmt->peakLiveJavaThreads = mgmt->liveJavaThreads;

		omrthread_rwmutex_exit_write( mgmt->managementDataLock );
	}
}

void JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_setThreadContentionMonitoringEnabledImpl(JNIEnv *env, jobject beanInstance, jboolean flag)
{
	PORT_ACCESS_FROM_ENV(env);
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	J9VMThread * currentThread;
	J9HookInterface** vmHooks = javaVM->internalVMFunctions->getVMHookInterface(javaVM);
	
	U_64 timeNowNS = (U_64)j9time_nano_time();

	if (mgmt->threadContentionMonitoringFlag == flag ) {
		return;
	}

	omrthread_monitor_enter(javaVM->vmThreadListMutex);
	omrthread_rwmutex_enter_write( mgmt->managementDataLock );

	mgmt->threadContentionMonitoringFlag = flag;

	if (flag == JNI_FALSE) {
		/* Disconnect all hooks */
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTER, handlerContendedEnter, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, handlerContendedEntered, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_WAIT, handlerMonitorWait, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_WAITED, handlerMonitorWaited, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_PARK, handlerMonitorWait, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_UNPARKED, handlerMonitorWaited, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SLEEP, handlerMonitorWait, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SLEPT, handlerMonitorWaited, NULL);
	}

	/* Grab any thread (mainThread will do) */
	currentThread = javaVM->mainThread;
	/* Loop over all vmThreads until we get back to the starting thread */
	while (TRUE) {
		if (flag == JNI_TRUE) {
			currentThread->mgmtBlockedTimeTotal = 0;
			currentThread->mgmtWaitedTimeTotal = 0;
			/* the blocked/waited start times are initialized when the contention hook is invoked */

		} else {
			/* Update the total times with the time until now */
			if (currentThread->mgmtBlockedStart) {
				currentThread->mgmtBlockedTimeTotal +=
						checkedTimeInterval(timeNowNS, currentThread->mgmtBlockedTimeStart);
			}
			if (currentThread->mgmtWaitedStart) {
				currentThread->mgmtWaitedTimeTotal +=
						checkedTimeInterval(timeNowNS, currentThread->mgmtWaitedTimeStart);
			}
		}
		currentThread->mgmtBlockedStart = JNI_FALSE;
		currentThread->mgmtWaitedStart = JNI_FALSE;

		if ((currentThread = currentThread->linkNext) == javaVM->mainThread) {
			/* Back at starting thread: we're done */
			break;
		}		
	}

	if (flag == JNI_TRUE) {
		/* Connect all hooks */
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTER, handlerContendedEnter, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, handlerContendedEntered, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_WAIT, handlerMonitorWait, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_WAITED, handlerMonitorWaited, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_PARK, handlerMonitorWait, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_UNPARKED, handlerMonitorWaited, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SLEEP, handlerMonitorWait, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SLEPT, handlerMonitorWaited, OMR_GET_CALLSITE(), NULL);
		
	}

	omrthread_rwmutex_exit_write( mgmt->managementDataLock );
	omrthread_monitor_exit(javaVM->vmThreadListMutex);

}

void JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_setThreadCpuTimeEnabledImpl(JNIEnv *env, jobject beanInstance, jboolean flag)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	mgmt->threadCpuTimeEnabledFlag = (U_32)flag;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isCurrentThreadCpuTimeSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	jboolean isSupported = JNI_FALSE;

	if (((J9VMThread *)env)->javaVM->managementData->isCurrentThreadCpuTimeSupported == 1) {
		isSupported = JNI_TRUE;
	}

	return isSupported;
}

jboolean JNICALL 
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isObjectMonitorUsageSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_isSynchronizerUsageSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	return JNI_TRUE;
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getThreadInfoImpl(JNIEnv *env, jobject beanInstance,
	jlong id, jint maxStackDepth)
{
	jboolean getLockedMonitors = JNI_FALSE;
	jboolean getLockedSynchronizers = JNI_FALSE;
	jobject result = NULL;
	ThreadInfo *allinfo = NULL;
	PORT_ACCESS_FROM_ENV(env);

	Trc_JCL_threadmxbean_getThreadInfoImpl6_Entry(env, id, 
			maxStackDepth, getLockedMonitors, getLockedSynchronizers);

	/* This function call ensures that the correct tenant thread is matched with the thread ID */
	allinfo = getArrayOfThreadInfo(env, &id, 1, maxStackDepth, getLockedMonitors, getLockedSynchronizers);
	if (allinfo) {
		if (allinfo[0].thread) {
			result = createThreadInfo(env, &allinfo[0], maxStackDepth);
		}
		j9mem_free_memory(allinfo);
	}

	Trc_JCL_threadmxbean_getThreadInfoImpl6_Exit(env, result);

	return result;
}

jobjectArray JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getMultiThreadInfoImpl(
	JNIEnv *env, jobject beanInstance,
	jlongArray ids, jint maxStackDepth, 
	jboolean getLockedMonitors, jboolean getLockedSynchronizers)
{
	ThreadInfo *allinfo = NULL;
	jobjectArray result = NULL;
	jint numThreads = 0;
	jlong *threadIDs = NULL;
	jboolean isCopy = JNI_FALSE;
	PORT_ACCESS_FROM_ENV(env);
	
	Trc_JCL_threadmxbean_getArrayOfThreadInfo_Entry(
			env, ids, getLockedMonitors, getLockedSynchronizers);

	numThreads = (*env)->GetArrayLength(env, ids);
	
	/* There's a special case when we get a 0 length thread list that
	 * we are still expected to return an array of size zero, not null,
	 * so skip doing anything extra and just return the new array
	 */
	if (0 == numThreads){
		jclass cls = JCL_CACHE_GET(env, CLS_java_lang_management_ThreadInfo);
		result = (*env)->NewObjectArray(env, 0, cls, NULL);
	} else {
		threadIDs = (*env)->GetLongArrayElements(env, ids, &isCopy);
		if (!threadIDs) {
			return NULL;
		}

		/* Threads that belong to other tenants will be filtered out in this function call */
		allinfo = getArrayOfThreadInfo(env, threadIDs, numThreads,
				maxStackDepth,
				getLockedMonitors, getLockedSynchronizers);

		(*env)->ReleaseLongArrayElements(env, ids, threadIDs, JNI_ABORT);

		if (NULL != allinfo) {
			result = createThreadInfoArray(env, allinfo, numThreads, (jsize)maxStackDepth);
			j9mem_free_memory(allinfo);
		}
	}
	
	Trc_JCL_threadmxbean_getArrayOfThreadInfo_Exit(env, result);
	
	return result;
}

jobjectArray JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_dumpAllThreadsImpl(JNIEnv *env, jobject beanInstance,
	jboolean getLockedMonitors, jboolean getLockedSynchronizers, jint maxDepth)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = vm->internalVMFunctions;
	J9VMThread *vmThread = NULL;
	ThreadInfo *allinfo = NULL;
	ThreadInfo *info = NULL;
	jobjectArray result = NULL;
	IDATA exc = 0;
	UDATA allinfolen = 0;
	UDATA numThreads = 0;
	UDATA i;

	Trc_JCL_threadmxbean_dumpAllThreads_Entry(env, getLockedMonitors, getLockedSynchronizers);

	if (initIDCache(env) != JNI_OK) {
		return NULL;
	}

	vmfns->internalEnterVMFromJNI(currentThread);
	vmfns->acquireExclusiveVMAccess(currentThread);

	allinfolen = vm->totalThreadCount;

	/* build array of ThreadInfo */
	if (allinfolen > 0) {
		allinfo = j9mem_allocate_memory(sizeof(ThreadInfo) * allinfolen, J9MEM_CATEGORY_VM_JCL);
		if (!allinfo) {
			exc = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
			goto dumpAll_failWithExclusive;
		}

		memset(allinfo, 0, sizeof(ThreadInfo) * allinfolen);
		numThreads = 0;
		info = allinfo;
		vmThread = vm->mainThread;
		do {
			if (!vmThread) {
				break;
			}

			/* Verify that thread is alive */	
			if (vmThread->threadObject && (J9VMJAVALANGTHREAD_THREADREF(currentThread, vmThread->threadObject) == vmThread)) {
				{
					++numThreads;
					exc = getThreadInfo(currentThread, vmThread, info,
							maxDepth, getLockedMonitors);
					if (exc > 0) {
						freeThreadInfos(currentThread, allinfo, numThreads);
						goto dumpAll_failWithExclusive;
					}
					++info;
				}
			}
			vmThread = vmThread->linkNext;
		} while (vmThread != vm->mainThread);

		if (JNI_TRUE == getLockedSynchronizers) {
			exc = getSynchronizers(currentThread, allinfo, numThreads);
			if (exc > 0) {
				freeThreadInfos(currentThread, allinfo, numThreads);
				goto dumpAll_failWithExclusive;
			}
		} /* if (getLockedSynchronizers == JNI_TRUE) */
	} /* if (allinfolen > 0) */


	for (i = 0; i < numThreads; ++i) {
		/* can't do this the first time through because the thread might be
		 * walking its own stack
		 */
		exc = saveObjectRefs(env, &allinfo[i]);
		if (exc > 0) {
			freeThreadInfos(currentThread, allinfo, numThreads);
			goto dumpAll_failWithExclusive;
		}
	}

	vmfns->releaseExclusiveVMAccess(currentThread);

	for (i = 0; i < numThreads; ++i) {
		/* allocates objects */
		allinfo[i].stackTrace = createStackTrace(currentThread, &allinfo[i]);
		if (!allinfo[i].stackTrace) {
			freeThreadInfos(currentThread, allinfo, numThreads);
			vmfns->internalExitVMToJNI(currentThread);
			return NULL;
		}
	}
	vmfns->internalExitVMToJNI(currentThread);

	result = createThreadInfoArray(env, allinfo, numThreads, (jsize)J9_THREADINFO_MAX_STACK_DEPTH);
	j9mem_free_memory(allinfo);
	
	Trc_JCL_threadmxbean_dumpAllThreads_Exit(env, result);
	return result;

dumpAll_failWithExclusive:
	vmfns->releaseExclusiveVMAccess(currentThread);
	if (exc > 0) {
		throwError(currentThread, exc);
	}
	vmfns->internalExitVMToJNI(currentThread);
	return NULL;
}


/**
 * Allocate and populate an array of ThreadInfo for a given array of threadIDs
 * 
 * @param[in] env
 * @param[in] threadIDs Array of thread IDs. May include dead threads.
 * @param[in] numThreads Length of threadIDs[].
 * @param[in] maxStackDepth A guideline for capping the maximum number of
 * stack frames that need to be walked. The actual number of frames returned 
 * may be greater. The caller must prune the returned stack trace if an 
 * exact maximum is required.
 * @param[in] getLockedMonitors Whether locked monitors should be discovered.
 * @param[in] getLockedSynchronizers Whether locked synchronizers should be discovered.
 * 
 * @return array of ThreadInfo
 * @retval non-NULL success
 * @retval NULL error, an exception is set
 */
static ThreadInfo *
getArrayOfThreadInfo(JNIEnv *env, 
	jlong *threadIDs, jint numThreads, jint maxStackDepth,
	jboolean getLockedMonitors, jboolean getLockedSynchronizers)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = vm->internalVMFunctions;
	ThreadInfo *allinfo = NULL;
	IDATA exc = 0;
	UDATA i;

	if (JNI_TRUE == getLockedMonitors) {
		/* we need the full stack trace to build the locked monitors info */
		maxStackDepth = J9_THREADINFO_MAX_STACK_DEPTH;
	} else if (0 == maxStackDepth) {
		/* we need a stack frame to test whether the thread is in a native */
		maxStackDepth = 1;
	}

	if (initIDCache(env) != JNI_OK) {
		return NULL;
	}

	vmfns->internalEnterVMFromJNI(currentThread);
	vmfns->acquireExclusiveVMAccess(currentThread);

	/** 
	 * @todo Slightly evil.
	 * Overwrites entries in the threadID array with the J9VMThread*s.
	 */
	Assert_JCL_true(sizeof(J9VMThread *) <= sizeof(jlong));

	for (i = 0; (jint)i < numThreads; ++i) {
		J9VMThread *vmThread;
		
		vmThread = getThread(env, threadIDs[i]);
		/* 
		 * Dead threads should not be removed from the array.
		 * They should get a null entry in the corresponding ThreadInfo array.
		 */
		threadIDs[i] = (jlong)(UDATA)vmThread;
	}

	/* build array of ThreadInfo */
	if (numThreads > 0) {
		allinfo = j9mem_allocate_memory(sizeof(ThreadInfo) * numThreads, J9MEM_CATEGORY_VM_JCL);
		if (!allinfo) {
			goto getArray_failWithExclusive;
		}
		memset(allinfo, 0, sizeof(ThreadInfo) * numThreads);
		for (i = 0; (jint)i < numThreads; ++i) {

			if (threadIDs[i]) {
				exc = getThreadInfo(currentThread, (J9VMThread *)(UDATA)threadIDs[i],
						&allinfo[i], maxStackDepth, getLockedMonitors);
				if (exc > 0) {
					freeThreadInfos(currentThread, allinfo, numThreads);
					goto getArray_failWithExclusive;
				}
			} else {
				allinfo[i].thread = NULL;
			}
		}

		if (JNI_TRUE == getLockedSynchronizers) {
			exc = getSynchronizers(currentThread, allinfo, numThreads);
			if (exc > 0) {
				freeThreadInfos(currentThread, allinfo, numThreads);
				goto getArray_failWithExclusive;
			}
		} /* if (getLockedSynchronizers == JNI_TRUE) */
	} /* if (numThreads > 0) */

	for (i = 0; i < (UDATA)numThreads; ++i) {
		/* can't do this the first time through because the thread might be
		 * walking its own stack
		 */
		if (allinfo[i].thread) {
			exc = saveObjectRefs(env, &allinfo[i]);
			if (exc > 0) {
				freeThreadInfos(currentThread, allinfo, numThreads);
				goto getArray_failWithExclusive;
			}
		}
	}

	vmfns->releaseExclusiveVMAccess(currentThread);

	for (i = 0; (jint)i < numThreads; ++i) {
		/* allocates objects, may set exception */
		if (allinfo[i].thread) {
			allinfo[i].stackTrace = createStackTrace(currentThread, &allinfo[i]);
			if (!allinfo[i].stackTrace) {
				freeThreadInfos(currentThread, allinfo, numThreads);
				vmfns->internalExitVMToJNI(currentThread);
				return NULL;
			}
		}
	}
	vmfns->internalExitVMToJNI(currentThread);

	return allinfo;

getArray_failWithExclusive:
	vmfns->releaseExclusiveVMAccess(currentThread);
	if (exc > 0) {
		throwError(currentThread, exc);
	}
	vmfns->internalExitVMToJNI(currentThread);
	return NULL;
}

/**
 * Get the tid of a thread object (equivalent of Thread.getId())
 * 
 * @pre VM access
 * @param[in] currentThread
 * @param[in] threadObj
 * @return threadObj's tid
 */
static jlong
getThreadID(J9VMThread *currentThread, j9object_t threadObj)
{
	return J9VMJAVALANGTHREAD_TID(currentThread, threadObj);
}

/**
 * Maps a threadID (the tid field of the java/lang/Thread object)
 * to its corresponding J9VMThread.
 *
 * @pre VM access
 * @pre The caller must hold vmThreadListMutex.
 * @param[in] env
 * @param[in] threadID
 * @return a J9VMThread
 * @retval NULL the J9VMThread does not exist or is dead
 * @retval non-NULL the J9VMThread corresponding to the tid
 */
static J9VMThread *
getThread(JNIEnv *env, jlong threadID) 
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9VMThread *currentThread;	

	/* Grab any thread (mainThread will do) */
	currentThread = javaVM->mainThread;
	/* Loop over all vmThreads until we get back to the starting thread: look for matching threadID */
	while (TRUE) {
		if ((currentThread->threadObject != NULL) && (getThreadID((J9VMThread *)env, (j9object_t)currentThread->threadObject)  == threadID)) {
			{
				/*
				 * We've found our matching thread, so we're done.
				 * But first we have to check if the thread is alive
				 * i.e. The j.l.Thread object's thread ref != 0
				 */
				if (J9VMJAVALANGTHREAD_THREADREF((J9VMThread *)env,currentThread->threadObject) == NULL) {
					currentThread = NULL;
				}
				return currentThread;
			}
		}

		if ((currentThread = currentThread->linkNext) == javaVM->mainThread) {
			/* Back at starting thread with no match, so return NULL */
			return NULL;
		}		
	}
}

/**
 * Get an arbitrary thread's user mode CPU time.
 * @pre thread must not be NULL
 * @pre thread must be alive
 * @param thread
 * @returns thread's user mode CPU time, in ns
 * @retval -1 function is not supported
 */
static jlong
getThreadUserTime(omrthread_t thread)
{
	jlong userTime = -1;
	
	userTime = omrthread_get_user_time(thread);
	
#if defined(J9ZOS390) || defined(LINUX) || defined(OSX)
	if (-1 == userTime) {
		/* For z/OS, Linux and OSX, user time is not available from the OS.
		 * So provide cpu time in its place.
		 */
		userTime = omrthread_get_cpu_time(thread);
	}
#endif /* ZOS, LINUX or OSX */
		
	return userTime;
}

/**
 * Get the current thread's user mode CPU time.
 * @returns thread's user mode CPU time, in ns
 * @retval -1 function is not supported
 */
static jlong
getCurrentThreadUserTime(omrthread_t self)
{
	jlong userTime = -1;
	
	userTime = omrthread_get_self_user_time(self);
	
#if defined(J9ZOS390) || defined(LINUX) || defined(OSX)
	if (-1 == userTime) {
		/* For z/OS, Linux and OSX, user time is not available from the OS.
		 * So provide cpu time in its place.
		 */
		userTime = omrthread_get_self_cpu_time(self);
	}
#endif /* defined(J9ZOS390) || defined(LINUX) || defined(OSX) */
		
	return userTime;
}

static void 
handlerContendedEnter(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorContendedEnterEvent* event = (J9VMMonitorContendedEnterEvent*)eventData;
	J9JavaVM* vm = event->currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM( vm );

	if(event->currentThread) {
		event->currentThread->mgmtBlockedStart = JNI_TRUE;
		event->currentThread->mgmtBlockedTimeStart = (U_64)j9time_nano_time();
	}
}

static void 
handlerContendedEntered(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorContendedEnteredEvent* event = (J9VMMonitorContendedEnteredEvent*)eventData;
	J9JavaVM* vm = event->currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM( vm );

	if(event->currentThread) {
		if (event->currentThread->mgmtBlockedStart) {
			event->currentThread->mgmtBlockedTimeTotal +=
					checkedTimeInterval((U_64)j9time_nano_time(), event->currentThread->mgmtBlockedTimeStart);
			event->currentThread->mgmtBlockedStart = JNI_FALSE;
		}
	}
}

static void 
handlerMonitorWait(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* currentThread = NULL;
	switch (eventNum){
	case J9HOOK_VM_MONITOR_WAIT: {
			J9VMMonitorWaitEvent* event = (J9VMMonitorWaitEvent*)eventData;
			currentThread = event->currentThread;
			break;
		}
	case J9HOOK_VM_PARK: {
			J9VMParkEvent* event = (J9VMParkEvent*)eventData;
			currentThread = event->currentThread;
			break;
		}
	case J9HOOK_VM_SLEEP: {
			J9VMSleepEvent* event = (J9VMSleepEvent*)eventData;
			currentThread = event->currentThread;
			break;
		}
	default: /* We will do nothing in this case */
		break;
	}

	if(currentThread) {
		PORT_ACCESS_FROM_JAVAVM( currentThread->javaVM );
		currentThread->mgmtWaitedStart = JNI_TRUE;
		currentThread->mgmtWaitedTimeStart = (U_64)j9time_nano_time();
	}
}

static void 
handlerMonitorWaited(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* currentThread = NULL;
	switch (eventNum){
	case J9HOOK_VM_MONITOR_WAITED: {
			J9VMMonitorWaitedEvent* event = (J9VMMonitorWaitedEvent*)eventData;
			currentThread = event->currentThread;
			break;
		}
	case J9HOOK_VM_UNPARKED: {
			J9VMUnparkedEvent* event = (J9VMUnparkedEvent*)eventData;
			currentThread = event->currentThread;
			break;
		}
	case J9HOOK_VM_SLEPT: {
			J9VMSleptEvent* event = (J9VMSleptEvent*)eventData;
			currentThread = event->currentThread;
			break;
		}
	default: /* We will do nothing in this case */
		break;
	}

	if (currentThread){ 
		PORT_ACCESS_FROM_JAVAVM( currentThread->javaVM );
	
		if (currentThread->mgmtWaitedStart) {
			currentThread->mgmtWaitedTimeTotal +=
					checkedTimeInterval((U_64)j9time_nano_time(), currentThread->mgmtWaitedTimeStart);
			currentThread->mgmtWaitedStart = JNI_FALSE;
		}
	}
}

/**
 * Initialize the JCL ID cache with commonly used classes.
 * These classes should be loaded only if ThreadMXBean is used.
 * @pre must not have VM access
 * @todo When should these references be deleted?
 * @param[in] env
 * @return error status
 * @retval JNI_OK success
 * @retval JNI_ENOMEM OutOfMemoryError is set
 * @retval JNI_ERR FindClass() or GetMethodID() failed, an exception is set
 */
static jint
initIDCache(JNIEnv *env)
{
	jclass oom;
	jclass cls;
	jclass gcls;
	jmethodID mid;
	jint err = JNI_OK;
	
	/* isNativeMethod is the last cache member to be set */
	if (JCL_CACHE_GET(env, MID_java_lang_StackTraceElement_isNativeMethod) != NULL) 
		return JNI_OK;

	/* cache OutOfMemoryError */
	oom = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
	if (!oom) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	
	cls = (*env)->FindClass(env, "java/lang/management/ThreadInfo");
	if (!cls) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	if (!(gcls = (*env)->NewGlobalRef(env, cls))) {
		err = JNI_ENOMEM;
		goto initIDCache_fail;
	}
	(*env)->DeleteLocalRef(env, cls);
	JCL_CACHE_SET(env, CLS_java_lang_management_ThreadInfo, gcls);

	mid = (*env)->GetMethodID(env, gcls, "<init>", 
		"(Ljava/lang/Thread;JIZZJJJJ[Ljava/lang/StackTraceElement;Ljava/lang/Object;Ljava/lang/Thread;[Ljava/lang/management/MonitorInfo;[Ljava/lang/management/LockInfo;)V");
	if (mid) {
		JCL_CACHE_SET(env, MID_java_lang_management_ThreadInfo_init, mid);
		JCL_CACHE_SET(env, MID_java_lang_management_ThreadInfo_init_nolocks, NULL);
	}
	if (!mid) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}

	cls = (*env)->FindClass(env, "java/lang/management/MonitorInfo");
	if (!cls) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	if (!(gcls = (*env)->NewGlobalRef(env, cls))) {
		err = JNI_ENOMEM;
		goto initIDCache_fail;
	}
	(*env)->DeleteLocalRef(env, cls);
	JCL_CACHE_SET(env, CLS_java_lang_management_MonitorInfo, gcls);

	mid = (*env)->GetMethodID(env, gcls, "<init>",
			"(Ljava/lang/String;IILjava/lang/StackTraceElement;)V");
	if (!mid) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	JCL_CACHE_SET(env, MID_java_lang_management_MonitorInfo_init, mid);

	cls = (*env)->FindClass(env, "java/lang/Class");
	if (!cls) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	mid = (*env)->GetMethodID(env, cls, "getName", "()Ljava/lang/String;");		
	if (!mid) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	(*env)->DeleteLocalRef(env, cls);
	JCL_CACHE_SET(env, MID_java_lang_Class_getName, mid);

	cls = (*env)->FindClass(env, "java/lang/management/LockInfo");
	if (!cls) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	if (!(gcls = (*env)->NewGlobalRef(env, cls))) {
		err = JNI_ENOMEM;
		goto initIDCache_fail;
	}
	(*env)->DeleteLocalRef(env, cls);
	JCL_CACHE_SET(env, CLS_java_lang_management_LockInfo, gcls);

	mid = (*env)->GetMethodID(env, gcls, "<init>", "(Ljava/lang/Object;)V");
	if (!mid) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	JCL_CACHE_SET(env, MID_java_lang_management_LockInfo_init, mid);

	cls = (*env)->FindClass(env, "java/lang/StackTraceElement");
	if (!cls) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	if (!(gcls = (*env)->NewGlobalRef(env, cls))) {
		err = JNI_ENOMEM;
		goto initIDCache_fail;
	}
	(*env)->DeleteLocalRef(env, cls);
	JCL_CACHE_SET(env, CLS_java_lang_StackTraceElement, gcls);

	mid = (*env)->GetMethodID(env, gcls, "isNativeMethod", "()Z");
	if (!mid) {
		err = JNI_ERR;
		goto initIDCache_fail;
	}
	JCL_CACHE_SET(env, MID_java_lang_StackTraceElement_isNativeMethod, mid);

	(*env)->DeleteLocalRef(env, oom);
	return JNI_OK;

initIDCache_fail:
	gcls = JCL_CACHE_GET(env, CLS_java_lang_StackTraceElement);
	if (NULL != gcls) {
		(*env)->DeleteGlobalRef(env, gcls);
	}
	gcls = JCL_CACHE_GET(env, CLS_java_lang_management_LockInfo);
	if (NULL != gcls) {
		(*env)->DeleteGlobalRef(env, gcls);
	}
	gcls = JCL_CACHE_GET(env, CLS_java_lang_management_MonitorInfo);
	if (NULL != gcls) {
		(*env)->DeleteGlobalRef(env, gcls);
	}
	gcls = JCL_CACHE_GET(env, CLS_java_lang_management_ThreadInfo);
	if (NULL != gcls) {
		(*env)->DeleteGlobalRef(env, gcls);
	}
	if (JNI_ENOMEM == err) {
		(*env)->ThrowNew(env, oom, "initIDCache failed");
	}
	return err;
}

/**
 * Frees portlib-allocated memory from ThreadInfo array.
 *
 * @pre allinfo should be initialized to 0 so that it doesn't contain garbage pointers.
 * @note This function may free() null pointers.
 * 
 * @param[in] currentThread
 * @param[in] allinfo array of ThreadInfo
 * @param[in] allinfolen length of allinfo[]
 * @return n/a
 */
static void
freeThreadInfos(J9VMThread *currentThread, ThreadInfo *allinfo, UDATA allinfolen)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	SynchronizerInfo *sinfo, *next;
	UDATA i;

	for (i = 0; i < allinfolen; ++i) {
		j9mem_free_memory(allinfo[i].stack.pcs);
		j9mem_free_memory(allinfo[i].lockedMonitors.arr_unsafe);
		j9mem_free_memory(allinfo[i].lockedMonitors.arr_safe);
		
		sinfo = allinfo[i].lockedSynchronizers.list;
		while (sinfo) {
			next = sinfo->next;
			j9mem_free_memory(sinfo);
			sinfo = next;
		}
	}
	j9mem_free_memory(allinfo);
}

/**
 * Populate the ThreadInfo for a given VMThread.
 * 
 * @pre VM access.
 * @pre The target thread must be halted.
 * @pre The owner of the target thread's blocking object must not be able to exit. 
 * This usually implies that exclusive VM access is required.
 * 
 * @param[in] currentThread
 * @param[in] targetThread The thread to be examined.
 * @param[out] info The ThreadInfo element to be populated. Must be non-NULL.
 * @param[in] maxStackDepth A guideline for limiting the number of stack frames walked.
 * The returned stack trace may have more frames.
 * @param[in] getLockedMonitors Whether locked monitors should be discovered.
 * 
 * @return error status
 * @retval 0 success
 * @retval >0 error, the index of a known exception
 */
static IDATA
getThreadInfo(J9VMThread *currentThread, J9VMThread *targetThread, ThreadInfo *info,
		jint maxStackDepth, jboolean getLockedMonitors)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = vm->internalVMFunctions;

	j9object_t monitorObject = NULL;
	J9VMThread *monitorOwner = NULL;
	j9object_t monitorOwnerObject = NULL;
	IDATA exc = 0; /* exception index */

	Trc_JCL_threadmxbean_getThreadInfo_Entry(currentThread, targetThread);

	info->thread =
		vmfns->j9jni_createLocalRef((JNIEnv *)currentThread, (j9object_t)targetThread->threadObject);
	/* Set the native thread ID available through the thread library. */
	info->nativeTID = (jlong) omrthread_get_osId(targetThread->osThread);
	info->vmstate = getVMThreadObjectState(targetThread, &monitorObject, &monitorOwner, NULL);
	if (targetThread->threadObject) {
		info->jclThreadState = getJclThreadState(info->vmstate, J9VMJAVALANGTHREAD_STARTED(currentThread, targetThread->threadObject));
	} else {
		info->jclThreadState = getJclThreadState(info->vmstate, JNI_TRUE);
	}
	monitorOwnerObject = monitorOwner? (j9object_t)monitorOwner->threadObject : NULL;

	/* The monitorOwner thread could have exited before we read it.
	 * Force the thread to be RUNNABLE in this case.
	 */
	if ((J9VMTHREAD_STATE_BLOCKED == info->vmstate) && 
		(NULL != monitorOwner) && (NULL == monitorOwnerObject)) {
		info->vmstate = J9VMTHREAD_STATE_RUNNING;
		monitorObject = NULL;
	}
	info->blocker = vmfns->j9jni_createLocalRef((JNIEnv *)currentThread, monitorObject);
	info->blockerOwner = vmfns->j9jni_createLocalRef((JNIEnv *)currentThread, monitorOwnerObject);

	/* this may block on vm->managementDataLock */
	getContentionStats(currentThread, targetThread, info);

	exc = getStackFramePCs(currentThread, targetThread, info, maxStackDepth);
	if (exc > 0) {
		Trc_JCL_threadmxbean_getThreadInfo_Exit(currentThread, exc);
		return exc;
	}

	/* Allocate lockedMonitors array */
	info->lockedMonitors.arr_unsafe = NULL;
	info->lockedMonitors.arr_safe = NULL;
	info->lockedMonitors.len = 0;
	if (getLockedMonitors == JNI_TRUE) {
		exc = getMonitors(currentThread, targetThread, info, info->stack.len);
	}

	Trc_JCL_threadmxbean_getThreadInfo_Exit(currentThread, exc);
	return exc;
}

/**
 * Populate the contention monitoring statistics for a given VMThread.
 * @param[in] currentThread
 * @param[in] vmThread The thread to be examined.
 * @param[out] tinfo Where the statistics are to be stored. Must be non-NULL.
 * @return n/a
 */
static void
getContentionStats(J9VMThread *currentThread, J9VMThread *vmThread, ThreadInfo *tinfo)
{
	PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);
	J9JavaLangManagementData *mgmt = currentThread->javaVM->managementData;	

	tinfo->blockedCount = vmThread->mgmtBlockedCount;
	tinfo->waitedCount = vmThread->mgmtWaitedCount;

	/* NOTE: (Potential deadlock condition) Threads must not attempt to
	 * acquire VM access while holding managementDataLock
	 */
	omrthread_rwmutex_enter_read(mgmt->managementDataLock);

	/* ThreadContentionMonitoring is always supported */
	if (mgmt->threadContentionMonitoringFlag == JNI_TRUE) {
		U_64 timeNowNS = 0;

		if (vmThread->mgmtWaitedStart || vmThread->mgmtBlockedStart) {
			timeNowNS = (U_64)j9time_nano_time();
		}

		tinfo->blockedTime = vmThread->mgmtBlockedTimeTotal;
		if (vmThread->mgmtBlockedStart) {
			tinfo->blockedTime += checkedTimeInterval(timeNowNS, vmThread->mgmtBlockedTimeStart);
		}
		tinfo->blockedTime /= J9PORT_TIME_NS_PER_MS;

		tinfo->waitedTime = vmThread->mgmtWaitedTimeTotal;
		if (vmThread->mgmtWaitedStart) {
			tinfo->waitedTime += checkedTimeInterval(timeNowNS, vmThread->mgmtWaitedTimeStart);
		}
		tinfo->waitedTime /= J9PORT_TIME_NS_PER_MS;
	} else {
		tinfo->blockedTime = -1;
		tinfo->waitedTime = -1;
	}
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);
}

/**
 * Allocate and populate a cache for the PCs in the VMThread's stack trace.
 * @pre VM access.
 * @pre targetThread must be halted.
 * @param[in] currentThread
 * @param[in] targetThread The thread to be examined.
 * @param[out] tinfo Where stack trace should be stored. Must be non-NULL.
 * @param[in] maxStackDepth Guideline for limiting the number of frames walked.
 * @return error status
 * @retval 0 success
 * @retval >0 error, the index of a known exception
 */
static IDATA
getStackFramePCs(J9VMThread *currentThread, J9VMThread *targetThread, ThreadInfo *tinfo, jint maxStackDepth)
{
	IDATA exc = 0;
	UDATA rc;
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = vm->internalVMFunctions;
	J9StackWalkState walkState;

	/* walk stack and cache PCs */
	walkState.walkThread = targetThread;
	walkState.flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC
		| J9_STACKWALK_SKIP_INLINES 
		| J9_STACKWALK_INCLUDE_NATIVES 
		| J9_STACKWALK_VISIBLE_ONLY
		| J9_STACKWALK_COUNT_SPECIFIED;
		walkState.maxFrames = maxStackDepth;
	walkState.skipCount = 0;

	rc = vm->walkStackFrames(currentThread, &walkState);

	/* Check for stack walk failure */
	if (J9_STACKWALK_RC_NO_MEMORY == rc) {
		vmfns->freeStackWalkCaches(currentThread, &walkState);
		exc = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
		return exc;
	}
	
	/* copy cache */
	tinfo->stack.len = walkState.framesWalked;
	tinfo->stack.pcs = j9mem_allocate_memory(sizeof(UDATA) * tinfo->stack.len, J9MEM_CATEGORY_VM_JCL);
	if (tinfo->stack.pcs) {
		memcpy(tinfo->stack.pcs, walkState.cache, sizeof(UDATA) * tinfo->stack.len);
	}
	vmfns->freeStackWalkCaches(currentThread, &walkState);
	
	if (!tinfo->stack.pcs) {
		exc = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
	}
	return exc;
}

/**
 * Scan the heap and build locked synchronizer lists for all examined threads.
 * @pre exclusive VM access
 * @param[in] currentThread
 * @param[in] allinfo Threads being examined. Locked synchronizers are also stored into this array.
 * @param[in] allinfolen Length of allinfo[].
 * @return error status
 * @retval 0 success
 * @retval >0 error, the index of a known exception
 */
static UDATA
getSynchronizers(J9VMThread *currentThread, ThreadInfo *allinfo, UDATA allinfolen)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9MemoryManagerFunctions *mmfns = vm->memoryManagerFunctions;
	SynchronizerIterData data;
	UDATA exc = 0;
	jvmtiIterationControl rc;
	
	Trc_JCL_threadmxbean_getSynchronizers_Entry(currentThread, allinfo, allinfolen);

	data.allinfo = allinfo;
	data.allinfolen = allinfolen;

	/* ensure that all thread-local buffers are flushed */
	mmfns->j9gc_flush_nonAllocationCaches_for_walk(currentThread->javaVM);	
	rc = mmfns->j9mm_iterate_all_ownable_synchronizer_objects(currentThread, vm->portLibrary, 0, getSynchronizersHeapIterator, &data);
	if (rc == JVMTI_ITERATION_ABORT) {
		exc = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
	}
	
	Trc_JCL_threadmxbean_getSynchronizers_Exit(currentThread, exc);
	return exc;
}

/**
 * Heap object iterator for discovering locked synchronizers.
 * @pre exclusive VM access
 * @param[in] vm Thread
 * @param[in] objectDesc
 * @param[in] userData pointer to a SynchronizerIterData
 * @return iterator status
 * @retval JVMTI_ITERATION_CONTINUE success
 * @retval JVMTI_ITERATION_ABORT failure due to OutOfMemory
 */
static jvmtiIterationControl
getSynchronizersHeapIterator(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *objectDesc, void *userData)
{
	J9JavaVM *vm = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	j9object_t object = objectDesc->object;
	SynchronizerIterData *data = userData;
	J9Class *clazz;
	J9Class *aosClazz;
	SynchronizerInfo *sinfo;
	j9object_t owner;
	jvmtiIterationControl rc = JVMTI_ITERATION_CONTINUE;
	UDATA i;

	Assert_JCL_notNull(object);

	clazz = J9OBJECT_CLAZZ(vmThread, object);
	aosClazz = J9VMJAVAUTILCONCURRENTLOCKSABSTRACTOWNABLESYNCHRONIZER_OR_NULL(vm);

	/* If there no aos class in the system, this function should never be called */
	Assert_JCL_true(NULL != aosClazz);

	/* OwnableSynchronizer collection should only contain instances of java.util.concurrent.locks.AbstractOwnableSynchronizer */
	Assert_JCL_true(instanceOfOrCheckCast(clazz, aosClazz));
	
	owner = J9VMJAVAUTILCONCURRENTLOCKSABSTRACTOWNABLESYNCHRONIZER_EXCLUSIVEOWNERTHREAD(vmThread, object);
	if (owner) {
		/**
		 * @todo It would be nice to find a better way to get the ThreadInfo
		 * for the owner thread object.
		 */
		for (i = 0; i < data->allinfolen; ++i) {
			if ((data->allinfo[i].thread != NULL) && (J9OBJECT_FROM_JOBJECT(data->allinfo[i].thread) == owner)) {
				sinfo = j9mem_allocate_memory(sizeof(SynchronizerInfo), J9MEM_CATEGORY_VM_JCL);
				if (sinfo) {
					sinfo->obj.unsafe = object;
					sinfo->next = data->allinfo[i].lockedSynchronizers.list;
					data->allinfo[i].lockedSynchronizers.list = sinfo;
					data->allinfo[i].lockedSynchronizers.len++;
				} else {
					rc = JVMTI_ITERATION_ABORT;
				}
				break;
			}
		}
	}
	return rc;
}

/**
 * Allocates and populates owned monitor array.
 * @param[in] currentThread
 * @param[in] targetThread
 * @param[in] tinfo
 * @param[in] stackLen
 * @return error status
 * @retval 0 success
 * @retval >0 error, the index of a known exception
 */
static IDATA
getMonitors(J9VMThread *currentThread, J9VMThread *targetThread, ThreadInfo *tinfo, UDATA stackLen)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JavaVM *vm = currentThread->javaVM;
	IDATA rc = 0;
	IDATA exc = 0;
	IDATA infoLen = 0;
	J9ObjectMonitorInfo *info;

	Trc_JCL_threadmxbean_getMonitors_Entry(currentThread, targetThread, tinfo, stackLen);

	infoLen = vm->internalVMFunctions->getOwnedObjectMonitors(currentThread,
			targetThread, NULL, 0);
	if (infoLen > 0) {
		info = j9mem_allocate_memory(infoLen * sizeof(J9ObjectMonitorInfo), J9MEM_CATEGORY_VM_JCL);
		if (info) {
			rc = vm->internalVMFunctions->getOwnedObjectMonitors(currentThread,
					targetThread, info, infoLen);
			if (rc >= 0) {
				tinfo->lockedMonitors.arr_unsafe = info;
				tinfo->lockedMonitors.len = rc;
			} else {
				j9mem_free_memory(info);
				exc = J9VMCONSTANTPOOL_JAVALANGINTERNALERROR;
			}
		} else {
			exc = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
		}
	}
	
	Trc_JCL_threadmxbean_getMonitors_Exit(currentThread, exc);
	return exc;
}

/**
 * Allocate and populate an array of java/lang/management/ThreadInfo.
 * @pre must not have VM access
 * @post temp memory in allinfo[] is freed
 * @param[in] env
 * @param[in] allinfo
 * @param[in] allinfolen
 * @return java/lang/management/ThreadInfo[]
 * @retval non-null success
 * @retval null error, an exception is set
 */
static jobjectArray
createThreadInfoArray(JNIEnv *env, ThreadInfo *allinfo, UDATA allinfolen, jsize maxStackDepth)
{
	jclass cls;
	jobjectArray result;
	UDATA i;

	cls = JCL_CACHE_GET(env, CLS_java_lang_management_ThreadInfo);
	Assert_JCL_notNull(cls);

	result = (*env)->NewObjectArray(env, (jsize)allinfolen, cls, NULL);
	if (!result) {
		return NULL;
	}

	for (i = 0; i < allinfolen; ++i) {
		jobject tinfo;

		if (allinfo[i].thread) {
			tinfo = createThreadInfo(env, &allinfo[i], maxStackDepth);
			if (!tinfo) {
				return NULL;
			}
			(*env)->SetObjectArrayElement(env, result, (jsize)i, tinfo);
			(*env)->DeleteLocalRef(env, tinfo);
		} else {
			(*env)->SetObjectArrayElement(env, result, (jsize)i, NULL);
		}
	}

	return result;
}

/**
 * Allocate and populate a single java/lang/management/ThreadInfo object.
 * @pre must not have VM access
 * @post temp memory in tinfo is freed
 * @param[in] env
 * @param[in] tinfo
 * @param[in] maxStackDepth The stack trace is pruned to this depth.
 * @return java/lang/management/ThreadInfo object
 * @retval non-null success
 * @retval null error, an exception is set
 */
static jobject
createThreadInfo(JNIEnv *env, ThreadInfo *tinfo, jsize maxStackDepth)
{
	jclass cls = NULL;
	jmethodID ctor = NULL;
	jobject result = NULL;
	jint state;
	jobjectArray lockedMonitors = NULL;
	jobjectArray lockedSynchronizers = NULL;
	jobjectArray prunedTrace = NULL;
	jboolean isSuspended;
	jboolean isNative;
	jboolean getOwnedLocks = JNI_TRUE;

	cls = JCL_CACHE_GET(env, CLS_java_lang_management_ThreadInfo);
	Assert_JCL_notNull(cls);

	/* look for Java 6 version of the constructor */
	ctor = JCL_CACHE_GET(env, MID_java_lang_management_ThreadInfo_init);
	if (!ctor) {
		ctor = JCL_CACHE_GET(env, MID_java_lang_management_ThreadInfo_init_nolocks);
		getOwnedLocks = JNI_FALSE;
	}
	Assert_JCL_notNull(ctor);
	
	if (tinfo->vmstate == J9VMTHREAD_STATE_SUSPENDED) {
		isSuspended = JNI_TRUE;
	} else {
		isSuspended = JNI_FALSE;
	}
	state = tinfo->jclThreadState;

	isNative = isInNative(env, tinfo->stackTrace);
	if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
		return NULL;
	}

	if (JNI_TRUE == getOwnedLocks) {
		lockedMonitors = createLockedMonitors(env, tinfo);
		if (lockedMonitors == NULL) {
			return NULL;
		}
	
		lockedSynchronizers = createLockedSynchronizers(env, tinfo);
		if (!lockedSynchronizers) {
			return NULL;
		}
	}

	prunedTrace = pruneStackTrace(env, tinfo->stackTrace, maxStackDepth);
	if (!prunedTrace) {
		return NULL;
	}

	if (JNI_TRUE == getOwnedLocks) {
		result = (*env)->NewObject(env, cls, ctor, tinfo->thread, tinfo->nativeTID, state, isSuspended, isNative, 
			tinfo->blockedCount, tinfo->blockedTime, tinfo->waitedCount, tinfo->waitedTime,
			prunedTrace, tinfo->blocker, tinfo->blockerOwner, lockedMonitors, lockedSynchronizers);
	} else {
		result = (*env)->NewObject(env, cls, ctor, tinfo->thread, tinfo->nativeTID, state, isSuspended, isNative, 
			tinfo->blockedCount, tinfo->blockedTime, tinfo->waitedCount, tinfo->waitedTime,
			prunedTrace, tinfo->blocker, tinfo->blockerOwner);
	}

	(*env)->DeleteLocalRef(env, tinfo->thread);
	if (tinfo->blocker) {
		(*env)->DeleteLocalRef(env, tinfo->blocker);
	}
	if (tinfo->blockerOwner) {
		(*env)->DeleteLocalRef(env, tinfo->blockerOwner);
	}
	(*env)->DeleteLocalRef(env, tinfo->stackTrace);

	if (prunedTrace != tinfo->stackTrace) {
		(*env)->DeleteLocalRef(env, prunedTrace);
	}

	(*env)->DeleteLocalRef(env, lockedMonitors);
	(*env)->DeleteLocalRef(env, lockedSynchronizers);

	/* clear temps */
	memset(tinfo, 0, sizeof(ThreadInfo));
	return result;
}


/**
 * Allocate and populate the MonitorInfo array to be returned by
 * ThreadInfo.getLockedMonitors().
 * 
 * According to spec, lockedMonitors should be a 0-sized array
 * if there are no locked monitors.
 * 
 * @pre must not have VM access
 * @post frees tinfo->lockedMonitors.arr_safe.
 * @param[in] env
 * @param[in] tinfo
 * @return java/lang/management/MonitorInfo[]
 * @retval non-null success
 * @retval null error, an exception is set
 */ 
static jobjectArray
createLockedMonitors(JNIEnv *env, ThreadInfo *tinfo)
{
	PORT_ACCESS_FROM_ENV(env);
	jobjectArray lockedMonitors = NULL;
	jclass cls;
	jmethodID ctor;
	jmethodID getName;
	UDATA count;
	UDATA i, j;

	cls = JCL_CACHE_GET(env, CLS_java_lang_management_MonitorInfo);
	Assert_JCL_notNull(cls);

	ctor = JCL_CACHE_GET(env, MID_java_lang_management_MonitorInfo_init);
	Assert_JCL_notNull(ctor);

	getName = JCL_CACHE_GET(env, MID_java_lang_Class_getName);
	Assert_JCL_notNull(getName);

	/* count recursive enters */
	count = 0;
	for (i = 0; i < tinfo->lockedMonitors.len; ++i) {
		count += tinfo->lockedMonitors.arr_safe[i].count;
	}

	/* allocate array object */
	lockedMonitors = (*env)->NewObjectArray(env, (jsize)count, cls, NULL);
	if (!lockedMonitors) {
		return NULL;
	}

	/* copy monitors from temp array to lockedMonitors object */
	count = 0;
	for (i = 0; i < tinfo->lockedMonitors.len; ++i) {
		jobject minfo = NULL;
		jobject clazz = NULL;
		jobject frame = NULL;
		jobject name = NULL;
		jint depth = 0;
		jint hash = 0;

		depth = (jint)(tinfo->lockedMonitors.arr_safe[i].depth - 1);
		clazz = tinfo->lockedMonitors.arr_safe[i].clazz;
		hash = (jint)(tinfo->lockedMonitors.arr_safe[i].identityHashCode);

		if (depth >= 0) {
			frame = (*env)->GetObjectArrayElement(env, tinfo->stackTrace, depth);
			if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
				return NULL;
			}
		}

		name = (*env)->CallObjectMethod(env, clazz, getName);
		if (name == NULL) {
			return NULL;
		}

		minfo = (*env)->NewObject(env, cls, ctor, name, hash, depth, frame);
		if (!minfo) {
			return NULL;
		}

		for (j = 0; j < tinfo->lockedMonitors.arr_safe[i].count; ++j) {
			(*env)->SetObjectArrayElement(env, lockedMonitors, (jsize)count, minfo);
			if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
				return NULL;
			}
			++count;
		}

		(*env)->DeleteLocalRef(env, minfo);
		(*env)->DeleteLocalRef(env, frame);
		(*env)->DeleteLocalRef(env, name);
		(*env)->DeleteLocalRef(env, clazz);
	}

	/* free temp array */
	j9mem_free_memory(tinfo->lockedMonitors.arr_safe);
	tinfo->lockedMonitors.arr_safe = NULL;

	return lockedMonitors;
}

/**
 * Allocate and populate the LockInfo array to be returned by 
 * ThreadInfo.getLockedSynchronizers().
 *
 * According to spec, lockedSynchronizers should be a 0-sized array
 * if there are no locked monitors.
 * 
 * @pre must not have VM access
 * @post frees tinfo->lockedSynchronizers.list
 * 
 * @param[in] env
 * @param[in] tinfo
 * @return java/lang/management/LockInfo[]
 * @retval non-null success
 * @retval null error, an exception is set
 */ 
static jobjectArray
createLockedSynchronizers(JNIEnv *env, ThreadInfo *tinfo)
{
	PORT_ACCESS_FROM_ENV(env);
	SynchronizerInfo *sinfo;
	jobjectArray lockedSyncs = NULL;
	jclass cls;
	jmethodID ctor;
	UDATA i;

	cls = JCL_CACHE_GET(env, CLS_java_lang_management_LockInfo);
	Assert_JCL_notNull(cls);

	ctor = JCL_CACHE_GET(env, MID_java_lang_management_LockInfo_init);
	Assert_JCL_notNull(ctor);

	lockedSyncs = (*env)->NewObjectArray(env, (jsize)tinfo->lockedSynchronizers.len, cls, NULL);
	if (!lockedSyncs) {
		return NULL;
	}

	sinfo = tinfo->lockedSynchronizers.list;
	i = 0;
	while (sinfo) {
		SynchronizerInfo *next;
		jobject linfo;
		jobject lock;

		lock = sinfo->obj.safe;
		linfo = (*env)->NewObject(env, cls, ctor, lock);
		if (!linfo) {
			return NULL;
		}

		(*env)->SetObjectArrayElement(env, lockedSyncs, (jsize)i, linfo);
		if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
			return NULL;
		}

		(*env)->DeleteLocalRef(env, linfo);
		(*env)->DeleteLocalRef(env, lock);

		next = sinfo->next;
		j9mem_free_memory(sinfo);
		sinfo = next;
		++i;
	}
	tinfo->lockedSynchronizers.list = NULL;
	return lockedSyncs;
}

/**
 * Allocate and populate a stack trace object.
 * 
 * @pre VM access. 
 * @post frees tinfo->stack.pcs.
 * @note May release VM access.
 * @param[in] currentThread
 * @param[in] tinfo
 * @return java/lang/StackTraceElement[]
 * @retval non-null success
 * @retval null error, exception is set
 */
static jobjectArray
createStackTrace(J9VMThread *currentThread, ThreadInfo *tinfo)
{
	J9InternalVMFunctions *vmfns = currentThread->javaVM->internalVMFunctions;
	PORT_ACCESS_FROM_VMC(currentThread);
	j9object_t throwable = NULL;
	j9object_t stackTrace = NULL;
	jobject throwableRef = NULL;
	jobjectArray stackTraceRef = NULL;

	Assert_JCL_mustHaveVMAccess(currentThread);

	throwable = (j9object_t)createStackTraceThrowable(currentThread,
			tinfo->stack.pcs, tinfo->stack.len);
	/* on failure, an exception will be set */

	j9mem_free_memory(tinfo->stack.pcs);
	tinfo->stack.pcs = NULL;
	
	if (throwable) {
		throwableRef = vmfns->j9jni_createLocalRef((JNIEnv *)currentThread, throwable);
		stackTrace = (j9object_t)getStackTrace(currentThread, (j9object_t*)throwableRef, FALSE);
		if (stackTrace && (!currentThread->currentException)) {
			stackTraceRef = vmfns->j9jni_createLocalRef((JNIEnv *)currentThread, stackTrace);
			if (!stackTraceRef) {
				vmfns->setNativeOutOfMemoryError(currentThread, 0, 0);
			}
		}
		vmfns->j9jni_deleteLocalRef((JNIEnv *)currentThread, throwableRef);
	}
	return stackTraceRef;
}

/**
 * Prune a stack trace object to a given length.
 * The original object might be returned if no pruning is required.
 * 
 * @pre most not have VM access. 
 * @param[in] env
 * @param[in] stackTrace
 * @param[in] maxStackDepth The maximum stack trace length desired.
 * @return java/lang/StackTraceElement[]
 * @retval non-null success
 * @retval null error, exception is set
 */
static jobjectArray
pruneStackTrace(JNIEnv *env, jobjectArray stackTrace, jsize maxStackDepth)
{
	jsize arrayLength;
	jclass elementClass;
	jobjectArray prunedTrace;
	jclass systemClass;
	jmethodID mid;

	if (!stackTrace) {
		return NULL;
	}

	if (maxStackDepth < 0) {
		return stackTrace;
	}
	
	arrayLength = (*env)->GetArrayLength(env, stackTrace);
	if ((arrayLength <= 0) || (arrayLength <= maxStackDepth)) {
		return stackTrace;
	}
	elementClass = JCL_CACHE_GET(env, CLS_java_lang_StackTraceElement);
	Assert_JCL_notNull(elementClass);

	prunedTrace = (*env)->NewObjectArray(env, maxStackDepth, elementClass, NULL);
	if (!prunedTrace) {
		return NULL;
	}
	systemClass = (*env)->FindClass(env, "java/lang/System");
	if (!systemClass) {
		return NULL;
	}
	mid = (*env)->GetStaticMethodID(env, systemClass, "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V");
	if (!mid) {
		return NULL;
	}
	(*env)->CallStaticVoidMethod(env, systemClass, mid, stackTrace, 0, prunedTrace, 0, maxStackDepth);
	if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
		return NULL;
	}
	return prunedTrace;
}

/** 
 * Convert unsafe object references into local refs before releasing VM access
 * @pre VM access
 * @param[in] env
 * @param[in] info
 * @return error status
 * @retval 0 success
 * @retval >0 error, the index of a known exception
 */
static IDATA
saveObjectRefs(JNIEnv *env, ThreadInfo *info)
{
	J9JavaVM * vm = ((J9VMThread *)env)->javaVM;
	J9InternalVMFunctions *vmfns = vm->internalVMFunctions;
	SynchronizerInfo *sinfo;
	UDATA i;
	IDATA exc = 0;

	if (info->lockedMonitors.len > 0) {
		PORT_ACCESS_FROM_ENV(env);
		info->lockedMonitors.arr_safe = j9mem_allocate_memory(sizeof(MonitorInfo) * info->lockedMonitors.len, J9MEM_CATEGORY_VM_JCL);
		if (info->lockedMonitors.arr_safe) {
			for (i = 0; i < info->lockedMonitors.len; ++i) {
				j9object_t object = info->lockedMonitors.arr_unsafe[i].object;

				info->lockedMonitors.arr_safe[i].clazz =
					vmfns->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS(J9OBJECT_CLAZZ((J9VMThread *)env, object)));
				info->lockedMonitors.arr_safe[i].identityHashCode = objectHashCode(vm, object);
				info->lockedMonitors.arr_safe[i].count = info->lockedMonitors.arr_unsafe[i].count;
				info->lockedMonitors.arr_safe[i].depth = info->lockedMonitors.arr_unsafe[i].depth;
			}
		} else {
			exc = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
		}
		j9mem_free_memory(info->lockedMonitors.arr_unsafe);
		info->lockedMonitors.arr_unsafe = NULL;
	}

	if (0 == exc) {
		if (info->lockedSynchronizers.len > 0) {
			sinfo = info->lockedSynchronizers.list;
			while (sinfo) {
				sinfo->obj.safe = vmfns->j9jni_createLocalRef(env, sinfo->obj.unsafe);
				sinfo = sinfo->next;
			}
		}
	}
	
	return exc;
}

/**
 * Returns stackTrace[0].isNativeMethod()
 * @pre must not have VM access
 * @param[in] env
 * @param[in] stackTrace
 * @returns Whether the top frame of the stack trace is a native method.
 * On error, this function may set an exception. The caller must check for pending exceptions. 
 */
static jboolean
isInNative(JNIEnv *env, jobjectArray stackTrace)
{
	jobject element;
	jmethodID mid;
	jboolean result = JNI_FALSE;

	/* call stackTrace[0].isNativeMethod() */

	element = (*env)->GetObjectArrayElement(env, stackTrace, 0);
	if (element) {
		mid = JCL_CACHE_GET(env, MID_java_lang_StackTraceElement_isNativeMethod);
		Assert_JCL_notNull(mid);
		result = (*env)->CallBooleanMethod(env, element, mid);
	} else {
		/* stack trace must be empty. OK to continue. */
		if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
			(*env)->ExceptionClear(env);
		}
	}
	
	return result;
}

/**
 * Discover deadlocked threads. In case of multitenancy, only the deadlocked threads that belong to
 * the calling tenant are returned.
 * @param[in] env
 * @param[in] findFlags J9VMTHREAD_FINDDEADLOCKFLAG_xxx. Flags passed to the VM helper 
 * that determine whether deadlocks among synchronizers or waiting threads should be 
 * considered.
 * @return array of thread IDs for deadlocked threads.
 * null array is returned if there are no deadlocked threads.
 * On error, this function may set an exception.
 */
static jlongArray
findDeadlockedThreads(JNIEnv *env, UDATA findFlags)
{
	PORT_ACCESS_FROM_ENV(env);
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *javaVM = currentThread->javaVM;
	j9object_t *threads = NULL;
	IDATA deadCount = 0;
	jlong *deadIDs = NULL;
	jlongArray resultArray = NULL;
	UDATA i;

	javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	
	deadCount = javaVM->internalVMFunctions->findObjectDeadlockedThreads(currentThread, &threads, NULL, findFlags);

	/* spec says to return NULL array for no threads */	
	if (deadCount <= 0) {
		j9mem_free_memory(threads);
		if (deadCount < 0) {
			javaVM->internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);
		}
		javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
		return NULL;
	}
		
	/* Finally, ready to output the list of deadlocked threads */
	deadIDs = (jlong *)j9mem_allocate_memory(deadCount * sizeof(jlong), J9MEM_CATEGORY_VM_JCL);
	if (!deadIDs) {
		j9mem_free_memory(threads);
		javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
		return NULL;
	}
	{
		for (i = 0; (IDATA)i < deadCount; i++) {
			deadIDs[i] = getThreadID(currentThread, threads[i]);
		}
	}


	j9mem_free_memory(threads);
	javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);

	resultArray = (*env)->NewLongArray(env, (jsize)deadCount);
	if (!resultArray) {
		j9mem_free_memory(deadIDs);
		return NULL;
	}
	
	(*env)->SetLongArrayRegion(env, resultArray, 0, (jsize)deadCount, deadIDs);
	j9mem_free_memory(deadIDs);
	if ((*env)->ExceptionCheck(env)) {
		return NULL;
	}

	return resultArray;
}

/**
 * @internal	Throw appropriate exception based on the error code passed.
 *
 * @param error	Error code from thread library
 */
static void
handle_error(JNIEnv *env, IDATA error)
{
	PORT_ACCESS_FROM_ENV(env);
	jclass exceptionClass = NULL;
	const char *msgString = NULL;

	if (-J9THREAD_ERR_USAGE_RETRIEVAL_UNSUPPORTED == error) {
		exceptionClass = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
		msgString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
										 J9NLS_JCL_JVM_CPU_USAGE_RETRIEVAL_UNSUPPORTED_MSG,
										 NULL);
	} else {
		exceptionClass = (*env)->FindClass(env, "java/lang/InternalError");
		if (-J9THREAD_ERR_USAGE_RETRIEVAL_ERROR == error) {
			msgString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
											 J9NLS_JCL_JVM_CPU_USAGE_RETRIEVAL_ERROR,
											 NULL);
		} else if (-J9THREAD_ERR_INVALID_TIMESTAMP == error) {
			msgString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
											 J9NLS_JCL_JVM_CPU_USAGE_INVALID_TIMESTAMP,
											 NULL);
		}
	}
	if (NULL != exceptionClass) {
		(*env)->ThrowNew(env, exceptionClass, msgString);
	}
}

/**
 * Returns the CPU usage of all attached jvm threads split into different categories.
 *
 * @param env						The JNI env.
 * @param beanInstance				beanInstance.
 * @param jvmCpuMonitorInfoObject 	The jvmCpuMonitorInfo object that needs to be filled in.
 * @return 							The jvmCpuMonitorInfo object that is filled in on success or NULL on failure.
 */
jobject JNICALL
Java_com_ibm_lang_management_internal_JvmCpuMonitor_getThreadsCpuUsageImpl(JNIEnv *env, jobject beanInstance, jobject jvmCpuMonitorInfoObject)
{
	IDATA rc = 0;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9ThreadsCpuUsage cpuUsage;
	jmethodID MID_updateValues = NULL;
	jclass CLS_JvmCpuMonitorInfo = NULL;
	jlongArray appUserArray = NULL;

	/* Check if the JvmCpuMonitorInfo class has been cached */
	CLS_JvmCpuMonitorInfo = JCL_CACHE_GET(env, CLS_java_com_ibm_lang_management_JvmCpuMonitorInfo);
	if (NULL == CLS_JvmCpuMonitorInfo) {
		jclass CLS_JvmCpuMonitorInfoLocal = NULL;

		/* Get the class */
		CLS_JvmCpuMonitorInfoLocal = (*env)->GetObjectClass(env, jvmCpuMonitorInfoObject);
		if (NULL == CLS_JvmCpuMonitorInfoLocal) {
			return NULL;
		}

		/* Convert to a global reference and delete the local one */
		CLS_JvmCpuMonitorInfo = (*env)->NewGlobalRef(env, CLS_JvmCpuMonitorInfoLocal);
		(*env)->DeleteLocalRef(env, CLS_JvmCpuMonitorInfoLocal);
		if (NULL == CLS_JvmCpuMonitorInfo) {
			return NULL;
		}
		JCL_CACHE_SET(env, CLS_java_com_ibm_lang_management_JvmCpuMonitorInfo, CLS_JvmCpuMonitorInfo);
	}
	MID_updateValues = JCL_CACHE_GET(env, MID_java_com_ibm_lang_management_JvmCpuMonitorInfo_updateValues);	
	if (NULL == MID_updateValues) {
		/* Get the method ID for updateValues() method */
		MID_updateValues = (*env)->GetMethodID(env, CLS_JvmCpuMonitorInfo, "updateValues", "(JJJJJJ[J)V");
		if (NULL == MID_updateValues) {
			return NULL;
		}
		JCL_CACHE_SET(env, MID_java_com_ibm_lang_management_JvmCpuMonitorInfo_updateValues, MID_updateValues);
	}

	memset(&cpuUsage, 0, sizeof(cpuUsage));

	/* Get the cpu usage for all threads while holding the vmThreadListMutex.
	 * This ensures that a thread doesn't die on us while walking the threads in the thread library
	 */
	omrthread_monitor_enter(javaVM->vmThreadListMutex);
	rc = omrthread_get_jvm_cpu_usage_info(&cpuUsage);
	omrthread_monitor_exit(javaVM->vmThreadListMutex);

	if (rc < 0) {
		handle_error(env, rc);
		return NULL;
	}

	/* Create a jlongArray of user defined application category types */
	appUserArray = (*env)->NewLongArray(env, (jsize) J9THREAD_MAX_USER_DEFINED_THREAD_CATEGORIES);
	if (NULL == appUserArray) {
		return NULL;
	}
	/* Populate the array with the values that we got from the thread library */
	(*env)->SetLongArrayRegion(env, appUserArray, 0, (jsize) J9THREAD_MAX_USER_DEFINED_THREAD_CATEGORIES, (jlong *)cpuUsage.applicationUserCpuTime);
	if ((*env)->ExceptionCheck(env)) {
		return NULL;
	}

	/* Call the update values method to update the object with the values obtained */
	(*env)->CallVoidMethod(env,
				jvmCpuMonitorInfoObject,
				MID_updateValues,
				(jlong)cpuUsage.timestamp,
				(jlong)cpuUsage.applicationCpuTime,
				(jlong)cpuUsage.resourceMonitorCpuTime,
				(jlong)cpuUsage.systemJvmCpuTime,
				(jlong)cpuUsage.gcCpuTime,
				(jlong)cpuUsage.jitCpuTime,
				(jlongArray)appUserArray);

	return jvmCpuMonitorInfoObject;
}

/**
 * Return the internal omrthread structure for a given threadID.
 * 
 * @param currentThread
 * @param threadID
 * @return the omrthread_t on success or NULL om failure.
 */
static omrthread_t
get_thread_from_id(J9VMThread *currentThread, jlong threadID)
{
	J9VMThread *targetThread = NULL;
	omrthread_t target = NULL;

	/* walk the vmthreads until we find the matching one */
	for (targetThread = currentThread->linkNext;
		targetThread != currentThread;
		targetThread = targetThread->linkNext
	) {
		if (NULL != targetThread->threadObject) {
			/* check that thread's ID matches */
			if (getThreadID(currentThread, (j9object_t)targetThread->threadObject) == threadID) {
				/* check if the thread is alive */
				if (NULL != J9VMJAVALANGTHREAD_THREADREF(currentThread, targetThread->threadObject)) {
					target = targetThread->osThread;
				}
				break;
			}
		}
	}

	return target;
}

/* Java defines for thread categories */
#define THREAD_CATEGORY_INVALID				-1
#define THREAD_CATEGORY_UNKNOWN				0
#define THREAD_CATEGORY_SYSTEM_JVM			1
#define THREAD_CATEGORY_GC					2
#define THREAD_CATEGORY_JIT					3

#define THREAD_CATEGORY_RESOURCE_MONITOR	10

#define THREAD_CATEGORY_APPLICATION			100
#define THREAD_CATEGORY_APPLICATION_USER1	101
#define THREAD_CATEGORY_APPLICATION_USER2	102
#define THREAD_CATEGORY_APPLICATION_USER3	103
#define THREAD_CATEGORY_APPLICATION_USER4	104
#define THREAD_CATEGORY_APPLICATION_USER5	105

#if defined(WIN32) || defined(WIN64)
#define STRNCASECMP	_strnicmp
#else
#define STRNCASECMP	strncasecmp
#endif /* WIN32 || WIN64 */

/**
 * Sets the thread category of the given threadID to the one that is passed with the following caveats
 * 1. System thread categories cannot be changed.
 * 2. Application and monitor threads cannot be set as System, GC or JIT threads.
 * 3. Application thread category can be changed to Monitor but not back to Application again.
 * 4. Application thread category can be changed to Application-UserX, X=1-5 and back to Application again.
 *
 * @param env			The JNI env.
 * @param beanInstance	beanInstance.
 * @param threadID		The thread ID of the thread whose category needs to be set.
 * @param category		The category to be set to.
 * @return				0 on success -1 on failure.
 */
jint JNICALL
Java_com_ibm_lang_management_internal_JvmCpuMonitor_setThreadCategoryImpl(JNIEnv *env, jobject beanInstance, jlong threadID, jint category)
{
	jint rc = -1;
	UDATA newCat = 0;
	UDATA origCat = 0;
	const char *err_msg = NULL;
	omrthread_t t_osThread = NULL;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = currentThread->javaVM->internalVMFunctions;

	/* "RESOURCE-MONITOR", "APPLICATION" and "APPLICATION-USERX" are the only valid thread categories */
	switch (category) {
	case THREAD_CATEGORY_RESOURCE_MONITOR:
		newCat = J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD;
		break;
	case THREAD_CATEGORY_APPLICATION:
		newCat = J9THREAD_CATEGORY_APPLICATION_THREAD;
		break;
	case THREAD_CATEGORY_APPLICATION_USER1:
		newCat = J9THREAD_USER_DEFINED_THREAD_CATEGORY_1;
		break;
	case THREAD_CATEGORY_APPLICATION_USER2:
		newCat = J9THREAD_USER_DEFINED_THREAD_CATEGORY_2;
		break;
	case THREAD_CATEGORY_APPLICATION_USER3:
		newCat = J9THREAD_USER_DEFINED_THREAD_CATEGORY_3;
		break;
	case THREAD_CATEGORY_APPLICATION_USER4:
		newCat = J9THREAD_USER_DEFINED_THREAD_CATEGORY_4;
		break;
	case THREAD_CATEGORY_APPLICATION_USER5:
		newCat = J9THREAD_USER_DEFINED_THREAD_CATEGORY_5;
		break;
	default:
		return rc;
	}

	vmfns->internalEnterVMFromJNI(currentThread);
	/* shortcut for the current thread */
	if (getThreadID(currentThread, (j9object_t)currentThread->threadObject) == threadID) {
		t_osThread = currentThread->osThread;
	} else {
		omrthread_monitor_enter(javaVM->vmThreadListMutex);
		t_osThread = get_thread_from_id(currentThread, threadID);
		if (NULL == t_osThread) {
			goto err_exit;
		}
	}
	
	/* Get the current thread category */
	origCat = omrthread_get_category(t_osThread);

	/* Is it a System-JVM thread or a Resource-Monitor Thread? If so the application cannot change it! */
	if (  (J9THREAD_CATEGORY_SYSTEM_THREAD == origCat)
	   || (J9THREAD_CATEGORY_SYSTEM_GC_THREAD == origCat)
	   || (J9THREAD_CATEGORY_SYSTEM_JIT_THREAD == origCat)
	) {
		err_msg = "Cannot modify System-JVM thread category";
		goto err_exit;
	}

	if (J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD == origCat) {
		err_msg = "Cannot modify Resource-Monitor thread category";
		goto err_exit;
	}
	/* Set it to the new category */
	rc = (jint) omrthread_set_category(t_osThread, newCat, J9THREAD_TYPE_SET_MODIFY);

err_exit:
	if (t_osThread != currentThread->osThread) {
		omrthread_monitor_exit(javaVM->vmThreadListMutex);
	}
	vmfns->internalExitVMToJNI(currentThread);
	if (NULL != err_msg) {
		throwNewIllegalArgumentException(env, (char *) err_msg);
	}

	return rc;
}

/**
 * Returns the category of the thread whose threadID is passed.
 *
 * @param env			The JNI env.
 * @param beanInstance	beanInstance.
 * @param threadID 		The thread ID of the thread whose category needs to be returned.
 * @return 				The category on success or -1 on failure.
 */
jint JNICALL
Java_com_ibm_lang_management_internal_JvmCpuMonitor_getThreadCategoryImpl(JNIEnv *env, jobject beanInstance, jlong threadID)
{
	UDATA category = 0;
	omrthread_t t_osThread = NULL;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = currentThread->javaVM->internalVMFunctions;

	vmfns->internalEnterVMFromJNI(currentThread);
	if (getThreadID(currentThread, (j9object_t)currentThread->threadObject) == threadID) {
		t_osThread = currentThread->osThread;
	} else {
		omrthread_monitor_enter(javaVM->vmThreadListMutex);
		t_osThread = get_thread_from_id(currentThread, threadID);
		if (NULL == t_osThread) {
			omrthread_monitor_exit(javaVM->vmThreadListMutex);
			vmfns->internalExitVMToJNI(currentThread);
			return THREAD_CATEGORY_INVALID;
		}
	}
	
	category = omrthread_get_category(t_osThread);

	if (t_osThread != currentThread->osThread) {
		omrthread_monitor_exit(javaVM->vmThreadListMutex);
	}
	vmfns->internalExitVMToJNI(currentThread);

	switch (category) {
	case J9THREAD_CATEGORY_SYSTEM_THREAD:
		return THREAD_CATEGORY_SYSTEM_JVM;
		break;
	case J9THREAD_CATEGORY_SYSTEM_GC_THREAD:
		return THREAD_CATEGORY_GC;
		break;
	case J9THREAD_CATEGORY_SYSTEM_JIT_THREAD:
		return THREAD_CATEGORY_JIT;
		break;
	case J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD:
		return THREAD_CATEGORY_RESOURCE_MONITOR;
		break;
	case J9THREAD_CATEGORY_APPLICATION_THREAD:
		return THREAD_CATEGORY_APPLICATION;
		break;
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_1:
		return THREAD_CATEGORY_APPLICATION_USER1;
		break;
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_2:
		return THREAD_CATEGORY_APPLICATION_USER2;
		break;
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_3:
		return THREAD_CATEGORY_APPLICATION_USER3;
		break;
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_4:
		return THREAD_CATEGORY_APPLICATION_USER4;
		break;
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_5:
		return THREAD_CATEGORY_APPLICATION_USER5;
		break;
	default:
		return THREAD_CATEGORY_INVALID;
	}

	return THREAD_CATEGORY_INVALID;
}

/**
 * @brief Helper function that finds a native thread identifier corresponding to
 * a unique thread identifier specified to it.
 * @param[in] currentThread
 * @param[in] threadID  The unique thread identifier
 * @return  Native thread identifier.  If no such thread is found, returns -1.
 * @pre  Caller must acquire the vmThreadListMutex when invoking findNativeThreadId 
 * to avoid thread list from being altered as it iterates through it.
 */
static jlong
findNativeThreadId(J9VMThread *currentThread, jlong threadID)
{
	J9JavaVM *vm = NULL;
	J9VMThread *threadIter = NULL;
	jlong nativeTId = -1;

	Trc_JCL_threadmxbean_findNativeThreadId_Entry(currentThread, threadID);
	Assert_JCL_notNull(currentThread);

	/* Walk the vmthreads until we find the matching one */
	vm = currentThread->javaVM;
	threadIter = vm->mainThread;
	do {
		if ((NULL != threadIter->threadObject)
			&& (J9VMJAVALANGTHREAD_THREADREF(currentThread, threadIter->threadObject) == threadIter)
			) {
			if (getThreadID(currentThread, (j9object_t)threadIter->threadObject) == threadID) {
				/* Found a thread with matching ID; check if the thread is alive. */
				omrthread_t osThread = threadIter->osThread;
				nativeTId = omrthread_get_osId(osThread);
				break;
			}
		}
		threadIter = threadIter->linkNext;
	} while ((NULL != threadIter) && (threadIter != vm->mainThread));

	Trc_JCL_threadmxbean_findNativeThreadId_Exit(currentThread, nativeTId);
	return nativeTId;
}

/**
 * Returns the native (operating system assigned) thread ID for a thread whose unique (java/lang/Thread.getId()) 
 * ID is passed.  If the thread ID passed cannot be found (thread no longer exists in the VM), return a -1 to
 * indicate this.
 *
 * @param[in] env  The JNI env.
 * @param[in] bean  The Class (static method).
 * @param[in] threadID   ID of the thread whose native ID needs to be returned.
 * @return  The native ID on success or -1 on failure.
 */
jlong JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_findNativeThreadIDImpl(JNIEnv *env, jobject bean, jlong threadID)
{
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = javaVM->internalVMFunctions;
	jlong nativeTID = -1;

	Trc_JCL_threadmxbean_findNativeThreadIDImpl_Entry(env, threadID);
	/* Ensure we search the thread ID while holding locks to avoid thread addition/removal. */
	vmfns->internalEnterVMFromJNI(currentThread);
	omrthread_monitor_enter(javaVM->vmThreadListMutex);
	nativeTID = findNativeThreadId(currentThread, threadID);
	omrthread_monitor_exit(javaVM->vmThreadListMutex);
	vmfns->internalExitVMToJNI(currentThread);

	Trc_JCL_threadmxbean_findNativeThreadIDImpl_Exit(env, nativeTID);
	return nativeTID;
}

/**
 * Returns an array of native (operating system assigned) thread IDs for a set of unique thread IDs
 * (java/lang/Thread.getId()) passed to it.  If a particular thread ID corresponds to a thread that
 * is no longer alive in the virtual machine, it simply sets the slot to -1 indicating this.
 *
 * @param[in] env  The JNI env.
 * @param[in] bean  The Class (static method).
 * @param[in] threadIDs   Array of thread ID whose native IDs needs to be returned.
 * @param[in,out] resultArray   Array to hold the native IDs, allocated and passed to be filled in.
 * @return  The native ID on success or NULL on failure .
 */
void JNICALL
Java_com_ibm_java_lang_management_internal_ThreadMXBeanImpl_getNativeThreadIdsImpl(JNIEnv *env, jobject bean, jlongArray threadIDs, jlongArray resultArray)
{
	PORT_ACCESS_FROM_ENV(env);
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9VMThread * vmThread = javaVM->mainThread;
	jlong *thrIdArr = NULL;
	jlong *nativeIds= NULL;
	jint arrLen = 0;
	jboolean isCopy = JNI_FALSE;
	IDATA iter = 0;

	Trc_JCL_threadmxbean_getNativeThreadIdsImpl_Entry(env);
	/* Java code checks for null */
	Assert_JCL_notNull(threadIDs);
	Assert_JCL_notNull(resultArray);

	/* Obtain the length of the array received. The output array must be of same length. */
	arrLen = (*env)->GetArrayLength(env, threadIDs);
	if (0 == arrLen) {
		Trc_JCL_threadmxbean_getNativeThreadIdsImpl_invalidArrLen(env);
		throwNewIllegalArgumentException(env, "Invalid thread identifier array received.");
		goto _exit;
	}
	/* Allocate space to hold a temporary array of native thread IDs we are about to find here. */
	nativeIds = j9mem_allocate_memory(arrLen * sizeof(jlong), J9MEM_CATEGORY_VM_JCL);
	if (NULL == nativeIds) {
		Trc_JCL_threadmxbean_getNativeThreadIdsImpl_outOfMemory(env, arrLen);
		javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
		goto _exit;
	}
	/* Extract primitive (jlong) array from the jlongArray passed by JNI.  Examine its elements for TIDs. */
	thrIdArr = (*env)->GetLongArrayElements(env, threadIDs, &isCopy);
	if (NULL == thrIdArr) {
		Trc_JCL_threadmxbean_getNativeThreadIdsImpl_failedGettingArrayElt(env);
		goto _exit;
	}
	enterVMFromJNI((J9VMThread *) env);
	omrthread_monitor_enter(javaVM->vmThreadListMutex);
	/* Loop through the array of thread IDs provided.  Populate the array nativeIds at the end of the loop. */
	for (iter = 0; iter < arrLen; iter++) {
		nativeIds[iter] = findNativeThreadId(vmThread, thrIdArr[iter]);
	}
	/* Release mutexes.  If thread counts go up or down from this point on, its acceptable as per the API. */
	omrthread_monitor_exit(javaVM->vmThreadListMutex);
	exitVMToJNI((J9VMThread *) env);
	(*env)->SetLongArrayRegion(env, resultArray, 0, (jsize) arrLen, nativeIds);

_exit:
	j9mem_free_memory(nativeIds);	/* Safe to free null */
	Trc_JCL_threadmxbean_getNativeThreadIdsImpl_Exit(env, resultArray);
}
