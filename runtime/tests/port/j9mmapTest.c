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
 * $RCSfile: j9mmapTest.c,v $
 * $Revision: 1.38 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library file system.
 * 
 * Exercise the API for port library mmap system operations.  These functions 
 * can be found in the file @ref j9mmap.c  
 * 
 * @note port library mmap system operations are not optional in the port library table.
 * @note The default processing if memory mapping is not supported by the OS is to allocate
 *       memory and read in the file.  This is the equivalent of copy-on-write mmapping
 * 
 */
#include <stdlib.h>
#include <string.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9fileTest.h"
#include "j9port.h"

#define J9MMAP_PROTECT_TESTS_BUFFER_SIZE 100

typedef struct simpleHandlerInfo {
	const char* testName;
} simpleHandlerInfo;

typedef struct protectedWriteInfo {
	const char* testName;
	char *text;
	int shouldCrash;
	char *address;
} protectedWriteInfo;

static UDATA 
simpleHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	simpleHandlerInfo* info = handler_arg;
	
	outputComment(portLibrary, " A crash occured, signal handler invoked (type = 0x%x)\n", gpType);
	return J9PORT_SIG_EXCEPTION_RETURN;
}


static UDATA
protectedWriter(J9PortLibrary* portLibrary, void* arg)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct protectedWriteInfo *info = (protectedWriteInfo *)arg;
	const char* testName = info->testName;
		
	/* verify that we can write to the file */
	strcpy( info->address, info->text);
	
	if (info->shouldCrash) {
		/* we should have crashed in the above call, so if we got here we didn't have protection */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpectedly continued execution");
	}
	return ~J9PORT_SIG_EXCEPTION_OCCURRED;
}

/**
 * Verify port library properly setup to run mmap tests
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9mmap_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test0";

	J9PortLibraryVersion libraryVersion;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_shutdown is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_map_file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_map_file is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_unmap_file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_unmap_file is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_msync) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_msync is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_protect) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_protect is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mmap_get_region_granularity) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_get_region_granularity is NULL\n");
	}

	rc = j9port_getVersion(portLibrary, &libraryVersion);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion returned %d expected %d\n", rc, 0);
		goto exit;
	}

exit:
	return reportTestExit(portLibrary, testName);
}



/**
 * Verify port memory mapping.
 * 
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * processes read-only mapping
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test1";

	const char *filename = "mmapTest1.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST1_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST1_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST1_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST1_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(mapAddr+30, readableText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	j9mmap_unmap_file(mmapHandle);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port memory mapping.
 * 
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * processes write mapping
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test2(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test2";

	const char *filename = "mmapTest2.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST2_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST2_BUFFER_SIZE];
	char updatedBuffer[J9MMAP_TEST2_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	if (!(mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE)) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST2_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST2_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy( mapAddr+30, modifiedText);

	j9mmap_unmap_file(mmapHandle);

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_TEST2_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, modifiedText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port memory mapping.
 * 
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * processes copy-on-write mapping
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test3(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test3";

	const char *filename = "mmapTest3.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST3_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST3_BUFFER_SIZE];
	char updatedBuffer[J9MMAP_TEST3_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	U_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);
	
	if (!(mmapCapabilities & J9PORT_MMAP_CAPABILITY_COPYONWRITE)) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_COPYONWRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST3_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST3_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_COPYONWRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy( mapAddr+30, modifiedText);

	j9mmap_unmap_file(mmapHandle);

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_TEST3_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close( fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, readableText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Original string does not match in updated buffer\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port memory mapping.
 * 
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * processes msync updates of the file on disk
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test4(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test4";

	const char *filename = "mmapTest4.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST4_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST4_BUFFER_SIZE];
	char updatedBuffer[J9MMAP_TEST4_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	if (!(mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & J9PORT_MMAP_CAPABILITY_MSYNC)) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE or _MSYNC not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST4_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST4_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite | EsOpenSync, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy( mapAddr+30, modifiedText);

	rc = j9mmap_msync(mapAddr, (UDATA)fileLength, J9PORT_MMAP_SYNC_WAIT);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Msync failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
	}

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_TEST4_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, modifiedText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

	j9mmap_unmap_file(mmapHandle);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port memory mapping.
 * 
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * processes the mappingName parameter on Windows and checks mapping
 * between processes
 * 
 * Note: This test uses helper functions from j9fileTest.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9mmap_test5(J9PortLibrary *portLibrary, char* argv0)
{	
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	I_32 mmapCapabilities = j9mmap_capabilities();
	const char* testName = "j9mmap_test5";
	
	reportTestEntry(portLibrary, testName);

	if (!(mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & J9PORT_MMAP_CAPABILITY_MSYNC)) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE or _MSYNC not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Remove status files */
	j9file_unlink("j9mmap_test5_child_1_mapped_file");
	j9file_unlink("j9mmap_test5_child_1_checked_file");
	j9file_unlink("j9mmap_test5_child_2_changed_file");

	/* parent */
	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9mmap_test5_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9mmap_test5_2");
	if(NULL == pid2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid2!\n");
		goto exit;
	}
	
	termstat1 = waitForTestProcess(portLibrary, pid1);
	termstat2 = waitForTestProcess(portLibrary, pid2);
	
	if(termstat1 != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child process 1 exited with status %i: Expected 0.\n", termstat1);
	}
	if(termstat2 != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child process 2 exited with status %i: Expected 0.\n", termstat2);
	}
	
