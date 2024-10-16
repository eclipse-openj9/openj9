/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <assert.h>
#include "jvmtiHelpers.h"
#include "jvmti_internal.h"

#if JAVA_SPEC_VERSION >= 19
#include "VMHelpers.hpp"
#endif /* JAVA_SPEC_VERSION >= 19 */

extern "C" {

typedef struct J9JVMTIRunAgentThreadArgs {
	jvmtiEnv *jvmti_env;
	jvmtiStartFunction proc;
	const void *arg;
} J9JVMTIRunAgentThreadArgs;

static int J9THREAD_PROC agentThreadStart(void *entryArg);
static jint walkLocalMonitorRefs(J9VMThread* currentThread, jobject *locks, J9VMThread *targetThread, J9VMThread *walkThread, UDATA maxCount);
static jvmtiError resumeThread(J9VMThread *currentThread, jthread thread);
static UDATA wrappedAgentThreadStart(J9PortLibrary *portLib, void *entryArg);
static void ownedMonitorIterator(J9VMThread *currentThread, J9StackWalkState *walkState, j9object_t *slot, const void *stackLocation);

#if JAVA_SPEC_VERSION >= 19
#include "HeapIteratorAPI.h"

typedef struct jvmtiVThreadCallBackData {
	const jthread *except_list;
	jint except_count;
	BOOLEAN is_suspend;
	BOOLEAN suspend_current_thread;
} jvmtiVThreadCallBackData;

static jvmtiIterationControl jvmtiSuspendResumeCallBack(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData);
#endif /* JAVA_SPEC_VERSION >= 19 */

jvmtiError JNICALL
jvmtiGetThreadState(jvmtiEnv *env,
	jthread thread,
	jint *thread_state_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jint rv_thread_state = 0;

	Trc_JVMTI_jvmtiGetThreadState_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;
		j9object_t threadObject = NULL;
		jboolean threadStartedFlag = JNI_FALSE;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_NON_NULL(thread_state_ptr);

		if (NULL != thread) {
			threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		} else {
			/* If the thread is NULL, then use the current thread. */
			threadObject = currentThread->threadObject;
		}

#if JAVA_SPEC_VERSION >= 19
		if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
			/* If thread is NULL, the current thread is used which cannot be a virtual thread.
			 * There is an assertion inside getVirtualThreadState() that thread is not NULL.
			 */
			if (NULL != thread) {
				rv_thread_state = getVirtualThreadState(currentThread, thread);
			} else {
				/* Create a local ref to pass the currentThread's threadObject since
				 * getVirtualThreadState releases and re-acquires VM access.
				 */
				JNIEnv *jniEnv = (JNIEnv *)currentThread;
				jobject virtualThreadRef = vmFuncs->j9jni_createLocalRef(jniEnv, threadObject);
				rv_thread_state = getVirtualThreadState(currentThread, (jthread)virtualThreadRef);
				vmFuncs->j9jni_deleteLocalRef(jniEnv, virtualThreadRef);
			}
		} else
#endif /* JAVA_SPEC_VERSION >= 19 */
		{
			rc = getVMThread(currentThread, thread, &targetThread, JVMTI_ERROR_NONE, 0);
			if (JVMTI_ERROR_NONE == rc) {
				/* Get the lock for the object. */
				j9object_t threadObjectLock = J9VMJAVALANGTHREAD_LOCK(currentThread, threadObject);
				if (NULL != threadObjectLock) {
					threadStartedFlag = (jboolean)J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject);
				} else {
					/* In this case, we must be early in the thread creation. We still need to call getVMThread so that
					 * the inspection count etc. are handled correctly, however, we just want to fall into the case were
					 * we return that the thread is NEW so we set the targetThread and threadStartedFlag to false.
					 */
					targetThread = NULL;
					threadStartedFlag = JNI_FALSE;
				}

				/* We use the values of targetThread and threadStartedFlag that were obtained while holding the lock on the
				 * thread so that get a consistent view of the values.  This is needed because if we don't get a consistent view
				 * we may think the thread is TERMINATED instead of just starting.
				 */
				if (NULL == targetThread) {
					/* If the thread is new, and we were not able to get and suspend its vmthread,
					 * we must not call getThreadState(). getThreadState() refetches the vmthread.
					 * If the second fetch succeeded, we would erroneously attempt to inspect the
					 * unsuspended thread.
					 */
					if (threadStartedFlag) {
						rv_thread_state = JVMTI_THREAD_STATE_TERMINATED;
					} else {
						/* Thread is new. */
						rv_thread_state = 0;
					}
				} else {
					vmFuncs->haltThreadForInspection(currentThread, targetThread);
					/* The VM thread can't be recycled because getVMThread() prevented it from exiting. */
#if JAVA_SPEC_VERSION >= 19
					rv_thread_state = getThreadState(currentThread, targetThread->carrierThreadObject);
#else /* JAVA_SPEC_VERSION >= 19 */
					rv_thread_state = getThreadState(currentThread, targetThread->threadObject);
#endif /* JAVA_SPEC_VERSION >= 19 */
					vmFuncs->resumeThreadForInspection(currentThread, targetThread);
				}
				releaseVMThread(currentThread, targetThread, thread);
			}
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	if (NULL != thread_state_ptr) {
		*thread_state_ptr = rv_thread_state; 
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadState);
}

