/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/


#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include "AllocationContextBalanced.hpp"

#include "AllocateDescription.hpp"
#include "AllocationContextTarok.hpp"
#include "CardTable.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryPool.hpp"
#include "MemorySubSpaceTarok.hpp"
#include "ObjectAllocationInterface.hpp"


MM_AllocationContextBalanced *
MM_AllocationContextBalanced::newInstance(MM_EnvironmentBase *env, MM_MemorySubSpaceTarok *subspace, UDATA numaNode, UDATA allocationContextNumber)
{
	MM_AllocationContextBalanced *context = (MM_AllocationContextBalanced *)env->getForge()->allocate(sizeof(MM_AllocationContextBalanced), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (context) {
		new(context) MM_AllocationContextBalanced(subspace, numaNode, allocationContextNumber);
		if (!context->initialize(env)) {
			context->kill(env);
			context = NULL;   			
		}
	}
	return context;
}

/**
 * Initialization.
 */
bool
MM_AllocationContextBalanced::initialize(MM_EnvironmentBase *env)
{
	if (!MM_AllocationContext::initialize(env)) {
		return false;
	}

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	if (!_contextLock.initialize(env, &extensions->lnrlOptions, "MM_AllocationContextBalanced:_contextLock")) {
		return false;
	}
	if (!_freeListLock.initialize(env, &extensions->lnrlOptions, "MM_AllocationContextBalanced:_freeListLock")) {
		return false;
	}

	UDATA freeProcessorNodeCount = 0;
	J9MemoryNodeDetail const *freeProcessorNodes = extensions->_numaManager.getFreeProcessorPool(&freeProcessorNodeCount);
	/* our local cache needs +1 since we reserve a slot for each context to use */
	_freeProcessorNodeCount = freeProcessorNodeCount + 1;
	UDATA arraySizeInBytes = sizeof(UDATA) * _freeProcessorNodeCount;
	_freeProcessorNodes = (UDATA *)env->getForge()->allocate(arraySizeInBytes, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != _freeProcessorNodes) {
		memset(_freeProcessorNodes, 0x0, arraySizeInBytes);
		_freeProcessorNodes[0] = getNumaNode();
		for (UDATA i = 0; i < freeProcessorNodeCount; i++) {
			/* we save at i+1 since index 0 is reserved */
			_freeProcessorNodes[i+1] = freeProcessorNodes[i].j9NodeNumber;
		}
	} else {
		return false;
	}

	_cachedReplenishPoint = this;
	_heapRegionManager = MM_GCExtensions::getExtensions(env)->heapRegionManager;
	
	return true;
}

/**
 * Shut down.
 */
void
MM_AllocationContextBalanced::tearDown(MM_EnvironmentBase *env)
{
	Assert_MM_true(NULL == _allocationRegion);
	Assert_MM_true(NULL == _nonFullRegions.peekFirstRegion());
	Assert_MM_true(NULL == _discardRegionList.peekFirstRegion());

	_contextLock.tearDown();
	_freeListLock.tearDown();

	if (NULL != _freeProcessorNodes) {
		env->getForge()->free(_freeProcessorNodes);
		_freeProcessorNodes = NULL;
	}

	MM_AllocationContext::tearDown(env);
}

void
MM_AllocationContextBalanced::flushInternal(MM_EnvironmentBase *env)
{
	/* flush all the regions we own for active allocation */
	if (NULL != _allocationRegion){
		MM_MemoryPool *pool = _allocationRegion->getMemoryPool();
		Assert_MM_true(NULL != pool);
		UDATA allocatableBytes = pool->getActualFreeMemorySize();
		_freeMemorySize -= allocatableBytes;
		_flushedRegions.insertRegion(_allocationRegion);
		_allocationRegion = NULL;
		Trc_MM_AllocationContextBalanced_flushInternal_clearAllocationRegion(env->getLanguageVMThread(), this);
	}
	MM_HeapRegionDescriptorVLHGC *walk = _nonFullRegions.peekFirstRegion();
	while (NULL != walk) {
		Assert_MM_true(this == walk->_allocateData._owningContext);
		MM_HeapRegionDescriptorVLHGC *next = _nonFullRegions.peekRegionAfter(walk);
		_nonFullRegions.removeRegion(walk);
		MM_MemoryPool *pool = walk->getMemoryPool();
		Assert_MM_true(NULL != pool);
		UDATA allocatableBytes = pool->getActualFreeMemorySize();
		_freeMemorySize -= allocatableBytes;
		_flushedRegions.insertRegion(walk);
		walk = next;
	}
	/* flush all the regions we own which were no longer candidates for allocation */
	walk = _discardRegionList.peekFirstRegion();
	while (NULL != walk) {
		Assert_MM_true(this == walk->_allocateData._owningContext);
		MM_HeapRegionDescriptorVLHGC *next = _discardRegionList.peekRegionAfter(walk);
		_discardRegionList.removeRegion(walk);
		MM_MemoryPool *pool = walk->getMemoryPool();
		Assert_MM_true(NULL != pool);
		pool->recalculateMemoryPoolStatistics(env);
		_flushedRegions.insertRegion(walk);
		walk = next;
	}
	_cachedReplenishPoint = this;
	Assert_MM_true(0 == _freeMemorySize);
}

void
MM_AllocationContextBalanced::flush(MM_EnvironmentBase *env)
{
	flushInternal(env);
}

void
MM_AllocationContextBalanced::flushForShutdown(MM_EnvironmentBase *env)
{
	flushInternal(env);
}


void *
MM_AllocationContextBalanced::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_ObjectAllocationInterface *objectAllocationInterface, bool shouldCollectOnFailure)
{
	void *result = NULL;
	lockCommon();
	result = lockedAllocateTLH(env, allocateDescription, objectAllocationInterface);
	/* if we failed, try to replenish */
	if (NULL == result) {
		result = lockedReplenishAndAllocate(env, objectAllocationInterface, allocateDescription, MM_MemorySubSpace::ALLOCATION_TYPE_TLH);
	}
	unlockCommon();
	/* if that still fails, try to invoke the collector */
	if (shouldCollectOnFailure && (NULL == result)) {
		result = _subspace->replenishAllocationContextFailed(env, _subspace, this, objectAllocationInterface, allocateDescription, MM_MemorySubSpace::ALLOCATION_TYPE_TLH);
	}
	return result;
}
void *
MM_AllocationContextBalanced::lockedAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_ObjectAllocationInterface *objectAllocationInterface)
{
	void *result = NULL;
	/* first, try allocating the TLH in our _allocationRegion (fast-path) */
	if (NULL != _allocationRegion) {
		MM_MemoryPool *memoryPool = _allocationRegion->getMemoryPool();
		Assert_MM_true(NULL != memoryPool);
		UDATA spaceBefore = memoryPool->getActualFreeMemorySize();
		result = objectAllocationInterface->allocateTLH(env, allocateDescription, _subspace, memoryPool);
		UDATA spaceAfter = memoryPool->getActualFreeMemorySize();
		if (NULL == result) {
			/* this region isn't useful so remove it from our list for consideration and add it to our discard list */
			Assert_MM_true(spaceAfter < memoryPool->getMinimumFreeEntrySize());
			Assert_MM_true(spaceBefore == spaceAfter);
			_freeMemorySize -= spaceBefore;
			_discardRegionList.insertRegion(_allocationRegion);
			_allocationRegion = NULL;
			Trc_MM_AllocationContextBalanced_lockedAllocateTLH_clearAllocationRegion(env->getLanguageVMThread(), this);
		} else {
			Assert_MM_true(spaceBefore > spaceAfter);
			_freeMemorySize -= (spaceBefore - spaceAfter);
		}
	}
	/* if we couldn't satisfy the allocate, go to the non-full region list (slow-path) before failing over into replenishment */
	if (NULL == result) {
		/* scan through our regions which are still active for allocation and attempt the TLH allocation in each.  Any which are too full or fragmented to satisfy a TLH allocation must be moved to the "discard" list so we won't consider them for allocation until after the next collection */
		MM_HeapRegionDescriptorVLHGC *region = _nonFullRegions.peekFirstRegion();
		while ((NULL == result) && (NULL != region)) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			UDATA spaceBefore = memoryPool->getActualFreeMemorySize();
			result = objectAllocationInterface->allocateTLH(env, allocateDescription, _subspace, memoryPool);
			UDATA spaceAfter = memoryPool->getActualFreeMemorySize();
			MM_HeapRegionDescriptorVLHGC *next = _nonFullRegions.peekRegionAfter(region);
			/* remove this region from the list since we are either discarding it or re-promoting it to the fast-path */
			_nonFullRegions.removeRegion(region);
			if (NULL == result) {
				/* this region isn't useful so remove it from our list for consideration and add it to our discard list */
				Assert_MM_true(spaceAfter < memoryPool->getMinimumFreeEntrySize());
				Assert_MM_true(spaceBefore == spaceAfter);
				_freeMemorySize -= spaceBefore;
				_discardRegionList.insertRegion(region);
			} else {
				Assert_MM_true(spaceBefore > spaceAfter);
				_freeMemorySize -= (spaceBefore - spaceAfter);
				/* we succeeded so this region is a good choice for future fast-path allocations */
				Assert_MM_true(NULL == _allocationRegion);
				_allocationRegion = region;
				Trc_MM_AllocationContextBalanced_lockedAllocateTLH_setAllocationRegion(env->getLanguageVMThread(), this, region);
			}
			region = next;
		}
	}
	return result;
}
void *
MM_AllocationContextBalanced::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure)
{
	void *result = NULL;
	lockCommon();
	result = lockedAllocateObject(env, allocateDescription);
	/* if we failed, try to replenish */
	if (NULL == result) {
		result = lockedReplenishAndAllocate(env, NULL, allocateDescription, MM_MemorySubSpace::ALLOCATION_TYPE_OBJECT);
	}
	unlockCommon();
	/* if that still fails, try to invoke the collector */
	if (shouldCollectOnFailure && (NULL == result)) {
		result = _subspace->replenishAllocationContextFailed(env, _subspace, this, NULL, allocateDescription, MM_MemorySubSpace::ALLOCATION_TYPE_OBJECT);
	}
	if (NULL != result) {
		allocateDescription->setObjectFlags(_subspace->getObjectFlags());
		allocateDescription->setMemorySubSpace(_subspace);
	}
	return result;
}
void *
MM_AllocationContextBalanced::lockedAllocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription)
{
	Assert_MM_true(allocateDescription->getContiguousBytes() <= _heapRegionManager->getRegionSize());

	void *result = NULL;
	/* first, try allocating the object in our _allocationRegion (fast-path) */
	if (NULL != _allocationRegion) {
		MM_MemoryPool *memoryPool = _allocationRegion->getMemoryPool();
		Assert_MM_true(NULL != memoryPool);
		UDATA spaceBefore = memoryPool->getActualFreeMemorySize();
		result = memoryPool->allocateObject(env, allocateDescription);
		UDATA spaceAfter = memoryPool->getActualFreeMemorySize();
		if (NULL == result) {
			Assert_MM_true(spaceBefore == spaceAfter);
			/* if we failed the allocate, move the region into the non-full list since a TLH allocate can consume any space remaining, prior to discarding */
			_nonFullRegions.insertRegion(_allocationRegion);
			_allocationRegion = NULL;
			Trc_MM_AllocationContextBalanced_lockedAllocateObject_clearAllocationRegion(env->getLanguageVMThread(), this);
		} else {
			Assert_MM_true(spaceBefore > spaceAfter);
			_freeMemorySize -= (spaceBefore - spaceAfter);
		}
	}
	/* if we couldn't satisfy the allocate, go to the non-full region list (slow-path) before failing over into replenishment */
	if (NULL == result) {
		Assert_MM_true(NULL == _allocationRegion);
		/* scan through our active region list and attempt the allocation in each.  Failing to satisfy a one-off object allocation, such as this, will not force a region into the discard list, however */
		MM_HeapRegionDescriptorVLHGC *region = _nonFullRegions.peekFirstRegion();
		while ((NULL == result) && (NULL != region)) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			UDATA spaceBefore = memoryPool->getActualFreeMemorySize();
		result = memoryPool->allocateObject(env, allocateDescription);
			if (NULL != result) {
				UDATA spaceAfter = memoryPool->getActualFreeMemorySize();
				Assert_MM_true(spaceBefore > spaceAfter);
				_freeMemorySize -= (spaceBefore - spaceAfter);
			}
			region = _nonFullRegions.peekRegionAfter(region);
		}
	} else {
		Assert_MM_true(NULL != _allocationRegion);
	}
	return result;
}

