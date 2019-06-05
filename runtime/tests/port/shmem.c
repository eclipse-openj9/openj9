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
 * $RCSfile: shmem.c,v $
 * $Revision: 1.86 $
 * $Date: 2012-11-23 21:11:32 $
 */

#if defined(WIN32)
#include <Windows.h>
#endif

#include "testHelpers.h"
#include "j9port.h"
#include "omrmemcategories.h"

#include "shmem.h"
#include "testProcessHelpers.h"
#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../port/sysvipc/j9shmem.h"
#else /* defined(LINUX) || defined (J9ZOS390) || defined (AIXPPC) || defined(OSX) */
#include "../port/win32_include/j9shmem.h"
#endif /* defined(LINUX) || defined (J9ZOS390) || defined (AIXPPC) || defined(OSX) */

#define SHAREDMEMORYA "sharedmemoryA"
#define SHAREDMEMORYB "sharedmemoryB"
#define TEST7_SHAREDMEMORYA "C240D1A32_memory_a_G03"
#define TEST7_SHAREDMEMORYB "C240D1A32_memory_b_G03"
#define TEST7_SHAREDMEMORYC "C240D1A32_memory_c_G03"
#define TEST7_SHAREDMEMORYD "C240D1A32_memory_memory_G03"
#define TEST8_SHAREDMEMORY "j9shmem_test8"
#define SEMAPHORE_NAME "testSemaphore"
#define TESTSTRING "CAFEBABE"

#define TESTMEM_NAME "Testmem"

#define SHMSIZE (1024*1024)

#define TEST15_LAUNCH_SEM_NAME "pltestTest15LaunchSem"
#define TEST15_MEM_NAME "pltestTest15Mem"
#define TEST15_MEM_SIZE SHMSIZE
#define TEST15_NUM_CHILDPROCESSES 30

#define TEST17_SHAREDMEMORY "j9shmem_test17"

/* The define FD_BIAS is defined in portpriv.h ... however we can't and shouldn't use this here ... so for now we redefine.
 * In the long run fstat will be implemented in the port librart for test 7 so we don't need this.
 */
#ifdef J9ZOS390
/*CMVC 99667: Introducing FD-biasing to fix MVS started-tasks and POSIX file-descriptors*/
#define FD_BIAS 1000
#else
#define FD_BIAS 0
#endif

typedef struct simpleHandlerInfo {
	const char* testName;
} simpleHandlerInfo;

typedef struct protectedWriteInfo {
	const char* testName;
	char *writeAddress;
    UDATA shouldCrash;
	UDATA length;
} protectedWriteInfo;

static UDATA
simpleHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	outputComment(portLibrary, " A crash occured, signal handler invoked (type = 0x%x)\n", gpType);
	return J9PORT_SIG_EXCEPTION_RETURN;
}


static UDATA
protectedWriter(J9PortLibrary* portLibrary, void* arg)
{
	struct protectedWriteInfo *info = (protectedWriteInfo *)arg;
	UDATA i;

	const char* testName = info->testName;
	char * regionChar = info->writeAddress;

	for(i=0; i<info->length; i++) {
		regionChar[i] = (char)((i%26)+'a');
		/* aChar = regionCharA[i]; */
	}

 /* If we expected to crash but did not, it is an error! */
 if (info->shouldCrash!=0) {
  /* we should have crashed in the above call, so if we got here we didn't have protection */
  outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpectedly continued execution");
 } else {
  /* should not crash */
  return 0;
 }
	return ~J9PORT_SIG_EXCEPTION_OCCURRED;
}

static void
verifyShmemStats(J9PortLibrary *portLibrary, struct j9shmem_handle *handle, struct J9PortShmemStatistic *statbuf, const char *testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA uid = j9sysinfo_get_euid();
	UDATA gid = j9sysinfo_get_egid();

#if !(defined(WIN32) || defined(WIN64))
	outputComment(PORTLIB, "shmid: %zu\n", statbuf->shmid);
	if (statbuf->shmid != handle->shmid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected shmid=%zu, shmid obtained by shm stats=%zu\n", handle->shmid, statbuf->shmid);
	}
	outputComment(PORTLIB, "owner uid: %zu\n", statbuf->ouid);
	if (statbuf->ouid != uid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected owner uid=%zu, owner uid obtained by shm stats=%zu\n", uid, statbuf->ouid);
	}
	outputComment(PORTLIB, "creator uid: %zu\n", statbuf->cuid);
	if (statbuf->cuid != uid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected creator uid=%zu, creator uid obtained by shm stats=%zu\n", uid, statbuf->cuid);
	}
	outputComment(PORTLIB, "owner gid: %zu\n", statbuf->ogid);
	if (statbuf->ogid != gid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected ownder gid=%zu, owner gid obtained by shm stats=%zu\n", gid, statbuf->ogid);
	}
	outputComment(PORTLIB, "creator gid: %zu\n", statbuf->cgid);
	if (statbuf->cgid != gid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected creator gid=%zu, creator gid obtained by shm stats=%zu\n", gid, statbuf->cgid);
	}
#endif /* !(defined(WIN32) || defined(WIN64)) */
	outputComment(PORTLIB, "size: %zu\n", statbuf->size);
	if (statbuf->size != handle->size) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected size of shared memory=%zu, size obtained by shm stats=%zu\n", handle->size, statbuf->size);
	}
}

/** Attempt to open shared memory */
int
j9shmem_test1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle* myhandle;
	IDATA rc;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test1";

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

	rc = j9shmem_open(cacheDir, 0, &myhandle, TESTMEM_NAME, 10, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if(rc==J9PORT_ERROR_SHMEM_OPFAILED) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open shared memory region");
		goto exit;
	}

	(void)j9shmem_destroy(cacheDir, 0, &myhandle);

exit:
	return reportTestExit(portLibrary, testName);
}

/** Open shared memory, attach to it and write to it */
int
j9shmem_test2(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle *shm0=NULL;
	struct j9shmem_handle *shm1=NULL;
	IDATA rc=0, i;
	void *regionA, *regionB;
	char *regionCharA, *regionCharB;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test2";

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

	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

	rc = j9shmem_open(cacheDir, 0, &shm1, SHAREDMEMORYB, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Constructor of shared memory 1 failed\n");
		goto exit;
	}

	regionA = j9shmem_attach(shm0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionA) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 0 failed\n");
		goto exit;
	}

	regionB = j9shmem_attach(shm1, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionB) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 1 failed\n");
		goto exit;
	}

	regionCharA = (char*) regionA;
	regionCharB = (char*) regionB;
	/*
	 * if SIGSEGV/SIGBUS here then something wrong with shared memory
	 * returned by attach/detach
	 */
	outputComment(PORTLIB, "If SIGSEGV here then shared memory is not allocated properly\n");
	for(i=0; i<SHMSIZE; i++) {
		regionCharA[i] = (char)((i%26)+'a');
		regionCharB[i] = (char)((i%26)+'A');
	}

