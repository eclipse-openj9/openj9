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
explainParkedAreIgnored(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "It never checks whether threads are parked.\n");
}

static void
explainAtypicalState(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadObjectState:\n");
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadRawState:\n");
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Atypical thread state. Should not occur normally.\n");
}

/* parked with no park blocker */
UDATA
test_vPm_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPm_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPm_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
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
test_vPm_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
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
test_vPm_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPm_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPm_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
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
test_vPm_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

/* These suspended states are pretty unusual. */
UDATA
test_vPm_nZP(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED|J9THREAD_FLAG_SUSPENDED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainAtypicalState(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OBJSTATE | FAILED_RAWSTATE);
	}
	return rc;
}

UDATA
test_vPm_nZBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED|J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainAtypicalState(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OBJSTATE | FAILED_RAWSTATE | FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPm_nZWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING|J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainAtypicalState(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OBJSTATE | FAILED_RAWSTATE | FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPTm_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPTm_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED|J9THREAD_FLAG_TIMER_SET, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPTm_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
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
test_vPTm_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
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
test_vPTm_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTm_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPTm_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
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
test_vPTm_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}


/* parked on an unowned object */
UDATA
test_vPMoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPMoc_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPMoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
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
test_vPMoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
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
test_vPMoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPMoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPMoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
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
test_vPMoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTMoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPTMoc_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED|J9THREAD_FLAG_TIMER_SET, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, TESTDATA(blockingObject), NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPTMoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
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
test_vPTMoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
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
test_vPTMoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTMoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPTMoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
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
test_vPTMoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

/* parked on an object owned by self */
UDATA
test_vPMOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPMOC_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPMOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
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
test_vPMOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
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
test_vPMOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPMOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPMOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
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
test_vPMOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTMOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPTMOC_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED|J9THREAD_FLAG_TIMER_SET, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, TESTDATA(blockingObject), NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPTMOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
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
test_vPTMOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
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
test_vPTMOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTMOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPTMOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
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
test_vPTMOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

/* parked on an object owned by another thread */
UDATA
test_vPMVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPMVC_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPMVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
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
test_vPMVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
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
test_vPMVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPMVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPMVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
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
test_vPMVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTMVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vPTMVC_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_PARKED|J9THREAD_FLAG_TIMER_SET, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, TESTDATA(blockingObject), NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);

	explainParkedAreIgnored(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vPTMVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
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
test_vPTMVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
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
test_vPTMVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vPTMVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vPTMVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
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
test_vPTMVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_PARKED|J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

