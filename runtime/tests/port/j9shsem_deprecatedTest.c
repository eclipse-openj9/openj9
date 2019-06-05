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

#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "shmem.h"

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../port/sysvipc/j9shsem.h"
#include "../port/sysvipc/j9shsemun.h"
#endif

#define SEMAPHOREA "semaphoreA"
#define SEMAPHOREB "semaphoreB"
#define SEMAPHORECHILD "semaphoreChild"
#define SEMAPHORESETSIZE 10

#define TESTSEM_NAME "Testsem"

#define TESTVALUE 9
#define TEST7_SEMAPHORE_NAME "pltestTest7Sem"
#define TEST7_SEMAPHORE_SIZE SEMAPHORESETSIZE
#define TEST7_NUM_CHILDPROCESSES 30

/* The define FD_BIAS is defined in portpriv.h ... however we can't and shouldn't use this here ... so for now we redefine.
 * In the long run fstat will be implemented in the port librart for test 7 so we don't need this.
 */
#ifdef J9ZOS390
/*CMVC 99667: Introducing FD-biasing to fix MVS started-tasks and POSIX file-descriptors*/
#define FD_BIAS 1000
#else
#define FD_BIAS 0
#endif

#if !(defined(WIN32) || defined(WIN64))
static void
verifyShsemStats(J9PortLibrary *portLibrary, j9shsem_handle *handle, J9PortShsemStatistic *statbuf, const char *testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA uid = j9sysinfo_get_euid();
	UDATA gid = j9sysinfo_get_egid();

	outputComment(PORTLIB, "semid: %zu\n", statbuf->semid);
	if (statbuf->semid != handle->semid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected semid=%zu, semid obtained by sem stats=%zu\n", handle->semid, statbuf->semid);
	}
	outputComment(PORTLIB, "owner uid: %zu\n", statbuf->ouid);
	if (statbuf->ouid != uid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected owner uid=%zu, owner uid obtained by sem stats=%zu\n", uid, statbuf->ouid);
	}
	outputComment(PORTLIB, "creator uid: %zu\n", statbuf->cuid);
	if (statbuf->cuid != uid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected creator uid=%zu, creator uid obtained by sem stats=%zu\n", uid, statbuf->cuid);
	}
	outputComment(PORTLIB, "owner gid: %zu\n", statbuf->ogid);
	if (statbuf->ogid != gid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected ownder gid=%zu, owner gid obtained by sem stats=%zu\n", gid, statbuf->ogid);
	}
	outputComment(PORTLIB, "creator gid: %zu\n", statbuf->cgid);
	if (statbuf->cgid != gid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected creator gid=%zu, creator gid obtained by sem stats=%zu\n", gid, statbuf->cgid);
	}
	outputComment(PORTLIB, "number of semaphores in the set: %zu\n", statbuf->nsems);
	/* j9shsem_deprecated_open() creates one semaphore more than requested. See j9shsem_deprecated.c:createSemaphore(). */
	if (statbuf->nsems != handle->nsems + 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected number of semaphores in the set=%zu, number of semaphores in the set obtained by sem stats=%zu\n", handle->nsems + 1, statbuf->nsems);
	}
}
#endif /* !(defined(WIN32) || defined(WIN64)) */

/* Sanity test, just to see whether create and destroy works */
int
j9shsem_deprecated_test1(J9PortLibrary *portLibrary)
{

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shsem_handle* myhandle;
	IDATA rc;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_deprecated_test1";

	reportTestEntry(portLibrary, testName);
	
	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto exit;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto exit;
	}

	rc = j9shsem_deprecated_open (cacheDir, 0, &myhandle, TESTSEM_NAME, 10, 0, J9SHSEM_NO_FLAGS, NULL);
	
	if(J9PORT_ERROR_SHSEM_OPFAILED == rc || J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open shared semaphore");
		goto exit;
	}
	
	(void)j9shsem_deprecated_destroy (&myhandle);

exit:
	return reportTestExit(portLibrary, testName);
}

