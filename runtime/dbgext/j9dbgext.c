/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "j9dbgext.h"

#include "j9consts.h"
#include "j9cp.h"
#include "j9port.h"
#include "stackwalk.h"
#include "util_api.h"


/* Note: must be 8-aligned */
typedef struct dbgNode {
	struct dbgNode * link;
	void * originalAddress;
	void * allocatedAddress;
	UDATA size;
	UDATA relocated;
} dbgNode;

dbgNode * memoryList = NULL;

#ifdef J9VM_ENV_DATA64
static struct {
	J9PortVmemIdentifier vmemIdentifier;
	UDATA size;
	void* basePointer;
	void* currentPointer;
	void* commitPointer;
} smallDbgAddressSpace;
#endif

J9JavaVM* cachedVM = NULL;

/* old file_write function */
static IDATA (*old_write)(OMRPortLibrary *portLibrary, IDATA fd, const void * buf, IDATA nbytes);

/* error handler */
static DBG_JMPBUF * dbgErrorHandler;

/* verbose mode */
static int dbgVerboseMode = 1;

static int testJavaVMPtr (J9JavaVM* ptr);
static IDATA dbg_write (OMRPortLibrary *portLibrary, IDATA fd, const void * buf, IDATA nbytes);
#if defined(J9ZOS390)
static IDATA dbg_write_text (OMRPortLibrary *portLibrary, IDATA fd, const char * buf, IDATA nbytes);
#endif

static IDATA hexValue(char c);

#define READU(field) dbgReadUDATA((UDATA*)&(field))
#define READP(field) ((void*)dbgReadUDATA((UDATA*)&(field)))

#define DBG_ARROW(base, item) dbgReadSlot((UDATA)&((base)->item), sizeof((base)->item))

#define LOCAL_MEMORY_ALIGNMENT 16

void *
dbgMalloc(UDATA size, void *originalAddress)
{
	PORT_ACCESS_FROM_PORT(dbgGetPortLibrary());
	dbgNode * node;
	void * allocatedAddress;

#ifdef J9VM_ENV_DATA64
	/* On 64 bit platforms, use a simple allocation scheme which keeps all pointers within 2GB of each other.
	 * This guarantees that SRP relocations between two allocated blocks will always be valid.
	 * See CMVC defect 94818
	 */

	/* initialize the memory region */
	if (smallDbgAddressSpace.basePointer == NULL) {
		UDATA pageSize = j9vmem_supported_page_sizes()[0];

		if (smallDbgAddressSpace.size == 0) {
			char envBuf[16];

			/* use a default of 1G, since 2G is too much for most ZOs users to be able to allocate
			 * (see CMVC 122069) */
			smallDbgAddressSpace.size = (U_64)1 * 1024 * 1024 * 1024;

			if (0 == j9sysinfo_get_env("J9DBGEXT_SCRATCH_SIZE", envBuf, sizeof(envBuf))) {
				if (strlen(envBuf) > 0) {
					smallDbgAddressSpace.size = (U_64)atol(envBuf) * 1024 * 1024;
					dbgPrint("\nEnvironment variable J9DBGEXT_SCRATCH_SIZE set, requested size is %zu MB\n", smallDbgAddressSpace.size / (1024 * 1024));
				}
			}
		} else if (smallDbgAddressSpace.size == (UDATA)-1) {
			return NULL;
		}

		smallDbgAddressSpace.basePointer = j9vmem_reserve_memory(
			NULL,
			smallDbgAddressSpace.size,
			&smallDbgAddressSpace.vmemIdentifier,
			J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE,
			pageSize,
			OMRMEM_CATEGORY_VM);
		if (smallDbgAddressSpace.basePointer == NULL) {
			dbgError("\nError: Unable to allocate debug scratch space (%zu MB).\n"
				"Try setting the J9DBGEXT_SCRATCH_SIZE environment variable to a smaller value.\n",
				smallDbgAddressSpace.size / (1024 * 1024));
			smallDbgAddressSpace.size = (UDATA)-1;
			/* This is fatal, any further dbgMalloc() calls would fail, so just exit */
			exit(1);
		}
		smallDbgAddressSpace.currentPointer = smallDbgAddressSpace.commitPointer = smallDbgAddressSpace.basePointer ;
	}

	/* check for overflow */
	if ( ( size > smallDbgAddressSpace.size - (sizeof(dbgNode) + LOCAL_MEMORY_ALIGNMENT) ) ||
		((void *)((U_8*)smallDbgAddressSpace.basePointer + smallDbgAddressSpace.size - (size + sizeof(dbgNode))) < smallDbgAddressSpace.currentPointer ) )
	{
		if (dbgVerboseMode) {
			dbgPrint(
				"Unable to allocate requested %zu bytes in debug scratch space (%zu MB).\n",
				size + + LOCAL_MEMORY_ALIGNMENT + sizeof(dbgNode),
				smallDbgAddressSpace.size / (1024 * 1024));
		}
		return NULL;
	}

	/* allocate and round up for alignment */
	allocatedAddress = smallDbgAddressSpace.currentPointer;
	smallDbgAddressSpace.currentPointer = (void*)(((UDATA)allocatedAddress + size + LOCAL_MEMORY_ALIGNMENT + sizeof(dbgNode) + sizeof(U_64) - 1) & ~(sizeof(U_64) - 1));

	/* commit the allocated memory */
	while (smallDbgAddressSpace.commitPointer < smallDbgAddressSpace.currentPointer) {
		UDATA pageSize = j9vmem_supported_page_sizes()[0];

		if (NULL == j9vmem_commit_memory(smallDbgAddressSpace.commitPointer, pageSize, &smallDbgAddressSpace.vmemIdentifier)) {
			return NULL;
		}
		smallDbgAddressSpace.commitPointer = (U_8*)smallDbgAddressSpace.commitPointer + pageSize;
	}

#else
	allocatedAddress = j9mem_allocate_memory(size + LOCAL_MEMORY_ALIGNMENT + sizeof(dbgNode), OMRMEM_CATEGORY_VM);
	if (allocatedAddress == NULL) {
		return NULL;
	}
#endif

	node = allocatedAddress;
	while (((UDATA)(node + 1) & (UDATA)(LOCAL_MEMORY_ALIGNMENT - 1)) != 0) {
		node = (dbgNode *) ((UDATA *) node + 1);
	}
	node->originalAddress = originalAddress;
	node->allocatedAddress = allocatedAddress;
	node->size = size;
	node->link = memoryList;
	node->relocated = 0;

	memoryList = node;

	return node + 1;
}

