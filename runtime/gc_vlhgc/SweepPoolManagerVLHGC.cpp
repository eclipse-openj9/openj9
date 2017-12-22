
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#include "j9cfg.h"
#include "j9consts.h"

#include "EnvironmentBase.hpp"
#include "MarkMap.hpp"
#include "MemoryPoolBumpPointer.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepPoolManagerVLHGC.hpp"
#include "SweepPoolState.hpp"


MM_SweepPoolManagerVLHGC *
MM_SweepPoolManagerVLHGC::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepPoolManagerVLHGC *sweepPoolManager = (MM_SweepPoolManagerVLHGC *)env->getForge()->allocate(sizeof(MM_SweepPoolManagerVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != sweepPoolManager) {
		new(sweepPoolManager) MM_SweepPoolManagerVLHGC(env);
		if (!sweepPoolManager->initialize(env)) { 
			sweepPoolManager->kill(env);        
			sweepPoolManager = NULL;            
		}                                       
	}

	return sweepPoolManager;
}

bool
MM_SweepPoolManagerVLHGC::initialize(MM_EnvironmentBase *env)
{
	return MM_SweepPoolManager::initialize(env);
}

MM_SweepPoolState *
MM_SweepPoolManagerVLHGC::getPoolState(MM_MemoryPool *memoryPool)
{
	MM_SweepPoolState *result = ((MM_MemoryPoolBumpPointer *)memoryPool)->getSweepPoolState();

	Assert_MM_true(NULL != result);
	
	return result;
}

MMINLINE void
MM_SweepPoolManagerVLHGC::calculateTrailingDetails(MM_ParallelSweepChunk *sweepChunk, UDATA *trailingCandidate, UDATA trailingCandidateSlotCount)
{
	/* Calculating the trailing free details implies that there is an object
	 * that is in front of the heapSlotFreeHead.  No safety checks required.
	 */
	UDATA trailingCandidateByteCount = (UDATA)MM_Bits::convertSlotsToBytes(trailingCandidateSlotCount);

	UDATA projection = _extensions->objectModel.getConsumedSizeInBytesWithHeader((J9Object *)(trailingCandidate - J9MODRON_HEAP_SLOTS_PER_MARK_BIT)) - (J9MODRON_HEAP_SLOTS_PER_MARK_BIT * sizeof(UDATA));
	if(projection > trailingCandidateByteCount) {
		projection -= trailingCandidateByteCount;
		sweepChunk->projection = projection;
	} else {
		if(projection < trailingCandidateByteCount) {
			sweepChunk->trailingFreeCandidate = (void *)(((UDATA)trailingCandidate) + projection);
			sweepChunk->trailingFreeCandidateSize = trailingCandidateByteCount - projection;
		}
	}
}