void *
MM_AllocationContextBalanced::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure)
{
	/* this AC implementation doesn't try to cache leaf regions so just call into the subspace to hand us a region and then we will use it in lockedAllocateArrayletLeaf */
	void *result = NULL;
	lockCommon();
	result = lockedReplenishAndAllocate(env, NULL, allocateDescription, MM_MemorySubSpace::ALLOCATION_TYPE_LEAF);
	unlockCommon();
	/* if that fails, try to invoke the collector */
	if (shouldCollectOnFailure && (NULL == result)) {
		result = _subspace->replenishAllocationContextFailed(env, _subspace, this, NULL, allocateDescription, MM_MemorySubSpace::ALLOCATION_TYPE_LEAF);
	}
	if (NULL != result) {
		/* for off-heap case zeroing leaf is unecessary */
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		GC_ArrayObjectModel *indexableObjectModel = &extensions->indexableObjectModel;
//		Assert_MM_false(indexableObjectModel->isVirtualLargeObjectHeapEnabled());
		if (!indexableObjectModel->isVirtualLargeObjectHeapEnabled()) {
			/* zero the leaf here since we are not under any of the context or exclusive locks */
			OMRZeroMemory(result, _heapRegionManager->getRegionSize());
		}
	}
	return result;
}

void *
MM_AllocationContextBalanced::lockedAllocateArrayletLeaf(MM_EnvironmentBase *envBase, MM_AllocateDescription *allocateDescription, MM_HeapRegionDescriptorVLHGC *freeRegionForArrayletLeaf)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);

	Assert_MM_true(NULL != freeRegionForArrayletLeaf);
	Assert_MM_true(MM_HeapRegionDescriptor::FREE == freeRegionForArrayletLeaf->getRegionType());

	J9IndexableObject *spine = allocateDescription->getSpine();
	Assert_MM_true(NULL != spine);

	/* cache the allocate data pointer since we need to use it in several operations */
	MM_HeapRegionDataForAllocate *leafAllocateData = &(freeRegionForArrayletLeaf->_allocateData);
	/* ask the region to become a leaf type */
	leafAllocateData->taskAsArrayletLeaf(env);
	/* look up the spine region since we need to add this region to its leaf list */
	MM_HeapRegionDescriptorVLHGC *spineRegion = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(spine);
	/* the leaf requires a pointer back to the spine object so that it can verify its liveness elsewhere in the collector */
	leafAllocateData->setSpine(spine);
	freeRegionForArrayletLeaf->resetAge(env, (U_64)_subspace->getBytesRemainingBeforeTaxation());
	/* add the leaf to the spine region's leaf list */
	/* We own the lock on the spine region's context when this call is made so we can safely manipulate this list.
	 * An exceptional scenario: A thread allocates a spine (and possibly a few arraylets), but does not complete the allocation. A global GC (or a series of regular PGCs) occurs
	 * that age out regions to max age. The spine moves into a common context. Now, we successfully resume the leaf allocation, but the common lock that
	 * we already hold is not sufficient any more. We need to additionally acquire common context' common lock, since multiple spines from different ACs could have come into this state,
	 * and worse multiple spines originally allocated from different ACs may end up in a single common context region.
	 */

	MM_AllocationContextTarok *spineContext = spineRegion->_allocateData._owningContext;
	if (this != spineContext) {
		Assert_MM_true(env->getCommonAllocationContext() == spineContext);
		/* The common allocation context is always an instance of AllocationContextBalanced */
		((MM_AllocationContextBalanced *)spineContext)->lockCommon();
	}

	leafAllocateData->addToArrayletLeafList(spineRegion);
	
	if (this != spineContext) {
		/* The common allocation context is always an instance of AllocationContextBalanced */
		((MM_AllocationContextBalanced *)spineContext)->unlockCommon();
	}

	/* store the base address of the leaf for the memset and the return */
	return freeRegionForArrayletLeaf->getLowAddress();
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::collectorAcquireRegion(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	region = internalCollectorAcquireRegion(env);
	return region;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::internalCollectorAcquireRegion(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	lockCommon();
	Assert_MM_true(NULL == _nonFullRegions.peekFirstRegion());
	do {
		Assert_MM_true(NULL == _allocationRegion);
		region = internalReplenishActiveRegion(env, false);
		/* If failed to allocate region, attempt to expand.
		 * If we successfully expanded, we are not yet guaranteed that retry on replenish will succeed,
		 * since another thread from another AC may steal the expanded regions. Thus we keep expanding
		 * until we succeed to replenish or no more expansion is possible.
		 * This may not be the same AC receiving the expanded region, so this problem exists even without
		 * stealing. 
		 */
	} while ((NULL == region) && (0 != _subspace->collectorExpand(env)));

	if (NULL != region) {
		Assert_MM_true(NULL == _nonFullRegions.peekFirstRegion());
		Assert_MM_true(region == _allocationRegion);
		UDATA regionSize = _heapRegionManager->getRegionSize();
		_freeMemorySize -= regionSize;

		_allocationRegion = NULL;
		Trc_MM_AllocationContextBalanced_internalCollectorAcquireRegion_clearAllocationRegion(env->getLanguageVMThread(), this);
		Assert_MM_true(NULL != region->getMemoryPool());
		_flushedRegions.insertRegion(region);
	}
	unlockCommon();

	return region;
}

void *
MM_AllocationContextBalanced::allocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType)
{
	void *result = NULL;
	switch(allocationType) {
	case MM_MemorySubSpace::ALLOCATION_TYPE_TLH:
		result = allocateTLH(env, allocateDescription, objectAllocationInterface, false);
		break;
	case MM_MemorySubSpace::ALLOCATION_TYPE_OBJECT:
		result = allocateObject(env, allocateDescription, false);
		break;
	case MM_MemorySubSpace::ALLOCATION_TYPE_LEAF:
		result = allocateArrayletLeaf(env, allocateDescription, false);
		break;
	default:
		Assert_MM_unreachable();
		break;
	}
	return result;
}

