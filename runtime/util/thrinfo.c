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

/* #define J9VM_DBG */

#if defined(J9VM_OUT_OF_PROCESS)
#include "j9dbgext.h"
#define READU(field) dbgReadUDATA((UDATA *)&(field))
#define READU_ADDR(fieldAddr) dbgReadUDATA((UDATA *)(fieldAddr))
#define READP(field) ((void *)READU(field))
#define READP_ADDR(fieldAddr) ((void *)READU_ADDR(fieldAddr))
#if defined(OMR_GC_COMPRESSED_POINTERS)
#define READMON(field) dbgReadU32((U_32 *)&(field))
#define READMON_ADDR(fieldAddr) dbgReadU32((U_32 *)(fieldAddr))
#else
#define READMON(field) READU(field)
#define READMON_ADDR(fieldAddr) READU_ADDR(fieldAddr)
#endif
#define READCLAZZ(vmThread, obj) readObjectClass(vmThread, obj)
#define LOCAL_TO_TARGET(addr) dbgLocalToTarget(addr)

/* This explicit conversion breaks the GC abstraction. Don't use this for in-process code. */
#if defined(OMR_GC_COMPRESSED_POINTERS)
#define J9OBJECT_FROM_FJ9OBJECT(vm, fobj) ((j9object_t)(((UDATA)(fobj)) << (vm)->compressedPointersShift))
#else
#define J9OBJECT_FROM_FJ9OBJECT(vm, fobj) ((j9object_t)(UDATA)(fobj))
#endif

#else /* not J9VM_OUT_OF_PROCESS */
#define READU(field) (field)
#define READU_ADDR(fieldAddr) (*(UDATA *)(fieldAddr))
#define READP(field) (field)
#define READP_ADDR(fieldAddr) (*(fieldAddr))
#define READMON(field) ((j9objectmonitor_t) (field))
#define READMON_ADDR(fieldAddr) (*(j9objectmonitor_t *) (fieldAddr))
#define READCLAZZ(vmThread, obj) J9OBJECT_CLAZZ((vmThread), (obj))
#define LOCAL_TO_TARGET(addr) (addr)
#endif

#include <string.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "omrthread.h"
#ifdef J9VM_THR_LOCK_NURSERY
#include "lockNurseryUtil.h"
#endif
#include "util_internal.h"
#include "ut_j9vmutil.h"
#include "monhelp.h"
#if defined(J9VM_OUT_OF_PROCESS)

/* include the debug extension prototypes if we are being recompiled in dbgext/ */
#include "dbggen.h"

#ifndef J9VM_THR_LOCK_NURSERY
/* The J9OBJECT_MONITOR_EA macro doesn't currently work OOP on Metronome VMs */
#undef J9OBJECT_MONITOR_EA
#define J9OBJECT_MONITOR_EA(token,obj) (&TMP_J9OBJECT_MONITOR(obj))
#endif

#endif /* J9VM_OUT_OF_PROCESS */


#define INCLUDE_RAW_MONITORS		TRUE
#define DONT_INCLUDE_RAW_MONITORS	FALSE


#define J9THREAD_MONITOR_FROM_LOCKWORD(lockWord) ((J9ThreadAbstractMonitor *)READU(J9_INFLLOCK_OBJECT_MONITOR(lockWord)->monitor))

static j9objectmonitor_t getLockWord(J9VMThread *vmThread, j9object_t object);
static UDATA getVMThreadStateHelper(J9VMThread *targetThread, 
		j9object_t *pLockObject, omrthread_monitor_t *pRawLock,
		J9VMThread **pLockOwner, UDATA *pCount, BOOLEAN includeRawMonitors);
static void getInflatedMonitorState(const J9VMThread *targetThread,
		const omrthread_t j9self, const omrthread_state_t *j9state,
		UDATA *vmstate, omrthread_monitor_t *rawLock,
		J9VMThread **lockOwner, UDATA *count);
#if defined(J9VM_OUT_OF_PROCESS)
static U_64 readObjectField(j9object_t object, J9Class *clazz, U_8 *fieldName, U_8 *fieldSig, UDATA fieldBytes);
static J9Class *readObjectClass(J9VMThread *vmtoken, j9object_t object);
#endif

#if defined(J9VM_OUT_OF_PROCESS)
/*
 * WrappedJ9ObjectMonitor and the hashWrappedMonitorHash and hashWrappedMonitorCompare functions are used
 * to create an in-memory copy of the monitor table when we are running out of process
 */

#define TAGGED_MONITOR_TABLE 0x1

typedef struct WrappedJ9ObjectMonitor {
	J9ObjectMonitor* objectMonitor;
	J9Object*        object;
} WrappedJ9ObjectMonitor;

static UDATA
hashWrappedMonitorHash(void *key, void *userData)
{
	return ((UDATA) ((WrappedJ9ObjectMonitor *) key )->object );
}

