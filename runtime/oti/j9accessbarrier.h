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

#ifndef J9ACCESSBARRIER_H
#define J9ACCESSBARRIER_H

#include "j9cfg.h"
#include "j9comp.h"

/*
 * Define type used to represent the class in the 'clazz' slot of an object's header
 */
#ifdef OMR_GC_COMPRESSED_POINTERS
typedef U_32 j9objectclass_t;
#else /* OMR_GC_COMPRESSED_POINTERS */
typedef UDATA j9objectclass_t;
#endif /* OMR_GC_COMPRESSED_POINTERS */

/*
 * Define type used to represent the lock in the 'monitor' slot of an object's header
 */
#ifdef OMR_GC_COMPRESSED_POINTERS
typedef U_32 j9objectmonitor_t;
#else /* OMR_GC_COMPRESSED_POINTERS */
typedef UDATA j9objectmonitor_t;
#endif /* OMR_GC_COMPRESSED_POINTERS */

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
#if defined (OMR_GC_COMPRESSED_POINTERS)
typedef U_32 fj9object_t;
typedef U_32 fj9array_t;
#else
typedef UDATA fj9object_t;
typedef UDATA fj9array_t;
#endif

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
#if defined(J9VM_OUT_OF_PROCESS)
#define J9VMTHREAD_JAVAVM(vmThread) ((J9JavaVM*)dbgReadUDATA((UDATA*)&(vmThread)->javaVM))
#else /* J9VM_OUT_OF_PROCESS */
#define J9VMTHREAD_JAVAVM(vmThread) ((vmThread)->javaVM)
#endif /* J9VM_OUT_OF_PROCESS */

/* There may be an unacceptable performance penalty in the TLS lookup of the current thread, so this conversion should be used sparingly. */
#define J9JAVAVM_VMTHREAD(javaVM) (javaVM->internalVMFunctions->currentVMThread(javaVM))

/* Internal macro - Do not use J9_CONVERT_POINTER_FROM_TOKEN__ or J9_CONVERT_POINTER_TO_TOKEN__! */
#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(J9VM_OUT_OF_PROCESS)
#define J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, pointer) ((void *) ((UDATA)(pointer) << dbgReadUDATA((UDATA*)&(javaVM->compressedPointersShift))))
#define J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, pointer) J9_CONVERT_POINTER_FROM_TOKEN_VM__(J9VMTHREAD_JAVAVM(vmThread),pointer)
#define J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, pointer) ((fj9object_t) ((UDATA)(pointer) >> dbgReadUDATA((UDATA*)&(javaVM->compressedPointersShift))))
#define J9_CONVERT_POINTER_TO_TOKEN__(vmThread, pointer) J9_CONVERT_POINTER_TO_TOKEN_VM__(J9VMTHREAD_JAVAVM(vmThread),pointer)
#else /* J9VM_OUT_OF_PROCESS */
#define J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, pointer) ((void *) ((UDATA)(pointer) << javaVM->compressedPointersShift))
#define J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, pointer) J9_CONVERT_POINTER_FROM_TOKEN_VM__(J9VMTHREAD_JAVAVM(vmThread),pointer)
#define J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, pointer) ((fj9object_t) ((UDATA)(pointer) >> javaVM->compressedPointersShift))
#define J9_CONVERT_POINTER_TO_TOKEN__(vmThread, pointer) J9_CONVERT_POINTER_TO_TOKEN_VM__(J9VMTHREAD_JAVAVM(vmThread),pointer)
#endif /* J9VM_OUT_OF_PROCESS */
#else /* OMR_GC_COMPRESSED_POINTERS */
#define J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, pointer) ((void *) (pointer))
#define J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, pointer) ((void *) (pointer))
#define J9_CONVERT_POINTER_TO_TOKEN__(vmThread, pointer) ((fj9object_t)(UDATA)(pointer))
#define J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, pointer) ((fj9object_t)(UDATA)(pointer))
#endif /* OMR_GC_COMPRESSED_POINTERS */