exit:
	
	return reportTestExit(portLibrary, testName); 
}

int
j9mmap_test5_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test5_child_1";

	const char *filename = "mmapTest5.tst";
	const char *mappingName = "C:\\full\\path\\to\\mmapTest5.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST5_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST5_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST4_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST4_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	
	outputComment(portLibrary, "Child 1: Created file\n");

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite | EsOpenSync, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, mappingName, J9PORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}
	
	outputComment(portLibrary, "Child 1: Mapped file, readable text = \"%s\"\n", mapAddr+30);
	
	j9file_create_status_file(portLibrary, "j9mmap_test5_child_1_mapped_file", testName);

	while( !j9file_status_file_exists(portLibrary, "j9mmap_test5_child_2_changed_file", testName) ) {
		SleepFor(1);
	}

	outputComment(portLibrary, "Child 1: Child 2 has changed file, readable text = \"%s\"\n", mapAddr+30);

	if (strcmp(mapAddr+30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in mapped buffer\n");
	}

	j9mmap_unmap_file(mmapHandle);
	

	j9file_create_status_file(portLibrary, "j9mmap_test5_child_1_checked_file", testName);

exit:
	return reportTestExit(portLibrary, testName);
}

int
j9mmap_test5_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test5_child_2";

	const char *filename = "mmapTest5.tst";
	const char *mappingName = "C:\\full\\path\\to\\mmapTest5.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST5_BUFFER_SIZE 100
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, "j9mmap_test5_child_1_mapped_file", testName) ) {
		SleepFor(1);
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite | EsOpenSync, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, mappingName, J9PORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	outputComment(portLibrary, "Child 2: Mapped file, readable text = \"%s\"\n", mapAddr+30);
	
	/* Check mapped area */
	if (strcmp(mapAddr+30, readableText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Readable string does not match in mapped buffer\n");
	}

	/* Change mapped area */
	strcpy( mapAddr+30, modifiedText);

	rc = j9mmap_msync(mapAddr, (UDATA)fileLength, J9PORT_MMAP_SYNC_WAIT);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Msync failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
	}
	
	outputComment(portLibrary, "Child 2: Changed file, readable text = \"%s\"\n", mapAddr+30);

	j9file_create_status_file(portLibrary, "j9mmap_test5_child_2_changed_file", testName);
	
	while( !j9file_status_file_exists(portLibrary, "j9mmap_test5_child_1_checked_file", testName) ) {
		SleepFor(1);
	}

	outputComment(portLibrary, "Child 2: Child 1 has checked file, exiting\n");

	j9mmap_unmap_file(mmapHandle);
	

exit:
	return reportTestExit(portLibrary, testName);
}