static UDATA
hashWrappedMonitorCompare(void *leftKey, void *rightKey, void *userData)
{
	return (((WrappedJ9ObjectMonitor *) leftKey )->object == ((WrappedJ9ObjectMonitor *) rightKey )->object);
}
#endif /*defined(J9VM_OUT_OF_PROCESS)*/

/**
 * CMVC 135066
 * getVMThreadObjectStatesAll() returns the thread states in a bit mask that is
 * compliant with JVMTI. Blocked/waiting/running J9VMTHREAD_STATE_XXX states can
 * be combined with J9VMTHREAD_STATE_INTERRUPTED or SUSPENDED.
 * However, users of getVMThreadObjectState() expect all the J9VMTHREAD_STATE_XXX
 * states to be mutually exclusive.
 * Therefore, this method calls getVMThreadObjectStatesAll() but:
 * - ignores the J9VMTHREAD_STATE_INTERRUPTED bit
 * - makes J9VMTHREAD_STATE_SUSPENDED mutually exclusive of other states
 * @deprecated
 */
UDATA 
getVMThreadObjectState(J9VMThread *targetThread,
		j9object_t *pLockObject, J9VMThread **pLockOwner, UDATA *pCount)
{
	UDATA rc = getVMThreadObjectStatesAll(targetThread, pLockObject, pLockOwner, pCount);

	/* hack out bits that aren't supported by users of this function */
	rc &= ~J9VMTHREAD_STATE_INTERRUPTED; 
	if (rc & J9VMTHREAD_STATE_SUSPENDED) {
		rc = J9VMTHREAD_STATE_SUSPENDED;
		if (pLockObject) {
			*pLockObject = NULL;
		}
		if (pLockOwner) {
			*pLockOwner = NULL;
		}
		if (pCount) {
			*pCount = 0;
		}
	}
	
	return rc; 
}

UDATA 
getVMThreadObjectStatesAll(J9VMThread *targetThread,
		j9object_t *pLockObject, J9VMThread **pLockOwner, UDATA *pCount)
{
	return getVMThreadStateHelper(targetThread, pLockObject, NULL, pLockOwner,
			pCount, DONT_INCLUDE_RAW_MONITORS);
}

/**
 * CMVC 135066
 * getVMThreadRawStatesAll() returns the thread states in a bit mask that is
 * compliant with JVMTI. Blocked/waiting/running J9VMTHREAD_STATE_XXX states can
 * be combined with J9VMTHREAD_STATE_INTERRUPTED or SUSPENDED.
 * However, users of getVMThreadRawState() expect all the J9VMTHREAD_STATE_XXX
 * states to be mutually exclusive.
 * Therefore, this method calls getVMThreadObjectStatesAll() but:
 * - ignores the J9VMTHREAD_STATE_INTERRUPTED bit
 * - makes J9VMTHREAD_STATE_SUSPENDED mutually exclusive of other states
 * @deprecated
 */
UDATA
getVMThreadRawState(J9VMThread *targetThread,
		j9object_t *pLockObject, omrthread_monitor_t *pRawMonitor, 
		J9VMThread **pLockOwner, UDATA *pCount) 
{
	UDATA rc = getVMThreadRawStatesAll(targetThread, pLockObject, pRawMonitor, pLockOwner, pCount);

	/* hack out bits that aren't supported by users of this function */
	rc &= ~J9VMTHREAD_STATE_INTERRUPTED; 
	if (rc & J9VMTHREAD_STATE_SUSPENDED) {
		rc = J9VMTHREAD_STATE_SUSPENDED;
		if (pLockObject) {
			*pLockObject = NULL;
		}
		if (pRawMonitor) {
			*pRawMonitor = NULL;
		}
		if (pLockOwner) {
			*pLockOwner = NULL;
		}
		if (pCount) {
			*pCount = 0;
		}
	}
	
	return rc; 
}

UDATA
getVMThreadRawStatesAll(J9VMThread *targetThread,
		j9object_t *pLockObject, omrthread_monitor_t *pRawMonitor, 
		J9VMThread **pLockOwner, UDATA *pCount) 
{
	return getVMThreadStateHelper(targetThread, pLockObject, pRawMonitor, pLockOwner,
			pCount, INCLUDE_RAW_MONITORS);
}