void
MM_SweepPoolManagerVLHGC::connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk)
{
	MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)chunk->memoryPool;
	MM_SweepPoolState *sweepState = getPoolState(memoryPool);

	MM_HeapLinkedFreeHeader *previousFreeEntry = sweepState->_connectPreviousFreeEntry;
	UDATA previousFreeEntrySize = sweepState->_connectPreviousFreeEntrySize;
	MM_ParallelSweepChunk *previousConnectChunk = sweepState->_connectPreviousChunk;

	MM_HeapLinkedFreeHeader *leadingFreeEntry = (MM_HeapLinkedFreeHeader *)chunk->leadingFreeCandidate;
	UDATA leadingFreeEntrySize = chunk->leadingFreeCandidateSize;

	Assert_MM_true((NULL == leadingFreeEntry) || (previousFreeEntry < leadingFreeEntry));
	/* Assume no projection until we prove otherwise */	
	UDATA projection = 0;

	/* If there is a projection from the previous chunk? It does not necessarily have to be associated to the pool for the
	 * projection to actually matter.
	 */
	if(NULL != chunk->_previous) {
		Assert_MM_true((0 == chunk->_previous->projection) || (chunk->_previous->chunkTop == chunk->chunkBase));
		projection = chunk->_previous->projection;
	}
	
	/* Any projection to be dealt with ? */
	if (0 != projection) {
		if(projection > (((UDATA)chunk->chunkTop) - ((UDATA)chunk->chunkBase))) {
			chunk->projection = projection - (((UDATA)chunk->chunkTop) - ((UDATA)chunk->chunkBase));
			leadingFreeEntry = (MM_HeapLinkedFreeHeader *)NULL;
			leadingFreeEntrySize = 0;
		} else {
			leadingFreeEntry = (MM_HeapLinkedFreeHeader *)(((UDATA)leadingFreeEntry) + projection);
			leadingFreeEntrySize -= projection;
		}
	} 
	
	/* See if there is a connection to be made between the previous free and the leading entry */
	if((previousFreeEntry != NULL) && ((((U_8 *)previousFreeEntry) + previousFreeEntrySize) == (U_8 *)leadingFreeEntry) && (previousConnectChunk->memoryPool == memoryPool) && chunk->_coalesceCandidate) {
		/* So should be using same MM_SweepPoolState */
		Assert_MM_true(getPoolState(previousConnectChunk->memoryPool) == sweepState);
		
		/* The previous free entry consumes the leading free entry */
		/* This trumps any checks on the trailing free space of the previous chunk */
		previousFreeEntrySize += leadingFreeEntrySize;
		sweepState->_sweepFreeBytes += leadingFreeEntrySize;
		sweepState->_largestFreeEntry = OMR_MAX(previousFreeEntrySize, sweepState->_largestFreeEntry);

		/* Consume the leading entry */
		leadingFreeEntry = NULL;
	}

	/* If the leading entry wasn't consumed, check if there is trailing free space in the previous chunk
	 * that could connect with the leading entry leading free space */
	if(NULL != previousConnectChunk) {
		if((NULL != leadingFreeEntry) && ((((U_8 *)previousConnectChunk->trailingFreeCandidate) + previousConnectChunk->trailingFreeCandidateSize) == (U_8 *)leadingFreeEntry) && (previousConnectChunk->memoryPool == memoryPool) && chunk->_coalesceCandidate) {
			/* The trailing/leading space forms a contiguous chunk - check if it belongs on the free list, abandon otherwise */
			
			UDATA jointFreeSize = leadingFreeEntrySize + previousConnectChunk->trailingFreeCandidateSize;

			if(memoryPool->canMemoryBeConnectedToPool(env, previousConnectChunk->trailingFreeCandidate, jointFreeSize)) {

				/* Free list candidate has been found - attach it to the free list */
				previousFreeEntry = (MM_HeapLinkedFreeHeader *)previousConnectChunk->trailingFreeCandidate;
				previousFreeEntrySize = jointFreeSize;
			
				/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
				if(0 != jointFreeSize) {
					sweepState->_sweepFreeBytes += jointFreeSize;
					sweepState->_sweepFreeHoles += 1;
					sweepState->_largestFreeEntry = OMR_MAX(jointFreeSize, sweepState->_largestFreeEntry);
				}
			}

			/* Consume the leading entry */
			leadingFreeEntry = NULL;
		} else {
			if(memoryPool->canMemoryBeConnectedToPool(env, previousConnectChunk->trailingFreeCandidate, previousConnectChunk->trailingFreeCandidateSize)) {
				previousFreeEntry = (MM_HeapLinkedFreeHeader *)previousConnectChunk->trailingFreeCandidate;
				previousFreeEntrySize = previousConnectChunk->trailingFreeCandidateSize;
			
				/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
				if(0 != previousConnectChunk->trailingFreeCandidateSize) {
					sweepState->_sweepFreeBytes += previousConnectChunk->trailingFreeCandidateSize;
					sweepState->_sweepFreeHoles += 1;
					sweepState->_largestFreeEntry = OMR_MAX(previousConnectChunk->trailingFreeCandidateSize, sweepState->_largestFreeEntry);
				}
			}
		}
	}

	/* If the leading free entry has not been consumed - check if it is now a trailing free candidate.  This scenario can occur if a leading
	 * free entry is pushed back from a projection from the previous sweep chunk.  A leading free candidate that spans the entire chunk in this
	 * case now becomes a trailing free entry.
	 */
	if(leadingFreeEntry && ((void *)(((UDATA)leadingFreeEntry) + leadingFreeEntrySize) == chunk->chunkTop)) {
		chunk->leadingFreeCandidate = NULL;
		chunk->leadingFreeCandidateSize = 0;
		chunk->trailingFreeCandidate = leadingFreeEntry;
		chunk->trailingFreeCandidateSize = leadingFreeEntrySize;
	} else {
		/* If the leading entry still hasn't been consumed, it touches neither the head or tail of a chunk.
		 * Check if it merits an entry in the free list
		 */
		if(NULL != leadingFreeEntry) { 
			if(memoryPool->canMemoryBeConnectedToPool(env, leadingFreeEntry, leadingFreeEntrySize)) {
				
				Assert_MM_true(previousFreeEntry < leadingFreeEntry);
				Assert_MM_true(previousFreeEntry <= leadingFreeEntry);

				previousFreeEntry = leadingFreeEntry;
				previousFreeEntrySize = leadingFreeEntrySize;
					
				/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
				if(0 != leadingFreeEntrySize) {
					sweepState->_sweepFreeBytes += leadingFreeEntrySize;
					sweepState->_sweepFreeHoles += 1;
					sweepState->_largestFreeEntry = OMR_MAX(leadingFreeEntrySize, sweepState->_largestFreeEntry);
				}
			}	
		}	
	}

	/* If there is a previous free entry
	 * and a free list head in the current chunk, attach
	 * otherwise, make chunk head the free list head for the profile 
	 */
	if(chunk->freeListHead) {
		Assert_MM_true(previousFreeEntry < chunk->freeListHead);
		
		/* If there is a head, there is a tail - update the previous free entry */
		previousFreeEntry = chunk->freeListTail;
		previousFreeEntrySize = chunk->freeListTailSize;

		/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
		if(0 != chunk->freeBytes) {
			sweepState->_sweepFreeBytes += chunk->freeBytes;
			sweepState->_sweepFreeHoles += chunk->freeHoles;
		}

		/* Adjusted the largest free entry found for the sweep state */
		sweepState->_largestFreeEntry = OMR_MAX(chunk->_largestFreeEntry, sweepState->_largestFreeEntry);
	}

	/* Update the allocate profile with the previous free entry and previous chunk */
	sweepState->_connectPreviousFreeEntry = previousFreeEntry;
	sweepState->_connectPreviousFreeEntrySize = previousFreeEntrySize;
	sweepState->_connectPreviousChunk = chunk;
	
	memoryPool->incrementDarkMatterBytes(chunk->_darkMatterBytes);
	memoryPool->incrementDarkMatterSamples(chunk->_darkMatterSamples);
	memoryPool->incrementScannableBytes(chunk->_scannableBytes, chunk->_nonScannableBytes);
	Assert_MM_true((sweepState->_sweepFreeBytes + memoryPool->getDarkMatterBytes()) <= _extensions->regionSize);
}

