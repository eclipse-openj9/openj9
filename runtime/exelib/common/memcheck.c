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

#include "j9cfg.h"
#include "exelib_internal.h"
#include "memchknls.h"
#include "memcheck.h"
#ifdef J9ZOS390
#include "atoe.h"
#endif

/* uncomment this line if you want support the the 'simulate' option */
/*#define J9VM_MEMCHK_SIM_SUP
*/

#ifdef J9VM_MEMCHK_SIM_SUP
#define SIM_TAG "/*MEMSIM*/"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9port.h"
#include "j9protos.h"
#include "avl_api.h"
#include "libhlp.h"		/* uses main_setNLSCatalog */
#include "omrmutex.h"
#include "ute_core.h"

#if defined (WIN32) && !defined(BREW)
/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#endif

/* Padding size for the beginning and end of each block, in bytes.  This should be a multiple
 of 8 (for alignment purposes).  If the padding size is smaller than sizeof(J9MemoryCheckHeader)
 it will be increased to that size automatically. */
#define J9_MEMCHECK_PADDING_SIZE			512
#define J9_MEMCHECK_DATA_PADDING_VALUE		((U_32) 0xDEADBEEF	)
#define J9_MEMCHECK_DATA_FILL_VALUE			((U_32) 0xE7E7E7E7	)
#define J9_MEMCHECK_DATA_FREED_VALUE		((U_32) 0xBEF0DDED	)
#define J9_MEMCHECK_CODE_PADDING_VALUE		((U_32) 0xBAADF00D	)
#define J9_MEMCHECK_CODE_FILL_VALUE			((U_32) 0xF7F7F7F7	)
#define J9_MEMCHECK_CODE_FREED_VALUE		((U_32) 0xCCCCCCCC	)
/* Must be defined, this limits the number of leaked memory blocks that will be dumped on shutdown.
	otherwise, it might print out thousands of blocks... ;) */
#define J9_MEMCHECK_MAX_DUMP_LEAKED_BLOCKS		32
typedef void *(* J9_MEM_ALLOCATE_FUNC)(OMRPortLibrary *, UDATA byteAmount, const char *callsite, U_32 category);
typedef void (* J9_MEM_FREE_FUNC)(OMRPortLibrary *, void *memoryPointer);
typedef void (* J9_MEM_FREE_CODE_FUNC)(OMRPortLibrary *, void *memoryPointer, UDATA size);
typedef I_32 (* J9_SHUTDOWN_FUNC)(OMRPortLibrary *);
typedef void (* J9_SHUTDOWN_AND_EXIT_FUNC)(OMRPortLibrary *, I_32 exitCode);
typedef I_32 (* J9_PORT_CONTROL_FUNC)(OMRPortLibrary *portLib, const char* key, UDATA value);
typedef void (* J9_MEM_SHUTDOWN_FUNC)(OMRPortLibrary *);

#define J9_MEMCHECK_FREED_SIZE						(~(UDATA)0)

/* Mode flags. */
#define J9_MCMODE_PAD_BLOCKS						0x00000001
#define J9_MCMODE_FULL_SCANS						0x00000002
#define J9_MCMODE_NEVER_FREE						0x00000004
#define J9_MCMODE_FAIL_AT							0x00000008
#define J9_MCMODE_SKIP_TO							0x00000010
#define J9_MCMODE_TOP_DOWN							0x00000020
#define J9_MCMODE_SIMULATE							0x00000040
#define J9_MCMODE_PRINT_CALLSITES					0x00000080
#define J9_MCMODE_PRINT_CALLSITES_SMALL				0x00000100
#define J9_MCMODE_ZERO								0x00000200
#define J9_MCMODE_LIMIT								0x00000400
#define J9_MCMODE_IGNORE_UNKNOWN_BLOCKS				0x00000800
#define J9_MCMODE_SUB_ALLOCATOR						0x00001000
#define J9_MCMODE_MPROTECT							0x00002000
#define J9_MCMODE_NO_SCAN							0x00004000

#define J9_MEMCHECK_SHUTDOWN_NORMAL 0
#define J9_MEMCHECK_SHUTDOWN_EXIT 1

static UDATA j9thrDescriptor = 0;
static IDATA (*f_omrthread_attach_ex)(omrthread_t* handle, omrthread_attr_t *attr);
static void  (*f_omrthread_detach)(omrthread_t thread);

static MUTEX mcMutex;

static UtInterface *uteInterface = NULL;

/* Internal memorycheck portLibrary */
OMRPortLibrary memCheckPortLibStruct;		/* Stack allocated Struct */
OMRPortLibrary *memCheckPortLib;			/* Pointer to it */

/* Memory Protect related variables for working with vmem */
static J9HashTable *vmemIDTable = NULL;	/* For MEMORY_PROTECT */
static const UDATA lockMode = 0;
static const UDATA unlockMode = J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE /*| J9PORT_VMEM_MEMORY_MODE_EXECUTE*/;
static const UDATA allocateMode = J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
static UDATA J9_ALIGN_BOTTOM = 1;

/* Standard memCheck variables */
static J9MemoryCheckHeader *mostRecentBlock = NULL;
static J9MemoryCheckHeader *mostRecentFreedBlock = NULL;
static J9AVLTree *avl_tree;
static UDATA mode = 0;
static UDATA failAtAlloc = 0;
static UDATA skipToAlloc = 0;
static UDATA callSitePrintCount = 0;
static UDATA limitAlloc = 0;

static J9_MEM_ALLOCATE_FUNC globalAllocator = NULL;
static J9_MEM_FREE_FUNC globalDeallocator = NULL;
static J9_MEM_FREE_FUNC globalAdviseDeallocator = NULL;
static J9_MEM_ALLOCATE_FUNC globalAllocator32 = NULL;
static J9_MEM_FREE_FUNC globalDeallocator32 = NULL;

static J9_MEM_ALLOCATE_FUNC old_mem_allocate_memory = NULL;
static J9_MEM_FREE_FUNC old_mem_free_memory = NULL;
static J9_MEM_FREE_FUNC old_mem_advise_and_free_memory = NULL;
static J9_MEM_ALLOCATE_FUNC old_mem_allocate_memory32 = NULL;
static J9_MEM_FREE_FUNC old_mem_free_memory32 = NULL;
static J9_SHUTDOWN_FUNC old_port_shutdown_library = NULL;
static J9_SHUTDOWN_AND_EXIT_FUNC old_shutdown_and_exit = NULL;
static J9_PORT_CONTROL_FUNC old_port_control = NULL;
static J9_MEM_SHUTDOWN_FUNC old_mem_shutdown = NULL;

static J9MemoryCheckStats memStats;
static char hexd[] = "0123456789ABCDEF";

static UDATA memoryCheck_get_page_size(OMRPortLibrary *portLib);
static IDATA memoryCheck_lockGuardPages(OMRPortLibrary *portLib, void *memcheckHeader, UDATA size, UDATA mode );
static IDATA memoryCheck_lockWrappedBlock(OMRPortLibrary *portLib, J9MemoryCheckHeader *blockHeader, const UDATA mode);
#define J9_MEMCHECK_PAGE_SIZE		memoryCheck_get_page_size(memCheckPortLib)

/* MEMORY_PROTECT related macros */
#define J9_MEMCHECK_LOCK( ptr )\
		((J9MemoryCheckHeader *) ptr)->isLocked = 1; /* Set to Locked */\
		((memoryCheck_lockGuardPages( memCheckPortLib, (ptr), J9_MEMCHECK_PAGE_SIZE, lockMode ) == 0) ? \
			(void)0 : memCheckPortLib->tty_printf( memCheckPortLib, "LOCK FAIL: (%s)(%d)\n", __FILE__, __LINE__ )  )

#define J9_MEMCHECK_UNLOCK( ptr )\
		((memoryCheck_lockGuardPages( memCheckPortLib, (ptr), J9_MEMCHECK_PAGE_SIZE, unlockMode ) == 0) ? \
			(void)0 : memCheckPortLib->tty_printf( memCheckPortLib, "UNLOCK FAIL: (%s)(%d)\n", __FILE__, __LINE__ )  );\
		((J9MemoryCheckHeader *)(ptr))->isLocked = 0 /* Set to unlocked */ 

#define J9_MEMCHECK_LOCK_BODY( blockHeader )\
		memoryCheck_lockWrappedBlock( memCheckPortLib, (blockHeader), lockMode )

#define J9_MEMCHECK_UNLOCK_BODY( blockHeader )\
		memoryCheck_lockWrappedBlock( memCheckPortLib, (blockHeader), unlockMode )

/*
 * Following is for the suballocator 
 *
 * It is based loosely on a Donald Knuth algorithm from his Art of Computer Programming books
 * Some of the memory is allocated from a small array of small structures (2400 bytes in this
 * version, to keep the "small" * memory from fragmented the heap. 
 * The size and quantity of this may need to be adjusted depending on the application. 
 * All references to smallblock could be removed, however, and just the heap manager used to 
 * provide memory.
 * 
 *  If the MEMDEBUG macro is defined, additional printf's (and pool audits, etc.) are
 *  activated, helping to resolve issues. This will make the allocate run much slower, however.
*/

/* subAllocator heap size in bytes (Note: heap is initialized in 4 byte blocks) */
static UDATA heapSizeMegaBytes = 0;
#define DEFAULT_HEAP_SIZE_BYTES J9_MEMORY_MAX
static  IDATA*	j9heap;

/* 
 * The following variable is used for aligning mallocs happening in conjunction
 * with the subAllocator option. This keeps mallocs on double 
 * boundaries and forces bottom padding to end on a double word boundary - so 
 * that the next top padding will start on a double word boundary when using 
 * the subAllocator. Reference CMVC 119506, related to xscale.
 * 
 * "sizeof double" was chosen because some quick internet research indicated that 
 * double is the standard basis used for allocating memory.
 */
#define BYTES_FOR_ALIGNMENT(numberOfBytes) (U_32)((sizeof(double) - (((UDATA) numberOfBytes) & (sizeof(double) - 1))) & (sizeof(double) - 1))

#define STARTIDX	3

#define start   j9heap[STARTIDX-2]
#define heapsize	(j9heap[STARTIDX-3])
							 
/* 
 * smallest block (in words) allocated, 
 * plus level at which block is extended to 
 * absorb next block to prevent fragmentation
 */
#define FRAGTHRESHOLD   6

#define MAX_SMALL_BLOCK 50
struct smblk {
	UDATA data [ FRAGTHRESHOLD ];
} smblk;
static struct smblk smallBlock [MAX_SMALL_BLOCK];
static unsigned char smblkstatus[MAX_SMALL_BLOCK];
static UDATA smblkindex;							 
static UDATA meminuse;

/* setting ignoredCallsites to be 128 bytes, hoping we won't try to ignore too many callsites */
#define IGNORECALLSITESTRLENGTH 128
#define MAX_CALLSITE_COUNT 10
#define MAX_CALLSITE_LENGTH 32

static char ignoreCallSiteStr[IGNORECALLSITESTRLENGTH];
#define CALLSITE_DELIMITER ":"

#ifdef MEMDEBUG
static UDATA freemem;
static UDATA usedmem;
static UDATA total;
static void *pTop;
static void *pBot;
static UDATA  freeCnt;
static UDATA usedCnt;
static UDATA bucket[6];
/* audit masks: */
#define AUDIT   0x0001
#define FREE	0x0002
#define BUCKET  0x0004
#define USED	0x0008
#endif


static void memoryCheck_exit_shutdown_and_exit(OMRPortLibrary *portLib, I_32 exitCode );
static void memoryCheck_update_callSites_allocate(OMRPortLibrary *portLib, J9MemoryCheckHeader *header, const char *callSite, UDATA byteAmount);
static void *memoryCheck_wrapper_allocate_memory(OMRPortLibrary *portLib, UDATA byteAmount, char const *operationName, J9_MEM_ALLOCATE_FUNC allocator, U_32 paddingValue, U_32 fillValue, U_32 freedValue, const char *callSite, U_32 category);
static void memoryCheck_dump_callSite_small(OMRPortLibrary *portLibrary, J9AVLTreeNode *node);
static void memoryCheck_dump_bytes(OMRPortLibrary *portLib, void *dumpAddress, UDATA dumpSize);
static void memoryCheck_free_memory(OMRPortLibrary *portLib, void *memoryPointer);
static void memoryCheck_advise_and_free_memory(OMRPortLibrary *portLib, void *memoryPointer);
static void memoryCheck_free_memory32(OMRPortLibrary *portLib, void *memoryPointer);
static void memoryCheck_fill_bytes(OMRPortLibrary *portLib, U_8 *fillAddress, UDATA fillSize, U_32 fillValue, U_8 *blockAddress);
static void memoryCheck_update_callSites_free (J9MEMAVLTreeNode *node, UDATA byteAmount);
static void memoryCheck_null_mem_free_memory(OMRPortLibrary *portLib, void *memoryPointer);
static BOOLEAN memoryCheck_describe_freed_block(OMRPortLibrary *portLib, char const *operationName, J9MemoryCheckHeader *blockHeader);
static void OMRNORETURN memoryCheck_abort(OMRPortLibrary *portLib);
static UDATA memoryCheck_filter_nonVM_unFreed_Blcoks(OMRPortLibrary *portLib);
static void memoryCheck_print_summary(OMRPortLibrary *portLib, I_32 shutdownMode);
static void memoryCheck_shutdown_internal(OMRPortLibrary *portLib, I_32 shutdownMode);
static void memoryCheck_print_stats(OMRPortLibrary *portLib);
static void memoryCheck_dump_callSites_small(OMRPortLibrary *portLibrary, J9AVLTree *tree);
static I_32 memoryCheck_control(OMRPortLibrary *portLib, const char* key, UDATA value);
static BOOLEAN memoryCheck_scan_block(OMRPortLibrary *portLib, J9MemoryCheckHeader *blockHeader);
static void *memoryCheck_wrapper_reallocate_memory(OMRPortLibrary *portLib, void *memoryPointer, UDATA byteAmount, char const *operationName, J9_MEM_ALLOCATE_FUNC allocator, J9_MEM_FREE_FUNC deallocator, U_32 paddingValue, U_32 fillValue, U_32 freedValue, const char *callSite, U_32 category);
static UDATA memoryCheck_verify_backward(OMRPortLibrary *portLib, U_8 *fillAddress, UDATA fillSize, U_32 fillValue, U_8 *blockAddress);
static void memoryCheck_print_stats_callSite(OMRPortLibrary *portLib, J9MEMAVLTreeNode *node);
static void * memoryCheck_reallocate_memory (struct OMRPortLibrary *portLibrary, void *memoryPointer, UDATA byteAmount, const char *callsite, U_32 category);
static void memoryCheck_free_AVLTreeNode(OMRPortLibrary *portLib, J9AVLTreeNode *node);
static I_32 memoryCheck_port_shutdown_library(OMRPortLibrary *portLib);
static void *memoryCheck_allocate_memory(OMRPortLibrary *portLib, UDATA byteAmount, const char *callSite, U_32 category);
static void *memoryCheck_allocate_memory32(OMRPortLibrary *portLib, UDATA byteAmount, const char *callSite, U_32 category);
static void memoryCheck_dump_callSites(OMRPortLibrary *portLibrary, J9AVLTree *tree);
static void memoryCheck_dump_callSite(OMRPortLibrary *portLibrary, J9AVLTreeNode *node);
static IDATA memoryCheck_insertion_Compare(J9AVLTree *tree, J9AVLTreeNode *insertNode, J9AVLTreeNode *walk);
static UDATA memoryCheck_verify_forward(OMRPortLibrary *portLib, U_8 *fillAddress, UDATA fillSize, U_32 fillValue, U_8 *blockAddress);
static BOOLEAN memoryCheck_parseOption(OMRPortLibrary *portLib, char const *option, size_t optionLen);
static IDATA memoryCheck_search_Compare(J9AVLTree *tree, UDATA search, J9AVLTreeNode *walk);
static void memoryCheck_initialize_AVLTree_stats (J9MEMAVLTreeNode *node, UDATA byteAmount);
static void memoryCheck_print_stats_callSite_small(OMRPortLibrary *portLib, J9MEMAVLTreeNode *node);
static void memoryCheck_wrapper_free_memory(OMRPortLibrary *portLib, void *memoryPointer, char const *operationName, J9_MEM_FREE_FUNC deallocator, U_32 paddingValue, U_32 fillValue, U_32 freedValue);
static void memoryCheck_set_AVLTree_prevStats (J9MEMAVLTreeNode *node);
static BOOLEAN memoryCheck_describe_block(OMRPortLibrary *portLib, char const *operationName, J9MemoryCheckHeader *blockHeader);
static BOOLEAN memoryCheck_scan_all_blocks(OMRPortLibrary *portLib);
static void memoryCheck_free_AVLTree(OMRPortLibrary *portLib, J9AVLTree *tree);
static void subAllocator_init_heap(void* memptr, UDATA size);
static void* subAllocator_allocate_memory(OMRPortLibrary *portLib, UDATA size, const char *callSite, U_32 category);
static void subAllocator_free_memory(OMRPortLibrary *portLib, void *ptr);
static UDATA memoryCheck_hashFn (void *vmemId, void *userData);
static UDATA memoryCheck_hashEqualFn (void *leftEntry, void *rightEntry, void *userData);
static UDATA memoryCheck_hashDoFn (void *entry, void *portLib);

/* we shouldn't need this with a dedicated portlib for memCheck */
#if 0
static void memoryCheck_mem_shutdown(OMRPortLibrary *portLib);
#endif

static void memoryCheck_lockAllBlocks(OMRPortLibrary *portLib, J9MemoryCheckHeader *listHead, const UDATA lockFlags, const UDATA lockBody);
static void memoryCheck_restore_old_shutdown_functions(OMRPortLibrary *portLib);



#define J9_MEMCHECK_MINIMUM_HEADER_SIZE	(sizeof(J9MemoryCheckHeader) + 4*sizeof(U_32))
#define J9_MEMCHECK_ADJUSTED_PADDING \
	((((J9_MEMCHECK_PADDING_SIZE > J9_MEMCHECK_MINIMUM_HEADER_SIZE) ? \
	J9_MEMCHECK_PADDING_SIZE : J9_MEMCHECK_MINIMUM_HEADER_SIZE) + 7) & ~7)

