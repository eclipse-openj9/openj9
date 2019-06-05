/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

/*
 * $RCSfile: j9timeTest.c,v $
 * $Revision: 1.55 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library timer operations.
 * 
 * Exercise the API for port library timer operations.  These functions 
 * can be found in the file @ref j9time.c  
 * 
 * @note port library string operations are not optional in the port library table.
 * 
 */
#include <stdlib.h>
#include <string.h>
#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif /* defined(WIN32) || defined(WIN64) */

#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"


static int J9THREAD_PROC nanoTimeDirectionTest(void *portLibrary);

/**
 * @internal
 * @def
 * The interval to run the time tests
 * @note Must be at least 100
 * @note Assume 1~=10ms
 */
#define TEST_DURATION 1000

/**
 * @internal
 * @def
 * Number of time intervals to test 
 * @note (1ms, 2ms, 3ms, ... J9TIME_REPEAT_TESTms)
 */
#define J9TIME_REPEAT_TEST 5

/**
 * Verify port library timer operations.
 * 
 * Ensure the library table is properly setup to run timer tests.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9time_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9time_test0";

	reportTestEntry(portLibrary, testName);
	 
	/* Verify that the time function pointers are non NULL */
	
	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibrary, it is not re-entrant safe
	 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_shutdown is NULL\n");
	}

	/* j9time_test1, j9time_test3 */	
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_msec_clock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_msec_clock is NULL\n");
	}

	/* j9time_test1, j9time_test3 */	
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_usec_clock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_usec_clock is NULL\n");
	}

	/* j9time_test1, j9time_test3 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_current_time_millis) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_current_time_millis is NULL\n");
	}

	/* j9time_test1, j9time_test3 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_hires_clock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_hires_clock is NULL\n");
	}

	/* j9time_test1, j9time_test3 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_hires_frequency) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_hires_frequency is NULL\n");
	}

	/* j9time_test1, j9time_test2, j9time_test3 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->time_hires_delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_hires_delta is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library timer operations.
 * 
 * Ensure that clocks are advancing.  Accuracy is not verified.
 * 
 * Functions verified by this test:
 * @arg @ref j9time.c::j9time_msec_clock "j9time_msec_clock()"
 * @arg @ref j9time.c::j9time_usec_clock "j9time_usec_clock()"
 * @arg @ref j9time.c::j9time_hires_clock "j9time_hires_clock()"
 * @arg @ref j9time.c::j9time_hires_frequency "j9time_hires_frequency()"
 * @arg @ref j9time.c::j9time_hires_delta "j9time_hires_delta()"
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9time_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9time_test1";

	/* get a thread so that we can use native time delays (the original solution would spin and fail on fast machines) */
	omrthread_t self;

	reportTestEntry(portLibrary, testName);

	/* Verify the current time is advancing */
	/* attach the thread so we can use the delay primitives */
	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		/* success in starting up thread library and attaching */
		I_64 timeStart, timeCheck; /* current time in millis */
		UDATA mtimeStart, mtimeCheck;
		UDATA utimeStart, utimeCheck;
		I_64 ntimeStart, ntimeCheck;
		
		timeStart = j9time_current_time_millis();
		mtimeStart = j9time_msec_clock();
		utimeStart = j9time_usec_clock();
		ntimeStart = j9time_nano_time();
		/* sleep for half a second */
		omrthread_sleep(500);
		timeCheck = j9time_current_time_millis();
		mtimeCheck = j9time_msec_clock();
		utimeCheck = j9time_usec_clock();
		ntimeCheck = j9time_nano_time();
		
		/* print errors if any of these failed */
		if (timeCheck == timeStart) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_current_time_millis did not change after a half-second forced delay\n");
		}
		if (mtimeStart == mtimeCheck) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_msec_clock did not change after a half-second forced delay\n");
		}
		if (utimeStart == utimeCheck) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_usec_clock did not change after a half-second forced delay\n");
		}
		if (ntimeStart == ntimeCheck) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_nano_time did not change after a half-second forced delay\n");
		}
		
		/* see if we can run the high-res test */
		if (0 != j9time_hires_frequency()) {
			/* timer is valid so run the tests */
			U_64 hiresTimeStart, hiresTimeCheck;
			
			hiresTimeStart = j9time_hires_frequency();
			/* sleep for half a second */
			omrthread_sleep(500);
			hiresTimeCheck = j9time_hires_frequency();
			
			if (hiresTimeCheck == hiresTimeStart) {
				/* we can run the tests since the timer is stable */
				hiresTimeStart = j9time_hires_clock();
				/* sleep for half a second */
				omrthread_sleep(500);
				hiresTimeCheck = j9time_hires_clock();
				if (hiresTimeCheck == hiresTimeStart) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_clock has not advanced after a half-second forced delay\n");
				}
			} else {
				/* the timer advanced so don't even try to run the test */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_frequency has advanced\n");
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_frequency is 0, timer will not advance\n");
		}
	} else {
		/* failure initializing thread library: we won't be able to test reliably */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_attach failed in j9time_test1 so testing is not possible\n");
	}

	/* Verify that hires timer can advance */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library timer operations.
 * 
 * Verify that hires timer properly handles rollover.
 * 
 * Functions verified by this test:
 * @arg @ref j9time.c::j9time_hires_delta "j9time_hires_delta()"
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9time_test2(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9time_test2";

	U_64 hiresTimeStart, hiresTimeStop;
	U_64 simulatedValue, expectedValue;
	const I_32 timeInterval = 5;

	reportTestEntry(portLibrary, testName);

	/* The time interval being simulated */
	expectedValue = timeInterval * j9time_hires_frequency();

	/* start < stop */
	simulatedValue = j9time_hires_delta(0, expectedValue, j9time_hires_frequency());
	if (simulatedValue != expectedValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_delta returned %llu expected %llu\n", simulatedValue, expectedValue);
	}

	/* start > stop (add one to the expected value for 0, we can live with it ...*/
	hiresTimeStart = ((U_64)-1) - ((timeInterval-2)*j9time_hires_frequency());
	hiresTimeStop = 2*j9time_hires_frequency();
	simulatedValue = j9time_hires_delta(hiresTimeStart, hiresTimeStop, j9time_hires_frequency());
	if (simulatedValue != expectedValue+1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_delta returned %llu expected %llu\n", simulatedValue, expectedValue);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library timer operations.
 * 
 * For various intervals verify the various timers for consistency.  There is no point in testing
 * against clock()
 * 
 * Functions verified by this test:
 * @arg @ref j9time.c::j9time_msec_clock "j9time_msec_clock()"
 * @arg @ref j9time.c::j9time_usec_clock "j9time_usec_clock()"
 * @arg @ref j9time.c::j9time_current_time_millis "j9time_current_time_millis()"
 * @arg @ref j9time.c::j9time_hires_clock "j9time_hires_clock()"
 * @arg @ref j9time.c::j9time_hires_frequency "j9time_hires_frequency()"
 * @arg @ref j9time.c::j9time_hires_delta "j9time_hires_delta()"
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9time_test3(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9time_test3";

	I_64 oldTime,time, newTime, timeDelta;
	UDATA mtimeStart, mtimeStop, mtimeDelta;
	UDATA utimeStart, utimeStop, utimeDelta;
	U_64 hiresTimeStart,hiresTimeStop;
	U_64 hiresDeltaAsMillis, hiresDeltaAsMicros;
	U_32 i,j;
	I_32 millires;

	const U_64 mDrift = 2; /* allowed tolerance when comparing two timers */
	const U_64 uDrift = 4; /* allowed tolerance when comparing two timers */

	reportTestEntry(portLibrary, testName);

	if (1 == j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE)) {
		/* one CPU means we have no chance of falling into the "difference CPUs have different times" trap.  Let the test begin */
		/*let's test the others vs current_time_millis*/
		outputComment(portLibrary, "%10s %10s %10s %10s %12s %12s\n","millires", "millis","msec","usec", "hires msec ", "hires usec");
		for(i=0;i<J9TIME_REPEAT_TEST;i++){
			/*change of millis*/
			time = j9time_current_time_millis();
			oldTime=time;
			for (j = 0; oldTime==time; j++) {
				oldTime = j9time_current_time_millis();
			}
			millires = (I_32)(oldTime - time);
	
			/*grab old times*/
			oldTime = j9time_current_time_millis();
			newTime = oldTime;
			mtimeStart = j9time_msec_clock();
			utimeStart = j9time_usec_clock();
			hiresTimeStart = j9time_hires_clock();
	
			/*(busy)wait some time*/
			time = newTime + TEST_DURATION * (i + 1);
			while (newTime < time) {
				newTime = j9time_current_time_millis();
			}
			/*grab new times*/
			hiresTimeStop = j9time_hires_clock();
			mtimeStop = j9time_msec_clock();
			utimeStop = j9time_usec_clock();	/*higher-precision CLK should get precedence NaH!*/
	
			hiresDeltaAsMillis = (U_32)j9time_hires_delta(hiresTimeStart, hiresTimeStop, J9PORT_TIME_DELTA_IN_MILLISECONDS);
			hiresDeltaAsMicros = (U_32)j9time_hires_delta(hiresTimeStart, hiresTimeStop, J9PORT_TIME_DELTA_IN_MICROSECONDS);
			mtimeDelta = mtimeStop - mtimeStart;
			utimeDelta = utimeStop - utimeStart;
			timeDelta = newTime - oldTime;
	
			outputComment(portLibrary, "%10d %10d %10d %10d %12d %12d\n",
				millires, (I_32)timeDelta, (I_32)mtimeDelta, (I_32)utimeDelta, (I_32)hiresDeltaAsMillis,(I_32) hiresDeltaAsMicros
			);
	
			hiresDeltaAsMillis = hiresDeltaAsMillis > mtimeDelta ? hiresDeltaAsMillis-mtimeDelta : mtimeDelta-hiresDeltaAsMillis;
			if (hiresDeltaAsMillis > (0.1 * mtimeDelta)) {
				outputComment(portLibrary,"\n");
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_clock() drift greater than 10%%\n");
				break;
			}
		}
	} else {
		/* test is invalid since this is a multi-way machine:  if we get the time on one CPU, then get rescheduled on a different one where we ask for the time,
		 * there is no reason why the time values need to be monotonically increasing */
		outputComment(portLibrary,"Test is invalid since the host machine reports more than one CPU (time may differ across CPUs - makes test results useless - re-enable if we develop thread affinity support)\n");
	}
	outputComment(portLibrary,"\n");

	return reportTestExit(portLibrary, testName);
}

/*
 * Check that value2 is greater or equal to value1 and that the
 * difference is less than epsilon.
 */
static BOOLEAN
compareMillis(U_64 value1, U_64 value2, U_64 epsilon)
{
	return (value2 >= value1) && (value2 - value1 < epsilon);
}

static U_64
portGetNanos(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const U_64 NANOS_PER_SECOND = 1000000000L;

	U_64 ticks = j9time_hires_clock();
	U_64 ticksPerSecond = j9time_hires_frequency();

	if (ticksPerSecond < NANOS_PER_SECOND ) {
		ticks *= (NANOS_PER_SECOND / ticksPerSecond);
	} else {
		ticks /= (ticksPerSecond / NANOS_PER_SECOND);
	}

	return ticks;
}

int
j9time_test4(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9time_test4";

	const UDATA SLEEP_TIME = 100; /* in millis */
	/* EPSILON is an arbitrarily-chosen duration used for the sanity check. */
	const UDATA EPSILON = 2; /* in millis */
	const UDATA NUMBER_OF_ITERATIONS = 100;

	UDATA i;
	U_64 milliStartOuter, milliEndOuter, milliDeltaOuter;
	U_64 nanoStart, nanoEnd, nanoDeltaInMillis;
	U_64 milliStartInner, milliEndInner, milliDeltaInner;
	U_64 delta;

	outputComment(portLibrary, "\nRunning time test with:\n");
	outputComment(portLibrary, "  SLEEP_TIME = %u\n", SLEEP_TIME);
	outputComment(portLibrary, "  EPSILON = %u\n", EPSILON);
	outputComment(portLibrary, "  NUMBER_OF_ITERATIONS = %u\n", NUMBER_OF_ITERATIONS);

	for (i = 0; i < NUMBER_OF_ITERATIONS; i++) {

		outputComment(portLibrary, "\nIteration %u:\n", i);

		milliStartOuter = j9time_current_time_millis();
		nanoStart = portGetNanos(portLibrary);
		milliStartInner = j9time_current_time_millis();

		if (0 != omrthread_sleep(SLEEP_TIME)) {
			outputComment(portLibrary, "WARNING: Skipping check due to omrthread_sleep() returning non-zero.\n");
			continue;
		}

		milliEndInner = j9time_current_time_millis();
		nanoEnd = portGetNanos(portLibrary);
		milliEndOuter = j9time_current_time_millis();

		milliDeltaInner = milliEndInner - milliStartInner;
		nanoDeltaInMillis = (nanoEnd - nanoStart)/1000000;
		milliDeltaOuter = milliEndOuter - milliStartOuter;

		outputComment(portLibrary, "milliStartOuter = %llu\n", milliStartOuter);
		outputComment(portLibrary, "nanoStart = %llu\n", nanoStart);
		outputComment(portLibrary, "milliStartInner = %llu\n", milliStartInner);

		outputComment(portLibrary, "milliEndInner = %llu\n", milliEndInner);
		outputComment(portLibrary, "nanoEnd = %llu\n", nanoEnd);
		outputComment(portLibrary, "milliEndOuter = %llu\n", milliEndOuter);

		outputComment(portLibrary, "milliDeltaInner = %llu\n", milliDeltaInner);
		outputComment(portLibrary, "nanoDeltaInMillis = %llu\n", nanoDeltaInMillis);
		outputComment(portLibrary, "milliDeltaOuter = %llu\n", milliDeltaOuter);

		/* To avoid false positives, do a sanity check on all the millis values. */
		if (!compareMillis(milliStartOuter, milliStartInner, EPSILON) ||
			!compareMillis(SLEEP_TIME, milliDeltaInner, EPSILON) ||
			!compareMillis(milliEndInner, milliEndOuter, EPSILON) ||
			(milliDeltaOuter < milliDeltaInner))
		{
			outputComment(portLibrary, "WARNING: Skipping check due to possible NTP daemon interference.\n");
			continue;
		}

		delta = (milliDeltaOuter > nanoDeltaInMillis ? milliDeltaOuter - nanoDeltaInMillis : nanoDeltaInMillis - milliDeltaOuter);
		if (delta > EPSILON) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "abs(milliDeltaOuter - nanoDeltaInMillis) > EPSILON (%llu)", delta);
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_current_time_millis() is not consistent with j9time_hires_clock()!");
			break;
		}

		delta = (nanoDeltaInMillis > milliDeltaInner ? nanoDeltaInMillis - milliDeltaInner : milliDeltaInner - nanoDeltaInMillis);
		if (delta > EPSILON) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "abs(nanoDeltaInMillis - milliDeltaInner) > EPSILON (%llu)", delta);
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_current_time_millis() is not consistent with j9time_hires_clock()!");
			break;
		}
	}

	return reportTestExit(portLibrary, testName);
}