/**
* Verify that if mmap has the capability to protect, that we get non-zero from j9mmap_get_region_granularity
* 
* Verify that if @ref j9mmap.c::j9mmap_protect "j9mmap_protect()" returns  J9PORT_MMAP_CAPABILITY_PROTECT, 
* 	that @ref j9mmap::j9mmap_get_region_granularity "j9j9mmap_get_region_granularity()" does not return 0
*  
* @param[in] portLibrary The port library under test
* 
* @return TEST_PASS on success, TEST_FAIL on error
*/
int
j9mmap_test6(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test6";

	const char *filename = "mmapTest6.tst";
	IDATA fd;
	IDATA rc;

	char buffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	if (!(mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & J9PORT_MMAP_CAPABILITY_MSYNC)) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE or _MSYNC not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_PROTECT_TESTS_BUFFER_SIZE ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE );
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;
	
	/* This is the heart of the test */
	if (mmapCapabilities & J9PORT_MMAP_CAPABILITY_PROTECT) {
		if (j9mmap_get_region_granularity(mapAddr) == 0 ) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap claims to be capable of PROTECT, however, region granularity is 0\n");
			goto exit;
		}
		rc = j9mmap_protect(mapAddr, (UDATA)fileLength, J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
		if ((rc & J9PORT_PAGE_PROTECT_NOT_SUPPORTED) != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap is claims to be capable of PROTECT, however, j9mmap_protect() returns J9PORT_PAGE_PROTECT_NOT_SUPPORTED\n");
			goto exit;
		}		
		
	} else {
		rc = j9mmap_protect(mapAddr, (UDATA)fileLength, J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
		if ((rc & J9PORT_PAGE_PROTECT_NOT_SUPPORTED) == 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap is not capable of PROTECT, however, j9mmap_protect() does not return J9PORT_PAGE_PROTECT_NOT_SUPPORTED\n");
			goto exit;
		}			
	}

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	j9mmap_unmap_file(mmapHandle);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify calling protect on memory acquired using mmap.
 * 
 * Verify @ref j9mmap.c::j9mmap_protect "j9mmap_protect()" with 
 * J9PORT_PAGE_PROTECT_READ allows the 
 * memory to be read.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test7(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test7";

	const char *filename = "mmapTest7.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST1_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST1_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_READ) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_READ not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST1_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST1_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	
	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	(void)j9mmap_protect(mapAddr, (UDATA)fileLength, J9PORT_PAGE_PROTECT_READ);
	
	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(mapAddr+30, readableText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	j9mmap_unmap_file(mmapHandle);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify calling protect on memory acquired using mmap.
 * 
 * Verify @ref j9mmap.c::j9mmap_protect "j9mmap_protect()" with 
 * J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE allows the 
 * memory to be read from and written to.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test8(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test8";

	const char *filename = "mmapTest8.tst";
	IDATA fd;
	IDATA rc;

	char buffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	char updatedBuffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_PROTECT_TESTS_BUFFER_SIZE ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE );
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	(void)j9mmap_protect(mapAddr, (UDATA)fileLength, J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
	
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}
	
	strcpy( mapAddr+30, modifiedText);

	j9mmap_unmap_file(mmapHandle);

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, modifiedText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}	

/**
 * Verify calling protect on memory acquired using mmap.
 * 
 * Verify @ref j9mmap.c::j9mmap_protect "j9mmap_protect()" with 
 * J9PORT_PAGE_PROTECT_READ causes us to crash when writing to the protected region, 
 * and that we are once again able to write when we set it back 
 *  to J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_READ
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test9(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test9";

	const char *filename = "mmapTest9.tst";
	IDATA fd;
	IDATA rc;

	char buffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	char updatedBuffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;
	U_32 signalHandlerFlags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	I_32 protectResult;
	UDATA result=0;

	reportTestEntry(portLibrary, testName);

	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_PROTECT_TESTS_BUFFER_SIZE ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE );
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE | J9PORT_MMAP_FLAG_SHARED, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	/* set the region to read only */
	rc = j9mmap_protect(mapAddr, (UDATA)fileLength, J9PORT_PAGE_PROTECT_READ);
	if (rc != 0) {
		if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
			outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		} else {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mmap_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
		}
		goto exit;
	}
	
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}
	
	/* try to write to region that was set to read only */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {
			
		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.shouldCrash = 1;
		writerInfo.address = mapAddr+30;
		
		outputComment(PORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);
		
		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}
	
	/* set the region back to be writeable */
	(void)j9mmap_protect(mapAddr, (UDATA)fileLength, J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
	
	/* verify that we can now write to the file */
	strcpy( mapAddr+30, modifiedText);
	
	j9mmap_unmap_file(mmapHandle);

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, modifiedText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify the page size is correct - create a file over two pages, protect the first and
 * then write to the second page (should be ok) and the last byte of the first page (should fail).
 */
int
j9mmap_test10(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test10";
	const char *filename = "mmapTest10.tst";
	IDATA fd;
	IDATA rc;

	char buffer[128 ];
	char updatedBuffer[128 ];
	UDATA i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;
	U_32 signalHandlerFlags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	I_32 protectResult;
	UDATA result=0;
	I_32 bufferSize = 128;
	UDATA pageSize;

	reportTestEntry(portLibrary, testName);

	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	/* How big do pages claim to be? */
	pageSize = j9mmap_get_region_granularity(mapAddr);
	outputComment(portLibrary,"Page size is reported as %d\n",pageSize);
	/* Initialize buffer to be written - 128 chars with a string right in the middle */
	for(i = 0; i < 128 ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);
	

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	i=0;
	while (i<(pageSize*2)) {
		rc = j9file_write(fd, buffer, bufferSize );
		if (-1 == rc) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
			j9file_close(fd);
			goto exit;
		}
		i+=bufferSize;
	}
	outputComment(portLibrary,"Written out the buffer %d times to fill two pages\n",i/bufferSize);

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	if (fileLength!=pageSize*2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File is wrong length, should be %d but is %d\n",pageSize*2,fileLength);
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE | J9PORT_MMAP_FLAG_SHARED, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	outputComment(portLibrary,"File mapped at location %d\n",mapAddr);
	outputComment(portLibrary,"Protecting first page\n");
	/* set the region to read only */
	rc = j9mmap_protect(mapAddr, (UDATA)fileLength/2, J9PORT_PAGE_PROTECT_READ);
	if (rc != 0) {
		if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
			outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		} else {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mmap_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
		}
		goto exit;
	}
	
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}
	
	/* try to write to region that was set to read only */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {
			
		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.shouldCrash = 1;
		writerInfo.address = mapAddr+30;
		
		outputComment(PORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);
		
		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/* try to write to region that is still writable */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {
			
		/* protectedWriter should not crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.address = mapAddr+pageSize;
		writerInfo.shouldCrash = 0;
		
		outputComment(PORTLIB, "Writing to page two region - should be ok\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);
		
		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Crashed writing to writable page\n");
		} else {
			/* test passed */
		}
	}

	/* set the region back to be writeable */
	(void)j9mmap_protect(mapAddr, (UDATA)fileLength/2, J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
	
	/* verify that we can now write to the file */
	strcpy( mapAddr+30, modifiedText);
	
	j9mmap_unmap_file(mmapHandle);

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, modifiedText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}	

