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
 * @brief Verify port library virtual memory management.
 * 
 * Exercise the API for port library virtual memory management operations.  These functions 
 * can be found in the file @ref j9vmem.c  
 * 
 * @note port library virtual memory management operations are not optional in the port library table.
 * 
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"
#include "omrmemcategories.h"

#if defined(AIXPPC)
#include <sys/vminfo.h>
#endif /* defined(AIXPPC) */

#define TWO_GIG_BAR 0x7FFFFFFF
#define ONE_MB (1*1024*1024)
#define FOUR_KB (4*1024)
#define TWO_GB	(0x80000000L)
#define SIXTY_FOUR_GB (0x1000000000L)

#define SIXTY_FOUR_KB (64*1024)
#define D2M (2*1024*1024)
#define D16M (16*1024*1024)
#define D512M (512*1024*1024)
#define D256M (256*1024*1024)
#define DEFAULT_NUM_ITERATIONS 50

#define MAX_ALLOC_SIZE 256 /**<@internal largest size to allocate */

#define allocNameSize 64 /**< @internal buffer size for name function */

#if defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(WIN64) || defined(J9ZOS390)
#define ENABLE_RESERVE_MEMORY_EX_TESTS
#endif /* LINUX || AIXPPC || WIN32 || WIN64 || J9ZOS390 */

#if defined(ENABLE_RESERVE_MEMORY_EX_TESTS)
static int j9vmem_testReserveMemoryEx_impl(struct J9PortLibrary *, const char*, UDATA options);
static int j9vmem_testReserveMemoryEx_StandardAndQuickMode(struct J9PortLibrary *, const char*, UDATA options);
static BOOLEAN memoryIsAvailable(struct J9PortLibrary *, BOOLEAN);
#endif /* ENABLE_RESERVE_MEMORY_EX_TESTS */

#define CYCLES 10000
#define FREE_PROB 0.5

int myFunction1();
void myFunction2();

/**
 * @internal
 * Helper function for memory management verification.
 * 
 * Given a pointer to an memory chunk verify it is
 * \arg a non NULL pointer
 * \arg correct size
 * \arg writeable
 * \arg aligned
 * \arg double aligned
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test requesting this functionality
 * @param[in] memPtr Pointer the memory under test
 * @param[in] byteAmount Size or memory under test in bytes
 * @param[in] allocName Calling function name to display in errors
 */
static void
verifyMemory(struct J9PortLibrary *portLibrary, const char* testName, char *memPtr, UDATA byteAmount, const char *allocName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char testCharA = 'A';
	const char testCharC = 'c';
	UDATA testSize;
	char stackMemory[MAX_ALLOC_SIZE];

	if(NULL == memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s returned NULL\n", allocName);
		return;
	}

	/* Don't trample memory */
	if (0 == byteAmount) {
		return;
	}

	/* Test write at start */
	memPtr[0] = testCharA;
	if (testCharA != memPtr[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test at 0\n", allocName);
	}

	/* Test write at middle */
	memPtr[byteAmount/2] = testCharA;
	if (testCharA != memPtr[byteAmount/2]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test at %u\n", allocName, byteAmount/2);
	}

	/* Can only verify characters as large as the stack allocated size */
	if (MAX_ALLOC_SIZE < byteAmount) {
		testSize = MAX_ALLOC_SIZE;
	} else {
		testSize = byteAmount;
	}

	/* Fill the buffer start */
	memset(memPtr, testCharC, testSize);
	memset(stackMemory, testCharC, testSize);

	/* Check memory retained value */
	if (0 != memcmp(stackMemory, memPtr, testSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to retain value at start\n", allocName);
	}

	/* Fill the buffer end */
	memset(memPtr + byteAmount - testSize, testCharA, testSize);
	memset(stackMemory, testCharA, testSize);

	/* Check memory retained value */
	if (0 != memcmp(stackMemory, memPtr + byteAmount - testSize, testSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to retain value at end\n", allocName);
	}
}

/**
 * Verify port library memory management.
 * 
 * Ensure the port library is properly setup to run vmem operations.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_verify_functiontable_slots(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9vmem_verify_functiontable_slots";

	reportTestEntry(portLibrary, testName);
	 
	/* Verify that the memory management function pointers are non NULL */
	
	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibrary, it is not re-entrant safe
	 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_startup is NULL\n");
	}

	/*  Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_shutdown is NULL\n");
	}

	/* j9mem_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_reserve_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_reserve_memory is NULL\n");
	}

	/* j9mem_test1 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_commit_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_commit_memory is NULL\n");
	}

	/* j9mem_test4 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_decommit_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_decommit_memory, is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_free_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_free_memory, is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->vmem_supported_page_sizes) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_supported_page_sizes, is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library memory management.
 * 
 * Make sure there is at least one page size returned by j9vmem_get_supported_page_sizes
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_test_verify_there_are_page_sizes(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_test_verify_there_are_page_sizes";
	char *memPtr = NULL;
	UDATA * pageSizes;
	UDATA * pageFlags;
	int i = 0;
	
	reportTestEntry(portLibrary, testName);

	/* First get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();
	
	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto exit;
	}
	
	outputComment(portLibrary, "\t Supported page sizes:\n");
	for (i =0 ; pageSizes[i] != 0 ; i++) {
		outputComment(portLibrary, "\t 0x%zx ", pageSizes[i]);
		outputComment(portLibrary, "\t 0x%zx \n", pageFlags[i]);
	}
	outputComment(portLibrary, "\n");

exit:
	return reportTestExit(portLibrary, testName);
}

#if defined(J9ZOS390)
/**
 * Returns TRUE if the given page size and flags corresponds to new page sizes added in z/OS.
 * z/OS tradditionally supports 4K and 1M fixed pages.
 * 1M pageable and 2G fixed, added recently, are considered new page sizes.
 */
BOOLEAN
isNewPageSize(UDATA pageSize, UDATA pageFlags)
{
	if ((FOUR_KB == pageSize) ||
		((ONE_MB == pageSize) && (J9PORT_VMEM_PAGE_FLAG_FIXED == (J9PORT_VMEM_PAGE_FLAG_FIXED & pageFlags)))
	) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Returns TRUE if the given page size and flags can be used to allocate memory below 2G bar, FALSE otherwise.
 * On z/OS, 1M fixed and 2G fixed pages can be used only to allocate above 2G bar.
 * Only 4K and 1M pageable pages can be used to allocate memory below the bar.
 */
isPageSizeSupportedBelowBar(UDATA pageSize, UDATA pageFlags)
{
	if ((FOUR_KB == pageSize) ||
		((ONE_MB == pageSize) && (J9PORT_VMEM_PAGE_FLAG_PAGEABLE == (J9PORT_VMEM_PAGE_FLAG_PAGEABLE & pageFlags)))
	) {
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif /* J9ZOS390 */

/**
 * Get all the page sizes and make sure we can allocate a memory chunk for each page size.
 * 
 * Checks that each allocation manipulates the memory categories appropriately.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_test1";
	char *memPtr = NULL;
	UDATA * pageSizes;
	UDATA * pageFlags;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	char allocName[allocNameSize];
	I_32 rc;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	
	reportTestEntry(portLibrary, testName);

	/* First get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();
	
	/* reserve and commit memory for each page size */
	for (i=0 ; pageSizes[i] != 0 ; i++) {
		UDATA initialBlocks;
		UDATA initialBytes;
		
		/* Sample baseline category data */
		getPortLibraryMemoryCategoryData(portLibrary, &initialBlocks, &initialBytes);
		
		/* reserve and commit */
#if defined(J9ZOS390)
		/* On z/OS skip this test for newly added large pages as obsolete j9vmem_reserve_memory() does not support them */
		if (TRUE == isNewPageSize(pageSizes[i], pageFlags[i])) {
			continue;
		}
#endif /* J9ZOS390 */
		memPtr = j9vmem_reserve_memory(0,pageSizes[i], &vmemID
				,J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT
				,pageSizes[i], OMRMEM_CATEGORY_PORT_LIBRARY);
		
		/* did we get any memory? */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", pageSizes[i], pageSizes[i], lastErrorNumber, lastErrorMessage);

			if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				outputComment(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				outputComment(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		} else {
			UDATA finalBlocks;
			UDATA finalBytes;
			outputComment(portLibrary, "\treserved and committed 0x%zx bytes with page size 0x%zx at address 0x%zx\n", pageSizes[i], vmemID.pageSize, memPtr);

			getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

			if (finalBlocks <= initialBlocks) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category block as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
			}
		
			if (finalBytes < (initialBytes + pageSizes[i])) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category bytes as expected. Initial bytes=%zu, final bytes=%zu, page size=%zu.\n", finalBytes, initialBytes, pageSizes[i]);
			}
		}
		
		/* can we read and write to the memory? */
		(void) j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", pageSizes[i]);
		verifyMemory(portLibrary, testName, memPtr, pageSizes[i], allocName);
		
		/* free the memory (reuse the vmemID) */
		rc = j9vmem_free_memory(memPtr, pageSizes[i], &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n"
					, rc, pageSizes[i], memPtr);
			goto exit;
		}

		{
			UDATA finalBlocks;
			UDATA finalBytes;

			getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

			if (finalBlocks != initialBlocks) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category block as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
			}

			if (finalBytes != initialBytes) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category bytes as expected. Initial bytes=%zu, final bytes=%zu, page size=%zu.\n", initialBytes, finalBytes, pageSizes[i]);
			}
		}
	}

exit:

	return reportTestExit(portLibrary, testName);
}

static void
setDisclaimVirtualMemory(struct J9PortLibrary *portLibrary, BOOLEAN disclaim)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (disclaim) {
		j9tty_printf(portLibrary, "\n *** Disclaiming virtual memory\n\n");
		j9port_control(J9PORT_CTLDATA_VMEM_ADVISE_OS_ONFREE, 1);
	} else {
		j9tty_printf(portLibrary, "\n *** Not disclaiming virtual memory\n\n");
		j9port_control(J9PORT_CTLDATA_VMEM_ADVISE_OS_ONFREE, 0);
	}
}

