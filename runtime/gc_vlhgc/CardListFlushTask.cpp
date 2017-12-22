
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
#include "ModronAssertions.h"

#include "CardListFlushTask.hpp"

#include "CardTable.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "InterRegionRememberedSet.hpp"
#include "RememberedSetCardListBufferIterator.hpp"
#include "RememberedSetCardListCardIterator.hpp"


void
MM_CardListFlushTask::masterSetup(MM_EnvironmentBase *env)
{
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);
}

void
MM_CardListFlushTask::masterCleanup(MM_EnvironmentBase *env)
{
}

void
MM_CardListFlushTask::writeFlushToCardState(Card *card, bool gmpIsActive)
{
	Card fromState = *card;
	switch(fromState) {
	case CARD_CLEAN:
		if (gmpIsActive) {
			*card = CARD_REMEMBERED_AND_GMP_SCAN;
		} else {
			*card = CARD_REMEMBERED;
		}
		break;
	case CARD_GMP_MUST_SCAN:
		*card = CARD_REMEMBERED_AND_GMP_SCAN;
		break;
	case CARD_REMEMBERED_AND_GMP_SCAN:
	case CARD_DIRTY:
		/* do nothing */
		break;
	case CARD_REMEMBERED:
		if (gmpIsActive) {
			*card = CARD_REMEMBERED_AND_GMP_SCAN;
		} else {
			/* do nothing */
		}
		break;
	case CARD_PGC_MUST_SCAN:
		if (gmpIsActive) {
			/* PGC_MUST_SCAN is a slightly stronger statement than REMEMBERED so we only add the GMP requirement by marking this DIRTY */
			*card = CARD_DIRTY;
		} else {
			/* do nothing */
		}
		break;
	default:
		Assert_MM_unreachable();
		break;
	}
}

void
MM_CardListFlushTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	/* this function has knowledge of the collection set, which is only valid during a PGC */
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);
	bool gmpIsActive = (NULL != env->_cycleState->_externalCycleState);

	/* flush RS Lists for CollectionSet and dirty card table */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	MM_InterRegionRememberedSet *interRegionRememberedSet = extensions->interRegionRememberedSet;
	bool shouldFlushBuffersForUnregisteredRegions = interRegionRememberedSet->getShouldFlushBuffersForDecommitedRegions();

	while (NULL != (region = regionIterator.nextRegion())) {
		if (NULL != region->getMemoryPool()) {
			if(region->_markData._shouldMark) {
				if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					Assert_MM_true(region->getRememberedSetCardList()->isAccurate());
					/* Iterate non-overflowed buckets of the list */
					GC_RememberedSetCardListCardIterator rsclCardIterator(region->getRememberedSetCardList());
					MM_RememberedSetCard card = 0;
					while(0 != (card = rsclCardIterator.nextReferencingCard(env))) {
						/* For Marking purposes we do not need to track references within Collection Set */
						MM_HeapRegionDescriptorVLHGC *referencingRegion = interRegionRememberedSet->tableDescriptorForRememberedSetCard(card);
						if (referencingRegion->containsObjects() && !referencingRegion->_markData._shouldMark) {
							Card *cardAddress = interRegionRememberedSet->rememberedSetCardToCardAddr(env, card);
							writeFlushToCardState(cardAddress, gmpIsActive);
						}
					}
	
					/* Clear remembered references to each region in Collection Set (completely clear RS Card List for those regions
					 * and appropriately update RSM). We are about to rebuild those references in PGC.
					 */
					_interRegionRememberedSet->clearReferencesToRegion(env, region);
				}
			} else if (shouldFlushBuffersForUnregisteredRegions) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					/* flush the content of buffers owned by decommitted regions (but used/borrowed by other active regions) */
					UDATA toRemoveCount = 0;
					UDATA totalCountBefore = region->getRememberedSetCardList()->getSize(env);
					MM_RememberedSetCard *lastCardInCurrentBuffer = NULL;
					MM_CardBufferControlBlock *cardBufferControlBlockCurrent = NULL;
					GC_RememberedSetCardListBufferIterator rsclBufferIterator(region->getRememberedSetCardList());
					while (NULL != (cardBufferControlBlockCurrent = rsclBufferIterator.nextBuffer(env, &lastCardInCurrentBuffer))) {
						/* find a region owning current Buffer being iterated */
						MM_HeapRegionDescriptorVLHGC *bufferOwningRegion = interRegionRememberedSet->getBufferOwningRegion(cardBufferControlBlockCurrent);
						/* owned by a decommited region, which hasn't released its buffer pool yet? */
						if (!bufferOwningRegion->isCommitted()) {
							Assert_MM_true(NULL != bufferOwningRegion->getRsclBufferPool());
							rsclBufferIterator.unlinkCurrentBuffer(env);
							for (MM_RememberedSetCard *cardSlot = cardBufferControlBlockCurrent->_card; cardSlot < lastCardInCurrentBuffer; cardSlot++) {
								MM_RememberedSetCard card = *cardSlot;
								MM_HeapRegionDescriptorVLHGC *referencingRegion = interRegionRememberedSet->tableDescriptorForRememberedSetCard(card);
								if (referencingRegion->containsObjects() && !referencingRegion->_markData._shouldMark) {
									Card *cardAddress = interRegionRememberedSet->rememberedSetCardToCardAddr(env, card);
									writeFlushToCardState(cardAddress, gmpIsActive);
								}
								toRemoveCount += 1;
							}
						}
					}
					UDATA totalCountAfter = region->getRememberedSetCardList()->getSize(env);
					Assert_MM_true(totalCountBefore == (toRemoveCount + totalCountAfter));
				}
			}
		}
	}
}

void
MM_CardListFlushTask::setup(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	}
}

void
MM_CardListFlushTask::cleanup(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		env->_cycleState = NULL;
	}
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
void
MM_CardListFlushTask::synchronizeGCThreads(MM_EnvironmentBase *env, const char *id)
{
	/* unused in this task */
	Assert_MM_unreachable();
}

bool
MM_CardListFlushTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id)
{
	/* unused in this task */
	Assert_MM_unreachable();
	return true;
}

bool
MM_CardListFlushTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id)
{
	/* unused in this task */
	Assert_MM_unreachable();
	return true;
}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