void
dbgFree(void * addr)
{
	PORT_ACCESS_FROM_PORT(dbgGetPortLibrary());
	dbgNode * node = ((dbgNode *) addr) - 1;

	if (memoryList == node) {
		memoryList = node->link;
#ifndef J9VM_ENV_DATA64
		j9mem_free_memory(node->allocatedAddress);
#endif
	} else {
		dbgNode * current = memoryList;

		while (current) {
			dbgNode * next = current->link;

			if (next == node) {
				current->link = node->link;
#ifndef J9VM_ENV_DATA64
				j9mem_free_memory(node->allocatedAddress);
#endif
				break;
			}
			current = next;
		}
	}

#ifdef J9VM_ENV_DATA64
	if (memoryList == NULL) {
		smallDbgAddressSpace.currentPointer = smallDbgAddressSpace.basePointer ;
	}
#endif
}

void *
dbgLocalToTarget(void * addr)
{
	dbgNode * current = memoryList;

	if (addr == NULL) return NULL;

	while (current) {
		U_8 * lowAddr = (U_8 *) (current + 1);
		U_8 * highAddr = lowAddr + current->size;

		if (((U_8 *)addr >= lowAddr) && ((U_8 *)addr < highAddr))
			return ((U_8*) addr) - lowAddr + ((U_8*) (current->originalAddress));

		current = current->link;
	}

	dbgError("Local memory %p has no mapping to target memory\n", addr);
	return NULL;
}



void *
dbgTargetToLocal(void * addr)
{
	return dbgTargetToLocalWithSize(addr, 0);
}

/*
 * Returns a program space pointer to the J9JavaVM.
 */
