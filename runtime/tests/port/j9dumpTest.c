/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * @brief Verify port library core file creation.
 * 
 * Exercise the API for port library virtual memory management operations.  These functions 
 * can be found in the file @ref j9osdump.c  
 * 
 * 
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(AIXPPC)
#include <sys/types.h>
#include <sys/var.h>
#endif

#include <signal.h>

#if defined(WIN32) || defined(WIN64)
/* for getcwd() */
#include <direct.h>
#define getcwd _getcwd
#endif

#include "j9cfg.h"
#include "testHelpers.h"
#include "j9port.h"

#define allocNameSize 64 /**< @internal buffer size for name function */

static UDATA simpleHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg);

typedef struct simpleHandlerInfo {
	char *coreFileName;
	const char* testName;
} simpleHandlerInfo;

/**
 * Verify port library memory management.
 * 
 * Ensure the port library is properly setup to run core file generation operations.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9dump_verify_functiontable_slots(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9dump_verify_functiontable_slots";
	PORT_ACCESS_FROM_PORT(portLibrary);
	reportTestEntry(portLibrary, testName);
	 

	/* j9mem_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->dump_create) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->dump_create is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/** call j9dump_create() without passing in core file name. This does not actually test that a core file was actually created.
  
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 *
 */
int
j9dump_test_create_dump_with_NO_name(struct J9PortLibrary *portLibrary)
{	
	const char* testName = "j9dump_test_create_dump_with_NO_name";
	UDATA rc = 99;
	char coreFileName[EsMaxPath];
	BOOLEAN doFileVerification = FALSE;
	PORT_ACCESS_FROM_PORT(portLibrary);
#if defined(AIXPPC)
	struct vario myvar;
	int sys_parmRC;
#endif
	
	reportTestEntry(portLibrary, testName);
	
	coreFileName[0] = '\0';

#if 0	
	/* try out a NULL test (turns out this crashes) */
	rc = j9dump_create(NULL, NULL, NULL); /* this crashes */
#endif
	
	/* try out a more sane NULL test */
	outputComment(portLibrary, "calling j9dump_create with empty filename\n");
	
#if defined(J9ZOS390)
	rc = j9dump_create(coreFileName, "CEEDUMP", NULL);
#else
	rc = j9dump_create(coreFileName, NULL, NULL);
#endif
	
	if (rc == 0) {
		UDATA verifyFileRC = 99;
		
		outputComment(portLibrary, "j9dump_create claims to have written a core file to: %s\n", coreFileName);


#if defined(AIXPPC)	
		/* We defer to fork abort on AIX machines that don't have "Enable full CORE dump" enabled in smit,
		 * in which case j9dump_create will not return the filename.
		 * So, only check for a specific filename if we are getting full core dumps */
		sys_parmRC = sys_parm(SYSP_GET, SYSP_V_FULLCORE, &myvar);
		
		if ((sys_parmRC == 0) && (myvar.v.v_fullcore.value == 1) ) {

			doFileVerification = TRUE;
		}
#elif defined(LINUX)
		doFileVerification = TRUE;
#endif
		if (doFileVerification) {
			verifyFileRC = verifyFileExists(PORTTEST_ERROR_ARGS, coreFileName);
			if (verifyFileRC == 0) {
				/* delete the file */
				remove(coreFileName);			
				outputComment(portLibrary, "\tremoved: %s\n", coreFileName);
			}
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9dump_create returned: %u, with filename: %s", rc, coreFileName);
	}

	return reportTestExit(portLibrary, testName);
}


/** Test j9dump_create(), this time passing in core file name
 * Verify that the returned core file name actually exists.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 *
 * @note this only tests CEEDUMP generation on z/OS
 */
int
j9dump_test_create_dump_with_name(struct J9PortLibrary *portLibrary)
{
	const char *testName = "j9dump_test_create_dump_with_name";
	UDATA rc = 99;
	char *cwd = NULL;
	char coreFileName[EsMaxPath + 1];

	PORT_ACCESS_FROM_PORT(portLibrary);
	
	reportTestEntry(portLibrary, testName);
	
#if defined(J9ZOS390)
	cwd = atoe_getcwd(coreFileName, EsMaxPath);
#else
	cwd = getcwd(coreFileName, EsMaxPath);
#endif

	if (NULL == cwd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to get current directory\n");
		goto done;
	}

	strncat(coreFileName, "/", EsMaxPath);	/* make sure the directory ends with a slash */
	strncat(coreFileName, testName, EsMaxPath);	

	outputComment(portLibrary, "calling j9dump_create with filename: %s\n", coreFileName);
#if !defined(J9ZOS390)
	rc = j9dump_create(coreFileName, NULL, NULL);
#else
	rc = j9dump_create(coreFileName, "CEEDUMP", NULL);
#endif
	if (rc == 0) {
		UDATA verifyFileRC = 99;
		
		outputComment(portLibrary, "j9dump_create claims to have written a core file to: %s\n", coreFileName);
		verifyFileRC = verifyFileExists(PORTTEST_ERROR_ARGS, coreFileName);
		if (verifyFileRC == 0) {
			/* delete the file */
			remove(coreFileName);
			outputComment(portLibrary, "\tremoved: %s\n", coreFileName);
		}
			
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9dump_create returned: %u, with filename: %s\n", rc, coreFileName);
	}

done:
	return reportTestExit(portLibrary, testName);
}



/** 
 * Test calling j9dump_create() from a signal handler.
 * 
 * Verify that the returned core file name actually exists.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9dump_test_create_dump_from_signal_handler(struct J9PortLibrary *portLibrary)
{
	const char *testName = "j9dump_test_create_dump_from_signal_handler";
	char *cwd = NULL;
	char coreFileName[EsMaxPath + 1];
	UDATA rc = 99;
	UDATA result;
	U_32 protectResult;
	simpleHandlerInfo handlerInfo;
	U_32 sig_protectFlags;

	PORT_ACCESS_FROM_PORT(portLibrary);
	
#if defined(J9ZOS390)
	cwd = atoe_getcwd(coreFileName, EsMaxPath);
#else
	cwd = getcwd(coreFileName, EsMaxPath);
#endif

	if (NULL == cwd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to get current directory\n");
		goto done;
	}

	strncat(coreFileName, "/", EsMaxPath);	/* make sure the directory ends with a slash */
	strncat(coreFileName, testName, EsMaxPath);	

	sig_protectFlags = J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_MAY_RETURN;

	reportTestEntry(portLibrary, testName);
	
	if (!j9sig_can_protect(sig_protectFlags)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect does not offer adequate protection for test\n");
	} else {
		handlerInfo.coreFileName = coreFileName;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			raiseSEGV, NULL, 
			simpleHandlerFunction, &handlerInfo, 
			sig_protectFlags,
			&result);
		
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 return value\n");
		}
	}

