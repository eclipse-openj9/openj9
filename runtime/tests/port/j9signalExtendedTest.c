/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library signal handling.
 * 
 * Exercise the API for port library signal handling.  These functions 
 * can be found in the file @ref j9signal.c  
 * 
 */
 
#if !defined(WIN32) 
#include <signal.h>
#include <pthread.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "j9cfg.h"
#include "j9port.h"
#include "omrthread.h"
#include "testHelpers.h"


static U_32 portTestOptionsGlobal;

#if defined(LINUX)
typedef enum SignalEvent {
	INVALID,
	READY,
	SIGMASK,
	PTHREADKILL,
	FINISHED,
	EXIT
} SignalEvent;

typedef enum SignalStatus {
	NOTSIGNALED = 1, /* no signal was received */
	SIGNALED,        /* the correct signal was received */
	INCORRECT        /* an incorrect signal was received */
} SignalStatus;

typedef struct SigMaskTestInfo {
	struct J9PortLibrary* portLibrary;
	const char* testName;
	omrthread_monitor_t monitor;
	j9sig_protected_fn fn;
	struct {
		volatile SignalEvent event;
		SignalStatus status;
		union {
			U_32 flags;
			sigset_t masks;
			pthread_t thread;
		} data;
	} bulletinBoard;
} SigMaskTestInfo;

static void setSigMaskTestInfo(SigMaskTestInfo* info, struct J9PortLibrary* portLibrary, const char* testName, omrthread_monitor_t monitor, j9sig_protected_fn fn, SignalEvent event);
static omrthread_t create_thread(struct J9PortLibrary* portLibrary, omrthread_entrypoint_t entpoint, void* entarg);
static BOOLEAN waitForEvent(const char* testName, SigMaskTestInfo* info, SignalEvent event, void* result, size_t size);
static void sendEvent(SigMaskTestInfo* info, SignalEvent event, void* value, size_t size);
static UDATA sigHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg);
static UDATA maskProtectedFunction(struct J9PortLibrary* portLibrary, void* arg);
static UDATA unmaskProtectedFunction(struct J9PortLibrary* portLibrary, void* arg);
static int sigMaskThread(void* arg);
static UDATA j9sig_ext_test1(struct J9PortLibrary *portLibrary);
#endif /* LINUX */

/**
 * Verify port library signal handling extended.
 *
 * @param[in] portLibrary 	the port library under test
 * @param[in] exeName 		the executable that invoked the test
 *
 * @return 0 on success, -1 on failure
 */
I_32
j9sig_ext_runTests(struct J9PortLibrary *portLibrary, const char* exeName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA rc = 0;

	portTestOptionsGlobal = j9sig_get_options();

	/* Display unit under test */
	HEADING(portLibrary, "Signal test extended");

#if defined(LINUX)
	rc |= j9sig_ext_test1(portLibrary);
#endif /* LINUX */

	/* Output results */
	j9tty_printf(PORTLIB, "\nSignal test extended done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return ((TEST_PASS == rc) ? 0 : -1);
}

#if defined(LINUX)

/**
 * Set SigMaskTestInfo for a child thread.
 *
 * @param info The SigMaskTestInfo to be set
 * @param testName The name of child thread
 * @param monitor The monitor object that is used to synchronize with main thread
 * @param event The initial event
 */
static void
setSigMaskTestInfo(SigMaskTestInfo* info, struct J9PortLibrary* portLibrary, const char* testName, omrthread_monitor_t monitor, j9sig_protected_fn fn, SignalEvent event)
{
	info->portLibrary = portLibrary;
	info->testName = testName;
	info->monitor = monitor;
	info->fn = fn;
	info->bulletinBoard.event = event;
	info->bulletinBoard.status = NOTSIGNALED;
}

/**
 * Create a J9Thread to carry on test operation
 *
 * @param entpoint The entry point of the newly created thread
 * @param entarg The argument for the entry point function
 * @return pointer to a J9Thread on success; otherwise, a NULL pointer and log error message
 */
static omrthread_t
create_thread(struct J9PortLibrary* portLibrary, omrthread_entrypoint_t entpoint, void* entarg)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SigMaskTestInfo* info = (SigMaskTestInfo*)entarg;
	const char* testName = info->testName;
	omrthread_t handle = NULL;
	IDATA ret = omrthread_create_ex(&handle, J9THREAD_ATTR_DEFAULT, 0, entpoint, entarg);
	if (J9THREAD_SUCCESS != ret) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to create omrthread, returned:%d\n", ret);
	}

	return handle;
}

