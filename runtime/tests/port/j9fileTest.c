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
 * @file
 * @ingroup PortTest
 * @brief Verify port library file system.
 *
 * Exercise the API for port library file system operations.  These functions
 * can be found in the file @ref j9file.c
 *
 * @note port library file system operations are not optional in the port library table.
 * @note Some file system operations are only available if the standard configuration of the port library is used.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(WIN32)
#include <direct.h>
#include <windows.h> /* for MAX_PATH */
#endif
#include "j9cfg.h"

#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9port.h"

#define J9S_ISGID 02000
/* CMVC 118265 disable tests that fail intermittently until they are fixed */
#if defined(LINUX) && (defined(J9X86) || defined(J9HAMMER))
#define PORT_FILETEST_FLAKY
#endif

#define FILE_OPEN_FUNCTION			j9file_open_helper
#define FILE_CLOSE_FUNCTION			j9file_close_helper
#define FILE_LOCK_BYTES_FUNCTION	j9file_lock_bytes_helper
#define FILE_UNLOCK_BYTES_FUNCTION	j9file_unlock_bytes_helper
#define FILE_READ_FUNCTION			j9file_read_helper
#define FILE_WRITE_FUNCTION			j9file_write_helper
#define FILE_SET_LENGTH_FUNCTION	j9file_set_length_helper
#define FILE_FLENGTH_FUNCTION		j9file_flength_helper

#define FILE_OPEN_FUNCTION_NAME			(isAsync? "j9file_blockingasync_open" : "j9file_open")
#define FILE_CLOSE_FUNCTION_NAME		(isAsync? "j9file_blockingasync_close" : "j9file_close")
#define FILE_LOCK_BYTES_FUNCTION_NAME	(isAsync? "j9file_blockingasync_lock_bytes" : "j9file_lock_bytes")
#define FILE_UNLOCK_BYTES_FUNCTION_NAME	(isAsync? "j9file_blockingasync_unlock_bytes" : "j9file_unlock_bytes")
#define FILE_READ_FUNCTION_NAME			(isAsync? "j9file_blockingasync_read" : "j9file_read")
#define FILE_WRITE_FUNCTION_NAME		(isAsync? "j9file_blockingasync_write" : "j9file_write")
#define FILE_SET_LENGTH_FUNCTION_NAME	(isAsync? "j9file_blockingasync_set_length" : "j9file_set_length")
#define FILE_FLENGTH_FUNCTION_NAME		(isAsync? "j9file_blockingasync_flength" : "j9file_flength")

/**
 * @internal
 * Some functionality supported by @ref j9file.c are optional based on the
 * standard configuration capability.  Cache the capability information
 * for the port library under test.
 *
 * The way the system works.
 *
 * If the port library DLL has a function pointer for any operation, it will put that
 * function pointer at the correct offset in the table.  It doesn't matter if the application
 * requires that functionality or not.  It is okay to give an application more functionality
 * than it requested.  It is not okay to give an application less functionality than it requested.
 *
 * So when checking slots it is an error if a NULL pointer is found in a required slot.
 * It is not an error if a non NULL pointer is found in an optional slot.
 *
 * 0 == standard configuration not supported
 * 1 == standard configuration supported
 */
static int standardConfiguration = 0;

static BOOLEAN isAsync = FALSE;

#define APPEND_ASYNC(testName) (isAsync ?  #testName "_async" : #testName )

/**
 * @internal
 * Write to a file
 * @param[in] portLibrary The port library under test
 * @param[in] fd1 File descriptor to write to
 * @param[in] format Format string to write
 * @param[in] ... Arguments for format
 *
 * @todo Now that j9file_printf exists, we should probably use that instead.
 */
void
fileWrite(struct J9PortLibrary *portLibrary, IDATA fd,const char *format, ...)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	va_list args;

	va_start(args,format);
	j9file_vprintf(fd, format, args);
	va_end(args);
}


IDATA
j9file_open_helper(struct J9PortLibrary *portLibrary, const char *path, I_32 flags, I_32 mode) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_open(path, flags, mode) : j9file_open(path, flags, mode);
}

I_32
j9file_close_helper(struct J9PortLibrary *portLibrary, IDATA fd) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_close(fd) : j9file_close(fd);
}

I_32
j9file_lock_bytes_helper(struct J9PortLibrary *portLibrary, IDATA fd, I_32 lockFlags, U_64 offset, U_64 length) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_lock_bytes(fd, lockFlags, offset, length) : j9file_lock_bytes(fd, lockFlags, offset, length);
}

I_32
j9file_unlock_bytes_helper(struct J9PortLibrary *portLibrary, IDATA fd, U_64 offset, U_64 length) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_unlock_bytes(fd, offset, length) : j9file_unlock_bytes(fd, offset, length);
}

IDATA
j9file_read_helper(struct J9PortLibrary *portLibrary, IDATA fd, void *buf, IDATA nbytes) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_read(fd, buf, nbytes) : j9file_read(fd, buf, nbytes);
}

IDATA
j9file_write_helper(struct J9PortLibrary *portLibrary, IDATA fd, void *buf, IDATA nbytes) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_write(fd, buf, nbytes) : j9file_write(fd, buf, nbytes);
}

I_32
j9file_set_length_helper(struct J9PortLibrary *portLibrary, IDATA fd, I_64 newLength) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_set_length(fd, newLength) :j9file_set_length(fd, newLength);
}

I_64
j9file_flength_helper(struct J9PortLibrary *portLibrary, IDATA fd) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	return isAsync? j9file_blockingasync_flength(fd) : j9file_flength(fd);
}



/**
 * Verify port library properly setup to run file tests
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9file_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test0);

	J9PortLibraryVersion libraryVersion;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Some operations are always present, even if the port library does not
	 * support file systems.  Verify these first
	 */

	/* Verify that the file function pointers are non NULL */

	if (FALSE == isAsync) {
		/* j9file_test1 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_write) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_write is NULL\n");
		}

		/* j9file_test7, j9file_test8 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_read) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_read is NULL\n");
		}

		/* j9file_test5, j9file_test6 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_open) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_open is NULL\n");
		}

		/* j9file_test5, j9file_test6 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_close) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_close is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_lock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_lock_bytes is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_unlock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_unlock_bytes is NULL\n");
		}

		/* j9file_test9 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_set_length) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_set_length is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_flength) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_flength is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_fstat) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_fstat is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality.
		 * Startup is private to the portlibrary, it is not re-entrant safe
		 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_startup) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_startup is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_shutdown) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_shutdown is NULL\n");
		}

	} else {
		/* j9file_test1 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_write) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_write is NULL\n");
		}

		/* j9file_test7, j9file_test8 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_read) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_read is NULL\n");
		}

		/* j9file_test5, j9file_test6 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_open) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_open is NULL\n");
		}

		/* j9file_test5, j9file_test6 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_close) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_close is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_lock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_lock_bytes is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_unlock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_unlock_bytes is NULL\n");
		}

		/* j9file_test9 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_set_length) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_set_length is NULL\n");
		}

		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_flength) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_flength is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality.
		 * Startup is private to the portlibrary, it is not re-entrant safe
		 */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_startup) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_startup is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality */
		if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_blockingasync_shutdown) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_shutdown is NULL\n");
		}
	}

	/* j9file_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_write_text) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_write_text is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_get_text_encoding) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_get_text_encoding is NULL\n");
	}

	/* j9file_test3 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_vprintf is NULL\n");
	}

	/* j9file_test17 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_printf is NULL\n");
	}

	/* j9file_test10 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_seek) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_seek is NULL\n");
	}

	/* j9file_test11, j9file_test12 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_unlink) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_unlink is NULL\n");
	}

	/* j9file_test4 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_attr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_attr is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_chmod) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_chmod is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_chown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_chown is NULL\n");
	}

	/* j9file_test9 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_lastmod) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_lastmod is NULL\n");
	}

	/* j9file_test9 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_length) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_length is NULL\n");
	}

	/* j9file_test15 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_sync) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_sync is NULL\n");
	}

	/* j9file_test25 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_stat) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_stat is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_stat_filesystem) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_stat_filesystem is NULL\n");
	}

	/* The rest of the file system operations are optional based on the capability mask.
	 * Need version information to determine if optional slots are populated.
	 * Fail test if can't get this information, could be a false error, too bad.
	 */
	rc = j9port_getVersion(portLibrary, &libraryVersion);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion returned %d expected %d\n", rc, 0);
		goto exit;
	}

	if (0 != (libraryVersion.capabilities & J9PORT_CAPABILITY_STANDARD)) {
		standardConfiguration = 1;
	} else {
		standardConfiguration = 0;
		goto exit; /* done */
	}

	/* functions  available with standard configuration */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_read_text) { /* TODO j9filetext.c */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_read_text is NULL\n");
	}

	/* j9file_test11 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_mkdir) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_mkdir is NULL\n");
	}

	/* j9file_test13 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_move) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_move is NULL\n");
	}

	/* j9file_test11 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_unlinkdir) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_unlinkdir is NULL\n");
	}

	/* j9file_test14 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_findfirst) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_findfirst is NULL\n");
	}

	/* j9file_test14 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_findnext) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_findnext is NULL\n");
	}

	/* j9file_test14 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_findclose) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_findclose is NULL\n");
	}

	/* j9file_test16 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_error_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_error_message is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->file_convert_native_fd_to_omrfile_fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_convert_native_fd_to_omrfile_fd is NULL\n");
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * @ref j9file.c::j9file_write "j9file_write()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test1(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char* testName = APPEND_ASYNC(j9file_test1);

	reportTestEntry(portLibrary, testName);

	/* TODO implement */

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * @ref j9file.c::j9file_write_text "j9file_write_text()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test2(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test2);

	reportTestEntry(portLibrary, testName);

	/* TODO implement - belongs in j9filetextTest.c */

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * @ref j9file.c::j9file_vprintf "j9file_vprintf()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test3(struct J9PortLibrary *portLibrary)
{
	/* first we need a file to copy use with vprintf */
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test3);

	I_32 rc;
	char *file1name = "tfileTest3";
	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;
	IDATA fd1;

	reportTestEntry(portLibrary, testName);

	fd1 = FILE_OPEN_FUNCTION(portLibrary, file1name, EsOpenCreate | EsOpenWrite, 0666);

	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	fileWrite(portLibrary, fd1,"Try a number - %d",1);
	fileWrite(portLibrary, fd1,"Try a string - %s","abc");
	fileWrite(portLibrary, fd1,"Try a mixture - %d %s %d",1,"abc",2);

	/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	fd1 = FILE_OPEN_FUNCTION(portLibrary, file1name, EsOpenRead, 0444);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
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

/* j9file_read_text(): translation from IBM1047 to ASCII is done on zOS */
	if (strcmp(readBufPtr, expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd1);
	j9file_unlink(file1name);
	return reportTestExit(portLibrary, testName);
}

/**
 */
void
openFlagTest(struct J9PortLibrary *portLibrary, const char *testName, const char* pathName, I_32 flags, I_32 mode)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA fd;
	J9FileStat buf;
	I_32 rc;

	/* The file should not exist */
	rc = j9file_stat(pathName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, -1);
	}

	/* Open, should succeed */
	fd = FILE_OPEN_FUNCTION(portLibrary, pathName, flags, mode);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
	}
	/* Verify the filesystem sees it */
	rc = j9file_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the file, should succeed */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}
	/* The file should still exist */
	rc = j9file_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Open again, should succeed */
	fd = FILE_OPEN_FUNCTION(portLibrary, pathName, flags, mode);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
	}
	/* The file should still exist */
	rc = j9file_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the file references, should succeed */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Delete this file, should succeed */
	rc = j9file_unlink(pathName);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected %d\n", rc, -1);
	}
}