exit:

	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);
	}

	if(NULL != shm1) {
		j9shmem_destroy(cacheDir, 0, &shm1);
	}

	return reportTestExit(portLibrary, testName);
}

/** open then close then reopen some shared memory, check we can write to it */
int
j9shmem_test3(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle* myhandle = NULL;
	IDATA rc,i;
	char* memoryArea;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test3";

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

	rc = j9shmem_open (cacheDir, 0, &myhandle, TESTMEM_NAME, 1024, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if(J9PORT_ERROR_SHMEM_OPFAILED == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open shared memory region");
		goto exit;
	}

	j9shmem_close(&myhandle);

	rc = j9shmem_open (cacheDir, 0, &myhandle, TESTMEM_NAME, 200, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if(J9PORT_ERROR_SHMEM_OPFAILED == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot reopen shared memory region");
		goto exit;
	}

	memoryArea = j9shmem_attach(myhandle, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == memoryArea) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot attach to shared memory region");
		goto exit;
	}

	outputComment(PORTLIB, "If SIGSEGV here then shared memory is not reopened properly\n");
	for(i=0; i<250; i++) {
		memoryArea[i]='A';
	}

exit:
	if(myhandle != NULL) {
		j9shmem_destroy(cacheDir, 0, &myhandle);
	}
	return reportTestExit(portLibrary, testName);
}

int
j9shmem_test4(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	char *exeName;
	J9ProcessHandle pid = 0;
	IDATA termstat;
	IDATA rc;
	void* region;
	char* regionChar;
	struct j9shmem_handle* mem0 = NULL;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test4";

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

	/* parent */
	rc = j9sysinfo_get_executable_name(argv0, &exeName);
	if(rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name failed!\n");
		goto exit;
	}

	rc = j9shmem_open(cacheDir, 0, &mem0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if( J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error opening shared memory region!\n");
	}

	pid = launchChildProcess ( PORTLIB, testName, exeName, "-child_j9shmem_test4");
	if (pid == NULL) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess failed! Can not perform test!\n");
		goto exit;
	}

	region = j9shmem_attach(mem0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == region) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_attach failed\n");
		goto exit;
	}

	regionChar = (char*) region;
	j9str_printf(PORTLIB, regionChar, 30, TESTSTRING);
	termstat = waitForTestProcess(portLibrary, pid);

	if(termstat != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child process exited with status %i: Expected 0.\n", termstat);
	}

exit:
	if(mem0 != NULL) {
		j9shmem_destroy(cacheDir, 0, &mem0);
	}

	return reportTestExit(portLibrary, testName);;
}

int
j9shmem_test4_child(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc;
	struct j9shmem_handle* mem0 = NULL;
	void* region;
	char* regionChar;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test4_child";

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

	rc = j9shmem_open(cacheDir, 0, &mem0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if (rc == J9PORT_INFO_SHMEM_CREATED) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "shared memory created "
									"- but it should have been there!\n");
		goto exit;
	} else if(rc == J9PORT_ERROR_SHMEM_OPFAILED || rc == J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "open child shareded memory fails:\n");
		goto exit;
	}

	region = j9shmem_attach(mem0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == region) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_attach failed with rc=%d\n", rc);
		goto exit;
	}

	SleepFor(2);
	regionChar = (char*) region;
	if(strcmp(regionChar,TESTSTRING)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "access shared memory region error! contents =\n");
		goto exit;
	}
exit:
	if(mem0 != NULL) {
		j9shmem_close(&mem0);
	}
	return reportTestExit(portLibrary,testName);
}



/* Sanity test, just to see whether create and destroy works */



int j9shmem_test5(J9PortLibrary *portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shmem_test5";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	char cacheDir[J9SH_MAXPATH];
/*
 * test open after reboot
 * After system reboot, the baseFile is still there, but the information is invalid.
 * To simulate this, we are going to create a shared memory area, make a copy of the
 * baseFile then destroy the shared memory area.
 * Finally we will try to do an open using the same name and see what happens.
 */

	struct j9shmem_handle *myhandle = NULL;
	int rc;
	IDATA rc2;
	IDATA fd=-1;
	void* remember = NULL;
	I_64 sizeofFile;
	IDATA fileSize;
	void* region;
	char mybaseFilePath[J9SH_MAXPATH];

	reportTestEntry(portLibrary, testName);

	rc2 = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc2 < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto exit;
	}
	rc2 = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc2 == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto exit;
	}

	rc = j9shmem_open (cacheDir, 0, &myhandle, TESTMEM_NAME, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if(J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open shared memory area");
		goto exit;
	}

	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", cacheDir, TESTMEM_NAME);
	rc = j9file_attr(mybaseFilePath);

	if(rc != EsIsFile) {
		j9tty_printf(PORTLIB, "No base file created - probably POSIX rt implementation - test does not apply\n");
		goto exit;
	}

	fd = j9file_open(mybaseFilePath, EsOpenRead, 0);
	if(-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file\n");
		goto exit;
	}

	sizeofFile = j9file_length(mybaseFilePath);
	fileSize = sizeof(IDATA) < sizeof(I_64) ? (IDATA)(sizeofFile & 0xFFFFFFFF) : sizeofFile;
	remember = j9mem_allocate_memory(fileSize, OMRMEM_CATEGORY_PORT_LIBRARY);

	if(j9file_read(fd, remember, fileSize) == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot read base file");
		goto exit;
	}

	j9file_close(fd);
	j9shmem_destroy(cacheDir, 0, &myhandle);

	fd = j9file_open (mybaseFilePath, EsOpenWrite | EsOpenCreate, 0770);
	if(-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file for write");
		goto exit;
	}

	if(j9file_write(fd, remember, fileSize) == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot write base file");
		goto exit;
	}

	j9file_close(fd);

	/* j9shmem_open is different from j9shsem_open in that if it found the
	 * basefile is invalid, it will recover by recreating the shared memory area
	 * and it should return J9PORT_INFO_SHMEM_CREATED
	 */
	rc = j9shmem_open(cacheDir, 0, &myhandle, TESTMEM_NAME, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if(rc != J9PORT_INFO_SHMEM_CREATED) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening shared memory after reboot - a new region not created");
		if(rc == J9PORT_INFO_SHMEM_OPENED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tInfo: We opened an existing 'shared memory' instead of creating a new one.");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tInfo: rc = %d.",rc);
		}
		goto exit;
	}

	region = j9shmem_attach(myhandle, OMRMEM_CATEGORY_PORT_LIBRARY);
	if( NULL == region) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error attach shared memory after reboot!");
		goto exit;
	}

exit:
	if(fd != -1) {
		j9file_close(fd);
	}

	if(myhandle != NULL) {
		j9shmem_destroy(cacheDir, 0, &myhandle);
	}

	if(remember != NULL) {
		j9mem_free_memory(remember);
	}

