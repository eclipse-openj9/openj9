/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "j9.h"
#include "j9protos.h"
#include "ut_j9vm.h"
#include "vm_internal.h"

extern "C" {

#if JAVA_SPEC_VERSION >= 16

static void * subAllocateThunkFromHeap(J9UpcallMetaData *data);
static J9UpcallThunkHeapList * allocateThunkHeap(J9UpcallMetaData *data);
static UDATA upcallMetaDataHashFn(void *key, void *userData);
static UDATA upcallMetaDataEqualFn(void *leftKey, void *rightKey, void *userData);
static UDATA freeUpcallMetaDataDoFn(J9UpcallMetaDataEntry *entry, void *userData);

/**
 * @brief Flush the generated thunk to the memory.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param thunkAddress The address of the generated thunk
 */
void
doneUpcallThunkGeneration(J9UpcallMetaData *data, void *thunkAddress)
{
	J9JavaVM * vm = data->vm;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL != thunkAddress) {
		/* Flush the generated thunk to the memory. */
		j9cpu_flush_icache(thunkAddress, data->thunkSize);
	}
}

/**
 * @brief Allocate a piece of thunk memory with a given size from the existing virtual memory block.
 *
 * @param data a pointer to J9UpcallMetaData
 * @return the start address of the upcall thunk memory, or NULL on failure
 */
void *
allocateUpcallThunkMemory(J9UpcallMetaData *data)
{
	J9JavaVM * vm = data->vm;
	void *thunkAddress = NULL;

	Assert_VM_true(data->thunkSize > 0);

	omrthread_monitor_enter(vm->thunkHeapListMutex);
	thunkAddress = subAllocateThunkFromHeap(data);
	omrthread_monitor_exit(vm->thunkHeapListMutex);

	return thunkAddress;
}

/**
 * Suballocate a piece of thunk memory on the heap and return the
 * allocated thunk address on success; otherwise return NULL.
 */
static void *
subAllocateThunkFromHeap(J9UpcallMetaData *data)
{
	J9JavaVM * vm = data->vm;
	UDATA thunkSize = data->thunkSize;
	J9UpcallThunkHeapList *thunkHeapHead = vm->thunkHeapHead;
	J9UpcallThunkHeapList *thunkHeapNode = NULL;
	void *subAllocThunkPtr = NULL;
	bool succeeded = false;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_VM_subAllocateThunkFromHeap_Entry(thunkSize);

	if (NULL == thunkHeapHead) {
retry:
		thunkHeapHead = allocateThunkHeap(data);
		if (NULL == thunkHeapHead) {
			/* The thunk address is NULL if VM fails to allocate the thunk heap. */
			goto done;
		}
	}
	thunkHeapNode = thunkHeapHead;

	omrthread_jit_write_protect_disable();
	while (!succeeded) {
		J9UpcallThunkHeapWrapper *thunkHeapWrapper = thunkHeapNode->thunkHeapWrapper;
		J9Heap *thunkHeap = thunkHeapWrapper->heap;

		subAllocThunkPtr = j9heap_allocate(thunkHeap, thunkSize);
		if (NULL != subAllocThunkPtr) {
			J9UpcallMetaDataEntry metaDataEntry = {0};

			data->thunkHeapWrapper = thunkHeapWrapper;
			metaDataEntry.thunkAddrValue = (UDATA)subAllocThunkPtr;
			metaDataEntry.upcallMetaData = data;

			/* Store the address of the generated thunk plus the corresponding metadata as an entry to
			 * a hashtable which will be used to release the memory automatically via freeUpcallStub
			 * in OpenJDK when their native scope is terminated or VM exits.
			 */
			if (NULL == hashTableAdd(thunkHeapNode->metaDataHashTable, &metaDataEntry)) {
				j9heap_free(thunkHeap, subAllocThunkPtr);
				subAllocThunkPtr = NULL;
				Trc_VM_subAllocateThunkFromHeap_suballoc_thunk_add_hashtable_failed(thunkSize, thunkHeap);
				break;
			} else {
				Trc_VM_subAllocateThunkFromHeap_suballoc_thunk_success(subAllocThunkPtr, thunkSize, thunkHeap);
				/* The list head always points to the free thunk heap if thunk
				 * is successfully allocated from the heap.
				 */
				vm->thunkHeapHead = thunkHeapNode;
				succeeded = true;
			}
		} else {
			Trc_VM_subAllocateThunkFromHeap_suballoc_thunk_failed(thunkSize, thunkHeap);
			if (thunkHeapNode->next != thunkHeapHead) {
				/* Get back to check the previous thunk heaps in the list to see whether
				 * a free thunk heap (in which part of the previously allocated thunks
				 * are already released) is available for use.
				 */
				thunkHeapNode = thunkHeapNode->next;
			} else {
				/* Attempt to allocate a new thunk heap given all thunk heaps are out of memory. */
				omrthread_jit_write_protect_enable();
				goto retry;
			}

		}
	}
	omrthread_jit_write_protect_enable();

done:

	Trc_VM_subAllocateThunkFromHeap_Exit(subAllocThunkPtr);
	return subAllocThunkPtr;
}

