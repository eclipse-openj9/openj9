/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
#include "VMAccess.hpp"

extern "C" {

jvmtiError
suspendThread(J9VMThread *currentThread, jthread thread, BOOLEAN allowNull, BOOLEAN *currentThreadSuspended)
{
	J9VMThread *targetThread = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
	UDATA flags = J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD;
	J9JavaVM *vm = currentThread->javaVM;

	*currentThreadSuspended = FALSE;
	if (!allowNull) {
		flags |= J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD;
	}

#if JAVA_SPEC_VERSION >= 19
	j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
	/* getVMThread can cause a deadlock for a virutal thread if the thread is already suspended.
	 * The suspended virtual thread halts during the mount or unmount phase while the
	 * virtualThreadInspectorCount is set to -1. Meanwhile, acquireVThreadInspector waits
	 * indefinitely for the virtualThreadInspectorCount to be set to 0 in the mount or unmount
	 * phase. To prevent the deadlock, jvmtiSuspendThread should return an error for already
	 * suspended threads without invoking getVMThread -> acquireVThreadInspector. This approach
	 * can be taken for both platform and virtual threads.
	 */
	if (VM_VMHelpers::isThreadSuspended(currentThread, threadObject)) {
		rc = JVMTI_ERROR_THREAD_SUSPENDED;
		goto done;
	}
#endif /* JAVA_SPEC_VERSION >= 19 */
	rc = getVMThread(currentThread, thread, &targetThread, JVMTI_ERROR_NONE, flags);
	if (rc == JVMTI_ERROR_NONE) {
#if JAVA_SPEC_VERSION >= 19
		/* Re-fetch object since VM access can be re-acquired in getVMThread. */
		threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
		/* The J9 PUBLIC FLAGS HALT THREAD JAVA SUSPEND flag will be set
		 * if the thread is mounted.
		 */
		if ((NULL != targetThread) && (threadObject == targetThread->threadObject))
#endif /* JAVA_SPEC_VERSION >= 19 */
		{
			if (OMR_ARE_ANY_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)) {
				rc = JVMTI_ERROR_THREAD_SUSPENDED;
			} else {
				if (OMR_ARE_ANY_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_STOPPED)) {
					/* Do not suspend dead threads. Mirrors SUN behaviour. */
					rc = JVMTI_ERROR_THREAD_NOT_ALIVE;
				} else {
					if (currentThread == targetThread) {
						*currentThreadSuspended = TRUE;
					} else {
						vm->internalVMFunctions->internalExitVMToJNI(currentThread);
						omrthread_monitor_enter(targetThread->publicFlagsMutex);
						VM_VMAccess::setHaltFlagForVMAccessRelease(targetThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
						if (VM_VMAccess::mustWaitForVMAccessRelease(targetThread)) {
							while (J9_ARE_ALL_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND | J9_PUBLIC_FLAGS_VM_ACCESS)) {
								omrthread_monitor_wait(targetThread->publicFlagsMutex);
							}
						}
						omrthread_monitor_exit(targetThread->publicFlagsMutex);
						vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
					}
					Trc_JVMTI_threadSuspended(targetThread);
				}
			}
		}
#if JAVA_SPEC_VERSION >= 19
		/* Re-fetch object to correctly set the field since VM access was re-acquired. */
		threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
		if (VM_VMHelpers::isThreadSuspended(currentThread, threadObject)) {
			rc = JVMTI_ERROR_THREAD_SUSPENDED;
		} else {
			U_64 internalSuspendState = J9OBJECT_U64_LOAD(currentThread, threadObject, vm->internalSuspendStateOffset);
			J9OBJECT_U64_STORE(currentThread, threadObject, vm->internalSuspendStateOffset, (internalSuspendState | J9_VIRTUALTHREAD_INTERNAL_STATE_SUSPENDED));
		}
#endif /* JAVA_SPEC_VERSION >= 19 */
		releaseVMThread(currentThread, targetThread, thread);
	}
#if JAVA_SPEC_VERSION >= 19
done:
#endif /* JAVA_SPEC_VERSION >= 19 */
	return rc;
}

} /* extern "C" */
