/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"

typedef struct J9JVMTIRunAgentThreadArgs {
	jvmtiEnv * jvmti_env;
	jvmtiStartFunction proc;
	const void * arg;
} J9JVMTIRunAgentThreadArgs;

static int J9THREAD_PROC agentThreadStart (void *entryArg);
static jint walkLocalMonitorRefs (J9VMThread* currentThread, jobject* locks, J9VMThread* targetThread, UDATA maxCount);
static jvmtiError resumeThread (J9VMThread * currentThread, jthread thread);
static UDATA wrappedAgentThreadStart (J9PortLibrary* portLib, void * entryArg);
static void ownedMonitorIterator (J9VMThread* aThread, J9StackWalkState* walkState, j9object_t* slot, const void * stackLocation);


jvmtiError JNICALL
jvmtiGetThreadState(jvmtiEnv* env,
	jthread thread,
	jint* thread_state_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jint rv_thread_state = 0;

	Trc_JVMTI_jvmtiGetThreadState_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;
		j9object_t threadObject = NULL;
		j9object_t threadObjectLock = NULL;
		jboolean   threadStartedFlag;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(thread_state_ptr);

		/* if the thread is NULL then use the current thread */
		if (thread == NULL) {
			threadObject = currentThread->threadObject;
		} else {
			threadObject = *(j9object_t *)thread;
		}

		/* get the lock for the object */
		threadObjectLock = J9VMJAVALANGTHREAD_LOCK(currentThread,threadObject);

		/* now get the vmThread for the object and whether the thread has been started, we get
		 * these under the thread object lock
		 * so that we get a consistent view of the two values */
		if (threadObjectLock != NULL){
			rc = getVMThread(currentThread, thread, &targetThread, TRUE, FALSE);
			threadStartedFlag = (jboolean) J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject);
		} else {
			/* in this case we must be early in the thread creation.  We still need to call getVMThread so that
			 * the inspection count etc. are handled correctly, however, we just want to fall into the case were
			 * we return that the thread is NEW so we set the targetThread and threadStartedFlag to false.
			 */
			rc = getVMThread(currentThread, thread, &targetThread, TRUE, FALSE);
			targetThread = NULL;
			threadStartedFlag = JNI_FALSE;
		}

		if (rc == JVMTI_ERROR_NONE) {
			/* we use the values of targetThread and threadStartedFlag that were obtained while holding the lock on the
			 * thread so that get a consistent view of the values.  This is needed because if we don't get a consistent view
			 * we may think the thread is TERMINATED instead of just starting.
			 */
			if (targetThread == NULL) {
				/* If the thread is new, and we were not able to get and suspend its vmthread, 
				 * we must not call getThreadState(). getThreadState() refetches the vmthread.
				 * If the second fetch succeeded, we would erroneously attempt to inspect the
				 * unsuspended thread.
				 */
				if (threadStartedFlag) {
					rv_thread_state = JVMTI_THREAD_STATE_TERMINATED;
				} else {
					/* thread is new */
					rv_thread_state = 0;
				}
			} else {
				vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
				/* The vmthread can't be recycled because getVMThread() prevented it from exiting. */
				rv_thread_state = getThreadState(currentThread, targetThread->threadObject);
				vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			}
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != thread_state_ptr) {
		*thread_state_ptr = rv_thread_state; 
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadState);
}


