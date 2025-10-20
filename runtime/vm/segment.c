/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "j9user.h"
#include "j9protos.h"
#include "j9cfg.h"
#include "j9port.h"
#if defined(J9VM_OPT_SNAPSHOTS)
#include "j9port_generated.h"
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
#include "j9consts.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "segment.h"
#include "j9modron.h"
#include "omrutilbase.h"

#define ROUND_TO(granularity, number) (((number) + (granularity) - 1) & ~((UDATA)(granularity) - 1))

J9MemorySegment * allocateMemorySegment (J9JavaVM *javaVM, UDATA size, UDATA type, U_32 memoryCategory);
void freeMemorySegmentListEntry (J9MemorySegmentList *segmentList, J9MemorySegment *segment);
J9MemorySegmentList *allocateMemorySegmentListWithSize (J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA sizeOfElements, U_32 memoryCategory);
U_32 memorySegmentListSize (J9MemorySegmentList *segmentList);
void freeMemorySegmentList (J9JavaVM *javaVM,J9MemorySegmentList *segmentList);
static void * allocateMemoryForSegment (J9JavaVM *javaVM,J9MemorySegment *segment, J9PortVmemParams *vmemParams, U_32 memoryCategory);
J9MemorySegmentList *allocateMemorySegmentList (J9JavaVM * javaVM, U_32 numberOfMemorySegments, U_32 memoryCategory);
J9MemorySegment * romImageNewSegment (J9JavaVM *vm, J9ROMImageHeader *header, UDATA isBaseType, J9ClassLoader *classLoader );
void freeMemorySegment (J9JavaVM *javaVM, J9MemorySegment *segment, BOOLEAN freeDescriptor);
J9MemorySegment * allocateMemorySegmentListEntry (J9MemorySegmentList *segmentList);

J9Class* allClassesStartDo (J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader);
J9Class* allClassesNextDo (J9ClassWalkState* state);
void allClassesEndDo (J9ClassWalkState* state);

J9Class* allLiveClassesStartDo (J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader);
J9Class* allLiveClassesNextDo (J9ClassWalkState* state);
void allLiveClassesEndDo (J9ClassWalkState* state);

J9MemorySegment * allocateMemorySegmentInList (J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, U_32 memoryCategory);
J9MemorySegmentList *allocateMemorySegmentListWithFlags (J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA flags, U_32 memoryCategory);

J9MemorySegment * findMemorySegment(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA valueToFind);

static IDATA segmentInsertionComparator (J9AVLTree *tree, J9MemorySegment *insertNode, J9MemorySegment *walkNode);
static IDATA segmentSearchComparator (J9AVLTree *tree, UDATA value, J9MemorySegment *searchNode);
static J9MemorySegment * allocateVirtualMemorySegmentInListInternal(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, J9PortVmemParams *vmemParams, U_32 memoryCategory);

static BOOLEAN shouldSkipClassLoader(J9ClassLoaderWalkState *state, J9ClassLoader *classLoader);


/*
 *	Call to create a memory segment and to add it to the memory segment list pointed to
 *	by the globalInfo.  All fields are filled out as necessary.
 */
J9MemorySegment * allocateMemorySegment(J9JavaVM *javaVM, UDATA size, UDATA type, U_32 memoryCategory)
{
	return allocateMemorySegmentInList(javaVM, javaVM->memorySegments, size, type, memoryCategory);
}


J9MemorySegmentList *allocateMemorySegmentList(J9JavaVM * javaVM, U_32 numberOfMemorySegments, U_32 memoryCategory)
{
	return allocateMemorySegmentListWithSize(javaVM, numberOfMemorySegments, sizeof(J9MemorySegment), memoryCategory);
}

J9MemorySegmentList *allocateMemorySegmentListWithFlags(J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA flags, U_32 memoryCategory)
{
	J9MemorySegmentList *segmentList;
	segmentList = allocateMemorySegmentListWithSize(javaVM, numberOfMemorySegments, sizeof(J9MemorySegment), memoryCategory);
	if (NULL != segmentList) {
		segmentList->flags |= flags;
	}
	return segmentList;
}


