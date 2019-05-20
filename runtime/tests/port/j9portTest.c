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
 * $RCSfile: j9portTest.c,v $
 * $Revision: 1.42 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library management operations.
 * 
 * Exercise the API for port library management.  These functions 
 * can be found in the file @ref j9port.c  The majority of these functions are exported
 * by the port library and are not in the actual port library table, however there
 * are a few.
 * 
 * @note port library maintenance operations are not optional in the port library table.
 */
#include <stdlib.h>
#include <string.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"

/**
 * @internal
 * Some platforms can't handle large objects on their stack, so use a global
 * version of the portlibrary for testing.
 */
J9PortLibrary portLibraryToTest;

/**
 * @internal
 * Some platforms can't handle large objects on their stack, so use a global
 * version of a large buffer representing memory for a port library.
 * @{
 */
#define bigBufferSize (2*sizeof(J9PortLibrary))
char bigBuffer[bigBufferSize];
/** @} */

/**
 * @internal
 * Initialize version structure.
 * 
 * Given a port library version structure and values for the fields ensure the
 * structure is properly initialized
 * 
 * @param[in,out] version The J9PortLibraryVersion structure
 * @param[in] majorVersionNumber Value of the major version number
 * @param[in] minorVersionNumber Value of the minor version number
 * @param[in] capabilities Value of the capabilities
 */
static void
setupVersionInformation(struct J9PortLibraryVersion *version, U_16 majorVersionNumber, U_16 minorVersionNumber, U_64 capabilities)
{
	memset(version, 0, sizeof(J9PortLibraryVersion));
	version->majorVersionNumber = majorVersionNumber;
	version->minorVersionNumber = minorVersionNumber;
	version->capabilities = capabilities;
	version->padding = 0xFFFFDDDD;
}

/**
 * @internal
 * @def
 * Verify version structure.
 * 
 * Set which fields of the version structure to be verified, allowing tests
 * for mismatched fields to not display false failures
 * @{
 */
#define J9VERSION_VERIFY_ALL ((UDATA)0xff)
#define J9VERSION_VERIFY_MAJOR ((UDATA)0x1)
#define J9VERSION_VERIFY_MINOR ((UDATA)0x2)
#define J9VERSION_VERIFY_CAPABILITIES ((UDATA)0x4)
/** @} */

/**
 * @internal
 * Verify version structure.
 *  
 * Given a port library (fakePortLibrary) verify that it's version information 
 * matches the expected value.
 * 
 * @param[in] portLibrary The port used for displaying messages etc.
 * @param[in] testNmae The test requesting the verification
 * @param[in] fakePortLibrary The port library under test
 * @param[in] expectedVersion The expected version information
 * @param[in] fields Which fields to verify
 */
static void
verifyVersionInformation(struct J9PortLibrary *portLibrary, const char *testName, struct J9PortLibrary *fakePortLibrary, struct J9PortLibraryVersion *expectedVersion, UDATA fields)
{
	J9PortLibraryVersion actualVersion;
	U_32 rc;

	memset(&actualVersion, 0, sizeof(J9PortLibraryVersion));
	rc = j9port_getVersion(fakePortLibrary, &actualVersion);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() returned %d expected 0\n", rc);
		return;
	}

	if (0 != (fields & J9VERSION_VERIFY_MAJOR))  {
		if (actualVersion.majorVersionNumber != expectedVersion->majorVersionNumber) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() majorVersion returned %d expected %d\n", actualVersion.majorVersionNumber, expectedVersion->majorVersionNumber);		
		}
	}

	if (0 != (fields & J9VERSION_VERIFY_MINOR))  {
		if (actualVersion.minorVersionNumber != expectedVersion->minorVersionNumber) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() minorVersion returned %d expected %d\n", actualVersion.minorVersionNumber, expectedVersion->minorVersionNumber);
		}
	}

	if (0 != (fields & J9VERSION_VERIFY_CAPABILITIES))  {
		if (actualVersion.capabilities != expectedVersion->capabilities) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() capabilites returned %d expected %d\n", actualVersion.capabilities, expectedVersion->capabilities);
		}
	}
}

