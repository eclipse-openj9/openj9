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

#define ROUND_TO(granularity, number) (((number) + (granularity) - 1) & ~((UDATA)(granularity) - 1))

J9MemorySegment * allocateMemorySegment (J9JavaVM *javaVM, UDATA size, UDATA type, U_32 memoryCategory);
void freeMemorySegmentListEntry (J9MemorySegmentList *segmentList, J9MemorySegment *segment);
J9MemorySegmentList *allocateMemorySegmentListWithSize (J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA sizeOfElements, U_32 memoryCategory);
U_32 memorySegmentListSize (J9MemorySegmentList *segmentList);
void freeMemorySegmentList (J9JavaVM *javaVM,J9MemorySegmentList *segmentList);
static void * allocateMemoryForSegment (J9JavaVM *javaVM,J9MemorySegment *segment, J9PortVmemParams *vmemParams, U_32 memoryCategory);
static void * allocateMemoryForCollocatedSegments (J9JavaVM *javaVM, U_32 count, J9MemorySegment **segments, J9PortVmemParams *vmemParams);
J9MemorySegmentList *allocateMemorySegmentList (J9JavaVM * javaVM, U_32 numberOfMemorySegments, U_32 memoryCategory);
J9MemorySegment * romImageNewSegment (J9JavaVM *vm, J9ROMImageHeader *header, UDATA isBaseType, J9ClassLoader *classLoader );
void freeMemorySegment (J9JavaVM *javaVM, J9MemorySegment *segment, BOOLEAN freeDescriptor);

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
static J9MemorySegment * allocateCollocatedVirtualMemorySegmentsInListsInternal(J9JavaVM *javaVM, UDATA count, J9MemorySegmentList **segmentLists, UDATA *sizes, UDATA *types, J9PortVmemParams *vmemParams, J9MemorySegment **segments);

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
#if defined(J9VM_OPT_SNAPSHOTS)
			if (IS_SNAPSHOT_RUN(javaVM) && J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_CODE))
				vmsnapshot_free_memory(segment->baseAddress);
			else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
				j9vmem_free_memory(segment->baseAddress, segment->size, &segment->vmemIdentifier);
		} else if ((useAdvise) && (MEMORY_TYPE_JIT_SCRATCH_SPACE & segment->type)) {
			j9mem_advise_and_free_memory(segment->baseAddress);
		} else if (segment->type & (MEMORY_TYPE_RAM_CLASS | MEMORY_TYPE_UNDEAD_CLASS)) {
#if defined(J9VM_OPT_SNAPSHOTS)
			if (IS_SNAPSHOTTING_ENABLED(javaVM)) {
				if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)) {
					vmsnapshot_free_memory32(segment->baseAddress);
				} else {
					vmsnapshot_free_memory(segment->baseAddress);
				}
			} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
			{
				if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)) {
					j9mem_free_memory32(segment->baseAddress);
				} else {
					j9mem_free_memory(segment->baseAddress);
				}
			}
		}
#if defined(J9VM_OPT_SNAPSHOTS)
		else if (J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_RAM) && IS_SNAPSHOTTING_ENABLED(javaVM)) {
			vmsnapshot_free_memory(segment->baseAddress);
		}
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		else {
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


void freeMemorySegmentList(J9JavaVM *javaVM,J9MemorySegmentList *segmentList)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	J9MemorySegment *currentSegment;

	do {
		currentSegment = segmentList -> nextSegment ;
		if ( currentSegment )
			freeMemorySegment(javaVM, currentSegment, 1);
	} while ( currentSegment );
	pool_kill(segmentList->segmentPool);

#if defined(J9VM_THR_PREEMPTIVE)
	if(segmentList->segmentMutex) omrthread_monitor_destroy(segmentList->segmentMutex);
#endif

#if defined(J9VM_OPT_SNAPSHOTS)
	/* Guaranteed that classMemorySegments was allocated on JVMImageHeap */
	/* TODO: In J9MemorySegmentList flags add allocation from image rather than this check. see @ref omr:j9nongenerated.h */
	if (IS_SNAPSHOTTING_ENABLED(javaVM) && ((javaVM->classMemorySegments == segmentList) || (javaVM->memorySegments == segmentList))) {
		vmsnapshot_free_memory(segmentList);
	} else if (IS_SNAPSHOTTING_ENABLED(javaVM) && IS_SNAPSHOT_RUN(javaVM) && ((javaVM->jitConfig->codeCacheList == segmentList) || (javaVM->jitConfig->dataCacheList == segmentList))) {
		vmsnapshot_free_memory(segmentList);
	} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	{
		j9mem_free_memory(segmentList);
	}
}


