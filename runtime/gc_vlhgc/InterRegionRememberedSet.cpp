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
/**
 * @file
 * @ingroup gc_vlhgc
 */

#include "ModronAssertions.h"

#include "InterRegionRememberedSet.hpp"

#include "AllocationContextTarok.hpp"
#include "AtomicOperations.hpp"
#include "Bits.hpp"
#include "CardTable.hpp"
#include "ClassLoaderRememberedSet.hpp"
#include "CollectionStatisticsVLHGC.hpp"
#include "CompressedCardTable.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "MixedObjectIterator.hpp"
#include "PointerArrayIterator.hpp"
#include "RememberedSetCardListBufferIterator.hpp"
#include "RememberedSetCardListCardIterator.hpp"
#include "Task.hpp"
#include "VMThreadListIterator.hpp"

MM_InterRegionRememberedSet::MM_InterRegionRememberedSet(MM_HeapRegionManager *heapRegionManager)
	: _heapRegionManager(heapRegionManager)
	, _rsclBufferControlBlockPool(NULL)
	, _rsclBufferControlBlockHead(NULL)
	, _freeBufferCount(0)
	, _bufferCountTotal(0)
	, _bufferControlBlockCountPerRegion(0)
	, _overflowedListHead(NULL)
	, _overflowedListTail(NULL)
	, _regionSize(0)
	, _shouldFlushBuffersForDecommitedRegions(false)
	, _overflowedRegionCount(0)
	, _stableRegionCount(0)
	, _beingRebuiltRegionCount(0)
	, _unusedRegionThreshold(0.0)
	, _regionTable(NULL)
	, _tableDescriptorSize(0)
	, _cardToRegionShift(0)
	, _cardToRegionDisplacement(0)
	, _cardTable(NULL)
	, _rememberedSetCardBucketPool(NULL)
{
	_typeId = __FUNCTION__;
}


MM_InterRegionRememberedSet*
MM_InterRegionRememberedSet::newInstance(MM_EnvironmentVLHGC* env, MM_HeapRegionManager *heapRegionManager)
{
	MM_InterRegionRememberedSet* interRegionRememberedSet = (MM_InterRegionRememberedSet*)env->getForge()->allocate(sizeof(MM_InterRegionRememberedSet), MM_AllocationCategory::REMEMBERED_SET, J9_GET_CALLSITE());
	if (interRegionRememberedSet) {
		new(interRegionRememberedSet) MM_InterRegionRememberedSet(heapRegionManager);
		if (!interRegionRememberedSet->initialize(env)) {
			interRegionRememberedSet->kill(env);
			interRegionRememberedSet = NULL;
		}
	}
	return interRegionRememberedSet;
}

void
MM_InterRegionRememberedSet::exportStats(MM_EnvironmentVLHGC* env, MM_CollectionStatisticsVLHGC *stats)
{
	/* TODO: this formula for _rememberedSetCount is an overstatement - try to be more accurate */
	stats->_rememberedSetCount = (_bufferCountTotal - _freeBufferCount) * MM_RememberedSetCardBucket::MAX_BUFFER_SIZE;
	stats->_rememberedSetBytesFree = _freeBufferCount * MM_RememberedSetCardBucket::MAX_BUFFER_SIZE * sizeof(MM_RememberedSetCard);
	stats->_rememberedSetBytesTotal = _bufferCountTotal * MM_RememberedSetCardBucket::MAX_BUFFER_SIZE * sizeof(MM_RememberedSetCard);
	stats->_rememberedSetOverflowedRegionCount = _overflowedRegionCount;
	stats->_rememberedSetStableRegionCount = _stableRegionCount;
	stats->_rememberedSetBeingRebuiltRegionCount = _beingRebuiltRegionCount;
}


void
MM_InterRegionRememberedSet::threadLocalInitialize(MM_EnvironmentVLHGC* env)
{
	for (UDATA index = 0; index < _heapRegionManager->getTableRegionCount(); index++) {
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->physicalTableDescriptorForIndex(index);
		region->getRememberedSetCardList()->initialize(env, index);
	}
}

MM_HeapRegionDescriptorVLHGC *
MM_InterRegionRememberedSet::getBufferOwningRegion(MM_CardBufferControlBlock *cardBufferControlBlock)
{
	UDATA bufferIndex = cardBufferControlBlock - _rsclBufferControlBlockPool;
	UDATA bufferOwningRegionIndex = bufferIndex / _bufferControlBlockCountPerRegion;
	return (MM_HeapRegionDescriptorVLHGC *) _heapRegionManager->mapRegionTableIndexToDescriptor(bufferOwningRegionIndex);
}

void
MM_InterRegionRememberedSet::flushBuffersForDecommitedRegions(MM_EnvironmentVLHGC* env)
{
	if (_shouldFlushBuffersForDecommitedRegions) {
		_shouldFlushBuffersForDecommitedRegions = false;

		/* At this point there should be no non-empty buffers used by decommitted regions.
		 * Now, walk the global pool of free buffers, and remove them from the list if they are owned by decommitted region.
		 * Before that, move all free buffers from thread local pools to the global
		 */

		releaseCardBufferControlBlockLocalPools(env);

		MM_CardBufferControlBlock *currentRsclBufferControlBlock = _rsclBufferControlBlockHead;
		MM_CardBufferControlBlock *previousRsclBufferControlBlock = NULL;

		while (NULL != currentRsclBufferControlBlock) {
			/* find a region owning this Buffer */
			MM_HeapRegionDescriptorVLHGC *bufferOwningRegion  = getBufferOwningRegion(currentRsclBufferControlBlock);
			if (!bufferOwningRegion->isCommitted()) {
				Assert_MM_true(NULL != bufferOwningRegion->getRsclBufferPool());
				/* remove current buffer from the list */
				if (NULL == previousRsclBufferControlBlock) {
					_rsclBufferControlBlockHead = currentRsclBufferControlBlock->_next;
				} else {
					previousRsclBufferControlBlock->_next = currentRsclBufferControlBlock->_next;
				}
				Assert_MM_true(_freeBufferCount > 0);
				_freeBufferCount -= 1;
			} else {
				previousRsclBufferControlBlock =  currentRsclBufferControlBlock;
			}

			/* move to next Buffer */
			currentRsclBufferControlBlock = currentRsclBufferControlBlock->_next;

		}
		

		/* finally, free up RSCL buffer memory owned by decommited regions */
		for (UDATA i = 0; i < _heapRegionManager->getTableRegionCount(); i++) {
			MM_HeapRegionDescriptorVLHGC *region  = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->mapRegionTableIndexToDescriptor(i);
			if (!region->isCommitted() && (NULL != region->_rsclBufferPool)) {
				MM_GCExtensions::getExtensions(env)->getForge()->free(region->_rsclBufferPool);
				Assert_MM_true(_bufferCountTotal > 0);
				_bufferCountTotal -= _bufferControlBlockCountPerRegion;
				region->_rsclBufferPool = NULL;
			}
		}
	}
}


