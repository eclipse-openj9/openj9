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

#if !defined(OBJECTMONITOR_HPP_)
#define OBJECTMONITOR_HPP_

#include "j9.h"
#ifdef J9VM_THR_LOCK_NURSERY
#include "lockNurseryUtil.h"
#endif
#include "j9modron.h"
#include "j9consts.h"
#include "vm_api.h"
#include "j9accessbarrier.h"
#include "monhelp.h"
#include "stackwalk.h"

#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

class VM_ObjectMonitor
{
/*
 * Data members
 */
private:

protected:

public:

/*
 * Function members
 */
private:

protected:

public:

	/**
	 * Return the address of the lockword for the given object.  It will fetch the object from
	 * the monitor table if it is not inlined in the object.
	 * 
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object from which to fetch the monitor
	 * 
	 * @return the the lockEA or NULL if the monitorTableAt returns NULL. 
	 * 		if lockEA is NULL then the current thread did not own the monitor
	 */
	static VMINLINE j9objectmonitor_t *
	inlineGetLockAddress(J9VMThread *currentThread, j9object_t object)
	{
		j9objectmonitor_t *lockEA = NULL;
#ifdef J9VM_THR_LOCK_NURSERY
		if (!LN_HAS_LOCKWORD(currentThread, object)) {
			J9ObjectMonitor *objectMonitor = J9_VM_FUNCTION(currentThread, monitorTableAt)(currentThread, object);
			if (NULL != objectMonitor) {
				lockEA = &(objectMonitor->alternateLockword);
			}
		} else
#endif
		{
			lockEA = J9OBJECT_MONITOR_EA(currentThread, object);
		}
		return lockEA;
	}

	/**
	 * Fetches the omrthread_monitor_t from an object for purposes of waiting.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object from which to fetch the monitor
	 * @param monitorPtr[out] pointer into which to store the omrthread_monitor_t
	 * @param checkOwner[in] should this function check that the current thread is the owner (default true)
	 *
	 * @returns	true if the returned monitor is valid for waiting
	 * 			false if the monitor is not valid for waiting
	 * 					if monitor is NULL, native memory allocation failed
	 * 					if monitor is not NULL, another thread owned this monitor
	 */
	static VMINLINE bool
	getMonitorForWait(J9VMThread *currentThread, j9object_t object, omrthread_monitor_t *monitorPtr, bool checkOwner = true)
	{
		bool wait = false;
		omrthread_monitor_t monitor = (omrthread_monitor_t)(UDATA)1; // invalid but non-NULL value
		j9objectmonitor_t lock = (j9objectmonitor_t)NULL;
		J9ObjectMonitor *objectMonitor = NULL;

#ifdef J9VM_THR_LOCK_NURSERY
		if (!LN_HAS_LOCKWORD(currentThread, object)) {
			objectMonitor = monitorTableAt(currentThread, object);
			if (objectMonitor == NULL) {
				/* Memory allocation failure */
				monitor = NULL;
				goto done;
			}
			lock = objectMonitor->alternateLockword;
		}
		else
#endif
		{
			lock = J9OBJECT_MONITOR(currentThread, object);
		}

		if (J9_LOCK_IS_INFLATED(lock)) {
			objectMonitor = J9_INFLLOCK_OBJECT_MONITOR(lock);
		} else {
			if (checkOwner) {
				if (currentThread != J9_FLATLOCK_OWNER(lock)) {
					goto done;
				}
#ifdef J9VM_THR_LOCK_RESERVATION
				if (OBJECT_HEADER_LOCK_RESERVED == (lock & (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_RESERVED))) {
					goto done;
				}
#endif
			}
			objectMonitor = objectMonitorInflate(currentThread, object, lock);
			if (objectMonitor == NULL) {
				/* Memory allocation failure */
				monitor = NULL;
				goto done;
			}
		}
		monitor = objectMonitor->monitor;
		wait = true;
done:
		*monitorPtr = monitor;
		return wait;
	}