#define J9TIME_TEST_DIRECTION_TIMEOUT_MILLIS 300000 /* 5 minutes */
static UDATA j9timeTestDirectionNumThreads = 0;

typedef struct J9TimeTestDirectionStruct {
	struct J9PortLibrary *portLibrary;
	omrthread_monitor_t monitor;
	BOOLEAN failed;
	UDATA finishedCount;
}J9TimeTestDirectionStruct;

/**
 * Check that j9time_nano_time always moves forward
 */
int
j9time_nano_time_direction(struct J9PortLibrary *portLibrary)
{
	omrthread_t self;
	const char *testName = "j9time_nano_time_direction";
	PORT_ACCESS_FROM_PORT(portLibrary);

	reportTestEntry(portLibrary, testName);

#if defined(WIN32) || defined(WIN64)
	/**
	 * On Windows, if QueryPerformanceCounter is used on a multiprocessor computer,
	 * time might be different across CPUs. Therefore skip this test and only
	 * re-enable if we develop thread affinity support.
	 */
	{
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i)) {
			outputComment(portLibrary,"WARNING: Test is invalid since the host machine uses QueryPerformanceCounter() (time may differ across CPUs - makes test results useless - re-enable if we develop thread affinity support)\n");
			return reportTestExit(portLibrary, testName);
		}
	}