/** 
 * @internal
 * Verify J9PortLibrary structure.
 * 
 * Given a port library structure, verify that it is correctly setup.
 * 
 * @param[in] portLibrary the port library to verify
 * @param[in] testName test requesting creation
 * @param[in] version the version of port library to verify
 * @param[in] size the expected size
 */
static void
verifyCreateTableContents(struct J9PortLibrary *portLibrary, const char *testName, struct J9PortLibraryVersion *version, UDATA size)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	(void)j9port_getSize(version);

	/* portGlobals should not be allocated */
	if (NULL != OMRPORT_FROM_J9PORT(PORTLIB)->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() portGlobals pointer is not NULL\n");
	}
	
	/* Not self allocated */
	if (NULL != portLibrary->self_handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() self_handle pointer is not NULL\n");
	}

	/* Verify the pointers look correct, must skip version and portGlobals portion information */

	/* reach into the port library for the various values of interest */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->tty_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() tty_printf pointer is NULL\n");
	}
} 

/**
 * @internal
 * Verify port library startup.
 * 
 * A known failure reason for port library startup
 */
static const I_32 failMemStartup = -100;

/**
 * @internal
 * Verify port library startup.
 * 
 * A startup function that can be put into a port library to cause failure
 * at startup.
 * 
 * @param[in] portLibrary the port library
 * @param[in] portGlobalSize size of portGlobal structure
 * 
 * @return negative error code
 */
static I_32
fake_mem_startup(struct OMRPortLibrary *portLibrary, UDATA portGlobalSize)
{
	return failMemStartup;
}

/**
 * @internal
 * Verify port library startup.
 * 
 * A startup function that can be put into a port library to cause failure
 * at startup.
 * 
 * @param[in] portLibrary the port library
 * 
 * @return negative error code
 */
static I_32
fake_time_startup(struct OMRPortLibrary *portLibrary)
{
	return failMemStartup;
}

