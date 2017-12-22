/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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

#include "j9.h"
#include "omrthread.h"
#include "j9consts.h"
#include "j9protos.h"
#include "ut_j9vm.h"
#include "objhelp.h"

#include <string.h>



/**
 * @param[in] vmThread the current thread
 * @param[in] timeoutIsEpochRelative is the timeout in milliseconds relative to the beginning of the epoch
 * @param[in] timeout nanosecond or millisecond timeout
 */
void
threadParkImpl(J9VMThread *vmThread, IDATA timeoutIsEpochRelative, I_64 timeout)
{
	I_64 millis = 0;
	IDATA nanos = 0;
	IDATA rc = 0;
	UDATA thrstate;


	/*Trc_JCL_park_Entry(vmThread, timeoutIsEpochRelative, timeout);*/

	if (timeout || timeoutIsEpochRelative) {
		if (timeoutIsEpochRelative) {
			/* Currently, the omrthread layer provides no direct support for absolute timeouts.
			 * Simulate the timeout by calculating the delta from the current time
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
			millis = timeout / 1000000;
			nanos = (IDATA)(timeout % 1000000);
		}
		thrstate = J9_PUBLIC_FLAGS_THREAD_PARKED | J9_PUBLIC_FLAGS_THREAD_TIMED;
	} else {
		thrstate = J9_PUBLIC_FLAGS_THREAD_PARKED;
	}

#ifdef J9VM_OPT_SIDECAR
	/* Increment the wait count even if the deadline is past */
	vmThread->mgmtWaitedCount++;
#endif

	if (rc != J9THREAD_TIMED_OUT) {
		PORT_ACCESS_FROM_VMC(vmThread);
		/* vmThread->threadObject != NULL because vmThread must be the current thread */
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, J9VMJAVALANGTHREAD_PARKBLOCKER(vmThread, vmThread->threadObject));
		TRIGGER_J9HOOK_VM_PARK(vmThread->javaVM->hookInterface, vmThread, millis, nanos);
		internalReleaseVMAccessSetStatus(vmThread, thrstate);

		while(1){
			I_64 timeNow;
			rc = omrthread_park(millis, nanos);

			if(!(timeoutIsEpochRelative && rc == J9THREAD_TIMED_OUT && ((timeNow=j9time_current_time_millis()) < timeout))){
				break;
			}
			millis = timeout - timeNow;
			nanos = 0;
		}
	
		internalAcquireVMAccessClearStatus(vmThread, thrstate);
		TRIGGER_J9HOOK_VM_UNPARKED(vmThread->javaVM->hookInterface, vmThread);
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, NULL);
	}
	
	/*Trc_JCL_park_Exit(vmThread, rc);*/
}


/**
 * @param[in] vmThread the current thread
 * @param[in] threadObject the thread to unpark
 */
void
threadUnparkImpl(J9VMThread* vmThread, j9object_t threadObject)
{
	J9VMThread* otherVmThread = NULL;
	j9object_t threadLock = J9VMJAVALANGTHREAD_LOCK(vmThread, threadObject);

	if (threadLock == NULL){
		/* thread not fully set up yet so we cannot really need to unpark
		 * just return
		 */
		/*Trc_JCL_park_unparkBeforeThreadFullySetup(vmThread);*/
		return;
	}

	/* don't allow another thread terminate while I'm unparking it, because we may release vm access during
	 * the enter and the gc could move the targetThreadObject we have to push/pop on a special frame so that
	 * we will have the updated value if the object is moved */
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, threadObject);
	threadLock = (j9object_t) objectMonitorEnter(vmThread, threadLock);
	threadObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
	if(threadLock == NULL) {
		/* we may be out of memory in which case we will not unpark 
		 * the call to objectMonitorEnter will already have posted an OOM on the
		 * thread in this case so we just return */
		/*Trc_JCL_park_outOfMemoryInUnpark(vmThread);*/
		return;
	}

	/* get the vmThread for the thread object, since we hold the lock this will not change under us */
	otherVmThread = J9VMJAVALANGTHREAD_THREADREF(vmThread, threadObject);
	/*Trc_JCL_unpark_Entry(vmThread, otherVmThread);*/
	if (otherVmThread != NULL){
		/* in this case the thread is already dead so we don't need to unpark */
		omrthread_unpark(otherVmThread->osThread);
	}
	objectMonitorExit(vmThread, threadLock);

	/*Trc_JCL_unpark_Exit(vmThread);*/
}

