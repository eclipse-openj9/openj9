/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include "j9jclnls.h"
#include "j9lib.h"
#include "j9protos.h"
#include "jcl.h"
#include "jcl_internal.h"
#include "jclprots.h"
#include "omrthread.h"
#include "ut_j9jcl.h"

#include "ObjectMonitor.hpp"
#include "VMAccess.hpp"
#include "VMHelpers.hpp"

extern "C" {

/* Enumeration values from java.lang.Thread.State: */
#define NEW 0
#define RUNNABLE 1
#define BLOCKED 2
#define WAITING 3
#define TIMED_WAITING 4
#define TERMINATED 5

jint
getJclThreadState(UDATA vmstate, jboolean started)
{
	switch (vmstate) {
	case J9VMTHREAD_STATE_RUNNING:
	case J9VMTHREAD_STATE_SUSPENDED:
		return RUNNABLE;
	case J9VMTHREAD_STATE_BLOCKED:
		return BLOCKED;
	case J9VMTHREAD_STATE_WAITING:
	case J9VMTHREAD_STATE_PARKED:
		return WAITING;
	case J9VMTHREAD_STATE_WAITING_TIMED:
	case J9VMTHREAD_STATE_PARKED_TIMED:
	case J9VMTHREAD_STATE_SLEEPING:
		return TIMED_WAITING;
	case J9VMTHREAD_STATE_DEAD:
		return TERMINATED;
	case J9VMTHREAD_STATE_UNKNOWN:
		if (started == JNI_TRUE) {
			return TERMINATED;
		}
		/* Return default */
	default:
		return NEW;
	}
}


jint JNICALL 
Java_java_lang_Thread_getStateImpl(JNIEnv *env, jobject recv, jlong threadRef)
{
	UDATA status;
	J9VMThread* vmThread = (J9VMThread *)(UDATA)threadRef;
	J9VMThread* currentThread = (J9VMThread*)env;
	jint state;

	Trc_JCL_Thread_getStateImpl_Entry(currentThread, vmThread);
	
	currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	currentThread->javaVM->internalVMFunctions->haltThreadForInspection(currentThread, vmThread);

	status = getVMThreadObjectState(vmThread, NULL, NULL, NULL);
	
	if (vmThread->threadObject) {
		state = getJclThreadState(status, J9VMJAVALANGTHREAD_STARTED(currentThread, vmThread->threadObject));
	} else {
		state = getJclThreadState(status, JNI_TRUE);
	}
	
	currentThread->javaVM->internalVMFunctions->resumeThreadForInspection(currentThread, vmThread);
	currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);

	Trc_JCL_Thread_getStateImpl_Exit(currentThread, status, state);
	return state;
}



/*
 * Sets the priority of a thread.
 * Assumes that the caller has:
 * - verified that threadRef is non-zero
 * - verified thread.started
 * - synchronized on thread.lock, so thread can't exit if it is alive
 * - verified the range of priority.
 */
void JNICALL
Java_java_lang_Thread_setPriorityNoVMAccessImpl(JNIEnv *env, jobject thread, jlong threadRef, jint priority)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	const UDATA *prioMap = currentThread->javaVM->java2J9ThreadPriorityMap;
	J9VMThread *vmThread = (J9VMThread *)(UDATA)threadRef;
	
	if (currentThread->javaVM->runtimeFlags & J9_RUNTIME_NO_PRIORITIES) {
		return;
	}
	
	Assert_JCL_notNull(vmThread);
	Assert_JCL_notNull(vmThread->osThread);
	Assert_JCL_notNull(prioMap);
	Assert_JCL_true(priority >= 0);
	Assert_JCL_true((size_t)priority < 
		sizeof(currentThread->javaVM->java2J9ThreadPriorityMap)/sizeof(currentThread->javaVM->java2J9ThreadPriorityMap[0]));
			
	omrthread_set_priority(vmThread->osThread, prioMap[priority]);
}


/*
 * Sets the thread native name to match the name in the Thread object.
 * The caller has synchronized on thread.lock, 
 * so the thread can't exit while its native name is being set by itself or by another thread
 */
