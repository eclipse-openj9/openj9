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

#if !defined(INTERREGIONREMEMBEREDSET_HPP)
#define INTERREGIONREMEMBEREDSET_HPP

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

class MM_CollectionStatisticsVLHGC;

#include "BaseVirtual.hpp"
#include "CardTable.hpp"
#include "CycleState.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "RememberedSetCardList.hpp"

/* value for MAX_LOCAL_RSCL_BUFFER_POOL_SIZE is empirically chosen to be the lowest one but still reduces most of contention on global pool lock */
#define MAX_LOCAL_RSCL_BUFFER_POOL_SIZE 16


class MM_InterRegionRememberedSet : public MM_BaseVirtual
{
private:

public:
	MM_HeapRegionManager *_heapRegionManager;				/**< cached pointer to heap region manager */

	MM_CardBufferControlBlock *_rsclBufferControlBlockPool; /**< starting address of the global pool of control blocks (kept around to be able to release memory at the end) */
	MM_CardBufferControlBlock * volatile _rsclBufferControlBlockHead; /**< current head of BufferControlBlock global pool list */
	volatile UDATA _freeBufferCount;							/**< current count of Buffers in the global free pool */
	UDATA _bufferCountTotal;									/**< total count of Buffers in the system (used or free) */
	UDATA _bufferControlBlockCountPerRegion;							/**< A const, count of buffers owned by a region */
	MM_LightweightNonReentrantLock _lock;					/**< lock used to protect concurrent alloc/free from/to BufferControlBlock global pool */

	MM_RememberedSetCardList * volatile _overflowedListHead; /**< global list of overflowed regions that have unreleased buffers - list head */
	MM_RememberedSetCardList * volatile _overflowedListTail; /**< global list of overflowed regions that have unreleased buffers - list tail */

	UDATA _regionSize;  			/**< Cached region size */

	bool _shouldFlushBuffersForDecommitedRegions;			/**< set to true at the end of a GC, if contraction occured. this is a signal for the next GC to perform flush buffers from regions contracted */

	volatile UDATA _overflowedRegionCount;					/**< count of regions overflowed as full */
	UDATA _stableRegionCount;								/**< count of regions overflowed as stable */
	volatile UDATA _beingRebuiltRegionCount;				/**< count of overflowed regions currently being rebuilt */
	double _unusedRegionThreshold;							/**< fraction of region unused (free&fragmented) to be considered full (used for stable region detection) */

	MM_HeapRegionDescriptor *_regionTable;					/**< cached copy of regionTable (from HeapRegionManager) */
	UDATA _tableDescriptorSize;								/**< cached heap region tableDescriptorSize (from  HeapRegionManager)*/
	UDATA _cardToRegionShift;  								/**< the shift value to use against RememberedSetCards to determine the corresponding region index */
	UDATA _cardToRegionDisplacement;						/**< the displacement value to use against RememberedSetcards to determine the corresponding region index */
	MM_CardTable *_cardTable;								/**< cached copy of card table */

	MM_RememberedSetCardBucket *_rememberedSetCardBucketPool; /**< RS bucket pool (for all regions) for Master thread or any other thread that caused GC in absence of Master thread */

private:

	/** 
	 * Get the physical table descriptor at the specified index.
	 * 
	 * @param regionIndex - the index of the region - this is not bounds checked
	 * @return the region descriptor at the specified index 
	 */
	MMINLINE MM_HeapRegionDescriptorVLHGC *physicalTableDescriptorForIndex(UDATA regionIndex) {
		UDATA descriptorSize =_tableDescriptorSize;
		UDATA regionTable = (UDATA)_regionTable;
		return (MM_HeapRegionDescriptorVLHGC *)(regionTable + (regionIndex * descriptorSize));
	}
	