/**
 * Wait for expected event and optionally bring event related data back.
 *
 * @param testName The name of the waiting thread
 * @param info The pointer to SigMaskTestInfo object
 * @param event The expected event
 * @param result The event related data
 * @param size The size of the event data
 * @return TRUE upon success; FALSE if expected event did not occur before timeout.
 */
static BOOLEAN
waitForEvent(const char* testName, SigMaskTestInfo* info, SignalEvent event, void* result, size_t size)
{
	struct J9PortLibrary* portLibrary = info->portLibrary;
	PORT_ACCESS_FROM_PORT(portLibrary);
	BOOLEAN ret = FALSE;
	IDATA waitRC = J9THREAD_WOULD_BLOCK;

	omrthread_monitor_enter(info->monitor);

	while (info->bulletinBoard.event != event) {
		waitRC = omrthread_monitor_wait_timed(info->monitor, 60000, 0);
	   	if (J9THREAD_TIMED_OUT == waitRC) {
			break;
		}
	}

	ret = (info->bulletinBoard.event == event);
	if (TRUE == ret) {
		if ((NULL != result) && (size > 0)) {
			memcpy(result, &info->bulletinBoard.data, size);
		}
	} else {
	   	if (J9THREAD_TIMED_OUT == waitRC) {
	   		outputErrorMessage(PORTTEST_ERROR_ARGS, "timed out without being notified. expected (%d), received (%d)\n", event, info->bulletinBoard.event);
	   	} else {
	   		outputErrorMessage(PORTTEST_ERROR_ARGS, "expected event(%d) was not received. bulletinBoard(%d)\n", event, info->bulletinBoard.event);
	   	}
	}

	info->bulletinBoard.event = INVALID;

	omrthread_monitor_exit(info->monitor);

	return ret;
}

/**
 * Send a event and optionally event data to the waiting thread.
 * 
 * @param info The pointer to SigMaskTestInfo object
 * @param event The event
 * @param value The pointer to event data
 * @param size The size of the event data 
 */
static void
sendEvent(SigMaskTestInfo* info, SignalEvent event, void* value, size_t size)
{
	omrthread_monitor_enter(info->monitor);
	info->bulletinBoard.event = event;
	if ((NULL != value) && (size > 0)) {
		memcpy(&info->bulletinBoard.data, value, size);
	}
	omrthread_monitor_notify(info->monitor);
	omrthread_monitor_exit(info->monitor);
}

/**
 * Signal handling function. 
 */
static UDATA 
sigHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	SigMaskTestInfo* info = (SigMaskTestInfo*)handler_arg;
	const char* testName = info->testName;
	
	outputComment(portLibrary, "\t%s\t:sigHandlerFunction invoked (type = 0x%x)\n", info->testName, gpType);

	validateGPInfo(portLibrary, gpType, sigHandlerFunction, gpInfo, testName);
	if (info->bulletinBoard.data.flags & gpType) {
		info->bulletinBoard.status = SIGNALED;
	} else {
		info->bulletinBoard.status = INCORRECT;
	}
	return J9PORT_SIG_EXCEPTION_RETURN;
}

/**
 * The function runs under j9sig_protect()'s protection
 * It tells main thread that it is fully started and waits for main thread telling it to finish
 * the execution
 */