jvmtiError JNICALL
jvmtiGetAllThreads(jvmtiEnv *env,
	jint *threads_count_ptr,
	jthread **threads_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_threads_count = 0;
	jthread *rv_threads = NULL;

	Trc_JVMTI_jvmtiGetAllThreads_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		jthread *threads = NULL;
			
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);					

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(threads_count_ptr);
		ENSURE_NON_NULL(threads_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		threads = (jthread *)j9mem_allocate_memory(sizeof(jthread) * vm->totalThreadCount, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (NULL == threads) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jthread *currentThreadPtr = threads;
			J9VMThread *targetThread = NULL;
			jint threadCount = 0;

			targetThread = vm->mainThread;
			do {
#if JAVA_SPEC_VERSION >= 19
				/* carrierThreadObject should always point to a platform thread.
				 * Thus, all virtual threads should be excluded.
				 */
				j9object_t threadObject = targetThread->carrierThreadObject;
#else /* JAVA_SPEC_VERSION >= 19 */
				j9object_t threadObject = targetThread->threadObject;
#endif /* JAVA_SPEC_VERSION >= 19 */

				/* Only count live threads. */
				if (NULL != threadObject) {
					if (J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject) && (J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject) != NULL)) {
						*currentThreadPtr++ = (jthread)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, (j9object_t)threadObject);
						++threadCount;
					}
				}
			} while ((targetThread = targetThread->linkNext) != vm->mainThread);

			rv_threads = threads;
			rv_threads_count = threadCount;
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != threads_count_ptr) {
		*threads_count_ptr = rv_threads_count;
	}
	if (NULL != threads_ptr) {
		*threads_ptr = rv_threads;
	}
	TRACE_JVMTI_RETURN(jvmtiGetAllThreads);
}

jvmtiError JNICALL
jvmtiSuspendThread(jvmtiEnv *env,
	jthread thread)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiSuspendThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		BOOLEAN currentThreadSuspended = FALSE;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);

		rc = suspendThread(currentThread, thread, TRUE, &currentThreadSuspended);

		/* If the current thread was suspended, block now until it is resumed. */
		if (currentThreadSuspended) {
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			setHaltFlag(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
			vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSuspendThread);
}

jvmtiError JNICALL
jvmtiSuspendThreadList(jvmtiEnv *env,
	jint request_count,
	const jthread *request_list,
	jvmtiError *results)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiSuspendThreadList_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		jint i = 0;
		BOOLEAN currentThreadEverSuspended = FALSE;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);

		ENSURE_NON_NEGATIVE(request_count);
		ENSURE_NON_NULL(request_list);
		ENSURE_NON_NULL(results);

		for (i = 0; i < request_count; ++i) {
			BOOLEAN currentThreadSuspended = FALSE;
			results[i] = suspendThread(currentThread, request_list[i], FALSE, &currentThreadSuspended);
			currentThreadEverSuspended |= currentThreadSuspended;
		}

		/* If the current thread appeared in the list (and was marked as suspended), block now until the thread is resumed. */
		if (currentThreadEverSuspended) {
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			setHaltFlag(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
			vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	}

	TRACE_JVMTI_RETURN(jvmtiSuspendThreadList);
}

jvmtiError JNICALL
jvmtiResumeThread(jvmtiEnv *env,
	jthread thread)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiResumeThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);

		rc = resumeThread(currentThread, thread);
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiResumeThread);
}

jvmtiError JNICALL
jvmtiResumeThreadList(jvmtiEnv *env,
	jint request_count,
	const jthread *request_list,
	jvmtiError *results)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiResumeThreadList_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		jint i = 0;
		
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);

		ENSURE_NON_NEGATIVE(request_count);
		ENSURE_NON_NULL(request_list);
		ENSURE_NON_NULL(results);

		for (i = 0; i < request_count; ++i) {
			results[i] = resumeThread(currentThread, request_list[i]);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiResumeThreadList);
}

jvmtiError JNICALL
jvmtiStopThread(jvmtiEnv *env,
	jthread thread,
	jobject exception)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiStopThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_signal_thread);

		ENSURE_JOBJECT_NON_NULL(exception);

		rc = getVMThread(
				currentThread, thread, &targetThread,
				JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD | J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
#if JAVA_SPEC_VERSION >= 21
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);

			/* Error if a virtual thread is not suspended and not the current thread. */
			if ((currentThread != targetThread)
			&& (!VM_VMHelpers::isThreadSuspended(currentThread, threadObject))
			&& IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)
			) {
				rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
				goto release;
			}

			/* Error if a virtual thread is unmounted since it won't be able to throw an
			 * asynchronous exception from the current frame.
			 */
			if (NULL == targetThread) {
				rc = JVMTI_ERROR_OPAQUE_FRAME;
				goto release;
			}
#endif /* JAVA_SPEC_VERSION >= 21 */
			omrthread_monitor_enter(targetThread->publicFlagsMutex);
			if (OMR_ARE_NO_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_STOPPED)) {
				omrthread_priority_interrupt(targetThread->osThread);
				targetThread->stopThrowable = J9_JNI_UNWRAP_REFERENCE(exception);
				setHaltFlag(targetThread, J9_PUBLIC_FLAGS_STOP);
			}
			omrthread_monitor_exit(targetThread->publicFlagsMutex);
#if JAVA_SPEC_VERSION >= 21
release:
#endif /* JAVA_SPEC_VERSION >= 21 */
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiStopThread);
}

jvmtiError JNICALL
jvmtiInterruptThread(jvmtiEnv *env,
	jthread thread)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
#if JAVA_SPEC_VERSION >= 19
	BOOLEAN isVirtualThread = FALSE;
	J9Class *vThreadClass = NULL;
	jclass vThreadJClass = NULL;
#endif /* JAVA_SPEC_VERSION >= 19 */

	Trc_JVMTI_jvmtiInterruptThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
#if JAVA_SPEC_VERSION >= 19
		JNIEnv *jniEnv = (JNIEnv *)currentThread;
#endif /* JAVA_SPEC_VERSION >= 19 */

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_signal_thread);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD | J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
#if JAVA_SPEC_VERSION >= 19
			j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
			isVirtualThread = IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject);

			if (isVirtualThread && (NULL == vm->vThreadInterrupt)) {
				vThreadClass = J9VMJAVALANGVIRTUALTHREAD_OR_NULL(vm);
				vThreadJClass = (jclass)vmFuncs->j9jni_createLocalRef(jniEnv, vThreadClass->classObject);
			}

			if ((NULL != targetThread) && !isVirtualThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				omrthread_interrupt(targetThread->osThread);
#ifdef J9VM_OPT_SIDECAR
				if (NULL != vm->sidecarInterruptFunction) {
					vm->sidecarInterruptFunction(targetThread);
				}
#endif /* J9VM_OPT_SIDECAR */
			}
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);

