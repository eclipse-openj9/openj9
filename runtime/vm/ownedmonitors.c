/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "rommeth.h"
#include "stackwalk.h"
#include "vm_internal.h"

#define J9_GETOWNEDMONITORS_ERROR -1

static UDATA getOwnedObjectMonitorsIterator(J9VMThread *currentThread, J9StackWalkState *walkState);
static UDATA walkFrameMonitorEnterRecords(J9VMThread *currentThread, J9StackWalkState *walkState);
static IDATA getJNIMonitors(J9VMThread *currentThread, J9VMThread *targetThread, J9ObjectMonitorInfo *minfoStart, J9ObjectMonitorInfo *minfoEnd);
static IDATA countMonitorEnterRecords(J9VMThread *targetThread);


/**
 * Get the object monitors owned by a thread
 * @param[in] currentThread
 * @param[in] targetThread
 * @param[in] info Array where monitor info should be returned
 * @param[in] infoLen Length of the info array in elements, not bytes
 * @return -1 on error<br>
 * >= 0, number of info elements
 *
 * walkState fields
 * - userData1 = curInfo
 * - userData2 = lastInfo / monitorCount
 * - userData3 = monitorEnterRecords
 * - userData4 = stack depth, including inlines
 */
IDATA
getOwnedObjectMonitors(J9VMThread *currentThread, J9VMThread *targetThread,
		J9ObjectMonitorInfo *info, IDATA infoLen)
{
	J9StackWalkState walkState;
	BOOLEAN countOnly = FALSE;
	IDATA rc = 0;

	/* Take the J9JavaVM from the targetThread as currentThread may be null. */
	J9JavaVM* javaVM = targetThread->javaVM;

	if (infoLen > 0) {
		if (NULL == info) {
			return J9_GETOWNEDMONITORS_ERROR;
		} 
		countOnly = FALSE;
		walkState.userData1 = info;
		walkState.userData2 = &info[infoLen - 1];
	} else {
		countOnly = TRUE;
		walkState.userData1 = NULL;
		walkState.userData2 = (void *)0;
	}
	walkState.userData3 = targetThread->monitorEnterRecords;
	walkState.userData4 = (void *)1;
	walkState.walkThread = targetThread;
	walkState.skipCount = 0;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY
		| J9_STACKWALK_INCLUDE_NATIVES
		| J9_STACKWALK_SKIP_INLINES
		| J9_STACKWALK_ITERATE_FRAMES;
	walkState.frameWalkFunction = getOwnedObjectMonitorsIterator;

	if (javaVM->walkStackFrames(currentThread, &walkState) != J9_STACKWALK_RC_NONE) {
		return J9_GETOWNEDMONITORS_ERROR;
	}
	
	if (TRUE == countOnly) {
		rc = (IDATA)walkState.userData2;
		if (rc < 0) {
			return J9_GETOWNEDMONITORS_ERROR;
		}
		rc += countMonitorEnterRecords(targetThread);
	} else {
		getJNIMonitors(currentThread, targetThread, walkState.userData1, walkState.userData2);
		rc = infoLen;
	}
	
	return rc;
}

/*
 * Stack frame iterator. Copies owned monitors into MonitorInfo array.
 *
 * Returns:
 * J9_STACKWALK_KEEP_ITERATING
 * J9_STACKWALK_STOP_ITERATING
 */
static UDATA
getOwnedObjectMonitorsIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	UDATA rc = J9_STACKWALK_KEEP_ITERATING;

	/* Take the J9JavaVM from the targetThread as currentThread may be null. */
	J9JavaVM* javaVM = walkState->walkThread->javaVM;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->jitInfo) {
		/* The jit walk may increment the stack depth */
		rc = javaVM->jitGetOwnedObjectMonitors(walkState);
	} else
#endif
	{
		rc = walkFrameMonitorEnterRecords(currentThread, walkState);
	}
	
	/* increment stack depth */
	walkState->userData4 = (void *)(((IDATA)walkState->userData4) + 1);

	return rc;
}

