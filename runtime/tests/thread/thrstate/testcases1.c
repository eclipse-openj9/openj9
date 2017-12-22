/*******************************************************************************
 * Copyright (c) 2007, 2007 IBM Corp. and others
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
#include "util_api.h"
#include "thrstatetest.h"

/* testcases where the VMThread is Runnable, but the OSthread varies */

/* test cases return 0 on success */
UDATA
test_vNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	rc |= checkObjAnswers(env, NULL, J9VMTHREAD_STATE_UNKNOWN, NULL, NULL, 0);
	rc |= checkRawAnswers(env, NULL, J9VMTHREAD_STATE_UNKNOWN, NULL, NULL, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, NULL, J9VMTHREAD_STATE_UNKNOWN, NULL, NULL, 0);
#endif
	return rc;
}

UDATA
test_v0_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), NULL, 0, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_UNKNOWN, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_UNKNOWN, NULL, NULL, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_UNKNOWN, NULL, NULL, 0);
#endif

	return rc;
}

UDATA
test_vBfVC_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	return rc;
}

UDATA
test_vBaVC_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 1);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	return rc;
}

UDATA
test_vWaoc_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_WAITING, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(objMonitor)->monitor, NULL, 0);
#endif
	return rc;
}

UDATA
test_vWTaoc_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_WAITING | J9_PUBLIC_FLAGS_THREAD_TIMED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor)));
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(objMonitor)->monitor, NULL, 0);
#endif
	return rc;
}
UDATA
test_vPMoc_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_PARKED, TESTDATA(blockingObject), 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(blockingObject), NULL, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED, TESTDATA(objMonitor)->monitor, NULL, 0);
#endif
	return rc;
}

UDATA
test_vPTm_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_PARKED | J9_PUBLIC_FLAGS_THREAD_TIMED, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_PARKED_TIMED, NULL, NULL, 0);
#endif
	return rc;
}

UDATA
test_vS_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), NULL, J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, NULL, 0);
#if 0 /* this crashes */
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);
#endif
	return rc;
}

/* vmthread running, osthread Started */

static void
explainWhyOldGetStateShouldReturnNullMonitor(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "This test has an invalid omrthread state (flags != BLOCKED|WAITING, monitor != NULL).\n");
	j9tty_printf(PORTLIB, "omrthread->monitor should be ignored if omrthread->flags is not BLOCKED or WAITING.\n");
}

static void
explainSuspendedThreadsShouldNotBeBlocked(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Suspended state should take precedence over any blocked state.\n");
	j9tty_printf(PORTLIB, "This omrthread state is unusual and should not occur in a real application.\n");
}

UDATA
test_v0_nStm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	return rc;
}

UDATA
test_v0_nStMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nStMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

/* vmthread running, osthread Started | Dead */
UDATA
test_v0_nDm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	return rc;
}

UDATA
test_v0_nDMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nDMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}
UDATA
test_v0_nDMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}
UDATA
test_v0_nDMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nDMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nDMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nDMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nDMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_DEAD, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_DEAD, NULL, NULL, 0);
	
	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

/* osthread Suspended */
UDATA
test_v0_nZm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	return rc;
}

UDATA
test_v0_nZMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainWhyOldGetStateShouldReturnNullMonitor(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

/* started | blocked */
UDATA
test_v0_nBm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "This test has an invalid omrthread state (omrthread.flags = BLOCKED, omrthread.monitor = NULL).\n");
	j9tty_printf(PORTLIB, "The thread should not be blocked if there is no blocking monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "The thread should not be blocked if its blocking monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "The thread should not be blocked if its blocking monitor is unowned.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_v0_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "The thread should not be blocked if its blocking monitor is owned by itself.\n");
	j9tty_printf(PORTLIB, "If the thread has no blocking monitor, there should be no monitor count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_v0_nBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);

	return rc;
}

UDATA
test_v0_nBMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 0);

	return rc;
}

UDATA
test_v0_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

	return rc;
}



/* started | suspended | blocked */
UDATA
test_v0_nZBm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	
	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nZBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SUSPENDED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);

	explainSuspendedThreadsShouldNotBeBlocked(env);
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

/* started | waiting */
UDATA
test_v0_nWm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, NULL, 0);

	return rc;
}

UDATA
test_v0_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_v0_nWMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "The monitor state is somewhat invalid (no owner, count > 0).\n");
	j9tty_printf(PORTLIB, "It makes no sense to report an owned count when there is no owner.\n");
	j9tty_printf(PORTLIB, "We can't guarantee a correct owned count in any case, regardless of old or new API.\n");
	j9tty_printf(PORTLIB, "Arguably, getVMThreadRawState could return a non-zero count.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nWMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	/* if the omrthread owns the monitor, it either hasn't waited or has already waited */
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

#if 0
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadRawState and getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Invalid omrthread state (waiting on a monitor that the thread itself owns).\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_RAWSTATE | FAILED_OLDSTATE);
	}
#endif
	return rc;
}

UDATA
test_v0_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	/* if the omrthread owns the monitor, it either hasn't waited or has already waited */
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

#if 0
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadRawState and getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Invalid omrthread state (waiting on a monitor that the thread itself owns).\n");

	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_RAWSTATE | FAILED_OLDSTATE);
	}
#endif
	return rc;
}

UDATA
test_v0_nWMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_v0_nWMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);

	return rc;
}

UDATA
test_v0_nWMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 0);

	return rc;
}

UDATA
test_v0_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

	return rc;
}

/* native thread blocked/waiting on an object monitor */
UDATA
test_v0_nBMVC_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

	return rc;
}

UDATA
test_v0_nWMVC_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

	return rc;
}

/* started | blocked | notified */
UDATA
test_v0_nBNm(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Invalid omrthread state (blocked with no blocking monitor)\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "thread should be runnable if the blocking monitor has no owner.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBNMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "thread should be runnable if the blocking monitor has no owner.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}


UDATA
test_v0_nBNMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_v0_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Makes no sense to report a monitor entered count when there is no monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_v0_nBNMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_v0_nBNMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);

	return rc;
}

UDATA
test_v0_nBNMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 0);
	return rc;
}

UDATA
test_v0_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED,
			TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

	return rc;
}


/* started | sleeping */
UDATA
test_v0_nS(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SLEEPING, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);

	return rc;
}

/* started | sleeping | interruptible */
UDATA
test_v0_nSI(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SLEEPING | J9THREAD_FLAG_INTERRUPTABLE, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);

	return rc;
}

/* started | sleeping | interruptible | interrupted */
UDATA
test_v0_nSII(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), 0, NULL, 0); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SLEEPING | J9THREAD_FLAG_INTERRUPTABLE | J9THREAD_FLAG_INTERRUPTED, NULL, NULL, 0);
	
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);

	return rc;
}