#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms\n", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

#if 0
/*
 * This test is no longer useful since 132894 was fixed. And it is leaving sys v 
 * artifacts around so I am removing it.
 */

int
j9shmem_test6(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test6";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	/* UNIX only
	 * test that our code can handle missing basefile
	 * If someone has deleted the baseFile by mistake, we should be able to recreate/open the old area without problem.
	 * To simulate this, we are going to create a semaphore, remember the contents of the base file, then unlink it.
	 * Finally We will try to do an open and see what happens.
	 * It should report either J9PORT_INFO_SHMEM_OPENED or J9PORT_INFO_SHMEM_CREATED
	 */

	struct j9shmem_handle* myhandle = NULL;
	int rc, fd;
	IDATA rc2;
	void* remember = NULL;
	I_64 sizeofFile;
	IDATA fileSize;
	char mybaseFilePath[J9SH_MAXPATH];

	reportTestEntry(PORTLIB, testName);

	rc2 = j9shmem_getDir(NULL, J9SHMEM_GETDIR_APPEND_BASEDIR, cacheDir, J9SH_MAXPATH);
	if (rc2 < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot get a directory");
		goto exit;
	}
	rc2 = j9shmem_createDir(cacheDir, J9SH_DIRPERM_ABSENT, TRUE);
	if (rc2 == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot create the directory");
		goto exit;
	}

	rc = j9shmem_open(cacheDir, 0, &myhandle, TESTMEM_NAME , 10, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if(J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file");
		goto exit;
	}

	j9shmem_close(&myhandle);

	/* WARNING: The following line need to be sync with j9shmem_open in j9shmem.c -
	 * maybe we should have a private interface so that porttest can get the
	 * basefile name
	 */
	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", cacheDir, TESTMEM_NAME);
	rc = j9file_attr(mybaseFilePath);

	if(rc != EsIsFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,"No base file created - probably POSIX rt implementation - test does not apply\n");
		j9shmem_destroy(cacheDir, 0, &myhandle);
		return 0;
	}

	fd = j9file_open(mybaseFilePath, EsOpenRead, 0);
	if(fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "canot open base file");
		goto exit;
	}

	sizeofFile = j9file_length(mybaseFilePath);
	fileSize = sizeof(IDATA) < sizeof(I_64) ? (IDATA)(sizeofFile & 0xFFFFFFFF) : sizeofFile;
	remember = j9mem_allocate_memory(fileSize, OMRMEM_CATEGORY_PORT_LIBRARY);

	if(remember == NULL) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot allocate memory");
		goto exit;
	}

	if(j9file_read(fd, remember, fileSize) == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot read base file\n");
		goto exit;
	}

	j9file_close(fd);
	j9file_unlink(mybaseFilePath);

	rc = j9shmem_open(cacheDir, 0, &myhandle, TESTMEM_NAME , 10, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if(rc == J9PORT_INFO_SHMEM_OPENED || rc == J9PORT_INFO_SHMEM_CREATED) {
		rc=0;
	} else if(rc == J9PORT_ERROR_SHMEM_OPFAILED || rc == J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,"Error opening semaphore after reboot!\n");
		rc = -1;
	} else {
		rc = 0;
	}

	j9shmem_destroy(cacheDir, 0, &myhandle);

	/* Now recreate the baseFile, open it, and then destroy it */
	fd = j9file_open(mybaseFilePath, EsOpenWrite | EsOpenCreate, 0770);
	if(fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file for recreation");
		goto exit;
	}

	if(j9file_write(fd, remember, fileSize) == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot write base file\n");
		goto exit;
	}

	rc = j9shmem_open(cacheDir, 0, &myhandle, TESTMEM_NAME, 10, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if(J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open base file");
		goto exit;
	}

exit:
	/*finish with base file contents now*/
	if(remember != NULL) {
		j9mem_free_memory(remember);
	}

	if(myhandle != NULL) {
		j9shmem_destroy(cacheDir, 0, &myhandle);
	}
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms\n", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}
#endif

/**
 * Verify @ref j9shmem.c::j9shmem_findfirst "j9shmem_findfirst",
 * @ref j9shmem.c::j9shmem_findnext "j9shmem_findnext" and @ref j9shmem.c::j9shmem_findclose "j9shmem_findclse"
 * functions.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9shmem_test7(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA findHandle=0;
	char result[100];
	int i;
	IDATA rc;
	/* This will work for both Windows and Unix */
	const char *name[]={TEST7_SHAREDMEMORYA, TEST7_SHAREDMEMORYB, TEST7_SHAREDMEMORYC, TEST7_SHAREDMEMORYD};
#define J9SHMEM_TEST7_NTEST 4
	int checked[J9SHMEM_TEST7_NTEST];
	struct j9shmem_handle* handles[J9SHMEM_TEST7_NTEST];
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test7";

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

	/* Make sure all the handles are NULL'd in case the next loop fails early. Otherwise
	 * j9shmem_destroy() (under the 'cleanup' label) may crash deleting garbage.
	 */
	memset(handles,0,sizeof(handles));

	for(i=0; i < J9SHMEM_TEST7_NTEST; i++){
		checked[i]=0;
		rc = j9shmem_open(cacheDir, 0, &handles[i], name[i], 1024, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
		if(J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test7 opening shmem area failed for name '%s' and cacheDir '%s'\n",name[i],cacheDir);
			goto cleanup;
		}
	}

	findHandle = j9shmem_findfirst(cacheDir, result);

	if(findHandle == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test7 findfirst return -1\n");
		goto cleanup;
	}

	do {
		for(i=0; i<J9SHMEM_TEST7_NTEST; i++) {
			if(strcmp(result, name[i])==0) {
				if(checked[i]) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test7 findfirst/findnext return repeated entry");
					goto cleanup;
				} else {
					checked[i]=1;
					break;
				}
			}
		}
	} while(j9shmem_findnext(findHandle, result) != -1);

	for(i=0; i<3; i++) {
		if(checked[i] == 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error: j9shmem_test7 findfirst/findnext missing entry: %s\n",name[i]);
			goto cleanup;
		}
	}

cleanup:
	if(findHandle > 0) {
		j9shmem_findclose(findHandle);
	}
	for(i=0; i<J9SHMEM_TEST7_NTEST; i++) {
		j9shmem_destroy(cacheDir, 0, &handles[i]);
	}
	return reportTestExit(portLibrary,testName);
}