/**
 * 	1. Reserve memory
 * 	2. Time running numIterations times:
 * 			commit memory
 * 			write to all of it
 * 			decommit all of it
 * 	3. Free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] byteAmount This value is the amount of memory in bytes to use for each commit/decommit within the loop
 * @param[in] numIterations The number of times to iterate through the loop
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_bench_write_and_decommit_memory(struct J9PortLibrary *portLibrary, UDATA pageSize, UDATA byteAmount, BOOLEAN disclaim, UDATA numIterations)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_bench_write_and_decommit_memory";
	char *memPtr = NULL;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	IDATA rc = 0;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	J9PortVmemParams params;

	reportTestEntry(portLibrary, testName);
	setDisclaimVirtualMemory(portLibrary, disclaim);
	
	/* reserve the memory, but don't commit it yet */
	j9vmem_vmem_params_init(&params);
	params.byteAmount = byteAmount;
	params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE;
	params.pageSize = pageSize;
	params.category = OMRMEM_CATEGORY_PORT_LIBRARY;
	params.options |= J9PORT_VMEM_STRICT_PAGE_SIZE;
	memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

	/* check if we got memory */
	if (memPtr == NULL) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve 0x%zx bytes with page size 0x%zx.\n"
				"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
		if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
			j9tty_printf(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
			j9tty_printf(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
		}
		goto exit;
	} else {
		j9tty_printf(portLibrary, "\treserved 0x%zx bytes with page size 0x%zx at 0x%zx\n", byteAmount, vmemID.pageSize, memPtr);
	}

	startTimeNanos = j9time_nano_time();

	for (i = 0; numIterations != i; i++) {
	
		memPtr = j9vmem_commit_memory(memPtr, byteAmount, &vmemID);
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_commit_memory returned NULL when trying to commit 0x%zx bytes backed by 0x%zx-byte pages\n", byteAmount, pageSize);
			goto exit;
		}

		/* write to the memory */
		memset(memPtr, 'c', byteAmount);

		/* decommit the memory */
		rc = j9vmem_decommit_memory(memPtr, byteAmount, &vmemID);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
			goto exit;
		}
	}

	deltaMillis = (j9time_nano_time() - startTimeNanos)/(1000*1000);
	j9tty_printf(portLibrary, "%s pageSize 0x%zx, byteAmount 0x%zx, millis for %u iterations: [write_to_all/decommit] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, numIterations, deltaMillis);

	rc = j9vmem_free_memory(memPtr, byteAmount, &vmemID);

	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPtr);
		goto exit;
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * 	1. Reserve memory
 * 	2. Commit memory
 * 	3. Time:
 * 		write to all of it
 * 		decommit a chunk of memory
 * 		write to the chunk
 * 	4. Free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] physicalMemorySize The size of physical memory on the machine (e.g. as read from /proc/meminfo)
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] disclaim When true, the OS is told it may disclaim the memory
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_bench_force_overcommit_then_decommit(struct J9PortLibrary *portLibrary, UDATA physicalMemorySize, UDATA pageSize, BOOLEAN disclaim)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_bench_force_overcommit_then_decommit";
	char *memPtr = NULL;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	IDATA rc;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	J9PortVmemParams params;
	UDATA byteAmount = physicalMemorySize + D512M;

	reportTestEntry(portLibrary, testName);
	setDisclaimVirtualMemory(portLibrary, disclaim);
	if (byteAmount < D512M) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "byteAmount 0x%zx bytes must be greater than 0x%zx\n", byteAmount, D512M);
		goto exit;
	}

	/* reserve the memory */
	j9vmem_vmem_params_init(&params);
	params.byteAmount = byteAmount;
	params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
	params.pageSize = pageSize;
	params.category = OMRMEM_CATEGORY_PORT_LIBRARY;
	params.options |= J9PORT_VMEM_STRICT_PAGE_SIZE;
	memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

	/* check we get memory */
	if (memPtr == NULL) {
		lastErrorMessage = (char *)j9error_last_error_message();
		lastErrorNumber = j9error_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
				"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
		if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
			j9tty_printf(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
			j9tty_printf(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
		}
		goto exit;
	} else {
		j9tty_printf(portLibrary, "reserved and committed 0x%zx bytes of pagesize 0x%zx\n", byteAmount, pageSize);
	}

	startTimeNanos = j9time_nano_time();

	/* write the first chunk */
	memset(memPtr, 5, D512M);

	/* byteAmount-D512M should be at least the amount of real memory on the system */
	memset(memPtr+D512M, 5, byteAmount-D512M);

	/* in order to write to region being decommitted, the system would have to go to disk,
	 * as the previous memset would have forced the region to have been paged out
	 */
	rc = j9vmem_decommit_memory(memPtr, D512M, &vmemID);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
		goto exit;
	}

	/* write to the decommitted region */
	memset(memPtr, 5, D512M);

	deltaMillis = (j9time_nano_time() - startTimeNanos)/(1000*1000);
	j9tty_printf(portLibrary, "%s pageSize 0x%zx, byteAmount 0x%zx, millis: [force page, decommit pagedout region, write decommitted region] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, deltaMillis);

	rc = j9vmem_free_memory(memPtr, byteAmount, &vmemID);

	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPtr);
		goto exit;
	}

exit:
	return reportTestExit(portLibrary, testName);
}



/**
 * 	1. Time running numIterations times:
 * 			reserve memory
 * 			write to all of it
 * 			decommit all of it
 * 			free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] byteAmount This value is the amount of memory in bytes to use for each commit/decommit within the loop
 * @param[in] disclaim When true, the OS is told it may disclaim the memory
 * @param[in] numIterations The number of times to iterate through the loop
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_bench_reserve_write_decommit_and_free_memory(struct J9PortLibrary *portLibrary, UDATA pageSize, UDATA byteAmount, BOOLEAN disclaim, UDATA numIterations)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_bench_reserve_write_decommit_and_free_memory";
	char *memPtr = NULL;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	IDATA rc;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	J9PortVmemParams params;

	reportTestEntry(portLibrary, testName);
	setDisclaimVirtualMemory(portLibrary, disclaim);

	j9vmem_vmem_params_init(&params);
	params.byteAmount = byteAmount;
	params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
	params.pageSize = pageSize;
	params.category = OMRMEM_CATEGORY_PORT_LIBRARY;
	params.options |= J9PORT_VMEM_STRICT_PAGE_SIZE;

	startTimeNanos = j9time_nano_time();

	/*
	 * Bench numIterations times: Reserve/write to all of it/decommit/free/
	 */
	for (i = 0; numIterations != i; i++) {

		memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

		/* check we get memory */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "In iteration %d, unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", i, byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
			if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				j9tty_printf(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				j9tty_printf(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		}

		/* write to the memory */
		memset(memPtr, 'c', byteAmount);

		rc = j9vmem_decommit_memory(memPtr, byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
			goto exit;
		}

		rc = j9vmem_free_memory(memPtr, byteAmount, &vmemID);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPtr);
			goto exit;
		}
	}

	deltaMillis = (j9time_nano_time() - startTimeNanos)/(1000*1000);

	j9tty_printf(portLibrary, "%s pageSize 0x%zx, byteAmount 0x%zx, millis for %u iterations: [reserve/write_to_all/decommit/free] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, numIterations, deltaMillis);

exit:
	return reportTestExit(portLibrary, testName);
}


/**
 * 	1. Time 1000 times:
 * 			reserve memory
 * 			write to all of it
 * 			decommit all of it
 *  2. Free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] byteAmount This value is the amount of memory in bytes to use for each commit/decommit within the loop
 * @param[in] disclaim When true, the OS is told it may disclaim the memory
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_exhaust_virtual_memory(struct J9PortLibrary *portLibrary, UDATA pageSize, UDATA byteAmount, BOOLEAN disclaim)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_exhaust_virtual_memory";
	char *memPtr = NULL;
	int i = 0;
#define NUM_VMEM_IDS 1000
	struct J9PortVmemIdentifier vmemID[NUM_VMEM_IDS];
	J9PortVmemParams params[NUM_VMEM_IDS];
	char *memPointers[NUM_VMEM_IDS];
	IDATA rc;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	UDATA totalAlloc = 0;

	reportTestEntry(portLibrary, testName);
	setDisclaimVirtualMemory(portLibrary, disclaim);
	startTimeNanos = j9time_nano_time();
	/*
	 * Bench 1000 times:
	 * Reserve/commit/write to all of it/decommit
	 */
	for(i = 0; i < NUM_VMEM_IDS; i++) {

		j9vmem_vmem_params_init(&params[i]);
		params[i].byteAmount = byteAmount;
		params[i].mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
		params[i].pageSize = pageSize;
		params[i].category = OMRMEM_CATEGORY_PORT_LIBRARY;
		params[i].options |= J9PORT_VMEM_STRICT_PAGE_SIZE;

		memPtr = memPointers[i] = j9vmem_reserve_memory_ex(&vmemID[i], &params[i]);

		/* check we get memory */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
			if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				j9tty_printf(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				j9tty_printf(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		} else {
			j9tty_printf(portLibrary, "\treserved 0x%zx bytes\n",byteAmount);
			totalAlloc = byteAmount + totalAlloc;
			j9tty_printf(portLibrary, "\ttotal: 0x%zx bytes\n",totalAlloc);
		}

		/* write to the memory */
		memset(memPtr, 'c', byteAmount);

		/* decommit the memory */
		rc = j9vmem_decommit_memory(memPtr, byteAmount, &vmemID[i]);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
			goto exit;
		}

	}

	deltaMillis = (j9time_nano_time() - startTimeNanos)/(1000*1000);

	for(i = 0; i < NUM_VMEM_IDS; i++) {
		rc = j9vmem_free_memory(memPointers[i], byteAmount, &vmemID[i]);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPointers[i]);
			goto exit;
		}
	}

	j9tty_printf(portLibrary, "%s pageSize 0x%zx, byteAmount 0x%zx, millis for %u iterations: [reserve/write_to_all/decommit] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, NUM_VMEM_IDS, deltaMillis);

exit:
	return reportTestExit(portLibrary, testName);
}



/**
 * Verify that we don't get any errors decomitting memory
 * 
 * Get all the page sizes and make sure we can allocate a memory chunk for each page size
 * 
 * After reserving and committing the memory, decommit the memory before freeing it.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_decommit_memory_test(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_decommit_memory_test";
	char *memPtr = NULL;
	UDATA * pageSizes;
	UDATA * pageFlags;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	char allocName[allocNameSize];
	IDATA rc;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	
	reportTestEntry(portLibrary, testName);

	/* First get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();
	
	/* reserve, commit, decommit, and memory for each page size */
	for (i=0 ; pageSizes[i] != 0 ; i++) {
		
		/* reserve and commit */
#if defined(J9ZOS390)
		/* On z/OS skip this test for newly added large pages as obsolete j9vmem_reserve_memory() does not support them */
		if (TRUE == isNewPageSize(pageSizes[i], pageFlags[i])) {
			continue;
		}
#endif /* J9ZOS390 */
		memPtr = j9vmem_reserve_memory(0,pageSizes[i], &vmemID
				,J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT
				,pageSizes[i], OMRMEM_CATEGORY_PORT_LIBRARY);
		
		/* did we get any memory? */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)j9error_last_error_message();
			lastErrorNumber = j9error_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", pageSizes[i], pageSizes[i], lastErrorNumber, lastErrorMessage);
			if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				outputComment(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				outputComment(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		} else {
			outputComment(portLibrary, "\treserved and committed 0x%zx bytes with page size 0x%zx\n", pageSizes[i], vmemID.pageSize);
		}
		
		/* can we read and write to the memory? */
		(void) j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", pageSizes[i]);
		verifyMemory(portLibrary, testName, memPtr, pageSizes[i], allocName);
		
		/* decommit the memory */
		rc = j9vmem_decommit_memory(memPtr, pageSizes[i], &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n"
					, rc, pageSizes[i], pageSizes[i]);
			goto exit;
		}
		
		/* free the memory (reuse the vmemID) */
		rc = j9vmem_free_memory(memPtr, pageSizes[i], &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n"
					, rc, pageSizes[i], memPtr);
			goto exit;
		}
	}

exit:

	return reportTestExit(portLibrary, testName);
}

#if defined(ENABLE_RESERVE_MEMORY_EX_TESTS)

/**
 * See @ref j9vmem_testReserveMemoryEx_impl
 */