J9JavaVM*
dbgSniffForJavaVM(void)
{
	U_8* startFrom = NULL;
	UDATA bytesRead;
	UDATA totalBytesSearched = 0;
	U_8* eyecatcher;

	if (cachedVM) {
		return cachedVM;
	}

#if !defined (J9VM_RAS_EYECATCHERS)
	dbgPrint("RAS not enabled, cannot search for VM -- use setvm instead\n");
	return NULL;
#else
	dbgPrint("Searching for J9JavaVM...\n");

	while (NULL != (eyecatcher = dbgFindPattern((U_8*)"J9VMRAS", sizeof("J9VMRAS"), 8, startFrom, &bytesRead))) {
		J9RAS ras;
		totalBytesSearched += bytesRead;

		dbgReadMemory((UDATA)eyecatcher, &ras, sizeof(ras), &bytesRead);
		if (bytesRead == sizeof(ras)) {
			if (ras.bitpattern1 == 0xaa55aa55 && ras.bitpattern2 == 0xaa55aa55) {
				cachedVM = (J9JavaVM *) ras.vm;
				dbgPrint("Found eyecatcher -- VM set to !setvm 0x%p\n", cachedVM);
				return cachedVM;
			}
		}

		/* this isn't it -- look for the next occurrence */
		startFrom = eyecatcher + 8;
	}
	totalBytesSearched += bytesRead;

	if (totalBytesSearched == 0) {
		/*
		 * The debugger didn't even try -- probably because it's a 64 bit platform and doesn't know how
		 * to find out which memory has been mapped. Try again with a more heavyweight API, but only
		 * in the low 4GB.
		 */
		UDATA bytesToSearch = 0xFFFFFFFF;

		startFrom = NULL;

		while (NULL != (eyecatcher = dbgFindPatternInRange((U_8*)"J9VMRAS", sizeof("J9VMRAS"), 8, startFrom, bytesToSearch, &bytesRead))) {
			J9RAS ras;
			totalBytesSearched += bytesRead;

			dbgReadMemory((UDATA)eyecatcher, &ras, sizeof(ras), &bytesRead);
			if (bytesRead == sizeof(ras)) {
				if (ras.bitpattern1 == 0xaa55aa55 && ras.bitpattern2 == 0xaa55aa55) {
					cachedVM = (J9JavaVM *) ras.vm;
					dbgPrint("Searched %zu bytes -- VM set to !setvm 0x%p\n", totalBytesSearched, cachedVM);
					return cachedVM;
				}
			}

			/* this isn't it -- look for the next occurrence */
			startFrom = eyecatcher + 8;
			if ((UDATA)eyecatcher > 0xFFFFFFFF - 8) {
				bytesToSearch = 0;
			}  else {
				bytesToSearch = 0xFFFFFFFF - (UDATA)startFrom;
			}
		}
		totalBytesSearched += bytesRead;
	}

	if (totalBytesSearched == 0) {
		dbgPrint("Cannot scan for eyecatchers on this platform -- use setvm instead\n");
	} else {
		dbgPrint("Could not locate J9JavaVM (searched %zu bytes)\n", totalBytesSearched);
		dbgPrint("Use setvm if you know (or suspect) the address of the J9JavaVM or a J9VMThread\n");
	}
	return NULL;

#endif
}



/*
 * Returns a program space pointer to the J9JavaVM.
 * Tries to find the VM from ptr. ptr may point to a J9VMThread, a J9JavaVM,
 * or maybe something else.
 */
static int
testJavaVMPtr(J9JavaVM* ptr)
{
	void* bytes = 0;
	UDATA bytesRead = 0;

	/* try to read the reserved slot */
	dbgReadMemory((UDATA)&ptr->reserved1_identifier, &bytes, sizeof(ptr->reserved1_identifier), &bytesRead);
	return ((bytesRead == sizeof(ptr->reserved1_identifier)) && (bytes == (void*)J9VM_IDENTIFIER));
}



U_8 dbgReadByte(U_8 * remoteAddress)
{
	U_8 byte = 0;
	UDATA bytesRead = 0;

	dbgReadMemory((UDATA) remoteAddress, &byte, 1, &bytesRead);
	if (bytesRead != 1) dbgError("could not read byte at %p\n", remoteAddress);
	return byte;
}


void
dbgFreeAll(void)
{
	while (memoryList) {
		dbgFree(memoryList + 1);
	}
}


void *dbgMallocAndRead(UDATA size, void * remoteAddress)
{
	void * localAddress;

	localAddress = dbgTargetToLocalWithSize(remoteAddress, size);
	if (!localAddress) {
		UDATA bytesRead;

		localAddress = dbgMalloc(size, remoteAddress);
		if (!localAddress) {
			dbgError("could not allocate temp space (%zu bytes for %p)\n", size, remoteAddress);
			return NULL;
		}
		dbgReadMemory((UDATA) remoteAddress, localAddress, size, &bytesRead);
		if (bytesRead != size) {
			dbgFree(localAddress);
			dbgError("could not read memory (%zu bytes from %p)\n", size, remoteAddress);
			return NULL;
		}
	}
	return localAddress;
}

J9JavaVM *
dbgReadJavaVM(J9JavaVM * remoteVM)
{
	J9JavaVM * localVM;

	localVM = (J9JavaVM *) dbgTargetToLocalWithSize(remoteVM, sizeof(J9JavaVM));
	if (!localVM) {
		localVM = dbgMallocAndRead(sizeof(J9JavaVM), remoteVM);
		if (localVM) {
			localVM->portLibrary = dbgGetPortLibrary();

#ifdef J9VM_INTERP_NATIVE_SUPPORT
			if (localVM->jitConfig) {
				DBG_TRY {
					localVM->jitConfig = (J9JITConfig *) dbgMallocAndRead(sizeof(J9JITConfig), localVM->jitConfig);
				} DBG_CATCH {
					dbgError("could not read jitconfig");
					dbgFree(localVM);
					return NULL;
				} DBG_FINALLY;

				if (localVM->jitConfig->i2jReturnTable) {
					DBG_TRY {
						localVM->jitConfig->i2jReturnTable = dbgMallocAndRead(J9SW_JIT_RETURN_TABLE_SIZE * sizeof(UDATA), localVM->jitConfig->i2jReturnTable);
					} DBG_CATCH {
						dbgError("could not read jitconfig->i2jReturnTable");
						dbgFree(localVM);
						return NULL;
					} DBG_FINALLY;
				}
			}
#endif
			localVM->walkStackFrames = NULL;
			localVM->localMapFunction = NULL;
#ifdef J9VM_INTERP_VERBOSE
			localVM->verboseStackDump = NULL;
#endif
		} else {
			dbgError("Could not read java VM\n");
		}
	}
	cachedVM = remoteVM;
	return localVM;
}