/*
 * Determine the state of a thread.
 * Results are undefined if the thread is not suspended.
 * The caller may pass NULL for parameters it is not interested in.
 *
 * If (includeRawMonitors == FALSE), ignore raw monitors (i.e. non-object monitors).
 * Threads that are blocked on raw monitors will be reported as RUNNING.
 *
 * Returns one of:
 *   J9VMTHREAD_STATE_RUNNING
 *   J9VMTHREAD_STATE_BLOCKED
 *   J9VMTHREAD_STATE_WAITING
 * 	 J9VMTHREAD_STATE_WAITING_TIMED
 *   J9VMTHREAD_STATE_PARKED
 *   J9VMTHREAD_STATE_PARKED_TIMED
 *   J9VMTHREAD_STATE_SLEEPING
 *   J9VMTHREAD_STATE_DEAD
 *   J9VMTHREAD_STATE_UNKNOWN
 * And if interrupted or suspended, combined with:
 *   J9VMTHREAD_STATE_INTERRUPTED
 *   J9VMTHREAD_STATE_SUSPENDED
 * 
 * Depending on the thread's state, the returned parameters have slightly
 * different interpretations.
 *
 * UNKNOWN/RUNNING/SLEEPING/DEAD
 * lockObject = NULL
 * rawLock = NULL
 * lockOwner = NULL
 * count = 0
 *
 * PARKED/PARKED_TIMED
 * lockObject = The parkBlocker object specified via LockSupport.park(parkBlocker)
 * rawLock = NULL
 * lockOwner = parkBlocker.getExclusiveOwnerThread(), if parkBlocker is an AbstractOwnableSynchronizer.
 * 		NULL otherwise.
 * count = 0
 *
 * BLOCKED/WAITING/WAITING_TIMED on an OBJECT monitor
 * lockObject = the blocking object
 * rawLock = the fat monitor associated with the blocking object
 * lockOwner = the thread that owns the object monitor
 * count = the lockOwner's recursion count.
 *
 * If the object monitor is not inflated, the owner and count reflect
 * the owner and count of the flat lock, not of the associated fat lock.
 *
 * BLOCKED/WAITING/WAITING_TIMED on a RAW monitor
 * lockObject = NULL
 * rawLock = the raw monitor 
 * lockOwner = the thread that owns the raw monitor
 * count = the lockOwner's recursion count.
 *
 * For BLOCKED/WAITING/WAITING_TIMED threads, lockOwner may be NULL if the owner of the
 * blocking monitor is unattached.
 *
 * count may be inaccurate if the lockOwner thread is not also suspended.
 * 
 * Out-of-process notes:
 *    Input targetThread is a LOCAL pointer to a J9VMThread.
 *    It must be bound to a LOCAL J9JavaVM.
 *    Output *pLockObject, *pRawLock and *pLockOwner are TARGET pointers
 * 
 */
