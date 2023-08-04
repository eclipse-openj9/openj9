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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "j9consts.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "omrthread.h"
#include "ut_j9vm.h"

#include "VMHelpers.hpp"

#include <string.h>

extern "C" {

void
threadParkImpl(J9VMThread *vmThread, BOOLEAN timeoutIsEpochRelative, I_64 timeout)
{
	const I_32 oneMillion = 1000000;
	I_64 millis = 0;
	I_32 nanos = 0;
	IDATA rc = 0;
	UDATA thrstate = J9_PUBLIC_FLAGS_THREAD_PARKED;
	J9JavaVM *vm = vmThread->javaVM;

	/* Trc_JCL_park_Entry(vmThread, timeoutIsEpochRelative, timeout); */
	if ((0 != timeout) || (timeoutIsEpochRelative)) {
		if (timeoutIsEpochRelative) {
			/* Currently, the omrthread layer provides no direct support for absolute timeouts.
			 * Simulate the timeout by calculating the delta from the current time.
			 */
			PORT_ACCESS_FROM_VMC(vmThread);
			I_64 timeNow = j9time_current_time_millis();

			millis = timeout - timeNow;
			nanos = 0;

			if (millis <= 0) {
				rc = J9THREAD_TIMED_OUT;
				/*Trc_JCL_park_timeIsInPast(vmThread, timeNow);*/
			}
		} else {
			millis = timeout / oneMillion;
			nanos = (I_32)(timeout % oneMillion);
		}
		thrstate |= J9_PUBLIC_FLAGS_THREAD_TIMED;
	}
#if defined(J9VM_OPT_CRIU_SUPPORT)
	else {
		/* timeoutIsEpochRelative is false and timeout = 0 */
		if (J9_THROW_BLOCKING_EXCEPTION_IN_SINGLE_THREAD_MODE(vm)
			&& VM_VMHelpers::threadCanRunJavaCode(vmThread)
			&& (vmThread == vm->checkpointState.checkpointThread)
			&& J9_ARE_NO_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT)
		) {
			/* This is the checkpoint/restore thread, can't park indefinitely. */
			setCRIUSingleThreadModeJVMCRIUException(vmThread, 0, 0);
			return;
		}
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

#ifdef J9VM_OPT_SIDECAR
	/* Increment the wait count even if the deadline is past. */
	vmThread->mgmtWaitedCount++;
#endif

	if (J9THREAD_TIMED_OUT != rc) {
		PORT_ACCESS_FROM_VMC(vmThread);
		/* vmThread->threadObject != NULL because vmThread must be the current thread */
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, J9VMJAVALANGTHREAD_PARKBLOCKER(vmThread, vmThread->threadObject));
		TRIGGER_J9HOOK_VM_PARK(vm->hookInterface, vmThread, millis, nanos);
		internalReleaseVMAccessSetStatus(vmThread, thrstate);

		while (1) {
			rc = timeCompensationHelper(vmThread, HELPER_TYPE_THREAD_PARK, NULL, millis, nanos);
			if (timeoutIsEpochRelative
				&& (J9THREAD_TIMED_OUT == rc)
			) {
				I_64 timeNow = j9time_current_time_millis();
				if (timeNow < timeout) {
					millis = timeout - timeNow;
					nanos = 0;
					continue;
				}
			}
			break;
		}

		internalAcquireVMAccessClearStatus(vmThread, thrstate);
		TRIGGER_J9HOOK_VM_UNPARKED(vm->hookInterface, vmThread);
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, NULL);
	}
	/* Trc_JCL_park_Exit(vmThread, rc); */
}

/**
 * @param[in] vmThread the current thread
 * @param[in] threadObject the thread to unpark
 */
void
threadUnparkImpl(J9VMThread *vmThread, j9object_t threadObject)
{
	j9object_t threadLock = J9VMJAVALANGTHREAD_LOCK(vmThread, threadObject);

	if (NULL == threadLock) {
		/* thread not fully set up yet so we cannot really need to unpark, just return */
		/*Trc_JCL_park_unparkBeforeThreadFullySetup(vmThread);*/
	} else {
		/* don't allow another thread terminate while I'm unparking it, because we may release vm access during
		 * the enter and the gc could move the targetThreadObject we have to push/pop on a special frame so that
		 * we will have the updated value if the object is moved */
		PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, threadObject);
		threadLock = (j9object_t)objectMonitorEnter(vmThread, threadLock);
		if (J9_OBJECT_MONITOR_ENTER_FAILED(threadLock)) {
#if defined(J9VM_OPT_CRIU_SUPPORT)
			if (J9_OBJECT_MONITOR_CRIU_SINGLE_THREAD_MODE_THROW == (UDATA)threadLock) {
				setCRIUSingleThreadModeJVMCRIUException(vmThread, 0, 0);
			} else
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
			if (J9_OBJECT_MONITOR_OOM == (UDATA)threadLock) {
				/* we may be out of memory in which case we will not unpark
				 * the call to objectMonitorEnter will already have posted an OOM on the
				 * thread in this case so we just return */
				/*Trc_JCL_park_outOfMemoryInUnpark(vmThread);*/
			}
		} else {
			threadObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
			/* get the vmThread for the thread object, since we hold the lock this will not change under us */
			J9VMThread *otherVmThread = J9VMJAVALANGTHREAD_THREADREF(vmThread, threadObject);
			/*Trc_JCL_unpark_Entry(vmThread, otherVmThread);*/
			if (NULL != otherVmThread) {
				/* in this case the thread is already dead so we don't need to unpark */
				omrthread_unpark(otherVmThread->osThread);
			}
			objectMonitorExit(vmThread, threadLock);
			/*Trc_JCL_unpark_Exit(vmThread);*/
		}
	}
}

}