void *
MM_AllocationContextBalanced::lockedAllocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType)
{
	void *result = NULL;
	switch (allocationType) {
	case MM_MemorySubSpace::ALLOCATION_TYPE_OBJECT:
		result = lockedAllocateObject(env, allocateDescription);
		break;
	case MM_MemorySubSpace::ALLOCATION_TYPE_TLH:
		result = lockedAllocateTLH(env, allocateDescription, objectAllocationInterface);
		break;
	case MM_MemorySubSpace::ALLOCATION_TYPE_LEAF:
		/* callers allocating an arraylet leaf should call lockedAllocateArrayletLeaf() directly */
		Assert_MM_unreachable();
		break;
	default:
		Assert_MM_unreachable();
	}
	return result;
}

void
MM_AllocationContextBalanced::setNextSibling(MM_AllocationContextBalanced *sibling)
{
	Assert_MM_true(NULL == _nextSibling);
	_nextSibling = sibling;
	Assert_MM_true(NULL != _nextSibling);
}

void
MM_AllocationContextBalanced::setStealingCousin(MM_AllocationContextBalanced *cousin)
{
	Assert_MM_true(NULL == _stealingCousin);
	_stealingCousin = cousin;
	_nextToSteal = cousin;
	Assert_MM_true(NULL != _stealingCousin);
}