UDATA
dbgReadUDATA(UDATA * remoteAddress)
{
	UDATA data = 0;
	UDATA bytesRead = 0;

	dbgReadMemory((UDATA) remoteAddress, &data, sizeof(UDATA), &bytesRead);
	if (bytesRead != sizeof(UDATA)) dbgError("could not read UDATA at %p\n", remoteAddress);
	return data;
}

U_64
dbgReadU64(U_64 * remoteAddress)
{
	U_64 data = 0;
	UDATA bytesRead = 0;

	dbgReadMemory((UDATA) remoteAddress, &data, sizeof(U_64), &bytesRead);
	if (bytesRead != sizeof(U_64)) dbgError("could not read U_64 at %p\n", remoteAddress);
	return data;
}

/**
 * Read a primitive data type from the target memory.
 * Does not support reading data types larger than UDATA.
 *
 * @param[in] remoteAddress Address in the target memory.
 * @param[in] size sizeof the primitive data type to return
 * @returns content of target memory
 */
UDATA
dbgReadPrimitiveType(UDATA remoteAddress, UDATA size)
{
	UDATA value = 0;
	UDATA bytesActuallyRead = 0;

	dbgReadMemory(remoteAddress, &value, size, &bytesActuallyRead);

	if (bytesActuallyRead != size) {
		dbgError("could not read %zu bytes at %p\n", size, remoteAddress);
	}

#if !defined(J9VM_ENV_LITTLE_ENDIAN)
	/* On big endian, shift datatypes smaller than UDATA down in the buffer
	 * to correctly widen to UDATA.
	 */
	value = value >> (8 * (sizeof(UDATA) - size));
#endif
	return value;
}

UDATA
dbgParseArgs(const char * originalArgs, UDATA * argValues, UDATA maxArgs)
{
	UDATA anyArgs = FALSE;
	UDATA argCount = 0;
	char c;
	const char * argStart;
	char *args;
	char *copiedArgs;
	UDATA len;
	PORT_ACCESS_FROM_PORT(dbgGetPortLibrary());

	len = strlen(originalArgs);
	argStart = args = copiedArgs = j9mem_allocate_memory(len + 1, OMRMEM_CATEGORY_VM);
	if (copiedArgs == NULL) {
		return 0;
	}
	strcpy(copiedArgs, originalArgs);

	do {
		c = *(args++);
		if ( (c == '\0') && !anyArgs) {
			break;
		}
		if (c != ' ') {
			anyArgs = TRUE;
		}
		if ( (c == '\0') || (c == ',')) {
			if (argCount < maxArgs) {

				/* NUL terminate the argument */
				args[-1] = '\0';

				argValues[argCount] = dbgGetExpression(argStart);
				argStart = args;
			}
			++argCount;
		}
	} while (c != '\0');

	j9mem_free_memory(copiedArgs);

	return argCount;
}

/**
 * Read the contents of size bytes of memory.  The result is 0-extended to UDATA
 * @param srcaddr the address of the memory to be read
 * @param size the number of bytes to read
 * @return the memory contents
 */
UDATA dbgReadSlot(UDATA srcaddr, UDATA size)
{
	UDATA value = 0;
	UDATA bytesActuallyRead = 0;

	if (size > sizeof(UDATA)) {
		dbgError("size (%d) > sizeof(UDATA) (%d)\n", size, sizeof(UDATA));
	}
	dbgReadMemory(srcaddr, &value, size, &bytesActuallyRead);

	if (bytesActuallyRead != size) {
		dbgError("unable to read %zu bytes at %p\n", size, srcaddr);
	}

/* since the order and size of the data are both orthogonally variable, we need an ifdef to split out the endian from the problem */
#if !defined(J9VM_ENV_LITTLE_ENDIAN)
	value = value >> (8* (sizeof(UDATA)-size));
#endif /* !defined(J9VM_ENV_LITTLE_ENDIAN) */
	return value;
}