void JNICALL 
Java_java_lang_Thread_setNameImpl(JNIEnv *env, jobject thread, jlong threadRef, jstring threadName)
{
	J9VMThread *currentThread = (J9VMThread *)env; 
	J9VMThread *vmThread = (J9VMThread *)(UDATA)threadRef;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *internalVMFunctions = javaVM->internalVMFunctions;
	j9object_t threadNameString;
		
	internalVMFunctions->internalEnterVMFromJNI(currentThread);
	threadNameString = J9_JNI_UNWRAP_REFERENCE(threadName);
	if(internalVMFunctions->setVMThreadNameFromString(
			currentThread, vmThread, threadNameString)) {
		internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);        
	} 
	internalVMFunctions->internalExitVMToJNI(currentThread);
}

void JNICALL
Java_java_lang_Thread_yield(JNIEnv *env, jclass threadClass)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	/* Check whether Thread.Stop has been called */
	if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_STOP)) {
		J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		vmFuncs->internalEnterVMFromJNI(currentThread);
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
		omrthread_monitor_enter(currentThread->publicFlagsMutex);
		vmFuncs->internalAcquireVMAccessNoMutex(currentThread);
#endif /*  J9VM_INTERP_ATOMIC_FREE_JNI */
		currentThread->currentException = currentThread->stopThrowable;
		currentThread->stopThrowable = NULL;
		clearEventFlag(currentThread, J9_PUBLIC_FLAGS_STOP);
		omrthread_clear_priority_interrupted();
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		vmFuncs->internalExitVMToJNI(currentThread);
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
		vmFuncs->internalReleaseVMAccessNoMutex(currentThread);
		omrthread_monitor_exit(currentThread->publicFlagsMutex);
#endif /*  J9VM_INTERP_ATOMIC_FREE_JNI */
	}
	omrthread_yield();
}

