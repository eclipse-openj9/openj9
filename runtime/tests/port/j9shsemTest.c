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



#define SEMAPHOREA "semaphoreA"
#define SEMAPHOREB "semaphoreB"
#define SEMAPHORECHILD "semaphoreChild"
#define SEMAPHORESETSIZE 10

#define TESTSEM_NAME "Testsem"

#define TESTVALUE 9

static void
getControlDirectoryName(struct J9PortLibrary *portLibrary, char* baseDir)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

#ifdef WIN32
#define J9SHTEST_BASEDIR "j9shsem/"
#else
#define J9SHTEST_BASEDIR "/tmp/j9shsem/"
#endif

	j9str_printf(PORTLIB, baseDir, J9SH_MAXPATH, "%s", J9SHTEST_BASEDIR);

}

static void
initializeControlDirectory(struct J9PortLibrary *portLibrary, char* baseDir)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* Create the control directory. Ensure that any Testsem files from failing tests are deleted prior to starting. */

	getControlDirectoryName(PORTLIB, baseDir);
	deleteControlDirectory(portLibrary, baseDir);
	j9file_mkdir(baseDir);
	j9file_chmod(baseDir, 0777);
}

/* Sanity test, just to see whether create and destroy works */
int
j9shsem_test1(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shsem_handle* myhandle;
	IDATA rc;
	J9PortShSemParameters params;
 	char baseDir[J9SH_MAXPATH];
	const char* testName = "j9shsem_test1";

	reportTestEntry(portLibrary, testName);

	initializeControlDirectory(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = TESTSEM_NAME;
	params.setSize = 10;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open (&myhandle, &params);

	if(J9PORT_ERROR_SHSEM_OPFAILED == rc || J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open shared semaphore");
	} else {
		(void)j9shsem_destroy (&myhandle);
	}

	return reportTestExit(portLibrary, testName);
}

int
j9shsem_test2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_test2";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	/*
	 * test check that after deleting with j9shsem_destroy
	 * the next call to j9shsem_open() results in J9PORT_INFO_SHSEM_CREATED
	 */
	struct j9shsem_handle* myhandle;
	int rc;
	IDATA fd = -1;
	void* remember = NULL;
	I_64 sizeofFile;
	IDATA fileSize;
	char mybaseFilePath[J9SH_MAXPATH];
	char baseDir[J9SH_MAXPATH];
	J9PortShSemParameters params;

	reportTestEntry(PORTLIB, testName);

	initializeControlDirectory(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = TESTSEM_NAME;
	params.setSize = 10;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open(&myhandle, &params);
	if(J9PORT_ERROR_SHSEM_OPFAILED == rc || J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "cannot create initial semaphore");
		goto cleanup;
	}

	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", params.controlFileDir, params.semName);
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
	j9shsem_destroy(&myhandle);

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

	rc = j9shsem_open(&myhandle, &params);
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
	rc = j9shsem_destroy(&myhandle);
	if(rc == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_destroy wasn't able to clean up! .... ");
	}

#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms\n", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int
j9shsem_test3(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc=0;
	struct j9shsem_handle *sem0 = NULL;
	const char* testName = "j9shsem_test3";
	J9PortShSemParameters params;
	char baseDir[J9SH_MAXPATH];

	reportTestEntry(portLibrary, testName);

	initializeControlDirectory(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = SEMAPHOREA;
	params.setSize = SEMAPHORESETSIZE;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open(&sem0, &params);
	if (J9PORT_ERROR_SHSEM_OPFAILED == rc || J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_open semaphore 0 failed\n");
		goto cleanup;
	}

	rc = j9shsem_post(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_post failed with rc=%d\n", rc);
		goto cleanup;
	}

	rc = j9shsem_wait(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_wait failed with rc=%d\n", rc);
	}

cleanup:
	if(sem0 != NULL) {
		j9shsem_destroy(&sem0);
	}

	return reportTestExit(portLibrary, testName);
}


#define J9SHSEM_TEST4_CHILDCOUNT 10
#define LAUNCH_CONTROL_SEMAPHORE "J9SHSEM_TEST4_LAUNCH"

int
j9shsem_test4(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_test4";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	J9PortShSemParameters params;
	char baseDir[J9SH_MAXPATH];
	struct j9shsem_handle *childrensemaphore=NULL;
	int i;
	J9ProcessHandle child[J9SHSEM_TEST4_CHILDCOUNT];
	IDATA launchSemaphore=-1;
	int created = 0;

	initializeControlDirectory(portLibrary, baseDir);

	reportTestEntry(portLibrary, testName);

	launchSemaphore = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, J9SHSEM_TEST4_CHILDCOUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		goto cleanup;
	}

	SetLaunchSemaphore(PORTLIB, launchSemaphore, J9SHSEM_TEST4_CHILDCOUNT);

	for(i = 0; i<J9SHSEM_TEST4_CHILDCOUNT; i++) {
		child[i]=launchChildProcess(PORTLIB, testName, argv0, "-child_j9shsem_test4");
		if(NULL == child[i]) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			goto cleanup;
		}
	}

	WaitForLaunchSemaphore(PORTLIB, launchSemaphore);

	for(i=0; i<J9SHSEM_TEST4_CHILDCOUNT; i++) {
		IDATA returnCode = waitForTestProcess(portLibrary, child[i]);
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

	j9shsem_params_init(&params);
	params.semName = SEMAPHORECHILD;
	params.setSize = SEMAPHORESETSIZE;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	j9shsem_open(&childrensemaphore, &params);
	j9shsem_destroy(&childrensemaphore);
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms\n", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int
j9shsem_test4_child(J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_test4_child";
	struct j9shsem_handle *childrensemaphore=NULL;
	IDATA launchSemaphore=-1;
	IDATA rc;
	int result;
	J9PortShSemParameters params;
	char baseDir[J9SH_MAXPATH];

	reportTestEntry(portLibrary, testName);

	launchSemaphore = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, J9SHSEM_TEST4_CHILDCOUNT);
	if(-1 == launchSemaphore) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot open launch semaphore");
		result=2;
		goto cleanup;
	}

	getControlDirectoryName(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = SEMAPHORECHILD;
	params.setSize = SEMAPHORESETSIZE;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	ReleaseLaunchSemaphore(PORTLIB, launchSemaphore, 1);
	rc = j9shsem_open(&childrensemaphore, &params);

	switch(rc) {
	case J9PORT_INFO_SHSEM_OPENED_STALE:
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
		j9shsem_close(&childrensemaphore);
	}

	reportTestExit(PORTLIB, testName);
	return result;
}

int
j9shsem_test5(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_test5";

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
	char baseDir[J9SH_MAXPATH];
	int numSems = 10;
	J9PortShSemParameters params;

	reportTestEntry(PORTLIB, testName);
	initializeControlDirectory(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = TESTSEM_NAME;
	params.setSize = numSems;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open(&myhandle1, &params);
	if(rc == J9PORT_ERROR_SHSEM_OPFAILED || rc == J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening waiting sempahore");
		goto cleanup;
	}

	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s", params.controlFileDir, params.semName);
	j9str_printf(PORTLIB, myNewFilePath, J9SH_MAXPATH, "%s%s_new", params.controlFileDir, params.semName);

	j9file_move(mybaseFilePath, myNewFilePath);

	rc = j9shsem_open(&myhandle2, &params);
	j9file_unlink(myNewFilePath);

	switch(rc) {
	case J9PORT_INFO_SHSEM_OPENED_STALE:
	case J9PORT_INFO_SHSEM_OPENED:
	case J9PORT_INFO_SHSEM_CREATED:
		j9shsem_destroy(&myhandle1);
		j9shsem_destroy(&myhandle2);
		break;
	case J9PORT_ERROR_SHSEM_OPFAILED:
	case J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
		outputErrorMessage(PORTTEST_ERROR_ARGS,
				"Error opening semaphore after reboot!\n");
		break;
	default:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unknown return code rc=%d\n", rc);
		break;
	}

	cleanup:
	if(myhandle1 != NULL) {
		j9shsem_destroy(&myhandle1);
	}
	if(myhandle2 != NULL) {
		j9shsem_destroy(&myhandle2);
	}
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int
j9shsem_test6(J9PortLibrary *portLibrary, const char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	char *exeName = NULL;
	J9ProcessHandle pid;
	IDATA rc;
	struct j9shsem_handle *sem0 = NULL;
	const char* testName = "j9shsem_test6";
	J9PortShSemParameters params;
	char baseDir[J9SH_MAXPATH];

	reportTestEntry(PORTLIB, testName);

	initializeControlDirectory(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = SEMAPHOREA;
	params.setSize = SEMAPHORESETSIZE;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open(&sem0, &params);
	if(rc<0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening sempahore");
		goto cleanup;
	}

	pid = launchChildProcess(PORTLIB, testName, argv0, "-child_j9shsem_test6");
	if(NULL == pid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
		goto cleanup;
	}

	rc = j9shsem_post(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_wait failed with rc=%d\n", rc);
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
		j9shsem_destroy(&sem0);
	}

	return reportTestExit(PORTLIB, testName);
}

int
j9shsem_test6_child(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shsem_handle *sem0=NULL;
	IDATA rc;
	const char* testName = "j9shsem_test6_child";
	J9PortShSemParameters params;
	char baseDir[J9SH_MAXPATH];

	reportTestEntry(PORTLIB, testName);

	getControlDirectoryName(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.semName = SEMAPHOREA;
	params.setSize = SEMAPHORESETSIZE;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open(&sem0, &params);
	switch(rc) {
	case J9PORT_INFO_SHSEM_OPENED_STALE:
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
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unknown result from j9shsem_open rc=%d\n",rc);
		goto cleanup;
	}

	rc = j9shsem_wait(sem0,0,0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9shsem_wait failed with rc=%d\n", rc);
		goto cleanup;
	}

cleanup:

	if(sem0 != NULL) {
		j9shsem_close(&sem0);
	}

	return reportTestExit(portLibrary, testName);
}


#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)

static void test7_cleanup(J9PortLibrary *portLibrary,
		J9PortShSemParameters *params, struct j9shsem_handle* myhandleA,
		struct j9shsem_handle* myhandleB) {
	char mybaseFilePath[J9SH_MAXPATH];
	PORT_ACCESS_FROM_PORT(portLibrary);

	if (myhandleA != NULL) {
		j9shsem_destroy(&myhandleA);
	}
	if (myhandleB != NULL) {
		j9shsem_destroy(&myhandleB);
	}
	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s",
			params->controlFileDir, params->semName);
	j9file_unlink(mybaseFilePath);
	j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s%s_new",
			params->controlFileDir, params->semName);
	j9file_unlink(mybaseFilePath);
}

static IDATA destroyAndReopenSemaphore(J9PortLibrary *portLibrary,
		char *handleName, J9PortShSemParameters *params, struct j9shsem_handle** handle)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	IDATA rc;
	IDATA fd = -1;
	char dummyFilePath[J9SH_MAXPATH];

	outputComment(PORTLIB, "destroy and reopen semaphore using %s\n", handleName);
	j9shsem_destroy(handle); /* Destroys semaphore 0. Should leave the basefile and reuse it */

	if (1 == params->deleteBasefile) {
		/*
		 * this code is for testing the test: to make the test fail, set deleteBasefile=1.
		 * j9shsem_destroy deletes the file and j9file_open with a different name ensures that j9shsem_open doesn't reuse the same inode.
		 */
		j9str_printf(PORTLIB, dummyFilePath, J9SH_MAXPATH, "%s%s_new", params->controlFileDir, params->semName);
		fd = j9file_open(dummyFilePath, EsOpenWrite | EsOpenCreate, 0);
	}

	rc = j9shsem_open(handle, params);
	outputComment(PORTLIB, "%s: result = %d\n", handleName, rc);
	if(-1 != fd) {
		j9file_unlink(dummyFilePath);
	}
	return rc;
}
#endif

int j9shsem_test7(J9PortLibrary *portLibrary) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9shsem_test7";

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	IDATA rc;
	/* UNIX only
	 * test that retaining the basefile prevents semaphore leaks
	 */
	struct j9shsem_handle* handleA = NULL;
	struct j9shsem_handle* handleB = NULL;
	char baseDir[J9SH_MAXPATH];
	const IDATA semaphoreValue = 69;
	IDATA actualValue;

	J9PortShSemParameters params;

	reportTestEntry(PORTLIB, testName);
	initializeControlDirectory(portLibrary, baseDir);
	j9shsem_params_init(&params);
	params.deleteBasefile = 0; /* set this to 1 to get the test to  fail */
	params.semName = TESTSEM_NAME;
	params.setSize = 2;
	params.permission = 0;
	params.controlFileDir = baseDir;
	params.proj_id = 10;
	rc = j9shsem_open(&handleA, &params);
	if(rc == J9PORT_ERROR_SHSEM_OPFAILED || rc == J9PORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error opening semaphore");
		test7_cleanup(portLibrary, &params, handleA, handleB);
		return reportTestExit(PORTLIB, testName);
	}
	outputComment(PORTLIB, "%s: Open semaphore: result = %d\n", testName, rc);

	rc = j9shsem_open(&handleB, &params);
	outputComment(PORTLIB, "%s: Open semaphore again: result = %d\n", testName, rc);
	/* both handles have semaphore 0 */

	/* destroy semaphore 0, create semaphore 1 */
	(void)destroyAndReopenSemaphore(portLibrary, "handle B", &params, &handleB);

	/*
	 * handleA still has the semid of semaphore 0.  It will (redundantly) delete it.
	 * The open operation uses the old file (same inode) to create the same key as the previous destroyAndReopenSemaphore.
	 * We will get semaphore 1.
	 */
	(void)destroyAndReopenSemaphore(portLibrary, "handle A", &params, &handleA);
	/*
	 * Should destroy semaphore 0 (redundantly) and open semaphore 1.
	 */
	/*
	 * Note that if deleteBasefile=0 (normal test case) then j9shsem_destroy retains the file that was used to create semaphore 1,
	 * so subsequent j9shsem_open will open the existing file and get a reference to the existing semaphore.
	 *
	 * If deleteBasefile=1 (i.e. to force an error) then j9shsem_destroy deletes the file that was used to create semaphore 1,
	 * j9shsem_open creates a new file and a new semaphore (semaphore 1).
	 * handleB would point to semaphore 1, but the file would generate a key to semaphore 2.
	 */

	/* set the value of semaphore 1 so we can distinguish it from later incarnations. */
	if (0 != j9shsem_setVal(handleB, 1, semaphoreValue)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error setting semaphore value");
		test7_cleanup(portLibrary, &params, handleA, handleB);
		return reportTestExit(PORTLIB, testName);
	}
	actualValue = j9shsem_getVal(handleB, 1);
	if (semaphoreValue != actualValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error getting semaphore value, got %d", actualValue);
		test7_cleanup(portLibrary, &params, handleA, handleB);
		return reportTestExit(PORTLIB, testName);

	}

	/*
	 * Note that if deleteBasefile=0, j9shsem_open would use the same file (inode) and would open semaphore 1.
	 * If deleteBasefile=1, j9shsem_close and j9shsem_open would replace handleB's reference to semaphore 1 with semaphore 2.
	 */
	outputComment(PORTLIB, "%s: close and reopen semaphore using handle B\n", testName);
	j9shsem_close(&handleB); /* should reuse the basefile */
	rc = j9shsem_open(&handleB, &params);

	outputComment(PORTLIB, "result = %d\n", rc);
	if (J9PORT_INFO_SHSEM_CREATED == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Created new semaphore, expected to open existing\n");
	}

	actualValue = j9shsem_getVal(handleB, 1);
	if (semaphoreValue != actualValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error checking semaphore B value, got %d", actualValue);
		test7_cleanup(portLibrary, &params, handleA, handleB);
		return reportTestExit(PORTLIB, testName);

	}

	actualValue = j9shsem_getVal(handleA, 1);
	if (semaphoreValue != actualValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Error checking semaphore A value, got %d", actualValue);
		test7_cleanup(portLibrary, &params, handleA, handleB);
		return reportTestExit(PORTLIB, testName);

	}
	test7_cleanup(portLibrary, &params, handleA, handleB);
#else
	reportTestEntry(PORTLIB, testName);
	outputComment(PORTLIB, "%s only valid on Unix platforms", testName);
#endif

	return reportTestExit(PORTLIB, testName);
}

int
j9shsem_runTests(struct J9PortLibrary *portLibrary, char* argv0, char* shsem_child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc=TEST_PASS;
	char baseDir[J9SH_MAXPATH];

	if(shsem_child != NULL) {
		if(strcmp(shsem_child,"j9shsem_test4") == 0) {
			return j9shsem_test4_child(portLibrary);
		} else if(strcmp(shsem_child, "j9shsem_test6") == 0) {
			return j9shsem_test6_child(portLibrary);
		}
	}

	HEADING(PORTLIB,"Shared Semaphore Test");

	getControlDirectoryName(PORTLIB, baseDir);
	deleteControlDirectory(portLibrary, baseDir);

	rc = j9shsem_test1(portLibrary);
	rc |= j9shsem_test2(portLibrary);
	rc |= j9shsem_test3(portLibrary);
	rc |= j9shsem_test4(portLibrary, argv0);
	rc |= j9shsem_test5(portLibrary);
	rc |= j9shsem_test6(portLibrary, argv0);
	rc |= j9shsem_test7(portLibrary);

	j9tty_printf(PORTLIB, "\nShared Semaphore test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	getControlDirectoryName(PORTLIB, baseDir);
	deleteControlDirectory(portLibrary, baseDir);
	return TEST_PASS == rc ? 0 : -1;
}

