/*******************************************************************************
 * Copyright (c) 2008, 2008 IBM Corp. and others
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
#include "j9cfg.h"
#include "j9comp.h"
#include "omrthread.h"
#include "thrstatetest.h"

/* vmthread waiting */

/* native thread is runnable/waiting/blocked/notifed on the same monitor */

UDATA
test_vWaoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	/*
	 * The thread is running because it either:
	 * - hasn't waited/blocked on the monitor yet
	 * - has already awakened from the wait and reacquired the monitor
	 */

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vWaoc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);

	/* blocked on the monitor means already awakened from the wait */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Blocked on the monitor means the omrthread has already awakened from the wait.\n");
	j9tty_printf(PORTLIB, "Since the monitor is unowned, it should be runnable, not blocked.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaoc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(objMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vWaoc_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(objMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Blocked on the monitor means the omrthread has already awakened from the wait.\n");
	j9tty_printf(PORTLIB, "Since the monitor is unowned, it should be runnable, not blocked.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	/*
	 * The thread is running because it either:
	 * - hasn't waited/blocked on the monitor yet
	 * - has already awakened from the wait and reacquired the monitor
	 */

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	return rc;
}

UDATA
test_vWaVC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);

	/* blocked on the monitor means already awakened from the wait */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaVC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaVC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vWaOC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaOC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);

	/* since the thread owns the monitor, it has either not waited yet, or has
	 * already waited.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaOC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);

	/* since the thread owns the monitor, it must have finished waiting and reacquiring the monitor */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}


/*
 * native thread is blocked/waiting on a different raw monitor
 * e.g. attempting to reacquire vmaccess after the wait
 * e.g. or locking publicFlagsMutex to update flags
 */

UDATA
test_vWaoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports the thread blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	/* from an objects-only point of view, the thread either hasn't waited yet
	 * or has already waited on the object monitor.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	/* otherwise the thread is waiting on the raw monitor */
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	/* from an objects-only point of view, the thread either hasn't waited yet
	 * or has already waited on the object monitor.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	/* from an objects-only point of view, the thread either hasn't waited yet
	 * or has already waited on the object monitor.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vWaoc_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaoc_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaoc_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports the thread blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vWaVC_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWaVC_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWaVC_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

/* vmthread timed waiting */

/* native thread is runnable/waiting/blocked/notifed on the same monitor */
UDATA
test_vWTaoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	/*
	 * The thread is running because it either:
	 * - hasn't waited/blocked on the monitor yet
	 * - has already awakened from the wait and reacquired the monitor
	 */

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vWTaoc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);

	/* blocked on the monitor means already awakened from the wait */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Blocked on the monitor means the omrthread has already awakened from the wait.\n");
	j9tty_printf(PORTLIB, "Since the monitor is unowned, it should be runnable, not blocked.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaoc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(objMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(objMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vWTaoc_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(objMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Blocked on the monitor means the omrthread has already awakened from the wait.\n");
	j9tty_printf(PORTLIB, "Since the monitor is unowned, it should be runnable, not blocked.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	/*
	 * The thread is running because it either:
	 * - hasn't waited/blocked on the monitor yet
	 * - has already awakened from the wait and reacquired the monitor
	 */

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	return rc;
}

UDATA
test_vWTaVC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);

	/* blocked on the monitor means already awakened from the wait */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaVC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaVC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vWTaOC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaOC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);

	/* since the thread owns the monitor, it has either not waited yet, or has
	 * already waited.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaOC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);

	/* since the thread owns the monitor, it must have finished waiting and reacquiring the monitor */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

/*
 * native thread is blocked/waiting on a different raw monitor
 * e.g. attempting to reacquire vmaccess after the wait
 * e.g. or locking publicFlagsMutex to update flags
 */

UDATA
test_vWTaoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports the thread blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	/* from an objects-only point of view, the thread either hasn't waited yet
	 * or has already waited on the object monitor.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	/* otherwise the thread is waiting on the raw monitor */
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	/* from an objects-only point of view, the thread either hasn't waited yet
	 * or has already waited on the object monitor.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(rawMonitor)->monitor, NULL, 0);

	/* from an objects-only point of view, the thread either hasn't waited yet
	 * or has already waited on the object monitor.
	 */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vWTaoc_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaoc_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaoc_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports the thread blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_TIMER_SET, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vWTaVC_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vWTaVC_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It fails to ignore the monitor enter count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vWTaVC_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_WAITING|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(osthread3), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_NOTIFIED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports blocked although the monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