int 
j9shsem_deprecated_test2(J9PortLibrary *portLibrary) 
{	
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_deprecated_test2";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	/* 
	 * test check that after deleting with j9shsem_deprecated_destroy 
	 * the next call to j9shsem_deprecated_open() results in J9PORT_INFO_SHSEM_CREATED
	 */
	struct j9shsem_handle* myhandle;
	int rc;
	IDATA fd = -1;
	void* remember = NULL;
	I_64 sizeofFile;
	IDATA fileSize;
	char mybaseFilePath[J9SH_MAXPATH];
	char cacheDir[J9SH_MAXPATH];
	
	reportTestEntry(PORTLIB, testName);
 
	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	rc = j9shsem_deprecated_open(cacheDir, 0, &myhandle, TESTSEM_NAME, 10, 0, J9SHSEM_NO_FLAGS, NULL);
	if(J9PORT_ERROR_SHSEM_OPFAILED == rc || J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot create initial semaphore");
		goto cleanup;
	}
	
	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", cacheDir, TESTSEM_NAME);
	fd = j9file_open(mybaseFilePath, EsOpenRead, 0);

	if(-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file");
		goto cleanup;
	}
	 
	sizeofFile = j9file_length(mybaseFilePath);
	fileSize = sizeof(IDATA) < sizeof(I_64) ? (IDATA)(sizeofFile & 0xFFFFFFFF) : sizeofFile;
	remember = j9mem_allocate_memory(fileSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if(NULL == remember) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot allocate memory\n");
		goto cleanup;
	}
 
	if(j9file_read(fd, remember, fileSize) == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot read base file\n");
		goto cleanup;
	}
	 
	j9file_close(fd);
	j9shsem_deprecated_destroy(&myhandle);
	 
	fd = j9file_open (mybaseFilePath, EsOpenWrite | EsOpenCreate, 0770);
	if(fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file for write\n");
		goto cleanup;
	}
	 
	if(j9file_write(fd, remember, fileSize) == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,"cannot write base file\n");
		goto cleanup;
	}
	j9file_close(fd);
	 
	rc = j9shsem_deprecated_open(cacheDir, 0, &myhandle, TESTSEM_NAME, 10, 0, J9SHSEM_NO_FLAGS, NULL);
	if(rc == J9PORT_INFO_SHSEM_CREATED) {
		/* J9PORT_INFO_SHSEM_CREATED reported, correct behaviour */
	} else if(rc == J9PORT_ERROR_SHSEM_OPFAILED || rc == J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,"Error opening semaphore after reboot!\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Did not report semaphore id invalid! .... ");
	}
	
cleanup:
	
	if(remember != NULL) {
		j9mem_free_memory(remember);
	}
	
	j9file_unlink(mybaseFilePath);
	
	/*j9shsem_destory should be able to clean up the base file even though semaphore is not there */
	rc = j9shsem_deprecated_destroy(&myhandle);
	if(rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_deprecated_destroy wasn't able to clean up! .... ");		
	}	 
	
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms\n", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int
j9shsem_deprecated_test3(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc=0;
	struct j9shsem_handle *sem0 = NULL;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_deprecated_test3";
	
	reportTestEntry(portLibrary, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	rc = j9shsem_deprecated_open(cacheDir, 0, &sem0, SEMAPHOREA, SEMAPHORESETSIZE, 0, J9SHSEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHSEM_OPFAILED == rc || J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_deprecated_open semaphore 0 failed\n");
		goto cleanup;
	}
	
	rc = j9shsem_deprecated_wait(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_deprecated_wait failed with rc=%d\n", rc);
	}

	rc = j9shsem_deprecated_post(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_deprecated_post failed with rc=%d\n", rc);
		goto cleanup;
	}
	
cleanup:
	if(sem0 != NULL) {
		j9shsem_deprecated_destroy(&sem0);
	}
	
	return reportTestExit(portLibrary, testName);
}


#define J9SHSEM_TEST4_CHILDCOUNT 10
#define LAUNCH_CONTROL_SEMAPHORE "j9shsem_deprecated_test4_LAUNCH"

int 
j9shsem_deprecated_test4(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_deprecated_test4";
	
#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	struct j9shsem_handle *childrensemaphore=NULL;
	int i;
	IDATA rc;
	char cacheDir[J9SH_MAXPATH];
	J9ProcessHandle child[J9SHSEM_TEST4_CHILDCOUNT];
	IDATA launchSemaphore=-1;
	int created = 0;
	
	reportTestEntry(portLibrary, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	launchSemaphore = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, J9SHSEM_TEST4_CHILDCOUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto cleanup;
	}

	SetLaunchSemaphore(PORTLIB, launchSemaphore, J9SHSEM_TEST4_CHILDCOUNT);

	for(i = 0; i<J9SHSEM_TEST4_CHILDCOUNT; i++) {
		child[i]=launchChildProcess(PORTLIB, testName, argv0, "-child_j9shsem_deprecated_test4");
		if(NULL == child[i]) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			goto cleanup;
		}	
	}

	WaitForLaunchSemaphore(PORTLIB, launchSemaphore);
	
	for(i=0; i<J9SHSEM_TEST4_CHILDCOUNT; i++) {
		IDATA returnCode = waitForTestProcess(PORTLIB, child[i]);
		switch(returnCode) {
		case 0: /*Semaphore is opened*/
			break;

		case 1: /*Semaphore is created*/
			if(created) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "more then 1 child created the semaphore.\n");
			} else {
				created=1;
			}
			break;

		default: /*Semaphore open call failed*/
			outputErrorMessage(PORTTEST_ERROR_ARGS, "child reported error. rc=%d\n",returnCode);
			break;
		}	
	}

	if(0 == created) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "None of the child reported semaphore creation\n");
	}

