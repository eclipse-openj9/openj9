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

#ifndef J9ACCESSBARRIER_H
#define J9ACCESSBARRIER_H

#include "j9cfg.h"
#include "j9comp.h"
#include "omr.h"

/*
 * Define type used to represent the class in the 'clazz' slot of an object's header
 */
#if defined(OMR_GC_FULL_POINTERS)
typedef UDATA j9objectclass_t;
#else /* OMR_GC_FULL_POINTERS */
typedef U_32 j9objectclass_t;
#endif /* OMR_GC_FULL_POINTERS */

/*
 * Define type used to represent the lock in the 'monitor' slot of an object's header
 */
#if defined(OMR_GC_FULL_POINTERS)
typedef UDATA j9objectmonitor_t;
#else /* OMR_GC_FULL_POINTERS */
typedef U_32 j9objectmonitor_t;
#endif /* OMR_GC_FULL_POINTERS */

/*
 * Define the type system for object pointers.
 * See VMDESIGN 1016
 */

struct J9Object;
struct J9IndexableObject;

/**
 * Root references.  
 * Root references to objects include stack pointers, JNI global reference pool entries, Java 
 * static fields, register values held by the user, etc. etc.  In concrete terms, from a compressed 
 * pointer perspective, these are the 64 bit decompressed values of objects.  Root references will 
 * always be pointer width in size on the given platform (i.e., sizeof(UDATA))
 */
typedef struct J9Object* j9object_t;
typedef struct J9IndexableObject* j9array_t;

/**
 * Object field references.  
 * Object reference fields that point to other objects require a different type from root references.  
 * In concrete terms (compressed pointers), this is the 32 bit representation of a pointer.
 */
#if defined(OMR_GC_FULL_POINTERS)
typedef UDATA fj9object_t;
typedef UDATA fj9array_t;
#else /* OMR_GC_FULL_POINTERS */
typedef U_32 fj9object_t;
typedef U_32 fj9array_t;
#endif /* OMR_GC_FULL_POINTERS */

/**
 * Internal references.  
 * Some modules need a structure type for objects that describe the header to effectively perform work.  
 * These modules are very limited and include the GC (and possibly the JIT).
 */
typedef struct J9Object* mm_j9object_t;
typedef struct J9IndexableObject* mm_j9array_t;

/*
 * Define macros used with access barriers
 */
#define J9VMTHREAD_JAVAVM(vmThread) ((vmThread)->javaVM)

/* There may be an unacceptable performance penalty in the TLS lookup of the current thread, so this conversion should be used sparingly. */
#define J9JAVAVM_VMTHREAD(javaVM) (javaVM->internalVMFunctions->currentVMThread(javaVM))

/* Internal macros - Do not use outside this file */
#if defined(OMR_GC_COMPRESSED_POINTERS)
#define J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, pointer) ((void*)((UDATA)(pointer) << (javaVM)->compressedPointersShift))
#define J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, pointer) ((U_32)((UDATA)(pointer) >> (javaVM)->compressedPointersShift))
#else
#define J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, pointer) ((void*)(UDATA)(pointer))
#define J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, pointer) ((U_32)(UDATA)(pointer))
#endif /* OMR_GC_COMPRESSED_POINTERS */
#define J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, pointer) J9_CONVERT_POINTER_FROM_TOKEN_VM__(J9VMTHREAD_JAVAVM(vmThread),pointer)
#define J9_CONVERT_POINTER_TO_TOKEN__(vmThread, pointer) J9_CONVERT_POINTER_TO_TOKEN_VM__(J9VMTHREAD_JAVAVM(vmThread),pointer)

#define J9JAVAARRAYDISCONTIGUOUS_EA(vmThread, array, index, elemType) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (&(((elemType*)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, ((U_32*)(((UDATA)(array)) + (vmThread)->discontiguousIndexableHeaderSize))[((U_32)index)/(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize/sizeof(elemType))]))[((U_32)index)%(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize/sizeof(elemType))])) \
		: (&(((elemType*)(((UDATA*)(((UDATA)(array)) + (vmThread)->discontiguousIndexableHeaderSize))[((U_32)index)/(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize/sizeof(elemType))]))[((U_32)index)%(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize/sizeof(elemType))])))

#define J9JAVAARRAYDISCONTIGUOUS_EA_VM(javaVM, array, index, elemType) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? (&(((elemType*)J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, ((U_32*)(((UDATA)(array)) + (javaVM)->discontiguousIndexableHeaderSize))[((U_32)index)/((javaVM)->arrayletLeafSize/sizeof(elemType))]))[((U_32)index)%((javaVM)->arrayletLeafSize/sizeof(elemType))])) \
		: (&(((elemType*)(((UDATA*)(((UDATA)(array)) + (javaVM)->discontiguousIndexableHeaderSize))[((U_32)index)/((javaVM)->arrayletLeafSize/sizeof(elemType))]))[((U_32)index)%((javaVM)->arrayletLeafSize/sizeof(elemType))])))

#define J9JAVAARRAYCONTIGUOUS_WITHOUT_DATAADDRESS_EA(vmThread, array, index, elemType) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (&((elemType*)((((J9IndexableObjectContiguousCompressed *)(array)) + 1)))[index]) \
		: (&((elemType*)((((J9IndexableObjectContiguousFull *)(array)) + 1)))[index]))

#define J9JAVAARRAYCONTIGUOUS_WITHOUT_DATAADDRESS_EA_VM(javaVM, array, index, elemType) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? (&((elemType*)((((J9IndexableObjectContiguousCompressed *)(array)) + 1)))[index]) \
		: (&((elemType*)((((J9IndexableObjectContiguousFull *)(array)) + 1)))[index]))

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
#define J9JAVAARRAYCONTIGUOUS_WITH_DATAADDRESS_EA(vmThread, array, index, elemType) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (&((elemType*)((((J9IndexableObjectWithDataAddressContiguousCompressed *)(array))->dataAddr)))[index]) \
		: (&((elemType*)((((J9IndexableObjectWithDataAddressContiguousFull *)(array))->dataAddr)))[index]))

#define J9JAVAARRAYCONTIGUOUS_WITH_DATAADDRESS_EA_VM(javaVM, array, index, elemType) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? (&((elemType*)((((J9IndexableObjectWithDataAddressContiguousCompressed *)(array))->dataAddr)))[index]) \
		: (&((elemType*)((((J9IndexableObjectWithDataAddressContiguousFull *)(array))->dataAddr)))[index]))