static UDATA
maskProtectedFunction(struct J9PortLibrary* portLibrary, void* arg)
{
	SigMaskTestInfo* info = (SigMaskTestInfo*)arg;

	outputComment(portLibrary, "\t%s\t:maskProtectedFunction is ready for test.\n", info->testName);
	sendEvent(info, READY, NULL, 0);

	/* 
	 * masked thread will be blocked here waiting for EXIT event.
	 */
	waitForEvent(info->testName, info, EXIT, NULL, 0);
	outputComment(portLibrary, "\t%s\t:maskProtectedFunction exit normally.\n", info->testName);

	return 8096;
}

/**
 * The function runs under j9sig_protect()'s protection
 * It shall be terminated by self-generated SIGSEGV.
 */
static UDATA
unmaskProtectedFunction(struct J9PortLibrary* portLibrary, void* arg)
{
	SigMaskTestInfo* info = (SigMaskTestInfo*)arg;
	char* p = NULL;

	*p = 'a';  /* SIGSEGV here */

	outputComment(portLibrary, "\t%s\t:unmaskProtectedFunction exit normally.\n", info->testName);
	return 8096;
}

/**
 * This is child thread's entry point. It uses waitForEvent() and sendEvent() to synchronize
 * its operations with the main thread.
 * @param arg The pointer to passed in parameter which is a type of SigMaskTestInfo.
 * @return 0 thread return normally. -1 thread return with error.
 */
static int
sigMaskThread(void* arg)
{
	SigMaskTestInfo* info = (SigMaskTestInfo*)arg;
	struct J9PortLibrary* portLibrary = info->portLibrary;
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = info->testName;
	U_32 protectResult = 0;
	UDATA result = 1;
	U_32 flags = 0;
	pthread_t thread = pthread_self();
	sigset_t mask;

	/* initialize local variables */
	sigemptyset(&mask);

	outputComment(portLibrary, "\t%s\t:thread is ready for testing.\n", testName);
	sendEvent(info, READY, &thread, sizeof(thread));

	/* pthread_sigmask */
	if (!waitForEvent(testName, info, SIGMASK, &flags, sizeof(flags))) {
		return -1;
	}
	outputComment(portLibrary, "\t%s\t:starting pthread_sigmask test...\n", testName);

	if (0 != pthread_sigmask(SIG_SETMASK, NULL, &mask)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "pthread_sigmask failed: %s(%d).\n", strerror(errno), errno);
		return -1;
	}

	if (0 != flags) {
		sigaddset(&mask, flags);
		if (0 != pthread_sigmask(SIG_SETMASK, &mask, NULL)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "pthread_sigmask failed: %s(%d).\n", strerror(errno), errno);
			return -1;
		}
		if (0 != pthread_sigmask(SIG_SETMASK, NULL, &mask)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "pthread_sigmask failed: %s(%d).\n", strerror(errno), errno);
			return -1;
		}
	}

	outputComment(portLibrary, "\t%s\t:pthread_sigmask test has been done. send result back to main thread.\n", testName);
	sendEvent(info, READY, &mask, sizeof(mask));

	/* pthread_kill */
	if (!waitForEvent(testName, info, PTHREADKILL, &flags, sizeof(flags))) {
		return -1;
	}
	outputComment(portLibrary, "\t%s\t:starting signal handling test in protected function...\n", testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
		return -1;
	} else {
		protectResult = j9sig_protect(
				info->fn, (void*)info,
				sigHandlerFunction, (void*)info,
				flags,
				&result);
				
		if ((J9PORT_SIG_EXCEPTION_OCCURRED == protectResult) && (0 != result)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}

		outputComment(portLibrary, "\t%s\t:protected function finished.\n", testName);
		sendEvent(info, FINISHED, NULL, 0);
	}

	return 0;
}