#if defined(J9VM_OUT_OF_PROCESS)
#define J9JAVAARRAYDISCONTIGUOUS_EA(vmThread, array, index, elemType) (&(((elemType*)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, dbgReadPrimitiveType((UDATA)&(((fj9object_t*)((J9IndexableObjectDiscontiguous *)array + 1))[((U_32)index)/(dbgReadUDATA((UDATA*)&(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize))/sizeof(elemType))]), sizeof(fj9object_t))))[((U_32)index)%(dbgReadUDATA((UDATA*)&(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize))/sizeof(elemType))]))
#define J9JAVAARRAYDISCONTIGUOUS_EA_VM(javaVM, array, index, elemType) (&(((elemType*)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, dbgReadPrimitiveType((UDATA)&(((fj9object_t*)((J9IndexableObjectDiscontiguous *)array + 1))[((U_32)index)/(dbgReadUDATA((UDATA*)&(javaVM->arrayletLeafSize))/sizeof(elemType))]), sizeof(fj9object_t))))[((U_32)index)%(dbgReadUDATA((UDATA*)&(javaVM->arrayletLeafSize))/sizeof(elemType))]))
#else /* J9VM_OUT_OF_PROCESS */
#define J9JAVAARRAYDISCONTIGUOUS_EA(vmThread, array, index, elemType) (&(((elemType*)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, ((fj9object_t*)((J9IndexableObjectDiscontiguous *)array + 1))[((U_32)index)/(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize/sizeof(elemType))]))[((U_32)index)%(J9VMTHREAD_JAVAVM(vmThread)->arrayletLeafSize/sizeof(elemType))]))
#define J9JAVAARRAYDISCONTIGUOUS_EA_VM(javaVM, array, index, elemType) (&(((elemType*)J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, ((fj9object_t*)((J9IndexableObjectDiscontiguous *)array + 1))[((U_32)index)/(javaVM->arrayletLeafSize/sizeof(elemType))]))[((U_32)index)%(javaVM->arrayletLeafSize/sizeof(elemType))]))
#endif /* J9VM_OUT_OF_PROCESS */
#define J9JAVAARRAYCONTIGUOUS_EA(vmThread, array, index, elemType) (&((elemType*)((((J9IndexableObjectContiguous *)(array)) + 1)))[(index)])
#define J9JAVAARRAYCONTIGUOUS_EA_VM(javaVM, array, index, elemType) (&((elemType*)((((J9IndexableObjectContiguous *)(array)) + 1)))[(index)])

#if defined(J9VM_GC_HYBRID_ARRAYLETS)
#if defined(J9VM_OUT_OF_PROCESS)
#define J9ISCONTIGUOUSARRAY(vmThread, array) (0 != dbgReadPrimitiveType((UDATA)&((J9IndexableObjectContiguous *)(array))->size, sizeof(((J9IndexableObjectContiguous *)(array))->size)))
#define J9ISCONTIGUOUSARRAY_VM(javaVM, array) (0 != dbgReadPrimitiveType((UDATA)&((J9IndexableObjectContiguous *)(array))->size, sizeof(((J9IndexableObjectContiguous *)(array))->size)))
#else /* J9VM_OUT_OF_PROCESS */
#define J9ISCONTIGUOUSARRAY(vmThread, array) (0 != ((J9IndexableObjectContiguous *)(array))->size)
#define J9ISCONTIGUOUSARRAY_VM(javaVM, array) (0 != ((J9IndexableObjectContiguous *)(array))->size)
#endif /* J9VM_OUT_OF_PROCESS */
#define J9JAVAARRAY_EA(vmThread, array, index, elemType) (J9ISCONTIGUOUSARRAY(vmThread, array) ? J9JAVAARRAYCONTIGUOUS_EA(vmThread, array, index, elemType) : J9JAVAARRAYDISCONTIGUOUS_EA(vmThread, array, index, elemType))
#define J9JAVAARRAY_EA_VM(javaVM, array, index, elemType) (J9ISCONTIGUOUSARRAY_VM(javaVM, array) ? J9JAVAARRAYCONTIGUOUS_EA_VM(javaVM, array, index, elemType) : J9JAVAARRAYDISCONTIGUOUS_EA_VM(javaVM, array, index, elemType))
#elif defined(J9VM_GC_ARRAYLETS)
/* synthesize J9ISCONTIGUOUSARRAY to do what it always did in arraylets since hybrid arraylets will soon be used everywhere */
#define J9ISCONTIGUOUSARRAY(vmThread, array) 0
#define J9ISCONTIGUOUSARRAY_VM(javaVM, array) 0
#define J9JAVAARRAY_EA(vmThread, array, index, elemType) J9JAVAARRAYDISCONTIGUOUS_EA(vmThread, array, index, elemType)
#define J9JAVAARRAY_EA_VM(javaVM, array, index, elemType) J9JAVAARRAYDISCONTIGUOUS_EA_VM(javaVM, array, index, elemType)
#else /* J9VM_GC_ARRAYLETS */
/* synthesize J9ISCONTIGUOUSARRAY to always say true in non-arraylet VMs since that is the only layout */
#define J9ISCONTIGUOUSARRAY(vmThread, array) 1
#define J9ISCONTIGUOUSARRAY_VM(javaVM, array) 1
#define J9JAVAARRAY_EA(vmThread, array, index, elemType) J9JAVAARRAYCONTIGUOUS_EA(vmThread, array, index, elemType)
#define J9JAVAARRAY_EA_VM(javaVM, array, index, elemType) J9JAVAARRAYCONTIGUOUS_EA_VM(javaVM, array, index, elemType)
#endif /* J9VM_GC_ARRAYLETS */