void freeMemorySegment(J9JavaVM *javaVM, J9MemorySegment *segment, BOOLEAN freeDescriptor)
{
	J9MemorySegmentList *segmentList;
	PORT_ACCESS_FROM_GINFO(javaVM);

	segmentList = segment->memorySegmentList;

#if defined(J9VM_THR_PREEMPTIVE)
	if(segmentList->segmentMutex) {
		omrthread_monitor_enter(segmentList->segmentMutex);
	}
#endif

	Trc_VM_freeMemorySegment(
		currentVMThread(javaVM),
		segment,
		segment->heapBase,
		segment->heapTop,
		segment->classLoader,
		segment->type);

	if (0 != (segmentList->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
		avl_delete(&segmentList->avlTreeData, (J9AVLTreeNode *) segment);
	}

	segmentList->totalSegmentSize -= segment->size;

	if (segment->type & MEMORY_TYPE_ALLOCATED) {
#if defined(J9VM_OPT_SNAPSHOTS)
		VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		BOOLEAN useAdvise = (J9_EXTENDED_RUNTIME_FLAG_JSCRATCH_ADV_ON_FREE == (javaVM->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_FLAG_JSCRATCH_ADV_ON_FREE));

		/* The order of these checks is important.
		 * MEMORY_TYPE_VIRTUAL is expected to be used along with another bit, like MEMORY_TYPE_JIT_SCRATCH_SPACE.
		 */
		if (J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_CODE | MEMORY_TYPE_FIXED_RAM_CLASS | MEMORY_TYPE_VIRTUAL)) {
			j9vmem_free_memory(segment->baseAddress, segment->size, &segment->vmemIdentifier);
		} else if (useAdvise && J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_JIT_SCRATCH_SPACE)) {
			j9mem_advise_and_free_memory(segment->baseAddress);
		} else if (J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS | MEMORY_TYPE_UNDEAD_CLASS)) {
#if defined(J9VM_OPT_SNAPSHOTS)
			if (IS_SNAPSHOTTING_ENABLED(javaVM)) {
				if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)
					&& J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS_SUB4G)
				) {
					vmsnapshot_free_memory32(segment->baseAddress);
				} else {
					vmsnapshot_free_memory(segment->baseAddress);
				}
			} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
			{
				if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)
					&& J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS_SUB4G)
				) {
					j9mem_free_memory32(segment->baseAddress);
				} else {
					j9mem_free_memory(segment->baseAddress);
				}
			}
#if defined(J9VM_OPT_SNAPSHOTS)
		} else if (J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_ROM_CLASS)
			&& IS_SNAPSHOTTING_ENABLED(javaVM)
		) {
			vmsnapshot_free_memory(segment->baseAddress);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		} else {
			j9mem_free_memory(segment->baseAddress);
		}
		segment->type &= ~MEMORY_TYPE_ALLOCATED;
	}

	if(freeDescriptor) {
		freeMemorySegmentListEntry(segmentList, segment);
	}

#if defined(J9VM_THR_PREEMPTIVE)
	if(segmentList->segmentMutex) {
		omrthread_monitor_exit(segmentList->segmentMutex);
	}
#endif
}


J9MemorySegment * allocateMemorySegmentListEntry(J9MemorySegmentList *segmentList)
{
	J9MemorySegment *segment;

	if ((segment = (J9MemorySegment *) pool_newElement(segmentList->segmentPool)) == NULL) {
		return 0;
	}

	/* add to singly linked list of segments */
	J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_ADD(segmentList->nextSegment, segment);

	segment->memorySegmentList = segmentList;

	return segment;
}


void freeMemorySegmentListEntry(J9MemorySegmentList *segmentList, J9MemorySegment *segment)
{
	J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_REMOVE(segmentList->nextSegment, segment);
	pool_removeElement(segmentList->segmentPool, segment);
}


void freeMemorySegmentList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	J9MemorySegment *currentSegment = NULL;

	do {
		currentSegment = segmentList->nextSegment;
		if (NULL != currentSegment)
			freeMemorySegment(javaVM, currentSegment, 1);
	} while (NULL != currentSegment);
	pool_kill(segmentList->segmentPool);

#if defined(J9VM_THR_PREEMPTIVE)
	if(segmentList->segmentMutex) omrthread_monitor_destroy(segmentList->segmentMutex);
#endif

	/* It is guaranteed that classMemorySegments were allocated on the VMSnapshotImpl heap. */
	/* TODO: In J9MemorySegmentList flags, add an option to signify allocation from the
	 * VMSnapshotImpl to replace this check (See @ref omr:j9nongenerated.h).
	 */
#if defined(J9VM_OPT_SNAPSHOTS)
	if (IS_SNAPSHOTTING_ENABLED(javaVM)
		&& ((javaVM->classMemorySegments == segmentList) || (javaVM->memorySegments == segmentList))
	) {
		vmsnapshot_free_memory(segmentList);
	} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	{
		j9mem_free_memory(segmentList);
	}
}

void
allSegmentsInMemorySegmentListDo(J9MemorySegmentList *segmentList, void (* segmentCallback)(J9MemorySegment*, void*), void *userData)
{
	Assert_VM_notNull(segmentList);
	Assert_VM_notNull(segmentCallback);
	if (!J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_IS_EMPTY(segmentList->nextSegment)) {
		J9MemorySegment *segment = J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_START_DO(segmentList->nextSegment);

		while (segment != NULL) {
			segmentCallback(segment, userData);
			segment = J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_NEXT_DO(segmentList->nextSegment, segment);
		}
	}
}