#if JAVA_SPEC_VERSION < 20
void JNICALL
Java_java_lang_Thread_resumeImpl(JNIEnv *env, jobject rcv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
	J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, receiverObject);
	Trc_JCL_threadResume(currentThread, targetThread);
	if (J9VMJAVALANGTHREAD_STARTED(currentThread, receiverObject)) {
		if (NULL != targetThread) {
			vmFuncs->clearHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

void JNICALL
Java_java_lang_Thread_suspendImpl(JNIEnv *env, jobject rcv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
	J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, receiverObject);
	Trc_JCL_threadSuspend(currentThread, targetThread);
	if (J9VMJAVALANGTHREAD_STARTED(currentThread, receiverObject)) {
		if (NULL != targetThread) {
			if (currentThread == targetThread) {
				/* Suspending the current thread will take place upon re-entering the VM after returning from
				 * this native.
				 */
				vmFuncs->setHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
			} else {
				vmFuncs->internalExitVMToJNI(currentThread);
				omrthread_monitor_enter(targetThread->publicFlagsMutex);
				VM_VMAccess::setHaltFlagForVMAccessRelease(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
				if (VM_VMAccess::mustWaitForVMAccessRelease(targetThread)) {
					while (J9_ARE_ALL_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND | J9_PUBLIC_FLAGS_VM_ACCESS)) {
						omrthread_monitor_wait(targetThread->publicFlagsMutex);
					}
				}
				omrthread_monitor_exit(targetThread->publicFlagsMutex);
				goto vmAccessReleased;
			}
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
vmAccessReleased: ;
}

void JNICALL
Java_java_lang_Thread_stopImpl(JNIEnv *env, jobject rcv, jobject stopThrowable)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
	j9object_t throwableObject = J9_JNI_UNWRAP_REFERENCE(stopThrowable);
	J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, receiverObject);
	Trc_JCL_threadStop(currentThread, targetThread, throwableObject);
	if (J9VMJAVALANGTHREAD_STARTED(currentThread, receiverObject)) {
		if (NULL != targetThread) {
			/* Stopping the current thread just means throwing the exception immediately */
			if (currentThread == targetThread) {
				VM_VMHelpers::setExceptionPending(currentThread, throwableObject);
			} else {
				/* Do not allow stops to be posted during Thread.cleanUp() */
				omrthread_monitor_enter(targetThread->publicFlagsMutex);
				if (J9_ARE_NO_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_STOPPED)) {
					targetThread->stopThrowable = throwableObject;
					vmFuncs->clearHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
					vmFuncs->setHaltFlag(targetThread, J9_PUBLIC_FLAGS_STOP);
					omrthread_priority_interrupt(targetThread->osThread);
				}
				omrthread_monitor_exit(targetThread->publicFlagsMutex);
			}
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}
#endif /* JAVA_SPEC_VERSION < 20 */

void JNICALL
Java_java_lang_Thread_interruptImpl(JNIEnv *env, jobject rcv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
#if defined(J9VM_OPT_CRIU_SUPPORT)
	if (J9_IS_SINGLE_THREAD_MODE(vm)) {
		vmFuncs->delayedLockingOperation(currentThread, receiverObject, J9_SINGLE_THREAD_MODE_OP_INTERRUPT);
		/* exception is already set if any */
	} else
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	{
		VM_VMHelpers::threadInterruptImpl(currentThread, receiverObject);
		Trc_JCL_Thread_interruptImpl(currentThread, receiverObject);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

jboolean JNICALL
Java_java_lang_Thread_holdsLock(JNIEnv *env, jclass threadClass, jobject obj)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jboolean result = JNI_TRUE;

	/* If the thread is alive, ask the OS thread.  Otherwise, answer false */

	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == obj) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t lockObject = J9_JNI_UNWRAP_REFERENCE(obj);
		j9objectmonitor_t *lockAddress = VM_ObjectMonitor::inlineGetLockAddress(currentThread, lockObject);
		if ((NULL == lockAddress) || ((j9objectmonitor_t)(UDATA)currentThread != J9_LOAD_LOCKWORD(currentThread, lockAddress))) {
			if (currentThread != getObjectMonitorOwner(vm, lockObject, NULL)) {
				result = JNI_FALSE;
			}
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return result;
}

jobject JNICALL
Java_java_lang_Thread_getStackTraceImpl(JNIEnv *env, jobject rcv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject result = NULL;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);

	/* Assume the thread is alive (guaranteed by java caller). */
	J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, receiverObject);

#if JAVA_SPEC_VERSION >= 19
	BOOLEAN releaseInspector = FALSE;
	if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, receiverObject)) {
		/* Do not spin when acquiring access, if acquire failed, return NULL.
		 * The caller of getStackTraceImpl will handle if should retry or get stack using unmounted path.
		 */
		if (!vmFuncs->acquireVThreadInspector(currentThread, rcv, FALSE)) {
			goto done;
		}
		j9object_t carrierThread = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, receiverObject);
		/* Ensure virtual thread is mounted and not during transition. */
		if (NULL != carrierThread) {
			releaseInspector = TRUE;
		} else {
			vmFuncs->releaseVThreadInspector(currentThread, rcv);
			goto done;
		}
		/* Gets targetThread from the carrierThread object. */
		targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, carrierThread);
		Assert_JCL_notNull(targetThread);
	}
	{
#endif /* JAVA_SPEC_VERSION >= 19 */

		/* If calling getStackTrace on the current Thread, drop the first element, which is this method. */
		UDATA skipCount = (currentThread == targetThread) ? 1 : 0;

		j9object_t resultObject = getStackTraceForThread(currentThread, targetThread, skipCount, receiverObject);
		result = vmFuncs->j9jni_createLocalRef(env, resultObject);

#if JAVA_SPEC_VERSION >= 19
	}
	if (releaseInspector) {
		receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
		/* Release the virtual thread (allow it to die) now that we are no longer inspecting it. */
		vmFuncs->releaseVThreadInspector(currentThread, rcv);
	}
done:
#endif /* JAVA_SPEC_VERSION >= 19 */

	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

