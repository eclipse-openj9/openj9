/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#include <stdio.h>
#include <stdlib.h>
#include "j9.h"
#include "omrthread.h"
#include "omrcfg.h"
#include "threadtest.h"

#if defined(OMR_THR_THREE_TIER_LOCKING)

#define START_DELAY (1 * 1000)

static const char *exitStatus(IDATA rc);

static int testRunning(void);
static int runningMain(void *arg);

static int testSleeping(void);
static int sleepingMain(void *arg);

static int testWaiting(void);
static int waitingMain(void *arg);

static int testBlocked(void);
static int blockedMain(void *arg);

static int testNotified(void);
static int notifiedMain(void *arg);

static int testManyWaiters(void);
static int testManyBlockers(void);
static int testManyBlockers2(void);
static int lockUntilInterrupted(void *arg);

static int testParked(void);
static int parkMain(void *arg);

static int testDeadlock(void);
static int deadlockMain(void *arg);

static int testRecursive(void);
static int recursiveMain(void *arg);

static int testRecursive2(void);
static int recursiveMain2(void *arg);

static int testLateInterrupt(void);
/*static int lateMain(void *arg);*/

int
abort_main(int argc, char *argv[])
{
	omrthread_t mainThread;

	omrthread_attach_ex(&mainThread, J9THREAD_ATTR_DEFAULT);

	testManyBlockers2();
	testRunning();
	testSleeping();
	testParked();
	testWaiting();
	testBlocked();
	testNotified();
	testManyWaiters();
	testManyBlockers();
	testDeadlock();
	testRecursive();
	testRecursive2();
	testLateInterrupt();

	omrthread_detach(omrthread_self());
	
	return 0;
}

static IDATA
createDefaultThread(omrthread_t *t, int (*entrypoint)(void *), void *entryarg)
{
	return omrthread_create_ex(t, J9THREAD_ATTR_DEFAULT, 0, entrypoint, entryarg);
}

static const char *
exitStatus(IDATA rc) 
{
	static const char *not_interrupted = "not interrupted";
	static const char *interrupted = "interrupted";
	static const char *prio_interrupted = "priority interrupted";
	static const char *interrupted_monitor_enter = "interrupted monitor enter";

	switch (rc) {
		default:
		case 0:
			return not_interrupted;
		case J9THREAD_INTERRUPTED:
			return interrupted;
		case J9THREAD_PRIORITY_INTERRUPTED:
			return prio_interrupted;
		case J9THREAD_INTERRUPTED_MONITOR_ENTER:
			return interrupted_monitor_enter;
	}
}

static void
printStatus(const char *testName, int pass)
{
	static const char *PASS = "PASS";
	static const char *FAIL = "FAIL";

	printf("%s: %s\n", testName, pass ? PASS : FAIL);
}

typedef struct run_testdata_t {
	omrthread_monitor_t exitSync;
} run_testdata_t;

static int
testRunning(void)
{
	omrthread_t t;
	run_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);

	omrthread_monitor_enter(testdata.exitSync);
	createDefaultThread(&t, runningMain, &testdata);
	omrthread_sleep(START_DELAY);

	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	omrthread_monitor_destroy(testdata.exitSync);

	printStatus("testRunning", 1);
	return 0;
}

static int
runningMain(void *arg)
{
	run_testdata_t *testdata = arg;
	J9AbstractThread *self = (J9AbstractThread *)omrthread_self();

	do {
		omrthread_yield();
	} while (!(self->flags & J9THREAD_FLAG_ABORTED));

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}


typedef struct sleep_testdata_t {
	omrthread_monitor_t exitSync;
	volatile IDATA rc;
} sleep_testdata_t;