#define J9JAVAARRAYCONTIGUOUS_EA(vmThread, array, index, elemType) \
	(((vmThread)->isIndexableDataAddrPresent) \
		? J9JAVAARRAYCONTIGUOUS_WITH_DATAADDRESS_EA(vmThread, array, index, elemType) \
		: J9JAVAARRAYCONTIGUOUS_WITHOUT_DATAADDRESS_EA(vmThread, array, index, elemType))

#define J9JAVAARRAYCONTIGUOUS_EA_VM(javaVM, array, index, elemType) \
	(((javaVM)->isIndexableDataAddrPresent) \
		? J9JAVAARRAYCONTIGUOUS_WITH_DATAADDRESS_EA_VM(javaVM, array, index, elemType) \
		: J9JAVAARRAYCONTIGUOUS_WITHOUT_DATAADDRESS_EA_VM(javaVM, array, index, elemType))

#else /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
#define J9JAVAARRAYCONTIGUOUS_EA(vmThread, array, index, elemType) \
	J9JAVAARRAYCONTIGUOUS_WITHOUT_DATAADDRESS_EA(vmThread, array, index, elemType)

#define J9JAVAARRAYCONTIGUOUS_EA_VM(javaVM, array, index, elemType) \
	J9JAVAARRAYCONTIGUOUS_WITHOUT_DATAADDRESS_EA_VM(javaVM, array, index, elemType)

#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

#define J9ISCONTIGUOUSARRAY(vmThread, array) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (0 != ((J9IndexableObjectContiguousCompressed *)(array))->size) \
		: (0 != ((J9IndexableObjectContiguousFull *)(array))->size))

#define J9ISCONTIGUOUSARRAY_VM(javaVM, array) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? (0 != ((J9IndexableObjectContiguousCompressed *)(array))->size) \
		: (0 != ((J9IndexableObjectContiguousFull *)(array))->size))

/* TODO: queries compressed twice - optimize? */
#define J9JAVAARRAY_EA(vmThread, array, index, elemType) (J9ISCONTIGUOUSARRAY(vmThread, array) ? J9JAVAARRAYCONTIGUOUS_EA(vmThread, array, index, elemType) : J9JAVAARRAYDISCONTIGUOUS_EA(vmThread, array, index, elemType))
#define J9JAVAARRAY_EA_VM(javaVM, array, index, elemType) (J9ISCONTIGUOUSARRAY_VM(javaVM, array) ? J9JAVAARRAYCONTIGUOUS_EA_VM(javaVM, array, index, elemType) : J9JAVAARRAYDISCONTIGUOUS_EA_VM(javaVM, array, index, elemType))

/*
 * Private helpers for reference field types
 */
#define J9OBJECT__PRE_OBJECT_LOAD_ADDRESS(vmThread, object, address) \
	((J9_GC_READ_BARRIER_TYPE_NONE != J9VMTHREAD_JAVAVM(vmThread)->gcReadBarrierType) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9ReadBarrier((vmThread), (fj9object_t*)(address)) : \
	(void)0)
#define J9OBJECT__PRE_OBJECT_LOAD_ADDRESS_VM(javaVM, object, address) \
	((J9_GC_READ_BARRIER_TYPE_NONE != javaVM->gcReadBarrierType) ? \
	(javaVM)->memoryManagerFunctions->J9ReadBarrier(J9JAVAVM_VMTHREAD(javaVM), (fj9object_t*)(address)) : \
	(void)0)
#define J9STATIC__PRE_OBJECT_LOAD(vmThread, clazz, address) \
	((J9_GC_READ_BARRIER_TYPE_NONE != J9VMTHREAD_JAVAVM(vmThread)->gcReadBarrierType) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9ReadBarrierClass((vmThread), (j9object_t*)(address)) : \
	(void)0)
#define J9STATIC__PRE_OBJECT_LOAD_VM(javaVM, clazz, address) \
	((J9_GC_READ_BARRIER_TYPE_NONE != javaVM->gcReadBarrierType) ? \
	(javaVM)->memoryManagerFunctions->J9ReadBarrierClass(J9JAVAVM_VMTHREAD(javaVM), (j9object_t*)(address)) : \
	(void)0)
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, object, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_ALWAYS <= J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType) && \
	(J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK)) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierPre((vmThread), (j9object_t)(object), (fj9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_ALWAYS <= javaVM->gcWriteBarrierType) && \
	(javaVM->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK)) ? \
	javaVM->memoryManagerFunctions->J9WriteBarrierPre(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (fj9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, object, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_OLDCHECK <= J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType) && \
	(J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_ALWAYS)) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierPost((vmThread), (j9object_t)(object), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_OLDCHECK <= javaVM->gcWriteBarrierType) && \
	(javaVM->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_ALWAYS)) ? \
	javaVM->memoryManagerFunctions->J9WriteBarrierPost(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__PRE_OBJECT_STORE(vmThread, clazz, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_ALWAYS <= J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType) && \
	(J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK)) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierPreClass((vmThread), J9VM_J9CLASS_TO_HEAPCLASS((clazz)), (j9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__PRE_OBJECT_STORE_VM(javaVM, clazz, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_ALWAYS <= javaVM->gcWriteBarrierType) && \
	(javaVM->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK)) ? \
	javaVM->memoryManagerFunctions->J9WriteBarrierPreClass(J9JAVAVM_VMTHREAD(javaVM), J9VM_J9CLASS_TO_HEAPCLASS((clazz)), (j9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__POST_OBJECT_STORE(vmThread, clazz, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_OLDCHECK <= J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType) && \
	(J9VMTHREAD_JAVAVM((vmThread))->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_ALWAYS)) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierPostClass((vmThread), (clazz), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__POST_OBJECT_STORE_VM(javaVM, clazz, address, value) \
	(\
	((OMR_GC_WRITE_BARRIER_TYPE_OLDCHECK <= javaVM->gcWriteBarrierType) && \
	(javaVM->gcWriteBarrierType <= OMR_GC_WRITE_BARRIER_TYPE_ALWAYS)) ? \
	javaVM->memoryManagerFunctions->J9WriteBarrierPostClass(J9JAVAVM_VMTHREAD(javaVM), (clazz), (j9object_t)(value)) : \
	(void)0 \
	)

/* Offset API to absolute address API. Only for objects, since for class it's always with absolute address. */
#define J9OBJECT__PRE_OBJECT_LOAD(vmThread, object, offset) J9OBJECT__PRE_OBJECT_LOAD_ADDRESS(vmThread, object, ((U_8*)(object) + (offset)))
#define J9OBJECT__PRE_OBJECT_LOAD_VM(javaVM, object, offset) J9OBJECT__PRE_OBJECT_LOAD_ADDRESS_VM(javaVM, object, ((U_8*)(object) + (offset)))
#define J9OBJECT__PRE_OBJECT_STORE(vmThread, object, offset, value) J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, object, ((U_8*)(object) + (offset)), value)
#define J9OBJECT__PRE_OBJECT_STORE_VM(javaVM, object, offset, value) J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(javaVM, object, ((U_8*)(object) + (offset)), value)
#define J9OBJECT__POST_OBJECT_STORE(vmThread, object, offset, value) J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, object, ((U_8*)(object) + (offset)), value)
#define J9OBJECT__POST_OBJECT_STORE_VM(javaVM, object, offset, value) J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(javaVM, object, ((U_8*)(object) + (offset)), value)

/*
 * Define the access barrier macros for reference instance fields
 */
/* Hard realtime needs to check for access violations on read; compressed refs needs to decompress */
#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER)
#define J9OBJECT_OBJECT_LOAD(vmThread, object, offset) ((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadObject((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_OBJECT_LOAD_VM(javaVM, object, offset) ((j9object_t)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadObject(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (offset), 0))
#define J9OBJECT_OBJECT_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreObject((vmThread), (j9object_t)(object), (offset), (j9object_t)(value), 0))
#define J9OBJECT_OBJECT_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreObject(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (offset), (j9object_t)(value), 0))
#else /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

#define J9OBJECT_OBJECT_LOAD(vmThread, object, offset) \
	(J9OBJECT__PRE_OBJECT_LOAD(vmThread, object, offset), \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, *(U_32*)((U_8*)(object) + (offset))) \
		: (j9object_t)*(UDATA*)((U_8*)(object) + (offset))))

#define J9OBJECT_OBJECT_LOAD_VM(javaVM, object, offset) \
	(J9OBJECT__PRE_OBJECT_LOAD_VM(javaVM, object, offset), \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, *(U_32*)((U_8*)(object) + (offset))) \
		: (j9object_t)*(UDATA*)((U_8*)(object) + (offset))))