/*
 * Private helpers for reference field types
 */
#if defined(J9VM_GC_COMBINATION_SPEC)
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
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9ReadBarrierJ9Class((vmThread), (j9object_t*)(address)) : \
	(void)0)
#define J9STATIC__PRE_OBJECT_LOAD_VM(javaVM, clazz, address) \
	((J9_GC_READ_BARRIER_TYPE_NONE != javaVM->gcReadBarrierType) ? \
	(javaVM)->memoryManagerFunctions->J9ReadBarrierJ9Class(J9JAVAVM_VMTHREAD(javaVM), (j9object_t*)(address)) : \
	(void)0)
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, object, address, value) \
	((void)0, \
	((OMR_GC_WRITE_BARRIER_TYPE_SATB == (J9VMTHREAD_JAVAVM((vmThread)))->gcWriteBarrierType) || \
	(OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK == (J9VMTHREAD_JAVAVM((vmThread)))->gcWriteBarrierType)) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9MetronomeWriteBarrierStore((vmThread), (j9object_t)(object), (fj9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) \
	((void)0, \
	((OMR_GC_WRITE_BARRIER_TYPE_SATB == javaVM->gcWriteBarrierType) || \
	(OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK == javaVM->gcWriteBarrierType)) ? \
	javaVM->memoryManagerFunctions->J9MetronomeWriteBarrierStore(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (fj9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, object, address, value) \
	((void)0, \
	(OMR_GC_ALLOCATION_TYPE_SEGREGATED != (J9VMTHREAD_JAVAVM((vmThread)))->gcAllocationType) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierStore((vmThread), (j9object_t)(object), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) \
	((void)0, \
	(OMR_GC_ALLOCATION_TYPE_SEGREGATED == javaVM->gcAllocationType) ? \
	javaVM->memoryManagerFunctions->J9WriteBarrierStore(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__PRE_OBJECT_STORE(vmThread, clazz, address, value) \
	((void)0, \
	((OMR_GC_WRITE_BARRIER_TYPE_SATB == (J9VMTHREAD_JAVAVM((vmThread)))->gcWriteBarrierType) || \
	(OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK == (J9VMTHREAD_JAVAVM((vmThread)))->gcWriteBarrierType)) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9MetronomeWriteBarrierJ9ClassStore((vmThread), J9VM_J9CLASS_TO_HEAPCLASS((clazz)), (j9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__PRE_OBJECT_STORE_VM(javaVM, object, address, value) \
	((void)0, \
	((OMR_GC_WRITE_BARRIER_TYPE_SATB == javaVM->gcWriteBarrierType) || \
	(OMR_GC_WRITE_BARRIER_TYPE_SATB_AND_OLDCHECK == javaVM->gcWriteBarrierType)) ? \
	javaVM->memoryManagerFunctions->J9MetronomeWriteBarrierJ9ClassStore(J9JAVAVM_VMTHREAD(javaVM), J9VM_J9CLASS_TO_HEAPCLASS((clazz)), (j9object_t*)(address), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__POST_OBJECT_STORE(vmThread, clazz, address, value) \
	((void)0, \
	(OMR_GC_ALLOCATION_TYPE_SEGREGATED != (J9VMTHREAD_JAVAVM((vmThread)))->gcAllocationType) ? \
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierJ9ClassStore((vmThread), (clazz), (j9object_t)(value)) : \
	(void)0 \
	)
#define J9STATIC__POST_OBJECT_STORE_VM(javaVM, clazz, address, value) \
	((void)0, \
	(OMR_GC_ALLOCATION_TYPE_SEGREGATED != javaVM->gcAllocationType) ? \
	javaVM->memoryManagerFunctions->J9WriteBarrierJ9ClassStore(J9JAVAVM_VMTHREAD(javaVM), (clazz), (j9object_t)(value)) : \
	(void)0 \
	)
#else /* defined(J9VM_GC_COMBINATION_SPEC) */
#if defined(J9VM_GC_REALTIME)
#define J9OBJECT__PRE_OBJECT_LOAD_ADDRESS(vmThread, object, address) 0
#define J9OBJECT__PRE_OBJECT_LOAD_ADDRESS_VM(javaVM, object, address) 0
#define J9STATIC__PRE_OBJECT_LOAD(vmThread, clazz, address) 0
#define J9STATIC__PRE_OBJECT_LOAD_VM(javaVM, clazz, address) 0
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, object, address, value) J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9MetronomeWriteBarrierStore((vmThread), (j9object_t)(object), (fj9object_t*)address, (j9object_t)(value))
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) javaVM->memoryManagerFunctions->J9MetronomeWriteBarrierStore(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (fj9object_t*)address, (j9object_t)(value))
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, object, address, value) 0
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) 0
#define J9STATIC__PRE_OBJECT_STORE(vmThread, clazz, address, value) J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9MetronomeWriteBarrierJ9ClassStore((vmThread), J9VM_J9CLASS_TO_HEAPCLASS((clazz)), (j9object_t*)(address), (j9object_t)(value))
#define J9STATIC__PRE_OBJECT_STORE_VM(javaVM, clazz, address, value) javaVM->memoryManagerFunctions->J9MetronomeWriteBarrierJ9ClassStore(J9JAVAVM_VMTHREAD(javaVM), J9VM_J9CLASS_TO_HEAPCLASS((clazz)), (j9object_t*)(address), (j9object_t)(value))
#define J9STATIC__POST_OBJECT_STORE(vmThread, clazz, address, value) 0
#define J9STATIC__POST_OBJECT_STORE_VM(javaVM, clazz, address, value) 0
#else /* J9VM_GC_REALTIME */
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
	J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9ReadBarrierJ9Class((vmThread), (j9object_t*)(address)) : \
	(void)0)
#define J9STATIC__PRE_OBJECT_LOAD_VM(javaVM, clazz, address) \
	((J9_GC_READ_BARRIER_TYPE_NONE != javaVM->gcReadBarrierType) ? \
	(javaVM)->memoryManagerFunctions->J9ReadBarrierJ9Class(J9JAVAVM_VMTHREAD(javaVM), (j9object_t*)(address)) : \
	(void)0)
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, object, address, value) 0
#define J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) 0
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, object, address, value) J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierStore((vmThread), (j9object_t)(object), (j9object_t)(value))
#define J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(javaVM, object, address, value) javaVM->memoryManagerFunctions->J9WriteBarrierStore(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (j9object_t)(value))
#define J9STATIC__PRE_OBJECT_STORE(vmThread, clazz, address, value) 0
#define J9STATIC__PRE_OBJECT_STORE_VM(javaVM, clazz, address, value) 0
#define J9STATIC__POST_OBJECT_STORE(vmThread, clazz, address, value) J9VMTHREAD_JAVAVM((vmThread))->memoryManagerFunctions->J9WriteBarrierJ9ClassStore((vmThread), (clazz), (j9object_t)(value))
#define J9STATIC__POST_OBJECT_STORE_VM(javaVM, clazz, address, value) javaVM->memoryManagerFunctions->J9WriteBarrierJ9ClassStore(J9JAVAVM_VMTHREAD(javaVM), (clazz), (j9object_t)(value))
#endif /* J9VM_GC_REALTIME */
#endif /* defined(J9VM_GC_COMBINATION_SPEC) */

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
#else /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

#define J9OBJECT_OBJECT_LOAD(vmThread, object, offset) \
	(J9OBJECT__PRE_OBJECT_LOAD(vmThread, object, offset), \
	 (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN__(vmThread, *(fj9object_t*)((U_8*)(object) + (offset))) \
	)
#define J9OBJECT_OBJECT_LOAD_VM(javaVM, object, offset) \
	(J9OBJECT__PRE_OBJECT_LOAD_VM(javaVM, object, offset), \
	 (j9object_t)J9_CONVERT_POINTER_FROM_TOKEN_VM__(javaVM, *(fj9object_t*)((U_8*)(object) + (offset))) \
	)
#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/* Hard realtime needs to perform SATB; compressed refs needs to compress */
#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) 
#define J9OBJECT_OBJECT_STORE(vmThread, object, offset, value) (J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreObject((vmThread), (j9object_t)(object), (offset), (j9object_t)(value), 0))
#define J9OBJECT_OBJECT_STORE_VM(javaVM, object, offset, value) (javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreObject(J9JAVAVM_VMTHREAD(javaVM), (j9object_t)(object), (offset), (j9object_t)(value), 0))
#else /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */
#define J9OBJECT_OBJECT_STORE(vmThread, object, offset, value) \
	((void)0, \
	J9OBJECT__PRE_OBJECT_STORE(vmThread, object, offset, value), \
	(*(fj9object_t*)((U_8*)(object) + (offset)) = J9_CONVERT_POINTER_TO_TOKEN__(vmThread, value)), \
	J9OBJECT__POST_OBJECT_STORE(vmThread, object, offset, value) \
	)
#define J9OBJECT_OBJECT_STORE_VM(javaVM, object, offset, value) \
	((void)0, \
	J9OBJECT__PRE_OBJECT_STORE_VM(javaVM, object, offset, value), \
	(*(fj9object_t*)((U_8*)(object) + (offset)) = J9_CONVERT_POINTER_TO_TOKEN_VM__(javaVM, value)), \
	J9OBJECT__POST_OBJECT_STORE_VM(javaVM, object, offset, value) \
	)
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
	((void)0, \
	J9STATIC__PRE_OBJECT_STORE_VM(javaVM, clazz, address, value), \
	(*(j9object_t*)(address) = (j9object_t)(value)), \
	J9STATIC__POST_OBJECT_STORE_VM(javaVM, clazz, address, value) \
	)

#define J9STATIC_OBJECT_LOAD(vmThread, clazz, address) \
	(J9STATIC__PRE_OBJECT_LOAD(vmThread, clazz, address), \
	 *(j9object_t*)(address) \
	)
#define J9STATIC_OBJECT_STORE(vmThread, clazz, address, value) \
	((void)0, \
	J9STATIC__PRE_OBJECT_STORE(vmThread, clazz, address, value), \
	(*(j9object_t*)(address) = (j9object_t)(value)), \
	J9STATIC__POST_OBJECT_STORE(vmThread, clazz, address, value) \
	)

#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/*
 * Define the access barrier macros for reference arrays
 */
#if defined (J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) 

#define J9JAVAARRAYOFOBJECT_LOAD(vmThread, array, index) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadObject((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFOBJECT_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreObject((vmThread), (J9IndexableObject *)(array), (I_32)(index), (j9object_t)(value), 0))
#define J9JAVAARRAYOFOBJECT_LOAD_VM(vm, array, index) ((void)0, vm->memoryManagerFunctions->j9gc_objaccess_indexableReadObject(J9JAVAVM_VMTHREAD(vm), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFOBJECT_STORE_VM(vm, array, index, value) ((void)0, vm->memoryManagerFunctions->j9gc_objaccess_indexableStoreObject(J9JAVAVM_VMTHREAD(vm), (J9IndexableObject *)(array), (I_32)(index), (j9object_t)(value), 0))

#else  /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

#define J9JAVAARRAYOFOBJECT_LOAD(vmThread, array, index) j9javaArrayOfObject_load(vmThread, (J9IndexableObject *)array, (I_32)index)

#define J9JAVAARRAYOFOBJECT_STORE(vmThread, array, index, value) \
	do { \
		fj9object_t *storeAddress = J9JAVAARRAY_EA(vmThread, array, index, fj9object_t); \
		J9OBJECT__PRE_OBJECT_STORE_ADDRESS(vmThread, array, storeAddress, value); \
		*storeAddress = J9_CONVERT_POINTER_TO_TOKEN__(vmThread, value); \
		J9OBJECT__POST_OBJECT_STORE_ADDRESS(vmThread, array, storeAddress, value); \
	} while(0)

#define J9JAVAARRAYOFOBJECT_LOAD_VM(vm, array, index) j9javaArrayOfObject_load_VM(vm, (J9IndexableObject *)array, (I_32)index)

#define J9JAVAARRAYOFOBJECT_STORE_VM(vm, array, index, value) \
	do { \
		fj9object_t *storeAddress = J9JAVAARRAY_EA_VM(vm, array, index, fj9object_t); \
		J9OBJECT__PRE_OBJECT_STORE_ADDRESS_VM(vm, array, storeAddress, value); \
		*storeAddress = J9_CONVERT_POINTER_TO_TOKEN_VM__(vm, value); \
		J9OBJECT__POST_OBJECT_STORE_ADDRESS_VM(vm, array, storeAddress, value); \
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

#define J9JAVAARRAYOFBOOLEAN_LOAD(vmThread, array, index) ((void)0, (U_8)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU8((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBOOLEAN_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU8((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_8)(value), 0))
#define J9JAVAARRAYOFBYTE_LOAD(vmThread, array, index) ((void)0, (I_8)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI8((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBYTE_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI8((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_8)(value), 0))
#define J9JAVAARRAYOFCHAR_LOAD(vmThread, array, index) ((void)0, (U_16)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU16((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFCHAR_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU16((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_16)(value), 0))
#define J9JAVAARRAYOFDOUBLE_LOAD(vmThread, array, index) ((void)0, (U_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU64((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFDOUBLE_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU64((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_64)(value), 0))
#define J9JAVAARRAYOFFLOAT_LOAD(vmThread, array, index) ((void)0, (U_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadU32((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFFLOAT_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreU32((vmThread), (J9IndexableObject *)(array), (I_32)(index), (U_32)(value), 0))
#define J9JAVAARRAYOFINT_LOAD(vmThread, array, index) ((void)0, (I_32)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI32((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFINT_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI32((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_32)(value), 0))
#define J9JAVAARRAYOFLONG_LOAD(vmThread, array, index) ((void)0, (I_64)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI64((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFLONG_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI64((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_64)(value), 0))
#define J9JAVAARRAYOFSHORT_LOAD(vmThread, array, index) ((void)0, (I_16)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableReadI16((vmThread), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFSHORT_STORE(vmThread, array, index, value) ((void)0, J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_indexableStoreI16((vmThread), (J9IndexableObject *)(array), (I_32)(index), (I_16)(value), 0))

#define J9JAVAARRAYOFBOOLEAN_LOAD_VM(javaVM, array, index) ((void)0, (U_8)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBOOLEAN_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_8)(value), 0))
#define J9JAVAARRAYOFBYTE_LOAD_VM(javaVM, array, index) ((void)0, (I_8)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFBYTE_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI8((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_8)(value), 0))
#define J9JAVAARRAYOFCHAR_LOAD_VM(javaVM, array, index) ((void)0, (U_16)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFCHAR_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_16)(value), 0))
#define J9JAVAARRAYOFDOUBLE_LOAD_VM(javaVM, array, index) ((void)0, (U_64)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFDOUBLE_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_64)(value), 0))
#define J9JAVAARRAYOFFLOAT_LOAD_VM(javaVM, array, index) ((void)0, (U_32)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadU32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFFLOAT_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreU32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (U_32)(value), 0))
#define J9JAVAARRAYOFINT_LOAD_VM(javaVM, array, index) ((void)0, (I_32)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFINT_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI32((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_32)(value), 0))
#define J9JAVAARRAYOFLONG_LOAD_VM(javaVM, array, index) ((void)0, (I_64)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFLONG_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI64((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_64)(value), 0))
#define J9JAVAARRAYOFSHORT_LOAD_VM(javaVM, array, index) ((void)0, (I_16)javaVM->memoryManagerFunctions->j9gc_objaccess_indexableReadI16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), 0))
#define J9JAVAARRAYOFSHORT_STORE_VM(javaVM, array, index, value) ((void)0, javaVM->memoryManagerFunctions->j9gc_objaccess_indexableStoreI16((J9JAVAVM_VMTHREAD(javaVM)), (J9IndexableObject *)(array), (I_32)(index), (I_16)(value), 0))


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
#define J9MONITORTABLE_OBJECT_LOAD(vmThread, objectSlot) ((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_monitorTableReadObject((vmThread), (j9object_t *)(objectSlot)))
#define J9MONITORTABLE_OBJECT_LOAD_VM(javaVM, objectSlot) ((j9object_t)(javaVM)->memoryManagerFunctions->j9gc_objaccess_monitorTableReadObjectVM(javaVM, (j9object_t *)(objectSlot)))
#else /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */
#define J9MONITORTABLE_OBJECT_LOAD(vmThread, objectSlot) \
		((J9_GC_READ_BARRIER_TYPE_NONE == (vmThread)->javaVM->gcReadBarrierType) ? \
		(*(j9object_t *)(objectSlot)) : \
		((J9_GC_READ_BARRIER_TYPE_RANGE_CHECK == (vmThread)->javaVM->gcReadBarrierType) ? \
		((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_monitorTableReadObject((vmThread), (j9object_t *)(objectSlot))) : \
		((j9object_t)J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_monitorTableReadObject((vmThread), (j9object_t *)(objectSlot)))))

#define J9MONITORTABLE_OBJECT_LOAD_VM(javaVM, objectSlot) \
		((J9_GC_READ_BARRIER_TYPE_NONE == (javaVM)->gcReadBarrierType) ? \
		(*(j9object_t *)(objectSlot)) : \
		((J9_GC_READ_BARRIER_TYPE_RANGE_CHECK == (javaVM)->gcReadBarrierType) ? \
		((j9object_t)(javaVM)->memoryManagerFunctions->j9gc_objaccess_monitorTableReadObjectVM(javaVM, (j9object_t *)(objectSlot))) : \
		((j9object_t)(javaVM)->memoryManagerFunctions->j9gc_objaccess_monitorTableReadObjectVM(javaVM, (j9object_t *)(objectSlot)))))

#endif /* J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER */

/*
 * Aliases for accessing UDATA sized arrays (U_32 or U_64 depending on the platform)
 */

#if defined (J9VM_ENV_DATA64)
#define J9JAVAARRAYOFUDATA_LOAD(vmThread, array, index) ((UDATA) J9JAVAARRAYOFLONG_LOAD(vmThread, array, index))
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
#define TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA(vmThread, array, index) ((void)0, (&(((I_8*)((J9VMTHREAD_JAVAVM(vmThread)->memoryManagerFunctions->j9gc_objaccess_getArrayObjectDataAddress((vmThread), (J9IndexableObject *)(array)))))[(index)])))
#else /* defined(J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */
#define TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA(vmThread, array, index) ((void)0, J9JAVAARRAY_EA(vmThread, array, index, I_8))
#endif /* defined(J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER) */

/*
 * Macros for accessing object header fields
 */
#if defined (OMR_GC_COMPRESSED_POINTERS) || !defined (J9VM_ENV_DATA64)
#define J9OBJECT_CLAZZ(vmThread, object) ((void)0, (struct J9Class*)((UDATA)J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9Object,clazz)) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_CLAZZ_VM(javaVM, object) ((void)0, (struct J9Class*)((UDATA)J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9Object,clazz)) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_FLAGS_FROM_CLAZZ(vmThread, object) ((void)0, ((UDATA)J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9Object,clazz)) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_FLAGS_FROM_CLAZZ_VM(javaVM, object) ((void)0, ((UDATA)J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9Object,clazz)) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_SET_CLAZZ(vmThread, object, value) ((void)0, J9OBJECT_U32_STORE(vmThread, object, offsetof(J9Object,clazz), (j9objectclass_t)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ((vmThread), (object)))))
#define J9OBJECT_SET_CLAZZ_VM(javaVM, object, value) ((void)0, J9OBJECT_U32_STORE_VM(javaVM, object, offsetof(J9Object,clazz), (j9objectclass_t)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ_VM((javaVM), (object)))))
#else
#define J9OBJECT_CLAZZ(vmThread, object) ((void)0, (struct J9Class*)((UDATA)J9OBJECT_U64_LOAD(vmThread, object, offsetof(J9Object,clazz)) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_CLAZZ_VM(javaVM, object) ((void)0, (struct J9Class*)((UDATA)J9OBJECT_U64_LOAD_VM(javaVM, object, offsetof(J9Object,clazz)) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_FLAGS_FROM_CLAZZ(vmThread, object) ((void)0, ((UDATA)J9OBJECT_U64_LOAD(vmThread, object, offsetof(J9Object,clazz)) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_FLAGS_FROM_CLAZZ_VM(javaVM, object) ((void)0, ((UDATA)J9OBJECT_U64_LOAD_VM(javaVM, object, offsetof(J9Object,clazz)) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))
#define J9OBJECT_SET_CLAZZ(vmThread, object, value) ((void)0, J9OBJECT_U64_STORE(vmThread, object, offsetof(J9Object,clazz), (j9objectclass_t)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ((vmThread), (object)))))
#define J9OBJECT_SET_CLAZZ_VM(javaVM, object, value) ((void)0, J9OBJECT_U64_STORE_VM(javaVM, object, offsetof(J9Object,clazz), (j9objectclass_t)(((UDATA)(value) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))) | J9OBJECT_FLAGS_FROM_CLAZZ_VM((javaVM), (object)))))
#endif
#define TMP_OFFSETOF_J9OBJECT_CLAZZ (offsetof(J9Object, clazz))
#define TMP_J9OBJECT_CLAZZ(object) ((struct J9Class*)((UDATA)((object)->clazz) & ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1))))

#define TMP_J9OBJECT_FLAGS(object) ((UDATA)((object)->clazz) & ((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1)))

#if defined( J9VM_THR_LOCK_NURSERY )
#define J9OBJECT_MONITOR_OFFSET(vmThread,object) (J9OBJECT_CLAZZ(vmThread, object))->lockOffset
#else
#define J9OBJECT_MONITOR_OFFSET(vmThread,object) offsetof(J9Object,monitor)
#endif

#if defined (OMR_GC_COMPRESSED_POINTERS) || !defined (J9VM_ENV_DATA64)
#define J9OBJECT_MONITOR(vmThread, object) ((void)0, (j9objectmonitor_t)J9OBJECT_U32_LOAD(vmThread, object, J9OBJECT_MONITOR_OFFSET(vmThread,object)))
#define J9OBJECT_SET_MONITOR(vmThread, object, value) ((void)0, J9OBJECT_U32_STORE(vmThread, object,J9OBJECT_MONITOR_OFFSET(vmThread,object), value))
#else
#define J9OBJECT_MONITOR(vmThread, object) ((void)0, (j9objectmonitor_t)J9OBJECT_U64_LOAD(vmThread, object, J9OBJECT_MONITOR_OFFSET(vmThread,object)))
#define J9OBJECT_SET_MONITOR(vmThread, object, value) ((void)0, J9OBJECT_U64_STORE(vmThread, object, J9OBJECT_MONITOR_OFFSET(vmThread,object), value))
#endif

#if defined( J9VM_THR_LOCK_NURSERY )
#if defined (OMR_GC_COMPRESSED_POINTERS) || !defined (J9VM_ENV_DATA64)
#define TMP_LOCKWORD_OFFSET(object) (dbgReadUDATA((UDATA*)((U_8*)(((UDATA)(dbgReadU32((U_32*)(((U_8*)object) + offsetof(J9Object, clazz))))& ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1)))+ offsetof(J9Class,lockOffset)))))
#else
#define TMP_LOCKWORD_OFFSET(object) (dbgReadUDATA((UDATA*)((U_8*)(((UDATA)(dbgReadU64((U_64*)(((U_8*)object) + offsetof(J9Object, clazz))))& ~((UDATA)(J9_REQUIRED_CLASS_ALIGNMENT - 1)))+ offsetof(J9Class,lockOffset)))))
#endif
#define TMP_HAS_LOCKWORD(object) (TMP_LOCKWORD_OFFSET(object) != -1)

#define TMP_OFFSETOF_J9OBJECT_MONITOR_FROM_CLASS(clazz) ( (clazz)->lockOffset )
#define TMP_J9OBJECT_MONITOR_EA(object) ((j9objectmonitor_t*)((U_8 *)object + TMP_LOCKWORD_OFFSET(object)))
#if defined(J9VM_OUT_OF_PROCESS)
#define J9OBJECT_MONITOR_EA_SAFE(vmThread, object) TMP_J9OBJECT_MONITOR_EA(object)
#else
#define J9OBJECT_MONITOR_EA_SAFE(vmThread, object) J9OBJECT_MONITOR_EA(vmThread, object)
#endif
#else
#define TMP_OFFSETOF_J9OBJECT_MONITOR (offsetof(J9Object, monitor))
#define TMP_J9OBJECT_MONITOR(object) ((object)->monitor)
#endif

#define J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object) ((void)0, (U_32)J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9IndexableObjectContiguous,size)))
#define J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object) ((void)0, (U_32)J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9IndexableObjectContiguous,size)))
#define TMP_OFFSETOF_J9INDEXABLEOBJECTCONTIGUOUS_SIZE (offsetof(J9IndexableObjectContiguous,size))
#define TMP_J9INDEXABLEOBJECTCONTIGUOUS_SIZE(object) (((J9IndexableObjectContiguous*)(object))->size)
#define J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE(vmThread, object) ((void)0, (U_32)J9OBJECT_U32_LOAD(vmThread, object, offsetof(J9IndexableObjectDiscontiguous,size)))
#define J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE_VM(javaVM, object) ((void)0, (U_32)J9OBJECT_U32_LOAD_VM(javaVM, object, offsetof(J9IndexableObjectDiscontiguous,size)))
#define TMP_OFFSETOF_J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE (offsetof(J9IndexableObjectDiscontiguous,size))
#define TMP_J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE(object) (((J9IndexableObjectDiscontiguous*)(object))->size)

#if defined(J9VM_GC_HYBRID_ARRAYLETS)
#define J9INDEXABLEOBJECT_SIZE(vmThread, object) (0 != J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object) ? J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object) : J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE(vmThread, object))
#define J9INDEXABLEOBJECT_SIZE_VM(javaVM, object) (0 != J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object) ? J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object) : J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE_VM(javaVM, object))
#elif defined(J9VM_GC_ARRAYLETS)
#define J9INDEXABLEOBJECT_SIZE(vmThread, object) J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE(vmThread, object)
#define J9INDEXABLEOBJECT_SIZE_VM(javaVM, object) J9INDEXABLEOBJECTDISCONTIGUOUS_SIZE_VM(javaVM, object)
#else /* defined(J9VM_GC_ARRAYLETS) */
#define J9INDEXABLEOBJECT_SIZE(vmThread, object) J9INDEXABLEOBJECTCONTIGUOUS_SIZE(vmThread, object)
#define J9INDEXABLEOBJECT_SIZE_VM(javaVM, object) J9INDEXABLEOBJECTCONTIGUOUS_SIZE_VM(javaVM, object)
#endif /* defined(J9VM_GC_ARRAYLETS) */


#if defined(J9VM_THR_LOCK_NURSERY) && defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
#define TMP_OFFSETOF_J9INDEXABLEOBJECT_MONITOR (offsetof(J9IndexableObject,monitor))
#endif /* defined(J9VM_THR_LOCK_NURSERY) && defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS) */

#endif /* J9ACCESSBARRIER_H */