/* 
 * Test pthread_sigmask() with cross fire signaling. 
 *  - create 2 threads
 *  - one thread calls pthread_sigmask() and verifies that the specified mask is set
 *  - other thread verifies that the specified mask is not set
 *  - send some signals to each thread, e.g. by pthread_kill and verify that it is blocked or not.
 *
 *  Note: under heavy load test environment, the signal may not be delivered in timely fashion
 *
 * mask   thread:----s(READY)-w(SIGMASK)-----------m---s(READY)-w(PTHREADKILL)----------j----s(READY)------w(EXIT)-------s(FINISHED)---------->>
 * main   thread:-w(READY)---------s(SIGMASK)-w(READY)------------c----s(PTHREADKILL)-w(READY)----------k----s(EXIT)--w(FINISHED)--------->>
 * 
 * main   thread:-w(READY)---------s(SIGMASK)-w(READY)------------c----s(PTHREADKILL)----w(FINISHED)---------->>
 * unmask thread:---s(READY)-w(SIGMASK)-------m--s(READY)-w(PTHREADKILL)----------j----i----s(FINISHED)-------------->>
 *
 * where:
 * 	m: run pthread_sigmask
 * 	c: check result of pthread_sigmask
 * 	j: run sigprotected function
 * 	k: run pthread_kill
 * 	i: signal handler (J9PORT_SIG_EXCEPTION_RETURN)
 *
 */