#endif /* defined(WIN32) || defined(WIN64) */

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		/* success in starting up thread library and attaching */
		J9TimeTestDirectionStruct tds;
		tds.failed = FALSE;
		tds.portLibrary = portLibrary;
		tds.finishedCount = 0;

		if (0 == omrthread_monitor_init(&tds.monitor,0)) {
			UDATA i;
			IDATA waitRetVal = 0;
			const UDATA threadToCPUFactor = 10;
			omrthread_t *threads = NULL;

			j9timeTestDirectionNumThreads = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE) * threadToCPUFactor;
			threads = j9mem_allocate_memory(j9timeTestDirectionNumThreads * sizeof(omrthread_t), OMRMEM_CATEGORY_PORT_LIBRARY);

			if (NULL != threads) {
				if (0 == omrthread_monitor_enter(tds.monitor)) {
					for (i = 0; i < j9timeTestDirectionNumThreads ; i ++) {
						IDATA rc = omrthread_create(&threads[i], 128 * 1024, J9THREAD_PRIORITY_MAX, 0, &nanoTimeDirectionTest, &tds);
						if (0 != rc) {
							outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create thread, rc=%zd, i=%zu", rc, i);
						}
					}

					outputComment(portLibrary, "Num threads created: %zu\n", j9timeTestDirectionNumThreads);
					outputComment(portLibrary, "Threads that have finished running: ");

					/* wait for all threads to finish */
					while ((0 == waitRetVal) && (tds.finishedCount < j9timeTestDirectionNumThreads)) {
						waitRetVal = omrthread_monitor_wait_timed(tds.monitor, J9TIME_TEST_DIRECTION_TIMEOUT_MILLIS, 0);
					}

					outputComment(portLibrary, "\n");

					if (0 != waitRetVal) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_monitor_wait_timed() failed, waitRetVal=%zd", waitRetVal);
					}

					if (tds.failed) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "j9time_hires_clock() did not go forward in at least one of the launched threads");
					}

					omrthread_monitor_exit(tds.monitor);
				}  else {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to enter tds.monitor");
				}

				j9mem_free_memory(threads);
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate memory for threads array");
			}
			omrthread_monitor_destroy(tds.monitor);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to initialize tds.monitor");
		}

		omrthread_detach(self);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to thread library");
	}

	return reportTestExit(portLibrary, testName);
}