/**
 * Verify @ref j9shmem.c::j9shmem_stat "j9shmem_stat" function.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9shmem_test8(J9PortLibrary *portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = TEST8_SHAREDMEMORY;
	IDATA rc;
	struct j9shmem_handle* handle = NULL;
	struct J9PortShmemStatistic statbuf;
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

	/* z/OS rounds up the size to 1M boundary, so create a 1M shared memory region to avoid platform specific test condition when verifying the size */
	rc = j9shmem_open(cacheDir, 0, &handle, testName, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if(J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test8 opening shmem area failed \n");
		goto cleanup;
	}

	rc = j9shmem_stat(cacheDir, 0, testName, &statbuf);
	if( -1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test8 j9shmem_stat call failed \n");
		goto cleanup;
	}

	verifyShmemStats(portLibrary, handle, &statbuf, testName);

cleanup:
	if(NULL != handle) {
		j9shmem_destroy(cacheDir, 0, &handle);
	}
	return reportTestExit(portLibrary,testName);

}
#define J9SHMEM_TEST9_SHMEM_NAME "j9shmem_test9_shmem_area"
#define J9SHMEM_TEST9_LAUNCH_CONTROL_NAME "j9shmem_test9_launch_control"
#define J9SHMEM_TEST9_CHILD_COUNT 10
int
j9shmem_test9 (J9PortLibrary *portLibrary, const char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test9";
	struct j9shmem_handle* childrenmemory = NULL;
	IDATA launchSemaphore = -1;
	IDATA rc;
	int i;
	J9ProcessHandle child[J9SHMEM_TEST9_CHILD_COUNT];
	int created = 0;

	reportTestEntry(PORTLIB, testName);

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

	/* first clean up any existing control file */	
	j9shmem_open(cacheDir, 0, &childrenmemory, J9SHMEM_TEST9_SHMEM_NAME, 10240, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	j9shmem_destroy(cacheDir, 0, &childrenmemory);

	launchSemaphore = openLaunchSemaphore(PORTLIB, J9SHMEM_TEST9_LAUNCH_CONTROL_NAME, J9SHMEM_TEST9_CHILD_COUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto exit;
	}
	
	SetLaunchSemaphore (PORTLIB, launchSemaphore, J9SHMEM_TEST9_CHILD_COUNT);
	
	for(i = 0; i<J9SHMEM_TEST9_CHILD_COUNT; i++) {
		child[i]=launchChildProcess(PORTLIB, testName, argv0, "-child_j9shmem_test9");
		if(child[i] == NULL) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			goto exit;
		}
	}

	WaitForLaunchSemaphore(PORTLIB, launchSemaphore);

	for(i=0; i<J9SHMEM_TEST9_CHILD_COUNT; i++) {
		IDATA returnCode = waitForTestProcess(portLibrary, child[i]);

		switch(returnCode) {
		case 0: /*Memory is opened*/
			break;
		case 1: /*Memory is created*/
			if(created) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "more then 1 child created the semaphore.\n");
			} else {
				created=1;
			}
			break;

		default: /*Memory open call failed*/
			outputErrorMessage(PORTTEST_ERROR_ARGS, "child reported error. rc=%d\n",returnCode);
			break;
		}
	}

	if(0 == created) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "None of the child reported semaphore creation: failed\n");
	}

exit:
	if(launchSemaphore != -1) {
		CloseLaunchSemaphore(PORTLIB, launchSemaphore);
	}
	j9shmem_open(cacheDir, 0, &childrenmemory, J9SHMEM_TEST9_SHMEM_NAME, 10240, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	j9shmem_destroy(cacheDir, 0, &childrenmemory);

	return reportTestExit(PORTLIB, testName);
}

int
j9shmem_test9_child (J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test9_child";
	IDATA rc;
	int result=-1;
	IDATA launchSemaphore = -1;
	struct j9shmem_handle* childrenmemory = NULL;

	reportTestEntry(PORTLIB, testName);

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

	launchSemaphore = openLaunchSemaphore(PORTLIB, J9SHMEM_TEST9_LAUNCH_CONTROL_NAME, J9SHMEM_TEST9_CHILD_COUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto exit;
	}

	ReleaseLaunchSemaphore (PORTLIB, launchSemaphore, 1);

	rc = j9shmem_open(cacheDir, 0, &childrenmemory, J9SHMEM_TEST9_SHMEM_NAME, 10240, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	switch(rc) {
	case J9PORT_INFO_SHMEM_OPENED:
		result=0;
		break;
	case J9PORT_INFO_SHMEM_CREATED:
		result=1;
		break;
	case J9PORT_ERROR_SHMEM_OPFAILED:
	case J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test9_child: Error opening shared memory\n");
		result=-1;
		break;
	default:
		result=2;
	}

exit:
	if(childrenmemory != NULL) {
		j9shmem_close(&childrenmemory);
	}
	reportTestExit(PORTLIB, testName);
	return result;
}

int
j9shmem_test10(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle* mem0 = NULL;
	int size = 1024*1024*1024; /* creating 1GB.. */
	IDATA rc = 0;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test10";

	reportTestEntry(PORTLIB, testName);

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

	rc=j9shmem_open(cacheDir, 0, &mem0, SHAREDMEMORYA, size, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if(rc != J9PORT_ERROR_SHMEM_OPFAILED && rc != J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_open report success with large area!\n");
		j9shmem_destroy(cacheDir, 0, &mem0);
	} else if(mem0 != NULL) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_open failed case does not clean up properly!\n");
	}

exit:
	return reportTestExit(PORTLIB, testName);
}

/* Test that if j9shmem_get_region_granularity() returns 0, that j9shmem_protect() also returns
 * J9PORT_PAGE_PROTECT_NOT_SUPPORTED
 */
int
j9shmem_test11(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle *shm0=NULL;
	IDATA rc=0;
	void *regionA;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test11";

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

	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

	regionA = j9shmem_attach(shm0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionA) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 0 failed\n");
		goto exit;
	}

	if (j9shmem_protect(cacheDir, 0, regionA,SHMSIZE,0) == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
		if (j9shmem_get_region_granularity(cacheDir, 0, regionA) !=0 ) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_protect() returned J9PORT_PAGE_PROTECT_NOT_SUPPORTED yet, j9shmem_get_region_granularity did not return 0.)\n");
			goto exit;
		}
	}

exit:

	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);
	}

	return reportTestExit(portLibrary, testName);
}

/*
 * Test that if j9shmem_protect() is called on the shared memory region
 * with flags = J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE,
 * that the memory region is still readable/writeable.
 */
int
j9shmem_test12(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle *shm0=NULL;
	IDATA rc=0, i;
	void *regionA;
	char *regionCharA;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test12";
	char aChar = 'a';
	
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

	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

	regionA = j9shmem_attach(shm0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionA) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 0 failed\n");
		goto exit;
	}

	rc = j9shmem_protect(cacheDir, 0, regionA,SHMSIZE,J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
	if (rc != 0) {
			if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
				outputComment(portLibrary, "shmem_protect not supported on this platform, bypassing test\n");
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_protect() failed\n");
			}
			goto exit;
	}


	/*
	 * if SIGSEGV/SIGBUS here then something wrong with shared memory
	 * returned by attach/detach
	 */
	regionCharA = (char*) regionA;
	outputComment(PORTLIB, "If SIGSEGV here then shmem_protect is allowing us to write to a region we just set to writeable\n");
	for(i=0; i<SHMSIZE; i++) {
		regionCharA[i] = (char)((i%26)+'a');
		aChar = regionCharA[i];
	}