/**
 * Verify port file system.
 *
 * Verify @ref j9file.c::j9file_attr "j9file_attr()" correctly
 * determines if files and directories exist.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] fileName a valid file to find
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test4(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test4);

	const char *localFilename = "tfileTest4.tst";
	char *knownFile = NULL;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	if ( 0 == j9sysinfo_get_executable_name("java.properties", &knownFile) ) {
		/* NULL name */
		rc = j9file_attr(NULL);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_attr(NULL) returned %d expected %d\n", rc, -1);
		}

		/* Query a file that should not be there */
		rc = j9file_attr(localFilename);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_attr(%s) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* Still should not be there */
		rc = j9file_attr(localFilename);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_attr(%s) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* A directory we know is there, how about the one we are in? */
		rc = j9file_attr(".");
		if (EsIsDir != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_attr(%s) returned %d expected %d\n", ".", rc, EsIsDir);
		}

		/* Query a file that should be there  */
		rc = j9file_attr(knownFile);
		if (EsIsFile != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_attr(%s) returned %d expected %d\n", knownFile, rc, EsIsFile);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"java.properties\", &knownFile) failed\n");
	}

	/* Don't delete executable name (knownFile); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 *
 * Very basic test for open, close, remove functionality.  Does not
 * exhaustively test any of these functions.  It does, however, give an
 * indication if the tests following that exhaustively test operations
 * will pass or fail.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test5(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test5);

	const char *fileName = "tfileTest5.tst";
	const char *dirName = "tdirTest5";
	IDATA fd;
	J9FileStat buf;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* There should be no directory or file named above, they are not that usual */
	rc = j9file_stat(dirName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}
	rc = j9file_stat(fileName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* try opening a "new" file, expect success. Requires one of read/write attributes too */
	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s(\"%s\") returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, fileName, -1);
		goto exit;
	}

	/* Close the file, expect success */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Create the directory, expect success */
	rc = j9file_mkdir(dirName);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* Remove the directory, expect success */
	rc = j9file_unlinkdir(dirName);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* Remove the file, expect success */
	rc = j9file_unlink(fileName);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected %d\n", rc, 0);
	}

	/* The file and directory should now be gone */
	rc = j9file_stat(dirName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}
	rc = j9file_stat(fileName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* try to open a file with no name, should fail */
	fd = FILE_OPEN_FUNCTION(portLibrary, NULL, EsOpenWrite | EsOpenCreate, 0);
	if (-1 != fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}

	/* try to close a nonexistent file, should fail */
	rc = FILE_CLOSE_FUNCTION(portLibrary, -1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, -1);
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 *
 * Open files in the various @ref j9file.c::PortFileOperations "modes" supported
 * by the port library file operations.  The two functions
 * @ref j9file.c::j9file_open "j9file_open()" and
 * @ref j9file.c::j9file_close "j9file_close()"
 * are verified by this test.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test6(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test6);

	const char *path1 = "tfileTest6a.tst";
	const char *path2 = "tfileTest6b.tst";
	const char *path3 = "tfileTest6c.tst";
	IDATA fd1, fd2;
	J9FileStat buf;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* See if the file exists, it shouldn't */
	rc = j9file_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d expected %d\n", rc, -1);
	}
	/* Create a new file, will fail if the file exists.  Expect success */
	fd1 = FILE_OPEN_FUNCTION(portLibrary, path1, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}
	/* Verify the filesystem sees it */
	rc = j9file_attr(path1);
	if (EsIsFile != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_attr() returned %d expected %d\n", rc, EsIsFile);
	}
	/* Try and create this file again, expect failure */
	fd2 = FILE_OPEN_FUNCTION(portLibrary, path1, EsOpenCreateNew, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	/* Verify the filesystem sees it */
	rc = j9file_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}
	/* Close the invalid file handle (from second attempt), expect failure */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd2);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, -1);
	}
	/* The file should still exist */
	rc = j9file_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}
	/* Close the valid file handle (from first attempt), expect success */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}
	/* The file should still exist */
	rc = j9file_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/*
	 * Testing EsOpenCreateNew Flag - if the file already exist (for which path1 is)
	 * and we specified EsOpenCreateNew, it should failed and
	 * return error number J9PORT_ERROR_FILE_EXIST
	 */
	fd1 = FILE_OPEN_FUNCTION(portLibrary, path1, EsOpenCreateNew | EsOpenWrite, 0666);
	if (-1 != fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
		FILE_CLOSE_FUNCTION(portLibrary, fd1);
	}
/* TODO once all platforms are ready
	if(rc != J9PORT_ERROR_FILE_EXIST) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open() returned %d expected %d\n", rc, J9PORT_ERROR_FILE_EXIST);
	}
*/
	/* Finally delete this file */
	rc = j9file_unlink(path1);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected %d\n", rc, 0);
	}
	/* The file should not exist */
	rc = j9file_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, -1);
	}

	/* Series of expected failures, the file does not exist, so can't open it
	 * for read/write truncate etc
	 */
	fd2 = FILE_OPEN_FUNCTION(portLibrary, path2, EsOpenRead, 0444);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(portLibrary, path2, EsOpenWrite, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(portLibrary, path2, EsOpenTruncate, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(portLibrary, path2, EsOpenText, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(portLibrary, path2, EsOpenAppend, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
/* TODO
 * Different behaviour on windows/UNIX.  Which is right?  Unix states must be one of read/write/execute
 *
	fd2 = j9file_open(path2, EsOpenCreate, 0444);
	j9file_close(fd2);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_open() returned valid file handle expected -1\n");
	}
 */

	/* The file should not exist */
	rc = j9file_stat(path2, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, -1);
	}

	/* flags = EsOpenCreate, requires one of Write/Read */
	openFlagTest(portLibrary, testName, path3, EsOpenCreate | EsOpenWrite, 0666);

	/* flags = EsOpenWrite */
	openFlagTest(portLibrary, testName, path3, EsOpenWrite | EsOpenCreate, 0666);

	/* flags = EsOpenTruncate */
/*
	openFlagTest(portLibrary, testName, path3, EsOpenTruncate | EsOpenCreate);
*/

	/* flags = EsOpenAppend, needs read/write and create */
	openFlagTest(portLibrary, testName, path3, EsOpenAppend | EsOpenCreate| EsOpenWrite, 0666);

	/* flags = EsOpenText */
	openFlagTest(portLibrary, testName, path3, EsOpenText | EsOpenCreate | EsOpenWrite, 0666);

	/* flags = EsOpenRead */
	openFlagTest(portLibrary, testName, path3, EsOpenRead | EsOpenCreate, 0444);

	/* Cleanup, delete this file */
	FILE_CLOSE_FUNCTION(portLibrary, fd1);
	j9file_unlink(path1);
	rc = j9file_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, -1);
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd2);
	j9file_unlink(path2);
	rc = j9file_stat(path2, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, -1);
	}

	rc = j9file_stat(path3, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d, expected %d\n", rc, -1);
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 *
 * Verify basic
 * @ref j9file.c::j9file_read "j9file_read()" and
 * @ref j9file::j9file_write "j9file_write()"
 * operations
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test7(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test7);

	I_32 expectedReturnValue;
	IDATA rc;
	const char *fileName = "tfileTest7.tst";
	char inputLine[] = "Input from test7";
	char outputLine[128];
	IDATA fd;

	reportTestEntry(portLibrary, testName);

	/* open "new" file - if file exists then should still work */
	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenWrite | EsOpenCreate, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* write to it */
	expectedReturnValue = sizeof(inputLine);
	rc = FILE_WRITE_FUNCTION(portLibrary, fd, inputLine, expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc ,0);
		goto exit;
	}

	/* a sneaky test to see that the fd is now useless */
	rc = FILE_WRITE_FUNCTION(portLibrary, fd, inputLine, expectedReturnValue);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenRead, 0444);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* lets read back what we wrote */
	rc = FILE_READ_FUNCTION(portLibrary, fd, outputLine, 128);
	if (rc != expectedReturnValue)	{
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d, expected %d\n", FILE_READ_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	if (strcmp(outputLine,inputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", outputLine, inputLine);
		goto exit;
	}

	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 *
 * Verify
 * @ref j9file.c::j9file_read "j9file_read()" and
 * @ref j9file::j9file_write "j9file_write()"
 * append operations.
 *
 * This test creates a file, writes ABCDE to it then closes it - reopens it for append and
 * writes FGHIJ to it, fileseeks to start of file and reads 10 chars and hopefully this matches
 * ABCDEFGHIJ ....
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test8);
	IDATA expectedReturnValue;
	IDATA rc;
	const char *fileName = "tfileTest8.tst";
	char inputLine1[] = "ABCDE";
	char inputLine2[] = "FGHIJ";
	char outputLine[] = "0123456789";
	char expectedOutput[] = "ABCDEFGHIJ";
	IDATA fd;
	I_64 filePtr = 0;
	I_64 prevFilePtr = 0;

	reportTestEntry(portLibrary, testName);
	expectedReturnValue = sizeof(inputLine1)-1; /* don't want the final 0x00 */

	/*lets try opening a "new" file - if file exists then should still work */
	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	prevFilePtr = j9file_seek (fd, 0, EsSeekCur);
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening file. Expected = 0, Got = %lld", prevFilePtr);
	}

	/* now lets write to it */
	rc = FILE_WRITE_FUNCTION(portLibrary, fd, inputLine1, expectedReturnValue);

	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = j9file_seek (fd, 0, EsSeekCur);
	if (prevFilePtr + expectedReturnValue != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after writing into file. Expected = %lld, Got = %lld", prevFilePtr + expectedReturnValue,  filePtr);
	}

	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenWrite | EsOpenAppend, 0666);

	prevFilePtr = j9file_seek (fd, 0, EsSeekCur);
#if defined (WIN32) | defined (WIN64)
	 /* Windows sets the file pointer to the end of file as soon as it is opened in append mode. */
	if (5 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening the file. Expected = 5, Got = %lld", prevFilePtr);
	}
#else
	 /* Other platforms set the file pointer to the start of file when it is opened in append mode and update the file pointer to the end only when it is needed in the next call. */
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening the file. Expected = 0, Got = %lld", prevFilePtr);
	}
#endif

	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* now lets write to it */
	expectedReturnValue = sizeof(inputLine2)-1; /* don't want the final 0x00 */
	rc = FILE_WRITE_FUNCTION(portLibrary, fd, inputLine2, expectedReturnValue);

	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = j9file_seek (fd, 0, EsSeekCur);
	if (10 != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after writing into file. Expected = 10, Got = %lld", filePtr);
	}

	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenRead, 0444);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	prevFilePtr = j9file_seek (fd, 0, EsSeekCur);
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening the file. Expected = 0, Got = %lld", prevFilePtr);
	}

	/* lets read back what we wrote */
	expectedReturnValue = 10;
	rc = FILE_READ_FUNCTION(portLibrary, fd, outputLine, expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d, expected %d\n", FILE_READ_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = j9file_seek (fd, 0, EsSeekCur);
	if (prevFilePtr + expectedReturnValue != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after reading from file. Expected = %lld, Got = %lld", prevFilePtr + expectedReturnValue, filePtr);
	}

	if (strcmp(expectedOutput, outputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching expected data \"%s\"\n", outputLine, expectedOutput);
		goto exit;
	}

	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}


