/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
/* These headers are required for ChildrenTest */
#if defined(WIN32)
#include <process.h>
#include <Windows.h>
#endif

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#define PROCESS_HELPER_SEM_KEY 0x05CAC8E /* Reads OSCACHE */
#endif

#if defined(J9ZOS390)
#include <spawn.h>
#include <stdlib.h> /* for malloc for atoe */
#include "atoe.h"
#endif

#include <string.h>

#include "testProcessHelpers.h"
#include "testHelpers.h"
#include "j9port.h"

#if 0
#define PORTTEST_PROCESS_HELPERS_DEBUG
#endif

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)

#if (defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)) || defined(OSX)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds* buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int* array;  /* array for GETALL, SETALL */
	struct seminfo* __buf;      /* buffer for IPC_INFO */
};
#endif

IDATA 
openLaunchSemaphore (J9PortLibrary* portLibrary, const char* name, UDATA nProcess) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	IDATA semid = semget(PROCESS_HELPER_SEM_KEY, 1, 0660 | IPC_CREAT | IPC_EXCL);

	if(-1 == semid) {
		if(errno == EEXIST) {
			semid = semget(PROCESS_HELPER_SEM_KEY, 1, 0660);
		}
	} else {
		union semun sem_union;

		sem_union.val=0;		
		semctl(semid, 0, SETVAL, sem_union);
	}
	
	return semid;
}

IDATA 
SetLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess) 
{
	union semun sem_union;
		
	sem_union.val = nProcess;

	return semctl(semaphore, 0, SETVAL, sem_union);
}

IDATA 
ReleaseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess) 
{
	/* we want to subtract from the launch semaphore so negate the value */
	const IDATA val = ((IDATA) nProcess * (-1));
	struct sembuf buffer;

	buffer.sem_num = 0;
	buffer.sem_op = val;
	buffer.sem_flg = 0;
		
	return semop(semaphore, &buffer, 1);
}

IDATA 
WaitForLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore) 
{
	struct sembuf buffer;

	/* set semop() to wait for zero */
	buffer.sem_num = 0;
	buffer.sem_op = 0;
	buffer.sem_flg = 0;
		
	return semop(semaphore, &buffer, 1);
}

IDATA 
CloseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	union semun sem_union;
	
	semctl(semaphore, 0, IPC_RMID, sem_union);
	
	return 0;
}

#elif defined(WIN32)
IDATA 
openLaunchSemaphore (J9PortLibrary* portLibrary, const char* name, UDATA nProcess)
{
	HANDLE semaphore = CreateSemaphore (NULL, 0, (LONG)nProcess, name);
	if (semaphore == NULL) {
		return -1;
	}
	
	return (IDATA) semaphore;
}

IDATA 
SetLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess) 
{
	IDATA rc = 0;
	UDATA i = 0;
	HANDLE semHandle = (HANDLE) semaphore;
	
	for(i = 0; i < nProcess; i++) {
		if(WaitForSingleObject(semHandle, 0) == WAIT_FAILED) {
			rc = -1;
			break;
		}
	}

	return rc;
}

IDATA
ReleaseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess)
{
	HANDLE semHandle = (HANDLE) semaphore;
	if(ReleaseSemaphore(semHandle, (LONG)nProcess, NULL)) {
		return 0;
	} else {
		return -1;
	}
}

IDATA
WaitForLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore)
{
	HANDLE semHandle = (HANDLE) semaphore;
	if(WaitForSingleObject(semHandle,INFINITE) == WAIT_FAILED) {
		return -1;
	} else {
		return 0;
	}
}

IDATA
CloseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore)
{
	HANDLE semHandle = (HANDLE) semaphore;
	if(CloseHandle(semHandle)) {
		return 0;
	} 
	return -1;
}
#else
/* Not supported on anything else */
IDATA openLaunchSemaphore (J9PortLibrary* portLibrary, const char* name, UDATA nProcess) {
	return -1;
}
IDATA ReleaseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess) {
	return -1;
}
IDATA WaitForLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore) {
	return -1;
}
IDATA CloseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore) {
	return -1;
}

#endif

J9ProcessHandle
launchChildProcess (J9PortLibrary* portLibrary, const char* testname, const char* argv0, const char* options) 
{
	J9ProcessHandle processHandle = NULL;
	char *command[2];
	UDATA commandLength = 2;
	char **env = NULL;
	UDATA envSize = 0;
	char *dir = ".";
	U_32 exeoptions = 0;
	IDATA retVal;
	PORT_ACCESS_FROM_PORT(portLibrary);

	retVal = j9sysinfo_get_executable_name((char*)argv0, &(command[0]));

	if(retVal != 0) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "%s: launchChildProcess: j9sysinfo_get_executable_name failed!\n\tportableErrno = %d\n", testname, portableErrno);
		} else {
			j9tty_printf(portLibrary, "%s: launchChildProcess: j9sysinfo_get_executable_name failed!\n\tportableErrno = %d portableErrMsg = %s\n", testname, portableErrno, errMsg);
		}
		goto done;

	}

	command[1] = (char*)options;

#if defined(PORTTEST_PROCESS_HELPERS_DEBUG)
	/* print our the commandline options for the process being launched
	* and have the child write its to the console by inheriting stdout and stderr
	*/

	{
		int i;
		j9tty_printf(portLibrary, "\n\n");

		for (i =0; i< commandLength; i++) {
			j9tty_printf(portLibrary, "\t\tcommand[%i]: %s\n", i, command[i]);
		}
		j9tty_printf(portLibrary, "\n\n");

		exeoptions = J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR;
	}