exit:

	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);
	}
	return reportTestExit(portLibrary, testName);
}

/* Test that if j9shmem_protect() is called on the shared memory region
 * with flags = J9PORT_PAGE_PROTECT_READ
 * that the memory region is readable, but not writeable, and that we are able to set it back to
 *  writeable by setting the flags = J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE.
 */
int
j9shmem_test13(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle *shm0=NULL;
	IDATA rc=0, i;
	void *regionA;
	char *regionCharA;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test13";
	char aChar = 'a';
	U_32 signalHandlerFlags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	I_32 protectResult;
	UDATA result=0;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;

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

    outputComment(portLibrary,"%s: test memory regions can be made read only\n",testName);

	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

    outputComment(portLibrary,"Region size: %d\n",j9shmem_get_region_granularity(cacheDir, 0, shm0));
	regionA = j9shmem_attach(shm0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionA) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 0 failed\n");
		goto exit;
	}

    /* Make the memory read only */
	rc = j9shmem_protect(cacheDir, 0, regionA,SHMSIZE,J9PORT_PAGE_PROTECT_READ);
	if (rc != 0) {
			if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
				outputComment(portLibrary, "shmem_protect not supported on this platform, bypassing test\n");
			} else {
				lastErrorMessage = (char *)j9error_last_error_message();
				lastErrorNumber = j9error_last_error_number();
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
			}
			goto exit;
	}


	/* make sure that we can read from this region that was set to read only
	 * if SIGSEGV/SIGBUS here then something wrong with call to shmem_protect
	 */
	regionCharA = (char*) regionA;
	outputComment(PORTLIB, "Reading from readonly region - If SIGSEGV here then shared memory is not allocated properly\n");
	for(i=0; i<SHMSIZE; i++) {
		aChar = regionCharA[i];
	}

	/* try to write to region that was set to read only */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {

		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.length = SHMSIZE;
		writerInfo.testName = testName;
        writerInfo.shouldCrash = 1; /* this should crash... */
		writerInfo.writeAddress = (char *)regionA;

		outputComment(PORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/* Set the region back to read/write and try to write
	 * we should be able to write to the entire region again
	 */
    outputComment(portLibrary,"Reseting memory to readwrite\n");
    rc = j9shmem_protect(cacheDir, 0, regionA,SHMSIZE,J9PORT_PAGE_PROTECT_WRITE);
	if (rc != 0) {
			if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
				outputComment(portLibrary, "shmem_protect not supported on this platform, bypassing test\n");
			} else {
				lastErrorMessage = (char *)j9error_last_error_message();
				lastErrorNumber = j9error_last_error_number();
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
			}
			goto exit;
	}
	/* if SIGSEGV/SIGBUS here then something wrong with call to shmem_protect */
	for(i=0; i<SHMSIZE; i++) {
		aChar = regionCharA[i];
	}

    /* try again to write to the memory, it should be fine now */
    if (!j9sig_can_protect(signalHandlerFlags)) {
        outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
        goto exit;
    } else {

        handlerInfo.testName = testName;
        writerInfo.length = SHMSIZE;
        writerInfo.testName = testName;
        writerInfo.shouldCrash = 0; /* Should not fail */
        writerInfo.writeAddress = (char *)regionA;

  outputComment(PORTLIB, "Writing to memory - this should be fine\n");
  protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

  if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
   outputErrorMessage(PORTTEST_ERROR_ARGS, "Unable to write to test region after resetting to read/write\n");
  } else {
   /* test passed */
  }
 }


exit:

	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);
	}
	return reportTestExit(portLibrary, testName);
}


/* Test that if we grab two pages (page size as returned by the granularity call) that
 * we can page protect the first and still write to the second, but not write to the first
 * or last byte of the first.
 */
int
j9shmem_test14(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle *shm0=NULL;
	IDATA rc=0;
	void *regionA;
	char *regionCharA;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test14";
	char aChar = 'a';
	U_32 signalHandlerFlags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	I_32 protectResult;
	UDATA result=0;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	UDATA pageSize;
	UDATA i;

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

    pageSize = j9shmem_get_region_granularity(cacheDir, 0, NULL);

    if (pageSize==0) {
    	/* means unsupported on this platform ... double check by calling protect */
		outputComment(portLibrary, "shmem_protect not supported on this platform, bypassing test\n");
		goto exit;
    }

	reportTestEntry(portLibrary, testName);
    outputComment(portLibrary,"%s: test protection at page boundaries\n",testName);

    outputComment(portLibrary,"%s: allocating a pair of pages\n",testName);
	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, pageSize*2, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

    outputComment(portLibrary,"Page protection granularity: %d\n",j9shmem_get_region_granularity(cacheDir, 0, shm0));
	regionA = j9shmem_attach(shm0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionA) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 0 failed\n");
		goto exit;
	}


	outputComment(PORTLIB,"Protecting the first page\n");
	rc = j9shmem_protect(cacheDir, 0, regionA,pageSize,J9PORT_PAGE_PROTECT_READ);
	if (rc != 0) {
			if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
				outputComment(portLibrary, "shmem_protect not supported on this platform, bypassing test\n");
			} else {
				lastErrorMessage = (char *)j9error_last_error_message();
				lastErrorNumber = j9error_last_error_number();
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
			}
			goto exit;
	}


	/* make sure that we can read from this region that was set to read only
	 * if SIGSEGV/SIGBUS here then something wrong with call to shmem_protect
	 */
	regionCharA = (char*) regionA;
	outputComment(PORTLIB, "Reading from readonly region - should be OK\n");
	for(i=0; i<pageSize; i++) {
		aChar = regionCharA[i];
	}

	/* try to write to region that was set to read only */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {
		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.length = pageSize;
		writerInfo.testName = testName;
        writerInfo.shouldCrash = 1; /* this should crash... */
		writerInfo.writeAddress = (char *)regionA;

		outputComment(PORTLIB, "Writing to readonly region - should crash\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/** Attempt write to second page, should be OK */
    if (!j9sig_can_protect(signalHandlerFlags)) {
      outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
      goto exit;
    } else {
	  handlerInfo.testName = testName;
	  writerInfo.length = pageSize;
	  writerInfo.testName = testName;
	  writerInfo.shouldCrash = 0; /* Should not fail */
	  writerInfo.writeAddress = ((char *)(regionA))+pageSize;

	  outputComment(PORTLIB, "Writing to second page - should be OK\n");
	  protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

	  if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
	   outputErrorMessage(PORTTEST_ERROR_ARGS, "Unable to write to test region after resetting to read/write\n");
	  } else {
	   /* test passed */
	  }
	}

	/* Set the region back to read/write and try to write
	 * we should be able to write to the entire region again
	 */
    outputComment(portLibrary,"Reseting first page to readwrite\n");
    rc = j9shmem_protect(cacheDir, 0, regionA,pageSize,J9PORT_PAGE_PROTECT_WRITE);
	if (rc != 0) {
			if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
				outputComment(portLibrary, "shmem_protect not supported on this platform, bypassing test\n");
			} else {
				lastErrorMessage = (char *)j9error_last_error_message();
				lastErrorNumber = j9error_last_error_number();
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
			}
			goto exit;
	}

	/* if SIGSEGV/SIGBUS here then something wrong with call to shmem_protect */
	for(i=0; i<pageSize; i++) {
		aChar = regionCharA[i];
	}

    /* try again to write to the memory, it should be fine now */
    if (!j9sig_can_protect(signalHandlerFlags)) {
      outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
      goto exit;
    } else {

      handlerInfo.testName = testName;
	  writerInfo.length = pageSize;
	  writerInfo.testName = testName;
	  writerInfo.shouldCrash = 0; /* Should not fail */
	  writerInfo.writeAddress = (char *)regionA;

      outputComment(PORTLIB, "Writing to page one again - should be OK\n");
      protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

      if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
        outputErrorMessage(PORTTEST_ERROR_ARGS, "Unable to write to test region after resetting to read/write\n");
      } else {
        /* test passed */
      }
    }