IDATA 
memoryCheck_initialize(J9PortLibrary *j9portLibrary, char const *modeStr, char **argv)
{
#define DLL_NAME J9_THREAD_DLL_NAME
	OMRPortLibrary *portLib = OMRPORT_FROM_J9PORT(j9portLibrary);
	if (old_port_shutdown_library)  {
		/* We were already initialized! */
		return 1;
	}
	
	if (portLib->sl_open_shared_library(portLib, DLL_NAME, &j9thrDescriptor, J9PORT_SLOPEN_DECORATE)) {
		portLib->nls_printf(portLib, J9NLS_ERROR, J9NLS_MEMCHK_INIT_ERROR);
		return -1;
	}
	
	if (portLib->sl_lookup_name(portLib, j9thrDescriptor, "omrthread_attach_ex", (UDATA*)&f_omrthread_attach_ex, "LL")) {
		portLib->nls_printf(portLib, J9NLS_ERROR, J9NLS_MEMCHK_INIT_ERROR);
		return -1;
	}

	if (portLib->sl_lookup_name(portLib, j9thrDescriptor, "omrthread_detach", (UDATA*)&f_omrthread_detach, "L")) {
		portLib->nls_printf(portLib, J9NLS_ERROR, J9NLS_MEMCHK_INIT_ERROR);
		return -1;
	}
	
	if (!MUTEX_INIT(mcMutex)) {
		/* Print warning and fail. */
		portLib->nls_printf(portLib, J9NLS_ERROR, J9NLS_MEMCHK_INIT_ERROR);
		return -1;
	}

	mode = J9_MCMODE_FULL_SCANS | J9_MCMODE_PAD_BLOCKS;
	memset(ignoreCallSiteStr, '\0', IGNORECALLSITESTRLENGTH);
	while (modeStr && *modeStr)  {
		char const *q = strchr(modeStr, ',');

		if (!memoryCheck_parseOption(portLib, modeStr, (q ? q-modeStr : strlen(modeStr))))  {
			portLib->nls_printf(portLib, J9NLS_ERROR, J9NLS_MEMCHK_UNRECOGNIZED_OPTION, modeStr);
			MUTEX_DESTROY(mcMutex);
			return 2;
		}

		if (q) {
			q++;
		}

		modeStr = q;
	}

	/* check to see if we have legal options */
	/* noscan is only supported with callsite, callsitesmall, failat, and zero */
	if (0 != (mode & J9_MCMODE_NO_SCAN)) {
		/* disable full scan and pad blocks (which are set by default) */
		mode = mode & ~(J9_MCMODE_PAD_BLOCKS | J9_MCMODE_FULL_SCANS);
		/* check for other options */
		if (0 != (mode&(~J9_MCMODE_PRINT_CALLSITES_SMALL)&(~J9_MCMODE_PRINT_CALLSITES)&(~J9_MCMODE_ZERO)
					&(~J9_MCMODE_NO_SCAN)&(~J9_MCMODE_SUB_ALLOCATOR)&(~J9_MCMODE_FAIL_AT))) {
			/* TODO - this should be an NLS message */
			portLib->tty_err_printf(portLib, "-Xcheck:memory:noscan is only supported with 'callsitesmall', 'callsite', 'failat' and 'zero'. Calling exit(3)\n", mode);
			exit(3);		
		}
	}
	
	/* Set up a private portLibrary for the memoryCheck to use */
	memCheckPortLib = &memCheckPortLibStruct;
	if (0 != portLib->port_init_library(memCheckPortLib, sizeof(OMRPortLibrary))) {
		portLib->tty_printf(portLib, "Error creating the private portLibrary for memoryCheck.\n");
		return -1;
	}

	/* Set up the hashTable to hold the J9PortVmemIdentifiers */
	vmemIDTable = hashTableNew(	 memCheckPortLib, J9_GET_CALLSITE(), 9391, sizeof( J9PortVmemIdentifier *), 0, 0, OMRMEM_CATEGORY_VM, memoryCheck_hashFn, memoryCheck_hashEqualFn, NULL, NULL );
	if ( NULL == vmemIDTable ) {
		memCheckPortLib->tty_printf( memCheckPortLib, "Error creating vmemID hashTable.\n");
		return -1;
	}

	/* The hashtable is not allowed to grow or it will cause crashes due to NULL pointers */
	hashTableSetFlag( vmemIDTable, J9HASH_TABLE_DO_NOT_GROW );
	/* Save original versions of memory allocation and port shutdown functions. */
	old_mem_allocate_memory 				=	portLib->mem_allocate_memory;
	old_mem_free_memory 					=	portLib->mem_free_memory;
	old_mem_advise_and_free_memory 			=	portLib->mem_advise_and_free_memory;
	old_mem_allocate_memory32 				= 	portLib->mem_allocate_memory32;
	old_mem_free_memory32 					= 	portLib->mem_free_memory32;
	old_mem_shutdown						=	portLib->mem_shutdown;
	old_port_shutdown_library				=	portLib->port_shutdown_library;
	old_shutdown_and_exit					=	portLib->exit_shutdown_and_exit;
	old_port_control						=	portLib->port_control;

	/* allocate the subAllocator's heap, and initialize it */
	if (mode & J9_MCMODE_SUB_ALLOCATOR) {
		int i = 0;
		UDATA heapSizeBytes = heapSizeMegaBytes * 1024 * 1024;
		j9heap = memCheckPortLib->mem_allocate_memory(portLib, heapSizeBytes, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM);

		if (NULL == j9heap) {
			/* We failed our first attempt, try something smaller */
			/* TODO - we should output how the user can change this */
			portLib->tty_printf(portLib, "Initial allocation of subAllocator heap failed. Tried for %i MB\n", heapSizeMegaBytes);
	
			while (heapSizeBytes >= 1 * 1024 * 1024) {
				heapSizeBytes = heapSizeBytes/2;
				j9heap = memCheckPortLib->mem_allocate_memory(portLib, heapSizeBytes, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM);
				if (NULL != j9heap) {
					break;
				}
			}
	
			heapSizeMegaBytes = (heapSizeBytes) / (1024 *1024);
		}
		
		if (NULL == j9heap) {
			/* give up */
			portLib->tty_printf(portLib, "Unable to allocate subAllocator heap of size %i MB), calling exit(3)\n\n", heapSizeMegaBytes);
			exit(3);
		}
			
		portLib->tty_printf(portLib, "Successfully allocated subAllocator heap of size %i MB\n\n", heapSizeMegaBytes);
		subAllocator_init_heap(j9heap, heapSizeBytes/sizeof(UDATA)); /* NOTE: the j9heap is an array of UDATA sized blocks  */
		globalAllocator = subAllocator_allocate_memory;
		globalDeallocator = subAllocator_free_memory;
		globalAllocator32 	= memCheckPortLib->mem_allocate_memory32;
		globalDeallocator32 = memCheckPortLib->mem_free_memory32;
	} else {
		globalAllocator 	= memCheckPortLib->mem_allocate_memory;
		globalDeallocator 	= memCheckPortLib->mem_free_memory;
		globalAdviseDeallocator = memCheckPortLib->mem_advise_and_free_memory;
		globalAllocator32 	= memCheckPortLib->mem_allocate_memory32;
		globalDeallocator32 = memCheckPortLib->mem_free_memory32;
	}
	
	/* Shutdown the portLibrary and reinitialize it so that memorycheck is used from the beginning */
	j9portLibrary->port_shutdown_library(j9portLibrary);

	/* Substitute checking versions for the memory allocation  
	 * and port shutdown functions. */
	portLib->mem_allocate_memory 			=	memoryCheck_allocate_memory;
	portLib->mem_free_memory 				=	memoryCheck_free_memory;
	portLib->mem_advise_and_free_memory 	=	memoryCheck_advise_and_free_memory;

	/* MPROTECT mode test will time out if original allocate/free_memory32 is replaced */
	if ((mode & J9_MCMODE_MPROTECT) == 0) {
		portLib->mem_allocate_memory32 			=	memoryCheck_allocate_memory32;
		portLib->mem_free_memory32				=	memoryCheck_free_memory32;
	}

	portLib->mem_reallocate_memory 			=	memoryCheck_reallocate_memory;

/* TODO we can consider to remove this given the two separate port libraries */
# if 0	
	portLib->mem_shutdown					=	memoryCheck_mem_shutdown;
#endif
	
	portLib->port_control					=	memoryCheck_control;
	portLib->port_shutdown_library 			=	memoryCheck_port_shutdown_library;
	portLib->exit_shutdown_and_exit 		=	memoryCheck_exit_shutdown_and_exit;
	portLib->port_control 					=	memoryCheck_control;

	/* Restart the portLibrary */
	if(j9portLibrary->port_startup_library(j9portLibrary)) {
		/* If the port library should fail to (re)start, we're going to 
		 * crash trying to continue the normal VM shutdown sequence since 
		 * we have pre-maturely shutdown the portlibrary a few lines up.
		 * 
		 * Exit with 'magic' exit code to indicate this condition
		 */
		exit( 1 ); 	 
	}

	/* Create AVL Tree to store callSite information.  This information is useful for describing blocks
	 * even if callsite is not passed as a parameter to memcheck 
	 */
	avl_tree = old_mem_allocate_memory(memCheckPortLib, sizeof(J9AVLTree), J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM);
	if(avl_tree) {
		memset(avl_tree, 0, sizeof(J9AVLTree));
		avl_tree->insertionComparator = memoryCheck_insertion_Compare;
		avl_tree->searchComparator = memoryCheck_search_Compare;
		avl_tree->genericActionHook = NULL;
		avl_tree->rootNode = NULL;
	} else {
		memCheckPortLib->nls_printf(memCheckPortLib, J9NLS_ERROR, J9NLS_MEMCHK_AVL_ERROR);
	}

	/* Reset up NLS - this must be after the setup of the AVL tree */
	if ( argv != NULL ) {
		/* j9vm/jvm.c currently calls this function with a null argv, but it has also already setup
		 * and NLS catalog. */
		main_setNLSCatalog(j9portLibrary, argv);
	}

	return 0;
}

static BOOLEAN memoryCheck_parseOption(OMRPortLibrary *portLib, char const *option, size_t optionLen)
{
	char const *optStrAll = "all";
	size_t optLenAll = strlen(optStrAll);
	char const *optStrQuick = "quick";
	size_t optLenQuick = strlen(optStrQuick);
	char const *optStrNofree = "nofree";
	size_t optLenNofree = strlen(optStrNofree);
	char const *optStrFailat = "failat=";
	size_t optLenFailat = strlen(optStrFailat);
	char const *optStrSkipto = "skipto=";
	size_t optLenSkipto = strlen(optStrSkipto);
	char const *optTopDown = "topdown";
	size_t optLenTopDown = strlen(optTopDown);
	char const *optStrCallPrint = "callsite=";
	size_t optLenCallPrint = strlen(optStrCallPrint);
	char const *optStrCallPrintSmall = "callsitesmall=";
	size_t optLenCallPrintSmall = strlen(optStrCallPrintSmall);
	char const *optStrZero = "zero";
	size_t optLenZero = strlen(optStrZero);
	char const *optStrLimit = "limit=";
	size_t optLenLimit = strlen(optStrLimit);
	char const *optStrIgnoreUnknownBlocks = "ignoreUnknownBlocks";
	size_t optLenIgnoreUnknownBlocks = strlen(optStrIgnoreUnknownBlocks);
	char const *optStrSubAllocator = "subAllocator";
	size_t optLenSubAllocator = strlen(optStrSubAllocator);

	char const *optStrMProtect = "mprotect=";
	size_t optLenMProtect = strlen(optStrMProtect);
	char const *optStrAlignTop = "top";
	size_t optLenAlignTop = strlen(optStrAlignTop);
	char const *optStrAlignBottom = "bottom";
	size_t optLenAlignBottom = strlen(optStrAlignBottom);

	char const *optStrNoScan = "noscan";
	size_t optLenNoScan = strlen(optStrNoScan);

	/* undocumented. used to ignore unfreed blocks from specified callsites 
	 * the format is -Xcheck:memory:ignoreUnfreedCallsite=[site1]:[site2]:...
	 *
	 * Note, each site string is meant to be a prefix match to call site info.
	 * if string is 'zip/', it will ignore all unfreed blocks with callsite string with 'zip/'
	 * Conversely, if a call site is abc/de.c, specifying a string of de.c will not be a match */
	char const *optStrIgnoreUnfreedCallsite = "ignoreUnfreedCallsite=";
	size_t optLenIgnoreUnfreedCallsite = strlen(optStrIgnoreUnfreedCallsite);


#ifdef J9VM_MEMCHK_SIM_SUP
	char const *optStrSimulate = "simulate";
	size_t optLenSimulate = strlen(optStrSimulate);

	if ((optionLen == optLenSimulate) && !strncmp(option, optStrSimulate, optLenSimulate)) {
		mode |= J9_MCMODE_SIMULATE;
		return TRUE;
	}
#endif

	if ((optionLen > optLenIgnoreUnfreedCallsite) && !strncmp(option, optStrIgnoreUnfreedCallsite, optLenIgnoreUnfreedCallsite)) {
		if (strlen (&(option[optLenIgnoreUnfreedCallsite])) < IGNORECALLSITESTRLENGTH) {
			strcpy(ignoreCallSiteStr, &(option[optLenIgnoreUnfreedCallsite]));
		} else {
			portLib->tty_printf(portLib,"WARNING : IgnoreUnfreedCallsite option too long for internal buffer. Ignoring\n");
			
		}
		return TRUE;
	}

	if ((optionLen > optLenMProtect) && !strncmp(option, optStrMProtect, optLenMProtect)) {
		char temp[20];
		IDATA tempSize = optionLen - optLenMProtect;

		mode |= J9_MCMODE_PAD_BLOCKS;
		if (tempSize > 19) {
			tempSize = 19;
		}
		strncpy(temp, option + optLenMProtect, tempSize);
		temp[tempSize] = '\0';
		if (!strncmp(temp, optStrAlignTop, optLenAlignTop) ) {
			mode |= J9_MCMODE_MPROTECT;
			J9_ALIGN_BOTTOM = 0; 
 			return TRUE;
		}
		else if (!strncmp(temp, optStrAlignBottom, optLenAlignBottom)) {
			mode |= J9_MCMODE_MPROTECT;
			J9_ALIGN_BOTTOM = 1;
			return TRUE;
		}
		return FALSE;
	}

	if ((optionLen == optLenAll) && !strncmp(option, optStrAll, optLenAll)) {
		mode |= J9_MCMODE_PAD_BLOCKS | J9_MCMODE_FULL_SCANS;
		return TRUE;
	}
	if ((optionLen == optLenQuick) && !strncmp(option, optStrQuick, optLenQuick)) {
		mode |= J9_MCMODE_PAD_BLOCKS;
		mode &= ~J9_MCMODE_FULL_SCANS;
		return TRUE;
	}
	if ((optionLen == optLenNofree) && !strncmp(option, optStrNofree, optLenNofree)) {
		mode |= J9_MCMODE_NEVER_FREE;
		return TRUE;
	}
	if ((optionLen == optLenTopDown) && !strncmp(option, optTopDown, optLenTopDown)) {
		mode |= J9_MCMODE_TOP_DOWN;
		mode &= ~J9_MCMODE_SUB_ALLOCATOR;
		return TRUE;
	}
	if ((optionLen > optLenCallPrint) && !strncmp(option, optStrCallPrint, optLenCallPrint)) {
		char temp[20];
		IDATA tempSize = optionLen - optLenCallPrint;
		if (tempSize > 19)
			tempSize = 19;
		strncpy(temp, option + optLenCallPrint, tempSize);
		temp[tempSize] = '\0';
		if (!(callSitePrintCount = atoi(temp)))
			return FALSE;
		mode |= J9_MCMODE_PRINT_CALLSITES;
		/* remove the bits for J9_MCMODE_PRINT_CALLSITES_SMALL since J9_MCMODE_PRINT_CALLSITES was defined after */
		mode &= ~J9_MCMODE_PRINT_CALLSITES_SMALL;
		return TRUE;
	}
	if ((optionLen > optLenCallPrintSmall) && !strncmp(option, optStrCallPrintSmall, optLenCallPrintSmall)) {
		char temp[20];
		IDATA tempSize = optionLen - optLenCallPrintSmall;
		if (tempSize > 19)
			tempSize = 19;
		strncpy(temp, option + optLenCallPrintSmall, tempSize);
		temp[tempSize] = '\0';
		if (!(callSitePrintCount = atoi(temp)))
			return FALSE;
		mode |= J9_MCMODE_PRINT_CALLSITES_SMALL;
		/* remove the bits for J9_MCMODE_PRINT_CALLSITES since J9_MCMODE_PRINT_CALLSITES_SMALL was defined after */
		mode &= ~J9_MCMODE_PRINT_CALLSITES;
		return TRUE;
	}
	if ((optionLen == optLenZero) && !strncmp(option, optStrZero, optLenZero)) {
		mode |= J9_MCMODE_ZERO;
		return TRUE;
	}
	if ((optionLen > optLenSkipto) && (!strncmp(option, optStrSkipto, optLenSkipto))) {
		char temp[20];
		IDATA tempSize = optionLen - optLenSkipto;
		if (tempSize > 19)
			tempSize = 19;
		strncpy(temp, option + optLenSkipto, tempSize);
		temp[tempSize] = '\0';
		if (!(skipToAlloc = atoi(temp)))
			return FALSE;
		mode |= J9_MCMODE_SKIP_TO;
		return TRUE;
	}
	if ((optionLen > optLenFailat) && (!strncmp(option, optStrFailat, optLenFailat))) {
		char temp[20];
		IDATA tempSize = optionLen - optLenFailat;
		if (tempSize > 19)
			tempSize = 19;
		strncpy(temp, option + optLenFailat, tempSize);
		temp[tempSize] = '\0';
		if (!(failAtAlloc = atoi(temp)))
			return FALSE;
		mode |= J9_MCMODE_FAIL_AT;
		return TRUE;
	}
	if ((optionLen > optLenLimit) && (!strncmp(option, optStrLimit, optLenLimit))) {
		char temp[20];
		IDATA tempSize = optionLen - optLenLimit;
		if (tempSize > 19)
			tempSize = 19;
		strncpy(temp, option + optLenLimit, tempSize);
		temp[tempSize] = '\0';
		if (!(limitAlloc = atoi(temp)))
			return FALSE;
		mode |= J9_MCMODE_LIMIT;
		return TRUE;
	}
	if ((optionLen == optLenIgnoreUnknownBlocks) && !strncmp(option, optStrIgnoreUnknownBlocks, optLenIgnoreUnknownBlocks)) {
		mode |= J9_MCMODE_IGNORE_UNKNOWN_BLOCKS;
		return TRUE;
	}
	/* The heap sub allocator cannot be used with top down */
	if ((optionLen >= optLenSubAllocator) && !strncmp(option, optStrSubAllocator, optLenSubAllocator)) {
		char temp[20];
		IDATA tempSize = optionLen - optLenSubAllocator;
		if (tempSize > 19) {
			tempSize = 19;
		}
		strncpy(temp, option + optLenSubAllocator + 1, tempSize);
		temp[tempSize] = '\0';
		if (tempSize == 0) {
			heapSizeMegaBytes = DEFAULT_HEAP_SIZE_BYTES / (1024 * 1024);
		} else if (!(heapSizeMegaBytes = atoi(temp))) {
			return FALSE;
		}
		mode &= ~J9_MCMODE_TOP_DOWN;
		mode |= J9_MCMODE_SUB_ALLOCATOR;
		return TRUE;
	}
	
	if ((optionLen == optLenNoScan) && !strncmp(option, optStrNoScan, optLenNoScan)) {
		/* This is only supported with Callsite or Zero 
		 * We will check for other options in memoryCHeck initialize */
		mode |= J9_MCMODE_NO_SCAN;
		return TRUE;
	}
	
	return FALSE;
}


static void OMRNORETURN memoryCheck_abort(OMRPortLibrary *portLib)
{

	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Unlock ALL blocks */
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentBlock, unlockMode, 0 );
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentFreedBlock, unlockMode, 1 );
	}

	memoryCheck_print_stats(portLib);

	if ( mode & J9_MCMODE_MPROTECT ) {
		/* free everything from the hashTable*/
		/* TODO - why are we freeing anything if we just crashed */
		hashTableForEachDo( vmemIDTable, memoryCheck_hashDoFn, memCheckPortLib );
		hashTableFree( vmemIDTable );
	}

	memCheckPortLib->tty_printf( memCheckPortLib, "Memory error(s) discovered, calling exit(3)\n");
	memCheckPortLib->exit_shutdown_and_exit(memCheckPortLib, 3);

dontreturn: goto dontreturn; /* avoid warnings */
}


/* This function passes memoryCheck's private portLibrary memCheckPortLibrary on, NOT the portlib passed into it. */
static void *memoryCheck_allocate_memory(OMRPortLibrary *portLib, UDATA byteAmount, const char *callSite, U_32 category)
{

#ifdef DEBUG
	memCheckPortLib->tty_printf( memCheckPortLib, "allocate_memory(%d)\n", byteAmount);
#endif

	return memoryCheck_wrapper_allocate_memory(memCheckPortLib, byteAmount, "allocate_memory", globalAllocator,
		J9_MEMCHECK_DATA_PADDING_VALUE, J9_MEMCHECK_DATA_FILL_VALUE, J9_MEMCHECK_DATA_FREED_VALUE, callSite, category);
}

static void *memoryCheck_allocate_memory32(OMRPortLibrary *portLib, UDATA byteAmount, const char *callSite, U_32 category)
{

#ifdef DEBUG
	memCheckPortLib->tty_printf( memCheckPortLib, "allocate_memory32(%d)\n", byteAmount);
#endif

	return memoryCheck_wrapper_allocate_memory(memCheckPortLib, byteAmount, "allocate_memory", globalAllocator32,
		J9_MEMCHECK_DATA_PADDING_VALUE, J9_MEMCHECK_DATA_FILL_VALUE, J9_MEMCHECK_DATA_FREED_VALUE, callSite, category);
}


/* This method prints whatever information as it can gather about a block from the block and its immediate neighbors (if any).
	If the block looks like a correct block, TRUE is returned.  If anything bogus is discovered about the block, FALSE is returned. */
/* @precondition: If #defined MEMORY_PROTECT, this function must be passed UNLOCK'ed blocks */
/* @internal 	The memCheckPortLib is the one that should always be passed to this function */

