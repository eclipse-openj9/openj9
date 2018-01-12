/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include <stdlib.h>
#include <string.h>

#include "testHelpers.h"
#include "j9port.h"

typedef struct TestStruct {
	char *fileName;
	char *testName;
	char *errorMessage;
	I_32 lineNumber;
} TestStruct;

#define MAX_NUM_TEST_FAILURES 256

/* 
 * A log of all the test failures. logTestFailure() writes to this.
 * 
 * A linked list would be better */
static TestStruct testFailures[MAX_NUM_TEST_FAILURES];

/* 
 * Total number of failed test cases in all components
 * 
 * logTestFailure increments this (note that C initializes this to 0) 
 */
static int numTestFailures;

/**
 * Number of failed tests
 * 
 * When @ref reportTestEntry is executed this global variable is cleared.  When
 * an error message is output via @ref outputMessage it is incremented.  Upon exit
 * of a testcase @ref reportTestExit will indicate if the test passed or failed based
 * on this variable.
 */
static int numberFailedTestsInComponent;

/**
 * Time at the start of the test, in usecs.
 */
static UDATA testStartTime;

/**
 * Display a message indicating the operation is not supported
 * 
 * The port library have various configurations to save size on very small VMs.
 * As a result entire areas of functionality may not be present.  Sockets and file systems
 * are examples of these areas.  In addition even if an area is present not all functionality
 * for that area may be available.  Again sockets and file systems are examples.
 * 
 * @param[in] portLibrary The port library
 * @param[in] operation The operation that is not supported
 * 
 * @internal @note For UTH environment only, displaying the string is controlled by the verbose 
 * level expressed by @ref engine_getTraceLevel.  Outside the UTH environment the messages
 * are always displayed.
 */
void
operationNotSupported(struct J9PortLibrary *portLibrary, const char *operationName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9tty_printf(PORTLIB,"%s not supported in this configuration\n", operationName);
}

/**
 * Display a message indicating the test is starting
 *
 * @param[in] portLibrary The port library
 * @param[in] testName The test that is starting
 *
 * @note Message is only displayed if verbosity level indicated by
 * @ref engine_getTraceLevel is sufficient
 * 
 * @note Clears number of failed tests.  @see outputMessage.
 */
void
reportTestEntry(struct J9PortLibrary *portLibrary, const char *testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9tty_printf(PORTLIB,"Starting test %s\n",testName);
	numberFailedTestsInComponent = 0;
	testStartTime = j9time_usec_clock();
}

/**
 * Display a message indicating the test is finished
 *
 * @param[in] portLibrary The port library
 * @param[in] testName The test that finished
 *
 * @return TEST_PASSED if no tests failed, else TEST_FAILED as reported
 * by @ref outputMessage
 * 
 * @note Message is only displayed if verbosity level indicated by
 * @ref engine_getTraceLevel is sufficient
 */
IDATA
reportTestExit(struct J9PortLibrary *portLibrary, const char *testName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA testEndTime = j9time_usec_clock();
	j9tty_printf(PORTLIB,"Ending test %s (time used = %d usecs)\n", testName, (testEndTime - testStartTime));
	return 0 == numberFailedTestsInComponent ? TEST_PASS : TEST_FAIL;
}

/**
 * Display a message to the console.
 * 
 * Given a format string and it's arguments display the string to the console.
 *
 * @param[in] portLibrary The port library
 * @param[in] foramt Format of string to be output
 * @param[in] ... argument list for format string
 * 
 * @note Does not increment number of failed tests.
 */
void
outputComment(struct J9PortLibrary *portLibrary, const char *format, ...)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	va_list args;
	va_start(args,format);
	j9tty_printf(PORTLIB, "  ");
	j9tty_vprintf(format, args);
	va_end(args);
}

/**
 * Iterate through the global @ref testFailures printing out each failed test case
 * 
 * The memory for each string in testFailers is freed after writing the string to the console.
 * 
 * @param[in] 	portLibrary
 * @param[out] 	dest container for pointer to copied memory
 * @param[in]	source string to be copied into dest
 */