bool
MM_InterRegionRememberedSet::allocateRegionBuffers(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region)
{
	bool success = false;

	if (NULL != region->_rsclBufferPool) {
		/* if already allocated in past for this region, do not do it again */
		success = true;
	} else {
		MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);

		UDATA bufferSize = MM_RememberedSetCardBucket::MAX_BUFFER_SIZE * sizeof(MM_RememberedSetCard);
		UDATA bufferCount = ext->tarokRememberedSetCardListSize / MM_RememberedSetCardBucket::MAX_BUFFER_SIZE;

		/* the pool of buffers have to be bufferSize aligned */
		UDATA rsclBufferPoolSize = (bufferCount + 1) * bufferSize;
		region->_rsclBufferPool = (MM_RememberedSetCard *)ext->getForge()->allocate(rsclBufferPoolSize, MM_AllocationCategory::REMEMBERED_SET, J9_GET_CALLSITE());

		if (NULL != region->_rsclBufferPool) {
			/* the pool of buffers have to be bufferSize aligned */
			MM_RememberedSetCard *rsclBufferPoolAligned = (MM_RememberedSetCard *)(((UDATA)region->_rsclBufferPool + bufferSize) & ~(bufferSize - 1));

			/* Initialize BufferControlBlocks */

			UDATA regionIndex = MM_GCExtensions::getExtensions(env)->heapRegionManager->mapDescriptorToRegionTableIndex(region);
			UDATA bufferControlBlockCountPerRegion = ext->tarokRememberedSetCardListSize / MM_RememberedSetCardBucket::MAX_BUFFER_SIZE;
			UDATA firstBufferControlBlockIndex = bufferControlBlockCountPerRegion * regionIndex;
			UDATA lastBufferControlBlockIndex = firstBufferControlBlockIndex + bufferControlBlockCountPerRegion;
			/* link buffers among themselves */
			for (UDATA i = firstBufferControlBlockIndex; i < lastBufferControlBlockIndex; i++) {
				_rsclBufferControlBlockPool[i]._card = &rsclBufferPoolAligned[(i - firstBufferControlBlockIndex) * MM_RememberedSetCardBucket::MAX_BUFFER_SIZE];
				_rsclBufferControlBlockPool[i]._next = &_rsclBufferControlBlockPool[i + 1];
			}

			/* link all buffers to the head of the global buffer pool list */
			_lock.acquire();
			/* last BCB of this region will point to head */
			_rsclBufferControlBlockPool[lastBufferControlBlockIndex - 1]._next = _rsclBufferControlBlockHead;
			/* head will point to the first BCB of this region */
			_rsclBufferControlBlockHead = &_rsclBufferControlBlockPool[firstBufferControlBlockIndex];
			_freeBufferCount += bufferControlBlockCountPerRegion;
			_bufferCountTotal += bufferControlBlockCountPerRegion;
			Assert_MM_true(_freeBufferCount <= _bufferCountTotal);
			Assert_MM_true(_bufferCountTotal <= (_bufferControlBlockCountPerRegion * _heapRegionManager->getTableRegionCount()));

			if (ext->tarokEnableExpensiveAssertions) {
				MM_CardBufferControlBlock *currentRsclBufferControlBlock = _rsclBufferControlBlockHead;
				UDATA countBCB = 0;
				while (NULL != currentRsclBufferControlBlock) {
					countBCB += 1;
					currentRsclBufferControlBlock = currentRsclBufferControlBlock->_next;
				}

				Assert_MM_true(countBCB ==_freeBufferCount);
			}

			_lock.release();

			success = true;
		}
	}

	return success;
}

bool
MM_InterRegionRememberedSet::initialize(MM_EnvironmentVLHGC* env)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);

	if (!_lock.initialize(env, &ext->lnrlOptions, "MM_InterRegionRememberedSet:_lock")) {
		return false;
	}
	/* if tarokRememberedSetCardListSize is not a multiple of  MM_RememberedSetCardBucket::MAX_BUFFER_SIZE, we are going to allocate a little less */
	_bufferControlBlockCountPerRegion = ext->tarokRememberedSetCardListSize / MM_RememberedSetCardBucket::MAX_BUFFER_SIZE;
	UDATA totalBufferControlBlockCount = _bufferControlBlockCountPerRegion * _heapRegionManager->getTableRegionCount();
	UDATA rsclBufferControlBlockPoolSize = totalBufferControlBlockCount * sizeof(MM_CardBufferControlBlock);
	/* buffer size has to be a power of 2 */
	UDATA bufferSize = MM_RememberedSetCardBucket::MAX_BUFFER_SIZE * sizeof(MM_RememberedSetCard);
	Assert_MM_true(((UDATA)1 << MM_Bits::leadingZeroes(bufferSize)) == bufferSize);

	/* All Buffer Control Block are pre-allocated on startup. Buffers themselves are allocated as regions are commited */
	_rsclBufferControlBlockPool = (MM_CardBufferControlBlock *)ext->getForge()->allocate(rsclBufferControlBlockPoolSize, MM_AllocationCategory::REMEMBERED_SET, J9_GET_CALLSITE());
	if (NULL == _rsclBufferControlBlockPool) {
		return false;
	}
	_rsclBufferControlBlockHead = NULL;

	/* Cache and make sure region size is a power of 2 */
	_regionSize = _heapRegionManager->getRegionSize();
	Assert_MM_true(((UDATA)1 << MM_Bits::leadingZeroes(_regionSize)) == _regionSize);
	
	/* Initialize fields related to RememberedSetCard calculations*/
	_regionTable = _heapRegionManager->_regionTable;
	_tableDescriptorSize = _heapRegionManager->_tableDescriptorSize;
	UDATA baseOfHeap = (UDATA) (_heapRegionManager->_regionTable)->getLowAddress();
