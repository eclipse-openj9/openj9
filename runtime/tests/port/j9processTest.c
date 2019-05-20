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

/**
 * @process
 * @ingroup PortTest
 * @brief Verify port library process management.
 * 
 * Exercise the API for port library process management operations.  These functions 
 * can be found in the file @ref j9process.c  
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9cfg.h"

#if defined(WIN32)
#include "Windows.h"
#endif

#if defined(LINUX)
#include <unistd.h>
#endif


#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9port.h"

#define HELLO_STANDARD_OUT "Hello Standard Out"
#define HELLO_STANDARD_ERR "Hello Standard Err"
#define HELLO_STANDARD_OUTNERR "Hello Standard OutHello Standard Err"
#define HANDLER_INVOKED "Handler_Invoked"

#define PROCESS_SAVE_ENV_FILE "processSaveEnv.test"

static void j9process_fireWriteAndReadImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength);
static void j9process_fireAndCheckIfCompleteImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength);
static void j9process_fireAndForgetImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength);
static void j9process_fireAndTerminateImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength);
static void j9process_fireAndWaitForImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength);
static void testNonBlockingIOAndIgnoreOutputImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength, U_32 options);
static void j9process_fireWriteAndReadWithRedirectImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength, U_32 options, IDATA fdInput, IDATA fdOutput, IDATA fdError);
static BOOLEAN verifyFileContent(struct J9PortLibrary *portLibrary, const char* testName, char *file1name, char* expectedResult);
static UDATA processGroupSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* handlerInfo, void* userData);
static void waitForChar(J9PortLibrary *portLibrary, J9ProcessHandle processHandle, I_64 timeoutMillis, UDATA stream);
/**
 * Crash process
 * 
 * Function that when executed stand-alone always causes the process to crash
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return non-zero error code
 */
static int 
processCrash(struct J9PortLibrary *portLibrary)
{
/* TODO Implement crashing process */
	return 1;
}

/**
 * Successful process
 * 
 * Function that when executed stand-alone always causes the process to complete successfully
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0
 */
static int 
processSuccess(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	return 0;
}

/**
 * Save environment variables to file in without translating them to native encoding.
 * @param[in] portLibrary The port library under test
 * @return 0
 */
static int
processSaveEnv(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int rc = -1;
	J9SysinfoEnvIteratorState state;
	U_32 envSize = 0;
	void *sysinfo_env_iterator_buffer = NULL;


	memset(&state, 0, sizeof(state));

	/* get the size of the buffer required to hold the iterator */
	rc = j9sysinfo_env_iterator_init(&state, NULL, 0);

	if (0 < rc) {
		envSize = rc;
		sysinfo_env_iterator_buffer = j9mem_allocate_memory(envSize, OMRMEM_CATEGORY_PORT_LIBRARY);

		/* This memory is a placeholder for the environment, for the purposes of passing it into j9process_create. The child process will replicate it,
		 * so once j9process_create returns, we can free it */
		sysinfo_env_iterator_buffer = j9mem_allocate_memory(envSize, OMRMEM_CATEGORY_PORT_LIBRARY);

		if ((NULL != sysinfo_env_iterator_buffer)) {
			IDATA fd1 = j9file_open(PROCESS_SAVE_ENV_FILE, EsOpenCreate | EsOpenWrite, 0666);
			if (-1 != fd1) {
				rc = j9sysinfo_env_iterator_init(&state, sysinfo_env_iterator_buffer, envSize);
				if (0 == rc) {
					while (j9sysinfo_env_iterator_hasNext(&state)) {
						J9SysinfoEnvElement element;
						rc = j9sysinfo_env_iterator_next(&state, &element);
						if (0 == rc) {
							j9file_write(fd1, element.nameAndValue, strlen(element.nameAndValue));
							j9file_write(fd1, "\n", strlen("\n"));
						} else {
							break;
						}
					}
				}
				rc = j9file_close(fd1);
			}
			j9mem_free_memory(sysinfo_env_iterator_buffer);
		}
	}
	return rc;
}


/**
 * Sleeping process
 * 
 * Function that when executed stand-alone always causes the process to complete successfully, after a timeout
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] seconds The seconds to sleep
 * 
 * @return 0
 */
static int 
processSleep(struct J9PortLibrary *portLibrary, unsigned int seconds)
{
	omrthread_sleep((I_64)seconds * 1000);
	return 0;
}

/**
 * Writes to stdout and stderr then reads from stdin until
 * the "quit" command is received
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0
 */
static int 
processWrite(struct J9PortLibrary *portLibrary)
{
	char buf[1024];
	int i=0;

	buf[0] = '\0';
	fprintf(stdout, HELLO_STANDARD_OUT);
	fflush(stdout);
	OMRPORT_FROM_J9PORT(portLibrary)->tty_printf(OMRPORT_FROM_J9PORT(portLibrary), HELLO_STANDARD_ERR);
	fflush(stderr);

	/* read standard in until the "quit" command is received */
	do {
		char c = getchar();
		if ( c == ' ' || c == '\n' ){
			/* null terminate the string and start over */
			buf[i] = '\0';
			i = 0;
		} else {
			buf[i] = c;
			i++;
		}
	}while(strcmp(buf, "quit") != 0);

	return 0;
}

#if defined(WIN32)

/*
 * Signal handler for unix platforms for process group tests
 * 
 * Simply writes #define'd HANDLER_INVOKED message to stdout
 *
 * @param dwCtrlType	the Windows event
 *
 * @return TRUE to get the process to continue,
 * 		otherwise FALSE to pass along to next
 * 		handler (in this case the default Windows handler)
 *
 */
static BOOL WINAPI
consoleCtrlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
		break;
	default:
		fprintf(stdout, "Unexpected Windows event: %u", dwCtrlType);
		fflush(stdout);
		return FALSE;
	}

	fprintf(stdout, HANDLER_INVOKED);
	fflush(stdout);
	return TRUE;
}

#else /* defined(WIN32) */

/*
 * Signal handler for unix platforms for process group tests
 * 
 * Simply writes #define'd HANDLER_INVOKED message to stdout
 * 
 * @param portLibrary 	the port library 
 * @param gpType 		the port library signal type
 * @param handlerInfo 	unused
 * @param userData		unused
 *
 */
static UDATA
processGroupSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* handlerInfo, void* userData)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	if (J9PORT_SIG_FLAG_SIGQUIT != gpType) {
		fprintf(stdout, "Unexpected port library signal: %u", gpType);
		fflush(stdout);
	} else {
		fprintf(stdout, HANDLER_INVOKED);
		fflush(stdout);
	}

	return 0;
}


#endif /* defined(WIN32) */

/**
 * Registers the signal handler used for the process group tests
 *
 * @param portLibrary the portLibrary
 *
 * @return 0 on success, non-zero on failure
 */
static int
setTestProcessGroupSignalHandler(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = -1;

#if defined(WIN32)
 	BOOL setConsoleCtrlHandlerRC = FALSE;

 	setConsoleCtrlHandlerRC = SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
 	if (FALSE == setConsoleCtrlHandlerRC) {
 		j9error_set_last_error(GetLastError(), 1);	/* this is a hack to get outputErrorMessage to report the error that occurred in SetConsoleCtrlHandler*/
 		rc = -1;
 	} else {
 		rc = 0;
 	}
#else /* defined(WIN32) */
 	I_32 setAsyncRC = J9PORT_SIG_ERROR;
	setAsyncRC = j9sig_set_async_signal_handler(processGroupSignalHandler, NULL, J9PORT_SIG_FLAG_SIGQUIT);
	if (J9PORT_SIG_ERROR == setAsyncRC) {
		rc = -1;
	} else {
		rc = 0;
	}
#endif /* defined(WIN32) */

	return rc;
}

/**
 *  Registers the signal handler used for the process group tests then spins until it receives "quit" on stdin.
 *
 *  @param[in] portLibrary the portLibrary
 *
 *  @return 0
 */