	/**
	 * Fetches the omrthread_monitor_t from an object for purposes of notification.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object from which to fetch the monitor
	 * @param monitorPtr[out] pointer into which to store the omrthread_monitor_t
	 * @param checkOwner[in] should this function check that the current thread is the owner (default true)
	 *
	 * @returns	true if the returned monitor should be notified
	 * 			false if the monitor should not be notified
	 * 				if checkOwner is true,
	 * 					if monitor is NULL, this thread owns the monitor and there are no waiters
	 * 					if monitor is not NULL, another thread owned this monitor
	 */
	static VMINLINE bool
	getMonitorForNotify(J9VMThread *currentThread, j9object_t object, omrthread_monitor_t *monitorPtr, bool checkOwner = true)
	{
		bool notify = false;
		omrthread_monitor_t monitor = (omrthread_monitor_t)(UDATA)1; // invalid but non-NULL value
		j9objectmonitor_t lock = (j9objectmonitor_t)NULL;
		j9objectmonitor_t *lockEA = inlineGetLockAddress(currentThread, object);
		if (NULL == lockEA) {
			goto done;
		}

		lock = *lockEA;

		if ((lock & ~(j9objectmonitor_t)OBJECT_HEADER_LOCK_BITS_MASK) == (j9objectmonitor_t)(UDATA)currentThread) {
#if defined(J9VM_THR_LOCK_RESERVATION)
			if (checkOwner) {
				/* If the RESERVED bit is set, then the recursion count must be non-zero */
				if (OBJECT_HEADER_LOCK_RESERVED == (lock & (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_RESERVED))) {
					goto done;
				}
			}
#endif /* J9VM_THR_LOCK_RESERVATION */
			/* Current thread owns the monitor, but it's not inflated, no need to notify */
			monitor = NULL;
		} else {
			if (checkOwner) {
				if (0 == (lock & OBJECT_HEADER_LOCK_INFLATED)) {
					/* flat lock, but this thread is not owner */
					goto done;
				}
			}
			J9ObjectMonitor *objectMonitor = (J9ObjectMonitor*)(UDATA)(lock & ~(j9objectmonitor_t)OBJECT_HEADER_LOCK_INFLATED);
			monitor = objectMonitor->monitor;
			notify = true;
		}
done:
		*monitorPtr = monitor;
		return notify;
	}

	/**
	 * Perform a compare and swap operation on the lockword of an object.
	 *
	 * @param lockEA[in] the location of the lockword
	 * @param oldValue[in] the compare value
	 * @param newValue[in] the swap value
	 * @param readBeforeCAS[in] Controls whether a pre-read occurs before the CAS attempt (default false)
	 *
	 * @returns	the contents of the lockword before the CAS whether it succeeds or not
	 */
	static VMINLINE j9objectmonitor_t
	compareAndSwapLockword(j9objectmonitor_t volatile *lockEA, j9objectmonitor_t oldValue, j9objectmonitor_t newValue, bool readBeforeCAS = false)
	{
#if defined(OMR_GC_COMPRESSED_POINTERS)
		j9objectmonitor_t contents = VM_AtomicSupport::lockCompareExchangeU32(lockEA, oldValue, newValue, readBeforeCAS);
#else /* OMR_GC_COMPRESSED_POINTERS */
		j9objectmonitor_t contents = VM_AtomicSupport::lockCompareExchange(lockEA, oldValue, newValue, readBeforeCAS);
#endif /* OMR_GC_COMPRESSED_POINTERS */
		return contents;
	}

