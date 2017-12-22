
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

/**
 * @file
 */

#include "StringTableIncrementalIterator.hpp"
#include "ModronAssertions.h"

void **
GC_StringTableIncrementalIterator::nextSlot()
{
	if (NULL == _currentPuddle) {
		return NULL;
	}

	_lastNode = _nextNode;
	if (NULL != _nextNode) {
		_nextNode = (void **)pool_nextDo(&_poolState);
	}

	if (NULL == _lastNode) {
		_lastSlot = NULL;
	} else {
		switch (_iterateState) {
		case ITERATE_NODE_POOL:
			/* Data section appears at the start of list nodes */
			_lastSlot = (void **)_lastNode;
			break;
		case ITERATE_TREE_POOL:
			/* Data section appears after the tree node fields */
			_lastSlot = (void **)AVL_NODE_TO_DATA(_lastNode);
			break;
		default:
			Assert_MM_unreachable();
			break;
		}
	}

	return _lastSlot;
}

void
GC_StringTableIncrementalIterator::removeSlot()
{
	hashTableRemove(_hashTable, _lastSlot);
}

/*
 * Update _nextNode, _currentPuddle, _nextPuddle fields based on the _currentIteratePool
 * and _nextPuddle.
 */
void
GC_StringTableIncrementalIterator::getNext()
{
	_currentPuddle = _nextPuddle;
	if (NULL != _currentPuddle) {
		_nextNode = poolPuddle_startDo(_currentIteratePool, _currentPuddle, &_poolState, FALSE);
		_nextPuddle = J9POOLPUDDLE_NEXTPUDDLE(_currentPuddle);
	}
}

/**
 * Move to the next increment in the string table. Must also be
 * called before any call to nextSlot, in order to set up iteration
 * for the first increment.
 * In the current implementation, this means to move on to the
 * next puddle in the pool.
 * @return true if there is another increment remaining
 * @return false if there are no more increments
 */
bool
GC_StringTableIncrementalIterator::nextIncrement()
{
	getNext();
	if (NULL == _currentPuddle) {
		if (ITERATE_NODE_POOL == _iterateState) {
			/* start iterating tree nodes pool */
			_iterateState = ITERATE_TREE_POOL;
			_currentIteratePool = _stringTableTreeNodePool;
			_nextPuddle = J9POOLPUDDLELIST_NEXTPUDDLE(J9POOL_PUDDLELIST(_stringTableTreeNodePool));
			getNext();
		}
	}

	if (NULL != _currentPuddle) {
		return true;
	} else {
		return false;
	}
}