/**
 * Verify port library management.
 * 
 * Ensure the port library is properly setup to run port library maintenance tests.
 * 
 * Most operations being tested are not stored in the port library table
 * as they are required for port library allocation, startup and shutdown.
 * There are a couple to look up.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9port_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9port_test0";

	reportTestEntry(portLibrary, testName);
	 
	/* Verify that the j9port function pointers are non NULL */

	/* j9port_test7 */
	if (NULL == portLibrary->port_shutdown_library) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->port_shutdown_library is NULL\n");
	}

	/* j9port_test9 */
	if (NULL == portLibrary->port_isFunctionOverridden) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->port_isFunctionOverridden is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * Applications wishing to self allocate the port library are required to provide a 
 * version as part of startup.  Applications must be able to query a running port
 * library for it's version via the exported function 
 * @ref j9port.c::j9port_getVersion "j9port_getVersion()".
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9port_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9port_test1";

	J9PortLibrary *fakePtr = &portLibraryToTest;
	J9PortLibraryVersion expectedVersion;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* NULL parameter */
	rc = j9port_getVersion(NULL, &expectedVersion);
	if (0 < rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion(NULL) returned %d expected negative value\n", rc);
	}

	/* Pass, identical structures*/
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	setupVersionInformation(&expectedVersion, 1, 2, 0);
	memcpy(&(fakePtr->portVersion), &expectedVersion, sizeof(J9PortLibraryVersion));
	verifyVersionInformation(portLibrary, testName, fakePtr, &expectedVersion, J9VERSION_VERIFY_ALL);

	/* Pass, padding different */
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	setupVersionInformation(&expectedVersion, 101, 102, 0);
	memcpy(&(fakePtr->portVersion), &expectedVersion, sizeof(J9PortLibraryVersion));
	fakePtr->portVersion.padding = 0xBADC0DE;
	verifyVersionInformation(portLibrary, testName, fakePtr, &expectedVersion, J9VERSION_VERIFY_ALL);

	/* Fail, major versions not matched */
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	setupVersionInformation(&expectedVersion, 2, 3, J9PORT_CAPABILITY_BASE);
	setupVersionInformation(&(fakePtr->portVersion), 3, 3, J9PORT_CAPABILITY_BASE);
	fakePtr->portVersion.padding = 0xBADC0DE;
	verifyVersionInformation(portLibrary, testName, fakePtr, &expectedVersion, J9VERSION_VERIFY_MINOR | J9VERSION_VERIFY_CAPABILITIES);
	if (3 != fakePtr->portVersion.majorVersionNumber) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() corrupted majorVersionNumber found %d expected %d\n", fakePtr->portVersion.majorVersionNumber, 3);
	}

	/* Fail, minor versions not matched */
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	setupVersionInformation(&expectedVersion, 12, 33, J9PORT_CAPABILITY_BASE);
	setupVersionInformation(&(fakePtr->portVersion), 12,72, J9PORT_CAPABILITY_BASE);
	fakePtr->portVersion.padding = 0xBADC0DE;
	verifyVersionInformation(portLibrary, testName, fakePtr, &expectedVersion, J9VERSION_VERIFY_MAJOR | J9VERSION_VERIFY_CAPABILITIES);
	if (72 != fakePtr->portVersion.minorVersionNumber) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() corrupted minorVersionNumber found %d expected %d\n", fakePtr->portVersion.minorVersionNumber, 72);
	}

	/* Fail, capability not present */
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	setupVersionInformation(&expectedVersion, 0, 400, 0);
	setupVersionInformation(&(fakePtr->portVersion), 0, 400, J9PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS);
	fakePtr->portVersion.padding = 0xDEADBEEF;
	verifyVersionInformation(portLibrary, testName, fakePtr, &expectedVersion, J9VERSION_VERIFY_MAJOR | J9VERSION_VERIFY_MINOR);
	if (fakePtr->portVersion.capabilities != J9PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getVersion() corrupted capabilities found %llu expected %llu\n", fakePtr->portVersion.capabilities, J9PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS);
	}
	
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * Applications wishing to self allocate the port library are required to provide
 * a size for the port library to create.  The size of any port library version can
 * be obtained via the exported function
 * @ref j9port.c::j9port_getSize "j9port_getSize()"
 *  
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9port_test2(struct J9PortLibrary *portLibrary) 
{
	const char* testName = "j9port_test2";

	J9PortLibraryVersion version;
	UDATA expectedSize, actualSize;

	/* First all supported versions */
	expectedSize = sizeof(J9PortLibrary);

	/* Pass, exact match all fields */
	setupVersionInformation(&version, J9PORT_MAJOR_VERSION_NUMBER, J9PORT_MINOR_VERSION_NUMBER, J9PORT_CAPABILITY_MASK);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);		
	}

	/* Pass, capabilities don't matter (Except J9PORT_CAPABILITY_STANDARD) */
	setupVersionInformation(&version, J9PORT_MAJOR_VERSION_NUMBER, J9PORT_MINOR_VERSION_NUMBER, J9PORT_CAPABILITY_STANDARD);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);		
	}

	/* Pass, capabilities don't matter (except Standard number) */
	setupVersionInformation(&version, J9PORT_MAJOR_VERSION_NUMBER, J9PORT_MINOR_VERSION_NUMBER, 0xBADC0DE|J9PORT_CAPABILITY_STANDARD);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);		
	}

	/* Pass, base capability requested, adjust size */
	expectedSize = offsetof(J9PortLibrary, ipcmutex_release) + sizeof(void*);
	setupVersionInformation(&version, J9PORT_MAJOR_VERSION_NUMBER, J9PORT_MINOR_VERSION_NUMBER, J9PORT_CAPABILITY_BASE);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);		
	}

	/* Pass, capabilities don't matter (Except J9PORT_CAPABILITY_STANDARD) */
	setupVersionInformation(&version, J9PORT_MAJOR_VERSION_NUMBER, J9PORT_MINOR_VERSION_NUMBER, 0);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);		
	}

	/* Unsupported Versions */
	expectedSize = 0;

	/* Fail, Major version wrong */
	setupVersionInformation(&version, 0, J9PORT_MINOR_VERSION_NUMBER, 0);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);	
	}

	/* Fail, Major version wrong */
	setupVersionInformation(&version, portLibrary->portVersion.majorVersionNumber+8, J9PORT_MINOR_VERSION_NUMBER, 0);
	actualSize = j9port_getSize(&version);
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_getSize() returned %zu expected %zu\n", actualSize, expectedSize);		
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * Prior to starting a port library the initial values for the function pointers
 * in the table need to be initialized.  The ability to initialize the table with
 * default values prior to starting the port library allows applications to override
 * functionality.  For example an application may with to control all memory management,
 * so overriding the appropriate functions is required prior to any memory being
 * allocated.  Creation of the port library table is done via the function
 * @ref j9port.c::j9port_create_library "j9port_create_library()"
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * 
 * @see j9port.c::j9port_startup_library "j9port_startup_library()"
 * @see j9port.c::j9port_init_library "j9port_init_library()"
 */
