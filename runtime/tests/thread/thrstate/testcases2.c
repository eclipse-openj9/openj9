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

/* vmthread blocked, osthread started */


/* no blockingenterobject */
UDATA
test_vBz_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, NULL, 0);
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

#if 0 /* this crashes and is unsupported */
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
#endif
	return rc;
}

/* flat lock*/
UDATA
test_vBfoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(NULL, 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Thread should be runnable if the blocking object has no owner.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfoC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(NULL, MON_COUNT));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Thread should be runnable if the blocking object has no owner.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Thread should be runnable if it owns the blocking object.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(selfVmthread), MON_COUNT));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);

	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Thread should be runnable if it owns the blocking object.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
	return rc;
}

UDATA
test_vBfVc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), 0));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 1);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 1);

	return rc;
}

UDATA
test_vBfVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	
	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getFlatLock(TESTDATA(otherVmthread), MON_COUNT-1));
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

	return rc;
}

/* inflated lock */
/* native thread is not blocked */
UDATA
test_vBaoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);

	
#if 0
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Although the inflated monitor is owned, the omrthread flags indicate\nthe thread has not yet blocked on the inflated monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
#endif
	return rc;
}

UDATA
test_vBaVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);

#if 0
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Although the inflated monitor is owned, the omrthread flags indicate\nthe thread has not yet blocked on the inflated monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
#endif
	return rc;
}

UDATA
test_vBaNc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);

#if 0
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Although the inflated monitor is owned, the omrthread flags indicate\nthe thread has not yet blocked on the inflated monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
#endif
	return rc;
}

UDATA
test_vBaNC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED, NULL, NULL, 0);
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);

#if 0
	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	j9tty_printf(PORTLIB, "Expected failure in getVMThreadStatus:\n");
	j9tty_printf(PORTLIB, "Although the inflated monitor is owned, the omrthread flags indicate\nthe thread has not yet blocked on the inflated monitor.\n");
	if (ignoreExpectedFailures) {
		rc = IGNORE_FAILURES(rc, FAILED_OLDSTATE);
	}
#endif
	return rc;
}


/* native thread is blocked on the same monitor */
UDATA
test_vBaoc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, NULL, 0);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), 0);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
			
	return rc;
}

UDATA
test_vBaVC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
			
	return rc;
}

UDATA
test_vBaNc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 0);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
			
	return rc;
}

UDATA
test_vBaNC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);
	/*
	setJ9MonitorState(TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);
	*/

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
			
	return rc;
}

/* native thread is blocked on a different monitor */
UDATA
test_vBaoc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, MON_COUNT+7);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT+7);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
			
	return rc;
}

UDATA
test_vBaVC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT+7);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
			
	return rc;
}

UDATA
test_vBaNc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
			
	return rc;
}

UDATA
test_vBaNC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT+7);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
			
	return rc;
}

/* native thread is blocked on a different monitor, 
 * monitor is owned by an attached thread
 */
UDATA
test_vBaoc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(vmthread3), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, MON_COUNT+7);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(vmthread3), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(vmthread3), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT+7);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(vmthread3), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);

	return rc;
}

UDATA
test_vBaVC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT+7);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
			
	return rc;
}

UDATA
test_vBaNc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
			
	return rc;
}

UDATA
test_vBaNC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_BLOCKED, TESTDATA(rawMonitor)->monitor, TESTDATA(osthread3), MON_COUNT+7);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
			
	return rc;
}

/* native thread is waiting on the same monitor */
UDATA
test_vBaoc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
			
	return rc;
}

UDATA
test_vBaVC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
			
	return rc;
}

UDATA
test_vBaNc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
			
	return rc;
}

UDATA
test_vBaNC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
			
	return rc;
}

/* native thread is waiting on an unowned monitor */
UDATA
test_vBaoc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
			
	return rc;
}

UDATA
test_vBaVC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
			
	return rc;
}

UDATA
test_vBaNc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
			
	return rc;
}

UDATA
test_vBaNC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, NULL, 0);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
			
	return rc;
}

/* native thread is waiting on a monitor owned by an attached thread */
UDATA
test_vBaoc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaoC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaOC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(selfOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_WAITING, NULL, TESTDATA(rawMonitor)->monitor, TESTDATA(otherVmthread), 1);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_RUNNING, NULL, NULL, 0);
			
	return rc;
}

UDATA
test_vBaVc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), 0);
			
	return rc;
}

UDATA
test_vBaVC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(otherOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, TESTDATA(otherVmthread), MON_COUNT);
			
	return rc;
}

UDATA
test_vBaNc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), 0);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, 0);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, 0);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, 0);
			
	return rc;
}

UDATA
test_vBaNC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	UDATA rc = 0;

	setVMThreadState(TESTDATA(selfVmthread), TESTDATA(selfOsthread), J9_PUBLIC_FLAGS_THREAD_BLOCKED, TESTDATA(blockingObject), getInflLock(TESTDATA(objMonitor))); 
	setJ9MonitorState(TESTDATA(objMonitor)->monitor, TESTDATA(unattachedOsthread), MON_COUNT);

	setJ9ThreadState(TESTDATA(selfOsthread), J9THREAD_FLAG_STARTED | J9THREAD_FLAG_WAITING, TESTDATA(rawMonitor)->monitor, TESTDATA(otherOsthread), 1);

	rc |= checkObjAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), NULL, MON_COUNT);
	rc |= checkRawAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(blockingObject), TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
	rc |= checkOldAnswers(env, TESTDATA(selfVmthread), J9VMTHREAD_STATE_BLOCKED, TESTDATA(objMonitor)->monitor, NULL, MON_COUNT);
			
	return rc;
}

