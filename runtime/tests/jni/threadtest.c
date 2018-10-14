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
#include "jnitest_internal.h"
#include "j9protos.h"

static BOOLEAN
enterThreadLock(J9VMThread *currentThread, j9object_t threadObj);
static void
exitThreadLock(J9VMThread *currentThread, j9object_t threadObj);

jobjectArray JNICALL 
Java_j9vm_test_thread_NativeHelpers_findDeadlockedThreads(JNIEnv *env, jclass cls)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmfns = currentThread->javaVM->internalVMFunctions;
	j9object_t *deadlockedThreads = NULL;
	IDATA deadlockCount;
	jobjectArray threads = NULL;
	jclass jlThread;
	jobject *threadRef = NULL;
	jsize index;
	PORT_ACCESS_FROM_VMC(currentThread);

	jlThread = (*env)->FindClass(env, "java/lang/Thread");
	if (!jlThread) {
		return NULL;
	}
	
	vmfns->internalEnterVMFromJNI(currentThread);
	vmfns->acquireExclusiveVMAccess(currentThread);
	deadlockCount = vmfns->findObjectDeadlockedThreads(currentThread, &deadlockedThreads, NULL, J9VMTHREAD_FINDDEADLOCKFLAG_ALREADYHAVEEXCLUSIVE);
	vmfns->releaseExclusiveVMAccess(currentThread);

	if (deadlockCount <= 0) {
		vmfns->internalExitVMToJNI(currentThread);
	} else {
		threadRef = j9mem_allocate_memory(deadlockCount * sizeof(jobject), OMRMEM_CATEGORY_VM);
		if (threadRef) {
			for (index = 0; index < deadlockCount; index++) {
				threadRef[index] = vmfns->j9jni_createLocalRef(env, deadlockedThreads[index]);
			}
		}
		j9mem_free_memory(deadlockedThreads);
		vmfns->internalExitVMToJNI(currentThread);

		if (threadRef) {
			threads = (*env)->NewObjectArray(env, (jsize)deadlockCount, jlThread, NULL);
			if (threads) {
				for (index = 0; index < deadlockCount; index++) {
					(*env)->SetObjectArrayElement(env, threads, index, threadRef[index]);
				}
			}
		}
		j9mem_free_memory(threadRef);
	}

	return threads;
}

void JNICALL 
Java_j9vm_test_thread_NativeHelpers_findDeadlockedThreadsAndObjects(JNIEnv *env, jclass cls, jobject list)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmfns = currentThread->javaVM->internalVMFunctions;
	j9object_t *deadlockedThreads = NULL;
	j9object_t *blockingObjects = NULL;
	IDATA deadlockCount;
	jsize index;
	jclass jlThread, jlObject, listclass;
	jfieldID tfid, mfid;
	PORT_ACCESS_FROM_VMC(currentThread);

	jlThread = (*env)->FindClass(env, "java/lang/Thread");
	if (!jlThread) {
		return;
	}
	jlObject = (*env)->FindClass(env, "java/lang/Object");
	if (!jlObject) {
		return;
	}
	listclass = (*env)->GetObjectClass(env, list);
	if (!listclass) {
		return;
	}
	tfid = (*env)->GetFieldID(env, listclass, "threads", "[Ljava/lang/Thread;");
	if (!tfid) {
		return;
	}
	mfid = (*env)->GetFieldID(env, listclass, "monitors", "[Ljava/lang/Object;");
	if (!mfid) {
		return;
	}
	
	vmfns->internalEnterVMFromJNI(currentThread);
	deadlockCount = vmfns->findObjectDeadlockedThreads(currentThread, &deadlockedThreads, &blockingObjects, 0);

	if (deadlockCount <= 0) {
		vmfns->internalExitVMToJNI(currentThread);
	} else {
		jobject *threadRef;
		jobject *objectRef;
		jobjectArray threads = NULL;
		jobjectArray objects = NULL;
		
		threadRef = j9mem_allocate_memory(deadlockCount * sizeof(jobject), OMRMEM_CATEGORY_VM);
		objectRef = j9mem_allocate_memory(deadlockCount * sizeof(jobject), OMRMEM_CATEGORY_VM);
		if (threadRef && objectRef) {
			for (index = 0; index < deadlockCount; index++) {
				threadRef[index] = vmfns->j9jni_createLocalRef(env, deadlockedThreads[index]);
				objectRef[index] = vmfns->j9jni_createLocalRef(env, blockingObjects[index]);
			}
		}
		j9mem_free_memory(deadlockedThreads);
		j9mem_free_memory(blockingObjects);
		vmfns->internalExitVMToJNI(currentThread);

		if (threadRef && objectRef) {
			threads = (*env)->NewObjectArray(env, (jsize)deadlockCount, jlThread, NULL);
			objects = (*env)->NewObjectArray(env, (jsize)deadlockCount, jlObject, NULL);

			if (threads && objects) {
				for (index = 0; index < deadlockCount; index++) {
					(*env)->SetObjectArrayElement(env, threads, index, threadRef[index]);
					(*env)->SetObjectArrayElement(env, objects, index, objectRef[index]);
				}
				(*env)->SetObjectField(env, list, tfid, threads);
				(*env)->SetObjectField(env, list, mfid, objects);
			}
		}
		
		j9mem_free_memory(threadRef);
		j9mem_free_memory(objectRef);
	}
}

static BOOLEAN
enterThreadLock(J9VMThread *currentThread, j9object_t threadObj)
{
	J9Object *threadLock = NULL;

	if (threadObj) {
		threadLock = J9VMJAVALANGTHREAD_LOCK(currentThread, threadObj);
		if (threadLock) {
			threadLock = (J9Object *)currentThread->javaVM->internalVMFunctions->objectMonitorEnter(currentThread, threadLock);
			/* Check for objectMonitorEnter() error */
			if ((NULL == threadLock) || (0 != ((UDATA)threadLock & OBJECT_HEADER_INVALID_ADDR_BITS))) {
				return FALSE;
			}
		}
	}
	return TRUE;		
}

static void
exitThreadLock(J9VMThread *currentThread, j9object_t threadObj)
{
	J9Object *threadLock;
	
	threadLock = J9VMJAVALANGTHREAD_LOCK(currentThread, threadObj);

	currentThread->javaVM->internalVMFunctions->objectMonitorExit(currentThread, threadLock);
}