static UDATA
getVMThreadStateHelper(J9VMThread *targetThread,
		j9object_t *pLockObject, omrthread_monitor_t *pRawLock,
		J9VMThread **pLockOwner, UDATA *pCount, 
		BOOLEAN includeRawMonitors)
{
	UDATA vmstate = J9VMTHREAD_STATE_UNKNOWN;
	j9object_t lockObject = NULL;
	omrthread_monitor_t rawLock = NULL;
	J9VMThread *lockOwner = NULL;
	UDATA count = 0;
	UDATA publicFlags;
	omrthread_t j9self;
	omrthread_state_t j9state;
	
	if (targetThread) {
		vmstate = J9VMTHREAD_STATE_RUNNING;
		publicFlags = targetThread->publicFlags;
		j9self = targetThread->osThread;
		/* j9self may be NULL if this function is used by RAS on a corrupt VM */
		
		if (j9self) {
			omrthread_get_state(j9self, &j9state);
		} else {
			memset(&j9state, 0, sizeof(j9state));
		}
		
		if (publicFlags & (J9_PUBLIC_FLAGS_THREAD_BLOCKED | J9_PUBLIC_FLAGS_THREAD_WAITING)) {
			j9objectmonitor_t lockWord;
			
			Assert_VMUtil_true(targetThread->blockingEnterObject != NULL);
	
			lockObject = targetThread->blockingEnterObject;
			lockWord = getLockWord(targetThread, lockObject);
	
			if (J9_LOCK_IS_INFLATED(lockWord)) {
				J9ThreadAbstractMonitor *objmon = getInflatedObjectMonitor(targetThread->javaVM, targetThread, lockObject, lockWord);

				/*
				 * If the monitor is out-of-line and NULL, then it was never entered,
				 * so the target thread is still runnable.
				 */

				if (objmon) {
					omrthread_t j9owner;
					
					j9owner = READP(objmon->owner);
					count = READU(objmon->count);
					
					if (publicFlags & J9_PUBLIC_FLAGS_THREAD_BLOCKED) {
						if (j9owner && (j9owner != j9self)) {
							/* 
							 * The omrthread may be accessing other raw monitors, but
							 * the vmthread is blocked while the object monitor is
							 * owned by a competing thread.
							 */
							vmstate = J9VMTHREAD_STATE_BLOCKED;
							lockOwner = getVMThreadFromOMRThread(targetThread->javaVM, j9owner);
							rawLock = (omrthread_monitor_t)objmon;
						}
					} else {
						if (!j9self) {
							if (publicFlags & J9_PUBLIC_FLAGS_THREAD_TIMED) {
								vmstate = J9VMTHREAD_STATE_WAITING_TIMED;
							} else {
								vmstate = J9VMTHREAD_STATE_WAITING;
							}
							lockOwner = getVMThreadFromOMRThread(targetThread->javaVM, j9owner);
							rawLock = (omrthread_monitor_t)objmon;
							
						} else if ((omrthread_monitor_t)objmon == j9state.blocker) {
							getInflatedMonitorState(targetThread, j9self, &j9state,
									&vmstate, &rawLock, &lockOwner, &count);
						}
						/* 
						 * If the omrthread is using a different monitor, it must be for vm access.
						 * So the vmthread is either not waiting yet or already awakened.
						 */
					}
				}
	
			} else {
				/* 
				 * Can't wait on an uninflated object monitor, so the thread
				 * must be blocked.
				 */
				Assert_VMUtil_true(publicFlags & J9_PUBLIC_FLAGS_THREAD_BLOCKED);
				
				lockOwner = (J9VMThread *)J9_FLATLOCK_OWNER(lockWord);
	
				if (lockOwner && (lockOwner != LOCAL_TO_TARGET(targetThread))) {
					count = J9_FLATLOCK_COUNT(lockWord);
					rawLock = (omrthread_monitor_t)monitorTablePeekMonitor(targetThread->javaVM, targetThread, lockObject);
					vmstate = J9VMTHREAD_STATE_BLOCKED;
				}
			}
			/* 
			 * targetThread may be blocked attempting to reacquire VM access, after 
			 * succeeding to acquire the object monitor. In this case, the returned 
			 * vmstate depends on includeRawMonitors.
			 * includeRawMonitors == FALSE: the vmstate is RUNNING.
			 * includeRawMonitors == TRUE: the vmstate depends on the state of
			 * the omrthread. e.g. The omrthread may be blocked on publicFlagsMutex.
			 */
			
		} else if (publicFlags & J9_PUBLIC_FLAGS_THREAD_PARKED) {
			/* if the osthread is not parked, then the thread is runnable */
			if (!j9self || (j9state.flags & J9THREAD_FLAG_PARKED)) {
				lockObject = targetThread->blockingEnterObject;
				if (publicFlags & J9_PUBLIC_FLAGS_THREAD_TIMED) {
					vmstate = J9VMTHREAD_STATE_PARKED_TIMED;
				} else {
					vmstate = J9VMTHREAD_STATE_PARKED;
				}
#if defined(J9VM_OPT_SIDECAR)
				/* If lockObject is an abstract ownable synchronizer, get its owner. */
				if (lockObject) {
					J9Class *aosClazz;
					J9Class *clazz;
					j9object_t lockOwnerObject = NULL;
					
					aosClazz = J9VMJAVAUTILCONCURRENTLOCKSABSTRACTOWNABLESYNCHRONIZER_OR_NULL(targetThread->javaVM);
					/* skip this step if aosClazz doesn't exist */
					if (aosClazz) {
						clazz = READCLAZZ(targetThread, lockObject);

						/* PR 80305 : Do not write back to the castClassCache as this code may be running while the GC is unloading the class */
						if (instanceOfOrCheckCastNoCacheUpdate(clazz, aosClazz)) {
#if defined(J9VM_OUT_OF_PROCESS)
							{
								U_64 fobj;

								/* don't cast to fj9object_t to avoid compiler type width warnings */
								fobj = readObjectField(lockObject, clazz, "exclusiveOwnerThread", "Ljava/lang/Thread;", sizeof(fj9object_t));
								lockOwnerObject = J9OBJECT_FROM_FJ9OBJECT(targetThread->javaVM, fobj);
							}
#else
							/* Simplify the macro usage by using the _VM version so we do not need to pass in the current thread */
							lockOwnerObject =
								J9VMJAVAUTILCONCURRENTLOCKSABSTRACTOWNABLESYNCHRONIZER_EXCLUSIVEOWNERTHREAD_VM(targetThread->javaVM, lockObject);
#endif
							if (lockOwnerObject) {
#if defined(J9VM_OUT_OF_PROCESS)
								lockOwner = (J9VMThread *)(UDATA)readObjectField(lockOwnerObject, READCLAZZ(targetThread, lockOwnerObject), "threadRef", "J", sizeof(jlong));
#else
								lockOwner = (J9VMThread *)J9VMJAVALANGTHREAD_THREADREF_VM(targetThread->javaVM, lockOwnerObject);
#endif
							}
						}
					}
				}
#endif /* defined(J9VM_OPT_SIDECAR) */
			}
				
		} else if (publicFlags & J9_PUBLIC_FLAGS_THREAD_SLEEPING) {
			/* if the osthread is not sleeping, then the thread is runnable */
			if (!j9self || (j9state.flags & J9THREAD_FLAG_SLEEPING)) {
				vmstate = J9VMTHREAD_STATE_SLEEPING;
			}

		} else { 
			/* no vmthread flags apply, so go through the omrthread flags */
			if (!j9self) {
				vmstate = J9VMTHREAD_STATE_UNKNOWN;
			} else if (j9state.flags & J9THREAD_FLAG_PARKED) {
				if (j9state.flags & J9THREAD_FLAG_TIMER_SET) {
					vmstate = J9VMTHREAD_STATE_PARKED_TIMED;
				} else {
					vmstate = J9VMTHREAD_STATE_PARKED;
				}
			} else if (j9state.flags & J9THREAD_FLAG_SLEEPING) {
				vmstate = J9VMTHREAD_STATE_SLEEPING;
			} else if (j9state.flags & J9THREAD_FLAG_DEAD) {
				vmstate = J9VMTHREAD_STATE_DEAD;
			} 
		}
		
		if (J9VMTHREAD_STATE_RUNNING == vmstate) {
			if (includeRawMonitors) {
				/* check if the omrthread is blocked/waiting on a raw monitor */
				lockObject = NULL;
				getInflatedMonitorState(targetThread, j9self, &j9state, &vmstate,
						&rawLock, &lockOwner, &count);
			}
		}
		
		/* sanitize irrelevant parameters */
		if ((J9VMTHREAD_STATE_RUNNING == vmstate) 
				|| (J9VMTHREAD_STATE_SUSPENDED == vmstate)
				|| (J9VMTHREAD_STATE_UNKNOWN == vmstate)) {
			lockObject = NULL;
			rawLock = NULL;
			lockOwner = NULL;
			count = 0;
		}
		
		if (rawLock && pLockObject && (!lockObject)) {
			if ((READU(((J9ThreadAbstractMonitor *)rawLock)->flags) & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT) {
				lockObject = (j9object_t)READP(((J9ThreadAbstractMonitor *)rawLock)->userData);
			}
		}

		/*
		 * Refer to the JVMTI docs for Get Thread State.
		 * INTERRUPTED and SUSPENDED are not mutually exclusive with the other states.
		 */
		/* j9state was zeroed if j9self is NULL */
		if (j9state.flags & J9THREAD_FLAG_INTERRUPTED) {
			vmstate |= J9VMTHREAD_STATE_INTERRUPTED;
		}
		if (j9state.flags & J9THREAD_FLAG_SUSPENDED) {
			vmstate |= J9VMTHREAD_STATE_SUSPENDED;
		}
		if (FALSE == includeRawMonitors) {
			/* 
			 * For compatibility with getVMThreadStatus(), ignore this flag if
			 * raw state is requested. 
			 */
			if (publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND) {
				vmstate |= J9VMTHREAD_STATE_SUSPENDED;
			}
		}
	} /* if (targetThread) */

	if (pLockObject) {
		*pLockObject = lockObject;
	}
	if (pLockOwner) {
		*pLockOwner = lockOwner;
	}
	if (pRawLock) {
		*pRawLock = rawLock;
	}
	if (pCount) {
		*pCount = count;
	}
	return vmstate;
}

/*
 * THIS FUNCTION IS OBSOLETE.
 * Use getVMThreadObjectState() or getVMThreadRawState() instead.
 *
 * Determine the status of the specified thread.
 * The thread should be suspended or the results may not be accurate.
 * If the thread is blocked or waiting, return the monitor it's blocked on, as well
 * as the owner of the monitor and the count. 
 * If the thread is blocked on a flat lock, then the owner and count will reflect 
 * the owner and count of the flat lock, not of the associated fat lock. 
 * If the thread is not blocked or waiting, the return values will all be NULL or 0.
 * If the owner of the blocking monitor is unattached, then the return value in
 * powner will be NULL. The osthread which actually owns the monitor can be
 * determined by inspecting the monitor.
 *
 * Returns one of:
 *   J9VMTHREAD_STATE_RUNNING
 *   J9VMTHREAD_STATE_BLOCKED
 *   J9VMTHREAD_STATE_WAITING
 * 	 J9VMTHREAD_STATE_WAITING_TIMED
 *   J9VMTHREAD_STATE_SLEEPING
 *   J9VMTHREAD_STATE_SUSPENDED
 *
 * Out-of-process notes:
 *    Input vmStruct is a LOCAL pointer to a J9VMThread. It must be bound to a LOCAL J9JavaVM
 *    Output *pmonitor and *powner are TARGET pointers
 */
UDATA 
getVMThreadStatus_DEPRECATED(J9VMThread* thread, J9ThreadAbstractMonitor** pmonitor, J9VMThread** powner, UDATA* pcount)
{
	omrthread_monitor_t blockingMonitor = NULL;
	j9object_t blockingEnterObject = thread->blockingEnterObject;
	J9VMThread* owner = NULL;
	UDATA count = 0;
	UDATA status = J9VMTHREAD_STATE_RUNNING;

	Trc_VMUtil_getVMThreadStatus_Entry(thread, pmonitor, powner, pcount);

	if (blockingEnterObject) {
		/* This function assumes that blockingEnterObject is set only if the
		 * thread is blocked on an object monitor.
		 * VMDesign 1231 changed this, so compensate here.
		 */
		if (!(thread->publicFlags & J9_PUBLIC_FLAGS_THREAD_BLOCKED)) {
			blockingEnterObject = NULL;
		}
	}

	if (blockingEnterObject) {
		j9objectmonitor_t lockWord = getLockWord(thread, blockingEnterObject);

		if (J9_LOCK_IS_INFLATED(lockWord)) {
			J9ThreadAbstractMonitor *monitorStruct = getInflatedObjectMonitor(thread->javaVM, thread, blockingEnterObject, lockWord);
			omrthread_t j9ThreadOwner = NULL;

			/*
			 * If the monitor is out-of-line and NULL, then no one has attempted to enter it yet,
			 * so the target thread is still runnable.
			 */
			if (monitorStruct) {
				j9ThreadOwner = (omrthread_t)READU(monitorStruct->owner);
				blockingMonitor = (omrthread_monitor_t)monitorStruct;
				status = J9VMTHREAD_STATE_BLOCKED;
			}

			if ( j9ThreadOwner != NULL ) {
				owner = getVMThreadFromOMRThread(thread->javaVM, j9ThreadOwner);
				/*
				 * Special case where the thread being queried has released VM access, dropped to the thread lib
				 * to take ownership of a monitor, acquired ownership of the monitor, but hasn't set blockingEnterObject 
				 * back to NULL yet... All while another thread has exclusive VM access.
				 * See CMVC 99377
				 */
				if (owner == thread) {
					blockingMonitor = NULL;
					owner = NULL;
					status = J9VMTHREAD_STATE_RUNNING;
				} else {
					count = READU(monitorStruct->count);
				}
			} else {
				/*
				 * The Java thread being examined is in a transitional state.
				 * It couldn't take non-blocking ownership of the monitor, but while in the process of 
				 * doing the blocking call to take ownership, the monitor was released by the owner.
				 * The java thread may or may not soon block, but right now it's still running.
				 */
				blockingMonitor = NULL;
				owner = NULL;
				status = J9VMTHREAD_STATE_RUNNING;
			}
		} else {
			/* 
			 * Not an inflated monitor
			 */
			blockingMonitor = (omrthread_monitor_t)monitorTablePeekMonitor(thread->javaVM, thread, blockingEnterObject);
			owner = J9_FLATLOCK_OWNER(lockWord);
			count = J9_FLATLOCK_COUNT(lockWord);
			status = J9VMTHREAD_STATE_BLOCKED;
		} 
	} else {
		/* 
		 * No blockingEnterObject
		 */
		UDATA flags = omrthread_get_flags(thread->osThread, &blockingMonitor);
		if (blockingMonitor) {
			omrthread_t osOwner = (omrthread_t)READU(((J9ThreadAbstractMonitor*)blockingMonitor)->owner);
			count = READU(((J9ThreadAbstractMonitor*)blockingMonitor)->count);
			owner = getVMThreadFromOMRThread(thread->javaVM, osOwner);
		}

		/* 
		 * CMVC 109614.
		 * Because blockingMonitor and blockingMonitor->owner are not read together atomically,
		 * some inconsistent "blocked/waiting on self" states can occur. These are checked and
		 * described below.
		 */
		if (flags & J9THREAD_FLAG_BLOCKED) {
			if (owner == thread) {
				/* 
				 * CMVC 109614.
				 * (3-tier) The thread is in monitor_enter_three_tier() (via monitor_wait()).
				 * It has acquired the spinlock and marked itself as the monitor's owner, 
				 * but has not yet cleared its BLOCKED flag.
				 * The thread is RUNNABLE in this case.
				 * 
				 * (non 3-tier) In monitor_wait(), thread->flags and thread->monitor were read 
				 * after the thread was notified, but monitor->owner was not read until
				 * after the thread has marked itself as the monitor's owner.
				 * The thread is RUNNABLE in this case.
				 * This can also happen in monitor_enter(), but blockingEnterObject should be non-NULL
				 * in this case, so it should be handled above.
				 */
				 owner = NULL;
				 blockingMonitor = NULL;
				 status = J9VMTHREAD_STATE_RUNNING;
			} else {
				status = J9VMTHREAD_STATE_BLOCKED;
			}
		} else if (flags & J9THREAD_FLAG_WAITING) {
			if (owner == thread) {
				/* 
				 * CMVC 109614.
				 * The thread is in monitor_wait() and preparing to wait, but has not yet released the monitor. 
				 * The thread is still RUNNABLE in this case.
				 */
				owner = NULL;
				blockingMonitor = NULL;
				status = J9VMTHREAD_STATE_RUNNING;
			} else {
				if (flags & J9THREAD_FLAG_TIMER_SET) {
					status = J9VMTHREAD_STATE_WAITING_TIMED;
				} else {
					status = J9VMTHREAD_STATE_WAITING;
				}
			}
		} else if (flags & J9THREAD_FLAG_SLEEPING) {
			status = J9VMTHREAD_STATE_SLEEPING;
		} else if (flags & J9THREAD_FLAG_SUSPENDED) {
			status = J9VMTHREAD_STATE_SUSPENDED;
		} else if (flags & J9THREAD_FLAG_DEAD) {
			status = J9VMTHREAD_STATE_DEAD;
		} 
	}

	if (pmonitor) {
		*pmonitor = (J9ThreadAbstractMonitor*)blockingMonitor;
	}
	if (powner) {
		*powner = owner;
	}
	if (pcount) {
		*pcount = count;
	}

	Trc_VMUtil_getVMThreadStatus_Exit(status, blockingMonitor, owner, count);

	return status;
}

static void
getInflatedMonitorState(const J9VMThread *targetThread, const omrthread_t j9self,
		const omrthread_state_t *j9state, UDATA *vmstate,
		omrthread_monitor_t *rawLock, J9VMThread **lockOwner, UDATA *count)
{
	*vmstate = J9VMTHREAD_STATE_RUNNING;

	if (!j9self) {
		*vmstate = J9VMTHREAD_STATE_UNKNOWN;
		
	} else if (j9state->flags & J9THREAD_FLAG_BLOCKED) {
		/* Check for BLOCKED before WAITING to catch waiting threads that
		 * have been notified.
		 */
		if (j9state->owner && (j9state->owner != j9self)) {
			*lockOwner = getVMThreadFromOMRThread(targetThread->javaVM, j9state->owner);
			*count = j9state->count;
			*rawLock = j9state->blocker;
			*vmstate = J9VMTHREAD_STATE_BLOCKED;
		}
	} else if (j9state->flags & J9THREAD_FLAG_WAITING) {
		/* The blocking object of a waiting thread need not be owned. */
		if (j9state->owner != j9self) {
			if (j9state->owner) {
				*lockOwner = getVMThreadFromOMRThread(targetThread->javaVM, j9state->owner);
				*count = j9state->count;
			} else {
				*lockOwner = NULL;
				*count = 0;
			}
			*rawLock = j9state->blocker;
			if (j9state->flags & J9THREAD_FLAG_TIMER_SET) {
				*vmstate = J9VMTHREAD_STATE_WAITING_TIMED;
			} else {
				*vmstate = J9VMTHREAD_STATE_WAITING;
			}
		}
	}
}


/**
 * Get the inflated monitor corresponding to an object, if it exists.
 * The inflated monitor is usually stored in the object lockword, but
 * this function may need to look up the monitor in vm->monitorTable.
 * 
 * This function may block on vm->monitorTableMutex.
 * This function can work out-of-process.
 * 
 * @pre The object monitor must be inflated.
 * 
 * @param[in] vm the JavaVM. For out-of-process: may be a local or target pointer. 
 * vm->monitorTable must be a target value.
 * @param[in] targetVMThread the target J9VMThread.
 * @param[in] object the object. For out-of-process: a target pointer.
 * @param[in] lockWord The object's lockword.
 * @returns a J9ThreadAbstractMonitor 
 * @retval NULL There is no inflated object monitor.
 * 
 * @see monitorTablePeekMonitor, monitorTablePeek
 */
J9ThreadAbstractMonitor *
getInflatedObjectMonitor(J9JavaVM *vm, J9VMThread *targetVMThread, j9object_t object, j9objectmonitor_t lockWord)
{
	return J9THREAD_MONITOR_FROM_LOCKWORD(lockWord);
}

static j9objectmonitor_t
getLockWord(J9VMThread *vmThread, j9object_t object)
{
	j9objectmonitor_t lockWord;
#ifdef J9VM_THR_LOCK_NURSERY

	if (LN_HAS_LOCKWORD(vmThread,object)) {
		lockWord = READMON_ADDR(J9OBJECT_MONITOR_EA_SAFE(vmThread, object));
	} else {
		J9ObjectMonitor *objectMonitor = monitorTablePeek(vmThread->javaVM, vmThread, object);
		if (objectMonitor != NULL){
			lockWord = objectMonitor->alternateLockword;
		} else {
			lockWord = 0;
		}
	}
#else
	lockWord = READMON_ADDR(J9OBJECT_MONITOR_EA(vmThread, object));
#endif

	return lockWord;
}

/**
 * Search vm->monitorTable for the inflated monitor corresponding to an object.
 * Similar to monitorTableAt(), but doesn't add the monitor if it isn't found in the hashtable.
 * 
 * This function may block on vm->monitorTableMutex.
 * This function can work out-of-process.
 * 
 * @param[in] vm the JavaVM. For out-of-process: may be a local or target pointer. 
 * vm->monitorTable must be a target value.
 * @param[in] targetVMThread	the target J9VMThread
 * @param[in] object the object. For out-of-process: a target pointer.
 * @returns the J9ThreadAbstractMonitor from a hashtable entry
 * @retval NULL There is no corresponding monitor in vm->monitorTable.
 * 
 * @see monitorTablePeek
 */
J9ThreadAbstractMonitor *
monitorTablePeekMonitor(J9JavaVM *vm, J9VMThread *targetVMThread, j9object_t object)
{
	J9ThreadAbstractMonitor *monitor = NULL;
	J9ObjectMonitor *objectMonitor = NULL;
	
	objectMonitor = monitorTablePeek(vm, targetVMThread, object);
	if (objectMonitor) {
		monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
	}
	return monitor;
}

/**
 * Search vm->monitorTable for the inflated monitor corresponding to an object.
 * Similar to monitorTableAt(), but doesn't add the monitor if it isn't found in the hashtable.
 * 
 * This function may block on vm->monitorTableMutex.
 * This function can work out-of-process.
 * 
 * @param[in] vm the JavaVM. For out-of-process: may be a local or target pointer. 
 * vm->monitorTable must be a target value.
 * @param[in] targetVMThread	the target J9VMThread
 * @param[in] object the object. For out-of-process: a target pointer.
 * @returns a J9ObjectMonitor from the monitor hashtable
 * @retval NULL There is no corresponding monitor in vm->monitorTable.
 * 
 * @see monitorTablePeekMonitor
 */
J9ObjectMonitor *
monitorTablePeek(J9JavaVM *vm, J9VMThread *targetVMThread, j9object_t object)
{
#if defined (J9VM_OUT_OF_PROCESS)
	/* No longer supported out of process */
	return NULL;
#else /* defined (J9VM_OUT_OF_PROCESS) */

	J9ObjectMonitor *monitor = NULL;
	/*
	 * As far as object's hash code is been using for hashing, object must have HASH flag(s) set in object header
	 * however RAS Dump might call this code on random data (in "shared access" mode in case of heap mutation).
	 * This check suppose to protect heap from corruption
	 * (If OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS bit happen to be not set in given data hashCode() would set it
	 * and corrupt heap by expected read-only operation. So "sniff" hash bits before and ignore request if hash bit is not set)
	 * If hash bit for object is not set such object can not be found in hash table any way
	 *
	 * For more information see CMVC 179161
	 */
	if (0 != (TMP_J9OBJECT_FLAGS(object) & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS))) {
		J9HashTable *monitorTable = NULL;
		omrthread_monitor_t mutex = vm->monitorTableMutex;
		J9ObjectMonitor key_objectMonitor;
		J9ThreadAbstractMonitor key_monitor;

		/* Create a "fake" monitor just to probe the hash-table */
		key_monitor.userData = (UDATA)object;
		key_objectMonitor.monitor = (omrthread_monitor_t) &key_monitor;
		key_objectMonitor.hash = objectHashCode(vm, object);

		omrthread_monitor_enter(mutex);
		if (NULL == monitorTable) {
			UDATA index = key_objectMonitor.hash % (U_32)vm->monitorTableCount;
			monitorTable = vm->monitorTables[index];
		}

		monitor = hashTableFind(monitorTable, &key_objectMonitor);

		omrthread_monitor_exit(mutex);
	}
	return monitor;
#endif /* defined (J9VM_OUT_OF_PROCESS) */
}

