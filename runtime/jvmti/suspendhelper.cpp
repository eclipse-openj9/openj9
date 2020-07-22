/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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
#include "VMAccess.hpp"

extern "C" {

jvmtiError
suspendThread(J9VMThread *currentThread, jthread thread, UDATA allowNull, UDATA *currentThreadSuspended)
{
	J9VMThread * targetThread;
	jvmtiError rc;

	*currentThreadSuspended = FALSE;
	rc = getVMThread(currentThread, thread, &targetThread, allowNull, TRUE);
	if (rc == JVMTI_ERROR_NONE) {
		if (targetThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND) {
			rc = JVMTI_ERROR_THREAD_SUSPENDED;
		} else {
			if (targetThread->publicFlags & J9_PUBLIC_FLAGS_STOPPED) {
				/* Do not suspend dead threads. Mirrors SUN behaviour. */
				rc = JVMTI_ERROR_THREAD_NOT_ALIVE;
			} else {
				if (currentThread == targetThread) {
					*currentThreadSuspended = TRUE;
				} else {
					J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
					/* Match Thread.suspend() synchronization by acquiring the target thread's Thread.lock before suspending.
					 * This will prevent deadlocks in the JCL library that could occur if a thread is suspended while
					 * holding Thread.lock.
					 * For example if thread1 is suspended while in the middle of running Thread.isAlive which is synchronized, 
					 * thread2 calling thread1.interrupt will hang forever since the lock belonging to thread1 will never be released.
					 */
					vmFuncs->objectMonitorEnter(currentThread, J9VMJAVALANGTHREAD_LOCK(currentThread, targetThread->threadObject));

					vmFuncs->internalExitVMToJNI(currentThread);

					omrthread_monitor_enter(targetThread->publicFlagsMutex);
					VM_VMAccess::setHaltFlagForVMAccessRelease(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
					if (VM_VMAccess::mustWaitForVMAccessRelease(targetThread)) {
						while (J9_ARE_ALL_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND | J9_PUBLIC_FLAGS_VM_ACCESS)) {
							omrthread_monitor_wait(targetThread->publicFlagsMutex);
						}
					}
					omrthread_monitor_exit(targetThread->publicFlagsMutex);

					vmFuncs->internalEnterVMFromJNI(currentThread);

					/* Release java.lang.Thread.lock */
					vmFuncs->objectMonitorExit(currentThread, J9VMJAVALANGTHREAD_LOCK(currentThread, targetThread->threadObject));
				}
				Trc_JVMTI_threadSuspended(targetThread);
			}
		}
		releaseVMThread(currentThread, targetThread);
	}

	return rc;
}

} /* extern "C" */