/**
 * Verify port file system.
 * @ref j9file.c::j9file_lastmod "j9file_lastmod()"
 * @ref j9file.c::j9file_length "j9file_length()"
 * @ref j9file.c::j9file_set_length "j9file_set_length()"
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test9(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test9);

	const char *fileName = "tfileTest9.tst";
	I_64 lastmod=0;
	I_64 lastmodPrev=-1;
	IDATA fd;
	I_64 length, savedLength, newLength;
	IDATA rc, i;
	IDATA numberIterations, timesEqual, timesLess;

	reportTestEntry(portLibrary, testName);

	/* Fast file systems may report time equal or before the previous value
	 * Take a good sampling.  If timeEqual + timesLess != numberIterations then
	 * time does advance, and lastmod is returning changing values
	 */
	numberIterations = 100;
	timesEqual = 0;
	timesLess = 0;
	for (i=0;i<numberIterations;i++) {
		lastmod = j9file_lastmod(fileName);

		if (i != 0) {
			if (-1 == lastmod) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_lastmod() returned -1, iteration %d\n", i+1);
				goto exit;
			}
		} else {
			lastmod = 0;
			lastmodPrev = lastmod;
		}

		fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
		if (-1 == fd) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned -1 expected valid file handle iteration %d\n", FILE_OPEN_FUNCTION_NAME, i+1);
			goto exit;
		}

		/* make sure we are actually writing something to the file */
		length = j9file_length(fileName);
		if (length != i) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %d\n", length, i);
			goto exit;
		}

		if (length > 2) {
			/* Truncate file by 1 */
			savedLength = length;
			newLength = FILE_SET_LENGTH_FUNCTION(portLibrary, fd, length-1);
			if (0 != newLength) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, newLength, 0);
				goto exit;
			}

			/* Verify see correct length */
			length = j9file_length(fileName);
			if (length != (savedLength-1)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %lld\n", length, savedLength-1);
				goto exit;
			}

			/* Restore length */
			newLength = FILE_SET_LENGTH_FUNCTION(portLibrary, fd, savedLength);
			if (0 != newLength) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, newLength, 0);
				goto exit;
			}
			length = j9file_length(fileName);
			if (length != savedLength) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %lld\n", length, savedLength);
				goto exit;
			}
		}

		/* Write something to the file and close it */
		rc = FILE_WRITE_FUNCTION(portLibrary, fd, "a", 1);
		if (1 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 1, iteration %d\n", FILE_WRITE_FUNCTION_NAME, rc, i+1);
			goto exit;
		}
 		rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
 		if (0 != rc) {
 			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d, iteration %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0, i+1);
 			goto exit;
 		}

 		/* lastmod should have advanced due to the delay */
		lastmod = j9file_lastmod(fileName);
		if (-1 == lastmod) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_lastmod() returned %d expected %d, iteration %d\n", lastmod, -1, i+1);
			goto exit;
		}

		/* Time advancing? Account for some slow IO devices */
		if (lastmod == lastmodPrev) {
			timesEqual++;
		} else if (lastmod < lastmodPrev) {
			timesLess++;
		}

		/* do it again */
		lastmodPrev = lastmod;
	}

	/* Was time advancing? */
	if (numberIterations == (timesEqual+timesLess)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_lastmod() failed time did not advance\n");
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * @ref j9file.c::j9file_seek "j9file_seek()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test10(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test10);

	IDATA expectedReturnValue;
	IDATA rc;
	const char *fileName = "tfileTest10.tst";
	char inputLine1[] = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
	char inputLine2[] = "j9WasHere";
	IDATA fd;
	I_64 resultingOffset;
	I_64 expectedOffset;
	I_64 offset;
	I_64 fileLength;
	I_64 expectedFileLength = 81;

	reportTestEntry(portLibrary, testName);

	/* Open new file, will overwrite.  ESOpenTruncate will collapse file
	 */
	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}
	j9file_sync(fd); /* need this to ensure that length matches what we expect */

	/* now lets look at file length - should always be 0 */
	fileLength = j9file_length(fileName);
	if (0 != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected 0\n", fileLength);
		goto exit;
	}

	/* now lets write to it */
	expectedReturnValue = sizeof(inputLine1);
	rc = FILE_WRITE_FUNCTION(portLibrary, fd,inputLine1,expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	/* leave file close and reopen without truncate option (or create) */
	/* close the file */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenWrite, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* lets find current offset */
	offset = 0;
	expectedOffset = 0;
	resultingOffset = j9file_seek(fd,offset,EsSeekCur);
	if (resultingOffset != expectedOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_seek() returned %lld expected %lld\n", resultingOffset, expectedOffset);
		goto exit;
	}

	/* lets seek to pos 40 in file */
	offset = 40;
	expectedOffset = 40;
	resultingOffset = j9file_seek(fd,offset,EsSeekSet);
	if (resultingOffset != expectedOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_seek() returned %lld expected %lld\n", resultingOffset, expectedOffset);
		goto exit;
	}

	/* ok now lets write something - should overwrite total file length should remain 81 */
	expectedReturnValue = sizeof(inputLine2);
	rc = FILE_WRITE_FUNCTION(portLibrary, fd,inputLine2,expectedReturnValue);
	j9file_sync(fd); /* need this to ensure that length matches what we expect */
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	/* now lets look at file length */
	fileLength = j9file_length(fileName);
	if (fileLength != expectedFileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %lld\n", fileLength, expectedFileLength);
		goto exit;
	}

	/* lets try seeking beyond end of file - this should be allowed however the
	 * length of the file will remain untouched until we do a write
	 */
	resultingOffset = j9file_seek(fd,fileLength+5,EsSeekSet);
	if (resultingOffset != fileLength+5) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_seek() returned %lld expected %lld\n", resultingOffset, fileLength+5);
		goto exit;
	}
	j9file_sync(fd); /* need this to ensure that length matches what we expect */

	/* now lets look at file length */
	fileLength = j9file_length(fileName);
	if (fileLength != expectedFileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %lld\n", fileLength, expectedFileLength);
		goto exit;
	}

	/* Now lets write to the file - expecting some nulls to be inserted and
	 * file length to change appropriately
	 */
	rc = FILE_WRITE_FUNCTION(portLibrary, fd,"X",1); /* Note this should just do 1 character X*/
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 1\n", FILE_WRITE_FUNCTION_NAME, rc);
		goto exit;
	}
	j9file_sync(fd); /* need this to ensure that length matches what we expect */

	/* now lets look at file length - should be 87 */
	expectedFileLength += 6; /* we seeked 5 past end and have written 1 more */
	fileLength = j9file_length(fileName);
	if (fileLength != expectedFileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %lld\n", fileLength, expectedFileLength);
		goto exit;
	}

	/* lets seek to pos 40 in file */
	offset = 40;
	expectedOffset = 40;
	resultingOffset = j9file_seek(fd,offset,EsSeekSet);
	if (resultingOffset != expectedOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_seek() returned %lld expected %lld\n", resultingOffset, expectedOffset);
		goto exit;
	}

	/* lets try seeking to a -ve offset */
	resultingOffset = j9file_seek(fd,-3,EsSeekSet);
	if (-1 != resultingOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_seek() returned %lld expected -1\n", resultingOffset);
		goto exit;
	}

	/* lets see that current offset is still 40 */
	resultingOffset = j9file_seek(fd,0,EsSeekCur);
	if (40 != resultingOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_seek() returned %lld expected 40\n", resultingOffset);
		goto exit;
	}

	/* close the file */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 *
 * Create directories @ref j9file.c::j9file_mkdir "j9file_mkdir() and
 * delete them @ref j9file.c::j9file_unlinkdir "j9file_unlinkdir()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test11(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test11);

	char *dir2name="tsubdirTest11";
	char dir1[]="tdirTest11";
	char dir2[128];
	J9FileStat buf;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	memset(dir2,0,sizeof(dir2));
	strcat(dir2,dir1);
	strcat(dir2,DIR_SEPARATOR_STR);
	strcat(dir2,dir2name);

	/* remove the directory.  don't verify if it isn't there then a failure
	 * is returned, if it is there then success is returned
	 */
	j9file_unlinkdir(dir1);

	/* Create the directory, expect success */
	rc = j9file_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, 0);
	}

 	/* Make sure it is there */
	rc = j9file_stat(dir1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d expected %d\n", rc, 0);
	} else if (buf.isDir != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isDir = %d, expected %d\n", buf.isDir, 1);
	}

	/* Create the directory again, expect failure */
	rc = j9file_mkdir(dir1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, -1);
	}

	/* Remove the directory, expect success */
	rc = j9file_unlinkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected 0\n", rc);
		goto exit;
	}

	/* OK basic test worked
	 * Try putting a file in the directory (and using DIR_SEPARATOR_STR)
	 * first remove the directory in case it is there, don't check for failure/success
	 */
	j9file_unlinkdir(dir2);

	/* Create the directory, expect failure (assumes all parts of path up to the
	 * last component are there)
	 */
	rc = j9file_mkdir(dir2);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, -1);
		goto exit;
	}

	/* Create the first part of the directory name, expect success */
	rc = j9file_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* Create the second part of the name, expect success */
	rc = j9file_mkdir(dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* Does the file system see it? */
	rc = j9file_stat(dir2, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() returned %d expected %d\n", rc, 0);
	} else if (buf.isDir != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() set buf.isDir = %d, expected %d\n", buf.isDir, 1);
	}

	/* Remove this directory, expect failure (too high) */
	rc = j9file_unlinkdir(dir1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected %d\n", rc, -1);
		goto exit;
	}

 	/* Remove this directory, expect success */
	rc = j9file_unlinkdir(dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

 	/* Remove this directory, expect success */
	rc = j9file_unlinkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

exit:
	j9file_unlinkdir(dir2);
	j9file_unlinkdir(dir1);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify file
 * @ref j9file.c::j9file_unlink "j9file_unlink()" and
 * @ref j9file.c::j9file_unlinkdir "j9file_unlinkdir()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9file_test12(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test12);

	I_32 rc;
	I_32 attrRc=0;
	IDATA fd1 = -1;
	IDATA fd2 = -1;
	char dir1[]="tdirTest12";
	char pathName1[128];
	char pathName2[128];
	char *file1name = "tfileTest12a.tst";
	char *file2name = "tfileTest12b.tst";

	reportTestEntry(portLibrary, testName);

	memset(pathName1,0,sizeof(pathName1));
	strcat(pathName1,dir1);
	strcat(pathName1,DIR_SEPARATOR_STR);
	strcat(pathName1,file1name);

	memset(pathName2,0,sizeof(pathName2));
	strcat(pathName2,dir1);
	strcat(pathName2,DIR_SEPARATOR_STR);
	strcat(pathName2,file2name);

	/* create a directory */
	rc = j9file_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected 0\n", rc);
		goto exit;
	}

	/* create some files within directory */
	fd1 = FILE_OPEN_FUNCTION(portLibrary, pathName1, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	fd2 = FILE_OPEN_FUNCTION(portLibrary, pathName2, EsOpenCreate | EsOpenWrite ,0666);
	if (-1 == fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	/* try unlinking a nonexistent file */
	rc = j9file_unlink("rubbish");
	if (-1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected -1\n", rc);
		goto exit;
	}

	/* Unix allows unlink of open files, windows doesn't */

	/* try unlink of closed file */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	rc = j9file_unlink(pathName1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected 0\n", rc);
		goto exit;
	}

	/* try open of file just unlinked: Don't use helper as expect failure */
	fd1 = FILE_OPEN_FUNCTION(portLibrary, pathName1, EsOpenWrite ,0666);
	if (-1 != fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	/* try unlinkdir of nonexistent directory */
	rc = j9file_unlinkdir("rubbish");
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected -1\n", rc);
		goto exit;
	}

	/* try unlinkdir on directory with files in it */
	rc = j9file_unlinkdir(dir1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected -1\n", rc);
		goto exit;
	}

	/* delete file from dir and retry unlinkdir */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	rc = j9file_unlink(pathName2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected 0\n", rc);
		goto exit;
	}

	rc = j9file_unlinkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir() returned %d expected 0\n", rc);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd1);
	FILE_CLOSE_FUNCTION(portLibrary, fd2);
	j9file_unlink(pathName1);
	j9file_unlink(pathName2);
	j9file_unlinkdir(dir1);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify file move operation
 * @ref j9file.c::j9file_move "j9file_move()"
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9file_test13(struct J9PortLibrary *portLibrary)
{
	/* This case tests ability to create a directory/file and rename it */
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test13);

	I_32 rc;
	IDATA fd1 = -1;
	char dir1[]="tdirTest13a";
	char dir2[]="tdirTest13b";
	char pathName1[128];
	char pathName2[128];
	char *file1name = "tfileTest13a.tst";
	char *file2name = "tfileTest13b.tst";

	reportTestEntry(portLibrary, testName);

	memset(pathName1,0,sizeof(pathName1));
	strcat(pathName1,dir2);
	strcat(pathName1,DIR_SEPARATOR_STR);
	strcat(pathName1,file1name);

	memset(pathName2,0,sizeof(pathName2));
	strcat(pathName2,dir2);
	strcat(pathName2,DIR_SEPARATOR_STR);
	strcat(pathName2,file2name);

	/* create a directory */
	rc = j9file_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* now rename it */
	rc = j9file_move(dir1,dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_move() from %s to %s returned %d, expected %d\n", dir1, dir2, rc, 0);
		goto exit;
	}

	/* create a file within the directory */
	fd1 = FILE_OPEN_FUNCTION(portLibrary, pathName1, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* so close it and try again */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd1); /* should have 0 length file */
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	rc = j9file_move(pathName1,pathName2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_move() from %s to %s returned %d, expected %d\n", pathName1, pathName2, rc, 0);
		goto exit;
	}

	/* could add in some stuff to check that file exists..... */

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd1);
	j9file_unlink(pathName1);
	j9file_unlink(pathName2);
	j9file_unlinkdir(dir1);
	j9file_unlinkdir(dir2);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * @ref j9file.c::j9file_findfirst "j9file_findfirst()"
 * @ref j9file.c::j9file_findnext "j9file_findnext()"
 * @ref j9file.c::j9file_findclose "j9file_findclose()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test14(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test14);

	IDATA rc;
	IDATA expectedReturnValue;
	I_32 attrRc=0;
	IDATA fd1 = -1;
	IDATA fd2 = -1;
	UDATA handle;

	char *dir2name="tdirTest14a";
	char dir1[]="tdirTest14b";
	char dir2[128];
	char resultbuffer[128];
	char pathName1[128];
	char pathName2[128];
	char *file1name = "tfileTest14a.tst";
	char *file2name = "tfileTest14b.tst";
	char search[128];
	I_32 count=0;
	I_32 expectedCount=3; /* dont forget that the tdirTest14a inside tdirTest14b also counts for the search */
	I_32 fileCount=0;
	I_32 expectedFileCount=2;
	I_32 dirCount=0;
	I_32 expectedDirCount=1;
	char fullname[128]="";

	reportTestEntry(portLibrary, testName);
	memset(dir2,0,sizeof(dir2));
	strcat(dir2,dir1);
	strcat(dir2,DIR_SEPARATOR_STR);
	strcat(dir2,dir2name);

	memset(pathName1,0,sizeof(pathName1));
	strcat(pathName1,dir1);
	strcat(pathName1,DIR_SEPARATOR_STR);
	strcat(pathName1,file1name);

	memset(pathName2,0,sizeof(pathName2));
	strcat(pathName2,dir1);
	strcat(pathName2,DIR_SEPARATOR_STR);
	strcat(pathName2,file2name);

	memset(search,0,sizeof(search));
	strcat(search,dir1);
	strcat(search,DIR_SEPARATOR_STR);
	strcat(search,"t*Test14*");

	rc = j9file_mkdir(dir1);
	if (0!= rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	rc = j9file_mkdir(dir2);
	if (0!= rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* try opening a "new" file - if file exists then should still work */
	fd1 = FILE_OPEN_FUNCTION(portLibrary, pathName1, EsOpenWrite | EsOpenCreate, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* try opening a "new" file - if file exists then should still work */
	fd2 = FILE_OPEN_FUNCTION(portLibrary, pathName2, EsOpenWrite | EsOpenCreate, 0666);
	if (-1 == fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* write to them */
	expectedReturnValue = sizeof("test");
	rc = FILE_WRITE_FUNCTION(portLibrary, fd1, "test", expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	rc = FILE_WRITE_FUNCTION(portLibrary, fd2, "test", expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	/* close this one */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	/* Now we can test findfirst etc..... */
	handle = j9file_findfirst("gobbledygook*", resultbuffer);
	if (-1 != handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_findfirst() returned valid handle expected -1\n");
		goto exit;
	}

/*
 * TODO: different behaviour on windows/Linux
 *
	handle = j9file_findfirst(search, resultbuffer);
	if (-1 == handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_findfirst() returned -1 expected a valid handle\n");
		goto exit;
	}

	memset(fullname,0,sizeof(fullname));
	strcat(fullname,dir1);
	strcat(fullname,DIR_SEPARATOR_STR);
	strcat(fullname,resultbuffer);
	attrRc = j9file_attr(fullname);
	if (EsIsFile == attrRc) {
		fileCount++;
	}
	if (EsIsDir == attrRc) {
		dirCount++;
	}

	rc = 0;
	while (rc != -1) {
		count ++;
		rc = j9file_findnext(handle, resultbuffer);
		if (rc >= 0) {
			memset(fullname,0,sizeof(fullname));
			strcat(fullname,dir1);
			strcat(fullname,DIR_SEPARATOR_STR);
			strcat(fullname,resultbuffer);
			attrRc = j9file_attr(fullname);

			if (EsIsFile == attrRc) {
				fileCount++;
			}
			if (EsIsDir == attrRc) {
				dirCount++;
			}
		}
	}

 	if (fileCount != expectedFileCount)	{
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file count incorrrect, found %d expected %d\n", fileCount, expectedFileCount);
		goto exit;
	}

	if (dirCount != expectedDirCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "directory count incorrect, found %d expected %d\n", dirCount, expectedDirCount);
		goto exit;
	}

	if (count != expectedCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "count incorrect, found %d expected %d\n", count, expectedCount);
		goto exit;
	}
	j9file_findclose(handle);
 *
 */

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd1);
	FILE_CLOSE_FUNCTION(portLibrary, fd2);
	j9file_unlink(pathName1);
	j9file_unlink(pathName2);
	j9file_unlinkdir(dir2);
	j9file_unlinkdir(dir1);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * @ref j9file.c::j9file_sync "j9file_sync()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test15(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test15);

	const char* fileName = "tfileTest15.tst";
	I_64 lastmod=0;
	I_64 lastmodPrev=-1;
	IDATA fd;
	I_64 length;
	IDATA rc, i;
	IDATA numberIterations, timesEqual, timesLess;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned -1 expected valid file handle\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	/* Fast file systems may report time equal or before the previous value
	 * Take a good sampling.  If timeEqual + timesLess != numberIterations then
	 * time does advance, and lastmod is returning changing values
	 */
	numberIterations = 100;
	timesEqual = 0;
	timesLess = 0;
	for (i=0;i<numberIterations;i++) {
		lastmod = j9file_lastmod(fileName);
	 	if (0 != i) {
			if (-1 == lastmod) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_lastmod() returned -1, iteration %d\n", i+1);
				goto exit;
			}
		} else {
			lastmod = 0;
			lastmodPrev = lastmod;
		}

		/* make sure we are actually writing something to the file */
		length = j9file_length(fileName);
		if (length != i) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %d\n", length, i);
			goto exit;
		}

		/* Write something to the file and close it */
		rc = FILE_WRITE_FUNCTION(portLibrary, fd, "a", 1);
		if (1 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 1, iteration %d\n", FILE_WRITE_FUNCTION_NAME, rc, i+1);
			goto exit;
		}

		rc = j9file_sync(fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_sync() returned %d expected %d, iteration %d\n", rc, 0, i+1);
			goto exit;
		}

 		/* lastmod should have advanced due to the delay */
		lastmod = j9file_lastmod(fileName);
		if (-1 == lastmod) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_lastmod() returned %d expected %d, iteration %d\n", lastmod, -1, i+1);
			goto exit;
		}

		/* Time advancing? Account for some slow IO devices */
		if (lastmod == lastmodPrev) {
			timesEqual++;
		} else if (lastmod < lastmodPrev) {
			timesLess++;
		}

		/* do it again */
		lastmodPrev = lastmod;
	}

	/* Was time always the same? */
	if (numberIterations == (timesEqual+timesLess)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_lastmod() failed time did not advance\n");
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port file system.
 * Verify @ref j9file.c::j9file_error_message "j9file_error_message()".
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test16(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test16);

	reportTestEntry(portLibrary, testName);

	/* TODO implement */

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify file printf function.
 * Verify @ref j9file.c::j9file_printf "j9file_printf()"
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test17(struct J9PortLibrary *portLibrary)
{
	/* first we need a file to copy use with vprintf */
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test17);

	I_32 rc;
	char *file1name = "tfileTest17";
	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;
	IDATA fd1;

	reportTestEntry(portLibrary, testName);

	fd1 = FILE_OPEN_FUNCTION(portLibrary, file1name, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	j9file_printf(portLibrary, fd1, "Try a number - %d", 1);
	j9file_printf(portLibrary, fd1, "Try a string - %s", "abc");
	j9file_printf(portLibrary, fd1, "Try a mixture - %d %s %d", 1, "abc", 2);

	/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	fd1 = FILE_OPEN_FUNCTION(portLibrary, file1name, EsOpenRead, 0444);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
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

/* j9file_read_text(): translation from IBM1047 to ASCII is done on zOS */
	if (strcmp(readBufPtr, expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd1);
	j9file_unlink(file1name);
	return reportTestExit(portLibrary, testName);
}

/*
 * Helper function used by the File Locking tests below to create the base file
 * used by the tests
 *
 */
IDATA
j9file_create_file(J9PortLibrary* portLibrary, const char* filename, I_32 openMode, const char* testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA returnFd = -1;
	IDATA createFd = -1;
	char buffer[100];
	IDATA i;
	IDATA rc;

	for(i = 0; i < 100; i++) {
		buffer[i] = (char)i;
	}

	(void)j9file_unlink(filename);

	createFd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == createFd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return returnFd;
	}

	rc = j9file_write(createFd, buffer, 100);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		j9file_close(createFd);
		return returnFd;
	}

	rc = j9file_close(createFd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return returnFd;
	}

	if (0 == openMode) {
		returnFd = 0;
	} else {
		returnFd = j9file_open(filename, openMode, 0);
		if (-1 == returnFd) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s in mode %d failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, openMode, lastErrorNumber, lastErrorMessage);
			return returnFd;
		}
	}

	return returnFd;
}

/*
 * Helper function used by File Locking tests below to create a file indicating
 * that the test has reached a certain stage in its processing
 *
 * Status files are used as we do not have a cross platform inter process
 * semaphore facility on all platforms
 *
 */
IDATA
j9file_create_status_file(J9PortLibrary* portLibrary, const char* filename, const char* testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA createFd = -1;
	int rc;

	(void)j9file_unlink(filename);

	createFd = j9file_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == createFd) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return -1;
	}

	rc = j9file_close(createFd);
	if (-1 == rc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return -1;
	}

	return 0;
}