static BOOLEAN memoryCheck_describe_block(OMRPortLibrary *portLib, char const *operationName,
										  J9MemoryCheckHeader *blockHeader)
{
	BOOLEAN everythingOkay = TRUE;
	UDATA topPaddingSize =  0;
	UDATA bottomPaddingSize = 0;
	U_32 paddingValue;
	UDATA topPaddingSmashed, bottomPaddingSmashed;

	U_8 *topPadding = ((U_8 *) blockHeader) + sizeof(J9MemoryCheckHeader);
	U_8 *wrappedBlock = NULL;
	U_8 *bottomPadding = NULL;
	U_8 *dumpPos = topPadding;

	if ( !(mode & J9_MCMODE_MPROTECT) ) {
		topPaddingSize =  J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);

		/* Adjust bottomPadding to force it to end on a double boundary */
		bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
		wrappedBlock = ((U_8 *) blockHeader) + J9_MEMCHECK_ADJUSTED_PADDING;
	} else {
		wrappedBlock = blockHeader->wrappedBlock;
		if (J9_ALIGN_BOTTOM) {
			topPaddingSize = wrappedBlock - (((U_8 *)blockHeader) + sizeof(J9MemoryCheckHeader)); 
		} else { 
			topPaddingSize = J9_MEMCHECK_PAGE_SIZE - sizeof(J9MemoryCheckHeader);
		}

		/* Adjust bottomPadding to force it to end on a double boundary */
		bottomPaddingSize = J9_MEMCHECK_PAGE_SIZE + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize); 
	}
	
	/* TODO - this doesn't take into account the offset area */
	bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;

	portLib->tty_printf(portLib, "%s describing block at %p (header at %p):\n", operationName, wrappedBlock,
						blockHeader);

	if ( mode & J9_MCMODE_MPROTECT ) {
		if ( J9_ALIGN_BOTTOM ) {
			U_32 offsetArea =  BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);	
			wrappedBlock = blockHeader->bottomPage - blockHeader->wrappedBlockSize - offsetArea;
			bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;
		}
	}

	/* Make a guess at what the block should look like by peeking right after the header.. */

	if (!memoryCheck_verify_forward(portLib, topPadding, 8, J9_MEMCHECK_DATA_PADDING_VALUE, wrappedBlock)) {
		paddingValue = J9_MEMCHECK_DATA_PADDING_VALUE;
	} else if (!memoryCheck_verify_forward(portLib, topPadding, 8, J9_MEMCHECK_CODE_PADDING_VALUE, wrappedBlock)) {
		paddingValue = J9_MEMCHECK_CODE_PADDING_VALUE;
	} else {
		/* TODO: check for legal padding with wrong wrappedBlock bits here? */

		/* Test for alignment off by 4 */
		if ( (UDATA)topPadding%8 == 4 ) {
			if (!memoryCheck_verify_forward(portLib, topPadding+4, 8, J9_MEMCHECK_DATA_PADDING_VALUE, wrappedBlock)) {
				paddingValue = J9_MEMCHECK_DATA_PADDING_VALUE;
			} else if (!memoryCheck_verify_forward(portLib, topPadding +4, 8, J9_MEMCHECK_CODE_PADDING_VALUE, wrappedBlock)) {
				paddingValue = J9_MEMCHECK_CODE_PADDING_VALUE;
			}

			portLib->tty_printf(portLib, "Block has unrecognized padding %08x %08x (header is probably trashed)!\n",
							((U_32 *) topPadding)[0], ((U_32 *) topPadding)[1]);
			everythingOkay = FALSE;
			goto dump_header_only;
		} else {
			portLib->tty_printf(portLib, "Block has unrecognized padding %08x %08x (header is probably trashed)!\n",
								((U_32 *) topPadding)[0], ((U_32 *) topPadding)[1]);
			everythingOkay = FALSE;
			goto dump_header_only;
		}
	}

	topPaddingSmashed = memoryCheck_verify_forward(portLib, topPadding, topPaddingSize, paddingValue, wrappedBlock);
	if (topPaddingSmashed) {
		portLib->tty_printf(portLib, "Last %d bytes of top padding are damaged\n", topPaddingSmashed);
		everythingOkay = FALSE;
	}

	/* Do additional checks to see if header is sane.  If not, we won't trust its length field */
	/* and we won't worry about the bottom padding. */

	if ( mode & J9_MCMODE_MPROTECT ) {
		const UDATA pageSize = J9_MEMCHECK_PAGE_SIZE;
		UDATA checkSize = pageSize;

		if (J9_ALIGN_BOTTOM) {
			
			/* Size to check on the bottom is the bottomPage and the alignment bytes. */
			/* Possible enhancement: don't check the bottom page as it can't be touched anyway */
			checkSize += BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
		}
		else {
			UDATA addMod = blockHeader->wrappedBlockSize % pageSize;
				
			if ( addMod ) {
				checkSize += pageSize - (blockHeader->wrappedBlockSize % pageSize);
			}
		}
		bottomPaddingSmashed = memoryCheck_verify_backward(portLib, bottomPadding, checkSize , paddingValue, wrappedBlock);
	} else {
		bottomPaddingSmashed = memoryCheck_verify_backward(portLib, bottomPadding, bottomPaddingSize, paddingValue, wrappedBlock);
	}

	if (bottomPaddingSmashed) {
		portLib->tty_printf(portLib, "First %d bytes of bottom padding are damaged\n", bottomPaddingSmashed);
		everythingOkay = FALSE;
	}

	portLib->tty_printf(portLib, "Wrapped block size is %d, allocation number is %d\n",
						blockHeader->wrappedBlockSize, blockHeader->allocationNumber);

	if (blockHeader->node) {
		portLib->tty_printf(portLib, "Block was allocated by %s\n", blockHeader->node->callSite);
	}

	if (everythingOkay) {
		UDATA size = blockHeader->wrappedBlockSize < 32 ? blockHeader->wrappedBlockSize : 32;
		portLib->tty_printf(portLib, "First %d bytes:\n", size);
		memoryCheck_dump_bytes(portLib, wrappedBlock, size);

		return TRUE;
	}

	if ( mode & J9_MCMODE_MPROTECT ) {
		portLib->tty_printf(portLib, "Extra Top Padding:\n");
		memoryCheck_dump_bytes(portLib, blockHeader->self, blockHeader->topPage - blockHeader->self);
	}

	portLib->tty_printf(portLib, "Block header:\n");
	memoryCheck_dump_bytes(portLib, blockHeader, sizeof(*blockHeader));
	portLib->tty_printf(portLib, "Top padding:\n");
	memoryCheck_dump_bytes(portLib, topPadding, topPaddingSize);
	portLib->tty_printf(portLib, "Block contents:\n");
	memoryCheck_dump_bytes(portLib, wrappedBlock, blockHeader->wrappedBlockSize);
	portLib->tty_printf(portLib, "Bottom padding:\n");
	if ( mode & J9_MCMODE_MPROTECT ) {
		memoryCheck_dump_bytes(portLib, bottomPadding, blockHeader->totalAllocation - (bottomPadding - blockHeader->self));
	} else {
		memoryCheck_dump_bytes(portLib, bottomPadding, bottomPaddingSize);
	}
	return everythingOkay;

  dump_header_only:
	portLib->tty_printf(portLib, "(only top padding + first 64 bytes of user data will be printed here)\n");
	if ( mode & J9_MCMODE_MPROTECT ) {
		portLib->tty_printf(portLib, "Extra Top Padding:\n");
		memoryCheck_dump_bytes(portLib, blockHeader->self, blockHeader->topPage - blockHeader->self);
	}
	portLib->tty_printf(portLib, "Block header:\n");
	memoryCheck_dump_bytes(portLib, blockHeader, sizeof(*blockHeader));
	portLib->tty_printf(portLib, "Top padding:\n");
	memoryCheck_dump_bytes(portLib, topPadding, topPaddingSize);
	portLib->tty_printf(portLib, "First 64 bytes at block contents:\n");
	memoryCheck_dump_bytes(portLib, wrappedBlock, 64);
	return everythingOkay;
}



/* This method fills fillSize bytes at fillAddress with copies of a 64-bit value formed by splitting fillValue into 2 16-bit values and inserting
	the low 32 bits of the block address in between.  Each instance of fillValue is aligned on an 8-byte boundary, so if fillAddress and/or
	fillAddress+fillSize is not 8-byte aligned, a partial copy will appear at the start and/or end. */

static void memoryCheck_fill_bytes(OMRPortLibrary *portLib, U_8 *fillAddress, UDATA fillSize, U_32 fillValue, U_8 *blockAddress)
{
	U_8 fillWith[8];
	I_32 blockAddressInt = (I_32)(IDATA)blockAddress;

	U_8 *c;
	UDATA index = ((UDATA)fillAddress) & 7;

	memcpy(fillWith, &fillValue, 2);
	memcpy(fillWith+2, &blockAddressInt, 4);
	memcpy(fillWith+6, ((U_8 *)(&fillValue)) + 2, 2);

	for (c = fillAddress; c < fillAddress+fillSize; c++)  {
		*c = fillWith[index];
		index++;
		index &= 7;
	}
}



/* @internal	The portLibrary passed on to other functions  must be the memCheckPortLib. */
static void 
memoryCheck_free_memory(OMRPortLibrary *portLib, void *memoryPointer)
{
#ifdef DEBUG
	memCheckPortLib->tty_printf( memCheckPortLib, "free_memory(%p)\n", memoryPointer);
#endif

	memoryCheck_wrapper_free_memory( memCheckPortLib, memoryPointer, "free_memory", globalDeallocator,
		J9_MEMCHECK_DATA_PADDING_VALUE, J9_MEMCHECK_DATA_FILL_VALUE, J9_MEMCHECK_DATA_FREED_VALUE);
}

static void
memoryCheck_advise_and_free_memory(OMRPortLibrary *portLib, void *memoryPointer)
{
#ifdef DEBUG
	memCheckPortLib->tty_printf( memCheckPortLib, "advise_and_free_memory(%p)\n", memoryPointer);
#endif

	memoryCheck_wrapper_free_memory( memCheckPortLib, memoryPointer, "advise_and_free_memory", globalAdviseDeallocator,
		J9_MEMCHECK_DATA_PADDING_VALUE, J9_MEMCHECK_DATA_FILL_VALUE, J9_MEMCHECK_DATA_FREED_VALUE);
}

static void
memoryCheck_free_memory32(OMRPortLibrary *portLib, void *memoryPointer)
{
#ifdef DEBUG
	memCheckPortLib->tty_printf( memCheckPortLib, "free_memory(%p)\n", memoryPointer);
#endif

	memoryCheck_wrapper_free_memory( memCheckPortLib, memoryPointer, "free_memory", globalDeallocator32,
		J9_MEMCHECK_DATA_PADDING_VALUE, J9_MEMCHECK_DATA_FILL_VALUE, J9_MEMCHECK_DATA_FREED_VALUE);
}

/* Designed to override the original port library shut down routine
 * 
 * Shuts down the passed in portLibrary (which should be the user's)
 * using the original shut down routines, then shut's down memcheck */
static I_32 
memoryCheck_port_shutdown_library(OMRPortLibrary *portLib)
{
	I_32 rc = 0;
	
	memoryCheck_restore_old_shutdown_functions(portLib);
	
	rc = portLib->port_shutdown_library(portLib);
	
	memoryCheck_shutdown_internal( memCheckPortLib, J9_MEMCHECK_SHUTDOWN_NORMAL );

	return rc;
}

/**
 * Returns the page size to be used in the memorycheck calls. 
 * The page size must always be large enough to hold the J9MemoryCheckHeader struct
 * as well as some scratch padding.  If the default pagesize is less then this size, 
 * then a multiple of the page size will be returned which is at least 
 * J9_MEMCHECK_ADJUSTED_PADDING in size.
 */
static UDATA memoryCheck_get_page_size(OMRPortLibrary *portLib)
{
	UDATA pageSize;
	 
 	pageSize = (portLib->vmem_supported_page_sizes( portLib )) [0];
 	if ( pageSize < J9_MEMCHECK_ADJUSTED_PADDING ) {
		UDATA systemPageSize = 	portLib->vmem_supported_page_sizes( portLib )[0];

 		pageSize = (J9_MEMCHECK_ADJUSTED_PADDING / systemPageSize)*systemPageSize + 
				(J9_MEMCHECK_ADJUSTED_PADDING % systemPageSize ? systemPageSize : 0 );
 	}
 	return pageSize;
}

/**
 * Used by the hashTableForEachDo iterator to call
 * omrvmem_free_memory() for each reserved section of virtual memory
 * and to free the J9PortVmemIdentifier struct.
 *
 * @param[in] entry	The hashTable node.
 * @param[in] userData	A pointer to an initialized OMRPortLibrary; The "memCheckPortLibrary" must be the one passed in.
 *					This is the portLibrary with its original functions
 * @return Always returns TRUE so that the nodes in the hash table will be deallocated.
 */
static UDATA 
memoryCheck_hashDoFn( void *entry, void *portLib )
{
	if ( entry && portLib ) {
		J9PortVmemIdentifier **vmemID = (J9PortVmemIdentifier **)entry;
		OMRPORT_ACCESS_FROM_OMRPORT(portLib);

		/* Ensure the node was allocated by us, as every node will have an address which is a multiple of J9_MEMCHECK_PAGE_SIZE */
		/* TODO - consider removing if no one else could have allocated it?? */
		if ( *vmemID && (UDATA)(*vmemID)->address % J9_MEMCHECK_PAGE_SIZE == 0 ) {
			/* Release the virtual mem and free the struct holding the vmemIdentifier */
			omrvmem_free_memory((*vmemID)->address, (*vmemID)->size, *vmemID);
			omrmem_free_memory(*vmemID);
			*vmemID = NULL;
		}
	}
	return TRUE;
}
/**
 * This is the function used to determine if two hashed items are the same item or not.  This is 
 * done by determining they have the same address in the J9PortVmemIdentifier structs's address 
 * field.  As Each address in the address space should only be mapped into the table once for 
 * each allocation, this should always be a unique way to determine if two items are equal.
 * 
 * @param[in] leftEntry	A valid J9PortVmemIdentifier
 * @param[in] rightEntry	A valid J9PortVmemIdentifier
 * @param[in] userData	An ignored parameter required by the format used by the hashTable fcn pointer.
 * @return A UDATA with 1 if equal, 0 if not.
 */
static UDATA 
memoryCheck_hashEqualFn(void *leftEntry, void *rightEntry, void *userData)
{
	if ( leftEntry && rightEntry && *(J9PortVmemIdentifier **)leftEntry && *(J9PortVmemIdentifier **)rightEntry ) {
		return ( *(J9PortVmemIdentifier **) leftEntry)->address == ( *(J9PortVmemIdentifier **) rightEntry)->address;
	} else {
		return 0;
	}
}

/**
 * This is the hash function to be used by the hashTable.  The magic constant 2654435761 is the 
 * golden ratio of 2^32 and the shift by 3 is because memorycheck assumes 8 byte alignment.
 * 
 * @param[in] vmemId	A valid J9PortVmemIdentifier struct pointer to reserved and committed memory.
 * @param[in] userData	An ignored parameter required by the format used by the hashTable fcn pointer.
 * @return The hashkey that will return the pointer to the J9PortVmemIdentifier struct for later use.
 */
static UDATA 
memoryCheck_hashFn(void *vmemId, void *userData)
{
	UDATA key;
	key = (UDATA) (*(J9PortVmemIdentifier **)vmemId)->address;
	return (key >> 3) * 0x9E3779B1;
}

/**
 * This function unlocks all of the blocks on a given list.
 *
 * @param[in] portLib	An initialized OMRPortLibrary with its original functions
 * @param[in] listHead	The block to start unlocking the list from. 
 * @param[in] lockFlags	The LockMode: lockMode to lock, unlockMode to unlock
 * @param[in] lockBody	Whether the body of the block needs to be unlocked. It should
 * 		only be set to true for the mostRecentFreedBlock list.
 */
static void 
memoryCheck_lockAllBlocks(OMRPortLibrary *portLib, J9MemoryCheckHeader *listHead, const UDATA lockFlags, const UDATA lockBody)
{
	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Unlock ALL the blocks */
		if ( listHead ) {
			J9MemoryCheckHeader *blockHeader = listHead;
			while ( blockHeader != NULL ) {
				if ( lockFlags == lockMode ) {
					if ( blockHeader->nextBlock ) {
						if ( lockBody ) {
							J9_MEMCHECK_LOCK_BODY( blockHeader ->nextBlock );
						}
						J9_MEMCHECK_LOCK( blockHeader->nextBlock );
					}
					if ( !blockHeader->previousBlock ) {
						if ( lockBody ) {
							J9_MEMCHECK_LOCK_BODY( blockHeader );
						}
						J9_MEMCHECK_LOCK( blockHeader );
						break;
					}		
				} else if ( lockFlags == unlockMode ) {
					J9_MEMCHECK_UNLOCK( blockHeader );
					if ( lockBody ) {
						J9_MEMCHECK_UNLOCK_BODY( blockHeader );
					}
				}
				blockHeader = blockHeader->previousBlock;
			}
			blockHeader = NULL;
		}
	}
	return;
}

/**
 * This function is used to lock and unlock the guardpage above and below the wrappedBlock.  
 * It is usually called through one of two macros: UNLOCK( portLib, header ) or LOCK( portLib, header ).
 *
 * @precondition The global variable "J9HashTable vmemIDTable" must be already initialized.
 * @param[in] portLib	An initialized OMRPortLibrary structure with its original functions
 * @param[in] memcheckHeader	An initialized J9MemoryCheckheader.
 * @param[in] requestedMode	The flags to apply to the guardPages.  This should be either lockMode or unlockMode;
 * @return 0 on Success, -1 on failure
 */
static IDATA 
memoryCheck_lockGuardPages(OMRPortLibrary *portLib, void *memcheckHeader, UDATA size, UDATA requestedMode)
{
	if ( mode & J9_MCMODE_MPROTECT ) {
		void *ret = NULL; 
		U_8 *topPageBoundary = NULL;
		J9PortVmemIdentifier getFromTable;
		J9PortVmemIdentifier *getFromTablePtr;
		J9PortVmemIdentifier **block = NULL;
		J9MemoryCheckHeader *head = (J9MemoryCheckHeader *)memcheckHeader;
		UDATA oldMode;
		OMRPORT_ACCESS_FROM_OMRPORT(portLib);

		if ( !J9_ALIGN_BOTTOM ) {
			topPageBoundary = (U_8 *)memcheckHeader;
		}
		else {
#if defined(J9ZOS390)
			topPageBoundary = (U_8 *)head->self;
#else
			topPageBoundary = (U_8 *)memcheckHeader;	/* Pointer to the J9MemoryCheckHeader */
			topPageBoundary = topPageBoundary - (UDATA)topPageBoundary % J9_MEMCHECK_PAGE_SIZE;	/* Align it to a page Boundary */
#endif
		}

		/* Get the correct struct from the hashTable */
		getFromTable.address = topPageBoundary;
		getFromTablePtr = &getFromTable;
		block = hashTableFind( vmemIDTable, &getFromTablePtr );
		if ( block ) {
			getFromTablePtr = *block;
			oldMode = getFromTablePtr->mode;

			/* Update it so it has the correct access mode and recommit the mem to allow reading */
			getFromTablePtr->mode = unlockMode;
			ret = omrvmem_commit_memory( getFromTablePtr->address, getFromTablePtr->pageSize,  getFromTablePtr );
			if ( NULL == ret ) {
				getFromTablePtr->mode = oldMode;
				return -1;
			}

			/* Apply user passed in mode to the topPage and bottom page */		
			getFromTablePtr->mode = requestedMode;
			ret = omrvmem_commit_memory( head->bottomPage, getFromTablePtr->pageSize,  getFromTablePtr );
			if ( NULL == ret ) {
				getFromTablePtr->mode = oldMode;
				return -1;
			}

			ret = omrvmem_commit_memory( topPageBoundary, getFromTablePtr->pageSize,  getFromTablePtr );
			if ( NULL == ret ) {
				getFromTablePtr->mode = oldMode;
				return -1;
			}
			getFromTablePtr->mode = oldMode;	

			return 0;
		}
		return -1;
	}
	return 0;
}

/**
 * This function is used to lock and unlock the wrappedBlock.  It is usually called through
 * one of two macros: UNLOCK_BODY( portLib, header ) or LOCK_BODY( portLib, header ).
 *
 * @precondition	The blockHeader must be unlocked before calling this function 
 * @param[in] portLib	An initialized OMRPortLibrary structure with its original functions
 * @param[in] blockHeader	An initialized blockHeader which already has its guard pages unlocked.
 * @param[in] requestedMode	The flags to apply to the wrappedBlock.  This should be either lockMode or unlockMode;
 * @return 0 on Success, -1 on failure
 */
