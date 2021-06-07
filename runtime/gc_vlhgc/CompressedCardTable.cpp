
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
 * @ingroup gc_vlhgc
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"
#include "ModronAssertions.h"

#include "CardCleaner.hpp"
#include "CardTable.hpp"
#include "CompressedCardTable.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"

#define BITS_PER_BYTE	8
#define COMPRESSED_CARDS_PER_WORD	(sizeof(UDATA) * BITS_PER_BYTE)

/*
 * Bit 1 meaning may be dirty (traditional) or clear (inverted)
 * Both of this cases are supported in code
 * just define COMPRESSED_CARD_TABLE_INVERTED if you want inverted version
 */
/* #define COMPRESSED_CARD_TABLE_INVERTED */

#if defined(COMPRESSED_CARD_TABLE_INVERTED)

#define AllCompressedCardsInWordClean	UDATA_MAX
#define AllCompressedCardsInWordDirty	0
#define CompressedCardDirty				0

#else /* defined(COMPRESSED_CARD_TABLE_INVERTED) */

#define	AllCompressedCardsInWordClean	0
#define AllCompressedCardsInWordDirty	UDATA_MAX
#define CompressedCardDirty				1

#endif /* defined(COMPRESSED_CARD_TABLE_INVERTED) */

/*
 * Number of cards per one compressed card table bit
 * 1 means bit per card, so compressed card table has full information and no need to look at original card table element
 * We can try to reduce size if compressed card table representing more then one card per bit however
 * if compressed card table bit is set an original card must be checked as well
 */
#define COMPRESSED_CARD_TABLE_DIV	1

MM_CompressedCardTable *
MM_CompressedCardTable::newInstance(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_CompressedCardTable *compressedCardTable = (MM_CompressedCardTable *)env->getForge()->allocate(sizeof(MM_CompressedCardTable), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != compressedCardTable) {
		new(compressedCardTable) MM_CompressedCardTable();
		if (!compressedCardTable->initialize(env, heap)) {
			compressedCardTable->kill(env);
			compressedCardTable = NULL;
		}
	}
	return compressedCardTable;
}

bool
MM_CompressedCardTable::initialize(MM_EnvironmentBase *env, MM_Heap *heap)
{
	/* heap maximum physical range must be aligned with compressed card table word */
	Assert_MM_true(0 == (heap->getMaximumPhysicalRange() % (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV * COMPRESSED_CARDS_PER_WORD)));

	/* Calculate compressed card table size in bytes */
	UDATA compressedCardTableSize = heap->getMaximumPhysicalRange() / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV * BITS_PER_BYTE);

	/* Allocate compressed card table */
	_compressedCardTable = (UDATA *)env->getForge()->allocate(compressedCardTableSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());

	_heapBase = (UDATA)heap->getHeapBase();

	return (NULL != _compressedCardTable);
}

void
MM_CompressedCardTable::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_CompressedCardTable::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _compressedCardTable) {
		env->getForge()->free(_compressedCardTable);
	}
}

