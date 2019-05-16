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
 * $RCSfile: j9memTest.c,v $
 * $Revision: 1.55 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library memory management.
 * 
 * Exercise the API for port library memory management operations.  These functions 
 * can be found in the file @ref j9mem.c  
 * 
 * @note port library memory management operations are not optional in the port library table.
 * 
 */
#include <stdlib.h>
#include <string.h>
#include "j9cfg.h"
#include "omrmemcategories.h"

#include "testHelpers.h"
#include "j9port.h"

#define MAX_ALLOC_SIZE 256 /**<@internal largest size to allocate */
#define J9MEM_ALLOCATE_FAKE_RETURN_VALUE ((void*) 0xBADC0DE) /**<@internal A known return for @ref fake_j9mem_allocate_memory */

#define allocNameSize 64 /**< @internal buffer size for name function */

#if defined(J9ZOS390)
#define MEM32_LIMIT 0X7FFFFFFF
#else
#define MEM32_LIMIT 0xFFFFFFFF
#endif

#if defined(J9VM_ENV_DATA64)
/* this macro corresponds to the one defined in j9mem32helpers */
#define HEAP_SIZE_BYTES 8*1024*1024
#endif

#define COMPLETE_LARGE_REGION 1
#define COMPLETE_NEAR_REGION 2
#define COMPLETE_SUBALLOC_BLOCK 4
#define COMPLETE_ALL_SIZES (COMPLETE_LARGE_REGION | COMPLETE_NEAR_REGION | COMPLETE_SUBALLOC_BLOCK)

static void freeMemPointers(struct J9PortLibrary *portLibrary, void **memPtrs, UDATA length);
static void shuffleArray(struct J9PortLibrary *portLibrary, UDATA *sizes, UDATA length);

/**
 * @internal
 * @typdef
 */
typedef void* (* J9MEM_ALLOCATE_MEMORY_FUNC) (struct J9PortLibrary *portLibrary, UDATA byteAmount, const char * callSite);

/**
 * @internal
 * Helper function for memory management verification.
 * 
 * @param[in] portLibrary The port library
 * @param[in] byteAmount Number of bytes to allocate.
 *
 * @return J9MEM_ALLOCATE_FAKE_RETURN_VALUE.
 */
static void *
fake_j9mem_allocate_memory(struct J9PortLibrary *portLibrary, UDATA byteAmount, const char *callSite)
{
	return J9MEM_ALLOCATE_FAKE_RETURN_VALUE;
}

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

	/* TODO alignment */
/*
	if (0 != ((UDATA)memPtr % sizeof(void*))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mem_allocate_memory(%d) pointer mis aligned\n", byteAmount);
	}
*/

	/* TODO double array alignment */

	/* Don't trample memory */
	if (0 == byteAmount) {
		return;
	}

	/* Test write */
	memPtr[0] = testCharA;
	if (testCharA != memPtr[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test 0\n", allocName);
	}

	/* Fill the buffer */
	memset(memPtr, testCharC, byteAmount);
	memset(stackMemory, testCharC, MAX_ALLOC_SIZE);

	/* Can only verify characters as large as the stack allocated size */
	if (MAX_ALLOC_SIZE < byteAmount) {
		testSize = MAX_ALLOC_SIZE;
	} else {
		testSize = byteAmount;
	}
	
	/* Check memory retained value */
	memPtr[testSize - 1] = testCharC;
	if (0 != memcmp(stackMemory, memPtr, testSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to retain value\n", allocName);
	}

	/* NUL terminate, check length */
	memPtr[byteAmount- 1] = '\0';
	if (strlen(memPtr) != byteAmount-1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test\n", allocName);
	}
}

/**
 * Verify port library memory management.
 * 
 * Ensure the port library is properly setup to run string operations.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */int
j9mem_test0(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9mem_test0";
	PORT_ACCESS_FROM_PORT(portLibrary);

	reportTestEntry(portLibrary, testName);
	 
	/* Verify that the memory management function pointers are non NULL */
	
	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibrary, it is not re-entrant safe
	 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_startup is NULL\n");
	}

	/*  Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_shutdown is NULL\n");
	}

	/* j9mem_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_allocate_memory is NULL\n");
	}

	/* j9mem_test1 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_free_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_free_memory is NULL\n");
	}

	/* j9mem_test4 */
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_reallocate_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_reallocate_memory, is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory32) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_allocate_memory32, is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_free_memory32) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_free_memory32, is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->mem_walk_categories) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_walk_categories, is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library memory management.
 * 
 * Ensure the port library memory management function 
 * @ref j9mem.c::j9mem_free_memory "j9mem_free_memory()"
 * handles NULL pointers as well as valid pointers values.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mem_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mem_test1";
	char *memPtr;

	reportTestEntry(portLibrary, testName);

	/* First verify that a NULL pointer can be freed, as specified by API docs. This ensures that one can free any pointer returned from
	 * allocation routines without checking for NULL first.
	 */
	memPtr = NULL;
	j9mem_free_memory(memPtr);
	if (NULL != memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mem_free_memory(NULL) altered pointer\n");
		goto exit; /* no point in continuing */		
	}

	/* One more test of free with a non NULL pointer */
	memPtr = j9mem_allocate_memory(32, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mem_allocate_memory(32) returned NULL\n");
		goto exit;
	}
	j9mem_free_memory(memPtr);
	
	/* What can we really check? */

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library memory management.
 * 
 * Allocate memory of various sizes, including a size of 0 using the 
 * port library function 
 * @ref j9mem.c::j9mem_allocate_memory "j9mem_allocate_memory()".
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9mem_test2(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mem_test2";

	char *memPtr;
	char allocName[allocNameSize];
	U_32 byteAmount;

	reportTestEntry(portLibrary, testName);

	/* allocate some memory of various sizes */
	byteAmount = 0;
	(void)j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_memory(%d)", byteAmount);
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = 1;
	(void)j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_memory(%d)", byteAmount);
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/8;
	(void)j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_memory(%d)", byteAmount);
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/4;
	(void)j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_memory(%d)", byteAmount);
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/2;
	(void)j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_memory(%d)", byteAmount);
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE;
	(void)j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_memory(%d)", byteAmount);
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	return reportTestExit(portLibrary, testName);
}