static UDATA
j9sig_ext_test1(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sig_ext_test1";
	omrthread_monitor_t maskMonitor = NULL;
	omrthread_monitor_t unmaskMonitor = NULL;
	omrthread_t mainThread = NULL;
    omrthread_t maskThread = NULL;
	omrthread_t unmaskThread = NULL;
	pthread_t osMaskThread = 0;
	pthread_t osUnmaskThread = 0;
	U_32 flags = 0;
	SigMaskTestInfo maskThreadInfo;
	SigMaskTestInfo unmaskThreadInfo;
	IDATA monitorRC = 0;
	int memcmp_ret = 0;
	sigset_t mask;
	sigset_t oldMask;
	sigset_t currentMask;
	sigset_t maskThread_mask;
	sigset_t unmaskThread_mask;

	reportTestEntry(portLibrary, testName);

	/* initialize local variables */
	memset(&maskThreadInfo, 0, sizeof(maskThreadInfo));
	memset(&unmaskThreadInfo, 0, sizeof(unmaskThreadInfo));
	sigemptyset(&mask);
	sigemptyset(&oldMask);
	sigemptyset(&currentMask);
	sigemptyset(&maskThread_mask);
	sigemptyset(&unmaskThread_mask);
#ifndef J9ZTPF
	sigaddset(&mask, SIGBUS);
#endif /* !defined(J9ZTPF) */
	if (0 != pthread_sigmask(SIG_SETMASK, &mask, &oldMask)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "pthread_sigmask failed: %s(%d).\n", strerror(errno), errno);
		return TEST_FAIL;
	}

	monitorRC = omrthread_monitor_init_with_name(&maskMonitor, 0, "j9sig_ext_sigmask_monitor");
	if (0 != monitorRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_monitor_init_with_name failed with %i\n", monitorRC);
		return TEST_FAIL;
	}
	setSigMaskTestInfo(&maskThreadInfo, portLibrary, "Masked Thread", maskMonitor, maskProtectedFunction, INVALID);
	
	monitorRC = omrthread_monitor_init_with_name(&unmaskMonitor, 0, "j9sig_ext_sigunmask_monitor");
	if (0 != monitorRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_monitor_init_with_name failed with %i\n", monitorRC);
		return TEST_FAIL;
	}
	setSigMaskTestInfo(&unmaskThreadInfo, portLibrary, "Unmasked Thread", unmaskMonitor, unmaskProtectedFunction, INVALID);

	mainThread = omrthread_self();
    maskThread = create_thread(portLibrary, (omrthread_entrypoint_t)sigMaskThread, &maskThreadInfo);
	unmaskThread = create_thread(portLibrary, (omrthread_entrypoint_t)sigMaskThread, &unmaskThreadInfo); 

	if ((NULL != mainThread) && (NULL != maskThread) && (NULL != unmaskThread)) {
		outputComment(portLibrary, "\t%s\t:created test threads.\n", testName);
		/* wait for maskThread and unmaskThread ready */
		if (!waitForEvent(testName, &maskThreadInfo, READY, &osMaskThread, sizeof(pthread_t))) {
			goto exit;
		}
		if (!waitForEvent(testName, &unmaskThreadInfo, READY, &osUnmaskThread, sizeof(pthread_t))) {
			goto exit;
		}
		outputComment(portLibrary, "\t%s\t:test threads are READY.\n", testName);

		/* test pthread_sigmask */
		/* ask maskThread to mask SIGILL signal */
		flags = SIGILL;
		outputComment(portLibrary, "\t%s\t:configure mask thread to mask SIGILL signal.\n", testName);
		sendEvent(&maskThreadInfo, SIGMASK, &flags, sizeof(flags));
		/* ask unMaskThread to not mask any signal */
		flags = 0;
		outputComment(portLibrary, "\t%s\t:configure unmask thread to not mask any signal.\n", testName);
		sendEvent(&unmaskThreadInfo, SIGMASK, &flags, sizeof(flags));

		outputComment(portLibrary, "\t%s\t:testing pthread_sigmask...\n", testName);

		/* check pthread_sigmask result */
		if (!waitForEvent(testName, &maskThreadInfo, READY, &maskThread_mask, sizeof(maskThread_mask))) {
			goto exit;
		}
		if (!waitForEvent(testName, &unmaskThreadInfo, READY, &unmaskThread_mask, sizeof(unmaskThread_mask))) {
			goto exit;
		}
		outputComment(portLibrary, "\t%s\t:operation pthread_sigmask has been done. checking pthread_sigmask result...\n", testName);

		/*
		 * Expected behavior:
		 * 1. main thread's signal mask shall be untouched
		 * 2. child threads shall inherit main thread's signal mask
		 * 3. child threads' pthread_sigmask operation shall not interfere each other
		 */
		if (0 != pthread_sigmask(SIG_SETMASK, NULL, &currentMask)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "pthread_sigmask failed: %s(%d).\n", strerror(errno), errno);
			goto exit;
		}
		/* check whether main thread signal mask was affected by child thread pthread_sigmask operation */
		if (0 != (memcmp_ret = memcmp(&currentMask, &mask, sizeof(currentMask)))) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "main thread mask was modified (old=0x%X, new=0x%X), %X\n", *((U_32*)&mask), *((U_32*)&currentMask), memcmp_ret);
			goto exit;
		} else {
			outputComment(portLibrary, "\t%s\t:main thread signal mask was not affected.\n", testName);
		}

#ifndef J9ZTPF
		/* UNIX opengroup example says that newly created thread shall inherit the mask */
		if (!sigismember(&maskThread_mask, SIGBUS) || !sigismember(&unmaskThread_mask, SIGBUS)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "created threads did not inherit mask.(maskThreadInfo=0x%X, unmaskThreadInfo=0x%X)\n", *((U_32*)&maskThread_mask), *((U_32*)&unmaskThread_mask));
			goto exit;
		} else {
			outputComment(portLibrary, "\t%s\t:child thread inherited main thread's signal mask.\n", testName);
		}
