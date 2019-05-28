/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "j9consts.h"
#include "j9protos.h"
#include "ut_j9vm.h"
#ifdef J9VM_THR_LOCK_NURSERY
#include "lockNurseryUtil.h"
#endif
#include "vm_internal.h"
#include "util_internal.h"
#include "monhelp.h"

IDATA 
objectMonitorExit(J9VMThread* vmStruct, j9object_t object) 
{
	j9objectmonitor_t *lockEA = NULL;
	j9objectmonitor_t lock = 0;

	Assert_VM_true(vmStruct != NULL);
	Assert_VM_true(0 == ((UDATA)vmStruct & OBJECT_HEADER_LOCK_BITS_MASK));

	Trc_VM_objectMonitorExit_Entry(vmStruct,object);

#ifdef J9VM_THR_LOCK_NURSERY
	if (!LN_HAS_LOCKWORD(vmStruct,object)) {
		J9ObjectMonitor *objectMonitor = NULL;

		objectMonitor = monitorTableAt(vmStruct, object);
		if (objectMonitor == NULL) {
			Trc_VM_objectMonitorExit_Exit_IllegalNullMonitor(vmStruct, object);
			return J9THREAD_ILLEGAL_MONITOR_STATE;
		}

		lockEA = &(objectMonitor->alternateLockword);
	} else 
#endif /* J9VM_THR_LOCK_NURSERY */
	{
		lockEA = J9OBJECT_MONITOR_EA(vmStruct, object);
	}
	lock = *lockEA;

	if (J9_FLATLOCK_OWNER(lock) == vmStruct) {
		/* the current thread owns the monitor, and it's a flat lock */
		UDATA count = lock & OBJECT_HEADER_LOCK_BITS_MASK;
		/* low bits may be:
		 * 	 00 (no flatlock recursion, flatlock contention bit (FLC) is clear), 
		 *	 01 (inflated -- impossible)
		 *	 02 (no flatlock recursion, FLC set), or 
		 *	 03 (FLC set and inflated set -- impossible)
		 *   04-07 (RES=1, COUNT=0 -- reserved but unentered -- illegal)
		 *	>=08 (flatlock recursion)
		 */

		Assert_VM_false(lock & OBJECT_HEADER_LOCK_INFLATED);
#ifndef J9VM_THR_LOCK_RESERVATION
		Assert_VM_false(lock & OBJECT_HEADER_LOCK_RESERVED);
#endif

		if (count == 0x00) {
			/* just release the flatlock */
			clearLockWord(vmStruct, lockEA);
		} else if (count >= OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT) {
			/* just decrement the flatlock recursion count */
			lock -= OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT;
			*lockEA = lock;
#ifdef J9VM_THR_LOCK_RESERVATION
		} else if (count & OBJECT_HEADER_LOCK_RESERVED) {
			/* lock is reserved but unowned (if it were owned the count would be >= OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT) */
			Trc_VM_objectMonitorExit_Exit_ReservedButUnownedFlatLock(vmStruct, lock, object);
			return J9THREAD_ILLEGAL_MONITOR_STATE;
#endif
		} else {
			/* FLC set, non-recursive */
			J9ObjectMonitor *objectMonitor = objectMonitorInflate(vmStruct, object, lock);

			if (objectMonitor == NULL) {
				/* out of memory - impossible? */
				clearLockWord(vmStruct, lockEA);
			} else {
				omrthread_monitor_t monitor = objectMonitor->monitor;

				TRIGGER_J9HOOK_VM_MONITOR_CONTENDED_EXIT(vmStruct->javaVM->hookInterface, vmStruct, monitor);

				omrthread_monitor_exit(monitor);
			}
		}
		Trc_VM_objectMonitorExit_Exit_FCBSet(vmStruct);
		return 0;
	} else if (J9_LOCK_IS_INFLATED(lock)) {
		/* Dealing with an inflated monitor */
		J9ObjectMonitor *objectMonitor = NULL;
		J9ThreadAbstractMonitor *monitor = NULL;
		IDATA rc = 0;
		IDATA deflate = 1;
		
		objectMonitor = J9_INFLLOCK_OBJECT_MONITOR(lock);		
		monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
		Assert_VM_notNull(monitor);

#ifdef OMR_THR_ADAPTIVE_SPIN
		/* for now we don't allow deflation if spinning has been disabled for this monitor
		 * because it has a longer hold time */
		if ((NULL != monitor->tracing) && (0 != (monitor->flags & J9THREAD_MONITOR_DISABLE_SPINNING))) {
			deflate = 0;
		}
#endif

		if (monitor->owner != vmStruct->osThread) {
			Trc_VM_objectMonitorExit_Exit_IllegalInflatedLock(vmStruct, monitor->owner, vmStruct->osThread);
			return J9THREAD_ILLEGAL_MONITOR_STATE;
		}

		/*
		 * Are we releasing the inflated monitor? (count is 1)
		 * If we are, the question becomes: 
		 *    May we deflate? If so, CAN we deflate? 
		 * 
		 * We may deflate if the user has specified that we may. (thrDeflationPolicy)
		 * We can deflate if no thread is blocked on this inflated monitor (pin count is 0) and
		 *   iff the deflation policy in effect decides it's ok.
		 */
		if (monitor->count == 1) {
			if (0 == monitor->pinCount) {
				if (deflate) {
					deflate = 0;
					switch (vmStruct->javaVM->thrDeflationPolicy) {
					case J9VM_DEFLATION_POLICY_NEVER:
						break;
					case J9VM_DEFLATION_POLICY_ASAP:
						deflate = 1;
						break;
#ifdef J9VM_THR_SMART_DEFLATION
					case J9VM_DEFLATION_POLICY_SMART:
						Trc_VM_objectMonitorExit_SmartDecisionPoint(vmStruct,objectMonitor->proDeflationCount,objectMonitor->antiDeflationCount);
#ifdef J9VM_CPU_TIMESTAMP_SUPPORT
						deflate = (objectMonitor->proDeflationCount > objectMonitor->antiDeflationCount);
#else
						deflate = ( (objectMonitor->proDeflationCount / 1024) > objectMonitor->antiDeflationCount);
#endif /* J9VM_CPU_TIMESTAMP_SUPPORT */
						break;
#endif /* J9VM_THR_SMART_DEFLATION */
					}

					if (deflate) {
						monitor->flags &= ~J9THREAD_MONITOR_INFLATED;
						clearLockWord(vmStruct, lockEA);
						Trc_VM_objectMonitorDeflated(vmStruct, vmStruct->osThread, object, lock);
					}
				}
			} else {
				if (J9_EVENT_IS_HOOKED(vmStruct->javaVM->hookInterface, J9HOOK_VM_MONITOR_CONTENDED_EXIT)) {
					/* Are CONTENDED_EXIT events suppressed? */
					if (!(monitor->flags & J9THREAD_MONITOR_SUPPRESS_CONTENDED_EXIT)) {
						/* Any threads blocked on this monitor? If so, report contended exit */
						if ((monitor->pinCount - omrthread_monitor_num_waiting((omrthread_monitor_t)monitor)) > 0) {
							ALWAYS_TRIGGER_J9HOOK_VM_MONITOR_CONTENDED_EXIT(vmStruct->javaVM->hookInterface, vmStruct, (omrthread_monitor_t)monitor);
							monitor->flags |= J9THREAD_MONITOR_SUPPRESS_CONTENDED_EXIT;
						}
					}
				}
			}
		}
		rc =  omrthread_monitor_exit((omrthread_monitor_t)monitor);
		Trc_VM_objectMonitorExit_Exit_InflatedLock(vmStruct, rc);
		return rc;
	} else {
		/* flat lock, but wrong thread */
		Assert_VM_true( (lock == 0) || (lock & ~(j9objectmonitor_t)OBJECT_HEADER_LOCK_BITS_MASK) );
		Trc_VM_objectMonitorExit_Exit_IllegalFlatLock(vmStruct, lock, object);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	Assert_VM_unreachable();

}


/* assumes:
	The current thread owns the monitor.
	The lock has not already been inflated.
   returns:
	NULL if out of memory
	the inflated monitor, if successful
 */
J9ObjectMonitor * 
objectMonitorInflate(J9VMThread* vmStruct, j9object_t object, UDATA lock) 
{
	J9ObjectMonitor *objectMonitor = monitorTableAt(vmStruct, object);
	omrthread_monitor_t monitor = NULL;

	if (objectMonitor == NULL) {
		return NULL;
	}
	monitor = objectMonitor->monitor;

	omrthread_monitor_enter(monitor);

	/* set the count to be the current thread's count */
	((J9ThreadAbstractMonitor*)monitor)->count = J9_FLATLOCK_COUNT(lock);	

#ifdef J9VM_THR_LOCK_NURSERY
	if (!LN_HAS_LOCKWORD(vmStruct,object)) {
			objectMonitor->alternateLockword = (j9objectmonitor_t)(UDATA)objectMonitor | OBJECT_HEADER_LOCK_INFLATED;
	}
	else
#endif
	{
		J9OBJECT_SET_MONITOR(vmStruct, object, (j9objectmonitor_t)(UDATA)objectMonitor | OBJECT_HEADER_LOCK_INFLATED);
	}

	((J9ThreadAbstractMonitor*)monitor)->flags |= J9THREAD_MONITOR_INFLATED;

	/* move any waiting threads into the blocked queue */
	omrthread_monitor_notify_all(monitor);

	Trc_VM_objectMonitorInflated(vmStruct, vmStruct->osThread, object, objectMonitor);

	return objectMonitor;
}


IDATA 
objectMonitorEnter(J9VMThread* vmStruct, j9object_t object) 
{
	IDATA rc = objectMonitorEnterNonBlocking(vmStruct, object);

	if (rc == 1) {
		rc = objectMonitorEnterBlocking(vmStruct);
	}

	return rc;
}

#if defined (J9VM_THR_LOCK_RESERVATION)

void
cancelLockReservation(J9VMThread* vmStruct)
{
	j9object_t object = NULL;
	j9objectmonitor_t lock = 0;

	Trc_VM_cancelLockReservation_Entry(vmStruct, vmStruct->blockingEnterObject);
	Assert_VM_mustHaveVMAccess(vmStruct);

	object = vmStruct->blockingEnterObject;
#ifdef J9VM_THR_LOCK_NURSERY
	if (!LN_HAS_LOCKWORD(vmStruct,object)) {
		J9ObjectMonitor *objectMonitor = monitorTableAt(vmStruct, object);

		Assert_VM_true(objectMonitor != NULL);

		lock = objectMonitor->alternateLockword;
	} else
#endif
	{
		lock = *J9OBJECT_MONITOR_EA(vmStruct, object);
	}

	if ( (lock & (OBJECT_HEADER_LOCK_INFLATED | OBJECT_HEADER_LOCK_RESERVED)) == OBJECT_HEADER_LOCK_RESERVED) {
		j9objectmonitor_t oldLock = 0;
		j9objectmonitor_t newLock = 0;
		j9objectmonitor_t *lockEA = NULL;
		J9VMThread* reservationOwner = J9_FLATLOCK_OWNER(lock);

		Trc_VM_cancelLockReservation_reservationOwner(vmStruct, reservationOwner);

		/* halt the reserving thread */
		haltThreadForInspection(vmStruct, reservationOwner);

		/* refresh the object pointer, since we may have released VM access */
		object = vmStruct->blockingEnterObject;
#ifdef J9VM_THR_LOCK_NURSERY
		if (!LN_HAS_LOCKWORD(vmStruct,object)) {
			J9ObjectMonitor *objectMonitor = monitorTableAt(vmStruct, object);

			Assert_VM_true(objectMonitor != NULL);

			lockEA = &objectMonitor->alternateLockword;
		} else
#endif
		{
			lockEA = J9OBJECT_MONITOR_EA(vmStruct, object);
		}

		/* swap in an unreserved lock word */
		/* (must use atomics since another thread might be doing this too) */
		oldLock = *lockEA;

		/* must verify that the halted thread still owns the reservation */
		if ( J9_FLATLOCK_OWNER(oldLock) == reservationOwner ) {
			if (oldLock & OBJECT_HEADER_LOCK_RESERVED) {
				if ( (oldLock & OBJECT_HEADER_LOCK_RECURSION_MASK) > 0 ) {
					/* if the lock is acquired recursively, decrement the count and remove the reserved bit */
					newLock = oldLock - OBJECT_HEADER_LOCK_RESERVED - OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT;
					Assert_VM_true(J9_FLATLOCK_COUNT(oldLock) == J9_FLATLOCK_COUNT(newLock));
				} else {
					/* otherwise clear the lock completely */
					newLock = 0;
					Assert_VM_true(J9_FLATLOCK_COUNT(oldLock) == 0);
				}

				if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmStruct)) {
					compareAndSwapU32((uint32_t*)lockEA, (uint32_t)oldLock, (uint32_t)newLock);
				} else {
					compareAndSwapUDATA((uintptr_t*)lockEA, (uintptr_t)oldLock, (uintptr_t)newLock);
				}

				/* 
				 * This can only fail if another canceller has modified the lockword, in which case the
				 * object is either no longer reserved or reserved by a different thread.
				 * Such cases should be detected by the calling function when it re-attempts to enter the monitor.
				 */
			}
		}

		/* resume the reserving thread */
		resumeThreadForInspection(vmStruct, reservationOwner);
	}

	Trc_VM_cancelLockReservation_Exit(vmStruct);
}
#endif /* J9VM_THR_LOCK_RESERVATION */

