/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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
#include "omr.h"
#include "util_api.h"
#include "thrstatetest.h"

testdata_t GlobalTestData;
static j9sem_t keepAliveSem;
static int numOSThreads = 0;
static char *testDataDesc = NULL;

static IDATA initVMThread(JNIEnv *env, J9VMThread **vmthread, omrthread_t osthread);
static void cleanupVMThread(JNIEnv *env, J9VMThread *vmthread);
static IDATA initOSThread(JNIEnv *env, omrthread_t *osthread, volatile IDATA *doneflag);
static void cleanupOSThread(omrthread_t *osthread, volatile IDATA *doneflag);
static IDATA allocateObject(JNIEnv *env, jobject *objectRef);
static IDATA initObject(JNIEnv *env, j9object_t *obj, jobject objectRef, J9ObjectMonitor **objmon);
static void cleanupObject(JNIEnv *env, j9object_t *obj, jobject *objectRef);
static IDATA initMonitor(JNIEnv *env, J9ObjectMonitor **monitor);
static void cleanupMonitor(JNIEnv *env, J9ObjectMonitor *monitor);
static int J9THREAD_PROC threadmain(void *arg);
static UDATA bufferTestDataDesc(JNIEnv *env, char *buf, UDATA buflen);
static void saveTestDataDesc(JNIEnv *env);

static int J9THREAD_PROC 
threadmain(void *arg)
{
	volatile IDATA *doneflag = arg;
	
	j9sem_wait(keepAliveSem);
	*doneflag = 1;
	return 0;
}

static IDATA
initVMThread(JNIEnv *env, J9VMThread **vmthread, omrthread_t osthread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;

	*vmthread = vm->internalVMFunctions->allocateVMThread(vm, osthread, 0, currentThread->omrVMThread->memorySpace, NULL);
	if (*vmthread == NULL) {
		return -1;
	}
	return 0;
}

/**
 * Deallocate the thread.  Must be called with VM access.
 * @param vmthread thread to deallocate.
 */
static void
cleanupVMThread(JNIEnv *env, J9VMThread *vmthread)
{
	J9JavaVM *vm = ((J9VMThread *)env)->javaVM;

	vm->internalVMFunctions->deallocateVMThread(vmthread, FALSE, TRUE);
}

static IDATA
initOSThread(JNIEnv *env, omrthread_t *osthread, volatile IDATA *doneflag)
{
	IDATA rc = -1;
	J9JavaVM *vm = ((J9VMThread *)env)->javaVM;

	if (0 == vm->internalVMFunctions->createThreadWithCategory(
				osthread,
				0,
				J9THREAD_PRIORITY_MIN,
				0,
				threadmain,
				(void *)doneflag,
				J9THREAD_CATEGORY_SYSTEM_THREAD)
	) {
		J9AbstractThread *thr = *(J9AbstractThread **)osthread;
		volatile UDATA *flags = &(thr->flags);

		while ((*flags & J9THREAD_FLAG_STARTED) == 0) {
			omrthread_yield();
		}

		++numOSThreads;
		rc = 0;
	}
	return rc;
}

static void
cleanupOSThread(omrthread_t *osthread, volatile IDATA *doneflag)
{
    while (*doneflag == 0) {
    	omrthread_yield();
    }

	*osthread = NULL;
	--numOSThreads;
}

/**
 * Allocate an object using JNI.  Must be called without VM access.
 * @param env Java environment
 * @param objectRef (out) Pointer to jobject to receive the the result.
 * @return 0 on success, non-zero otherwise.
 */
static IDATA
allocateObject(JNIEnv *env, jobject *objectRef)
{
	J9VMThread *vmthread = (J9VMThread *)env;
	PORT_ACCESS_FROM_ENV(env);
	jclass clazz;
	jobject javaObject;

	clazz = (*env)->FindClass(env,"java/lang/Object");
	javaObject = (*env)->AllocObject(env, clazz);
	/*
	 * prevent the object from being deleted.
	 */
	*objectRef = (*env)->NewGlobalRef(env, javaObject);
	if (!objectRef) {
		j9tty_printf(PORTLIB, "initObject: AllocObject failed\n");
		return -1;
	}
	return 0;
}

/**
 * Create a pointer to the j9object from a jobject and insert an object monitor.  Must be called with VM access.
 * @param env Java environment
 * @param objectRef jobject
 * @param obj (out) Pointer to j9object_t to receive the result.
 * @return 0 on success, non-zero otherwise.
 */