/*
 * Helper function used by File Locking tests below to check for the existence
 * of a status file from the other child in the test
 */
IDATA
j9file_status_file_exists(J9PortLibrary* portLibrary, const char * filename, const char* testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc;
	J9FileStat buf;

	rc = j9file_stat(filename, 0, &buf);
	if ((rc == 0) && (buf.isFile == 1)) {
		return 1;
	}

	return 0;
}

/*
 * File Locking Tests
 *
 * Basic test: ensure we can acquire and release a read lock
 *
 */
int
j9file_test18(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test18);
	const char* testFile = APPEND_ASYNC(j9file_test18_file);
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = 16;
	U_64 length = 4;

	reportTestEntry(portLibrary, testName);

	fd = j9file_create_file(portLibrary, testFile, EsOpenRead, testName);
	if (-1 == fd) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_READ_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_unlink(testFile);
exit:

	return reportTestExit(portLibrary, testName);
}

/*
 * File Locking Tests
 *
 * Basic test: ensure we can acquire and release a write lock
 *
 */
int
j9file_test19(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test19);
	const char* testFile = APPEND_ASYNC(j9file_test19_file);
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = 16;
	U_64 length = 4;

	reportTestEntry(portLibrary, testName);

	fd = j9file_create_file(portLibrary, testFile, EsOpenWrite, testName);
	if (-1 == fd) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	j9file_unlink(testFile);

exit:

	return reportTestExit(portLibrary, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets write lock
 *    Child 2 tries to get write lock on same bytes, fails
 *
 */
#define TEST20_OFFSET 16
#define TEST20_LENGTH 4
int
j9file_test20(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	IDATA rc;
	const char* testName = APPEND_ASYNC(j9file_test20);
	const char* testFile = "j9file_test20_file";
	const char* testFileLock = "j9file_test20_child_1_got_lock";
	const char* testFileDone = "j9file_test20_child_2_done";

	reportTestEntry(portLibrary, testName);

	/* Remove status files */
	j9file_unlink(testFileLock);
	j9file_unlink(testFileDone);

	rc = j9file_create_file(portLibrary, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	/* parent */

	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test20_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test20_2");
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
j9file_test20_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test20_child_1);
	const char* testFile = "j9file_test20_file";
	const char* testFileLock = "j9file_test20_child_1_got_lock";
	const char* testFileDone = "j9file_test20_child_2_done";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST20_OFFSET;
	U_64 length = TEST20_LENGTH;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_create_status_file(portLibrary, testFileLock, testName);

	while( !j9file_status_file_exists(portLibrary, testFileDone, testName) ) {
		SleepFor(1);
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

exit:
	return reportTestExit(portLibrary,testName);
}

int
j9file_test20_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test20_child_2);
	const char* testFile = "j9file_test20_file";
	const char* testFileLock = "j9file_test20_child_1_got_lock";
	const char* testFileDone = "j9file_test20_child_2_done";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	U_64 offset = TEST20_OFFSET;
	U_64 length = TEST20_LENGTH;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock, testName) ) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (-1 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld incorrectly succeeded: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

	j9file_create_status_file(portLibrary, testFileDone, testName);

exit:
	return reportTestExit(portLibrary,testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets write lock
 *    Child 2 tries to get write lock on adjacent bytes, succeeds
 *
 */
#define TEST21_OFFSET 16
#define TEST21_LENGTH 4
int
j9file_test21(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	IDATA rc;
	const char* testName = APPEND_ASYNC(j9file_test21);
	const char* testFile = "j9file_test21_file";
	const char* testFileLock = "j9file_test21_child_1_got_lock";
	const char* testFileDone = "j9file_test21_child_2_done";

	reportTestEntry(portLibrary, testName);

	/* Remove status files */
	j9file_unlink(testFileLock);
	j9file_unlink(testFileDone);

	rc = j9file_create_file(portLibrary, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test21_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test21_2");
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

	/* Remove status files */
	j9file_unlink(testFile);
	j9file_unlink(testFileLock);
	j9file_unlink(testFileDone);

exit:

	return reportTestExit(portLibrary, testName);
}

int
j9file_test21_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test21_child_1);
	const char* testFile = "j9file_test21_file";
	const char* testFileLock = "j9file_test21_child_1_got_lock";
	const char* testFileDone = "j9file_test21_child_2_done";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST21_OFFSET;
	U_64 length = TEST21_LENGTH;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_create_status_file(portLibrary, testFileLock, testName);

	while( !j9file_status_file_exists(portLibrary, testFileDone, testName) ) {
		SleepFor(1);
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

exit:
	return reportTestExit(portLibrary,testName);
}

int
j9file_test21_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test21_child_2);
	const char* testFile = "j9file_test21_file";
	const char* testFileDone = "j9file_test21_child_2_done";
	const char* testFileLock = "j9file_test21_child_1_got_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST21_OFFSET + TEST21_LENGTH;
	U_64 length = TEST21_LENGTH;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock, testName) ) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

	j9file_create_status_file(portLibrary, testFileDone, testName);

exit:
	return reportTestExit(portLibrary,testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets write lock
 *    Child 2 tries to get write lock on same bytes, waits
 *    Child 1 releases lock
 *    Child 2 acquires lock
 *
 */
#define TEST22_OFFSET 16
#define TEST22_LENGTH 4
int
j9file_test22(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	IDATA rc;
	const char* testName = APPEND_ASYNC(j9file_test22);
	const char* testFile = "j9file_test22_file";
	const char* testFileDone = "j9file_test22_child_2_done";
	const char* testFileLock = "j9file_test22_child_1_got_lock";
	const char* testFileTry = "j9file_test22_child_2_trying_for_lock";

	reportTestEntry(portLibrary, testName);

	/* Remove status files */
	j9file_unlink(testFileLock);
	j9file_unlink(testFileTry);
	j9file_unlink(testFileDone);

	rc = j9file_create_file(portLibrary, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test22_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test22_2");
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
j9file_test22_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test22_child_1);
	const char* testFile = "j9file_test22_file";
	const char* testFileLock = "j9file_test22_child_1_got_lock";
	const char* testFileTry = "j9file_test22_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST22_OFFSET;
	U_64 length = TEST22_LENGTH;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_create_status_file(portLibrary, testFileLock, testName);

	while( !j9file_status_file_exists(portLibrary, testFileTry, testName) ) {
		SleepFor(1);
	}

	SleepFor(5);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

exit:
	return reportTestExit(portLibrary,testName);
}

int
j9file_test22_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char* testName = APPEND_ASYNC(j9file_test22_child_2);
	const char* testFile = "j9file_test22_file";
	const char* testFileLock = "j9file_test22_child_1_got_lock";
	const char* testFileDone = "j9file_test22_child_2_done";
	const char* testFileTry = "j9file_test22_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST22_OFFSET;
	U_64 length = TEST22_LENGTH;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock, testName) ) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenWrite, 0);

	j9file_create_status_file(portLibrary, testFileTry, testName);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

	j9file_create_status_file(portLibrary, testFileDone, testName);