jvmtiError JNICALL
jvmtiGetAllThreads(jvmtiEnv* env,
	jint* threads_count_ptr,
	jthread** threads_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_threads_count = 0;
	jthread *rv_threads = NULL;

	Trc_JVMTI_jvmtiGetAllThreads_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jthread * threads;
			
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);					

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(threads_count_ptr);
		ENSURE_NON_NULL(threads_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		threads = j9mem_allocate_memory(sizeof(jthread) * vm->totalThreadCount, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (threads == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jthread * currentThreadPtr = threads;
			J9VMThread * targetThread;
			jint threadCount = 0;

			targetThread = vm->mainThread;
			do {
				j9object_t threadObject = targetThread->threadObject;

				/* Only count live threads */

				if (threadObject != NULL) {
					if (J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject) && (J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject) != NULL)) {
						*currentThreadPtr++ = (jthread) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t) threadObject);
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
jvmtiSuspendThread(jvmtiEnv* env,
	jthread thread)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSuspendThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		UDATA currentThreadSuspended;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);

		rc = suspendThread(currentThread, thread, TRUE, &currentThreadSuspended);

		/* If the current thread was suspended, block now until it is resumed */

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
jvmtiSuspendThreadList(jvmtiEnv* env,
	jint request_count,
	const jthread* request_list,
	jvmtiError* results)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSuspendThreadList_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jint i;
		UDATA currentThreadEverSuspended = FALSE;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_suspend);

		ENSURE_NON_NEGATIVE(request_count);
		ENSURE_NON_NULL(request_list);
		ENSURE_NON_NULL(results);

		for (i = 0; i < request_count; ++i) {
			UDATA currentThreadSuspended;

			results[i] = suspendThread(currentThread, request_list[i], FALSE, &currentThreadSuspended);
			currentThreadEverSuspended |= currentThreadSuspended;
		}

		/* If the current thread appeared in the list (and was marked as suspended), block now until the thread is resumed */

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
jvmtiResumeThread(jvmtiEnv* env,
	jthread thread)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiResumeThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
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
jvmtiResumeThreadList(jvmtiEnv* env,
	jint request_count,
	const jthread* request_list,
	jvmtiError* results)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiResumeThreadList_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jint i;
		
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
jvmtiStopThread(jvmtiEnv* env,
	jthread thread,
	jobject exception)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiStopThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_signal_thread);

		ENSURE_JOBJECT_NON_NULL(exception);

		rc = getVMThread(currentThread, thread, &targetThread, FALSE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			omrthread_monitor_enter(targetThread->publicFlagsMutex);
			if (!(targetThread->publicFlags & J9_PUBLIC_FLAGS_STOPPED)) {
				omrthread_priority_interrupt(targetThread->osThread);
				targetThread->stopThrowable = *((j9object_t*) exception);
				clearHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
				setHaltFlag(targetThread, J9_PUBLIC_FLAGS_STOP);
			}
			omrthread_monitor_exit(targetThread->publicFlagsMutex);
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiStopThread);
}


jvmtiError JNICALL
jvmtiInterruptThread(jvmtiEnv* env,
	jthread thread)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiInterruptThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		
		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_signal_thread);

		rc = getVMThread(currentThread, thread, &targetThread, FALSE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			omrthread_interrupt(targetThread->osThread);
#ifdef J9VM_OPT_SIDECAR
			if (vm->sidecarInterruptFunction != NULL) {
				vm->sidecarInterruptFunction(targetThread);
			}
#endif
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiInterruptThread);
}


