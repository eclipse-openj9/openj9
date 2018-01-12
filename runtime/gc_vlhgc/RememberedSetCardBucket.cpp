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

#include "AtomicOperations.hpp"
#include "CycleState.hpp"
#include "HeapRegionManager.hpp"
#include "InterRegionRememberedSet.hpp"
#include "RememberedSetCardBucket.hpp"
#include "RememberedSetCardList.hpp"

bool
MM_RememberedSetCardBucket::initialize(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *rscl, MM_RememberedSetCardBucket *next)
{
	_rscl = rscl;
	_next = next;
	_cardBufferControlBlockHead = NULL;
	_current = NULL;
	_bufferCount = 0;

	return true;
}

void
MM_RememberedSetCardBucket::tearDown(MM_GCExtensions *extensions)
{
	_cardBufferControlBlockHead = NULL;
	_current = NULL;
}

void
MM_RememberedSetCardBucket::setListAsOverflow(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *listToOverflow)
{
	UDATA oldValue = MM_AtomicOperations::lockCompareExchange(&listToOverflow->_overflowed, FALSE, TRUE);

	if (FALSE == oldValue) {
		/* the thread that sets the overflow flag first puts it on the global list of overflowed regions
		  so that buffers can be released by other threads */
		MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->enqueueOverflowedRscl(env, listToOverflow);
	}

	listToOverflow->releaseBuffersForCurrentThread(env);
}

void
MM_RememberedSetCardBucket::addToNewBuffer(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card)
{
	Assert_MM_true(_rscl->_bufferCount >= _bufferCount);

	if (!_rscl->_overflowed) {
		/* the current buffer is full or _current is NULL (no buffers in the bucket yet)
		 * allocate a new buffer from the buffer pool
		 * bound the total size of owning list
		 */
		MM_AtomicOperations::add(&_rscl->_bufferCount, 1);
		_bufferCount += 1;
		if ((_rscl->_bufferCount * MAX_BUFFER_SIZE) >  MM_GCExtensions::getExtensions(env)->tarokRememberedSetCardListMaxSize) {
			MM_AtomicOperations::subtract(&_rscl->_bufferCount, 1);
			_bufferCount -= 1;

			setListAsOverflow(env, _rscl);
		} else {
			MM_InterRegionRememberedSet *interRegionRememberedSet = MM_GCExtensions::getExtensions(env)->interRegionRememberedSet;

			MM_CardBufferControlBlock *newBuffer = interRegionRememberedSet->allocateCardBufferControlBlockFromLocalPool(env);
			if (NULL == newBuffer) {
				MM_AtomicOperations::subtract(&_rscl->_bufferCount, 1);
				_bufferCount -= 1;

				MM_RememberedSetCardList *rsclToOverflow = interRegionRememberedSet->findRsclToOverflow((MM_EnvironmentVLHGC *)env);
				if (NULL == rsclToOverflow) {
					/* Failed to find an appropriate region to overflow. Abort this transaction by overflowing the current list */
					setListAsOverflow(env, _rscl);
				} else {
					setListAsOverflow(env, rsclToOverflow);

					/* If we overflowed the list other then the current one, we can re-try allocating a buffer */
					newBuffer = interRegionRememberedSet->allocateCardBufferControlBlockFromLocalPool(env);

					if (NULL == newBuffer) {
						/* No luck, failed even after overflowing another list. Abort this transaction by overflowing the current list */
						setListAsOverflow(env, _rscl);
					} else {
						/* successfully allocated a buffer */
						MM_AtomicOperations::add(&_rscl->_bufferCount, 1);
						_bufferCount += 1;
					}
				}
			}

			if (NULL != newBuffer) {
				/* reserve space for current add */
				_current = newBuffer->_card + 1;
				newBuffer->_card[0] = card;

				/* link in the buffer in the bucket local list */
				newBuffer->_next = _cardBufferControlBlockHead;
				_cardBufferControlBlockHead = newBuffer;
			}
		}
	} else if (0 != _bufferCount) {
		/* The list is overflowed, but this bucket may have not released the buffers yet */
		globalReleaseBuffers(env);
	}
	
	Assert_MM_true(_rscl->_bufferCount >= _bufferCount);
}

bool
MM_RememberedSetCardBucket::isRemembered(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card)
{
	UDATA bufferCount = 0;
	MM_CardBufferControlBlock *currentCardBufferControlBlock = _cardBufferControlBlockHead;
	while (NULL != currentCardBufferControlBlock) {
		bufferCount += 1;

		MM_RememberedSetCard *bufferCardList = currentCardBufferControlBlock->_card;

		/* find top index for this buffer */
		UDATA cardIndexTop = MAX_BUFFER_SIZE;
		if (isCurrentSlotWithinBuffer(bufferCardList)) {
			cardIndexTop = _current -  bufferCardList;
		}
		
		for (UDATA cardIndex = 0; cardIndex < cardIndexTop; cardIndex++) {
			if (bufferCardList[cardIndex] == card) {
				return true;
			}
		}
		currentCardBufferControlBlock = currentCardBufferControlBlock->_next;
	}

	return false;
}

