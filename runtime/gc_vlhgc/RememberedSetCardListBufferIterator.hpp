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

#if !defined(REMEMBEREDSETCARDLISTBUFFERITERATOR_HPP)
#define REMEMBEREDSETCARDLISTBUFFERITERATOR_HPP


#include "RememberedSetCardList.hpp"

class GC_RememberedSetCardListBufferIterator
{
private:
	MM_RememberedSetCardList *_rscl; /**< RememberedSetCardList being iterated */

	MM_RememberedSetCardBucket *_currentBucket;				/**< current bucket pointer */
	MM_RememberedSetCard *_bufferCardList; 					/**< current buffer */
	MM_CardBufferControlBlock *_cardBufferControlBlockPrevious;	/**< previous buffer control block */
	MM_CardBufferControlBlock *_cardBufferControlBlockCurrent;	/**< current buffer control block */
	MM_CardBufferControlBlock *_cardBufferControlBlockNext; 	/**< next buffer control block */

private:
	/**
	 * Next bucket within the list that has at least one buffer in it.
	 * @return true if there was a new bucket
	 */
	bool nextBucket(MM_EnvironmentBase* env);

protected:
public:

	/**
	 * Construct a CardList Iterator for a given CardList
	 *
	 * @param rscl CardList being iterated
	 */
	GC_RememberedSetCardListBufferIterator(MM_RememberedSetCardList *rscl)
		: _rscl(rscl)
		, _currentBucket(NULL)
		, _bufferCardList(NULL)
		, _cardBufferControlBlockPrevious(NULL)
		, _cardBufferControlBlockCurrent(NULL)
		, _cardBufferControlBlockNext(NULL)
		{}

	/**
	 * Next buffer given a current buffer (control block). Initializes _bufferCardList and resets _cardIndex.
	 * @return true if there was a new buffer
	 */
	MM_CardBufferControlBlock *nextBuffer(MM_EnvironmentBase* env, MM_RememberedSetCard **lastCard);

	/**
	 * Unlink current buffer. Caller of the iterator is supposed to relocate the content of the buffer.
	 */
	void unlinkCurrentBuffer(MM_EnvironmentBase* env);

};

#endif /* REMEMBEREDSETCARDLISTBUFFERITERATOR_HPP */