#if JAVA_SPEC_VERSION >= 19
		if ((JVMTI_ERROR_NONE == rc) && isVirtualThread) {
			if (NULL == vm->vThreadInterrupt) {
				jmethodID vThreadInterrupt = jniEnv->GetMethodID(vThreadJClass, "interrupt", "()V");

				if (NULL == vThreadInterrupt) {
					rc = JVMTI_ERROR_INTERNAL;
					goto exit;
				}

				vm->vThreadInterrupt = vThreadInterrupt;
			}

			jniEnv->CallObjectMethod(thread, vm->vThreadInterrupt);

			if (jniEnv->ExceptionCheck()) {
				rc = JVMTI_ERROR_INTERNAL;
			}
		}
exit:;
#endif /* JAVA_SPEC_VERSION >= 19 */
	}

	TRACE_JVMTI_RETURN(jvmtiInterruptThread);
}

jvmtiError JNICALL
jvmtiGetThreadInfo(jvmtiEnv *env,
	jthread thread,
	jvmtiThreadInfo *info_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);
	char *rv_name = NULL;
	jint rv_priority = 0;
	jboolean rv_is_daemon = JNI_FALSE;
	jthreadGroup rv_thread_group = NULL;
	jobject rv_context_class_loader = NULL;

	Trc_JVMTI_jvmtiGetThreadInfo_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_NON_NULL(info_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, JVMTI_ERROR_NONE, 0);
		if (JVMTI_ERROR_NONE == rc) {
			char *name = NULL;
			j9object_t threadObject = (NULL == thread) ? targetThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			jobject threadGroup = NULL;
			jobject contextClassLoader = NULL;
#if JAVA_SPEC_VERSION >= 19
			j9object_t threadHolder = J9VMJAVALANGTHREAD_HOLDER(currentThread, threadObject);
			BOOLEAN isVirtual = IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject);
#endif /* JAVA_SPEC_VERSION >= 19 */

			if ((NULL == targetThread)
#if JAVA_SPEC_VERSION >= 19
				|| isVirtual
#endif /* JAVA_SPEC_VERSION >= 19 */
			) {
				/* java.lang.Thread may not have a ref to VM thread, for example, after it gets terminated.
				 * We still want to return the name, so we retrieve it from the thread object itself.
				 * Do this for virtual threads as well since they are not mapped to a VM thread.
				 */
				j9object_t threadName = J9VMJAVALANGTHREAD_NAME(currentThread, threadObject);

				if (NULL == threadName) {
					name = (char *)j9mem_allocate_memory(1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
					if (NULL == name) {
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
						goto release;
					}
					name[0] = '\0';
				} else {
					name = vm->internalVMFunctions->copyStringToUTF8WithMemAlloc(currentThread, threadName, J9_STR_NULL_TERMINATE_RESULT, "", 0, NULL, 0, NULL);
					if (NULL == name) {
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
						goto release;
					}
				}
			} else {
				char *threadName = getOMRVMThreadName(targetThread->omrVMThread);
				size_t threadNameLen = (NULL == threadName) ? 1 : (strlen(threadName) + 1);

				/* Be sure to allocate at least one byte for the nul termination. */
				name = (char *)j9mem_allocate_memory(threadNameLen, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (NULL == name) {
					/* Failed to allocate memory, so release VMTthread and exit. */
					releaseOMRVMThreadName(targetThread->omrVMThread);
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					goto release;
				}

				if (NULL == threadName) {
					name[0] = '\0';
				} else {
					memcpy(name, threadName, threadNameLen);
				}

				releaseOMRVMThreadName(targetThread->omrVMThread);
			}

			if (J9_ARE_ALL_BITS_SET(vm->jclFlags, J9_JCL_FLAG_THREADGROUPS)) {
				j9object_t group = NULL;

#if JAVA_SPEC_VERSION >= 19
				if (isVirtual) {
					/* For virtual threads, check its state. */
					if (JVMTI_VTHREAD_STATE_TERMINATED != J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObject)) {
						/* The thread group of a virtual thread is always j.l.Thread$Constants.VTHREAD_GROUP. */
						group = J9_JNI_UNWRAP_REFERENCE(vm->vthreadGroup);
					}
				} else {
					/* For platform threads, a NULL vmthread indicates that a thread is either unstarted or terminated.
					 * As per the JVMTI spec, the thread group should be NULL IFF the thread is terminated.
					 */
					if (((NULL != targetThread)
						|| !(jboolean)J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject))
					&& (NULL != threadHolder)
					) {
						group = (j9object_t)J9VMJAVALANGTHREADFIELDHOLDER_GROUP(currentThread, threadHolder);
					}
				}
#else /* JAVA_SPEC_VERSION >= 19 */
				if (NULL != targetThread) {
					group = (j9object_t)J9VMJAVALANGTHREAD_GROUP(currentThread, threadObject);
				}
#endif /* JAVA_SPEC_VERSION >= 19 */
				if (NULL != group) {
					threadGroup = vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, group);
				}
			}

			contextClassLoader = vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, J9VMJAVALANGTHREAD_CONTEXTCLASSLOADER(currentThread, threadObject));

			rv_name = name;
			{
#if JAVA_SPEC_VERSION >= 19
				if (isVirtual || (NULL == threadHolder)) {
					rv_priority = JVMTI_THREAD_NORM_PRIORITY;
				} else {
					rv_priority = J9VMJAVALANGTHREADFIELDHOLDER_PRIORITY(currentThread, threadHolder);
				}
#else /* JAVA_SPEC_VERSION >= 19 */
				rv_priority = J9VMJAVALANGTHREAD_PRIORITY(currentThread, threadObject);
#endif /* JAVA_SPEC_VERSION >= 19 */
			}