static int
testSleeping(void)
{
	omrthread_t t;
	sleep_testdata_t mythread;

	omrthread_monitor_init(&mythread.exitSync, 0);
	omrthread_monitor_enter(mythread.exitSync);
	createDefaultThread(&t, sleepingMain, &mythread);
	
	omrthread_sleep(START_DELAY);
	omrthread_abort(t);

	omrthread_monitor_wait(mythread.exitSync);
	omrthread_monitor_exit(mythread.exitSync);

	omrthread_monitor_destroy(mythread.exitSync);

	printStatus("testSleeping", mythread.rc == J9THREAD_PRIORITY_INTERRUPTED);
	return 0;
}

static int
sleepingMain(void *arg)
{
	sleep_testdata_t *mythread = arg;
	IDATA rc;

	rc = omrthread_sleep_interruptable(60 * 1000, 0);

	omrthread_monitor_enter(mythread->exitSync);
	mythread->rc = rc;
	omrthread_monitor_notify(mythread->exitSync);
	omrthread_monitor_exit(mythread->exitSync);

	return 0;
}

typedef struct wait_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t waitSync;
	volatile int waiting;
	volatile IDATA rc;
} wait_testdata_t;

static int
testWaiting(void)
{
	omrthread_t t;
	wait_testdata_t testdata;
	
	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.waitSync, 0);
	testdata.waiting = 0;

	omrthread_monitor_enter(testdata.exitSync);

	createDefaultThread(&t, waitingMain, &testdata);
	while (1) {
		omrthread_sleep(START_DELAY);
		omrthread_monitor_enter(testdata.waitSync);
		if (testdata.waiting == 1) {
			omrthread_monitor_exit(testdata.waitSync);
			break;
		}
		omrthread_monitor_exit(testdata.waitSync);
	} 

	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.waitSync;
		assert(mon->waiting == NULL);
		assert(mon->blocking == NULL);
		assert(mon->count == 0);
		assert(mon->owner == NULL);
	}

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.waitSync);

	printStatus("testWaiting", testdata.rc ==
			J9THREAD_INTERRUPTED_MONITOR_ENTER);
	return 0;
}

static int
waitingMain(void *arg)
{
	wait_testdata_t *testdata = arg;

	omrthread_monitor_enter(testdata->waitSync);
	testdata->waiting = 1;
	testdata->rc = omrthread_monitor_wait_interruptable(testdata->waitSync, 0, 0);

	if (J9THREAD_INTERRUPTED_MONITOR_ENTER == testdata->rc) {
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata->waitSync;

		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));
		assert(self->monitor == NULL);

		assert(mon->waiting == NULL);
		/*assert((J9AbstractThread *)mon->owner == self);*/
		assert(mon->blocking == NULL);
		/*assert(mon->count == 1);*/
	} else {
		omrthread_monitor_exit(testdata->waitSync); 
	}
	
	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}

typedef struct blocked_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t blockedSync;
	IDATA rc;
} blocked_testdata_t;

static int
testBlocked(void)
{
	omrthread_t t;
	blocked_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.blockedSync, 0);
	testdata.rc = 0;

	omrthread_monitor_enter(testdata.exitSync);
	omrthread_monitor_enter(testdata.blockedSync);
	createDefaultThread(&t, blockedMain, &testdata);

	omrthread_sleep(START_DELAY);
	omrthread_abort(t);
	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.blockedSync;

		assert(mon->owner == omrthread_self());
		assert(mon->count == 1);
		assert(mon->blocking == NULL);
		assert(mon->waiting == NULL);
	}

	omrthread_monitor_exit(testdata.blockedSync);

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.blockedSync);

	printStatus("testBlocked", testdata.rc ==
			J9THREAD_INTERRUPTED_MONITOR_ENTER);
	return 0;
}

static int
blockedMain(void *arg)
{
	blocked_testdata_t *testdata = arg;

	testdata->rc = omrthread_monitor_enter_abortable_using_threadId(
			testdata->blockedSync, omrthread_self());
	if (0 == testdata->rc) {
		omrthread_monitor_exit(testdata->blockedSync);
	} else {
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();

		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));
		assert(self->monitor == NULL);
	}

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}