	/**
	 * Performs the most optimistic object monitor enter possible.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param lockEA[in] the location of the lockword
	 * @param readBeforeCAS[in] Controls whether a pre-read occurs before the CAS attempt (default false)
	 * @param lock[in] the value expected in lockEA when uncontended - either 0 or OBJECT_HEADER_LOCK_RESERVED
	 *
	 * @returns	true if the lock was acquired, false if not
	 */
	static VMINLINE bool
	inlineFastInitAndEnterMonitor(J9VMThread *currentThread, j9objectmonitor_t volatile *lockEA, bool readBeforeCAS = false, j9objectmonitor_t lock = 0)
	{
		bool locked = false;
		j9objectmonitor_t mine = (j9objectmonitor_t)(UDATA)currentThread;
		if (0 != lock) {
			/* Monitor is reservable. Make a reservation to give long-lived
			 * objects a chance even if first locked in the interpreter
			 */
			mine |= (lock | OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT);
		}
		if (lock == compareAndSwapLockword(lockEA, lock, mine, readBeforeCAS)) {
			VM_AtomicSupport::monitorEnterBarrier();
			locked = true;
		}
		return locked;
	}

	/**
	 * Performs the most optimistic object monitor enter possible.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object to lock
	 *
	 * @returns	true if the lock was acquired, false if not
	 */
	static VMINLINE bool
	inlineFastObjectMonitorEnter(J9VMThread *currentThread, j9object_t object)
	{
		bool locked = false;
#if defined(J9VM_THR_LOCK_NURSERY)
		if (LN_HAS_LOCKWORD(currentThread, object))
#endif /* J9VM_THR_LOCK_NURSERY */
		{
			locked = inlineFastInitAndEnterMonitor(currentThread, J9OBJECT_MONITOR_EA(currentThread, object));
		}
		return locked;
	}

	/**
	 * Performs the most optimistic object monitor exit possible.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object to lock
	 *
	 * @returns	true if the lock was released, false if not
	 */
	static VMINLINE bool
	inlineFastObjectMonitorExit(J9VMThread *currentThread, j9object_t object)
	{
		bool unlocked = false;
#if defined(J9VM_THR_LOCK_NURSERY)
		if (LN_HAS_LOCKWORD(currentThread, object))
#endif /* J9VM_THR_LOCK_NURSERY */
		{
			j9objectmonitor_t *lockEA = J9OBJECT_MONITOR_EA(currentThread, object);

			if ((j9objectmonitor_t)(UDATA)currentThread == *lockEA) {
				VM_AtomicSupport::writeBarrier();
				*lockEA = 0;
				unlocked = true;
			}
		}
		return unlocked;
	}

	/**
	 * Enters the monitor for an object.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object to lock
	 *
	 * @returns	the possibly-updated object pointer on success, or an error code
	 */
	static VMINLINE IDATA
	enterObjectMonitor(J9VMThread *currentThread, j9object_t object)
	{
		IDATA rc = (IDATA)object;
		if (!inlineFastObjectMonitorEnter(currentThread, object)) {
			rc = J9_VM_FUNCTION(currentThread, objectMonitorEnterNonBlocking)(currentThread, object);
			if (1 == rc) {
				rc = J9_VM_FUNCTION(currentThread, objectMonitorEnterBlocking)(currentThread);
			}
		}
		return rc;
	}

	/**
	 * Exits the monitor for an object.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object to lock
	 *
	 * @returns	0 on success, otherwise an error code
	 */
	static VMINLINE IDATA
	exitObjectMonitor(J9VMThread *currentThread, j9object_t object)
	{
		IDATA rc = 0;
		if (!inlineFastObjectMonitorExit(currentThread, object)) {
			rc = J9_VM_FUNCTION(currentThread, objectMonitorExit)(currentThread, object);
		}
		return rc;
	}