exit:
	return reportTestExit(portLibrary,testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets read lock
 *    Child 2 tries to get read lock on same bytes, succeeds
 *    Child 1 releases lock
 *    Child 2 acquires lock
 *
 */
#define TEST23_OFFSET 16
#define TEST23_LENGTH 4
int
j9file_test23(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	IDATA rc;
	const char* testName = APPEND_ASYNC(j9file_test23);
	const char* testFile = "j9file_test23_file";
	const char* testFileDone = "j9file_test23_child_2_done";
	const char* testFileLock1 = "j9file_test23_child_1_got_lock";
	const char* testFileLock2 = "j9file_test23_child_2_got_lock";

	reportTestEntry(portLibrary, testName);

	/* Remove status files */
	j9file_unlink(testFileLock1);
	j9file_unlink(testFileLock2);
	j9file_unlink(testFileDone);

	rc = j9file_create_file(portLibrary, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test23_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test23_2");
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

	/* Remove status files */
	j9file_unlink(testFile);
	j9file_unlink(testFileLock1);
	j9file_unlink(testFileLock2);
	j9file_unlink(testFileDone);
exit:

	return reportTestExit(portLibrary, testName);
}

int
j9file_test23_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test23_child_1);
	const char* testFile = "j9file_test23_file";
	const char* testFileLock1 = "j9file_test23_child_1_got_lock";
	const char* testFileLock2 = "j9file_test23_child_2_got_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST23_OFFSET;
	U_64 length = TEST23_LENGTH;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_READ_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_create_status_file(portLibrary, testFileLock1, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock2, testName) ) {
		SleepFor(1);
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

exit:
	return reportTestExit(portLibrary,testName);
}

int
j9file_test23_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test23_child_2);
	const char* testFile = "j9file_test23_file";
	const char* testFileDone = "j9file_test23_child_2_done";
	const char* testFileLock1 = "j9file_test23_child_1_got_lock";
	const char* testFileLock2 = "j9file_test23_child_2_got_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST23_OFFSET;
	U_64 length = TEST23_LENGTH;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock1, testName) ) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_READ_LOCK | J9PORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_create_status_file(portLibrary, testFileLock2, testName);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

	j9file_create_status_file(portLibrary, testFileDone, testName);

exit:
	return reportTestExit(portLibrary,testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets read lock
 *    Child 2 tries to get write lock on same bytes, waits
 *    Child 1 releases lock
 *    Child 2 acquires lock
 *
 */
#define TEST24_OFFSET 16
#define TEST24_LENGTH 4
int
j9file_test24(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	IDATA rc;
	const char* testName = APPEND_ASYNC(j9file_test24);
	const char* testFile = "j9file_test24_file";
	const char* testFileDone = "j9file_test24_child_2_done";
	const char* testFileLock1 = "j9file_test24_child_1_got_lock";
	const char* testFileTry = "j9file_test24_child_2_trying_for_lock";

	reportTestEntry(portLibrary, testName);

	/* Remove status files */
	j9file_unlink(testFileLock1);
	j9file_unlink(testFileTry);
	j9file_unlink(testFileDone);

	rc = j9file_create_file(portLibrary, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test24_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test24_2");
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
j9file_test24_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test24_child_1);
	const char* testFile = "j9file_test24_file";
	const char* testFileLock1 = "j9file_test24_child_1_got_lock";
	const char* testFileTry = "j9file_test24_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST24_OFFSET;
	U_64 length = TEST24_LENGTH;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_READ_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	j9file_create_status_file(portLibrary, testFileLock1, testName);

	while( !j9file_status_file_exists(portLibrary, testFileTry, testName) ) {
		SleepFor(1);
	}

	SleepFor(5);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

exit:
	return reportTestExit(portLibrary,testName);
}

int
j9file_test24_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test24_child_2);
	const char* testFile = "j9file_test24_file";
	const char* testFileDone = "j9file_test24_child_2_done";
	const char* testFileLock1 = "j9file_test24_child_1_got_lock";
	const char* testFileTry = "j9file_test24_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST24_OFFSET;
	U_64 length = TEST24_LENGTH;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock1, testName) ) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenRead | EsOpenWrite, 0);

	j9file_create_status_file(portLibrary, testFileTry, testName);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(portLibrary, fd);

	j9file_create_status_file(portLibrary, testFileDone, testName);

exit:
	return reportTestExit(portLibrary,testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets read lock
 *    Child 2 tries to get write lock on same bytes, waits
 *    Child 1 changes part of read lock to write lock
 *    Child 1 releases read lock
 *    Child 1 releases write lock
 *    Child 2 acquires lock
 *    Child 2 releases lock
 *
 * Note:
 *    This test does not really add value to the Windows
 *    platforms as it is not possible to release part of
 *    a lock or obtain a write lock on an area that is
 *    partially read locked, so on Windows the initial
 *    read lock is released - the test works, but is
 *    not really different to earlier tests.
 *
 */
#define TEST25_OFFSET 16
#define TEST25_LENGTH 4
int
j9file_test25(J9PortLibrary *portLibrary, char* argv0)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	J9ProcessHandle pid1 = 0;
	J9ProcessHandle pid2 = 0;
	IDATA termstat1;
	IDATA termstat2;
	IDATA rc;
	const char* testName = APPEND_ASYNC(j9file_test25);
	const char* testFile = "j9file_test25_file";
	const char* testFileLock = "j9file_test25_child_1_got_lock";
	const char* testFileTry = "j9file_test25_child_2_trying_for_lock";
	const char* testFileDone = "j9file_test25_child_2_done";

	reportTestEntry(portLibrary, testName);

	/* Remove status files */
	j9file_unlink(testFileLock);
	j9file_unlink(testFileTry);
	j9file_unlink(testFileDone);

	rc = j9file_create_file(portLibrary, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, j9file_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test25_1");
	if(NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess (portLibrary, testName, argv0, "-child_j9file_test25_2");
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
j9file_test25_child_1(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test25_child_1);
	const char* testFile = "j9file_test25_file";
	const char* testFileLock = "j9file_test25_child_1_got_lock";
	const char* testFileTry = "j9file_test25_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST25_OFFSET;
	U_64 length = TEST25_LENGTH;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_READ_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 1: Got read lock on bytes %lld for %lld\n", offset, length);

	j9file_create_status_file(portLibrary, testFileLock, testName);

	while( !j9file_status_file_exists(portLibrary, testFileTry, testName) ) {
		SleepFor(1);
	}

	SleepFor(5);

#if defined(_WIN32)
	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", (offset + 2), (length - 2), lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 1: Released read lock on bytes %lld for %lld\n", offset, length);
#endif


	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_WAIT_FOR_LOCK, (offset + 2), length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", (offset + 2), length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 1: Got write lock on bytes %lld for %lld\n", (offset + 2), length);

#if !defined(_WIN32)
	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, (length - 2));

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, (length - 2), lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 1: Released read lock on bytes %lld for %lld\n", offset, (length - 2));
#endif

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, (offset + 2), length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", (offset + 2), length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 1: Released write lock on bytes %lld for %lld\n", (offset + 2), length);

	FILE_CLOSE_FUNCTION(portLibrary, fd);

exit:
	return reportTestExit(portLibrary,testName);
}

int
j9file_test25_child_2(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	int pid1 = 0;
	int pid2 = 0;
	const char* testName = APPEND_ASYNC(j9file_test25_child_2);
	const char* testFile = "j9file_test25_file";
	const char* testFileLock = "j9file_test25_child_1_got_lock";
	const char* testFileDone = "j9file_test25_child_2_done";
	const char* testFileTry = "j9file_test25_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	IDATA fd;
	I_32 lockRc;
	I_32 unlockRc;
	U_64 offset = TEST25_OFFSET;
	U_64 length = TEST25_LENGTH;

	reportTestEntry(portLibrary, testName);

	while( !j9file_status_file_exists(portLibrary, testFileLock, testName) ) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(portLibrary, testFile, EsOpenRead | EsOpenWrite, 0);

	outputComment(portLibrary, "Child 2: Trying for write lock on bytes %lld for %lld\n", offset, length);
	j9file_create_status_file(portLibrary, testFileTry, testName);

	lockRc = FILE_LOCK_BYTES_FUNCTION(portLibrary, fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 2: Got write lock on bytes %lld for %lld\n", offset, length);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(portLibrary, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	outputComment(portLibrary, "Child 2: Released write lock on bytes %lld for %lld\n", offset, length);

	FILE_CLOSE_FUNCTION(portLibrary, fd);

	j9file_create_status_file(portLibrary, testFileDone, testName);

exit:
	return reportTestExit(portLibrary,testName);
}

/**
 * Verify port file system.
 *
 * Verify @ref j9file.c::j9file_stat "j9file_stat()" correctly
 * determines if files and directories exist.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test26(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test26);

	const char *localFilename = "tfileTest26.tst";
	char *knownFile = NULL;
	J9FileStat buf;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	if ( 0 == j9sysinfo_get_executable_name("java.properties", &knownFile) ) {
		/* NULL name */
		rc = j9file_stat(NULL, 0, &buf);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(NULL, ..) returned %d expected %d\n", rc, -1);
		}

		/* Query a file that should not be there */
		rc = j9file_stat(localFilename, 0, &buf);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* Still should not be there */
		rc = j9file_stat(localFilename, 0, &buf);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* A directory we know is there, how about the one we are in? */
		rc = j9file_stat(".", 0, &buf);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) returned %d expected %d\n", ".", rc, 0);
		} else if (buf.isDir != 1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) set buf.isDir = %d expected 1\n", ".", buf.isDir);
		}

		/* Query a file that should be there  */
		rc = j9file_stat(knownFile, 0, &buf);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) returned %d expected %d\n", ".", rc, 0);
		} else if (buf.isFile != 1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) set buf.isFile = %d expected 1\n", ".", buf.isFile);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(\"java.properties\", &knownFile) failed\n");
	}

	/* Don't delete executable name (knownFile); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify chmod on files
 *
 * Verify @ref j9file.c::j9file_chmod "j9file_chmod()" correctly
 * sets file permissions.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int j9file_test27(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test27);
	const char *localFilename = "j9file_test27.tst";
	IDATA fd1, fd2;
	I_32 expectedMode;
	const I_32 allWrite = 0222;
	const I_32 allRead = 0444;
	const I_32 ownerWritable = 0200;
	const I_32 ownerReadable = 0400;
	I_32 m;
	I_32 rc;
#define nModes 22
	/* do not attempt to set sticky bit (01000) on files on AIX: will fail if file is on a locally-mounted filesystem */
	I_32 testModes[nModes] = {0, 1, 2, 4, 6, 7, 010, 020, 040, 070, 0100, 0200, 0400, 0700, 0666, 0777, 0755, 0644, 02644, 04755, 06600, 04777};

	reportTestEntry(portLibrary, testName);

	j9file_chmod(localFilename, 0777);
	j9file_unlink(localFilename);
	fd1 = FILE_OPEN_FUNCTION(portLibrary, localFilename, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
	} else {
		(void)FILE_CLOSE_FUNCTION(portLibrary, fd1);
		for (m = 0; m < nModes; ++m) {
#if defined(AIXPPC) || defined(J9ZOS390)
			if (0 != (testModes[m] & J9S_ISGID)) {
				continue; /* sgid not support on some versions of AIX */
			}
#endif
#if defined(_WIN32)
			if (allWrite == (testModes[m] & allWrite)) {
				expectedMode = allRead + allWrite;
			} else {
				expectedMode = allRead;
			}
#else
			expectedMode = testModes[m];
#endif
			rc = j9file_chmod(localFilename, testModes[m]);
			if (expectedMode != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_chmod() returned %d expected %d\n", rc, expectedMode);
				break;
			}
			fd2 = FILE_OPEN_FUNCTION(portLibrary, localFilename, EsOpenWrite, rc);
			if (0 == (rc & ownerWritable)) {
				if (-1 != fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened read-only file for writing fd=%d expected -1\n", rc);
					break;
				}
			} else {
				if (-1 == fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened owner-writable file for writing failed");
					break;
				}
			}
			FILE_CLOSE_FUNCTION(portLibrary, fd2);
			fd2 = FILE_OPEN_FUNCTION(portLibrary, localFilename, EsOpenRead, rc);
			if (0 == (rc & ownerReadable)) {
				if (-1 != fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened write-only file for reading fd=%d expected -1\n", rc);
					break;
				}
			} else {
				if (-1 == fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened owner-readable file for reading failed");
					break;
				}
			}
			FILE_CLOSE_FUNCTION(portLibrary, fd2);
		}
	}
	j9file_chmod(localFilename, 0777);
	j9file_unlink(localFilename);
	rc = j9file_chmod(localFilename, 0755);
	if (-1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_chmod() on non-existent file returned %d expected %d\n", rc, -1);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify chmod on directories
 *
 * Verify @ref j9file.c::j9file_chmod "j9file_chmod()" correctly
 * sets file permissions.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int j9file_test28(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test28);
	const char *localDirectoryName = "j9file_test28_dir";
	I_32 expectedMode;
	const I_32 allWrite = 0222;
	const I_32 allRead = 0444;
	const I_32 ownerWritable = 0200;
	const I_32 ownerReadable = 0400;
	I_32 m;
	I_32 rc;
#define nModes 22
	I_32 testModes[nModes] = {0, 1, 2, 4, 6, 7, 01010, 02020, 04040, 070, 0100, 0200, 0400, 0700, 0666, 0777, 0755, 0644, 01644, 02755, 03600, 07777};

	reportTestEntry(portLibrary, testName);

	j9file_chmod(localDirectoryName, 0777);
	j9file_unlinkdir(localDirectoryName);
	rc = j9file_mkdir(localDirectoryName);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed for %s: could not create\n", FILE_OPEN_FUNCTION_NAME, localDirectoryName);
	} else {
		for (m = 0; m < nModes; ++m) {
#if defined(AIXPPC) || defined(J9ZOS390)
			if (0 != (testModes[m] & J9S_ISGID)) {
				continue; /* sgid not support on some versions of AIX */
			}
#endif
#if defined(_WIN32)
			if (allWrite == (testModes[m] & allWrite)) {
				expectedMode = allRead + allWrite;
			} else {
				expectedMode = allRead;
			}
#else
			expectedMode = testModes[m];
#endif
			rc = j9file_chmod(localDirectoryName, testModes[m]);
			if (expectedMode != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_chmod() returned %d expected %d\n", rc, expectedMode);
				break;
			}
		}
	}
	j9file_chmod(localDirectoryName, 0777);
	j9file_unlinkdir(localDirectoryName);

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify chown on directories
 *
 * Verify @ref j9file.c::j9file_chown "j9file_chown()" correctly
 * sets file group.  Also test j9sysinfo_get_egid(), j9sysinfo_get_euid()
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int j9file_test29(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test29);
	const char *testFile = "j9file_test29_file";