static int
J9THREAD_PROC nanoTimeDirectionTest(void *arg)
{
	J9TimeTestDirectionStruct *tds = (J9TimeTestDirectionStruct*) arg;
	UDATA i = 0;
	const UDATA numLoops = 500;
	const I_64 sleepMillis = 20; /* 20*500 -> total thread execution time should be ~ 10 seconds */
	PORT_ACCESS_FROM_PORT(tds->portLibrary);

	for(i = 0 ; i < numLoops ; i++) {
		I_64 finish = 0;
		I_64 start = j9time_nano_time();

		if (0 != omrthread_sleep(sleepMillis)) {
			j9tty_printf(PORTLIB, "\tomrthread_sleep() did not return zero.\n");
			omrthread_monitor_enter(tds->monitor);
			tds->failed = TRUE;
			omrthread_monitor_exit(tds->monitor);
			break;
		}

		finish = j9time_nano_time();
		if (finish <= start) {
			j9tty_printf(PORTLIB, "\tTime did not go forward after omrthread_sleep, start=%llu, finish=%llu\n", start, finish);
			omrthread_monitor_enter(tds->monitor);
			tds->failed = TRUE;
			omrthread_monitor_exit(tds->monitor);
			break;
		}
	}

	omrthread_monitor_enter(tds->monitor);
	tds->finishedCount += 1;
	if (j9timeTestDirectionNumThreads == tds->finishedCount) {
		omrthread_monitor_notify(tds->monitor);
	}
	j9tty_printf(PORTLIB, "%zu ", tds->finishedCount);
	omrthread_monitor_exit(tds->monitor);

	return 0;
}