U_32 memorySegmentListSize (J9MemorySegmentList *segmentList)
{
	return (U_32) pool_numElements(segmentList->segmentPool);
}


/*
 *	Allocates the memory for a segment.
 *	only 2 cases are delt with:
 *	1. MEMORY_TYPE_CODE
 *	2.	Everything else.  Regardless of the type, it will be allocated
 */
static void *
allocateMemoryForSegment(J9JavaVM *javaVM, J9MemorySegment *segment, J9PortVmemParams *vmemParams, U_32 memoryCategory)
{
	void *tmpAddr = NULL;
	PORT_ACCESS_FROM_GINFO(javaVM);
#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */

	/* The order of these checks is important.
	 * MEMORY_TYPE_VIRTUAL is expected to be used along with another bit, like MEMORY_TYPE_JIT_SCRATCH_SPACE.
	 */
	if (J9_ARE_ANY_BITS_SET(segment->type,  MEMORY_TYPE_CODE | MEMORY_TYPE_VIRTUAL)) {
		/* MEMORY_TYPE_CODE and MEMORY_TYPE_VIRTUAL are allocated via the virtual memory functions.
		 * Assert MEMORY_TYPE_VIRTUAL is used in combination with another bit, like MEMORY_TYPE_JIT_SCRATCH_SPACE.
		 */
		Assert_VM_true(J9_ARE_NO_BITS_SET(segment->type, MEMORY_TYPE_VIRTUAL) || J9_ARE_ANY_BITS_SET(segment->type, ~MEMORY_TYPE_VIRTUAL));

		/* Memory segments can be marked as MEMORY_TYPE_DISCLAIMABLE_TO_FILE
		 * to indicate the intent of allocating memory backed-up by a file.
		 * This information is passed to the omr port library by setting the
		 * flag OMRPORT_VMEM_MEMORY_MODE_SHARE_TMP_FILE_OPEN.
		 * Note that when setting the OMRPORT_VMEM_MEMORY_MODE_SHARE_TMP_FILE_OPEN bit,
		 * we also have to set the OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN bit.
		 */
		if (J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_DISCLAIMABLE_TO_FILE)) {
			vmemParams->mode |= (OMRPORT_VMEM_MEMORY_MODE_SHARE_TMP_FILE_OPEN | OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN);
		}

		tmpAddr = j9vmem_reserve_memory_ex(&segment->vmemIdentifier, vmemParams);
	} else if (J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_FIXED_RAM_CLASS)) {
		tmpAddr = j9vmem_reserve_memory_ex(&segment->vmemIdentifier, vmemParams);
		Trc_VM_virtualRAMClassAlloc(tmpAddr);
	} else if (J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS)) {
#if defined(J9VM_OPT_SNAPSHOTS)
		if (IS_SNAPSHOTTING_ENABLED(javaVM)) {
			if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)
				&& J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS_SUB4G)
			) {
				tmpAddr = vmsnapshot_allocate_memory32(segment->size, memoryCategory);
			} else {
				tmpAddr = vmsnapshot_allocate_memory(segment->size, memoryCategory);
			}
		} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		{
			if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)
				&& J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS_SUB4G)
			) {
				tmpAddr = j9mem_allocate_memory32(segment->size, memoryCategory);
			} else {
				tmpAddr = j9mem_allocate_memory(segment->size, memoryCategory);
			}
		}
#if defined(J9VM_OPT_SNAPSHOTS)
	} else if (J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_ROM_CLASS) && IS_SNAPSHOTTING_ENABLED(javaVM)) {
		tmpAddr = vmsnapshot_allocate_memory(segment->size, memoryCategory);
		/* TODO: Add Memory type for allocation inside the snapshot. (MEMORY_TYPE_IMAGE_ALLOCATED)
		 * (See @ref omr:j9nongenerated.h).
		 */
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	} else {
		tmpAddr = j9mem_allocate_memory(segment->size, memoryCategory);
	}

	if (NULL != tmpAddr) {
		segment->type = segment->type | MEMORY_TYPE_ALLOCATED;
	}

	return tmpAddr;
}