void
MM_AllocationContextBalanced::recycleRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
{
	MM_HeapRegionDataForAllocate *allocateData = &region->_allocateData;
	MM_AllocationContextTarok *owningContext = allocateData->_owningContext;
	MM_AllocationContextTarok *originalOwningContext = allocateData->_originalOwningContext;
	Assert_MM_true((this == owningContext) || (this == originalOwningContext));
	Assert_MM_true(region->getNumaNode() == getNumaNode());
	if (NULL == originalOwningContext) {
		originalOwningContext = owningContext;
	}
	Assert_MM_true(this == originalOwningContext);
	
	/* the region is being returned to us, set the fields appropriately before returning it to the list  */
	allocateData->_originalOwningContext = NULL;
	allocateData->_owningContext = this;

	switch (region->getRegionType()) {
		case MM_HeapRegionDescriptor::ADDRESS_ORDERED:
		case MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED:
		{
			owningContext->removeRegionFromFlushedList(region);
			allocateData->taskAsIdlePool(env);
			_freeListLock.acquire();
			_idleMPRegions.insertRegion(region);
			_freeListLock.release();
			MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
			if (extensions->tarokEnableExpensiveAssertions) {
				void *low = region->getLowAddress();
				void *high = region->getHighAddress();
				MM_CardTable *cardTable = extensions->cardTable;
				Card *card = cardTable->heapAddrToCardAddr(env, low);
				Card *toCard = cardTable->heapAddrToCardAddr(env, high);
				
				while (card < toCard) {
					Assert_MM_true(CARD_CLEAN == *card);
					card += 1;
				}
			}
		}
			break;
		case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
			Assert_MM_true(NULL == allocateData->getNextArrayletLeafRegion());
			Assert_MM_true(NULL == allocateData->getSpine());

			if (MM_GCExtensions::getExtensions(env)->tarokDebugEnabled) {
				/* poison the unused region so we can identify it in a crash (to be removed when 1953 is stable) */
				memset(region->getLowAddress(), 0x0F, region->getSize());
			}
			allocateData->taskAsFreePool(env);
			/* now, return the region to our free list */
			addRegionToFreeList(env, region);
			break;
		case MM_HeapRegionDescriptor::FREE:
			/* calling recycle on a free region implies an incorrect assumption in the caller */
			Assert_MM_unreachable();
			break;
		default:
			Assert_MM_unreachable();
	}
}