#if ! defined(_WIN32)
	UDATA gid, uid;
	IDATA rc;
#endif

	reportTestEntry(portLibrary, testName);
#if ! defined(_WIN32)
	j9file_unlink(testFile);
	(void)j9file_create_file(portLibrary, testFile, 0, testName);
	gid = j9sysinfo_get_egid();
	rc = j9file_chown(testFile, J9PORT_FILE_IGNORE_ID, gid);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_chown() change group returned %d expected %d\n", rc, 0);
	}
	uid = j9sysinfo_get_euid();
	rc = j9file_chown(testFile, uid, J9PORT_FILE_IGNORE_ID);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_chown()set uid returned %d expected %d\n", rc, 0);
	}
	rc = j9file_chown(testFile, 0, J9PORT_FILE_IGNORE_ID);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_chown() to root succeeded, should have failed\n");
	}
	j9file_unlink(testFile);
#endif

	return reportTestExit(portLibrary, testName);
}



/**
 *
 * Verify @ref j9file.c::j9file_open returns J9PORT_ERROR_FILE_ISDIR if we are trying to create a directory.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test30(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test30);
	const char *validFilename = "tfileTest27.30";

	const char *illegalFileName = "../";
	IDATA validFileFd, existingFileFd, invalidFileFd;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* 1. First we want to trigger an error so that we know j9error_last_error() has been set
	 * 	Do this by trying to create the same file twice
	 * 2. Then try to create a directory {../)
	 *  Make sure j9error_last_error_number() returns J9PORT_ERROR_FILE_ISDIR
	 */

	/* create a file that doesn't exist, this should succeed */
	validFileFd = FILE_OPEN_FUNCTION(portLibrary, validFilename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == validFileFd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to create file %s: error_last_error_number %i\n", FILE_OPEN_FUNCTION_NAME, validFilename, j9error_last_error_number());
		goto exit;
	}

	/* try to create the file again, this should fail. The only reason we do this is to have j9file set the last error code */
	existingFileFd = FILE_OPEN_FUNCTION(portLibrary, validFilename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 != existingFileFd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s should have failed to create existing file: %s\n", FILE_OPEN_FUNCTION_NAME, validFilename);
		goto exit;
	}

	/* try opening a "../" file, expect failure (write open) */
	invalidFileFd = FILE_OPEN_FUNCTION(portLibrary, illegalFileName, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == invalidFileFd) {
		I_32 lastErrorNumber = j9error_last_error_number();

		if (lastErrorNumber == J9PORT_ERROR_FILE_EXIST) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number(\"%s\") returned %i (J9PORT_ERROR_FILE_EXIST), anything but would be fine %i\n", illegalFileName, lastErrorNumber);
		}

	} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s(\"%s\") did not return failure.\n", FILE_OPEN_FUNCTION_NAME, illegalFileName);
	}

	/* try opening a "../" file, expect failure (read open) */
	invalidFileFd = FILE_OPEN_FUNCTION(portLibrary, illegalFileName, EsOpenCreateNew | EsOpenRead | EsOpenTruncate, 0666);
	if (-1 == invalidFileFd) {
		I_32 lastErrorNumber = j9error_last_error_number();

		if (lastErrorNumber == J9PORT_ERROR_FILE_EXIST) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number(\"%s\") returned %i (J9PORT_ERROR_FILE_EXIST), anything but would be fine %i\n", illegalFileName, lastErrorNumber);
		}

	} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s(\"%s\") did not return failure.\n", FILE_OPEN_FUNCTION_NAME, illegalFileName);
	}

exit:

	/* Close the file references, should succeed */
	rc = FILE_CLOSE_FUNCTION(portLibrary, validFileFd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Delete this file, should succeed */
	rc = j9file_unlink(validFilename);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected %d\n", rc, -1);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Test file owner and group attributes
 */
int
j9file_test31(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test31);
	const char *filename = "tfileTest31";
	IDATA fileFd;
	UDATA myUid, myGid, fileGid;
	J9FileStat stat;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	j9file_unlink(filename); /* ensure there is no stale file */
	fileFd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == fileFd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to create file %s: error_last_error_number %i\n", FILE_OPEN_FUNCTION_NAME,
				filename, j9error_last_error_number());
		goto exit;
	}
	rc = j9file_stat(filename, 0, &stat);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) returned %d expected %d\n", filename, rc, 0);
	}
	myUid = j9sysinfo_get_euid();
	myGid = j9sysinfo_get_egid();
	fileGid = stat.ownerGid;

	if (myUid != stat.ownerUid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat.ownerUid is %d expected %d\n", stat.ownerUid, myUid);
	}

	if (myGid != fileGid) {
		outputComment(portLibrary, "unexpected group ID.  Possibly setgid bit set. Comparing to group of the current directory\n");
		rc = j9file_stat(".", 0, &stat);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(%s, ..) returned %d expected %d\n", ".", rc, 0);
		}
		if (fileGid != stat.ownerGid) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat.ownerGid is %d expected %d\n", fileGid, stat.ownerGid);
		}
	}
exit:

	/* Close the file references, should succeed */
	rc = FILE_CLOSE_FUNCTION(portLibrary, fileFd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Delete this file, should succeed */
	rc = j9file_unlink(filename);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() returned %d expected %d\n", rc, -1);
	}

	return reportTestExit(portLibrary, testName);
}

int
j9file_test32(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test32_test_file_permission_bits_using_j9file_stat());
	const char *localFilename = "tfileTest32";
	IDATA fd;
	J9FileStat stat;

	reportTestEntry(portLibrary, testName);

	/*
	 * Test a read-writeable file
	 */
	outputComment(PORTLIB, "j9file_stat() Test a read-writeable file\n");
	j9file_unlink(localFilename);
	fd = FILE_OPEN_FUNCTION(portLibrary, localFilename, EsOpenCreate | EsOpenWrite, 0666);
	if (fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() file open failed\n", FILE_OPEN_FUNCTION_NAME);
		goto done;
	}
	FILE_CLOSE_FUNCTION(portLibrary, fd);
#if defined (WIN32)
	/*The mode param is ignored by j9file_open, so j9file_chmod() is called*/
	j9file_chmod(localFilename, 0666);
#endif
	j9file_stat(localFilename, 0, &stat);

	if (stat.perm.isUserWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserWriteable != 1'\n");
	}
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserReadable != 1'\n");
	}
#if defined (WIN32) 
	if (stat.perm.isGroupWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isGroupWriteable != 1'\n");
	}
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isOtherWriteable != 1'\n");
	}
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isOtherReadable != 1'\n");
	}
#else
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif

	j9file_unlink(localFilename);
	
	/*
	 * Test a readonly file
	 */
	outputComment(PORTLIB, "j9file_stat() Test a readonly file\n");
	j9file_unlink(localFilename);
	fd = FILE_OPEN_FUNCTION(portLibrary, localFilename, EsOpenCreate | EsOpenRead, 0444);
	
	if (fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() file open failed\n", FILE_OPEN_FUNCTION_NAME);
		goto done;
	}
	FILE_CLOSE_FUNCTION(portLibrary, fd);
#if defined (WIN32)
	/*The mode param is ignored by j9file_open, so j9file_chmod() is called*/
	j9file_chmod(localFilename, 0444);
#endif
	j9file_stat(localFilename, 0, &stat);

	if (stat.perm.isUserWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserWriteable != 0'\n");
	}
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserReadable != 1'\n");
	}
#if defined (WIN32)
	if (stat.perm.isGroupWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isGroupWriteable != 0'\n");
	}
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isOtherWriteable != 0'\n");
	}
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isOtherReadable != 1'\n");
	}
#else
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif

	j9file_unlink(localFilename);
	
	/*
	 * Test a writeable file
	 */
	outputComment(PORTLIB, "j9file_stat() Test a writeable file\n");
	j9file_unlink(localFilename);
	fd = FILE_OPEN_FUNCTION(portLibrary, localFilename, EsOpenCreate | EsOpenWrite, 0222);
	if (fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() file open failed\n", FILE_OPEN_FUNCTION_NAME);
		goto done;
	}
	FILE_CLOSE_FUNCTION(portLibrary, fd);
#if defined (WIN32)
	/*The mode param is ignored by j9file_open, so j9file_chmod() is called*/
	j9file_chmod(localFilename, 0222);
#endif
	j9file_stat(localFilename, 0, &stat);

	if (stat.perm.isUserWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserWriteable != 1'\n");
	}
#if defined (WIN32)
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserReadable != 1'\n");
	}
#else 
	if (stat.perm.isUserReadable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isUserReadable != 0'\n");
	}
#endif
	
#if defined (WIN32) 
	if (stat.perm.isGroupWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isGroupWriteable != 1'\n");
	}
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isOtherWriteable != 1'\n");
	}
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat() error 'stat.isOtherReadable != 1'\n");
	}
#else
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif

	
done:
	j9file_unlink(localFilename);
	return reportTestExit(portLibrary, testName);
}


int
j9file_test33(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char* testName = APPEND_ASYNC(j9file_test33: test UTF-8 encoding);

	const char single_byte_utf8[2] = {0x24, '\n'}; /* U+0024 $ */
	const char two_byte_utf8[3] = {0xC2, 0xA2, '\n'}; /* U+00A2 ???*/
	const char three_byte_utf8[4] = {0xE2, 0x82, 0xAC, '\n'}; /* U+20AC ???*/
	const char four_byte_utf8[5] = {0xF0, 0xA4, 0xAD, 0xA2, '\n'}; /* U+024B62 beyond BMP */
	const char invalid_byte_utf8[3] = {0xFE, 0xFF, '\n'}; /* Invalid */

	size_t i = 0;
	char many_byte_utf8[512];

	for (i = 0; i < 508; i++) {
		many_byte_utf8[i] = '*';
	}
	many_byte_utf8[i++] = 0xE2;
	many_byte_utf8[i++] = 0x82;
	many_byte_utf8[i++] = 0xAC;
	many_byte_utf8[i] = '\n'; /* force outbuf to be too small for converted unicode format */

	reportTestEntry(portLibrary, testName);

	j9file_write_text(1, (const char*)single_byte_utf8, sizeof(char)*2);
	j9file_write_text(1, (const char*)two_byte_utf8, sizeof(char)*3);
	j9file_write_text(1, (const char*)three_byte_utf8, sizeof(char)*4);		/* Unicode string representation should be printed */
	j9file_write_text(1, (const char*)four_byte_utf8, sizeof(char)*5);		/* '????' should be printed */
	j9file_write_text(1, (const char*)invalid_byte_utf8, sizeof(char)*3);	/* '??' should be printed */
	j9file_write_text(1, (const char*)many_byte_utf8, sizeof(char)*512);	/* Unicode string representation should be printed */

	return reportTestExit(portLibrary, testName);
}

#if defined(WIN32)
/**
 * Test a very long file name
 *
 * For both relative and absolute paths, build up a directory hierarchy that is greater than the system defined MAX_PATH
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 *
 * Note that this test is only run on windows, as it is the only OS (AFAIK)
 * that supports file names longer than 256 chars.
 */