#define J9OBJECT_OBJECT_STORE(vmThread, object, offset, value) \
	(\
	J9OBJECT__PRE_OBJECT_STORE(vmThread, object, offset, value), \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (*(U_32*)((U_8*)(object) + (offset)) = J9_CONVERT_POINTER_TO_TOKEN__(vmThread, value)) \
		: (*(UDATA*)((U_8*)(object) + (offset)) = (UDATA)(value))), \
	J9OBJECT__POST_OBJECT_STORE(vmThread, object, offset, value))

#define J9OBJECT_OBJECT_STORE_VM(javaVM, object, offset, value) \
	(\
	J9OBJECT__PRE_OBJECT_STORE_VM(javaVM, object, offset, value), \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? (*(U_32*)((U_8*)(object) + (offset)) = J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, value)) \
		: (*(UDATA*)((U_8*)(object) + (offset)) = (UDATA)(value))), \
	J9OBJECT__POST_OBJECT_STORE_VM(javaVM, object, offset, value))

#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/*
 * Define the access barrier macros for reference static fields
 */
#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) 
/* Target function prototypes:
 * 	j9gc_objaccess_staticReadObject(J9VMThread *vmThread, J9Class *clazz, J9Object **srcSlot, UDATA isVolatile) {
 * 	j9gc_objaccess_staticStoreObject(J9VMThread *vmThread, J9Class *clazz, J9Object **destSlot, J9Object *value, UDATA isVolatile);
 */
#define J9STATIC_OBJECT_LOAD_VM(javaVM, clazz, address) ((j9object_t)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadObject((J9JAVAVM_VMTHREAD(javaVM)), (J9Class*)(clazz), (J9Object **)(address), 0))
#define J9STATIC_OBJECT_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreObject((J9JAVAVM_VMTHREAD(javaVM)), (J9Class*)(clazz), (J9Object **)(address), (J9Object *)(value), 0))

#define J9STATIC_OBJECT_LOAD(vmThread, clazz, address) ((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadObject((vmThread), (J9Class*)(clazz), (J9Object **)(address), 0))
#define J9STATIC_OBJECT_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreObject((vmThread), (J9Class*)(clazz), (J9Object **)(address), (J9Object *)(value), 0))

#else /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

#define J9STATIC_OBJECT_LOAD_VM(javaVM, clazz, address) \
	(J9STATIC__PRE_OBJECT_LOAD(javaVM, clazz, address), \
	 *(j9object_t*)(address) \
	)
#define J9STATIC_OBJECT_STORE_VM(javaVM, clazz, address, value) \
	(\
	J9STATIC__PRE_OBJECT_STORE_VM(javaVM, clazz, address, value), \
	(*(j9object_t*)(address) = (j9object_t)(value)), \
	J9STATIC__POST_OBJECT_STORE_VM(javaVM, clazz, address, value) \
	)

#define J9STATIC_OBJECT_LOAD(vmThread, clazz, address) \
	(J9STATIC__PRE_OBJECT_LOAD(vmThread, clazz, address), \
	 *(j9object_t*)(address) \
	)
#define J9STATIC_OBJECT_STORE(vmThread, clazz, address, value) \
	(\
	J9STATIC__PRE_OBJECT_STORE(vmThread, clazz, address, value), \
	(*(j9object_t*)(address) = (j9object_t)(value)), \
	J9STATIC__POST_OBJECT_STORE(vmThread, clazz, address, value) \
	)

#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/*
 * Define the access barrier macros for reference arrays
 */
#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) 

#define J9JAVAARRAYOFOBJECT_LOAD(vmThread, array, index) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadObject((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFOBJECT_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreObject((vmThread), (J9IndexableObject *)(array), (I_32)(index), (j9object_t)(value), 0))
#define J9JAVAARRAYOFOBJECT_LOAD_VM(vm, array, index) (vm->memoryManagerFunctions->j9gc_objaccess_indexableReadObject(J9JAVAVM_VMTHREAD(vm), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFOBJECT_STORE_VM(vm, array, index, value) (vm->memoryManagerFunctions->j9gc_objaccess_indexableStoreObject(J9JAVAVM_VMTHREAD(vm), (J9IndexableObject *)(array), (I_32)(index), (j9object_t)(value), 0))

#else  /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

#define J9JAVAARRAYOFOBJECT_LOAD(vmThread, array, index) j9javaArrayOfObject_load(vmThread, (J9IndexableObject *)array, (I_32)index)