jvmtiError JNICALL
jvmtiGetThreadInfo(jvmtiEnv* env,
	jthread thread,
	jvmtiThreadInfo* info_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	char *rv_name = NULL;
	jint rv_priority = 0;
	jboolean rv_is_daemon = JNI_FALSE;
	jthreadGroup rv_thread_group = NULL;
	jobject rv_context_class_loader = NULL;

	Trc_JVMTI_jvmtiGetThreadInfo_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(info_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, FALSE);
		if (rc == JVMTI_ERROR_NONE) {
			char* name = NULL;
			j9object_t threadObject = (NULL == thread) ? targetThread->threadObject : *((j9object_t*) thread);
			jobject threadGroup = NULL;
			jobject contextClassLoader;

			if (NULL == targetThread) {
				/* java.lang.thread may not have a ref to vm thread. for example, after it gets terminated.
				 * but we still want to return the name, so we retrieve it from the thread object itself. */
				j9object_t threadName = J9VMJAVALANGTHREAD_NAME(currentThread, threadObject);

				if (NULL == threadName) {
					name = j9mem_allocate_memory(1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
					name[0] = '\0';
				} else {
					name = vm->internalVMFunctions->copyStringToUTF8WithMemAlloc(currentThread, threadName, J9_STR_NULL_TERMINATE_RESULT, "", 0, NULL, 0, NULL);

					if (NULL == name) {
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
						goto done;
					}
				}
			} else {
				char * threadName = getOMRVMThreadName(targetThread->omrVMThread);
				size_t threadNameLen = (threadName == NULL) ? 1 : strlen(threadName) + 1;

				/* Be sure to allocate at least one byte for the nul termination */

				name = j9mem_allocate_memory(threadNameLen, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (NULL == name) {
					/* failed to allocate memory, so release VMTthread and exit */
					releaseOMRVMThreadName(targetThread->omrVMThread);
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					goto done;
				}

				if (NULL == threadName) {
					name[0] = '\0';
				} else {
					memcpy(name, threadName, threadNameLen);
				}

				releaseOMRVMThreadName(targetThread->omrVMThread);

				if (JAVAVM_FROM_ENV(env)->jclFlags & J9_JCL_FLAG_THREADGROUPS) {
					threadGroup = vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t)J9VMJAVALANGTHREAD_GROUP(currentThread, threadObject));
				}
			}

			contextClassLoader = vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, J9VMJAVALANGTHREAD_CONTEXTCLASSLOADER(currentThread, threadObject));

			rv_name = name;
			{
				rv_priority = J9VMJAVALANGTHREAD_PRIORITY(currentThread, threadObject);
			}

			rv_is_daemon = J9VMJAVALANGTHREAD_ISDAEMON(currentThread, threadObject) ? JNI_TRUE : JNI_FALSE;
			rv_thread_group = (jthreadGroup) threadGroup;
			rv_context_class_loader = contextClassLoader;
		}