static IDATA 
memoryCheck_lockWrappedBlock(OMRPortLibrary *portLib, J9MemoryCheckHeader *blockHeader, const UDATA requestedMode)
{
	if ( mode & J9_MCMODE_MPROTECT ) {
		void *ret = NULL; 
		U_8 *topPageBoundary = NULL;
		J9PortVmemIdentifier getFromTable;
		J9PortVmemIdentifier *getFromTablePtr = NULL;
		J9PortVmemIdentifier **block = NULL;
		J9MemoryCheckHeader *head = (J9MemoryCheckHeader *)blockHeader;
		UDATA userPages = 0;
		UDATA oldMode;
		OMRPORT_ACCESS_FROM_OMRPORT(portLib);

		if ( !J9_ALIGN_BOTTOM ) {
			topPageBoundary = (U_8 *)blockHeader;
		}
		else {
			topPageBoundary = (U_8 *)blockHeader;	/* Pointer to the J9MemoryCheckHeader */
			topPageBoundary = topPageBoundary - (UDATA)topPageBoundary % J9_MEMCHECK_PAGE_SIZE;	/* Align it to a page Boundary */
		}

		/* Get the correct struct from the hashTable */
		getFromTable.address = topPageBoundary;
		getFromTablePtr = &getFromTable;
		block = hashTableFind( vmemIDTable, &getFromTablePtr );
		getFromTablePtr = *block;

		/* Determine the number of pages that are used to hold the 'body' or user-allocated mem */
		userPages = blockHeader->wrappedBlockSize/J9_MEMCHECK_PAGE_SIZE + ( blockHeader->wrappedBlockSize % J9_MEMCHECK_PAGE_SIZE ? 1 : 0 );	

		/* Update and commit struct to lock the userPages */
		oldMode = getFromTablePtr->mode;
		getFromTablePtr->mode = requestedMode;
		ret = omrvmem_commit_memory( topPageBoundary + getFromTablePtr->pageSize, userPages * getFromTablePtr->pageSize, getFromTablePtr );
		getFromTablePtr->mode = oldMode;
		if ( NULL == ret ) {
			return -1;
		}
	}
	return 0;
}


/* This method scans the entire list of allocated blocks to make sure none of the padding has been scratched.
	We do this on every allocate and every free to make sure we notice trashing as early as possible.  If a
	screwed up block is detected, details are printed to the console.  If possible, the scan is continued to print
	out other screwed up blocks.  Returns FALSE if any blocks are screwed up, or TRUE if everything looks okay.  */
/* @internal The passed in portLib should be the memCheckPortLib with its original functions */

static BOOLEAN memoryCheck_scan_all_blocks(OMRPortLibrary *portLib)
{
	J9MemoryCheckHeader *blockHeader, *prevBlockHeader;
	BOOLEAN everythingOkay = TRUE;
	char const *operationName = "scan_all_blocks";
	UDATA topPaddingSize = 0;
	UDATA bottomPaddingSize;

	if ( !(mode & J9_MCMODE_MPROTECT) ) {
		/* topPaddingSize is constant with all blocks if MPROTECT is not enabled */
		topPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);
	}

	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Unlock ALL blocks */
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentBlock, unlockMode, 0 );
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentFreedBlock, unlockMode, 1 );
	}

	for (prevBlockHeader = NULL, blockHeader = mostRecentBlock;
		 blockHeader != NULL; prevBlockHeader = blockHeader, blockHeader = blockHeader->previousBlock) {

		U_8 *topPadding = ((U_8 *) blockHeader) + sizeof(J9MemoryCheckHeader);
		U_8 *wrappedBlock;
		U_8 *bottomPadding;

		if (mode & J9_MCMODE_MPROTECT) {
			topPaddingSize = blockHeader->wrappedBlock - topPadding;
			wrappedBlock = blockHeader->wrappedBlock;
		} else {
			/* topPaddingSize was already calculated above */
			wrappedBlock = topPadding + topPaddingSize;
		}
		
		/* Verify top padding */
		if ( (mode & J9_MCMODE_MPROTECT) && !J9_ALIGN_BOTTOM ) {
			/* all the top padding is write protected, no point in scanning */
		} else {
			if (memoryCheck_verify_forward
				(portLib, topPadding, topPaddingSize, J9_MEMCHECK_DATA_PADDING_VALUE, wrappedBlock)) {
				if (memoryCheck_verify_forward
					(portLib, topPadding, topPaddingSize, J9_MEMCHECK_CODE_PADDING_VALUE, wrappedBlock)) {
	
					if (blockHeader->wrappedBlockSize == J9_MEMCHECK_FREED_SIZE) {
						portLib->tty_printf(portLib,
											"ERROR: Found an already freed block %p (header at %p) on the allocated list (should never happen)!\n",
											wrappedBlock, blockHeader);
						goto describe_block_and_fail;
					}
	
					/* Block not freed yet, but top padding at least is damaged.  If the nextBlock pointer is damaged too
					   then we assume the header is useless and we stop scanning (to reduce the chances of a GPF). */
	
					if (blockHeader->nextBlock != prevBlockHeader) {
						portLib->tty_printf(portLib,
											"ERROR: Header of block at %p (header at %p) is damaged, scan stopped.\n",
											wrappedBlock, blockHeader);
						goto describe_block_and_fail;
					}
	
					everythingOkay = FALSE;
					goto describe_block_and_continue;
				}
			}
		}

		if (blockHeader->allocationNumber > memStats.totalBlocksAllocated) {
			portLib->tty_printf(portLib, "ERROR: Invalid allocation number %d in block %p (header at %p)\n",
								blockHeader->allocationNumber, wrappedBlock, blockHeader);
			goto describe_block_and_fail;	/* If header is screwed then we risk a GPF by continuing.. */
		}

		if (prevBlockHeader && (blockHeader->allocationNumber >= prevBlockHeader->allocationNumber)) {
			portLib->tty_printf(portLib, "ERROR: Allocation number %d in block %p (header at %p) should be lower than %d in block %p (header at %p)\n",
								blockHeader->allocationNumber, wrappedBlock, blockHeader,
								prevBlockHeader->allocationNumber, ((U_8 *) prevBlockHeader) + J9_MEMCHECK_ADJUSTED_PADDING, prevBlockHeader);
			goto describe_block_and_fail;	/* If header is screwed then we risk a GPF by continuing.. */
		}

		if (blockHeader->nextBlock != prevBlockHeader) {
			portLib->tty_printf(portLib,
								"ERROR: Allocated list links between %p (header at %p) and %p (header at %p) are corrupt\n",
								((U_8 *) prevBlockHeader) + J9_MEMCHECK_ADJUSTED_PADDING, prevBlockHeader, wrappedBlock,
								blockHeader);
		}

		bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;

		if ( mode & J9_MCMODE_MPROTECT ) {

			/* Adjust bottomPadding to force it to end on a double boundary */
			bottomPaddingSize = blockHeader->totalAllocation - J9_MEMCHECK_PAGE_SIZE - (bottomPadding - blockHeader->topPage);
		} else {

			/* Adjust bottomPadding to force it to end on a double boundary */
			bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
		}

		if (memoryCheck_verify_forward
			(portLib, bottomPadding, bottomPaddingSize, J9_MEMCHECK_DATA_PADDING_VALUE, wrappedBlock)) {
			if (memoryCheck_verify_forward
				(portLib, bottomPadding, bottomPaddingSize, J9_MEMCHECK_CODE_PADDING_VALUE, wrappedBlock)) {

				/* Bottom padding is damaged. */
				goto describe_block_and_continue;

			}
		}

		/* This block looked okay, so go back up to top of loop */
		continue;

	  /* 
	   * Comes here when bottom padding is damaged. But, we still
	   * want to continue with other blocks.
	   * TODO: Consider refactor code to avoid gotos
	   * TODO: Why is return value from memoryCheck_describe_block() ignored?
	   */
	  describe_block_and_continue:
		everythingOkay = FALSE;
		memoryCheck_describe_block(portLib, operationName, blockHeader);
	}

	if (mode & J9_MCMODE_NEVER_FREE) {
		/* Scan all previously freed blocks too */
		for (prevBlockHeader = NULL, blockHeader = mostRecentFreedBlock;
			 blockHeader != NULL; prevBlockHeader = blockHeader, blockHeader = blockHeader->previousBlock) {

			U_8 *topPadding = ((U_8 *) blockHeader) + sizeof(J9MemoryCheckHeader);
			U_8 *bottomPadding, *wrappedBlock;
			UDATA bottomSize; 	/* this includes the wrappedblock plus the bottom padding */
	
			if (mode & J9_MCMODE_MPROTECT) {
				topPaddingSize = blockHeader->wrappedBlock - topPadding;
				wrappedBlock = blockHeader->wrappedBlock;
			} else {
				/* topPaddingSize was already calculated above */
				wrappedBlock = topPadding + topPaddingSize;
			}

			if ( (mode & J9_MCMODE_MPROTECT) && !J9_ALIGN_BOTTOM ) {
				/* all the top padding is write protected, no point in scanning */
			} else {
				if (memoryCheck_verify_forward
					(portLib, topPadding, topPaddingSize, J9_MEMCHECK_DATA_FREED_VALUE, wrappedBlock)) {
					if (memoryCheck_verify_forward
						(portLib, topPadding, topPaddingSize, J9_MEMCHECK_CODE_FREED_VALUE, wrappedBlock)) {
	
						/* Freed block: top padding at least is damaged.  If the nextBlock pointer is damaged too
						   then we assume the header is useless and we stop scanning (to reduce the chances of a GPF). */
	
						if (blockHeader->nextBlock != prevBlockHeader) {
							portLib->tty_printf(portLib,
												"ERROR: Header of previously freed block at %p (header at %p) is damaged, scan stopped.\n",
												wrappedBlock, blockHeader);
							goto describe_freed_block_and_fail;
						}
	
						everythingOkay = FALSE;
						goto describe_freed_block_and_continue;
					}
				}
			}

			if (blockHeader->allocationNumber > memStats.totalBlocksAllocated) {
				portLib->tty_printf(portLib, "ERROR: Invalid allocation number %d in previously freed block %p (header at %p)\n",
									blockHeader->allocationNumber, wrappedBlock, blockHeader);
				goto describe_freed_block_and_fail;	/* If header is trashed then we risk a GPF by continuing.. */
			}

			if (blockHeader->nextBlock != prevBlockHeader) {
				portLib->tty_printf(portLib,
									"ERROR: Previously freed block list links between %p (header at %p) and %p (header at %p) are corrupt\n",
									((U_8 *) prevBlockHeader) + J9_MEMCHECK_ADJUSTED_PADDING, prevBlockHeader, wrappedBlock,
									blockHeader);
			}

			bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;

			if ( mode & J9_MCMODE_MPROTECT ) {
				bottomPaddingSize = blockHeader->totalAllocation - J9_MEMCHECK_PAGE_SIZE - (bottomPadding - blockHeader->topPage);
			} else {

				/* Adjust bottomPadding to force it to end on a double boundary */
				bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
			}
			bottomSize = bottomPaddingSize + blockHeader->wrappedBlockSize;
			
			if (memoryCheck_verify_forward
				(portLib, wrappedBlock, bottomSize, J9_MEMCHECK_DATA_FREED_VALUE, wrappedBlock)) {
				if (memoryCheck_verify_forward
					(portLib, wrappedBlock, bottomSize, J9_MEMCHECK_CODE_FREED_VALUE, wrappedBlock)) {

					goto describe_freed_block_and_continue;
				}
			}

			/* this block looked okay */
			continue;

		  /* TODO: Why is return value from memoryCheck_describe_block() ignored? */
		  describe_freed_block_and_continue:
			everythingOkay = FALSE;
			memoryCheck_describe_freed_block(portLib, operationName, blockHeader);
		}

	}

	if (memStats.totalBlocksFreed > memStats.totalBlocksAllocated) {
		/* This shouldn't happen.  In order to cause it, somebody has to free garbage (or free a block twice) without
		   this error being caught by any of the previous checks.  If global variables were trashed then we would
		   hopefully go down in flames before we got here. */

		portLib->tty_printf(portLib, "ERROR: More blocks freed (%d) than allocated (%d)!\n",
							memStats.totalBlocksFreed, memStats.totalBlocksAllocated);
		everythingOkay = FALSE;
	}

	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Lock All the Blocks */
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentBlock, lockMode, 0 );
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentFreedBlock, lockMode, 1 );
	}
	return everythingOkay;

  describe_block_and_fail:
	memoryCheck_describe_block(portLib, operationName, blockHeader);
	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Lock All the Blocks */
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentBlock, lockMode, 0 );
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentFreedBlock, lockMode, 1 );
	}
	return FALSE;

  describe_freed_block_and_fail:
	memoryCheck_describe_freed_block(portLib, operationName, blockHeader);
	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Lock All the Blocks */
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentBlock, lockMode, 0 );
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentFreedBlock, lockMode, 1 );
	}
	return FALSE;
}