static int
processRegisterHandlerAndSpin(struct J9PortLibrary *portLibrary)
{

	PORT_ACCESS_FROM_PORT(portLibrary);
	U_32 setHandlerRC = 1;
	char buf[1024];
	int i=0;
	int setSignalHandlerRC = -1;

	setSignalHandlerRC = setTestProcessGroupSignalHandler(portLibrary);

 	if (0 != setSignalHandlerRC) {
 		/* the parent reads stdout and writes it to the console, so this is the best way to advertise failure */
 		fprintf(stdout, "Failed to register the signal handler\n");
 	}

 	/* Tell the parent we're up and running and have registered the signal */
 	fprintf(stdout, "t");
 	fflush(stdout);

	/* read standard in until the "quit" command is received */
	do {
		char c;

		c = getchar();
		if ( c == ' ' || c == '\n' ){
			/* null terminate the string and start over */
			buf[i] = '\0';
			i = 0;
		} else {
			buf[i] = c;
			i++;
		}
	} while(strcmp(buf, "quit") != 0);

	return 0;
}

/**
 * Waits for the specified timeout for a character to be received on the specified stream
 *
 * @param portLibrary	the port library
 * @param processHandle	the process we're listening to
 * @param timeoutMillis	the timeout
 * @param stream		one of J9PORT_PROCESS_STDOUT or J9PORT_PROCESS_STDERR
 * 
 */
static void
waitForChar(J9PortLibrary *portLibrary, J9ProcessHandle processHandle, I_64 timeoutMillis, UDATA stream)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_64 waitTime = 0;
	IDATA numBytesAvailable = -1;

	while (waitTime < timeoutMillis) {
		I_64 sleepMillis = 100;
		numBytesAvailable = j9process_get_available (processHandle, stream);
		if (0 < numBytesAvailable) {
			break;
		} else {
			omrthread_sleep(sleepMillis);
			waitTime += sleepMillis;
		}
	}
}

/**
 *
 * Tests correct operation of flag J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP using signals
 *
 * The two tests that are run are:
 * 	Test1 - verify the correct behaviour without J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 * 	Test2 - verify the correct behaviour with J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 *
 * The mechanism in both is:
 *
 * 1. Register a handler.
 * 2. Create the child which also registers a handler.
 * 3. The parent then triggers a signal that will only be picked up by the child handler if it is 
 * 		in the same process group.
 * 4. Parent listens to stdout for acknowledgement that child handler was invoked
 *
 * See platform specific considerations below.
 *
 * Test 1
 * 		- Create the child process _without_ J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 * 		- Trigger the signal that will only be received by processes in the same group
 * 		- Verify that the child's handler was invoked
 * 
 * Test 2
 * 		- Create the child process _with_ J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 * 		- Trigger the signal that will only be received by processes in the same group
 * 		- Verify that the child's handler was not invoked
 * 			- unfortunately, the only way to do this is to wait for a "reasonable" amount time
 * 				such that the child's handler would have had time to be invoked and write the ack to stdout.
 * *
 *	Platform specific considerations:
 *		Windows:
 *			- Uses CTRL_C_EVENT (CTRL + C) as this is disabled in the child if it is created with
 *				J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP (Windows flag CREATE_NEW_PROCESS_GROUP)
 *			- Can't use the port library API, as the port library handler will cause the process
 *				to exit if it receives the CTRL_C_EVENT
 *				
 *		Unix:
 *			Uses SIGQUIT as returning from a SIGINT handler causes the process to exit
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static IDATA
j9process_testNewProcessGroupByTriggeringSignal(struct J9PortLibrary *portLibrary)
{
#define MAX_BUF_SIZE 	256
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9ProcessHandle processHandle;
	IDATA retVal;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;
 	I_64 timeoutMillis = 60000;
 	U_32 setHandlerRC = 1;
 	I_32 numBytesAvailable = 0;
 	char* testName = "j9process_testNewProcessGroupByTriggeringSignal";
 	char *command[2];
 	UDATA commandLength = 2;
 	char buffer[MAX_BUF_SIZE];
 	int setSignalHandlerRC = -1;

 	reportTestEntry(portLibrary, testName);

	setSignalHandlerRC = setTestProcessGroupSignalHandler(portLibrary);
  	if (0 != setSignalHandlerRC) {
 		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to register the signal handler\n");
 		return reportTestExit(portLibrary, testName);
 	}

 	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
 		command[1] = "-child_j9process_processRegisterHandlerAndSpin";
  	} else {
 		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
 		return reportTestExit(portLibrary, testName);
 	}

 	/********************************/
 	/********************************/
 	/* Test 1: When J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP is not used, the child's CTRL_C_EVENT handler should be invoked */
 	/********************************/
 	/********************************/
 	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

 	memset(buffer, 0, sizeof(buffer));
 	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);

	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {

		/* wait until the child has registered its handler, which it will indicate by writing "t" to stdout */
		j9tty_printf(portLibrary, " Waiting for child to register handler\n");
		waitForChar(portLibrary, processHandle, timeoutMillis, J9PORT_PROCESS_STDOUT);

		retVal = readFromProcess(portLibrary, testName, processHandle, J9PORT_PROCESS_STDOUT, buffer, MAX_BUF_SIZE);

		if (retVal != -1) {
			if (strcmp(buffer, "t") != 0) {
				/* unexpected output */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected output received from child: %s\n", buffer);
			} else {
				j9tty_printf(portLibrary, " Received confirmation that child has registered handler\n");
			}
		}

		/* trigger the CTRL_C_EVENT, then sleep for 3 seconds, which should leave enough time for the child handler to be invoked and write to stdout
		 * Note that the parent (this) process' handler will also be invoked */
#if defined(WIN32)
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0 /* send to all processes that share this console */);
#else /* defined(WIN32) */
		killpg(getpgrp(), SIGQUIT);
#endif /* defined(WIN32) */

		omrthread_sleep(3000);

		if (retVal != -1) {
			retVal = readFromProcess(portLibrary, testName, processHandle, J9PORT_PROCESS_STDOUT, buffer, MAX_BUF_SIZE);
		}

		if (retVal != -1) {
			if (strcmp(buffer, HANDLER_INVOKED) != 0) {
				/* unexpected output */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected: unexpected output received from child: \"%s\", expected: \"%s\"\n", buffer, HANDLER_INVOKED);
			} else {
				/* success, the child handler was invoked as expected */
			}
		}

		/* We're done with this test, tell the child to shut down */
		retVal = writeToProcess(portLibrary, testName, processHandle, "hi quit\n", 9);

		if (retVal != -1) {
			retVal = pollProcessForComplete(portLibrary, testName, processHandle, timeoutMillis);
		}
	}

	if (retVal != -1) {
		retVal = j9process_close(&processHandle, 0);
	}

	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

 	/********************************/
 	/********************************/
 	/* Test 2: When J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP is used, the child's handler should not be invoked */
 	/********************************/
 	/********************************/
 	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

 	memset(buffer, 0, sizeof(buffer));
 	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);

	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {

		/* same as before, wait until the child has registered its handler, which it will indicate by writing "t" to stdout */
		j9tty_printf(portLibrary, " Waiting for child to register handler\n");
		waitForChar(portLibrary, processHandle, timeoutMillis, J9PORT_PROCESS_STDOUT);

		retVal = readFromProcess(portLibrary, testName, processHandle, J9PORT_PROCESS_STDOUT, buffer, MAX_BUF_SIZE);

		if (retVal != -1) {
			if (strcmp(buffer, "t") != 0) {
				/* unexpected output */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpected output received from child: %s\n", buffer);
			} else {
				j9tty_printf(portLibrary, " Received confirmation that child has registered handler\n");
			}
		}

		/* trigger the signal/event, then sleep for 3 seconds, which should leave enough time for the event to propagate to the child
		 * Note that the parent (this) process' handler will also be invoked, and write the #defined HANDLER_INVOKED message to the console */
#if defined(WIN32)
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0 /* send to all processes that shared this console, but in this case, the child should not be part of this console because it was started with  J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP*/);
#else /* defined(WIN32) */
		killpg(getpgrp(), SIGQUIT);