#if defined(J9VM_OUT_OF_PROCESS)
/**
 * Out-of-process equivalent of an object field access macro,
 * such as J9VMJAVALANGTHREAD_THREADREF()
 *
 * @param[in] object Target j9object_t value.
 * @param[in] clazz Target J9Class pointer.
 * @param[in] fieldName Name of the object field. \0-terminated.
 * @param[in] fieldSig Signature of the object field. \0-terminated.
 * @param[in] fieldBytes Width of the object field in bytes.
 * @returns Content of the object field, extended to 64-bits.
 */
static U_64
readObjectField(j9object_t object, J9Class *clazz, U_8 *fieldName, U_8 *fieldSig, UDATA fieldBytes)
{
	U_64 field = 0;
	IDATA offset;

	offset = instanceFieldOffset(NULL, clazz, fieldName, strlen(fieldName), fieldSig, strlen(fieldSig), NULL, NULL, J9_LOOK_NO_JAVA);
	if (offset >= 0) {
		void *remoteAddr = (U_8 *)object + offset + sizeof(J9Object);

		switch (fieldBytes) {
		case 1:
			field = (U_64)dbgReadByte(remoteAddr);
			break;
		case 2:
			field = (U_64)dbgReadU16(remoteAddr);
			break;
		case 4:
			field = (U_64)dbgReadU32(remoteAddr);
			break;
		case 8:
			field = dbgReadU64(remoteAddr);
			break;
		default:
			dbgError("readObjectField: %s Unexpected field width: %d", fieldName, fieldBytes);
			break;
		}
	}
	return field;
}

/**
 * Out-of-process equivalent of J9OBJECT_CLAZZ()
 *
 * @param[in] vmtoken Is passed a local vmtoken pointer. Unused.
 * @param[in] object Target j9object_t value.
 * @returns Target address of a J9Class.
 */
static J9Class *
readObjectClass(J9VMThread *vmtoken, j9object_t object)
{
	J9Class *clazz = NULL;
	void *remoteAddr = &(object->clazz);

	/* read class slot and mask flags located in low bits */
	if (sizeof(j9objectclass_t) == sizeof(U_32)) {
		clazz = (J9Class *)((UDATA)dbgReadU32(remoteAddr) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1)));
	} else {
		clazz = (J9Class *)(dbgReadUDATA(remoteAddr) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1)));
	}
	return clazz;
}
#endif