int
j9vmem_testReserveMemoryEx(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (memoryIsAvailable(portLibrary, FALSE)) {
		return j9vmem_testReserveMemoryEx_StandardAndQuickMode(portLibrary, "j9vmem_testReserveMemoryEx", 0);
	} else {
		outputComment(portLibrary, "***Did not find enough memory available on system, not running test j9vmem_testReserveMemoryEx\n");
		return TEST_PASS;
	}
}

/**
 * See @ref j9vmem_testReserveMemoryEx_impl
 */
int
j9vmem_testReserveMemoryExTopDown(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (memoryIsAvailable(portLibrary, FALSE)) {
		return j9vmem_testReserveMemoryEx_StandardAndQuickMode(portLibrary, "j9vmem_testReserveMemoryExTopDown", J9PORT_VMEM_ALLOC_DIR_TOP_DOWN);
	} else {
		outputComment(portLibrary, "***Did not find enough memory available on system, not running test j9vmem_testReserveMemoryExTopDown\n");
		return TEST_PASS;
	}
}

/**
 * See @ref j9vmem_testReserveMemoryEx_impl
 */
int
j9vmem_testReserveMemoryExStrictAddress(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (memoryIsAvailable(portLibrary, TRUE)) {
		return j9vmem_testReserveMemoryEx_StandardAndQuickMode(portLibrary, "j9vmem_testReserveMemoryExStrictAddress", J9PORT_VMEM_STRICT_ADDRESS);
	} else {
		outputComment(portLibrary, "***Did not find enough memory available on system, not running test j9vmem_testReserveMemoryExStrictAddress\n");
		return TEST_PASS;
	}
}

/**
 * See @ref j9vmem_testReserveMemoryEx_impl
 */
int
j9vmem_testReserveMemoryExStrictPageSize(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (memoryIsAvailable(portLibrary, FALSE)) {
		return j9vmem_testReserveMemoryEx_StandardAndQuickMode(portLibrary, "j9vmem_testReserveMemoryExStrictPageSize", J9PORT_VMEM_STRICT_PAGE_SIZE);
	} else {
		outputComment(portLibrary, "***Did not find enough memory available on system, not running test j9vmem_testReserveMemoryExStrictPageSize\n");
		return TEST_PASS;
	}
}

/**********/

/**
 * See @ref j9vmem_testReserveMemoryEx_impl
 */
int
j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemory(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (memoryIsAvailable(portLibrary, FALSE)) {
		return j9vmem_testReserveMemoryEx_StandardAndQuickMode(portLibrary, "j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemory", J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA);
	} else {
		outputComment(portLibrary, "***Did not find enough memory available on system, not running test j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemory\n");
		return TEST_PASS;
	}
}

/**
 * See @ref j9vmem_testReserveMemoryEx_impl
 */
int
j9vmem_testReserveMemoryExStrictPageSize_zOS_useExtendedPrivateAreaMemory(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (memoryIsAvailable(portLibrary, FALSE)) {
		return j9vmem_testReserveMemoryEx_StandardAndQuickMode(portLibrary, "j9vmem_testReserveMemoryExStrictPageSize_zOS_useExtendedPrivateAreaMemory", J9PORT_VMEM_STRICT_PAGE_SIZE | J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA);
	} else {
		outputComment(portLibrary, "***Did not find enough memory available on system, not running test j9vmem_testReserveMemoryExStrictPageSize_zOS_useExtendedPrivateAreaMemory\n");
		return TEST_PASS;
	}
}



/**
 * Get all the page sizes and make sure we can allocate a memory chunk
 * within a certain range of addresses for each page size
 * using standard mode for most OS. For Linux, also test the quick mode
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test
 * @param[in] memory allocation options
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
j9vmem_testReserveMemoryEx_StandardAndQuickMode(struct J9PortLibrary *portLibrary, const char* testName, UDATA options)
{
	int rc = j9vmem_testReserveMemoryEx_impl(portLibrary, testName, options);
#if defined(LINUX)
	if (TEST_PASS == rc) {
		rc |= j9vmem_testReserveMemoryEx_impl(portLibrary, testName, options | J9PORT_VMEM_ALLOC_QUICK);
	}
#endif /* LINUX */
	return rc;
}


/**
 * Get all the page sizes and make sure we can allocate a memory chunk 
 * within a certain range of addresses for each page size
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test
 * @param[in] memory allocation options
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
j9vmem_testReserveMemoryEx_impl(struct J9PortLibrary *portLibrary, const char* testName, UDATA options)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
#define NUM_SEGMENTS 3
	char *memPtr[NUM_SEGMENTS];
	UDATA * pageSizes;
	UDATA * pageFlags;
	int i = 0;
	int j = 0;
	struct J9PortVmemIdentifier vmemID[NUM_SEGMENTS];
	char allocName[allocNameSize];
	I_32 rc;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;

	BOOLEAN strictAddress = (J9PORT_VMEM_STRICT_ADDRESS == (options & J9PORT_VMEM_STRICT_ADDRESS));
	BOOLEAN strictPageSize = (J9PORT_VMEM_STRICT_PAGE_SIZE == (options & J9PORT_VMEM_STRICT_PAGE_SIZE));
	BOOLEAN useExtendedPrivateAreaMemory = (J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA == (options & J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA));

	J9PortVmemParams params[NUM_SEGMENTS];


	reportTestEntry(portLibrary, testName);

	/* Get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i=0 ; pageSizes[i] != 0 ; i++) {
		for(j=0; j<NUM_SEGMENTS; j++){
			UDATA initialBlocks;
			UDATA initialBytes;

			memPtr[j] = NULL;

			/* Sample baseline category data */
			getPortLibraryMemoryCategoryData(portLibrary, &initialBlocks, &initialBytes);

			j9vmem_vmem_params_init(&params[j]);
			if (useExtendedPrivateAreaMemory) {
				params[j].startAddress = (void *) (UDATA) TWO_GB;
			} else {
				params[j].startAddress = (void *) (UDATA) (pageSizes[i]*2);
			}
#if defined(LINUX)
			params[j].endAddress = (void *) (pageSizes[i]*100);
#elif defined(WIN64)
			params[j].endAddress = (void *) 0x7FFFFFFFFFF;
#endif /* LINUX */
			params[j].byteAmount = pageSizes[i];
			params[j].mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
			params[j].pageSize = pageSizes[i];
			params[j].pageFlags = pageFlags[i];
			params[j].category = OMRMEM_CATEGORY_PORT_LIBRARY;

			params[j].options |= options;

			outputComment(portLibrary, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
						"topDown: %s quickSearch: %s strictAddress: %s strictPageSize: %s useExtendedPrivateAreaMemory: %s\n",\
						params[j].pageSize, params[j].startAddress, params[j].endAddress,\
						(0 != (params[j].options & J9PORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
						(0 != (params[j].options & J9PORT_VMEM_ALLOC_QUICK)) ? "true" : "false",
						(0 != (params[j].options & J9PORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
						(0 != (params[j].options & J9PORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
						(0 != (params[j].options & J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA)) ? "true" : "false");

			/* reserve and commit */
			memPtr[j] = j9vmem_reserve_memory_ex(&vmemID[j], &params[j]);

			/* did we get any memory and is it in range if strict is set? */
			if (memPtr[j] == NULL) {
				if (strictPageSize) {
					outputComment(portLibrary, "\t! Unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n",pageSizes[i],pageSizes[i]);
				} else {
					lastErrorMessage = (char *)j9error_last_error_message();
					lastErrorNumber = j9error_last_error_number();
					outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
							"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", pageSizes[i], pageSizes[i], lastErrorNumber, lastErrorMessage);

					if (J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
						outputComment(portLibrary, "\tPortable error J9PORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
						outputComment(portLibrary, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
					}
				}
			} else if (strictAddress && ( ((void *)memPtr[j] < params[j].startAddress) || ((void *)memPtr[j] > params[j].endAddress)) ) {
				/* if returned pointer is outside of range then fail */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Strict address flag set and returned pointer [0x%zx] is outside of range.\n"
						"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", memPtr[j], lastErrorNumber, lastErrorMessage);
				
			} else if (strictPageSize && (vmemID[j].pageSize != params[j].pageSize)) {
				/* fail if strict page size flag and returned memory does not have the requested page size */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Strict page size flag set and returned memory has a page size of [0x%zx] "
						"while a page size of [0x%zx] was requested.\n"
						"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", vmemID[j].pageSize, params[j].pageSize, lastErrorNumber, lastErrorMessage);
#if defined(J9VM_ENV_DATA64)
			} else if (useExtendedPrivateAreaMemory && (((U_64) memPtr[j] < TWO_GB) && ((U_64) memPtr[j] >= SIXTY_FOUR_GB))) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\t useExtendedPrivateAreaMemory flag is set and returned pointer [0x%zx] is outside of range.\n", memPtr[j]);
#endif /* J9VM_ENV_DATA64 */
			} else {
				outputComment(portLibrary, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%zx at address 0x%zx\n",\
							pageSizes[i], vmemID[j].pageSize, vmemID[j].pageFlags, memPtr[j]);
			}

			/* perform memory checks */
			if (NULL != memPtr[j]) {
				UDATA finalBlocks;
				UDATA finalBytes;
				
				/* are the page sizes stored and reported correctly? */
				if (vmemID[j].pageSize != j9vmem_get_page_size(&(vmemID[j]))) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmemID[j].pageSize (0x%zx)!= j9vmem_get_page_size (0x%zx) .\n",
								vmemID[j].pageSize, j9vmem_get_page_size(vmemID));
				}
				
				/* are the page types stored and reported correctly? */
				if (vmemID[j].pageFlags != j9vmem_get_page_flags(&(vmemID[j]))) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmemID[j].pageFlags (0x%zx)!= j9vmem_get_page_flags (0x%zx) .\n",
								vmemID[j].pageFlags, j9vmem_get_page_flags(vmemID));
				}

				/* can we read and write to the memory? */
				j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", pageSizes[i]);
				verifyMemory(portLibrary, testName, memPtr[j], pageSizes[i], allocName);

				/* Have the memory categories been updated properly */
				getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

				if (finalBlocks <= initialBlocks) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category block as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
				}

				if (finalBytes < (initialBytes + pageSizes[i])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category bytes as expected. Final bytes=%zu, initial bytes=%zu, page size=%zu.\n", finalBytes, initialBytes, pageSizes[i]);
				}
			}
		}

		/* free the memory */
		for(j=0; j<NUM_SEGMENTS; j++){
			if(NULL != memPtr[j]){
				UDATA initialBlocks, initialBytes, finalBlocks, finalBytes;

				getPortLibraryMemoryCategoryData(portLibrary, &initialBlocks, &initialBytes);

				rc = j9vmem_free_memory(memPtr[j], pageSizes[i], &vmemID[j]);
				if (rc != 0) {
					outputErrorMessage(PORTTEST_ERROR_ARGS
							, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n"
							, rc, pageSizes[i], memPtr[j]);
				}

				getPortLibraryMemoryCategoryData(portLibrary, &finalBlocks, &finalBytes);

				if (finalBlocks >= initialBlocks) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category blocks as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
				}

				if (finalBytes > (initialBytes - pageSizes[i])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category bytes as expected. Final bytes=%zu, initial bytes=%zu, page size=%zu.\n", finalBytes, initialBytes, pageSizes[i]);
				}
			}
		}

	}

#undef NUM_SEGMENTS

	return reportTestExit(portLibrary, testName);
}