J9MemorySegment *
romImageNewSegment(J9JavaVM *vm, J9ROMImageHeader *header, UDATA isBaseType, J9ClassLoader *classLoader )
{
	J9MemorySegment  *romSegment;

#if defined(J9VM_THR_PREEMPTIVE)
	if(vm->classMemorySegments->segmentMutex) omrthread_monitor_enter(vm->classMemorySegments->segmentMutex);
#endif

	if((romSegment = allocateMemorySegmentListEntry(vm->classMemorySegments)) != NULL) {
		U_8 *aot;
		U_32 size;

		if (isBaseType) {
			romSegment->type = MEMORY_TYPE_BASETYPE_ROM_CLASS | MEMORY_TYPE_ROM | MEMORY_TYPE_FIXEDSIZE;
		} else {
			romSegment->type = MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_ROM | MEMORY_TYPE_FIXEDSIZE;
		}

		romSegment->type |= MEMORY_TYPE_FROM_JXE;

		size = header->romSize + sizeof(*header);
		aot = (U_8 *)J9ROMIMAGEHEADER_AOTPOINTER(header);
		romSegment->size = size;
		romSegment->baseAddress = (U_8*)header;
		romSegment->heapBase = (U_8*)J9ROMIMAGEHEADER_FIRSTCLASS(header);
		romSegment->heapTop = (U_8 *)header + size;
		romSegment->heapAlloc = (aot == NULL) ? romSegment->heapTop : aot;
		romSegment->classLoader = classLoader;

		if (0 != (vm->classMemorySegments->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
			avl_insert(&vm->classMemorySegments->avlTreeData, (J9AVLTreeNode *) romSegment);
		}
	}

#if defined(J9VM_THR_PREEMPTIVE)
	if(vm->classMemorySegments->segmentMutex) omrthread_monitor_exit(vm->classMemorySegments->segmentMutex);
#endif

	return romSegment;
}


/*
 *	Call to create a memory segment and to add it to the memory segment list pointed to
 *	by the globalInfo.  All fields are filled out as necessary.
 */
J9MemorySegment * allocateMemorySegmentInList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, U_32 memoryCategory)
{
	return allocateFixedMemorySegmentInList(javaVM, segmentList, size, type, NULL, memoryCategory);
}


/*
 *	Call to create a memory segment at a desired address, and to add it to the memory segment list pointed to
 *	by the globalInfo.  All fields are filled out as necessary.
 */
J9MemorySegment * allocateFixedMemorySegmentInList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, U_8* desiredAddress, U_32 memoryCategory)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	J9PortVmemParams params;
	J9PortVmemParams * pParams = NULL;
	UDATA flags = 0;

	if (J9_ARE_ALL_BITS_SET(type, MEMORY_TYPE_CODE)) {
		flags = J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_EXECUTE;

		if (J9_ARE_NO_BITS_SET(type, MEMORY_TYPE_UNCOMMITTED)) {
			flags |= J9PORT_VMEM_MEMORY_MODE_COMMIT;
		}
	} else if (J9_ARE_ALL_BITS_SET(type, MEMORY_TYPE_FIXED_RAM_CLASS)) {
		flags = J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
#ifdef WIN32
		if (J9_ARE_ALL_BITS_SET(J9PORT_CAPABILITY_MASK, J9PORT_CAPABILITY_ALLOCATE_TOP_DOWN)) {
			flags |= J9PORT_VMEM_ALLOCATE_TOP_DOWN;
		}
#endif
	} else if (J9_ARE_ALL_BITS_SET(type, MEMORY_TYPE_VIRTUAL)) {

		flags = J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_VIRTUAL;

		if (J9_ARE_NO_BITS_SET(type, MEMORY_TYPE_UNCOMMITTED)) {
			flags |= J9PORT_VMEM_MEMORY_MODE_COMMIT;
		}
	}

	if (flags != 0) {
		pParams = &params;
		j9vmem_vmem_params_init(pParams);
		if (desiredAddress != NULL) {
			params.startAddress = desiredAddress;
			params.endAddress = desiredAddress;
		}
		params.mode = flags;
		params.category = memoryCategory;
		/* params.byteAmount set in allocateVirtualMemorySegmentInList */
	}

	return allocateVirtualMemorySegmentInListInternal(javaVM, segmentList, size, type, pParams, memoryCategory);
}




/*
 *	Call to create a memory segment at a desired address, and to add it to the memory segment list pointed to
 *	by the globalInfo.  All fields are filled out as necessary.
 */
J9MemorySegment * allocateVirtualMemorySegmentInList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, J9PortVmemParams *vmemParams)
{
	return allocateVirtualMemorySegmentInListInternal(javaVM, segmentList, size, type, vmemParams, OMRMEM_CATEGORY_UNKNOWN);
}