#endif /* defined(WIN32) */

		omrthread_sleep(3000);

		/* If the child handler was invoked, it would have written to its stdout - verify this was not the case */
		numBytesAvailable = (I_32)j9process_get_available (processHandle, J9PORT_PROCESS_STDOUT);
		if (0 < numBytesAvailable) {
			readFromProcess(portLibrary, testName, processHandle, J9PORT_PROCESS_STDOUT, buffer, MAX_BUF_SIZE);
			outputErrorMessage(PORTTEST_ERROR_ARGS, "The child handle was invoked when it should not have been (we know this because the child wrote this to stdout: \"%s\")\n", buffer);
		}

		/* We're done with this test, tell the child to shut down */
		retVal = writeToProcess(portLibrary, testName, processHandle, "hi quit\n", 9);

		if (retVal != -1) {
			retVal = pollProcessForComplete(portLibrary, testName, processHandle, timeoutMillis);
		}
	}

	if (retVal != -1) {
		retVal = j9process_close(&processHandle, 0);
	}

	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);

#undef MAX_BUF_SIZE
}


#if !defined(WIN32)

/**
 * Writes the process group id to stdout then reads from stdin until
 * the "quit" command is received
 *
 * @param[in] portLibrary The port library under test
 *
 * @return 0
 */
static int
processWriteProcessGroupID(struct J9PortLibrary *portLibrary)
{
	char buf[1024];
	int i=0;
	pid_t processGroupID = getpgrp();

	buf[0] = '\0';
	fprintf(stdout, "%u", processGroupID);
	fflush(stdout);

	/* read standard in until the "quit" command is received */
	do {
		char c = getchar();
		if ( c == ' ' || c == '\n' ){
			/* null terminate the string and start over */
			buf[i] = '\0';
			i = 0;
		} else {
			buf[i] = c;
			i++;
		}
	} while(strcmp(buf, "quit") != 0);

	
	return 0;
}

/**
 *
 * Tests correct operation of flag J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 *
 * Test 1
 * 		- Create the child process without J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 * 			- child writes its GPID to stdout
 *		- Read the group pid of the child, verify that it is the same this process
 *
 * Test 2
 * 		- Create/start another child process with J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 * 			- child writes its GPID to stdout
 *
 * 		- Read the group pid, verify that it is _not_ the same as this process
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test that is running, used in PORTTEST_ERROR_ARGS
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static IDATA
j9process_testCreateInNewProcessGroupUnix(struct J9PortLibrary *portLibrary)
{
#define MAX_BUF_SIZE 	256
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9ProcessHandle processHandle;
	IDATA retVal;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;
 	I_32 waitTime = 0;
 	pid_t parentProcessGroupID = getpgrp();
 	UDATA childProcessGroupID = 0;

 	char* testName = "j9process_testCreateProcessInNewGroup";
 	char *command[2];
 	UDATA commandLength = 2;
 	char buffer[MAX_BUF_SIZE];
 	I_64 timeoutMillis = 60000;

 	reportTestEntry(portLibrary, testName);

 	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
 		command[1] = "-child_j9process_processWriteProcessGroupID";
  	} else {
 		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
 		return reportTestExit(portLibrary, testName);
 	}

 	/********************************/
 	/********************************/
 	/* Test 1: When J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP is not used, the child should have the same pgid as the parent */
 	/********************************/
 	/********************************/
 	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);

	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {
		retVal = writeToProcess(portLibrary, testName, processHandle, "hi quit\n", 9);
	}

	if (retVal != -1) {
		retVal = pollProcessForComplete(portLibrary, testName, processHandle, timeoutMillis);
	}

	if (retVal != -1) {
		retVal = readFromProcess(portLibrary, testName, processHandle, J9PORT_PROCESS_STDOUT, buffer, MAX_BUF_SIZE);
	}

	childProcessGroupID = (UDATA)atoi(buffer);

	if (childProcessGroupID != parentProcessGroupID) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "parent and child should have same process group ID, instead parent pgid = %u, child pgid = %u\n", parentProcessGroupID, childProcessGroupID);
	} else {
		outputComment(PORTLIB, "child and parent have same pgid (as expected): (%u, %u)\n", childProcessGroupID, parentProcessGroupID);
	}

	if (retVal != -1) {
		retVal = j9process_close(&processHandle, 0);
	}

	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

	/********************************/
	/********************************/
	/* Test 2: When J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP is used, the child should not have the same pgid as the parent */
	/********************************/
	/********************************/
 	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);

	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {
		retVal = writeToProcess(portLibrary, testName, processHandle, "hi quit\n", 9);
	}

	if (retVal != -1) {
		retVal = pollProcessForComplete(portLibrary, testName, processHandle, timeoutMillis);
	}

	if (retVal != -1) {
		retVal = readFromProcess(portLibrary, testName, processHandle, J9PORT_PROCESS_STDOUT, buffer, MAX_BUF_SIZE);
	}

	childProcessGroupID = (UDATA)atoi(buffer);

	if (childProcessGroupID == parentProcessGroupID) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP used, so parent and child should not have same process group ID, instead parent pgid = %u, child pgid = %u\n", parentProcessGroupID, childProcessGroupID);
	} else {
		outputComment(PORTLIB, "child and parent different pgid (as expected): (%u, %u)\n", childProcessGroupID, parentProcessGroupID);
	}

	if (retVal != -1) {
		retVal = j9process_close(&processHandle, 0);
	}

	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);

#undef MAX_BUF_SIZE
}
#endif /* !defined(WIN32) */

/**
 * Writes approximately 12 MB of garbage to stdout and stderr
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0
 */
static int 
processWriteMore(struct J9PortLibrary *portLibrary)
{
	int i=0;
	PORT_ACCESS_FROM_PORT(portLibrary);
#define MESSAGE "This is exactly 128 characters if I keep on going for this amount until I get bored...........................................\n"

	for (i=0;i<100000;i++) {
		fprintf(stdout, MESSAGE);
		fflush(stdout);
		fprintf(stderr, MESSAGE);
		fflush(stderr);
	}
	return 0;
#undef MESSAGE
}

