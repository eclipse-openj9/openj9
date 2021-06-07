
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

#if !defined(COMPRESSEDCARDTABLE_HPP_)
#define COMPRESSEDCARDTABLE_HPP_


#include "j9.h"
#include "j9cfg.h"

#include "AtomicOperations.hpp"
#include "Base.hpp"
#include "CardTable.hpp"

class MM_CardCleaner;
class MM_EnvironmentBase;
class MM_Heap;
class MM_HeapRegionDescriptor;


class MM_CompressedCardTable : public MM_BaseNonVirtual
{
public:
protected:
private:
	UDATA *_compressedCardTable;	/**< start address of compressed card table */
	UDATA _heapBase;	/**< Store heap base locally. Use UDATA type because need it for arithmetic only */
	volatile UDATA _totalRegions;	/**< total number of regions discovered at table rebuild time */
	volatile UDATA _regionsProcessed; /**< number of regions completed while table is being rebuilt */

public:
	/**
	 * Create new instance of class
	 * @param env current thread environment
	 * @param heap current heap
	 */
	static MM_CompressedCardTable *newInstance(MM_EnvironmentBase *env, MM_Heap *heap);

	/**
	 * Set all Compressed Cards correspondent with given heap range dirty for partial collect
	 * @param startHeapAddress start heap address
	 * @param endHeapAddress end heap address
	 */
	void setCompressedCardsDirtyForPartialCollect(void *startHeapAddress, void *endHeapAddress);

	/**
	 * Rebuild Compressed Cards for given heap range
	 * Card is going to be marked dirty if original card is dirty for partial collect
	 * @param env current thread environment
	 * @param startHeapAddress start heap address
	 * @param endHeapAddress end heap address
	 */
	void rebuildCompressedCardTableForPartialCollect(MM_EnvironmentBase *env, void *startHeapAddress, void *endHeapAddress);

	/**
	 * Check is Compressed Card correspondent with heap address dirty for partial collect
	 * @param env current thread environment
	 * @param heapAddress heap address
	 * @return true if fast card is set dirty
	 */
	bool isCompressedCardDirtyForPartialCollect(MM_EnvironmentBase *env, void *heapAddr);

	/**
	 * Cleaning cards for range
	 * Iterate Compressed Cards and clean marked dirty
	 * It is important that set of card states treated 'dirty' in given Card Cleaner must be the same or narrower then
	 * was used for in Compressed Card Table rebuild. If it is not correct some dirty cards might be missed
	 * @param env current thread environment
	 * @param cardCleaner given Card Cleaner
	 * @param region region card cleaning should be done
	 */
	void cleanCardsInRegion(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, MM_HeapRegionDescriptor *region);

	/**
	 * Check is Card Table Summary rebuild is completed
	 * @return true if rebuild is completed
	 */
	bool isReady();

	/**
	 * Clear processed regions counter
	 */
	MMINLINE void clearRegionsProcessedCounter()
	{
		_regionsProcessed = 0;
	}

	/**
	 * Atomically increment processed regions counter
	 * @param total total number of discovered regions
	 * @param processed number of processed regions
	 */
	MMINLINE void incrementProcessedRegionsCounter(UDATA total, UDATA processed)
	{
		_totalRegions = total;

		if (processed > 0) {
			MM_AtomicOperations::storeSync();
			MM_AtomicOperations::add(&_regionsProcessed, processed);
		}
	}

	/**
	 * General class kill
	 * @param env current thread environment
	 */
	void kill(MM_EnvironmentBase *env);

protected:
	/**
	 * General class initialization
	 * @param env current thread environment
	 * @param heap heap
	 */
	bool initialize(MM_EnvironmentBase *env, MM_Heap *heap);

	/**
	 * General class tear down
	 * @param env current thread environment
	 */
	void tearDown(MM_EnvironmentBase *env);

	/**
	 * Create a compressedCardTable object.
	 */
	MM_CompressedCardTable()
		: MM_BaseNonVirtual()
		, _compressedCardTable(NULL)
		, _heapBase(0)
		, _totalRegions(1)
		, _regionsProcessed(0)
	{
		_typeId = __FUNCTION__;
	}

private:
	/**
	 * Check should card be treated as dirty for partial collect
	 * @param state current card state
	 */
	bool isDirtyCardForPartialCollect(Card state);

	/**
	 * Cleaning cards for range
	 * Iterate Compressed Cards and clean marked dirty
	 * It is important that set of card states treated 'dirty' in given Card Cleaner must be the same or narrower then
	 * was used for in Compressed Card Table rebuild. If it is not correct some dirty cards might be missed
	 * @param env current thread environment
	 * @param cardCleaner given Card Cleaner
	 * @param startHeapAddress start heap address
	 * @param endHeapAddress end heap address
	 */
	void cleanCardsInRange(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, void *startHeapAddress, void *endHeapAddress);
};

#endif /* COMPRESSEDCARDTABLE_HPP_ */