U_16 dbgReadU16(U_16 * remoteAddress)
{
	U_16 data = 0;
	UDATA bytesRead = 0;

	dbgReadMemory((UDATA) remoteAddress, &data, sizeof(U_16), &bytesRead);
	if (bytesRead != sizeof(U_16)) dbgError("could not read U_16 at %p\n", remoteAddress);
	return data;
}

U_32 dbgReadU32(U_32 * remoteAddress)
{
	U_32 data = 0;
	UDATA bytesRead = 0;

	dbgReadMemory((UDATA) remoteAddress, &data, sizeof(U_32), &bytesRead);
	if (bytesRead != sizeof(U_32)) dbgError("could not read U_32 at %p\n", remoteAddress);
	return data;
}

void *
dbgReadSRP(J9SRP * remoteSRPAddress)
{
	J9SRP srpValue;

	srpValue = (J9SRP)dbgReadU32((U_32*)remoteSRPAddress);
	if (!srpValue) {
		return NULL;
	}
	return ((U_8 *) remoteSRPAddress) + srpValue;
}

J9PortLibrary * dbgGetPortLibrary(void)
{
	static int initialized;
	static J9PortLibrary port;
	static J9PortLibraryVersion portLibraryVersion;
	J9PortLibrary * portLib = &port;

	if (!initialized) {
		initialized = 1;

		/* Use portlibrary version which we compiled against, and have allocated space
		 * for on the stack.  This version may be different from the one in the linked DLL.
		 */
		J9PORT_SET_VERSION(&portLibraryVersion, J9PORT_CAPABILITY_MASK);
		dbg_j9port_create_library(portLib, &portLibraryVersion, sizeof(J9PortLibrary));

		old_write = OMRPORT_FROM_J9PORT(portLib)->file_write;
		OMRPORT_FROM_J9PORT(portLib)->file_write = dbg_write;
#ifdef J9ZOS390
		OMRPORT_FROM_J9PORT(portLib)->file_write_text = dbg_write_text;
#endif

		portLib->port_startup_library(portLib);

	}

	return portLib;
}

void dbgext_findvm(const char *args)
{
	cachedVM = NULL;
	dbgSniffForJavaVM();
}

void
dbgext_setvm(const char *args)
{
	dbgSetVM((void*)dbgGetExpression(args));
}

void
dbgext_j9help(const char *args)
{
	dbgPrint("J9 DBG Extension - FOR INTERNAL USE ONLY\n");
	dbgPrint("%s\n", J9_COPYRIGHT_STRING);
	dbgPrint("Options: \n");
	dbgPrint("\n");
	dbgPrint("findvm                         - find the JavaVM struct.\n");
	dbgPrint("setvm <address>                - set the JavaVM address.\n");
	dbgPrint("trprint <name>, <address>      - dump jit data structure.\n");
}


void * dbgTargetToLocalWithSize(void * addr, UDATA size)
{
	dbgNode * current = memoryList;

	while (current) {
		U_8 * lowAddr = (U_8 *) (current->originalAddress);
		U_8 * highAddr = lowAddr + current->size;

		if (((U_8 *)addr >= lowAddr) && ((U_8 *)addr < highAddr)) {
			if ( (U_8*)addr + size > highAddr ) {
				dbgError("Found partial memory match for %p at %p, but it only has %d bytes (needed %d)\n", addr, current, highAddr - (U_8*)addr, size);
				return NULL;
			} else {
				return ((U_8*) addr) - lowAddr + ((U_8*) (current + 1));
			}
		}

		current = current->link;
	}

	return NULL;
}

#if defined(J9ZOS390)
/* on z/OS, we redefine the portlib file_write_text as well as file_write so create this wrapper to satisfy the type system */
static IDATA
dbg_write_text(OMRPortLibrary *portLibrary, IDATA fd, const char * buf, IDATA nbytes)
{
	return dbg_write(portLibrary, fd, buf, nbytes);
}
#endif /* defined(J9ZOS390) */


static IDATA
dbg_write(OMRPortLibrary *portLibrary, IDATA fd, const void * buf, IDATA nbytes)
{
	if (fd == J9PORT_TTY_OUT || fd == J9PORT_TTY_ERR) {
		dbgPrint("%.*s", nbytes, buf);
		return nbytes;
	} else {
		return old_write(portLibrary, fd, buf, nbytes);
	}
}

void*
dbgSetHandler(void* bufPointer) {
	DBG_JMPBUF* oldHandler = dbgErrorHandler;
	dbgErrorHandler = bufPointer;
	return oldHandler;
}

void
dbgSetVerboseMode(int verboseMode) {
	dbgVerboseMode = verboseMode;
}

void
dbgError(const char* format, ...) {
	PORT_ACCESS_FROM_PORT(dbgGetPortLibrary());
	va_list args;

	if (dbgVerboseMode) {
		va_start( args, format );
		j9tty_vprintf(format, args);
		va_end( args );
	}

	if (dbgErrorHandler) {
		longjmp(dbgErrorHandler->buf, 0);
	}
}