typedef struct notify_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t waitSync;
	IDATA rc;
} notify_testdata_t;

static int
testNotified(void)
{
	omrthread_t t;
	notify_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.waitSync, 0);
	testdata.rc = 0;

	omrthread_monitor_enter(testdata.exitSync);

	createDefaultThread(&t, notifiedMain, &testdata);

	omrthread_sleep(START_DELAY);

	omrthread_monitor_enter(testdata.waitSync);
	omrthread_monitor_notify(testdata.waitSync);
	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.waitSync;
		
		assert(mon->owner == omrthread_self());
		assert(mon->count == 1);
		assert(mon->waiting == NULL);
		assert(mon->blocking == NULL);
	}

	omrthread_monitor_exit(testdata.waitSync);

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.waitSync);

	printStatus("testNotified", testdata.rc ==
			J9THREAD_INTERRUPTED_MONITOR_ENTER);
	return 0;
}

static int
notifiedMain(void *arg)
{
	notify_testdata_t *testdata = arg;

	omrthread_monitor_enter(testdata->waitSync);
	testdata->rc = omrthread_monitor_wait_interruptable(testdata->waitSync, 0, 0);

	if (0 == testdata->rc) {
		omrthread_monitor_exit(testdata->waitSync);
	} else {
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();

		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));
		assert(self->monitor == NULL);
	}

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata->waitSync;
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();
		J9AbstractThread *thread;

		thread = (J9AbstractThread *)mon->waiting;
		while (thread) {
			assert(thread != self);
			thread = (J9AbstractThread *)thread->next;
		}

		thread = (J9AbstractThread *)mon->blocking;
		while (thread) {
			assert(thread != self);
			thread = (J9AbstractThread *)thread->next;
		}
	}

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}

/*
 * Many threads wait on the same mutex, but only one should be interrupted.
 */
static int
testManyWaiters(void)
{
#define MAX_THREADS 3
#define INTR_THREAD 1
	int ok = 1;
	int i;
	notify_testdata_t testdata[MAX_THREADS];
	omrthread_t t[MAX_THREADS];

	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_init(&testdata[i].exitSync, 0);
		testdata[i].rc = 0;
	}

	omrthread_monitor_init(&testdata[0].waitSync, 0);
	for (i = 1; i < MAX_THREADS; i++) {
		testdata[i].waitSync = testdata[0].waitSync;
	}

	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_enter(testdata[i].exitSync);
		createDefaultThread(&t[i], notifiedMain, &testdata[i]);
	}
	omrthread_sleep(2 * START_DELAY);

	omrthread_monitor_enter(testdata[0].waitSync);
	omrthread_monitor_notify_all(testdata[0].waitSync);

	omrthread_abort(t[INTR_THREAD]);
	omrthread_monitor_wait(testdata[INTR_THREAD].exitSync);

	{
		J9AbstractThread *thread;
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata[0].waitSync;

		for (i = 0; i < MAX_THREADS; i++) {
			thread = (J9AbstractThread *)t[i];
			if (INTR_THREAD != i) {
				assert(!(thread->flags & J9THREAD_FLAG_INTERRUPTED));
				assert(!(thread->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
			}
		}
	}

	omrthread_monitor_exit(testdata[0].waitSync);
	for (i = 0; i < MAX_THREADS; i++) {
		if (INTR_THREAD != i) {
			omrthread_monitor_wait(testdata[i].exitSync);
		}
		omrthread_monitor_exit(testdata[i].exitSync);
	}

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata[0].waitSync;

		assert(mon->waiting == NULL);
		assert(mon->blocking == NULL);
		assert(mon->owner == NULL);
		assert(mon->count == 0);
	}


	omrthread_monitor_destroy(testdata[0].waitSync);
	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_destroy(testdata[i].exitSync);
	}

	ok = 1;
	for (i = 0; i < MAX_THREADS; i++) {
		if (INTR_THREAD == i) {
			if (testdata[i].rc != J9THREAD_INTERRUPTED_MONITOR_ENTER) {
				ok = 0;
				printf("testManyWaiters: interrupted thread returned \"%s\"\n",
						exitStatus(testdata[i].rc));
			}
		} else {
			if (testdata[i].rc != 0) {
				ok = 0;
				printf("testManyWaiters: uninterrupted thread returned \"%s\"\n",
						exitStatus(testdata[i].rc));
			}
		}
	}

	printStatus("testManyWaiters", ok);
	return ok;
}