#if defined(OMR_GC_COMPRESSED_POINTERS)
	if (env->compressObjectReferences()) {
		_cardToRegionShift = _heapRegionManager->_regionShift - CARD_SIZE_SHIFT;
		_cardToRegionDisplacement = baseOfHeap >> CARD_SIZE_SHIFT;
	} else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	{
		_cardToRegionShift = _heapRegionManager->_regionShift;
		_cardToRegionDisplacement = baseOfHeap;
	}
	_cardTable = ext->cardTable;

	return true;
}


void 
MM_InterRegionRememberedSet::rememberReferenceInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, MM_HeapRegionDescriptorVLHGC *toRegion)
{
	toRegion->getRememberedSetCardList()->add(env, fromObject);

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	/* since we are now remembering this object, set its remembered bit */
	if (!extensions->objectModel.isRemembered(fromObject)) {
		extensions->objectModel.setRememberedBits(fromObject, STATE_REMEMBERED);
	}
}

MM_CardBufferControlBlock *
MM_InterRegionRememberedSet::allocateCardBufferControlBlockFromLocalPool(MM_EnvironmentVLHGC* env)
{
	MM_CardBufferControlBlock *retValue = NULL;

	if (NULL == env->_rsclBufferControlBlockHead) {
		 allocateCardBufferControlBlockList(env, MAX_LOCAL_RSCL_BUFFER_POOL_SIZE);
	}

	if (NULL != env->_rsclBufferControlBlockHead) {
		retValue = env->_rsclBufferControlBlockHead;
		env->_rsclBufferControlBlockHead = env->_rsclBufferControlBlockHead->_next;
		env->_rsclBufferControlBlockCount -= 1;
		if (NULL == env->_rsclBufferControlBlockHead) {
			Assert_MM_true(0 == env->_rsclBufferControlBlockCount);
			env->_rsclBufferControlBlockTail = NULL;
		} else {
			Assert_MM_true(0 < env->_rsclBufferControlBlockCount);
			Assert_MM_true(NULL != env->_rsclBufferControlBlockTail);
			Assert_MM_true(NULL == env->_rsclBufferControlBlockTail->_next);
		}
	}

	return retValue;
}

void
MM_InterRegionRememberedSet::allocateCardBufferControlBlockList(MM_EnvironmentVLHGC* env, UDATA bufferCount)
{
	Assert_MM_true(bufferCount >= 1);
	Assert_MM_true(0 == env->_rsclBufferControlBlockCount);
	Assert_MM_true(NULL == env->_rsclBufferControlBlockTail);
	_lock.acquire();

	if (NULL != _rsclBufferControlBlockHead) {
		MM_CardBufferControlBlock *controlBlockCurrent = _rsclBufferControlBlockHead;

		do {
			bufferCount -= 1;
			_freeBufferCount -= 1;
			env->_rsclBufferControlBlockCount += 1;
			env->_rsclBufferControlBlockTail = controlBlockCurrent;
			controlBlockCurrent = controlBlockCurrent->_next;
		} while ((NULL != controlBlockCurrent) && (0 < bufferCount));

		env->_rsclBufferControlBlockHead = _rsclBufferControlBlockHead;
		_rsclBufferControlBlockHead = controlBlockCurrent;
		Assert_MM_true(NULL != env->_rsclBufferControlBlockTail);
		env->_rsclBufferControlBlockTail->_next = NULL;
	}
	_lock.release();

}

UDATA
MM_InterRegionRememberedSet::releaseCardBufferControlBlockListToLocalPool(MM_EnvironmentVLHGC* env, MM_CardBufferControlBlock *controlBlockHead, UDATA maxBuffersToLocalPool)
{
	UDATA bufferCount = 0;
	MM_CardBufferControlBlock *controlBlockCurrent = controlBlockHead;
	MM_CardBufferControlBlock *controlBlockTail = NULL;

	/* only up to maxBuffersToLocalPool buffers is released to the local pool */
	if ((NULL != controlBlockCurrent) && ((UDATA)env->_rsclBufferControlBlockCount < maxBuffersToLocalPool)) {
		do {
			bufferCount += 1;
			controlBlockTail = controlBlockCurrent;
			controlBlockCurrent = controlBlockCurrent->_next;
			env->_rsclBufferControlBlockCount += 1;
		} while ((NULL != controlBlockCurrent) && ((UDATA)env->_rsclBufferControlBlockCount < maxBuffersToLocalPool));

		controlBlockTail->_next = env->_rsclBufferControlBlockHead;
		if (NULL == env->_rsclBufferControlBlockHead) {
			env->_rsclBufferControlBlockTail = controlBlockTail;
		} else {
			Assert_MM_true(0 < env->_rsclBufferControlBlockCount);
			Assert_MM_true(NULL != env->_rsclBufferControlBlockTail);
			Assert_MM_true(NULL == env->_rsclBufferControlBlockTail->_next);
		}
		env->_rsclBufferControlBlockHead = controlBlockHead;
	}

	/* if there are buffers still left, release them to the global pool */
	if (NULL != controlBlockCurrent) {
		bufferCount += releaseCardBufferControlBlockList(env, controlBlockCurrent, NULL);
	}

	return bufferCount;
}

UDATA
MM_InterRegionRememberedSet::releaseCardBufferControlBlockListToGlobalPool(MM_EnvironmentVLHGC* env, MM_CardBufferControlBlock *controlBlockHead)
{
	return releaseCardBufferControlBlockListToLocalPool(env, controlBlockHead, MAX_LOCAL_RSCL_BUFFER_POOL_SIZE);
}