/* returns an ALLOCATED string */
char*
dbgReadString(char* remoteAddress)
{
	UDATA len = 0;
	while (dbgReadByte((U_8*)remoteAddress + len)) len++;
	return dbgMallocAndRead(len + 1, remoteAddress);
}


static J9PoolPuddle *
dbgReadPoolPuddle(J9Pool* head, J9PoolPuddle* remotePuddle)
{
	J9PoolPuddle * localPuddle;

	localPuddle = dbgTargetToLocalWithSize(remotePuddle, head->puddleAllocSize);
	if (!localPuddle) {
		IDATA offset;

		localPuddle = dbgMallocAndRead(head->puddleAllocSize, remotePuddle);
		if (!localPuddle) {
			dbgError("could not read puddle\n");
			return NULL;
		}

		offset = (UDATA)remotePuddle - (UDATA)localPuddle;
		if (localPuddle->nextPuddle) {
			IDATA nextPuddle;

			nextPuddle = (IDATA)LOCAL_NNWSRP_GET(localPuddle->nextPuddle, J9PoolPuddle*) + offset;
			WSRP_SET(localPuddle->nextPuddle, dbgReadPoolPuddle(head, (J9PoolPuddle*)nextPuddle));
		}
		if (localPuddle->prevPuddle) {
			IDATA prevPuddle;

			prevPuddle = (IDATA)LOCAL_NNWSRP_GET(localPuddle->prevPuddle, J9PoolPuddle*) + offset;
			WSRP_SET(localPuddle->prevPuddle, dbgReadPoolPuddle(head, (J9PoolPuddle*)prevPuddle));
		}
		if (localPuddle->nextAvailablePuddle) {
			IDATA nextAvailablePuddle;

			nextAvailablePuddle = (IDATA)LOCAL_NNWSRP_GET(localPuddle->nextAvailablePuddle, J9PoolPuddle*) + offset;
			WSRP_SET(localPuddle->nextAvailablePuddle, dbgReadPoolPuddle(head, (J9PoolPuddle*)nextAvailablePuddle));
		}
		if (localPuddle->prevAvailablePuddle) {
			IDATA prevAvailablePuddle;

			prevAvailablePuddle = (IDATA)LOCAL_NNWSRP_GET(localPuddle->prevAvailablePuddle, J9PoolPuddle*) + offset;
			WSRP_SET(localPuddle->prevAvailablePuddle, dbgReadPoolPuddle(head, (J9PoolPuddle*)prevAvailablePuddle));
		}
	}
	return localPuddle;
}

static J9PoolPuddleList *
dbgReadPoolPuddleList(J9Pool* pool, J9PoolPuddleList* removePuddleList)
{
	J9PoolPuddleList * localPuddleList;

	localPuddleList = dbgTargetToLocalWithSize(removePuddleList, sizeof(J9PoolPuddleList));
	if (!localPuddleList) {
		IDATA offset;

		localPuddleList = dbgMallocAndRead(sizeof(J9PoolPuddleList), removePuddleList);
		if (!localPuddleList) {
			dbgError("could not read puddleList\n");
			return NULL;
		}

		offset = (UDATA)removePuddleList - (UDATA)localPuddleList;
		if (localPuddleList->nextPuddle) {
			IDATA nextPuddle;

			nextPuddle = (IDATA)LOCAL_NNWSRP_GET(localPuddleList->nextPuddle, J9PoolPuddle*) + offset;
			WSRP_SET(localPuddleList->nextPuddle, dbgReadPoolPuddle(pool, (J9PoolPuddle*)nextPuddle));
		}
		if (localPuddleList->nextAvailablePuddle) {
			IDATA nextAvailablePuddle;

			nextAvailablePuddle = (IDATA)LOCAL_NNWSRP_GET(localPuddleList->nextAvailablePuddle, J9PoolPuddle*) + offset;
			WSRP_SET(localPuddleList->nextAvailablePuddle, dbgReadPoolPuddle(pool, (J9PoolPuddle*)nextAvailablePuddle));
		}
	}
	return localPuddleList;
}

/* returns a copy of the specified pool in local memory space
 * The pool is walkable in local space.
 * Note that elements in the pool are not fixed up in any way, so
 * the result of walking the pool will be local, but non-relocated
 * copies of the elements of the pool
 */
