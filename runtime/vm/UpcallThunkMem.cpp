/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "j9.h"
#include "j9port.h"
#include "j9cp.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "j9consts.h"

#if JAVA_SPEC_VERSION >= 16
/**
 * @brief Flush the generated thunk to the memory after generating the thunk.
 *
 * @param vm The pointer to J9JavaVM
 * @param thunkAddress The address of the generated thunk
 * @param thunkSize The byte size of the generated thunk
 * @return void
 */
void
doneUpcallThunkGeneration(J9JavaVM *vm, void *thunkAddress, UDATA thunkSize)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL != thunkAddress) {
		/* Flash the generated thunk to the memory */
		j9cpu_flush_icache(thunkAddress, thunkSize);
	}
}

/**
 * @brief Allocate a piece of thunk memory with a given size from the existing virtual memory block
 *
 * @param vm The pointer to J9JavaVM
 * @param data the pointer to J9UpcallMetaData
 * @param thunkSize The byte size of the upcall thunk to be generated
 * @return The start address of the upcall thunk memory, or NULL on failure.
 */
void *
allocateUpcallThunkMemory(J9JavaVM *vm, J9UpcallMetaData *data, UDATA thunkSize)
{
	void *thunkAddress = NULL;
	Assert_VM_true(thunkSize > 0);

	omrthread_monitor_enter(vm->thunkHeapWrapperMutex);
	thunkAddress = subAllocateThunkFromHeap(vm, data, thunkSize);
	if ((NULL == thunkAddress) && (NULL == vm->thunkHeapWrapper)) {
		thunkAddress = allocateThunkHeap(vm, data, thunkSize);
	}
	omrthread_monitor_exit(vm->thunkHeapWrapperMutex);

	return thunkAddress;
}

/* Attempt to suballocate thunk on the heap and return the allocated address on success; otherwise return NULL */
static void *
subAllocateThunkFromHeap(J9JavaVM *vm, J9UpcallMetaData *data, UDATA thunkSize)
{
	void *subAllocThunkPtr = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_VM_subAllocateThunkFromHeap_Entry(thunkSize);

	if (NULL != vm->thunkHeapWrapper) {
		J9UpcallThunkHeapWrapper *thunkHeapWrapper = vm->thunkHeapWrapper;
		J9Heap *heap = thunkHeapWrapper->heap;
		if (NULL != heap) {
			subAllocThunkPtr = j9heap_allocate(heap, thunkSize);
			if (NULL != subAllocThunkPtr) {
				J9UpcallMetaDataList *metaDataNode = (J9UpcallMetaDataList *)j9mem_allocate_memory(sizeof(J9UpcallMetaDataList), OMRMEM_CATEGORY_VM);
				if (NULL == metaDataNode) {
					j9heap_free(heap, subAllocThunkPtr);
					Trc_VM_subAllocateThunkFromHeap_alloc_metadata_failed(subAllocThunkPtr);
					goto done;
				}
				metaDataNode->data = data;
				metaDataNode->next = thunkHeapWrapper->metaDataHead;
				thunkHeapWrapper->metaDataHead = metaDataNode;
				Trc_VM_subAllocateThunkFromHeap_suballoc_thunk_success(subAllocThunkPtr, thunkSize, heap);
			} else {
				Trc_VM_subAllocateThunkFromHeap_suballoc_thunk_failed(thunkSize, heap);
			}
		}
	}

	Trc_VM_subAllocateThunkFromHeap_Exit(subAllocThunkPtr);
	return subAllocThunkPtr;
}

/* The memory will be allocated using vmem, and an attempt will be made to
 * suballocate the thunk within that memory. If the overhead of omrheap
 * precludes using suballocation, omrheap will not be used and the memory
 * will be used directly instead.
 */
static void *
allocateThunkHeap(J9JavaVM *vm, J9UpcallMetaData *data, UDATA thunkSize)
{
	J9UpcallThunkHeapWrapper *thunkHeapWrapper = NULL;
	UDATA pageSize = j9vmem_supported_page_sizes()[0];
	void *allocMemPtr = NULL;
	J9Heap *thunkHeap = NULL;
	void *subAllocThunkPtr = NULL;
	void *returnPtr = NULL;
	J9PortVmemIdentifier vmemID;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_VM_allocateThunkHeap_Entry(thunkSize);

	/* Create the wrapper struct for the thunk heap */
	thunkHeapWrapper = (J9UpcallThunkHeapWrapper *)j9mem_allocate_memory(sizeof(J9UpcallThunkHeapWrapper), OMRMEM_CATEGORY_VM);
	if (NULL == thunkHeapWrapper) {
		Trc_VM_allocateThunkHeap_allocate_thunk_heap_wrapper_failed();
		goto done;
	}
	thunkHeapWrapper->metaDataHead = NULL;

	/* Reserve a block of memory with the fixed page size for heap creation */
	allocMemPtr = j9vmem_reserve_memory(NULL, pageSize, &vmemID,
		J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_EXECUTE | J9PORT_VMEM_MEMORY_MODE_COMMIT,
		pageSize, OMRMEM_CATEGORY_VM);
	if (NULL == allocMemPtr) {
		j9mem_free_memory(thunkHeapWrapper);
		Trc_VM_allocateThunkHeap_reserve_memory_failed(pageSize);
		goto done;
	}

	/* Initialize the allocated memory as a J9Heap */
	thunkHeap = j9heap_create(allocMemPtr, pageSize, 0);
	Assert_VM_true(NULL != thunkHeap);

	subAllocThunkPtr = j9heap_allocate(thunkHeap, thunkSize);
	if (NULL != subAllocThunkPtr) {
		J9UpcallMetaDataList *metaDataNode = (J9UpcallMetaDataList *)j9mem_allocate_memory(sizeof(J9UpcallMetaDataList), OMRMEM_CATEGORY_VM);
		if (NULL == metaDataNode) {
			j9heap_free(heap, subAllocThunkPtr);
			Trc_VM_allocateThunkHeap_alloc_metadata_failed(subAllocThunkPtr);
			goto done;
		}
		metaDataNode->data = data;
		metaDataNode->next = thunkHeapWrapper->metaDataHead;
		thunkHeapWrapper->metaDataHead = metaDataNode;

		/* Store the heap handle in J9UpcallThunkHeapWrapper if the suballocation
		 * attempt on the newly created heap is successful.
		 */
		thunkHeapWrapper->heap = thunkHeap;
		thunkHeapWrapper->heapSize = pageSize;
		returnPtr = subAllocThunkPtr;

		Trc_VM_allocateThunkHeap_suballoc_thunk_success(returnPtr, thunkSize, thunkHeap);
	} else {
		Trc_VM_allocateThunkHeap_suballoc_thunk_failed(thunkSize, heap);
	}
	thunkHeapWrapper->vmemID = vmemID;
	vm->thunkHeapWrapper = thunkHeapWrapper;

done:
	Trc_VM_allocateThunkHeap_Exit(returnPtr);
	return returnPtr;
}
#endif /* JAVA_SPEC_VERSION >= 16 */