void JNICALL
Java_java_lang_Thread_startImpl(JNIEnv *env, jobject rcv, jlong millis, jint nanos)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	/* If the thread is already started, throw an exception */
	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
	if (J9VMJAVALANGTHREAD_STARTED(currentThread, receiverObject)) {
		vmFuncs->setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALTHREADSTATEEXCEPTION, J9NLS_JCL_THREAD_ALREADY_STARTED);
	} else {
#if JAVA_SPEC_VERSION >= 19
		UDATA priority = 0;
		UDATA isDaemon = 0;
		j9object_t threadHolder = J9VMJAVALANGTHREAD_HOLDER(currentThread, receiverObject);
		if (NULL != threadHolder) {
			priority = J9VMJAVALANGTHREADFIELDHOLDER_PRIORITY(currentThread, threadHolder);
			isDaemon = J9VMJAVALANGTHREADFIELDHOLDER_DAEMON(currentThread, threadHolder);
		}
#else /* JAVA_SPEC_VERSION >= 19 */
		UDATA priority = J9VMJAVALANGTHREAD_PRIORITY(currentThread, receiverObject);
		UDATA isDaemon = J9VMJAVALANGTHREAD_ISDAEMON(currentThread, receiverObject);
#endif /* JAVA_SPEC_VERSION >= 19 */
		if (vm->runtimeFlags & J9_RUNTIME_NO_PRIORITIES) {
			priority = J9THREAD_PRIORITY_NORMAL;
		}
		UDATA privateFlags = 0;
		if (isDaemon) {
			privateFlags |= J9_PRIVATE_FLAGS_DAEMON_THREAD;
		}
		UDATA startRC = vmFuncs->startJavaThread(currentThread, receiverObject, privateFlags, vm->defaultOSStackSize, priority, (omrthread_entrypoint_t)vmFuncs->javaThreadProc, (void*)vm, NULL);
		switch(startRC) {
		case J9_THREAD_START_NO_ERROR:
		case J9_THREAD_START_THROW_CURRENT_EXCEPTION:
			break;
		case J9_THREAD_START_FAILED_VMTHREAD_ALLOC:
			vmFuncs->setThreadForkOutOfMemoryError(currentThread, J9NLS_JCL_FAILED_TO_ALLOCATE_VMTHREAD);
			break;
		case J9_THREAD_START_FAILED_TO_FORK_THREAD:
			vmFuncs->setThreadForkOutOfMemoryError(currentThread, J9NLS_JCL_FAILED_TO_FORK_OS_THREAD);
			break;
		default:
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			break;
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

#if JAVA_SPEC_VERSION >= 19
/* static native Thread currentCarrierThread(); */
jobject JNICALL
Java_java_lang_Thread_currentCarrierThread(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	jobject result = VM_VMHelpers::createLocalRef(env, currentThread->carrierThreadObject);
	VM_VMAccess::inlineExitVMToJNI(currentThread);

	return result;
}

/* private static native StackTraceElement[][] dumpThreads(Thread[] threads); */
jobjectArray JNICALL
Java_java_lang_Thread_dumpThreads(JNIEnv *env, jclass clazz, jobjectArray threads)
{
	Assert_JCL_unimplemented();
	return NULL;
}

/* private static native Thread[] getThreads(); */
jobjectArray JNICALL
Java_java_lang_Thread_getThreads(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobjectArray result = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->acquireExclusiveVMAccess(currentThread);

	jobject *threads = (jobject*)j9mem_allocate_memory(sizeof(jobject) * vm->totalThreadCount, J9MEM_CATEGORY_VM_JCL);
	if (NULL == threads) {
		vmFuncs->releaseExclusiveVMAccess(currentThread);
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		jobject *currentThreadPtr = threads;
		UDATA threadCount = 0;

		J9VMThread *targetThread = vm->mainThread;
		do {
#if JAVA_SPEC_VERSION >= 19
			/* carrierThreadObject should always point to a platform thread.
			 * Thus, all virtual threads should be excluded.
			 */
			j9object_t threadObject = targetThread->carrierThreadObject;
#else /* JAVA_SPEC_VERSION >= 19 */
			j9object_t threadObject = targetThread->threadObject;
#endif /* JAVA_SPEC_VERSION >= 19 */

			/* Only count live threads */
			if (NULL != threadObject) {
				if (J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject) && (NULL != J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject))) {
					*currentThreadPtr++ = vmFuncs->j9jni_createLocalRef(env, threadObject);
					threadCount += 1;
				}
			}
			targetThread = targetThread->linkNext;
		} while (targetThread != vm->mainThread);
		vmFuncs->releaseExclusiveVMAccess(currentThread);

		J9Class *arrayClass = fetchArrayClass(currentThread, J9VMJAVALANGTHREAD_OR_NULL(vm));
		if (NULL != arrayClass) {
			j9object_t arrayObject = vm->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, (U_32)threadCount, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			if (NULL != arrayObject) {
				for (UDATA i = 0; i < threadCount; i++) {
					J9JAVAARRAYOFOBJECT_STORE(currentThread, arrayObject, i, J9_JNI_UNWRAP_REFERENCE(threads[i]));
				}
				result = (jobjectArray)vmFuncs->j9jni_createLocalRef(env, arrayObject);
			} else {
				vmFuncs->setHeapOutOfMemoryError(currentThread);
			}
		}

		j9mem_free_memory(threads);
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return result;
}

/* private static native long getNextThreadIdOffset(); */
jlong JNICALL
Java_java_lang_Thread_getNextThreadIdOffset(JNIEnv *env, jclass clazz)
{
	J9JavaVM *vm = ((J9VMThread *)env)->javaVM;
	return (U_64)(uintptr_t)&(vm->nextTID);
}

void JNICALL
Java_java_lang_Thread_registerNatives(JNIEnv *env, jclass clazz)
{
	JNINativeMethod natives[] = {
		{
			(char*)"yield0",
			(char*)"()V",
			(void*)&Java_java_lang_Thread_yield
		}
	};
	jint numNatives = sizeof(natives)/sizeof(JNINativeMethod);
	env->RegisterNatives(clazz, natives, numNatives);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	clearNonZAAPEligibleBit(env, clazz, natives, numNatives);
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}

#if JAVA_SPEC_VERSION >= 20
/**
 * static native Object[] scopedValueCache();
 *
 * Get the scoped value cache.
 *
 * @param env instance of JNIEnv
 * @param unusedClass
 * @return jobjectArray the scoped value cache object array
 */
jobjectArray JNICALL
Java_java_lang_Thread_scopedValueCache(JNIEnv *env, jclass unusedClass)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	jobjectArray result = (jobjectArray)VM_VMHelpers::createLocalRef(env, currentThread->scopedValueCache);
	VM_VMAccess::inlineExitVMToJNI(currentThread);

	return result;
}

