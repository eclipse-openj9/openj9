/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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
#include "jclprots.h"
#include "j9cp.h"
#include "j9protos.h"
#include "omrlinkedlist.h"
#include "ut_j9jcl.h"

/**
 * C implementations of the sun.misc.Unsafe memory functions.
 * Called from Builder natives in J9SunMiscUnsafeNatives.
 */

static void
unsafeLinkedListAddLast(omrthread_monitor_t mutex, J9UnsafeMemoryBlock** listHead, J9UnsafeMemoryBlock** memBlock);

static void
unsafeLinkedListRemove(omrthread_monitor_t mutex, J9UnsafeMemoryBlock** listHead, J9UnsafeMemoryBlock** memBlock);

/**
 * Initialize the tracking system for unsafe memory allocations.
 * @return JNI_OK on success, or a JNI_ERR* constant on failure.
 */
UDATA
initializeUnsafeMemoryTracking(J9JavaVM* vm)
{
		if (omrthread_monitor_init_with_name(&vm->unsafeMemoryTrackingMutex, 0, "Unsafe memory allocation tracking")) {
			return JNI_ENOMEM;
		}
		return JNI_OK;
}


 /**
  * Free any remaining unsafe memory blocks and the mutex that
  * guards the list.
  */
void
freeUnsafeMemory(J9JavaVM* vm)
{
	if (vm->unsafeMemoryTrackingMutex != NULL) {
		PORT_ACCESS_FROM_VMC(vm);
		J9UnsafeMemoryBlock *memBlock;

		while (NULL != (memBlock = vm->unsafeMemoryListHead)) {
			J9_LINKED_LIST_REMOVE(vm->unsafeMemoryListHead, memBlock);
			j9mem_free_memory((void *) memBlock);
		}
		omrthread_monitor_destroy(vm->unsafeMemoryTrackingMutex);
	}
}


/**
 * Allocates a block and adds it to a doubly-linked list for tracking purposes.  Actual size of the block is
 * requested size plus space for two pointers.
 * Allocating 0 bytes returns a valid pointer (cf. unsafeReallocateMemory).
 * @param vmThread thread pointer
 * @param size number of bytes to allocate
 * @return pointer to allocated buffer (after the linked list pointers).  Null if allocation failure. 
 */
void* 
unsafeAllocateMemory(J9VMThread* vmThread, UDATA size, UDATA throwExceptionOnFailure) 
{
	U_8* newAddress;
	J9UnsafeMemoryBlock *memBlock;
	omrthread_monitor_t mutex = vmThread->javaVM->unsafeMemoryTrackingMutex;
	
	PORT_ACCESS_FROM_VMC(vmThread);
	size = size + offsetof(J9UnsafeMemoryBlock, data); /* leave space for the linked list pointers */ 
	Trc_JCL_sun_misc_Unsafe_allocateMemory_Entry(vmThread, size);
	memBlock = (J9UnsafeMemoryBlock*) j9mem_allocate_memory(size, J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATE);
	if (0 == memBlock) {
		if (throwExceptionOnFailure) {
			vmThread->javaVM->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
			/* the builder code actually throws the exception */
		}
		Trc_JCL_sun_misc_Unsafe_allocateMemory_OutOfMemory(vmThread);
		return 0;	
	}
	
	omrthread_monitor_enter(mutex);
	J9_LINKED_LIST_ADD_LAST(vmThread->javaVM->unsafeMemoryListHead, memBlock);
	omrthread_monitor_exit(mutex);
	newAddress = (void*) &(memBlock->data);
	Trc_JCL_sun_misc_Unsafe_allocateMemory_Exit(vmThread, newAddress);
	return newAddress;
}

/**
 * Removes the block from the list.  Note that the pointers are located immediately before the oldAddress value.
 * @param vmThread thread pointer
 * @param pointer to buffer. May be null.
 */
void
unsafeFreeMemory(J9VMThread* vmThread, void* oldAddress) 
{
	J9UnsafeMemoryBlock *memBlock;
		
	PORT_ACCESS_FROM_VMC(vmThread);
	Trc_JCL_sun_misc_Unsafe_freeMemory_Entry(vmThread, oldAddress);

	if (0 != oldAddress) {
		omrthread_monitor_t mutex = vmThread->javaVM->unsafeMemoryTrackingMutex;
		
		memBlock = (J9UnsafeMemoryBlock*) ((U_8*) oldAddress - offsetof(J9UnsafeMemoryBlock, data));
		omrthread_monitor_enter(mutex);
		J9_LINKED_LIST_REMOVE(vmThread->javaVM->unsafeMemoryListHead, memBlock);
		omrthread_monitor_exit(mutex);
		j9mem_free_memory(memBlock);
	}
	Trc_JCL_sun_misc_Unsafe_freeMemory_Exit(vmThread);
	
	return;
}

