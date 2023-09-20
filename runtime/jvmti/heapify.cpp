/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#include "jvmtiHelpers.h"
#include "objhelp.h"
#include "HeapIteratorAPI.h"
#include "MethodMetaData.h"
#include "Bits.hpp"
#include "ObjectAccessBarrierAPI.hpp"
#include "VMAccess.hpp"

extern "C" {

/* Representation of a stack-allocated object that remains correct if the stack grows */
typedef struct {
	J9VMThread *vmThread;
	UDATA *offset;
} J9JVMTIStackObject;

/* Map of stack-allocated object to its heap equivalent */
typedef struct {
	J9JVMTIStackObject stackObject;
	j9object_t heapObject;
} J9JVMTIObjectMap;

/* User data for cloneObject call */
typedef struct {
	J9VMThread *stackObjectThread;
	J9HashTable *objectMap;
} J9JVMTIObjectMapData;

/**
 * Determine if an object pointer is stack-allocated in a thread.
 *
 * @param vmThread - thread to query
 * @param obj - the object pointer
 *
 * @return true if the object is stack allocated, false if not
 */
static VMINLINE bool
objectIsStackAllocated(J9VMThread *vmThread, j9object_t obj)
{
	J9JavaStack *javaStack = vmThread->stackObject;
	return ((J9JavaStack*)obj >= (javaStack + 1)) && ((UDATA*)obj < javaStack->end);
}

/**
 * Initialize an object map exemplar for a stack-allocated object
 *
 * @param objectThread - thread containing stack-allocated object
 * @param obj - the object pointer
 * @param exemplar - the exemplar to initialize
 */
static VMINLINE void
initializeExemplar(J9VMThread *objectThread, j9object_t obj, J9JVMTIObjectMap *exemplar)
{
	exemplar->stackObject.vmThread = objectThread;
	exemplar->stackObject.offset = CONVERT_TO_RELATIVE_STACK_OFFSET(objectThread, obj);
	exemplar->heapObject = NULL;
}

/**
 * Derive the stack-allocated object pointer from an object map entry
 *
 * @param entry - the object map entry
 *
 * @return the stack-allocated object pointer
 */
static VMINLINE j9object_t
stackObjectFromMapEntry(J9JVMTIObjectMap *entry)
{
	J9JVMTIStackObject *stackObject = &entry->stackObject;
	return (j9object_t)CONVERT_FROM_RELATIVE_STACK_OFFSET(stackObject->vmThread, stackObject->offset);
}

/**
 * Hash function for the object map hash table
 *
 * @param entry - the object map entry
 * @param userData - unused
 *
 * @return the hash of the entry
 */
static UDATA
heapifyStackAllocatedObjects_HashFn(void *entry, void *userData)
{
	J9JVMTIObjectMap *key = (J9JVMTIObjectMap*)entry;

	return (UDATA)key->stackObject.vmThread ^ (UDATA)key->stackObject.offset;
}

/**
 * Comparator function for the object map hash table
 *
 * @param lhsEntry - first object map entry
 * @param lhsEntry - second object map entry
 * @param userData - unused
 *
 * @return TRUE if the entries refer to the same stack-allocated object, FALSE if not
 */
static UDATA
heapifyStackAllocatedObjects_HashEqualFn(void *lhsEntry, void *rhsEntry, void *userData)
{
	J9JVMTIObjectMap *e1 = (J9JVMTIObjectMap*)lhsEntry;
	J9JVMTIObjectMap *e2 = (J9JVMTIObjectMap*)rhsEntry;

	return (e1->stackObject.vmThread == e2->stackObject.vmThread) && (e1->stackObject.offset == e2->stackObject.offset);
}

/**
 * Stackwalker object slot iterator to add stack-allocated objects
 *
 * @param currentThread - the current thread
 * @param walkState - the stack walk state
 * @param objectSlot - pointer to slot containing an object pointer
 * @param stackLocation - pointer to the slot in the stack containing possibly compressed object pointer
 */
static void
addStackAllocatedObjectsIterator(J9VMThread *currentThread, J9StackWalkState *walkState, j9object_t *objectSlot, const void *stackLocation)
{
	j9object_t obj = *objectSlot;
	J9VMThread *stackObjectThread = walkState->walkThread;
	if (objectIsStackAllocated(stackObjectThread, obj)) {
		J9HashTable *objectMap = (J9HashTable*)walkState->userData1;
		J9JVMTIObjectMap exemplar;
		initializeExemplar(stackObjectThread, obj, &exemplar);
		if (NULL == hashTableAdd(objectMap, &exemplar)) {
			walkState->userData2 = (void*)JVMTI_ERROR_OUT_OF_MEMORY;
		}
	}
}

/**
 * Stackwalker object slot iterator to replace stack-allocated objects with their heap equivalents
 *
 * @param currentThread - the current thread
 * @param walkState - the stack walk state
 * @param objectSlot - pointer to slot containing an object pointer
 * @param stackLocation - pointer to the slot in the stack containing possibly compressed object pointer
 */
static void
replaceStackAllocatedObjectsIterator(J9VMThread *currentThread, J9StackWalkState *walkState, j9object_t *objectSlot, const void *stackLocation)
{
	J9HashTable *objectMap = (J9HashTable*)walkState->userData1;
	j9object_t obj = *objectSlot;
	J9VMThread *stackObjectThread = walkState->walkThread;
	if (objectIsStackAllocated(stackObjectThread, obj)) {
		J9JVMTIObjectMap exemplar;
		initializeExemplar(stackObjectThread, obj, &exemplar);
		J9JVMTIObjectMap *entry = (J9JVMTIObjectMap*)hashTableFind(objectMap, &exemplar);
		if (NULL != entry) {
//			PORT_ACCESS_FROM_VMC(currentThread);
//			j9tty_printf(PORTLIB, "<%p> Replacing %p with %p in slot %p\n", walkState->walkThread, obj, entry->heapObject, stackLocation);
			*objectSlot = entry->heapObject;
		}
	}
}

/**
 * Allocate the heap replacement for a stack-allocated object
 *
 * @param currentThread - the current thread
 * @param object - the stack-allocated object
 *
 * @return New heap object or NULL on allocation failure
 */
static j9object_t
allocateHeapObject(J9VMThread *currentThread, j9object_t object)
{
	j9object_t heapObject = NULL;
	J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, object);
	J9MemoryManagerFunctions const * const mmFuncs = currentThread->javaVM->memoryManagerFunctions;
	if (J9_ARE_ANY_BITS_SET(J9CLASS_FLAGS(objectClass), J9AccClassArray)) {
		U_32 size = J9INDEXABLEOBJECT_SIZE(currentThread, object);
		heapObject = mmFuncs->J9AllocateIndexableObject(currentThread, objectClass, size, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	} else {
		heapObject = mmFuncs->J9AllocateObject(currentThread, objectClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	}
	return heapObject;
}

/**
 * Allocate the heap replacements for all discovered stack-allocated objects
 *
 * @param currentThread - the current thread
 * @param objectMap - the object map
 *
 * @return Array of heap objects or NULL on allocation failure
 *
 * The objects in the returned array are ordered by the object map iteration.
 */
static j9object_t
allocateHeapObjects(J9VMThread *currentThread, J9HashTable *objectMap)
{
	U_32 objectCount = hashTableGetCount(objectMap);
	J9JavaVM *vm = currentThread->javaVM;
	J9Class *jlObject = J9VMJAVALANGOBJECT_OR_NULL(vm);
	J9Class *arrayClass = jlObject->arrayClass;
	j9object_t objectArray = vm->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, objectCount, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL != objectArray) {
		U_32 index = 0;
		J9HashTableState hashWalkState;
		J9JVMTIObjectMap *entry = (J9JVMTIObjectMap*)hashTableStartDo(objectMap, &hashWalkState);
		PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, objectArray);
		while (NULL != entry) {
			j9object_t heapObject = allocateHeapObject(currentThread, stackObjectFromMapEntry(entry));
			if (NULL == heapObject) {
				objectArray = NULL;
				break;
			}
			objectArray = PEEK_OBJECT_IN_SPECIAL_FRAME(currentThread, 0);
			J9JAVAARRAYOFOBJECT_STORE(currentThread, objectArray, index, heapObject);
			index += 1;
			entry = (J9JVMTIObjectMap*)hashTableNextDo(&hashWalkState);
		}
		DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	}
	return objectArray;
}

