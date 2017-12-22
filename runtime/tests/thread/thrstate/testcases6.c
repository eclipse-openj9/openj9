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

/*
 * sleeping states 
 */
UDATA
test_vS_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vS_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED,
					 TESTDATA(rawMonitor)->monitor, NULL, 0);

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
test_vS_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED,
					 TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

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
test_vS_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED,
					 TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vS_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING,
					 TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor,
						NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	return rc;
}

UDATA
test_vS_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING,
					 TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

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
test_vS_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING,
					 TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor,
						TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor,
						TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vS_nWTMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING | J9THREAD_FLAG_TIMER_SET,
					 TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, NULL,
						TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(rawMonitor)->monitor,
						NULL, 0);

	return rc;
}

UDATA
test_vS_nWTMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING | J9THREAD_FLAG_TIMER_SET,
					 TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 1);

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
test_vS_nWTMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING | J9THREAD_FLAG_TIMER_SET,
					 TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, NULL,
						TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING_TIMED, TESTDATA(rawMonitor)->monitor,
						TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vS_nST(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_SLEEPING, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_SLEEPING | J9THREAD_FLAG_TIMER_SET,
					 NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SLEEPING, NULL, NULL, 0);

	return rc;
}

/* suspended threads */
UDATA
test_vZ_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	/* 
	 * We should probably report suspended, rather than running. 
	 * However, public APIs consider SUSPENDED==RUNNING, so I don't consider this a regression.
	 */ 
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	return rc;
}

UDATA
test_vZ_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	/* 
	 * We should probably report suspended, rather than running. 
	 * However, public APIs consider SUSPENDED==RUNNING, so I don't consider this a regression.
	 */ 
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vZ_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	/* 
	 * We should probably report suspended, rather than running. 
	 * However, public APIs consider SUSPENDED==RUNNING, so I don't consider this a regression.
	 */ 
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vZ_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED|J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	/* 
	 * We should probably report suspended, rather than running. 
	 * However, public APIs consider SUSPENDED==RUNNING, so I don't consider this a regression.
	 */ 
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_SUSPENDED, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(rawMonitor)->monitor,  NULL, 0);

	return rc;
}