static int
testManyBlockers(void)
{
#undef MAX_THREADS
#undef INTR_THREAD
#define MAX_THREADS 2
#define INTR_THREAD 0
	int ok = 1;
	int i;
	blocked_testdata_t testdata[MAX_THREADS];
	omrthread_t t[MAX_THREADS];
	omrthread_monitor_t blockedSync;

	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_init(&testdata[i].exitSync, 0);
		testdata[i].rc = 0;
	}

	omrthread_monitor_init(&blockedSync, 0);
	for (i = 0; i < MAX_THREADS; i++) {
		testdata[i].blockedSync = blockedSync;
	}

	omrthread_monitor_enter(blockedSync);
	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_enter(testdata[i].exitSync);
		createDefaultThread(&t[i], blockedMain, &testdata[i]);
	}
	omrthread_sleep(START_DELAY);

	omrthread_abort(t[INTR_THREAD]);
	omrthread_monitor_wait(testdata[INTR_THREAD].exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)blockedSync;

		assert(mon->owner == omrthread_self());
		assert(mon->count == 1);
		assert(mon->waiting == NULL);
		assert(mon->blocking == t[1]);
		assert(((J9AbstractThread *)mon->blocking)->next == NULL);
	}

	omrthread_monitor_exit(testdata[0].blockedSync);
	for (i = 0; i < MAX_THREADS; i++) {
		if (INTR_THREAD != i) {
			omrthread_monitor_wait(testdata[i].exitSync);
		}
		omrthread_monitor_exit(testdata[i].exitSync);
	}

	omrthread_monitor_destroy(blockedSync);
	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_destroy(testdata[i].exitSync);
	}

	ok = 1;
	for (i = 0; i < MAX_THREADS; i++) {
		if (INTR_THREAD == i) {
			if (testdata[i].rc != J9THREAD_INTERRUPTED_MONITOR_ENTER) {
				ok = 0;
				printf("testManyBlockers: interrupted thread returned \"%s\"\n",
						exitStatus(testdata[i].rc));
			}
		} else {
			if (testdata[i].rc != 0) {
				ok = 0;
				printf("testManyBlockers: uninterrupted thread returned \"%s\"\n",
						exitStatus(testdata[i].rc));
			}
		}
	}

	printStatus("testManyBlockers", ok);
	return ok;
}

typedef struct park_testdata_t {
	omrthread_monitor_t exitSync;
	IDATA rc;
} park_testdata_t;

static int
testParked(void)
{
	park_testdata_t testdata;
	omrthread_t t;

	omrthread_monitor_init(&testdata.exitSync, 0);
	testdata.rc = 0;

	omrthread_monitor_enter(testdata.exitSync);
	createDefaultThread(&t, parkMain, &testdata);
	omrthread_sleep(START_DELAY);

	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	omrthread_monitor_destroy(testdata.exitSync);

	printStatus("testParked", testdata.rc == J9THREAD_PRIORITY_INTERRUPTED);
	return 0;
}

static int
parkMain(void *arg)
{
	park_testdata_t *testdata = arg;

	testdata->rc = omrthread_park(0, 0);

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}


typedef struct deadlock_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t lockA;
	omrthread_monitor_t lockB;
	IDATA rc;
} deadlock_testdata_t;