/**
 * Computes error as a fraction of expected result.
 *
 * @param[in] exp Expected result
 * @param[in] actual Actual result
 *
 * @return abs(actual - exp) / exp
 */
static double
j9time_test_compute_error_pct(double exp, double actual)
{
	double error = 0.0;

	if (exp > actual) {
		error = (exp - actual) / exp;
	} else {
		error = (actual - exp) / exp;
	}
	return error;
}

/**
 * Verify precision of j9time_hires_delta().
 * 
 * j9time_hires_delta() converts the time interval into the requested resolution by
 * effectively multiplying the interval by (requiredRes / j9time_hires_frequency()).
 *
 * We use a test interval equal to (ticksPerSec = j9time_hires_frequency()), so if
 * there was no loss of precision, we should have:
 *     delta = ticksPerSec * (requiredRes / ticksPerSec) = requiredRes
 *
 * This test fails if the difference between the computed and expected delta is > 1%.
 *
 * The worst roundoff error cases for j9time_hires_delta() are:
 * 1. (ticksPerSec > requiredRes) and (ticksPerSec / requiredRes) truncates down a lot.
 * 2. (ticksPerSec < requiredRes) and (ticks * requiredRes) overflows.
 *
 * @param[in] portLibrary The port library under test
 *
 * Functions verified by this test:
 * @arg @ref j9time.c::j9time_hires_delta "j9time_hires_delta()"
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
j9time_test_hires_delta_rounding(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9time_test_hires_delta_rounding";
	U_64 ticksPerSec = 0;
	U_64 requiredRes = 0;
	U_64 delta = 0;
	double error = 0.0;

	reportTestEntry(portLibrary, testName);

	ticksPerSec = j9time_hires_frequency();
	if (0 == ticksPerSec) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "invalid hires frequency\n");
		goto exit;
	}
	outputComment(portLibrary, "hires frequency: %llu\n", ticksPerSec);

	/* Case 1: ticksPerSec / requiredRes potentially rounds down */
	/*
	 * requiredRes is picked so that the ratio (ticksPerSec / requiredRes) is 1.9, which
	 * has a big fractional part that would be lost if naive integer math were used.
	 *
	 * If we did lose the fractional part, then we would get:
	 * delta = ticksPerSec / (ticksPerSec / requiredRes) = ticksPerSec / 1 = ticksPerSec
	 * error = abs(ticksPerSec - requiredRes) / requiredRes = 0.9 ... which fails the test.
	 */
	requiredRes = (U_64)(ticksPerSec / (double)1.9);

	delta = j9time_hires_delta(0, ticksPerSec, requiredRes);
	if (0 == delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 1. j9time_hires_delta returned 0");
	}

	error = j9time_test_compute_error_pct((double)requiredRes, (double)delta);
	if (error > 0.01) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 1. error is too high");
	}
 	outputComment(portLibrary, "Case 1. expected: %llu    actual: %llu    error: %lf\n", requiredRes, delta, error);

	/* Case 2: ticks * requiredRes overflows */
	requiredRes = U_64_MAX / (ticksPerSec - 1) + ticksPerSec;

	delta = j9time_hires_delta(0, ticksPerSec, requiredRes);
	if (0 == delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 2. j9time_hires_delta returned 0");
	}

	error = j9time_test_compute_error_pct((double)requiredRes, (double)delta);
	if (error > 0.01) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 2. error is too high");
	}
	outputComment(portLibrary, "Case 2. expected: %llu    actual: %llu    error: %lf\n", requiredRes, delta, error);

	/* Case 3: requiredRes = ticksPerSec */
	requiredRes = ticksPerSec;

	delta = j9time_hires_delta(0, ticksPerSec, requiredRes);
	if (0 == delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 3. j9time_hires_delta returned 0");
	}

	error = j9time_test_compute_error_pct((double)requiredRes, (double)delta);
	if (error > 0.01) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 3. error is too high");
	}
	outputComment(portLibrary, "Case 3. expected: %llu    actual: %llu    error: %lf\n", requiredRes, delta, error);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library timer operations.
 * 
 * Exercise all API related to port library timer operations found in
 * @ref j9time.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return0 on success, -1 on failure
 */
I_32 
j9time_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "Time test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9time_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9time_test1(portLibrary);
		rc |= j9time_test2(portLibrary);
		rc |= j9time_test3(portLibrary);
#if 0
		/* Disabled because
		 * - generates way too much output
		 * - the output is confusing and does not help to diagnose errors
		 * - the test mostly does not actually test anything, because most of the time it detects:
		 * 		"WARNING: Skipping check due to possible NTP daemon interference."
		 * 		- this only adds to the confusion
		 * - the test is known to not work on x86 because of the j9time_hires_clock implementation
		 */
		rc |= j9time_test4(portLibrary);
#endif
		rc |= j9time_nano_time_direction(portLibrary);
		rc |= j9time_test_hires_delta_rounding(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nTime test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;	
}

 