UDATA
MM_InterRegionRememberedSet::releaseCardBufferControlBlockList(MM_EnvironmentVLHGC* env, MM_CardBufferControlBlock *controlBlockHead, MM_CardBufferControlBlock *controlBlockTail)
{
	UDATA bufferCount = 0;

	if (NULL != controlBlockHead) {
		MM_CardBufferControlBlock *controlBlockCurrent = controlBlockHead;

		/* even if caller does not provide tail info, for statistical reasons
		 * we want to explicitly find the tail and count the buffers in the list
		 */
		MM_CardBufferControlBlock *controlBlockTailPrevious = NULL;

		while (NULL != controlBlockCurrent) {
			bufferCount += 1;
			controlBlockTailPrevious = controlBlockCurrent;
			controlBlockCurrent = controlBlockCurrent->_next;
		}

		if (NULL == controlBlockTail) {
			controlBlockTail = controlBlockTailPrevious;
		} else {
			Assert_MM_true(controlBlockTail == controlBlockTailPrevious);
		}

		_lock.acquire();

		_freeBufferCount += bufferCount;
		controlBlockTail->_next = _rsclBufferControlBlockHead;
		_rsclBufferControlBlockHead = controlBlockHead;

		_lock.release();
	}

	return bufferCount;
}

void
MM_InterRegionRememberedSet::releaseCardBufferControlBlockListForThread(MM_EnvironmentVLHGC* env, MM_EnvironmentVLHGC* threadEnv)
{
	threadEnv->_rsclBufferControlBlockCount -= releaseCardBufferControlBlockList(env, threadEnv->_rsclBufferControlBlockHead, threadEnv->_rsclBufferControlBlockTail);
	Assert_MM_true(0 == threadEnv->_rsclBufferControlBlockCount);
	threadEnv->_rsclBufferControlBlockHead = NULL;
	threadEnv->_rsclBufferControlBlockTail = NULL;
	threadEnv->_lastOverflowedRsclWithReleasedBuffers = NULL;
}

void
MM_InterRegionRememberedSet::releaseCardBufferControlBlockLocalPools(MM_EnvironmentVLHGC* env)
{
	GC_VMThreadListIterator vmThreadListIterator((J9JavaVM *)env->getLanguageVM());
	J9VMThread *aThread;

	/* release all slave-GC-thread local buffers */
	while((aThread = vmThreadListIterator.nextVMThread()) != NULL) {
		MM_EnvironmentVLHGC *threadEnvironment = MM_EnvironmentVLHGC::getEnvironment(aThread);
		if (GC_SLAVE_THREAD == threadEnvironment->getThreadType()) {
			releaseCardBufferControlBlockListForThread(env, threadEnvironment);
		}
	}

	/* do the same for master-GC-thread */
	releaseCardBufferControlBlockListForThread(env, env);

	_overflowedListHead = NULL;
	_overflowedListTail = NULL;

}


void
MM_InterRegionRememberedSet::tearDown(MM_EnvironmentBase* env)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);

	if (NULL != _rsclBufferControlBlockPool) {
		ext->getForge()->free(_rsclBufferControlBlockPool);
	}

	/* TODO: _lock initialize might have failed */
	_lock.tearDown();
}


void
MM_InterRegionRememberedSet::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}


void
MM_InterRegionRememberedSet::resetOverflowedList() {
	_overflowedListHead = NULL;
	_overflowedListTail = NULL;
}


void
MM_InterRegionRememberedSet::setUnusedRegionThreshold(MM_EnvironmentVLHGC *env, double averageEmptinessOfCopyForwardedRegions)
{
	_unusedRegionThreshold = averageEmptinessOfCopyForwardedRegions;
}

void
MM_InterRegionRememberedSet::overflowIfStableRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
{
	if (MM_GCExtensions::getExtensions(env)->tarokEnableStableRegionDetection) {
		MM_RememberedSetCardList *rscl = region->getRememberedSetCardList();

		if (rscl->isAccurate()) {
			MM_MemoryPoolBumpPointer *pool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
			IDATA unusedSize = pool->getDarkMatterBytes() + pool->getActualFreeMemorySize();
			if (unusedSize < (IDATA)(_regionSize * _unusedRegionThreshold)) {
				rscl->setAsStable();
				_stableRegionCount += 1;
				rscl->releaseBuffers(env);
			}
		}
	}
}

void
MM_InterRegionRememberedSet::enqueueOverflowedRscl(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *rsclToEnqueue)
{
	/* have to use atomic update, since other overflowed RSCLs can be updating it concurrently */
	MM_AtomicOperations::add(&_overflowedRegionCount, 1);

	rsclToEnqueue->_nonEmptyOverflowedNext = NULL;
	/* make sure rsclToEnqueue->_nonEmptyOverflowedNext does not point to a stale RSCL before we make it visible to the other users of the list */
	MM_AtomicOperations::storeSync();

	/* put rsclToEnqueue at the end of the list - append it to the current tail */
	volatile UDATA *localAddr = (volatile UDATA *)&_overflowedListTail;
	MM_RememberedSetCardList *oldTail = _overflowedListTail;
	while ((UDATA)oldTail != MM_AtomicOperations::lockCompareExchange(localAddr, (UDATA)oldTail, (UDATA)rsclToEnqueue)) {
		oldTail = _overflowedListTail;
	}

	if (NULL == oldTail) {
		/* if this thread set tail first time, it is to set head as well (to the same RSCL we set tail to point to) */
		_overflowedListHead = rsclToEnqueue;
	} else {
		/* head and tail have been established already, just make connect old with our RSCL */
		oldTail->_nonEmptyOverflowedNext = rsclToEnqueue;
	}
}