static J9MemorySegment * allocateVirtualMemorySegmentInListInternal(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, J9PortVmemParams *vmemParams, U_32 memoryCategory)
{
	U_8 *allocatedBase;
	J9MemorySegment *segment;

	Trc_VM_allocateMemorySegmentInList_Entry(segmentList, size, type);

#if defined(J9VM_THR_PREEMPTIVE)
	if(segmentList->segmentMutex) {
		omrthread_monitor_enter(segmentList->segmentMutex);
	}
#endif

	segment = allocateMemorySegmentListEntry(segmentList);
	if(segment == NULL) {
		Trc_VM_allocateMemorySegmentInList_EntryAllocFailed(segmentList, type);
	} else {
		segment->type = type;
		segment->size = size;

		if (vmemParams != NULL) {
			vmemParams->byteAmount = segment->size;
		}

		allocatedBase = (U_8 *) allocateMemoryForSegment(javaVM, segment, vmemParams, memoryCategory);

		if (NULL == allocatedBase) {
			Trc_VM_allocateMemorySegmentInList_AllocFailed(segmentList, size, type);
			freeMemorySegmentListEntry(segmentList, segment);
			segment = NULL;
		} else {
			if (J9_ARE_ALL_BITS_SET(type, MEMORY_TYPE_CODE)) {
				/* For CodeCache segments the JIT will later write a TR::CodeCache structure pointer at the begining of the segment.
				 * Until then, make sure that a potential reader sees a NULL pointer.
				 */
				omrthread_jit_write_protect_disable();
				*((UDATA**)allocatedBase) = NULL;
				issueWriteBarrier();
				omrthread_jit_write_protect_enable();
			}
			segment->baseAddress = allocatedBase;
			segment->heapBase = allocatedBase;
			segment->heapTop = (U_8 *)&(segment->heapBase)[size];
			segment->heapAlloc = segment->heapBase;

			segmentList->totalSegmentSize += segment->size;

			Trc_VM_allocateMemorySegmentInList_Alloc( segment, segment->heapBase, segment->heapTop, segment->type);

			if (0 != (segmentList->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
				avl_insert(&segmentList->avlTreeData, (J9AVLTreeNode *) segment);
			}
		}
	}

#if defined(J9VM_THR_PREEMPTIVE)
	if(segmentList->segmentMutex) {
		omrthread_monitor_exit(segmentList->segmentMutex);
	}
#endif

	Trc_VM_allocateMemorySegmentInList_Exit(segment);

	return segment;
}


J9MemorySegmentList *allocateMemorySegmentListWithSize(J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA sizeOfElements, U_32 memoryCategory)
{
	J9MemorySegmentList *segmentList = NULL;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */

#if defined(J9VM_OPT_SNAPSHOTS)
	/* Check if the current run is a snapshot run, to ensure correct allocation for the
	 * class memory segments list.
	 */
	if (IS_SNAPSHOT_RUN(javaVM)
		&& ((J9MEM_CATEGORY_CLASSES == memoryCategory) || (OMRMEM_CATEGORY_VM == memoryCategory))
	) {
		segmentList = vmsnapshot_allocate_memory(sizeof(J9MemorySegmentList), memoryCategory);
		if (NULL == segmentList) {
			return NULL;
		}
		segmentList->segmentPool = pool_new(
				sizeOfElements,
				numberOfMemorySegments,
				0,
				0,
				J9_GET_CALLSITE(),
				memoryCategory,
				POOL_FOR_PORT(VMSNAPSHOTIMPL_OMRPORT_FROM_JAVAVM(javaVM)));
		if (NULL == segmentList->segmentPool) {
			vmsnapshot_free_memory(segmentList);
			return NULL;
		}
	} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	{
		segmentList = j9mem_allocate_memory(sizeof(J9MemorySegmentList), memoryCategory);
		if (NULL == segmentList) {
			return NULL;
		}
		segmentList->segmentPool = pool_new(
				sizeOfElements,
				numberOfMemorySegments,
				0,
				0,
				J9_GET_CALLSITE(),
				memoryCategory,
				POOL_FOR_PORT(PORTLIB));
		if (NULL == segmentList->segmentPool) {
			j9mem_free_memory(segmentList);
			return NULL;
		}
	}
	segmentList->nextSegment = NULL;
	segmentList->totalSegmentSize = 0;
	segmentList->flags = 0;

#if defined(J9VM_THR_PREEMPTIVE)
	if (omrthread_monitor_init_with_name(&(segmentList->segmentMutex), 0, "VM mem segment list")) {
		pool_kill(segmentList->segmentPool);
		j9mem_free_memory(segmentList);
		return NULL;
	}
#endif

	memset(&segmentList->avlTreeData,0, sizeof(J9AVLTree));
	segmentList->avlTreeData.insertionComparator = (IDATA (*)(J9AVLTree *, J9AVLTreeNode *, J9AVLTreeNode *)) segmentInsertionComparator;
	segmentList->avlTreeData.searchComparator = (IDATA (*)(J9AVLTree *, UDATA, J9AVLTreeNode *)) segmentSearchComparator;
	segmentList->avlTreeData.portLibrary = OMRPORT_FROM_J9PORT(PORTLIB);

	return segmentList;
}

J9Class *
segmentIteratorNextClass(J9Class **nextClass)
{
	for (;;) {
		J9Class *current = *nextClass;

		if (NULL == current) {
			break;
		}
		*nextClass = current->nextClassInSegment;
#if defined(J9VM_OPT_SNAPSHOTS)
		if (J9_ARE_NO_BITS_SET(current->classFlags, J9ClassIsFrozen))
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		{
			return current;
		}
	}

	return NULL;
}

static void
prepareClassWalkState(J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader)
{
	state->vm = vm;
	if (NULL == classLoader) {
		state->nextSegment = vm->classMemorySegments->nextSegment;
	} else {
		state->nextSegment = classLoader->classSegments;
	}

	state->heapPtr = NULL;
	state->classLoader = classLoader;
}

J9Class*
allClassesStartDo(J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(vm->classTableMutex);
#endif

	prepareClassWalkState(state, vm, classLoader);

	return allClassesNextDo(state);
}

static J9Class *
allClassesNextDoInternal(J9ClassWalkState *state)
{
	J9Class *clazzPtr = NULL;
	while ((state->nextSegment != NULL) && (clazzPtr == NULL)) {
		J9MemorySegment *nextSegment = state->nextSegment;
		if ((nextSegment->type & MEMORY_TYPE_RAM_CLASS) == MEMORY_TYPE_RAM_CLASS) {
			if (state->heapPtr < nextSegment->heapBase || state->heapPtr > nextSegment->heapAlloc) {
				state->heapPtr = *(U_8 **)nextSegment->heapBase;
			}
			if (state->heapPtr != NULL) {
				clazzPtr = (J9Class *)state->heapPtr;
				state->heapPtr = (U_8 *)clazzPtr->nextClassInSegment;
			}
			if (state->heapPtr != NULL) {
				break;
			}
		}
		if (NULL == state->classLoader) {
			state->nextSegment = nextSegment->nextSegment;
		} else {
			state->nextSegment = nextSegment->nextSegmentInClassLoader;
		}
	}

	return clazzPtr;
}

J9Class *
allClassesNextDo(J9ClassWalkState *state)
{
	J9Class *clazz = allClassesNextDoInternal(state);

#if defined(J9VM_OPT_SNAPSHOTS)
	if (IS_RESTORE_RUN(state->vm)) {
		while ((NULL != clazz) && J9_ARE_ANY_BITS_SET(clazz->classFlags, J9ClassIsFrozen)) {
			clazz = allClassesNextDoInternal(state);
		}
	}
#endif /* defined(J9VM_OPT_SNAPSHOTS) */

	return clazz;
}

void allClassesEndDo(J9ClassWalkState* state)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(state->vm->classTableMutex);
#endif
}