/**
 * Tries to allocate and free same amount of memory that will be used by
 * reserve_memory_ex tests to see whether or not there is enough memory
 * on the system.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] strictAddress TRUE if J9PORT_VMEM_STRICT_ADDRESS flag is to be used
 * 
 * @return TRUE if memory is available, FALSE otherwise
 * 
 */
static BOOLEAN
memoryIsAvailable(struct J9PortLibrary *portLibrary, BOOLEAN strictAddress){
#define NUM_SEGMENTS 3
	PORT_ACCESS_FROM_PORT(portLibrary);
	char *memPtr[NUM_SEGMENTS] = {(char *)NULL, (char *)NULL, (char *)NULL};
	UDATA * pageSizes;
	UDATA * pageFlags;
	int i = 0;
	int j = 0;
	struct J9PortVmemIdentifier vmemID[NUM_SEGMENTS];
	I_32 rc;
	J9PortVmemParams params[NUM_SEGMENTS];
	BOOLEAN isMemoryAvailable = TRUE;

	/* Get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i=0 ; pageSizes[i] != 0 ; i++) {
		for(j=0; j<NUM_SEGMENTS; j++){
			j9vmem_vmem_params_init(&params[j]);
			params[j].startAddress = (void *) (pageSizes[i]*2);
#if defined(LINUX)
			params[j].endAddress = (void *) (pageSizes[i]*100);
#elif defined(WIN64)
			params[j].endAddress = (void *) 0x7FFFFFFFFFF;
#endif /* LINUX */
			params[j].byteAmount = pageSizes[i];
			params[j].mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
			params[j].pageSize = pageSizes[i];
			params[j].pageFlags = pageFlags[i];

			if(strictAddress){
				params[j].options |= J9PORT_VMEM_STRICT_ADDRESS;
			}

			/* reserve and commit */
			memPtr[j] = j9vmem_reserve_memory_ex(&vmemID[j], &params[j]);

			/* did we get any memory */
			if (memPtr[j] == NULL) {
				outputComment(portLibrary, "\t**Could not find 0x%zx bytes available with page size 0x%zx\n", pageSizes[i], pageSizes[i]);
				isMemoryAvailable = FALSE;
				break;
			}
		}

		for(j=0; j<NUM_SEGMENTS; j++){
			/* free the memory (reuse the vmemID) */
			if(NULL != memPtr[j]){
				rc = j9vmem_free_memory(memPtr[j], pageSizes[i], &vmemID[j]);
				if (rc != 0) {
					isMemoryAvailable = FALSE;
					outputComment(portLibrary, "\t**j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n"
						, rc, pageSizes[i], memPtr);
				}
			}
		}

	}

#undef NUM_SEGMENTS

	return isMemoryAvailable;
}

#if defined(J9ZOS390)
/**
 * Test request for pages below the 2G bar on z/OS
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
j9vmem_testReserveMemoryEx_zOSLargePageBelowBar(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA * pageSizes = NULL;
	UDATA * pageFlags = NULL;
	const char* testName = "j9vmem_testReserveMemoryEx_zOSLargePageBelowBar";
	I_32 i = 0;

	reportTestEntry(portLibrary, testName);

	/* Get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		char allocName[allocNameSize];
		void * memPtr = NULL;
		I_32 rc = 0;

		j9vmem_vmem_params_init(&params);
		
		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = 0;
		
		outputComment(portLibrary, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					"topDown: %s strictAddress: %s strictPageSize: %s useExtendedPrivateAreaMemory: %s\n",\
					params.pageSize, params.startAddress, params.endAddress,\
					(0 != (params.options & J9PORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

		/* did we get any memory? */
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx, page type 0x%zx.\n", \
							params.byteAmount, pageSizes[i], pageFlags[i]);

			goto exit;

		} else {
			outputComment(portLibrary, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%x at address 0x%zx\n",\
						params.byteAmount, vmemID.pageSize, vmemID.pageFlags, memPtr);
		}
	
		/* can we read and write to the memory? */
		j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", params.byteAmount);
		verifyMemory(portLibrary, testName, memPtr, params.byteAmount, allocName);
	
		/* free the memory */
		rc = j9vmem_free_memory(memPtr, params.byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_free_memory returned 0x%zx when trying to free 0x%zx bytes\n"
					, rc, params.byteAmount);
			goto exit;
		}
	}

exit:
	return reportTestExit(portLibrary, testName);

}

/**
 * Test request for pages below the 2G bar with J9PORT_VMEM_STRICT_ADDRESS set on z/OS
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
j9vmem_testReserveMemoryExStrictAddress_zOSLargePageBelowBar(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA * pageSizes = NULL;
	UDATA * pageFlags = NULL;
	const char* testName = "j9vmem_testReserveMemoryExStrictAddress_zOSLargePageBelowBar";
	I_32 i = 0;

	reportTestEntry(portLibrary, testName);

	/* Get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		char allocName[allocNameSize];
		void * memPtr = NULL;
		I_32 rc = 0;

		if (TWO_GB <= pageSizes[i]) {
			/* On z/OS "below bar" range spans from 0-2GB.
			 * If the page size is >= 2G, then attempt to allocate memory with J9PORT_VMEM_STRICT_ADDRESS flag is expected to fail.
			 */
			continue;
		}

		j9vmem_vmem_params_init(&params);

		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = J9PORT_VMEM_STRICT_ADDRESS;

		outputComment(portLibrary, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					"topDown: %s strictAddress: %s strictPageSize: %s useExtendedPrivateAreaMemory: %s\n",\
					params.pageSize, params.startAddress, params.endAddress,\
					(0 != (params.options & J9PORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

		/* did we get any memory? */
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx, page type 0x%zx.\n", \
							params.byteAmount, pageSizes[i], pageFlags[i]);
					
			goto exit;
			
		} else {
			outputComment(portLibrary, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%x at address 0x%zx\n",\
							params.byteAmount, vmemID.pageSize, vmemID.pageFlags, memPtr);

			if ((U_64) memPtr >= TWO_GIG_BAR) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\t returned memory: %p not less than 2G\n", memPtr);
				/* Don't exit here, need to free the allocate memory */
			}
			/* When using page size that can allocate memory only above 2G bar,
			 * j9vmem_reserve_memory_ex should fail to reserve memory below 2G using strict address
			 * but should fall back to 4K page size.
			 */
			if (FALSE == isPageSizeSupportedBelowBar(pageSizes[i], pageFlags[i])) {
				if (FOUR_KB != vmemID.pageSize) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\t didn't expect to allocate memory below 2GB using page size 0x%zx and page flags 0x%x with strict address\n",\
									vmemID.pageSize, vmemID.pageFlags);
					goto exit;
				}
			}
		}

		/* can we read and write to the memory? */
		j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", params.byteAmount);
		verifyMemory(portLibrary, testName, memPtr, params.byteAmount, allocName);

		/* free the memory */
		rc = j9vmem_free_memory(memPtr, params.byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_free_memory returned 0x%zx when trying to free 0x%zx bytes\n"
					, rc, params.byteAmount);
			goto exit;
		}

	}

exit:
	return reportTestExit(portLibrary, testName);

}

/**
 * Test request for pages below the 2G bar with J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA set on z/OS.
 * In this case, since STRICT_ADDRESS flag is not set, USE2TO32G flag is given priority over
 * the requested address range. Therefore, memory is expected to be allocated between 2G-32G.
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemoryLargePage(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA * pageSizes = NULL;
	UDATA * pageFlags = NULL;
	const char* testName = "j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemoryLargePage";
	I_32 i = 0;

	reportTestEntry(portLibrary, testName);

	/* Get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		char allocName[allocNameSize];
		void * memPtr = NULL;
		I_32 rc = 0;

		j9vmem_vmem_params_init(&params);

		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA;

		outputComment(portLibrary, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					"topDown: %s strictAddress: %s strictPageSize: %s useExtendedPrivateAreaMemory: %s\n",\
					params.pageSize, params.startAddress, params.endAddress,\
					(0 != (params.options & J9PORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					(0 != (params.options & J9PORT_VMEM_ZOS_USE_EXTENDED_PRIVATE_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

		/* did we get any memory? */
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx, page type 0x%zx.\n", \
							params.byteAmount, pageSizes[i], pageFlags[i]);

			goto exit;

		} else {
			outputComment(portLibrary, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%x at address 0x%zx\n",\
						params.byteAmount, vmemID.pageSize, vmemID.pageFlags, memPtr);

			if (((U_64) memPtr < TWO_GB) ||
				((U_64) memPtr >= SIXTY_FOUR_GB)
			) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\t returned memory: %p is not in 2G-32G range\n", memPtr);
				goto exit;
			}
		}

		/* can we read and write to the memory? */
		j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", params.byteAmount);
		verifyMemory(portLibrary, testName, memPtr, params.byteAmount, allocName);

		/* free the memory */
		rc = j9vmem_free_memory(memPtr, params.byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_free_memory returned 0x%zx when trying to free 0x%zx bytes\n"
					, rc, params.byteAmount);
			goto exit;
		}
	}

exit:
	return reportTestExit(portLibrary, testName);

}
#endif /* defined(J9ZOS390) */

#endif /* ENABLE_RESERVE_MEMORY_EX_TESTS */

/**
 * Verify port library memory management.
 * 
 * Make sure there is at least one page size returned by j9vmem_get_supported_page_sizes
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_test_commitOutsideOfReservedRange(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_test_commitOutsideOfReservedRange";
	char *memPtr = NULL;
	UDATA * pageSizes;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	J9PortVmemParams params;
	
	reportTestEntry(portLibrary, testName);

	pageSizes = j9vmem_supported_page_sizes();

	/* reserve and commit memory for each page size */
	for (i=0 ; pageSizes[i] != 0 ; i++) {
		j9vmem_vmem_params_init(&params);
		params.byteAmount = pageSizes[i];
		params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE;
		params.options |= J9PORT_VMEM_STRICT_PAGE_SIZE;
		params.pageSize = pageSizes[i];

		/* reserve */
		memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

		if (NULL == memPtr) {
			outputComment(PORTLIB, "! Could not find 0x%zx bytes available with page size 0x%zx\n", params.byteAmount, pageSizes[i]);
		} else {
			IDATA rc; 
			void *commitResult;

			/* attempt to commit 1 byte more than we reserved */
			commitResult = j9vmem_commit_memory(memPtr,params.byteAmount+1,&vmemID); 

			if (NULL != commitResult) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_commit_memory did not return an error when attempting to commit more memory"
						"than was reserved. \n");
			} else {
				commitResult = j9vmem_commit_memory(memPtr,params.byteAmount,&vmemID);
				
				if (NULL == commitResult) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_commit_memory returned an error while attempting to commit reserved memory: %s\n", j9error_last_error_message());
				} else {
					/* attempt to decommit 1 byte more than we reserved */
					rc = j9vmem_decommit_memory(memPtr,params.byteAmount+1,&vmemID);
					
					if (0 == rc) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_decommit_memory did not return an error when attempting to decommit "
								"more memory than was reserved. \n");
					} else {
						rc = j9vmem_decommit_memory(memPtr,params.byteAmount,&vmemID);
						
						if ( rc != 0) {
							outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_decommit_memory returned an error when attempting to decommit "
									"reserved memory, rc=%zd\n",rc);
						}
					}
				}
			}

			rc = j9vmem_free_memory(memPtr, params.byteAmount, &vmemID);
			if (rc != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned %d when trying to free 0x%zx bytes backed by 0x%zx-byte pages\n"
						, rc, params.byteAmount, pageSizes[i]);
			}
		}


	}

	return reportTestExit(portLibrary, testName);
}

