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
#include "jnitest_internal.h"
#include "j9protos.h"
#include "omrthread.h"
#include "thread_api.h"

/*
 * **********
 *   Legend:
 * **********
 *
 * JOO: Java thread has object monitor, wants object monitor.
 * JOS: Java thread has object monitor, wants system monitor.
 * JSO: Java thread has system monitor, wants object monitor.
 * JSS: Java thread has system monitor, wants system monitor.
 * NSS: Native thread has system monitor, wants system monitor.
 *
 * N.B. A native thread can never hold/want an object monitor,
 * because it must be attached first (i.e. it's a J9VMThread / Java thread).
 *
 */

static omrthread_monitor_t firstMonitor;
static omrthread_monitor_t secondMonitor;
static omrthread_monitor_t mutex; /* Used for synchronizing threads when setting up test conditions. */

/* ****************************
 * Utility methods shared by all tests.
 * Note that monitors are reused between tests, run only *one* test at a time.
 * The convention is for the Java code to always call the setup function first.
 * *****************************/

void JNICALL
Java_j9vm_test_corehelper_DeadlockCoreGenerator_setup(JNIEnv *env)
{
	omrthread_monitor_init_with_name(&firstMonitor,0,"First Monitor");
	omrthread_monitor_init_with_name(&secondMonitor,0,"Second Monitor");
	omrthread_monitor_init_with_name(&mutex,0,"Test Mutex"); /* Used for synchronization to ensure deadlock. */
}

void JNICALL
Java_j9vm_test_corehelper_DeadlockCoreGenerator_enterFirstMonitor(JNIEnv *env)
{
	omrthread_monitor_enter(firstMonitor); /* No way to exit from Java code, but used for deadlocks only. */
}

void JNICALL
Java_j9vm_test_corehelper_DeadlockCoreGenerator_enterSecondMonitor(JNIEnv *env)
{
	omrthread_monitor_enter(secondMonitor); /* No way to exit from Java code, but used for deadlocks only. */
}

static int J9THREAD_PROC
genericTestThreadMain(void *arg)
{
	PORT_ACCESS_FROM_PORT(arg);
	omrthread_set_name(omrthread_self(), "Native Deadlock Test Thread");
	j9tty_printf(PORTLIB, "Native Thread Started...\n");

	omrthread_monitor_enter(secondMonitor);
	j9tty_printf(PORTLIB, "Native Thread Acquired Second Monitor...\n");

	/* Notify caller that the second monitor has been acquired. */
	omrthread_monitor_enter(mutex);
	omrthread_monitor_notify(mutex);
	omrthread_monitor_exit(mutex);

	j9tty_printf(PORTLIB, "Second Thread Attempting to Acquire First Monitor...\n");
	omrthread_monitor_enter(firstMonitor); /* Deadlock. */
	j9tty_printf(PORTLIB, "Error, should never see me! Native thread should never acquire first monitor.\n");
	return 0;
}

/* ****************************
 *         Test Case #4
 *	     JSS, NSS deadlock.
 *         Test Case #6
 *	   JOS, NSS, JSO deadlock.
 * *****************************/

void JNICALL
Java_j9vm_test_corehelper_DeadlockCoreGenerator_spawnNativeThread(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	J9VMThread *currentThread = (J9VMThread *)env;
	omrthread_t *osthread = NULL;

	omrthread_monitor_enter(mutex); /* Make sure we don't get notified before we wait! */

	if (0 != omrthread_create(osthread, 0, J9THREAD_PRIORITY_MIN, 0, genericTestThreadMain, (void*) PORTLIB)) {
		j9tty_printf(PORTLIB, "Error Spawning Native Thread!\n");
	}
	/* Pause until the native thread has acquired the monitor. */
	omrthread_monitor_wait(mutex);
	omrthread_monitor_exit(mutex);
}

/* ****************************
 *         Test Case #5
 *	     NSS, NSS deadlock.
 * *****************************/

static int J9THREAD_PROC
test5FirstThreadMain(void *arg)
{
	PORT_ACCESS_FROM_PORT(arg);
	omrthread_t *osthread = NULL;

	omrthread_set_name(omrthread_self(), "First Deadlock Test Thread");
	j9tty_printf(PORTLIB, "First Thread Started...\n");
	omrthread_monitor_enter(firstMonitor);
	j9tty_printf(PORTLIB, "First Thread Acquired First Monitor...\n");

	omrthread_monitor_enter(mutex); /* Make sure we don't get notified before we wait! */

	if (0 != omrthread_create(osthread, 0, J9THREAD_PRIORITY_MIN, 0, genericTestThreadMain, (void*) PORTLIB)) {
		j9tty_printf(PORTLIB, "Error Spawning Second Thread!\n");
	}
	/* Pause until the native thread has acquired the monitor. */
	omrthread_monitor_wait(mutex);
	omrthread_monitor_exit(mutex);
	j9tty_printf(PORTLIB, "First thread waiting...\n");
	omrthread_monitor_enter(secondMonitor); /* Deadlock. */
	j9tty_printf(PORTLIB, "Error, should never see me! First thread should never acquire second monitor.\n");
	return 0;
}

void JNICALL
Java_j9vm_test_corehelper_DeadlockCoreGenerator_createNativeDeadlock(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	omrthread_t *osthread = NULL;
	J9VMThread *currentThread = (J9VMThread*) env;

	if (0 != omrthread_create(osthread, 0, J9THREAD_PRIORITY_MIN, 0, test5FirstThreadMain, (void*) PORTLIB)) {
		j9tty_printf(PORTLIB, "Error Spawning First Thread!\n");
	}
}