U_32 memorySegmentListSize (J9MemorySegmentList *segmentList)
{
		return (U_32) pool_numElements(segmentList->segmentPool);
}


/*
 *	Allocates the memory for a segment.
 *	only 2 cases are dealt with:
 *	1. MEMORY_TYPE_CODE
 *	2.	Everything else.  Regardless of the type, it will be allocated
 */
static void *
allocateMemoryForSegment(J9JavaVM *javaVM,J9MemorySegment *segment, J9PortVmemParams *vmemParams, U_32 memoryCategory)
{
	void *tmpAddr;
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

#if defined(J9VM_OPT_SNAPSHOTS)
		if (IS_SNAPSHOT_RUN(javaVM) && J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_CODE))
			tmpAddr = vmsnapshot_allocate_memory(segment->size, memoryCategory);
		else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
			tmpAddr = j9vmem_reserve_memory_ex(&segment->vmemIdentifier, vmemParams);
	} else if (J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_FIXED_RAM_CLASS)) {
		tmpAddr = j9vmem_reserve_memory_ex(&segment->vmemIdentifier, vmemParams);
		Trc_VM_virtualRAMClassAlloc(tmpAddr);
	} else if (J9_ARE_ALL_BITS_SET(segment->type, MEMORY_TYPE_RAM_CLASS)) {
#if defined(J9VM_OPT_SNAPSHOTS)
		if (IS_SNAPSHOTTING_ENABLED(javaVM)) {
			if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)) {
				tmpAddr = vmsnapshot_allocate_memory32(segment->size, memoryCategory);
			} else {
				tmpAddr = vmsnapshot_allocate_memory(segment->size, memoryCategory);
			}
		} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		{
			if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)) {
				tmpAddr = j9mem_allocate_memory32(segment->size, memoryCategory);
			} else {
				tmpAddr = j9mem_allocate_memory(segment->size, memoryCategory);
			}
		}
	}
#if defined(J9VM_OPT_SNAPSHOTS)
	else if (J9_ARE_ANY_BITS_SET(segment->type, MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_RAM) && IS_SNAPSHOTTING_ENABLED(javaVM)) {
		tmpAddr = vmsnapshot_allocate_memory(segment->size, memoryCategory);
		/* TODO: Add Memory type for allocation inside JVMImage (MEMORY_TYPE_IMAGE_ALLOCATED). see @ref omr:j9nongenerated.h */
	}
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	else {
		tmpAddr = j9mem_allocate_memory(segment->size, memoryCategory);
	}

	if(tmpAddr) {
		segment->type = segment->type | MEMORY_TYPE_ALLOCATED;
	}

	return tmpAddr;
}


/*
 *	Allocates the memory for a segment.
 *	only 2 cases are dealt with:
 *	1. MEMORY_TYPE_CODE
 *	2. MEMORY_TYPE_VIRTUAL
 */