MM_RememberedSetCardList *
MM_InterRegionRememberedSet::findRsclToOverflow(MM_EnvironmentVLHGC *env)
{
	MM_RememberedSetCardList *listToOverflow = NULL;

	MM_RememberedSetCardList *candidateListToOverflow = NULL;
	/* if we started walking the list of overflowed regions in this cycle, continue walking at where we stopped last time
	 * otherwise start from the beginning of the list
	 */
	if (NULL !=	env->_lastOverflowedRsclWithReleasedBuffers) {
		candidateListToOverflow = env->_lastOverflowedRsclWithReleasedBuffers->_nonEmptyOverflowedNext;
	} else {
		candidateListToOverflow = _overflowedListHead;
	}

	/* keep iterating the list until we find an (overflowed) RSCL that with buffers in the current bucket */
	while (NULL != candidateListToOverflow) {
		Assert_MM_true(candidateListToOverflow->isOverflowed());
		env->_lastOverflowedRsclWithReleasedBuffers = candidateListToOverflow;
		if (0 < candidateListToOverflow->mapToBucket(env)->_bufferCount) {
			listToOverflow = candidateListToOverflow;
			break;
		}

		/* make sure _nonEmptyOverflowedNext is not prematurely pre-fetched (probably only possible if compiler unrolls the loop) */
		MM_AtomicOperations::loadSync();

		/* remember that this is the last RSCL we examined (and found no buffers in it) and only then advance to the next in the list */
		candidateListToOverflow = candidateListToOverflow->_nonEmptyOverflowedNext;
	}


	/* if we found no previously overflowed region with unreleased buffers for the current bucket, we will try to find a new RSCL to overflow.
	 * during the search some other threads could have overflowed some other RSCLs (not very likely in practice) so we did not find them in the global list of overflowed RSCLs,
	 * but want still to consider them. bottom line, we do not limit ourselves on non-overflowed RSCLs in this search. not even those that are being rebuilt (if we are in GMP).
	 * what we do look for is the largest RSCL with at least one buffer in our bucket (this is just one of many possible heuristics).
	 */
	if (NULL == listToOverflow) {
		GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			MM_RememberedSetCardList *rscl = region->getRememberedSetCardList();

			if ((MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED == region->getRegionType()) && (0 < rscl->mapToBucket(env)->_bufferCount)) {
				if (NULL == listToOverflow || (listToOverflow->getBufferCount() < rscl->getBufferCount())) {
					listToOverflow = rscl;
				}
			}
		}
	}

	return listToOverflow;
}

void
MM_InterRegionRememberedSet::prepareRegionsForGlobalCollect(MM_EnvironmentVLHGC *env, bool gmpInProgress)
{
	if (!gmpInProgress) {
		Assert_MM_true(0 == _beingRebuiltRegionCount);
		/* since we aren't picking up a partially-completed mark state, we can safely clear all remembered object meta-data */
		GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			Assert_MM_false(region->getRememberedSetCardList()->isBeingRebuilt());
			if (region->getRememberedSetCardList()->isOverflowed()) {
				if (region->getRememberedSetCardList()->isStable()) {
					_stableRegionCount -= 1;
				} else {
					_overflowedRegionCount -= 1;
				}
			}
			region->getRememberedSetCardList()->clear(env);
		}
		Assert_MM_true(0 == _overflowedRegionCount);
		Assert_MM_true(0 == _stableRegionCount);
	}
}

void
MM_InterRegionRememberedSet::prepareOverflowedRegionsForRebuilding(MM_EnvironmentVLHGC *env)
{
	/* do not need to call this for global GC - it rebuilds all RSCLs */
	if (MM_CycleState::CT_GLOBAL_MARK_PHASE == env->_cycleState->_collectionType) {
		Assert_MM_true(0 == _beingRebuiltRegionCount);
		for (UDATA index = 0; index < _heapRegionManager->getTableRegionCount(); index++) {
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->physicalTableDescriptorForIndex(index);
			Assert_MM_false(region->getRememberedSetCardList()->isBeingRebuilt());
			if (region->getRememberedSetCardList()->isOverflowed()) {
				_beingRebuiltRegionCount += 1;
				if (region->getRememberedSetCardList()->isStable()) {
					_stableRegionCount -= 1;
				} else {
					_overflowedRegionCount -= 1;
				}
				region->getRememberedSetCardList()->clear(env);
				region->getRememberedSetCardList()->setAsBeingRebuilt();
			}
		}
		Assert_MM_true(0 == _overflowedRegionCount);
		Assert_MM_true(0 == _stableRegionCount);
	}
}

void
MM_InterRegionRememberedSet::setRegionsAsRebuildingComplete(MM_EnvironmentVLHGC *env)
{
	/* We set RSCL as beingRebuilt only at the beginning of GMP, but an AF may happen before GMP completes
	 * Thus we want to clear rebuilt flag if it is either the end of GMP or the end of global collect
	 */
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION != env->_cycleState->_collectionType);
	UDATA beingRebuiltCount = 0;
	UDATA stillOverflowedCount = 0;
	for (UDATA index = 0; index < _heapRegionManager->getTableRegionCount(); index++) {
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->physicalTableDescriptorForIndex(index);
		if (region->getRememberedSetCardList()->isBeingRebuilt()) {
			beingRebuiltCount += 1;
			if (region->getRememberedSetCardList()->isOverflowed()) {
				stillOverflowedCount += 1;
			}
			region->getRememberedSetCardList()->setAsRebuildingComplete();
			_beingRebuiltRegionCount -= 1;
		}
	}

	Trc_MM_InterRegionRememberedSet_setRegionsAsRebuildingComplete_rebuildingSummary(env->getLanguageVMThread(), beingRebuiltCount, stillOverflowedCount);
	Assert_MM_true(0 == _beingRebuiltRegionCount);
}

void
MM_InterRegionRememberedSet::rememberReferenceForMarkInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
{
	MM_HeapRegionDescriptorVLHGC *toRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(toObject);
	bool isGlobalMarkPhase = (MM_CycleState::CT_GLOBAL_MARK_PHASE == env->_cycleState->_collectionType);

	/* PGC and Global Collect always rebuild RSCL. GMP only for some of them (overflowed for example) */
	if (!isGlobalMarkPhase || toRegion->getRememberedSetCardList()->isBeingRebuilt()) {
		rememberReferenceInternal(env, fromObject, toRegion);
	}
}

void
MM_InterRegionRememberedSet::rememberReferenceForCompactInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
{
	MM_HeapRegionDescriptorVLHGC *toRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(toObject);
	rememberReferenceInternal(env, fromObject, toRegion);
}