void
MM_SweepPoolManagerVLHGC::flushFinalChunk(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool)
{
	MM_SweepPoolState *sweepState = getPoolState(memoryPool);

	/* If the last chunk had trailing free space, try and add it to the free list */
	if((sweepState->_connectPreviousChunk != NULL) && (sweepState->_connectPreviousChunk->trailingFreeCandidateSize > 0)) {
		/* Check if the entry is a candidate */
		if (((MM_MemoryPoolBumpPointer *)memoryPool)->canMemoryBeConnectedToPool(envModron, sweepState->_connectPreviousChunk->trailingFreeCandidate, sweepState->_connectPreviousChunk->trailingFreeCandidateSize)) {
			sweepState->_connectPreviousFreeEntry = (MM_HeapLinkedFreeHeader *)sweepState->_connectPreviousChunk->trailingFreeCandidate;
			sweepState->_connectPreviousFreeEntrySize = sweepState->_connectPreviousChunk->trailingFreeCandidateSize;
			Assert_MM_true(sweepState->_connectPreviousFreeEntry != sweepState->_connectPreviousChunk->leadingFreeCandidate);
	
			sweepState->_sweepFreeBytes += sweepState->_connectPreviousChunk->trailingFreeCandidateSize;
			sweepState->_sweepFreeHoles += 1;
			sweepState->_largestFreeEntry = OMR_MAX(sweepState->_connectPreviousChunk->trailingFreeCandidateSize, sweepState->_largestFreeEntry);
		}
	}
}