void
MM_AllocationContextBalanced::tearDownRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region)
{
	MM_MemoryPoolAddressOrderedList *memoryPool = (MM_MemoryPoolAddressOrderedList *)region->getMemoryPool();
	if (NULL != memoryPool) {
		memoryPool->tearDown(env);
		region->setMemoryPool(NULL);
	}
}

void
MM_AllocationContextBalanced::addRegionToFreeList(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region)
{
	Assert_MM_true(MM_HeapRegionDescriptor::FREE == region->getRegionType());
	Assert_MM_true(getNumaNode() == region->getNumaNode());
	Assert_MM_true(NULL == region->_allocateData._originalOwningContext);
	_freeListLock.acquire();
	_freeRegions.insertRegion(region);
	_freeListLock.release();
}

void
MM_AllocationContextBalanced::resetLargestFreeEntry()
{
	lockCommon();
	if (NULL != _allocationRegion) {
		_allocationRegion->getMemoryPool()->resetLargestFreeEntry();
	}
	MM_HeapRegionDescriptorVLHGC *region = _nonFullRegions.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->resetLargestFreeEntry();
		region = _nonFullRegions.peekRegionAfter(region);
	}
	region = _discardRegionList.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->resetLargestFreeEntry();
		region = _discardRegionList.peekRegionAfter(region);
	}
	region = _flushedRegions.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->resetLargestFreeEntry();
		region = _flushedRegions.peekRegionAfter(region);
	}
	unlockCommon();
}