#define J9JAVAARRAYOFOBJECT_STORE(vmThread, array, index, value) \
	do { \
		if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) { \
			U_32 *storeAddress = J9JAVAARRAY_EA(vmThread, array, index, U_32); \
			J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, array, storeAddress, value); \
			*storeAddress = J9_CONVERT_POINTER_TO_TOKEN__(vmThread, value); \
			J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, array, storeAddress, value); \
		} else { \
			UDATA *storeAddress = J9JAVAARRAY_EA(vmThread, array, index, UDATA); \
			J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, array, storeAddress, value); \
			*storeAddress = (UDATA)(value); \
			J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, array, storeAddress, value); \
		} \
	} while(0)

#define J9JAVAARRAYOFOBJECT_LOAD_VM(vm, array, index) j9javaArrayOfObject_load_VM(vm, (J9IndexableObject *)array, (I_32)index)

#define J9JAVAARRAYOFOBJECT_STORE_VM(vm, array, index, value) \
	do { \
		if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) { \
			U_32 *storeAddress = J9JAVAARRAY_EA_VM(vm, array, index, U_32); \
			J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(vm, array, storeAddress, value); \
			*storeAddress = J9_CONVERT_POINTER_TO_TOKEN_VM__(vm, value); \
			J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(vm, array, storeAddress, value); \
		} else { \
			UDATA *storeAddress = J9JAVAARRAY_EA_VM(vm, array, index, UDATA); \
			J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(vm, array, storeAddress, value); \
			*storeAddress = (UDATA)(value); \
			J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(vm, array, storeAddress, value); \
		} \
	} while(0)

#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/*
 * Define the access barrier macros for primitive fields and primitive arrays 
 */

#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER)

#define J9OBJECT_ADDRESS_LOAD(vmThread, object, offset) ((void*)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadAddress((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_ADDRESS_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreAddress((vmThread), (j9object_t)(object), (offset), (void*)(value), 0))
#define J9OBJECT_U64_LOAD(vmThread, object, offset) ((U_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU64((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_U64_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU64((vmThread), (j9object_t)(object), (offset), (U_64)(value), 0))
#define J9OBJECT_U32_LOAD(vmThread, object, offset) ((U_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU32((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_U32_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU32((vmThread), (j9object_t)(object), (offset), (U_32)(value), 0))
#define J9OBJECT_I64_LOAD(vmThread, object, offset) ((I_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadI64((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_I64_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreI64((vmThread), (j9object_t)(object), (offset), (I_64)(value), 0))
#define J9OBJECT_I32_LOAD(vmThread, object, offset) ((I_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadI32((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_I32_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreI32((vmThread), (j9object_t)(object), (offset), (I_32)(value), 0))
#if defined (J9VM_ENV_DATA64)
#define J9OBJECT_UDATA_LOAD(vmThread, object, offset) ((UDATA)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU64((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_UDATA_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU64((vmThread), (j9object_t)(object), (offset), (UDATA)(value), 0))
#else
#define J9OBJECT_UDATA_LOAD(vmThread, object, offset) ((UDATA)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU32((vmThread), (j9object_t)(object), (offset), 0))
#define J9OBJECT_UDATA_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU32((vmThread), (j9object_t)(object), (offset), (UDATA)(value), 0))
#endif

#define J9OBJECT_ADDRESS_LOAD_VM(javaVM, object, offset) ((void*)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadAddress((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_ADDRESS_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreAddress((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (void*)(value), 0))
#define J9OBJECT_U64_LOAD_VM(javaVM, object, offset) ((U_64)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU64((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_U64_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU64((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (U_64)(value), 0))
#define J9OBJECT_U32_LOAD_VM(javaVM, object, offset) ((U_32)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU32((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_U32_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU32((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (U_32)(value), 0))
#define J9OBJECT_I64_LOAD_VM(javaVM, object, offset) ((I_64)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadI64((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_I64_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreI64((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (I_64)(value), 0))
#define J9OBJECT_I32_LOAD_VM(javaVM, object, offset) ((I_32)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadI32((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_I32_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreI32((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (I_32)(value), 0))
#if defined (J9VM_ENV_DATA64)
#define J9OBJECT_UDATA_LOAD_VM(javaVM, object, offset) ((UDATA)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU64((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_UDATA_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU64((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (UDATA)(value), 0))
#else
#define J9OBJECT_UDATA_LOAD_VM(javaVM, object, offset) ((UDATA)javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectReadU32((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), 0))
#define J9OBJECT_UDATA_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreU32((J9JAVAVM_VMTHREAD(javaVM)), (j9object_t)(object), (offset), (UDATA)(value), 0))
#endif

#define J9STATIC_ADDRESS_LOAD(vmThread, clazz, address) ((void*)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadAddress((vmThread), (J9Class*)(clazz), (address), 0))
#define J9STATIC_ADDRESS_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreAddress((vmThread), (J9Class*)(clazz), (address), (void*)(value), 0))
#define J9STATIC_U64_LOAD(vmThread, clazz, address) ((U_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadU64((vmThread), (J9Class*)(clazz), (U_64 *)(address), 0))
#define J9STATIC_U64_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreU64((vmThread), (J9Class*)(clazz), (U_64 *)(address), (U_64)(value), 0))
#define J9STATIC_U32_LOAD(vmThread, clazz, address) ((U_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadU32((vmThread), (J9Class*)(clazz), (U_32 *)(address), 0))
#define J9STATIC_U32_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreU32((vmThread), (J9Class*)(clazz), (U_32 *)(address), (U_32)(value), 0))
#define J9STATIC_I64_LOAD(vmThread, clazz, address) ((I_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadI64((vmThread), (J9Class*)(clazz), (I_64 *)(address), 0))
#define J9STATIC_I64_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreI64((vmThread), (J9Class*)(clazz), (I_64 *)(address), (I_64)(value), 0))
#define J9STATIC_I32_LOAD(vmThread, clazz, address) ((I_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadI32((vmThread), (J9Class*)(clazz), (I_32 *)(address), 0))
#define J9STATIC_I32_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreI32((vmThread), (J9Class*)(clazz), (I_32 *)(address), (I_32)(value), 0))
#if defined (J9VM_ENV_DATA64)
#define J9STATIC_UDATA_LOAD(vmThread, clazz, address) ((UDATA)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadU64((vmThread), (J9Class*)(clazz), (UDATA *)(address), 0))
#define J9STATIC_UDATA_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreU64((vmThread), (J9Class*)(clazz), (UDATA *)(address), (UDATA)(value), 0))
#else
#define J9STATIC_UDATA_LOAD(vmThread, clazz, address) ((UDATA)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticReadU32((vmThread), (J9Class*)(clazz), (address), 0))
#define J9STATIC_UDATA_STORE(vmThread, clazz, address, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_staticStoreU32((vmThread), (J9Class*)(clazz), (address), (UDATA)(value), 0))
#endif

