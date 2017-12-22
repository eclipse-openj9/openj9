/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
#include "j9jclnls.h"
#include "ut_j9vm.h"
#ifdef J9VM_THR_LOCK_NURSERY
#include "lockNurseryUtil.h"
#endif
#include "monhelp.h"
#include "objhelp.h"

#include <string.h>


extern "C" {

#define CASTMON(monitor)  ((J9ThreadAbstractMonitor*)(monitor))


static IDATA validateTimeouts(J9VMThread* vmThread, I_64 millis, I_32 nanos);
static IDATA failedToSetAttr(IDATA rc);

/**
 * @return non-zero on error. If an error occurs, an IllegalArgumentException will be set.
 */
static IDATA
validateTimeouts(J9VMThread* vmThread, I_64 millis, I_32 nanos)
{
	if (millis < 0) {
		setCurrentExceptionNLS(
			vmThread, 
			J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION,
			J9NLS_JCL_TIMEOUT_VALUE_IS_NEGATIVE);
	} else if (nanos < 0 || nanos >= 1000000) {
		setCurrentExceptionNLS(
			vmThread, 
			J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION,
			J9NLS_JCL_NANOSECOND_TIMEOUT_VALUE_OUT_OF_RANGE);
	} else {
		return 0;
	}

//	Trc_JCL_InvalidTimeout(vmThread, millis, nanos);

	return -1;

}
/**
 * @param[in] vmThread current thread
 * @param[in] object the object to wait on
 * @param[in] millis millisecond timeout
 * @param[in] nanos nanosecond timeout
 * @param[in] interruptable set to FALSE to ignore interrupts
 *
 * @return 0 on success, non-zero on failure. This function always sets the current exception on failure.
 */
IDATA
monitorWaitImpl(J9VMThread *vmThread, j9object_t object, I_64 millis, I_32 nanos, UDATA interruptable)
{
	IDATA rc;
	UDATA thrstate;
	omrthread_monitor_t monitor;
	J9JavaVM* javaVM = vmThread->javaVM;

//	Trc_JCL_wait_Entry(vmThread, object, millis, nanos);

	if (validateTimeouts(vmThread, millis, (I_32)nanos)) {
//		Trc_JCL_wait_Exit(vmThread);
		return -1;
	}
	
	if (millis | nanos) {
		thrstate = J9_PUBLIC_FLAGS_THREAD_WAITING | J9_PUBLIC_FLAGS_THREAD_TIMED;
	} else {
		thrstate = J9_PUBLIC_FLAGS_THREAD_WAITING;
	}

	monitor = getMonitorForWait(vmThread, object);
	if (monitor == NULL) {
		/* some error occurred. The helper will have set the current exception */
//		Trc_JCL_wait_Exit(vmThread);
		return -1;
	}
	omrthread_monitor_pin(monitor, vmThread->osThread);
	
	/* We need to put the blocking object in the special frame since calling out to the hooks could cause
	 * a GC wherein the object might move.  Note that we can't simply store the object before the hook call since the
	 * hook might call back into this method if it tries to wait in Java code.
	 */
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, object);
	TRIGGER_J9HOOK_VM_MONITOR_WAIT(vmThread->javaVM->hookInterface, vmThread, monitor, millis, (I_32)nanos);
	object = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

#ifdef J9VM_OPT_SIDECAR
	vmThread->mgmtWaitedCount++;
#endif

	J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, object);
	object = NULL;
	internalReleaseVMAccessSetStatus(vmThread, thrstate);
	if (interruptable) {
		rc = omrthread_monitor_wait_interruptable(monitor, millis, nanos);
	} else {
		rc = omrthread_monitor_wait_timed(monitor, millis, nanos);
	}
	internalAcquireVMAccessClearStatus(vmThread, thrstate);
	J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, NULL);

	omrthread_monitor_unpin(monitor, vmThread->osThread);
	
	TRIGGER_J9HOOK_VM_MONITOR_WAITED(javaVM->hookInterface, vmThread, monitor, millis, (I_32)nanos, rc);

	switch (rc) {
	case 0:		
//		Trc_JCL_wait_Exit(vmThread);
		return 0;
		
	case J9THREAD_TIMED_OUT:
//		Trc_JCL_wait_TimedOut(vmThread);
		return 0;
		
	case J9THREAD_PRIORITY_INTERRUPTED:
//		Trc_JCL_wait_PriorityInterrupted(vmThread);
		/* just return and allow #checkAsyncEvents:checkForContextSwitch: to do its job */
		return 0;
		
	case J9THREAD_INTERRUPTED:
//		Trc_JCL_wait_Interrupted(vmThread);

		setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERRUPTEDEXCEPTION, NULL);

#if defined(J9VM_OPT_SIDECAR) && ( defined (WIN32) || defined(WIN64))
		/* since the interrupt status was consumed by interrupting the Wait or Sleep
		 * reset the sidecar interrupt status
		 */
		if (javaVM->sidecarClearInterruptFunction != NULL){
			((void(*)(J9VMThread *vmThread))javaVM->sidecarClearInterruptFunction)(vmThread);
		}
#endif

		return -1;
		
	case J9THREAD_ILLEGAL_MONITOR_STATE:
//		Trc_JCL_wait_IllegalState(vmThread);
		setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
		return -1;
		
	default:
//		Trc_JCL_wait_Error(vmThread, rc);
		setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		return -1;
	}
}

/**
 * @param[in] vmThread current thread
 * @param[in] object the object to wait on
 * @param[in] millis millisecond timeout
 * @param[in] nanos nanosecond timeout
 *
 * @return 0 on success, non-zero on failure. This function always sets the current exception on failure.
 */
IDATA
threadSleepImpl(J9VMThread* vmThread, I_64 millis, I_32 nanos)
{
	IDATA rc;
	J9JavaVM* javaVM = vmThread->javaVM;

//	Trc_JCL_sleep_Entry(vmThread, millis, nanos);

	if (validateTimeouts(vmThread, millis, (I_32)nanos)) {
//		Trc_JCL_sleep_Exit(vmThread);
		return -1;
	}
	
#ifdef J9VM_OPT_SIDECAR
	/* Increment the wait count even if the deadline is past */
	vmThread->mgmtWaitedCount++;
#endif

	TRIGGER_J9HOOK_VM_SLEEP(javaVM->hookInterface, vmThread, millis, nanos);
	internalReleaseVMAccessSetStatus(vmThread, J9_PUBLIC_FLAGS_THREAD_SLEEPING);
	rc = omrthread_sleep_interruptable(millis, nanos);
	internalAcquireVMAccessClearStatus(vmThread, J9_PUBLIC_FLAGS_THREAD_SLEEPING);
	TRIGGER_J9HOOK_VM_SLEPT(javaVM->hookInterface, vmThread);

	if (rc == 0) {
//		Trc_JCL_sleep_Exit(vmThread);
		return 0;
	} else if (rc == J9THREAD_INTERRUPTED) {
//		Trc_JCL_sleep_Interrupted(vmThread);
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERRUPTEDEXCEPTION, NULL);


#if defined(J9VM_OPT_SIDECAR) && ( defined (WIN32) || defined(WIN64))
		/* since the interrupt status was consumed by interrupting the Wait or Sleep
		 * reset the sidecar interrupt status
		 */
		if (javaVM->sidecarClearInterruptFunction != NULL){
			((void(*)(J9VMThread *vmThread))javaVM->sidecarClearInterruptFunction)(vmThread);
		}
#endif

		return -1;
	} else if (rc == J9THREAD_PRIORITY_INTERRUPTED) {
//		Trc_JCL_sleep_PriorityInterrupted(vmThread);
		/* just return and allow #checkAsyncEvents:checkForContextSwitch: to do its job */
		return 0;
	} else {
//		Trc_JCL_sleep_Error(vmThread, rc);
		setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		return -1;
	}
}