MMINLINE bool
MM_CompressedCardTable::isDirtyCardForPartialCollect(Card state)
{
	bool result = false;

	switch(state) {
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
MM_CompressedCardTable::setCompressedCardsDirtyForPartialCollect(void *startHeapAddress, void *endHeapAddress)
{
	UDATA compressedCardStartOffset = ((UDATA)startHeapAddress - _heapBase) / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
	UDATA compressedCardEndOffset = ((UDATA)endHeapAddress - _heapBase) / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
	UDATA compressedCardStartIndex = compressedCardStartOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA compressedCardEndIndex = compressedCardEndOffset / COMPRESSED_CARDS_PER_WORD;

	/*
	 *  To simplify test logic assume here that given addresses are aligned to correspondent compressed card word border
	 *  So no need to handle side pieces (no split of compressed card table words between regions)
	 *  However put an assertions here
	 */
	Assert_MM_true(0 == (compressedCardStartOffset % COMPRESSED_CARDS_PER_WORD));
	Assert_MM_true(0 == (compressedCardEndOffset % COMPRESSED_CARDS_PER_WORD));

	UDATA i;
	for (i = compressedCardStartIndex; i < compressedCardEndIndex; i++) {
		_compressedCardTable[i] = AllCompressedCardsInWordDirty;
	}
}

void
MM_CompressedCardTable::rebuildCompressedCardTableForPartialCollect(MM_EnvironmentBase *env, void *startHeapAddress, void *endHeapAddress)
{
	MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
	Card *card = cardTable->heapAddrToCardAddr(env, startHeapAddress);
	Card *cardLast = cardTable->heapAddrToCardAddr(env, endHeapAddress);
	UDATA compressedCardStartOffset = ((UDATA)startHeapAddress - _heapBase) / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
	UDATA compressedCardStartIndex = compressedCardStartOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA *compressedCard = &_compressedCardTable[compressedCardStartIndex];
	UDATA mask = 1;
	const UDATA endOfWord = ((UDATA)1) << (COMPRESSED_CARDS_PER_WORD - 1);
	UDATA compressedCardWord = AllCompressedCardsInWordClean;

	/*
	 *  To simplify test logic assume here that given addresses are aligned to correspondent compressed card word border
	 *  So no need to handle side pieces (no split of compressed card table words between regions)
	 *  However put an assertion here
	 */
	Assert_MM_true(0 == (compressedCardStartOffset % COMPRESSED_CARDS_PER_WORD));

	while (card < cardLast) {

#if (1 == COMPRESSED_CARD_TABLE_DIV)

		Card state = *card++;
		if (isDirtyCardForPartialCollect(state)) {
			/* invert bit */
			compressedCardWord ^= mask;
		}

#else /* COMPRESSED_CARD_TABLE_DIV == 1 */
		/*
		 * This implementation supports case for COMPRESSED_CARD_TABLE_DIV == 1 as well
		 * Special implementation above extracted with hope that it is faster
		 */
		Card *next = card + COMPRESSED_CARD_TABLE_DIV;
		/* check cards responsible for this bit until first dirty or value found */
		for (UDATA j = 0; j < COMPRESSED_CARD_TABLE_DIV; j++) {
			Card state = *card++;
			if (isDirtyCardForPartialCollect(state)) {
				/* invert bit */
				compressedCardWord ^= mask;
				break;
			}
		}
		/* rewind card pointer to first card for next bit */
		card = next;
#endif /* COMPRESSED_CARD_TABLE_DIV == 1 */

		if (mask == endOfWord) {
			/* last bit in word handled - save word and prepare mask for next one */
			*compressedCard++ = compressedCardWord;
			mask = 1;
			compressedCardWord = AllCompressedCardsInWordClean;
		} else {
			/* mask for next bit to handle */
			mask = mask << 1;
		}
	}

	/* end heap address must be aligned*/
	Assert_MM_true(1 == mask);
}

bool
MM_CompressedCardTable::isCompressedCardDirtyForPartialCollect(MM_EnvironmentBase *env, void *heapAddr)
{
	UDATA compressedCardOffset = ((UDATA)heapAddr - _heapBase) / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
	UDATA compressedCardIndex = compressedCardOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA compressedCardWord = _compressedCardTable[compressedCardIndex];
	bool cardDirty = false;

	if (AllCompressedCardsInWordClean != compressedCardWord) {
		UDATA bit = compressedCardOffset % COMPRESSED_CARDS_PER_WORD;
		cardDirty = (CompressedCardDirty == ((compressedCardWord >> bit) & 1));

#if (COMPRESSED_CARD_TABLE_DIV > 1)
		/*
		 * One bit of compressed card table is responsible for few cards
		 * so if fast check return true we should look at card itself
		 */
		if (cardDirty) {
			MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
			Card *cardAddr = cardTable->heapAddrToCardAddr(env, heapAddr);
			cardDirty = isDirtyCardForPartialCollect(*cardAddr);
		}
#endif /* COMPRESSED_CARD_TABLE_DIV > 1 */
	}

	return cardDirty;
}

void
MM_CompressedCardTable::cleanCardsInRegion(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, MM_HeapRegionDescriptor *region)
{
	cleanCardsInRange(env, cardCleaner, region->getLowAddress(), region->getHighAddress());
}

void
MM_CompressedCardTable::cleanCardsInRange(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, void *startHeapAddress, void *endHeapAddress)
{
	UDATA compressedCardStartOffset = ((UDATA)startHeapAddress - _heapBase) / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
	UDATA compressedCardEndOffset = ((UDATA)endHeapAddress - _heapBase) / (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
	UDATA compressedCardStartIndex = compressedCardStartOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA compressedCardEndIndex = compressedCardEndOffset / COMPRESSED_CARDS_PER_WORD;

	/*
	 *  To simplify test logic assume here that given addresses are aligned to correspondent compressed card word border
	 *  So no need to handle side pieces (no split of compressed card table words between regions)
	 *  However put an assertions here
	 */
	Assert_MM_true(0 == (compressedCardStartOffset % COMPRESSED_CARDS_PER_WORD));
	Assert_MM_true(0 == (compressedCardEndOffset % COMPRESSED_CARDS_PER_WORD));

	MM_CardTable *cardTable = MM_GCExtensions::getExtensions(env)->cardTable;
	Card *card = cardTable->heapAddrToCardAddr(env, startHeapAddress);
	U_8 *address = (U_8 *)startHeapAddress;
	UDATA cardsCleaned = 0;

	for (UDATA i = compressedCardStartIndex; i < compressedCardEndIndex; i++) {
		UDATA compressedCardWord = _compressedCardTable[i];
		if (AllCompressedCardsInWordClean != compressedCardWord) {
			/* search for dirty cards - iterate bits */
			for (UDATA j = 0; j < COMPRESSED_CARDS_PER_WORD; j++) {
				if (CompressedCardDirty == (compressedCardWord & 1)) {
					for (UDATA k = 0; k < COMPRESSED_CARD_TABLE_DIV; k++) {
						/* clean card */
						cardCleaner->clean(env, address, address + CARD_SIZE, card);
						card += 1;
						address += CARD_SIZE;
						cardsCleaned += 1;
					}
				} else {
					/* skip cards this bit responsible for */
					card += COMPRESSED_CARD_TABLE_DIV;
					address += (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV);
				}
				compressedCardWord >>= 1;
			}
		} else {
			/* skip cards this word responsible for */
			card += (COMPRESSED_CARD_TABLE_DIV * COMPRESSED_CARDS_PER_WORD);
			address += (CARD_SIZE * COMPRESSED_CARD_TABLE_DIV * COMPRESSED_CARDS_PER_WORD);
		}
	}

	env->_cardCleaningStats._cardsCleaned += cardsCleaned;
}

bool
MM_CompressedCardTable::isReady()
{
	Assert_MM_true(_regionsProcessed <= _totalRegions);

	bool result = (_regionsProcessed == _totalRegions);
	if (result) {
		/* need load sync at weak ordered platforms */
		MM_AtomicOperations::loadSync();
	}

	return result;
}