J9Class*
allLiveClassesStartDo(J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_enter(vm->classTableMutex);
#endif

	prepareClassWalkState(state, vm, classLoader);

	return allLiveClassesNextDo(state);
}

J9Class*
allLiveClassesNextDo(J9ClassWalkState* state)
{
	J9Class* clazzPtr = NULL;
	J9Class* clazzPtrCandidate = NULL;
	J9JavaVM* vm = state->vm;
	const BOOLEAN needCheckInGC = ((J9_GC_WRITE_BARRIER_TYPE_SATB == vm->gcWriteBarrierType) || (J9_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK == vm->gcWriteBarrierType));

	do {
		clazzPtrCandidate = allClassesNextDo(state);
		clazzPtr = clazzPtrCandidate;
		if (clazzPtr != NULL) {
			J9ClassLoader *classLoader = clazzPtr->classLoader;
			if ((J9_GC_CLASS_LOADER_DEAD == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD))
					|| (J9AccClassDying == (J9CLASS_FLAGS(clazzPtr) & J9AccClassDying))
					|| (needCheckInGC && (0 == vm->memoryManagerFunctions->j9gc_objaccess_checkClassLive(vm, clazzPtr))))
			{
				/* class is not alive */
				clazzPtr = NULL;
				if (state->classLoader == NULL) {
					/* if one class is dead so all of them in this segment are */
					state->nextSegment = state->nextSegment->nextSegment;
				} else {
					/* if one class for this classloader is dead so all of them are */
					clazzPtrCandidate = NULL;
				}
			}
		}
	} while ((clazzPtr == NULL) && (clazzPtrCandidate != NULL));

	return clazzPtr;
}

void allLiveClassesEndDo(J9ClassWalkState* state)
{
#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(state->vm->classTableMutex);
#endif
}

VMINLINE static BOOLEAN
shouldSkipClassLoader(J9ClassLoaderWalkState *state, J9ClassLoader *classLoader)
{
#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	if (0 != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
		/* Dead classLoader - skip if not includeDead. */
		return 0 == (state->flags & J9CLASSLOADERWALK_INCLUDE_DEAD);
	}