cleanup:
	if(launchSemaphore != -1) {
		CloseLaunchSemaphore(PORTLIB, launchSemaphore);
	}
	
	j9shsem_deprecated_open(cacheDir, 0, &childrensemaphore, SEMAPHORECHILD, SEMAPHORESETSIZE, 0, J9SHSEM_NO_FLAGS, NULL);
	j9shsem_deprecated_destroy(&childrensemaphore);
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms\n", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int 
j9shsem_deprecated_test4_child(J9PortLibrary *portLibrary) 
{

	PORT_ACCESS_FROM_PORT(portLibrary);
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_deprecated_test4_child";
	struct j9shsem_handle *childrensemaphore=NULL;
	IDATA launchSemaphore=-1;
	IDATA rc;
	I_32 result = 5;
	
	reportTestEntry(portLibrary, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	launchSemaphore = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, J9SHSEM_TEST4_CHILDCOUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		result=2;
		goto cleanup;
	}
	
	ReleaseLaunchSemaphore(PORTLIB, launchSemaphore, 1);
	rc = j9shsem_deprecated_open(cacheDir, 0, &childrensemaphore, SEMAPHORECHILD, SEMAPHORESETSIZE, 0, J9SHSEM_NO_FLAGS, NULL);

	switch(rc) {
	case J9PORT_INFO_SHSEM_OPENED: 
		result = 0;
		break;
	case J9PORT_INFO_SHSEM_CREATED:
		result = 1;
		break;
	case J9PORT_ERROR_SHSEM_OPFAILED:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening shared semaphore: J9PORT_ERROR_SHSEM_OPFAILED\n");
		result = 3;
		break;
	case J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening shared semaphore: J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT\n");
		result = 4;
		break;
	default:
		result = 5;
	}
	
cleanup:
	
	if(childrensemaphore != NULL) {	
		j9shsem_deprecated_close(&childrensemaphore);
	}
	
	reportTestExit(PORTLIB, testName);
	return result;
}

