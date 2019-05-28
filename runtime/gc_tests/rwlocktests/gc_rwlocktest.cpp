/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "CuTest.h"
#include "j9.h"
#include "omrutil.h"
#include "thread_api.h"

#include "LightweightNonReentrantReaderWriterLock.hpp"

#define MILLI_TIMEOUT	10000
#define NANO_TIMEOUT	0

#define STEP_MILLI_TIMEOUT		600000
#define STEP_NANO_TIMEOUT		0


extern J9PortLibrary *sharedPortLibrary;

MM_LightweightNonReentrantReaderWriterLock g_lock;
BOOLEAN									   g_isInitialized = FALSE;

/* structure used to pass info to concurrent threads for some tests */
typedef struct SupportThreadInfo {
	MM_LightweightNonReentrantReaderWriterLock* lock;
	omrthread_monitor_t 			synchronization;
	omrthread_entrypoint_t*	    functionsToRun;
	UDATA			   			numberFunctions;
	volatile UDATA	   			readCounter;
	volatile UDATA	   			writeCounter;
	volatile BOOLEAN			done;
}SupportThreadInfo ;

/* forward declarations */
void freeSupportThreadInfo(SupportThreadInfo* info);

/**
 * This method is called to run the set of steps that will be run on a thread
 * @param info SupportThreadInfo structure that contains the information for the steps and other parameters
 *        that are used
 */
static IDATA J9THREAD_PROC
runRequest(SupportThreadInfo* info)
{
	IDATA result = 0;
	UDATA i =0;

	omrthread_monitor_enter(info->synchronization);
	omrthread_monitor_exit(info->synchronization);
	for (i=0;i<info->numberFunctions;i++){
		result = info->functionsToRun[i]((void*)info);
		omrthread_monitor_enter(info->synchronization);
		omrthread_monitor_notify(info->synchronization);
		if (info->done == TRUE){
			omrthread_exit(info->synchronization);
			break;
		}
		omrthread_monitor_wait_interruptable(info->synchronization,STEP_MILLI_TIMEOUT,STEP_NANO_TIMEOUT);
		omrthread_monitor_exit(info->synchronization);
	}

	return result;
}

/**
 * This method is called to create a SupportThreadInfo for a test.  It will populate the monitor and
 * lock being used and zero out the counter
 *
 * @param functionsToRun an array of functions pointers. Each function will be run one in sequence synchronized
 *        using the monitor within the SupportThreadInfo
 * @param numberFunctions the number of functions in the functionsToRun array
 * @returns a pointer to the newly created SupportThreadInfo
 */
SupportThreadInfo*
createSupportThreadInfo(omrthread_entrypoint_t* functionsToRun, UDATA numberFunctions){
	PORT_ACCESS_FROM_PORT(sharedPortLibrary);
	SupportThreadInfo* info = (SupportThreadInfo*) j9mem_allocate_memory(sizeof(SupportThreadInfo), OMRMEM_CATEGORY_THREADS);
	info->readCounter = 0;
	info->writeCounter = 0;
	info->functionsToRun = functionsToRun;
	info->numberFunctions = numberFunctions;
	info->done = FALSE;
	info->lock = &g_lock;
	if (!g_isInitialized) {
		info->lock->initialize(128);
		g_isInitialized = TRUE;
	}

	omrthread_monitor_init_with_name(&info->synchronization, 0, "supportThreadAInfo monitor");
	return info;
}

/**
 * This method free the internal structures and memory for a SupportThreadInfo
 * @param info the SupportThreadInfo instance to be freed
 */
void
freeSupportThreadInfo(SupportThreadInfo* info){
	PORT_ACCESS_FROM_PORT(sharedPortLibrary);
	if (info->synchronization != NULL){
		omrthread_monitor_destroy(info->synchronization);
	}

	if (g_isInitialized) {
		info->lock->tearDown();
		g_isInitialized = FALSE;
	}
	j9mem_free_memory(info);
}

/**
 * This method is called to push the concurrent thread to run the next function
 */