void
MM_InterRegionRememberedSet::rememberReferenceForCopyForwardInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
{
	MM_HeapRegionDescriptorVLHGC *toRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(toObject);
	rememberReferenceInternal(env, fromObject, toRegion);
}

bool
MM_InterRegionRememberedSet::isReferenceRememberedForMark(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
{
	/* if CardList not overflowed, check there first */
	if (NULL != toObject) {
		MM_HeapRegionDescriptorVLHGC *toRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(toObject);
		MM_HeapRegionDescriptorVLHGC *fromRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(fromObject);

		/* References within a region are not explicitly remembered - they are implied */
		if (toRegion == fromRegion) {
			return true;
		}
		
		MM_RememberedSetCardList *toRememberedSetCardList = toRegion->getRememberedSetCardList();

		if (!toRememberedSetCardList->isOverflowed()) {
			/* If the list is not overflowed is and the reference should have been remembered, it must be in the list.
			 * The fact that RSM might have remembered associated regions (because of another reference) is irrelevant
			 */
			return toRememberedSetCardList->isRemembered(env, fromObject);
		}
	}
	return true;
}

void
MM_InterRegionRememberedSet::clearReferencesToRegion(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *toRegion)
{
	if (!toRegion->getRememberedSetCardList()->isEmpty(env)) {
		Assert_MM_false(toRegion->getRememberedSetCardList()->isBeingRebuilt());
		if (toRegion->getRememberedSetCardList()->isOverflowed()) {
			if (toRegion->getRememberedSetCardList()->isStable()) {
				Assert_MM_true(0 < _stableRegionCount);
				_stableRegionCount -= 1;
			} else {
				Assert_MM_true(0 < _overflowedRegionCount);
				_overflowedRegionCount -= 1;
			}
		}
		toRegion->getRememberedSetCardList()->clear(env);
	}
}

/*
 * This is temporary helper and will be removed as soon as
 * optimized/non-optimized version is chosen
 */
void
MM_InterRegionRememberedSet::clearFromRegionReferencesForCompact(MM_EnvironmentVLHGC* env)
{
	if(MM_GCExtensions::getExtensions(env)->tarokEnableCompressedCardTable) {
		clearFromRegionReferencesForCompactOptimized(env);
	} else {
		clearFromRegionReferencesForCompactDirect(env);
	}

	releaseCardBufferControlBlockListForThread(env, env);
}

void
MM_InterRegionRememberedSet::clearFromRegionReferencesForCompactDirect(MM_EnvironmentVLHGC* env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
	U_64 startTime = j9time_hires_clock();

	GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region;

	UDATA cardsProcessed = 0;
	UDATA cardsRemoved = 0;

	while (NULL != (region = regionIterator.nextRegion())) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (!region->getRememberedSetCardList()->isOverflowed()) {
				MM_RememberedSetCard card = 0;
				UDATA toRemoveCount = 0;
				UDATA totalCountBefore = 0;
				GC_RememberedSetCardListCardIterator rsclCardIterator(region->getRememberedSetCardList());
				while(0 != (card = rsclCardIterator.nextReferencingCard(env))) {
					MM_HeapRegionDescriptorVLHGC *fromRegion = tableDescriptorForRememberedSetCard(card);
					/* Regions that are completely swept after a GMP, might still have outgoing references (thus we consider empty regions too) */
					Card * cardAddress = rememberedSetCardToCardAddr(env, card);
					if (fromRegion->_compactData._shouldCompact || !fromRegion->containsObjects() || isDirtyCardForPartialCollect(env, cardTable, cardAddress)) {
						toRemoveCount += 1;
						rsclCardIterator.removeCurrentCard();
					}
					totalCountBefore +=1;
				}


				if (0 != toRemoveCount) {
					region->getRememberedSetCardList()->compact(env);
					UDATA totalCountAfter = region->getRememberedSetCardList()->getSize(env);
					
					Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForCompact_cardCounts(env->getLanguageVMThread(),  MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount, _heapRegionManager->mapDescriptorToRegionTableIndex(region), totalCountBefore, toRemoveCount, totalCountAfter);
					Assert_MM_true(totalCountBefore == toRemoveCount + totalCountAfter);
				}

				cardsProcessed += totalCountBefore;
				cardsRemoved += toRemoveCount;

			} else {
				region->getRememberedSetCardList()->releaseBuffers(env);
			}
		}
	}

	env->_irrsStats._clearFromRegionReferencesTimesus = j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
	env->_irrsStats._clearFromRegionReferencesCardsProcessed = cardsProcessed;
	env->_irrsStats._clearFromRegionReferencesCardsCleared = cardsRemoved;

	Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForCompact_timesus(env->getLanguageVMThread(), env->_irrsStats._clearFromRegionReferencesTimesus, 0);
}