static IDATA
initObject(JNIEnv *env, j9object_t *obj, jobject objectRef, J9ObjectMonitor **objmon)
{
	J9VMThread *vmthread = (J9VMThread *)env;
	PORT_ACCESS_FROM_ENV(env);
	*obj = J9_JNI_UNWRAP_REFERENCE(objectRef);
	
	if (objmon) {
		*objmon = vmthread->javaVM->internalVMFunctions->monitorTableAt(vmthread, *obj);
	}
	return 0;
}

/**
 * Destroy the reference to an object and update the associated variables.  Must be called without VM access.
 * @param env Java environment
 * @param objectRef jobject
 * @param obj (out) Pointer to j9object_t to receive the result.
 */
static void
cleanupObject(JNIEnv *env, j9object_t *obj, jobject *objectRef)
{
	PORT_ACCESS_FROM_ENV(env);

	*obj = NULL;
	if (NULL != *objectRef) {
		(*env)->DeleteGlobalRef(env, *objectRef);
	}
	*objectRef = NULL;
}

static IDATA
initMonitor(JNIEnv *env, J9ObjectMonitor **monitor)
{
	IDATA rc = -1;
	PORT_ACCESS_FROM_ENV(env);
	
	*monitor = j9mem_allocate_memory(sizeof(J9ObjectMonitor), OMRMEM_CATEGORY_THREADS);
	if (*monitor) {
		rc = omrthread_monitor_init_with_name(&(*monitor)->monitor, 0, "thrstatetest");
	}
	return rc;
}

static void
cleanupMonitor(JNIEnv *env, J9ObjectMonitor *monitor)
{
	J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)monitor->monitor;
	PORT_ACCESS_FROM_ENV(env);

	mon->owner = NULL;
	mon->count = 0;
	omrthread_monitor_destroy(monitor->monitor);
	j9mem_free_memory(monitor);
}

void
setJ9ThreadState(omrthread_t osthread, UDATA flags, omrthread_monitor_t blocker, omrthread_t owner, UDATA count)
{
	J9AbstractThread *thr = (J9AbstractThread *)osthread;
	
	if (thr) {
		thr->flags = flags;
		thr->monitor = blocker;
	}
	setJ9MonitorState(blocker, owner, count);
}

void
setVMThreadState(J9VMThread *vmthread, omrthread_t osThread, UDATA publicFlags, 
		j9object_t blockingEnterObject, UDATA lockWord)
{
	vmthread->osThread = osThread;
	vmthread->publicFlags = publicFlags;
	vmthread->blockingEnterObject = blockingEnterObject;
	if (blockingEnterObject) {
		J9OBJECT_SET_MONITOR(vmthread, blockingEnterObject, (j9objectmonitor_t)lockWord);
	}
}

void
setJ9MonitorState(omrthread_monitor_t monitor, omrthread_t owner, UDATA count)
{
	J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)monitor;
	
	if (mon) {
		mon->owner = owner;
		mon->count = count;
	}
}

UDATA
getFlatLock(J9VMThread *owner, UDATA count)
{
	UDATA flatlock;
	
	count <<= OBJECT_HEADER_LOCK_RECURSION_OFFSET;
	flatlock = (UDATA)owner | count;
	return flatlock;
}

UDATA
getInflLock(J9ObjectMonitor *monitor)
{
	UDATA lockword;
	
	lockword = (UDATA)monitor | OBJECT_HEADER_LOCK_INFLATED;
	return lockword;
}

/* test cases return 0 on success */
UDATA
checkObjAnswers(JNIEnv *env, J9VMThread *vmthread, UDATA exp_vmstate, j9object_t exp_lockObj, J9VMThread *exp_owner, UDATA exp_count)
{
	PORT_ACCESS_FROM_ENV(env);
	UDATA rc = 0;
	UDATA vmstate;
	j9object_t lockObj;
	J9VMThread *owner;
	UDATA count;
	
	vmstate = getVMThreadObjectState(vmthread, &lockObj, &owner, &count);
	
	if (vmstate != exp_vmstate) rc = FAILED_OBJSTATE;
	if (lockObj != exp_lockObj)	rc = FAILED_OBJSTATE;
	if (owner != exp_owner) rc = FAILED_OBJSTATE;
	if (count != exp_count) rc = FAILED_OBJSTATE;

	if (rc) {
		j9tty_printf(PORTLIB, "getVMThreadObjectState %s:\n", (rc == 0)? "passed" : "failed");
		j9tty_printf(PORTLIB, "\tactual: vmstate %d lockObj %p lockOwner %p count %d\n", vmstate, lockObj, owner, count);
		j9tty_printf(PORTLIB, "\texpect: vmstate %d lockObj %p lockOwner %p count %d\n", exp_vmstate, exp_lockObj, exp_owner, exp_count);
	}
	return rc;
}