#define J9STATIC_ADDRESS_LOAD_VM(javaVM, clazz, address) ((void*)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadAddress(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (address), 0))
#define J9STATIC_ADDRESS_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreAddress(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (address), (void*)(value), 0))
#define J9STATIC_U64_LOAD_VM(javaVM, clazz, address) ((U_64)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadU64(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (U_64 *)(address), 0))
#define J9STATIC_U64_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreU64(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (U_64 *)(address), (U_64)(value), 0))
#define J9STATIC_U32_LOAD_VM(javaVM, clazz, address) ((U_32)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadU32(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (U_32 *)(address), 0))
#define J9STATIC_U32_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreU32(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (U_32 *)(address), (U_32)(value), 0))
#define J9STATIC_I64_LOAD_VM(javaVM, clazz, address) ((I_64)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadI64(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (I_64 *)(address), 0))
#define J9STATIC_I64_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreI64(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (I_64 *)(address), (I_64)(value), 0))
#define J9STATIC_I32_LOAD_VM(javaVM, clazz, address) ((I_32)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadI32(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (I_32 *)(address), 0))
#define J9STATIC_I32_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreI32(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (I_32 *)(address), (I_32)(value), 0))
#if defined (J9VM_ENV_DATA64)
#define J9STATIC_UDATA_LOAD_VM(javaVM, clazz, address) ((UDATA)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadU64(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (UDATA *)(address), 0))
#define J9STATIC_UDATA_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreU64(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (UDATA *)(address), (UDATA)(value), 0))
#else
#define J9STATIC_UDATA_LOAD_VM(javaVM, clazz, address) ((UDATA)javaVM->memoryManagerFunctions->j9gc_objaccess_staticReadU32(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (address), 0))
#define J9STATIC_UDATA_STORE_VM(javaVM, clazz, address, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_staticStoreU32(J9JAVAVM_VMTHREAD(javaVM), (J9Class*)(clazz), (address), (UDATA)(value), 0))
#endif

#else /* defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */

#define J9OBJECT_ADDRESS_LOAD(vmThread, object, offset) (((void)0, *(void**)((U_8*)(object) + (offset))))
#define J9OBJECT_ADDRESS_STORE(vmThread, object, offset, value) (((void)0, (*(void**)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_U64_LOAD(vmThread, object, offset) (((void)0, *(U_64*)((U_8*)(object) + (offset))))
#define J9OBJECT_U64_STORE(vmThread, object, offset, value) (((void)0, (*(U_64*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_U32_LOAD(vmThread, object, offset) (((void)0, *(U_32*)((U_8*)(object) + (offset))))
#define J9OBJECT_U32_STORE(vmThread, object, offset, value) (((void)0, (*(U_32*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_I64_LOAD(vmThread, object, offset) (((void)0, *(I_64*)((U_8*)(object) + (offset))))
#define J9OBJECT_I64_STORE(vmThread, object, offset, value) (((void)0, (*(I_64*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_I32_LOAD(vmThread, object, offset) (((void)0, *(I_32*)((U_8*)(object) + (offset))))
#define J9OBJECT_I32_STORE(vmThread, object, offset, value) (((void)0, (*(I_32*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_UDATA_LOAD(vmThread, object, offset) (((void)0, *(UDATA*)((U_8*)(object) + (offset))))
#define J9OBJECT_UDATA_STORE(vmThread, object, offset, value) (((void)0, (*(UDATA*)((U_8*)(object) + (offset)) = (value))))

#define J9OBJECT_ADDRESS_LOAD_VM(javaVM, object, offset) (((void)0, *(void**)((U_8*)(object) + (offset))))
#define J9OBJECT_ADDRESS_STORE_VM(javaVM, object, offset, value) (((void)0, (*(void**)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_U64_LOAD_VM(javaVM, object, offset) (((void)0, *(U_64*)((U_8*)(object) + (offset))))
#define J9OBJECT_U64_STORE_VM(javaVM, object, offset, value) (((void)0, (*(U_64*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_U32_LOAD_VM(javaVM, object, offset) (((void)0, *(U_32*)((U_8*)(object) + (offset))))
#define J9OBJECT_U32_STORE_VM(javaVM, object, offset, value) (((void)0, (*(U_32*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_I64_LOAD_VM(javaVM, object, offset) (((void)0, *(I_64*)((U_8*)(object) + (offset))))
#define J9OBJECT_I64_STORE_VM(javaVM, object, offset, value) (((void)0, (*(I_64*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_I32_LOAD_VM(javaVM, object, offset) (((void)0, *(I_32*)((U_8*)(object) + (offset))))
#define J9OBJECT_I32_STORE_VM(javaVM, object, offset, value) (((void)0, (*(I_32*)((U_8*)(object) + (offset)) = (value))))
#define J9OBJECT_UDATA_LOAD_VM(javaVM, object, offset) (((void)0, *(UDATA*)((U_8*)(object) + (offset))))
#define J9OBJECT_UDATA_STORE_VM(javaVM, object, offset, value) (((void)0, (*(UDATA*)((U_8*)(object) + (offset)) = (value))))

#define J9STATIC_ADDRESS_LOAD(vmThread, clazz, address) (((void)0, *(void**)(address)))
#define J9STATIC_ADDRESS_STORE(vmThread, clazz, address, value) (((void)0, (*(void**)(address) = (value))))
#define J9STATIC_U64_LOAD(vmThread, clazz, address) (((void)0, *(U_64*)(address)))
#define J9STATIC_U64_STORE(vmThread, clazz, address, value) (((void)0, (*(U_64*)(address) = (value))))
#define J9STATIC_U32_LOAD(vmThread, clazz, address) (((void)0, *(U_32*)(address)))
#define J9STATIC_U32_STORE(vmThread, clazz, address, value) (((void)0, (*(U_32*)(address) = (value))))
#define J9STATIC_I64_LOAD(vmThread, clazz, address) (((void)0, *(I_64*)(address)))
#define J9STATIC_I64_STORE(vmThread, clazz, address, value) (((void)0, (*(I_64*)(address) = (value))))
#define J9STATIC_I32_LOAD(vmThread, clazz, address) (((void)0, *(I_32*)(address)))
#define J9STATIC_I32_STORE(vmThread, clazz, address, value) (((void)0, (*(I_32*)(address) = (value))))
#define J9STATIC_UDATA_LOAD(vmThread, clazz, address) (((void)0, *(UDATA*)(address)))
#define J9STATIC_UDATA_STORE(vmThread, clazz, address, value) ( ((void)0, (*(UDATA*)(address) = (value))))