static int
testDeadlock(void)
{
	int ok = 1;
	deadlock_testdata_t testdata[2];
	omrthread_t t1;
	omrthread_t t0;
	omrthread_monitor_t lockA;
	omrthread_monitor_t lockB;

	omrthread_monitor_init(&lockA, 0);
	omrthread_monitor_init(&lockB, 0);


	testdata[0].rc = 0;
	testdata[0].lockA = lockA;
	testdata[0].lockB = lockB;
	omrthread_monitor_init(&testdata[0].exitSync, 0);

	testdata[1].rc = 0;
	testdata[1].lockA = lockB; /* reverse */
	testdata[1].lockB = lockA;
	omrthread_monitor_init(&testdata[1].exitSync, 0);

	omrthread_monitor_enter(testdata[0].exitSync);
	createDefaultThread(&t0, deadlockMain, &testdata[0]);

	omrthread_monitor_enter(testdata[1].exitSync);
	createDefaultThread(&t1, deadlockMain, &testdata[1]);

	omrthread_sleep(2 * START_DELAY);

	omrthread_abort(t0);

	omrthread_monitor_wait(testdata[0].exitSync);
	omrthread_monitor_exit(testdata[0].exitSync);
	omrthread_monitor_wait(testdata[1].exitSync);
	omrthread_monitor_exit(testdata[1].exitSync);


	{
		J9ThreadAbstractMonitor *mon;

		mon = (J9ThreadAbstractMonitor *)lockA;
		assert(NULL == mon->owner);
		assert(0 == mon->count);
		assert(NULL == mon->blocking);
		assert(NULL == mon->waiting);

		mon = (J9ThreadAbstractMonitor *)lockB;
		assert(NULL == mon->owner);
		assert(0 == mon->count);
		assert(NULL == mon->blocking);
		assert(NULL == mon->waiting);
	}

	omrthread_monitor_destroy(testdata[0].exitSync);
	omrthread_monitor_destroy(testdata[1].exitSync);
	omrthread_monitor_destroy(lockA);
	omrthread_monitor_destroy(lockB);

	ok = 1;
	if (testdata[0].rc != J9THREAD_INTERRUPTED_MONITOR_ENTER) {
		ok = 0;
		printf("testDeadlocked: interrupted thread returned \"%s\"\n",
				exitStatus(testdata[0].rc));
	} 
	if (testdata[1].rc != 0) {
		ok = 0;
		printf("testDeadlocked: uninterrupted thread returned \"%s\"\n",
				exitStatus(testdata[1].rc));
	}

	printStatus("testDeadlocked", ok);
	return 0;
}

static int
deadlockMain(void *arg)
{
	deadlock_testdata_t *testdata = arg;
	omrthread_t self = omrthread_self();
	IDATA rc;

	omrthread_monitor_enter(testdata->lockA);
	omrthread_sleep(START_DELAY);
	rc = omrthread_monitor_enter_abortable_using_threadId(testdata->lockB, self);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata->lockB;
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();
		J9AbstractThread *thread;

		assert(self->monitor == NULL);
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));

		thread = (J9AbstractThread *)mon->waiting;
		while (thread) {
			assert(thread != self);
			thread = (J9AbstractThread *)thread->next;
		}

		thread = (J9AbstractThread *)mon->blocking;
		while (thread) {
			assert(thread != self);
			thread = (J9AbstractThread *)thread->next;
		}

		if (0 == rc) {
			assert((J9AbstractThread *)mon->owner == self);
			assert(mon->count == 1);
		} else {
			assert((J9AbstractThread *)mon->owner != self);
		}
	}

	if (0 == rc) {
		omrthread_monitor_exit(testdata->lockB);
	}
	omrthread_monitor_exit(testdata->lockA);
	testdata->rc = rc;

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);
	return 0;
}


typedef struct rec_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t waitSync;
	IDATA rc;
} rec_testdata_t;

