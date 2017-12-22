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
 * Exercise the API for port library nls  operations.  These functions 
 * can be found in the file @ref j9nls.c  

 * @TODO integrate tests properly
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9cfg.h"
#include "j9port.h"


#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "portnls.h"

/**
 * Verify port library properly setup to run nls tests
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
I_32
j9nls_verify_function_slots(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9nls_verify_function_slots";

	reportTestEntry(portLibrary, testName);

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_set_locale) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->j9nls_set_locale is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_get_language) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_get_language is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_get_region) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_get_region is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_get_variant) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_get_variant is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_printf is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_vprintf is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_lookup_message is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_set_catalog) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_set_catalog is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_free_cached_data) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_free_cached_data is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_shutdown is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->nls_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->nls_startup is NULL\n");
	}

	return reportTestExit(portLibrary, testName);

}


/**
 * Exercise the basic nls api.
 * 
 * @ref j9nls.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
I_32
j9nls_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9nls_test1";
	UDATA rc = 0;
		
	reportTestEntry(portLibrary, testName);
	
	j9tty_printf(portLibrary, "\tThe following allocation failure message indicates the test is behaving properly\n");
	j9nls_printf(portLibrary, J9NLS_ERROR, J9NLS_PORT_FILE_MEMORY_ALLOCATE_FAILURE, "this is a default string");
	
	return reportTestExit(portLibrary, testName);
}

/**
 * Exercise API related to port library nls 
 * @ref j9nls.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9nls_runTests(struct J9PortLibrary *portLibrary)
{
		
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = TEST_FAIL;
	
	HEADING(portLibrary, "NLS");
	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9nls_verify_function_slots(portLibrary);

	if (TEST_PASS == rc) {
		rc |= j9nls_test1(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nNLS tests done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