UDATA
MM_AllocationContextBalanced::getLargestFreeEntry()
{
	UDATA largest = 0;
	
	lockCommon();
	/* if we have a free region, largest free entry is the region size */
	MM_HeapRegionDescriptorVLHGC *free = _idleMPRegions.peekFirstRegion();
	if (NULL == free) {
		free = _freeRegions.peekFirstRegion();
	}
	if (NULL != free) {
		largest = free->getSize();
	} else {
		if (NULL != _allocationRegion) {
			MM_MemoryPool *memoryPool = _allocationRegion->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			UDATA candidate = memoryPool->getLargestFreeEntry();
			largest = candidate;
		}
		MM_HeapRegionDescriptorVLHGC *region = _nonFullRegions.peekFirstRegion();
		while (NULL != region) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			UDATA candidate = memoryPool->getLargestFreeEntry();
			largest = OMR_MAX(largest, candidate);
			region = _nonFullRegions.peekRegionAfter(region);
		}
		region = _flushedRegions.peekFirstRegion();
		while (NULL != region) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			UDATA candidate = memoryPool->getLargestFreeEntry();
			largest = OMR_MAX(largest, candidate);
			region = _flushedRegions.peekRegionAfter(region);
		}
	}
	unlockCommon();
	
	return largest;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::acquireMPRegionFromHeap(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_AllocationContextTarok *requestingContext)
{
	MM_HeapRegionDescriptorVLHGC *region = acquireMPRegionFromNode(env, subspace, requestingContext);

	/* _nextToSteal will be this if NUMA is not enabled */
	if ((NULL == region) && (_nextToSteal != this)) {
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		Assert_MM_true(0 != extensions->_numaManager.getAffinityLeaderCount());
		/* we didn't get any memory yet we are in a NUMA system so we should steal from a foreign node */
		MM_AllocationContextBalanced *firstTheftAttempt = _nextToSteal;
		do {
			region = _nextToSteal->acquireMPRegionFromNode(env, subspace, requestingContext);
			if (NULL != region) {
				/* make sure that we record the original owner so that the region can be identified as foreign */
				Assert_MM_true(NULL == region->_allocateData._originalOwningContext);
				region->_allocateData._originalOwningContext = _nextToSteal;
			}
			/* advance to the next node whether we succeeded or not since we want to distribute our "theft" as evenly as possible */
			_nextToSteal = _nextToSteal->getStealingCousin();
			if (this == _nextToSteal) {
				/* never try to steal from ourselves since that wouldn't be possible and the code interprets this case as a uniform system */
				_nextToSteal = _nextToSteal->getStealingCousin();
			}
		} while ((NULL == region) && (firstTheftAttempt != _nextToSteal));
	}

	return region;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::acquireFreeRegionFromHeap(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorVLHGC *region = acquireFreeRegionFromNode(env);

	/* _nextToSteal will be this if NUMA is not enabled */
	if ((NULL == region) && (_nextToSteal != this)) {
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		Assert_MM_true(0 != extensions->_numaManager.getAffinityLeaderCount());
		/* we didn't get any memory yet we are in a NUMA system so we should steal from a foreign node */
		MM_AllocationContextBalanced *firstTheftAttempt = _nextToSteal;
		do {
			region = _nextToSteal->acquireFreeRegionFromNode(env);
			if (NULL != region) {
				region->_allocateData._originalOwningContext = _nextToSteal;
			}
			/* advance to the next node whether we succeeded or not since we want to distribute our "theft" as evenly as possible */
			_nextToSteal = _nextToSteal->getStealingCousin();
			if (this == _nextToSteal) {
				/* never try to steal from ourselves since that wouldn't be possible and the code interprets this case as a uniform system */
				_nextToSteal = _nextToSteal->getStealingCousin();
			}
		} while ((NULL == region) && (firstTheftAttempt != _nextToSteal));
	}

	return region;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::acquireMPRegionFromNode(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocationContextTarok *requestingContext)
{
	Trc_MM_AllocationContextBalanced_acquireMPBPRegionFromNode_Entry(env->getLanguageVMThread(), this, requestingContext);
	/* this can only be called on the context itself or through stealing cousin relationships */
	Assert_MM_true((this == requestingContext) || (getNumaNode() != requestingContext->getNumaNode()));

	MM_HeapRegionDescriptorVLHGC *region = _cachedReplenishPoint->acquireMPRegionFromContext(env, subSpace, requestingContext);
	MM_AllocationContextBalanced *targetContext = _cachedReplenishPoint->getNextSibling();
	while ((NULL == region) && (targetContext != this)) {
		region = targetContext->acquireMPRegionFromContext(env, subSpace, requestingContext);
		if (NULL != region) {
			_cachedReplenishPoint = targetContext;
		}
		targetContext = targetContext->getNextSibling();
	}
	if (NULL != region) {
		/* Regions made available for allocation are identified by their region type (ADDRESS_ORDERED, as opposed to ADDRESS_ORDERED_MARKED) */
		Assert_MM_true(MM_HeapRegionDescriptor::ADDRESS_ORDERED == region->getRegionType());
		Assert_MM_true(requestingContext == region->_allocateData._owningContext);
		Assert_MM_true(getNumaNode() == region->getNumaNode());
	}
	Trc_MM_AllocationContextBalanced_acquireMPBPRegionFromNode_Exit(env->getLanguageVMThread(), region);
	return region;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::acquireMPRegionFromContext(MM_EnvironmentBase *envBase, MM_MemorySubSpace *subSpace, MM_AllocationContextTarok *requestingContext)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	
	_freeListLock.acquire();
	MM_HeapRegionDescriptorVLHGC *region= _idleMPRegions.peekFirstRegion();
	if (NULL != region) {
		_idleMPRegions.removeRegion(region);
	} else {
		region = _freeRegions.peekFirstRegion();
		if (NULL != region) {
			_freeRegions.removeRegion(region);
		}
	}
	_freeListLock.release();
	if (NULL != region) {
		if (MM_HeapRegionDescriptor::FREE == region->getRegionType()) {
			if (region->_allocateData.taskAsMemoryPool(env, requestingContext)) {
				/* this is a new region. Initialize it for the given pool */
				region->resetAge(env, (U_64)_subspace->getBytesRemainingBeforeTaxation());
				MM_MemoryPool *mpaol = region->getMemoryPool();
				mpaol->setSubSpace(subSpace);
				mpaol->expandWithRange(env, region->getSize(), region->getLowAddress(), region->getHighAddress(), false);
				mpaol->recalculateMemoryPoolStatistics(env);
			} else {
				/* something went wrong so put the region back in the free list and return NULL (even though the region might have been found in another context, where we put it back is largely arbitrary and this path should never actually be taken) */
				addRegionToFreeList(env, region);
				region = NULL;
			}
		} else if (MM_HeapRegionDescriptor::ADDRESS_ORDERED_IDLE == region->getRegionType()) {
			bool success = region->_allocateData.taskAsMemoryPool(env, requestingContext);
			/* we can't fail to convert an IDLE region to an active one */
			Assert_MM_true(success);
			/* also add this region into our owned region list */
			region->resetAge(env, (U_64)_subspace->getBytesRemainingBeforeTaxation());
			region->_allocateData._owningContext = requestingContext;
			MM_MemoryPool *pool = region->getMemoryPool();
			Assert_MM_true(subSpace == pool->getSubSpace());
			pool->rebuildFreeListInRegion(env, region, NULL);
			pool->recalculateMemoryPoolStatistics(env);
			Assert_MM_true(pool->getLargestFreeEntry() == region->getSize());
		} else {
			Assert_MM_unreachable();
		}
		if (NULL != region) {
			Assert_MM_true(getNumaNode() == region->getNumaNode());
			Assert_MM_true(NULL == region->_allocateData._originalOwningContext);
		}
	}
	return region;
	
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::acquireFreeRegionFromNode(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorVLHGC *region = _cachedReplenishPoint->acquireFreeRegionFromContext(env);
	MM_AllocationContextBalanced *targetContext = _cachedReplenishPoint->getNextSibling();
	while ((NULL == region) && (targetContext != this)) {
		region = targetContext->acquireFreeRegionFromContext(env);
		if (NULL != region) {
			_cachedReplenishPoint = targetContext;
		}
		targetContext = targetContext->getNextSibling();
	}
	if (NULL != region) {
		Assert_MM_true(getNumaNode() == region->getNumaNode());
	}
	return region;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::acquireFreeRegionFromContext(MM_EnvironmentBase *env)
{
	_freeListLock.acquire();
	MM_HeapRegionDescriptorVLHGC *region = _freeRegions.peekFirstRegion();
	if (NULL != region) {
		_freeRegions.removeRegion(region);
	} else {
		region = _idleMPRegions.peekFirstRegion();
		if (NULL != region) {
			_idleMPRegions.removeRegion(region);
			region->_allocateData.taskAsFreePool(env);
		}
	}
	_freeListLock.release();
	if (NULL != region) {
		Assert_MM_true(getNumaNode() == region->getNumaNode());
	}
	return region;
	
}

void
MM_AllocationContextBalanced::lockCommon()
{
	_contextLock.acquire();
}
void
MM_AllocationContextBalanced::unlockCommon()
{
	_contextLock.release();
}

UDATA
MM_AllocationContextBalanced::getFreeMemorySize()
{
	UDATA regionSize = _heapRegionManager->getRegionSize();
	UDATA freeRegions = getFreeRegionCount();
	return _freeMemorySize + (freeRegions * regionSize);
}

UDATA
MM_AllocationContextBalanced::getFreeRegionCount()
{
	return _idleMPRegions.listSize() + _freeRegions.listSize();
}

void
MM_AllocationContextBalanced::migrateRegionToAllocationContext(MM_HeapRegionDescriptorVLHGC *region, MM_AllocationContextTarok *newOwner)
{
	/*
	 * This is the point where we reconcile the data held in the region descriptors and the contexts.  Prior to this, compaction planning may have decided
	 * to migrate a region into a new context but couldn't update the contexts' meta-structures due to performance concerns around the locks required to
	 * manipulate the lists.  After this call returns, region's meta-data will be consistent with its owning context.
	 */
	if (region->containsObjects()) {
		Assert_MM_true(NULL != region->getMemoryPool());
		_flushedRegions.removeRegion(region);
		Assert_MM_true(region->_allocateData._owningContext == newOwner);
		newOwner->acceptMigratingRegion(region);
	} else if (region->isArrayletLeaf()) {
		/* nothing to do */
	} else {
		Assert_MM_unreachable();
	}
	/* we can only do direct migration between contexts with the same NUMA properties, at this time (note that 0 is special since it can accept memory from any node) */
	Assert_MM_true((region->getNumaNode() == newOwner->getNumaNode()) || (0 == newOwner->getNumaNode()));
}

void
MM_AllocationContextBalanced::acceptMigratingRegion(MM_HeapRegionDescriptorVLHGC *region)
{
	_flushedRegions.insertRegion(region);
}

void
MM_AllocationContextBalanced::resetHeapStatistics(bool globalCollect)
{
	lockCommon();
	if (NULL != _allocationRegion) {
		_allocationRegion->getMemoryPool()->resetHeapStatistics(globalCollect);
	}
	MM_HeapRegionDescriptorVLHGC *region = _nonFullRegions.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->resetHeapStatistics(globalCollect);
		region = _nonFullRegions.peekRegionAfter(region);
	}
	region = _discardRegionList.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->resetHeapStatistics(globalCollect);
		region = _discardRegionList.peekRegionAfter(region);
	}
	region = _flushedRegions.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->resetHeapStatistics(globalCollect);
		region = _flushedRegions.peekRegionAfter(region);
	}
	unlockCommon();
}

void
MM_AllocationContextBalanced::mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType)
{
	lockCommon();
	if (NULL != _allocationRegion) {
		_allocationRegion->getMemoryPool()->mergeHeapStats(heapStats, true);
	}
	MM_HeapRegionDescriptorVLHGC *region = _nonFullRegions.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->mergeHeapStats(heapStats, true);
		region = _nonFullRegions.peekRegionAfter(region);
	}
	region = _discardRegionList.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->mergeHeapStats(heapStats, true);
		region = _discardRegionList.peekRegionAfter(region);
	}
	region = _flushedRegions.peekFirstRegion();
	while (NULL != region) {
		region->getMemoryPool()->mergeHeapStats(heapStats, true);
		region = _flushedRegions.peekRegionAfter(region);
	}
	unlockCommon();
}