static UDATA
walkFrameMonitorEnterRecords(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	J9MonitorEnterRecord *monitorEnterRecords = walkState->userData3;
	J9ObjectMonitorInfo *info = walkState->userData1;
	J9ObjectMonitorInfo *lastInfo = walkState->userData2;
	U_32 modifiers;
	UDATA *frameID;
	
	IDATA monitorCount = (IDATA)walkState->userData2;

	/*
	 * Walk the monitor enter records from this frame and
	 * add the objects to the array.
	 */

	frameID = walkState->arg0EA;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->jitInfo != NULL) {
		frameID = walkState->unwindSP;
	}
#endif

	while (monitorEnterRecords &&
			(frameID == CONVERT_FROM_RELATIVE_STACK_OFFSET(walkState->walkThread, monitorEnterRecords->arg0EA))
		) {
		/* Do not report monitors for stack allocated objects */
		if (!isObjectStackAllocated(walkState->walkThread, monitorEnterRecords->object)) {
			if (NULL == info) {
				++monitorCount;
			} else {
				if (info > lastInfo) {
					/* Don't overflow the MonitorInfo array */
					return J9_STACKWALK_STOP_ITERATING;
				}
				info->object = monitorEnterRecords->object;
				info->count = monitorEnterRecords->dropEnterCount;
				info->depth = (IDATA) walkState->userData4;
				info++;
			}
		}
		monitorEnterRecords = monitorEnterRecords->next;
	}

	/* If the current method is synchronized, add the syncObject to the array. */
	modifiers = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers;
	if (modifiers & J9AccSynchronized) {
		j9object_t syncObject;

		if ((modifiers & J9AccNative)
#ifdef J9VM_INTERP_NATIVE_SUPPORT
			|| (walkState->jitInfo != NULL)
#endif
		) {
			if (modifiers & J9AccStatic) {
				syncObject = J9VM_J9CLASS_TO_HEAPCLASS(walkState->constantPool->ramClass);
			} else {
				syncObject = *((j9object_t *) walkState->arg0EA);
			}
		} else {
			syncObject = (j9object_t) (walkState->bp[1]);
		}

		/* Do not report monitors for stack allocated objects */
		if ((!isObjectStackAllocated(walkState->walkThread, syncObject))) {
			if (NULL == info) {
				++monitorCount;
			} else {
				if (info > lastInfo) {
					/* Don't overflow the MonitorInfo array */
					return J9_STACKWALK_STOP_ITERATING;
				}

				info->object = syncObject;
				info->count = 1;
				info->depth = (IDATA)walkState->userData4;
				info++;
			}
		}
	}

	walkState->userData1 = info;
	if (NULL == info) {
		walkState->userData2 = (void *)monitorCount;
	}
	walkState->userData3 = monitorEnterRecords;
	return J9_STACKWALK_KEEP_ITERATING;
}

/* Assemble JNI monitors */
static IDATA
getJNIMonitors(J9VMThread *currentThread, J9VMThread *targetThread, J9ObjectMonitorInfo *firstInfo, J9ObjectMonitorInfo *lastInfo)
{
	IDATA count = 0;
	J9MonitorEnterRecord *enterRecord;
	J9ObjectMonitorInfo *minfo = firstInfo;

	enterRecord = targetThread->jniMonitorEnterRecords;
	while (enterRecord != NULL) {
		if (minfo > lastInfo) {
			/* Don't overflow the MonitorInfo array */
			return count;
		}
		/* Do not report monitors for stack allocated objects */
		if (!isObjectStackAllocated(targetThread, enterRecord->object)) {
			minfo->object = enterRecord->object;
			minfo->count = enterRecord->dropEnterCount;
			minfo->depth = 0;
			++minfo;
			++count;
		}
		enterRecord = enterRecord->next;
	}
	return count;
}

/*
 * Count the length of a monitor record list.
 * Returns:
 * >=0 - number of records in list
 */
static IDATA
countMonitorEnterRecords(J9VMThread *targetThread)
{
	IDATA count = 0;
	J9MonitorEnterRecord *rec = targetThread->jniMonitorEnterRecords;

	while (rec != NULL) {
		if (!isObjectStackAllocated(targetThread, rec->object)) {
			++count;
		}
		rec = rec->next;
	}

	return count;
}

