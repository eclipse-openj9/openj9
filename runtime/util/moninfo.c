/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "omrthread.h"
#include "util_internal.h"
#include "ut_j9vmutil.h"
#ifdef J9VM_THR_LOCK_NURSERY
#include "lockNurseryUtil.h"
#endif
#include "monhelp.h"


/* Verify if aObj is allocated on targetThread stack
 * Returns :
 * 1 : aObj is stack allocated
 * 0 : aObj is not stack allocated
 */
UDATA
isObjectStackAllocated(J9VMThread *targetThread, j9object_t aObj) {
	J9JavaStack *stack = targetThread->stackObject;

	if (((UDATA)aObj > (UDATA)stack) && ((UDATA)aObj < (UDATA)(stack->end))) {
		return 1;
	}

	return 0;
}

/*
 * Determine the owner of an object's monitor.
 * Return the owner, or NULL if unowned.
 * If pcount != NULL, return the entry count in pcount.
*/
J9VMThread * 
getObjectMonitorOwner(J9JavaVM *vm, J9VMThread *vmThread, j9object_t object, UDATA *pcount)
{

	UDATA count = 0;
	J9VMThread *owner = NULL;
	j9objectmonitor_t lock;
#ifdef J9VM_THR_LOCK_NURSERY
	J9ObjectMonitor *objectMonitor;
#endif

	Trc_VMUtil_getObjectMonitorOwner_Entry(vm, object, pcount);

#ifdef J9VM_THR_LOCK_NURSERY
	if (!LN_HAS_LOCKWORD(vmThread,object)) {
		objectMonitor = monitorTablePeek(vm, vmThread, object);
		if (objectMonitor != NULL){
			lock = objectMonitor->alternateLockword;
		} else {
			lock = 0;
		}
	} else {
		lock = J9OBJECT_MONITOR(vmThread, object);
	}
#else
	lock = J9OBJECT_MONITOR(vmThread, object);
#endif

	if (J9_LOCK_IS_INFLATED(lock)) {
		J9ThreadAbstractMonitor *monitor = getInflatedObjectMonitor(vm, vmThread, object, lock);

		/*
		 * If the monitor is out-of-line and NULL, then it was never entered,
		 * so it can't have an owner.
		 */

		if (monitor) {
			omrthread_t osOwner = monitor->owner;
			if (osOwner) {
				owner = getVMThreadFromOMRThread(vm, osOwner);
				/* possible timing hole -- owner might exit monitor */
				count = monitor->count;
				if (count == 0) {
					owner = NULL;
				}
			}
		}
	} else {
		owner = J9_FLATLOCK_OWNER(lock);
		if (owner) {
			count = J9_FLATLOCK_COUNT(lock);	
			if (count == 0) {
				/* this can happen if the lock is reserved but unowned */
				owner = NULL;
			}
		}
	}

	if (pcount) {
		*pcount = count;
	}

	Trc_VMUtil_getObjectMonitorOwner_Exit2(object, owner, count);

	return owner;
}

