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
#include "j9jclnls.h"
#include "j9protos.h"
#include "monhelp.h"
#include "objhelp.h"
#include "omrthread.h"
#include "ut_j9vm.h"

#include "VMHelpers.hpp"

#include <string.h>

extern "C" {

#define CASTMON(monitor)  ((J9ThreadAbstractMonitor*)(monitor))

static IDATA validateTimeouts(J9VMThread *vmThread, I_64 millis, I_32 nanos);
static IDATA failedToSetAttr(IDATA rc);
static IDATA setThreadAttributes(omrthread_attr_t *attr, uintptr_t stacksize, uintptr_t priority, uint32_t category, omrthread_detachstate_t detachState);

/**
 * @return non-zero on error. If an error occurs, an IllegalArgumentException will be set.
 */
static IDATA
validateTimeouts(J9VMThread *vmThread, I_64 millis, I_32 nanos)
{
	IDATA result = -1;

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
		result = 0;
	}
	/* Trc_JCL_InvalidTimeout(vmThread, millis, nanos); */

	return result;
}

IDATA
monitorWaitImpl(J9VMThread *vmThread, j9object_t object, I_64 millis, I_32 nanos, BOOLEAN interruptable)
{
	IDATA rc = -1;
	omrthread_monitor_t monitor = getMonitorForWait(vmThread, object);

	/* Trc_JCL_wait_Entry(vmThread, object, millis, nanos); */
	if ((0 != validateTimeouts(vmThread, millis, nanos))
		|| (NULL == monitor)
	) {
		/* Trc_JCL_wait_Exit(vmThread); */
		/* some error occurred. The helper will have set the current exception */
	} else {
		UDATA thrstate = 0;
		J9JavaVM *javaVM = vmThread->javaVM;
		PORT_ACCESS_FROM_JAVAVM(javaVM);
		J9Class *monitorClass = NULL;
		I_64 startTicks = j9time_nano_time();

		monitorClass = J9OBJECT_CLAZZ(vmThread, object);

		if ((millis > 0) || (nanos > 0)) {
			thrstate = J9_PUBLIC_FLAGS_THREAD_WAITING | J9_PUBLIC_FLAGS_THREAD_TIMED;
		} else {
			thrstate = J9_PUBLIC_FLAGS_THREAD_WAITING;
		}
		omrthread_monitor_pin(monitor, vmThread->osThread);
		/* We need to put the blocking object in the special frame since calling out to the hooks could cause
		 * a GC wherein the object might move. Note that we can't simply store the object before the hook call since the
		 * hook might call back into this method if it tries to wait in Java code.
		 */
		PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, object);
		TRIGGER_J9HOOK_VM_MONITOR_WAIT(javaVM->hookInterface, vmThread, monitor, millis, nanos);
		object = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
#ifdef J9VM_OPT_SIDECAR
		vmThread->mgmtWaitedCount++;
#endif
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, object);
		object = NULL;
		internalReleaseVMAccessSetStatus(vmThread, thrstate);
		rc = timeCompensationHelper(vmThread,
			interruptable ? HELPER_TYPE_MONITOR_WAIT_INTERRUPTABLE : HELPER_TYPE_MONITOR_WAIT_TIMED, monitor, millis, nanos);
		internalAcquireVMAccessClearStatus(vmThread, thrstate);
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(vmThread, vmThread, NULL);
		omrthread_monitor_unpin(monitor, vmThread->osThread);
		TRIGGER_J9HOOK_VM_MONITOR_WAITED(javaVM->hookInterface, vmThread, monitor, millis, nanos, rc, startTicks, (UDATA) monitor, VM_VMHelpers::currentClass(monitorClass));

		switch (rc) {
		case 0:
			/* Trc_JCL_wait_Exit(vmThread); */
			/* FALLTHROUGH */
		case J9THREAD_TIMED_OUT:
			/* Trc_JCL_wait_TimedOut(vmThread); */
			/* FALLTHROUGH */
		case J9THREAD_PRIORITY_INTERRUPTED:
			/* Trc_JCL_wait_PriorityInterrupted(vmThread); */
			/* just return and allow #checkAsyncEvents:checkForContextSwitch: to do its job */
			rc = 0;
			break;

		case J9THREAD_INTERRUPTED:
			/* Trc_JCL_wait_Interrupted(vmThread); */
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERRUPTEDEXCEPTION, NULL);
#if JAVA_SPEC_VERSION >= 19
			J9VMJAVALANGTHREAD_SET_DEADINTERRUPT(vmThread, vmThread->threadObject, JNI_FALSE);