	/**
	 * Remember reference  from an object to an object (or appropriate structure (like card or region) the object belongs to)
	 * @param fromObject object (its slot) pointing from
	 * @param toRegion region being pointed at
	 */
	MMINLINE void rememberReferenceInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, MM_HeapRegionDescriptorVLHGC *toRegion);
	
	/**
	 * Find a region (its RSCL) to overflow. Look for a list with large number of buffers.
	 * @param env current thread environment
	 * @return region to overflow, NULL if none is found.
	 */
	MM_RememberedSetCardList *findRsclToOverflow(MM_EnvironmentVLHGC *env);

	/**
	 * Add provided (overflowed) RSCL to the global list of overflowed RSCL found during this GC cycle
	 * @param env current thread environment
	 * @param rsclToEnqueue RSCL to add to the global overflow list
	 */
	void enqueueOverflowedRscl(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *rsclToEnqueue);

	/**
	 * Out-of-line implementation of rememberReferenceForMark()
	 * @param fromObject object (its slot) pointing from (must not be NULL)
	 * @param toObject object being pointed (must not be NULL and must be in a different region than fromObject)
	 */
	void rememberReferenceForMarkInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject);

	/**
	 * Out-of-line implementation of rememberReferenceForCompact()
	 * @param fromObject object (its slot) pointing from (must not be NULL)
	 * @param toObject object being pointed (must not be NULL and must be in a different region than fromObject)
	 */
	void rememberReferenceForCompactInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject);

	/**
	 * Out-of-line implementation of rememberReferenceForCopyForward()
	 * @param fromObject object (its slot) pointing from (must not be NULL)
	 * @param toObject object being pointed (must not be NULL and must be in a different region than fromObject)
	 */
	void rememberReferenceForCopyForwardInternal(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject);

	/**
	 * Check should card be treated as dirty
	 * @param env current thread environment
	 * @param cardTable current card table
	 * @param heapAddress heap address we check card for
	 * @return true if card should be treated as dirty
	 */
	bool isDirtyCardForPartialCollect(MM_EnvironmentVLHGC *env, MM_CardTable *cardTable, Card *card);

	/**
	 * Rebuild Compressed Card Table for Mark (multithreaded, by regions)
	 * @param env current thread environment
	 */
	void rebuildCompressedCardTableForMark(MM_EnvironmentVLHGC* env);

	/**
	 * Rebuild Compressed Card Table for Compact (multithreaded, by regions)
	 * @param env current thread environment
	 */
	void rebuildCompressedCardTableForCompact(MM_EnvironmentVLHGC* env);

	/**
	 * Clears references from Collection Set and from dirty cards
	 * without using of compressed card table
	 */
	void clearFromRegionReferencesForMarkDirect(MM_EnvironmentVLHGC* env);

	/**
	 * Clears references from Collection Set and from dirty cards
	 * using compressed card table
	 */
	void clearFromRegionReferencesForMarkOptimized(MM_EnvironmentVLHGC* env);

	/**
	 * Clears references from Compaction Set and from dirty cards
	 * without using of compressed card table
	 */
	void clearFromRegionReferencesForCompactDirect(MM_EnvironmentVLHGC* env);

	/**
	 * Clears references from Compaction Set and from dirty cards
	 * using compressed card table
	 */
	void clearFromRegionReferencesForCompactOptimized(MM_EnvironmentVLHGC* env);
	MM_InterRegionRememberedSet(MM_HeapRegionManager *heapRegionManager);
	