#if defined(AIXPPC) || defined(LINUX) || defined(WIN32) || defined(WIN64) || defined(J9ZOS390)
/**
 * Verify port library memory management.
 * 
 * Ensure that we can execute code within memory returned by
 * j9vmem_reserve_memory_ex with the J0PORT_VMEM_MEMORY_MODE_EXECUTE
 * mode flag specified.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_test_reserveExecutableMemory(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_test_reserveExecutableMemory";
	unsigned int *memPtr = NULL;
	UDATA * pageSizes;
	UDATA * pageFlags;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	J9PortVmemParams params;
	
	reportTestEntry(portLibrary, testName);

	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i=0 ; pageSizes[i] != 0 ; i++) {
		j9vmem_vmem_params_init(&params);
		params.byteAmount = pageSizes[i];
		params.mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | 
						J9PORT_VMEM_MEMORY_MODE_EXECUTE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
		params.options |= J9PORT_VMEM_STRICT_PAGE_SIZE;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
	
		/* reserve */
		memPtr = j9vmem_reserve_memory_ex(&vmemID, &params);

		if (NULL == memPtr) {
			outputComment(PORTLIB, "! Could not find 0x%zx bytes available with page size 0x%zx and specified mode\n", params.byteAmount, pageSizes[i]);
		} else {
			IDATA rc;
			void (*function)();
			
#if defined(AIXPPC)
			
			*memPtr = 0x4e800020; /* blr instruction (equivalent to RET on x86)*/

			struct fDesc { 
				UDATA func_addr; 
				IDATA toc; 
				IDATA envp; 
			} myDesc; 
			myDesc.func_addr = (UDATA)memPtr;

			function = (void (*)()) &myDesc;
			outputComment(PORTLIB, "About to call dynamically created function, pageSize=0x%zx...\n",params.pageSize);
			function();
			outputComment(PORTLIB, "Dynamically created function returned successfully\n");

#elif defined(LINUX) || defined(WIN32) || defined(WIN64) || defined(J9ZOS390)
			size_t length;

#if defined(J9ZOS39064)
			/* On 64-bit z/OS, only 4K and pageable 1M large pages that allocate memory below 2G bar
			 * can be used as executable pages. Other page sizes do not support executable pages.
			 */
			if (FALSE == isPageSizeSupportedBelowBar(pageSizes[i], pageFlags[i])) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Did not expect to allocate executable pages with large page size 0x%zx, page type 0x%zx\n", \
						pageSizes[i], pageFlags[i]);
			} else {
#endif /* J9ZOS39064 */

				memset(memPtr, 0, params.pageSize);

				if ((UDATA)&myFunction2 > (UDATA)&myFunction1) {
					length = (size_t)((UDATA)&myFunction2 - (UDATA)&myFunction1);
				} else {
					length = (size_t)((UDATA)&myFunction1 - (UDATA)&myFunction2);
				}

				outputComment(PORTLIB, "function length = %d\n",length);

				memcpy(memPtr, (void *)&myFunction1, length);

				outputComment(PORTLIB, "*memPtr: 0x%zx\n",*memPtr);

				function = (void (*)()) memPtr;
				outputComment(PORTLIB, "About to call dynamically created function, pageSize=0x%zx...\n",params.pageSize);
				function();
				outputComment(PORTLIB, "Dynamically created function returned successfully\n");
#if defined(J9ZOS39064)
			}
#endif /* J9ZOS39064 */
			
#endif /* defined(AIXPPC) */

			rc = j9vmem_free_memory(memPtr, params.byteAmount, &vmemID);
			if (rc != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n"
						, rc, params.byteAmount, memPtr);
			}
		}
	}

	return reportTestExit(portLibrary, testName);
}
#endif /* defined(AIXPPC) || defined(LINUX) || defined(WIN32) || defined(WIN64) */

#if defined(J9VM_GC_VLHGC)
/**
 * Try to associate memory with each NUMA node at each page size.
 * 
 * If NUMA is not available, this test does nothing.
 * 
 * This test cannot test that the NUMA affinity is correctly set, just that the API succeeds.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9vmem_test_numa(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9vmem_test_numa";
	UDATA * pageSizes;
	UDATA * pageFlags;
	char *lastErrorMessage = NULL;
	I_32 lastErrorNumber = 0;
	UDATA totalNumaNodeCount = 0;
	IDATA detailReturnCode = 0;
	
	reportTestEntry(portLibrary, testName);

	/* First get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();
	
	/* Find out how many NUMA nodes are available */
	detailReturnCode = j9vmem_numa_get_node_details(NULL, &totalNumaNodeCount);
	
	if ((detailReturnCode != 0) || (0 == totalNumaNodeCount)) {
		outputComment(portLibrary, "\tNUMA not available\n");
	} else {
		UDATA i = 0;
		UDATA nodesWithMemoryCount = 0;
		UDATA originalNodeCount = totalNumaNodeCount;
		J9MemoryState policyForMemoryNode = J9NUMA_PREFERRED;
		UDATA nodeArraySize = sizeof(J9MemoryNodeDetail) * totalNumaNodeCount;
		J9MemoryNodeDetail *nodeArray = j9mem_allocate_memory(nodeArraySize, OMRMEM_CATEGORY_PORT_LIBRARY);
		IDATA rc = 0;
		
		if (nodeArray == NULL) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9vmem_free_memory returned NULL when trying to allocate J9MemoryNodeDetail array for NUMA test (FAIL)\n");
			goto exit;
		}
		memset(nodeArray, 0x0, nodeArraySize);
		rc = j9vmem_numa_get_node_details(nodeArray, &totalNumaNodeCount);
		if ((detailReturnCode != rc) || (totalNumaNodeCount != originalNodeCount)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS
					, "j9vmem_numa_get_node_details returned different results on second call (return %zd versus %zd, count %zu versus %zu)\n"
					, rc, detailReturnCode, totalNumaNodeCount, originalNodeCount);
			goto exit;
		}
		/* count the number of nodes with physical memory */
		{
			UDATA activeNodeIndex = 0;
			UDATA preferred = 0;
			UDATA allowed = 0;
			for (activeNodeIndex = 0; activeNodeIndex < totalNumaNodeCount; activeNodeIndex++) {
				J9MemoryState policy = nodeArray[activeNodeIndex].memoryPolicy;
				if (J9NUMA_PREFERRED == policy) {
					preferred += 1;
				} else if (J9NUMA_ALLOWED == policy) {
					allowed += 1;
				}
			}
			
			nodesWithMemoryCount = preferred;
			policyForMemoryNode = J9NUMA_PREFERRED;
			if (nodesWithMemoryCount == 0) {
				nodesWithMemoryCount = allowed;
				policyForMemoryNode = J9NUMA_ALLOWED;
			}
		}
		if (0 == nodesWithMemoryCount) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Found zero nodes with memory even after NUMA was reported as supported (FAIL)\n");
			goto exit;
		}
		/* reserve and commit memory for each page size */
		for (i=0 ; pageSizes[i] != 0 ; i++) {
			struct J9PortVmemIdentifier vmemID;
			char *memPtr = NULL;
			UDATA pageSize = pageSizes[i];
			UDATA reserveSizeInBytes = pageSize * nodesWithMemoryCount;
			
			/* reserve and commit */
#if defined(J9ZOS390)
			/* On z/OS skip this test for newly added large pages as obsolete j9vmem_reserve_memory() does not support them */
			if (TRUE == isNewPageSize(pageSizes[i], pageFlags[i])) {
				continue;
			}
#endif /* J9ZOS390 */
			memPtr = j9vmem_reserve_memory(
					NULL,
					reserveSizeInBytes, 
					&vmemID,
					J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE,
					pageSize, OMRMEM_CATEGORY_PORT_LIBRARY);
			
			outputComment(portLibrary, "\treserved 0x%zx bytes with page size 0x%zx at address 0x%zx\n", reserveSizeInBytes, vmemID.pageSize, memPtr);

			/* did we get any memory? */
			if (memPtr != NULL) {
				char allocName[allocNameSize];
				UDATA nodeIndex = 0;
				char * numaBlock = memPtr;
				void * commitResult = NULL;
				
				for (nodeIndex = 0; nodeIndex < totalNumaNodeCount; nodeIndex++) {
					if (policyForMemoryNode == nodeArray[nodeIndex].memoryPolicy) {
						UDATA nodeNumber = nodeArray[nodeIndex].j9NodeNumber;
						rc = j9vmem_numa_set_affinity(nodeNumber, numaBlock, pageSize, &vmemID); 
						if (rc != 0) {
							outputErrorMessage(PORTTEST_ERROR_ARGS
									, "j9vmem_numa_set_affinity returned 0x%zx when trying to set affinity for 0x%zx bytes at 0x%p to node %zu\n"
									, rc, pageSize, numaBlock, nodeNumber);
							goto exit;
						}
						numaBlock += pageSize;
					}
				}
				
				/* can we commit the memory? */
				commitResult = j9vmem_commit_memory(memPtr, numaBlock - memPtr, &vmemID);
				if (commitResult != memPtr) {
					outputErrorMessage(PORTTEST_ERROR_ARGS
							, "j9vmem_commit_memory returned 0x%p when trying to commit 0x%zx bytes at 0x%p\n"
							, commitResult, numaBlock - memPtr, memPtr);
					goto exit;
				}
				
				/* ideally we'd test that the memory actually has some NUMA characteristics, but that's difficult to prove */

				/* can we read and write to the memory? */
				j9str_printf(PORTLIB, allocName, allocNameSize, "j9vmem_reserve_memory(%d)", pageSize);
				verifyMemory(portLibrary, testName, memPtr, reserveSizeInBytes, allocName);
				
				/* free the memory (reuse the vmemID) */
				rc = j9vmem_free_memory(memPtr, reserveSizeInBytes, &vmemID);
				if (rc != 0) {
					outputErrorMessage(PORTTEST_ERROR_ARGS
							, "j9vmem_free_memory returned 0x%zx when trying to free 0x%zx bytes backed by 0x%zx-byte pages\n"
							, rc, reserveSizeInBytes, pageSize);
					goto exit;
				}
			}
		}
	}

exit:

	return reportTestExit(portLibrary, testName);
}
#endif /* defined(J9VM_GC_VLHGC) */

#define PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, pageSize, pageFlags) \
	outputComment(PORTLIB, "Input > mode: %zu, requestedPageSize: 0x%zx, requestedPageFlags: 0x%zx\n", mode, pageSize, pageFlags); \