J9Pool *
dbgReadPool(J9Pool* remotePool)
{
	J9Pool * localPool;

	localPool = dbgTargetToLocalWithSize(remotePool, sizeof(J9Pool));
	if (!localPool) {
		IDATA offset;
		IDATA puddleList;

		localPool = dbgMallocAndRead(sizeof(J9Pool), remotePool);
		if (!localPool) {
			dbgError("could not read pool\n");
			return NULL;
		}

		offset = (UDATA)remotePool - (UDATA)localPool;
		puddleList = (IDATA) LOCAL_NNWSRP_GET(localPool->puddleList, J9PoolPuddle*) + offset;
		WSRP_SET(localPool->puddleList, dbgReadPoolPuddleList(localPool, (J9PoolPuddleList*)puddleList));
	}
	return localPool;
}

/* returns a target process pointer to the J9ThreadLibrary */
void*
dbgGetThreadLibrary(void) {
	J9JavaVM* remoteVM = dbgSniffForJavaVM();
	if (remoteVM) {
		J9VMThread* remoteThread = (J9VMThread*)dbgReadUDATA((UDATA*)&remoteVM->mainThread);
		if (remoteThread) {
			J9AbstractThread* remoteOSThread = (J9AbstractThread*)dbgReadUDATA((UDATA*)&remoteThread->osThread);
			if (remoteOSThread) {
				return (void*)dbgReadUDATA((UDATA*)&remoteOSThread->library);
			}
		}
	}
	dbgError("Unable to find thread library\n");
	return NULL;
}

void
dbgSetLocalBlockRelocated(void * addr, UDATA value)
{
	dbgNode * current = memoryList;

	while (current) {
		U_8 * lowAddr = (U_8 *) (current + 1);
		U_8 * highAddr = lowAddr + current->size;

		if (((U_8 *)addr >= lowAddr) && ((U_8 *)addr < highAddr)) {
			current->relocated = value;
			return;
		}

		current = current->link;
	}

	dbgError("dbgSetLocalBlockRelocated: Local memory %p has no mapping to target memory\n", addr);
}

UDATA
dbgGetLocalBlockRelocated(void * addr)
{
	dbgNode * current = memoryList;

	if (addr == NULL) return 1;

	while (current) {
		U_8 * lowAddr = (U_8 *) (current + 1);
		U_8 * highAddr = lowAddr + current->size;

		if (((U_8 *)addr >= lowAddr) && ((U_8 *)addr < highAddr))
			return current->relocated;

		current = current->link;
	}

	dbgError("dbgGetLocalBlockRelocated: Local memory %p has no mapping to target memory\n", addr);
	return 0;
}



/*
 * pattern: a pointer to the eyecatcher pattern to search for
 * patternLength: length of pattern
 * patternAlignment: guaranteed minimum alignment of the pattern (must be a power of 2)
 * startSearchFrom: minimum address to search at (useful for multiple occurrences of the pattern)
 * bytesToSearch: maximum number of bytes to search
 * bytesSearched: number of bytes searched is returned through this pointer. 0 indicates that no attempt was made to find the pattern.
 *
 * Returns:
 *    The address of the eyecatcher in TARGET memory space
 *     or NULL if it was not found.
 *
 * NOTES:
 *    Currently, this may fail to find the pattern if patternLength > patternAlignment
 *    Generally, dbgFindPattern should be called instead of dbgFindPatternInRange. It can be
 *      more clever about not searching ranges which aren't in use.
 */
void*
dbgFindPatternInRange(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched)
{
	U_8* page = startSearchFrom;
	UDATA align;

	/* round to a page */
	align = (UDATA)page & 4095;
	page -= align;
	bytesToSearch += align;

	*bytesSearched = 0;

	for (;;) {
		U_8 data[4096];
		UDATA bytesRead = 0;

		dbgReadMemory((UDATA)page, data, sizeof(data), &bytesRead);
		if (0 != bytesRead) {
			UDATA i;
			if (bytesRead > bytesToSearch) {
				bytesRead = bytesToSearch;
			}
			*bytesSearched += bytesRead;
			for (i = 0; (IDATA)i < (IDATA)(bytesRead - patternLength); i += patternAlignment) {
				if (memcmp(data + i, pattern, patternLength) == 0) {
					/* in case the page started before startSearchFrom */
					if (page + i >= startSearchFrom) {
						return page + i;
					}
				}
			}
		}

		if (bytesToSearch < 4096) {
			break;
		}

		page += 4096;
		bytesToSearch -= 4096;
	}

	return NULL;
}

void
dbgSetVM(void* vmPtr)
{
	J9JavaVM * ptr = (J9JavaVM *)vmPtr;
	J9VMThread* vmThread;
	J9JavaVM* bytes;
	UDATA bytesRead;

	if (testJavaVMPtr(ptr)) {
		cachedVM = ptr;
found:
		dbgPrint("VM set to %p\n", cachedVM);
		return;
	}

	/* hmm. It's not a J9JavaVM. Maybe it's a vmThread? */
	/* try to read the VM slot */
	vmThread = (J9VMThread *) ptr;
	bytesRead = 0;
	dbgReadMemory((UDATA)&vmThread->javaVM, &bytes, sizeof(vmThread->javaVM), &bytesRead);
	if (bytesRead == sizeof(vmThread->javaVM)) {
		if (testJavaVMPtr(bytes)) {
			cachedVM = bytes;
			goto found;
		}
	}

	dbgError("Error: Specified value is not a javaVM or vmThread pointer, VM not set\n");
}