void dumpTestFailuresToConsole(struct J9PortLibrary *portLibrary) {
	I_32 i;

	PORT_ACCESS_FROM_PORT(portLibrary);

	j9tty_printf(portLibrary, "-------------------------------------------------------------------------\n");
	j9tty_printf(portLibrary, "-------------------------------------------------------------------------\n\n");
	j9tty_printf(PORTLIB,"FAILURES DETECTED. Number of failed tests: %u\n\n", numTestFailures);

	for (i = 0; i < numTestFailures ; i++) {
		j9tty_printf(portLibrary, "%i: %s\n", i+1, testFailures[i].testName);
		j9tty_printf(portLibrary, "\t%s line %4zi: %s\n", testFailures[i].fileName, testFailures[i].lineNumber, testFailures[i].errorMessage);

		j9mem_free_memory(testFailures[i].fileName);
		j9mem_free_memory(testFailures[i].testName);
		j9mem_free_memory(testFailures[i].errorMessage);
	}

}

/**
 * Allocate memory for destination then copy the source into it.
 * 
 * @param[in] 	portLibrary
 * @param[out] 	dest container for pointer to copied memory
 * @param[in]	source string to be copied into dest
 */
static void allocateMemoryForAndCopyInto(struct J9PortLibrary *portLibrary, char **dest, const char* source) {

	UDATA strLenPlusTerminator = 0;

	PORT_ACCESS_FROM_PORT(portLibrary);

	strLenPlusTerminator = strlen(source) +1 /*null terminator */;
	*dest = j9mem_allocate_memory(strLenPlusTerminator, J9MEM_CATEGORY_CLASSES);
	if (*dest == NULL)  {
		return;
	} else {
		strncpy(*dest, source, strLenPlusTerminator);
	}
}
/** 
 * Log the test failure so that it can be dumped at test harness exit.
 * 
 * Allocate the memory required to log the failures. This memory is freed in @ref dumpTestFailuresToConsole
 * 
 * @param[in] portLibrary
 * @param[in] testName null-terminated string;
 * @param[in] errorMessage null-terminated string;
 * @param[in] lineNumber;
 * 
 * This uses the global numTestFailures */
static void logTestFailure(struct J9PortLibrary *portLibrary, const char *fileName, I_32 lineNumber, const char *testName, I_32 portErrorNumber, const char *portErrorMessage, const char *testErrorMessage) {

	PORT_ACCESS_FROM_PORT(portLibrary);

	if (MAX_NUM_TEST_FAILURES <= numTestFailures) {
		return;
	}

	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].testName, testName);
	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].errorMessage, testErrorMessage);
	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].fileName, fileName);

	testFailures[numTestFailures].lineNumber = lineNumber;

	numTestFailures++;
}

/**
 * Write a formatted string to the console, lookup the last port error message and port error number, then store it.
 *
 * Update the number of failed tests
 * 
 * 
 * @param[in] portLibrary The port library
 * @param[in] fileName File requesting message output
 * @param[in] lineNumber Line number in the file of request
 * @param[in] testName Name of the test requesting output
 * @param[in] foramt Format of string to be output
 * @param[in] ... argument list for format string
 *
 * @internal @note For UTH environment only, displaying the string is controlled by the verbose 
 * level expressed by @ref engine_getTraceLevel.  Outside the UTH environment the messages
 * are always displayed.
 */