void verifyFindValidPageSizeOutput(struct J9PortLibrary *portLibrary,
								char *testName,
								UDATA pageSizeExpected,
								UDATA pageFlagsExpected,
								UDATA isSizeSupportedExpected,
								UDATA pageSizeReturned,
								UDATA pageFlagsReturned,
								UDATA isSizeSupportedReturned)
{
	UDATA result = FALSE;
	PORT_ACCESS_FROM_PORT(portLibrary);

	outputComment(PORTLIB, "Expected > expectedPageSize: 0x%zx, expectedPageFlags: 0x%zx, isSizeSupported: 0x%zx\n\n",
			pageSizeExpected, pageFlagsExpected, isSizeSupportedExpected);

	if ((pageSizeExpected == pageSizeReturned) && (pageFlagsExpected == pageFlagsReturned)) {
		if (isSizeSupportedExpected != isSizeSupportedReturned) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "isSizeSupported should be: 0x%zx but found: 0x%zx\n",
								isSizeSupportedExpected, isSizeSupportedReturned);
		} else {
			result = TRUE;
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected j9vmem_find_valid_page_size() to return "
					"pageSize: 0x%zx, pageFlags: 0x%zx, but found pageSize: 0x%zx, pageFlags: 0x%zx\n",
					pageSizeExpected, pageFlagsExpected, pageSizeReturned, pageFlagsReturned);
	}

	if (TRUE == result) {
		outputComment(PORTLIB, "PASSED\n");
	}
}

#if (defined(LINUX) && !defined(LINUXPPC)) || defined(WIN32) || defined(OSX)

static int
j9vmem_testFindValidPageSize_impl(struct J9PortLibrary *portLibrary, char *testName)
{
#define MODE_NOT_USED 0

	UDATA requestedPageSize = 0;
	UDATA requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSizeSupported = FALSE;

	UDATA expectedPageSize = 0;
	UDATA expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN expectedIsSizeSupported = FALSE;

	UDATA defaultLargePageSize = 0;
	UDATA defaultLargePageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	UDATA * pageSizes = NULL;
	UDATA * pageFlags = NULL;

	BOOLEAN fourMBPageSize = FALSE;

	IDATA caseIndex = 1;

	PORT_ACCESS_FROM_PORT(portLibrary);

	outputComment(PORTLIB, "\n");
	reportTestEntry(portLibrary, testName);

	/* Linux and Windows support only one large page size
	 * which is also the default large page size at index 1 of PPG_vmem_pageSize.
	 * Moreover, since large page size supported by Linux and Windows is not fixed,
	 * we assume it to be 4M for testing purpose.
	 */
	j9vmem_default_large_page_size_ex(0, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		outputComment(PORTLIB, "defaultLargePageSize: 0x%zx, defaultLargePageFlags: 0x%zx\n", defaultLargePageSize, defaultLargePageFlags);
		fourMBPageSize = TRUE;
	}

	/* Note that -Xlp<size>, -Xlp:objectheap and -Xlp:codecache are treated same, and
	 * callers always pass J9PORT_VMEM_PAGE_FLAG_NOT_USED as the requestedPageFlags
	 * to j9vmem_find_valid_page_size().
	 * Only difference is in case of -Xlp<size>, j9vmem_find_valid_page_size() is not called
	 * if default large page size is not present.
	 */

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4M\n", caseIndex++);

	requestedPageSize = 4 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K\n", caseIndex++);

	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not in 4K, 4M>\n", caseIndex++);

	/* Add any new cases before this line */
	outputComment(PORTLIB, "%d tests completed\n", caseIndex);

	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	return reportTestExit(portLibrary, testName);
}

int
j9vmem_testFindValidPageSize(struct J9PortLibrary *portLibrary)
{

#if defined(LINUX) || defined(OSX)
#define J9PORT_VMEM_PAGESIZE_COUNT 5	/* This should be same as defined in port/unix_include/j9portpg.h */
#elif defined(WIN32)
#define J9PORT_VMEM_PAGESIZE_COUNT 3	/* This should be same as defined in port/win32_include/j9portpg.h */
#endif

	UDATA *pageSizes = NULL;
	UDATA *pageFlags = NULL;
	UDATA old_vmem_pageSize[J9PORT_VMEM_PAGESIZE_COUNT];
	UDATA old_vmem_pageFlags[J9PORT_VMEM_PAGESIZE_COUNT];
	char *testName = "j9vmem_testFindValidPageSize (setup)";
	int rc = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	/* Create copy of existing PPG_vmem_pageSize and PPG_vmem_pageFlags as they would be modified in this test */
	memcpy(old_vmem_pageSize, pageSizes, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));
	memcpy(old_vmem_pageFlags, pageFlags, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));

	testName = "j9vmem_testFindValidPageSize(no default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags without any large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 0;
	pageFlags[1] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);


	testName = "j9vmem_testFindValidPageSize(with 4M default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with 4M large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 4 * ONE_MB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);

	/* Restore PPG_vmem_pageSize and PPG_vmem_pageFlags to their actual values */
	memcpy(pageSizes, old_vmem_pageSize, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));
	memcpy(pageFlags, old_vmem_pageFlags, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));

_exit:
	return rc;
}

#elif defined(AIXPPC) || defined(LINUXPPC)

static int
j9vmem_testFindValidPageSize_impl(struct J9PortLibrary *portLibrary, char *testName)
{
#define MODE_NOT_USED 0

	UDATA requestedPageSize = 0;
	UDATA requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSizeSupported = FALSE;

	UDATA expectedPageSize = 0;
	UDATA expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN expectedIsSizeSupported = FALSE;

	UDATA defaultLargePageSize = 0;
	UDATA defaultLargePageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	UDATA mode = 0;
	UDATA * pageSizes = NULL;
	UDATA * pageFlags = NULL;

#if defined(J9VM_ENV_DATA64)
	BOOLEAN sixteenGBPageSize = FALSE;
#endif /* defined(J9VM_ENV_DATA64) */
	BOOLEAN sixteenMBPageSize = FALSE;
	BOOLEAN sixtyFourKBPageSize = FALSE;

	void *address = NULL;
	UDATA dataSegmentPageSize = 0;

	IDATA caseIndex = 1;
	IDATA i = 0;
	IDATA rc = 0;

	PORT_ACCESS_FROM_PORT(portLibrary);

	outputComment(PORTLIB, "\n");
	reportTestEntry(portLibrary, testName);

	/* First get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	for (i =0 ; pageSizes[i] != 0 ; i++) {
#if defined(J9VM_ENV_DATA64)
		if ((8*TWO_GB == pageSizes[i]) &&
			(0 != (J9PORT_VMEM_PAGE_FLAG_NOT_USED & pageFlags[i]))
		) {
			sixteenGBPageSize = TRUE;
		}
#endif /* defined(J9VM_ENV_DATA64) */
		if ((16*ONE_MB == pageSizes[i]) &&
			(0 != (J9PORT_VMEM_PAGE_FLAG_NOT_USED & pageFlags[i]))
		) {
			sixteenMBPageSize = TRUE;
		}

		if ((16*FOUR_KB == pageSizes[i]) &&
			(0 != (J9PORT_VMEM_PAGE_FLAG_NOT_USED & pageFlags[i]))
		) {
			sixtyFourKBPageSize = TRUE;
		}
	}

#if defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "16G page size supported: %s\n", (sixteenGBPageSize) ? "yes" : "no");
#endif /* defined(J9VM_ENV_DATA64) */
	outputComment(PORTLIB, "16M page size supported: %s\n", (sixteenMBPageSize) ? "yes" : "no");
	outputComment(PORTLIB, "64K page size supported: %s\n", (sixtyFourKBPageSize) ? "yes" : "no");

	/* On AIX default large page size should always be available.
	 * It is 16M, if available, 64K otherwise
	 */
	j9vmem_default_large_page_size_ex(0, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		outputComment(PORTLIB, "defaultLargePageSize: 0x%zx, defaultLargePageFlags: 0x%zx\n", defaultLargePageSize, defaultLargePageFlags);
	}

	/* Note that -Xlp<size> and -Xlp:objectheap are treated same, and
	 * callers always pass J9PORT_VMEM_PAGE_FLAG_NOT_USED as the requestedPageFlags
	 * to j9vmem_find_valid_page_size().
	 * Only difference is in case of -Xlp<size> j9vmem_find_valid_page_size() is not called
	 * if default large page size is not present.
	 */

#if defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=16G\n", caseIndex++);

	requestedPageSize = 8 * TWO_GB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == sixteenGBPageSize) {
		expectedPageSize = 8 * TWO_GB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* defined(J9VM_ENV_DATA64) */

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=16M\n", caseIndex++);

	requestedPageSize = 16 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == sixteenMBPageSize) {
		expectedPageSize = 16 * ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=64K\n", caseIndex++);

	requestedPageSize = 16 * FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != sixtyFourKBPageSize) {
		expectedPageSize = 16 * FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K\n", caseIndex++);

	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not in 4K, 64K, 16M, 16G>\n", caseIndex++);

	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);


	/* Test -Xlp:codecache options */

	/* First get the page size of the data segment. This is the page size used by the JIT code cache if 16M Code Pages are not being used */
#if defined(AIXPPC)
#define SAMPLE_BLOCK_SIZE 4
	/* Allocate a memory block using j9mem_allocate_memory, and use the address to get the page size of data segment */
	address = j9mem_allocate_memory(SAMPLE_BLOCK_SIZE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == address) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate block of memory to determine page size of data segment");
		goto _exit;
	}
	struct vm_page_info pageInfo;
	pageInfo.addr = (uint64_t) address;
	rc = vmgetinfo(&pageInfo, VM_PAGE_INFO, sizeof(struct vm_page_info));
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to get page size of data segment using vmgetinfo()");
		goto _exit;
	} else {
		dataSegmentPageSize = (UDATA) pageInfo.pagesize;
	}
	j9mem_free_memory(address);
#else
	dataSegmentPageSize = getpagesize();
#endif /* defined(AIXPPC) */

	outputComment(PORTLIB, "Page size of data segment: 0x%zx\n", dataSegmentPageSize);

#if defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=16G\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * TWO_GB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (8 * TWO_GB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

#endif /* defined(J9VM_ENV_DATA64) */

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=16M\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

#if defined(J9VM_ENV_DATA64)
	if (TRUE == sixteenMBPageSize) {
		expectedPageSize = 16 * ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else
#endif /* defined(J9VM_ENV_DATA64) */
	{
		expectedPageSize = dataSegmentPageSize;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = (16 * ONE_MB == dataSegmentPageSize) ? TRUE : FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=16M with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	if (TRUE == sixteenMBPageSize) {
		expectedPageSize = 16 * ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = dataSegmentPageSize;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = (16 * ONE_MB == dataSegmentPageSize) ? TRUE : FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(J9VM_ENV_DATA64) */

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=64K\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (16 * FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=64K with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (16 * FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(J9VM_ENV_DATA64) */

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(J9VM_ENV_DATA64) */

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not in 16G, 16M, 64K, 4K>\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not in 16G, 16M, 64K, 4K> with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(J9VM_ENV_DATA64) */

_exit:
	return reportTestExit(portLibrary, testName);
}

int
j9vmem_testFindValidPageSize(struct J9PortLibrary *portLibrary)
{
#define J9PORT_VMEM_PAGESIZE_COUNT 5	/* This should be same as defined in port/unix_include/j9portpg.h */

	UDATA *pageSizes = NULL;
	UDATA *pageFlags = NULL;
	UDATA old_vmem_pageSize[J9PORT_VMEM_PAGESIZE_COUNT];
	UDATA old_vmem_pageFlags[J9PORT_VMEM_PAGESIZE_COUNT];
	char *testName = "j9vmem_testFindValidPageSize (setup)";
	int rc = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	/* Create copy of existing PPG_vmem_pageSize and PPG_vmem_pageFlags as they would be modified in this test */
	memcpy(old_vmem_pageSize, pageSizes, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));
	memcpy(old_vmem_pageFlags, pageFlags, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));

	testName = "j9vmem_testFindValidPageSize(no default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags without any large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 0;
	pageFlags[1] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);


	testName = "j9vmem_testFindValidPageSize(64K large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags only 64K large page size  */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 16 * FOUR_KB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);


	testName = "j9vmem_testFindValidPageSize(64K and 16M large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with 64K and 16M large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 16 * FOUR_KB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 16 * ONE_MB;
	pageFlags[2] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[3] = 0;
	pageFlags[3] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);

#if defined(J9VM_ENV_DATA64)
	testName = "j9vmem_testFindValidPageSize(all possible page sizes)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with all possible large pages
	 * (16G, 16M, 64K)
	 */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 16 * FOUR_KB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 16 * ONE_MB;
	pageFlags[2] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[3] = 8 * TWO_GB;
	pageFlags[3] = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[4] = 0;
	pageFlags[4] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);