int
j9file_test_long_file_name(struct J9PortLibrary *portLibrary)
{
	/* first we need a file to copy use with vprintf */
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test_long_file_name);
	I_32 rc, mkdirRc,unlinkRc;
	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;
	char *longDirName = "\\abcdefghijklmnopqrstuvwx";
#define MIN_LENGTH (MAX_PATH + 1)
#define FILENAME_LENGTH MIN_LENGTH*2 /* double the size to accommodate a file size that is just under twice MIN_LENGTH */
	char filePathName[FILENAME_LENGTH];
	IDATA fd;
	char cwd[FILENAME_LENGTH];
	char *basePaths[2];
	int i;

	reportTestEntry(portLibrary, testName);

	/* get the current drive designator for the absolute path*/
	_getcwd(cwd, FILENAME_LENGTH);
	cwd[2] = '\0'; /* now we have something like "c:" */

	basePaths[0] = "."; /* to test a relative path */
	basePaths[1] = cwd; /* to test an absolute path */

	for ( i = 0; i < 2; i ++) {
		j9str_printf(portLibrary, filePathName, FILENAME_LENGTH, "%s", basePaths[i]);

		/* build up a file name that is longer than 256 characters,
		 * comprised of directories, each of which are less than 256 characters in length*/
		while (strlen(filePathName) < MIN_LENGTH ) {

			j9str_printf(portLibrary, filePathName + strlen(filePathName), FILENAME_LENGTH - strlen(filePathName), "%s", longDirName);

			mkdirRc = j9file_mkdir(filePathName);

			if ((mkdirRc != 0)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_mkdir() failed for dir with length %i: %s\n", strlen(filePathName), filePathName);
				goto exit;
			}
		}

		/* now append filePathName with the actual filename */
		j9str_printf(portLibrary, filePathName + strlen(filePathName), FILENAME_LENGTH - strlen(filePathName), "\\%s", testName);
		
		j9tty_printf(portLibrary, "\ttesting filename: %s\n", filePathName);
		
		/* can we open and write to the file? */
		fd = FILE_OPEN_FUNCTION(portLibrary, filePathName, EsOpenCreate | EsOpenWrite, 0666);
		if (-1 == fd) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
			goto exit;
		}

		fileWrite(portLibrary, fd,"Try a number - %d",1);
		fileWrite(portLibrary, fd,"Try a string - %s","abc");
		fileWrite(portLibrary, fd,"Try a mixture - %d %s %d",1,"abc",2);

		/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
		rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
			goto exit;
		}

		fd = FILE_OPEN_FUNCTION(portLibrary, filePathName, EsOpenRead, 0444);
		if (-1 == fd) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
			goto exit;
		}

		readBufPtr = j9file_read_text(fd, readBuf, sizeof(readBuf));
		if (NULL == readBufPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned NULL\n");
			goto exit;
		}

		if (strlen(readBufPtr) != strlen(expectedResult)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned %d expected %d\n", strlen(readBufPtr), strlen(expectedResult));
			goto exit;
		}

/* j9file_read_text(): translation from IBM1047 to ASCII is done on zOS */
		if (strcmp(readBufPtr, expectedResult)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
			goto exit;
		}

	exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);

		/* delete the file */
		unlinkRc = j9file_unlink(filePathName);
		if (0 != unlinkRc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink failed: %s\n", filePathName);
		}

		/* Now we need to unwind and delete all the directories we created one by one
		 * 	- we simply delete the base directory because it is not empty.
		 * Do this by stripping actual name of the filename, then each occurrence of longDirName from the full path.
		 *
		 * You know you're done when the filePathName doesn't contain the occurrence of longDirName
		 */
		filePathName[strlen(filePathName) - strlen(testName)] = '\0';

		{
			size_t lenLongDirName = strlen(longDirName);

			while (NULL != strstr(filePathName, longDirName)) {

				unlinkRc = j9file_unlinkdir(filePathName);
				if (0 != unlinkRc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlinkdir failed: %s\n", filePathName);
				}

				filePathName[strlen(filePathName) - lenLongDirName] = '\0';
			}
		}
	}

#undef FILENAME_LENGTH
#undef MIN_LENGTH

	return reportTestExit(portLibrary, testName);
}
#endif /* WIN32 */


/**
 * Test j9file_stat_filesystem ability to measure free disk space
 *
 * This test check the amount of free disk space, create a file and check the amount of free disk space again.
 * It is necessary for the amount of free disk space to changes after the file is created.
 * It is necessary for the total amount of disk space to be constant after the file is created.
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test34(struct J9PortLibrary *portLibrary)
{
#define J9FILETEST_FILESIZE 1000 * 512
	J9FileStatFilesystem buf_before, buf_after;
	J9FileStat buf;
	IDATA rc, fd;
	UDATA freeSpaceDelta;
	char * stringToWrite = NULL;
	int returnCode = TEST_PASS;
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char* testName = APPEND_ASYNC(j9file_test34);
	const char* fileName = "j9file_test34.tst";

	reportTestEntry(portLibrary, testName);

	/* delete fileName to have a clean test, ignore the result */
	(void)j9file_unlink(fileName);

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666);
	if (-1 == fd) {
		outputComment(portLibrary, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		returnCode = TEST_FAIL;
		goto exit;
	}

#if defined(LINUX)
	sync();
#endif /* defined(LINUX) */
	rc = j9file_stat_filesystem(fileName, 0, &buf_before);
	if (0 != rc) {
		outputComment(portLibrary, "first j9file_stat_filesystem() returned %d, expected %d\n", rc, 0);
		returnCode = TEST_FAIL;
		goto exit;
	}
	rc = j9file_stat(fileName, 0, &buf);
	if (0 != rc) {
		outputComment(portLibrary, "first j9file_stat() returned %d, expected %d\n", rc, 0);
		returnCode = TEST_FAIL;
		goto exit;
	}

	if (buf.isRemote) {
		outputComment(portLibrary, "WARNING: Test j9file_test34 cannot be run on a remote file system.\nSkipping test.\n");
		returnCode = TEST_PASS;
		goto exit;
	}

	stringToWrite = j9mem_allocate_memory(J9FILETEST_FILESIZE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == stringToWrite) {
		outputComment(portLibrary, "Failed to allocate %zu bytes\n", J9FILETEST_FILESIZE);
		returnCode = TEST_FAIL;
		goto exit;
	}
	memset(stringToWrite, 'a', J9FILETEST_FILESIZE);
	stringToWrite[J9FILETEST_FILESIZE - 1] = '\0';

	j9file_printf(portLibrary, fd, "%s", stringToWrite);
	j9file_sync(fd); /* flush data written to disk */

#if defined(LINUX)
	sync();
#endif /* defined(LINUX) */
	rc = j9file_stat_filesystem(fileName, 0, &buf_after);
	if (0 != rc) {
		outputComment(portLibrary, "second j9file_stat_filesystem() returned %d, expected %d\n", rc, 0);
		returnCode = TEST_FAIL;
		goto exit;
	}

	freeSpaceDelta = (UDATA)(buf_before.freeSizeBytes - buf_after.freeSizeBytes);

	/* Checks if the free disk space changed after the file has been created. */
	if (0 == freeSpaceDelta) {
		outputComment(portLibrary, "The amount of free disk space does not change after the creation of a file.\n\
			It can be due to a file of exactly %d bytes being deleted during this test, or j9file_stat_filesystem failing.\n\
			The test does not work on a remote file system. Ignore failures on remote file systems.\n", J9FILETEST_FILESIZE);
		returnCode = TEST_FAIL;
		goto exit;
	}

	if (buf_before.totalSizeBytes != buf_after.totalSizeBytes) {
		outputComment(portLibrary, "Total disk space changed between runs, it was %llu changed to %llu.\n\
				The disk space should not change between runs.\n", buf_before.totalSizeBytes, buf_after.totalSizeBytes);
		returnCode = TEST_FAIL;
		goto exit;
	}

exit:

	/* Close the file, expect success */
	if (-1 != fd) {
		rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
		if (0 != rc) {
			outputComment(portLibrary, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		}

		/* Delete the file, expect success */
		rc = j9file_unlink(fileName);
		if (0 != rc) {
			outputComment(portLibrary, "j9file_unlink() returned %d expected %d\n", rc, 0);
		}
	}

	j9mem_free_memory(stringToWrite);

	reportTestExit(portLibrary, testName);

	return returnCode;
}

/**
 * Test j9file_test34 nbOfTries times
 *
 * @return TEST_PASS as soon as the test passes, TEST_FAIL if the test always fails
 */
int
j9file_test34_multiple_tries(struct J9PortLibrary *portLibrary, int nbOfTries)
{
	int i, rc;
	char* testName = APPEND_ASYNC(j9file_test34_multiple_tries:_test_j9file_stat_filesystem_disk_space);

	reportTestEntry(portLibrary, testName);
	/* The test j9file_test34 may fail if other processes writes to the hard drive, trying the test nbOfTries times */
	for (i = 0; i < nbOfTries; i++) {
		rc = j9file_test34(portLibrary);
		if (TEST_PASS == rc) {
			goto exit;
		}
		omrthread_sleep(100);
	}
	outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_test34_multiple_tries failed %d time(s)\n", nbOfTries);
exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Test j9file_test35
 * Tests j9file_flength()
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test35(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char* testName = APPEND_ASYNC(j9file_test35);
	const char* fileName = "j9file_test35.txt";
	IDATA fd;
	I_64 fileLength = 1024, seekOffset = 10, offset, newLength, newfLength;
	IDATA rc;

	reportTestEntry(portLibrary, testName);

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned -1 expected valid file handle", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	rc = FILE_SET_LENGTH_FUNCTION(portLibrary, fd, fileLength);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	newLength = j9file_length(fileName);
	if (newLength != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_length() returned %lld expected %lld\n", newLength, fileLength);
		goto exit;
	}

	newfLength = FILE_FLENGTH_FUNCTION(portLibrary, fd);
	if (newfLength != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %lld\n", FILE_FLENGTH_FUNCTION_NAME, newfLength, fileLength);
		goto exit;
	}
	if (newfLength != newLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %lld\n", FILE_FLENGTH_FUNCTION_NAME, newfLength, newLength);
		goto exit;
	}

	/* Testing that j9file_flength doesn't change the file_seek offset */
	offset = j9file_seek(fd, seekOffset, EsSeekSet);
	if (seekOffset != offset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_seek() returned %lld expected %lld\n", offset, seekOffset);
		goto exit;
	}

	newfLength = FILE_FLENGTH_FUNCTION(portLibrary, fd);
	if (newfLength != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %lld\n", FILE_FLENGTH_FUNCTION_NAME, newfLength, fileLength);
		goto exit;
	}

	offset = j9file_seek(fd, 0, EsSeekCur);
	if (seekOffset != offset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_seek() returned %lld expected %lld\n", offset, seekOffset);
		goto exit;
	}
exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Test j9file_test36
 *
 * Simple test of j9file_convert_native_fd_to_j9file_fd (it's a simple function!)
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test36(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char* testName = APPEND_ASYNC(j9file_test36);

	IDATA nativeFD = 999; /* pick an arbitrary number */
	IDATA j9fileFD = -1;

	reportTestEntry(portLibrary, testName);

	j9fileFD = j9file_convert_native_fd_to_j9file_fd(nativeFD);

#if !defined(J9ZOS390)
	if (j9fileFD != nativeFD) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9fileFD != nativeFD, they should be the same. j9fileFD: %x, nativeFD: %x\n", j9fileFD, nativeFD);
	}
#else /* defined(J9ZOS390) */
	if (j9fileFD == nativeFD) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9fileFD == nativeFD, they should be different. j9fileFD: %x, nativeFD: %x\n", j9fileFD, nativeFD);
	}

#endif /* defined(J9ZOS390) */


	return reportTestExit(portLibrary, testName);
}

#if !defined(WIN32)
/**
 * Verify port file system.
 *
 * Verify @ref j9file.c::j9file_fstat "j9file_fstat()" correctly
 * determines if files and directories exist.
 * Similar to j9file_test26 for j9file_stat().
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test37(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = APPEND_ASYNC(j9file_test37);
	char *knownFile = NULL;
	J9FileStat buf;
	I_32 rc = -1;

	reportTestEntry(portLibrary, testName);

	memset(&buf, 0, sizeof(J9FileStat));
	
	/* Invalid fd */
	rc = j9file_fstat(-1, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Negative test failure: j9file_fstat(-1, ..) on invalid file descriptor is expected to fail but it returned success with rc=%d (ignore the following LastErrorNumber and LastErrorMessage)\n", rc);
	}

	if (0 == j9sysinfo_get_executable_name(NULL, &knownFile)) {
		/* Query a file that should be there  */
		IDATA fd = FILE_OPEN_FUNCTION(portLibrary, knownFile, EsOpenRead, 0666);
		if (-1 != fd) {
			rc = j9file_fstat(fd, &buf);
			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d(filename=%s)\n", fd, knownFile);
			} else if (1 != buf.isFile) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) for %s returned buf.isFile = %d, expected 1\n", knownFile, buf.isFile);
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to open file %s\n", FILE_OPEN_FUNCTION_NAME, knownFile);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sysinfo_get_executable_name(NULL, &knownFile) failed\n");
	}

	/* Don't delete executable name (knownFile); system-owned. */
	return reportTestExit(portLibrary, testName);
}

