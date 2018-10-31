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

jvmtiError JNICALL
jvmtiGetTopThreadGroups(jvmtiEnv* env,
	jint* group_count_ptr,
	jthreadGroup** groups_ptr)
{
	jvmtiError rc;
	jint rv_group_count = 0;
	jthreadGroup *rv_groups = NULL;

	Trc_JVMTI_jvmtiGetTopThreadGroups_Entry(env);

	if (JAVAVM_FROM_ENV(env)->jclFlags & J9_JCL_FLAG_THREADGROUPS) {
		J9JavaVM * vm = JAVAVM_FROM_ENV(env);
		J9VMThread * currentThread;
		PORT_ACCESS_FROM_JAVAVM(vm);

		rc = getCurrentVMThread(vm, &currentThread);
		if (rc == JVMTI_ERROR_NONE) {
			jthreadGroup * groups;

			vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

			ENSURE_PHASE_LIVE(env);

			ENSURE_NON_NULL(group_count_ptr);
			ENSURE_NON_NULL(groups_ptr);

			/* Currently, there is exactly one top-level ThreadGroup */

			groups = j9mem_allocate_memory(sizeof(jthreadGroup), J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (groups == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				*groups = (jthreadGroup) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, *(vm->systemThreadGroupRef));
				rv_group_count = 1;
				rv_groups = groups;
			}
done:
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);

		}
	} else {
		rv_group_count = 0;
		rv_groups = NULL;
		rc = JVMTI_ERROR_NONE;
	}

	if (NULL != group_count_ptr) {
		*group_count_ptr = rv_group_count;
	}
	if (NULL != groups_ptr) {
		*groups_ptr = rv_groups;
	}
	TRACE_JVMTI_RETURN(jvmtiGetTopThreadGroups);
}


jvmtiError JNICALL
jvmtiGetThreadGroupInfo(jvmtiEnv* env,
	jthreadGroup group,
	jvmtiThreadGroupInfo* info_ptr)
{
	jvmtiError rc = JVMTI_ERROR_INVALID_THREAD_GROUP;
	jthreadGroup rv_parent = NULL;
	char *rv_name = NULL;
	jint rv_max_priority = 0;
	jboolean rv_is_daemon = JNI_FALSE;

	if (JAVAVM_FROM_ENV(env)->jclFlags & J9_JCL_FLAG_THREADGROUPS) {
		J9JavaVM * vm = JAVAVM_FROM_ENV(env);
		J9VMThread * currentThread;

		rc = getCurrentVMThread(vm, &currentThread);
		if (rc == JVMTI_ERROR_NONE) {
			j9object_t threadGroupObject;
			j9object_t groupName;
			char* name = NULL;

			vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

			ENSURE_PHASE_LIVE(env);

			ENSURE_JTHREADGROUP_NON_NULL(group);
			ENSURE_NON_NULL(info_ptr);

			threadGroupObject = *((j9object_t*) group);

			groupName = J9VMJAVALANGTHREADGROUP_NAME(currentThread, threadGroupObject);
			name = vm->internalVMFunctions->copyStringToUTF8WithMemAlloc(currentThread, groupName, J9_STR_NULL_TERMINATE_RESULT, "", 0, NULL, 0, NULL);

			if (NULL == name) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				rv_name = name;
				rv_parent = (jthreadGroup) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t)J9VMJAVALANGTHREADGROUP_PARENT(currentThread, threadGroupObject));
				rv_max_priority = J9VMJAVALANGTHREADGROUP_MAXPRIORITY(currentThread, threadGroupObject);
				rv_is_daemon = (jboolean)J9VMJAVALANGTHREADGROUP_ISDAEMON(currentThread, threadGroupObject);
			}
done:
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
		}
	}

	if (NULL != info_ptr) {
		info_ptr->parent = rv_parent;
		info_ptr->name = rv_name;
		info_ptr->max_priority = rv_max_priority;
		info_ptr->is_daemon = rv_is_daemon;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadGroupInfo);
}