done:
		releaseVMThread(currentThread, targetThread);
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
jvmtiGetOwnedMonitorInfo(jvmtiEnv* env,
	jthread thread,
	jint* owned_monitor_count_ptr,
	jobject** owned_monitors_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_owned_monitor_count = 0;
	jobject *rv_owned_monitors = NULL;

	Trc_JVMTI_jvmtiGetOwnedMonitorInfo_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_owned_monitor_info);

		ENSURE_NON_NULL(owned_monitor_count_ptr);
		ENSURE_NON_NULL(owned_monitors_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			jobject * locks;
			jint count = 0;

			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);

			count = walkLocalMonitorRefs(currentThread, NULL, targetThread, UDATA_MAX);

			locks = j9mem_allocate_memory(sizeof(jobject) * count, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (locks == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else if (0 != count) {
				/* Refetch count as it may decrease due to uniquifying the object list.
				 * Only fill up 'locks' with number of monitors recorded in the first pass.
				 * Suppress any newly acquired monitors in between.
				 */
				count = walkLocalMonitorRefs(currentThread, locks, targetThread, count);
			}
			rv_owned_monitors = locks;
			rv_owned_monitor_count = count;

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			releaseVMThread(currentThread, targetThread);
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
jvmtiGetOwnedMonitorStackDepthInfo(jvmtiEnv* env,
	jthread thread,
	jint* monitor_info_count_ptr,
	jvmtiMonitorStackDepthInfo** monitor_info_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_monitor_info_count = 0;
	jvmtiMonitorStackDepthInfo *rv_monitor_info = NULL;

	Trc_JVMTI_jvmtiGetOwnedMonitorStackDepthInfo_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_owned_monitor_stack_depth_info);

		ENSURE_NON_NULL(monitor_info_count_ptr);
		ENSURE_NON_NULL(monitor_info_ptr);
		ENSURE_JTHREAD_NON_NULL(thread);

		if (thread) {
			ENSURE_JTHREAD(currentThread, thread);
		}
		
		rv_monitor_info_count = 0;
		
		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			IDATA maxRecords;
			J9ObjectMonitorInfo * monitorEnterRecords = NULL;
			jvmtiMonitorStackDepthInfo * resultArray = NULL;

			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);

			/* Get the count of owned monitors */

			maxRecords = vm->internalVMFunctions->getOwnedObjectMonitors(currentThread, targetThread, NULL, 0);
			if (maxRecords < 0) {
				rc = JVMTI_ERROR_INTERNAL;
				goto doneRelease;
			}

			/* Do we have any enter records at all? */

			if (maxRecords == 0) {
				rc = JVMTI_ERROR_NONE;
				goto doneRelease;
			}

			monitorEnterRecords = (J9ObjectMonitorInfo *) j9mem_allocate_memory(sizeof(J9ObjectMonitorInfo) * (UDATA)maxRecords, J9MEM_CATEGORY_JVMTI);
			if (NULL == monitorEnterRecords) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto doneRelease;
			}

			maxRecords = 
				vm->internalVMFunctions->getOwnedObjectMonitors(currentThread, targetThread, monitorEnterRecords, maxRecords);
			if (maxRecords < 0) {
				rc = JVMTI_ERROR_INTERNAL;
				goto doneRelease;
			}

			/* Do we have any records at all? */

			if (maxRecords > 0) {
				resultArray = j9mem_allocate_memory((jlong) maxRecords * sizeof(jvmtiMonitorStackDepthInfo), J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (resultArray == NULL) {
					maxRecords = 0;
					resultArray = NULL;
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
				} else {
					IDATA i;

					for (i = 0; i < maxRecords; i++) {
						/* Negative stack depth indicates inability to retrieve it */
						if (monitorEnterRecords[i].depth > 0) {
							/* Subtract one since jvmti indexes stack depth starting at 0 instead of 1 */							 
							resultArray[i].stack_depth = (jint)monitorEnterRecords[i].depth - 1;
						} else {
							resultArray[i].stack_depth = (jint)monitorEnterRecords[i].depth;
						}
						resultArray[i].monitor =
							currentThread->javaVM->internalVMFunctions->j9jni_createLocalRef(
											(JNIEnv *) currentThread, monitorEnterRecords[i].object);
					}
				}
			}

			rv_monitor_info_count = (jint)maxRecords;
			rv_monitor_info = resultArray; 

doneRelease:
			if (monitorEnterRecords) {
				j9mem_free_memory(monitorEnterRecords);
			}

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			releaseVMThread(currentThread, targetThread);
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
jvmtiGetCurrentContendedMonitor(jvmtiEnv* env,
	jthread thread,
	jobject* monitor_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jobject rv_monitor = NULL;

	Trc_JVMTI_jvmtiGetCurrentContendedMonitor_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_current_contended_monitor);

		ENSURE_NON_NULL(monitor_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			j9object_t lockObject;
			UDATA vmstate;
			
			/* CMVC 184481 - The targetThread should be suspended while we attempt to get its state */
			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
			vmstate = getVMThreadObjectStatesAll(targetThread, &lockObject, NULL, NULL);

			if (lockObject && (0 == (vmstate & (J9VMTHREAD_STATE_PARKED | J9VMTHREAD_STATE_PARKED_TIMED)))) { 
				rv_monitor = (jobject) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, lockObject);
			} else {
				rv_monitor = NULL;
			}

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			releaseVMThread(currentThread, targetThread);
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
jvmtiRunAgentThread(jvmtiEnv* env,
	jthread thread,
	jvmtiStartFunction proc,
	const void* arg,
	jint priority)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiRunAgentThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIRunAgentThreadArgs * args;
		PORT_ACCESS_FROM_JAVAVM(vm);

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_JTHREAD_NON_NULL(thread);
		/* Perhaps verify that thread has not already been started? */
		ENSURE_NON_NULL(proc);
		if ((priority < JVMTI_THREAD_MIN_PRIORITY) || (priority > JVMTI_THREAD_MAX_PRIORITY)) {
			JVMTI_ERROR(JVMTI_ERROR_INVALID_PRIORITY);
		}

		/* Create entry args for the thread proc */

		args = j9mem_allocate_memory(sizeof(J9JVMTIRunAgentThreadArgs), J9MEM_CATEGORY_JVMTI);
		if (args == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			j9object_t threadObject = *((j9object_t*) thread);
			UDATA result;

			args->jvmti_env = env;
			args->proc = (jvmtiStartFunction) J9_COMPATIBLE_FUNCTION_POINTER( proc );
			args->arg = arg;

			/* Run the thread */

			J9VMJAVALANGTHREAD_SET_PRIORITY(currentThread, threadObject, priority);
			J9VMJAVALANGTHREAD_SET_ISDAEMON(currentThread, threadObject, TRUE);
			result = vm->internalVMFunctions->startJavaThread(currentThread, threadObject,
				J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_EXCEPTION_IN_START_JAVA_THREAD,
				vm->defaultOSStackSize, (UDATA) priority, agentThreadStart, args, NULL);
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
jvmtiSetThreadLocalStorage(jvmtiEnv* env,
	jthread thread,
	const void* data)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetThreadLocalStorage_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			THREAD_DATA_FOR_VMTHREAD((J9JVMTIEnv *) env, targetThread)->tls = (void *) data;
			releaseVMThread(currentThread, targetThread);
		}

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSetThreadLocalStorage);
}