static IDATA
hexValue(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return 10 + c - 'a';
	} else if (c >= 'A' && c <= 'F') {
		return 10 + c - 'A';
	} else {
		return -1;
	}
}

void
dbgext_findpattern(const char *args)
{
	UDATA argCount;
	UDATA argValues[3];
	const char* hexend;
	U_8 pattern[1024];
	UDATA i;
	UDATA length;
	UDATA bytesSearched;
	void* result;

	hexend = strchr(args, ',');

	if (hexend == NULL) {
		dbgPrint("Usage: \n");
		dbgPrint("  !findpattern hexstring,alignment\n");
		dbgPrint("  !findpattern hexstring,alignment,startPtr\n");
		dbgPrint("  !findpattern hexstring,alignment,startPtr,bytesToSearch\n");
		return;
	}

	argCount = dbgParseArgs(hexend + 1, argValues, 3);
	if (argCount == 0) {
		dbgError("Error: must specify alignment\n");
		return;
	} else if (argCount == 1) {
		argValues[1] = 0;
		argValues[2] = (UDATA)-1;
	} else if (argCount == 2) {
		argValues[2] = (UDATA)-1 - argValues[1];
	} else if (argCount > 3) {
		dbgError("Error: too many arguments\n");
		return;
	}

	length = (hexend - args) / 2;
	if (length > sizeof(pattern)) {
		dbgPrint("Pattern is too long. Truncating to %d bytes\n", sizeof(pattern));
		length = sizeof(pattern);
	}

	for (i = 0; i < length; i++) {
		IDATA hex1 = hexValue(args[i * 2]);
		IDATA hex2 = hexValue(args[i * 2 + 1]);

		if ( (hex1 < 0) || (hex2 < 0) ) {
			dbgError("Error: non-hex value found in hex string\n");
			return;
		}

		pattern[i] = (U_8) ((hex1 << 4) + hex2);

#if 0
		dbgPrint("%c%c = (%u << 4) + %u = %u\n", args[i * 2], args[i * 2 + 1], hex1, hex2, pattern[i]);
#endif
	}

	/* ensure that alignment is > 0 */
	if (argValues[0] == 0) {
		argValues[0] = 1;
	}

	dbgPrint("Searching for %zu bytes. Alignment = %zu, start = 0x%p, bytesToSearch = %zu ...\n", length, argValues[0], argValues[1], argValues[2]);

	result = dbgFindPatternInRange(pattern, length, argValues[0], (U_8 *) argValues[1], argValues[2], &bytesSearched);

	dbgPrint("Searched %zu bytes -- result = 0x%p\n", bytesSearched, result);
}

/*
 * Prints message to the debug print screen
 */
void
dbgVPrint (const char* message, va_list arg_ptr)
{
	char printBuf[4096];
	PORT_ACCESS_FROM_PORT(dbgGetPortLibrary());

	j9str_vprintf(printBuf, sizeof(printBuf), message, arg_ptr );

	dbgWriteString(printBuf);
}


/*
 * Prints message to the debug print screen
 */
void
dbgPrint (const char* message, ...)
{
	va_list arg_ptr;

	va_start(arg_ptr, message);

	dbgVPrint(message, arg_ptr);

	va_end(arg_ptr);
}

/*
 * Parse the args for dumping a structure.
 * The args may consist of just an address, or a field name (with wildcards)
 * followed by a command and an address.
 *
 * e.g. "0x3738000" or "foo*,0x3738000"
 *
 * See parseWildcard() for doc on how to use needle, needleLength and matchFlag
 *
 */
IDATA
dbgParseArgForStructDump(const char * args, UDATA* structAddress, const char** needle, UDATA* needleLength, U_32 * matchFlag)
{
	const char* comma = strchr(args, ',');
	IDATA result = 0;

	if (comma == NULL) {
		*structAddress = dbgGetExpression(args);
		result = parseWildcard("*", 1, needle, needleLength, matchFlag);
	} else {
		*structAddress = dbgGetExpression(comma + 1);
		result = parseWildcard(args, comma - args, needle, needleLength, matchFlag);
	}

	if (result != 0) {
		dbgPrint("unable to parse field name\n");
		result = -1;
	}

	if (*structAddress == 0) {
		dbgPrint("bad or missing address\n");
		result = -1;
	}

	return result;

}