#if JAVA_SPEC_VERSION >= 19
			if (isVirtual) {
				rv_is_daemon = JNI_TRUE;
			} else {
				if (NULL == threadHolder) {
					rv_is_daemon = JNI_FALSE;
				} else {
					rv_is_daemon = J9VMJAVALANGTHREADFIELDHOLDER_DAEMON(currentThread, threadHolder) ? JNI_TRUE : JNI_FALSE;
				}
			}
#else /* JAVA_SPEC_VERSION >= 19 */
			rv_is_daemon = J9VMJAVALANGTHREAD_ISDAEMON(currentThread, threadObject) ? JNI_TRUE : JNI_FALSE;
#endif /* JAVA_SPEC_VERSION >= 19 */
			rv_thread_group = (jthreadGroup)threadGroup;
			rv_context_class_loader = contextClassLoader;

release:
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != info_ptr) {
		info_ptr->name = rv_name;
		info_ptr->priority = rv_priority;
		info_ptr->is_daemon = rv_is_daemon;
		info_ptr->thread_group = rv_thread_group;
		info_ptr->context_class_loader = rv_context_class_loader;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadInfo);
}

jvmtiError JNICALL
jvmtiGetOwnedMonitorInfo(jvmtiEnv *env,
	jthread thread,
	jint *owned_monitor_count_ptr,
	jobject **owned_monitors_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_owned_monitor_count = 0;
	jobject *rv_owned_monitors = NULL;

	Trc_JVMTI_jvmtiGetOwnedMonitorInfo_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_owned_monitor_info);

		ENSURE_NON_NULL(owned_monitor_count_ptr);
		ENSURE_NON_NULL(owned_monitors_ptr);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
			jobject *locks = NULL;
			jint count = 0;
			J9VMThread *threadToWalk = targetThread;

#if JAVA_SPEC_VERSION >= 19
			J9VMThread stackThread = {0};
			J9VMEntryLocalStorage els = {0};
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			J9VMContinuation *continuation = NULL;

			if ((NULL == targetThread) && IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
				goto release;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */

			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);

#if JAVA_SPEC_VERSION >= 19
			continuation = getJ9VMContinuationToWalk(currentThread, targetThread, threadObject);
			if (NULL != continuation) {
				memcpy(&stackThread, targetThread, sizeof(J9VMThread));
				vm->internalVMFunctions->copyFieldsFromContinuation(currentThread, &stackThread, &els, continuation);
				threadToWalk = &stackThread;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */

			count = walkLocalMonitorRefs(currentThread, NULL, targetThread, threadToWalk, UDATA_MAX);

			locks = (jobject *)j9mem_allocate_memory(sizeof(jobject) * count, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (NULL == locks) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else if (0 != count) {
				/* Refetch count as it may decrease due to uniquifying the object list.
				 * Only fill up 'locks' with number of monitors recorded in the first pass.
				 * Suppress any newly acquired monitors in between.
				 */
				count = walkLocalMonitorRefs(currentThread, locks, targetThread, threadToWalk, count);
			}
			rv_owned_monitors = locks;
			rv_owned_monitor_count = count;

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
#if JAVA_SPEC_VERSION >= 19
release:
#endif /* JAVA_SPEC_VERSION >= 19 */
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != owned_monitor_count_ptr) {
		*owned_monitor_count_ptr = rv_owned_monitor_count;
	}
	if (NULL != owned_monitors_ptr) {
		*owned_monitors_ptr = rv_owned_monitors;
	}
	TRACE_JVMTI_RETURN(jvmtiGetOwnedMonitorInfo);
}

jvmtiError JNICALL
jvmtiGetOwnedMonitorStackDepthInfo(jvmtiEnv *env,
	jthread thread,
	jint *monitor_info_count_ptr,
	jvmtiMonitorStackDepthInfo **monitor_info_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_monitor_info_count = 0;
	jvmtiMonitorStackDepthInfo *rv_monitor_info = NULL;

	Trc_JVMTI_jvmtiGetOwnedMonitorStackDepthInfo_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_owned_monitor_stack_depth_info);

		ENSURE_NON_NULL(monitor_info_count_ptr);
		ENSURE_NON_NULL(monitor_info_ptr);
		
		rv_monitor_info_count = 0;
		
		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
			IDATA maxRecords = 0;
			J9ObjectMonitorInfo *monitorEnterRecords = NULL;
			jvmtiMonitorStackDepthInfo *resultArray = NULL;
			J9VMThread *threadToWalk = targetThread;

#if JAVA_SPEC_VERSION >= 19
			J9VMThread stackThread = {0};
			J9VMEntryLocalStorage els = {0};
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			J9VMContinuation *continuation = NULL;

			if ((NULL == targetThread) && IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
				goto release;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */

			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);

