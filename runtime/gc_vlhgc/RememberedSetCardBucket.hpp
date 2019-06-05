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

#if !defined(REMEMBEREDSETCARDBUCKET_HPP)
#define REMEMBEREDSETCARDBUCKET_HPP

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

#include "BaseNonVirtual.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"

#if defined (OMR_GC_COMPRESSED_POINTERS)
/* Compresses a heap address by shifting right by CARD_SIZE_SHIFT */
typedef U_32 MM_RememberedSetCard; 
#else
/* Just a heap address */
typedef UDATA MM_RememberedSetCard;
#endif /* OMR_GC_COMPRESSED_POINTERS */

class MM_RememberedSetCardList;

struct MM_CardBufferControlBlock {
	MM_RememberedSetCard *_card;	/**< the actual buffer */
	MM_CardBufferControlBlock *_next;		/**< next Buffer Control Block in a list (either bucket local, or global pool */
};

class MM_RememberedSetCardBucket : public MM_BaseNonVirtual {
	/* data members */
private:
	enum {
		MAX_BUFFER_SIZE_LOG = 5,
		MAX_BUFFER_SIZE = (1 << MAX_BUFFER_SIZE_LOG),
		OFFSET_MASK = MAX_BUFFER_SIZE * sizeof(MM_RememberedSetCard) - 1
	};

	friend class MM_RememberedSetCardList;
	friend class GC_RememberedSetCardListCardIterator;
	friend class GC_RememberedSetCardListBufferIterator;
	friend class MM_InterRegionRememberedSet;

	MM_CardBufferControlBlock *_cardBufferControlBlockHead; 		/**< head pointer to the list of buffers */
	MM_RememberedSetCard * _current;						/**< pointer to the current Card slot */
	MM_RememberedSetCardList *_rscl;						/**< owning RSCL */
	MM_RememberedSetCardBucket *_next;						/**< next bucket in the owning RSCL */
	UDATA _bufferCount;										/**< the count of buffers in this bucket */
protected:
public:
	/* function members */
private:
	/**
	 * Clear the list (release all the buffers back to the buffer pool)
	 * @param buffersToLocalPoolCount max buffers returned to the local pool. typically UDATA_MAX (all goes to local) or MAX_LOCAL_RSCL_BUFFER_POOL_SIZE (small part goes to local, rest to global)
	 */
	void releaseBuffers(MM_EnvironmentVLHGC *env, UDATA buffersToLocalPoolCount);
protected:
public:
	bool initialize(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *rscl, MM_RememberedSetCardBucket *next);
	void tearDown(MM_GCExtensions *extensions);

	/**
	 * Add a card represented by the object to the list
	 * @param card  card to be remembered
	 */
	MMINLINE void add(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card)
	{
		MM_RememberedSetCard *current = _current;
		UDATA offset = (UDATA)current & OFFSET_MASK;

		if (0 != offset) {
			/* there is room in the current buffer
			 * simple optimization to avoid duplicates: check if this card is same as the last stored card
			 * (at this point we know current is no NULL)
			 */
			if (card != *(current - 1)) {
				/* no, not same, add it */
				_current = current + 1;
				*current = card;
			}
		} else {
			addToNewBuffer(env, card);
		}
	}

	/**
	 * If current buffer is full or bucket is empty,
	 * add a card to a new buffer.
	 * Handle various misc scenarios when unable to allocate new buffer
	 * or list is already overflowed.
	 * @param card  card to be remembered
	 */
	void addToNewBuffer(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card);

	/**
	 * Search the list and check if this card is remembered.
	 * Not thread safe.
	 * @param card  card being searched for
	 * @return true if card is remembered, false otherwise
	 */
	bool isRemembered(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card);

	/**
	 * Clear the list. Release buffers back to the global buffer pool. A small fraction may be release to the thread local pool.
	 */
	void globalReleaseBuffers(MM_EnvironmentVLHGC *env);

	/**
	 * Clear the list. Release buffers back to the local pool.
	 */
	void localReleaseBuffers(MM_EnvironmentVLHGC *env);

	/**
	 * Set a list as being overflowed.
	 * This releases all the buffers from the current bucket (but not from other buckets of the list).
	 * The list may be anyone (the owner of the current bucket or any other).
	 * @param listToOverflow RSCL to overflow
	 */
	void setListAsOverflow(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *listToOverflow);

	/**
	 * Remove NULL entries and compact the list. Releases unused buffers.
	 * Not thread safe. Called only for non-overflowed lists.
	 */
	void compact(MM_EnvironmentVLHGC *env);

	/**
	 * Is bucket Empty (it is sufficient to check if the current buffer is empty)
	 * return true if empty
	 */
	bool isEmpty(MM_EnvironmentVLHGC *env) {
		return (NULL == _current);
	}

	/**
	 * @return the size of the bucket (in terms card count) 
	 */
	UDATA getSize(MM_EnvironmentVLHGC *env);
	
	/**
	 * @return the number of buffers in the receiver's linked list
	 */
	UDATA getBufferCount(MM_EnvironmentVLHGC *env) {
		return _bufferCount;
	}
	
	/**
	 * @return true if _current pointer points within the provided buffer's card list
	 */
	bool isCurrentSlotWithinBuffer(MM_RememberedSetCard *bufferCardList) {
		return ((bufferCardList < _current) && (_current < bufferCardList + MAX_BUFFER_SIZE));
	}

	MM_RememberedSetCardBucket()
	  :	_cardBufferControlBlockHead(NULL)
	  , _current(NULL)
	  , _rscl(NULL)
	  , _next(NULL)
	  , _bufferCount(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* REMEMBEREDSETCARDBUCKET_HPP */