/**
 * @param vmThread thread pointer
 * @param pointer to old buffer, advanced past the linked list pointers.
 * @param size new size of buffer to allocate
 * @return pointer to new buffer.  Null if allocation failure. 
 * Reallocating 0 bytes returns a null pointer (cf. unsafeAllocateMemory), advanced past the linked list pointers.
 */
void* 
unsafeReallocateMemory(J9VMThread* vmThread, void* oldAddress, UDATA size) 
{
	void* newAddress = 0;
	J9UnsafeMemoryBlock *memBlock;
	UDATA newSize;
	omrthread_monitor_t mutex = vmThread->javaVM->unsafeMemoryTrackingMutex;
	
	PORT_ACCESS_FROM_VMC(vmThread);
	Trc_JCL_sun_misc_Unsafe_reallocateMemory_Entry(vmThread, oldAddress, size);
	
	newSize = size + offsetof(J9UnsafeMemoryBlock, data); /* leave space for the linked list pointers */ 
	if (0 == oldAddress) {
		memBlock = (J9UnsafeMemoryBlock*) oldAddress;
	} else {
		memBlock = (J9UnsafeMemoryBlock*) ((U_8*) oldAddress - offsetof(J9UnsafeMemoryBlock, data));
	
		omrthread_monitor_enter(mutex);
		J9_LINKED_LIST_REMOVE(vmThread->javaVM->unsafeMemoryListHead, memBlock);
		omrthread_monitor_exit(mutex);
	}
	
	/* memBlock may be null at this point */
	if (0 != size) {
		memBlock = (J9UnsafeMemoryBlock*) j9mem_reallocate_memory(memBlock, newSize, J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATE);
		if (0 == memBlock) {
			Trc_JCL_sun_misc_Unsafe_reallocateMemory_OutOfMemory(vmThread);
			vmThread->javaVM->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
			/* the builder code actually throws the exception */
			return 0;
		}
		omrthread_monitor_enter(mutex);
		J9_LINKED_LIST_ADD_LAST(vmThread->javaVM->unsafeMemoryListHead, memBlock);
		omrthread_monitor_exit(mutex);	
		newAddress = (void*) &(memBlock->data);
	} else {
		/* emulate old behaviour of returning null pointer if new size is 0 */
		j9mem_free_memory((void *) memBlock);
	}
	Trc_JCL_sun_misc_Unsafe_reallocateMemory_Exit(vmThread, newAddress);
	return (void *) newAddress;
}

/**
 * Insert a memory block into the linked list.
 * @param mutex that guards the linked list
 * @param head of the linked list
 * @param pointer to the memory block.
 */
static void
unsafeLinkedListAddLast(omrthread_monitor_t mutex, J9UnsafeMemoryBlock** listHead, J9UnsafeMemoryBlock** memBlock)
{
	omrthread_monitor_enter(mutex);
	J9_LINKED_LIST_ADD_LAST(*listHead, *memBlock);
	omrthread_monitor_exit(mutex);
}

/**
 * Remove a memory block from the linked list.
 * @param mutex that guards the linked list
 * @param head of the linked list
 * @param pointer to the memory block.
 */
static void
unsafeLinkedListRemove(omrthread_monitor_t mutex, J9UnsafeMemoryBlock** listHead, J9UnsafeMemoryBlock** memBlock)
{
	omrthread_monitor_enter(mutex);
	J9_LINKED_LIST_REMOVE(*listHead, *memBlock);
	omrthread_monitor_exit(mutex);
}


/**
 * Allocates a block and adds it to a doubly-linked list for tracking purposes.  Actual size of the block is
 * requested size plus space for two pointers.
 * Allocating 0 bytes returns a valid pointer (cf. unsafeReallocateDBBMemory).
 * @param vmThread thread pointer
 * @param size number of bytes to allocate
 * @return pointer to allocated buffer (after the linked list pointers).  Null if allocation failure.
 */