/**
 * Map function for replacement of stack-allocated objects with heap equivalents during clone
 *
 * @param currentThread - the current thread
 * @param obj - potentially stack-allocated object
 * @param objectMapData - user data from clone function
 *
 * @return obj if obj is a heap object, the replacement if obj is stack-allocated
 */
static j9object_t
objectMapFunction(J9VMThread *currentThread, j9object_t obj, void *objectMapData)
{
	J9JVMTIObjectMapData *data = (J9JVMTIObjectMapData*)objectMapData;
	J9VMThread *stackObjectThread = data->stackObjectThread;
	if (objectIsStackAllocated(stackObjectThread, obj)) {
		J9JVMTIObjectMap exemplar;
		initializeExemplar(stackObjectThread, obj, &exemplar);
		J9JVMTIObjectMap *entry = (J9JVMTIObjectMap*)hashTableFind(data->objectMap, &exemplar);
		if (NULL != entry) {
			obj = entry->heapObject;
		}
	}
	return obj;
}

/**
 * Copy stack-allocated object contents to the heap equivalent, replacing pointers to stack-allocated
 * objects with their heap equivalents. If there is an inline lockword in the object, it will be copied.
 *
 * @param currentThread - the current thread
 * @param entry - object map entry
 * @param objecMap - the object map
 */
