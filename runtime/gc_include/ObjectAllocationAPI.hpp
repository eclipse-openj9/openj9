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

#if !defined(OBJECTALLOCATIONAPI_HPP_)
#define OBJECTALLOCATIONAPI_HPP_

#include "j9cfg.h"

#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
#if OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES
#define MM_ObjectAllocationAPI MM_ObjectAllocationAPICompressed
#else /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#define MM_ObjectAllocationAPI MM_ObjectAllocationAPIFull
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */

#include "j9.h"
#include "j9consts.h"
#include "j9generated.h"
#include "j9protos.h"
#include "omrgcconsts.h"

#include "AtomicSupport.hpp"
#include "ObjectMonitor.hpp"

class MM_ObjectAllocationAPI
{
	/*
	 * Data members
	 */
private:
	const UDATA _gcAllocationType;
#if defined(J9VM_GC_BATCH_CLEAR_TLH)
	const UDATA _initializeSlotsOnTLHAllocate;
#endif /* J9VM_GC_BATCH_CLEAR_TLH */
	const UDATA _objectAlignmentInBytes;

#if defined (J9VM_GC_SEGREGATED_HEAP)
	const J9VMGCSizeClasses *_sizeClasses;
#endif /* J9VM_GC_SEGREGATED_HEAP */

protected:
public:

	/*
	 * Function members
	 */
private:

	VMINLINE j9object_t
	inlineAllocateIndexableObjectImpl(J9VMThread *currentThread, J9Class *arrayClass, U_32 size, UDATA dataSize, bool validSize, bool initializeSlots = true, bool memoryBarrier = true)
	{
		j9object_t instance = NULL;

#if defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP)
		if (0 != size) {
			/* Contiguous Array */

#if !defined(J9VM_ENV_DATA64)
			if (validSize)
#endif /* J9VM_ENV_DATA64 */
			{
				/* Calculate the size of the object */
				UDATA const headerSize = J9VMTHREAD_CONTIGUOUS_HEADER_SIZE(currentThread);
				UDATA const dataSize = ((UDATA)size) * J9ARRAYCLASS_GET_STRIDE(arrayClass);
				UDATA allocateSize = (dataSize + headerSize + _objectAlignmentInBytes - 1) & ~(UDATA)(_objectAlignmentInBytes - 1);
				if (allocateSize < J9_GC_MINIMUM_OBJECT_SIZE) {
					allocateSize = J9_GC_MINIMUM_OBJECT_SIZE;
				}

				/* Allocate the memory */
				j9object_t objectHeader = NULL;

				switch(_gcAllocationType) {

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
				case OMR_GC_ALLOCATION_TYPE_TLH:
					if (allocateSize <= ((UDATA) currentThread->heapTop - (UDATA) currentThread->heapAlloc)) {
						U_8 *heapAlloc = currentThread->heapAlloc;
						UDATA afterAlloc = (UDATA)heapAlloc + allocateSize;
						objectHeader = (j9object_t)heapAlloc;
						currentThread->heapAlloc = (U_8 *)afterAlloc;
#if defined(J9VM_GC_TLH_PREFETCH_FTA)
						currentThread->tlhPrefetchFTA -= allocateSize;
#endif /* J9VM_GC_TLH_PREFETCH_FTA */
#if defined(J9VM_GC_BATCH_CLEAR_TLH)
						/* Do not zero the TLH if it is already zero's */
						initializeSlots = initializeSlots && _initializeSlotsOnTLHAllocate;
#endif /* J9VM_GC_BATCH_CLEAR_TLH */
					} else {
						return NULL;
					}
					break;
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

#if defined(J9VM_GC_SEGREGATED_HEAP)

				case OMR_GC_ALLOCATION_TYPE_SEGREGATED:
					/* Metronome requires that slots are always initialized */

					/* ensure the allocation will fit in a small size */
					if (allocateSize <= J9VMGC_SIZECLASSES_MAX_SMALL_SIZE_BYTES) {

						/* fetch the size class based on the allocation size */
						UDATA slotsRequested = allocateSize / sizeof(UDATA);
						UDATA sizeClassIndex = _sizeClasses->sizeClassIndex[slotsRequested];

						/* Ensure the cache for the current size class is not empty. */
						J9VMGCSegregatedAllocationCacheEntry *cacheEntry =
								(J9VMGCSegregatedAllocationCacheEntry *)((UDATA)currentThread + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET
										+ (sizeClassIndex * sizeof(J9VMGCSegregatedAllocationCacheEntry)));
						UDATA cellSize = _sizeClasses->smallCellSizes[sizeClassIndex];

						if (cellSize <= ((UDATA) cacheEntry->top - (UDATA) cacheEntry->current)) {
							objectHeader = (j9object_t)cacheEntry->current;
							cacheEntry->current = (UDATA *) ((UDATA) cacheEntry->current + cellSize);
							/* The metronome pre write barrier might scan this object - always zero it */
							initializeSlots = true;
						} else {
							return NULL;
						}
					} else {
						return NULL;
					}
					break;
#endif /* J9VM_GC_SEGREGATED_HEAP */

				default:
					/* Inline allocation not supported */
					return NULL;
				}

				/* Initialize the object */
				if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(currentThread)) {
					J9IndexableObjectContiguousCompressed *header = (J9IndexableObjectContiguousCompressed*)objectHeader;
					header->clazz = (U_32)(UDATA)arrayClass;
					header->size = size;
					if (initializeSlots) {
						memset(header + 1, 0, dataSize);
					}
				} else {
					J9IndexableObjectContiguousFull *header = (J9IndexableObjectContiguousFull*)objectHeader;
					header->clazz = (UDATA)arrayClass;
					header->size = size;
					if (initializeSlots) {
						memset(header + 1, 0, dataSize);
					}
				}
				if (memoryBarrier) {
					VM_AtomicSupport::writeBarrier();
				}
				instance = objectHeader;
			}
		} else {
			/* Zero-length array is discontiguous - assume minimum object size */
			UDATA allocateSize = J9_GC_MINIMUM_OBJECT_SIZE;

			/* Allocate the memory */
			j9object_t objectHeader = NULL;
			switch(_gcAllocationType) {

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
			case OMR_GC_ALLOCATION_TYPE_TLH:

				if (allocateSize <= ((UDATA) currentThread->heapTop - (UDATA) currentThread->heapAlloc)) {
					U_8 *heapAlloc = currentThread->heapAlloc;
					UDATA afterAlloc = (UDATA)heapAlloc + allocateSize;
					objectHeader = (j9object_t) heapAlloc;
					currentThread->heapAlloc = (U_8 *)afterAlloc;
#if defined(J9VM_GC_TLH_PREFETCH_FTA)
					currentThread->tlhPrefetchFTA -= allocateSize;
#endif /* J9VM_GC_TLH_PREFETCH_FTA */
				} else {
					return NULL;
				}
				break;
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

#if defined(J9VM_GC_SEGREGATED_HEAP)
			case OMR_GC_ALLOCATION_TYPE_SEGREGATED:
				/* ensure the allocation will fit in a small size */
				if (allocateSize <= J9VMGC_SIZECLASSES_MAX_SMALL_SIZE_BYTES) {

					/* fetch the size class based on the allocation size */
					UDATA slotsRequested = allocateSize / sizeof(UDATA);
					UDATA sizeClassIndex = _sizeClasses->sizeClassIndex[slotsRequested];

					/* Ensure the cache for the current size class is not empty. */
					J9VMGCSegregatedAllocationCacheEntry *cacheEntry =
							(J9VMGCSegregatedAllocationCacheEntry *)((UDATA)currentThread + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET
									+ (sizeClassIndex * sizeof(J9VMGCSegregatedAllocationCacheEntry)));
					UDATA cellSize = _sizeClasses->smallCellSizes[sizeClassIndex];

					if (cellSize <= ((UDATA) cacheEntry->top - (UDATA) cacheEntry->current)) {
						objectHeader = (j9object_t) cacheEntry->current;
						cacheEntry->current = (UDATA *) ((UDATA) cacheEntry->current + cellSize);
					} else {
						return NULL;
					}
				} else {
					return NULL;
				}
				break;
#endif /* J9VM_GC_SEGREGATED_HEAP */

			default:
				return NULL;
				break;
			}

			/* Initialize the object */
			if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(currentThread)) {
				J9IndexableObjectDiscontiguousCompressed *header = (J9IndexableObjectDiscontiguousCompressed*)objectHeader;
				header->clazz = (U_32)(UDATA)arrayClass;
				header->mustBeZero = 0;
				header->size = 0;
			} else {
				J9IndexableObjectDiscontiguousFull *header = (J9IndexableObjectDiscontiguousFull*)objectHeader;
				header->clazz = (UDATA)arrayClass;
				header->mustBeZero = 0;
				header->size = 0;
			}
			if (memoryBarrier) {
				VM_AtomicSupport::writeBarrier();
			}
			instance = objectHeader;

#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP) */

		}

		return instance;
	}