#define J9STATIC_ADDRESS_LOAD_VM(javaVM, clazz, address) (((void)0, *(void**)(address)))
#define J9STATIC_ADDRESS_STORE_VM(javaVM, clazz, address, value) (((void)0, (*(void**)(address) = (value))))
#define J9STATIC_U64_LOAD_VM(javaVM, clazz, address) (((void)0, *(U_64*)(address)))
#define J9STATIC_U64_STORE_VM(javaVM, clazz, address, value) (((void)0, (*(U_64*)(address) = (value))))
#define J9STATIC_U32_LOAD_VM(javaVM, clazz, address) (((void)0, *(U_32*)(address)))
#define J9STATIC_U32_STORE_VM(javaVM, clazz, address, value) (((void)0, (*(U_32*)(address) = (value))))
#define J9STATIC_I64_LOAD_VM(javaVM, clazz, address) (((void)0, *(I_64*)(address)))
#define J9STATIC_I64_STORE_VM(javaVM, clazz, address, value) (((void)0, (*(I_64*)(address) = (value))))
#define J9STATIC_I32_LOAD_VM(javaVM, clazz, address) (((void)0, *(I_32*)(address)))
#define J9STATIC_I32_STORE_VM(javaVM, clazz, address, value) (((void)0, (*(I_32*)(address) = (value))))
#define J9STATIC_UDATA_LOAD_VM(javaVM, clazz, address) (((void)0, *(UDATA*)(address)))
#define J9STATIC_UDATA_STORE_VM(javaVM, clazz, address, value) ( ((void)0, (*(UDATA*)(address) = (value))))

#endif /* defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */

#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER)

#define J9JAVAARRAYOFBOOLEAN_LOAD(vmThread, array, index) ((U_8)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU8((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBOOLEAN_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU8((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_8)(value), 0))
#define J9JAVAARRAYOFBYTE_LOAD(vmThread, array, index) ((I_8)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI8((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBYTE_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI8((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_8)(value), 0))
#define J9JAVAARRAYOFCHAR_LOAD(vmThread, array, index) ((U_16)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU16((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFCHAR_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU16((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_16)(value), 0))
#define J9JAVAARRAYOFDOUBLE_LOAD(vmThread, array, index) ((U_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU64((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFDOUBLE_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU64((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_64)(value), 0))
#define J9JAVAARRAYOFFLOAT_LOAD(vmThread, array, index) ((U_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU32((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFFLOAT_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU32((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_32)(value), 0))
#define J9JAVAARRAYOFINT_LOAD(vmThread, array, index) ((I_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI32((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFINT_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI32((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_32)(value), 0))
#define J9JAVAARRAYOFLONG_LOAD(vmThread, array, index) ((I_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI64((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFLONG_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI64((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_64)(value), 0))
#define J9JAVAARRAYOFSHORT_LOAD(vmThread, array, index) ((I_16)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI16((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFSHORT_STORE(vmThread, array, index, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI16((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_16)(value), 0))

#define J9JAVAARRAYOFBOOLEAN_LOAD_VM(javaVM, array, index) ((U_8)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBOOLEAN_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_8)(value), 0))
#define J9JAVAARRAYOFBYTE_LOAD_VM(javaVM, array, index) ((I_8)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBYTE_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_8)(value), 0))
#define J9JAVAARRAYOFCHAR_LOAD_VM(javaVM, array, index) ((U_16)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFCHAR_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_16)(value), 0))
#define J9JAVAARRAYOFDOUBLE_LOAD_VM(javaVM, array, index) ((U_64)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFDOUBLE_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_64)(value), 0))
#define J9JAVAARRAYOFFLOAT_LOAD_VM(javaVM, array, index) ((U_32)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFFLOAT_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_32)(value), 0))
#define J9JAVAARRAYOFINT_LOAD_VM(javaVM, array, index) ((I_32)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFINT_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_32)(value), 0))
#define J9JAVAARRAYOFLONG_LOAD_VM(javaVM, array, index) ((I_64)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFLONG_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_64)(value), 0))
#define J9JAVAARRAYOFSHORT_LOAD_VM(javaVM, array, index) ((I_16)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFSHORT_STORE_VM(javaVM, array, index, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_16)(value), 0))


#else /* defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */

#define J9JAVAARRAYOFBOOLEAN_LOAD(vmThread, array, index) ((void)0, *(U_8 *)J9JAVAARRAY_EA(vmThread, array, index, U_8))
#define J9JAVAARRAYOFBOOLEAN_STORE(vmThread, array, index, value) ((void)0, *(U_8 *)J9JAVAARRAY_EA(vmThread, array, index, U_8) = (U_8)(value))
#define J9JAVAARRAYOFBYTE_LOAD(vmThread, array, index) ((void)0, *(I_8 *)J9JAVAARRAY_EA(vmThread, array, index, I_8))
#define J9JAVAARRAYOFBYTE_STORE(vmThread, array, index, value) ((void)0, *(I_8 *)J9JAVAARRAY_EA(vmThread, array, index, I_8) = (I_8)(value))
#define J9JAVAARRAYOFCHAR_LOAD(vmThread, array, index) ((void)0, *(U_16 *)J9JAVAARRAY_EA(vmThread, array, index, U_16))
#define J9JAVAARRAYOFCHAR_STORE(vmThread, array, index, value) ((void)0, *(U_16 *)J9JAVAARRAY_EA(vmThread, array, index, U_16) = (U_16)(value))
#define J9JAVAARRAYOFDOUBLE_LOAD(vmThread, array, index) ((void)0, *(U_64 *)J9JAVAARRAY_EA(vmThread, array, index, U_64))
#define J9JAVAARRAYOFDOUBLE_STORE(vmThread, array, index, value) ((void)0, *(U_64 *)J9JAVAARRAY_EA(vmThread, array, index, U_64) = (U_64)(value))
#define J9JAVAARRAYOFFLOAT_LOAD(vmThread, array, index) ((void)0, *(U_32 *)J9JAVAARRAY_EA(vmThread, array, index, U_32))
#define J9JAVAARRAYOFFLOAT_STORE(vmThread, array, index, value) ((void)0, *(U_32 *)J9JAVAARRAY_EA(vmThread, array, index, U_32) = (U_32)(value))
#define J9JAVAARRAYOFINT_LOAD(vmThread, array, index) ((void)0, *(I_32 *)J9JAVAARRAY_EA(vmThread, array, index, I_32))
#define J9JAVAARRAYOFINT_STORE(vmThread, array, index, value) ((void)0, *(I_32 *)J9JAVAARRAY_EA(vmThread, array, index, I_32) = (I_32)(value))
#define J9JAVAARRAYOFLONG_LOAD(vmThread, array, index) ((void)0, *(I_64 *)J9JAVAARRAY_EA(vmThread, array, index, I_64))
#define J9JAVAARRAYOFLONG_STORE(vmThread, array, index, value) ((void)0, *(I_64 *)J9JAVAARRAY_EA(vmThread, array, index, I_64) = (I_64)(value))
#define J9JAVAARRAYOFSHORT_LOAD(vmThread, array, index) ((void)0, *(I_16 *)J9JAVAARRAY_EA(vmThread, array, index, I_16))
#define J9JAVAARRAYOFSHORT_STORE(vmThread, array, index, value) ((void)0, *(I_16 *)J9JAVAARRAY_EA(vmThread, array, index, I_16) = (I_16)(value))