void
MM_SweepPoolManagerVLHGC::connectFinalChunk(MM_EnvironmentBase *envModron,MM_MemoryPool *memoryPool)
{
	/* Update pool free memory statistics since they are required to identify free regions to recycle */
	MM_SweepPoolState *sweepState = getPoolState(memoryPool);
	memoryPool->setFreeMemorySize(sweepState->_sweepFreeBytes);
	memoryPool->setFreeEntryCount(sweepState->_sweepFreeHoles);
	memoryPool->setLargestFreeEntry(sweepState->_largestFreeEntry);
	MM_MemoryPoolBumpPointer *bpPool = (MM_MemoryPoolBumpPointer *)memoryPool;
	UDATA actualFreeMemory = bpPool->getActualFreeMemorySize();
	UDATA allocatableBytes = bpPool->getAllocatableBytes();
	if (0 == actualFreeMemory) {
		Assert_MM_true(allocatableBytes < bpPool->getMinimumFreeEntrySize());
	} else {
		Assert_MM_true(allocatableBytes <= actualFreeMemory);
	}
}

void
MM_SweepPoolManagerVLHGC::poolPostProcess(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool)
{
	Assert_MM_unreachable();
}

void
MM_SweepPoolManagerVLHGC::updateTrailingFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, UDATA *heapSlotFreeHead, UDATA heapSlotFreeCount)
{
	calculateTrailingDetails(sweepChunk, heapSlotFreeHead, heapSlotFreeCount);
}

bool
MM_SweepPoolManagerVLHGC::addFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, UDATA *address, UDATA size)
{
	bool result = false;
	
	/* This implementation is able to support SORTED pieces of memory ONLY!!! */
	Assert_MM_true((UDATA *)sweepChunk->freeListTail <= address);

	if(address == (UDATA *)sweepChunk->chunkBase) {
		/* Update the sweep chunk table entry with the leading free hole information */
		sweepChunk->leadingFreeCandidate = address;
		sweepChunk->leadingFreeCandidateSize = (UDATA)MM_Bits::convertSlotsToBytes(size);
		Assert_MM_true(sweepChunk->leadingFreeCandidate > sweepChunk->trailingFreeCandidate);
	} else if(address + size == (UDATA *)sweepChunk->chunkTop) {
		/* Update the sweep chunk table entry with the trailing free hole information */
		calculateTrailingDetails(sweepChunk, address, size);
	} else {
		UDATA freeSizeInBytes = MM_Bits::convertSlotsToBytes(size);
		J9Object *object = (J9Object *)(address - J9MODRON_HEAP_SLOTS_PER_MARK_BIT);
		Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(object));
		UDATA objectSizeDelta = 
			_extensions->objectModel.getConsumedSizeInBytesWithHeader(object)
			- (J9MODRON_HEAP_SLOTS_PER_MARK_BIT * sizeof(UDATA));
		Assert_MM_true(objectSizeDelta <= freeSizeInBytes);
		freeSizeInBytes -= objectSizeDelta;
		address = (UDATA *) (((UDATA)address) + objectSizeDelta);

		if (((MM_MemoryPoolBumpPointer *)sweepChunk->memoryPool)->canMemoryBeConnectedToPool(env, address, freeSizeInBytes)) {
			/* If the hole is first in this chunk, make it the header */
			if(NULL == sweepChunk->freeListTail) {
				sweepChunk->freeListHead = (MM_HeapLinkedFreeHeader *)address;
				sweepChunk->freeListHeadSize = freeSizeInBytes;
			}

			/* Maintain the free bytes/holes count in the chunk for heap expansion/contraction purposes (will be gathered up in the allocate profile) */
			if(0 != freeSizeInBytes) {
				sweepChunk->freeBytes += freeSizeInBytes;
				sweepChunk->freeHoles += 1;
				sweepChunk->_largestFreeEntry = OMR_MAX(sweepChunk->_largestFreeEntry , freeSizeInBytes);
			}

			sweepChunk->freeListTail = (MM_HeapLinkedFreeHeader *)address;
			sweepChunk->freeListTailSize = freeSizeInBytes;
		}
		result = true;
	}
	return result;
}