protected:
public:

	/**
	 * Create an instance.
	 */
	MM_ObjectAllocationAPI(J9VMThread *currentThread)
		: _gcAllocationType(currentThread->javaVM->gcAllocationType)
#if defined(J9VM_GC_BATCH_CLEAR_TLH)
		, _initializeSlotsOnTLHAllocate(currentThread->javaVM->initializeSlotsOnTLHAllocate)
#endif /* J9VM_GC_BATCH_CLEAR_TLH */
		, _objectAlignmentInBytes(currentThread->omrVMThread->_vm->_objectAlignmentInBytes)
#if defined (J9VM_GC_SEGREGATED_HEAP)
		, _sizeClasses(currentThread->javaVM->realtimeSizeClasses)
#endif /* J9VM_GC_SEGREGATED_HEAP */
	{}

	VMINLINE j9object_t
	inlineAllocateObject(J9VMThread *currentThread, J9Class *clazz, bool initializeSlots = true, bool memoryBarrier = true)
	{
		j9object_t instance = NULL;
#if defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP)
		/* Calculate the size of the object */
		UDATA const headerSize = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
		UDATA dataSize = clazz->totalInstanceSize;
		UDATA allocateSize = (dataSize + headerSize + _objectAlignmentInBytes - 1) & ~(UDATA)(_objectAlignmentInBytes - 1);
		if (allocateSize < J9_GC_MINIMUM_OBJECT_SIZE) {
			allocateSize = J9_GC_MINIMUM_OBJECT_SIZE;
		}

		/* Allocate the object */
		switch(_gcAllocationType) {
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
		case OMR_GC_ALLOCATION_TYPE_TLH:
			if (allocateSize <= ((UDATA) currentThread->heapTop - (UDATA) currentThread->heapAlloc)) {
				U_8 *heapAlloc = currentThread->heapAlloc;
				UDATA afterAlloc = (UDATA)heapAlloc + allocateSize;
				currentThread->heapAlloc = (U_8 *)afterAlloc;
#if defined(J9VM_GC_TLH_PREFETCH_FTA)
				currentThread->tlhPrefetchFTA -= allocateSize;
#endif /* J9VM_GC_TLH_PREFETCH_FTA */
				instance = (j9object_t) heapAlloc;
#if defined(J9VM_GC_BATCH_CLEAR_TLH)
				/* Do not zero the TLH if it is already zero'd */
				initializeSlots = initializeSlots && _initializeSlotsOnTLHAllocate;
#endif /* J9VM_GC_BATCH_CLEAR_TLH */
			} else {
				return NULL;
			}
			break;
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

#if defined(J9VM_GC_SEGREGATED_HEAP)
		case OMR_GC_ALLOCATION_TYPE_SEGREGATED:
			/* ensure the allocation will fit in a small size */
			if (allocateSize <= J9VMGC_SIZECLASSES_MAX_SMALL_SIZE_BYTES) {

				/* fetch the size class based on the allocation size */
				UDATA slotsRequested = allocateSize / sizeof(UDATA);
				UDATA sizeClassIndex = _sizeClasses->sizeClassIndex[slotsRequested];

				/* Ensure the cache for the current size class is not empty. */
				J9VMGCSegregatedAllocationCacheEntry *cacheEntry =
						(J9VMGCSegregatedAllocationCacheEntry *)((UDATA)currentThread + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET
								+ (sizeClassIndex * sizeof(J9VMGCSegregatedAllocationCacheEntry)));
				UDATA cellSize = _sizeClasses->smallCellSizes[sizeClassIndex];

				if (cellSize <= ((UDATA) cacheEntry->top - (UDATA) cacheEntry->current)) {
					instance = (j9object_t) cacheEntry->current;
					cacheEntry->current = (UDATA *) ((UDATA) cacheEntry->current + cellSize);
					/* The metronome pre write barrier might scan this object - always zero it */
					initializeSlots = true;
				} else {
					return NULL;
				}
			} else {
				return NULL;
			}
			break;
#endif /* J9VM_GC_SEGREGATED_HEAP */

		default:
			return NULL;
		}

		/* Initialize the object */
		if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(currentThread)) {
			J9ObjectCompressed *objectHeader = (J9ObjectCompressed*) instance;
			objectHeader->clazz = (U_32)(UDATA)clazz;
			if (initializeSlots) {
				memset(objectHeader + 1, 0, dataSize);
			}
		} else {
			J9ObjectFull *objectHeader = (J9ObjectFull*) instance;
			objectHeader->clazz = (UDATA)clazz;
			if (initializeSlots) {
				memset(objectHeader + 1, 0, dataSize);
			}
		}

		if (initializeSlots) {
			if (LN_HAS_LOCKWORD(currentThread, instance)) {
				j9objectmonitor_t initialLockword = VM_ObjectMonitor::getInitialLockword(currentThread->javaVM, clazz);
				if (0 != initialLockword) {
					j9objectmonitor_t *lockEA = J9OBJECT_MONITOR_EA(currentThread, instance);
					J9_STORE_LOCKWORD(currentThread, lockEA, initialLockword);
				}
			}
		}

		if (memoryBarrier) {
			VM_AtomicSupport::writeBarrier();
		}