#if JAVA_SPEC_VERSION >= 19
			continuation = getJ9VMContinuationToWalk(currentThread, targetThread, threadObject);
			if (NULL != continuation) {
				memcpy(&stackThread, targetThread, sizeof(J9VMThread));
				vm->internalVMFunctions->copyFieldsFromContinuation(currentThread, &stackThread, &els, continuation);
				threadToWalk = &stackThread;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */

			/* Get the count of owned monitors. */
			maxRecords = vm->internalVMFunctions->getOwnedObjectMonitors(currentThread, threadToWalk, NULL, 0, TRUE);
			if (maxRecords < 0) {
				rc = JVMTI_ERROR_INTERNAL;
				goto freeMemory;
			}

			/* Do we have any enter records at all? */
			if (0 == maxRecords) {
				rc = JVMTI_ERROR_NONE;
				goto freeMemory;
			}

			monitorEnterRecords = (J9ObjectMonitorInfo *)j9mem_allocate_memory(sizeof(J9ObjectMonitorInfo) * (UDATA)maxRecords, J9MEM_CATEGORY_JVMTI);
			if (NULL == monitorEnterRecords) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto freeMemory;
			}

			maxRecords = vm->internalVMFunctions->getOwnedObjectMonitors(currentThread, threadToWalk, monitorEnterRecords, maxRecords, TRUE);
			if (maxRecords < 0) {
				rc = JVMTI_ERROR_INTERNAL;
				goto freeMemory;
			}

			/* Do we have any records at all? */
			if (maxRecords > 0) {
				resultArray = (jvmtiMonitorStackDepthInfo *)j9mem_allocate_memory((jlong)maxRecords * sizeof(jvmtiMonitorStackDepthInfo), J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (NULL == resultArray) {
					maxRecords = 0;
					resultArray = NULL;
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
				} else {
					IDATA i = 0;
					for (i = 0; i < maxRecords; i++) {
						/* Negative stack depth indicates inability to retrieve it. */
						if (monitorEnterRecords[i].depth > 0) {
							/* Subtract one since jvmti indexes stack depth starting at 0 instead of 1. */
							resultArray[i].stack_depth = (jint)monitorEnterRecords[i].depth - 1;
						} else {
							resultArray[i].stack_depth = (jint)monitorEnterRecords[i].depth;
						}
						resultArray[i].monitor =
							currentThread->javaVM->internalVMFunctions->j9jni_createLocalRef(
											(JNIEnv *)currentThread, monitorEnterRecords[i].object);
					}
				}
			}

			rv_monitor_info_count = (jint)maxRecords;
			rv_monitor_info = resultArray; 

freeMemory:
			if (NULL != monitorEnterRecords) {
				j9mem_free_memory(monitorEnterRecords);
			}

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
#if JAVA_SPEC_VERSION >= 19
release:
#endif /* JAVA_SPEC_VERSION >= 19 */
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != monitor_info_count_ptr) {
		*monitor_info_count_ptr = rv_monitor_info_count;
	}
	if (NULL != monitor_info_ptr) {
		*monitor_info_ptr = rv_monitor_info;
	}
	TRACE_JVMTI_RETURN(jvmtiGetOwnedMonitorStackDepthInfo);
}

jvmtiError JNICALL
jvmtiGetCurrentContendedMonitor(jvmtiEnv *env,
	jthread thread,
	jobject *monitor_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jobject rv_monitor = NULL;

	Trc_JVMTI_jvmtiGetCurrentContendedMonitor_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_current_contended_monitor);

		ENSURE_NON_NULL(monitor_ptr);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
			j9object_t lockObject = NULL;
			UDATA vmstate = 0;

#if JAVA_SPEC_VERSION >= 19
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);

			/* Unmounted VirtualThread and CarrierThread with VirtualThread mounted cannot be contended. */
			if (((NULL == targetThread) && IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject))
			|| ((NULL != targetThread) && (threadObject == targetThread->carrierThreadObject) && (NULL != targetThread->currentContinuation))
			) {
				goto release;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */

			/* CMVC 184481 - The targetThread should be suspended while we attempt to get its state. */
			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);

			vmstate = getVMThreadObjectStatesAll(targetThread, &lockObject, NULL, NULL);

			if ((NULL != lockObject)
			&& (OMR_ARE_NO_BITS_SET(vmstate, J9VMTHREAD_STATE_PARKED | J9VMTHREAD_STATE_PARKED_TIMED))
			) {
				rv_monitor = (jobject)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, lockObject);
			} else {
				rv_monitor = NULL;
			}

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
#if JAVA_SPEC_VERSION >= 19
release:
#endif /* JAVA_SPEC_VERSION >= 19 */
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != monitor_ptr) {
		*monitor_ptr = rv_monitor;
	}
	TRACE_JVMTI_RETURN(jvmtiGetCurrentContendedMonitor);
}

jvmtiError JNICALL
jvmtiRunAgentThread(jvmtiEnv *env,
	jthread thread,
	jvmtiStartFunction proc,
	const void *arg,
	jint priority)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiRunAgentThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9JVMTIRunAgentThreadArgs *args = NULL;
		PORT_ACCESS_FROM_JAVAVM(vm);

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_JOBJECT_NON_NULL(thread);
		ENSURE_JTHREAD(currentThread, thread);
#if JAVA_SPEC_VERSION >= 19
		ENSURE_JTHREAD_NOT_VIRTUAL(currentThread, thread, JVMTI_ERROR_UNSUPPORTED_OPERATION);
#endif /* JAVA_SPEC_VERSION >= 19 */
		/* Perhaps verify that thread has not already been started? */
		ENSURE_NON_NULL(proc);
		if ((priority < JVMTI_THREAD_MIN_PRIORITY) || (priority > JVMTI_THREAD_MAX_PRIORITY)) {
			JVMTI_ERROR(JVMTI_ERROR_INVALID_PRIORITY);
		}

		/* Create entry args for the thread proc. */
		args = (J9JVMTIRunAgentThreadArgs *)j9mem_allocate_memory(sizeof(J9JVMTIRunAgentThreadArgs), J9MEM_CATEGORY_JVMTI);
		if (NULL == args) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
			UDATA result = 0;

			args->jvmti_env = env;
			args->proc = (jvmtiStartFunction)J9_COMPATIBLE_FUNCTION_POINTER(proc);
			args->arg = arg;

			/* Run the thread. */
#if JAVA_SPEC_VERSION >= 19
			j9object_t threadHolder = J9VMJAVALANGTHREAD_HOLDER(currentThread, threadObject);
			if (NULL != threadHolder) {
				J9VMJAVALANGTHREADFIELDHOLDER_SET_PRIORITY(currentThread, threadHolder, priority);
				J9VMJAVALANGTHREADFIELDHOLDER_SET_DAEMON(currentThread, threadHolder, TRUE);
			}
#else /* JAVA_SPEC_VERSION >= 19 */
			J9VMJAVALANGTHREAD_SET_PRIORITY(currentThread, threadObject, priority);
			J9VMJAVALANGTHREAD_SET_ISDAEMON(currentThread, threadObject, TRUE);