#endif /* defined(J9VM_ENV_DATA64) */

	/* Restore PPG_vmem_pageSize and PPG_vmem_pageFlags to their actual values */
	memcpy(pageSizes, old_vmem_pageSize, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));
	memcpy(pageFlags, old_vmem_pageFlags, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));

_exit:
	return rc;
}

#elif defined(J9ZOS390)

static int
j9vmem_testFindValidPageSize_impl(struct J9PortLibrary *portLibrary, char *testName)
{
	UDATA requestedPageSize = 0;
	UDATA requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSizeSupported = FALSE;

	UDATA expectedPageSize = 0;
	UDATA expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN expectedIsSizeSupported = FALSE;

	UDATA defaultLargePageSize = 0;
	UDATA defaultLargePageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;

	UDATA mode = 0;
	UDATA * pageSizes = NULL;
	UDATA * pageFlags = NULL;

#if	defined(J9VM_ENV_DATA64)
	BOOLEAN twoGBFixed = FALSE;
	BOOLEAN oneMBFixed = FALSE;
#endif /* defined(J9VM_ENV_DATA64) */
	BOOLEAN oneMBPageable = FALSE;

	IDATA caseIndex = 1;
	IDATA i = 0;

	PORT_ACCESS_FROM_PORT(portLibrary);

	outputComment(PORTLIB, "\n");
	reportTestEntry(portLibrary, testName);

	/* First get all the supported page sizes */
	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	for (i = 0; pageSizes[i] != 0; i++) {
#if	defined(J9VM_ENV_DATA64)
		if ((TWO_GB == pageSizes[i]) &&
			(0 != (J9PORT_VMEM_PAGE_FLAG_FIXED & pageFlags[i]))
		) {
			twoGBFixed = TRUE;
		}

		if ((ONE_MB == pageSizes[i]) &&
			(0 != (J9PORT_VMEM_PAGE_FLAG_FIXED & pageFlags[i]))
		) {
			oneMBFixed = TRUE;
		}
#endif /* defined(J9VM_ENV_DATA64) */
		if ((ONE_MB == pageSizes[i]) &&
				(0 != (J9PORT_VMEM_PAGE_FLAG_PAGEABLE & pageFlags[i]))
		) {
			oneMBPageable = TRUE;
		}
	}

#if defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "2G nonpageable supported: %s\n", (twoGBFixed) ? "yes" : "no");
	outputComment(PORTLIB, "1M nonpageable supported: %s\n", (oneMBFixed) ? "yes" : "no");
#endif /* defined(J9VM_ENV_DATA64) */
	outputComment(PORTLIB, "1M pageable supported: %s\n", (oneMBPageable) ? "yes" : "no");

	j9vmem_default_large_page_size_ex(0, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		outputComment(PORTLIB, "defaultLargePageSize: 0x%zx, defaultLargePageFlags: 0x%zx\n", defaultLargePageSize, defaultLargePageFlags);
	}

	/* Test -Xlp:codecache options */
	
	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=1M,pageable\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=1M,nonpageable\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not 4K, 1M>,pageable\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not 4K, 1M>,nonpageable\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K,pageable\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K,nonpageable\n", caseIndex++);
	mode = J9PORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	/* Test -Xlp:objectheap options */
	
#if defined(J9VM_ENV_DATA64)
	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=2G,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = TWO_GB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == twoGBFixed) {
		expectedPageSize = TWO_GB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
		expectedIsSizeSupported = TRUE;
	} else if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* defined(J9VM_ENV_DATA64) */

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=1M,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);
#if defined(J9VM_ENV_DATA64)
	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = TRUE;
	} else
#endif /* defined(J9VM_ENV_DATA64) */
	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not 4K, 1M, 2G>,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);
#if defined(J9VM_ENV_DATA64)
	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else
#endif /* defined(J9VM_ENV_DATA64) */
	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=1M,pageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not 4K, 1M>,pageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K,pageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(PORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(portLibrary, testName,
								expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								requestedPageSize, requestedPageFlags, isSizeSupported);

#if defined(J9VM_ENV_DATA64)

	/* Test -Xlp<size> options */

	outputComment(PORTLIB, "\nCase %d: -Xlp2G\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(PORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = TWO_GB;
		requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		if (TRUE == twoGBFixed) {
			expectedPageSize = TWO_GB;
			expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_FIXED;
			expectedIsSizeSupported = TRUE;
		} else {
			expectedPageSize = defaultLargePageSize;
			expectedPageFlags = defaultLargePageFlags;
			expectedIsSizeSupported = FALSE;
		}

		verifyFindValidPageSizeOutput(portLibrary, testName,
									expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									requestedPageSize, requestedPageFlags, isSizeSupported);
	}

	outputComment(PORTLIB, "\nCase %d: -Xlp1M\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(PORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = ONE_MB;
		requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = TRUE;

		verifyFindValidPageSizeOutput(portLibrary, testName,
									expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									requestedPageSize, requestedPageFlags, isSizeSupported);
	}

	outputComment(PORTLIB, "\nCase %d: -Xlp<not 4K, 1M, 2G>\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(PORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = 8 * ONE_MB;
		requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;

		verifyFindValidPageSizeOutput(portLibrary, testName,
									expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									requestedPageSize, requestedPageFlags, isSizeSupported);
	}

	outputComment(PORTLIB, "\nCase %d: -Xlp4K\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(PORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = FOUR_KB;
		requestedPageFlags = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		j9vmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		expectedPageSize = FOUR_KB;
		expectedPageFlags = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = TRUE;

		verifyFindValidPageSizeOutput(portLibrary, testName,
									expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									requestedPageSize, requestedPageFlags, isSizeSupported);
	}

#endif /* defined(J9VM_ENV_DATA64) */

_exit:
	return reportTestExit(portLibrary, testName);
}

int
j9vmem_testFindValidPageSize(struct J9PortLibrary *portLibrary)
{
#define J9PORT_VMEM_PAGESIZE_COUNT 5	/* This should be same as defined in port/zos390/j9portpg.h */

	UDATA *pageSizes = NULL;
	UDATA *pageFlags = NULL;
	UDATA old_vmem_pageSize[J9PORT_VMEM_PAGESIZE_COUNT];
	UDATA old_vmem_pageFlags[J9PORT_VMEM_PAGESIZE_COUNT];
	char *testName = "j9vmem_testFindValidPageSize (setup)";
	int rc = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	pageSizes = j9vmem_supported_page_sizes();
	pageFlags = j9vmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	/* Create copy of existing PPG_vmem_pageSize and PPG_vmem_pageFlags as they would be modified in this test */
	memcpy(old_vmem_pageSize, pageSizes, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));
	memcpy(old_vmem_pageFlags, pageFlags, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));

	testName = "j9vmem_testFindValidPageSize(no default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags without any large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = 0;
	pageFlags[1] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);


	testName = "j9vmem_testFindValidPageSize(pageable 1M pages)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with only Pageable 1MB large pages  */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);

#if defined(J9VM_ENV_DATA64)

	testName = "j9vmem_testFindValidPageSize(only default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with only default large page size (Non-pageable 1MB pages) */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);


	testName = "j9vmem_testFindValidPageSize(both pageable and nonpageable 1M pages)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with both pageable and non-pageable 1M large pages */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[2] = ONE_MB;
	pageFlags[2] = J9PORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[3] = 0;
	pageFlags[3] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);


	testName = "j9vmem_testFindValidPageSize(all possible page sizes)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with all possible large pages
	 * (non-pageable 2G, non-pageable 1M and pageable 1M)
	 */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = J9PORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[2] = ONE_MB;
	pageFlags[2] = J9PORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[3] = TWO_GB;
	pageFlags[3] = J9PORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[4] = 0;
	pageFlags[4] = 0;

	rc |= j9vmem_testFindValidPageSize_impl(portLibrary, testName);

#endif /* defined(J9VM_ENV_DATA64) */

	/* Restore PPG_vmem_pageSize and PPG_vmem_pageFlags to their actual values */
	memcpy(pageSizes, old_vmem_pageSize, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));
	memcpy(pageFlags, old_vmem_pageFlags, J9PORT_VMEM_PAGESIZE_COUNT * sizeof(UDATA));

_exit:
	return rc;
}

#endif /* defined(J9ZOS390) */


UDATA
getSizeFromString(struct J9PortLibrary *portLibrary, char *sizeString)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	if (0 == strcmp(sizeString, "4K")) {
		return FOUR_KB;
	} else if (0 == strcmp(sizeString, "64K")) {
		return SIXTY_FOUR_KB;
	} else if (0 == strcmp(sizeString, "16M")) {
		return D16M;
	} else if (0 == strcmp(sizeString, "2M")) {
		return D2M;
	} else if (0 == strcmp(sizeString, "256M")) {
		return D256M;
	} else if (0 == strcmp(sizeString, "512M")) {
		return D512M;
	} else if (0 == strcmp(sizeString, "2G")) {
		return TWO_GB;
#if defined (J9VM_ENV_DATA64)
	} else if (0 == strcmp(sizeString, "4G")) {
		return J9CONST_U64(2) * TWO_GB;
	} else if (0 == strcmp(sizeString, "10G")) {
		return J9CONST_U64(5) * TWO_GB;
	} else if (0 == strcmp(sizeString, "16G")) {
		return J9CONST_U64(8) * TWO_GB;
	} else if (0 == strcmp(sizeString, "32G")) {
		return J9CONST_U64(16) * TWO_GB;
#endif
	} else {
		outputComment(PORTLIB, "\n\nWarning: invalid option (%s)\n\n", sizeString);
	}
	return 0;
}

/**
 * Run with varied arguments based on values from /proc/meminfo, this function tests effects of disclaiming memory
 * on performance and prints out performance results
 *
 * @ref j9vmem.c
 *
 * @param[in] portLibrary The port library under test
 * @param[in] disclaimPerfTestArg This arg holds the page size to disclaim and the byte amount
 * @param[in] shouldDisclaim  If true, disclaim virtual memory, otherwise do not disclaim.
 *
 * @return 0 on success, -1 on failure
 */