/**
 * static native void setScopedValueCache(Object[] cache);
 *
 * Set the scoped value cache.
 *
 * @param env instance of JNIEnv
 * @param unusedClass
 * @param cache the scoped value cache object array
 */
void JNICALL
Java_java_lang_Thread_setScopedValueCache(JNIEnv *env, jclass unusedClass, jobjectArray cache)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	currentThread->scopedValueCache = J9_JNI_UNWRAP_REFERENCE(cache);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}
#else /* JAVA_SPEC_VERSION >= 20 */
/**
 * static native Object[] extentLocalCache();
 *
 * Get the extent local cache.
 *
 * @param env instance of JNIEnv
 * @param unusedClass
 * @return jobjectArray
 */
jobjectArray JNICALL
Java_java_lang_Thread_extentLocalCache(JNIEnv *env, jclass unusedClass)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	jobjectArray result = (jobjectArray)VM_VMHelpers::createLocalRef(env, currentThread->scopedValueCache);
	VM_VMAccess::inlineExitVMToJNI(currentThread);

	return result;
}

/**
 * static native void setExtentLocalCache(Object[] cache);
 *
 * Set the extent local cache.
 *
 * @param env instance of JNIEnv
 * @param unusedClass
 * @param cache the extent local cache object array
 */
void JNICALL
Java_java_lang_Thread_setExtentLocalCache(JNIEnv *env, jclass unusedClass, jobjectArray cache)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	currentThread->scopedValueCache = J9_JNI_UNWRAP_REFERENCE(cache);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}
#endif /* JAVA_SPEC_VERSION >= 20 */
#endif /* JAVA_SPEC_VERSION >= 19 */

#if JAVA_SPEC_VERSION >= 20
/**
 * static native Object findScopedValueBindings();
 *
 * Find the most recent scoped value bindings in the stack.
 * TODO: Complete the function description.
 *
 * @param env instance of JNIEnv
 * @param unusedClass
 * @return jobject
 */
jobject JNICALL
Java_java_lang_Thread_findScopedValueBindings(JNIEnv *env, jclass unusedClass)
{
	/* TODO: Implement. See https://github.com/eclipse-openj9/openj9/issues/16677. */
	return NULL;
}

/**
 * static native void ensureMaterializedForStackWalk(Object o);
 *
 * This is expected to invoke JVM_EnsureMaterializedForStackWalk and ensure
 * that the stackwalk code sees a materialized value.
 *
 * @param env instance of JNIEnv
 * @param unusedClass
 * @param obj the object to be checked
 */
void JNICALL
Java_java_lang_Thread_ensureMaterializedForStackWalk(JNIEnv *env, jclass unusedClass, jobject obj)
{
	/* An empty implementation is sufficient since the hotspot
	 * JVM_EnsureMaterializedForStackWalk() does nothing.
	 * https://github.com/eclipse-openj9/openj9/issues/16577
	 */
}
#endif /* JAVA_SPEC_VERSION >= 20 */

}