jvmtiError JNICALL
jvmtiGetThreadLocalStorage(jvmtiEnv* env,
	jthread thread,
	void** data_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;
	void *rv_data = NULL;

	Trc_JVMTI_jvmtiGetThreadLocalStorage_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_NON_NULL(data_ptr);

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			rv_data = THREAD_DATA_FOR_VMTHREAD((J9JVMTIEnv *) env, targetThread)->tls;
			releaseVMThread(currentThread, targetThread);
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
resumeThread(J9VMThread * currentThread, jthread thread)
{
	J9VMThread * targetThread;
	jvmtiError rc;

	rc = getVMThread(currentThread, thread, &targetThread, FALSE, TRUE);
	if (rc == JVMTI_ERROR_NONE) {
		if (targetThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND) {
			clearHaltFlag(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
			Trc_JVMTI_threadResumed(targetThread);
		} else {
			rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
		}
		releaseVMThread(currentThread, targetThread);
	}

	return rc;
}


static int J9THREAD_PROC
agentThreadStart(void *entryArg)
{
	UDATA result;
	J9JVMTIRunAgentThreadArgs * args = entryArg;
	J9JavaVM * vm = JAVAVM_FROM_ENV((J9JVMTIEnv *) args->jvmti_env);
	J9VMThread* vmThread = vm->internalVMFunctions->currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	vmThread->gpProtected = 1;

	j9sig_protect(wrappedAgentThreadStart, args, 
		vmThread->javaVM->internalVMFunctions->structuredSignalHandler, vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&result);

	vm->internalVMFunctions->exitJavaThread(vm);
	/* Execution never reaches this point */
	return 0;
}


static UDATA
wrappedAgentThreadStart(J9PortLibrary* portLib, void * entryArg)
{
	J9JVMTIRunAgentThreadArgs * args = entryArg;
	J9JavaVM * vm = JAVAVM_FROM_ENV((J9JVMTIEnv *) args->jvmti_env);
	J9VMThread* vmThread = vm->internalVMFunctions->currentVMThread(vm);
	jvmtiEnv * jvmti_env = args->jvmti_env;
	jvmtiStartFunction proc = args->proc;
	const void * arg = args->arg;
	UDATA osStackFree;
	PORT_ACCESS_FROM_PORT(portLib);

	j9mem_free_memory(args);

	/* Determine the thread's free OS stack size (the default value has already been set if the query fails) */

	osStackFree = omrthread_current_stack_free();
	if (osStackFree != 0) {
		vmThread->currentOSStackFree = osStackFree - (osStackFree / J9VMTHREAD_RESERVED_C_STACK_FRACTION);
	}

	vm->internalVMFunctions->threadAboutToStart(vmThread);

	TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, vmThread, vmThread);

	/* Run the agent thread */

	proc(jvmti_env, (JNIEnv *) vmThread, (void *) arg);

	/* The following is not quite correct - may need a new destructor */

	vmThread->currentException = NULL;
	vm->internalVMFunctions->threadCleanup(vmThread, TRUE);

	/* Exit the OS thread */

	return 0;
}

static void
ownedMonitorIterator(J9VMThread* targetThread, J9StackWalkState* walkState, j9object_t* slot, const void * stackLocation)
{
	J9VMThread * currentThread = (J9VMThread *) walkState->userData1;
	J9JavaVM * vm = currentThread->javaVM;
	jobject * locks = (jobject *) walkState->userData2;
	UDATA count = (UDATA) walkState->userData3;
	j9object_t obj = slot ? *slot : NULL;

	if (count >= (UDATA)walkState->userData4) {
		/* PMR 69633,001,866 / CMVC 197115 
		 * There is a race condition that target thread actually acquires monitor(s)
		 * between first and second pass (monitor enter is done outside of vm access).
		 * userData4 is initialized to UDATA_MAX
		 * 	1) at first pass, count will never be bigger than UDATA_MAX
		 * 	2) at second pass, userData4 now holds the 'count' value obtained from first pass
		 */
		return;
	}

	if (obj && getObjectMonitorOwner(vm, targetThread, obj, NULL) == walkState->walkThread
			&& !isObjectStackAllocated(walkState->walkThread, obj)) {
		if (NULL != locks) {
			UDATA i;

			/* Make sure each object only appears once in the list - n^2, but we expect n to be small */
			for (i = 0; i < count; ++i) {
				if (*((j9object_t*) (locks[i])) == obj) {
					return;
				}
			}

			locks[count] = (jobject)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, obj);
		}
		walkState->userData3 = (void *) (count + 1);
	}
}