#endif /* JAVA_SPEC_VERSION >= 19 */
			result = vm->internalVMFunctions->startJavaThread(currentThread, threadObject,
				J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_EXCEPTION_IN_START_JAVA_THREAD,
				vm->defaultOSStackSize, (UDATA)priority, agentThreadStart, args, NULL);
			if (result != J9_THREAD_START_NO_ERROR) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			}
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiRunAgentThread);
}

jvmtiError JNICALL
jvmtiSetThreadLocalStorage(jvmtiEnv *env,
	jthread thread,
	const void *data)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiSetThreadLocalStorage_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
			J9JVMTIEnv *j9env = (J9JVMTIEnv *)env;
			J9JVMTIThreadData *threadData = NULL;
			/* Use the current thread if the thread isn't specified. */
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);

#if JAVA_SPEC_VERSION >= 19
			if (NULL == targetThread) {
				/* targetThread is NULL when the virtual thread is yielded. It is only used to fetch the javaVM so
				 * set it to currentThread in this case.
				 */
				targetThread = currentThread;
			}
			rc = allocateTLS(vm, threadObject);
			if (JVMTI_ERROR_NONE != rc) {
				goto release;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */
			rc = createThreadData(j9env, targetThread, threadObject);
			if (JVMTI_ERROR_NONE != rc) {
				goto release;
			}
			threadData = jvmtiTLSGet(targetThread, threadObject, j9env->tlsKey);
			/* threadData cannot be NULL here since createThreadData has succeeded. */
			threadData->tls = (void *)data;

release:
			releaseVMThread(currentThread, targetThread, thread);
		}

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSetThreadLocalStorage);
}

jvmtiError JNICALL
jvmtiGetThreadLocalStorage(jvmtiEnv *env,
	jthread thread,
	void **data_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
	void *rv_data = NULL;

	Trc_JVMTI_jvmtiGetThreadLocalStorage_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_NON_NULL(data_ptr);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
			J9JVMTIEnv *j9env = (J9JVMTIEnv *)env;
			J9JVMTIThreadData *threadData = NULL;
			/* Use the current thread if the thread isn't specified. */
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);

#if JAVA_SPEC_VERSION >= 19
			if (NULL == targetThread) {
				/* targetThread is NULL when the virtual thread is yielded. It is only used to fetch the javaVM so
				 * set it to currentThread in this case.
				 */
				targetThread = currentThread;
			}
			void *tlsArray = J9OBJECT_ADDRESS_LOAD_VM(vm, threadObject, vm->tlsOffset);
			if (NULL == tlsArray) {
				goto release;
			}
#endif /* JAVA_SPEC_VERSION >= 19 */
			threadData = jvmtiTLSGet(targetThread, threadObject, j9env->tlsKey);
			if (NULL == threadData) {
				goto release;
			}
			rv_data = threadData->tls;

release:
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != data_ptr) {
		*data_ptr = rv_data;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadLocalStorage);
}

static jvmtiError
resumeThread(J9VMThread *currentThread, jthread thread)
{
	J9VMThread *targetThread = NULL;
	jvmtiError rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD | J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
	if (JVMTI_ERROR_NONE == rc) {
#if JAVA_SPEC_VERSION >= 19
		J9JavaVM *vm = currentThread->javaVM;
		j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		J9VMThread *carrierVMThread = NULL;

		/* Update internal flag before clearing flag to avoid sync issue. */
		if (!VM_VMHelpers::isThreadSuspended(currentThread, threadObject)) {
			rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
		} else {
			carrierVMThread = VM_VMHelpers::getCarrierVMThread(currentThread, threadObject);
			J9OBJECT_U64_STORE(currentThread, threadObject, vm->internalSuspendStateOffset, (U_64)carrierVMThread);
			/* Check if this is a vthread in transition. */
			if (NULL != carrierVMThread) {
				MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
				I_64 vthreadInspectorCount = J9OBJECT_I64_LOAD(currentThread, threadObject, vm->virtualThreadInspectorCountOffset);

				/* Increment inspector count by 1 to reverse the decrement done during suspend. */
				while (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(currentThread, threadObject, vm->virtualThreadInspectorCountOffset, vthreadInspectorCount, vthreadInspectorCount + 1)) {
					vthreadInspectorCount = J9OBJECT_I64_LOAD(currentThread, threadObject, vm->virtualThreadInspectorCountOffset);
				}
				Assert_JVMTI_true(vthreadInspectorCount < 0);
				targetThread = (J9VMThread *)carrierVMThread;
				/* For virtual thread in transition, ensure thread cannot start running until releaseVMThread is called. */
				vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
				threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
			}
		}

		/* The J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND will be cleared only
		 * if the thread is resumed and it has been mounted
		 * i.e. threadObject == targetThread->threadObject.
		 * or if the thread is suspended in transition.
		 */
		if ((NULL != targetThread) && ((threadObject == targetThread->threadObject) || (NULL != carrierVMThread)))
#endif /* JAVA_SPEC_VERSION >= 19 */
		{
			if (OMR_ARE_ANY_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)) {
				clearHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
				Trc_JVMTI_threadResumed(targetThread);
			} else {
				rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
			}
		}
		releaseVMThread(currentThread, targetThread, thread);
#if JAVA_SPEC_VERSION >= 19
		if (0 != carrierVMThread) {
			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
		}
#endif /* JAVA_SPEC_VERSION >= 19 */
	}

	return rc;
}

static int J9THREAD_PROC
agentThreadStart(void *entryArg)
{
	UDATA result = 0;
	J9JVMTIRunAgentThreadArgs *args = (J9JVMTIRunAgentThreadArgs *)entryArg;
	J9JavaVM *vm = JAVAVM_FROM_ENV((J9JVMTIEnv *)args->jvmti_env);
	J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	vmThread->gpProtected = 1;

	j9sig_protect(wrappedAgentThreadStart, args, 
		vmThread->javaVM->internalVMFunctions->structuredSignalHandler, vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&result);

	vm->internalVMFunctions->exitJavaThread(vm);
	/* Execution never reaches this point. */
	return 0;
}