/**
 * Verify port library memory management.
 * 
 * Allocate memory of various sizes, including a size of 0.  Free this memory
 * then re-allocate via the port library function 
 * @ref j9mem.c::j9mem_reallocate_memory "j9mem_reallocate_memory()".
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int 
j9mem_test4(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mem_test4";

	/* Not visible outside the port library, so can't verify.  Should this be visible
	 * outside the portlibrary?
	 */
#if 0
	void *memPtr;
	void *saveMemPtr;
	char allocName[allocNameSize];
	I_32 rc;
	U_32 byteAmount;

	reportTestEntry(portLibrary, testName);

	/* allocate some memory and then free it */
	byteAmount = MAX_ALLOC_SIZE;
	memPtr = j9mem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == memPtr) {
		goto exit;
	}
	saveMemPtr = memPtr;
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	/* Now re-allocate memory of various sizes */
	/* TODO is memPtr supposed to be re-used? */
	byteAmount = 0;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_reallocate_memory(%d)", byteAmount);
	memPtr = j9mem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = 1;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_reallocate_memory(%d)", byteAmount);
	memPtr = j9mem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/8;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_reallocate_memory(%d)", byteAmount);
	memPtr = j9mem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/4;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_reallocate_memory(%d)", byteAmount);
	memPtr = j9mem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/2;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_reallocate_memory(%d)", byteAmount);
	memPtr = j9mem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_reallocate_memory(%d)", byteAmount);
	memPtr = j9mem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_free_memory(memPtr);
	memPtr = NULL;