/**
 * Verify port library process management.
 * 
 * Ensure the port library is properly setup to run process operations.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_verifyPortLibrary(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "Verify_PortLibrary";

	reportTestEntry(portLibrary, testName);

	/* Verify that the j9port function pointers are non NULL */

	if (NULL == portLibrary->process_create) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_create is NULL\n");
	}
	
	if (NULL == portLibrary->process_waitfor) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_waitfor is NULL\n");
	}
	if (NULL == portLibrary->process_terminate) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_terminate is NULL\n");
	}
	
	if (NULL == portLibrary->process_write) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_write is NULL\n");
	}
	if (NULL == portLibrary->process_read) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_read is NULL\n");
	}
	
	if (NULL == portLibrary->process_get_available) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_get_available is NULL\n");
	}
	
	if (NULL == portLibrary->process_getStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_getStream is NULL\n");
	}
	
	if (NULL == portLibrary->process_isComplete) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_isComplete is NULL\n");
	}
	
	if (NULL == portLibrary->process_get_exitCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->process_get_exitCode is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Fire & Forget Usecase:
 * - create/start the process
 * - close the process (don't hold onto any resources)
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * 
 */
static void
j9process_fireAndForgetImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA retVal;

	J9ProcessHandle processHandle;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	 retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {
		retVal = j9process_close(&processHandle, 0);
		 	if (retVal != 0) {
		 		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
			}
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
}

/**
 * Verify port library process management.
 * 
 * Fire & Forget:
 * - Execute the different scenarios of this test (processSuccess, processCrash, etc.)
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_fireAndForget(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_fireAndForget";
	char *command[2];
	UDATA commandLength = 2;
	
	reportTestEntry(portLibrary, testName);
	
	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processSuccess"; 
		j9process_fireAndForgetImpl(portLibrary, testName, command, commandLength);	
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}
	/* Don't delete executable name (command[0]); system-owned. */

/* TODO fix this test, result incorrect where executing this test case after the dump tests */
#if 0	
	command[0] = "fake_pltest";
	command[1] = "";
	retVal = j9process_fireAndForgetImpl(portLibrary, command, commandLength);
	if(retVal == TEST_PASS){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "fake_pltest is an inexisting process");
	}
#endif
	
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Fire and WaitFor Usecase - Runtime.exec (Blocking)
 * - create/start the process
 * - block until the child process has completes, capture the exit code.
 * - close the process (don't hold onto any resources).
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * 
 */
static void
j9process_fireAndWaitForImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA retVal;

	J9ProcessHandle processHandle;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {

		retVal = j9process_waitfor(processHandle);
		if (retVal != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_waitfor returned error code %d.\n", retVal);
		} else {
			retVal = j9process_close(&processHandle, 0);
			if (retVal != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
			}
		}
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
}

/**
 * Verify port library process management.
 * 
 * Fire and WaitFor - Runtime.exec (Blocking)
 * - Execute the different scenarios of this test (processSuccess, processCrash, etc.)
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_fireAndWaitFor(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_fireAndWaitFor";
	char *command[2];
	UDATA commandLength = 2;
	
	reportTestEntry(portLibrary, testName);
	
	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processSuccess";
		j9process_fireAndWaitForImpl(portLibrary, testName, command, commandLength);

		command[1] = "-child_j9process_processSleep";
		j9process_fireAndWaitForImpl(portLibrary, testName, command, commandLength);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Fire and Check if Complete Usecase - Runtime.exe (Non-Blocking)
 * - create/start the process
 * - poll to see if the process is still running
 * - determine if the process exited normally
 * - get the return code
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test that is running, used in PORTTEST_ERROR_ARGS
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * 
 */
static void
j9process_fireAndCheckIfCompleteImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA retVal;
	J9ProcessHandle processHandle;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {
		/* wait for the process to complete */
		retVal = j9process_waitfor(processHandle);
		if (retVal != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_waitfor returned error code %d.\n", retVal);
		} else {
			retVal = j9process_isComplete(processHandle);
			if(0 == retVal){
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_isComplete returned that the process has not completed yet to get the exit code.\n");
			} else if (1 != retVal) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_isComplete returned with error code: %d\n", retVal);
			} else {
				retVal = j9process_get_exitCode(processHandle);
				outputComment(portLibrary, "j9process_get_exitCode report exit code: %d.\n", retVal);
			}

			retVal = j9process_close(&processHandle, 0);
			if (retVal != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
			}
		}
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
}

/**
 * Verify port library process management.
 * 
 * Fire and Check if Complete - Runtime.exe (Non-Blocking)
 * - Execute the different scenarios of this test (processSuccess, processCrash, etc.)
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_fireAndCheckIfComplete(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_fireAndCheckIfComplete";
	char *command[2];
	UDATA commandLength = 2;
	
	reportTestEntry(portLibrary, testName);
	
	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processSuccess";
		j9process_fireAndCheckIfCompleteImpl(portLibrary, testName, command, commandLength);


		/* TODO call this after processCrash correctly implemented */
#if 0	
		command[1] = "-child_j9process_processCrash";
		retVal = j9process_fireAndCheckIfCompleteImpl(portLibrary, command, commandLength);
		if(retVal != TEST_PASS){
			outputErrorMessage(PORTTEST_ERROR_ARGS, "processCrash failed: %d.\n", retVal);
		}
#endif 

	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Fire and Terminate Usecase - Hang
 * - create/start the process
 * - poll to see if the process is still running
 * - process did not complete, terminate explicitly
 * - close the process, freeing any resources the API was holding onto.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * 
 */
static void
j9process_fireAndTerminateImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA retVal;

	J9ProcessHandle processHandle;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {

		retVal = j9process_isComplete(processHandle);
		if (0 == retVal) { /* the process has not completed */

			retVal = j9process_terminate(processHandle);
			if (retVal != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", retVal);
			}
		} else if (1 == retVal) { /* the process has completed */

			retVal = j9process_close(&processHandle, 0);
			if (0 != retVal) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_isComplete returned error code %d.\n", retVal);
		}
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
}

/**
 * Verify port library process management.
 * 
 * Fire and Terminate - Hang
 * - Execute the different scenarios of this test (processSuccess, processCrash, etc.)
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_fireAndTerminate(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "Fire_And_Terminate";
	char *command[2];
	UDATA commandLength = 2;
	
	reportTestEntry(portLibrary, testName);

	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processSuccess";
		j9process_fireAndTerminateImpl(portLibrary, testName, command, commandLength);

		command[1] = "-child_j9process_processSleep";
		j9process_fireAndTerminateImpl(portLibrary, testName, command, commandLength);

		/* TODO call this after processCrash correctly implemented */	
#if 0 
		command[1] = "-child_j9process_processCrash";
		retVal = j9process_fireAndTerminateImpl(portLibrary, command, commandLength);
		if(retVal != TEST_PASS){
			outputErrorMessage(PORTTEST_ERROR_ARGS, "processCrash failed: %d.\n", retVal);
		}
#endif 
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Fire, Write and Read Usecase
 * - create/start the process
 * - write to the input stream of the process
 * - read from the output/error stream of the process
 * - process did not complete, terminate explicitly
 * - close the process, freeing any resources the API was holding onto.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test that is running, used in PORTTEST_ERROR_ARGS
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * 
 */
static void
j9process_fireWriteAndReadImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength) 
{
#define TIMEOUT_MILLIS	60000
#define SLEEP_MILLIS	100
#define MAX_BUF_SIZE 	256
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9ProcessHandle processHandle;
	IDATA retVal;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;
 	I_32 numBytesAvailable;
 	I_32 waitTime = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
		return;
	}


	retVal = j9process_write(processHandle, "hi quit\n", 9);
	if ( 0 <= retVal) {
		outputComment(portLibrary, "j9process_write wrote %d characters.\n", retVal);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_write returned error code %d.\n", retVal);
		retVal = j9process_terminate(processHandle);
		if (0 != retVal) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", retVal);
		}
		return;
	}

	/* wait for child to complete, break after timeout */
	while (waitTime < TIMEOUT_MILLIS) {
		retVal = j9process_isComplete(processHandle);
		if (retVal == 1) {
			break;
		} else {
			omrthread_sleep((I_64)SLEEP_MILLIS);
			waitTime += SLEEP_MILLIS;
		}
	}

	if (1 != retVal) {
		/* The timeout was reached and the child has not yet finished */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child did not finish before timeout\n", retVal);
		retVal = j9process_terminate(processHandle);
		if (0 != retVal) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", retVal);
		}
		return;
	}

	/* Read J9PORT_PROCESS_STDOUT */
	numBytesAvailable = (I_32)j9process_get_available (processHandle, J9PORT_PROCESS_STDOUT);
	if (MAX_BUF_SIZE < numBytesAvailable) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "(numBytesAvailable = %d) > MAX_BUF_SIZE\n", numBytesAvailable);
	} else if (0 < numBytesAvailable) {
		char buffer[MAX_BUF_SIZE];

		outputComment(portLibrary, "j9process_get_available reports %d character(s) to read from the process output stream.\n", numBytesAvailable);
		retVal = j9process_read(processHandle, J9PORT_PROCESS_STDOUT, buffer, numBytesAvailable);

		if (0 <= retVal) {
			buffer[numBytesAvailable] = '\0';
			outputComment(portLibrary, "j9process_read read %d character(s) from the process output stream: \"%s\"\n", retVal, buffer);
			if ( 0 != strcmp(buffer, HELLO_STANDARD_OUT) ) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read incorrect message read from J9PORT_PROCESS_STDOUT\n", retVal);
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read trying to read from the output stream returned error code %d.\n", retVal);
		}
	}else if (0 == numBytesAvailable) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available reports that there are no characters to read from the process output stream.\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available failed with error code: %d\n", numBytesAvailable);
	}

	/* Read J9PORT_PROCESS_STDERR */
	numBytesAvailable = (I_32)j9process_get_available (processHandle, J9PORT_PROCESS_STDERR);
	if (MAX_BUF_SIZE < numBytesAvailable) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "(numBytesAvailable = %d) > MAX_BUF_SIZE\n", numBytesAvailable);
	} else if (0 < numBytesAvailable) {
		char buffer[MAX_BUF_SIZE];

		outputComment(portLibrary, "j9process_get_available reports %d character(s) to read from the process error stream.\n", numBytesAvailable);
		retVal = j9process_read(processHandle, J9PORT_PROCESS_STDERR, buffer, numBytesAvailable);

		if (0 <= retVal) {
			buffer[numBytesAvailable] = '\0';
			outputComment(portLibrary, "j9process_read read %d character(s) from the process error stream: \"%s\"\n", retVal, buffer);
			if ( 0 != strcmp(buffer, HELLO_STANDARD_ERR) ) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read incorrect message read from J9PORT_PROCESS_STDERR\n", retVal);
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read trying to read from the error stream returned error code %d.\n", retVal);
		}
	} else if (0 == numBytesAvailable) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available reports that there are no characters to read from the process error stream.\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available failed with error code: %d\n", numBytesAvailable);
	}

	retVal = j9process_close(&processHandle, 0);
	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