done:
	return reportTestExit(portLibrary, testName);
}


static UDATA 
simpleHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	simpleHandlerInfo* info = handler_arg;
	const char* testName = info->testName;
	UDATA rc;
	
	outputComment(portLibrary, "calling j9dump_create with filename: %s\n", info->coreFileName);
	
#if defined(J9ZOS390)
	rc = j9dump_create(info->coreFileName, "CEEDUMP", NULL);
#else
	rc = j9dump_create(info->coreFileName, NULL, NULL);
#endif

	if (rc == 0) {
		UDATA verifyFileRC = 99;
		
		outputComment(portLibrary, "j9dump_create claims to have written a core file to: %s\n", info->coreFileName);
		verifyFileRC = verifyFileExists(PORTTEST_ERROR_ARGS, info->coreFileName);
		if (verifyFileRC == 0) {
			/* delete the file */
			remove(info->coreFileName);
			outputComment(portLibrary, "\tremoved: %s\n", info->coreFileName);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9dump_create returned: %u, with filename: %s", rc, info->coreFileName);
	}
	
	return J9PORT_SIG_EXCEPTION_RETURN;
}


/* test calling j9dump_create from a signal handler */

/**
 * Verify port library memory management.
 * 
 * Exercise all API related to port library memory management found in
 * @ref j9osdump.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9dump_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "dump tests");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9dump_verify_functiontable_slots(portLibrary);
	j9tty_printf(PORTLIB, "\n");
	if (TEST_PASS == rc) {
		rc |= j9dump_test_create_dump_with_name(portLibrary);
		j9tty_printf(PORTLIB, "\n");
		rc |= j9dump_test_create_dump_from_signal_handler(portLibrary);
		j9tty_printf(PORTLIB, "\n");
		rc |= j9dump_test_create_dump_with_NO_name(portLibrary);

	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nDump test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
