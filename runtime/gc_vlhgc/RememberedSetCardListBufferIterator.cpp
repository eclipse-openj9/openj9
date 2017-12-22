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

#include "InterRegionRememberedSet.hpp"
#include "RememberedSetCardListBufferIterator.hpp"

void
GC_RememberedSetCardListBufferIterator::unlinkCurrentBuffer(MM_EnvironmentBase* env)
{
	/* remove from the list
	 * (it's a single linked list, so we keep track of the previous buffer while iterating)
	 */
	if (NULL == _cardBufferControlBlockPrevious) {
		_currentBucket->_cardBufferControlBlockHead = _cardBufferControlBlockCurrent->_next;
	} else {
		_cardBufferControlBlockPrevious->_next = _cardBufferControlBlockCurrent->_next;
	}

	if (_currentBucket->isCurrentSlotWithinBuffer(_bufferCardList)) {
		/* make the _current bucket looks like point to the end of a full buffer */
		_currentBucket->_current = _bufferCardList + MM_RememberedSetCardBucket::MAX_BUFFER_SIZE;
	}

	_currentBucket->_bufferCount -= 1;
	_rscl->_bufferCount -= 1;
	if (0 == _currentBucket->_bufferCount) {
		/* no more buffers, in the bucket */
		_currentBucket->_current = NULL;
		Assert_MM_true(NULL == _currentBucket->_cardBufferControlBlockHead);
	}

}

bool
GC_RememberedSetCardListBufferIterator::nextBucket(MM_EnvironmentBase* env)
{
	/* next bucket in the list */
	do {
		if (NULL == _currentBucket) {
			_currentBucket = _rscl->_bucketListHead;
		} else {
			_currentBucket = _currentBucket->_next;
			_cardBufferControlBlockPrevious = NULL;
		}
		if (NULL == _currentBucket) {
			/* this was the last bucket */
			return false;
		}

		_cardBufferControlBlockNext = _currentBucket->_cardBufferControlBlockHead;
		/* if no buffer in this bucket, retry */
	} while (NULL == _cardBufferControlBlockNext);

	return true;
}

MM_CardBufferControlBlock *
GC_RememberedSetCardListBufferIterator::nextBuffer(MM_EnvironmentBase* env, MM_RememberedSetCard **lastCard)
{
	do {
		if (NULL != _cardBufferControlBlockNext) {
			/* TODO: could this condition be simplified? */
			if (((NULL != _cardBufferControlBlockPrevious) && (_cardBufferControlBlockPrevious->_next == _cardBufferControlBlockCurrent))
					|| ((NULL == _cardBufferControlBlockPrevious) && (_currentBucket->_cardBufferControlBlockHead == _cardBufferControlBlockCurrent))) {
				_cardBufferControlBlockPrevious = _cardBufferControlBlockCurrent;
			}
			_cardBufferControlBlockCurrent = _cardBufferControlBlockNext;
			_cardBufferControlBlockNext = _cardBufferControlBlockCurrent->_next;

			_bufferCardList = _cardBufferControlBlockCurrent->_card;
			if (_currentBucket->isCurrentSlotWithinBuffer(_bufferCardList)) {
				*lastCard = _currentBucket->_current;
			} else {
				*lastCard = _cardBufferControlBlockCurrent->_card + MM_RememberedSetCardBucket::MAX_BUFFER_SIZE;
			}
			return _cardBufferControlBlockCurrent;
		}
	} while (nextBucket(env));

	return NULL;
}