/** Work on a funny boundary (ie. not a 64k boundary, just a 4k one) */
int
j9mmap_test11(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test11";
	const char *filename = "mmapTest11.tst";
	IDATA fd;
	IDATA rc;

	char buffer[128 ];
	char updatedBuffer[128 ];
	UDATA i =0;
	char *readableText = "Here is some readable text in the file";
	char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;
	U_32 signalHandlerFlags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	I_32 protectResult;
	UDATA result=0;
	I_32 bufferSize = 128;
	UDATA pageSize;

	reportTestEntry(portLibrary, testName);

	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	if ((mmapCapabilities & J9PORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}
	
	/* How big do pages claim to be? */
	pageSize = j9mmap_get_region_granularity(mapAddr);
	outputComment(portLibrary,"Page size is reported as %d\n",pageSize);
	/* Initialize buffer to be written - 128 chars with a string right in the middle */
	for(i = 0; i < 128 ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);
	

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	i=0;
	while (i<(pageSize*3)) {
		rc = j9file_write(fd, buffer, bufferSize );
		if (-1 == rc) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
			j9file_close(fd);
			goto exit;
		}
		i+=bufferSize;
	}
	outputComment(portLibrary,"Written out the buffer %d times to fill three pages\n",i/bufferSize);

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	if (fileLength!=pageSize*3) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File is wrong length, should be %d but is %d\n",pageSize*2,fileLength);
	}

	fd = j9file_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_WRITE | J9PORT_MMAP_FLAG_SHARED, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	outputComment(portLibrary,"File mapped at location %d\n",mapAddr);
	outputComment(portLibrary,"Protecting first page 4k into that address\n");
	/* set the region to read only */
	rc = j9mmap_protect(mapAddr+pageSize, pageSize, J9PORT_PAGE_PROTECT_READ);
	if (rc != 0) {
		if (rc == J9PORT_PAGE_PROTECT_NOT_SUPPORTED) {
			outputComment(portLibrary, "J9PORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		} else {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mmap_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
		}
		goto exit;
	}
	
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}
	/* Checking we can write into the bit before the locked page */
	strcpy(mapAddr+30,modifiedText);
	
	/* try to write to region that was set to read only */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {
			
		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.shouldCrash = 1;
		writerInfo.address = mapAddr+30+pageSize;
		
		outputComment(PORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);
		
		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/* try to write to region that is still writable */
	if (!j9sig_can_protect(signalHandlerFlags)) {
		outputComment(portLibrary, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {
			
		/* protectedWriter should not crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.address = mapAddr+(2*pageSize);
		writerInfo.shouldCrash = 0;
		
		outputComment(PORTLIB, "Writing to page two region - should be ok\n");
		protectResult = j9sig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);
		
		if (protectResult == J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Crashed writing to writable page\n");
		} else {
			/* test passed */
		}
	}
	strcpy(mapAddr+(2*pageSize),modifiedText);

	/* set the region back to be writeable */
	(void)j9mmap_protect(mapAddr+pageSize, pageSize, J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
	
	/* verify that we can now write to the file */
	strcpy( mapAddr+30, modifiedText);
	
	j9mmap_unmap_file(mmapHandle);

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer+30, modifiedText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * manages the memory counters
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test12(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test12";

	const char *filename = "mmapTest1.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST1_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST1_BUFFER_SIZE];
	int i =0;
	char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	/* Values for tracking memory usage */
	UDATA initialBytes;
	UDATA initialBlocks;
	UDATA finalBytes;
	UDATA finalBlocks;

	reportTestEntry(portLibrary, testName);

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST1_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST1_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = j9file_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	getPortLibraryMemoryCategoryData(portLibrary, &initialBlocks, &initialBytes);

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, (UDATA)fileLength, NULL, J9PORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

	/* Category tests aren't entirely straightforward - because the mmap implementation may have done other allocations as part of the map.*/
	if (finalBlocks <= initialBlocks) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Block count did not increase as expected. initialBlocks=%zu, finalBlocks=%zu\n", initialBlocks, finalBlocks);
	}

	if (finalBytes < (initialBytes + fileLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Byte count did not increase as expected. initialBytes=%zu, finalBytes=%zu, fileLength=%lld\n", initialBytes, finalBytes, fileLength);
	}

	if (strcmp(mapAddr+30, readableText) != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	j9mmap_unmap_file(mmapHandle);

	getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

	if (finalBlocks != initialBlocks) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Block count did not decrease as expected. initialBlocks=%zu, finalBlocks=%zu\n", initialBlocks, finalBlocks);
	}

	if (finalBytes != initialBytes) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Byte count did not decrease as expected. initialBytes=%zu, finalBytes=%zu\n", initialBytes, finalBytes);
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref j9mmap.c::j9mmap_map_file "j9mmap_map_file()" correctly
 * maps the whole file when the size argument is 0.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mmap_test13(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mmap_test13";

	const char *filename = "mmapTest13.tst";
	IDATA fd;
	IDATA rc;

#define J9MMAP_TEST13_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST13_BUFFER_SIZE];
	int i = 0;
	char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_32 mmapCapabilities = j9mmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(portLibrary, testName);

	/* Initialize buffer to be written */
	for(i = 0; i < J9MMAP_TEST13_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer+30, readableText);

	(void)j9file_unlink(filename);

	fd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = j9file_write(fd, buffer, J9MMAP_TEST13_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(fd);
		goto exit;
	}

	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = j9file_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)j9mmap_map_file(fd, 0, 0, NULL, J9PORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = mmapHandle->pointer;

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = j9file_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (0 != strcmp(mapAddr+30, readableText)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	j9mmap_unmap_file(mmapHandle);

exit:
	return reportTestExit(portLibrary, testName);
}

#define TEST_BUFF_LEN 0x10000
int
j9mmap_testDontNeed(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9mmap_testDontNeed";
	U_32 *testData;
	U_32 cursor = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	reportTestEntry(portLibrary, testName);
	testData = j9mem_allocate_memory(TEST_BUFF_LEN * sizeof(U_32), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == testData) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "memory allocation failed\n");
	}
	for (cursor = 0; cursor < TEST_BUFF_LEN; ++cursor) {
		testData[cursor] = 3 * cursor;
	}
	j9mmap_dont_need(testData, sizeof(testData));
	for (cursor = 0; cursor < TEST_BUFF_LEN; ++cursor) {
		if (3 * cursor != testData[cursor]) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Test buffer corrupted at index %d\n", cursor);
			break;
		}
	}
	j9mem_free_memory(testData);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * 
 * Exercise all API related to port library memory mapping found in
 * @ref j9mmap.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9mmap_runTests(struct J9PortLibrary *portLibrary, char *argv0, char *j9mmap_child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;
	
	if(j9mmap_child != NULL) {
		if(strcmp(j9mmap_child,"j9mmap_test5_1") == 0) {
			return j9mmap_test5_child_1(portLibrary);
		} else if(strcmp(j9mmap_child,"j9mmap_test5_2") == 0) {
			return j9mmap_test5_child_2(portLibrary);
		} 
	}

	HEADING(portLibrary, "Memory Mapping");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9mmap_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9mmap_test1(portLibrary);
		rc |= j9mmap_test2(portLibrary);
		rc |= j9mmap_test3(portLibrary);
		rc |= j9mmap_test4(portLibrary);
		rc |= j9mmap_test5(portLibrary, argv0);
		rc |= j9mmap_test6(portLibrary);
		rc |= j9mmap_test7(portLibrary);
		rc |= j9mmap_test8(portLibrary);
		rc |= j9mmap_test9(portLibrary);
		rc |= j9mmap_test10(portLibrary);
		rc |= j9mmap_test11(portLibrary);
		rc |= j9mmap_test12(portLibrary);
		rc |= j9mmap_test13(portLibrary);
		rc |= j9mmap_testDontNeed(portLibrary);

	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nMemory Mapping test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;	
}


 


