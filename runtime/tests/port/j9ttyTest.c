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
 * $RCSfile: j9ttyTest.c,v $
 * $Revision: 1.31 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify TTY operations
 * 
 * Exercise the API for port library functions in @ref j9tty.c
 * 
 * @note TTY operations are not optional in the port library table.
 * 
 * @internal 
 * @note TTY operations are mainly helper functions for @ref j9file.c::j9file_vprintf "j9file_vprintf" and 
 * @ref j9fileText.c::j9file_write_file "j9file_write_file()".  Verify that the TTY operations call the appropriate functions.
 * 
 */
#include <string.h>
#include <stdio.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"

#define J9TTY_STARTUP_PRIVATE_RETURN_VALUE ((I_32) 0xDEADBEEF) /** <@internal A known return for @ref fake_j9tty_startup */

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref j9file.c::j9file_vprintf "j9file_vprintf()"
 */
typedef void (*J9FILE_VPRINTF_FUNC)(struct OMRPortLibrary *portLibrary, IDATA fd, const char *format, va_list args);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref j9fileText.c::j9file_write_text "j9file_write_text()"
 */
typedef IDATA (*J9FILE_WRITE_TEXT_FUNC)(struct OMRPortLibrary *portLibrary, IDATA fd, const char *buf, IDATA nbytes);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref j9tty.c::j9tty_vprintf "j9tty_vprintf()"
 */
typedef void (*J9TTY_VPRINTF_FUNC)(struct OMRPortLibrary *portLibrary, const char *format, va_list args);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref j9tty.c::j9tty_vprintf "j9tty_vprintf()"
 */
typedef void (*J9TTY_ERR_VPRINTF_FUNC)(struct OMRPortLibrary *portLibrary, const char *format, va_list args);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref j9file.c::j9file_write_text" j9file_write_text()"
 */
typedef I_32  (*J9TTY_STARTUP_FUNC)(struct OMRPortLibrary *portLibrary);

/**
 * @internal
 * Verify helpers functions call correct functions.
 * 
 * Override @ref j9tt.c::j9tty_startup "j9tty_startup()" and return a known value.
 *
 * @param[in] portLibrary The port library
 *
 * @return J9TTY_STARTUP_PRIVATE_RETURN_VALUE.
 */
static I_32
fake_j9tty_startup(struct OMRPortLibrary *portLibrary)
{
	return J9TTY_STARTUP_PRIVATE_RETURN_VALUE;
}

/**
 * @internal
 * Verify helpers functions call correct functions.
 * 
 * Override @ref j9file.c::j9file_vprintf  "j9file_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function.
 *
 * @ref j9file.c::j9file_vprintf  "j9file_vprintf()" does not return a value, the caller needs a way to know this overriding
 * function has been called.  As a result this function simply replaces the port library function table slot tty_startup with
 * a second overriding function.  The caller then calls @ref j9tty.c::j9tty_startup "j9tty_startup()" to verify they have
 * indeed overridden @ref j9file.c::j9file_vprintf  "j9file_vprintf()" .
 *  
 * @param[in] portLibrary The port library
 * @param[in] fd File descriptor to write.
 * @param[in] format The format String.
 * @param[in] args Variable argument list.
 * 
 * @note The caller is reponsible for restoring the tty_startup slot in the port library function table.
 */
void
fake_j9file_vprintf(struct OMRPortLibrary *portLibrary, IDATA fd, const char *format, va_list args)
{
	/* No need to save the real value, it is not restored here */
	portLibrary->tty_startup = fake_j9tty_startup;
}

/**
 * @internal
 * Verify helpers functions call correct functions.
 * 
 * Override @ref j9tty.c::j9tty_vprintf  "j9tty_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function.
 *
 * @ref j9tty.c::j9tty_vprintf  "j9tty_vprintf()" does not return a value, the caller needs a way to know this overriding
 * function has been called.  As a result this function simply replaces the port library function table slot tty_startup with
 * a second overriding function.  The caller then calls @ref j9tty.c::j9tty_startup "j9tty_startup()" to verify they have
 * indeed overridden @ref j9tty.c::j9tty_vprintf  "j9tty_vprintf()" .
 *  
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] args Variable argument list..
 * 
 * @note The caller is reponsible for restoring the tty_startup slot in the port library function table.
 */