int 
j9shsem_deprecated_test5(J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_deprecated_test5";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	/* UNIX only
	 * test that our code can handle missing basefile
	 * If someone has deleted the baseFile by mistake, we should be able to recreate/open the old area without problem.
	 * It should report either J9PORT_INFO_SHSEM_OPENED or J9PORT_INFO_SHSEM_CREATED
	 */
	struct j9shsem_handle* myhandle1 = NULL;
	struct j9shsem_handle* myhandle2 = NULL;
	int rc;
	char mybaseFilePath[J9SH_MAXPATH];
	char myNewFilePath[J9SH_MAXPATH];
	char cacheDir[J9SH_MAXPATH];
	int numSems = 10;

	reportTestEntry(PORTLIB, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}


	rc = j9shsem_deprecated_open(cacheDir, 0, &myhandle1, TESTSEM_NAME, numSems, 0, J9SHSEM_NO_FLAGS, NULL);
	if(rc == J9PORT_ERROR_SHSEM_OPFAILED || rc == J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening waiting sempahore");
		goto cleanup;
	}

	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", cacheDir, TESTSEM_NAME);
	j9str_printf(PORTLIB, myNewFilePath, J9SH_MAXPATH, "%s%s_new", cacheDir, TESTSEM_NAME);

	j9file_move(mybaseFilePath, myNewFilePath);

	rc = j9shsem_deprecated_open(cacheDir, 0, &myhandle2, TESTSEM_NAME, numSems, 0, J9SHSEM_NO_FLAGS, NULL);
	j9file_unlink(myNewFilePath);

	switch(rc) {
	case J9PORT_INFO_SHSEM_OPENED:
	case J9PORT_INFO_SHSEM_CREATED:
		j9shsem_deprecated_destroy(&myhandle1);
		j9shsem_deprecated_destroy(&myhandle2);
		break;
	case J9PORT_ERROR_SHSEM_OPFAILED:
	case J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
		outputErrorMessage(PORTTEST_ERROR_ARGS,
				"Error opening semaphore after reboot!\n");
		break;
	default:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unknown return code rc=%d\n",rc);
		break;
	}

	cleanup:
	if(myhandle1 != NULL) {
		j9shsem_deprecated_destroy(&myhandle1);
	}
	if(myhandle2 != NULL) {
		j9shsem_deprecated_destroy(&myhandle2);
	}
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int 
j9shsem_deprecated_test6(J9PortLibrary *portLibrary, const char* argv0)
{	
	PORT_ACCESS_FROM_PORT(portLibrary);

	char *exeName = NULL;
	J9ProcessHandle pid;
	IDATA rc;
	struct j9shsem_handle *sem0 = NULL;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_deprecated_test6";
	
	reportTestEntry(PORTLIB, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	rc = j9shsem_deprecated_open(cacheDir, 0, &sem0, SEMAPHOREA, SEMAPHORESETSIZE, 0, J9SHSEM_NO_FLAGS, NULL);
	if(rc<0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening sempahore");
		goto cleanup;
	}
	
	pid = launchChildProcess(PORTLIB, testName, argv0, "-child_j9shsem_deprecated_test6");
	if(NULL == pid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
		goto cleanup;
	}

	rc = waitForTestProcess(portLibrary, pid);
	
	if(rc!=0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child process exited with status %i: Expected 0.\n", rc);
	}
	
cleanup:
	if(exeName != NULL) {
		j9mem_free_memory(exeName);
	}
	
	if(sem0 != NULL) {
		j9shsem_deprecated_destroy(&sem0);
	}
	
	return reportTestExit(PORTLIB, testName);   
}

int 
j9shsem_deprecated_test6_child(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shsem_handle *sem0=NULL;
	IDATA rc;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_deprecated_test6_child";
	
	reportTestEntry(PORTLIB, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	rc = j9shsem_deprecated_open(cacheDir, 0, &sem0, SEMAPHOREA, SEMAPHORESETSIZE, 0, J9SHSEM_NO_FLAGS, NULL);
	switch(rc) {
	case J9PORT_INFO_SHSEM_OPENED:
		break;
	case J9PORT_INFO_SHSEM_CREATED:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "open semaphore 0 failed: semaphore created!\n");
		goto cleanup;
	case J9PORT_ERROR_SHSEM_OPFAILED:
	case J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "open semaphore 0 failed: cannot open semaphore\n");
		goto cleanup;
	default:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unknown result from j9shsem_deprecated_open rc=%d\n",rc);
		goto cleanup;
	}
		
	rc = j9shsem_deprecated_wait(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_deprecated_wait failed with rc=%d\n", rc);
		goto cleanup;
	}
	
cleanup:

	if(sem0 != NULL) {
		j9shsem_deprecated_close(&sem0);
	}
	
	return reportTestExit(portLibrary, testName);	
}

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
/*Note:
 * This is more than one test. The first test that fails blocks the rest of the test. The test should not fail :-) 
 * This test is meant to exercise the race conditions that can occur when multiple vm's create sysv obj
 * at the same time.
 */
int
j9shsem_deprecated_test7(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=0;
	int i = 0;
	int j = 0;
	struct j9shsem_handle *sem0 = NULL;
	const char* testName = "j9shsem_deprecated_test7";	
	IDATA readfd = 0;
	IDATA writefd = 0;
	j9shsem_baseFileFormat oldfileinfo;
	struct stat statbefore;
	struct stat statafter;
	IDATA launchSemaphore = -1;
	J9ProcessHandle child[TEST7_NUM_CHILDPROCESSES];
	union semun semctlarg;
	struct semid_ds buf;
	int oldsemval;
	char basedir[1024];
	char mybaseFilePath[1024];
	semctlarg.buf = NULL;

	reportTestEntry(portLibrary, testName);	

	if (j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, basedir, 1024) >= 0) {
		j9str_printf(PORTLIB, mybaseFilePath, 1024, "%s%s", basedir, TEST7_SEMAPHORE_NAME);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	

	/* This first call to j9shxxx_open is simply to ensure that we don't use any old SysV objects.*/
	rc = j9shsem_deprecated_open(basedir, 0, &sem0, TEST7_SEMAPHORE_NAME, TEST7_SEMAPHORE_SIZE, 0600, J9SHSEM_NO_FLAGS, NULL);
	if (rc != J9PORT_ERROR_SHSEM_OPFAILED) {
		/* If OPENED, or CREATED, is returned then the semaphore is destroyed  
		 * to ensure the test is starting with a clean slate
		 */
		if (j9shsem_deprecated_destroy(&sem0) == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "A precondition of this test is that we don't use any old SysV objects: %s\n", mybaseFilePath);
			goto cleanup;
		}
	} else {
		/* Remove the control file b/c it may be pointing to something that can't be used.
		 * 		e.g. File exists but semaphore is not accessible.
		 *  
		 * In this case clean up the file, and know the next call to 
		 * j9shsem_deprecated_open() will attempt to create something from scratch.
		 * 
		 * If the next call to j9shsem_deprecated_open() fails it will likely be due to 
		 * system resources being low, or less likely a bug.
		 */
	 	j9file_unlink(mybaseFilePath);
	}

	/*This test starts multiple process we use a semaphore to synchronize the launch*/
	launchSemaphore = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, J9SHSEM_TEST4_CHILDCOUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto cleanup;
	}

	/*Create a new clean semaphore for testing*/
	rc = j9shsem_deprecated_open(basedir, 0, &sem0, TEST7_SEMAPHORE_NAME, TEST7_SEMAPHORE_SIZE, 0600, J9SHSEM_NO_FLAGS, NULL);
	if (rc != J9PORT_INFO_SHSEM_CREATED && rc != J9PORT_INFO_SHSEM_OPENED) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open/create new semaphore: %s\n", mybaseFilePath);
		goto cleanup;
	}
	if (sem0 != NULL) {
		j9shsem_deprecated_close(&sem0);
	}

	/*Read/Backup the control file*/
	if(-1 == (readfd = j9file_open(mybaseFilePath,  EsOpenRead, 0600))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open control file: %s\n", mybaseFilePath);
		goto cleanup;
	}	
	if(j9file_read(readfd, &oldfileinfo, sizeof(j9shsem_baseFileFormat)) != sizeof(j9shsem_baseFileFormat)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot read control file\n");
		goto cleanup;
	}
	if(-1 == j9file_close(readfd)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot close the control file\n");
		goto cleanup;		
	} else {
		readfd = -1;
	}

	outputComment(PORTLIB,  "Test control file, and SysV object, modification recovery!\n");

	/*---Start our tests---*/
	for (i =0; i < 11; i+=1)  {
		/*---Setup our test---*/		
		IDATA tmpfd;
		BOOLEAN shouldUnlink = TRUE;
		BOOLEAN shouldOpenFail = FALSE;
		BOOLEAN doFileWrite = TRUE;
		BOOLEAN doDestroy = FALSE;
		BOOLEAN setFileReadOnly = FALSE;
		j9shsem_baseFileFormat newfileinfo = oldfileinfo;
		IDATA numCreatesReturned = 0;
		IDATA numOpensReturned = 0;
		IDATA numErrorsReturned = 0;

		switch (i) { 
			case 0: {
				outputComment(PORTLIB, "\tSub Test %d: test modified fkey\n", i+1);
				newfileinfo.ftok_key = 0xcafebabe;
				break;
			}
			case 1: {
				outputComment(PORTLIB, "\tSub Test %d: test modified id\n", i+1);
				/*-1 is worst value ... b/c it may slip thru some if statements if i was not careful during dev*/				
				newfileinfo.semid = 0xFFFFFFFF;
				break;
			}
			case 2: {
				outputComment(PORTLIB, "\tSub Test %d: test a size that is too small\n", i+1);
				/*Too small*/				
				newfileinfo.semsetSize = 1;
				break;
			}
			case 3: {
				outputComment(PORTLIB, "\tSub Test %d: test a size that is too large\n", i+1);
				/*Too small*/				
				newfileinfo.semsetSize = 30;
				break;
			}
			case 4: {
				outputComment(PORTLIB, "\tSub Test %d: Set file to use invalid key. Chmod file to be readonly.\n", i+1);
				newfileinfo.ftok_key = 0xcafebabe;
				setFileReadOnly = TRUE;
				shouldUnlink = FALSE;
				shouldOpenFail = TRUE;
				break;
			}
			case 5: {
				outputComment(PORTLIB, "\tSub Test %d: change the  major modlevel\n", i+1);	
				newfileinfo.modlevel = 0x00010001;
				shouldUnlink = FALSE;
				shouldOpenFail = TRUE;
				break;
			}
			case 6: {
				outputComment(PORTLIB, "\tSub Test %d: change the version\n", i+1);	
				newfileinfo.version = 0;
				shouldUnlink = FALSE;
				shouldOpenFail = FALSE;
				break;
			}

			case 7: {
				/*IMPORTANT NOTE: there is a little 'hack' at the bottom of this loop to set the marker back ... otherwise the next test will fail*/
				outputComment(PORTLIB, "\tSub Test %d: change the marker in the semaphore set\n", i+1);
				oldsemval = semctl(newfileinfo.semid, TEST7_SEMAPHORE_SIZE, GETVAL);
				semctlarg.val = 0;
				if (semctl(newfileinfo.semid, TEST7_SEMAPHORE_SIZE, SETVAL, semctlarg) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not modify semaphore marker\n", mybaseFilePath);
					goto cleanup;
				}
				shouldUnlink = TRUE;
				shouldOpenFail = FALSE;
				break;			
			}
			case 8: {
				/*IMPORTANT NOTE: there is a little 'hack' at the bottom of this loop to set the permissions back ... otherwise the next test will fail*/
				outputComment(PORTLIB, "\tSub Test %d: set the SysV IPC obj to be readonly\n", i+1);
				semctlarg.buf = &buf;
				if (semctl(newfileinfo.semid, TEST7_SEMAPHORE_SIZE, IPC_STAT, semctlarg) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not get semaphore permissions\n", mybaseFilePath);
					goto cleanup;
				}				
				semctlarg.buf->sem_perm.mode=0400;
				if (semctl(newfileinfo.semid, TEST7_SEMAPHORE_SIZE, IPC_SET, semctlarg) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not modify semaphore permissions\n", mybaseFilePath);
					goto cleanup;
				}
				shouldUnlink = FALSE;
				shouldOpenFail = TRUE;
				break;			
			}
			/*--- These 2 tests should always be last ... b/c it deletes the sem and control file ---*/
			case 9: {
				outputComment(PORTLIB, "\tSub Test %d: try with no error ... every one should open\n", i+1);	
				shouldUnlink = FALSE;
				shouldOpenFail = FALSE;
				doDestroy = TRUE;
				break;
			}
			case 10: {
				outputComment(PORTLIB, "\tSub Test %d: try with control file ... same as test4 only 1 should create\n", i+1);	
				shouldUnlink = FALSE;
				shouldOpenFail = FALSE;
				doFileWrite= FALSE;
				doDestroy = TRUE;
				break;
			}
			default: {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tto many loops\n");
				goto cleanup;
			}
		}

		if (doFileWrite == TRUE) {
			/*Write new data to the control file*/
			if(-1 == (writefd = j9file_open(mybaseFilePath,  EsOpenWrite | EsOpenRead | EsOpenCreate, 0600))) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot open control file: %s\n", mybaseFilePath);
				goto cleanup;
			}	
			if(j9file_write(writefd, &newfileinfo, sizeof(j9shsem_baseFileFormat)) != sizeof(j9shsem_baseFileFormat)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot write control file\n");
				goto cleanup;
			}
			if(-1 == j9file_close(writefd)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot close the control file\n");
				goto cleanup;		
			} else {
				writefd = -1;
			}
			if (setFileReadOnly == TRUE) {
				if (chmod(mybaseFilePath, S_IRUSR) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot chmod 0400 the control file: %s\n", mybaseFilePath);
					goto cleanup;
				}
			}
			if(-1 == (tmpfd = j9file_open(mybaseFilePath,  EsOpenRead, 0600))) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot open control file: %s\n", mybaseFilePath);
				goto cleanup;
			}
			if (fstat(tmpfd - FD_BIAS, &statbefore) == -1) {
				j9file_close(tmpfd);
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot stat control file before: %s\n", mybaseFilePath);
				goto cleanup;
			}
		}
	
		SetLaunchSemaphore(portLibrary, launchSemaphore, TEST7_NUM_CHILDPROCESSES);

		/*--- Start our test processes ---*/
		for (j = 0; j < TEST7_NUM_CHILDPROCESSES; j+=1) {
			child[j] = launchChildProcess(PORTLIB, testName, argv0, "-child_j9shsem_deprecated_test7");
			if (NULL == child[j]) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildJ9Process failed! Can not perform test!");
				goto cleanup;
			}
		}
	
		WaitForLaunchSemaphore(PORTLIB, launchSemaphore);

		for(j=0; j<TEST7_NUM_CHILDPROCESSES; j+=1) {
			IDATA returnCode = waitForTestProcess(PORTLIB, child[j]);
			switch(returnCode) {
			case J9PORT_INFO_SHSEM_OPENED: /*Semaphore is opened*/	
				numOpensReturned +=1;
				break;
			case  J9PORT_INFO_SHSEM_CREATED: /*Semaphore is created*/
				numCreatesReturned +=1;
				break;
			default: /*Semaphore open call failed*/
				numErrorsReturned+=1;
				break;
			}	
		}
		
		/*--- Done our test processes ---*/
		outputComment(PORTLIB, "\t\tChild Process Exit Code Summary <Created %d, Opened %d, Errors %d>\n", numCreatesReturned, numOpensReturned, numErrorsReturned);	

		if (shouldOpenFail == FALSE) {
			if (shouldUnlink == TRUE || doFileWrite==FALSE) {
				if (numCreatesReturned!=1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of creates should be %d, instead it is %d\n", 1, numCreatesReturned);
					goto cleanup;
				}
				if (numOpensReturned != (TEST7_NUM_CHILDPROCESSES-1)) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of opens should be %d, instead it is %d\n", (TEST7_NUM_CHILDPROCESSES-1), numOpensReturned);
					goto cleanup;
				}
			} else {
				if (numCreatesReturned!=0) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of creates should be %d, instead it is %d\n", 0, numCreatesReturned);
					goto cleanup;
				}
				if (numOpensReturned != TEST7_NUM_CHILDPROCESSES) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of opens should be %d, instead it is %d\n", TEST7_NUM_CHILDPROCESSES, numOpensReturned);
					goto cleanup;
				}
			}
		} else {
				if (numErrorsReturned !=TEST7_NUM_CHILDPROCESSES) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of errors should be %d, instead it is %d\n", TEST7_NUM_CHILDPROCESSES, numErrorsReturned);
					goto cleanup;
				}
		}
			
		if (doFileWrite == TRUE) { 	
			if (fstat(tmpfd - FD_BIAS, &statafter) == -1) {
				j9file_close(tmpfd);
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot stat control file after: %s\n", mybaseFilePath);
				goto cleanup;
			}
			if(j9file_close(tmpfd)==-1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot close control file: %s\n", mybaseFilePath);
				goto cleanup;
			}
			if (shouldUnlink == TRUE) {
				if (statafter.st_nlink != 0) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tOur control file was NOT unlinked!: %s\n", mybaseFilePath);
					goto cleanup;
				}
			} else {
				if (statafter.st_nlink == 0) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tOur control file was unlinked!: %s\n", mybaseFilePath);
					goto cleanup;
				}
			}
		}
		if (doDestroy == TRUE) {
			/*Some clean up for next iteration of the loop*/
			j9shsem_deprecated_open(basedir, 0, &sem0, TEST7_SEMAPHORE_NAME, TEST7_SEMAPHORE_SIZE, 0, J9SHSEM_NO_FLAGS, NULL);
			if(sem0 != NULL) {
				j9shsem_deprecated_destroy(&sem0);
			}
		}

		
		if (i == 4) {
			if (chmod(mybaseFilePath, S_IRUSR | S_IWUSR) == -1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot chmod 0600 the control file: %s\n", mybaseFilePath);
				goto cleanup;
			}
		}
		
		if (i == 7) {
				/*IMPORTANT NOTE: set the semaphore marker back*/
				semctlarg.val = oldsemval;
				if (semctl(newfileinfo.semid, TEST7_SEMAPHORE_SIZE, SETVAL, semctlarg) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not reset semaphore marker\n", mybaseFilePath);
					goto cleanup;
				}
		}
		if (i == 8) {
				/*IMPORTANT NOTE: set the semaphore perm back*/
				if (semctlarg.buf != NULL) {
					semctlarg.buf->sem_perm.mode=0600;
					if (semctl(newfileinfo.semid, TEST7_SEMAPHORE_SIZE, IPC_SET, semctlarg) == -1) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not reset semaphore permissions\n", mybaseFilePath);
						goto cleanup;
					}
				}
		}

	}
	/*---End our test---*/