#endif /* JAVA_SPEC_VERSION >= 19 */
#if defined(J9VM_OPT_SIDECAR) && ( defined (WIN32) || defined(WIN64))
			/* Since the interrupt status was consumed by interrupting the Wait or Sleep
			 * reset the sidecar interrupt status.
			 */
			if (NULL != javaVM->sidecarClearInterruptFunction) {
				((void(*)(J9VMThread *vmThread))javaVM->sidecarClearInterruptFunction)(vmThread);
			}
#endif
			rc = -1;
			break;
		case J9THREAD_ILLEGAL_MONITOR_STATE:
			/* Trc_JCL_wait_IllegalState(vmThread); */
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
			rc = -1;
			break;
		default:
			/* Trc_JCL_wait_Error(vmThread, rc); */
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			rc = -1;
			break;
		}
	}

	return rc;
}

IDATA
threadSleepImpl(J9VMThread *vmThread, I_64 millis, I_32 nanos)
{
	IDATA rc = 0;
	J9JavaVM *javaVM = vmThread->javaVM;

	/* Trc_JCL_sleep_Entry(vmThread, millis, nanos); */
	if (0 != validateTimeouts(vmThread, millis, nanos)) {
		/* An IllegalArgumentException has been set. */
		rc = -1;
	} else {
		PORT_ACCESS_FROM_JAVAVM(javaVM);
		I_64 startTicks = (U_64) j9time_nano_time();

#ifdef J9VM_OPT_SIDECAR
		/* Increment the wait count even if the deadline is past. */
		vmThread->mgmtWaitedCount++;
#endif
		if (0 == rc) {
			TRIGGER_J9HOOK_VM_SLEEP(javaVM->hookInterface, vmThread, millis, nanos);
			internalReleaseVMAccessSetStatus(vmThread, J9_PUBLIC_FLAGS_THREAD_SLEEPING);
			rc = timeCompensationHelper(vmThread, HELPER_TYPE_THREAD_SLEEP, NULL, millis, nanos);
			internalAcquireVMAccessClearStatus(vmThread, J9_PUBLIC_FLAGS_THREAD_SLEEPING);
			TRIGGER_J9HOOK_VM_SLEPT(javaVM->hookInterface, vmThread, millis, nanos, startTicks);
		}

		if (0 == rc) {
			/* Trc_JCL_sleep_Exit(vmThread); */
			/* omrthread_sleep_interruptable returned on success */
		} else if (J9THREAD_INTERRUPTED == rc) {
			/* Trc_JCL_sleep_Interrupted(vmThread); */
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERRUPTEDEXCEPTION, NULL);
#if JAVA_SPEC_VERSION >= 19
			J9VMJAVALANGTHREAD_SET_DEADINTERRUPT(vmThread, vmThread->threadObject, JNI_FALSE);
#endif /* JAVA_SPEC_VERSION >= 19 */
#if defined(J9VM_OPT_SIDECAR) && ( defined (WIN32) || defined(WIN64))
			/* Since the interrupt status was consumed by interrupting the Wait or Sleep
			 * reset the sidecar interrupt status.
			 */
			if (NULL != javaVM->sidecarClearInterruptFunction) {
				((void(*)(J9VMThread *vmThread))javaVM->sidecarClearInterruptFunction)(vmThread);
			}
#endif
			rc = -1;
		} else if (J9THREAD_PRIORITY_INTERRUPTED == rc) {
			/* Trc_JCL_sleep_PriorityInterrupted(vmThread); */
			/* just return and allow #checkAsyncEvents:checkForContextSwitch: to do its job */
			rc = 0;
		} else {
			/* Trc_JCL_sleep_Error(vmThread, rc); */
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			rc = -1;
		}
	}

	return rc;
}

/**
 * @return NULL on error. If an error occurs, the current exception will be set.
 */