void
fake_j9tty_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args)
{
	/* No need to save the real value, it is not restored here */
	portLibrary->tty_startup = fake_j9tty_startup;
}

/**
 * @internal
 * Verify helpers functions call correct functions.
 * 
 * Override @ref j9tty.c::j9tty_err_vprintf  "j9tty_err_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function.
 *
 * @ref j9tty.c::j9tty_err_vprintf  "j9tty_err_vprintf()" does not return a value, the caller needs a way to know this overriding
 * function has been called.  As a result this function simply replaces the port library function table slot tty_startup with
 * a second overriding function.  The caller then calls @ref j9tty.c::j9tty_startup "j9tty_startup()" to verify they have
 * indeed overridden @ref j9tty.c::j9tty_err_vprintf  "j9tty_err_vprintf()" .
 *  
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] args Variable argument list..
 * 
 * @note The caller is reponsible for restoring the tty_startup slot in the port library function table.
 */
void
fake_j9tty_err_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args)
{
	/* No need to save the real value, it is not restored here */
	portLibrary->tty_startup = fake_j9tty_startup;
}

/**
 * Verify port library TTY operations.
 * 
 * Ensure the library table is properly setup to run TTY tests.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9tty_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test0";

	reportTestEntry(portLibrary, testName);

	/* The TTY functions should be implemented in terms of other functions.  Ensure those function
	 * pointers are correctly setup.  The API for TTY functions clearly state these relationships.
	 * @TODO make this true
	 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_vprintf is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->file_write_text) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_write_text is NULL\n");
	}

	/* Verify that the TTY function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibrary, it is not re-entrant safe
	 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_shutdown is NULL\n");
	}

	/* j9tty_test1 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_printf is NULL\n");
	}

	/* j9tty_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_vprintf is NULL\n");
	}

	/* j9tty_test3 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_get_chars) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_get_chars is NULL\n");
	}

	/* j9tty_test4 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_err_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_err_printf is NULL\n");
	}

	/* j9tty_test5 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_err_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_err_vprintf is NULL\n");
	}

	/* j9tty_test6 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->tty_available) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_available is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library TTY operations.
 * 
 * @ref j9tty.c::j9tty_printf "j9tty_printf()" is a helper function for
 * @ref j9tty.c::j9tty_vprintf "j9tty_vprintf()".  Ensure that relationship is properly set up.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test1";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9TTY_VPRINTF_FUNC real_tty_vprintf;
	I_32  rc;

	reportTestEntry(portLibrary, testName);

	/* Save the real function pointers from the port library table */
	real_tty_vprintf = OMRPORT_FROM_J9PORT(portLibrary)->tty_vprintf;
	real_tty_startup = OMRPORT_FROM_J9PORT(portLibrary)->tty_startup;

	/* Override the tty_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORT_FROM_J9PORT(portLibrary)->tty_vprintf = fake_j9tty_vprintf;
	
	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	j9tty_printf(PORTLIB, "You should not see this\n");
	rc = j9tty_startup();

	/* Restore the function pointers */
	OMRPORT_FROM_J9PORT(portLibrary)->tty_vprintf = real_tty_vprintf;
	OMRPORT_FROM_J9PORT(portLibrary)->tty_startup = real_tty_startup ;
	
	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9tty_printf() does not call j9tty_vprintf()\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * @internal
 * Wrapper to call j9tty_vprintf
 */
static void
j9tty_vprintf_wrapper(struct J9PortLibrary *portLibrary, const char *format, ...)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	va_list args;

	va_start(args, format);
	j9tty_vprintf(format, args);
	va_end(args);
}