exit:

	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);
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
j9shmem_test15(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=0;
	int i = 0;
	int j = 0;
	IDATA rc2;
	struct j9shmem_handle *mem0 = NULL;
	const char* testName = "j9shmem_test15";
	char mybaseFilePath[J9SH_MAXPATH];
	char cacheDir[J9SH_MAXPATH];
	IDATA readfd = 0;
	IDATA writefd = 0;
	j9shmem_controlFileFormat oldfileinfo;
	struct stat statbefore;
	struct stat statafter;
	IDATA launchSemaphore = -1;
	J9ProcessHandle child[TEST15_NUM_CHILDPROCESSES];
	struct shmid_ds buf;

	reportTestEntry(portLibrary, testName);	

	/*Get the control file name*/

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
	
	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", cacheDir, TEST15_MEM_NAME);

	/*This first call to j9shxxx_open is simply to ensure that we don't use any old SysV objects. If j9shxxx_open returns 'OPENED' then we destroy the object.*/
	rc = j9shmem_open(cacheDir, 0, &mem0, TEST15_MEM_NAME, TEST15_MEM_SIZE, 0600, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (rc == J9PORT_ERROR_SHMEM_OPFAILED) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open sysv obj: %s\n", cacheDir);
		goto cleanup;
	} else if (rc == J9PORT_INFO_SHMEM_OPENED) {
		if (j9shmem_destroy(cacheDir, 0, &mem0) == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "A precondition of this test is that we don't use any old SysV objects: %s\n", cacheDir);
			goto cleanup;
		}	
	} else {
		/*a new mem was created ... just close it*/
		if (mem0 != NULL) {
			j9shmem_close(&mem0);
		}
	}

	/*This test starts multiple process we use a semaphore to synchronize the launch*/
	launchSemaphore = openLaunchSemaphore(PORTLIB, TEST15_LAUNCH_SEM_NAME, 1);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto cleanup;
	}

	/*Create a new clean memory for testing*/
	rc = j9shmem_open(cacheDir, 0, &mem0, TEST15_MEM_NAME, TEST15_MEM_SIZE, 0600, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (rc != J9PORT_INFO_SHMEM_CREATED && rc != J9PORT_INFO_SHMEM_OPENED) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open/create new memory: %s\n", mybaseFilePath);
		goto cleanup;
	}
	if (mem0 != NULL) {
		j9shmem_close(&mem0);
	}


	
	/*Read/Backup the control file*/
	if(-1 == (readfd = j9file_open(mybaseFilePath,  EsOpenRead, 0600))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot open control file: %s\n", mybaseFilePath);
		goto cleanup;
	}	
	if(j9file_read(readfd, &oldfileinfo, sizeof(j9shmem_controlFileFormat)) != sizeof(j9shmem_controlFileFormat)) {
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
	for (i =0; i < 12; i+=1)  {
		/*---Setup our test---*/		
		IDATA tmpfd;
		BOOLEAN shouldUnlink = TRUE;
		BOOLEAN shouldOpenFail = FALSE;
		BOOLEAN doFileWrite = TRUE;
		BOOLEAN doDestroy = FALSE;
		BOOLEAN setFileReadOnly = FALSE;
		j9shmem_controlFileFormat newfileinfo = oldfileinfo;
		IDATA numCreatesReturned = 0;
		IDATA numOpensReturned = 0;
		IDATA numErrorsReturned = 0;

		switch (i) { 
			case 0: {
				outputComment(PORTLIB, "\tSub Test %d: test modified fkey\n", i+1);
				newfileinfo.common.ftok_key = 0xcafebabe;
				break;
			}
			case 1: {
				outputComment(PORTLIB, "\tSub Test %d: test modified id\n", i+1);
				/*-1 is worst value ... b/c it may slip thru some if statements if i was not careful during dev*/				
				newfileinfo.common.shmid = 0xFFFFFFFF;
				break;
			}
			case 2: {
				outputComment(PORTLIB, "\tSub Test %d: test a size that is too small\n", i+1);
				/*Too small*/				
				newfileinfo.size = 1;
				break;
			}
			case 3: {
				outputComment(PORTLIB, "\tSub Test %d: test a size that is too large\n", i+1);
				/*Too big*/				
				newfileinfo.size = 0x01000000; /*16 M*/
				break;
			}
			case 4: {
				outputComment(PORTLIB, "\tSub Test %d: test a uid change\n", i+1);
				newfileinfo.uid = 0;
				break;
			}
			case 5: {
				outputComment(PORTLIB, "\tSub Test %d:test a gid change\n", i+1);
				newfileinfo.gid = 0;
				break;
			}
			case 6: {
				outputComment(PORTLIB, "\tSub Test %d: Set file to use invalid key. Chmod file to be readonly.\n", i+1);
				newfileinfo.common.ftok_key = 0xcafebabe;
				setFileReadOnly = TRUE;
				shouldUnlink = FALSE;
				shouldOpenFail = TRUE;
				break;
			}
			case 7: {
				outputComment(PORTLIB, "\tSub Test %d: change the version\n", i+1);	
				newfileinfo.common.version = 0;
				shouldUnlink = FALSE;
				shouldOpenFail = FALSE;
				break;
			}
			case 8: {
				outputComment(PORTLIB, "\tSub Test %d: change the major modlevel\n", i+1);	
				newfileinfo.common.modlevel = 0x00010001;
				shouldUnlink = FALSE;
				shouldOpenFail = TRUE;
				break;
			}
			case 9: {
				/*IMPORTANT NOTE: there is a little 'hack' at the bottom of this loop to set the permissions back ... otherwise the next test will fail*/
				outputComment(PORTLIB, "\tSub Test %d: set the SysV IPC obj to be readonly\n", i+1);
				if (shmctl(newfileinfo.common.shmid, IPC_STAT, &buf) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not get memory permissions\n");
					goto cleanup;
				}				
				buf.shm_perm.mode=0400;
				if (shmctl(newfileinfo.common.shmid, IPC_SET, &buf) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not modify memory permissions\n");
					goto cleanup;
				}
				shouldUnlink = FALSE;
				shouldOpenFail = TRUE;
				break;			
			}
			/*--- These 2 tests should always be last ... b/c it deletes the sysv obj and control file ---*/
			case 10: {
				outputComment(PORTLIB, "\tSub Test %d: try with no error ... every one should open\n", i+1);	
				shouldUnlink = FALSE;
				shouldOpenFail = FALSE;
				doDestroy = TRUE;
				break;
			}
			case 11: {
				outputComment(PORTLIB, "\tSub Test %d: try with control file ... same as test9 only 1 should create\n", i+1);	
				shouldUnlink = FALSE;
				shouldOpenFail = FALSE;
				doDestroy = TRUE;
				doFileWrite= FALSE;
				break;
			}
			default: {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\ttoo many loops\n");
				goto cleanup;
			}
		}

		if (doFileWrite == TRUE) {
			/*Write new data to the control file*/
			if(-1 == (writefd = j9file_open(mybaseFilePath,  EsOpenWrite | EsOpenRead | EsOpenCreate, 0600))) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot open control file: %s\n", mybaseFilePath);
				goto cleanup;
			}	
			if(j9file_write(writefd, &newfileinfo, sizeof(j9shmem_controlFileFormat)) != sizeof(j9shmem_controlFileFormat)) {
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

		SetLaunchSemaphore(PORTLIB, launchSemaphore, TEST15_NUM_CHILDPROCESSES);

		/*--- Start our test processes ---*/
		for (j = 0; j < TEST15_NUM_CHILDPROCESSES; j+=1) {
			child[j]=launchChildProcess(PORTLIB, testName, argv0, "-child_j9shmem_test15");
			if(child[j] == NULL) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
				goto cleanup;
			}	
		}

		WaitForLaunchSemaphore(PORTLIB, launchSemaphore);
	
		for(j=0; j<TEST15_NUM_CHILDPROCESSES; j++) {
			IDATA returnCode = waitForTestProcess(PORTLIB, child[j]);
			switch(returnCode) {
			case J9PORT_INFO_SHMEM_OPENED: /*memory is opened*/	
				numOpensReturned +=1;
				break;
			case  J9PORT_INFO_SHMEM_CREATED:/*memory is created*/
				numCreatesReturned +=1;
				break;
			default: /*memory open call failed*/
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
				if (numOpensReturned != (TEST15_NUM_CHILDPROCESSES-1)) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of opens should be %d, instead it is %d\n", (TEST15_NUM_CHILDPROCESSES-1), numOpensReturned);
					goto cleanup;
				}
			} else {
				if (numCreatesReturned!=0) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of creates should be %d, instead it is %d\n", 0, numCreatesReturned);
					goto cleanup;
				}
				if (numOpensReturned != TEST15_NUM_CHILDPROCESSES) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of opens should be %d, instead it is %d\n", TEST15_NUM_CHILDPROCESSES, numOpensReturned);
					goto cleanup;
				}
			}
		} else {
				if (numErrorsReturned !=TEST15_NUM_CHILDPROCESSES) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of errors should be %d, instead it is %d\n", TEST15_NUM_CHILDPROCESSES, numErrorsReturned);
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
			j9shmem_open(cacheDir, 0, &mem0, TEST15_MEM_NAME, TEST15_MEM_SIZE, 0, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
			if(mem0 != NULL) {
				j9shmem_destroy(cacheDir, 0, &mem0);
			}
		}
		
		if (i == 6) {
			if (chmod(mybaseFilePath, S_IRUSR | S_IWUSR) == -1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot chmod 0600 the control file: %s\n", mybaseFilePath);
				goto cleanup;
			}
		}

		if (i == 9) {
				/*IMPORTANT NOTE: set the sysv obj  perm back*/
				buf.shm_perm.mode=0600;
				if (shmctl(newfileinfo.common.shmid, IPC_SET, &buf) == -1) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not reset sysv obj  permissions\n");
					goto cleanup;
				}
		}

	}
	/*---End our test---*/