/**
 * Destroy a monitor as appropriate for the given platform.
 *  For example, destroying a monitor may free the internal resources associated with the monitor. 
 *  Alternatively, destroying a monitor may return the monitor to a monitor pool for subsequent reuse. 
 *
 * @note A monitor must NOT be destroyed if the monitor is in use - if threads are waiting on
 * it, or if it is currently owned.
 *
 * @pre the caller must have VM access. 
 *
 * @pre Destroying a monitor modifies the hash key used by the VM's MonitorTable.
 * It must not be possible for another thread to be searching for this node in the
 * table, using this monitor, or attempting to add it to the table. Since this
 * monitor is associated with a destroyed object, we can assume that no
 * thread has references to it, and the monitor is unreachable. Since the
 * monitor is marked allocated, no other thread will be attempting to reuse
 * it.
 * 
 * @param[in] vm Java VM
 * @param[in] vmThread the J9VMThread calling this function
 * @param[in] monitor a monitor to be destroyed
 * @return  0 on success or non-0 on failure (the monitor is in use)
 */
IDATA 
objectMonitorDestroy(J9JavaVM *vm, J9VMThread* vmThread, omrthread_monitor_t monitor)
{
	return (IDATA)omrthread_monitor_destroy_nolock(vmThread->osThread, monitor);
}

/**
 * Called by the GC to do any required work at the end of monitor clearing
 *
 * @note A monitor must NOT be destroyed if the monitor is in use - if threads are waiting on
 * it, or if it is currently owned.
 *
 * @pre the caller must have VM access. 
 * 
 * @param[in] vm Java VM
 * @param[in] vmThread the J9VMThread calling this function
 */
void
objectMonitorDestroyComplete(J9JavaVM *vm, J9VMThread *vmThread)
{
	omrthread_monitor_flush_destroyed_monitor_list(vmThread->osThread);
}