cleanup:
	if (i == 4) {
		if (chmod(mybaseFilePath, S_IRUSR | S_IWUSR) == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot chmod 0600 the control file: %s\n", mybaseFilePath);
		}
	}

	if (i == 7) {
		/*IMPORTANT NOTE: set the semaphore marker back*/
		semctlarg.val = oldsemval;
		if (semctl(oldfileinfo.semid, TEST7_SEMAPHORE_SIZE, SETVAL, semctlarg) == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not recover/clean semaphore marker\n", mybaseFilePath);
		}
	}
	if (i == 8) {
		/*IMPORTANT NOTE: set the semaphore perm back so we can cleanup*/
		if (semctlarg.buf != NULL) {
			semctlarg.buf->sem_perm.mode=0600;
			if (semctl(oldfileinfo.semid, TEST7_SEMAPHORE_SIZE, IPC_SET, semctlarg) == -1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not recover/clean semaphore permissions\n", mybaseFilePath);
			}
		}
	}
	if(launchSemaphore != -1) {
		CloseLaunchSemaphore(PORTLIB, launchSemaphore);
	}
	if(readfd != -1) {
		j9file_close(readfd);
	}
	if(writefd != -1) {
		j9file_close(writefd);
	}
	/*Set the control file back*/
	if(-1 == (writefd = j9file_open(mybaseFilePath,  EsOpenWrite | EsOpenRead | EsOpenCreate, 0600))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open control file: %s\n", mybaseFilePath);
	}		
	if(j9file_write(writefd, &oldfileinfo, sizeof(j9shsem_baseFileFormat)) != sizeof(j9shsem_baseFileFormat)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot reset the control file\n");
	}
	if(-1 == j9file_close(writefd)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot close the control file\n");
	}
	
	/*Some tests intentionally fail open ... so sem0 may be NULL ... we call an unchecked open just to set sem0 if it can be done*/
	j9shsem_deprecated_open(basedir, 0, &sem0, TEST7_SEMAPHORE_NAME, TEST7_SEMAPHORE_SIZE, 0, J9SHSEM_NO_FLAGS, NULL);
	if(sem0 != NULL) {
		j9shsem_deprecated_destroy(&sem0);
	}
	j9file_unlink(mybaseFilePath);
	return reportTestExit(PORTLIB, testName);
}