void *
MM_AllocationContextBalanced::lockedReplenishAndAllocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType)
{
	void * result = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	UDATA regionSize = extensions->regionSize;
	
	UDATA contiguousAllocationSize;
	if (MM_MemorySubSpace::ALLOCATION_TYPE_LEAF == allocationType) {
		contiguousAllocationSize = regionSize;
	} else {
		contiguousAllocationSize = allocateDescription->getContiguousBytes();
	}

	Trc_MM_AllocationContextBalanced_lockedReplenishAndAllocate_Entry(env->getLanguageVMThread(), regionSize, contiguousAllocationSize);

	if (MM_MemorySubSpace::ALLOCATION_TYPE_LEAF == allocationType) {
		if (_subspace->consumeFromTaxationThreshold(env, regionSize)) {
			/* acquire a free region */
			MM_HeapRegionDescriptorVLHGC *leafRegion = acquireFreeRegionFromHeap(env);
			if (NULL != leafRegion) {
				result = lockedAllocateArrayletLeaf(env, allocateDescription, leafRegion);
				leafRegion->_allocateData._owningContext = this;
				Assert_MM_true(leafRegion->getLowAddress() == result);
				Trc_MM_AllocationContextBalanced_lockedReplenishAndAllocate_acquiredFreeRegion(env->getLanguageVMThread(), regionSize);
			}
		}
	} else {
		Assert_MM_true(NULL == _allocationRegion);
		MM_HeapRegionDescriptorVLHGC *newRegion = internalReplenishActiveRegion(env, true);
		if (NULL != newRegion) {
			/* the new region must be our current allocation region and it must be completely empty */
			Assert_MM_true(_allocationRegion == newRegion);
			Assert_MM_true(newRegion->getMemoryPool()->getActualFreeMemorySize() == newRegion->getSize());

			result = lockedAllocate(env, objectAllocationInterface, allocateDescription, allocationType);
			Assert_MM_true(NULL != result);
		}
	}
	
	if (NULL != result) {
		Trc_MM_AllocationContextBalanced_lockedReplenishAndAllocate_Success(env->getLanguageVMThread());
	} else {
		Trc_MM_AllocationContextBalanced_lockedReplenishAndAllocate_Failure(env->getLanguageVMThread());
	}
	
	return result;
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::internalReplenishActiveRegion(MM_EnvironmentBase *env, bool payTax)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	UDATA regionSize = extensions->regionSize;
	MM_HeapRegionDescriptorVLHGC *newRegion = NULL;

	Assert_MM_true(NULL == _allocationRegion);
	
	if (!payTax || _subspace->consumeFromTaxationThreshold(env, regionSize)) {
		newRegion = acquireMPRegionFromHeap(env, _subspace, this);
		if (NULL != newRegion) {
			Trc_MM_AllocationContextBalanced_internalReplenishActiveRegion_convertedFreeRegion(env->getLanguageVMThread(), newRegion, regionSize);
			_allocationRegion = newRegion;
			Trc_MM_AllocationContextBalanced_internalReplenishActiveRegion_setAllocationRegion(env->getLanguageVMThread(), this, newRegion);
			_freeMemorySize += newRegion->getMemoryPool()->getActualFreeMemorySize();
		}
	}
	
	Assert_MM_true(newRegion == _allocationRegion);
	
	return newRegion;
}