/**
 * Test file owner and group attributes using j9file_fstat()
 * Similar to j9file_test31 for j9file_stat().
 */
int
j9file_test38(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test38);
	const char *filename = "tfileTest38.tst";
	IDATA fd = -1;
	UDATA myUid = 0;
	UDATA myGid = 0;
	UDATA fileGid = 0;
	J9FileStat stat;
	I_32 rc = -1;

	reportTestEntry(portLibrary, testName);

	memset(&stat, 0, sizeof(J9FileStat));
	
	j9file_unlink(filename); /* ensure there is no stale file */
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 != fd) {
		rc = j9file_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			myUid = j9sysinfo_get_euid();
			myGid = j9sysinfo_get_egid();
			fileGid = stat.ownerGid;

			if (myUid != stat.ownerUid) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) for %s returned ownerUid as %d, expected %d\n", filename, stat.ownerUid, myUid);
			}

			if (myGid != fileGid) {
				outputComment(portLibrary, "Unexpected group ID. Possibly setgid bit set. Comparing to group of the current directory\n");
				rc = j9file_stat(".", 0, &stat);
				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_stat(...) failed for current directory with rc=%d\n", rc);
				} else {
					if (fileGid != stat.ownerGid) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) for current directory returned ownerGid as %d, expected %d\n", fileGid, stat.ownerGid);
					}
				}
			}
		}

		/* Close the file references, should succeed */
		rc = FILE_CLOSE_FUNCTION(portLibrary, fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to close the file\n", FILE_CLOSE_FUNCTION_NAME);
		}

		/* Delete this file, should succeed */
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to create file %s\n", FILE_OPEN_FUNCTION_NAME, filename);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Test file file permission bits using j9file_fstat()
 * Similar to j9file_test32 for j9file_stat().
 */
int
j9file_test39(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test39: test file permission bits using j9file_fstat());
	const char *filename = "tfileTest39.tst";
	IDATA fd = -1;
	J9FileStat stat;
	I_32 mode = 0;
	I_32 rc = -1;
	BOOLEAN doUnlink = FALSE;
	
	reportTestEntry(portLibrary, testName);

	memset(&stat, 0, sizeof(J9FileStat));
	
	/*
	 * Test a read-writeable file
	 */
	outputComment(PORTLIB, "j9file_fstat() Test a read-writeable file\n");
	j9file_unlink(filename);

	mode = 0666;
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = j9file_fstat( fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserReadable != 1'\n");
			}
			/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
		}

		FILE_CLOSE_FUNCTION(portLibrary, fd);
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a read-only file
	 */
	outputComment(PORTLIB, "j9file_fstat() Test a read-only file\n");

	mode = 0444;
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreate | EsOpenRead, mode);
	if (-1 != fd) {
		rc = j9file_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserWriteable != 0'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserReadable != 1'\n");
			}
			/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
		}

		FILE_CLOSE_FUNCTION(portLibrary, fd);
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a write-only file
	 */
	outputComment(PORTLIB, "j9file_fstat() Test a write-only file\n");

	mode = 0222;
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = j9file_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserReadable != 0'\n");
			}
			/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
		}

		FILE_CLOSE_FUNCTION(portLibrary, fd);
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}
	
	/*
	 * Test a user only read-writeable file
	 */
	outputComment(PORTLIB, "j9file_fstat() Test a user only read-writeable file\n");
	j9file_unlink(filename);

	mode = 0600;
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = j9file_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserReadable != 1'\n");
			}
			if (stat.perm.isGroupWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isGroupWriteable != 0'\n");
			}
			if (stat.perm.isGroupReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isGroupReadable != 0'\n");
			}
			if (stat.perm.isOtherWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isOtherWriteable != 0'\n");
			}
			if (stat.perm.isOtherReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isOtherReadable != 0'\n");
			}
		}

		FILE_CLOSE_FUNCTION(portLibrary, fd);
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "5s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}
	
	/*
	 * Test a user only read-only file
	 */
	outputComment(PORTLIB, "j9file_fstat() Test a user only read-only file\n");

	mode = 0400;
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreate | EsOpenRead, mode);
	if (-1 != fd) {
		rc = j9file_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserWriteable != 0'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserReadable != 1'\n");
			}
			if (stat.perm.isGroupWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isGroupWriteable != 0'\n");
			}
			if (stat.perm.isGroupReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isGroupReadable != 0'\n");
			}
			if (stat.perm.isOtherWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isOtherWriteable != 0'\n");
			}
			if (stat.perm.isOtherReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isOtherReadable != 0'\n");
			}
		}

		FILE_CLOSE_FUNCTION(portLibrary, fd);
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a user only write-only file
	 */
	outputComment(PORTLIB, "j9file_fstat() Test a user only write-only file\n");

	mode = 0200;
	fd = FILE_OPEN_FUNCTION(portLibrary, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = j9file_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isUserReadable != 0'\n");
			}
			if (stat.perm.isGroupWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isGroupWriteable != 0'\n");
			}
			if (stat.perm.isGroupReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isGroupReadable != 0'\n");
			}
			if (stat.perm.isOtherWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isOtherWriteable != 0'\n");
			}
			if (stat.perm.isOtherReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_fstat() error 'stat.isOtherReadable != 0'\n");
			}
		}

		FILE_CLOSE_FUNCTION(portLibrary, fd);
		rc = j9file_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9file_unlink() failed to delete the file %s\n", filename);
		}

	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "5s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	return reportTestExit(portLibrary, testName);
}
#endif



/**
 * Verify port file system.
 * @ref j9file.c::j9file_set_length "j9file_set_length()"
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9file_test40(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = APPEND_ASYNC(j9file_test40);
	const char *fileName = "tfileTest40.tst";
	I_64 filePtr = 0;
	I_64 prevFilePtr = 0;
	IDATA fd = 0;
	char inputLine1[] = "ABCDE";
	IDATA expectedReturnValue;
	IDATA rc = 0;


	reportTestEntry(portLibrary, testName);
	expectedReturnValue = sizeof(inputLine1)-1; /* don't want the final 0x00 */

	fd = FILE_OPEN_FUNCTION(portLibrary, fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed \n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	prevFilePtr = j9file_seek (fd, 0, EsSeekCur);
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening file. Expected = 0, Got = %lld", prevFilePtr);
	}

	/* now lets write to it */
	rc = FILE_WRITE_FUNCTION(portLibrary, fd, inputLine1, expectedReturnValue);

	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = j9file_seek (fd, 0, EsSeekCur);
	if (prevFilePtr + expectedReturnValue != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after writing into file. Expected = %lld, Got = %lld", prevFilePtr + expectedReturnValue,  filePtr);
	}

	prevFilePtr = filePtr;
	rc = FILE_SET_LENGTH_FUNCTION(portLibrary, fd, 100);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	filePtr = j9file_seek (fd, 0, EsSeekCur);

	if (prevFilePtr != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() Changed the location of file pointer. Expected : %lld. Got : %lld \n", FILE_SET_LENGTH_FUNCTION_NAME, prevFilePtr, filePtr);
		goto exit; /* Although we dont need this, lets keep it to make it look like other failure cases */
	}

exit:
	FILE_CLOSE_FUNCTION(portLibrary, fd);
	j9file_unlink(fileName);
	return reportTestExit(portLibrary, testName);
}






/**
 * Verify port file system.
 *
 * Exercise all API related to port library memory management found in
 * @ref j9mem.c
 *
 * @param[in] portLibrary The port library under test
 *
 * @return 0 on success, -1 on failure
 */
I_32
j9file_runTests(struct J9PortLibrary *portLibrary, char *argv0, char* j9file_child, BOOLEAN async)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	isAsync = async;

	if (TRUE == isAsync) {
		HEADING(portLibrary, "File Blockingasync Test");
	} else {
		HEADING(portLibrary, "File Test");
	}

	if(j9file_child != NULL) {
		if(strcmp(j9file_child,"j9file_test20_1") == 0) {
			return j9file_test20_child_1(portLibrary);
		} else if(strcmp(j9file_child, "j9file_test20_2") == 0) {
			return j9file_test20_child_2(portLibrary);
		} else if(strcmp(j9file_child,"j9file_test21_1") == 0) {
			return j9file_test21_child_1(portLibrary);
		} else if(strcmp(j9file_child, "j9file_test21_2") == 0) {
			return j9file_test21_child_2(portLibrary);
		} else if(strcmp(j9file_child,"j9file_test22_1") == 0) {
			return j9file_test22_child_1(portLibrary);
		} else if(strcmp(j9file_child, "j9file_test22_2") == 0) {
			return j9file_test22_child_2(portLibrary);
		} else if(strcmp(j9file_child,"j9file_test23_1") == 0) {
			return j9file_test23_child_1(portLibrary);
		} else if(strcmp(j9file_child, "j9file_test23_2") == 0) {
			return j9file_test23_child_2(portLibrary);
		} else if(strcmp(j9file_child,"j9file_test24_1") == 0) {
			return j9file_test24_child_1(portLibrary);
		} else if(strcmp(j9file_child, "j9file_test24_2") == 0) {
			return j9file_test24_child_2(portLibrary);
		} else if(strcmp(j9file_child,"j9file_test25_1") == 0) {
			return j9file_test25_child_1(portLibrary);
		} else if(strcmp(j9file_child, "j9file_test25_2") == 0) {
			return j9file_test25_child_2(portLibrary);
		}
	}

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9file_test0(portLibrary);


	if (TEST_PASS == rc) {
		rc |= j9file_test1(portLibrary);
		rc |= j9file_test2(portLibrary);
		/* Uses j9file_read_text() which uses the synchronous IO support */
		if (TRUE != async) {
			rc |= j9file_test3(portLibrary);
		}
		rc |= j9file_test4(portLibrary);
		rc |= j9file_test5(portLibrary);
		rc |= j9file_test6(portLibrary);
		rc |= j9file_test7(portLibrary);

		rc |= j9file_test8(portLibrary);

		rc |= j9file_test9(portLibrary);
		rc |= j9file_test10(portLibrary);
		rc |= j9file_test11(portLibrary);
		rc |= j9file_test12(portLibrary);
		rc |= j9file_test13(portLibrary);
		rc |= j9file_test14(portLibrary);
		rc |= j9file_test15(portLibrary);
		rc |= j9file_test16(portLibrary);
		/* Uses j9file_read_text() which uses the synchronous IO support */
		if (TRUE != async) {
			rc |= j9file_test17(portLibrary);
		}
		rc |= j9file_test18(portLibrary, argv0);
		rc |= j9file_test19(portLibrary, argv0);

#if !defined(PORT_FILETEST_FLAKY)
		rc |= j9file_test20(portLibrary, argv0);
#else
		j9tty_printf(portLibrary, "\n\n!!! j9file_test20 has been disabled on this platform until the implementation/test has been fixed\n\n");
#endif

		rc |= j9file_test21(portLibrary, argv0);

#if !defined(PORT_FILETEST_FLAKY)
		rc |= j9file_test22(portLibrary, argv0);
#else
		j9tty_printf(portLibrary, "\n\n!!! j9file_test22 has been disabled on this platform until the implementation/test has been fixed\n\n");
#endif

		rc |= j9file_test23(portLibrary, argv0);

#if !defined(PORT_FILETEST_FLAKY)
		rc |= j9file_test24(portLibrary, argv0);
		rc |= j9file_test25(portLibrary, argv0);
#else
		j9tty_printf(portLibrary, "\n\n!!! j9file_test24/25 has been disabled on this platform until the implementation/test has been fixed\n\n");
#endif

		rc |= j9file_test26(portLibrary);
		rc |= j9file_test27(portLibrary);
		rc |= j9file_test28(portLibrary);
		rc |= j9file_test29(portLibrary);
		rc |= j9file_test30(portLibrary);
		rc |= j9file_test31(portLibrary);
		rc |= j9file_test32(portLibrary);
		rc |= j9file_test33(portLibrary);
		/* Uses j9file_write_text() which uses the synchronous IO support */
		if (TRUE != async) {
			rc |= j9file_test34_multiple_tries(portLibrary, 5);
		}
		rc |= j9file_test35(portLibrary);
		rc |= j9file_test36(portLibrary);

#if !defined(WIN32)
		rc |= j9file_test37(portLibrary);
		rc |= j9file_test38(portLibrary);
		rc |= j9file_test39(portLibrary);
#endif /* !defined(WIN32) */
#if defined(WIN32)
		/* Windows accepts file names of up to 32,767 characters, whereas the unix-derived systems do not */
		/* Uses j9file_write_text() which uses the synchronous IO support */
		if (TRUE != async) {
			rc |= j9file_test_long_file_name(portLibrary);
		}
#endif
		rc |= j9file_test40(portLibrary);
	}

	/* Output results */
	if (async == TRUE) {
		j9tty_printf(PORTLIB, "\nFile BlockingAsync test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	} else {
		j9tty_printf(PORTLIB, "\nFile test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	}
	return TEST_PASS == rc ? 0 : -1;
}