static void
copyObjectContents(J9VMThread *currentThread, J9JVMTIObjectMap *entry, J9HashTable *objectMap)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrierAPI(currentThread);
	j9object_t stackObject = stackObjectFromMapEntry(entry);
	J9VMThread *stackObjectThread = entry->stackObject.vmThread;
	j9object_t heapObject = entry->heapObject;
	J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, stackObject);
	J9JVMTIObjectMapData objectMapData = { stackObjectThread, objectMap };
	if (J9_ARE_ANY_BITS_SET(J9CLASS_FLAGS(objectClass), J9AccClassArray)) {
		U_32 size = J9INDEXABLEOBJECT_SIZE(currentThread, stackObject);
		objectAccessBarrierAPI.cloneArray(currentThread, stackObject, heapObject, objectClass, size, objectMapFunction, (void*)&objectMapData, false);
	} else {
		objectAccessBarrierAPI.cloneObject(currentThread, stackObject, heapObject, objectClass, objectMapFunction, (void*)&objectMapData, false);
	}
}

/**
 * Copy all stack-allocated object contents to their heap equivalents, replacing pointers to stack-allocated
 * objects with their heap equivalents.
 *
 * @param currentThread - the current thread
 * @param objecMap - the object map
 * @param objectArray - array of heap objects
 */
static void
copyObjects(J9VMThread *currentThread, J9HashTable *objectMap, j9object_t objectArray)
{
	J9HashTableState hashWalkState;
	U_32 index = 0;
	J9JVMTIObjectMap *entry = (J9JVMTIObjectMap*)hashTableStartDo(objectMap, &hashWalkState);
	while (NULL != entry) {
		entry->heapObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, objectArray, index);
		index += 1;
		entry = (J9JVMTIObjectMap*)hashTableNextDo(&hashWalkState);
	}
	entry = (J9JVMTIObjectMap*)hashTableStartDo(objectMap, &hashWalkState);
	while (NULL != entry) {
		copyObjectContents(currentThread, entry, objectMap);
		entry = (J9JVMTIObjectMap*)hashTableNextDo(&hashWalkState);
	}
}

jvmtiError
heapifyStackAllocatedObjects(J9VMThread *currentThread, UDATA safePoint)
{
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9JavaVM *vm = currentThread->javaVM;
	J9HashTable *objectMap = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), J9_GET_CALLSITE(), 256, sizeof(J9JVMTIObjectMap), 0, 0, J9MEM_CATEGORY_JVMTI,
			heapifyStackAllocatedObjects_HashFn, heapifyStackAllocatedObjects_HashEqualFn, NULL, NULL);

	if (NULL == objectMap) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		J9VMThread *walkThread = currentThread;
		J9StackWalkState walkState = {0};
		j9object_t objectArray = NULL;

		walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS;
		walkState.userData1 = (void*)objectMap;
		walkState.userData2 = (void*)JVMTI_ERROR_NONE;
		walkState.objectSlotWalkFunction = addStackAllocatedObjectsIterator;
		do {
			if (VM_VMHelpers::threadCanRunJavaCode(walkThread)) {
				/* Do not mark the current thread */
				if (walkThread != currentThread) {
					VM_VMAccess::setPublicFlags(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_HCR);
				}
				walkState.walkThread = walkThread;
				vm->walkStackFrames(currentThread, &walkState);
				rc = (jvmtiError)(UDATA)walkState.userData2;
				if (JVMTI_ERROR_NONE != rc) {
					goto unmarkThreads;
				}
			}
			walkThread = walkThread->linkNext;
		} while (walkThread != currentThread);

		if (safePoint) {
			vm->internalVMFunctions->releaseSafePointVMAccess(currentThread);
		} else {
			vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
		}

		objectArray = allocateHeapObjects(currentThread, objectMap);
		if (NULL == objectArray) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, objectArray);
		}

		if (safePoint) {
			vm->internalVMFunctions->acquireSafePointVMAccess(currentThread);
		} else {
			vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		}

		if (JVMTI_ERROR_NONE == rc) {
			objectArray = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			copyObjects(currentThread, objectMap, objectArray);
		}

unmarkThreads:
		walkState.objectSlotWalkFunction = replaceStackAllocatedObjectsIterator;
		do {
			if (VM_VMHelpers::threadCanRunJavaCode(walkThread)) {
				VM_VMAccess::clearPublicFlags(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_HCR);
				if (JVMTI_ERROR_NONE == rc) {
					walkState.walkThread = walkThread;
					vm->walkStackFrames(currentThread, &walkState);
				}
			}
			walkThread = walkThread->linkNext;
		} while (walkThread != currentThread);
		hashTableFree(objectMap);
	}

	return rc;
}

} /* extern "C" */
