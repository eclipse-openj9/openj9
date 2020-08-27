/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
 * $RCSfile: j9sockTest.c,v $
 * $Revision: 1.49 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library file system.
 * 
 * Exercise the API for port library socket system operations.  These functions 
 * can be found in the file @ref j9sock.c  
 * 
 * @note port library socket operations are optional in the port library table.
 * @note The default processing if memory mapping is not supported by the OS is to allocate
 *       memory and read in the file.  This is the equivalent of copy-on-write mmapping
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9cfg.h"
#include "j9port.h"
#include "portsock.h"
#include "j9socket.h"

#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9fileTest.h"



#if 0
#define SOCKTEST_DBG
#endif

/**
 * Verify port library properly setup to run j9sock tests
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9sock_verify_function_slots(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_verify_function_slots";

	reportTestEntry(portLibrary, testName);

	if (NULL == portLibrary->sock_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_startup is NULL\n");
	}

	if (NULL == portLibrary->sock_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_shutdown is NULL\n");
	}

	if (NULL == portLibrary->sock_gethostbyname) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_gethostbyname is NULL\n");
	}
	

	return reportTestExit(portLibrary, testName);

}


I_32
j9sock_test10_get_addrinfo(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test10_get_addrinfo";
#ifdef IPv6_FUNCTION_SUPPORT
	j9addrinfo_struct res;
	j9addrinfo_t hints;
	struct addrinfo *addrinf;
	BOOLEAN value = 1;
	I_32 length = 0;
	I_32 i = 0;
#endif

	reportTestEntry(portLibrary, testName);

#ifdef IPv6_FUNCTION_SUPPORT

	memset(&hints, 0, sizeof(hints));
	if (0 != j9sock_getaddrinfo_create_hints(&hints, (I_16) J9ADDR_FAMILY_UNSPEC, SOCK_STREAM, J9PROTOCOL_FAMILY_UNSPEC, 0)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (NULL == hints) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints, hints == NULL\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo("localhost", hints, &res)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo_length(&res, &length)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_length: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	for (i = 0; i < length; i++) {
		U_32 scopeID = 0;
		I_32 family = 0;

		I_32 bytesToCopy = 0;

		if (0 != j9sock_getaddrinfo_family(&res, &family, i)){
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_family: %s\n", j9error_last_error_message());
			return reportTestExit(portLibrary, testName);
		}
		bytesToCopy = (J9ADDR_FAMILY_AFINET4 == family) ? J9SOCK_INADDR_LEN	: J9SOCK_INADDR6_LEN;
		addrinf = res.addr_info;

#if defined(SOCKTEST_DBG)
		j9tty_printf(portLibrary, "# of results: %d\n", length);
		j9tty_printf(portLibrary, "family: %d\n", family);
		if (family == J9ADDR_FAMILY_AFINET4) {
			j9tty_printf(portLibrary, "socktype: %d\n", &addrinf.ai_socktype);
			j9tty_printf(portLibrary, "protocol: %d\n", &addrinf.ai_protocol);
		}
#endif
	}

	if (0 != j9sock_freeaddrinfo(&res)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_freeaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}
#endif
	return reportTestExit(portLibrary, testName);
}

I_32
j9sock_test11_getnameinfo(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test11_getnameinfo";
#ifdef IPv6_FUNCTION_SUPPORT
	char hostname3[1025];
	j9addrinfo_struct res;
	j9addrinfo_t hints;
	I_32 family;
	I_32 i = 0;
	U_8 nipAddr[16];
	I_32 numOfAddrinfo = 0;
	U_32 scope_id = 0;
	I_32 index = 0;
	I_32 nameLength = 0;
#define CANONNAMEFLAG 0x02
#endif

	reportTestEntry(portLibrary, testName);
#ifdef IPv6_FUNCTION_SUPPORT
	memset(&hints, 0, sizeof(hints));

	if (0 != j9sock_getaddrinfo_create_hints(&hints, (I_16) J9ADDR_FAMILY_UNSPEC, SOCK_STREAM, J9PROTOCOL_FAMILY_UNSPEC, CANONNAMEFLAG)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (NULL == hints) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints, hints == NULL\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo("127.0.0.1", hints, &res)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo_length(&res, &numOfAddrinfo)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_length: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	for (i = 0; i < numOfAddrinfo; i++) {
	    if (0!= j9sock_getaddrinfo_family(&res, &family, i)){
	    	outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_family: %s\n", j9error_last_error_message());
	    	return reportTestExit(portLibrary, testName);
	    }
	    if ((family == J9ADDR_FAMILY_AFINET4)){
	    	if (0 != j9sock_getaddrinfo_address(&res, (U_8 *) nipAddr, i, &scope_id)){
	    		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_address: %s\n", j9error_last_error_message());
	    		return reportTestExit(portLibrary, testName);
	    	}
	    	if (0 != j9sock_getaddrinfo_name(&res, hostname3, 0)){
	    		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_name: %s\n", j9error_last_error_message());
	    		return reportTestExit(portLibrary, testName);
	    	}
	    	break;
	    }
	}

	if (0 != j9sock_freeaddrinfo(&res)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_freeaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

#endif
	return reportTestExit(portLibrary, testName);
}


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
j9sock_runTests(struct J9PortLibrary *portLibrary)
{
		
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	int rc = TEST_FAIL;
	
	HEADING(portLibrary, "Sockets");
	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9sock_verify_function_slots(portLibrary);

	if (TEST_PASS == rc) {
		rc |= j9sock_test10_get_addrinfo(portLibrary);
		rc |= j9sock_test11_getnameinfo(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nsocket tests done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;

}