jvmtiError JNICALL
jvmtiGetThreadGroupChildren(jvmtiEnv* env,
	jthreadGroup group,
	jint* thread_count_ptr,
	jthread** threads_ptr,
	jint* group_count_ptr,
	jthreadGroup** groups_ptr)
{
	jvmtiError rc = JVMTI_ERROR_INVALID_THREAD_GROUP;
	jint rv_thread_count = 0;
	jthread *rv_threads = NULL;
	jint rv_group_count = 0;
	jthreadGroup *rv_groups = NULL;

	Trc_JVMTI_jvmtiGetThreadGroupChildren_Entry(env);

	if (JAVAVM_FROM_ENV(env)->jclFlags & J9_JCL_FLAG_THREADGROUPS) {
		J9JavaVM * vm = JAVAVM_FROM_ENV(env);
		J9VMThread * currentThread;
		PORT_ACCESS_FROM_JAVAVM(vm);

		rc = getCurrentVMThread(vm, &currentThread);
		if (rc == JVMTI_ERROR_NONE) {
			j9object_t threadGroupObject;
			j9object_t childrenThreadsLock;
			j9object_t childrenGroupsLock;
			jthreadGroup * groups;
			jint numGroups;
			jthread * threads;
			jint numThreads;

			vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

			ENSURE_PHASE_LIVE(env);

			ENSURE_JTHREADGROUP_NON_NULL(group);
			ENSURE_NON_NULL(thread_count_ptr);
			ENSURE_NON_NULL(threads_ptr);
			ENSURE_NON_NULL(group_count_ptr);
			ENSURE_NON_NULL(groups_ptr);
			
			/* Construct the Children Groups array under a lock */

 			threadGroupObject = *((j9object_t*) group);
			childrenGroupsLock = J9VMJAVALANGTHREADGROUP_CHILDRENGROUPSLOCK(currentThread, threadGroupObject);
			childrenGroupsLock = (j9object_t) currentThread->javaVM->internalVMFunctions->objectMonitorEnter(currentThread, childrenGroupsLock);
			if (childrenGroupsLock == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto done;
			}
 			threadGroupObject = *((j9object_t*) group);
 			
 			/* The threadGroupObject has to be reobtained as it might have been GC'ed while waiting for the lock */

			numGroups = J9VMJAVALANGTHREADGROUP_NUMGROUPS(currentThread, threadGroupObject);
			groups = j9mem_allocate_memory(sizeof(jthreadGroup) * numGroups, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (groups == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				jint i;
				j9object_t childrenGroups = (j9object_t) J9VMJAVALANGTHREADGROUP_CHILDRENGROUPS(currentThread, threadGroupObject);

				for (i = 0; i < numGroups; ++i) {
					j9object_t group = J9JAVAARRAYOFOBJECT_LOAD(currentThread, childrenGroups, i);

					groups[i] = (jthreadGroup) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, group);
				}
			}

            currentThread->javaVM->internalVMFunctions->objectMonitorExit(currentThread, childrenGroupsLock);

            /* Construct the Children Threads array under a lock */
			
			threadGroupObject = *((j9object_t*) group);
 			childrenThreadsLock = J9VMJAVALANGTHREADGROUP_CHILDRENTHREADSLOCK(currentThread, threadGroupObject);
			childrenThreadsLock = (j9object_t) currentThread->javaVM->internalVMFunctions->objectMonitorEnter(currentThread, childrenThreadsLock);
			if (childrenThreadsLock == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				j9mem_free_memory(groups);
				goto done;
			}
			threadGroupObject = *((j9object_t*) group);

 			/* The threadGroupObject has to be reobtained as it might have been GC'ed while waiting for the lock */
 			
			numThreads = J9VMJAVALANGTHREADGROUP_NUMTHREADS(currentThread, threadGroupObject);
			threads = j9mem_allocate_memory(sizeof(jthread) * numThreads, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (threads == NULL) {
				j9mem_free_memory(groups);
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				jint i;
				j9object_t childrenThreads = (j9object_t) J9VMJAVALANGTHREADGROUP_CHILDRENTHREADS(currentThread, threadGroupObject);
				jint numLiveThreads;

				/* Include only live threads in the result */

				numLiveThreads = 0;
				for (i = 0; i < numThreads; ++i) {
					j9object_t thread = J9JAVAARRAYOFOBJECT_LOAD(currentThread, childrenThreads, i);
					J9VMThread * targetThread;

					if (getVMThread(currentThread, (jthread) &thread, &targetThread, FALSE, TRUE) == JVMTI_ERROR_NONE) {
						threads[numLiveThreads++] = (jthread) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, thread);
						releaseVMThread(currentThread, targetThread);
					}
				}

				rv_thread_count = numLiveThreads;
				rv_threads = threads;
				rv_group_count = numGroups;
				rv_groups = groups; 
			}

			currentThread->javaVM->internalVMFunctions->objectMonitorExit(currentThread, childrenThreadsLock);

done:
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
		}
	}

	if (NULL != thread_count_ptr) {
		*thread_count_ptr = rv_thread_count;
	}
	if (NULL != threads_ptr) {
		*threads_ptr = rv_threads;
	}
	if (NULL != group_count_ptr) {
		*group_count_ptr = rv_group_count;
	}
	if (NULL != groups_ptr) {
		*groups_ptr = rv_groups;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadGroupChildren);
}