void*
unsafeAllocateDBBMemory(J9VMThread* vmThread, UDATA size, UDATA throwExceptionOnFailure)
{
	U_8* newAddress = NULL;
	J9UnsafeMemoryBlock* memBlock = NULL;
	omrthread_monitor_t mutex = NULL;
	J9JavaVM* vm = vmThread->javaVM;

	PORT_ACCESS_FROM_VMC(vmThread);

	{
		/* still use original mutex when it is not in the cloud mode */
		mutex = vm->unsafeMemoryTrackingMutex;
	}

	size = size + offsetof(J9UnsafeMemoryBlock, data); /* leave space for the linked list pointers */
	Trc_JCL_sun_misc_Unsafe_allocateDBBMemory_Entry(vmThread, size);
	memBlock = (J9UnsafeMemoryBlock*) j9mem_allocate_memory(size, J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATEDBB);
	if (0 == memBlock) {
		if (throwExceptionOnFailure) {
			vm->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
			/* the builder code actually throws the exception */
		}
		Trc_JCL_sun_misc_Unsafe_allocateDBBMemory_OutOfMemory(vmThread);
		return 0;
	}

	{
		/* still use the original list to insert DBB memory block when it is not in cloud mode */
		unsafeLinkedListAddLast(mutex, &vm->unsafeMemoryListHead, &memBlock);
	}

	newAddress = (void*) &(memBlock->data);
	Trc_JCL_sun_misc_Unsafe_allocateDBBMemory_Exit(vmThread, newAddress);
	return newAddress;
}

/**
 * Removes the block from the list.  Note that the pointers are located immediately before the oldAddress value.
 * @param vmThread thread pointer
 * @param pointer to buffer. May be null.
 */
void
unsafeFreeDBBMemory(J9VMThread* vmThread, void* oldAddress)
{
	J9UnsafeMemoryBlock *memBlock = NULL;
	omrthread_monitor_t mutex = NULL;
	J9JavaVM* vm = vmThread->javaVM;

	PORT_ACCESS_FROM_VMC(vmThread);
	Trc_JCL_sun_misc_Unsafe_freeDBBMemory_Entry(vmThread, oldAddress);

	if (0 != oldAddress) {
		memBlock = (J9UnsafeMemoryBlock*) ((U_8*) oldAddress - offsetof(J9UnsafeMemoryBlock, data));

		{
			/* release DBB block from the original list when it is not in cloud mode */
			mutex = vm->unsafeMemoryTrackingMutex;
			unsafeLinkedListRemove(mutex, &vm->unsafeMemoryListHead, &memBlock);
			j9mem_free_memory(memBlock);
		}
	}
	Trc_JCL_sun_misc_Unsafe_freeDBBMemory_Exit(vmThread);

	return;
}

/**
 * @param vmThread thread pointer
 * @param pointer to old buffer, advanced past the linked list pointers.
 * @param size new size of buffer to allocate
 * @return pointer to new buffer.  Null if allocation failure.
 * Reallocating 0 bytes returns a null pointer (cf. unsafeAllocateDBBMemory), advanced past the linked list pointers.
 */
void*
unsafeReallocateDBBMemory(J9VMThread* vmThread, void* oldAddress, UDATA size)
{
	void* newAddress = 0;
	J9UnsafeMemoryBlock *memBlock = NULL;
	UDATA newSize = 0;
	omrthread_monitor_t mutex = NULL;
	J9JavaVM* vm = vmThread->javaVM;

	PORT_ACCESS_FROM_VMC(vmThread);
	Trc_JCL_sun_misc_Unsafe_reallocateDBBMemory_Entry(vmThread, oldAddress, size);

	{
		/* still use original mutex when it is not in the cloud mode */
		mutex = vm->unsafeMemoryTrackingMutex;
	}

	newSize = size + offsetof(J9UnsafeMemoryBlock, data); /* leave space for the linked list pointers */
	if (0 == oldAddress) {
		memBlock = (J9UnsafeMemoryBlock*) oldAddress;
	} else {
		memBlock = (J9UnsafeMemoryBlock*) ((U_8*) oldAddress - offsetof(J9UnsafeMemoryBlock, data));

		{
			/* release DBB block from the original list when it is not in cloud mode */
			unsafeLinkedListRemove(mutex, &vm->unsafeMemoryListHead, &memBlock);
		}
	}

	/* memBlock may be null at this point */
	if (0 != size) {
		memBlock = (J9UnsafeMemoryBlock*) j9mem_reallocate_memory(memBlock, newSize, J9MEM_CATEGORY_SUN_MISC_UNSAFE_ALLOCATEDBB);
		if (0 == memBlock) {
			Trc_JCL_sun_misc_Unsafe_reallocateDBBMemory_OutOfMemory(vmThread);
			vmThread->javaVM->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
			/* the builder code actually throws the exception */
			return 0;
		}

		{
			/* still use the original list to insert DBB memory block when it is not in cloud mode */
			unsafeLinkedListAddLast(mutex, &vm->unsafeMemoryListHead, &memBlock);
		}

		newAddress = (void*) &(memBlock->data);
	} else {
		/* emulate old behaviour of returning null pointer if new size is 0 */
		j9mem_free_memory((void *) memBlock);
	}
	Trc_JCL_sun_misc_Unsafe_reallocateDBBMemory_Exit(vmThread, newAddress);
	return (void *) newAddress;
}