omrthread_monitor_t
getMonitorForWait(J9VMThread* vmThread, j9object_t object)
{
	j9objectmonitor_t lock;
	j9objectmonitor_t *lockEA = NULL;
	omrthread_monitor_t monitor;
	J9ObjectMonitor * objectMonitor;

	if (!LN_HAS_LOCKWORD(vmThread,object)) {
		objectMonitor = monitorTableAt(vmThread, object);

//		Trc_JCL_foundMonitorInNursery(vmThread, objectMonitor? objectMonitor->monitor : NULL, object);

		if (objectMonitor == NULL) {
			setNativeOutOfMemoryError(vmThread, J9NLS_JCL_FAILED_TO_INFLATE_MONITOR);
			return NULL;
		}
		lockEA = &objectMonitor->alternateLockword;
	}
	else {
		lockEA = J9OBJECT_MONITOR_EA(vmThread, object);
	}
	lock = J9_LOAD_LOCKWORD(vmThread, lockEA);

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
		if (((lock & (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_RESERVED)) == OBJECT_HEADER_LOCK_RESERVED) ||
		    ((lock & (OBJECT_HEADER_LOCK_LEARNING_RECURSION_MASK | OBJECT_HEADER_LOCK_LEARNING)) == OBJECT_HEADER_LOCK_LEARNING)
		) {
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

static IDATA
setThreadAttributes(omrthread_attr_t *attr, uintptr_t stacksize, uintptr_t priority, uint32_t category, omrthread_detachstate_t detachState)
{
	IDATA rc = J9THREAD_SUCCESS;

	if (failedToSetAttr(omrthread_attr_set_schedpolicy(attr, J9THREAD_SCHEDPOLICY_OTHER))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto _end;
	}

	if (failedToSetAttr(omrthread_attr_set_priority(attr, priority))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto _end;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize(attr, stacksize))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto _end;
	}

	if (failedToSetAttr(omrthread_attr_set_category(attr, category))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto _end;
	}

	if (failedToSetAttr(omrthread_attr_set_detachstate(attr, detachState))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto _end;
	}

_end:
	return rc;
}

/*
 * See vm_api.h for doc
 */
IDATA
createJoinableThreadWithCategory(omrthread_t* handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend,
						 omrthread_entrypoint_t entrypoint, void* entryarg, uint32_t category)
{
	omrthread_attr_t attr;
	IDATA rc = J9THREAD_SUCCESS;

	if (J9THREAD_SUCCESS != omrthread_attr_init(&attr)) {
		return J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
	}

	rc = setThreadAttributes(&attr, stacksize, priority, category, J9THREAD_CREATE_JOINABLE);
	if (J9THREAD_SUCCESS != rc) {
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
createThreadWithCategory(omrthread_t* handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend,
						 omrthread_entrypoint_t entrypoint, void* entryarg, uint32_t category)
{
	omrthread_attr_t attr;
	IDATA rc = J9THREAD_SUCCESS;

	if (J9THREAD_SUCCESS != omrthread_attr_init(&attr)) {
		return J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
	}

	rc = setThreadAttributes(&attr, stacksize, priority, category, J9THREAD_CREATE_DETACHED);
	if (J9THREAD_SUCCESS != rc) {
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

#if JAVA_SPEC_VERSION >= 19
/**
 * Frees a thread object's TLS array.
 *
 * @param[in] currentThread the current thread
 * @param[in] threadObj the thread object to free TLS from
 */
void
freeTLS(J9VMThread *currentThread, j9object_t threadObj)
{
	J9JavaVM *vm = currentThread->javaVM;
	void *tlsArray = J9OBJECT_ADDRESS_LOAD(currentThread, threadObj, vm->tlsOffset);
	if (NULL != tlsArray) {
		omrthread_monitor_enter(vm->tlsPoolMutex);
		pool_removeElement(vm->tlsPool, tlsArray);
		omrthread_monitor_exit(vm->tlsPoolMutex);
		J9OBJECT_ADDRESS_STORE(currentThread, threadObj, vm->tlsOffset, NULL);
	}
}
#endif /* JAVA_SPEC_VERSION >= 19 */

IDATA
timeCompensationHelper(J9VMThread *vmThread, U_8 threadHelperType, omrthread_monitor_t monitor, I_64 millis, I_32 nanos)
{
	IDATA rc = 0;

#if defined(J9VM_OPT_CRIU_SUPPORT)
	J9JavaVM *vm = vmThread->javaVM;
	/* Time compensation only supports CRIURestoreNonPortableMode which is default mode. */
	bool waitTimed = (millis > 0) || (nanos > 0);
	bool compensationMightBeRequired = waitTimed && !J9_IS_CRIU_RESTORED(vm);
	I_64 nanoTimeStart = 0;
	if (compensationMightBeRequired) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		nanoTimeStart = j9time_nano_time();
	}
continueTimeCompensation:
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	switch(threadHelperType) {
	/* Following three APIs return J9THREAD_TIMED_OUT when the timeout expired.
	 * A timeout of 0 (0ms, 0ns) indicates wait/park indefinitely.
	 */
	case HELPER_TYPE_MONITOR_WAIT_INTERRUPTABLE:
		rc = omrthread_monitor_wait_interruptable(monitor, millis, nanos);
		break;
	case HELPER_TYPE_MONITOR_WAIT_TIMED:
		rc = omrthread_monitor_wait_timed(monitor, millis, nanos);
		break;
	case HELPER_TYPE_THREAD_PARK:
		rc = omrthread_park(millis, nanos);
		break;
	case HELPER_TYPE_THREAD_SLEEP:
		/* Returns 0 when the timeout specified passed.
		 * A timeout of 0 (0ms, 0ns) indicates an immediate return.
		 */
		rc = omrthread_sleep_interruptable(millis, nanos);
		break;
	default:
		Assert_VM_unreachable();
	}

#if defined(J9VM_OPT_CRIU_SUPPORT)
	bool isThreadHaltedAtSingleThreadedMode = J9_IS_SINGLE_THREAD_MODE(vm)
		&& J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
	if ((compensationMightBeRequired && J9_IS_CRIU_RESTORED(vm))
		|| isThreadHaltedAtSingleThreadedMode
	) {
		/* There was a C/R and a compensation might be required,
		 * or this is a halted thread at single threaded mode.
		 */
		bool timeCompensationRequired = true;
		const I_8 timeout10ms = 10;
		bool timedout = false;
		if (HELPER_TYPE_THREAD_SLEEP == threadHelperType) {
			timedout = 0 == rc;
		} else {
			timedout = J9THREAD_TIMED_OUT == rc;
		}
		if (waitTimed && timedout) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			const I_32 oneMillion = 1000000;
			I_64 nanoTimePast = j9time_nano_time() - nanoTimeStart;
			I_64 millisPast = nanoTimePast / oneMillion;
			nanoTimePast = nanoTimePast % oneMillion;
			if (millisPast < millis) {
				/* not timed out yet */
				millis -= millisPast;
				if (nanos >= nanoTimePast) {
					nanos = (I_32)(nanos - nanoTimePast);
				} else {
					/* nanos/nanoTimePast are within [0, 999999] */
					nanos = (I_32)(oneMillion - nanoTimePast + nanos);
					millis -= 1;
				}
			} else if ((millisPast > millis)
				|| (nanoTimePast >= nanos)
			) {
				/* timed out since millisPast > millis or millisPast == millis && nanoTimePast >= nanos */
				if (isThreadHaltedAtSingleThreadedMode) {
					/* This is a halted thread at single threaded mode, sleep another 10ms. */
					millis = timeout10ms;
					nanos = 0;
				} else {
					/* timed out and no compensation required */
					timeCompensationRequired = false;
				}
			} else {
				/* not timed out yet, millisPast == millis and nanos > nanoTimePast */
				millis = 0;
				nanos = (I_32)(nanos - nanoTimePast);
			}
		} else if (isThreadHaltedAtSingleThreadedMode) {
			/* This is a halted thread at single threaded mode, wait another 10ms. */
			millis = timeout10ms;
			nanos = 0;
		} else {
			/* No time compensation if not waitTimed and timedout, or not paused at single threaded mode. */
			timeCompensationRequired = false;
		}
		if (timeCompensationRequired) {
			Assert_VM_false((0 == millis) && (0 == nanos));
			goto continueTimeCompensation;
		}
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	return rc;
}

}