/**
 * Allocate a new thunk heap and add it to the end of the thunk heap list.
 *
 * Note:
 * The memory of the thunk heap will be allocated using vmem. If the overhead
 * of omrheap precludes using suballocation, omrheap will not be used and the
 * memory will be used directly instead.
 */
static J9UpcallThunkHeapList *
allocateThunkHeap(J9UpcallMetaData *data)
{
	J9JavaVM * vm = data->vm;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA pageSize = j9vmem_supported_page_sizes()[0];
	UDATA thunkSize = data->thunkSize;
	J9UpcallThunkHeapList *thunkHeapNode = NULL;
	J9UpcallThunkHeapWrapper *thunkHeapWrapper = NULL;
	J9HashTable *metaDataHashTable = NULL;
	void *allocMemPtr = NULL;
	J9Heap *thunkHeap = NULL;
	J9PortVmemIdentifier vmemID;

	Trc_VM_allocateThunkHeap_Entry(thunkSize);

	/* Create a thunk heap node of the list. */
	thunkHeapNode = (J9UpcallThunkHeapList *)j9mem_allocate_memory(sizeof(J9UpcallThunkHeapList), J9MEM_CATEGORY_VM_FFI);
	if (NULL == thunkHeapNode) {
		Trc_VM_allocateThunkHeap_allocate_thunk_heap_node_failed();
		goto done;
	}

	/* Create the wrapper struct for the thunk heap. */
	thunkHeapWrapper = (J9UpcallThunkHeapWrapper *)j9mem_allocate_memory(sizeof(J9UpcallThunkHeapWrapper), J9MEM_CATEGORY_VM_FFI);
	if (NULL == thunkHeapWrapper) {
		Trc_VM_allocateThunkHeap_allocate_thunk_heap_wrapper_failed();
		goto freeAllMemoryThenExit;
	}

	/* Reserve a block of memory with the fixed page size for heap creation. */
	allocMemPtr = j9vmem_reserve_memory(NULL, pageSize, &vmemID,
		J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_EXECUTE | J9PORT_VMEM_MEMORY_MODE_COMMIT,
		pageSize, J9MEM_CATEGORY_VM_FFI);
	if (NULL == allocMemPtr) {
		Trc_VM_allocateThunkHeap_reserve_memory_failed(pageSize);
		goto freeAllMemoryThenExit;
	}

	omrthread_jit_write_protect_disable();

	/* Initialize the allocated memory as a J9Heap. */
	thunkHeap = j9heap_create(allocMemPtr, pageSize, 0);
	if (NULL == thunkHeap) {
		Trc_VM_allocateThunkHeap_create_heap_failed(allocMemPtr, pageSize);
		goto freeAllMemoryThenExit;
	}

	/* Store the heap handle in J9UpcallThunkHeapWrapper if the thunk heap is successfully created. */
	thunkHeapWrapper->heap = thunkHeap;
	thunkHeapWrapper->heapSize = pageSize;
	thunkHeapWrapper->vmemID = vmemID;
	thunkHeapNode->thunkHeapWrapper = thunkHeapWrapper;

	/* Set the newly created thunk heap node as the list head if it doesn't exist
	 * or when there is no free thunk heap to be allocated in the list for now.
	 */
	if (NULL == vm->thunkHeapHead) {
		metaDataHashTable = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), "Upcall metadata table", 0,
				sizeof(J9UpcallMetaDataEntry), 0, 0, J9MEM_CATEGORY_VM_FFI, upcallMetaDataHashFn, upcallMetaDataEqualFn, NULL, NULL);
		if (NULL == metaDataHashTable) {
			Trc_VM_allocateThunkHeap_create_metadata_hash_table_failed();
			goto freeAllMemoryThenExit;
		}
		thunkHeapNode->metaDataHashTable = metaDataHashTable;
		vm->thunkHeapHead = thunkHeapNode;
		vm->thunkHeapHead->next = vm->thunkHeapHead;
	} else {
		/* The hashtable is shared among all thunk heaps in the list. */
		thunkHeapNode->metaDataHashTable = vm->thunkHeapHead->metaDataHashTable;
		thunkHeapNode->next = vm->thunkHeapHead->next;
		vm->thunkHeapHead->next = thunkHeapNode;
		vm->thunkHeapHead = thunkHeapNode;
	}

	omrthread_jit_write_protect_enable();