	/**
	 * Record a monitor entry for an object in a monitor enter
	 * records list.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object whose monitor was just entered
	 * @param framePointer[in] the frame pointer for the object
	 * @param recordList[in/out] pointer to the head of the monitor enter record list to update
	 *
	 * @returns	true on success, false if out of memory
	 */
	static VMINLINE bool
	recordMonitorEnter(J9VMThread *currentThread, j9object_t object, UDATA *framePointer, J9MonitorEnterRecord **recordList)
	{
		bool success = true;
		J9Pool *pool = currentThread->monitorEnterRecordPool;
		J9MonitorEnterRecord *enterRecord = *recordList;
		for (;;) {
			if (NULL == enterRecord) {
				/* Reached the end of the list - new record required */
				break;
			}
			if (enterRecord->arg0EA != framePointer) {
				/* Passed the current frame (list is ordered by frame) - new record required */
				break;
			}
			if (enterRecord->object == object) {
				/* Existing record for object found, increment count */
				enterRecord->dropEnterCount += 1;
				goto done;
			}
			enterRecord = enterRecord->next;
		}
		enterRecord = (J9MonitorEnterRecord*)pool_newElement(pool);
		if (J9_UNEXPECTED(NULL == enterRecord)) {
			success = false;
			goto done;
		}
		enterRecord->object = object;
		enterRecord->dropEnterCount = 1;
		enterRecord->arg0EA = framePointer;
		enterRecord->next = *recordList;
		*recordList = enterRecord;
done:
		return success;
	}

	/**
	 * Record a bytecoded monitor entry for an object in the monitor enter
	 * records list.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object whose monitor was just entered
	 * @param arg0EA[in] the current arg0EA
	 *
	 * @returns	true on success, false if out of memory
	 */
	static VMINLINE bool
	recordBytecodeMonitorEnter(J9VMThread *currentThread, j9object_t object, UDATA *arg0EA)
	{
		return recordMonitorEnter(currentThread, object, CONVERT_TO_RELATIVE_STACK_OFFSET(currentThread, arg0EA), &currentThread->monitorEnterRecords);
	}

	/**
	 * Record a JNI monitor entry for an object in the monitor enter
	 * records list.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object whose monitor was just entered
	 *
	 * @returns	true on success, false if out of memory
	 */
	static VMINLINE bool
	recordJNIMonitorEnter(J9VMThread *currentThread, j9object_t object)
	{
		return recordMonitorEnter(currentThread, object, NULL, &currentThread->jniMonitorEnterRecords);
	}

	/**
	 * Record a monitor exit for an object in a monitor enter
	 * records list.
	 *
	 * Assumes that the list is valid, i.e. the first time the object
	 * is found in the list, that record is the correct one.  If no
	 * record for the object is found, nothing happens.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object whose monitor was just exited
	 * @param recordList[in/out] pointer to the head of the monitor enter record list to update
	 */
	static VMINLINE void
	recordMonitorExit(J9VMThread *currentThread, j9object_t object, J9MonitorEnterRecord **recordList)
	{
		J9Pool *pool = currentThread->monitorEnterRecordPool;
		for (;;) {
			J9MonitorEnterRecord *enterRecord = *recordList;
			if (J9_UNEXPECTED(NULL == enterRecord)) {
				break;
			}
			if (enterRecord->object == object) {
				if (0 == --(enterRecord->dropEnterCount)) {
					J9MonitorEnterRecord* next = enterRecord->next;
					pool_removeElement(pool, enterRecord);
					*recordList = next;
				}
				break;
			}
			recordList = &enterRecord->next;
		}
	}

	/**
	 * Record a bytecoded monitor exit for an object in the monitor enter
	 * records list.
	 *
	 * Assumes that the list is valid, i.e. the first time the object
	 * is found in the list, that record is the correct one.  If no
	 * record for the object is found, nothing happens.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object whose monitor was just exited
	 */
	static VMINLINE void
	recordBytecodeMonitorExit(J9VMThread *currentThread, j9object_t object)
	{
		recordMonitorExit(currentThread, object, &currentThread->monitorEnterRecords);
	}

	/**
	 * Record a JNI monitor exit for an object in the monitor enter
	 * records list.
	 *
	 * Assumes that the list is valid, i.e. the first time the object
	 * is found in the list, that record is the correct one.  If no
	 * record for the object is found, nothing happens.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object whose monitor was just exited
	 */
	static VMINLINE void
	recordJNIMonitorExit(J9VMThread *currentThread, j9object_t object)
	{
		recordMonitorExit(currentThread, object, &currentThread->jniMonitorEnterRecords);
	}

};

#endif /* OBJECTMONITOR_HPP_ */