static UDATA
wrappedAgentThreadStart(J9PortLibrary *portLib, void *entryArg)
{
	J9JVMTIRunAgentThreadArgs *args = (J9JVMTIRunAgentThreadArgs *)entryArg;
	J9JavaVM *vm = JAVAVM_FROM_ENV((J9JVMTIEnv *)args->jvmti_env);
	J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
	jvmtiEnv *jvmti_env = args->jvmti_env;
	jvmtiStartFunction proc = args->proc;
	const void *arg = args->arg;
	PORT_ACCESS_FROM_PORT(portLib);

	j9mem_free_memory(args);

	initializeCurrentOSStackFree(vmThread, vmThread->osThread, vm->defaultOSStackSize);

	vm->internalVMFunctions->threadAboutToStart(vmThread);

	TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, vmThread, vmThread);

	/* Run the agent thread. */
	proc(jvmti_env, (JNIEnv *)vmThread, (void *)arg);

	/* The following is not quite correct - may need a new destructor. */
	vmThread->currentException = NULL;
	vm->internalVMFunctions->threadCleanup(vmThread, TRUE);

	/* Exit the OS thread. */
	return 0;
}

static void
ownedMonitorIterator(J9VMThread *currentThread, J9StackWalkState *walkState, j9object_t *slot, const void *stackLocation)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9VMThread *targetThread = (J9VMThread *)walkState->userData1;
	jobject *locks = (jobject *)walkState->userData2;
	UDATA count = (UDATA)walkState->userData3;
	j9object_t obj = (NULL != slot) ? *slot : NULL;

	if (count >= (UDATA)walkState->userData4) {
		/* PMR 69633,001,866 / CMVC 197115.
		 * There is a race condition that target thread actually acquires monitor(s)
		 * between first and second pass (monitor enter is done outside of vm access).
		 * userData4 is initialized to UDATA_MAX.
		 * 1) At first pass, count will never be bigger than UDATA_MAX.
		 * 2) At second pass, userData4 now holds the 'count' value obtained from first pass.
		 */
		return;
	}

	if ((NULL != obj)
	&& (getObjectMonitorOwner(vm, obj, NULL) == targetThread)
	&& !isObjectStackAllocated(walkState->walkThread, obj)
	) {
		if (NULL != locks) {
			UDATA i = 0;
			/* Make sure each object only appears once in the list - n^2, but we expect n to be small. */
			for (i = 0; i < count; ++i) {
				if (J9_JNI_UNWRAP_REFERENCE(locks[i]) == obj) {
					return;
				}
			}
			locks[count] = (jobject)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, obj);
		}
		walkState->userData3 = (void *)(count + 1);
	}
}

static jint
walkLocalMonitorRefs(J9VMThread *currentThread, jobject *locks, J9VMThread *targetThread, J9VMThread *walkThread, UDATA maxCount)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9StackWalkState walkState = {0};
	J9JNIReferenceFrame *frame = NULL;
	pool_state poolState = {0};
	j9object_t *ref = NULL;

	walkState.userData1 = targetThread;                     /* target thread */
	walkState.userData2 = locks;                            /* array */
	walkState.userData3 = (void *)0;                        /* count */
	walkState.userData4 = (void *)maxCount;                 /* max count */
	walkState.objectSlotWalkFunction = ownedMonitorIterator;
	walkState.walkThread = walkThread;
	walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS;
	walkState.skipCount = 0;

	/* Check the Java stack. */
	vm->walkStackFrames(currentThread, &walkState);

	frame = (J9JNIReferenceFrame *)targetThread->jniLocalReferences;

	/* Check the local JNI refs. */
	while (NULL != frame) {
		ref = (j9object_t *)pool_startDo((J9Pool *)frame->references, &poolState);
		while (NULL != ref) {
			ownedMonitorIterator(currentThread, &walkState, ref, ref);
			ref = (j9object_t *)pool_nextDo(&poolState);
		}
		frame = frame->previous;
	}

	return (jint)(IDATA)(UDATA)walkState.userData3;
}

jvmtiError JNICALL
jvmtiGetCurrentThread(jvmtiEnv *env,
	jthread *thread_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
	jthread rv_thread = NULL;

	Trc_JVMTI_jvmtiGetCurrentThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_NON_NULL(thread_ptr);

		rv_thread = (jthread)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, (j9object_t)currentThread->threadObject);
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != thread_ptr) {
		*thread_ptr = rv_thread;
	}
	TRACE_JVMTI_RETURN(jvmtiGetCurrentThread);
}