static jint
walkLocalMonitorRefs(J9VMThread* currentThread, jobject* locks, J9VMThread* targetThread, UDATA maxCount)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9StackWalkState walkState;
	J9JNIReferenceFrame * frame;
	pool_state poolState;
	j9object_t* ref;

	walkState.userData1 = currentThread;					/* self */
	walkState.userData2 = locks;							/* array */
	walkState.userData3 = (void *)0;						/* count */
	walkState.userData4 = (void *)maxCount;					/* Max count */
	walkState.objectSlotWalkFunction = ownedMonitorIterator;
	walkState.walkThread = targetThread;
	walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS;
	walkState.skipCount = 0;

	/* Check the Java stack */
	vm->walkStackFrames(currentThread, &walkState);

	frame = (J9JNIReferenceFrame *)targetThread->jniLocalReferences;

	/* Check the local JNI refs */
	while (frame != NULL) {
		ref = pool_startDo(frame->references, &poolState);

		while (ref != NULL) {
			ownedMonitorIterator(targetThread, &walkState, ref, ref);
			ref = pool_nextDo(&poolState);
		}

		frame = frame->previous;
	}

	return (jint) (UDATA) walkState.userData3;
}

jvmtiError JNICALL
jvmtiGetCurrentThread(jvmtiEnv* env,
	jthread* thread_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;
	jthread rv_thread = NULL;

	Trc_JVMTI_jvmtiGetCurrentThread_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_NON_NULL(thread_ptr);

		rv_thread = (jthread) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t) currentThread->threadObject);
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != thread_ptr) {
		*thread_ptr = rv_thread;
	}
	TRACE_JVMTI_RETURN(jvmtiGetCurrentThread);
}



