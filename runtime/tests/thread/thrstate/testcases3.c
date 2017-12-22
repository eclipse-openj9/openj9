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

static void
explainThreadIsNotBlocked(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "The thread is not blocked if it owns the blocking monitor,\nor if the blocking monitor is not owned.\n");
}

/*
 * vmthread blocked, owns blocking object;
 * native thread blocked on a different raw monitor
 * e.g. attempting to reacquire vmaccess after acquiring an object monitor. 
 */
UDATA
test_vBfOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainThreadIsNotBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainThreadIsNotBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports the thread blocked on the object monitor, which it already owns, instead of the raw monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 1));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 1);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It reports the thread blocked on the object monitor, which it already owns, instead of the raw monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}


UDATA
test_vBaOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vBaOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vBaOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vBaOC_nBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

/*
 * vmthread blocked, owns blocking object; 
 * native thread waiting on a different raw monitor
 * e.g. attempting to reacquire vmaccess after acquiring an object monitor. 
 */
UDATA
test_vBfOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 1));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainThreadIsNotBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 1));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	/* the thread has not yet released the monitor to wait on it, so still runnable */
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainThreadIsNotBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 1));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainThreadIsNotBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nWMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 1));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainThreadIsNotBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}


UDATA
test_vBaOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vBaOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	/* the thread has not yet released the monitor to wait on it, so still runnable */
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vBaOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vBaOC_nWMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}


/* 
 * vmthread's blocking object is flatlocked by another vmthread;
 * native thread owns the object monitor, or is waiting on it
 * e.g. blocked spinning on a flat object monitor
 */
UDATA
test_vBfVC_nStMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	
	return rc;
}
UDATA
test_vBfVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	
	return rc;
}

UDATA
test_vBfVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	
	return rc;
}

UDATA
test_vBfVC_nStMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	
	return rc;
}

UDATA
test_vBfVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	
	return rc;
}

UDATA
test_vBfVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	
	return rc;
}