/**
 * Verify port library TTY operations.
 * 
 * @ref j9tty.c::j9tty_vprintf "j9tty_vprintf()" is a helper function for
 * @ref j9file.c::j9file_vprintf "j9file_vprintf()".  Ensure that relationship is properly set up.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test2(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test2";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9FILE_VPRINTF_FUNC real_file_vprintf;
	const char *format = "You should not see this %d %d %d\n";
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Save the real function pointers from the port library table */
	real_file_vprintf = OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf;
	real_tty_startup = OMRPORT_FROM_J9PORT(portLibrary)->tty_startup;

	/* Override the file_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf = fake_j9file_vprintf;
	
	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	j9tty_vprintf_wrapper(portLibrary, format, 1, 2, 3);
	rc = j9tty_startup();

	/* Restore the function pointers */
	OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf = real_file_vprintf;
	OMRPORT_FROM_J9PORT(portLibrary)->tty_startup = real_tty_startup ;
	
	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9tty_vprintf() does not call j9file_vprintf()\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library TTY operations.
 * Verify @ref j9tty.c::j9tty_get_chars.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test3(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test3";

	reportTestEntry(portLibrary, testName);

	/* TODO implement */

	return reportTestExit(portLibrary, testName);
}


/**
 * Verify port library TTY operations.
 * Verify @ref j9tty.c::j9tty_err_printf.
 * 
 * @ref j9tty.c::j9tty_err_printf "j9tty_err_printf()" is a helper function for
 * @ref j9tty.c::j9tty_err_vprintf "j9tty_err_vprintf()".  Ensure that relationship is properly set up.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test4(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test4";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9TTY_VPRINTF_FUNC real_tty_vprintf;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Save the real function pointers from the port library table */
	real_tty_vprintf = OMRPORT_FROM_J9PORT(portLibrary)->tty_vprintf;
	real_tty_startup = OMRPORT_FROM_J9PORT(portLibrary)->tty_startup;

	/* Override the file_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORT_FROM_J9PORT(portLibrary)->tty_err_vprintf = fake_j9tty_vprintf;
	
	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	j9tty_err_printf(PORTLIB, "You should not see this\n");
	rc = j9tty_startup();

	/* Restore the function pointers */
	OMRPORT_FROM_J9PORT(portLibrary)->tty_vprintf = real_tty_vprintf;
	OMRPORT_FROM_J9PORT(portLibrary)->tty_startup = real_tty_startup ;
	
	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9tty_err_printf() does not call j9tty_err_vprintf()\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library TTY operations.
 * 
 * @ref j9tty.c::j9tty_err_vprintf "j9tty_err_vprintf()" is a helper function for
 * @ref j9file.c::j9file_vprintf "j9file_vprintf()".  Ensure that relationship is properly set up.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test5(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test5";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9FILE_VPRINTF_FUNC real_file_vprintf;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Save the real function pointers from the port library table */
	real_file_vprintf = OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf;
	real_tty_startup = OMRPORT_FROM_J9PORT(portLibrary)->tty_startup;

	/* Override the file_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf = fake_j9file_vprintf;
	
	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	j9tty_err_printf(PORTLIB, "You should not see this\n");
	rc = j9tty_startup();

	/* Restore the function pointers */
	OMRPORT_FROM_J9PORT(portLibrary)->file_vprintf = real_file_vprintf;
	OMRPORT_FROM_J9PORT(portLibrary)->tty_startup = real_tty_startup ;
	
	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9tty_err_printf() does not call j9file_vprintf()\n");
	}

	return reportTestExit(portLibrary, testName);
}


/**
 * Verify port library TTY operations.
 * Verify @ref j9tty.c::j9tty_available.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test6(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test6";

	reportTestEntry(portLibrary, testName);

	/* TODO implement */

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library TTY operations.
 *  
 * Print a message to the terminal.  If the user doesn't see anything then it has failed.
 * This test is included so the user has a better feel that things actually worked.  The
 * rest of the tests were verifying function relationships.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9tty_test7(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_test7";
	const char *format = "TTY printf, check check %d %d %d check ...\n";

	reportTestEntry(portLibrary, testName);

	/* Use printf to indicate something is expected on the terminal */
	j9tty_printf(PORTLIB, format, 1, 2, 3);
	j9tty_printf(PORTLIB, "New line\n");

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library TTY operations.
 * 
 * Exercise all API related to port library string operations found in
 * @ref j9tty.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return0 on success, -1 on failure
 */
I_32 
j9tty_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "TTY test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9tty_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9tty_test1(portLibrary);
		rc |= j9tty_test2(portLibrary);
		rc |= j9tty_test3(portLibrary);
		rc |= j9tty_test4(portLibrary);
		rc |= j9tty_test5(portLibrary);
		rc |= j9tty_test6(portLibrary);
		rc |= j9tty_test7(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nTTY test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;	
}

 

