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

/*
 * $RCSfile: j9riTest.c,v $
 * $Revision: 1.5 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library signal handling.
 * 
 * Exercise the API for port library signal handling.  These functions 
 * can be found in the file @ref j9signal.c  
 * 
 */
 
#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9port.h"
#include "j9ri.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif


static U_32 portTestOptionsGlobal;

static I_32 j9ri_test0(struct J9PortLibrary *portLibrary);

/**
 * Verify port library runtime instrumentation.
 * 
 * @param[in] portLibrary 	the port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9ri_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc;
	

	/* Display unit under test */
	HEADING(portLibrary, "Runtime Instrumentation test");

	rc = j9ri_test0(portLibrary);

	/* Output results */
	j9tty_printf(PORTLIB, "Runtime Instrumentation test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

/**
 * Sanity check for the j9ri calls; verify that functions do not fail catastrophically.
 * Does not check the results of the calls.
 * @return 0 on success.
 */
static I_32
j9ri_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9ri_test0";
	struct riControlBlock testControlBlock;
	struct J9RIParameters riParams;
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	U_32 initializeIteration;
#endif

	memset(&testControlBlock, 0, sizeof(testControlBlock));
	memset(&riParams, 0xDEADBEEF, sizeof(riParams)); /* put crud in to ensure j9ri_params_init() does its job */
	reportTestEntry(portLibrary, testName);
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	j9ri_params_init(&riParams, &testControlBlock);
	for (initializeIteration = 0; initializeIteration < 2; ++initializeIteration) {
		U_32 enableIteration;

		outputComment(portLibrary, "j9ri_test0 initialize\n");
		j9ri_initialize(&riParams);
		for (enableIteration = 0; enableIteration < 2; ++enableIteration) {
			outputComment(portLibrary, "j9ri_test0 enable\n");
			j9ri_enable(&riParams);
			outputComment(portLibrary, "j9ri_test0 disable\n");
			j9ri_disable(&riParams);
		}
		outputComment(portLibrary, "j9ri_test0 deinitialize\n");
		j9ri_deinitialize(&riParams);
	}
#else
	outputComment(portLibrary, "j9ri_test0 not yet implemented on this platform\n");
#endif
	return reportTestExit(portLibrary, testName);
}

