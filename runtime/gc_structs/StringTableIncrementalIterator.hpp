
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
 * @ingroup GC_Structs
 */

#if !defined(STRINGTABLEINCREMENTALITERATOR_HPP_)
#define STRINGTABLEINCREMENTALITERATOR_HPP_

#include "j9.h"
#include "j9protos.h"
#include "modron.h"

#include "HashTableIterator.hpp"

/**
 * Iterate over the string constant table incrementally.
 * Call nextIncrement() to move to the first increment,
 * then iterate over the slots in that increment, calling
 * nextSlot() until it returns NULL. Call nextIncrement()
 * again, and continue this process until nextIncrement()
 * returns false, indicating that there are no more increments
 * left to process.
 * @note The fact that this is a subclass of GC_HashTableIterator
 * is a bit of a hack. The iterator must be passed into the
 * doStringTableSlot function of MM_RootScanner, so instead of
 * changing the type of that argument in every subclass, we
 * do this instead.
 */
class GC_StringTableIncrementalIterator : public GC_HashTableIterator
{
private:
	typedef enum {
		ITERATE_NODE_POOL = 0,
		ITERATE_TREE_POOL
	} IterateState ;

	J9HashTable *_hashTable; /**< The string table (currently implemented as a hash table) */
	J9Pool* _currentIteratePool; /**< Pool that is currently being iterated over iterating */
	J9Pool* _stringTableTreeNodePool; /**< The string table tree node pool */
	J9PoolPuddle *_currentPuddle; /**< Puddle that is currently being iterated over */
	J9PoolPuddle *_nextPuddle; /**< Next puddle to iterate over -- prefetched in case the current puddle is deleted */
	pool_state _poolState; /**< State of the iteration over the pool */
	void *_nextNode; /**< Node to be used for the next call to nextSlot() */
	void *_lastNode; /**< Node to be used for the last call to nextSlot() -- required right now to implement deletion */
	void **_lastSlot; /**< Slot used for the last call to nextSlot() -- required for hashtable removes */
	IterateState _iterateState;


private:
	void getNext();
public:
	void **nextSlot();
	virtual void removeSlot();

	bool nextIncrement();

	GC_StringTableIncrementalIterator(J9HashTable *hashTable) :
		GC_HashTableIterator(hashTable),
		_hashTable(hashTable),
		_currentIteratePool(hashTable->listNodePool),
		_stringTableTreeNodePool(hashTable->treeNodePool),
		_currentPuddle(NULL),
		_nextPuddle(J9POOLPUDDLELIST_NEXTPUDDLE(J9POOL_PUDDLELIST(hashTable->listNodePool))),
		_poolState(),
		_nextNode(NULL),
		_lastNode(NULL),
		_lastSlot(NULL),
		_iterateState(ITERATE_NODE_POOL)
	{}
};

#endif /* STRINGTABLEINCREMENTALITERATOR_HPP_ */