cleanup:
	if (i == 6) {
		if (chmod(mybaseFilePath, S_IRUSR | S_IWUSR) == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcannot chmod 0600 the control file: %s\n", mybaseFilePath);
		}
	}

	if (i == 9) {
		/*IMPORTANT NOTE: set the sysv obj perm back so we can cleanup*/
		buf.shm_perm.mode=0600;
		if (shmctl(oldfileinfo.common.shmid, IPC_SET, &buf) == -1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tcould not recover/clean sysv obj permissions\n", mybaseFilePath);
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
	if(j9file_write(writefd, &oldfileinfo, sizeof(j9shmem_controlFileFormat)) != sizeof(j9shmem_controlFileFormat)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot reset the control file\n");
	}
	if(-1 == j9file_close(writefd)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot close the control file\n");
	}
	
	/*Some tests intentionally fail open ... so mem0 may be NULL ... we call an unchecked open just to set mem0 if it can be done*/
	j9shmem_open(cacheDir, 0, &mem0, TEST15_MEM_NAME, TEST15_MEM_SIZE, 0, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if(mem0 != NULL) {
		j9shmem_destroy(cacheDir, 0, &mem0);
	}
	j9file_unlink(mybaseFilePath);
	return reportTestExit(PORTLIB, testName);
}

int 
j9shmem_test15_testchild(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test15_testchild";
	struct j9shmem_handle * mem0=NULL;
	IDATA launchSemaphore=-1;
	int rc;
	rc = J9PORT_ERROR_SHMEM_OPFAILED;
	
#if defined TEST15_CHILDOUTPUT	
	outputComment(PORTLIB, "\t\tStarting test \t%s\n", testName);
#endif

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

	launchSemaphore = openLaunchSemaphore(PORTLIB, TEST15_LAUNCH_SEM_NAME, 1);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto cleanup;
	}
	
	ReleaseLaunchSemaphore(PORTLIB, launchSemaphore, 1);
	rc = j9shmem_open(cacheDir, 0, &mem0, TEST15_MEM_NAME, TEST15_MEM_SIZE, 0, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

#if defined TEST15_CHILDOUTPUT	
	switch(rc) {
		case J9PORT_ERROR_SHMEM_OPFAILED:
			outputComment(PORTLIB, "\t\t%s Error opening shared j9shmem_test15\n", testName);	
			break;
	}
#endif	
	
cleanup:
	
	if(mem0 != NULL) {	
		j9shmem_close(&mem0);
	}

#if defined TEST15_CHILDOUTPUT	
	outputComment(PORTLIB, "\t\tEnding test \t%s\n", testName);	
#endif
	
	return rc;
}
#endif

/** Open shared memory, attach to it, check that memory counters are updated correctly */
int
j9shmem_test16(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shmem_handle *shm0=NULL;
	IDATA rc=0, i;
	void *regionA;
	char *regionCharA;
	char cacheDir[J9SH_MAXPATH];
	const char* testName = "j9shmem_test16";
	UDATA initialBlocks, initialBytes, finalBlocks, finalBytes;

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

	getPortLibraryMemoryCategoryData(portLibrary, &initialBlocks, &initialBytes);

	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

	/* Try destroying the shmem immediately - check we don't alter the memory counters */
	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);

		getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "shmem_destroy without attach didn't update the memory block counters as expected. Initial blocks=%zu, final blocks=%zu\n", initialBlocks, finalBlocks);
		}

		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "shmem_destroy without attach didn't update the memory byte counters as expected. Initial bytes=%zu, final bytes=%zu\n", initialBytes, finalBytes);
		}
	}

	getPortLibraryMemoryCategoryData(portLibrary, &initialBlocks, &initialBytes);

	/* Open again and attach the shared memory */
	rc = j9shmem_open(cacheDir, 0, &shm0, SHAREDMEMORYA, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);
	if (J9PORT_ERROR_SHMEM_OPFAILED == rc || J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "creation of shared memory area 0 failed\n");
		goto exit;
	}

	regionA = j9shmem_attach(shm0, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == regionA) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "attach shared memory 0 failed\n");
		goto exit;
	}

	regionCharA = (char*) regionA;
	/*
	 * if SIGSEGV/SIGBUS here then something wrong with shared memory
	 * returned by attach/detach
	 */
	outputComment(PORTLIB, "If SIGSEGV here then shared memory is not allocated properly\n");
	for(i=0; i<SHMSIZE; i++) {
		regionCharA[i] = (char)((i%26)+'a');
	}

	getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

	if (finalBlocks <= initialBlocks) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "shmem_open & attach didn't update the memory block counters as expected. Initial blocks=%zu, final blocks=%zu\n", initialBlocks, finalBlocks);
	}

	if (finalBytes < (initialBytes + SHMSIZE)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "shmem_open & attach didn't update the memory byte counters as expected. Initial bytes=%zu, final bytes=%zu\n", initialBytes, finalBytes);
	}