UDATA
checkRawAnswers(JNIEnv *env, J9VMThread *vmthread, UDATA exp_vmstate, j9object_t exp_lockObj, omrthread_monitor_t exp_rawLock, J9VMThread *exp_owner, UDATA exp_count)
{
	PORT_ACCESS_FROM_ENV(env);
	UDATA rc = 0;
	UDATA vmstate;
	j9object_t lockObj;
	omrthread_monitor_t rawLock;
	J9VMThread *owner;
	UDATA count;
	
	vmstate = getVMThreadRawState(vmthread, &lockObj, &rawLock, &owner, &count);
	
	if (vmstate != exp_vmstate) rc = FAILED_RAWSTATE;
	if (lockObj != exp_lockObj)	rc = FAILED_RAWSTATE;
	if (rawLock != exp_rawLock) rc = FAILED_RAWSTATE;
	if (owner != exp_owner) rc = FAILED_RAWSTATE;
	if (count != exp_count) rc = FAILED_RAWSTATE;

	if (rc) {
		j9tty_printf(PORTLIB, "getVMThreadRawState %s:\n", (rc == 0)? "passed" : "failed");
		j9tty_printf(PORTLIB, "\tactual: vmstate %d lockObj %p rawLock %p lockOwner %p count %d\n", vmstate, lockObj, rawLock, owner, count);
		j9tty_printf(PORTLIB, "\texpect: vmstate %d lockObj %p rawLock %p lockOwner %p count %d\n", exp_vmstate, exp_lockObj, exp_rawLock, exp_owner, exp_count);
	}
	return rc;
}

UDATA
checkOldAnswers(JNIEnv *env, J9VMThread *vmthread, UDATA exp_vmstate, omrthread_monitor_t exp_rawLock, J9VMThread *exp_owner, UDATA exp_count)
{
	PORT_ACCESS_FROM_ENV(env);
	UDATA rc = 0;
	UDATA vmstate;
	omrthread_monitor_t rawLock;
	J9VMThread *owner;
	UDATA count;
	
	vmstate = getVMThreadStatus_DEPRECATED(vmthread, (J9ThreadAbstractMonitor **)&rawLock, &owner, &count);
	
	if (vmstate != exp_vmstate) rc = FAILED_OLDSTATE;
	if (rawLock != exp_rawLock) rc = FAILED_OLDSTATE;
	if (owner != exp_owner) rc = FAILED_OLDSTATE;
	if (count != exp_count) rc = FAILED_OLDSTATE;

	if (rc) {
		j9tty_printf(PORTLIB, "getVMThreadStatus %s:\n", (rc == 0)? "passed" : "failed");
		j9tty_printf(PORTLIB, "\tactual: vmstate %d rawLock %p lockOwner %p count %d\n", vmstate, rawLock, owner, count);
		j9tty_printf(PORTLIB, "\texpect: vmstate %d rawLock %p lockOwner %p count %d\n", exp_vmstate, exp_rawLock, exp_owner, exp_count);
	}
	return rc;
}

