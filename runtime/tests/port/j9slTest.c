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

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library file system.
 * 
 * Exercise the API for port library shared library system operations.  These functions 
 * can be found in the file @ref j9sl.c  

 * @TODO integrate tests properly
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9cfg.h"
#include "j9port.h"
#include "j9lib.h"


#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9fileTest.h"

/**
 * Verify port library properly setup to run sl tests
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9sl_verify_function_slots(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sl_verify_function_slots";

	reportTestEntry(portLibrary, testName);

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->sl_close_shared_library) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sl_close_shared_library is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->sl_lookup_name) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sl_lookup_name is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->sl_open_shared_library) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sl_open_shared_library is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->sl_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sl_shutdown is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->sl_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sl_startup is NULL\n");
	}

	return reportTestExit(portLibrary, testName);

}


/**
 * Basic test: load the port Library dll.
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sl_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sl_test1";
	UDATA handle;
	UDATA rc = 0;
	char *sharedLibName = NULL;

	reportTestEntry(portLibrary, testName);
	
	sharedLibName = J9_PORT_DLL_NAME;
	
	rc = j9sl_open_shared_library(sharedLibName, &handle, J9PORT_SLOPEN_DECORATE);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unable to open %s, \n", sharedLibName, j9error_last_error_message());
		goto exit;
	}
	
exit:

	return reportTestExit(portLibrary, testName);
}

#if defined(AIXPPC)

/**
 * CMVC 197740
 * The exact message varies depending on the OS version. look for any of several possible strings.
 * @param dlerrorOutput actual dlerror() result
 * @param expectedOutputs list of possible strings, terminated by a null  pointer
 * @result true if one of expectedOutputs is found in dlerrorOutput
 */
static BOOLEAN
isValidLoadErrorMessage(const char *dlerrorOutput, const char *possibleMessageStrings[])
{
	BOOLEAN result = FALSE;
	UDATA candidate = 0;
	while (NULL != possibleMessageStrings[candidate]) {
		if (NULL != strstr(dlerrorOutput, possibleMessageStrings[candidate])) {
			result = TRUE;
			break;
		}
		++candidate;
	}
	return result;
}

/**
 * Loading dll with missing dependency on AIX, expecting a descriptive OS error message
 *
 * @ref j9sock.c
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sl_AixDLLMissingDependency(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sl_AixDLLMissingDependency";
	UDATA handle;
	UDATA rc = 0;
	char *sharedLibName = "j9aixbaddep";
	const char *osErrMsg;
	const char *possibleMessageStrings[] = {"0509-150", "Dependent module dummy.exp could not be loaded", NULL};

	reportTestEntry(portLibrary, testName);

	rc = j9sl_open_shared_library(sharedLibName, &handle, J9PORT_SLOPEN_DECORATE);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, " Unexpectedly loaded %s, should have failed with dependency error\n", sharedLibName, j9error_last_error_message());
		goto exit;
	}

	osErrMsg = j9error_last_error_message();
	outputComment(portLibrary, "System error message=\n\"%s\"\n", osErrMsg);
	if (!isValidLoadErrorMessage(osErrMsg, possibleMessageStrings)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, " Cannot find valid error code, should have failed with dependency error\n", sharedLibName, j9error_last_error_message());
	}
exit:

	return reportTestExit(portLibrary, testName);
}

/**
 * Loading dll of wrong platform on AIX, expecting a descriptive OS error message
 *
 * @ref j9sock.c
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sl_AixDLLWrongPlatform(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sl_AixDLLWrongPlatform";
	UDATA handle;
	UDATA rc = 0;
	/* Intentionally load 64bit dll on 32 bit AIX, and 32bit dll on 64bit AIX */
#if defined(J9VM_ENV_DATA64)
	char *sharedLibName = "/usr/lib/jpa";
#else
	char *sharedLibName = "/usr/lib/jpa64";
#endif
	const char *osErrMsg;
	const char *possibleMessageStrings[] = {"0509-103", "System error: Exec format error",
											"0509-026", "System error: Cannot run a file that does not have a valid format",
											NULL};

	reportTestEntry(portLibrary, testName);

	rc = j9sl_open_shared_library(sharedLibName, &handle, J9PORT_SLOPEN_DECORATE);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpectedly loaded %s, should have failed with dependency error\n", sharedLibName, j9error_last_error_message());
		goto exit;
	}

	osErrMsg = j9error_last_error_message();
	outputComment(portLibrary, "System error message=\n\"%s\"\n", osErrMsg);
	if (!isValidLoadErrorMessage(osErrMsg, possibleMessageStrings)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot find valid error code, should have failed with wrong platform error\n", sharedLibName, j9error_last_error_message());
	}
exit:

	return reportTestExit(portLibrary, testName);
}
#endif /* defined(AIXPPC) */

/**
 * Verify port file system.
 * 
 * Exercise all API related to port library sockets found in
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9sl_runTests(struct J9PortLibrary *portLibrary)
{
		
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = TEST_FAIL;
	
	HEADING(portLibrary, "Shared Library (sl)");
	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9sl_verify_function_slots(portLibrary);

	if (TEST_PASS == rc) {
		rc |= j9sl_test1(portLibrary);
#if defined(AIXPPC)
		rc |= j9sl_AixDLLMissingDependency(portLibrary);
		rc |= j9sl_AixDLLWrongPlatform(portLibrary);
#endif /* defined(AIXPPC) */
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nshared library (sl) tests done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