int
j9port_test3(struct J9PortLibrary *portLibrary) 
{
	const char* testName = "j9port_test3";

	J9PortLibraryVersion version;
	J9PortLibrary *fakePtr = (J9PortLibrary *)bigBuffer;
	char eyeCatcher;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Initialize version information to same as running portLibrary */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);

	/* Pass, buffer bigger than required */
	eyeCatcher = '1';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, 2*sizeof(J9PortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected %d\n", rc);
	}
	verifyCreateTableContents(fakePtr, testName, &version, sizeof(J9PortLibrary));

	/* Pass, should create a library equal to that currently running */
	eyeCatcher = '2';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, sizeof(J9PortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected 0\n", rc);

	}
	verifyCreateTableContents(fakePtr, testName, &version, sizeof(J9PortLibrary));

	/* Fail, buffer 1 byte too small */
	eyeCatcher = '3';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, sizeof(J9PortLibrary)-1);
	if (J9PORT_ERROR_INIT_WRONG_SIZE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected %d\n", rc, J9PORT_ERROR_INIT_WRONG_SIZE);
	}
	/* Ensure we didn't write anything, being lazy only checking first byte */
	if (eyeCatcher != bigBuffer[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() trampled memory expected \"%c\" found \"%c\"\n", eyeCatcher, bigBuffer[0]);
	}

	/* Fail, buffer size way too small */
	eyeCatcher = '4';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, 20);
	if (J9PORT_ERROR_INIT_WRONG_SIZE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected %d\n", rc, J9PORT_ERROR_INIT_WRONG_SIZE);
	}
	/* Ensure we didn't write anything, being lazy only checking first byte */
	if (eyeCatcher != bigBuffer[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() trampled memory expected \"%c\" found \"%c\"\n", eyeCatcher, bigBuffer[0]);
	}

	/* Fail, capabilities requested don't match */
	eyeCatcher = '5';
	version.capabilities = (U_64)-1;
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, bigBufferSize);
	if (J9PORT_ERROR_INIT_WRONG_CAPABILITIES != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected %d\n", rc, J9PORT_ERROR_INIT_WRONG_CAPABILITIES);
	}
	/* Ensure we didn't write anything, being lazy only checking first byte */
	if (eyeCatcher != bigBuffer[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() trampled memory expected \"%c\" found \"%c\"\n", eyeCatcher, bigBuffer[0]);
	}

	/* Pass, capabilities are a subset of what present */
	eyeCatcher = '6';
	version.capabilities = 0;
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, bigBufferSize);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected 0\n", rc);
	}

	/* Fail major version not found */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);
	version.majorVersionNumber = (U_16)-1;

	eyeCatcher = '7';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, bigBufferSize);
	if (J9PORT_ERROR_INIT_WRONG_MAJOR_VERSION != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected %d\n", rc, J9PORT_ERROR_INIT_WRONG_MAJOR_VERSION);
	}
	/* Ensure we didn't write anything, being lazy only checking one byte.  Note that buffer will have the first U_16 set to the major version */
	if (eyeCatcher != bigBuffer[sizeof(U_16)]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() trampled memory expected \"%x\" found \"%x\"\n", eyeCatcher, bigBuffer[sizeof(U_16)]);
	}

	/* Pass minorVersion number mismatch */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);
	version.minorVersionNumber = (U_16)-1;

	eyeCatcher = '8';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = j9port_create_library(fakePtr, &version, bigBufferSize);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected 0\n", rc);
	}
	verifyCreateTableContents(fakePtr, testName, &version, sizeof(J9PortLibrary));

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * The second stage in starting a port library.  Once a port library table has
 * been created via 
 * @ref j9port.c::j9port_create_library "j9port_create_library()"
 * it is started via the exported function
 * @ref j9port.c::j9port_startup_library "j9port_startup_library()".
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * 
 * @see j9port.c::j9port_create_library "j9port_create_library()"
 * @see j9port.c::j9port_init_library "j9port_init_library()"
 */