protected:
public:

	/**
	 *	Setup for partial collect
	 *	@param env current thread environment
	 */
	void setupForPartialCollect(MM_EnvironmentVLHGC *env);

	/**
	 * Overflow region's RSCL if considered stable (full and high survivor rate)
	 * @param env[in] of a GC thread
	 * @param region[in] to overflow
	 */
	void overflowIfStableRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Set the threshold for when a region is consider full. Used each time before stable region detection.
	 * @param env[in] of a GC thread
	 * @param averageEmptinessOfCopyForwardedRegions[in] average fragmentation of copy-forward is used to determine the final threshold
	 */
	void setUnusedRegionThreshold(MM_EnvironmentVLHGC *env, double averageEmptinessOfCopyForwardedRegions);

	/**
	 * Reset the list of overflowed RSCL (used to fast-find an overflowed RSCL with unrelease buffers)
	 */
	void resetOverflowedList();

	/**
	 * Copy internal stats counters into external structure used for Verbose GC reporting
	 * @param env[in] of a GC thread
	 * @param stats[out] the structure where the counters will be exported to
	 */
	void exportStats(MM_EnvironmentVLHGC* env, MM_CollectionStatisticsVLHGC *stats);

	/**
	 * Allocate a BufferControlBlock (that represents a buffer for a Buffer in a Card List Bucket) from the thread local pool
	 * Replenish local pool from global pool, if needed
	 * @return pointer to the BufferControlBlock or NULL if no more left in the pool
	 */
	MM_CardBufferControlBlock *allocateCardBufferControlBlockFromLocalPool(MM_EnvironmentVLHGC* env);
	/**
	 * allocate a BufferControlBlock list from the global pool
	 * @param bufferCount desired length of the list to be allocated
	 */
	void allocateCardBufferControlBlockList(MM_EnvironmentVLHGC* env, UDATA bufferCount);

	/**
	 * release a list of BufferControlBlocks back to the global pool
	 * @param controlBlock pointer to the head of BufferControlBlock list being returned
	 * @return the number of buffers in the list being released 
	 */
	UDATA releaseCardBufferControlBlockList(MM_EnvironmentVLHGC* env, MM_CardBufferControlBlock *controlBlockHead, MM_CardBufferControlBlock *controlBlockTail = NULL);
	
	/**
	 * release the complete list (from head to tail) of BufferControlBlocks for a given thread, and set the head and tail pointers to null.
	 */
	void releaseCardBufferControlBlockListForThread(MM_EnvironmentVLHGC* env, MM_EnvironmentVLHGC* threadEnv);

	/**
	 * release a BufferControlBlock list to the thread local pool.
	 * Only a portion of the list (for example, up to MAX_LOCAL_RSCL_BUFFER_POOL_SIZE) will be released to the local pool.
	 * The rest is released to the global pool.
	 * @param controlBlockHead head of the list to be released
	 * @param maxBuffersToLocalPool portion of the list provided to be returned to the local pool. typically UDATA_MAX (all goes to local) or MAX_LOCAL_RSCL_BUFFER_POOL_SIZE (small part goes to local, rest to global)
	 * @return the count of buffers in the pool being returned
	 */
	UDATA releaseCardBufferControlBlockListToLocalPool(MM_EnvironmentVLHGC* env, MM_CardBufferControlBlock *controlBlockHead, UDATA maxBuffersToLocalPool);

	/**
	 * release a BufferControlBlock list to the global pool of buffers. This is multithreaded safe, but a lock is used (may cause contention on large SMPs)
	 * A small fraction of the list may be release to the local pool before.
	 * @param controlBlockHead head of the list to be released
	 * @return the count of buffers in the pool being returned
	 */
	UDATA releaseCardBufferControlBlockListToGlobalPool(MM_EnvironmentVLHGC* env, MM_CardBufferControlBlock *controlBlockHead);

	/**
	 * release the thread local pool list (into the global pool list)
	 */
	void releaseCardBufferControlBlockLocalPools(MM_EnvironmentVLHGC* env);

	/**
	 * Allocate RSCL Buffer pool local to this region's RSCL (Buffers can still be shared among other regions)
	 * @param region[in] region being committed
	 * @param env[in] of a GC thread
	 */
	bool allocateRegionBuffers(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region);
	
	/**
	 * Identify buffers owned by regions being decommited. Flush their content and unlink them from any
	 * list they belong (either some regions RSCL or free list)
	 * @param env[in] of a GC thread
	 */
	void flushBuffersForDecommitedRegions(MM_EnvironmentVLHGC* env);


	/**
	 * During marking, remember reference  from an object to an object (or appropriate structure (like card or region) the object belongs to)
	 * @param fromObject object (its slot) pointing from
	 * @param toObject object being pointed
	 */
	MMINLINE void
	rememberReferenceForMark(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
	{
		if (NULL != toObject) {
			/* We use the XOR and region mask of object pointers to determine if the objects share the same region.
			 * Getting the region descriptors for comparison is a much more expensive operation (and reserved for when
			 * we have identified the objects as being from different regions)
			 */
			if( (((UDATA)fromObject) ^ ((UDATA)toObject)) >= _regionSize ) {
				rememberReferenceForMarkInternal(env, fromObject, toObject);
			}
		}
	}

	/**
	 * Used for card scrubbing. Determine if a reference from fromObject to toObject needs to be remembered
	 * during the current global mark.
	 * @param env the current thread
	 * @param fromObject object (its slot) pointing from
	 * @param toObject object being pointed
	 */
	MMINLINE bool
	shouldRememberReferenceForGlobalMark(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
	{
		Assert_MM_true(MM_CycleState::CT_GLOBAL_MARK_PHASE == env->_cycleState->_collectionType);
		bool shouldRemember = false;
		if (NULL != toObject) {
			if( (((UDATA)fromObject) ^ ((UDATA)toObject)) >= _regionSize ) {
				MM_HeapRegionDescriptorVLHGC *toRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(toObject);
				/* PGC and Global Collect always rebuild RSCL. GMP only for some of them (overflowed for example) */
				shouldRemember = toRegion->getRememberedSetCardList()->isBeingRebuilt();
			}
		}
		return shouldRemember;
	}
	
	/**
	 * During compact, remember reference  from an object to an object (or appropriate structure (like card or region) the object belongs to)
	 * @param fromObject object (its slot) pointing from
	 * @param toObject object being pointed
	 */
	MMINLINE void 
	rememberReferenceForCompact(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
	{

		if (NULL != toObject) {
			/* We use the XOR and region mask of object pointers to determine if the objects share the same region.
			 * Getting the region descriptors for comparison is a much more expensive operation (and reserved for when
			 * we have identified the objects as being from different regions)
			 */
			if( (((UDATA)fromObject) ^ ((UDATA)toObject)) >= _regionSize ) {
				rememberReferenceForCompactInternal(env, fromObject, toObject);
			}
		}
	}

	/**
	 * During copy forward operations, remember reference  from an object to an object (or appropriate structure (like card or region) the object belongs to).
	 * @param fromObject object (its slot) pointing from
	 * @param toObject object being pointed
	 */
	MMINLINE void
	rememberReferenceForCopyForward(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject)
	{
		if (NULL != toObject) {
			/* We use the XOR and region mask of object pointers to determine if the objects share the same region.
			 * Getting the region descriptors for comparison is a much more expensive operation (and reserved for when
			 * we have identified the objects as being from different regions)
			 */
			if( (((UDATA)fromObject) ^ ((UDATA)toObject)) >= _regionSize ) {
				rememberReferenceForCopyForwardInternal(env, fromObject, toObject);
			}
		}
	}
	
	/**
	 * Gets the remembered set card which the object belongs to
	 * @param object the object who's remembered set card we wish to obtain
	 * @return the card which the object belongs to
	 */
	MMINLINE MM_RememberedSetCard
	getRememberedSetCardFromJ9Object(J9Object* object)
	{
		void *cardAddress = (void *)((UDATA)object & ~((UDATA)CARD_SIZE - 1));
		return convertRememberedSetCardFromHeapAddress(cardAddress);
	}

	/**
	 * Converts a heap address to the remembered set card that refers to it
	 * @param address the heap address which we wish to convert
	 * @return the card which refers to the address
	 */	
	MMINLINE MM_RememberedSetCard
	convertRememberedSetCardFromHeapAddress(void* address)
	{
		MM_RememberedSetCard card = 0;
#if defined(OMR_GC_COMPRESSED_POINTERS)
		card = (MM_RememberedSetCard)((UDATA)address >> CARD_SIZE_SHIFT);
#else
		card = (MM_RememberedSetCard) address;	
#endif
		return card;
	}

	/**
	 * Converts a remembered set card to the heap address it refers to
	 * @param card the card which we wish to convert
	 * @return the heap address which the card refers to
	 */	
	MMINLINE void *
	convertHeapAddressFromRememberedSetCard(MM_RememberedSetCard card)
	{
		void *address = NULL;
#if defined(OMR_GC_COMPRESSED_POINTERS)
		address = (void *)((UDATA)card << CARD_SIZE_SHIFT);
#else
		address = (void *) card;
#endif
		return address;
	}
	
	/**
	 * Converts an MM_RememberedSetCard to a corresponding Card *
	 * @param env[in] A GC thread
	 * @param card[in] The MM_RememberedSetCard we are converting
	 * @return the Card * corresponding to the input MM_RememberedSetCard
	 */
	MMINLINE Card *rememberedSetCardToCardAddr(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card)
	{
#if defined(OMR_GC_COMPRESSED_POINTERS)
		Card *virtualStart = _cardTable->getCardTableVirtualStart();
		return virtualStart + card;
#else
		return _cardTable->heapAddrToCardAddr(env, (void *) card);
#endif
	}
	
	/**
	 * Get the physical table descriptor associated with the MM_RememberedSetCard.
	 *
	 * @param card - the MM_RememberedSetCard we are interested in
	 * @return the region descriptor associated with the card
	 */
	MMINLINE MM_HeapRegionDescriptorVLHGC *physicalTableDescriptorForRememberedSetCard(MM_RememberedSetCard card) 
	{
		UDATA regionIndex = ((UDATA)card-_cardToRegionDisplacement) >> _cardToRegionShift;
		return physicalTableDescriptorForIndex(regionIndex);
	}

	/**
	 * Get the logical table descriptor associated with the MM_RememberedSetCard. Note that the region described
	 * could be part of a spanning region so return the headOfSPan.
	 *
	 * @param card - the MM_RememberedSetCard whose region we are trying to find
	 * @return the region descriptor for the specified card
	 */
	MMINLINE MM_HeapRegionDescriptorVLHGC *tableDescriptorForRememberedSetCard(MM_RememberedSetCard card) 
	{
		MM_HeapRegionDescriptorVLHGC *tableDescriptor = physicalTableDescriptorForRememberedSetCard(card);
		return (MM_HeapRegionDescriptorVLHGC *)tableDescriptor->_headOfSpan;
	}

	/**
	 * Checks if a reference is remembered
	 * @param fromObject object (its slot) pointing from
	 * @param toObject object being pointed
	 * @return true if the reference is remembered
	 */
	bool isReferenceRememberedForMark(MM_EnvironmentVLHGC* env, J9Object* fromObject, J9Object* toObject);

	/**
	 * Clears the inter-region references that point to toRegion
	 */
	void clearReferencesToRegion(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *toRegion);

	/**
	 * Clears references from Collection Set and from dirty cards
	 * (top level dispatcher)
	 * temporary call until optimized or not optimized version is chosen
	 */
	void clearFromRegionReferencesForMark(MM_EnvironmentVLHGC* env);

	/**
	 * Clears references from Compaction Set and from dirty cards
	 * (top level dispatcher)
	 * temporary call until optimized or not optimized version is chosen
	 */
	void clearFromRegionReferencesForCompact(MM_EnvironmentVLHGC* env);

	/**
	 * Clears references from Collection Set and from dirty cards
	 */
	void clearFromRegionReferencesForCopyForward(MM_EnvironmentVLHGC* env);

	/**
	 * Clear all RSCLs. Global collect will rebuild them from scratch.
	 */
	void prepareRegionsForGlobalCollect(MM_EnvironmentVLHGC *env, bool gmpInProgress);

	/**
	 * Clear RSCL for overflowed regions, and set them as being rebuilt.
	 * GMP will invoke this at the beginning of the cycle to rebuild RSCL for overflowed regions
	 */
	void prepareOverflowedRegionsForRebuilding(MM_EnvironmentVLHGC *env);

	/**
	 * Rebuilding of the RSCL is complete. Reset the rebuild flag for the regions, so that they can be used.
	 * GMP will invoke this at the end of the cycle.
	 */
	void setRegionsAsRebuildingComplete(MM_EnvironmentVLHGC *env);

	/** 
	 * Given a pointer to a buffer (its control block) return the region that owns this buffer
	 * @param cardBufferControlBlock buffer for which region is to be returned
	 * @return region that owns the buffer
	 */
	MM_HeapRegionDescriptorVLHGC *getBufferOwningRegion(MM_CardBufferControlBlock *cardBufferControlBlock);
	
	/**
	 * @return value of _shouldFlushBuffersForUnregisteredRegions flag
	 */
	bool getShouldFlushBuffersForDecommitedRegions() { return _shouldFlushBuffersForDecommitedRegions; }

	/**
	 * Set _shouldFlushBuffersForDecommitedRegions to true
	 * Invoked, when heap contraction occurred. Next GC should flush buffers for decommited regions.
	 */	
	void setShouldFlushBuffersForDecommitedRegions() { _shouldFlushBuffersForDecommitedRegions = true; }

	/**
	 * Propagate fromRegion information bottom-up (From RSM to RS Card Lists): remove remembered references from
	 * RS lists that do not exist in RSM.
	 * Not multi-threaded safe.
	 */

	static MM_InterRegionRememberedSet *newInstance(MM_EnvironmentVLHGC* env, MM_HeapRegionManager *heapRegionManager);
	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Initialize InterRegionRememberedSet
	 */
	bool initialize(MM_EnvironmentVLHGC* env);


	/**
	 * Initialize Thread local resources for RS CardLists for all threads
	 * @param env master GC thread
	 */
	void threadLocalInitialize(MM_EnvironmentVLHGC* env);

	/**
	 * Teardown InterRegionRememberedSet
	 */	
	virtual void tearDown(MM_EnvironmentBase* env);

	/**
	 * Remembered Set specific initialize during jvm initialize
	 * @param env[in] the current thread
	 */
	MMINLINE void initializeRememberedSetCardBucketPool(MM_EnvironmentVLHGC *env)
	{
		env->initializeGCThread();
		_rememberedSetCardBucketPool = env->_rememberedSetCardBucketPool;
	}
	
	/**
	 * set RememberedSetCardBucketPool for gcMasterThread or the thread acting as gcMasterThread
	 * @param env[in] the current thread
	 */
	MMINLINE void setRememberedSetCardBucketPoolForMasterThread(MM_EnvironmentVLHGC *env)
	{
		env->_rememberedSetCardBucketPool = &_rememberedSetCardBucketPool[0];
	}

	/*
	 * Friends
	 */
	friend class MM_RememberedSetCardBucket;
};

#endif /* INTERREGIONREMEMBEREDSET_HPP */