void
outputErrorMessage(struct J9PortLibrary *portLibrary, const char *fileName, I_32 lineNumber, const char *testName, const char *format, ...)
{

	char *buf, *portErrorBuf = NULL;
	UDATA sizeBuf;
	size_t sizePortErrorBuf;
	va_list args;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;

	PORT_ACCESS_FROM_PORT(portLibrary);

	lastErrorMessage = (char *)j9error_last_error_message();
	lastErrorNumber = j9error_last_error_number();

	/* Capture the error message now 
	 * Get the size needed to hold the last error message (don't use str_printf to avoid polluting error message */
	sizePortErrorBuf = strlen(lastErrorMessage) + 1 /* for the null terminator */;

	portErrorBuf = j9mem_allocate_memory(sizePortErrorBuf, J9MEM_CATEGORY_CLASSES);

	if (NULL != portErrorBuf) {
		strncpy(portErrorBuf, lastErrorMessage, sizePortErrorBuf);
	} else {
		j9tty_printf(portLibrary, "\n\n******* j9mem_allocate_memory failed to allocate %i bytes, exiting.\n\n", sizePortErrorBuf);
		exit(EXIT_OUT_OF_MEMORY);
	}

	va_start( args, format );
	/* get the size needed to hold the error message that was passed in */
	sizeBuf = j9str_vprintf(NULL, 0, format, args);
	va_end( args );

	buf = j9mem_allocate_memory(sizeBuf, J9MEM_CATEGORY_CLASSES);
	if (NULL != buf) {
		va_start( args, format );
		j9str_vprintf(buf ,sizeBuf, format, args);
		va_end( args );
	} else {
		j9tty_printf(portLibrary, "\n\n******* j9mem_allocate_memory failed to allocate %i bytes, exiting.\n\n", sizeBuf);
		exit(EXIT_OUT_OF_MEMORY);

	}

	j9tty_printf(portLibrary, "%s line %4zi: %s ", fileName, lineNumber, testName);
	j9tty_printf(portLibrary, "%s\n", buf);
	j9tty_printf(portLibrary, "\t\tLastErrorNumber: %i\n", lastErrorNumber);
	j9tty_printf(portLibrary, "\t\tLastErrorMessage: %s\n\n", portErrorBuf);

	logTestFailure(portLibrary, fileName, lineNumber, testName, lastErrorNumber, portErrorBuf, buf);

	j9mem_free_memory(portErrorBuf);
	j9mem_free_memory(buf);

	numberFailedTestsInComponent++;
}

/**
 * Display an eyecatcher prior to starting a test suite.
 */
void 
HEADING(struct J9PortLibrary *portLibrary, const char *string)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char* dash = "----------------------------------------";
	j9tty_printf(PORTLIB,"%s\n%s\n%s\n\n",dash,string,dash);
}

/**
 * Check that the file exists on the file system.
 *
 * This takes the porttest filename and line number information so that the caller is properly identified as failing.
 * 
 * @param[in] portLibrary The port library
 * @param[in] fileName File requesting file check
 * @param[in] lineNumber Line number in the file of request
 * @param[in] testName Name of the test requesting file check
 * @param[in] fileName file who's existence we need to verify
 * @param[in] lineNumber Line number in the file of request
 *
 * @return 0 if the file exists, non-zero otherwise.
 */
UDATA
verifyFileExists(struct J9PortLibrary *portLibrary, const char *pltestFileName, I_32 lineNumber, const char *testName, const char *fileName) {

	J9FileStat fileStat;
	I_32 fileStatRC = 99;
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* stat the fileName */
	fileStatRC = j9file_stat(fileName, 0, &fileStat);

	if (0 == fileStatRC) {
		if (fileStat.isFile) {
			outputComment(portLibrary, "\tfile: %s exists\n", fileName);
			return 0;
		} else {
			outputErrorMessage(portLibrary, pltestFileName, lineNumber, testName, "\tfile: %s does not exist!\n", -1, fileName);
			return -1;
		}
	} else {
		/* error in file_stat */
		outputErrorMessage(portLibrary, pltestFileName, lineNumber, testName, "\nj9file_stat call in verifyFileExists() returned %i: %s\n", fileStatRC, j9error_last_error_message());
	}

	return -1;
}


/**
 * Delete all files inside a directory ...
 *
 * @param[in] portLibrary The port library
 * @param[in] directory to clean up
 *
 * @return void
 */
void
deleteControlDirectory(struct J9PortLibrary *portLibrary, char* baseDir) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct J9FileStat buf;
	char resultBuffer[J9SH_MAXPATH];

	j9file_stat(baseDir, (U_32)0, &buf);

	if (buf.isDir != 1) {
		j9file_unlink(baseDir);
	} else {
		UDATA handle, rc;
		char mybaseFilePath[J9SH_MAXPATH];

		j9str_printf(PORTLIB, mybaseFilePath, J9SH_MAXPATH, "%s/*", baseDir);
		rc = handle = j9file_findfirst(mybaseFilePath, resultBuffer);
		while (-1 != rc) {
			j9file_unlink(resultBuffer);
			rc = j9file_findnext(handle, resultBuffer);
		}
		if (handle != -1) {
			j9file_findclose(handle);
		}
		j9file_unlinkdir(baseDir);
	}
}