I_32
j9vmem_disclaimPerfTests(struct J9PortLibrary *portLibrary, char *disclaimPerfTestArg, BOOLEAN shouldDisclaim)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;
	UDATA byteAmount = 0;
	UDATA memTotal = 0;
	UDATA pageSize = 0;
	char * pchar;
	UDATA numIterations = DEFAULT_NUM_ITERATIONS;

	/* parse the disclaimPerfTestArg to get the pageSize, byteAmount and disclaim values
	 * disclaimPerfTestArg is the string after the ':' from -disclaimPerfTest:<pageSize>,<byteAmount>,<totalMemory>[,<numIterations>]
	 * while shouldDisclaim is set from the +/- prefix, numIterations is optional (default is 50 iterations)
	 * Example options: -disclaimPerfTest:4K,256M,4G
	 *              	+disclaimPerfTest:4K,256M,4G,100
	 */
	pchar = strtok(disclaimPerfTestArg, ",");
	pageSize = getSizeFromString(portLibrary, pchar);

	pchar = strtok(NULL, ",");
	byteAmount = getSizeFromString(portLibrary, pchar);

	pchar = strtok(NULL, ",");
	memTotal = getSizeFromString(portLibrary, pchar);

	pchar = strtok(NULL, ",");
	if (NULL != pchar) {
		numIterations = atoi(pchar);
	}

	if ((0 == pageSize) || (0 == byteAmount) || (0 == memTotal)) {
		return -1;
	} else if (0 == numIterations) {
		outputComment(PORTLIB, "\n\nWarning: invalid option numIterations = (%s)\n\n", pchar);
		return -1;
	}

	/* Display unit under test */
	HEADING(portLibrary, "Disclaim performance tests");

	/* verify sanity of port library for these tests.  If this fails do not continue */
 	rc = j9vmem_verify_functiontable_slots(portLibrary);

	if (TEST_PASS == rc) {
		outputComment(portLibrary, "\npageSize: 0x%zx, byteAmount 0x%zx, memTotal 0x%zx, numIterations %i, disclaim %i\n", pageSize, byteAmount, memTotal, numIterations, shouldDisclaim);
		rc |= j9vmem_bench_write_and_decommit_memory(portLibrary, pageSize, byteAmount, shouldDisclaim, numIterations);
		rc |= j9vmem_bench_reserve_write_decommit_and_free_memory(portLibrary, pageSize, byteAmount, shouldDisclaim, numIterations);
#if defined (J9VM_ENV_DATA64)
		rc |= j9vmem_exhaust_virtual_memory(portLibrary, pageSize, byteAmount, shouldDisclaim);
#endif
		rc |= j9vmem_bench_force_overcommit_then_decommit(portLibrary, memTotal, pageSize, shouldDisclaim);

		/* Output results */
		outputComment(PORTLIB, "\nDisclaim performance test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	}
	return TEST_PASS == rc ? 0 : -1;
}

/**
 * Helper for j9vmem_testOverlappingSegments. Initializes the J9PortVmemParams struct.
 */
J9PortVmemParams*
getParams(struct J9PortLibrary *portLibrary, J9PortVmemParams *vmemParams) {
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA * pageSizes = j9vmem_supported_page_sizes();
	j9vmem_vmem_params_init(vmemParams);
	vmemParams->byteAmount = pageSizes[0];
	vmemParams->pageSize = pageSizes[0];
	vmemParams->mode |= J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_VIRTUAL;
	vmemParams->options |= J9PORT_VMEM_STRICT_PAGE_SIZE;
	vmemParams->category = OMRMEM_CATEGORY_PORT_LIBRARY;

	return vmemParams;
}

/**
 * Helper for j9vmem_testOverlappingSegments. Checks whether two memory segments overlap.
 */
BOOLEAN
checkForOverlap(J9PortVmemIdentifier *a, J9PortVmemIdentifier *b) {
	BOOLEAN A_before_B = ((UDATA)a->address + a->size) <= (UDATA)b->address;
	BOOLEAN B_before_A = ((UDATA)b->address + b->size) <= (UDATA)a->address;

	if (A_before_B || B_before_A) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Allocates CYCLES virtual memory segments and checks for overlapping segments.
 *
 * @ref j9vmem.c
 *
 * @param[in] portLibrary The port library under test
 *
 * @return 0 on success, -1 on failure
 */
int
j9vmem_testOverlappingSegments(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 result = 0;
	J9PortVmemParams *vmemParams;
	struct J9PortVmemIdentifier *vmemID;
	int *keepCycles;
	int freed = 0;
	int i = 0;
	int j = 0;
	const char* testName = "j9vmem__testOverlappingSegments";

	vmemParams = j9mem_allocate_memory(sizeof(J9PortVmemParams) * CYCLES, OMRMEM_CATEGORY_PORT_LIBRARY);
	vmemID = j9mem_allocate_memory(sizeof(J9PortVmemIdentifier) * CYCLES, OMRMEM_CATEGORY_PORT_LIBRARY);
	keepCycles = j9mem_allocate_memory(sizeof(int) * CYCLES, OMRMEM_CATEGORY_PORT_LIBRARY);

	reportTestEntry(portLibrary, testName);
	memset(keepCycles, 0, CYCLES);
	srand(10);

	outputComment(PORTLIB, "Cycles: %d\n\n", CYCLES);
	outputComment(PORTLIB, "Segment           Start             End               Size              Keep cycle\n");

	for (i = 0; i < CYCLES; i++) {
		char *memPtr = NULL;
		getParams(portLibrary, &vmemParams[i]);
		memPtr = j9vmem_reserve_memory_ex(&vmemID[i], &vmemParams[i]);

		if (NULL == memPtr) {
			outputComment(PORTLIB, "Failed to get memory. Error: %s.\n", strerror(errno));
			outputComment(PORTLIB, "Ignoring memory allocation failure(%d of %d loops finished).\n", i, CYCLES);
			goto exit;
		}
		/* Determine how long to keep the segment */
		keepCycles[i] = (rand() % (((int)((double)CYCLES/FREE_PROB)) - i)) + i + 1;
		outputComment(PORTLIB, "%.16d  %.16X  %.16X  %.16X  %d\n", i, (UDATA)vmemID[i].address, (UDATA)vmemID[i].address + vmemID[i].size, vmemID[i].size, keepCycles[i]);

		/* Check for overlapping segments */
		for (j = 0; j < i; j++) {
			if (keepCycles[j] >= i) { /* If not already freed */
				if (checkForOverlap(&vmemID[i], &vmemID[j])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "OVERLAP\n");
					outputComment(PORTLIB, "%.16d  %.16X  %.16X  %.16X  %d\n", j, (UDATA)vmemID[j].address, (UDATA)vmemID[j].address + vmemID[j].size, vmemID[j].size, keepCycles[j]);
					keepCycles[i] = i - 1; /* Pretend this segment is already freed to avoid double freeing overlapping segments */
					result = -1;
					goto exit;
				}
			}
		}

		/* Free segment if marked for freeing at this cycle*/
		for (j = 0; j < i; j++) {
			if (keepCycles[j] == i) {
				I_32 rc = j9vmem_free_memory(vmemID[j].address, vmemParams[j].byteAmount, &vmemID[j]);
				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Problem freeing segment...");
					outputComment(PORTLIB, "%.16d  %.16X  %.16X  %.16X  %d\n", j, (UDATA)vmemID[j].address, (UDATA)vmemID[j].address + vmemID[j].size, vmemID[j].size, keepCycles[j]);
					result = -1;
					goto exit;
				} else {
					outputComment(PORTLIB, "%.16d  Freed\n", j);
					freed++;
				}
			}
		}
	}
exit:
	outputComment(PORTLIB, "\n=========================\n");
	outputComment(PORTLIB, "%d cycles completed\n", i);
	outputComment(PORTLIB, "%d segments freed\n\n", freed);

	/* Free remaining segments */
	freed = 0;
	for (j = 0; j < CYCLES; j++) {
		if (keepCycles[j] >= i) {
			I_32 rc = j9vmem_free_memory(vmemID[j].address, vmemParams[j].byteAmount, &vmemID[j]);
			if (0 == rc) {
				freed++;
			}
		}
	}
	outputComment(PORTLIB, "%d remaining segments freed\n\n", freed);

	j9mem_free_memory(vmemParams);
	j9mem_free_memory(vmemID);
	j9mem_free_memory(keepCycles);
	reportTestExit(portLibrary, testName);
	return result;
}

/**
 * Verify port library memory management.
 * 
 * Exercise all API related to port library memory management found in
 * @ref j9vmem.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9vmem_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "vmem tests");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9vmem_verify_functiontable_slots(portLibrary);


	if (TEST_PASS == rc) {
		rc |= j9vmem_test_verify_there_are_page_sizes(portLibrary);
		rc |= j9vmem_testFindValidPageSize(portLibrary);
#if !defined(J9ZOS390)
		/* Commit and decommit do nothing on z/OS */
		rc |= j9vmem_test_commitOutsideOfReservedRange(portLibrary); 
#endif /* !J9ZOS390 */
		rc |= j9vmem_test1(portLibrary);
		rc |= j9vmem_decommit_memory_test(portLibrary);
		rc |= j9vmem_testOverlappingSegments(portLibrary);

#if defined(J9VM_GC_VLHGC)
		/* Only test j9vmem NUMA support on specs where it's needed. */
		rc |= j9vmem_test_numa(portLibrary);
#endif /* defined(J9VM_GC_VLHGC) */

#if defined(ENABLE_RESERVE_MEMORY_EX_TESTS)
		rc |= j9vmem_testReserveMemoryEx(portLibrary);
		rc |= j9vmem_testReserveMemoryExTopDown(portLibrary);
		rc |= j9vmem_testReserveMemoryExStrictAddress(portLibrary);
		rc |= j9vmem_testReserveMemoryExStrictPageSize(portLibrary);
		
#if defined(J9ZOS390)
		rc |= j9vmem_testReserveMemoryEx_zOSLargePageBelowBar(portLibrary);
		rc |= j9vmem_testReserveMemoryExStrictAddress_zOSLargePageBelowBar(portLibrary);
#if defined(J9ZOS39064)
		rc |= j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemoryLargePage(portLibrary);
		rc |= j9vmem_testReserveMemoryEx_zOS_useExtendedPrivateAreaMemory(portLibrary);
		rc |= j9vmem_testReserveMemoryExStrictPageSize_zOS_useExtendedPrivateAreaMemory(portLibrary);
#endif /* J9ZOS39064 */
#endif /* J9ZOS390 */

#if (defined(AIXPPC) || defined(LINUX) || defined(WIN32) || defined(WIN64) || defined(J9ZOS390)) && !(defined(HARDHAT) && defined(LINUXPPC))
		rc |= j9vmem_test_reserveExecutableMemory(portLibrary);
#endif /* defined(AIXPPC) || defined(LINUX) || defined(WIN32) || defined(WIN64) */
		
#endif /* ENABLE_RESERVE_MEMORY_EX_TESTS */
	}

	
	/* Output results */
	outputComment(PORTLIB, "\nMemory test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

/* This function is used by j9vmem_test_reserveExecutableMemory */
int myFunction1()
{
	return 1;
}

/* This function is used by j9vmem_test_reserveExecutableMemory to tell 
 * where myFunction1() ends. It is never called but its body must be
 * different from myFunction1() so that it is not optimized away by the
 * C compiler. */
void myFunction2()
{
	int i=0;
	while(++i<10);
	return;
}