int
j9port_test4(struct J9PortLibrary *portLibrary) 
{
	const char* testName = "j9port_test4";

	J9PortLibrary *fakePtr = &portLibraryToTest;
	J9PortLibraryVersion version;
	I_32 rc;
	omrthread_t attachedThread = NULL;

	reportTestEntry(portLibrary, testName);
	
	if (0 != omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to omrthread\n");
		goto exit;
	}

	/* create a library and ensure the port globals is non NULL, what else can we do? */
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);
	rc = j9port_create_library(fakePtr, &version, sizeof(J9PortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* check the rc */
	rc = j9port_startup_library(fakePtr);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_startup_library() returned %d expected 0\n", rc);
	}

	/* check portGlobals were allocated */
	if (NULL == fakePtr->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_startup_library() portGlobals not allocated\n");
	}

	if (NULL != fakePtr->self_handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_startup_library() self_handle pointer not NULL\n");
	}

	/* Clean this port library up */
	portLibraryToTest.port_shutdown_library(fakePtr);

	/* Do it again, but before starting override memory (first startup function) and
	 * ensure everything is ok
	 */
	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);
	rc = j9port_create_library(fakePtr, &version, sizeof(J9PortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_create_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* override time_startup */
	OMRPORT_FROM_J9PORT(fakePtr)->time_startup = fake_time_startup;
	rc = j9port_startup_library(fakePtr);
	if (failMemStartup != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_startup_library() returned %d expected %d\n", rc, failMemStartup);
	}

exit:
	portLibraryToTest.port_shutdown_library(fakePtr);
	if (NULL != attachedThread) {
		omrthread_detach(attachedThread);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 *
 * The majority of applications will not wish to override any functionality
 * prior to use of the port library.  The exported function
 * @ref j9port.c::j9port_init_library "j9port_init_library()"
 * creates and starts the port library.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * 
 * @see j9port.c::j9port_create_library "j9port_create_library()"
 * @see j9port.c::j9port_startup_library "j9port_startup_library()"
 */
int
j9port_test5(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9port_test5";

	reportTestEntry(portLibrary, testName);

	/* TODO copy test3 here once it compiles */

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * The port library requires a region of memory for it's function table pointer.
 * This table must not be declared on a stack that is torn down prior to the application
 * running to completion and tearing down the port library.  It can either be 
 * created as a global variable to the program, or if a running port library is 
 * present it can be allocated on behalf of the application.  Self allocating
 * port libraries use the exported function 
 * @ref j9port.c::j9port_allocate_library "j9port_allocate_library()".
 * This function allocates, initializes and starts the port library.
 * The port library is responsible for deallocation of this memory
 * as part of it's shutdown.
* 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9port_test6(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9port_test6";

	J9PortLibrary *fakePortLibrary;
	J9PortLibraryVersion version;
	I_32 rc;
	omrthread_t attachedThread = NULL;

	reportTestEntry(portLibrary, testName);
	
	if (0 != omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to omrthread\n");
		goto exit;
	}

	/* Pass */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	fakePortLibrary = NULL;
	rc = j9port_allocate_library(&version, &fakePortLibrary);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_allocate_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* Verify we have a pointer */
	if (NULL == fakePortLibrary) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_allocate_library() returned NULL pointer\n");
		goto exit;
	}

	/* Verify not started */
	if (NULL != OMRPORT_FROM_J9PORT(fakePortLibrary)->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_allocate_library() portGlobals pointer is not NULL\n");
	}

	/* Verify self allocated */
	if (NULL == fakePortLibrary->self_handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_allocate_library() self_handle pointer is NULL\n");
	}

	/* Verify it will start */	
	rc = j9port_startup_library(fakePortLibrary);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_startup_library() returned %d expected 0\n", rc);
	}

	/* Take this port library down */
	fakePortLibrary->port_shutdown_library(fakePortLibrary);

	/* Try again, but force failure during startup by over riding one of the startup functions */
	fakePortLibrary = NULL;
	rc = j9port_allocate_library(&version, &fakePortLibrary);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_allocate_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* Override j9mem_startup */
	OMRPORT_FROM_J9PORT(fakePortLibrary)->mem_startup = fake_mem_startup;
	rc = j9port_startup_library(fakePortLibrary);
	if (failMemStartup != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_startup_library() returned %d expected %d\n", rc, failMemStartup);
	}

	/* No way to tell if it freed the memory properly.  The pointer we have to the port library is 
	 * not updated as part of j9port_startup_library(), so we are now pointing at dead memory.
	 * 
	 * NULL our pointer to exit can clean-up if required
	 */
	fakePortLibrary = NULL;

exit:
	if (NULL != fakePortLibrary) {
		fakePortLibrary->port_shutdown_library(fakePortLibrary);
	}
	if (NULL != attachedThread) {
		omrthread_detach(attachedThread);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * The port library allocates resources as part of it's normal running.  Thus
 * prior to terminating the application the port library must free all it's 
 * resource via the port library table function
 * @ref j9port.c::j9port_shutdown_library "j9port_shutdown_library()"
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * @note self allocated port libraries de-allocate their memory as part of this
 * function.
 */
int
j9port_test7(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9port_test7";

	J9PortLibrary *fakePtr;
	J9PortLibraryVersion version;
	I_32 rc;
	omrthread_t attachedThread = NULL;

	reportTestEntry(portLibrary, testName);
	
	if (0 != omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to omrthread\n");
		goto exit;
	}

	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	fakePtr = &portLibraryToTest;
	rc = j9port_init_library(fakePtr, &version, sizeof(J9PortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_init_library() returned %d expected 0\n", rc);
	}
	/* Shut it down */
	portLibraryToTest.port_shutdown_library(fakePtr);
	omrthread_detach(attachedThread);
	
	/* Global pointer should be NULL */
	if (NULL != fakePtr->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "port_shutdown_library() portGlobal pointer is not NULL\n");
	}

	/* Shutdown again, should be ok, nothing to check ... 
	 * TODO support this?
	portLibraryToTest.port_shutdown_library(fakePtr);
	 */

	/* Let's do it all over again, this time self allocated 
	 * Note a self allocated library does not startup, so there are no
	 * port globals etc.  Verifies the shutdown routines are safe
	 */
	fakePtr= NULL;
	rc = j9port_allocate_library(&version, &fakePtr);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_allocate_library() returned %d expected 0\n", rc);
		goto exit;
	}
	/*	TODO library not started, fair to shut it down ?	
	fakePtr->port_shutdown_library(fakePtr);
	*/
	fakePtr = NULL;

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * When an application upgrades it's version of the port library it needs
 * to check that the version it compiled against is compatible with the 
 * new one that is running.  The exported function
 * @ref j9port.c::j9port_isCompatible "j9port_isCompatible()"
 * is used for this purpose.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9port_test8(struct J9PortLibrary *portLibrary) 
{
	const char* testName = "j9port_test8";

	J9PortLibraryVersion version;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	/* Pass, exact match */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	rc = j9port_isCompatible(&version);
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isCompatible() returned %d expected 1\n", rc);
	}

	/* Pass, minor version number less, or at least attempt to */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	if (0 != version.minorVersionNumber) {
		version.minorVersionNumber--;
	}
	rc = j9port_isCompatible(&version);
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isCompatible() returned %d expected 1\n", rc);
	}

	/* Pass, don't require all capabiliteis supported */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	version.capabilities = 0;
	rc = j9port_isCompatible(&version);
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isCompatible() returned %d expected 1\n", rc);
	}

	/* Fail, major number greater than present */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	version.majorVersionNumber++;
	rc = j9port_isCompatible(&version);
	if (1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isCompatible() returned %d expected 0\n", rc);
	}

	/* Fail, minor number greater than present */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	version.minorVersionNumber++;
	rc = j9port_isCompatible(&version);
	if (1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isCompatible() returned %d expected 0\n", rc);
	}

	/* Fail, find some capability not in the running portlibrary */
	/* Fail, capabilities required not present */
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
/*
	rc = j9port_isCompatible(&version);
	if (1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isCompatible() returned %d expected 0\n", rc);
	}
*/

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * The JIT needs to verify that the function it wants to optimize is what
 * it expects, that is an application has not provided it's own implementation.
 * The port library table function 
 * @ref j9port.c::j9port_isFunctionOverridden "j9port_isFunctionOverridden"
 * is used for this purpose.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9port_test9(struct J9PortLibrary *portLibrary) 
{
	const char* testName = "j9port_test9";

	J9PortLibraryVersion version;
	J9PortLibrary *fakePtr = &portLibraryToTest;
	I_32 rc;

	reportTestEntry(portLibrary, testName);

	memset(&portLibraryToTest, '0', sizeof(J9PortLibrary));
	J9PORT_SET_VERSION(&version, J9PORT_CAPABILITY_MASK);	
	rc = j9port_init_library(&portLibraryToTest, &version, sizeof(J9PortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_init_library() returned %d expected 0\n", rc);
	}

	/* override a couple of function */
	OMRPORT_FROM_J9PORT(fakePtr)->mem_startup = fake_mem_startup;
	OMRPORT_FROM_J9PORT(fakePtr)->mem_shutdown = NULL;
	OMRPORT_FROM_J9PORT(fakePtr)->time_hires_clock = NULL;
	OMRPORT_FROM_J9PORT(fakePtr)->time_startup = OMRPORT_FROM_J9PORT(fakePtr)->tty_startup;

	rc = OMRPORT_FROM_J9PORT(fakePtr)->port_isFunctionOverridden(OMRPORT_FROM_J9PORT(fakePtr), offsetof(OMRPortLibrary, mem_startup));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = OMRPORT_FROM_J9PORT(fakePtr)->port_isFunctionOverridden(OMRPORT_FROM_J9PORT(fakePtr), offsetof(OMRPortLibrary, mem_shutdown));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = OMRPORT_FROM_J9PORT(fakePtr)->port_isFunctionOverridden(OMRPORT_FROM_J9PORT(fakePtr), offsetof(OMRPortLibrary, time_hires_clock));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = OMRPORT_FROM_J9PORT(fakePtr)->port_isFunctionOverridden(OMRPORT_FROM_J9PORT(fakePtr), offsetof(OMRPortLibrary, time_startup));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = OMRPORT_FROM_J9PORT(fakePtr)->port_isFunctionOverridden(OMRPORT_FROM_J9PORT(fakePtr), offsetof(OMRPortLibrary, time_hires_frequency));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isFunctionOverridden returned %d expected 0\n", rc);
	}

	rc = OMRPORT_FROM_J9PORT(fakePtr)->port_isFunctionOverridden(OMRPORT_FROM_J9PORT(fakePtr), offsetof(OMRPortLibrary, tty_printf));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9port_isFunctionOverridden returned %d expected 0\n", rc);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library management.
 * 
 * Exercise all API related to port library management found in
 * @ref j9port.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9port_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "Port library management test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9port_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9port_test1(portLibrary);
		rc |= j9port_test2(portLibrary);
		rc |= j9port_test3(portLibrary);
		rc |= j9port_test4(portLibrary);
		rc |= j9port_test5(portLibrary);
		rc |= j9port_test6(portLibrary);
		rc |= j9port_test7(portLibrary);
		rc |= j9port_test8(portLibrary);
		rc |= j9port_test9(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nPort library management test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

 