void
triggerNextStepWithStatus(SupportThreadInfo* info, BOOLEAN done){
	omrthread_monitor_enter(info->synchronization);
	info->done = done;
	omrthread_monitor_notify(info->synchronization);
	omrthread_monitor_wait_interruptable(info->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
}

/**
 * This method is called to push the concurrent thread to run the next function
 */
void
triggerNextStep(SupportThreadInfo* info){
	triggerNextStepWithStatus(info,FALSE);
}

/**
 * This method is called to push the concurrent thread to run the next function
 * and tell the thread that the test is done
 */
void
triggerNextStepDone(SupportThreadInfo* info){
	triggerNextStepWithStatus(info,TRUE);
}

/**
 * This method starts a concurrent thread and runs the functions specified in the
 * SupportThreadInfo passed in.  It only returns once the first function has run
 *
 * @param info SupportThreadInfo structure containing the functions and lock for the tests
 * @returns 0 on success
 */
IDATA
startConcurrentThread(SupportThreadInfo* info){
	omrthread_t newThread = NULL;

	omrthread_monitor_enter(info->synchronization);
	createThreadWithCategory(
			&newThread,
			0,			/* default stack size */
			J9THREAD_PRIORITY_NORMAL,
			0,			/* start immediately */
			(omrthread_entrypoint_t) runRequest,
			(void*) info,
			J9THREAD_CATEGORY_SYSTEM_GC_THREAD);

	omrthread_monitor_wait_interruptable(info->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);

	return 0;
}

/***********************************************
 * Functions that can be used as steps in concurrent threads
 * for a test
 ************************************************/
/**
 * This step enters the lock in the SupportThreadInfo for read
 * @param info the SupportThreadInfo which can be used by the step
 */
static IDATA J9THREAD_PROC
enter_rwlock_read(SupportThreadInfo* info)
{
	info->lock->enterRead();
	info->readCounter++;
	return 0;
}

/**
 * This step exits the lock in the SupportThreadInfo for read
 * @param info the SupportThreadInfo which can be used by the step
 */
static IDATA J9THREAD_PROC
exit_rwlock_read(SupportThreadInfo* info)
{
	info->lock->exitRead();
	info->readCounter--;
	return 0;
}

/**
 * This step enters the lock in the SupportThreadInfo for write
 * @param info the SupportThreadInfo which can be used by the step
 */
static IDATA J9THREAD_PROC
enter_rwlock_write(SupportThreadInfo* info)
{
	info->lock->enterWrite();
	info->writeCounter++;
	return 0;
}

/**
 * This step exits the lock in the SupportThreadInfo for write
 * @param info the SupportThreadInfo which can be used by the step
 */
static IDATA
J9THREAD_PROC exit_rwlock_write(SupportThreadInfo* info)
{
	info->lock->exitWrite();
	info->writeCounter--;
	return 0;
}



/**
 * validate that we can initialize a LightweightNonReentrantReaderWriterLock successfully
 */
void
Test_RWLock_InitializeTest(CuTest *tc)
{
	IDATA result;
	MM_LightweightNonReentrantReaderWriterLock lock;
	/* initialize */
	result = lock.initialize(128);
	CuAssertTrue(tc, 0 == result);
	/* clean up */
	result = lock.tearDown();
	CuAssertTrue(tc, 0 == result);
}

/**
 * Validate that we can enter/exit a LightweightNonReentrantReaderWriterLock for read
 */
void
Test_RWLock_RWReadEnterExitTest(CuTest *tc)
{
	IDATA result;
	MM_LightweightNonReentrantReaderWriterLock lock;
	/* initialize */
	result = lock.initialize(128);
	CuAssertTrue(tc, 0 == result);
	/* enterRead */
	result = lock.enterRead();
	CuAssertTrue(tc, 0 == result);
	/* exitRead */
	result = lock.exitRead();
	CuAssertTrue(tc, 0 == result);
	/* clean up */
	result = lock.tearDown();
	CuAssertTrue(tc, 0 == result);
}

/**
 * Validate that we can enter/exit a LightweightNonReentrantReaderWriterLock for write
 */
void
Test_RWLock_RWWriteEnterExitTest(CuTest *tc)
{
	IDATA result;
	MM_LightweightNonReentrantReaderWriterLock lock;
	/* initialize */
	result = lock.initialize(128);
	CuAssertTrue(tc, 0 == result);
	/* enterWrite */
	result = lock.enterWrite();
	CuAssertTrue(tc, 0 == result);
	/* exitWrite */
	result = lock.exitWrite();
	CuAssertTrue(tc, 0 == result);
	/* clean up */
	result = lock.tearDown();
	CuAssertTrue(tc, 0 == result);
}

/**
 * Validate threads can shared the LightweightNonReentrantReaderWriterLock for read
 */
void
Test_RWLock_multipleReadersTest(CuTest *tc)
{

	/* Start concurrent thread acquired the LightweightNonReentrantReaderWriterLock for read */
	SupportThreadInfo* info;
	omrthread_entrypoint_t  functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwlock_read;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwlock_read;
	info = createSupportThreadInfo(functionsToRun, 2);
	startConcurrentThread(info);

	/* Validate that we can acquire the lock for read as well */
	CuAssertTrue(tc, 1 == info->readCounter);
	enter_rwlock_read(info);
	CuAssertTrue(tc, 2 == info->readCounter);
	exit_rwlock_read(info);
	CuAssertTrue(tc, 1 == info->readCounter);

	/* ok we were not blocked by the other thread holding the LightweightNonReentrantReaderWriterLock for read
	 * so ask it to release the lock
	 */
	triggerNextStepDone(info);
	CuAssertTrue(tc, 0 == info->readCounter);
	freeSupportThreadInfo(info);
}

/**
 * Validates the following
 *
 * readers are excludes while another thread holds the LightweightNonReentrantReaderWriterLock for write
 * once writer exits, reader can enter
 */
void
Test_RWLock_readersExcludedTest(CuTest *tc)
{
	SupportThreadInfo* info;
	omrthread_entrypoint_t  functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwlock_read;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwlock_read;
	info = createSupportThreadInfo(functionsToRun, 2);
	/* first enter the lock for write */
	CuAssertTrue(tc, 0 == info->readCounter);
	enter_rwlock_write(info);
	CuAssertTrue(tc, 1 == info->writeCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	CuAssertTrue(tc, 0 == info->readCounter);

	/* now release the lock and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	exit_rwlock_write(info);
	CuAssertTrue(tc, 0 == info->writeCounter);
	omrthread_monitor_wait_interruptable(info->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	CuAssertTrue(tc, 1 == info->readCounter);
	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	CuAssertTrue(tc, 0 == info->readCounter);
	freeSupportThreadInfo(info);

}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the LightweightNonReentrantReaderWriterLock for read
 * once reader exits writer can enter
 */
void
Test_RWLock_writersExcludedTest(CuTest *tc)
{
	SupportThreadInfo* info;
	SupportThreadInfo* infoReader;
	omrthread_entrypoint_t  functionsToRun[2];
	omrthread_entrypoint_t  functionsToRunReader[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwlock_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwlock_write;
	functionsToRunReader[0] = (omrthread_entrypoint_t) &enter_rwlock_read;
	functionsToRunReader[1] = (omrthread_entrypoint_t) &exit_rwlock_read;
	info = createSupportThreadInfo(functionsToRun, 2);
	infoReader = createSupportThreadInfo(functionsToRunReader, 2);

	/* start the thread that will enter for read */
	startConcurrentThread(infoReader);
	CuAssertTrue(tc, 1 == infoReader->readCounter);

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	CuAssertTrue(tc, 0 == info->writeCounter);

	/* now ask the thread that as entered for read to exit we know that for RT
	 * it cannot complete until the writer exist but it will have released the lock
	 */
	omrthread_monitor_enter(info->synchronization);
	triggerNextStepDone(infoReader);
	omrthread_monitor_wait_interruptable(info->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	CuAssertTrue(tc, 1 == info->writeCounter);

	/* now ask the thread that should now be entered write to exit */
	omrthread_monitor_enter(infoReader->synchronization);
	triggerNextStepDone(info);
	omrthread_monitor_wait_interruptable(infoReader->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader->synchronization);
	CuAssertTrue(tc, 0 == info->writeCounter);
	CuAssertTrue(tc, 0 == infoReader->readCounter);

	/* done now clean up */
	freeSupportThreadInfo(info);
	freeSupportThreadInfo(infoReader);
}

/**
 * validates the following
 *
 * writer is excluded while another thread holds the lock for write
 * once writer exits second writer can enter
 */
void
Test_RWLock_writerExcludesWriterTest(CuTest *tc)
{
	SupportThreadInfo* info;
	omrthread_entrypoint_t  functionsToRun[2];
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwlock_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwlock_write;
	info = createSupportThreadInfo(functionsToRun, 2);
	/* first enter the lock for write */
	CuAssertTrue(tc, 0 == info->writeCounter);
	info->lock->enterWrite();

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	CuAssertTrue(tc, 0 == info->writeCounter);

	/* now release the lock and validate that the thread enters it */
	omrthread_monitor_enter(info->synchronization);
	info->lock->exitWrite();
	omrthread_monitor_wait_interruptable(info->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	CuAssertTrue(tc, 1 == info->writeCounter);

	/* done now so ask thread to release and clean up */
	triggerNextStepDone(info);
	freeSupportThreadInfo(info);
}

/* validates the following
 *
 * the new reader will be excluded if the writer is pending (waiting on another reader)
 * 2nd reader to enter the lock after all of writer threads exit
 */
void
Test_RWLock_waitingWriterExcludes2ndReader(CuTest *tc)
{
	SupportThreadInfo* info;
	SupportThreadInfo* infoReader;
	SupportThreadInfo* infoReader2;
	omrthread_entrypoint_t  functionsToRun[2];
	omrthread_entrypoint_t  functionsToRunReader[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRun[0] = (omrthread_entrypoint_t) &enter_rwlock_write;
	functionsToRun[1] = (omrthread_entrypoint_t) &exit_rwlock_write;
	functionsToRunReader[0] = (omrthread_entrypoint_t) &enter_rwlock_read;
	functionsToRunReader[1] = (omrthread_entrypoint_t) &exit_rwlock_read;

	info = createSupportThreadInfo(functionsToRun, 2);
	infoReader = createSupportThreadInfo(functionsToRunReader, 2);
	infoReader2 = createSupportThreadInfo(functionsToRunReader, 2);

	/* SupportThreadInfo structures use the same lock (g_lock) */

	/* start the first concurrent thread that will enter for read */
	startConcurrentThread(infoReader2);
	CuAssertTrue(tc, 1 == infoReader2->readCounter);

	/* start the concurrent thread that will try to enter for write and
	 * check that it is blocked
	 */
	startConcurrentThread(info);
	CuAssertTrue(tc, 0 == info->writeCounter);

	/* start the second concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader);
	CuAssertTrue(tc, 0 == info->readCounter);

	/* now ask the first reader to exit so that the write can enter */
	omrthread_monitor_enter(info->synchronization);
	triggerNextStep(infoReader2);
	omrthread_monitor_wait_interruptable(info->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(info->synchronization);
	CuAssertTrue(tc, 1 == info->writeCounter);

	/* now let the writer exit so the second reader can enter */
	CuAssertTrue(tc, 0 == infoReader->readCounter);
	omrthread_monitor_enter(infoReader->synchronization);
	triggerNextStepDone(info);
	omrthread_monitor_wait_interruptable(infoReader->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader->synchronization);
	CuAssertTrue(tc, 1 == infoReader->readCounter);
	CuAssertTrue(tc, 0 == info->writeCounter);

	/* ok now let second reader exit */
	triggerNextStepDone(infoReader);
	CuAssertTrue(tc, 0 == infoReader->readCounter);

	/* now let the threads clean up. */
	freeSupportThreadInfo(info);
	freeSupportThreadInfo(infoReader);
	freeSupportThreadInfo(infoReader2);
}
/**
 * validates the following
 *
 * readers are excluded while another thread holds the lock for write
 * once writer exits, all readers wake up and can enter
 */
void
Test_RWLock_AllReadersProceedTest(CuTest *tc)
{
	SupportThreadInfo* infoReader1;
	SupportThreadInfo* infoReader2;
	omrthread_entrypoint_t  functionsToRunReader1[2];
	omrthread_entrypoint_t  functionsToRunReader2[2];

	/* set up the steps for the 2 concurrent threads */
	functionsToRunReader1[0] = (omrthread_entrypoint_t) &enter_rwlock_read;
	functionsToRunReader1[1] = (omrthread_entrypoint_t) &exit_rwlock_read;
	functionsToRunReader2[0] = (omrthread_entrypoint_t) &enter_rwlock_read;
	functionsToRunReader2[1] = (omrthread_entrypoint_t) &exit_rwlock_read;

	infoReader1 = createSupportThreadInfo(functionsToRunReader1, 2);
	infoReader2 = createSupportThreadInfo(functionsToRunReader2, 2);

	/* two SupportThreadInfo structures use the same lock(g_lock) */

	/* first enter the lock for write */
	CuAssertTrue(tc, 0 == infoReader1->readCounter);
	CuAssertTrue(tc, 0 == infoReader2->readCounter);
	infoReader1->lock->enterWrite();
	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader1);
	CuAssertTrue(tc, 0 == infoReader1->readCounter);

	/* start the concurrent thread that will try to enter for read and
	 * check that it is blocked
	 */
	startConcurrentThread(infoReader2);
	CuAssertTrue(tc, 0 == infoReader2->readCounter);

	/* now release the lock and validate that the second readers still excludes the writer */
	omrthread_monitor_enter(infoReader2->synchronization);
	omrthread_monitor_enter(infoReader1->synchronization);

	infoReader1->lock->exitWrite();

	/* now validate that the readers have entered the lock*/
	omrthread_monitor_wait_interruptable(infoReader1->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader1->synchronization);
	omrthread_monitor_wait_interruptable(infoReader2->synchronization,MILLI_TIMEOUT,NANO_TIMEOUT);
	omrthread_monitor_exit(infoReader2->synchronization);

	CuAssertTrue(tc, 1 == infoReader1->readCounter);
	CuAssertTrue(tc, 1 == infoReader2->readCounter);

	/* ok now let the readers exit */
	triggerNextStepDone(infoReader1);
	CuAssertTrue(tc, 0 == infoReader1->readCounter);
	triggerNextStepDone(infoReader2);
	CuAssertTrue(tc, 0 == infoReader2->readCounter);

	/* now let the threads clean up. */
	freeSupportThreadInfo(infoReader1);
	freeSupportThreadInfo(infoReader2);
}

CuSuite
*GetRWLockTestSuite()
{
	CuSuite *suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, Test_RWLock_InitializeTest);
	SUITE_ADD_TEST(suite, Test_RWLock_RWReadEnterExitTest);
	SUITE_ADD_TEST(suite, Test_RWLock_RWWriteEnterExitTest);
	SUITE_ADD_TEST(suite, Test_RWLock_multipleReadersTest);
	SUITE_ADD_TEST(suite, Test_RWLock_readersExcludedTest);
	SUITE_ADD_TEST(suite, Test_RWLock_writersExcludedTest);
	SUITE_ADD_TEST(suite, Test_RWLock_waitingWriterExcludes2ndReader);
	SUITE_ADD_TEST(suite, Test_RWLock_writerExcludesWriterTest);
	SUITE_ADD_TEST(suite, Test_RWLock_AllReadersProceedTest);
	return suite;
}

