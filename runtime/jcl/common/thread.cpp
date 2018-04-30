/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
#include <stdlib.h>
#include "jcl.h"
#include "j9lib.h"
#include "j9protos.h"
#include "omrthread.h"
#include "jclprots.h"
#include "ut_j9jcl.h"
#include "j9jclnls.h"

#include "VMHelpers.hpp"
#include "ObjectMonitor.hpp"
#include "VMAccess.hpp"

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

void JNICALL
Java_java_lang_Thread_interruptImpl(JNIEnv *env, jobject rcv)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);
	J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, receiverObject);
	if (J9VMJAVALANGTHREAD_STARTED(currentThread, receiverObject)) {
		if (NULL != targetThread) {
			void (*sidecarInterruptFunction)(J9VMThread*) = vm->sidecarInterruptFunction;
			if (NULL != sidecarInterruptFunction) {
				sidecarInterruptFunction(targetThread);
			}
			omrthread_interrupt(targetThread->osThread);
		}
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
		if ((NULL == lockAddress) || ((j9objectmonitor_t)(UDATA)currentThread != *lockAddress)) {
			if (currentThread != getObjectMonitorOwner(vm, currentThread, lockObject, NULL)) {
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

	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(rcv);

	/* Assume the thread is alive (guaranteed by java caller) */
	J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, receiverObject);

	/* If calling getStackTrace on the current Thread, drop the first element, which is this method. */
	UDATA skipCount = (currentThread == targetThread) ? 1 : 0;

	j9object_t resultObject = getStackTraceForThread(currentThread, targetThread, skipCount);
	jobject result = vmFuncs->j9jni_createLocalRef(env, resultObject);
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
		UDATA priority = J9VMJAVALANGTHREAD_PRIORITY(currentThread, receiverObject);
		if (vm->runtimeFlags & J9_RUNTIME_NO_PRIORITIES) {
			priority = J9THREAD_PRIORITY_NORMAL;
		}
		UDATA privateFlags = 0;
		if (J9VMJAVALANGTHREAD_ISDAEMON(currentThread, receiverObject)) {
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

}