void
MM_AllocationContextBalanced::accountForRegionLocation(MM_HeapRegionDescriptorVLHGC *region, UDATA *localCount, UDATA *foreignCount)
{
	Assert_MM_true((NULL == region->_allocateData._owningContext) || (this == region->_allocateData._owningContext));
	if (NULL == region->_allocateData._originalOwningContext) {
		/* local */
		*localCount += 1;
		Assert_MM_true(region->getNumaNode() == getNumaNode());
	} else {
		/* foreign (stolen) */
		*foreignCount += 1;
		Assert_MM_true(region->getNumaNode() != getNumaNode());
	}
}

void
MM_AllocationContextBalanced::countRegionsInList(MM_RegionListTarok *list, UDATA *localCount, UDATA *foreignCount)
{
	MM_HeapRegionDescriptorVLHGC *region = list->peekFirstRegion();
	while (NULL != region) {
		accountForRegionLocation(region, localCount, foreignCount);
		region = list->peekRegionAfter(region);
	}
}
void
MM_AllocationContextBalanced::getRegionCount(UDATA *localCount, UDATA *foreignCount)
{
	if (NULL != _allocationRegion) {
		accountForRegionLocation(_allocationRegion, localCount, foreignCount);
	}
	countRegionsInList(&_nonFullRegions, localCount, foreignCount);
	countRegionsInList(&_discardRegionList, localCount, foreignCount);
	countRegionsInList(&_flushedRegions, localCount, foreignCount);
	countRegionsInList(&_freeRegions, localCount, foreignCount);
	countRegionsInList(&_idleMPRegions, localCount, foreignCount);
}

void
MM_AllocationContextBalanced::removeRegionFromFlushedList(MM_HeapRegionDescriptorVLHGC *region)
{
	_flushedRegions.removeRegion(region);
}

MM_HeapRegionDescriptorVLHGC *
MM_AllocationContextBalanced::selectRegionForContraction(MM_EnvironmentBase *env)
{
	/* since we know this is only called during contraction we could skip the lock, but rather be safe */
	_freeListLock.acquire();
	
	/* prefer free regions since idle MPAOL regions are more valuable to us */
	MM_HeapRegionDescriptorVLHGC *region = _freeRegions.peekFirstRegion();
	if (NULL != region) {
		_freeRegions.removeRegion(region);
	} else {
		region = _idleMPRegions.peekFirstRegion();
		if (NULL != region) {
			_idleMPRegions.removeRegion(region);
			region->_allocateData.taskAsFreePool(env);
		}
	}
	if (NULL != region) {
		Assert_MM_true(getNumaNode() == region->getNumaNode());
		Assert_MM_true(MM_HeapRegionDescriptor::FREE == region->getRegionType());
	}

	_freeListLock.release();

	return region;
}

bool
MM_AllocationContextBalanced::setNumaAffinityForThread(MM_EnvironmentBase *env)
{
	bool success = true;

	bool hasPhysicalNUMASupport = MM_GCExtensions::getExtensions(env)->_numaManager.isPhysicalNUMASupported();
	if (hasPhysicalNUMASupport && (0 != getNumaNode())) {
		/* TODO: should we try to read the affinity first and find the best node? */
		success = env->setNumaAffinity(_freeProcessorNodes, _freeProcessorNodeCount);
	}

	return success;
}