/* @internal The passed in portLib should be the memCheckPortLib */
static void *
memoryCheck_wrapper_allocate_memory(OMRPortLibrary *portLib, UDATA byteAmount, char const *operationName,
												 J9_MEM_ALLOCATE_FUNC allocator, U_32 paddingValue, U_32 fillValue,
												 U_32 freedValue, const char *callSite, U_32 category)
{
	U_8 *topPadding, *wrappedBlock, *bottomPadding;
	J9MemoryCheckHeader *blockHeader;
	BOOLEAN everythingOkay = TRUE;
	UDATA newAllocationNumber;
	UDATA newTotalBytesAllocated;
	J9PortVmemIdentifier *vmemID = NULL;
	U_8 *topPage = NULL;					/* J9_MCMODE_MPROTECT: Points to the first page boundary in the returned data */
	U_8 *bottomPage = NULL;				/* J9_MCMODE_MPROTECT: Points to the bottom page boundary in the returned data */
	UDATA topPaddingSize = 0;
	UDATA bottomPaddingSize = 0;
	UDATA allocAmount = 0;
	
	MUTEX_ENTER(mcMutex);
	/* Disable trace to avoid deadlock between mcMutex and trace global lock. JTC-JAT 93458 */	
	if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
		uteInterface->server->DisableTrace(UT_DISABLE_THREAD);
	}
	
	if (0 == (mode & J9_MCMODE_NO_SCAN) ) {

		if ( mode & J9_MCMODE_MPROTECT ) {
			/* Need only allocate 1 extra page above and below b/c j9vmem always returns page aligned pointers */
			topPaddingSize = J9_MEMCHECK_PAGE_SIZE - sizeof(J9MemoryCheckHeader);
			bottomPaddingSize = J9_MEMCHECK_PAGE_SIZE;
			allocAmount = sizeof(J9MemoryCheckHeader) + topPaddingSize + (byteAmount/J9_MEMCHECK_PAGE_SIZE)*J9_MEMCHECK_PAGE_SIZE +
			   	(byteAmount%J9_MEMCHECK_PAGE_SIZE ? 1 : 0 )*J9_MEMCHECK_PAGE_SIZE + bottomPaddingSize;
		} else {

			/* Adjust bottomPadding to force it to end on a double boundary */			
			bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (byteAmount);
			topPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);
			allocAmount = sizeof(J9MemoryCheckHeader) + topPaddingSize + byteAmount + bottomPaddingSize;
		}	
		
		newAllocationNumber = memStats.totalBlocksAllocated+1;
		newTotalBytesAllocated = memStats.currentBytesAllocated + byteAmount;
	
		if ((mode & J9_MCMODE_SKIP_TO) && (newAllocationNumber < skipToAlloc))  {
			/* We aren't interested in scanning anything yet. */
		} else  {
			if (mode & J9_MCMODE_FULL_SCANS)  {
				everythingOkay = memoryCheck_scan_all_blocks(portLib);
			} else  {  /* quick mode - just check most recently allocated block */
				if (mostRecentBlock)  {
					J9_MEMCHECK_UNLOCK( mostRecentBlock );
					everythingOkay = memoryCheck_scan_block(portLib, mostRecentBlock);
					J9_MEMCHECK_LOCK( mostRecentBlock );
				}
			}
		}
		if (!everythingOkay)  {
			memoryCheck_abort(portLib);
		}
	
		if ((mode & J9_MCMODE_FAIL_AT) && (newAllocationNumber >= failAtAlloc))  {
			++memStats.failedAllocs;
			if ((memStats.totalBlocksAllocated+memStats.failedAllocs) == failAtAlloc) {
				portLib->tty_printf(portLib, "WARNING: failat: Returning NULL for allocation attempt %d in %s at %s (%d bytes were requested)\n",
										(memStats.totalBlocksAllocated+memStats.failedAllocs), operationName, callSite, byteAmount);
				portLib->tty_printf(portLib, "         All subsequent memory allocation attempt will be failed intentionally\n");
			}

			if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
				uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
			}
			MUTEX_EXIT(mcMutex);
			return NULL;
		}
		if ((mode & J9_MCMODE_LIMIT) && (newTotalBytesAllocated > limitAlloc)) {
			if (memStats.currentBytesAllocated <= limitAlloc) {
				portLib->tty_printf(portLib, "WARNING: limit: Returning NULL for attempt to allocate %d bytes in %s at %s (%d bytes already allocated)\n",
												byteAmount, operationName, callSite, memStats.currentBytesAllocated);
				portLib->tty_printf(portLib, "         All subsequent memory allocation attempt will be failed intentionally\n");
			}
			if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
				uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
			}
			MUTEX_EXIT(mcMutex);
			return NULL;
		}
	
		if ( 0 == (mode & J9_MCMODE_MPROTECT) ) {
	#if defined (WIN32) && !defined(BREW)
			if (mode & J9_MCMODE_TOP_DOWN) {
				topPadding = VirtualAlloc(NULL, allocAmount, MEM_COMMIT | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
			} else {
				/* The portLib being used here is the memCheckPortLib, not the user portLib */
				topPadding = allocator(portLib, allocAmount, callSite, category);
			}
	#else
			/* The portLib being used here is the memCheckPortLib, not the user portLib */
			topPadding = allocator(portLib, allocAmount, callSite, category);
	#endif
			if (!topPadding) {
				++memStats.failedAllocs;
				portLib->tty_printf(portLib, "WARNING: Out of memory in %s at %s, returning NULL (%d bytes were requested)\n",
								operationName, callSite, byteAmount);
				if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
					uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
				}
				MUTEX_EXIT(mcMutex);
				return NULL;
			}
			blockHeader = (J9MemoryCheckHeader *) topPadding;
			topPadding += sizeof(J9MemoryCheckHeader);
			wrappedBlock = topPadding + topPaddingSize;
			bottomPadding = wrappedBlock + byteAmount;
			/* zero out the header */
			memset(blockHeader, 0, sizeof(J9MemoryCheckHeader));
		
		} else {
			/* we're in mode = J9_MCMODE_MPROTECT */
			/* Allocate the structure.  
			 * NOTE: memCheckPortLib->mem_allocate_memory is the "allocator" function (because we can't be
			 *  using the subAllocator in conjunction with mode J9_MCMODE_MPROTECT).
			 * 		making it clear that we are using memCheckPortLib->mem_allocate_memory here because the 
			 * 		corresponding free is done using memCheckPortLib->mem_free_memory (and we don't want to pass in
			 * 		a deallocator to memoryCheck_wrapper_allocate_memory
			 */ 
			vmemID = memCheckPortLib->mem_allocate_memory( memCheckPortLib, sizeof(J9PortVmemIdentifier), callSite, OMRMEM_CATEGORY_VM);
			if (NULL == vmemID) {
				++memStats.failedAllocs;
				portLib->tty_printf(portLib, "WARNING: J9PortVmemIdentifier struct could not be allocated!\n"
								"WARNING: Out of memory in %s at %s, returning NULL (%d bytes were requested)\n",
								operationName, callSite, byteAmount);
				if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
					uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
				}
				MUTEX_EXIT(mcMutex);
				return NULL;
			}	
			/* Reserve and Commit the memory - Pointer is page aligned by j9vmem.  Do NOT use the J9_MEMCHECK_PAGE_SIZE
			 * macro for the pageSize parameter to this function call only!  The macro may return a multiple of the system page size
			 * due to the default implementation of j9vmem using sizeof(UDATA) as a pagesize.
			 */
			topPage = memCheckPortLib->vmem_reserve_memory( memCheckPortLib, NULL, allocAmount, vmemID, allocateMode,
												memCheckPortLib->vmem_supported_page_sizes( memCheckPortLib )[0], category );
	
			if (!topPage) {
				++memStats.failedAllocs;
				portLib->tty_printf(portLib, "WARNING: Out of memory in %s at %s, returning NULL (%d bytes were requested)\n",
									operationName, callSite, byteAmount);
				memCheckPortLib->mem_free_memory( memCheckPortLib, vmemID );
				vmemID = NULL;
				if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
					uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
				}
				MUTEX_EXIT(mcMutex);
				return NULL;
			}
	
			/* Add the vmemID to the hashTable */
			if ( !hashTableAdd( vmemIDTable, &vmemID ) ){
				memCheckPortLib->vmem_free_memory( memCheckPortLib, vmemID->address, vmemID->size, vmemID );
				memCheckPortLib->mem_free_memory( memCheckPortLib, vmemID );
				vmemID = NULL;
				++memStats.failedAllocs;
				portLib->tty_printf(portLib, "WARNING: Unable to add vmemID to hashTable! In %s, returning NULL (%d bytes were requested)\n",
									operationName, byteAmount);
				if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
					uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
				}
				MUTEX_EXIT(mcMutex);
				return NULL;
			}
					
			blockHeader = (J9MemoryCheckHeader *) topPage;
			blockHeader->self = topPage; 	/* Grab the malloc'd ptr for calls to free */
			/* TODO - why is the page size not the same as what was used in vmem_reserve_memory ? */
			wrappedBlock = topPage + J9_MEMCHECK_PAGE_SIZE;
			topPadding = (U_8*)blockHeader + sizeof(J9MemoryCheckHeader);
			bottomPadding = wrappedBlock + byteAmount;
			bottomPage = wrappedBlock + (byteAmount/J9_MEMCHECK_PAGE_SIZE)*J9_MEMCHECK_PAGE_SIZE + (byteAmount % J9_MEMCHECK_PAGE_SIZE ? 1 : 0)*J9_MEMCHECK_PAGE_SIZE;
	
			/* Set it to unlocked */
			blockHeader->isLocked = 0;
			blockHeader->totalAllocation = allocAmount;
			blockHeader->topPage = topPage;
			blockHeader->wrappedBlock = wrappedBlock;
			blockHeader->bottomPage = bottomPage;
			blockHeader->wrappedBlockSize = byteAmount;
				
			/* adjust the user pointer the location of the struct.  This adjustment only needs to be made if the 
			 * data is to be aligned on the bottom page 
			 */
			if ( J9_ALIGN_BOTTOM ) {
				U_32 offsetArea;	
				/* 
				 * TOP and BOTTOM locked pages don't move, just adjust the data between them and the 
				 * other pointers into the memory.  The blockHeader will always be 1 J9_MEMCHECK_PAGE_SIZE above
				 * the wrappedBlock 
				 */	 
				offsetArea =  BYTES_FOR_ALIGNMENT (byteAmount);		
				wrappedBlock = bottomPage - byteAmount - offsetArea;
				blockHeader->wrappedBlock = wrappedBlock;
				/* Ensure header fits entirely in topPage */
				if ( wrappedBlock - J9_MEMCHECK_PAGE_SIZE + sizeof(J9MemoryCheckHeader) > topPage + J9_MEMCHECK_PAGE_SIZE ) {
					memmove( wrappedBlock - J9_MEMCHECK_PAGE_SIZE - sizeof(J9MemoryCheckHeader), blockHeader, sizeof(J9MemoryCheckHeader) );
					blockHeader = (J9MemoryCheckHeader *)(wrappedBlock - J9_MEMCHECK_PAGE_SIZE - sizeof(J9MemoryCheckHeader) );
				}
				else {
					memmove( wrappedBlock - J9_MEMCHECK_PAGE_SIZE, blockHeader, sizeof(J9MemoryCheckHeader) );
					blockHeader = (J9MemoryCheckHeader *)(wrappedBlock - J9_MEMCHECK_PAGE_SIZE );
				}
				topPadding = (U_8 *)blockHeader + sizeof(J9MemoryCheckHeader);
				bottomPadding = wrappedBlock + byteAmount;
			}	
		} /* J9_MCMODE_MPROTECT */
	
		if ( !(mode & J9_MCMODE_MPROTECT) ) {
			if ((mode & J9_MCMODE_SKIP_TO) && (newAllocationNumber < skipToAlloc))  {
				/* We aren't interested in having blocks filled with padding yet. */
			} else  {
				memoryCheck_fill_bytes(portLib, topPadding, topPaddingSize, paddingValue, wrappedBlock);
				memoryCheck_fill_bytes(portLib, bottomPadding, bottomPaddingSize, paddingValue, wrappedBlock);
				if (mode & J9_MCMODE_ZERO)  {
					memset(wrappedBlock, 0, byteAmount);
				} else {
					memoryCheck_fill_bytes(portLib, wrappedBlock, byteAmount, fillValue, wrappedBlock);
				}
			}
		} else {
			if ((mode & J9_MCMODE_SKIP_TO) && (newAllocationNumber < skipToAlloc))  {
				/* We aren't interested in having blocks filled with padding yet. */
			} else  {
				/* Extra padding above */
				if ( blockHeader->self != blockHeader->topPage ) {
					memoryCheck_fill_bytes(portLib, blockHeader->self, blockHeader->topPage - blockHeader->self, paddingValue, wrappedBlock); 
				}
				/* Normal top Padding */	
				memoryCheck_fill_bytes(portLib, topPadding, wrappedBlock - topPadding, paddingValue, wrappedBlock);
				/* Extra padding below */
				memoryCheck_fill_bytes(portLib, bottomPadding, allocAmount - (bottomPadding - blockHeader->self), paddingValue, wrappedBlock);  
				if (mode & J9_MCMODE_ZERO)  {
					memset(wrappedBlock, 0, byteAmount );
				} else {
					memoryCheck_fill_bytes(portLib, wrappedBlock, byteAmount, fillValue, wrappedBlock);
				}
			}
		} 
	
	/* Recompute the allocation number, in case we have been called recursively (due to tracing, for example). */
	newAllocationNumber = memStats.totalBlocksAllocated + 1;
		
	#ifdef J9VM_MEMCHK_SIM_SUP
		if (mode & J9_MCMODE_SIMULATE) {
			portLib->tty_printf(portLib, "void * BLOCK_%d; %s\n", newAllocationNumber, SIM_TAG);
			portLib->tty_printf(portLib, "BLOCK_%d = j9mem_allocate_memory(%d); %s\n", newAllocationNumber, byteAmount, SIM_TAG);
		}
	#endif
	
	} else {
		/* (mode & J9_MCMODE_NO_SCAN) */
		U_8 * blockStart;

		newAllocationNumber = memStats.totalBlocksAllocated + 1;
		if ((mode & J9_MCMODE_FAIL_AT) && (newAllocationNumber >= failAtAlloc))  {
			++memStats.failedAllocs;
			if ((memStats.totalBlocksAllocated+memStats.failedAllocs) == failAtAlloc) {
				portLib->tty_printf(portLib, "WARNING: failat: Returning NULL for allocation attempt %d in %s at %s (%d bytes were requested)\n",
										(memStats.totalBlocksAllocated+memStats.failedAllocs), operationName, callSite, byteAmount);
				portLib->tty_printf(portLib, "         All subsequent memory allocation attempt will be failed intentionally\n");
			}

			if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
				uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
			}
			MUTEX_EXIT(mcMutex);
			return NULL;
		}

		allocAmount = sizeof(J9MemoryCheckHeader) + byteAmount;
		blockStart = allocator(portLib, allocAmount, callSite, category);

		if (NULL == blockStart) {
			portLib->tty_printf(portLib, "WARNING: Out of memory in %s at %s, returning NULL (%d bytes were requested)\n",
					operationName, callSite, byteAmount);
			if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
				uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
			}
			MUTEX_EXIT(mcMutex);
			return NULL;
		}

		blockHeader = (J9MemoryCheckHeader *) blockStart;
		/* zero out the header */
		memset(blockHeader, 0, sizeof(J9MemoryCheckHeader));
		
		wrappedBlock = blockStart + sizeof(J9MemoryCheckHeader);
		blockHeader->wrappedBlock = wrappedBlock;
		blockHeader->wrappedBlockSize = byteAmount;

		if (mode & J9_MCMODE_ZERO)  {
			memset(wrappedBlock, 0, byteAmount );
		}

		/* Recompute the allocation number, in case we have been called recursively (due to tracing, for example). */
		newAllocationNumber = memStats.totalBlocksAllocated + 1;
	
	} /* (mode & J9_MCMODE_NO_SCAN) */

	++memStats.totalBlocksAllocated;
	blockHeader->wrappedBlockSize = byteAmount;
	blockHeader->allocationNumber = newAllocationNumber;
	memStats.totalBytesAllocated += byteAmount;
	memStats.currentBlocksAllocated += 1;
	if (memStats.currentBlocksAllocated > memStats.hiWaterBlocksAllocated) {
		memStats.hiWaterBlocksAllocated = memStats.currentBlocksAllocated;
	}
	memStats.currentBytesAllocated += byteAmount;
	if (memStats.currentBytesAllocated > memStats.hiWaterBytesAllocated) {
		memStats.hiWaterBytesAllocated = memStats.currentBytesAllocated;
	}
	if (byteAmount > memStats.largestBlockAllocated) {
		memStats.largestBlockAllocated = byteAmount;
		memStats.largestBlockAllocNum = newAllocationNumber;
	}

	blockHeader->nextBlock = NULL;
	if ((mode & J9_MCMODE_SKIP_TO) && (newAllocationNumber < skipToAlloc))  {
		/* We don't want this block on the allocated list - no padding or verifying it. */
		blockHeader->previousBlock = NULL;
	} else  {
		blockHeader->previousBlock = mostRecentBlock;
		if (mostRecentBlock) {
			J9_MEMCHECK_UNLOCK( mostRecentBlock );
			mostRecentBlock->nextBlock = blockHeader;
			J9_MEMCHECK_LOCK( mostRecentBlock );
		}
		mostRecentBlock = blockHeader;
	}

	if( avl_tree ) {
		memoryCheck_update_callSites_allocate(portLib, blockHeader, callSite, byteAmount);
		if( (mode & J9_MCMODE_PRINT_CALLSITES) || (mode & J9_MCMODE_PRINT_CALLSITES_SMALL) ) {
			if(memStats.totalBlocksAllocated % callSitePrintCount == 0) {
				if (mode & J9_MCMODE_PRINT_CALLSITES) {
					memoryCheck_dump_callSites(portLib, avl_tree);
				} else if (mode & J9_MCMODE_PRINT_CALLSITES_SMALL) {
					memoryCheck_dump_callSites_small(portLib, avl_tree);
				}
			}
		}
	}

	J9_MEMCHECK_LOCK( blockHeader );
	if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
		uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
	}
	MUTEX_EXIT(mcMutex);
	return wrappedBlock;
}


/* @internal The passed in portLib should be the memCheckPortLib */
static void 
memoryCheck_wrapper_free_memory(OMRPortLibrary *portLib, void *memoryPointer, char const *operationName,
								   J9_MEM_FREE_FUNC deallocator, U_32 paddingValue, U_32 fillValue,
								   U_32 freedValue)
{
	J9MemoryCheckHeader *listBlockHeader, *topHeader;
	UDATA topPaddingSize = 0;
	UDATA bottomPaddingSize = 0;
	U_8 *topPadding = NULL;
	U_8 *wrappedBlock;
	BOOLEAN listHeadersAndPaddingOkay = TRUE;
	BOOLEAN currentBlockOkay = TRUE;
	BOOLEAN blockWasSkipped = FALSE;
	J9PortVmemIdentifier getFromTable;		/* stack allocated object to index into hashTable */
	J9PortVmemIdentifier *getFromTablePtr = NULL;
	J9PortVmemIdentifier **vmemID = NULL;

	MUTEX_ENTER(mcMutex);
	/* Disable trace to avoid deadlock between mcMutex and trace global lock. JTC-JAT 93458 */
	if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
		uteInterface->server->DisableTrace(UT_DISABLE_THREAD);
	}

	if (memoryPointer == NULL) {
		if (0 == (mode & J9_MCMODE_NO_SCAN)) {
			memoryCheck_scan_all_blocks(portLib);	/* just in case */
		} else {
			/* J9_MCMODE_NO_SCAN - don't scan! */
		}
		if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
			uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
		}
		MUTEX_EXIT(mcMutex);
		return;
	}
	
	if (0 == (mode & J9_MCMODE_NO_SCAN) ) {

		if ( !(mode & J9_MCMODE_MPROTECT) ) {
			topPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);
		} else {
			topPaddingSize = J9_MEMCHECK_PAGE_SIZE - sizeof(J9MemoryCheckHeader);
		}

		wrappedBlock = (U_8 *) memoryPointer;
		topPadding = wrappedBlock - topPaddingSize;
		topHeader = (J9MemoryCheckHeader *) (topPadding - sizeof(J9MemoryCheckHeader));

		if ( mode & J9_MCMODE_MPROTECT ) {
			/* Find the J9MemoryCheckHeader in the guardPage */
			topHeader = (J9MemoryCheckHeader *)(wrappedBlock - J9_MEMCHECK_PAGE_SIZE);

#if defined(J9ZOS390)
			/* if unlocking fails subtract sizeof(J9MemoryCheckHeader) from topHeader and try again*/
			if (memoryCheck_lockGuardPages( memCheckPortLib, topHeader, J9_MEMCHECK_PAGE_SIZE, unlockMode ) != 0) {
				topHeader = (J9MemoryCheckHeader *)((U_8 *)topHeader - sizeof(J9MemoryCheckHeader));
				J9_MEMCHECK_UNLOCK( topHeader );
			} else {
				/* We must set to unlocked here because we did not call the J9_MEMCHECK_UNLOCK() macro */
				topHeader->isLocked = 0;
			}
			topPadding = (U_8 *)topHeader + sizeof(J9MemoryCheckHeader);
#else
			/* adjust pointer to the header because the header will be stored so that it is entirely in the guard page */
			if ( ( ((UDATA)topHeader & 0XF000) != ((UDATA)((U_8 *)topHeader + sizeof(J9MemoryCheckHeader)) & 0XF000) ) ) {
				if ( (UDATA)((U_8*)topHeader + sizeof(J9MemoryCheckHeader)) % J9_MEMCHECK_PAGE_SIZE != 0 ) {
					topHeader = (J9MemoryCheckHeader *)((U_8 *)topHeader - sizeof(J9MemoryCheckHeader));
				}
			}		

			topPadding = (U_8 *)topHeader + sizeof(J9MemoryCheckHeader);
			J9_MEMCHECK_UNLOCK( topHeader );
#endif
			
		}

#ifdef J9VM_MEMCHK_SIM_SUP
		if (mode & J9_MCMODE_SIMULATE) {
			portLib->tty_printf(portLib, "j9mem_free_memory(BLOCK_%d); %s\n", topHeader->allocationNumber, SIM_TAG);
		}
#endif

		if (mode & J9_MCMODE_SKIP_TO)  {
			/* We must trust the allocation number in the header, to know if the block was
				considered interesting or not when it was allocated. */
			if (topHeader->allocationNumber < skipToAlloc)  {
				blockWasSkipped = TRUE;
				goto found_block;
			}
		}

		currentBlockOkay = memoryCheck_scan_block(portLib, topHeader);

		if ( (mode & J9_MCMODE_FULL_SCANS) || (!currentBlockOkay) ) {
			/* The current block could be an unknown block, but before we check to see if the block is on the allocate list  
			 * we have to scan all blocks to make sure the list is not trashed */
			/* scan all blocks expects all blocks to be locked and returns them as all locked */
			J9_MEMCHECK_LOCK( topHeader);
			listHeadersAndPaddingOkay = memoryCheck_scan_all_blocks(portLib);
			J9_MEMCHECK_UNLOCK(topHeader);
		} else  {  /* quick mode */
			if (mostRecentBlock)  {
				/* memoryCheck_scan_block expects the block to be unlocked */
				J9_MEMCHECK_UNLOCK( mostRecentBlock );
				listHeadersAndPaddingOkay = memoryCheck_scan_block(portLib, mostRecentBlock);
				J9_MEMCHECK_LOCK(mostRecentBlock);
			}
		}
		if (!listHeadersAndPaddingOkay) {
			memoryCheck_abort(portLib);
		}

		if ( (mode & J9_MCMODE_FULL_SCANS) || (!currentBlockOkay && listHeadersAndPaddingOkay) )  {
			/* Walk the allocated list to make sure this block is on it.  If not, then we're either freeing random garbage or
			   this block has already been freed, or this block was not allocated by the port library */

			J9_MEMCHECK_LOCK( topHeader ); 			
			listBlockHeader = mostRecentBlock;
			J9_MEMCHECK_UNLOCK( listBlockHeader );

			while ( listBlockHeader ) {
				if (listBlockHeader == topHeader) {
					J9_MEMCHECK_UNLOCK( topHeader );
					goto found_block;
				}
				if ( listBlockHeader->previousBlock ) {
					J9_MEMCHECK_UNLOCK( listBlockHeader->previousBlock );
					listBlockHeader = listBlockHeader->previousBlock;
					if (listBlockHeader->nextBlock ) {
						J9_MEMCHECK_LOCK( listBlockHeader->nextBlock );
					}
				} else {
					break;
				}
			}

			J9_MEMCHECK_LOCK( listBlockHeader );
			J9_MEMCHECK_UNLOCK( topHeader );
			if (mode & J9_MCMODE_IGNORE_UNKNOWN_BLOCKS) {
				/* ignore this block and return*/
				portLib->tty_printf(portLib, "WARNING: Tried %s on %p (header at %p), but block is not on the allocated list.\n",
									operationName, wrappedBlock, topHeader);
				memStats.totalUnknownBlocksIgnored++;
				J9_MEMCHECK_LOCK( topHeader );
				if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
					uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
				}
				MUTEX_EXIT(mcMutex);
				return;
			} else {
				portLib->tty_printf(portLib, "ERROR: Tried %s on %p (header at %p), but block is not on the allocated list\n",
									operationName, wrappedBlock, topHeader);
				goto describe_block_and_fail;
			}
		} else  {
			/* Quick mode: Don't walk allocated list, just rely on scanning the block to catch freeing errors. */
			J9_MEMCHECK_UNLOCK( topHeader );
		}
found_block:

		/* Adjust bottomPadding to force it to end on a double boundary */
		bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (topHeader->wrappedBlockSize);

	} else {
		/* we are in J9_MCMODE_NO_SCAN mode  */
		wrappedBlock = (U_8 *) memoryPointer;
		topHeader = (J9MemoryCheckHeader *) (wrappedBlock - sizeof(J9MemoryCheckHeader));

	} /* ( 0 == (mode & J9_MCMODE_NO_SCAN)) */
	
	/* So we scanned it before (unless we were in J9_MCMODE_NO_SCAN mode) 
	 * and it looked okay, unlink the block.. */
	if (mostRecentBlock == topHeader) {
		mostRecentBlock = topHeader->previousBlock;
	}
	if (topHeader->nextBlock) {
		J9_MEMCHECK_UNLOCK( topHeader->nextBlock );
		topHeader->nextBlock->previousBlock = topHeader->previousBlock;
		J9_MEMCHECK_LOCK( topHeader->nextBlock );
	}
	if (topHeader->previousBlock) {
		J9_MEMCHECK_UNLOCK( topHeader->previousBlock );
		topHeader->previousBlock->nextBlock = topHeader->nextBlock;
		J9_MEMCHECK_LOCK( topHeader->previousBlock );
	}
	topHeader->previousBlock = topHeader->nextBlock = NULL;

	if (!blockWasSkipped)  {
		if ( mode & J9_MCMODE_MPROTECT ) {
			if ( J9_ALIGN_BOTTOM ) {
				if (topHeader->self != topHeader->topPage ) {
					memoryCheck_fill_bytes(portLib, topHeader->self, topHeader->topPage - topHeader->self, freedValue, wrappedBlock);
				}
			}
			memoryCheck_fill_bytes(portLib, topPadding, topHeader->totalAllocation - ((UDATA)topPadding - (UDATA)topHeader->self),
								   freedValue, wrappedBlock);
		} else if (0 == (mode & J9_MCMODE_NO_SCAN)) {
			memoryCheck_fill_bytes(portLib, topPadding, topPaddingSize + topHeader->wrappedBlockSize + bottomPaddingSize,
								   freedValue, wrappedBlock);
		}
	}

	memStats.totalBlocksFreed += 1;
	memStats.totalBytesFreed += topHeader->wrappedBlockSize;
	memStats.currentBytesAllocated -= topHeader->wrappedBlockSize;
	memStats.currentBlocksAllocated -= 1;

	/* update the J9MEMAVLTreeNode responsible for this memory block */
	memoryCheck_update_callSites_free(topHeader->node, topHeader->wrappedBlockSize);

	if (!blockWasSkipped && (mode & J9_MCMODE_NEVER_FREE))  {
		/* Block was painted with recognizable contents above. */
		/* Put block on freed list. */
		topHeader->previousBlock = mostRecentFreedBlock;
		if (mostRecentFreedBlock) {
			J9_MEMCHECK_UNLOCK( mostRecentFreedBlock);
			mostRecentFreedBlock->nextBlock = topHeader;
			J9_MEMCHECK_LOCK( mostRecentFreedBlock);
		}
		mostRecentFreedBlock = topHeader;

		/* Lock the data area of the block */
		J9_MEMCHECK_UNLOCK( topHeader);
		J9_MEMCHECK_LOCK_BODY(  topHeader );
		J9_MEMCHECK_LOCK( topHeader);
	} else  {
		topHeader->wrappedBlockSize = J9_MEMCHECK_FREED_SIZE;
		if ( !(mode & J9_MCMODE_MPROTECT) ) {
#if defined (WIN32) && !defined(BREW)
			if (mode & J9_MCMODE_TOP_DOWN) {
				VirtualFree(topHeader, 0, MEM_RELEASE);
			} else {
				/* The portLib being used here is the memCheckPortLib, not the user portLib */
				deallocator(portLib, topHeader);
			}
#else
			/* The portLib being used here is the memCheckPortLib, not the user portLib */
			deallocator(portLib, topHeader);
#endif
		} else {		/* J9_MCMODE_MPROTECT */
			/* Create a J9PortVmemIdentifier with the correct address, that can be used to get the right object from the hashTable */
			getFromTable.address = topHeader->self;
			getFromTablePtr = &getFromTable;

			/* Remove from the vmemIDTable and release the vmem */
			vmemID = (J9PortVmemIdentifier **)hashTableFind( vmemIDTable, &getFromTablePtr );
			if ( vmemID ) {
				J9PortVmemIdentifier *realVmemID = *vmemID;
				
				hashTableRemove( vmemIDTable, &getFromTablePtr );
				memCheckPortLib->vmem_free_memory( memCheckPortLib, realVmemID->address, realVmemID->size, realVmemID );
				memCheckPortLib->mem_free_memory( memCheckPortLib, realVmemID );
			}
		}
	}

	if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
		uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
	}
	MUTEX_EXIT(mcMutex);
	return;

