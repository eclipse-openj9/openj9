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

#include "ProcessHelper.h"
#include "j9port.h"
#include "j9memcategories.h"

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#define PROCESS_HELPER_SEM_KEY 0x05CAC8E /* Reads OSCACHE */
#endif

#if defined(J9ZOS390)
#include <spawn.h>
#include <stdlib.h> /* for malloc for atoe */
#include <string.h>
extern "C" {
#include "atoe.h"
}
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

typedef struct ProcessHelperSemaphoreHandle {
	int semid;
	U_8 creator;
} ProcessHelperSemaphoreHandle;

IDATA 
openLaunchSemaphore (J9PortLibrary* portLibrary, const char* name, UDATA nProcess) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	ProcessHelperSemaphoreHandle* handle;

	handle = (ProcessHelperSemaphoreHandle*) j9mem_allocate_memory(sizeof(ProcessHelperSemaphoreHandle), J9MEM_CATEGORY_CLASSES);
	if(handle == NULL) {
		return -1;
	}
	
	handle->semid = semget(PROCESS_HELPER_SEM_KEY, 1, 0660 | IPC_CREAT | IPC_EXCL);

	if(-1 == handle->semid) {
		if(errno == EEXIST) {
			handle->semid = semget(PROCESS_HELPER_SEM_KEY, 1, 0660);
			handle->creator = 0;
		}
		if(handle->semid == -1) {
			j9mem_free_memory(handle);
			return -1;
		}
	} else {
		union semun sem_union;

		sem_union.val=0;		
		semctl(handle->semid, 0, SETVAL, sem_union);

		handle -> creator = 1;
	}
	
	return (IDATA) handle;	
}

IDATA 
ReleaseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess) 
{
	ProcessHelperSemaphoreHandle* handle = (ProcessHelperSemaphoreHandle*) semaphore;

	struct sembuf buffer;

	buffer.sem_num = 0;
	buffer.sem_op = nProcess;
	buffer.sem_flg = SEM_UNDO;
		
	return semop(handle->semid, &buffer, 1);
}

IDATA 
WaitForLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore) 
{
	ProcessHelperSemaphoreHandle* handle = (ProcessHelperSemaphoreHandle*) semaphore;

	struct sembuf buffer;

	buffer.sem_num = 0;
	buffer.sem_op = -1;
	buffer.sem_flg = SEM_UNDO;
		
	return semop(handle->semid, &buffer, 1);
}

IDATA 
CloseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	ProcessHelperSemaphoreHandle* handle = (ProcessHelperSemaphoreHandle*) semaphore;	
	
	if(handle->creator) {
		semctl(handle->semid, 0, IPC_RMID);
	}

	j9mem_free_memory(handle);
	
	return 0;
}

#endif /* defined(LINUX) | defined(J9ZOS390) | defined(AIXPPC) | defined(OSX)  */


#if defined(WIN32)
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
#endif

#if defined(J9ZOS390)
extern "C" char ** environ;
#endif


/**
 * Launches a child process
 * @param portLibrary
 * @param testname name of test
 * @param argv0 exe name
 * @param options options for the exe e.g. -child
 * @returns return code of process
 */
J9ProcessHandle
LaunchChildProcess (J9PortLibrary* portLibrary, const char* testname, char * newargv[SHRTEST_MAX_CMD_OPTS], UDATA newargc)
{
	J9ProcessHandle processHandle = NULL;
	char **env = NULL;
	UDATA envSize = 0;
	const char *dir = ".";
	U_32 exeoptions = (J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR);
	IDATA retVal;
	PORT_ACCESS_FROM_PORT(portLibrary);

	retVal = j9sysinfo_get_executable_name(newargv[0], &(newargv[0]));

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


	retVal = j9process_create((const char **)newargv, newargc, env, envSize, dir, exeoptions, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);

	if (0 != retVal) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d\n", testname, newargv[0], newargv[1],portableErrno);
		} else {
			j9tty_printf(portLibrary, "%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d portableErrMsg = %s\n", testname, newargv[0], newargv[1],portableErrno, errMsg);
		}

		goto done;
	}

	done:
		return processHandle;

}

/**
 * Wait on a 'processHandle'
 * @param portLibrary
 * @param processHandle
 * @returns return code of process
 */
IDATA
WaitForTestProcess (J9PortLibrary* portLibrary, J9ProcessHandle processHandle)
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
	Sleep((DWORD)(second*1000));
#elif defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	sleep(second);
#endif
}