exit:

	if(NULL != shm0) {
		j9shmem_destroy(cacheDir, 0, &shm0);

		getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "shmem_destroy didn't update the memory block counters as expected. Initial blocks=%zu, final blocks=%zu\n", initialBlocks, finalBlocks);
		}

		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "shmem_destroy didn't update the memory byte counters as expected. Initial bytes=%zu, final bytes=%zu\n", initialBytes, finalBytes);
		}
	}

	return reportTestExit(portLibrary, testName);
}

#if !(defined(WIN32) || defined(WIN64))
/**
 * Verify @ref j9shmem.c::j9shmem_handle_stat "j9shmem_handle_stat" function.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9shmem_test17(J9PortLibrary *portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = TEST17_SHAREDMEMORY;
	IDATA rc;
	struct j9shmem_handle* handle = NULL;
	struct J9PortShmemStatistic statbuf;
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

	/* z/OS rounds up the size to 1M boundary, so create a 1M shared memory region to avoid platform specific test condition when verifying the size */
	rc = j9shmem_open(cacheDir, 0, &handle, testName, SHMSIZE, J9SH_SHMEM_PERM_READ_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY, J9SHMEM_NO_FLAGS, NULL);

	if((J9PORT_ERROR_SHMEM_OPFAILED == rc) || (J9PORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test17 opening shmem area failed \n");
		goto cleanup;
	}

	rc = j9shmem_handle_stat(handle, &statbuf);
	if( -1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shmem_test17 j9shmem_handle_stat call failed \n");
		goto cleanup;
	}

	verifyShmemStats(portLibrary, handle, &statbuf, testName);

cleanup:
	if(NULL != handle) {
		j9shmem_destroy(cacheDir, 0, &handle);
	}
	return reportTestExit(portLibrary, testName);
}
#endif /* !(defined(WIN32) || defined(WIN64)) */

int
j9shmem_runTests(J9PortLibrary *portLibrary, char* argv0, const char* shmem_child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=TEST_PASS;
	IDATA rc2;
	char basedir[1024];

	if(shmem_child != NULL) {
		if(strcmp(shmem_child,"j9shmem_test4") == 0) {
			return j9shmem_test4_child(portLibrary);
		} else if(strcmp(shmem_child, "j9shmem_test9") == 0) {
			return j9shmem_test9_child(portLibrary);
		}
#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
		else if(strcmp(shmem_child, "j9shmem_test15") == 0) {
			return j9shmem_test15_testchild(portLibrary);
		}
#endif 
	}
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
	/* Ensure that any Testmem files from failing tests are deleted prior to starting */
	if (rc2 >= 0) {
		deleteControlDirectory(PORTLIB, basedir);
	}

	HEADING(PORTLIB,"Shared Memory Test");

	rc |= j9shmem_test1(PORTLIB);
	rc |= j9shmem_test2(PORTLIB);
	rc |= j9shmem_test3(PORTLIB);
	rc |= j9shmem_test4(PORTLIB, argv0);
	rc |= j9shmem_test5(PORTLIB);
#if 0
	/*
	 * This test is no longer useful since 132894 was fixed. And it is leaving sys v 
	 * artifacts around so I am removing it.
	 */
	rc |= j9shmem_test6(PORTLIB);
#endif
	rc |= j9shmem_test7(PORTLIB);
	rc |= j9shmem_test8(PORTLIB);
	rc |= j9shmem_test9(PORTLIB, argv0);
	rc |= j9shmem_test11(PORTLIB);
	rc |= j9shmem_test12(PORTLIB);
	rc |= j9shmem_test13(PORTLIB);
	rc |= j9shmem_test14(PORTLIB);

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	rc |= j9shmem_test15(portLibrary, argv0);	
#endif

	rc |= j9shmem_test16(portLibrary);
#if !(defined(WIN32) || defined(WIN64))
	rc |= j9shmem_test17(portLibrary);
#endif /* !(defined(WIN32) || defined(WIN64)) */

	/* Test 10 not ready yet */
	/* rc |= j9shmem_test10(PORTLIB); */

	j9tty_printf(PORTLIB, "\nShared Memory test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