#undef MAX_BUF_SIZE
#undef SLEEP_MILLIS
#undef TIMEOUT_MILLIS
}


/**
 * Verify port library process management.
 * 
 * Verify translation of environment variables from UTF8 to platform native encoding.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_envTest(struct J9PortLibrary *portLibrary)
{
#define J9_MAX_ENV 32767
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_envTest";
	char *dir = ".";
	const UDATA commandLength = 2;
	const char *command[2];
	IDATA retVal = 0;
	J9ProcessHandle processHandle;
	I_64 timeoutMillis = 60000;
	char **env = NULL;
	UDATA envSize = 0;

	reportTestEntry(portLibrary, testName);

	if (0 == j9sysinfo_get_executable_name("pltest", (char **)&command[0]) ) {
		command[1] = "-child_j9process_processSaveEnv";
		retVal = j9process_create(command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
		if (0 == retVal) {
			retVal = pollProcessForComplete(portLibrary, testName, processHandle, timeoutMillis);
			if (-1 != retVal) {
				IDATA rc = -1;
				J9SysinfoEnvIteratorState state;
				UDATA envSize = 0;
				void *sysinfo_env_iterator_buffer = NULL;

				memset(&state, 0, sizeof(state));

				/* get the size of the buffer required to hold the iterator */
				rc = j9sysinfo_env_iterator_init(&state, NULL, 0);

				if (0 < rc) {
					envSize = rc;
					sysinfo_env_iterator_buffer = j9mem_allocate_memory(envSize, OMRMEM_CATEGORY_PORT_LIBRARY);
					if ((NULL != sysinfo_env_iterator_buffer)) {
						IDATA fd1 = j9file_open(PROCESS_SAVE_ENV_FILE, EsOpenRead, 0444);
						if (-1 != fd1) {
							rc = j9sysinfo_env_iterator_init(&state, sysinfo_env_iterator_buffer, envSize);
							if (0 == rc) {
								while (j9sysinfo_env_iterator_hasNext(&state)) {
									J9SysinfoEnvElement element;
									rc = j9sysinfo_env_iterator_next(&state, &element);
									if (0 == rc) {
										IDATA elementLen = strlen(element.nameAndValue);

										char *buffer = (char *) j9mem_allocate_memory(elementLen + 1, OMRMEM_CATEGORY_PORT_LIBRARY);
										rc = j9file_read(fd1, buffer, elementLen + 1);
										if (-1 != rc) {
											buffer[elementLen] = 0;
											rc = strcmp(buffer, element.nameAndValue);
											if (0 != rc) {
												outputErrorMessage(PORTTEST_ERROR_ARGS, "Matching failed: rc = %d, str1 = %s, str2=%s\n", retVal, buffer, element.nameAndValue);
												j9mem_free_memory(buffer);
												break;
											} else {
												outputComment(portLibrary, "Matching %s\n", buffer);
											}
										} else {
											outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to read env var %s from file\n", retVal, buffer);
											j9mem_free_memory(buffer);
											break;
										}
										j9mem_free_memory(buffer);
									} else {
										outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_env_iterator_next failed\n");
										break;
									}
								}
							}
							if (-1 != j9file_close(fd1)) {
								if (-1 == j9file_unlink(PROCESS_SAVE_ENV_FILE)) {
									outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to delete test file\n");
								}
							} else {
								outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to close test file\n");
							}
						} else {
							outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to open env file\n");
						}
						j9mem_free_memory(sysinfo_env_iterator_buffer);
					} else {
						outputErrorMessage(PORTTEST_ERROR_ARGS,"j9mem_allocate_memory failed\n");
					}
				} else {
					outputErrorMessage(PORTTEST_ERROR_ARGS,"j9sysinfo_env_iterator_init failed\n");
				}
			}
			j9process_terminate(processHandle);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Error: child returned error code %d.\n", retVal);
		}
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
#undef J9_MAX_ENV
}
/**
 * Verify port library process management.
 *
 * Fire, Write and Read
 * - Execute the different scenarios of this test (processSuccess, processCrash, etc.)
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_fireWriteAndRead(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_fireWriteAndRead";
	char *command[2];
	UDATA commandLength = 2;

	reportTestEntry(portLibrary, testName);

	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {

		command[1] = "-child_j9process_processWrite";
		j9process_fireWriteAndReadImpl(portLibrary, testName, command, commandLength);

		/* TODO call this after processCrash correctly implemented */	
#if 0 
		command[1] = "-child_j9process_processCrash";
		retVal = j9process_fireWriteAndReadImpl(portLibrary, command, commandLength);
		if(retVal != TEST_PASS){
			outputErrorMessage(PORTTEST_ERROR_ARGS, "processCrash failed: %d.\n", retVal);
		}
