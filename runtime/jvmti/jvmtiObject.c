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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"

typedef struct J9JVMTIMonitorStats {
	J9JavaVM *vm;
	J9VMThread *currentThread;
	j9object_t lockObject;
	UDATA numWaiting;
	UDATA waitingCntr;
	UDATA numBlocked;
	UDATA blockedCntr;
	jthread *waiting;
	jthread *blocked;
} J9JVMTIMonitorStats;

static void findMonitorThreads(J9VMThread *vmThread, J9JVMTIMonitorStats *pStats);

jvmtiError JNICALL
jvmtiGetObjectSize(jvmtiEnv *env,
	jobject object,
	jlong *size_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jlong rv_size = 0;

	Trc_JVMTI_jvmtiGetObjectSize2_Entry(env, object, size_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
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
jvmtiGetObjectHashCode(jvmtiEnv *env,
	jobject object,
	jint *hash_code_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jint rv_hash_code = 0;

	Trc_JVMTI_jvmtiGetObjectHashCode2_Entry(env, object, hash_code_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		j9object_t obj = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(hash_code_ptr);

		obj = *((j9object_t *)object);
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
jvmtiGetObjectMonitorUsage(jvmtiEnv *env,
	jobject object,
	jvmtiMonitorUsage *info_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jthread rv_owner = NULL;
	jint rv_entry_count = 0;
	jint rv_waiter_count = 0;
	jthread *rv_waiters = NULL;
	jint rv_notify_waiter_count = 0;
	jthread *rv_notify_waiters = NULL;

	Trc_JVMTI_jvmtiGetObjectMonitorUsage2_Entry(env, object, info_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *owner = NULL;
		J9VMThread *walkThread = NULL;
		UDATA count = 0;
		J9JVMTIMonitorStats stats = {0};

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_monitor_info);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(info_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		owner = getObjectMonitorOwner(vm, *((j9object_t *)object), &count);
		memset(info_ptr, 0, sizeof(jvmtiMonitorUsage));

		if ((NULL != owner) && (NULL != owner->threadObject)
#if JAVA_SPEC_VERSION >= 23
		&& !IS_JAVA_LANG_VIRTUALTHREAD(currentThread, owner->threadObject)
#endif /* JAVA_SPEC_VERSION >= 23 */
		) {
			j9object_t target = (j9object_t)owner->threadObject;
			rv_owner = (jthread)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, target);
			rv_entry_count = (jint)count;
		}

		memset(&stats, 0, sizeof(J9JVMTIMonitorStats));

		stats.vm = vm;
		stats.currentThread = currentThread;
		stats.lockObject = *((j9object_t *)object);

		/* Search for blocked/waiting threads (wind up counts) */
		walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
		while (NULL != walkThread) {
			findMonitorThreads(walkThread, &stats);
			walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
		}

		stats.waiting = j9mem_allocate_memory(sizeof(jthread) * stats.numWaiting, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (NULL == stats.waiting) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			stats.blocked = j9mem_allocate_memory(sizeof(jthread) * stats.numBlocked, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (NULL == stats.blocked) {
				j9mem_free_memory(stats.waiting);
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				rv_notify_waiters = stats.waiting;
				rv_waiters = stats.blocked;

				/* Record blocked/waiting threads (wind down counts) */
				walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
				while (NULL != walkThread) {
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
findMonitorThreads(J9VMThread *vmThread, J9JVMTIMonitorStats *pStats)
{
	j9object_t lockObject = NULL;
	J9VMThread *currentThread = (J9VMThread *)pStats->currentThread;
	j9object_t target = (j9object_t)vmThread->threadObject;
	UDATA threadState = 0;

	if (NULL == target) {
		return;
	}

#if JAVA_SPEC_VERSION >= 23
	if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, target)) {
		return;
	}
#endif /* JAVA_SPEC_VERSION >= 23 */

	threadState = getVMThreadObjectStatesAll(vmThread, &lockObject, NULL, NULL);

	/* Stopped on the same monitor object? */
	if (lockObject == pStats->lockObject) {
		J9JavaVM *vm = pStats->vm;
		JNIEnv *jniEnv = (JNIEnv *)currentThread;

		threadState &= ~(J9VMTHREAD_STATE_SUSPENDED | J9VMTHREAD_STATE_INTERRUPTED);
		switch (threadState) {
		case J9VMTHREAD_STATE_WAITING:
		case J9VMTHREAD_STATE_WAITING_TIMED:
			if (NULL == pStats->waiting) {
				pStats->numWaiting += 1;
			} else {
				if (pStats->waitingCntr < pStats->numWaiting) {
					pStats->waiting[pStats->waitingCntr] =
							(jthread)vm->internalVMFunctions->j9jni_createLocalRef(jniEnv, target);
					pStats->waitingCntr += 1;
				}
			}

			/* CMVC 87023 - the spec is unclear, but from experimentation it appears that 'waiting to be notified' threads
			 * should appear in both lists.
			 *
			 * In JDK23+, these threads no longer appear in both lists. This behaviour will incrementally be backported to
			 * JDK8/11/17/21 as the RI backports the fix and dependent test changes in their codebase. Backporting this
			 * behaviour to older JDK versions before the RI will break existing third party tests.
			 */
#if JAVA_SPEC_VERSION >= 23
			break;
#else /* JAVA_SPEC_VERSION >= 23 */
			/* FALL THROUGH */
#endif /* JAVA_SPEC_VERSION >= 23 */
		case J9VMTHREAD_STATE_BLOCKED:
			if (NULL == pStats->blocked) {
				pStats->numBlocked += 1;
			} else {
				if (pStats->blockedCntr < pStats->numBlocked) {
					pStats->blocked[pStats->blockedCntr] =
							(jthread)vm->internalVMFunctions->j9jni_createLocalRef(jniEnv, target);
					pStats->blockedCntr += 1;
				}
			}
			break;

		default:
			break;
		}
	}
}