UDATA
test_setup(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{
	PORT_ACCESS_FROM_ENV(env);

	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	if (j9sem_init(&keepAliveSem, 0)) {
		j9tty_printf(PORTLIB, "test_setup: unable to create semaphore\n");
		j9exit_shutdown_and_exit(-1);
	}
	
	if (initOSThread(env, &TESTDATA(selfOsthread), &TESTDATA(selfOsthreadDone))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create osthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initOSThread(env, &TESTDATA(otherOsthread), &TESTDATA(otherOsthreadDone))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create osthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initOSThread(env, &TESTDATA(unattachedOsthread), &TESTDATA(unattachedOsthreadDone))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create osthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initOSThread(env, &TESTDATA(osthread3), &TESTDATA(thirdOsthreadDone))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create osthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initVMThread(env, &TESTDATA(selfVmthread), TESTDATA(selfOsthread))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create vmthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initVMThread(env, &TESTDATA(otherVmthread), TESTDATA(otherOsthread))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create vmthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initVMThread(env, &TESTDATA(vmthread3), TESTDATA(osthread3))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create vmthread\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initMonitor(env, &TESTDATA(rawMonitor))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create monitor\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (allocateObject(env, &TESTDATA(blockingObjectRef))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create object\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (allocateObject(env, &TESTDATA(objectNoMonitorRef))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create object\n");
		j9exit_shutdown_and_exit(-1);
	}

	j9tty_printf(PORTLIB, "acquiring VM access\n");
	vmFuncs->internalEnterVMFromJNI(currentThread); /* prevent the GC from moving objects */

	if (initObject(env, &TESTDATA(blockingObject), TESTDATA(blockingObjectRef), &TESTDATA(objMonitor))) {
		j9tty_printf(PORTLIB, "test_setup: unable to create object\n");
		j9exit_shutdown_and_exit(-1);
	}

	if (initObject(env, &TESTDATA(objectNoMonitor), TESTDATA(objectNoMonitorRef), NULL)) {
		j9tty_printf(PORTLIB, "test_setup: unable to create object\n");
		j9exit_shutdown_and_exit(-1);
	}
	
	saveTestDataDesc(env);
	return 0;
}

UDATA
test_cleanup(JNIEnv *env, BOOLEAN ignoreExpectedFailures)
{

	PORT_ACCESS_FROM_ENV(env);

	IDATA i;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	cleanupVMThread(env, TESTDATA(selfVmthread));
	cleanupVMThread(env, TESTDATA(otherVmthread));
	cleanupVMThread(env, TESTDATA(vmthread3));

	for (i = 0; i < numOSThreads; ++i) {
		j9sem_post(keepAliveSem);
	}
	
	cleanupOSThread(&TESTDATA(selfOsthread), &TESTDATA(selfOsthreadDone));
	cleanupOSThread(&TESTDATA(otherOsthread), &TESTDATA(otherOsthreadDone));
	cleanupOSThread(&TESTDATA(unattachedOsthread), &TESTDATA(unattachedOsthreadDone));
	cleanupOSThread(&TESTDATA(osthread3), &TESTDATA(thirdOsthreadDone));

	cleanupMonitor(env, TESTDATA(rawMonitor));

	j9tty_printf(PORTLIB, "releasing VM access\n");
	vmFuncs->internalExitVMToJNI(currentThread); /* release access so we can use JNI */
	cleanupObject(env, &TESTDATA(blockingObject), &TESTDATA(blockingObjectRef));

	cleanupObject(env, &TESTDATA(objectNoMonitor), &TESTDATA(objectNoMonitorRef));
	
	j9sem_destroy(keepAliveSem);
	return 0;
}

static UDATA
bufferTestDataDesc(JNIEnv *env, char *buf, UDATA buflen)
{
	UDATA rc = 0;
	PORT_ACCESS_FROM_ENV(env);

	rc += j9str_printf(PORTLIB, buf, buflen,
			"Test Data\n"
			"\tself  vmthread: %p osthread: %p\n"
			"\tother vmthread: %p osthread: %p\n"
			"\t#3    vmthread: %p osthread: %p\n"
			"\tunattached osthread: %p\n"
			"\tblockingObject: %p\n"
			"\tobjMonitor: %p->%p\n"
			"\trawMonitor: %p->%p\n"
			"\tobjectNoMonitor: %p\n"
			"\tfake monitor count: %d\n",
			TESTDATA(selfVmthread), TESTDATA(selfOsthread),
			TESTDATA(otherVmthread), TESTDATA(otherOsthread),
			TESTDATA(vmthread3), TESTDATA(osthread3),
			TESTDATA(unattachedOsthread),
			TESTDATA(blockingObject),
			TESTDATA(objMonitor), TESTDATA(objMonitor)->monitor,
			TESTDATA(rawMonitor), TESTDATA(rawMonitor)->monitor,
			TESTDATA(objectNoMonitor),
			MON_COUNT);
	return rc;
}

static void
saveTestDataDesc(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	UDATA buflen;
	
	buflen = bufferTestDataDesc(env, NULL, 0);
	testDataDesc = j9mem_allocate_memory(sizeof(char) * buflen, OMRMEM_CATEGORY_THREADS);
	bufferTestDataDesc(env, testDataDesc, buflen * sizeof(char));
}

void
printTestData(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);

	if (testDataDesc) {
		j9tty_printf(PORTLIB, testDataDesc);
	}
}

void
cleanupTestData(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);

	j9mem_free_memory(testDataDesc);
	testDataDesc = NULL;
}