int 
j9shsem_deprecated_test7_testchild(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_deprecated_test7_testchild";
	struct j9shsem_handle *childrensemaphore=NULL;
	IDATA launchSemaphore=-1;
	int rc;
	
	rc = J9PORT_ERROR_SHSEM_OPFAILED;

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}
	
	launchSemaphore = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, J9SHSEM_TEST4_CHILDCOUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto cleanup;
	}

	ReleaseLaunchSemaphore(PORTLIB, launchSemaphore, 1);

	rc = j9shsem_deprecated_open(cacheDir, 0, &childrensemaphore, TEST7_SEMAPHORE_NAME, TEST7_SEMAPHORE_SIZE, 0, J9SHSEM_NO_FLAGS, NULL);
	
cleanup:
	
	if(childrensemaphore != NULL) {	
		j9shsem_deprecated_close(&childrensemaphore);
	}

	return rc;
}
#endif

#if !(defined(WIN32) || defined(WIN64))
int
j9shsem_deprecated_test8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_deprecated_test8";
	IDATA rc;
	struct j9shsem_handle* handle = NULL;
	struct J9PortShsemStatistic statbuf;
	char cacheDir[J9SH_MAXPATH];

	reportTestEntry(portLibrary, testName);

	rc = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto cleanup;
	}
	rc = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto cleanup;
	}

	rc = j9shsem_deprecated_open (cacheDir, 0, &handle, testName, 10, 0, J9SHSEM_NO_FLAGS, NULL);

	if((J9PORT_ERROR_SHSEM_OPFAILED == rc) || (J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open shared semaphore");
		goto cleanup;
	}

	rc = j9shsem_deprecated_handle_stat(handle, &statbuf);
	if( -1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_deprecated_test8 j9shsem_deprecated_handle_stat call failed \n");
		goto cleanup;
	}

	verifyShsemStats(portLibrary, handle, &statbuf, testName);

cleanup:
	if(NULL != handle) {
		j9shsem_deprecated_destroy(&handle);
	}
	return reportTestExit(portLibrary, testName);
}
#endif /* !(defined(WIN32) || defined(WIN64)) */