done:
	Trc_VM_allocateThunkHeap_Exit(thunkHeap);
	return thunkHeapNode;

freeAllMemoryThenExit:
	j9mem_free_memory(thunkHeapNode);
	thunkHeapNode = NULL;

	if (NULL != thunkHeapWrapper) {
		if (NULL != metaDataHashTable) {
			hashTableFree(metaDataHashTable);
			metaDataHashTable = NULL;
		}
		j9mem_free_memory(thunkHeapWrapper);
		thunkHeapWrapper = NULL;
	}
	if (NULL != allocMemPtr) {
		omrthread_jit_write_protect_enable();
		j9vmem_free_memory(allocMemPtr, pageSize, &vmemID);
		allocMemPtr = NULL;
	}
	goto done;
}

/**
 * @brief Release the thunk heap list if exists.
 *
 * @param vm a pointer to J9JavaVM
 *
 * Note:
 * This function empties the thunk heap list by cleaning up all resources
 * created via allocateUpcallThunkMemory, including the generated thunk
 * and the corresponding metadata of each entry in the hashtable.
 */
void
releaseThunkHeap(J9JavaVM *vm)
{
	J9UpcallThunkHeapList *thunkHeapHead = vm->thunkHeapHead;
	J9UpcallThunkHeapList *thunkHeapNode = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA byteAmount = j9vmem_supported_page_sizes()[0];

	if (NULL != thunkHeapHead->metaDataHashTable) {
		J9HashTable *metaDataHashTable = thunkHeapHead->metaDataHashTable;
		hashTableForEachDo(metaDataHashTable, (J9HashTableDoFn)freeUpcallMetaDataDoFn, NULL);
		hashTableFree(metaDataHashTable);
		metaDataHashTable = NULL;
	}

	thunkHeapNode = thunkHeapHead->next;
	thunkHeapHead->next = NULL;
	thunkHeapHead = thunkHeapNode;
	while (NULL != thunkHeapHead) {
		J9UpcallThunkHeapWrapper *thunkHeapWrapper = thunkHeapHead->thunkHeapWrapper;
		if (NULL != thunkHeapWrapper) {
			J9PortVmemIdentifier vmemID = thunkHeapWrapper->vmemID;
			j9vmem_free_memory(vmemID.address, byteAmount, &vmemID);
			j9mem_free_memory(thunkHeapWrapper);
			thunkHeapWrapper = NULL;
		}
		thunkHeapNode = thunkHeapHead;
		thunkHeapHead = thunkHeapHead->next;
		j9mem_free_memory(thunkHeapNode);
	}
	vm->thunkHeapHead = NULL;
}

/**
 * Compute the hash code (namely the thunk address value) for the supplied J9UpcallMetaDataEntry.
 */
static UDATA
upcallMetaDataHashFn(void *key, void *userData)
{
	return ((J9UpcallMetaDataEntry *)key)->thunkAddrValue;
}

/**
 * Determine if leftKey and rightKey refer to the same entry in the upcall metadata hashtable.
 */
static UDATA
upcallMetaDataEqualFn(void *leftKey, void *rightKey, void *userData)
{
	return ((J9UpcallMetaDataEntry *)leftKey)->thunkAddrValue == ((J9UpcallMetaDataEntry *)rightKey)->thunkAddrValue;
}

/**
 * Release the memory of the generated thunk and the metadata in the
 * specified entry of the metadata hashtable intended for upcall.
 */
static UDATA
freeUpcallMetaDataDoFn(J9UpcallMetaDataEntry *entry, void *userData)
{
	void *thunkAddr = (void *)(entry->thunkAddrValue);
	J9UpcallMetaData *metaData = entry->upcallMetaData;

	if ((NULL != thunkAddr) && (NULL != metaData)) {
		J9JavaVM *vm = metaData->vm;
		const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9VMThread *currentThread = vmFuncs->currentVMThread(vm);
		J9UpcallNativeSignature *nativeFuncSig = metaData->nativeFuncSignature;
		J9Heap *thunkHeap = metaData->thunkHeapWrapper->heap;
		PORT_ACCESS_FROM_JAVAVM(vm);

		if (NULL != nativeFuncSig) {
			j9mem_free_memory(nativeFuncSig->sigArray);
			j9mem_free_memory(nativeFuncSig);
			nativeFuncSig = NULL;
		}
		vmFuncs->j9jni_deleteGlobalRef((JNIEnv *)currentThread, metaData->mhMetaData, JNI_FALSE);
		j9mem_free_memory(metaData);
		metaData = NULL;

		if (NULL != thunkHeap) {
			omrthread_jit_write_protect_disable();
			j9heap_free(thunkHeap, thunkAddr);
			omrthread_jit_write_protect_enable();
		}
		entry->thunkAddrValue = 0;
	}
	return JNI_OK;
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