void
MM_InterRegionRememberedSet::clearFromRegionReferencesForCompactOptimized(MM_EnvironmentVLHGC* env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
	MM_CompressedCardTable *compressedCardTable = MM_GCExtensions::getExtensions(env)->compressedCardTable;
	U_64 startTime = j9time_hires_clock();

	rebuildCompressedCardTableForCompact(env);

	U_64 timeAfterRebuild = j9time_hires_clock();

	GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region;

	UDATA cardsProcessed = 0;
	UDATA cardsRemoved = 0;
	bool tableIsReady = false;

	while (NULL != (region = regionIterator.nextRegion())) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (!region->getRememberedSetCardList()->isOverflowed()) {
				MM_RememberedSetCard card = 0;
				UDATA toRemoveCount = 0;
				UDATA totalCountBefore = 0;
				GC_RememberedSetCardListCardIterator rsclCardIterator(region->getRememberedSetCardList());
				while(0 != (card = rsclCardIterator.nextReferencingCard(env))) {
					bool remove = true;
					if (tableIsReady) {
						/* Rebuild of Compressed Card Table has been completed - use it */
						remove = compressedCardTable->isCompressedCardDirtyForPartialCollect(env, convertHeapAddressFromRememberedSetCard(card));
					} else {
						if (compressedCardTable->isReady()) {
							tableIsReady = true;
							/* Rebuild of Compressed Card Table has been completed - use it for first time */
							remove = compressedCardTable->isCompressedCardDirtyForPartialCollect(env, convertHeapAddressFromRememberedSetCard(card));
						} else {
							/* rebuild is not complete - look at the region PGC selection and card itself directly */
							MM_HeapRegionDescriptorVLHGC *fromRegion = tableDescriptorForRememberedSetCard(card);
							if (fromRegion->containsObjects() && !fromRegion->_compactData._shouldCompact) {
								Card * cardAddress = rememberedSetCardToCardAddr(env, card);
								remove = isDirtyCardForPartialCollect(env, cardTable, cardAddress);
							}
						}
					}

					if (remove) {
						toRemoveCount += 1;
						rsclCardIterator.removeCurrentCard();
					}
					totalCountBefore +=1;
				}

				if (0 != toRemoveCount) {
					region->getRememberedSetCardList()->compact(env);
					UDATA totalCountAfter = region->getRememberedSetCardList()->getSize(env);

					Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForCompact_cardCounts(env->getLanguageVMThread(),  MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount, _heapRegionManager->mapDescriptorToRegionTableIndex(region), totalCountBefore, toRemoveCount, totalCountAfter);
					Assert_MM_true(totalCountBefore == toRemoveCount + totalCountAfter);
				}

				cardsProcessed += totalCountBefore;
				cardsRemoved += toRemoveCount;

			} else {
				region->getRememberedSetCardList()->releaseBuffers(env);
			}
		}
	}

	env->_irrsStats._clearFromRegionReferencesTimesus = j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
	env->_irrsStats._rebuildCompressedCardTableTimesus = j9time_hires_delta(startTime, timeAfterRebuild, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	env->_irrsStats._clearFromRegionReferencesCardsProcessed = cardsProcessed;
	env->_irrsStats._clearFromRegionReferencesCardsCleared = cardsRemoved;

	Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForCompact_timesus(env->getLanguageVMThread(), env->_irrsStats._clearFromRegionReferencesTimesus, env->_irrsStats._rebuildCompressedCardTableTimesus);
}

/*
 * This is temporary helper and will be removed as soon as
 * optimized/non-optimized version is chosen
 */
void
MM_InterRegionRememberedSet::clearFromRegionReferencesForMark(MM_EnvironmentVLHGC* env)
{
	if(MM_GCExtensions::getExtensions(env)->tarokEnableCompressedCardTable) {
		clearFromRegionReferencesForMarkOptimized(env);
	} else {
		clearFromRegionReferencesForMarkDirect(env);
	}

	releaseCardBufferControlBlockListForThread(env, env);
}

void
MM_InterRegionRememberedSet::clearFromRegionReferencesForMarkDirect(MM_EnvironmentVLHGC* env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
	U_64 startTime = j9time_hires_clock();

	GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region;

	UDATA cardsProcessed = 0;
	UDATA cardsRemoved = 0;

	while (NULL != (region = regionIterator.nextRegion())) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (!region->getRememberedSetCardList()->isOverflowed()) {
				MM_RememberedSetCard card = 0;
				UDATA toRemoveCount = 0;
				UDATA totalCountBefore = 0;
				GC_RememberedSetCardListCardIterator rsclCardIterator(region->getRememberedSetCardList());
				while(0 != (card = rsclCardIterator.nextReferencingCard(env))) {
					MM_HeapRegionDescriptorVLHGC *fromRegion = tableDescriptorForRememberedSetCard(card);
					Card * cardAddress = rememberedSetCardToCardAddr(env, card);
					/* Regions that are completely swept after a GMP, might still have outgoing references (thus we consider empty regions too) */
					if (fromRegion->_markData._shouldMark  || !fromRegion->containsObjects() || isDirtyCardForPartialCollect(env, cardTable, cardAddress)) {
						toRemoveCount += 1;
						rsclCardIterator.removeCurrentCard();
					}
					totalCountBefore +=1;
				}

				if (0 != toRemoveCount) {
					region->getRememberedSetCardList()->compact(env);
					UDATA totalCountAfter = region->getRememberedSetCardList()->getSize(env);

					Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForMark_cardCounts(env->getLanguageVMThread(), MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount, _heapRegionManager->mapDescriptorToRegionTableIndex(region), totalCountBefore, toRemoveCount, totalCountAfter);
					Assert_MM_true(totalCountBefore == toRemoveCount + totalCountAfter);
				}

				cardsProcessed += totalCountBefore;
				cardsRemoved += toRemoveCount;

			} else {
				region->getRememberedSetCardList()->releaseBuffers(env);
			}
		}
	}

	env->_irrsStats._clearFromRegionReferencesTimesus = j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
	env->_irrsStats._clearFromRegionReferencesCardsProcessed = cardsProcessed;
	env->_irrsStats._clearFromRegionReferencesCardsCleared = cardsRemoved;

	Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForMark_timesus(env->getLanguageVMThread(), env->_irrsStats._clearFromRegionReferencesTimesus, 0);
}