#if JAVA_SPEC_VERSION >= 19
static jvmtiIterationControl
jvmtiSuspendResumeCallBack(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData)
{
	j9object_t continuationObj = object->object;
	j9object_t vthread = J9VMJDKINTERNALVMCONTINUATION_VTHREAD(vmThread, continuationObj);
	ContinuationState continuationState = J9VMJDKINTERNALVMCONTINUATION_STATE(vmThread, continuationObj);

	if ((NULL != vthread) && J9_ARE_NO_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_LAST_UNMOUNT)) {
		jvmtiVThreadCallBackData *data = (jvmtiVThreadCallBackData*)userData;
		BOOLEAN is_excepted = FALSE;
		for (jint i = 0; i < data->except_count; ++i) {
			if (vthread == J9_JNI_UNWRAP_REFERENCE(data->except_list[i])) {
				is_excepted = TRUE;
				break;
			}
		}
		if (!is_excepted) {
			J9JavaVM *vm = vmThread->javaVM;
			J9VMThread *targetThread = NULL;
			j9object_t carrierThread = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(vmThread, vthread);
			U_64 isSuspendedFlag = J9OBJECT_U64_LOAD(vmThread, vthread, vm->internalSuspendStateOffset);
			J9VMThread *carrierVMThread = VM_VMHelpers::getCarrierVMThread(vmThread, vthread);
			bool inTransition = (NULL != carrierVMThread);
			if (NULL != carrierThread) {
				targetThread = J9VMJAVALANGTHREAD_THREADREF(vmThread, carrierThread);
				Assert_JVMTI_notNull(targetThread);
			} else if (inTransition) {
				/* Virtual thread during transition. */
				targetThread = carrierVMThread;
			}
			if (data->is_suspend) {
				J9OBJECT_U64_STORE(vmThread, vthread, vm->internalSuspendStateOffset, (isSuspendedFlag | J9_VIRTUALTHREAD_INTERNAL_STATE_SUSPENDED));
				if ((NULL != targetThread) && ((vthread == targetThread->threadObject) || inTransition)) {
					if (OMR_ARE_NO_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND | J9_PUBLIC_FLAGS_STOPPED)) {
						setHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
						Trc_JVMTI_threadSuspended(targetThread);
					}
					if (inTransition) {
						/* Decrement inspector count to reflect vthread suspended in transition. */
						MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(vmThread);
						objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(vmThread, vthread, vm->virtualThreadInspectorCountOffset, (U_64)-1, (U_64)-2);
					}
				}
			} else if (VM_VMHelpers::isThreadSuspended(vmThread, vthread)) {
				/* Resume the virtual thread. */
				J9OBJECT_U64_STORE(vmThread, vthread, vm->internalSuspendStateOffset, (isSuspendedFlag & J9_VIRTUALTHREAD_INTERNAL_STATE_CARRIERID_MASK));
				if ((NULL != targetThread) && ((vthread == targetThread->threadObject) || inTransition)) {
					if (OMR_ARE_ANY_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)) {
						clearHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
						Trc_JVMTI_threadResumed(targetThread);
					}
					if (inTransition) {
						MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(vmThread);

						/* Increment inspector count by 1 to reverse the decrement done during suspend. */
						I_64 vthreadInspectorCount = J9OBJECT_I64_LOAD(vmThread, vthread, vm->virtualThreadInspectorCountOffset);
						objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(vmThread, vthread, vm->virtualThreadInspectorCountOffset, vthreadInspectorCount, vthreadInspectorCount + 1);
						Assert_JVMTI_true(vthreadInspectorCount < 0);
					}
				}
			}
		}
	}
	return JVMTI_ITERATION_CONTINUE;
}

jvmtiError JNICALL
jvmtiSuspendAllVirtualThreads(jvmtiEnv *env,
	jint except_count,
	const jthread *except_list)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiSuspendAllVirtualThreads_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		jint i = 0;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		PORT_ACCESS_FROM_JAVAVM(vm);
		jvmtiVThreadCallBackData data = {except_list, except_count, TRUE, FALSE};

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);
		ENSURE_CAPABILITY(env, can_support_virtual_threads);

		ENSURE_NON_NEGATIVE(except_count);
		if ((NULL == except_list) && (0 != except_count)) {
			JVMTI_ERROR(JVMTI_ERROR_NULL_POINTER);
		}

		for (i = 0; i < except_count; ++i) {
			jthread thread = except_list[i];
			if ((NULL == thread)
			|| !IS_JAVA_LANG_VIRTUALTHREAD(currentThread, J9_JNI_UNWRAP_REFERENCE(thread))
			) {
				JVMTI_ERROR(JVMTI_ERROR_INVALID_THREAD);
			}
		}

		/* Walk all virtual threads. */
		vmFuncs->acquireExclusiveVMAccess(currentThread);
		vm->memoryManagerFunctions->j9gc_flush_nonAllocationCaches_for_walk(vm);
		vm->memoryManagerFunctions->j9mm_iterate_all_continuation_objects(currentThread, PORTLIB, 0, jvmtiSuspendResumeCallBack, (void*)&data);
		vmFuncs->releaseExclusiveVMAccess(currentThread);

		/* If the current thread appeared in the list (and was marked as suspended), block now until the thread is resumed. */
		if (data.suspend_current_thread) {
			vmFuncs->internalExitVMToJNI(currentThread);
			setHaltFlag(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
			vmFuncs->internalEnterVMFromJNI(currentThread);
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSuspendAllVirtualThreads);
}

jvmtiError JNICALL
jvmtiResumeAllVirtualThreads(jvmtiEnv *env,
	jint except_count,
	const jthread *except_list)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	J9VMThread *currentThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiResumeAllVirtualThreads_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jint i = 0;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		PORT_ACCESS_FROM_JAVAVM(vm);
		jvmtiVThreadCallBackData data = {except_list, except_count, FALSE, FALSE};

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);
		ENSURE_CAPABILITY(env, can_support_virtual_threads);

		ENSURE_NON_NEGATIVE(except_count);
		if ((NULL == except_list) && (0 != except_count)) {
			JVMTI_ERROR(JVMTI_ERROR_NULL_POINTER);
		}

		for (i = 0; i < except_count; ++i) {
			jthread thread = except_list[i];
			if ((NULL == thread)
			|| !IS_JAVA_LANG_VIRTUALTHREAD(currentThread, J9_JNI_UNWRAP_REFERENCE(thread))
			) {
				JVMTI_ERROR(JVMTI_ERROR_INVALID_THREAD);
			}
		}

		/* Walk all virtual threads. */
		vmFuncs->acquireExclusiveVMAccess(currentThread);
		vm->memoryManagerFunctions->j9gc_flush_nonAllocationCaches_for_walk(vm);
		vm->memoryManagerFunctions->j9mm_iterate_all_continuation_objects(currentThread, PORTLIB, 0, jvmtiSuspendResumeCallBack, (void*)&data);
		vmFuncs->releaseExclusiveVMAccess(currentThread);
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiResumeAllVirtualThreads);
}
#endif /* JAVA_SPEC_VERSION >= 19 */

} /* extern "C" */