#endif 
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Ensure that an attempt to create a new process which runs a non-existent
 * command fails and returns with an error.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_testExecFailure(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_testExecFailure";
	const char *command[1];
	UDATA commandLength = 1;
	IDATA retVal;
	J9ProcessHandle processHandle;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;

	reportTestEntry(portLibrary, testName);

	command[0] = "./THERES_NO_WAY_THIS_PROCESS_EXISTS";

	retVal = j9process_create(command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (0 == retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned successfully even though the non-existent process %s was called.\n", command[0]);
		j9process_terminate(processHandle);
	} else {
		outputComment(PORTLIB, "j9process_create successfully detected that process doesn't exist, returned error code %d.\n", retVal);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Ensure that passing in an invalid dir to j9process_create
 * causes it to return an error
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_testChdirFailure(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_testChdirFailure";
	const char *command[1];
	UDATA commandLength = 1;
	IDATA retVal;
	J9ProcessHandle processHandle;
	char *dir = "./THERES_NO_WAY_THIS_DIRECTORY_EXISTS";
	char **env = NULL;
	UDATA envSize = 0;

	reportTestEntry(portLibrary, testName);

	command[0] = "./THERES_NO_WAY_THIS_PROCESS_EXISTS";

	retVal = j9process_create(command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (0 == retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned successfully even though the non-existent directory %s was passed in as a parameter.\n", dir);
		j9process_terminate(processHandle);
	} else {
		outputComment(PORTLIB, "j9process_create successfully detected that directory doesn't exist, returned error code %d.\n", retVal);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 * 
 * Ensure that children launched with J9PORT_PROCESS_NONBLOCKING_IO and/or
 * J9PORT_PROCESS_IGNORE_OUTPUT will not hang if they attempt to output a
 * larger amount of data than can be held within the outHandle and errHandle
 * pipe buffers
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_testNonBlockingIOAndIgnoreOutput(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_testNonBlockingIOAndIgnoreOutput";
	char *command[2];
	UDATA commandLength = 2;
	U_32 options;
	
	reportTestEntry(portLibrary, testName);
	
	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processWriteMore";

		options = J9PORT_PROCESS_NONBLOCKING_IO;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_NONBLOCKING_IO\n");
		testNonBlockingIOAndIgnoreOutputImpl(portLibrary, testName, command, commandLength, options);

		options = J9PORT_PROCESS_IGNORE_OUTPUT;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_IGNORE_OUTPUT\n");
		testNonBlockingIOAndIgnoreOutputImpl(portLibrary, testName, command, commandLength, options);

		options = J9PORT_PROCESS_IGNORE_OUTPUT | J9PORT_PROCESS_NONBLOCKING_IO;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_IGNORE_OUTPUT | J9PORT_PROCESS_NONBLOCKING_IO\n");
		testNonBlockingIOAndIgnoreOutputImpl(portLibrary, testName, command, commandLength, options);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Create a child with the specified options flag and wait until the
 * child's execution completes or the timeout is reached. If the timeout
 * is reached before the child has completed this is considered a failure.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test that is running, used in PORTTEST_ERROR_ARGS
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * @param[in] options The options flag to be passed into @ref j9process_create
 */
static void
testNonBlockingIOAndIgnoreOutputImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength, U_32 options) 
{
#define TIMEOUT_MILLIS	60000
#define SLEEP_MILLIS	100
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9ProcessHandle processHandle;
	IDATA retVal;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;
 	I_32 waitTime = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
		return;
	}

	/* Wait for the process to complete */
	while ( (0 == j9process_isComplete(processHandle)) && (TIMEOUT_MILLIS > waitTime) ) {
		omrthread_sleep(SLEEP_MILLIS);
		waitTime += SLEEP_MILLIS;
	}
	retVal = j9process_isComplete(processHandle);
	if ( 0 == retVal ) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "child process hung\n");
	} else if ( 0 > retVal ) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_isComplete returned with error=%d\n", retVal);
	}

	retVal = j9process_close(&processHandle, 0);
	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
#undef SLEEP_MILLIS
#undef TIMEOUT_MILLIS
}

/**
 * Verify port library process management.
 *
 * Fire & Forget Usecase:
 * - create/start the process
 * - close the process handle via option J9PORT_PROCESS_DO_NOT_CLOSE_STREAMS
 * - close streams via j9file_close()
 *
 * @param[in] portLibrary The port library under test
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 *
 */
static void
j9process_testCloseScenarioImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA retVal;

	J9ProcessHandle processHandle;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;
	IDATA inHandle, outHandle, errHandle;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, 0, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
	} else {
		j9process_getStream(processHandle, J9PORT_PROCESS_STDIN, &inHandle);
		j9process_getStream(processHandle, J9PORT_PROCESS_STDOUT, &outHandle);
		j9process_getStream(processHandle, J9PORT_PROCESS_STDERR, &errHandle);
		retVal = j9process_close(&processHandle, J9PORT_PROCESS_DO_NOT_CLOSE_STREAMS);
		if (retVal != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
		}
		retVal = j9file_close(inHandle);
		if (retVal != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_close returned error code %d.\n", retVal);
		}
		retVal = j9file_close(outHandle);
		if (retVal != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_close returned error code %d.\n", retVal);
		}
		retVal = j9file_close(errHandle);
		if (retVal != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_close returned error code %d.\n", retVal);
		}
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
}

/**
 * Verify port library process management.
 *
 * Fire & Forget:
 * - Execute the different scenarios of this test (processSuccess, processCrash, etc.)
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_testCloseScenario(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_fireAndForget";
	char *command[2];
	UDATA commandLength = 2;

	reportTestEntry(portLibrary, testName);

	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processSuccess";
		j9process_testCloseScenarioImpl(portLibrary, testName, command, commandLength);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library process management.
 *
 * Fire, Write and Read
 * - Execute the different scenarios of this test (
 *  default options,
 *  	i.e., options = 0, fdInput = fdOutput = fdError = J9PORT_INVALID_FD
 *  	stdin, stdout, stderr from/to pipes
 *  options = J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT
 *  	i.e., stderr redirected to stdout
 * 	options = J9PORT_PROCESS_INHERIT_STDIN
 * 		i.e., stdin inherit from parent process
 * 	options = J9PORT_PROCESS_INHERIT_STDOUT
 * 		i.e., stdout inherit from parent process
 * 	options = J9PORT_PROCESS_INHERIT_STDERR
 * 		i.e., stderr inherit from parent process
 * 	options = J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR
 * 		i.e., both stdout and stderr inherit from parent process
 *  options = 0 && fdInput != J9PORT_INVALID_FD
 *  	i.e., stdin redirected from fdInput
 *  options = 0 && fdOutput != J9PORT_INVALID_FD
 *  	i.e., stdout redirected to fdOutput
 *  options = 0 && fdError != J9PORT_INVALID_FD
 *  	i.e., stderr redirected to fdError
 *  options = J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT && fdInput != J9PORT_INVALID_FD
 *  	i.e., both stdout & stderr redirected to fdOutput
 *  options = 0 && fdOutput != J9PORT_INVALID_FD && fdError != J9PORT_INVALID_FD
 *  	i.e., stdout redirected to fdOutput and stderr redirected to fdError
 *  )
 *
 * Notes: Not find a way to write own stdin yet,
 * 		hence J9PORT_PROCESS_INHERIT_STDIN can't be tested within automatic environment.
 * 		To do a manual test instead, comment out ifdef 0 for following code:
 * 			options = J9PORT_PROCESS_INHERIT_STDIN;
 *			outputComment(PORTLIB, "options: J9PORT_PROCESS_INHERIT_STDIN\n");
 *			j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);
 *   	typing "hi quit\n" at pltest console window when test stopped with following output:
 *     		options: J9PORT_PROCESS_INHERIT_STDIN
 * 			Starting process E:\IVE\IVEjres\HEAD\jvmwi3270\jvmwi3270-20100805\jre\bin\default\pltest.exe -child_j9process_processWrite
 * 		Test should continue without failure
 * 		otherwise a failure will be generated for "Child did not finish before timeout"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9process_testRedirect(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* testName = "j9process_testRedirect";
	char *command[2];
	UDATA commandLength = 2;
	U_32 options;
	IDATA fdInput = J9PORT_INVALID_FD;
	IDATA fdOutput = J9PORT_INVALID_FD;
	IDATA fdError = J9PORT_INVALID_FD;
	IDATA fd1;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	if ( 0 == j9sysinfo_get_executable_name("pltest", &command[0]) ) {
		command[1] = "-child_j9process_processWrite";

		/* default options */
		options = 0;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_PIPE_TO_PARENT\n");
		j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);

		/* J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT */
		options = J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT\n");
		j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);

		/* Following test require manual input "hi quit\n" in current process otherwise child process would wait forever */
#if 0
		/* J9PORT_PROCESS_INHERIT_STDIN */
		options = J9PORT_PROCESS_INHERIT_STDIN;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_INHERIT_STDIN\n");
		j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);
