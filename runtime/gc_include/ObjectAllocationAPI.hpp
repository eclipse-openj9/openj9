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

#if !defined(OBJECTALLOCATIONAPI_HPP_)
#define OBJECTALLOCATIONAPI_HPP_

#include "j9.h"
#include "j9generated.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "AtomicSupport.hpp"

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
		bool initializeLockWord = initializeSlots
			&& J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassReservableLockWordInit);

		/* Calculate the size of the object */
		UDATA dataSize = clazz->totalInstanceSize;
		UDATA allocateSize = (dataSize + J9_OBJECT_HEADER_SIZE + _objectAlignmentInBytes - 1) & ~(UDATA)(_objectAlignmentInBytes - 1);
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
		J9NonIndexableObject *objectHeader = (J9NonIndexableObject *) instance;
		objectHeader->clazz = (j9objectclass_t)(UDATA)clazz;

		if (initializeSlots) {
			memset(objectHeader + 1, 0, dataSize);
		}

		if (initializeLockWord) {
			*J9OBJECT_MONITOR_EA(currentThread, instance) = OBJECT_HEADER_LOCK_RESERVED;
		}

		if (memoryBarrier) {
			VM_AtomicSupport::writeBarrier();
		}
#endif /* J9VM_GC_THREAD_LOCAL_HEAP || J9VM_GC_SEGREGATED_HEAP */
		return instance;
	}

	VMINLINE j9object_t
	inlineAllocateIndexableObject(J9VMThread *currentThread, J9Class *arrayClass, U_32 size, bool initializeSlots = true, bool memoryBarrier = true, bool sizeCheck = true)
	{
		j9object_t instance = NULL;

#if defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP)
		if (0 != size) {
			/* Contiguous Array */


#if !defined(J9VM_ENV_DATA64)
			if (!sizeCheck || (size < ((U_32)J9_MAXIMUM_INDEXABLE_DATA_SIZE / J9ARRAYCLASS_GET_STRIDE(arrayClass))))
#endif /* J9VM_ENV_DATA64 */
			{
				/* Calculate the size of the object */
				UDATA dataSize = ((UDATA)size) * J9ARRAYCLASS_GET_STRIDE(arrayClass);
				UDATA allocateSize = (dataSize + sizeof(J9IndexableObjectContiguous) + _objectAlignmentInBytes - 1) & ~(UDATA)(_objectAlignmentInBytes - 1);
				if (allocateSize < J9_GC_MINIMUM_OBJECT_SIZE) {
					allocateSize = J9_GC_MINIMUM_OBJECT_SIZE;
				}

				/* Allocate the memory */
				J9IndexableObjectContiguous *objectHeader = NULL;

				switch(_gcAllocationType) {

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
				case OMR_GC_ALLOCATION_TYPE_TLH:
					if (allocateSize <= ((UDATA) currentThread->heapTop - (UDATA) currentThread->heapAlloc)) {
						U_8 *heapAlloc = currentThread->heapAlloc;
						UDATA afterAlloc = (UDATA)heapAlloc + allocateSize;
						objectHeader = (J9IndexableObjectContiguous*)heapAlloc;
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
							objectHeader = (J9IndexableObjectContiguous*)cacheEntry->current;
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
				objectHeader->clazz = (j9objectclass_t)(UDATA)arrayClass;
				objectHeader->size = size;

				if (initializeSlots) {
					memset(objectHeader + 1, 0, dataSize);
				}
				if (memoryBarrier) {
					VM_AtomicSupport::writeBarrier();
				}
				instance = (j9object_t) objectHeader;
			}
		} else {
			/* Discontiguous Array */
#if J9SIZEOF_J9IndexableObjectDiscontiguous <= J9_GC_MINIMUM_OBJECT_SIZE

			/* Calculate the object size */
			UDATA allocateSize = J9_GC_MINIMUM_OBJECT_SIZE;

			/* Allocate the memory */
			J9IndexableObjectDiscontiguous *objectHeader = NULL;
			switch(_gcAllocationType) {

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
			case OMR_GC_ALLOCATION_TYPE_TLH:

				if (allocateSize <= ((UDATA) currentThread->heapTop - (UDATA) currentThread->heapAlloc)) {
					U_8 *heapAlloc = currentThread->heapAlloc;
					UDATA afterAlloc = (UDATA)heapAlloc + allocateSize;
					objectHeader = (J9IndexableObjectDiscontiguous *) heapAlloc;
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
						objectHeader = (J9IndexableObjectDiscontiguous *) cacheEntry->current;
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
			objectHeader->clazz = (j9objectclass_t)(UDATA)arrayClass;
			objectHeader->mustBeZero = 0;
			objectHeader->size = 0;
			if (memoryBarrier) {
				VM_AtomicSupport::writeBarrier();
			}
			instance = (j9object_t) objectHeader;
#endif /* J9SIZEOF_J9IndexableObjectDiscontiguous <= J9_GC_MINIMUM_OBJECT_SIZE */

#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP) */

		}

		return instance;
	}

};

#endif /* OBJECTALLOCATIONAPI_HPP_ */