#define J9JAVAARRAYOFBOOLEAN_LOAD_VM(javaVM, array, index) ((void)0, *(U_8 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_8))
#define J9JAVAARRAYOFBOOLEAN_STORE_VM(javaVM, array, index, value) ((void)0, *(U_8 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_8) = (U_8)(value))
#define J9JAVAARRAYOFBYTE_LOAD_VM(javaVM, array, index) ((void)0, *(I_8 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_8))
#define J9JAVAARRAYOFBYTE_STORE_VM(javaVM, array, index, value) ((void)0, *(I_8 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_8) = (I_8)(value))
#define J9JAVAARRAYOFCHAR_LOAD_VM(javaVM, array, index) ((void)0, *(U_16 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_16))
#define J9JAVAARRAYOFCHAR_STORE_VM(javaVM, array, index, value) ((void)0, *(U_16 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_16) = (U_16)(value))
#define J9JAVAARRAYOFDOUBLE_LOAD_VM(javaVM, array, index) ((void)0, *(U_64 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_64))
#define J9JAVAARRAYOFDOUBLE_STORE_VM(javaVM, array, index, value) ((void)0, *(U_64 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_64) = (U_64)(value))
#define J9JAVAARRAYOFFLOAT_LOAD_VM(javaVM, array, index) ((void)0, *(U_32 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_32))
#define J9JAVAARRAYOFFLOAT_STORE_VM(javaVM, array, index, value) ((void)0, *(U_32 *)J9JAVAARRAY_EA_VM(javaVM, array, index, U_32) = (U_32)(value))
#define J9JAVAARRAYOFINT_LOAD_VM(javaVM, array, index) ((void)0, *(I_32 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_32))
#define J9JAVAARRAYOFINT_STORE_VM(javaVM, array, index, value) ((void)0, *(I_32 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_32) = (I_32)(value))
#define J9JAVAARRAYOFLONG_LOAD_VM(javaVM, array, index) ((void)0, *(I_64 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_64))
#define J9JAVAARRAYOFLONG_STORE_VM(javaVM, array, index, value) ((void)0, *(I_64 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_64) = (I_64)(value))
#define J9JAVAARRAYOFSHORT_LOAD_VM(javaVM, array, index) ((void)0, *(I_16 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_16))
#define J9JAVAARRAYOFSHORT_STORE_VM(javaVM, array, index, value) ((void)0, *(I_16 *)J9JAVAARRAY_EA_VM(javaVM, array, index, I_16) = (I_16)(value))


#endif /* defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */

#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER)
#define J9WEAKROOT_OBJECT_LOAD(vmThread, objectSlot) ((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_weakRoot_readObject((vmThread), (j9object_t *)(objectSlot)))
#define J9WEAKROOT_OBJECT_LOAD_VM(javaVM, objectSlot) ((j9object_t)(javaVM)->memoryManagerFunctions->j9gc_weakRoot_readObjectVM(javaVM, (j9object_t *)(objectSlot)))
#else /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */
#define J9WEAKROOT_OBJECT_LOAD(vmThread, objectSlot) \
		((J9_GC_READ_BARRIER_TYPE_NONE == (vmThread)->javaVM->gcReadBarrierType) ? \
		(*(j9object_t *)(objectSlot)) : \
		((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_weakRoot_readObject((vmThread), (j9object_t *)(objectSlot))))

#define J9WEAKROOT_OBJECT_LOAD_VM(javaVM, objectSlot) \
		((J9_GC_READ_BARRIER_TYPE_NONE == (javaVM)->gcReadBarrierType) ? \
		(*(j9object_t *)(objectSlot)) : \
		((j9object_t)(javaVM)->memoryManagerFunctions->j9gc_weakRoot_readObjectVM(javaVM, (j9object_t *)(objectSlot))))

#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/*
 * Aliases for accessing UDATA sized arrays (U_32 or U_64 depending on the platform)
 */

#if defined (J9VM_ENV_DATA64)
#define J9JAVAARRAYOFUDATA_LOAD(vmThread, array, index) ((UDATA) J9JAVAARRAYOFLONG_LOAD(vmThread, array, index))
#define J9JAVAARRAYOFUDATA_STORE(vmThread, array, index, value) J9JAVAARRAYOFLONG_STORE(vmThread, array, index, value)
#else /* J9VM_ENV_DATA64 */
#define J9JAVAARRAYOFUDATA_LOAD(vmThread, array, index) ((UDATA) J9JAVAARRAYOFINT_LOAD(vmThread, array, index))
#define J9JAVAARRAYOFUDATA_STORE(vmThread, array, index, value) J9JAVAARRAYOFINT_STORE(vmThread, array, index, value)
#endif /* J9VM_ENV_DATA64 */

/*
 * Deprecated direct array access macros. 
 * TODO: To be deleted.
 */


#if defined(J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER)
#define TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA(vmThread, array, index) ((&(((I_8*)((J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_getArrayObjectDataAddress((vmThread), (J9IndexableObject *)(array)))))[(index)])))
#else /* defined(J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */
#define TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA(vmThread, array, index) ((void)0, J9JAVAARRAY_EA(vmThread, array, index, I_8))
#endif /* defined(J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */

/*
 * Macros for accessing object header fields
 */

/* clazz is the first field in all objects, so no need to split this macro */
#define TMP_OFFSETOF_J9OBJECT_CLAZZ (offsetof(J9Object, clazz))

#define J9OBJECT_CLAZZ(vmThread, object) \
		(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
				? (struct J9Class*)((UDATA)J9OBJECT_U32_LOAD(vmThread, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) \
				: (struct J9Class*)((UDATA)J9OBJECT_UDATA_LOAD(vmThread, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))

#define J9OBJECT_CLAZZ_VM(javaVM, object) \
		(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
				? (struct J9Class*)((UDATA)J9OBJECT_U32_LOAD_VM(javaVM, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) \
				: (struct J9Class*)((UDATA)J9OBJECT_UDATA_LOAD_VM(javaVM, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))

#define J9OBJECT_FLAGS_FROM_CLAZZ(vmThread, object) \
		(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
				? ((UDATA)J9OBJECT_U32_LOAD(vmThread, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) \
				: ((UDATA)J9OBJECT_UDATA_LOAD(vmThread, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))

#define J9OBJECT_FLAGS_FROM_CLAZZ_VM(javaVM, object)\
		(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
				? ((UDATA)J9OBJECT_U32_LOAD_VM(javaVM, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) \
				: ((UDATA)J9OBJECT_UDATA_LOAD_VM(javaVM, object, TMP_OFFSETOF_J9OBJECT_CLAZZ) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))

#define J9OBJECT_SET_CLAZZ(vmThread, object, value) \
		(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
				? J9OBJECT_U32_STORE(vmThread, object, TMP_OFFSETOF_J9OBJECT_CLAZZ, (U_32)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ((vmThread), (object)))) \
				: J9OBJECT_UDATA_STORE(vmThread, object, TMP_OFFSETOF_J9OBJECT_CLAZZ, (UDATA)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ((vmThread), (object)))))

#define J9OBJECT_SET_CLAZZ_VM(javaVM, object, value) \
		(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
				? J9OBJECT_U32_STORE_VM(javaVM, object, TMP_OFFSETOF_J9OBJECT_CLAZZ, (U_32)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ_VM((javaVM), (object)))) \
				: J9OBJECT_UDATA_STORE_VM(javaVM, object, TMP_OFFSETOF_J9OBJECT_CLAZZ, (UDATA)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ_VM((javaVM), (object)))))

#define J9OBJECT_MONITOR_OFFSET(vmThread,object) (J9OBJECT_CLAZZ(vmThread, object)->lockOffset)
#define J9OBJECT_MONITOR_OFFSET_VM(vm,object) (J9OBJECT_CLAZZ_VM(vm, object)->lockOffset)
#define LN_HAS_LOCKWORD(vmThread,obj) (((IDATA)J9OBJECT_MONITOR_OFFSET(vmThread, obj)) >= 0)
#define LN_HAS_LOCKWORD_VM(vm,obj) (((IDATA)J9OBJECT_MONITOR_OFFSET_VM(vm, obj)) >= 0)
#define J9OBJECT_MONITOR_EA(vmThread, object) ((j9objectmonitor_t*)(((U_8 *)(object)) + J9OBJECT_MONITOR_OFFSET(vmThread, object)))
#define J9OBJECT_MONITOR_EA_VM(vm, object) ((j9objectmonitor_t*)(((U_8 *)(object)) + J9OBJECT_MONITOR_OFFSET_VM(vm, object)))

/* Note the volatile in these macros may be unncessary, but they replace hard-coded volatile operations in the
 * source, so maintaining the volatility is safest.
 */
#define J9_LOAD_LOCKWORD(vmThread, lwEA) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (j9objectmonitor_t)*(U_32 volatile *)(lwEA) \
		: (j9objectmonitor_t)*(UDATA volatile *)(lwEA))

#define J9_LOAD_LOCKWORD_VM(vm, lwEA) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm) \
		? (j9objectmonitor_t)*(U_32 volatile *)(lwEA) \
		: (j9objectmonitor_t)*(UDATA volatile *)(lwEA))

#define J9_STORE_LOCKWORD(vmThread, lwEA, lock) \
	do { \
		if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) { \
			*(U_32 volatile *)(lwEA) = (U_32)(UDATA)(lock); \
		} else { \
			*(UDATA volatile *)(lwEA) = (UDATA)(lock); \
		} \
	} while(0)

#define J9_STORE_LOCKWORD_VM(vm, lwEA, lock) \
	do { \
		if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) { \
			*(U_32 volatile *)(lwEA) = (U_32)(UDATA)(lock); \
		} else { \
			*(UDATA volatile *)(lwEA) = (UDATA)(lock); \
		} \
	} while(0)

#define J9OBJECT_MONITOR(vmThread, object) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? (j9objectmonitor_t)J9OBJECT_U32_LOAD(vmThread, object, J9OBJECT_MONITOR_OFFSET(vmThread,object)) \
		: (j9objectmonitor_t)J9OBJECT_UDATA_LOAD(vmThread, object, J9OBJECT_MONITOR_OFFSET(vmThread,object)))

#define J9OBJECT_MONITOR_VM(vm, object) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm) \
		? (j9objectmonitor_t)J9OBJECT_U32_LOAD_VM(vm, object, J9OBJECT_MONITOR_OFFSET_VM(vm,object)) \
		: (j9objectmonitor_t)J9OBJECT_UDATA_LOAD_VM(vm, object, J9OBJECT_MONITOR_OFFSET_VM(vm,object)))

#define J9OBJECT_SET_MONITOR(vmThread, object, value) \
	do { \
		if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) { \
			J9OBJECT_U32_STORE(vmThread, object,J9OBJECT_MONITOR_OFFSET(vmThread,object), (U_32)(value)); \
		} else { \
			J9OBJECT_UDATA_STORE(vmThread, object,J9OBJECT_MONITOR_OFFSET(vmThread,object), value); \
		} \
	} while(0)

#define J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9IndexableObjectContiguousCompressed, size)) \
		: J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9IndexableObjectContiguousFull, size)))

#define J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9IndexableObjectContiguousCompressed, size)) \
		: J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9IndexableObjectContiguousFull, size)))

#define J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE(vmThread, object) \
	(J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) \
		? J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9IndexableObjectDiscontiguousCompressed, size)) \
		: J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9IndexableObjectDiscontiguousFull, size)))

#define J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE_VM(javaVM, object) \
	(J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM) \
		? J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9IndexableObjectDiscontiguousCompressed, size)) \
		: J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9IndexableObjectDiscontiguousFull, size)))

/* TODO: queries compressed twice - optimize? */
#define J9INDEXABLEOBJECT_SIZE(vmThread, object) (0 != J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object) ? J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object) : J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE(vmThread, object))
#define J9INDEXABLEOBJECT_SIZE_VM(javaVM, object) (0 != J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object) ? J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object) : J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE_VM(javaVM, object))

#endif /* J9ACCESSBARRIER_H */