int 
j9shsem_deprecated_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* shsem_child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=TEST_PASS;
	IDATA rc2;
	char basedir[J9SH_MAXPATH];
	
	/*return j9shsem_deprecated_test7_testchild(portLibrary);*/
	
	if(shsem_child != NULL) {
		if(strcmp(shsem_child,"j9shsem_deprecated_test4") == 0) {
			return j9shsem_deprecated_test4_child(portLibrary);
		} else if(strcmp(shsem_child, "j9shsem_deprecated_test6") == 0) {
			return j9shsem_deprecated_test6_child(portLibrary);
		}
#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
		else if(strcmp(shsem_child,"j9shsem_deprecated_test7") == 0) {
			return j9shsem_deprecated_test7_testchild(portLibrary);
		}
#endif
	}
	
	HEADING(PORTLIB,"Deprecated Shared Semaphore Test");

	/* Ensure that any Testsem files from failing tests are deleted prior to starting */
	rc2 = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, basedir, J9SH_MAXPATH);
	if (rc2 < 0) {
		j9tty_printf(PORTLIB, "Cannot get a directory\n");
		return -1;
	}
	rc2 = j9shmem_createDir(basedir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc2 == -1) {
		j9tty_printf(PORTLIB, "Cannot create the directory\n");
		return -1;
	}
	if (rc2 >= 0) {
		deleteControlDirectory(PORTLIB, basedir);
	}

	rc = j9shsem_deprecated_test1(portLibrary);	
	rc |= j9shsem_deprecated_test2(portLibrary);
	rc |= j9shsem_deprecated_test3(portLibrary);
	rc |= j9shsem_deprecated_test4(portLibrary, argv0);	
	rc |= j9shsem_deprecated_test5(portLibrary);
	rc |= j9shsem_deprecated_test6(portLibrary, argv0);	
#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	rc |= j9shsem_deprecated_test7(portLibrary, argv0);	
#endif
#if !(defined(WIN32) || defined(WIN64))
	rc |= j9shsem_deprecated_test8(portLibrary);
#endif /* !(defined(WIN32) || defined(WIN64)) */
	
	j9tty_printf(PORTLIB, "\nDeprecated Shared Semaphore test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

