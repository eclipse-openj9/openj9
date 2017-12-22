/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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

#ifndef THRSTATETEST_H_
#define THRSTATETEST_H_

#include <assert.h>
#if defined(J9ZOS390)
#include <stdlib.h> /* for malloc for atoe */
#include "atoe.h"
#endif
#include "j9.h"
#include "j9comp.h"
#include "omrthread.h"

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif

/* testsetup.c */
extern void setJ9ThreadState(omrthread_t thr, UDATA flags, omrthread_monitor_t blocker,
		omrthread_t owner, UDATA count);
extern void setVMThreadState(J9VMThread *vmthread, omrthread_t osThread,
		UDATA publicFlags, j9object_t blockingEnterObject, UDATA lockWord);
extern void setJ9MonitorState(omrthread_monitor_t monitor, omrthread_t owner,	UDATA count);
extern UDATA getFlatLock(J9VMThread *owner, UDATA count);
extern UDATA getInflLock(J9ObjectMonitor *monitor);

extern UDATA checkObjAnswers(JNIEnv *env, J9VMThread *vmthread, UDATA exp_vmstate, j9object_t exp_lockObj, J9VMThread *exp_owner, UDATA exp_count);
extern UDATA checkRawAnswers(JNIEnv *env, J9VMThread *vmthread, UDATA exp_vmstate, j9object_t exp_lockObj, omrthread_monitor_t exp_rawLock, J9VMThread *exp_owner, UDATA exp_count);
extern UDATA checkOldAnswers(JNIEnv *env, J9VMThread *vmthread, UDATA exp_vmstate, omrthread_monitor_t exp_rawLock, J9VMThread *exp_owner, UDATA exp_count);

extern UDATA test_setup(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_cleanup(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern void printTestData(JNIEnv *env);
extern void cleanupTestData(JNIEnv *env);

typedef struct testdata_t {
	J9VMThread *selfVmthread;
	J9VMThread *otherVmthread;
	J9VMThread *vmthread3;
	omrthread_t selfOsthread;
	omrthread_t otherOsthread;
	omrthread_t unattachedOsthread;
	omrthread_t osthread3;
	jobject blockingObjectRef;
	j9object_t blockingObject;
	J9ObjectMonitor *objMonitor;
	J9ObjectMonitor *rawMonitor;

	j9object_t objectNoMonitor;
	jobject objectNoMonitorRef;
	
	volatile IDATA selfOsthreadDone;
	volatile IDATA otherOsthreadDone;
	volatile IDATA unattachedOsthreadDone;
	volatile IDATA thirdOsthreadDone;
	
	/* unused */
	jobject ownableSyncRef;
	j9object_t ownableSync;
} testdata_t;
extern testdata_t GlobalTestData;

#define J9OBJECT_FROM_JOBJECT(jobj) (*((j9object_t *)(jobj)))

#define MON_COUNT ((UDATA)3)
#define TESTDATA(zzz) (GlobalTestData.zzz)

#define FAILED_OBJSTATE 0x1
#define FAILED_RAWSTATE 0x2
#define FAILED_OLDSTATE 0x4

#define FAILURE_MASK (0xff)
#define IGNORED_FAILURE_MASK (0xff00)
#define EXPECTED_FAILURE_MASK (0xff0000)
#define IGNORE_FAILURES(status, bits) (((status) & (~(bits))) | (((status) & (bits)) << 8) | ((bits) << 16))

/*
 * v = vmthread
 * n = osthread
 * 
 * state flags:
 * 	St = started
 * 	D = dead
 * 	Z = suspended
 * 	B = blocked
 * 	W = waiting
 * 	N = notified
 * 	P = parked
 * 	S = sleeping
 * 
 * object monitors:
 * z = null blockingEnterObject
 * f = flat blockingEnterObject monitor
 * a = inflated blockingEnterObject monitor
 * U = out-of-line blockingEnterObject monitor, has montable entry
 * u = out-of-line blockingEnterObject monitor, no montable entry
 * 
 * raw monitors:
 * m = null monitor
 * M = non-null monitor (raw)
 * 
 * monitor owners:
 * o = null owner
 * O = self
 * N = other (not attached)
 * V = other (attached)
 * 
 * owned count:
 * c = 0 count
 * C = >0 count
 */
#include "testcases1.h"
#include "testcases2.h"
#include "testcases3.h"
#include "testcases4.h"
#include "testcases5.h"
#include "testcases6.h"

#endif /*THRSTATETEST_H_*/