#endif /* !defined(J9ZTPF) */

		/* check whether two child threads' pthread_sigmask operation can interfere each other */
		if (!sigismember(&maskThread_mask, SIGILL) || sigismember(&unmaskThread_mask, SIGILL)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "pthread_sigmask did not work.(maskThreadInfo=0x%X, unmaskThreadInfo=0x%X)\n", *((U_32*)&maskThread_mask), *((U_32*)&unmaskThread_mask));
			goto exit;
		} else {
			outputComment(portLibrary, "\t%s\t:pthread_sigmask operation in each child thread did not interfere each other.\n", testName);
		}

		/* 
		 * test unmask thread signal handling behavior
		 * unmask thread will install SIGSEGV handler and launch a protected function unmaskProtectedFunction().
		 * unmaskProtectedFunction() shall be terminated by SIGSEGV generated in its body, and the unmask thread
		 * will send report to this main thread.
		 */ 
		flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV; /* SIGSEGV shall be received */
		outputComment(portLibrary, "\t%s\t:configure unmask thread to prepare for unmasked signal SIGSEGV test.\n", testName);
		sendEvent(&unmaskThreadInfo, PTHREADKILL, &flags, sizeof(flags));

		if (!waitForEvent(testName, &unmaskThreadInfo, FINISHED, NULL, 0)) {
			goto exit;
		} else {
			outputComment(portLibrary, "\t%s\t:unmasked thread finished. checking unmask thread's signal status...\n", testName);

			if (unmaskThreadInfo.bulletinBoard.status ==  SIGNALED) {
				outputComment(portLibrary, "\t%s\t:unmasked thread received signal as expected.\n", testName);
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "unmasked thread did not received signal as expected.(SignalStatus=%d)\n", unmaskThreadInfo.bulletinBoard.status);
				goto exit;
			}
		}


		/* 
		 * test mask thread signal handling behavior
		 * mask thread will install SIGILL handler and launch a protected function maskProtectedFunction().
		 */ 
		outputComment(portLibrary, "\t%s\t:testing pthread_kill...\n", testName);
		sendEvent(&maskThreadInfo, PTHREADKILL, &flags, sizeof(flags));
		flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGILL; /* SIGILL shall not be received */
		outputComment(portLibrary, "\t%s\t:configure mask thread to prepare for pthread_kill test.\n", testName);

		if (!waitForEvent(testName, &maskThreadInfo, READY, NULL, 0)) {
			goto exit;
		}
		outputComment(portLibrary, "\t%s\t:mask thread is ready to receive signal. sending pthread_kill...\n", testName);
		/* send SIGILL to maskThread which will never receive this signal */
		pthread_kill(osMaskThread, SIGILL);

		/* check pthread_kill result */
		/* 
		 * Expected behavior:
		 * 1. child thread with signal mask on SIGILL shall not receive this signal and therefore NOTSIGNALED.
		 */

		outputComment(portLibrary, "\t%s\t:notify mask thread to exit.\n", testName);
		sendEvent(&maskThreadInfo, EXIT, NULL, 0);
		if (!waitForEvent(testName, &maskThreadInfo, FINISHED, NULL, 0)) {
			goto exit;
		} else {
			outputComment(portLibrary, "\t%s\t:masked thread finished. checking mask thread's signal status...\n", testName);
			if (maskThreadInfo.bulletinBoard.status ==  NOTSIGNALED) {
				outputComment(portLibrary, "\t%s\t:masked thread did not receive signal as expected.\n", testName);
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "masked thread received signal as not expected.(SignalStatus=%d)\n", maskThreadInfo.bulletinBoard.status);
				goto exit;
			}
		}

		outputComment(portLibrary, "\t%s\t:pthread_sigmask works on each individual thread.\n", testName);

	} 
	
	outputComment(portLibrary, "\t%s\t:destroying unmaskMonitor...\n", testName);
	omrthread_monitor_destroy(unmaskMonitor);

	outputComment(portLibrary, "\t%s\t:destroying maskMonitor...\n", testName);
	omrthread_monitor_destroy(maskMonitor);

	goto cleanup;

exit:
	outputComment(portLibrary, "\t%s\t:test stopped with errors\n", testName);

cleanup:
	/* restore signal mask */
	pthread_sigmask(SIG_SETMASK, &oldMask, NULL);

	return reportTestExit(portLibrary, testName);
}
#endif /* LINUX */