#endif
	/* Live classLoader - skip if excludeLive. */
	return 0 != (state->flags & J9CLASSLOADERWALK_EXCLUDE_LIVE);
}

J9ClassLoader*
allClassLoadersStartDo (J9ClassLoaderWalkState* state, J9JavaVM* vm, UDATA flags)
{
	J9ClassLoader *walk;

	state->vm = vm;
	state->flags = flags;
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_enter(state->vm->classLoaderBlocksMutex);
#endif

	walk = pool_startDo(vm->classLoaderBlocks, &state->classLoaderBlocksWalkState);
	while ((NULL != walk) && shouldSkipClassLoader(state, walk)) {
		walk = pool_nextDo(&state->classLoaderBlocksWalkState);
	}

	return walk;
}

J9ClassLoader*
allClassLoadersNextDo (J9ClassLoaderWalkState* state)
{
	J9ClassLoader *walk;

	walk = pool_nextDo(&state->classLoaderBlocksWalkState);
	while ((NULL != walk) && shouldSkipClassLoader(state, walk)) {
		walk = pool_nextDo(&state->classLoaderBlocksWalkState);
	}

	return walk;
}

void
allClassLoadersEndDo (J9ClassLoaderWalkState* state)
{
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(state->vm->classLoaderBlocksMutex);
#endif
}