#endif

	reportTestEntry(portLibrary, testName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library memory management.
 * 
 * Ensure the port library memory management function 
 * @ref j9mem.c::j9mem_deallocate_portLibrary "j9mem_deallocate_memory()"
 * handles NULL pointers as well as valid pointers values.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * 
 * @note This function does not need to be implemented in terms of
 * @ref j9mem.c::j9mem_free_memory "j9mem_free_memory()", but it may be on some
 * platforms.
 */
int
j9mem_test5(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mem_test5";

	/* Not visible outside the port library, so can't verify.  Should this be visible
	 * outside the portlibrary?
	 */
#if 0
	J9PortLibrary *memPtr;

	reportTestEntry(portLibrary, testName);

	/* First verify that a NULL pointer can be freed, then can free any pointer returned from
	 * allocation routines without checking for NULL
	 */
	memPtr = NULL;
	j9mem_deallocate_portLibrary(memPtr);
	if (NULL != memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mem_deallocate_portLibrary(NULL) altered pointer\n");
		goto exit; /* no point in continuing */		
	}

	/* One more test of free with a non NULL pointer */
	memPtr = j9mem_allocate_portLibrary(32);
	if (NULL == memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9mem_allocate_library(32) returned NULL\n");
		goto exit;
	}
	j9mem_deallocate_portLibrary(memPtr);
	
	/* What can we really check? */
#endif

	reportTestEntry(portLibrary, testName);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library memory management.
 * 
 * Self allocating port libraries call a function not available via the port library
 * table.  Need to verify that these functions have been correctly implemented.  These
 * functions are called by the port library management routines in @ref j9port.c "j9port.c".
 * The API for these functions clearly state they must be implemented.
 * 
 * Verify @ref j9mem.c::j9mem_allocate_portLibrary "j9mem_allocate_portLibrary()" allocates
 * memory correctly.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * 
 * @note This function does not need to be implemented in terms of
 * @ref j9mem.c::j9mem_allocate_memory "j9mem_allocate_memory()", but it may be on some
 * platforms.
 */
int
j9mem_test6(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mem_test6";

	/* Not visible outside the port library, so can't verify.  Should this be visible
	 * outside the portlibrary?
	 */
#if 0
	char *memPtr;
	char allocName[allocNameSize];
	I_32 rc;
	U_32 byteAmount;

	reportTestEntry(portLibrary, testName);

	/* allocate some memory of various sizes */
	byteAmount = 0;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_portLibrary(%d)", byteAmount);
	memPtr = j9mem_allocate_portLibrary(byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = 1;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_portLibrary(%d)", byteAmount);
	memPtr = j9mem_allocate_portLibrary(byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/8;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_portLibrary(%d)", byteAmount);
	memPtr = j9mem_allocate_portLibrary(byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/4;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_portLibrary(%d)", byteAmount);
	memPtr = j9mem_allocate_portLibrary(byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE/2;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_portLibrary(%d)", byteAmount);
	memPtr = j9mem_allocate_portLibrary(byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE;
	rc = j9str_printf(PORTLIB, allocName, allocNameSize, "j9mem_allocate_portLibrary(%d)", byteAmount);
	memPtr = j9mem_allocate_portLibrary(byteAmount);
	verifyMemory(portLibrary, testName, memPtr, byteAmount, allocName);
	j9mem_deallocate_portLibrary(memPtr);
	memPtr = NULL;
#endif

	reportTestEntry(portLibrary, testName);
	return reportTestExit(portLibrary, testName);
}

/* helper that shuffle contents of an array using a random number generator */
static void
shuffleArray(struct J9PortLibrary *portLibrary, UDATA *array, UDATA length)
{
	UDATA i;
	
	for (i = 0; i < length; i++) {
		IDATA randNum = (IDATA)(rand()%length);
		UDATA temp = array[i];
		
		array[i] = array[randNum];
		array[randNum] = temp;
	}
}

/**
 * Verify port library memory management.
 * 
 * Ensure the function @ref j9mem.c::j9mem_allocate_memory32 "j9mem_allocate_memory()"
 * returns a memory location that is below the 4 gig boundary, and as well that the entire region
 * is below the 4 gig boundary.
 * 
 * We allocate a variety of sizes comparable to the heap size used in mem_allocate_memory32 implementation (see HEAP_SIZE_BYTES in j9mem32helpers.c):
 * 1. size requests that are much smaller than the heap size are satisfied with j9heap suballocation.
 * 2. size requests that are slightly smaller than the heap size will not use j9heap suballocation (because they cannot be satisfied by 
 *    suballocating a j9heap due to heap management overhead), but rather given their own vmem allocated chunk.
 * 3. size requests that are greater or equal to the heap size also will not use j9heap suballocation, but rather given their own vmem allocated chunk.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 * 
 * @note This function does not need to be implemented in terms of
 * @ref j9mem.c::j9mem_free_memory "j9mem_free_memory()", but it may be on some
 * platforms.
 */
int
j9mem_test7_allocate32(struct J9PortLibrary *portLibrary, int randomSeed)
{
	void *pointer;
#if defined(J9VM_ENV_DATA64)
	UDATA finalAllocSize;
#endif
	char allocName[allocNameSize];
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9mem_test7_allocate32";
	
	/* Use a more exhaustive size array for 64 bit platforms to exercise the j9heap API.
	 * Retain the old sizes for regular 32 bit VMs, including ME platforms with very limited amount of physical memory.
	 */
#if defined(J9VM_ENV_DATA64)
	UDATA allocBlockSizes[] = {0, 1, 512, 4096, 1024*1024, 
			HEAP_SIZE_BYTES/8, HEAP_SIZE_BYTES/4, HEAP_SIZE_BYTES/3, HEAP_SIZE_BYTES/2,
			HEAP_SIZE_BYTES-6, HEAP_SIZE_BYTES-10,
			HEAP_SIZE_BYTES, HEAP_SIZE_BYTES+6};
#else
	UDATA allocBlockSizes[] = {0,1,512,4096,4096*1024};
#endif
	UDATA allocBlockSizesLength = sizeof(allocBlockSizes)/sizeof(allocBlockSizes[0]); 
	void *allocBlockReturnPtrs[sizeof(allocBlockSizes)/sizeof(allocBlockSizes[0])];
	UDATA allocBlockCursor = 0;
	
	
	reportTestEntry(portLibrary, testName);
	
	if (0 == randomSeed) {
		randomSeed = (int) j9time_current_time_millis();
	}
	outputComment(portLibrary, "Random seed value: %d. Add -srand:[seed] to the command line to reproduce this test manually.\n", randomSeed);
	srand(randomSeed);
	
	shuffleArray(portLibrary, allocBlockSizes, allocBlockSizesLength);
	
	for (; allocBlockCursor < allocBlockSizesLength; allocBlockCursor++) {		
		UDATA byteAmount = allocBlockSizes[allocBlockCursor];
		void **returnPtrLocation = &allocBlockReturnPtrs[allocBlockCursor]; 
		
		j9str_printf(PORTLIB, allocName, allocNameSize, "\nj9mem_allocate_memory32(%d)", byteAmount);
		pointer = j9mem_allocate_memory32(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
		verifyMemory(portLibrary, testName, pointer, byteAmount, allocName);
		
#if defined(J9VM_ENV_DATA64)
		if ( ((UDATA)pointer + byteAmount) > MEM32_LIMIT) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Entire memory block allocated by %s is not below 32-bit limit\n", allocName);
		}
#endif
		*returnPtrLocation = pointer;
	}
	
	/* all return pointers are verified, now free them */
	freeMemPointers(portLibrary, allocBlockReturnPtrs, allocBlockSizesLength);
		
#if defined(J9VM_ENV_DATA64)
	/* now we should have at least one entirely free J9Heap, try suballocate a large portion of the it. 
	 * This should not incur any vmem allocation.  
	 */
	finalAllocSize = HEAP_SIZE_BYTES-60;
	j9str_printf(PORTLIB, allocName, allocNameSize, "\nj9mem_allocate_memory32(%d)", finalAllocSize);
	pointer = j9mem_allocate_memory32(finalAllocSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(portLibrary, testName, pointer, finalAllocSize, allocName);
#endif
	
	/* should not result in a crash */
	j9mem_free_memory32(NULL);
	
	return reportTestExit(portLibrary, testName);
}


/* Dummy categories for j9mem_test8_categories */

#define DUMMY_CATEGORY_ONE 0
#define DUMMY_CATEGORY_TWO 1
#define DUMMY_CATEGORY_THREE 2

static U_32 childrenOfDummyCategoryOne[] = {DUMMY_CATEGORY_TWO, DUMMY_CATEGORY_THREE, OMRMEM_CATEGORY_PORT_LIBRARY, OMRMEM_CATEGORY_UNKNOWN};

static OMRMemCategory dummyCategoryOne = {"Dummy One", DUMMY_CATEGORY_ONE, 0, 0, 4, childrenOfDummyCategoryOne};

static OMRMemCategory dummyCategoryTwo = {"Dummy Two", DUMMY_CATEGORY_TWO, 0, 0, 0, NULL};

static OMRMemCategory dummyCategoryThree = {"Dummy Three", DUMMY_CATEGORY_THREE, 0, 0, 0, NULL};

static OMRMemCategory* categoryList[3] = {&dummyCategoryOne, &dummyCategoryTwo, &dummyCategoryThree};

static OMRMemCategorySet dummyCategorySet = {3, categoryList};

/* Structure used to summarise the state of the categories */
struct CategoriesState {
	BOOLEAN dummyCategoryOneWalked;
	BOOLEAN dummyCategoryTwoWalked;
	BOOLEAN dummyCategoryThreeWalked;
	BOOLEAN portLibraryWalked;
	BOOLEAN unknownWalked;
#if defined(J9VM_ENV_DATA64)
	BOOLEAN unused32bitSlabWalked;
#endif


	BOOLEAN otherError;

	UDATA dummyCategoryOneBytes;
	UDATA dummyCategoryOneBlocks;
	UDATA dummyCategoryTwoBytes;
	UDATA dummyCategoryTwoBlocks;
	UDATA dummyCategoryThreeBytes;
	UDATA dummyCategoryThreeBlocks;
	UDATA portLibraryBytes;
	UDATA portLibraryBlocks;
	UDATA unknownBytes;
	UDATA unknownBlocks;
#if defined(J9VM_ENV_DATA64)
	UDATA unused32bitSlabBytes;
	UDATA unused32bitSlabBlocks;
#endif
};

static UDATA categoryWalkFunction(U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * walkState)
{
	J9PortLibrary * portLibrary = (J9PortLibrary*)walkState->userData1;
	struct CategoriesState * state = (struct CategoriesState *)walkState->userData2;
	const char* testName = "j9mem_test8_categories - categoryWalkFunction";

	switch (categoryCode) {
	case DUMMY_CATEGORY_ONE:
		if (! isRoot) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_ONE should be a root, and isn't\n");
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (categoryName != dummyCategoryOne.name) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_ONE has the wrong name: %p=%s instead of %p=%s\n", categoryName, categoryName, dummyCategoryOne.name, dummyCategoryOne.name);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		state->dummyCategoryOneWalked = TRUE;
		state->dummyCategoryOneBytes = liveBytes;
		state->dummyCategoryOneBlocks = liveAllocations;

		break;

	case DUMMY_CATEGORY_TWO:
		if (isRoot) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_TWO shouldn't be a root, and is\n");
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (DUMMY_CATEGORY_ONE != parentCategoryCode) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_TWO's parent should be DUMMY_CATEGORY_ONE, not %u\n", parentCategoryCode);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (categoryName != dummyCategoryTwo.name) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_TWO has the wrong name: %p=%s instead of %p=%s\n", categoryName, categoryName, dummyCategoryTwo.name, dummyCategoryTwo.name);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		state->dummyCategoryTwoWalked = TRUE;
		state->dummyCategoryTwoBytes = liveBytes;
		state->dummyCategoryTwoBlocks = liveAllocations;
		break;

	case DUMMY_CATEGORY_THREE:
		if (isRoot) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_THREE shouldn't be a root, and is\n");
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (DUMMY_CATEGORY_ONE != parentCategoryCode) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_THREE's parent should be DUMMY_CATEGORY_ONE, not %u\n", parentCategoryCode);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (categoryName != dummyCategoryThree.name) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_THREE has the wrong name: %p=%s instead of %p=%s\n", categoryName, categoryName, dummyCategoryThree.name, dummyCategoryThree.name);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		state->dummyCategoryThreeWalked = TRUE;
		state->dummyCategoryThreeBytes = liveBytes;
		state->dummyCategoryThreeBlocks = liveAllocations;
		break;

	case OMRMEM_CATEGORY_PORT_LIBRARY:
		/* Root status changes depending on whether we're using the default category set or not. */
		state->portLibraryWalked = TRUE;
		state->portLibraryBytes = liveBytes;
		state->portLibraryBlocks = liveAllocations;

		break;

	case OMRMEM_CATEGORY_UNKNOWN:
		/* Root status changes depending on whether we're using the default category set or not. */
		state->unknownWalked = TRUE;
		state->unknownBytes = liveBytes;
		state->unknownBlocks = liveAllocations;
		break;

#if defined(J9VM_ENV_DATA64)
	case OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS:
		state->unused32bitSlabWalked = TRUE;
		state->unused32bitSlabBytes = liveBytes;
		state->unused32bitSlabBlocks = liveAllocations;
		break;
#endif

	default:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected categoryCode: %u\n", categoryCode);
		state->otherError = TRUE;
		return J9MEM_CATEGORIES_STOP_ITERATING;
	}

	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Utility method that calls j9mem_walk_categories and populates the CategoriesState structure
 */
static void
getCategoriesState(struct J9PortLibrary *portLibrary, struct CategoriesState * state)
{
	OMRMemCategoryWalkState walkState;
	PORT_ACCESS_FROM_PORT(portLibrary);

	memset(state, 0, sizeof(struct CategoriesState));
	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.userData1 = portLibrary;
	walkState.userData2 = state;
	walkState.walkFunction = &categoryWalkFunction;

	j9mem_walk_categories(&walkState);
}

/**
 * Verifies that memory categories work with the j9mem_memory_allocate, reallocate and allocate32 APIs
 *
 * We test:
 *
 * - That unknown categories are mapped to unknown
 * - That each API increments and decrements the appropriate counter
 * - That the special OMRMEM_CATEGORY_PORT_LIBRARY works
 *
 * The tests for vmem, mmap and shmem test those APIs also manipulate the counters properly
 */
int
j9mem_test8_categories(struct J9PortLibrary *portLibrary)
{
	struct CategoriesState categoriesState;
	const char* testName = "j9mem_test8_categories";
	PORT_ACCESS_FROM_PORT(portLibrary);

	reportTestEntry(portLibrary, testName);

	/* Reset the categories */
	j9port_control(J9PORT_CTLDATA_MEM_CATEGORIES_SET, 0);

	/* Check that walking the categories state only walks port library & unknown categories */
	getCategoriesState(portLibrary, &categoriesState);

	if (categoriesState.dummyCategoryOneWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "dummyCategoryOne unexpectedly walked\n");
		goto end;
	}
	if (categoriesState.dummyCategoryTwoWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "dummyCategoryTwo unexpectedly walked\n");
		goto end;
	}
	if (categoriesState.dummyCategoryThreeWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "dummyCategoryThree unexpectedly walked\n");
		goto end;
	}
	if (! categoriesState.portLibraryWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary unexpectedly not walked\n");
		goto end;
	}
	if (! categoriesState.unknownWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unknown unexpectedly not walked\n");
		goto end;
	}
#if defined(J9VM_ENV_DATA64)
	if (! categoriesState.unused32bitSlabWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unused32bitSlab unexpectedly not walked\n");
		goto end;
	}
#endif
	if (categoriesState.otherError) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
		goto end;
	}

	/* Try allocating under the port library category and check the block and byte counters are incremented */
	{
		UDATA initialBlocks;
		UDATA initialBytes;
		UDATA finalBlocks;
		UDATA finalBytes;
		UDATA expectedBlocks;
		UDATA expectedBytes;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocks = categoriesState.portLibraryBlocks;
		initialBytes = categoriesState.portLibraryBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory(32, OMRMEM_CATEGORY_PORT_LIBRARY);

		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 32;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.portLibraryBlocks;
		finalBytes = categoriesState.portLibraryBytes;

		j9mem_free_memory(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.portLibraryBlocks;
		finalBytes = categoriesState.portLibraryBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* Try allocating with a user category code - having not registered any categories. Check it maps to unknown */
	{
		UDATA initialBlocks;
		UDATA initialBytes;
		UDATA finalBlocks;
		UDATA finalBytes;
		UDATA expectedBlocks;
		UDATA expectedBytes;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocks = categoriesState.unknownBlocks;
		initialBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;

		j9mem_free_memory(ptr);
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* Register our dummy categories */
	j9port_control(J9PORT_CTLDATA_MEM_CATEGORIES_SET, (UDATA) &dummyCategorySet);

	/* Try allocating to one of our user category codes - check the arithmetic is done correctly */
	{
		UDATA initialBlocks;
		UDATA initialBytes;
		UDATA finalBlocks;
		UDATA finalBytes;
		UDATA expectedBlocks;
		UDATA expectedBytes;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocks = categoriesState.dummyCategoryOneBlocks;
		initialBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;

		j9mem_free_memory(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}
		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* Try allocating with allocate32 */
	{
		UDATA initialBlocks;
		UDATA initialBytes;
		UDATA finalBlocks;
		UDATA finalBytes;
		UDATA expectedBlocks;
		UDATA expectedBytes;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocks = categoriesState.dummyCategoryOneBlocks;
		initialBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory32(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;

		j9mem_free_memory32(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}
		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* On 64 bit, check that allocate32 manipulates the "unused 32 bit slab" category */
	/* n.b. we're reliant on previous tests having initialized the 32bithelpers. */
#if defined(J9VM_ENV_DATA64)
	{
		UDATA initialBlocks;
		UDATA initialBytes;
		UDATA finalBlocks;
		UDATA finalBytes;
		UDATA expectedBlocks;
		UDATA expectedBytes;
		UDATA initialUnused32BitSlabBytes;
		UDATA initialUnused32BitSlabBlocks;
		UDATA minimumExpectedUnused32BitSlabBytes;
		UDATA finalUnused32BitSlabBytes;
		UDATA finalUnused32BitSlabBlocks;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocks = categoriesState.dummyCategoryOneBlocks;
		initialBytes = categoriesState.dummyCategoryOneBytes;
		initialUnused32BitSlabBytes = categoriesState.unused32bitSlabBytes;
		initialUnused32BitSlabBlocks = categoriesState.unused32bitSlabBlocks;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory32(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;
		/* Should have decremented by at least 64 bytes */
		minimumExpectedUnused32BitSlabBytes = initialUnused32BitSlabBytes - 64;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		finalUnused32BitSlabBytes = categoriesState.unused32bitSlabBytes;
		finalUnused32BitSlabBlocks = categoriesState.unused32bitSlabBlocks;

		j9mem_free_memory32(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}
		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}
		/* Should have the same number of unused slab blocks */
		if (finalUnused32BitSlabBlocks != initialUnused32BitSlabBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of unused 32bit slab blocks should have stayed the same. Was %zu, now %zu\n", initialUnused32BitSlabBlocks, finalUnused32BitSlabBlocks);
			goto end;
		}

		if (finalUnused32BitSlabBytes > minimumExpectedUnused32BitSlabBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected value for unused 32 bit slab bytes. Expected %zu, was %zu. Initial value was %zu.\n", minimumExpectedUnused32BitSlabBytes, finalUnused32BitSlabBytes, initialUnused32BitSlabBytes);
			goto end;
		}

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		finalUnused32BitSlabBytes = categoriesState.unused32bitSlabBytes;
		finalUnused32BitSlabBlocks = categoriesState.unused32bitSlabBlocks;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
		if (finalUnused32BitSlabBlocks != initialUnused32BitSlabBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of unused 32 bit slab blocks after free. Expected %zu, now %zu\n", initialUnused32BitSlabBlocks, finalUnused32BitSlabBlocks);
			goto end;
		}
		if (finalUnused32BitSlabBytes != initialUnused32BitSlabBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of unused 32 bit slab bytes after free. Expected %zu, now %zu\n", initialUnused32BitSlabBytes, finalUnused32BitSlabBytes);
			goto end;
		}
	}
#endif

	/* Try allocating and reallocating */
	{
		UDATA initialBlocksCat1;
		UDATA initialBytesCat1;
		UDATA initialBlocksCat2;
		UDATA initialBytesCat2;
		UDATA finalBlocksCat1;
		UDATA finalBytesCat1;
		UDATA finalBlocksCat2;
		UDATA finalBytesCat2;
		UDATA expectedBlocks1;
		UDATA expectedBytes1;
		UDATA expectedBlocks2;
		UDATA expectedBytes2;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		initialBytesCat1 = categoriesState.dummyCategoryOneBytes;
		initialBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		initialBytesCat2 = categoriesState.dummyCategoryTwoBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks1 = initialBlocksCat1 + 1;
		expectedBytes1 = initialBytesCat1 + 64;
		expectedBlocks2 = initialBlocksCat2;
		expectedBytes2 = initialBlocksCat2;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		finalBytesCat1 = categoriesState.dummyCategoryOneBytes;
		finalBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		finalBytesCat2 = categoriesState.dummyCategoryTwoBytes;

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
		}
		if (finalBlocksCat1 != expectedBlocks1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks1, finalBlocksCat1);
		}
		if (finalBytesCat1 < expectedBytes1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes1, finalBytesCat1);
		}
		if (finalBlocksCat2 != expectedBlocks2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks2, finalBlocksCat2);
		}
		if (finalBytesCat2 < expectedBytes2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes2, finalBytesCat2);
		}

		/* reallocate under a different category */

		ptr = j9mem_reallocate_memory(ptr, 128, DUMMY_CATEGORY_TWO);

		expectedBlocks1 = initialBlocksCat1;
		expectedBytes1 = initialBytesCat1;
		expectedBlocks2 = initialBlocksCat2 + 1;
		expectedBytes2 = initialBlocksCat2 + 128;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		finalBytesCat1 = categoriesState.dummyCategoryOneBytes;
		finalBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		finalBytesCat2 = categoriesState.dummyCategoryTwoBytes;

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
		}
		if (finalBlocksCat1 != expectedBlocks1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks1, finalBlocksCat1);
		}
		if (finalBytesCat1 < expectedBytes1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes1, finalBytesCat1);
		}
		if (finalBlocksCat2 != expectedBlocks2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks2, finalBlocksCat2);
		}
		if (finalBytesCat2 < expectedBytes2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes2, finalBytesCat2);
		}

		j9mem_free_memory(ptr);

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		finalBytesCat1 = categoriesState.dummyCategoryOneBytes;
		finalBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		finalBytesCat2 = categoriesState.dummyCategoryTwoBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocksCat1 != initialBlocksCat1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocksCat1, finalBlocksCat1);
			goto end;
		}
		if (finalBytesCat1 != initialBytesCat1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytesCat1, finalBytesCat1);
			goto end;
		}
		if (finalBlocksCat2 != initialBlocksCat2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocksCat2, finalBlocksCat2);
			goto end;
		}
		if (finalBytesCat2 != initialBytesCat2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytesCat2, finalBytesCat2);
			goto end;
		}
	}



	/* Try allocating to an unknown category code - check it maps to unknown properly */
	{
		UDATA initialBlocks;
		UDATA initialBytes;
		UDATA finalBlocks;
		UDATA finalBytes;
		UDATA expectedBlocks;
		UDATA expectedBytes;
		void * ptr;

		getCategoriesState(portLibrary, &categoriesState);
		initialBlocks = categoriesState.unknownBlocks;
		initialBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = j9mem_allocate_memory(64, DUMMY_CATEGORY_THREE + 1);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;
		if (! categoriesState.unknownWalked) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Category walk didn't cover unknown category.\n");
			goto end;
		}
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		j9mem_free_memory(ptr);

		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(portLibrary, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

end:
	/* Reset categories to NULL */
	j9port_control(J9PORT_CTLDATA_MEM_CATEGORIES_SET, 0);

	return reportTestExit(portLibrary, testName);
}