static int
testRecursive(void)
{
	omrthread_t t;
	rec_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.waitSync, 0);

	omrthread_monitor_enter(testdata.exitSync);
	createDefaultThread(&t, recursiveMain, &testdata);
	omrthread_sleep(START_DELAY);

	omrthread_monitor_enter(testdata.waitSync);
	omrthread_monitor_exit(testdata.waitSync);

	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.waitSync;
		assert(mon->owner == NULL);
		assert(mon->count == 0);
		assert(mon->blocking == NULL);
		assert(mon->waiting == NULL);
	}

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.waitSync);

	printStatus("testRecursive", testdata.rc ==
			J9THREAD_INTERRUPTED_MONITOR_ENTER);
	return 0;
}

static int
recursiveMain(void *arg)
{
	rec_testdata_t *testdata = arg;

	omrthread_monitor_enter(testdata->waitSync);
	omrthread_monitor_enter(testdata->waitSync);
	omrthread_monitor_enter(testdata->waitSync);

	testdata->rc = omrthread_monitor_wait_interruptable(testdata->waitSync, 0, 0);

	{
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata->waitSync;

		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));
		assert(self->monitor == NULL);

		assert((J9AbstractThread *)mon->owner == NULL);
		assert(mon->count == 0);
		assert(mon->blocking == NULL);
		assert(mon->waiting == NULL);

		omrthread_monitor_exit(testdata->waitSync);
		omrthread_monitor_exit(testdata->waitSync);
		omrthread_monitor_exit(testdata->waitSync);
	}

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);
	return 0;
}

static int
testRecursive2(void)
{
	omrthread_t t;
	rec_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.waitSync, 0);

	omrthread_monitor_enter(testdata.exitSync);
	createDefaultThread(&t, recursiveMain2, &testdata);
	omrthread_sleep(START_DELAY);

	omrthread_monitor_enter(testdata.waitSync);
	omrthread_monitor_notify(testdata.waitSync);
	omrthread_abort(t);

	omrthread_monitor_exit(testdata.waitSync);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.waitSync;
		
		assert(mon->owner == NULL);
		assert(mon->count == 0);
		assert(mon->blocking == NULL);
		assert(mon->waiting == NULL);
	}

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.waitSync);

	printStatus("testRecursive2", testdata.rc == J9THREAD_INTERRUPTED_MONITOR_ENTER);
	return 0;
}

static int
recursiveMain2(void *arg)
{
	rec_testdata_t *testdata = arg;

	omrthread_monitor_enter(testdata->waitSync);
	omrthread_monitor_enter(testdata->waitSync);
	omrthread_monitor_enter(testdata->waitSync);

	testdata->rc = omrthread_monitor_wait(testdata->waitSync);

	if (0 == testdata->rc) {
		omrthread_monitor_exit(testdata->waitSync);
		omrthread_monitor_exit(testdata->waitSync);
		omrthread_monitor_exit(testdata->waitSync);
	} else {
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata->waitSync;

		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));
		assert(self->monitor == NULL);

		assert((J9AbstractThread *)mon->owner != self);
		assert(mon->count <= 1);
		assert(mon->blocking == NULL);
		assert(mon->waiting == NULL);
	}

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);
	return 0;
}

typedef struct late_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t blockedSync;
	int rc;
} late_testdata_t;

/*
 * Try to interrupt the blocker after it has unblocked from its condvar, but
 * while its thread state is still BLOCKED.
 *
 * Need to put a delay after the while(1) loop in monitor_enter_three_tier().
 */
static int
testLateInterrupt(void)
{
	late_testdata_t testdata;
	omrthread_t t;
	
	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.blockedSync, 0);
	testdata.rc = 0;

	omrthread_monitor_enter(testdata.blockedSync);

	omrthread_monitor_enter(testdata.exitSync);
	createDefaultThread(&t, blockedMain, &testdata);
	omrthread_sleep(START_DELAY);
	omrthread_monitor_exit(testdata.blockedSync);
	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.blockedSync);

	printStatus("testLateInterrupt", testdata.rc == J9THREAD_PRIORITY_INTERRUPTED);
	return 0;
}