J9MemorySegment *
findMemorySegment(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA valueToFind)
{
	if (0 != (segmentList->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
		return (J9MemorySegment *) avl_search(&segmentList->avlTreeData, valueToFind);
	} else {
		return NULL;
	}

}

static IDATA
segmentInsertionComparator (J9AVLTree *tree, J9MemorySegment *insertNode, J9MemorySegment *walkNode)
{
	UDATA insertBaseAddress = (UDATA) insertNode->baseAddress;
	UDATA walkBaseAddress = (UDATA) walkNode->baseAddress;

	if (insertBaseAddress == walkBaseAddress) {
		return 0;
	} else {
		return insertBaseAddress > walkBaseAddress ? 1 : -1;
	}
}

static IDATA
segmentSearchComparator (J9AVLTree *tree, UDATA value, J9MemorySegment *searchNode)
{
	UDATA baseAddress = (UDATA) searchNode->baseAddress;

	if ((value >= baseAddress) && (value < (baseAddress + searchNode->size))) {
		return 0;
	} else {
		return baseAddress > value ? -1 : 1;
	}
}

#if defined(DEBUG_PRINT_SEGMENT_TYPES)
/* Debug helper to print all the memory types known at the
 * time this function was added.
 *
 * To enable, define DEBUG_PRINT_SEGMENT_TYPES and use this
 * as the `segmentCallback` for `allSegmentsInMemorySegmentListDo`
 */
void
printSegments(J9MemorySegment *s, void* data)
{
	uintptr_t type = s->type;
	printf("segment=%p heapBase=%p size=%d type=%lX\n", s, s->heapBase, (int)s->size, type);
	if ((type & MEMORY_TYPE_OLD) == MEMORY_TYPE_OLD) printf("MEMORY_TYPE_OLD ");
	if ((type & MEMORY_TYPE_NEW_RAM) == MEMORY_TYPE_NEW_RAM) printf("MEMORY_TYPE_NEW_RAM ");
	if ((type & MEMORY_TYPE_SCOPED) == MEMORY_TYPE_SCOPED) printf("MEMORY_TYPE_SCOPED ");
	if ((type & MEMORY_TYPE_ALLOCATED) == MEMORY_TYPE_ALLOCATED) printf("MEMORY_TYPE_ALLOCATED ");
	if ((type & MEMORY_TYPE_IMMORTAL) == MEMORY_TYPE_IMMORTAL) printf("MEMORY_TYPE_IMMORTAL ");
	if ((type & MEMORY_TYPE_DEBUG_INFO) == MEMORY_TYPE_DEBUG_INFO) printf("MEMORY_TYPE_DEBUG_INFO ");
	if ((type & MEMORY_TYPE_BASETYPE_ROM_CLASS) == MEMORY_TYPE_BASETYPE_ROM_CLASS) printf("MEMORY_TYPE_BASETYPE_ROM_CLASS ");
	if ((type & MEMORY_TYPE_DYNAMIC_LOADED_CLASSES) == MEMORY_TYPE_DYNAMIC_LOADED_CLASSES) printf("MEMORY_TYPE_DYNAMIC_LOADED_CLASSES ");
	if ((type & MEMORY_TYPE_NEW) == MEMORY_TYPE_NEW) printf("MEMORY_TYPE_NEW ");
	if ((type & MEMORY_TYPE_DISCARDABLE) == MEMORY_TYPE_DISCARDABLE) printf("MEMORY_TYPE_DISCARDABLE ");
	if ((type & MEMORY_TYPE_NUMA) == MEMORY_TYPE_NUMA) printf("MEMORY_TYPE_NUMA ");
	if ((type & MEMORY_TYPE_ROM_CLASS) == MEMORY_TYPE_ROM_CLASS) printf("MEMORY_TYPE_ROM_CLASS ");
	if ((type & MEMORY_TYPE_UNCOMMITTED) == MEMORY_TYPE_UNCOMMITTED) printf("MEMORY_TYPE_UNCOMMITTED ");
	if ((type & MEMORY_TYPE_FROM_JXE) == MEMORY_TYPE_FROM_JXE) printf("MEMORY_TYPE_FROM_JXE ");
	if ((type & MEMORY_TYPE_OLD_ROM) == MEMORY_TYPE_OLD_ROM) printf("MEMORY_TYPE_OLD_ROM ");
	if ((type & MEMORY_TYPE_SHARED_META) == MEMORY_TYPE_SHARED_META) printf("MEMORY_TYPE_SHARED_META ");
	if ((type & MEMORY_TYPE_VIRTUAL) == MEMORY_TYPE_VIRTUAL) printf("MEMORY_TYPE_VIRTUAL ");
	if ((type & MEMORY_TYPE_FIXED_RAM_CLASS) == MEMORY_TYPE_FIXED_RAM_CLASS) printf("MEMORY_TYPE_FIXED_RAM_CLASS ");
	if ((type & MEMORY_TYPE_RAM_CLASS) == MEMORY_TYPE_RAM_CLASS) printf("MEMORY_TYPE_RAM_CLASS ");
	if ((type & MEMORY_TYPE_RAM_CLASS_SUB4G) == MEMORY_TYPE_RAM_CLASS_SUB4G) printf("MEMORY_TYPE_RAM_CLASS_SUB4G ");
	if ((type & MEMORY_TYPE_RAM_CLASS_ABOVE4G_FREQUENTLY_ACCESSED) == MEMORY_TYPE_RAM_CLASS_ABOVE4G_FREQUENTLY_ACCESSED) printf("MEMORY_TYPE_RAM_CLASS_ABOVE4G_FREQUENTLY_ACCESSED ");
	if ((type & MEMORY_TYPE_RAM_CLASS_ABOVE4G_INFREQUENTLY_ACCESSED) == MEMORY_TYPE_RAM_CLASS_ABOVE4G_INFREQUENTLY_ACCESSED) printf("MEMORY_TYPE_RAM_CLASS_ABOVE4G_INFREQUENTLY_ACCESSED ");
	if ((type & MEMORY_TYPE_RAM) == MEMORY_TYPE_RAM) printf("MEMORY_TYPE_RAM ");
	if ((type & MEMORY_TYPE_FIXED) == MEMORY_TYPE_FIXED) printf("MEMORY_TYPE_FIXED ");
	if ((type & MEMORY_TYPE_JIT_SCRATCH_SPACE) == MEMORY_TYPE_JIT_SCRATCH_SPACE) printf("MEMORY_TYPE_JIT_SCRATCH_SPACE ");
	if ((type & MEMORY_TYPE_FIXED_RAM) == MEMORY_TYPE_FIXED_RAM) printf("MEMORY_TYPE_FIXED_RAM ");
	if ((type & MEMORY_TYPE_OLD_RAM) == MEMORY_TYPE_OLD_RAM) printf("MEMORY_TYPE_OLD_RAM ");
	if ((type & MEMORY_TYPE_CODE) == MEMORY_TYPE_CODE) printf("MEMORY_TYPE_CODE ");
	if ((type & MEMORY_TYPE_ROM) == MEMORY_TYPE_ROM) printf("MEMORY_TYPE_ROM ");
	if ((type & MEMORY_TYPE_CLASS_FILE_BYTES) == MEMORY_TYPE_CLASS_FILE_BYTES) printf("MEMORY_TYPE_CLASS_FILE_BYTES ");
	if ((type & MEMORY_TYPE_UNDEAD_CLASS) == MEMORY_TYPE_UNDEAD_CLASS) printf("MEMORY_TYPE_UNDEAD_CLASS ");
	if ((type & MEMORY_TYPE_JIT_PERSISTENT) == MEMORY_TYPE_JIT_PERSISTENT) printf("MEMORY_TYPE_JIT_PERSISTENT ");
	if ((type & MEMORY_TYPE_FIXEDSIZE) == MEMORY_TYPE_FIXEDSIZE) printf("MEMORY_TYPE_FIXEDSIZE ");
	if ((type & MEMORY_TYPE_DEFAULT) == MEMORY_TYPE_DEFAULT) printf("MEMORY_TYPE_DEFAULT ");
	printf("\n");
}
#endif /* DEBUG_PRINT_SEGMENT_TYPES */