void
MM_RememberedSetCardBucket::releaseBuffers(MM_EnvironmentVLHGC *env, UDATA buffersToLocalPoolCount)
{
	Assert_MM_true(_rscl->_bufferCount >= _bufferCount);

	UDATA releasedCount = MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->releaseCardBufferControlBlockListToLocalPool(env, _cardBufferControlBlockHead, buffersToLocalPoolCount);
	Assert_MM_true(_bufferCount == releasedCount);

	_cardBufferControlBlockHead = NULL;
	MM_AtomicOperations::subtract(&_rscl->_bufferCount, releasedCount);
	_bufferCount = 0;
	_current = NULL;
}

void
MM_RememberedSetCardBucket::globalReleaseBuffers(MM_EnvironmentVLHGC *env)
{
	releaseBuffers(env, MAX_LOCAL_RSCL_BUFFER_POOL_SIZE);
}

void
MM_RememberedSetCardBucket::localReleaseBuffers(MM_EnvironmentVLHGC *env)
{
	releaseBuffers(env, UDATA_MAX);

}

UDATA
MM_RememberedSetCardBucket::getSize(MM_EnvironmentVLHGC *env)
{
	UDATA size = _bufferCount * MAX_BUFFER_SIZE;
	if (_bufferCount > 0) {
		Assert_MM_true(NULL != _current);
		UDATA offset = (UDATA)_current & OFFSET_MASK;
		if (0 != offset) {
			/* subtract the unused portion of the current buffer */
			size -= MAX_BUFFER_SIZE - (offset / sizeof (MM_RememberedSetCard));
		}
	}

	return size;
}

void
MM_RememberedSetCardBucket::compact(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(_rscl->_bufferCount >= _bufferCount);
	
	if (NULL != _cardBufferControlBlockHead) {
		UDATA toIndex = 0;
		MM_CardBufferControlBlock *toCardBufferControlBlock = _cardBufferControlBlockHead;
		MM_CardBufferControlBlock *prevToCardBufferControlBlock = NULL;
		MM_RememberedSetCard *toBufferCardList = toCardBufferControlBlock->_card;

		MM_CardBufferControlBlock *fromCardBufferControlBlock = _cardBufferControlBlockHead;

		/* shift left the cards across the buffer list */
		do {
			MM_RememberedSetCard *fromBufferCardList = fromCardBufferControlBlock->_card;

			/* find top index for this buffer */
			UDATA fromIndexTop = MAX_BUFFER_SIZE;
			if (isCurrentSlotWithinBuffer(fromBufferCardList)) {
				fromIndexTop = _current -  fromBufferCardList;
			}

			for (UDATA fromIndex = 0; fromIndex < fromIndexTop; fromIndex++) {
				if (0 != fromBufferCardList[fromIndex]) {
					toBufferCardList[toIndex++] = fromBufferCardList[fromIndex];
					if (MAX_BUFFER_SIZE == toIndex) {
						prevToCardBufferControlBlock = toCardBufferControlBlock;
						toCardBufferControlBlock = toCardBufferControlBlock->_next;
						if (NULL != toCardBufferControlBlock) {
							toBufferCardList = toCardBufferControlBlock->_card;
						}
						toIndex = 0;
					}
				}
			}

			/* next buffer in the bucket */
			fromCardBufferControlBlock = fromCardBufferControlBlock->_next;
		} while (NULL != fromCardBufferControlBlock);

		/* update various pointers */
		MM_CardBufferControlBlock *toDeleteCardBufferControlBlock = NULL;
		if (0 == toIndex) {
			toDeleteCardBufferControlBlock = toCardBufferControlBlock;
			/* _current should point to the end of previous buffer */
			if (NULL == prevToCardBufferControlBlock) {
				_current = NULL;
				_cardBufferControlBlockHead = NULL;
			} else {
				_current = prevToCardBufferControlBlock->_card + MAX_BUFFER_SIZE;
				prevToCardBufferControlBlock->_next = NULL;
			}
		} else {
			toDeleteCardBufferControlBlock = toCardBufferControlBlock->_next;
			_current = &toBufferCardList[toIndex];
			toCardBufferControlBlock->_next = NULL;
		}

		UDATA releasedCount = MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->releaseCardBufferControlBlockListToLocalPool(env, toDeleteCardBufferControlBlock, UDATA_MAX);
		Assert_MM_true(releasedCount <= _bufferCount);
		_bufferCount -= releasedCount;
		_rscl->_bufferCount -= releasedCount;
	}
	
	Assert_MM_true(_rscl->_bufferCount >= _bufferCount);
}