#endif

		/* J9PORT_PROCESS_INHERIT_STDOUT */
		options = J9PORT_PROCESS_INHERIT_STDOUT;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_INHERIT_STDOUT\n");
		j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);

		/* J9PORT_PROCESS_INHERIT_STDERR */
		options = J9PORT_PROCESS_INHERIT_STDERR;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_INHERIT_STDERR\n");
		j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);

		/* J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR */
		options = J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR\n");
		j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD);

		/* redirect to fdInput */
		fd1 = j9file_open("fdinput.txt", EsOpenCreate | EsOpenWrite, 0666);
		if (-1 != fd1) {
			j9file_write(fd1, "hi quit\n", 8);

			rc = j9file_close(fd1);
			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_close of fdinput.txt NOT successful \n");
			} else {
				options = 0;
				outputComment(PORTLIB, "options: fdInput != J9PORT_INVALID_FD \n");
				fdInput = j9file_open("fdinput.txt", EsOpenRead | EsOpenForInherit, 0444);
				j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, fdInput, J9PORT_INVALID_FD, J9PORT_INVALID_FD);
				j9file_close(fdInput);
				j9file_unlink("fdinput.txt");
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open of fdinput.txt NOT successful \n");
		}

		/* redirect to fdOutput */
		options = 0;
		outputComment(PORTLIB, "options: fdOutput != J9PORT_INVALID_FD \n");
		fdOutput = j9file_open("fdOutput.txt", EsOpenCreate | EsOpenWrite | EsOpenForInherit, 0666);
		if (fdOutput != -1) {
			j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, fdOutput, J9PORT_INVALID_FD);
			j9file_close(fdOutput);
			if (verifyFileContent(portLibrary, testName, "fdOutput.txt", HELLO_STANDARD_OUT) == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "verifyFileContent of fdOutput.txt NOT successful \n");
			} else {
				outputComment(PORTLIB, "verifyFileContent of fdOutput.txt successful \n");
			}
			j9file_unlink("fdOutput.txt");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open fdOutput.txt NOT successful \n");
		}

		/* redirect to fdError */
		options = 0;
		outputComment(PORTLIB, "options: fdError != J9PORT_INVALID_FD \n");
		fdError = j9file_open("fdError.txt", EsOpenCreate | EsOpenWrite | EsOpenForInherit, 0666);
		if (fdError != -1) {
			j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, J9PORT_INVALID_FD, fdError);
			j9file_close(fdError);
			if (verifyFileContent(portLibrary, testName, "fdError.txt", HELLO_STANDARD_ERR) == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "verifyFileContent of fdError.txt NOT successful \n");
			} else {
				outputComment(PORTLIB, "verifyFileContent of fdError.txt successful \n");
			}
			j9file_unlink("fdError.txt");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open fdError.txt NOT successful \n");
		}

		/* J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT */
		options = J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT;
		outputComment(PORTLIB, "options: J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT && fdOutput != J9PORT_INVALID_FD \n");
		fdOutput = j9file_open("fdOutput.txt", EsOpenCreate | EsOpenWrite | EsOpenForInherit, 0666);
		if (fdOutput != -1) {
			j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, fdOutput, J9PORT_INVALID_FD);
			j9file_close(fdOutput);
			if (verifyFileContent(portLibrary, testName, "fdOutput.txt", HELLO_STANDARD_OUTNERR) == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "verifyFileContent of fdOutput.txt with J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT NOT successful \n");
			} else {
				outputComment(PORTLIB, "verifyFileContent of fdOutput.txt with J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT successful \n");
			}
			j9file_unlink("fdOutput.txt");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open fdOutput.txt with J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT NOT successful \n");
		}

		/* redirect to fdOutput && fdError */
		options = 0;
		outputComment(PORTLIB, "options: fdOutput != J9PORT_INVALID_FD && fdError != J9PORT_INVALID_FD \n");
		fdOutput = j9file_open("fdOutput.txt", EsOpenCreate | EsOpenWrite | EsOpenForInherit, 0666);
		fdError = j9file_open("fdError.txt", EsOpenCreate | EsOpenWrite | EsOpenForInherit, 0666);
		if (fdOutput != -1 && fdError != -1) {
			j9process_fireWriteAndReadWithRedirectImpl(portLibrary, testName, command, commandLength, options, J9PORT_INVALID_FD, fdOutput, fdError);
			j9file_close(fdOutput);
			if (verifyFileContent(portLibrary, testName, "fdOutput.txt", HELLO_STANDARD_OUT) == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "verifyFileContent of fdOutput.txt NOT successful \n");
			} else {
				outputComment(PORTLIB, "verifyFileContent of fdOutput.txt successful \n");
			}
			j9file_unlink("fdOutput.txt");
			j9file_close(fdError);
			if (verifyFileContent(portLibrary, testName, "fdError.txt", HELLO_STANDARD_ERR) == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "verifyFileContent of fdError.txt NOT successful \n");
			} else {
				outputComment(PORTLIB, "verifyFileContent of fdError.txt successful \n");
			}
			j9file_unlink("fdError.txt");
		} else {
			if (fdOutput == -1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open fdOutput.txt NOT successful \n");
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open fdError.txt NOT successful \n");
			}
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"pltest\", &command[0]) failed\n");
	}

	/* Don't delete executable name (command[0]); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify if the file content match incoming string
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test that is running, used in PORTTEST_ERROR_ARGS
 * @param[in] file1name The file to be verified
 * @param[in] expectedResult The string to be compared
 * 
 * @return TRUE on match, FALSE on NOT match
 */