describe_block_and_fail:
	memoryCheck_describe_block(portLib, operationName, topHeader);
	memoryCheck_abort(portLib);
}



/* This method formats and prints the contents of the indicated region to the console. 
 *
 * @internal The portlibrary passed in should always be the memCheckPortLib 
 */
static void
memoryCheck_dump_bytes(OMRPortLibrary *portLib, void *dumpAddress, UDATA dumpSize)
{
	U_8 *c;
	UDATA digit;
	UDATA column = 0;
	char buffer[128];
	char *bufp, *asciip;
	UDATA address;
	UDATA skipCols = ((UDATA) dumpAddress) & 15;

	for (c = (U_8 *) dumpAddress; c < ((U_8 *) dumpAddress) + dumpSize; c++) {
		if (column == 0) {
			memset(buffer, ' ', sizeof(buffer));
			address = (UDATA) c & ~15;
			digit = sizeof(UDATA) * 2;
			bufp = buffer + digit;
			asciip = buffer + digit + (16 * 3) + 3;
			*bufp++ = ':';
			while (digit--) {
				buffer[digit] = hexd[address & 15];
				address >>= 4;
			}
		}
		while (skipCols) {
			bufp++;
			*bufp++ = '-';
			bufp++;
			asciip++;
			column++;
			skipCols--;
		}
		*bufp++ = ' ';
		*bufp++ = hexd[((*c) >> 4) & 15];
		*bufp++ = hexd[(*c) & 15];
		*asciip++ = (*c >= ' ' && *c <= '~') ? *c : '.';

		if (++column >= 16) {
			column = 0;
			*asciip++ = '\n';
			*asciip++ = '\0';
			portLib->tty_printf(portLib, "%s", buffer);
		}
	}
	if (column != 0) {
		*asciip++ = '\n';
		*asciip = '\0';
		/*increment asciip here if it needs to be used further*/
		portLib->tty_printf(portLib, "%s", buffer);
	}
}


/**
  * @param portLib	The "memCheckPortLib" with its original functions is the one that should be used here (I think that should read:
  *   ""memCheckPortLib" should be passed into this function "
  */ 
static void 
memoryCheck_print_stats(OMRPortLibrary *portLib)
{
	portLib->tty_printf(portLib, "Memory checker statistics:\n");
	portLib->tty_printf(portLib, "Total blocks allocated = %u ( = most recent allocationNumber)\n", memStats.totalBlocksAllocated);
	portLib->tty_printf(portLib, "Total blocks freed = %u\n", memStats.totalBlocksFreed);
	portLib->tty_printf(portLib, "Total bytes allocated = %llu\n", memStats.totalBytesAllocated);
	portLib->tty_printf(portLib, "Total bytes freed = %llu\n", memStats.totalBytesFreed);
	portLib->tty_printf(portLib, "Total unknown blocks ignored = %u\n", memStats.totalUnknownBlocksIgnored);
	portLib->tty_printf(portLib, "High water blocks allocated = %u\n", memStats.hiWaterBlocksAllocated);
	portLib->tty_printf(portLib, "High water bytes allocated = %u\n", memStats.hiWaterBytesAllocated);
	portLib->tty_printf(portLib, "Largest block ever allocated = size %u, allocation number %u\n",
		memStats.largestBlockAllocated, memStats.largestBlockAllocNum);
	portLib->tty_printf(portLib, "Failed allocation attempts = %u\n", memStats.failedAllocs);
}

/* This method scans a single block and returns FALSE if it is damaged or TRUE if everything looks okay.
	This method does not check links to/from neighboring blocks.  If any damage is detected in the block, then
	memoryCheck_scan_all_blocks will be run to detect and print out as much damage as it can, and then FALSE
	will be returned.  Otherwise, this method will return TRUE. */
/*@precondition:	The block must be UNLOCKED 
 * @internal The portLibrary used must be the memCheckPortLib 
 */
static BOOLEAN 
memoryCheck_scan_block(OMRPortLibrary *portLib, J9MemoryCheckHeader *blockHeader)
{
	BOOLEAN everythingOkay = TRUE;
	char const *operationName = "scan_block";
	UDATA topPaddingSize = 0;
	UDATA bottomPaddingSize;
	U_8 *wrappedBlock;

	U_8 *topPadding = ((U_8 *) blockHeader) + sizeof(J9MemoryCheckHeader);
	U_8 *bottomPadding;

	if (mode & J9_MCMODE_MPROTECT) {
		topPaddingSize = blockHeader->wrappedBlock - topPadding;
		wrappedBlock = blockHeader->wrappedBlock;
	} else {
		topPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);
		wrappedBlock = topPadding + topPaddingSize;

	}

	/* Verify top padding */
	if ( (mode & J9_MCMODE_MPROTECT) && !J9_ALIGN_BOTTOM ) {
		/* all the top padding is write protected, no point in scanning */
	} else {
		if (memoryCheck_verify_forward(
			portLib, topPadding, topPaddingSize, J9_MEMCHECK_DATA_PADDING_VALUE, wrappedBlock)) {
			if (memoryCheck_verify_forward(
				portLib, topPadding, topPaddingSize, J9_MEMCHECK_CODE_PADDING_VALUE, wrappedBlock)) {
	
				/* Either block was already freed, or top padding is damaged. */
				goto scan_heap_and_fail;
			}
		}
	}
	
	if (blockHeader->allocationNumber > memStats.totalBlocksAllocated) {
		/* Allocation number is clearly invalid. */
		goto scan_heap_and_fail;
	}

	bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;
	
	if ( mode & J9_MCMODE_MPROTECT ) {
		bottomPaddingSize = blockHeader->totalAllocation - J9_MEMCHECK_PAGE_SIZE - (bottomPadding - blockHeader->topPage);
	} else {

		/* Adjust bottomPadding to force it to end on a double boundary */
		bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
	}

	/* Verify bottom padding */
	if (memoryCheck_verify_forward
		(portLib, bottomPadding, bottomPaddingSize, J9_MEMCHECK_DATA_PADDING_VALUE, wrappedBlock)) {
		if (memoryCheck_verify_forward
			(portLib, bottomPadding, bottomPaddingSize, J9_MEMCHECK_CODE_PADDING_VALUE, wrappedBlock)) {
			/* Bottom padding looks trashed. */
			goto scan_heap_and_fail;
		}
	}

	if (memStats.totalBlocksFreed > memStats.totalBlocksAllocated) {
		/* This shouldn't happen.  In order to cause it, somebody has to free garbage (or free a block twice) without
		   this error being caught by any of the previous checks.  If global variables were trashed then we would
		   hopefully go down in flames before we got here. */
		goto scan_heap_and_fail;
	}
	return TRUE;

  scan_heap_and_fail:
	memoryCheck_scan_all_blocks(portLib);
	return FALSE;
}

/**
 * This method prints whatever information as it can gather about a block from the block and its immediate neighbors (if any).
 * If the block looks like an uncorrupted freed block, TRUE is returned.  If detectable corruption of the block is discovered, FALSE
 *	is returned.
 * 
 * The function will Lock and Unlock the body of the wrappedBlocked, but the top and bottom page must be unlocked when the 
 * block is passed in 
 *
 * @internal The portLibrary passed in should be the memCheckPortLibrary, not the one from the user 
 */

static BOOLEAN memoryCheck_describe_freed_block(OMRPortLibrary *portLib, char const *operationName,
										  J9MemoryCheckHeader *blockHeader)
{
	BOOLEAN everythingOkay = TRUE;
	UDATA topPaddingSize = 0;
	UDATA bottomPaddingSize = 0;
	U_32 paddingValue;
	UDATA topPaddingSmashed, bottomPaddingSmashed;

	U_8 *topPadding = ((U_8 *) blockHeader) + sizeof(J9MemoryCheckHeader);
	U_8 *wrappedBlock = NULL;
	U_8 *bottomPadding = NULL;
	UDATA dumpSize = 0;
	U_8 *dumpPos = topPadding;

	if ( !(mode & J9_MCMODE_MPROTECT) ) {
		topPaddingSize =  J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);
		bottomPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
		wrappedBlock = ((U_8 *) blockHeader) + J9_MEMCHECK_ADJUSTED_PADDING;
	} else {
		topPaddingSize =  J9_MEMCHECK_PAGE_SIZE - sizeof(J9MemoryCheckHeader);

		/* Adjust bottomPadding to force it to end on a double boundary */
		bottomPaddingSize = J9_MEMCHECK_PAGE_SIZE + BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
		wrappedBlock = ((U_8 *) blockHeader) + J9_MEMCHECK_PAGE_SIZE;
	}
	bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;
	dumpSize = topPaddingSize + blockHeader->wrappedBlockSize + bottomPaddingSize;

	if ( mode & J9_MCMODE_MPROTECT ) {
		if ( J9_ALIGN_BOTTOM ) {
			U_32 offsetArea =  BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
			wrappedBlock = blockHeader->bottomPage - blockHeader->wrappedBlockSize - offsetArea;
		}
		bottomPadding = wrappedBlock + blockHeader->wrappedBlockSize;
	}

	portLib->tty_printf(portLib, "%s describing previously freed block at %p (header at %p):\n", operationName, wrappedBlock,
						blockHeader);

	if (!memoryCheck_verify_forward(portLib, topPadding, 8, J9_MEMCHECK_DATA_FREED_VALUE, wrappedBlock)) {
		paddingValue = J9_MEMCHECK_DATA_FREED_VALUE;
	} else if (!memoryCheck_verify_forward(portLib, topPadding, 8, J9_MEMCHECK_CODE_FREED_VALUE, wrappedBlock)) {
		paddingValue = J9_MEMCHECK_CODE_FREED_VALUE;
	} else {
		/* TODO: check for legal padding with wrong wrappedBlock bits here? */

		/* Test if alignment is off by 4 */
		if ( (UDATA)topPadding%8 == 4 ) {
			if (!memoryCheck_verify_forward(portLib, topPadding+4, 8, J9_MEMCHECK_DATA_FREED_VALUE, wrappedBlock)) {
				paddingValue = J9_MEMCHECK_DATA_FREED_VALUE;
			} else if (!memoryCheck_verify_forward(portLib, topPadding +4, 8, J9_MEMCHECK_CODE_FREED_VALUE, wrappedBlock)) {
				paddingValue = J9_MEMCHECK_CODE_FREED_VALUE;
			}
			portLib->tty_printf(portLib, "Previously freed block has unrecognized padding %08x %08x (header is probably trashed)!\n",
							((U_32 *) topPadding)[0], ((U_32 *) topPadding)[1]);
			everythingOkay = FALSE;
			goto dump_header_only;
		} else {
			portLib->tty_printf(portLib, "Previously freed block has unrecognized padding %08x %08x (header is probably trashed)!\n",
							((U_32 *) topPadding)[0], ((U_32 *) topPadding)[1]);
			everythingOkay = FALSE;
			goto dump_header_only;
		}
	}

	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Check if extra top smashed */
		if (blockHeader->self != blockHeader->topPage ) {
			topPaddingSmashed = memoryCheck_verify_forward(portLib, blockHeader->self, 
							blockHeader->topPage - blockHeader->self, paddingValue, wrappedBlock);
			if (topPaddingSmashed) {
				portLib->tty_printf(portLib, "Last %d bytes of extra top padding are damaged\n", topPaddingSmashed);
				everythingOkay = FALSE;
			}
		}
	}

	topPaddingSmashed = memoryCheck_verify_forward(portLib, topPadding, topPaddingSize, paddingValue, wrappedBlock);
	if (topPaddingSmashed) {
		portLib->tty_printf(portLib, "Last %d bytes of top padding have been corrupted\n", topPaddingSmashed);
		everythingOkay = FALSE;
	}

	J9_MEMCHECK_UNLOCK_BODY( blockHeader );

	if (memoryCheck_verify_forward(portLib, wrappedBlock, blockHeader->wrappedBlockSize, paddingValue, wrappedBlock))
	{
		portLib->tty_printf(portLib, "Some bytes of wrapped block have been corrupted\n");
		everythingOkay = FALSE;
	}

	if ( mode & J9_MCMODE_MPROTECT ) {
		const UDATA pageSize = J9_MEMCHECK_PAGE_SIZE;
		UDATA checkSize = pageSize;

		if (J9_ALIGN_BOTTOM) {
			
			/* Size to check on the bottom is the bottomPage and the alignment bytes. */
			checkSize += BYTES_FOR_ALIGNMENT (blockHeader->wrappedBlockSize);
		} else {
 			UDATA addMod = blockHeader->wrappedBlockSize % pageSize;
 			if ( addMod ) {
 				checkSize += pageSize - (blockHeader->wrappedBlockSize % pageSize);
 			}
		}
		bottomPaddingSmashed = memoryCheck_verify_backward(portLib, bottomPadding, checkSize , paddingValue, wrappedBlock);
	} else {
		bottomPaddingSmashed = memoryCheck_verify_backward(portLib, bottomPadding, bottomPaddingSize, paddingValue, wrappedBlock);
	}

	if (bottomPaddingSmashed) {
		portLib->tty_printf(portLib, "First %d bytes of bottom padding have been corrupted\n", bottomPaddingSmashed);
		everythingOkay = FALSE;
	}

	portLib->tty_printf(portLib, "Wrapped block size was %d, allocation number was %d\n",
						blockHeader->wrappedBlockSize, blockHeader->allocationNumber);

	if (everythingOkay) {
		J9_MEMCHECK_LOCK_BODY( blockHeader );
		return TRUE;
	}

  dump_block:
	memoryCheck_dump_bytes(portLib, blockHeader, sizeof(blockHeader));
	memoryCheck_dump_bytes(portLib, dumpPos, dumpSize);

	J9_MEMCHECK_LOCK_BODY( blockHeader );
	return everythingOkay;

  dump_header_only:

	portLib->tty_printf(portLib, "(only top padding + first 64 bytes of wrapped block will be printed here)\n");
	dumpSize = topPaddingSize + 64;
	goto dump_block;

}


/**
 * This function scans the region of fillSize bytes at fillAddress, from lower to higher addresses.  It determines the number of correct bytes of padding in
 *	the scan direction before the first incorrect byte.  If an incorrect byte is found, all remaining bytes are considered incorrect, and the number of incorrect
 *	bytes (remaining+1) is returned. 
 * @precondition: block must be UNLOCK'ed prior to calling this function 
 */
static UDATA 
memoryCheck_verify_forward(OMRPortLibrary *portLib, U_8 *fillAddress, UDATA fillSize, U_32 fillValue, U_8 *blockAddress)
{
	U_8 fillWith[8];
	I_32 blockAddressInt = (I_32)(IDATA)blockAddress;
	UDATA smashedBytes = fillSize;
	U_8 *c;
	UDATA index = ((UDATA) fillAddress) & 7;

	memcpy(fillWith, &fillValue, 2);
	memcpy(fillWith+2, &blockAddressInt, 4);
	memcpy(fillWith+6, ((U_8 *)(&fillValue)) + 2, 2);

	for (c = fillAddress; c < fillAddress+fillSize; c++)  {
		if (*c != fillWith[index])  break;
		index++;
		index &= 7;
		smashedBytes--;
	}
	return smashedBytes;
}



/**
 * This function scans the region of fillSize bytes at fillAddress, from lower to higher addresses.  It determines the number of correct bytes of padding in
 *	the scan direction before the first incorrect byte.  If an incorrect byte is found, all remaining bytes are considered incorrect, and the number of incorrect
 *	bytes (remaining+1) is returned. 
 * @precondition: block must be UNLOCK'ed prior to calling this function 
 */
static UDATA 
memoryCheck_verify_backward(OMRPortLibrary *portLib, U_8 *fillAddress, UDATA fillSize, U_32 fillValue, U_8 *blockAddress)
{
	U_8 fillWith[8];
	I_32 blockAddressInt = (I_32)(IDATA)blockAddress;
	UDATA smashedBytes = fillSize;

	U_8 *c;
	UDATA index = ((UDATA) fillAddress + fillSize - 1) & 7;

	memcpy(fillWith, &fillValue, 2);
	memcpy(fillWith+2, &blockAddressInt, 4);
	memcpy(fillWith+6, ((U_8 *)(&fillValue)) + 2, 2);

	for (c = fillAddress+fillSize-1; c >= fillAddress; c--) {
		if (*c != fillWith[index])  break;
		index--;
		index &= 7;
		smashedBytes--;
	}
	return smashedBytes;
}


/* 
 * Normal shutdown of memory check 
 */
void memoryCheck_print_report(J9PortLibrary *j9portLib)
{
	if (NULL == old_port_shutdown_library) {
		/* We aren't installed */
		return;
	}
	
	MUTEX_ENTER(mcMutex);
	/* Disable trace to avoid deadlock between mcMutex and trace global lock. JTC-JAT 93458 */	
	if (uteInterface && uteInterface->server) {
		uteInterface->server->DisableTrace(UT_DISABLE_THREAD);
	}
	memoryCheck_print_summary(memCheckPortLib, J9_MEMCHECK_SHUTDOWN_NORMAL);
	if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
		uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
	}
	MUTEX_EXIT(mcMutex);
}

/*
 * Search for a memorycheck option on the command line. Elements in argv
 * are searched in reverse order. The first element is always ignored (typically
 * the first argv element is the program name, not a command line option).
 *
 * If one is found, return its index, otherwise return 0.
 *
 * This should happen before anybody allocates memory!  Otherwise, shutdown will
 * not work properly.
 */
IDATA 
memoryCheck_parseCmdLine(J9PortLibrary *j9portLibrary, UDATA lastLegalArg, char **argv)
{
	OMRPortLibrary *portLibrary = OMRPORT_FROM_J9PORT(j9portLibrary);
	UDATA i;

	for (i = lastLegalArg; i >= 1; i--)  {
		/* new style -Xcheck:memory options */
		if (0 == strcmp("-Xcheck", argv[i])) {
			memoryCheck_initialize(j9portLibrary, "all", argv);
			return i;
		} else if ( (0 == strcmp("-Xcheck:none", argv[i])) ||
					(0 == strcmp("-Xcheck:memory:none", argv[i])) ||
					(0 == strcmp("-Xcheck:help", argv[i])) ) {
			/* stop looking at -Xcheck options */
			return -1;
			break;
		} else if ( (0 == strcmp("-Xcheck:memory:help", argv[i])) ) {
			/* print out the -Xcheck:memory:help text */
			portLibrary->tty_printf( portLibrary, "\nUsage: Xcheck:memory[:<option>]\n\n");
			portLibrary->tty_printf( portLibrary, "options (default is all):\n");
			portLibrary->tty_printf( portLibrary, "  all\n");
			portLibrary->tty_printf( portLibrary, "  quick\n");
			portLibrary->tty_printf( portLibrary, "  nofree\n");
			portLibrary->tty_printf( portLibrary, "  failat\n");
			portLibrary->tty_printf( portLibrary, "  skipto\n");
			portLibrary->tty_printf( portLibrary, "  callsite\n");
			portLibrary->tty_printf( portLibrary, "  zero\n\n");
			return -1;
			break;
		} else if (0 == strcmp("-Xcheck:memory",argv[i])) {
			memoryCheck_initialize(j9portLibrary, "all", argv);
			return i;
		} else if (0 == strncmp("-Xcheck:memory:", argv[i], sizeof("-Xcheck:memory:") - 1  )) {
			memoryCheck_initialize(j9portLibrary, argv[i] + sizeof("-Xcheck:memory:") - 1, argv );
			return i;
		} 
	}

	return 0;
}