#if 0
static int
lateMain(void *arg)
{
	late_testdata_t *testdata = arg;

	testdata->rc = omrthread_monitor_enter_abortable_using_threadId(
			testdata->blockedSync, omrthread_self());

	if (0 == testdata->rc) {
		omrthread_monitor_exit(testdata->blockedSync);
	}

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);
	return 0;
}
#endif

static int
lockUntilInterrupted(void *arg)
{
	omrthread_t self = omrthread_self();
	omrthread_monitor_t mon = arg;

	omrthread_monitor_enter_using_threadId(mon, self);

	do {
		omrthread_yield();
	} while (!omrthread_priority_interrupted(self));

	omrthread_monitor_exit_using_threadId(mon, self);
	return 0;
}

/*
 * Some other thread (not the interrupting thread) holds the lock.
 */
static int
testManyBlockers2(void)
{
#undef MAX_THREADS
#undef INTR_THREAD
#define MAX_THREADS 5
#define INTR_THREAD 1
	int ok = 1;
	int i;
	blocked_testdata_t testdata[MAX_THREADS];
	omrthread_t t[MAX_THREADS];
	omrthread_monitor_t blockedSync;
	omrthread_t locker;

	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_init(&testdata[i].exitSync, 0);
		testdata[i].rc = 0;
	}

	omrthread_monitor_init(&blockedSync, 0);
	for (i = 0; i < MAX_THREADS; i++) {
		testdata[i].blockedSync = blockedSync;
	}

	createDefaultThread(&locker, lockUntilInterrupted, blockedSync);
	omrthread_sleep(START_DELAY);

	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_enter(testdata[i].exitSync);
		createDefaultThread(&t[i], blockedMain, &testdata[i]);
	}

	omrthread_abort(t[INTR_THREAD]);
	omrthread_monitor_wait(testdata[INTR_THREAD].exitSync);

	omrthread_sleep(START_DELAY);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)blockedSync;
		J9AbstractThread *thread;
		int blockedcount;

		assert(mon->owner == locker);
		assert(mon->count == 1);
		assert(mon->waiting == NULL);

		blockedcount = 0;
		thread = (J9AbstractThread *)mon->blocking;
		while (thread) {
			++blockedcount;
			assert(thread != (J9AbstractThread *)thread->next);
			thread = (J9AbstractThread *)thread->next;
		}
		assert(blockedcount == 4);
	}

	omrthread_monitor_exit(testdata[0].blockedSync);
	omrthread_priority_interrupt(locker);

	for (i = 0; i < MAX_THREADS; i++) {
		if (INTR_THREAD != i) {
			omrthread_monitor_wait(testdata[i].exitSync);
		}
		omrthread_monitor_exit(testdata[i].exitSync);
	}

	omrthread_monitor_destroy(blockedSync);
	for (i = 0; i < MAX_THREADS; i++) {
		omrthread_monitor_destroy(testdata[i].exitSync);
	}

	ok = 1;
	for (i = 0; i < MAX_THREADS; i++) {
		if (INTR_THREAD == i) {
			if (testdata[i].rc != J9THREAD_INTERRUPTED_MONITOR_ENTER) {
				ok = 0;
				printf("testManyBlockers2: interrupted thread returned \"%s\"\n",
						exitStatus(testdata[i].rc));
			}
		} else {
			if (testdata[i].rc != 0) {
				ok = 0;
				printf("testManyBlockers2: uninterrupted thread returned \"%s\"\n",
						exitStatus(testdata[i].rc));
			}
		}
	}

	printStatus("testManyBlockers2", ok);
	return ok;
}
/* TODO test interrupting states that are not supposed to be unblocked */

#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