void
MM_InterRegionRememberedSet::clearFromRegionReferencesForMarkOptimized(MM_EnvironmentVLHGC* env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
	MM_CompressedCardTable *compressedCardTable = MM_GCExtensions::getExtensions(env)->compressedCardTable;
	U_64 startTime = j9time_hires_clock();

	rebuildCompressedCardTableForMark(env);

	U_64 timeAfterRebuild = j9time_hires_clock();

	GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region;

	UDATA cardsProcessed = 0;
	UDATA cardsRemoved = 0;
	bool tableIsReady = false;

	while (NULL != (region = regionIterator.nextRegion())) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (!region->getRememberedSetCardList()->isOverflowed()) {
				MM_RememberedSetCard card = 0;
				UDATA toRemoveCount = 0;
				UDATA totalCountBefore = 0;
				GC_RememberedSetCardListCardIterator rsclCardIterator(region->getRememberedSetCardList());
				while(0 != (card = rsclCardIterator.nextReferencingCard(env))) {
					/* Regions that are completely swept after a GMP, might still have outgoing references (thus we consider empty regions too) */
					bool remove = true;
					if (tableIsReady) {
						/* Rebuild of Compressed Card Table has been completed - use it */
						remove = compressedCardTable->isCompressedCardDirtyForPartialCollect(env, convertHeapAddressFromRememberedSetCard(card));
					} else {
						if (compressedCardTable->isReady()) {
							tableIsReady = true;
							/* Rebuild of Compressed Card Table has been completed - use it for first time */
							remove = compressedCardTable->isCompressedCardDirtyForPartialCollect(env, convertHeapAddressFromRememberedSetCard(card));
						} else {
							/* rebuild is not complete - look at the region PGC selection and card itself directly */
							MM_HeapRegionDescriptorVLHGC *fromRegion = tableDescriptorForRememberedSetCard(card);
							if (fromRegion->containsObjects() && !fromRegion->_markData._shouldMark) {
								Card * cardAddress = rememberedSetCardToCardAddr(env, card);
								remove = isDirtyCardForPartialCollect(env, cardTable, cardAddress);
							}
						}
					}

					if (remove) {
						toRemoveCount += 1;
						rsclCardIterator.removeCurrentCard();
					}
					totalCountBefore +=1;
				}

				if (0 != toRemoveCount) {
					region->getRememberedSetCardList()->compact(env);
					UDATA totalCountAfter = region->getRememberedSetCardList()->getSize(env);
					
					Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForMark_cardCounts(env->getLanguageVMThread(), MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount, _heapRegionManager->mapDescriptorToRegionTableIndex(region), totalCountBefore, toRemoveCount, totalCountAfter);
					Assert_MM_true(totalCountBefore == toRemoveCount + totalCountAfter);
				}

				cardsProcessed += totalCountBefore;
				cardsRemoved += toRemoveCount;

			} else {
				region->getRememberedSetCardList()->releaseBuffers(env);
			}
		}
	}

	env->_irrsStats._clearFromRegionReferencesTimesus = j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
	env->_irrsStats._rebuildCompressedCardTableTimesus = j9time_hires_delta(startTime, timeAfterRebuild, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	env->_irrsStats._clearFromRegionReferencesCardsProcessed = cardsProcessed;
	env->_irrsStats._clearFromRegionReferencesCardsCleared = cardsRemoved;

	Trc_MM_InterRegionRememberedSet_clearFromRegionReferencesForMark_timesus(env->getLanguageVMThread(), env->_irrsStats._clearFromRegionReferencesTimesus, env->_irrsStats._rebuildCompressedCardTableTimesus);
}

void
MM_InterRegionRememberedSet::rebuildCompressedCardTableForMark(MM_EnvironmentVLHGC* env)
{
	MM_CompressedCardTable *compressedCardTable = MM_GCExtensions::getExtensions(env)->compressedCardTable;
	UDATA totalNumber = 0;
	UDATA doneByThisThread = 0;

	GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region;

	while (NULL != (region = regionIterator.nextRegion())) {
		totalNumber += 1;
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (!region->containsObjects() || region->_markData._shouldMark) {
				/* region is empty or chosen for PCG, so set all correspondent compressed cards dirty */
				compressedCardTable->setCompressedCardsDirtyForPartialCollect(region->getLowAddress(), region->getHighAddress());
			} else {
				/* rebuild compressed card table for region */
				compressedCardTable->rebuildCompressedCardTableForPartialCollect(env, region->getLowAddress(), region->getHighAddress());
			}
			doneByThisThread += 1;
		}
	}

	compressedCardTable->incrementProcessedRegionsCounter(totalNumber, doneByThisThread);
}

void
MM_InterRegionRememberedSet::rebuildCompressedCardTableForCompact(MM_EnvironmentVLHGC* env)
{
	MM_CompressedCardTable *compressedCardTable = MM_GCExtensions::getExtensions(env)->compressedCardTable;
	UDATA totalNumber = 0;
	UDATA doneByThisThread = 0;

	GC_HeapRegionIteratorVLHGC regionIterator(_heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region;

	while (NULL != (region = regionIterator.nextRegion())) {
		totalNumber += 1;
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (!region->containsObjects() || region->_compactData._shouldCompact) {
				/* region is empty or chosen for PCG, so set all correspondent compressed cards dirty */
				compressedCardTable->setCompressedCardsDirtyForPartialCollect(region->getLowAddress(), region->getHighAddress());
			} else {
				/* rebuild compressed card table for region */
				compressedCardTable->rebuildCompressedCardTableForPartialCollect(env, region->getLowAddress(), region->getHighAddress());
			}
			doneByThisThread += 1;
		}
	}

	compressedCardTable->incrementProcessedRegionsCounter(totalNumber, doneByThisThread);
}

void
MM_InterRegionRememberedSet::clearFromRegionReferencesForCopyForward(MM_EnvironmentVLHGC *env)
{
	/* The same collection set tagging mechanism is used for marking and copy forwarding - so just forward on to the equivalent routine for mark */
	clearFromRegionReferencesForMark(env);
}

bool
MM_InterRegionRememberedSet::isDirtyCardForPartialCollect(MM_EnvironmentVLHGC *env, MM_CardTable *cardTable, Card *card)
{
	bool result = false;

	Card fromState = *card;
	switch(fromState) {
	case CARD_CLEAN:
	case CARD_GMP_MUST_SCAN:
		break;
	case CARD_REMEMBERED_AND_GMP_SCAN:
	case CARD_REMEMBERED:
	case CARD_DIRTY:
	case CARD_PGC_MUST_SCAN:
		result = true;
		break;
	default:
		Assert_MM_unreachable();
		break;
	}

	return result;
}

void
MM_InterRegionRememberedSet::setupForPartialCollect(MM_EnvironmentVLHGC *env)
{
	if(MM_GCExtensions::getExtensions(env)->tarokEnableCompressedCardTable) {
		MM_CompressedCardTable *compressedCardTable = MM_GCExtensions::getExtensions(env)->compressedCardTable;
		compressedCardTable->clearRegionsProcessedCounter();
	}

	/* List of overflowed RSCLs is built from scratch for every Mark/Copy-Forward/Compact. It will contain only newly overflowed RSCLs.
	 * Previously overflowed RSCLs will have buffers released already and are not interesting for this list.
	 */
	Assert_MM_true(NULL == _overflowedListHead);
	Assert_MM_true(NULL == _overflowedListTail);
}


