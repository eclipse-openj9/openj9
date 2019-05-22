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
 * $RCSfile: j9errorTest.c,v $
 * $Revision: 1.32 $
 * $Date: 2011-12-30 14:15:00 $
 */
 
/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library error handling operations.
 * 
 * Exercise the API for port library error handling operations.  These functions 
 * can be found in the file @ref j9error.c  
 * 
 * @note port library error handling operations are not optional in the port library table.
 * 
 * @TODO make this multi-threaded.
 * 
 */
#include <stdlib.h>
#include <string.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"

/**
 * Verify port library error handling operations.
 * 
 * Ensure the library table is properly setup to run error handling tests.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9error_test0(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9error_test0";
	PORT_ACCESS_FROM_PORT(portLibrary);
	reportTestEntry(portLibrary, testName);
	 
	/* Verify that the error handling function pointers are non NULL */
	
	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibrary, it is not re-entrant safe
	 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->error_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->error_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_shutdown is NULL\n");
	}

	/* j9error_test1 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->error_last_error_number) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_last_error_number is NULL\n");
	}

	/* j9error_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->error_last_error_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_last_error_message is NULL\n");
	}

	/* j9error_test1 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->error_set_last_error) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_set_last_error is NULL\n");
	}

	/* j9error_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->error_set_last_error_with_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_set_last_error_with_message is NULL\n");
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library error handling operations.
 * 
 * Error codes are stored in per thread buffers so errors reported by one thread
 * do not effect those reported by a second.  Errors stored via the helper
 * function @ref j9error.c::j9error_set_last_error "j9error_set_last_error()" are recorded in the per thread
 * buffers without an error message.
 * Verify the @ref j9error_last_error_number "j9error_last_error_number()" returns
 * the correct portable error code.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9error_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9error_test1";

	I_32 errorCode;

	reportTestEntry(portLibrary, testName);

	/* Delete the ptBuffers */
	j9port_tls_free();
	
	/* In theory there is now nothing stored, if we really did free the buffers.
	 * Guess it is time to find out
	 */
	errorCode = j9error_last_error_number();
	if (0 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number() returned %d expected %d\n", errorCode, 0);
	}

	/* Set an error code, verify it is what we get back */
	errorCode = j9error_set_last_error(100, 200);
	if (200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_set_last_error() returned %d expected %d\n", errorCode, 200);
	}
	errorCode = j9error_last_error_number();
	if (200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number() returned %d expected %d\n", errorCode, 200);
	}

	/* Again with negative values */
	errorCode = j9error_set_last_error(100, -200);
	if (-200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_set_last_error() returned %d expected %d\n", errorCode, 200);
	}
	errorCode = j9error_last_error_number();
	if (-200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number() returned %d expected %d\n", errorCode, 200);
	}

	/* Delete the ptBuffers, verify no error is stored */
	j9port_tls_free();
	errorCode = j9error_last_error_number();
	if (0 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number() returned %d expected %d\n", errorCode, 0);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library error handling operations.
 * 
 * Error codes are stored in per thread buffers so errors reported by one thread
 * do not effect those reported by a second.  Errors stored via the helper
 * function @ref j9error.c::j9error_set_last_error_with_message "j9error_set_last_error_with_message()" 
 * are recorded in the per thread buffers without an error message.
 * Verify the @ref j9error_last_error_message "j9error_last_error_message()" returns
 * the correct message.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9error_test2(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9error_test2";

	const char *message;
	const char *knownMessage = "This is a test";
	const char *knownMessage2 = "This is also a test";
	I_32 errorCode;

	reportTestEntry(portLibrary, testName);

	/* Delete the ptBuffers */
	j9port_tls_free();
	
	/* In theory there is now nothing stored, if we really did free the buffers.
	 * Guess it is time to find out
	 */
	message = j9error_last_error_message();
	if (0 != strlen(message)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned message of length %d expected %d\n", strlen(message), 1);
	}

	/* Set an error message, verify it is what we get back */
	errorCode = j9error_set_last_error_with_message(200, knownMessage);
	message = j9error_last_error_message();
	if (200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_set_last_error_with_message() returned %d expected %d\n", errorCode, 200);
	}
	if (strlen(message) != strlen(knownMessage)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned length %d expected %d\n", strlen(message), strlen(knownMessage));
	}
	if (0 != memcmp(message, knownMessage, strlen(knownMessage))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned \"%s\" expected \"%s\"\n", message, knownMessage);
	}

	/* Again, different message */
	errorCode = j9error_set_last_error_with_message(100, knownMessage2);
	message = j9error_last_error_message();
	if (100 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_set_last_error_with_message() returned %d expected %d\n", errorCode, 100);
	}
	if (strlen(message) != strlen(knownMessage2)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned length %d expected %d\n", strlen(message), strlen(knownMessage2));
	}
	if (0 != memcmp(message, knownMessage2, strlen(knownMessage2))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned \"%s\" expected \"%s\"\n", message, knownMessage2);
	}

	/* A null message, valid test?*/
#if 0
	errorCode = j9error_set_last_error_with_message(-100, NULL);
	message = j9error_last_error_message();
	if (-100 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned %d expected %d\n", errorCode, -100);
	}
	if (NULL != message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned \"%s\" expected NULL\n", message);
	}
#endif

	j9port_tls_free();
	errorCode = j9error_set_last_error_with_message(-300, knownMessage);
	message = j9error_last_error_message();
	if (-300 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number() returned %d expected %d\n", errorCode, -300);
	}
	if (strlen(message) != strlen(knownMessage)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned length %d expected %d\n", strlen(message), strlen(knownMessage));
	}
	if (0 != memcmp(message, knownMessage, strlen(knownMessage))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_message() returned \"%s\" expected \"%s\"\n", message, knownMessage);
	}

	/* Delete the ptBuffers, verify no error is stored */
	j9port_tls_free();
	errorCode = j9error_last_error_number();
	if (0 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9error_last_error_number() returned %d expected %d\n", errorCode, 0);
	}

	return reportTestExit(portLibrary, testName);
}


	
/**
 * Verify port library error handling operations.
 * 
 * Exercise all API related to port library error handling operations found in
 * @ref j9error.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9error_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "error handling test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9error_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9error_test1(portLibrary);
		rc |= j9error_test2(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nerror handling test done %s\n\n", TEST_PASS == rc ? "." : ", failures detected.");
	return TEST_PASS == rc ? TEST_PASS : TEST_FAIL;
}

 

