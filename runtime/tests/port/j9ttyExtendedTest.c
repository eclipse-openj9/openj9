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
 * @brief Verify TTY operations
 * 
 * Exercise the j9tty_daemonize API for port library functions in @ref j9tty.c
 * 
 * 
 */
#include <string.h>
#include <stdio.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"

/**
 * Verify that a call to j9tty_printf/j9tty_vprintf after a call to j9tty_daemonize does not crash.
 *
 * Call j9tty_daemonize, then call j9tty_printf/j9tty_vprintf.  If we don't crash then the test has failed. Note
 * that this test must be run on its own because the call to j9tty_daemonize can affect other tests
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9tty_daemonize_test(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9tty_daemonize_test";
	const char *format = "Test string %d %d %d\n";

	reportTestEntry(portLibrary, testName);

	j9tty_printf(PORTLIB, "pre-daemonize\n");
	j9tty_daemonize();
	/* outputComment uses j9tty_vprintf */
	outputComment(PORTLIB, format , 1, 2, 3);
	j9tty_printf(PORTLIB, "post-daemonize\n");

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
j9tty_runExtendedTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = 0;

	/* Display unit under test */
	HEADING(portLibrary, "Extended TTY test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc |= j9tty_daemonize_test(portLibrary);

	/* Output results */
	j9tty_printf(PORTLIB, "\nExtended TTY test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;	
}

 

