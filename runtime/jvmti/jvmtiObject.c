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

typedef struct J9JVMTIMonitorStats {
	J9JavaVM * vm;
	J9VMThread * currentThread;
	j9object_t lockObject;
	UDATA numWaiting;
	UDATA waitingCntr;
	UDATA numBlocked;
	UDATA blockedCntr;
	jthread * waiting;
	jthread * blocked;
} J9JVMTIMonitorStats;

static void findMonitorThreads(J9VMThread * vmThread, J9JVMTIMonitorStats * pStats);

jvmtiError JNICALL
jvmtiGetObjectSize(jvmtiEnv* env,
	jobject object,
	jlong* size_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jlong rv_size = 0;

	Trc_JVMTI_jvmtiGetObjectSize2_Entry(env, object, size_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(size_ptr);

		rv_size = getObjectSize(vm, *(j9object_t*)object);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != size_ptr) {
		*size_ptr = rv_size;
	}
	TRACE_ONE_JVMTI_RETURN(jvmtiGetObjectSize2, rv_size);
}


jvmtiError JNICALL
jvmtiGetObjectHashCode(jvmtiEnv* env,
	jobject object,
	jint* hash_code_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jint rv_hash_code = 0;

	Trc_JVMTI_jvmtiGetObjectHashCode2_Entry(env, object, hash_code_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		j9object_t obj;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(hash_code_ptr);

		obj = *((j9object_t*) object);
		rv_hash_code = (jint)objectHashCode(vm, obj);
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != hash_code_ptr) {
		*hash_code_ptr = rv_hash_code;
	}
	TRACE_ONE_JVMTI_RETURN(jvmtiGetObjectHashCode2, rv_hash_code);
}


jvmtiError JNICALL
jvmtiGetObjectMonitorUsage(jvmtiEnv* env,
	jobject object,
	jvmtiMonitorUsage* info_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jthread rv_owner = NULL;
	jint rv_entry_count = 0;
	jint rv_waiter_count = 0;
	jthread *rv_waiters = NULL;
	jint rv_notify_waiter_count = 0;
	jthread *rv_notify_waiters = NULL;

	Trc_JVMTI_jvmtiGetObjectMonitorUsage2_Entry(env, object, info_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * owner;
		J9VMThread * walkThread;
		UDATA count = 0;
		J9JVMTIMonitorStats stats;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_monitor_info);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(info_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		/*
		 * getObjectMonitorOwner requires a J9VMThread solely for multi-tenant support,
		 *  however, the multi-tenant JVM does not support can_get_monitor_info (see ENSURE_CAPABILITY check above)
		 *  and therefore will not use jvmtiGetObjectMonitorUsage: so just pass in NULL as the vm thread
		 */
		owner = getObjectMonitorOwner(vm, NULL, *((j9object_t*) object), &count);
		memset(info_ptr, 0, sizeof(jvmtiMonitorUsage));

		if (owner && owner->threadObject) {
			j9object_t target = (j9object_t)owner->threadObject;
			rv_owner = (jthread) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, target);
			rv_entry_count = (jint)count;
		}

		memset(&stats, 0, sizeof(J9JVMTIMonitorStats));

		stats.vm = vm;
		stats.currentThread = currentThread;
		stats.lockObject = *((j9object_t*) object);

		/* Search for blocked/waiting threads (wind up counts) */
		walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
		while (walkThread != NULL) {
			findMonitorThreads(walkThread, &stats);
			walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
		}

		stats.waiting = j9mem_allocate_memory(sizeof(jthread) * stats.numWaiting, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (stats.waiting == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			stats.blocked = j9mem_allocate_memory(sizeof(jthread) * stats.numBlocked, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (stats.blocked == NULL) {
				j9mem_free_memory(stats.waiting);
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				rv_notify_waiters = stats.waiting;
				rv_waiters = stats.blocked;

				/* Record blocked/waiting threads (wind down counts) */
				walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
				while (walkThread != NULL) {
					findMonitorThreads(walkThread, &stats);
					walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
				}

				/* adjust count after filling the output buffer */
				rv_notify_waiter_count = (jint)stats.waitingCntr;
				rv_waiter_count = (jint)stats.blockedCntr;
			}
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != info_ptr) {
		info_ptr->owner = rv_owner;
		info_ptr->entry_count = rv_entry_count;
		info_ptr->waiter_count = rv_waiter_count;
		info_ptr->waiters = rv_waiters;
		info_ptr->notify_waiter_count = rv_notify_waiter_count;
		info_ptr->notify_waiters = rv_notify_waiters;
	}
	TRACE_FOUR_JVMTI_RETURN(jvmtiGetObjectMonitorUsage2, rv_owner, rv_entry_count, rv_notify_waiter_count, rv_waiter_count);
}


static void
findMonitorThreads(J9VMThread * vmThread, J9JVMTIMonitorStats * pStats)
{
	j9object_t lockObject;

	UDATA threadState = getVMThreadObjectStatesAll(vmThread, &lockObject, NULL, NULL);

	/* Stopped on the same monitor object? */
	if (lockObject == pStats->lockObject) {

		j9object_t target = (j9object_t)vmThread->threadObject;

		if (target) {

			J9JavaVM * vm = pStats->vm;
			JNIEnv * jniEnv = (JNIEnv *)pStats->currentThread;

			/* CMVC 87023 - the spec is unclear, but from experimentation it appears that 'waiting to be notified' threads
			 * should appear in both lists
			 */
			threadState &= ~(J9VMTHREAD_STATE_SUSPENDED | J9VMTHREAD_STATE_INTERRUPTED);
			switch (threadState) {
				case J9VMTHREAD_STATE_WAITING:
				case J9VMTHREAD_STATE_WAITING_TIMED:
					if (pStats->waiting == NULL) {
						(pStats->numWaiting)++;
					} else {
						if (pStats->waitingCntr < pStats->numWaiting) {
							pStats->waiting[pStats->waitingCntr] =
									(jthread) vm->internalVMFunctions->j9jni_createLocalRef(jniEnv, target);
							++(pStats->waitingCntr);
						}
					}
					/* FALL THROUGH */

				case J9VMTHREAD_STATE_BLOCKED:
					if (pStats->blocked == NULL) {
						(pStats->numBlocked)++;
					} else {
						if (pStats->blockedCntr < pStats->numBlocked) {
							pStats->blocked[pStats->blockedCntr] =
									(jthread) vm->internalVMFunctions->j9jni_createLocalRef(jniEnv, target);
							++(pStats->blockedCntr);
						}
					}
					break;

				default:
					break;
			}
		}
	}
}