static void *
allocateMemoryForCollocatedSegments(J9JavaVM *javaVM, U_32 count, J9MemorySegment **segments, J9PortVmemParams *vmemParams)
{
	struct J9PortVmemIdentifier *identifiers[OMRPORT_MAX_COLLOCATED_SEGMENTS_ALLOC];
	void *tmpAddr = NULL;
	PORT_ACCESS_FROM_GINFO(javaVM);
#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */

	for (int i = 0; i < count; i++) {
		identifiers[i] = &segments[i]->vmemIdentifier;
	}

	/* The order of these checks is important.
	 * MEMORY_TYPE_VIRTUAL is expected to be used along with another bit, like MEMORY_TYPE_JIT_SCRATCH_SPACE.
	 */
	if (J9_ARE_ANY_BITS_SET(segments[0]->type,  MEMORY_TYPE_CODE | MEMORY_TYPE_VIRTUAL)) {
		/* MEMORY_TYPE_CODE and MEMORY_TYPE_VIRTUAL are allocated via the virtual memory functions.
		 * Assert MEMORY_TYPE_VIRTUAL is used in combination with another bit, like MEMORY_TYPE_JIT_SCRATCH_SPACE.
		 */
		Assert_VM_true(J9_ARE_NO_BITS_SET(segments[0]->type, MEMORY_TYPE_VIRTUAL) || J9_ARE_ANY_BITS_SET(segments[0]->type, ~MEMORY_TYPE_VIRTUAL));

		for (int i = 1; i < count; i++) {
			Assert_VM_true(J9_ARE_ANY_BITS_SET(segments[i]->type,  MEMORY_TYPE_CODE | MEMORY_TYPE_VIRTUAL));
			Assert_VM_true(J9_ARE_NO_BITS_SET(segments[i]->type, MEMORY_TYPE_VIRTUAL) || J9_ARE_ANY_BITS_SET(segments[i]->type, ~MEMORY_TYPE_VIRTUAL));
		}

#if defined(J9VM_OPT_SNAPSHOTS)
		if (IS_SNAPSHOTTING_ENABLED(javaVM)
		    && J9_ARE_ANY_BITS_SET(segments[0]->type, MEMORY_TYPE_CODE | MEMORY_TYPE_CCDATA)) {
			uintptr_t totalSize = 0;
			for (int i = 0; i < count; i++) {
				totalSize += segments[i]->size;
			}
			tmpAddr = vmsnapshot_allocate_memory(totalSize, OMRMEM_CATEGORY_UNKNOWN);
		}
		else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
			tmpAddr = j9vmem_reserve_memory_collocated_ex(count, identifiers, vmemParams);
	}

	if (tmpAddr) {
		for (int i = 0; i < count; i++) {
			segments[i]->type = segments[i]->type | MEMORY_TYPE_ALLOCATED;
		}
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

J9MemorySegment * allocateCollocatedVirtualMemorySegmentsInLists(J9JavaVM *javaVM, UDATA count, J9MemorySegmentList **segmentLists, UDATA *sizes, UDATA *types, J9PortVmemParams *vmemParams, J9MemorySegment **segments)
{
	Assert_VM_true(count > 0 && count <= OMRPORT_MAX_COLLOCATED_SEGMENTS_ALLOC);
	return allocateCollocatedVirtualMemorySegmentsInListsInternal(javaVM, count, segmentLists, sizes, types, vmemParams, segments);
}

static J9MemorySegment * allocateCollocatedVirtualMemorySegmentsInListsInternal(J9JavaVM *javaVM, UDATA count, J9MemorySegmentList **segmentLists, UDATA *sizes, UDATA *types, J9PortVmemParams *vmemParams, J9MemorySegment **segments)
{
	U_8 *allocatedBase;

	for (UDATA i = 0; i < count; i++) {
		Trc_VM_allocateMemorySegmentInList_Entry(segmentLists[i], sizes[i], types[i]);
#if defined(J9VM_THR_PREEMPTIVE)
		/* Monitors are recursive, so it should be OK to lock a segment multiple times here. */
		if (segmentLists[i]->segmentMutex) {
			omrthread_monitor_enter(segmentLists[i]->segmentMutex);
		}
#endif
	}

	for (UDATA i = 0; i < count; i++) {
		segments[i] = allocateMemorySegmentListEntry(segmentLists[i]);
		if (segments[i] == NULL) {
			Trc_VM_allocateMemorySegmentInList_EntryAllocFailed(segmentLists[i], types[i]);
			for (UDATA j = 0; j < i; j++) {
				freeMemorySegmentListEntry(segmentLists[j], segments[j]);
				segments[j] = NULL;
			}
			goto done;
		}

		segments[i]->type = types[i];
		segments[i]->size = sizes[i];

		if (vmemParams != NULL) {
			vmemParams[i].byteAmount = segments[i]->size;
		}
	}

	allocatedBase = (U_8 *) allocateMemoryForCollocatedSegments(javaVM, count, segments, vmemParams);

	if (NULL == allocatedBase) {
		for (UDATA i = 0; i < count; i++) {
			Trc_VM_allocateMemorySegmentInList_AllocFailed(segmentLists[i], sizes[i], types[i]);
			freeMemorySegmentListEntry(segmentLists[i], segments[i]);
			segments[i] = NULL;
		}
	} else {
		U_8 *segmentBase = allocatedBase;
		for (UDATA i = 0; i < count; i++) {
			segments[i]->baseAddress = segmentBase;
			segments[i]->heapBase = segmentBase;
			segments[i]->heapTop = (U_8 *)&(segments[i]->heapBase)[segments[i]->size];
			segments[i]->heapAlloc = segments[i]->heapBase;

			segmentLists[i]->totalSegmentSize += segments[i]->size;

			Trc_VM_allocateMemorySegmentInList_Alloc(segments[i], segments[i]->heapBase, segments[i]->heapTop, segments[i]->type);

			if (0 != (segmentLists[i]->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
				avl_insert(&segmentLists[i]->avlTreeData, (J9AVLTreeNode *) segments[i]);
			}

			segmentBase += segments[i]->size;
		}
	}

done:
	for (UDATA i = 0; i < count; i++) {
#if defined(J9VM_THR_PREEMPTIVE)
		if (segmentLists[i]->segmentMutex) {
			omrthread_monitor_exit(segmentLists[i]->segmentMutex);
		}
#endif
		Trc_VM_allocateMemorySegmentInList_Exit(segments[i]);
	}

	return segments[0];
}


J9MemorySegmentList *allocateMemorySegmentListWithSize(J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA sizeOfElements, U_32 memoryCategory)
{
	J9MemorySegmentList *segmentList;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(javaVM);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */

#if defined(J9VM_OPT_SNAPSHOTS)
	/* Check if its a cold run and class memory segments list */
	if (IS_SNAPSHOT_RUN(javaVM) && ((J9MEM_CATEGORY_CLASSES == memoryCategory) || (OMRMEM_CATEGORY_VM == memoryCategory) || (J9MEM_CATEGORY_JIT == memoryCategory))) {
		if (NULL == (segmentList = vmsnapshot_allocate_memory(sizeof(J9MemorySegmentList), memoryCategory))) {
			return NULL;
		}
		segmentList->segmentPool = pool_new(sizeOfElements, numberOfMemorySegments, 0, 0, J9_GET_CALLSITE(), memoryCategory, POOL_FOR_PORT(VMSNAPSHOTIMPL_OMRPORT_FROM_JAVAVM(javaVM)));
		if (!(segmentList->segmentPool)) {
			vmsnapshot_free_memory(segmentList);
			return NULL;
		}
	} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	{
		if (NULL == (segmentList = j9mem_allocate_memory(sizeof(J9MemorySegmentList), memoryCategory))) {
			return NULL;
		}
		segmentList->segmentPool = pool_new(sizeOfElements, numberOfMemorySegments, 0, 0, J9_GET_CALLSITE(), memoryCategory, POOL_FOR_PORT(PORTLIB));
		if (!(segmentList->segmentPool)) {
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

J9Class*
allClassesNextDo(J9ClassWalkState* state)
{
	J9Class* clazzPtr = NULL;
	while ((state->nextSegment != NULL) && (clazzPtr == NULL)) {
		J9MemorySegment* nextSegment = state->nextSegment;
		if ((nextSegment->type & MEMORY_TYPE_RAM_CLASS) == MEMORY_TYPE_RAM_CLASS) {
			if (state->heapPtr < nextSegment->heapBase || state->heapPtr > nextSegment->heapAlloc) {
				state->heapPtr = *(U_8 **)nextSegment->heapBase;
			}
			if (state->heapPtr != NULL) {
				clazzPtr = (J9Class*)state->heapPtr;
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
	const BOOLEAN needCheckInGC = (J9_GC_WRITE_BARRIER_TYPE_REALTIME == vm->gcWriteBarrierType);

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