BOOLEAN
verifyFileContent(struct J9PortLibrary *portLibrary, const char* testName, char *file1name, char* expectedResult)
{
	BOOLEAN result = FALSE;
	IDATA fd1;
	char *readBufPtr;
	char readBuf[255];

	PORT_ACCESS_FROM_PORT(portLibrary);

	fd1 = j9file_open(file1name, EsOpenRead, 0444);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open() failed\n");
		goto exit;
	}

	readBufPtr = j9file_read_text(fd1, readBuf, sizeof(readBuf));
	if (NULL == readBufPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned NULL\n");
		goto exit;
	}

	if (strlen(readBufPtr) != strlen(expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned %d expected %d\n", strlen(readBufPtr), strlen(expectedResult));
		goto exit;
	}

	/* TODO j9file_read_text() does not currently do translations */
#if !defined(J9ZOS390)
	if (strcmp(readBufPtr, expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
		goto exit;
	}
#endif /* J9ZOS390 */

	result = TRUE;

exit:
	j9file_close(fd1);

	return	result;
}

/**
 * Verify port library process management.
 *
 * Fire, Write and Read Use-case
 * - create/start the process
 * - write to the input stream of the process
 * 		or open an existing file as child process input
 * - read from the output/error stream of the process
 * 		or verify the files redirected by child process stdout/stderr
 * - process did not complete, terminate explicitly
 * - close the process, freeing any resources the API was holding onto.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test that is running, used in PORTTEST_ERROR_ARGS
 * @param[in] command An array of NULL-terminated strings representing the command line to be executed
 * @param[in] commandLength The number of elements in the command line where by convention the first element is the file to be executed
 * @param[in] options The options flag to be passed into @ref j9process_create
 * @param[in] fdInput The fdInput to be passed into @ref j9process_create
 * @param[in] fdOutput The fdOutput to be passed into @ref j9process_create
 * @param[in] fdError The fdError to be passed into @ref j9process_create
 */

static void
j9process_fireWriteAndReadWithRedirectImpl(struct J9PortLibrary *portLibrary, const char *testName, char** command, UDATA commandLength, U_32 options, IDATA fdInput, IDATA fdOutput, IDATA fdError)
{
#define TIMEOUT_MILLIS	60000
#define SLEEP_MILLIS	100
#define MAX_BUF_SIZE 	256
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9ProcessHandle processHandle;
	IDATA retVal;
	char *dir = ".";
	char **env = NULL;
	UDATA envSize = 0;
 	I_32 numBytesAvailable = 0;
 	I_32 waitTime = 0;

	j9tty_printf(portLibrary, " Starting process %s %s\n", command[0], command[1]);

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, options, fdInput, fdOutput, fdError, &processHandle);
	if (retVal != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_create returned error code %d.\n", retVal);
		return;
	}

	if ((options & J9PORT_PROCESS_INHERIT_STDIN) == J9PORT_PROCESS_INHERIT_STDIN) {
		/* send quit signal to child process via parent stdin */
		/* ToDo: to identify a way to write to own stdin */
	} else if ( fdInput == J9PORT_INVALID_FD) {
		/* child process stdin not redirect from a file */
		retVal = j9process_write(processHandle, "hi quit\n", 9);
		if ( 0 <= retVal) {
			outputComment(portLibrary, "j9process_write wrote %d characters.\n", retVal);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_write returned error code %d.\n", retVal);
			retVal = j9process_terminate(processHandle);
			if (0 != retVal) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", retVal);
			}
			return;
		}
	}


	/* wait for child to complete, break after timeout */
	while (waitTime < TIMEOUT_MILLIS) {
		retVal = j9process_isComplete(processHandle);
		if (retVal == 1) {
			break;
		} else {
			omrthread_sleep((I_64)SLEEP_MILLIS);
			waitTime += SLEEP_MILLIS;
		}
	}

	if (1 != retVal) {
		/* The timeout was reached and the child has not yet finished */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child did not finish before timeout\n", retVal);
		retVal = j9process_terminate(processHandle);
		if (0 != retVal) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", retVal);
		}
		return;
	}

	/* Read J9PORT_PROCESS_STDOUT */
	if ((options & J9PORT_PROCESS_INHERIT_STDOUT) == J9PORT_PROCESS_INHERIT_STDOUT) {
		outputComment(portLibrary, "\n STDOUT inherit from parent process, "
				"i.e., child process output (%s) show up at parent process output. \n", HELLO_STANDARD_OUT);
	} else if (fdOutput == J9PORT_INVALID_FD) {
		/* child process stdout redirected to pipe by default */
		numBytesAvailable = (I_32)j9process_get_available (processHandle, J9PORT_PROCESS_STDOUT);

		if (MAX_BUF_SIZE < numBytesAvailable) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "(numBytesAvailable = %d) > MAX_BUF_SIZE\n", numBytesAvailable);
		} else if (0 < numBytesAvailable) {
			char buffer[MAX_BUF_SIZE];

			outputComment(portLibrary, "available reports %d character(s) to read from the process output stream.\n", numBytesAvailable);

			retVal = j9process_read(processHandle, J9PORT_PROCESS_STDOUT, buffer, numBytesAvailable);
			if (0 <= retVal) {
				buffer[numBytesAvailable] = '\0';
				outputComment(portLibrary, "j9process_read read %d character(s) from the process output stream: \"%s\"\n", retVal, buffer);
				if ((options & J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT) == J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT) {
					if ( 0 != strcmp(buffer, HELLO_STANDARD_OUTNERR) ) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read incorrect message read from J9PORT_PROCESS_STDOUT with option J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT\n", retVal);
					}
				} else {
					if ( 0 != strcmp(buffer, HELLO_STANDARD_OUT) ) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read incorrect message read from J9PORT_PROCESS_STDOUT\n", retVal);
					}
				}
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read trying to read from the output stream returned error code %d.\n", retVal);
			}
		} else if (0 == numBytesAvailable) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available reports that there are no characters to read from the process output stream.\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available failed with error code: %d\n", numBytesAvailable);
		}
	}
	/* in case of fdOutput != J9PORT_INVALID_FD, i.e., stdout redirected to fdOutput */
	/* File created from STDOUT to be verified outside of this method by verifyFileContent() */

	/* Read J9PORT_PROCESS_STDERR */
	if ((options & J9PORT_PROCESS_INHERIT_STDERR) == J9PORT_PROCESS_INHERIT_STDERR) {
		outputComment(portLibrary, "\n STDERR inherit from parent process, "
				"i.e., child process err (%s) show up at parent process err. \n", HELLO_STANDARD_ERR);
	} else if ((options & J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT) == J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT) {
		outputComment(portLibrary, "\n STDERR (%s) redirected to STDOUT. \n", HELLO_STANDARD_ERR);
	} else  if (fdError == J9PORT_INVALID_FD) {
		/* child process stderr redirected to pipe by default */
		numBytesAvailable = (I_32)j9process_get_available (processHandle, J9PORT_PROCESS_STDERR);
		if (MAX_BUF_SIZE < numBytesAvailable) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "(numBytesAvailable = %d) > MAX_BUF_SIZE\n", numBytesAvailable);
		} else if (0 < numBytesAvailable) {
			char buffer[MAX_BUF_SIZE];

			outputComment(portLibrary, "j9process_get_available reports %d character(s) to read from the process error stream.\n", numBytesAvailable);

			retVal = j9process_read(processHandle, J9PORT_PROCESS_STDERR, buffer, numBytesAvailable);
			if (0 <= retVal) {
				buffer[numBytesAvailable] = '\0';
				outputComment(portLibrary, "j9process_read read %d character(s) from the process error stream: \"%s\"\n", retVal, buffer);
				if ( 0 != strcmp(buffer, HELLO_STANDARD_ERR) ) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read incorrect message read from J9PORT_PROCESS_STDERR\n", retVal);
				}
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read trying to read from the error stream returned error code %d.\n", retVal);
			}
		} else if (0 == numBytesAvailable) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available reports that there are no characters to read from the process error stream.\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available failed with error code: %d\n", numBytesAvailable);
		}
	}
	/* in case of fdError != J9PORT_INVALID_FD, i.e., stderr redirected to fdError */
	/* File created from STDERR to be verified outside of this method by verifyFileContent() */

	retVal = j9process_close(&processHandle, 0);
	if (0 != retVal) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_close returned error code %d.\n", retVal);
	}

	j9tty_printf(portLibrary, " Ending process %s %s\n", command[0], command[1]);
#undef MAX_BUF_SIZE
#undef SLEEP_MILLIS
#undef TIMEOUT_MILLIS
}

/**
 * Verify port library process management.
 * 
 * Exercise all API related to port library process management found in
 * @ref j9process.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9process_runTests(struct J9PortLibrary *portLibrary, char *argv0, char* j9process_child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = 0;
	
	if (j9process_child != NULL) {
		if (strcmp(j9process_child, "j9process_processCrash") == 0) {
			return processCrash(portLibrary);
		} else if (strcmp(j9process_child, "j9process_processSleep") == 0) {
			return processSleep(portLibrary, 1);
		} else if (strcmp(j9process_child, "j9process_processSuccess") == 0) {
			return processSuccess(portLibrary);
		} else if (strcmp(j9process_child, "j9process_processWrite") == 0) {
			return processWrite(portLibrary);
		} else if (strcmp(j9process_child, "j9process_processWriteMore") == 0) {
			return processWriteMore(portLibrary);
		} else if (strcmp(j9process_child, "j9process_processRegisterHandlerAndSpin") == 0) {
			return processRegisterHandlerAndSpin(portLibrary);
		} else if (strcmp(j9process_child, "j9process_processSaveEnv") == 0) {
			return processSaveEnv(portLibrary);
		}
#if !defined(WIN32)
		else if (strcmp(j9process_child, "j9process_processWriteProcessGroupID") == 0) {
			return processWriteProcessGroupID(portLibrary);
		}
#endif /* !defined(WIN32) */
		else {
			return TEST_FAIL;
		}
	}

	/* Display unit under test */
	HEADING(portLibrary, "Process test");

	rc = j9process_verifyPortLibrary(portLibrary);

	if (TEST_PASS == rc) {
		rc |= j9process_fireAndForget(portLibrary);
		rc |= j9process_fireAndWaitFor(portLibrary);
		rc |= j9process_fireAndCheckIfComplete(portLibrary);
		rc |= j9process_fireAndTerminate(portLibrary);
		rc |= j9process_fireWriteAndRead(portLibrary);
		rc |= j9process_testExecFailure(portLibrary);
		rc |= j9process_testChdirFailure(portLibrary);
		rc |= j9process_testNonBlockingIOAndIgnoreOutput(portLibrary);
		rc |= j9process_testRedirect(portLibrary);
#if !defined(WIN32)
		rc |= j9process_testCreateInNewProcessGroupUnix(portLibrary);
#if !(defined(J9ZOS390) || defined(LINUX) || defined(OSX))
		/* z/OS: writes extra characters to the console in the build environment when a process catches a SIGQUIT.
		 * Linux: in certain Linux distros the /bin/sh command receives a SIGQUIT signal, see PR 56109.
		 * OSX: make, tee and other test framework processes receive a SIGQUIT signal, which causes the test
		 *      framework to crash. See https://github.com/eclipse/openj9/issues/4520.
		 */
		rc |= j9process_testNewProcessGroupByTriggeringSignal(portLibrary);
#endif /* !(defined(J9ZOS390) || defined(LINUX) || defined(OSX)) */
#endif	/* !defined(WIN32) */
		rc |= j9process_envTest(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nProcess test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