/* The port library only shuts down components that absolutely need to be. 
 *  EG., very few or none of the port library components are shut down.
 * Therefore, shutdown memorycheck, restore user's original portlib shutdown 
 *  functions and call the original exit_shutdown_and_exit.
 * 
*/ 
static void
memoryCheck_exit_shutdown_and_exit(OMRPortLibrary *portLib, I_32 exitCode)
{
	if (!old_shutdown_and_exit) {
		/* We aren't installed */
		return;
	}
	

	memoryCheck_shutdown_internal( memCheckPortLib, J9_MEMCHECK_SHUTDOWN_EXIT );

	memoryCheck_restore_old_shutdown_functions( portLib );

	portLib->exit_shutdown_and_exit( portLib, exitCode );
}

/* remove all unfreedBlocks that is was not allocated by VM
 * The list of ignored callsites are defined in ignoredCallsites
 *
 * This function assumes mcMutex is acquired
 *
 *
 * returns the number of unfreed blocks that was removed from the link list
 */

static UDATA
memoryCheck_filter_nonVM_unFreed_Blcoks(OMRPortLibrary *portLib)
{
	UDATA result = 0;
	J9MemoryCheckHeader *blockHeader, *previous, *next;
	UDATA unfreedBlocks = memStats.totalBlocksAllocated - memStats.totalBlocksFreed;
	UDATA i;
	char ignoredCallsites[MAX_CALLSITE_COUNT][MAX_CALLSITE_LENGTH], *strPtr;
	UDATA ignoredCallsitesSize = 0;
#ifdef J9ZOS390
	char *ebcdic_callsite;
#endif

	memset(ignoredCallsites, '\0', MAX_CALLSITE_COUNT * MAX_CALLSITE_LENGTH* sizeof(char));

	strPtr = strtok( ignoreCallSiteStr, CALLSITE_DELIMITER );

	while (NULL != strPtr) {
		if (ignoredCallsitesSize >= MAX_CALLSITE_COUNT) {
				portLib->tty_printf(portLib, "internal buffer full, ignoredCallSite %s discarded\n", strPtr);
				strPtr = strtok( NULL, CALLSITE_DELIMITER );
				continue;
		}
		if (strlen(strPtr) >= MAX_CALLSITE_LENGTH) {
			portLib->tty_printf(portLib, "ignoredCallSite %s length exceeds internal buffer size. Callsite discarded\n", strPtr);
			strPtr = strtok( NULL, CALLSITE_DELIMITER );
			continue;
		}
		strcpy(ignoredCallsites[ignoredCallsitesSize++], strPtr);
		strPtr = strtok( NULL, CALLSITE_DELIMITER );
	}

	for (blockHeader = mostRecentBlock; (NULL != blockHeader) && (0 != unfreedBlocks); unfreedBlocks--) {
		if (blockHeader->node) {
			previous = blockHeader->previousBlock;
			next = blockHeader->nextBlock;
#ifdef J9ZOS390
			ebcdic_callsite = e2a_string(blockHeader->node->callSite);
#endif
			for (i = 0; i < ignoredCallsitesSize; i++) {
				if ((0 == strncmp(blockHeader->node->callSite, ignoredCallsites[i], strlen(ignoredCallsites[i])))
#ifdef J9ZOS390
					|| (0 == strncmp(ebcdic_callsite, ignoredCallsites[i], strlen(ignoredCallsites[i])))

#endif
				) {
#if 0
					portLib->tty_printf(portLib, "WARNING: unfreed block #%d allocated by %s is being ignored from memcheck\n",
							blockHeader->allocationNumber,blockHeader->node->callSite);
#endif
					if (previous) {
						previous->nextBlock = next;
					}

					if (next) {
						next->previousBlock = previous;
					}

					if (blockHeader == mostRecentBlock) {
						mostRecentBlock = previous;
					}
					globalDeallocator(portLib, blockHeader);
					result++;
#ifdef J9ZOS390
					free(ebcdic_callsite);
#endif
					break;
				}
			}
#ifdef J9ZOS390
			free(ebcdic_callsite);
#endif
		}
		blockHeader = previous;
	}

	portLib->tty_printf(portLib, "WARNING: %d blocks were ignored per ignoredCallsite parameter\n", result);
	return result;

}

/* Does a final scan and prints the final summary/stats. Please note that this
 * function expects the caller to have already locked mcMutex.
 * 
 * @internal The portLib should be the memCheckPortLib
 */
static void 
memoryCheck_print_summary(OMRPortLibrary *portLib, I_32 shutdownMode)
{
	J9MemoryCheckHeader *blockHeader;
	UDATA ignored = 0;

	if (0 == (mode & J9_MCMODE_NO_SCAN) ) {		
		memoryCheck_scan_all_blocks(portLib);	/* One last check for trashing. */
	}

	/* don't report memory leaks when terminating with exit_shutdown_and_exit */
	if ( (shutdownMode == J9_MEMCHECK_SHUTDOWN_NORMAL) && (mostRecentBlock)) {
		UDATA unfreedBlocks = memStats.totalBlocksAllocated - memStats.totalBlocksFreed;
		portLib->tty_printf(portLib, "WARNING: %d unfreed blocks remaining at shutdown!\n", unfreedBlocks);
		ignored = memoryCheck_filter_nonVM_unFreed_Blcoks(portLib);
		if (unfreedBlocks > J9_MEMCHECK_MAX_DUMP_LEAKED_BLOCKS) {
			unfreedBlocks = J9_MEMCHECK_MAX_DUMP_LEAKED_BLOCKS;
			portLib->tty_printf(portLib, "WARNING: only %d most recent leaked blocks will be described\n",
								unfreedBlocks);
		}
		for (blockHeader = mostRecentBlock; blockHeader && unfreedBlocks; unfreedBlocks--) {
			if (0 == (mode & J9_MCMODE_NO_SCAN ) ) {
				memoryCheck_describe_block(portLib, "port_shutdown_library", blockHeader);
			} else {
				UDATA size = blockHeader->wrappedBlockSize < 32 ? blockHeader->wrappedBlockSize : 32;
				portLib->tty_printf(portLib, "Wrapped block size is %d, allocation number is %d\n",
						blockHeader->wrappedBlockSize, blockHeader->allocationNumber);

				if (blockHeader->node) {
					portLib->tty_printf(portLib, "Block was allocated by %s\n", blockHeader->node->callSite);
				}
				portLib->tty_printf(portLib, "First %d bytes:\n", size);
				memoryCheck_dump_bytes(portLib, blockHeader->wrappedBlock, size);
			}
			blockHeader = blockHeader->previousBlock;
		}
	}
		
	/* If an AVL Tree was created free the memory for the tree */
	if(avl_tree) {
		if (mode & J9_MCMODE_PRINT_CALLSITES) {
			/* Report callsite informtation */
			memoryCheck_dump_callSites(portLib, avl_tree);
		} else if (mode & J9_MCMODE_PRINT_CALLSITES_SMALL) {
			/* Report callsite informtation */
			memoryCheck_dump_callSites_small(portLib, avl_tree);
		}
		/* Free the avl tree and nodes*/
		memoryCheck_free_AVLTree(portLib, avl_tree);
	}

	memoryCheck_print_stats(portLib);
	
	if (memStats.totalBlocksAllocated != (memStats.totalBlocksFreed + ignored))  {
		portLib->tty_printf(portLib, "%d allocated blocks totaling %llu bytes were not freed before shutdown!\n",
			memStats.totalBlocksAllocated - memStats.totalBlocksFreed,
			memStats.totalBytesAllocated - memStats.totalBytesFreed);
		if( shutdownMode == J9_MEMCHECK_SHUTDOWN_EXIT ) {
			portLib->tty_printf( portLib, "The VM terminated due to exit() so unfreed blocks are expected.\n" );
		}
	} else  {
		portLib->tty_printf(portLib, "All allocated blocks were freed.\n");
	}

	return;
}

/* Does a final scan (if it was not called by memoryCheck_exit_shutdown_and_exit
 * and prints the stats.
 * Finally, it shuts down the passed in portlibrary (which should be the memCheckPortLib)
 * 
 * @internal The portLib should be the memCheckPortLib */
static void 
memoryCheck_shutdown_internal(OMRPortLibrary *portLib, I_32 shutdownMode)
{
	omrthread_t thread = NULL;

	/* Make sure we are attached, so that we can enter/exit/destroy the mutex */
	if (f_omrthread_attach_ex(&thread, J9THREAD_ATTR_DEFAULT)) {
		return;
	}
	
	MUTEX_ENTER(mcMutex);
	/* Disable trace to avoid deadlock between mcMutex and trace global lock. JTC-JAT 93458 */
	if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
		uteInterface->server->DisableTrace(UT_DISABLE_THREAD);
	}

	if ( mode & J9_MCMODE_MPROTECT ) {
		/* Unlock ALL the blocks */
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentBlock, unlockMode, 0 );
		memoryCheck_lockAllBlocks( memCheckPortLib, mostRecentFreedBlock, unlockMode, 1 );
	}
		
	memoryCheck_print_summary(portLib, shutdownMode);

	/* If the VM is terminating due to exit, the trace engine and other bits of 
	 * the VM may still be operating - so we can't tear their storage out from 
	 * under them.
	 */
	if ( shutdownMode != J9_MEMCHECK_SHUTDOWN_EXIT && mode & J9_MCMODE_MPROTECT ) {
		/* Free all the blocks remaining in the hashTable and then free the Table itself*/
		hashTableForEachDo( vmemIDTable, memoryCheck_hashDoFn, memCheckPortLib );
		hashTableFree( vmemIDTable );
	}

	memCheckPortLib->port_shutdown_library(memCheckPortLib);
	
	/* Port library has shut down, don't attempt to re-enable trace at this point */
	MUTEX_EXIT(mcMutex);
	MUTEX_DESTROY(mcMutex);
	
	f_omrthread_detach(thread);
}


/* Restore the original shutdown functions */
static void 
memoryCheck_restore_old_shutdown_functions(OMRPortLibrary *portLib)
{

	portLib->port_shutdown_library = old_port_shutdown_library;
	portLib->exit_shutdown_and_exit = old_shutdown_and_exit;
}


static I_32 
memoryCheck_control(OMRPortLibrary *portLib, const char* key, UDATA value)
{
	if (0 == strcmp(key, "MEMCHECK")) {
		UDATA everythingOkay;
		MUTEX_ENTER(mcMutex);
		/* Disable trace to avoid deadlock between mcMutex and trace global lock. JTC-JAT 93458 */
		if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
			uteInterface->server->DisableTrace(UT_DISABLE_THREAD);
		}

		everythingOkay = memoryCheck_scan_all_blocks(memCheckPortLib);

		if (!everythingOkay)  {
			memoryCheck_abort(memCheckPortLib);
		}
	
		if ((NULL != uteInterface) && (NULL != uteInterface->server)) {
			uteInterface->server->EnableTrace(UT_ENABLE_THREAD);
		}	
		MUTEX_EXIT(mcMutex);

		return 1;
	} else {
		if (0 == strcmp(key, J9PORT_CTLDATA_TRACE_START) && value) {
			MUTEX_ENTER(mcMutex);
			uteInterface = (UtInterface*) value;
			MUTEX_EXIT(mcMutex);
		} else if (0 == strcmp(key, J9PORT_CTLDATA_TRACE_STOP)) {
			MUTEX_ENTER(mcMutex);
			uteInterface = NULL;
			MUTEX_EXIT(mcMutex);
		}

		return old_port_control(portLib, key, value);
	}
}



/* This function will print the stats for the given node and then
  * recurse over its children.
  *
  * @param portLibrary 
  * @param node J9MEMAVLTreeNode to print callSite information for
  * @internal	The portLibrary passed in should be the memCheckPortLib->
  *
  */
static void 
memoryCheck_dump_callSite(OMRPortLibrary *portLibrary, J9AVLTreeNode *node)
{
	J9AVLTreeNode *nodePtr = AVL_GETNODE(node);

	if (nodePtr == NULL) {
		return;
	}

	memoryCheck_print_stats_callSite(portLibrary, ((J9MEMAVLTreeNode *)nodePtr));
	memoryCheck_set_AVLTree_prevStats(((J9MEMAVLTreeNode *)nodePtr));
	
	memoryCheck_dump_callSite(portLibrary, J9AVLTREENODE_LEFTCHILD(nodePtr));
	memoryCheck_dump_callSite(portLibrary, J9AVLTREENODE_RIGHTCHILD(nodePtr));
}


/* This function will print the header for the callSite information and 
  * call memoryCheck_dump_callSite on the root node of the tree
  *
  * @param portLibrary 
  * @param tree J9AVLTree storing the callSite information
  *
  */
static void 
memoryCheck_dump_callSites(OMRPortLibrary *portLibrary, J9AVLTree *tree)
{
	if( !(tree) || !(tree->rootNode))	{
		return;
	}

	portLibrary->tty_printf(portLibrary, " total alloc   | total freed   | delta alloc   | delta freed   | high water	| largest\n");
	portLibrary->tty_printf(portLibrary, " blocks| bytes | blocks| bytes | blocks| bytes | blocks| bytes | blocks| bytes | bytes | num   | callsite\n");
	portLibrary->tty_printf(portLibrary,
		"-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+------------\n");
	
	memoryCheck_dump_callSite(portLibrary, tree->rootNode);
	portLibrary->tty_printf(portLibrary,
		"-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+------------\n");
}


/*
 * This function will start the recursive freeing of the nodes and the free
 * the memory for the AVL Tree
 *
 * @parm portLib OMRPortLibrary used to access the memory functions
 * @param tree J9AVLTree storing the callSite information
 * @internal	The portLibrary passed in should be the memCheckPortLib.
 */
static void 
memoryCheck_free_AVLTree(OMRPortLibrary *portLib, J9AVLTree *tree)
{
	if(!tree) {
		return;
	}
	
	memoryCheck_free_AVLTreeNode(portLib, tree->rootNode);

	portLib->mem_free_memory( portLib, tree );
}


/*
 * This function will recurse over the left and right child of the given
 * node and then free the memory for itself
 *
 * @parm portLib OMRPortLibrary used to access the memory functions
 * @param tree J9AVLTreeNode the node to free
 * @internal	The portLibrary passed in should be the memCheckPortLib.
 */
static void 
memoryCheck_free_AVLTreeNode(OMRPortLibrary *portLib, J9AVLTreeNode *node)
{
	J9AVLTreeNode *nodePtr = AVL_GETNODE(node);
	
	if (nodePtr == NULL) {
		return;
	}

	memoryCheck_free_AVLTreeNode(portLib, J9AVLTREENODE_LEFTCHILD(nodePtr));
	memoryCheck_free_AVLTreeNode(portLib, J9AVLTREENODE_RIGHTCHILD(nodePtr));

	portLib->mem_free_memory( portLib, nodePtr );
}


/*
 * This function is the insertion comparator for the J9AVLTree that stores J9MEMAVLTreeNodes
 * It will compare the char * of both nodes.
 *
 * @param tree J9ALVTree storing the callSite information
 * @param insertNode The node which will be inserted into the tree
 * @param walk The current search position in the tree
 *
 * @return IDATA the difference between the two strings alphabetically
 *
 */
static IDATA 
memoryCheck_insertion_Compare(J9AVLTree *tree, J9AVLTreeNode *insertNode, J9AVLTreeNode *walk) 
{
	return strcmp(((J9MEMAVLTreeNode *)insertNode)->callSite, ((J9MEMAVLTreeNode *)walk)->callSite);
}


/*
 * This function prints the deatils of the callSite
 *
 * @param portLib OMRPortLibrary used to handle printing
 * @param node The node to print the stats for
 *
 */
static void 
memoryCheck_print_stats_callSite(OMRPortLibrary *portLib, J9MEMAVLTreeNode *node)
{
	portLib->tty_printf(portLib, "%7u %7llu %7u %7llu %7u %7lld %7u %7lld %7u %7u %7u %7u %s\n",
						node->stats->totalBlocksAllocated,
						node->stats->totalBytesAllocated,
						node->stats->totalBlocksFreed,
						node->stats->totalBytesFreed,
						node->stats->totalBlocksAllocated - node->prevStats->totalBlocksAllocated,
						node->stats->totalBytesAllocated - node->prevStats->totalBytesAllocated,
						node->stats->totalBlocksFreed - node->prevStats->totalBlocksFreed,
						node->stats->totalBytesFreed - node->prevStats->totalBytesFreed,
						node->stats->hiWaterBlocksAllocated,
						node->stats->hiWaterBytesAllocated,
						node->stats->largestBlockAllocated,
						node->stats->largestBlockAllocNum,
						node->callSite);
}



/*
 * This function is the search comparator for the J9AVLTree that stores J9MEMAVLTreeNodes
 * It will compare the char * with the node callSite.
 *
 * @param tree J9ALVTree storing the callSite information
 * @param search The char * to search for
 * @param walk The current search position in the tree
 *
 * @return IDATA the difference between the two strings alphabetically
 *
 */
static IDATA 
memoryCheck_search_Compare(J9AVLTree *tree, UDATA search, J9AVLTreeNode *walk) 
{
	return strcmp((char *)search, ((J9MEMAVLTreeNode *)walk)->callSite);
}


/*
 * This function initializes both node->stats and node->prevStats.  All 
 * values for prevStats should be initialized to 0 since at this point there have
 * been no previous stats.
 *
 * @param node J9MEMAVLTreeNode to initialize stats for
 * @param byteAmount used to initialize bytesAllocated for node->stats only
 *
 */
static void
memoryCheck_initialize_AVLTree_stats(J9MEMAVLTreeNode *node, UDATA byteAmount)
{
	node->stats->totalBlocksAllocated = 1;
	node->stats->totalBlocksFreed = 0;
	node->stats->totalBytesAllocated = byteAmount;
	node->stats->totalBytesFreed = 0;
	node->stats->totalUnknownBlocksIgnored = 0;
	node->stats->largestBlockAllocated = byteAmount;
	node->stats->largestBlockAllocNum = 1;
	node->stats->currentBlocksAllocated = 1;
	node->stats->hiWaterBlocksAllocated = 1;
	node->stats->currentBytesAllocated = byteAmount;
	node->stats->hiWaterBytesAllocated = byteAmount;	
	node->stats->failedAllocs = 0;

	node->prevStats->totalBlocksAllocated = 0;
	node->prevStats->totalBlocksFreed = 0;
	node->prevStats->totalBytesAllocated = 0;
	node->prevStats->totalBytesFreed = 0;
	node->prevStats->totalUnknownBlocksIgnored = 0;
	node->prevStats->largestBlockAllocated = 0;
	node->prevStats->largestBlockAllocNum = 0;
	node->prevStats->currentBlocksAllocated = 0;
	node->prevStats->hiWaterBlocksAllocated = 0;
	node->prevStats->currentBytesAllocated = 0;
	node->prevStats->hiWaterBytesAllocated = 0;	
	node->prevStats->failedAllocs = 0;
}



/*
 * This function inserts or updates the node pertaining to the newly allocated memory block.
 *
 * @param portLib OMRPortLibrary used for memory functions
 * @param header Memory block header which will point to the node
 * @param callSite Char * to the location that called the allocation function
 * @param byteAmount Number of bytes allocated for the memory block
 *
 */