#endif 

	retVal = j9process_create((const char **)command, commandLength, env, envSize, dir, exeoptions, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);
	
	if (0 != retVal) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d\n", testname, command[0], command[1],portableErrno);
		} else {
			j9tty_printf(portLibrary, "%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d portableErrMsg = %s\n", testname, command[0], command[1],portableErrno, errMsg);
		}

		processHandle = NULL;

		goto done;
	}
	
	done:
		return processHandle;

}

/*
 * Writes the message to the specified process.
 *
 * Terminates the process if j9process_write returns error
 *
 * @param processHandle		write to the stdin of this process
 * @param message			the message to write
 * @param lenByteMessage	the length in bytes of the message
 *
 * @returns value returned by j9process_write()
 */
IDATA
writeToProcess(J9PortLibrary *portLibrary, char *testName, J9ProcessHandle processHandle, char *message, UDATA lenByteMessage)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = -1;

	rc = j9process_write(processHandle, (void *)message, 9);

	if ( 0 <= rc) {
		outputComment(portLibrary, "j9process_write wrote %d characters.\n", rc);
	} else {
		IDATA terminateRC = -1;

		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_write returned error code %d.\n", rc);
		terminateRC = j9process_terminate(processHandle);
		if (0 != terminateRC) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", terminateRC);
		}
	}

	return rc;
}

/*
 * Polls the process with j9process_isComplete() until the time timeout has elapsed
 *
 * Terminates the process if j9process_isComplete() does not indicate the process has completed before the timeout has elapsed
 *
 * @param processHandle the process
 * @param timeout	poll the process using j9process_isComplete until the timeout has been reached
 *
 * @returns 0 if the process completed successfully
 */
IDATA
pollProcessForComplete(J9PortLibrary *portLibrary, char *testName, J9ProcessHandle processHandle, I_64 timeoutMillis)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = -1;
	I_64 waitTime = 0;
	I_64 sleepMillis = 100;
	IDATA processIsCompleteRC = -1;

	/* wait for child to complete, break after timeout */
	while (waitTime < timeoutMillis) {
		processIsCompleteRC = j9process_isComplete(processHandle);
		if (processIsCompleteRC == 1) {
			rc = 0;
			break;
		} else {
			omrthread_sleep(sleepMillis);
			waitTime += sleepMillis;
		}
	}

	if (1 != processIsCompleteRC) {
		IDATA terminateRC = -1;

		/* The timeout was reached and the child has not yet finished */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child did not finish before timeout\n", rc);
		terminateRC = j9process_terminate(processHandle);
		if (0 != terminateRC) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_terminate returned error code %d.\n", terminateRC);
		}
	}

	return rc;
}

/*
 * Read the contents of stdout of the process
 *
 * @param processHandle 	the process we are reading from
 * @param stream			either of
 * 									J9PORT_PROCESS_STDOUT
 *									J9PORT_PROCESS_STDERR
 * @param buffer			the buffer to read into
 * @param lenBytesBuffer	the length in bytes of the buffer
 *
 * @return -1 if j9process_get_available() reports there are no bytes available, otherwise, the return vale from j9process_read();
 */
IDATA
readFromProcess(J9PortLibrary *portLibrary, char *testName, J9ProcessHandle processHandle, UDATA stream, char *buffer, UDATA lenBytesBuffer)
{

	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA numBytesAvailable = 0;
	IDATA rc = -1;
	
	numBytesAvailable = (I_32)j9process_get_available (processHandle, stream);

	if (lenBytesBuffer < numBytesAvailable) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "(numBytesAvailable = %d) > lenBytesBuffer\n", numBytesAvailable);
	} else if (0 < numBytesAvailable) {

		outputComment(portLibrary, "j9process_get_available reports %d character(s) to read from the process output stream.\n", numBytesAvailable);
		rc = j9process_read(processHandle, stream, buffer, numBytesAvailable);

		if (0 <= rc) {
			buffer[numBytesAvailable] = '\0';
			outputComment(portLibrary, "j9process_read read %d character(s) from the process output stream: \"%s\"\n", rc, buffer);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_read trying to read from the output stream returned error code %d.\n", rc);
		}
	} else if (0 == numBytesAvailable) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available reports that there are no characters to read from the process output stream.\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9process_get_available failed with error code: %d\n", numBytesAvailable);
	}

	return rc;
}

IDATA
waitForTestProcess (J9PortLibrary* portLibrary, J9ProcessHandle processHandle)
{
	IDATA retval = -1;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	if (NULL == processHandle) {
		j9tty_printf(portLibrary, "waitForTestProcess: processHandle == NULL\n");
		goto done;
	}
	
	if (0 != j9process_waitfor(processHandle)) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_waitfor() failed\n\tportableErrno = %d\n" , portableErrno);
		} else {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_waitfor() failed\n\tportableErrno = %d portableErrMsg = %s\n" , portableErrno, errMsg);
		}
		
		goto done;
	}

	retval = j9process_get_exitCode(processHandle);

	if (0 != j9process_close(&processHandle, 0)) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_close() failed\n\tportableErrno = %d\n" , portableErrno);
		} else {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_close() failed\n\tportableErrno = %d portableErrMsg = %s\n" , portableErrno, errMsg);
		}
		goto done;
	}

	done:
	return retval;
}

void
SleepFor(IDATA second)
{
#if defined(WIN32)
	Sleep((DWORD)second*1000);
#elif defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	sleep(second);
#endif
}