static U_32 categoriesWalked;

/**
 * Category walk function that always returns STOP_ITERATING.
 *
 * Used with j9mem_test9_category_walk
 */
static UDATA categoryWalkFunction2(U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * walkState)
{
	categoriesWalked++;

	return J9MEM_CATEGORIES_STOP_ITERATING;
}

/*
 * Tests that the memory category walk can be stopped by returning J9MEM_CATEGORIES_STOP_ITERATING
 */
int
j9mem_test9_category_walk(struct J9PortLibrary* portLibrary)
{
	const char* testName = "j9mem_test9_category_walk";
	OMRMemCategoryWalkState walkState;
	PORT_ACCESS_FROM_PORT(portLibrary);

	reportTestEntry(portLibrary, testName);

	/* Register our dummy categories */
	j9port_control(J9PORT_CTLDATA_MEM_CATEGORIES_SET, (UDATA) &dummyCategorySet);

	categoriesWalked = 0;

	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.walkFunction = &categoryWalkFunction2;

	j9mem_walk_categories(&walkState);

	if (categoriesWalked != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of categories walk. Expected 1, got %u\n", categoriesWalked);
	}

	/* Reset categories to NULL */
	j9port_control(J9PORT_CTLDATA_MEM_CATEGORIES_SET, 0);

	return reportTestExit(portLibrary, testName);
}

/* attempt to free all mem pointers stored in memPtrs array with length */
static void
freeMemPointers(struct J9PortLibrary *portLibrary, void **memPtrs, UDATA length)
{
	void *mem32Ptr = NULL;
	UDATA i;
	
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	for (i = 0; i<length; i++) {
		mem32Ptr = memPtrs[i];
		j9mem_free_memory32(mem32Ptr);
	}
}

/**
 * Verify port library memory management.
 * 
 * Exercise all API related to port library memory management found in
 * @ref j9mem.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9mem_runTests(struct J9PortLibrary *portLibrary, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "Memory test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9mem_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9mem_test1(portLibrary);
		rc |= j9mem_test2(portLibrary);
		/* removed test 3 due to DESIGN 355 (no longer separate allocate function for callsite) */
		rc |= j9mem_test4(portLibrary);
		rc |= j9mem_test5(portLibrary);
		rc |= j9mem_test6(portLibrary);
		rc |= j9mem_test7_allocate32(portLibrary, randomSeed);
		rc |= j9mem_test8_categories(portLibrary);
		rc |= j9mem_test9_category_walk(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nMemory test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