static void
memoryCheck_update_callSites_allocate(OMRPortLibrary *portLib, J9MemoryCheckHeader *header, const char *callSite, UDATA byteAmount)
{
	J9MEMAVLTreeNode *node;

	node = (J9MEMAVLTreeNode *)avl_search(avl_tree, (UDATA)callSite);
	if(node) {
		node->stats->totalBlocksAllocated++;
		node->stats->totalBytesAllocated += byteAmount;
		node->stats->currentBlocksAllocated += 1;
		if (node->stats->currentBlocksAllocated > node->stats->hiWaterBlocksAllocated) {
			node->stats->hiWaterBlocksAllocated = node->stats->currentBlocksAllocated;
		}
		node->stats->currentBytesAllocated += byteAmount;
		if (node->stats->currentBytesAllocated > node->stats->hiWaterBytesAllocated) {
			node->stats->hiWaterBytesAllocated = node->stats->currentBytesAllocated;
		}
		if (byteAmount > node->stats->largestBlockAllocated) {
			node->stats->largestBlockAllocated = byteAmount;
			node->stats->largestBlockAllocNum = node->stats->totalBlocksAllocated;
		}
		header->node = node;
	} else {
		/* allocate enough room for the node, char *, and two J9MemoryCheckStats */
		node = old_mem_allocate_memory(portLib, sizeof(J9MEMAVLTreeNode) + strlen(callSite) + 1 +
										sizeof(J9MemoryCheckStats) + sizeof(J9MemoryCheckStats), J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM);
		if(node) {
			node->parentAVLTreeNode.leftChild = 0;
			node->parentAVLTreeNode.rightChild = 0;
			node->stats = (J9MemoryCheckStats *)(node + 1);
			node->prevStats = (J9MemoryCheckStats *)((UDATA)(node->stats) + sizeof(J9MemoryCheckStats));

			memcpy((char *)((UDATA)(node->prevStats) + sizeof(J9MemoryCheckStats)), callSite, strlen(callSite) + 1);
			node->callSite = (char *)((UDATA)(node->prevStats) + sizeof(J9MemoryCheckStats));

			memoryCheck_initialize_AVLTree_stats(node, byteAmount);

			header->node = node;

			avl_insert(avl_tree, (J9AVLTreeNode *)node);
		}
	}
}


/*
 * This function sets the node->prevStats to the current value of
 * node->stats
 *
 * @param node J9MEMAVLTreeNode to initialize stats for
 *
 */
static void 
memoryCheck_set_AVLTree_prevStats(J9MEMAVLTreeNode *node)
{
	node->prevStats->totalBlocksAllocated = node->stats->totalBlocksAllocated;
	node->prevStats->totalBlocksFreed = node->stats->totalBlocksFreed;
	node->prevStats->totalBytesAllocated = node->stats->totalBytesAllocated;
	node->prevStats->totalBytesFreed = node->stats->totalBytesFreed;
	node->prevStats->largestBlockAllocated = node->stats->largestBlockAllocated;
	node->prevStats->largestBlockAllocNum = node->stats->largestBlockAllocNum;
	node->prevStats->currentBlocksAllocated = node->stats->currentBlocksAllocated;
	node->prevStats->hiWaterBlocksAllocated = node->stats->hiWaterBlocksAllocated;
	node->prevStats->currentBytesAllocated = node->stats->currentBytesAllocated;
	node->prevStats->hiWaterBytesAllocated = node->stats->hiWaterBytesAllocated;	
}


/*
 * This function prints the deatils of the callSite.  This only 
 * prints the total/delta alloc/free information.
 *
 * @param portLib OMRPortLibrary used to handle printing
 * @param node The node to print the stats for
 *
 */
static void 
memoryCheck_print_stats_callSite_small(OMRPortLibrary *portLib, J9MEMAVLTreeNode *node)
{
	portLib->tty_printf(portLib, "%7u %7llu %7u %7llu %7u %7llu %7u %7llu %s\n",
						node->stats->totalBlocksAllocated,
						node->stats->totalBytesAllocated,
						node->stats->totalBlocksFreed,
						node->stats->totalBytesFreed,
						node->stats->totalBlocksAllocated - node->prevStats->totalBlocksAllocated,
						node->stats->totalBytesAllocated - node->prevStats->totalBytesAllocated,
						node->stats->totalBlocksFreed - node->prevStats->totalBlocksFreed,
						node->stats->totalBytesFreed - node->prevStats->totalBytesFreed,
						node->callSite);
}


/* This function will print the header for the callSite information and 
  * call memoryCheck_dump_callSite on the root node of the tree
  *
  * @param portLibrary 
  * @param tree J9AVLTree storing the callSite information
  * @internal	The portLibrary passed in should be the memCheckPortLib->
  *
  */
static void 
memoryCheck_dump_callSites_small(OMRPortLibrary *portLibrary, J9AVLTree *tree)
{
	if(!(tree) || !(tree->rootNode)) {
		return;
	}

	portLibrary->tty_printf(portLibrary, " total alloc   | total freed   | delta alloc   | delta freed\n");
	portLibrary->tty_printf(portLibrary, " blocks| bytes | blocks| bytes | blocks| bytes | blocks| bytes | callsite\n");
	portLibrary->tty_printf(portLibrary,
		"-------+-------+-------+-------+-------+-------+-------+-------+-----------\n");
	
	memoryCheck_dump_callSite_small(portLibrary, tree->rootNode);
	portLibrary->tty_printf(portLibrary,
		"-------+-------+-------+-------+-------+-------+-------+-------+-----------\n");
}



/* This function will print the stats (only the total/delta for alloc/free) for the given node and then
  * recurse over its children.
  *
  * @param portLibrary 
  * @param node J9MEMAVLTreeNode to print callSite information for
  * @internal	The portLibrary passed in should be the memCheckPortLib.
  */
static void 
memoryCheck_dump_callSite_small(OMRPortLibrary *portLibrary, J9AVLTreeNode *node)
{
	J9AVLTreeNode *nodePtr = AVL_GETNODE(node);
	
	if (nodePtr == NULL) {
		return;
	}

	memoryCheck_print_stats_callSite_small(portLibrary, ((J9MEMAVLTreeNode *)nodePtr));
	memoryCheck_set_AVLTree_prevStats(((J9MEMAVLTreeNode *)nodePtr));
	
	memoryCheck_dump_callSite_small(portLibrary, J9AVLTREENODE_LEFTCHILD(nodePtr));
	memoryCheck_dump_callSite_small(portLibrary, J9AVLTREENODE_RIGHTCHILD(nodePtr));
}


/*
 * This function will update the node to account for the memory block 
 * which was just freed.
 *
 * @param node J9MEMAVLTreeNode that will be updated
 * @param byteAmount Number of bytes allocated for the memory block
 *
 */
static void 
memoryCheck_update_callSites_free(J9MEMAVLTreeNode *node, UDATA byteAmount)
{
	if(node) {
		node->stats->totalBlocksFreed++;
		node->stats->totalBytesFreed+=byteAmount;
		node->stats->currentBlocksAllocated--;
		node->stats->currentBytesAllocated-=byteAmount;
	}
}


/**
  * @internal The portLibrary passed on will be the memCheckPortLibrary with its original functions
  */
static void * 
memoryCheck_reallocate_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer, UDATA byteAmount, const char * callsite, U_32 category)
{
#ifdef DEBUG
	memCheckPortLib->tty_printf( memCheckPortLib, "reallocate_memory(%p, %d)\n", memoryPointer, byteAmount);
#endif
	return memoryCheck_wrapper_reallocate_memory(memCheckPortLib, memoryPointer, byteAmount, "reallocate_memory", globalAllocator, globalDeallocator,
		J9_MEMCHECK_DATA_PADDING_VALUE, J9_MEMCHECK_DATA_FILL_VALUE, J9_MEMCHECK_DATA_FREED_VALUE, NULL == callsite ? "unknown" : callsite, category);
}



/* @internal The portLib passed in should be the memCheckPortLib */
static void *
memoryCheck_wrapper_reallocate_memory(OMRPortLibrary *portLib, void *memoryPointer, UDATA byteAmount, char const *operationName,
											J9_MEM_ALLOCATE_FUNC allocator, J9_MEM_FREE_FUNC deallocator, U_32 paddingValue, U_32 fillValue,
											U_32 freedValue, const char *callSite, U_32 category)
{

	/* It doesn't really make sense to use realloc with memoryCheck since it would try to re-use the 
	 * pointer which would hide most realloc bugs so we will just emulate its behaviour so we get
	 * the rigorous memoryCheck verifications. 
	 */
	void *newBlock = NULL;

	if (memoryPointer == NULL) {
		newBlock = memoryCheck_wrapper_allocate_memory(portLib, byteAmount, operationName, allocator, paddingValue, fillValue, freedValue, "unknown", category);
	} else if (byteAmount == 0) {
		memoryCheck_wrapper_free_memory(portLib, memoryPointer, operationName, deallocator, paddingValue, fillValue, freedValue);
		newBlock = NULL;
	} else {
		newBlock = memoryCheck_wrapper_allocate_memory(portLib, byteAmount, operationName, allocator, paddingValue, fillValue, freedValue, "unknown", category);

		if (newBlock != NULL) {
			U_8 *wrappedBlock = NULL;
			J9MemoryCheckHeader *topHeader = NULL;
			UDATA oldByteAmount = 0;
			UDATA bytesToCopy = 0;
			
			/* Sniff out the size of the actual wrapped block */
			wrappedBlock = (U_8 *) memoryPointer;
			if (0 == (mode & J9_MCMODE_NO_SCAN) ) {
				if ( mode & J9_MCMODE_MPROTECT ) {
					topHeader = (J9MemoryCheckHeader *)(wrappedBlock - J9_MEMCHECK_PAGE_SIZE);
					if ( ( ((UDATA)topHeader & 0XF000) != ((UDATA)((U_8 *)topHeader + sizeof(J9MemoryCheckHeader)) & 0XF000) ) ) {
						if ( (UDATA)((U_8*)topHeader + sizeof(J9MemoryCheckHeader)) % J9_MEMCHECK_PAGE_SIZE != 0 ) {
							topHeader = (J9MemoryCheckHeader *)((U_8 *)topHeader - sizeof(J9MemoryCheckHeader));
						}
					}
				} else {
					UDATA topPaddingSize = J9_MEMCHECK_ADJUSTED_PADDING - sizeof(J9MemoryCheckHeader);
					topHeader = (J9MemoryCheckHeader *) (wrappedBlock - topPaddingSize - sizeof(J9MemoryCheckHeader));
				}
			} else {
				topHeader = (J9MemoryCheckHeader *) (wrappedBlock - sizeof(J9MemoryCheckHeader));
			}

			J9_MEMCHECK_UNLOCK( topHeader);
			oldByteAmount = topHeader->wrappedBlockSize;
			J9_MEMCHECK_LOCK( topHeader);

			/* Copy the existing data to the new block */
			bytesToCopy = (oldByteAmount <= byteAmount) ? oldByteAmount : byteAmount;
			memcpy(newBlock, memoryPointer, bytesToCopy);

			/* Free the old block */
			memoryCheck_wrapper_free_memory(portLib, memoryPointer, operationName, deallocator, paddingValue, fillValue, freedValue);
		}
	}
	/* Return the new pointer */
	return newBlock;
}




/*
 * This function is used after memcheck has shut down to prevent crashes
 */
static void 
memoryCheck_null_mem_free_memory(OMRPortLibrary *portLib, void *memoryPointer)
{
	return;
}

#ifdef MEMDEBUG
static void 
subAllocator_audit(OMRPortLibrary *portLib, UDATA mask)
{
	UDATA top = STARTIDX;
	UDATA bottom;
	UDATA i;

	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	freemem = 0;
	usedmem = 0;
	total = 0;
	freeCnt = 0;
	usedCnt = 0;

	bucket[0] = bucket[1] = bucket[2] = bucket[3] = bucket[4] = bucket[5] = 0;

	while (top < heapsize-2) {

		bottom = top + abs(j9heap[top]);
		pTop = &j9heap[top];
		pBot = &j9heap[bottom];

		if (j9heap[top] != j9heap[bottom]) {
			total = freemem + usedmem;
			break;
		}

		if (j9heap[top] > 0) {
			freemem += (j9heap[top] + 1);
			if (mask & FREE) {
				j9tty_printf(NULL, "FR: %7d		@ %6d ", j9heap[top], top);
			}
			freeCnt++;
		} else if (j9heap[top] < 0) {
			usedmem += (-j9heap[top] + 1);
			if (mask & USED) {
				j9tty_printf(NULL, "US:		%7d @ %6d ", -j9heap[top], top);
			}
			/* arbitrary values to gauge memory usage; change as needed */
			if (-j9heap[top] < 10)
				bucket[1]++;
			else if (-j9heap[top] < 32)
				bucket[2]++;
			else if (-j9heap[top] < 256)
				bucket[3]++;
			else if (-j9heap[top] < 2048)
				bucket[4]++;
			else 
				bucket[5]++;

			usedCnt++;
		}

		top = abs(j9heap[top]) + top + 1;
	}
	total = freemem + usedmem;

	for (i=0; i<MAX_SMALL_BLOCK; i++) {
		if (smblkstatus[i] != 0)
			bucket[0]++;
	}

	if (mask & AUDIT)
	j9tty_printf(NULL, "AUDIT: F %d  U %d  FC %d  UC %d", freemem, usedmem, freeCnt, usedCnt);

	if (mask & BUCKET) {
		j9tty_printf(NULL, "BUCKET: [%d], %d, %d, %d, %d %d",
						bucket[0], bucket[1], bucket[2], bucket[3], bucket[4], bucket[5]);
	}
}
#endif

/**
 * Initialize the heap, providng a pointer to the heap space and the size of the heap
 * These two values can be hard coded at this point, if desired.
 * @param[in] void* memptr -- pointer to block of memory used as a heap, or arbitrary size
 * @param[in] UDATA size -- size of heap block, in number of words
 */
static void 
subAllocator_init_heap(void* memptr, UDATA size)
{
	UDATA *j9heap_local = NULL;

	j9heap_local = (UDATA *) memptr;
	memset(j9heap_local, 0, sizeof(UDATA) * size);
	heapsize = size;
	
	/* "Top", at index=2 is 0 */
	j9heap_local[STARTIDX-1] = 0;
	
	/* Indicate the size of the initial block, at index = 3*/
	j9heap_local[STARTIDX] = heapsize-(STARTIDX+2);

	/* Indicate the size of the initial starting block at the second last slot of j9heap */
	j9heap_local[heapsize-2] = heapsize-(STARTIDX+2);
	
	/* Place a '0' at the end of the heap */
	j9heap_local[heapsize-1] = 0;
	start = STARTIDX;

	meminuse = 0;

	memset(smallBlock, 0, sizeof(smallBlock));
	memset(smblkstatus, 0, sizeof(smblkstatus));
	smblkindex = 0;
}


static UDATA 
subAllocator_findFirstFreeBlock(UDATA index, IDATA *j9heap)
{
	IDATA val = j9heap[index];
	UDATA maxIndex = heapsize-2;

	/* Logically: abs(j9heap[index]), but using IDATA width types */
	if (val < 0) {
		val *= -1;
	}

	index += (val + 1);

	while (index < maxIndex) {

		if (j9heap[index] > 0) {
			return index;
		}
		index = -j9heap[index] + index + 1;
	}
	return STARTIDX;
}

/*
	Release the memory block (if valid). If part of the smallBlock array, return it
	If in heap, coalesce with adjacent free blocks as necessary
*/

static void
subAllocator_free_memory(OMRPortLibrary *portLib, void *ptr)
{
	UDATA top;
	UDATA newtop;
	UDATA bottom;
	UDATA newval;

	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	 
	if ((ptr < (void*) &j9heap[0]) || (ptr > (void*) &j9heap[heapsize-2])) {
		if ((ptr < (void*) &smallBlock[0]) || (ptr > (void*) &smallBlock[MAX_SMALL_BLOCK - 1])) {
			return;
		}
		else
		{
			top = (((UDATA) ptr - (UDATA) &smallBlock[0]) / sizeof(smallBlock[0]));
			if (smblkstatus[top]) {
				smblkstatus[top] = 0;
				smblkindex = top;
				/*MSG_FATAL("SMBLK- %d @ %x", top, ptr, 0);*/
				return;
			}
			else
				/*error*/;
		}
	}
	top = (((UDATA) ptr - (UDATA) &j9heap[0]) / sizeof(UDATA)) - 1;

/*	if (-(j9heap[top]) > 256) 
		MSG_FATAL("j9 free   %7d bytes @ %8X", (-j9heap[top]-1)*4, &j9heap[top+1], 0);*/
#ifdef MEMDEBUG
	pTop = &j9heap[top];
# endif
	if (j9heap[top] < 0) {
		bottom = top + -(j9heap[top]);

#ifdef MEMDEBUG
		pBot = &j9heap[bottom];
#endif
		if (j9heap[top] == j9heap[bottom]) {
			j9heap[top] = -j9heap[top];
			j9heap[bottom] = -j9heap[bottom];

			/* start is a signed quantity */
			if ((IDATA)top < start)
				start = top;

			meminuse -= ((j9heap[top]+1)*sizeof(UDATA));

			if (j9heap[top-1] > 0) {
				/* merge blocks */
				newtop = top - j9heap[top-1] - 1; 
				j9heap[newtop] = j9heap[top] + j9heap[top-1] + 1;
				j9heap[bottom] = j9heap[newtop];

				/* start is a signed quantity */
				if ((IDATA)newtop < start)
					start = newtop;
			}

			if (j9heap[bottom+1] > 0) {
				newval = j9heap[bottom] + j9heap[bottom+1] + 1;
				j9heap[bottom + j9heap[bottom + 1] + 1 ] = newval;
				newtop = bottom - j9heap[bottom];
				j9heap[ newtop ] = newval;

				/* start is a signed quantity */
				if ((IDATA)newtop < start)
					start = newtop;
			}
#ifdef MEMDEBUG
			subAllocator_audit(portLib, j9heap, AUDIT|BUCKET);
#endif
			return;
		}
	}
	start = STARTIDX;
}

/* 
	allocate a block of size bytes from the heap, using a minimum of FRAGTHRESHOLD words
*/

static void*
subAllocator_allocate_memory(OMRPortLibrary *portLib, UDATA byteAmount, const char *callSite, U_32 category)
{
	IDATA size = (IDATA) byteAmount;
	UDATA idx;
	const UDATA maxIndex = heapsize-2;
	UDATA newstart;

	/* round to even heap slot (32/64 bit amount) */
	size = ((size + (sizeof(UDATA)-1)) & ~(sizeof(UDATA)-1)) / sizeof(UDATA);

	if (size <= FRAGTHRESHOLD)
	{
		UDATA idx = smblkindex;
		while (idx < MAX_SMALL_BLOCK) {
			if (0 == smblkstatus[idx])
			{
				memset(&smallBlock[idx], 0, sizeof(smallBlock[idx]));
				smblkstatus [idx] = 1;
				/*MSG_FATAL("SMBLK+ %d @ %x", idx, &smallBlock[idx], 0);*/
				return &smallBlock[idx];
			}
			else
			{
				if (++idx >= MAX_SMALL_BLOCK)
					idx = 0;
				else if (idx == smblkindex)
					break;
			}
		}
	}

#ifdef MEMDEBUG
	subAllocator_audit(portLib, j9heap, AUDIT|BUCKET);
#endif

	idx = start;

	while (idx < maxIndex)
	{
		if (j9heap [idx] >= size + 1) {
	
			if (j9heap[idx] > size + FRAGTHRESHOLD ) {
				j9heap[idx + j9heap[idx]] -= (size + 2);
				newstart = idx + size + 2;
				j9heap[newstart] = j9heap[idx + j9heap[idx]];
				if (start == idx)
					start = newstart;
			}
			else {
				/* current block too small to split, use whole thing */
				size = j9heap[idx] - 1;
				if (start == idx)
					start =subAllocator_findFirstFreeBlock(idx, (IDATA *)j9heap);
			}
	
#ifdef MEMDEBUG
			pTop = &j9heap[idx];
			pBot = &j9heap[idx + size + 1];
#endif
	
			j9heap[idx + size + 1] = -(size + 1);
			j9heap[idx] = -(size + 1);
			memset(&j9heap[idx+1], 0, size*sizeof(UDATA));

			meminuse += ((size+1)*sizeof(UDATA));
	
#ifdef MEMDEBUG
			if (meminuse > (((heapsize/sizeof(UDATA))*3)*sizeof(UDATA))) {
				subAllocator_audit(portLib, j9heap, AUDIT|BUCKET|FREE); 
			}
#endif
/*			if (size > 256) 
				MSG_FATAL("j9 malloc %7d bytes @ %8X", size*4, &j9heap[idx+1], 0);*/
			return &j9heap[idx+1];
		}
		else {
			IDATA absoluteIdx = j9heap[idx];
			if (absoluteIdx < 0) {
				absoluteIdx *= -1;
			}
			idx = absoluteIdx + idx + 1;
		}
	}
#ifdef MEMDEBUG
	subAllocator_audit(portLib, j9heap, AUDIT|FREE|BUCKET|USED);
#endif
/*	MSG_FATAL("j9 malloc fail, sz %d", size*4, 0, 0); */
	return 0;
}