/**
 * @return NULL on error. If an error occurs, the current exception will be set.
 */
omrthread_monitor_t
getMonitorForWait(J9VMThread* vmThread, j9object_t object)
{
	j9objectmonitor_t lock;
	omrthread_monitor_t monitor;
	J9ObjectMonitor * objectMonitor;

#ifdef J9VM_THR_LOCK_NURSERY
	if (!LN_HAS_LOCKWORD(vmThread,object)) {
		objectMonitor = monitorTableAt(vmThread, object);

//		Trc_JCL_foundMonitorInNursery(vmThread, objectMonitor? objectMonitor->monitor : NULL, object);

		if (objectMonitor == NULL) {
			setNativeOutOfMemoryError(vmThread, J9NLS_JCL_FAILED_TO_INFLATE_MONITOR);
			return NULL;
		}
		lock = objectMonitor->alternateLockword;
	} 
	else 
#endif
	{
		lock = J9OBJECT_MONITOR(vmThread, object);
	}

	if (J9_LOCK_IS_INFLATED(lock)) {
		objectMonitor = J9_INFLLOCK_OBJECT_MONITOR(lock);
		
		monitor = objectMonitor->monitor;
//		Trc_JCL_foundMonitorInLockword(vmThread, monitor, object);

		return monitor;
	} else {

		if ( vmThread != J9_FLATLOCK_OWNER(lock) ) {
//			Trc_JCL_threadDoesntOwnFlatlock(vmThread, object);
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
			return NULL;
		}

#ifdef J9VM_THR_LOCK_RESERVATION
		if ( (lock & (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_RESERVED)) == OBJECT_HEADER_LOCK_RESERVED) {
//			Trc_JCL_threadReservedButDoesntOwnFlatlock(vmThread, object);
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
			return NULL;
		}
#endif

		objectMonitor = objectMonitorInflate(vmThread, object, lock);

//		Trc_JCL_foundMonitorInTable(vmThread, objectMonitor ? objectMonitor->monitor : NULL, object);

		if (objectMonitor == NULL) {
			setNativeOutOfMemoryError(vmThread, J9NLS_JCL_FAILED_TO_INFLATE_MONITOR);
			monitor = NULL;
		} else {
			monitor = objectMonitor->monitor;
		}

		return monitor;
	}
}

/*
 * See vm_api.h for doc
 */
IDATA
createThreadWithCategory(omrthread_t* handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend,
						 omrthread_entrypoint_t entrypoint, void* entryarg, uint32_t category)
{
	omrthread_attr_t attr;
	IDATA rc = J9THREAD_SUCCESS;

	if (J9THREAD_SUCCESS != omrthread_attr_init(&attr)) {
		return J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_priority(&attr, priority))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize(&attr, stacksize))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&attr, category))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	rc = omrthread_create_ex(handle, &attr, suspend, entrypoint, entryarg);

destroy_attr:
	omrthread_attr_destroy(&attr);
	return rc;
}

/*
 * See vm_api.h for doc
 */
IDATA
attachThreadWithCategory(omrthread_t* handle, uint32_t category)
{
	omrthread_attr_t attr;
	IDATA rc = J9THREAD_SUCCESS;

	if (J9THREAD_SUCCESS != omrthread_attr_init(&attr)) {
		return J9THREAD_ERR_CANT_ALLOC_ATTACH_ATTR;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&attr, category))) {
		rc = J9THREAD_ERR_INVALID_ATTACH_ATTR;
		goto destroy_attr;
	}

	rc = omrthread_attach_ex(handle, &attr);

destroy_attr:
	omrthread_attr_destroy(&attr);
	return rc;
}

static IDATA
failedToSetAttr(IDATA rc)
{
	rc &= ~J9THREAD_ERR_OS_ERRNO_SET;
	return ((rc != J9THREAD_SUCCESS) && (rc != J9THREAD_ERR_UNSUPPORTED_ATTR));
}

}