#endif /* J9VM_GC_THREAD_LOCAL_HEAP || J9VM_GC_SEGREGATED_HEAP */
		return instance;
	}

	VMINLINE j9object_t
	inlineAllocateIndexableValueTypeObject(J9VMThread *currentThread, J9Class *arrayClass, U_32 size, bool initializeSlots = true, bool memoryBarrier = true, bool sizeCheck = true)
	{
		UDATA dataSize = ((UDATA)size) * J9ARRAYCLASS_GET_STRIDE(arrayClass);
		bool validSize = true;
#if !defined(J9VM_ENV_DATA64)
		validSize = !sizeCheck || (size < ((U_32)J9_MAXIMUM_INDEXABLE_DATA_SIZE / J9ARRAYCLASS_GET_STRIDE(arrayClass)));
#endif /* J9VM_ENV_DATA64 */
		return inlineAllocateIndexableObjectImpl(currentThread, arrayClass, size, dataSize, validSize, initializeSlots, memoryBarrier);
	}

	VMINLINE j9object_t
	inlineAllocateIndexableObject(J9VMThread *currentThread, J9Class *arrayClass, U_32 size, bool initializeSlots = true, bool memoryBarrier = true, bool sizeCheck = true)
	{
		UDATA scale = ((J9ROMArrayClass*)(arrayClass->romClass))->arrayShape;
		UDATA dataSize = ((UDATA)size) << scale;
		bool validSize = true;
#if !defined(J9VM_ENV_DATA64)
		validSize = !sizeCheck || (size < ((U_32)J9_MAXIMUM_INDEXABLE_DATA_SIZE >> scale));
#endif /* J9VM_ENV_DATA64 */
		return inlineAllocateIndexableObjectImpl(currentThread, arrayClass, size, dataSize, validSize, initializeSlots, memoryBarrier);
	}

};

#endif /* OBJECTALLOCATIONAPI_HPP_ */
