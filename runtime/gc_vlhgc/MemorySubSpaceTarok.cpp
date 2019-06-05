
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

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "MemorySubSpaceTarok.hpp"

#include "AllocationContextBalanced.hpp"
#include "AllocationContextTarok.hpp"
#include "AllocateDescription.hpp"
#include "CardTable.hpp"
#include "Collector.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GlobalAllocationManagerTarok.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "HeapStats.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "Math.hpp"
#include "MarkMap.hpp"
#include "MarkMapManager.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "ModronTypes.hpp"
#include "ObjectAllocationInterface.hpp"
#include "PhysicalSubArena.hpp"

#define HEAP_FREE_RATIO_EXPAND_DIVISOR		100
#define HEAP_FREE_RATIO_EXPAND_MULTIPLIER	17

/**
 * Return the memory pool associated to the receiver.
 * @return MM_MemoryPool
 */
MM_MemoryPool *
MM_MemorySubSpaceTarok::getMemoryPool()
{
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Return the number of memory pools associated to the receiver.
 * @return count of number of memory pools 
 */
UDATA
MM_MemorySubSpaceTarok::getMemoryPoolCount()
{
	Assert_MM_unreachable();
	return UDATA_MAX;
}

/**
 * Return the number of active memory pools associated to the receiver.
 * @return count of number of memory pools 
 */
UDATA
MM_MemorySubSpaceTarok::getActiveMemoryPoolCount()
{
	Assert_MM_unreachable();
	return UDATA_MAX;
}

/**
 * Return the memory pool associated with a given storage location 
 * @param Address of storage location 
 * @return MM_MemoryPool
 */
MM_MemoryPool *
MM_MemorySubSpaceTarok::getMemoryPool(void * addr)
{
	MM_MemoryPool *pool = NULL;

	if (NULL != addr) {
		MM_HeapRegionDescriptorVLHGC *descriptor = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(addr);
		if (descriptor->containsObjects()) {
			pool = descriptor->getMemoryPool();
		}
	}
	return pool;
}

/**
 * Return the memory pool associated with a given allocation size
 * @param Size of allocation request
 * @return MM_MemoryPool
 */
MM_MemoryPool *
MM_MemorySubSpaceTarok::getMemoryPool(UDATA size)
{
	/* this function is only used by ConcurrentSweepScheme, which is disabled in Tarok */
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Return the memory pool associated with a specified range of storage locations. 
 * 
 * @param addrBase Low address in specified range  
 * @param addrTop High address in  specified range 
 * @param highAddr If range spans end of memory pool set to address of first byte
 * which does not belong in returned pool.
 * @return MM_MemoryPool for storage location addrBase
 */
MM_MemoryPool *
MM_MemorySubSpaceTarok::getMemoryPool(MM_EnvironmentBase *env, 
							void *addrBase, void *addrTop, 
							void * &highAddr)
{
	MM_MemoryPool *pool = NULL;

	if ((NULL != addrBase) && (NULL != addrTop)) {
		MM_HeapRegionDescriptorVLHGC *descriptor = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(addrBase);
		MM_HeapRegionDescriptorVLHGC *highDescriptor = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress((void *)((UDATA)addrTop-1));
		/* we can only work on committed regions with in-use memory pools */
		if (descriptor->containsObjects()) {
			pool = descriptor->getMemoryPool();
			if (descriptor != highDescriptor) {
				/* they requested a spanning area so set the highAddr to indicate a split */
				highAddr = descriptor->getHighAddress();
			} else {
				/* they requested a single region so set the highAddr to NULL to communicate that it is fully contained in the region */
				highAddr = NULL;
			}
		}
	}
	return pool;
}		


/* ***************************************
 * Allocation
 * ***************************************
 */

/**
 * @copydoc MM_MemorySubSpace::getActualFreeMemorySize()
 */
UDATA
MM_MemorySubSpaceTarok::getActualFreeMemorySize()
{
	if (isActive()) {
		return _globalAllocationManagerTarok->getActualFreeMemorySize();
	} else {
		return 0;
	}
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateFreeMemorySize()
 */
UDATA
MM_MemorySubSpaceTarok::getApproximateFreeMemorySize()
{
	if (isActive()) {
		return _globalAllocationManagerTarok->getApproximateFreeMemorySize();
	} else {
		return 0;
	}
}

UDATA
MM_MemorySubSpaceTarok::getActiveMemorySize()
{
	return getCurrentSize();
}

UDATA
MM_MemorySubSpaceTarok::getActiveMemorySize(UDATA includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		return getCurrentSize();
	} else { 
		return 0;
	}	
}

UDATA
MM_MemorySubSpaceTarok::getActiveLOAMemorySize(UDATA includeMemoryType)
{
	/* LOA is not supported in Tarok */
	return 0;
}	

/**
 * @copydoc MM_MemorySubSpace::getActualActiveFreeMemorySize()
 */
UDATA
MM_MemorySubSpaceTarok::getActualActiveFreeMemorySize()
{
	return _globalAllocationManagerTarok->getActualFreeMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getActualActiveFreeMemorySize(UDATA)
 */
UDATA
MM_MemorySubSpaceTarok::getActualActiveFreeMemorySize(UDATA includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType ) {
		return _globalAllocationManagerTarok->getActualFreeMemorySize();
	} else {
		return 0;
	}		 	
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeMemorySize()
 */
UDATA
MM_MemorySubSpaceTarok::getApproximateActiveFreeMemorySize()
{
	return _globalAllocationManagerTarok->getApproximateFreeMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeMemorySize(UDATA)
 */
UDATA
MM_MemorySubSpaceTarok::getApproximateActiveFreeMemorySize(UDATA includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType ) {
		return _globalAllocationManagerTarok->getApproximateFreeMemorySize();
	} else {
		return 0;
	}		 	
}


/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeLOAMemorySize()
 */
UDATA
MM_MemorySubSpaceTarok::getApproximateActiveFreeLOAMemorySize()
{
	/* LOA is not supported in Tarok */
	return 0;
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeLOAMemorySize(UDATA)
 */
UDATA
MM_MemorySubSpaceTarok::getApproximateActiveFreeLOAMemorySize(UDATA includeMemoryType)
{
	/* LOA is not supported in Tarok */
	return 0;
}		 	

void
MM_MemorySubSpaceTarok::mergeHeapStats(MM_HeapStats *heapStats)
{
	Assert_MM_unreachable();
}

void
MM_MemorySubSpaceTarok::mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType)
{
	_globalAllocationManagerTarok->mergeHeapStats(heapStats, includeMemoryType);
}

void
MM_MemorySubSpaceTarok::resetHeapStatistics(bool globalCollect)
{
	_globalAllocationManagerTarok->resetHeapStatistics(globalCollect);
}

/**
 * Return the allocation failure stats for this subSpace.
 */
MM_AllocationFailureStats *
MM_MemorySubSpaceTarok::getAllocationFailureStats()
{
	/* this subspace doesn't have a parent so it must have a collector. */
	Assert_MM_true(NULL != _collector);
	return MM_MemorySubSpace::getAllocationFailureStats();
}

/****************************************
 * Allocation
 ****************************************
 */

void *
MM_MemorySubSpaceTarok::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Allocate the arraylet spine in immortal or scoped memory.
 */
#if defined(J9VM_GC_ARRAYLETS)
void *
MM_MemorySubSpaceTarok::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	Assert_MM_unreachable();
	return NULL;
}
#endif /* J9VM_GC_ARRAYLETS */

void *
MM_MemorySubSpaceTarok::allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace)
{
	Assert_MM_unreachable();
	return NULL;
}

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
void *
MM_MemorySubSpaceTarok::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	Assert_MM_unreachable();
	return NULL;
}
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

/****************************************
 * Internal Allocation
 ****************************************
 */
void *
MM_MemorySubSpaceTarok::collectorAllocate(MM_EnvironmentBase *env, MM_Collector *requestCollector,  MM_AllocateDescription *allocDescription)
{
	Assert_MM_unreachable();
	return NULL;
}

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)

void *
MM_MemorySubSpaceTarok::collectorAllocateTLH(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription, 
												UDATA maximumBytesRequired, void * &addrBase, void * &addrTop)
{
	Assert_MM_unreachable();
	return NULL;
}

#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

void
MM_MemorySubSpaceTarok::abandonHeapChunk(void *addrBase, void *addrTop)
{
	if (addrBase != addrTop) {
		MM_HeapRegionDescriptorVLHGC *base = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(addrBase);
		MM_HeapRegionDescriptorVLHGC *verify = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress((void *)((UDATA)addrTop - 1));
		Assert_MM_true(base == verify);
		/* we can only work on committed regions with in-use memory pools */
		Assert_MM_true(base->containsObjects());
		base->getMemoryPool()->abandonHeapChunk(addrBase, addrTop);
	}
}

/****************************************
 *	Sub Space Categorization
 ****************************************
 */
MM_MemorySubSpace *
MM_MemorySubSpaceTarok::getDefaultMemorySubSpace()
{
	return this;
}

MM_MemorySubSpace *
MM_MemorySubSpaceTarok::getTenureMemorySubSpace()
{
	return this;
}

bool
MM_MemorySubSpaceTarok::isActive()
{
	Assert_MM_true(NULL == _parent);
	return true;
}

/**
 * Ask memory pools if a complete rebuild of freelist is required
 */
bool
MM_MemorySubSpaceTarok::completeFreelistRebuildRequired(MM_EnvironmentBase *env)
{
	/* 
	 * this function is only used by MemoryPoolLargeObjects. Since we don't 
	 * support LOA in Tarok we can just return false. 
	 */
	return false;
}	

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemorySubSpaceTarok::reset()
{
	/* unused in Tarok collectors */
	Assert_MM_unreachable();
}

/**
 * As opposed to reset, which will empty out, this will fill out as if everything is free
 */
void
MM_MemorySubSpaceTarok::rebuildFreeList(MM_EnvironmentBase *env)
{
	Assert_MM_unreachable();
}

void
MM_MemorySubSpaceTarok::resetLargestFreeEntry()
{
	_globalAllocationManagerTarok->resetLargestFreeEntry();
	Assert_MM_true(NULL == getChildren());
}

void
MM_MemorySubSpaceTarok::recycleRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
{
	MM_EnvironmentVLHGC *envVLHGC = (MM_EnvironmentVLHGC *)env;
	MM_HeapRegionDescriptorVLHGC *regionStandard = (MM_HeapRegionDescriptorVLHGC *)region;
	/* first try to recycle into the original owning context, if there is one */
	MM_AllocationContextTarok *context = regionStandard->_allocateData._originalOwningContext;

	//TODO: move selection logic into ACT to minimize #ifdefs

	if (NULL == context) {
		context = regionStandard->_allocateData._owningContext;
	}

	switch (region->getRegionType()) {
		case MM_HeapRegionDescriptor::BUMP_ALLOCATED:
		case MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED:
			/* declare previous mark map cleared, except if the region is arraylet leaf.
			 * leaving _nextMarkMapCleared unchanged
			 */
			regionStandard->_previousMarkMapCleared = true;
#if defined(J9VM_GC_ARRAYLETS)
		case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
#endif /* defined(J9VM_GC_ARRAYLETS) */
			context->recycleRegion(envVLHGC, regionStandard);
			break;
		default:
			Assert_MM_unreachable();
	}
}

UDATA
MM_MemorySubSpaceTarok::findLargestFreeEntry(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription)
{
	return _globalAllocationManagerTarok->getLargestFreeEntry();
}

/**
 * Initialization
 */
MM_MemorySubSpaceTarok *
MM_MemorySubSpaceTarok::newInstance(MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_GlobalAllocationManagerTarok *gamt, bool usesGlobalCollector, UDATA minimumSize, UDATA initialSize, UDATA maximumSize, UDATA memoryType, U_32 objectFlags)
{
	MM_MemorySubSpaceTarok *memorySubSpace;
	
	memorySubSpace = (MM_MemorySubSpaceTarok *)env->getForge()->allocate(sizeof(MM_MemorySubSpaceTarok), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != memorySubSpace) {
		MM_HeapRegionManager *heapRegionManager = MM_GCExtensions::getExtensions(env)->heapRegionManager;
		new(memorySubSpace) MM_MemorySubSpaceTarok(env, physicalSubArena, gamt, heapRegionManager, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryType, objectFlags);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	}
	return memorySubSpace;
}

bool
MM_MemorySubSpaceTarok::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemorySubSpace::initialize(env)) {
		return false;
	}
	
	if (!_expandLock.initialize(env, &MM_GCExtensions::getExtensions(env)->lnrlOptions, "MM_MemorySubSpaceTarok:_expandLock")) {
		return false;
	}

	return true;
}

void
MM_MemorySubSpaceTarok::tearDown(MM_EnvironmentBase *env)
{
	/* shutdown all regions with pools */
	GC_MemorySubSpaceRegionIterator regionIterator(this);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = (MM_HeapRegionDescriptorVLHGC*)regionIterator.nextRegion())) {
		/* first try to teardown with the original owning context, if there is one */
		MM_AllocationContextTarok *context = region->_allocateData._originalOwningContext;
		if (NULL == context) {
			context = region->_allocateData._owningContext;
		}
		if (NULL != context) {
			context->tearDownRegion(env, region);
		}
	}

	_expandLock.tearDown();

	MM_MemorySubSpace::tearDown(env);
}

/**
 * Memory described by the range has added to the heap and been made available to the subspace as free memory.
 */
bool
MM_MemorySubSpaceTarok::expanded(
	MM_EnvironmentBase *env,
	MM_PhysicalSubArena *subArena,
	MM_HeapRegionDescriptor *region,
	bool canCoalesce)
{
	void* regionLowAddress = region->getLowAddress();
	void* regionHighAddress = region->getHighAddress();

	/* Inform the sub space hierarchy of the size change */
	bool result = heapAddRange(env, this, region->getSize(), regionLowAddress, regionHighAddress);

	if (result) {
		/* Expand the valid range for arraylets. */
		MM_GCExtensions::getExtensions(_extensions)->indexableObjectModel.expandArrayletSubSpaceRange(this, regionLowAddress, regionHighAddress, largestDesirableArraySpine());

		/* this region should be reserved when we first expand into it */
		Assert_MM_true(MM_HeapRegionDescriptor::RESERVED == region->getRegionType());

		/* Region about to be set reserved better not to be marked overflowed in GMP or PGC */
		Assert_MM_true(0 == ((MM_HeapRegionDescriptorVLHGC *)region)->_markData._overflowFlags);

		/* now, mark the region as free and pass it to the region pool for management in its free list */
		region->setRegionType(MM_HeapRegionDescriptor::FREE);
		((MM_HeapRegionDescriptorVLHGC *)region)->_previousMarkMapCleared = false;
		((MM_HeapRegionDescriptorVLHGC *)region)->_nextMarkMapCleared = false;

		if (_extensions->tarokEnableExpensiveAssertions) {
			/* dirty the mark map (not assert code per se, but helps enhance other asserts when checkBitsForRegion is called) */
			MM_MarkMapManager *markMapManager = ((MM_IncrementalGenerationalGC *)_extensions->getGlobalCollector())->getMarkMapManager();
			markMapManager->getPartialGCMap()->setBitsForRegion(env, region, false);
			markMapManager->getGlobalMarkPhaseMap()->setBitsForRegion(env, region, false);
		}
	
		result = _extensions->cardTable->commitCardsForRegion(env, region);

		if (result) {
			_extensions->cardTable->clearCardsInRange(env, region->getLowAddress(), region->getHighAddress());
			_globalAllocationManagerTarok->expand(env, (MM_HeapRegionDescriptorVLHGC *)region);
		} else {
			heapRemoveRange(env, this, region->getSize(), regionLowAddress, regionHighAddress, NULL, NULL);
		}
	}
	return result;
}

/**
 * Memory described by the range which was already part of the heap has been made available to the subspace
 * as free memory.
 * @note Size information (current) is not updated.
 * @warn This routine is fairly hacky - is there a better way?
 */
void
MM_MemorySubSpaceTarok::addExistingMemory(
	MM_EnvironmentBase *env,
	MM_PhysicalSubArena *subArena,
	UDATA size,
	void *lowAddress,
	void *highAddress,
	bool canCoalesce)
{
	Assert_MM_unreachable();
}

/**
 * Memory described by the range which was already part of the heap is being removed from the current memory spaces
 * ownership.  Adjust accordingly.
 * @note Size information (current) is not updated.
 */
void *
MM_MemorySubSpaceTarok::removeExistingMemory(
	MM_EnvironmentBase *env,
	MM_PhysicalSubArena *subArena,
	UDATA contractSize,
	void *lowAddress,
	void *highAddress)
{
	/* Routine is normally used to contract within a memory pool (removing free memory) or adjusting barrier ranges, neither of which in this
	 * configuration are handled here (a contract is a full pool / region removal, and there is no barrier range "change" as a result of removing
	 * regions).
	 */

	return lowAddress;
}

MM_HeapRegionDescriptor * 
MM_MemorySubSpaceTarok::selectRegionForContraction(MM_EnvironmentBase *env, UDATA numaNode)
{
	MM_AllocationContextTarok * allocationContext = _globalAllocationManagerTarok->getAllocationContextForNumaNode(numaNode);
	Assert_MM_true(NULL != allocationContext);
	Assert_MM_true(allocationContext->getNumaNode() == numaNode);
	
	MM_HeapRegionDescriptorVLHGC * region = allocationContext->selectRegionForContraction(env);

	return region;
}

void *
MM_MemorySubSpaceTarok::lockedReplenishAndAllocate(MM_EnvironmentBase *env, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType)
{
	Trc_MM_MemorySubSpaceTarok_lockedReplenishAndAllocate_Entry(env->getLanguageVMThread());
	
	/* cast the context type since we know that we are operating on the Tarok context and we are its friend */
	MM_AllocationContextTarok *tarokContext = (MM_AllocationContextTarok *) context;
	
	void *result = tarokContext->lockedReplenishAndAllocate(env, objectAllocationInterface, allocateDescription, allocationType);
	if (NULL == result) {
		Trc_MM_MemorySubSpaceTarok_lockedReplenishAndAllocate_Failure(env->getLanguageVMThread(), _bytesRemainingBeforeTaxation);
	} else {
		Trc_MM_MemorySubSpaceTarok_lockedReplenishAndAllocate_Success(env->getLanguageVMThread(), result, _bytesRemainingBeforeTaxation);
	}
	
	return result;
	
}

void
MM_MemorySubSpaceTarok::setBytesRemainingBeforeTaxation(UDATA remaining)
{
	Trc_MM_setBytesRemainingBeforeTaxation(remaining);
	_bytesRemainingBeforeTaxation = remaining;
}

bool 
MM_MemorySubSpaceTarok::consumeFromTaxationThreshold(MM_EnvironmentBase *env, UDATA bytesToConsume)
{
	bool success = false;
	
	/* loop until we either subtract bytesToConsume from _bytesRemainingBeforeTaxation, 
	 * or set _bytesRemainingBeforeTaxation to zero 
	 */
	bool thresholdUpdated = false;
	do {
		UDATA oldBytesRemaining = _bytesRemainingBeforeTaxation;
		if (oldBytesRemaining < bytesToConsume) {
			_bytesRemainingBeforeTaxation = 0;
			success = false;
			thresholdUpdated = true;
		} else {
			UDATA newBytesRemaining = oldBytesRemaining - bytesToConsume;
			success = true;
			thresholdUpdated = (MM_AtomicOperations::lockCompareExchange(&_bytesRemainingBeforeTaxation, oldBytesRemaining, newBytesRemaining) == oldBytesRemaining);
		}
	} while (!thresholdUpdated);

	return success;
}

void *
MM_MemorySubSpaceTarok::replenishAllocationContextFailed(MM_EnvironmentBase *env, MM_MemorySubSpace *replenishingSpace, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType)
{
	void *result = NULL;
	Trc_MM_MemorySubSpaceTarok_replenishAllocationContextFailed_Entry(env->getLanguageVMThread(), context, (UDATA)allocationType, allocateDescription->getContiguousBytes());
	Assert_MM_true(this == replenishingSpace);

	/* we currently have no design for handling AC replenishment in a multi-subspace world, so reach for the collector */
	MM_IncrementalGenerationalGC *collector = (MM_IncrementalGenerationalGC*)MM_GCExtensions::getExtensions(env)->getGlobalCollector();
	Assert_MM_true(NULL != collector);

	allocateDescription->saveObjects(env);
	if(!env->acquireExclusiveVMAccessForGC(collector, true, true)) {
		allocateDescription->restoreObjects(env);
		/* don't have exclusive access - another thread beat us to the GC.  retry the allocate using the standard locking path */
		result = context->allocate(env, objectAllocationInterface, allocateDescription, allocationType);

		if(NULL == result) {
			allocateDescription->saveObjects(env);
			/* still can't satisfy - grab exclusive at all costs and retry the allocate */
			if(!env->acquireExclusiveVMAccessForGC(collector)) {
				allocateDescription->restoreObjects(env);
				/* now that we have exclusive, see if there is memory to satisfy the allocate (since we might not be the first to get in here) */
				result = lockedAllocate(env, context, objectAllocationInterface, allocateDescription, allocationType);

				if(NULL != result) {
					/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
					 * an event for tracing / verbose to report the occurrence.
					 */
					reportAcquiredExclusiveToSatisfyAllocate(env, allocateDescription);
				}
			} else {
				allocateDescription->restoreObjects(env);
			}
		}
	} else {
		allocateDescription->restoreObjects(env);
	}

	if (NULL == result) {
		Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

		/* check if this is a taxation point or a true failure */
		if (0 == _bytesRemainingBeforeTaxation) {
			allocateDescription->saveObjects(env);
			collector->taxationEntryPoint(env, this, allocateDescription);
			allocateDescription->restoreObjects(env);
			result = lockedAllocate(env, context, objectAllocationInterface, allocateDescription, allocationType);
			Trc_MM_MemorySubSpaceTarok_replenishAllocationContextFailed_didPerformTaxationAndReplenish(env->getLanguageVMThread(), context, (UDATA)allocationType, allocateDescription->getContiguousBytes(), result);
		}
	}
	if (NULL == result) {
		Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

		/* we failed, so this thread will handle the allocation failure */
		reportAllocationFailureStart(env, allocateDescription);
		/* first, try a resize to satisfy without invoking the collector */
		performResize(env, allocateDescription);
		result = lockedAllocate(env, context, objectAllocationInterface, allocateDescription, allocationType);
		Trc_MM_MemorySubSpaceTarok_replenishAllocationContextFailed_didPerformResizeAndReplenish(env->getLanguageVMThread(), context, (UDATA)allocationType, allocateDescription->getContiguousBytes(), result);
		if (NULL == result) {
			/* the resize wasn't enough so invoke the collector */
			allocateDescription->saveObjects(env);
			allocateDescription->setAllocationType(allocationType);
			result = collector->garbageCollect(env, this, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_DEFAULT, objectAllocationInterface, replenishingSpace, context);
			Trc_MM_MemorySubSpaceTarok_replenishAllocationContextFailed_didPerformCollect(env->getLanguageVMThread(), context, (UDATA)allocationType, allocateDescription->getContiguousBytes(), result);
			allocateDescription->restoreObjects(env);
			if (NULL == result) {
				/* we _still_ failed so invoke an aggressive collect */
				allocateDescription->saveObjects(env);
				result = collector->garbageCollect(env, this, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE, objectAllocationInterface, replenishingSpace, context);
				Trc_MM_MemorySubSpaceTarok_replenishAllocationContextFailed_didPerformAggressiveCollect(env->getLanguageVMThread(), context, (UDATA)allocationType, allocateDescription->getContiguousBytes(), result);
				allocateDescription->restoreObjects(env);
			}
		}
		/* allocation failure is over, whether we satisfied the allocate or not */
		reportAllocationFailureEnd(env);
	}

	Trc_MM_MemorySubSpaceTarok_replenishAllocationContextFailed_Exit(env->getLanguageVMThread(), result);
	return result;
}

void* 
MM_MemorySubSpaceTarok::lockedAllocate(MM_EnvironmentBase *env, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType)
{
	void* result = NULL;
	
	/* arraylet leaves need to be allocated directly after a new region has been found so fall through to the replenish path in that case */
#if defined(J9VM_GC_ARRAYLETS)
	if (MM_MemorySubSpace::ALLOCATION_TYPE_LEAF != allocationType)
#endif /* defined(J9VM_GC_ARRAYLETS) */
	{
		result = ((MM_AllocationContextTarok *)context)->lockedAllocate(env, objectAllocationInterface, allocateDescription, allocationType);
	}
	if (NULL == result) {
		/* we failed so do the locked replenish */
		result = lockedReplenishAndAllocate(env, context, objectAllocationInterface, allocateDescription, allocationType);
	}
	
	return result;
}


/****************************************
 * Expansion/Contraction
 ****************************************
 */

/**
 * Adjust the specified expansion amount by the specified user increment amount (i.e. -Xmoi)
 * @return the updated expand size
 */
UDATA
MM_MemorySubSpaceTarok::adjustExpansionWithinUserIncrement(MM_EnvironmentBase *env, UDATA expandSize)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());

	if (extensions->allocationIncrementSetByUser) {
		UDATA expandIncrement = extensions->allocationIncrement;

		/* increment of 0 means no expansion is to occur. Don't round  to a size of 0 */
		if (0 == expandIncrement) {
			return expandSize;
		}

		/* Round to the Xmoi value */
		return MM_Math::roundToCeiling(expandIncrement, expandSize);
	}
	
	return MM_MemorySubSpace::adjustExpansionWithinUserIncrement(env, expandSize);
}

/**
 * Determine the maximum expansion amount the memory subspace can expand by.
 * The amount returned is restricted by values within the receiver of the call, as well as those imposed by
 * the parents of the receiver and the owning MemorySpace of the receiver.
 * 
 * @return the amount by which the receiver can expand
 */
UDATA
MM_MemorySubSpaceTarok::maxExpansionInSpace(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	UDATA expandIncrement = extensions->allocationIncrement;
	UDATA maxExpandAmount;
	
	if (extensions->allocationIncrementSetByUser) {
		/* increment of 0 means no expansion */
		if (0 == expandIncrement) {
			return 0;
		}
	}

	maxExpandAmount = MM_MemorySubSpace::maxExpansionInSpace(env);

	return maxExpandAmount;
}

/**
 * Get the size of heap available for contraction.
 * Return the amount of heap available to be contracted, factoring in any potential allocate that may require the
 * available space.
 * @return The amount of heap available for contraction factoring in the size of the allocate (if applicable)
 */
UDATA 
MM_MemorySubSpaceTarok::getAvailableContractionSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	return _physicalSubArena->getAvailableContractionSize(env, this, allocDescription);
}

UDATA
MM_MemorySubSpaceTarok::collectorExpand(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription)
{
	/* we inherit this collectorExpand method, but don't implement it with this signature. */
	Assert_MM_unreachable();
	return 0;
}

UDATA
MM_MemorySubSpaceTarok::collectorExpand(MM_EnvironmentBase *env)
{
	Trc_MM_MemorySubSpaceTarok_collectorExpand_Entry(env->getLanguageVMThread());
	
	_expandLock.acquire();

	/* Determine the amount to expand the heap */
	UDATA expandSize = calculateCollectorExpandSize(env);
	Assert_MM_true((0 == expandSize) || (_heapRegionManager->getRegionSize() == expandSize));

	_extensions->heap->getResizeStats()->setLastExpandReason(SATISFY_COLLECTOR);
	
	/* expand by a single region */
	/* for the most part the code path is not multi-threaded safe, so we do this under expandLock */
	UDATA expansionAmount= expand(env, expandSize);
	Assert_MM_true((0 == expansionAmount) || (expandSize == expansionAmount));

	/* Inform the requesting collector that an expand attempt took place (even if the expansion failed) */
	MM_IncrementalGenerationalGC *collector = (MM_IncrementalGenerationalGC*)MM_GCExtensions::getExtensions(env)->getGlobalCollector();
	Assert_MM_true(NULL != collector);
	collector->collectorExpanded(env, this, expansionAmount);

	_expandLock.release();

	Trc_MM_MemorySubSpaceTarok_collectorExpand_Exit(env->getLanguageVMThread(), expansionAmount);
	
	return expansionAmount;
}

/**
 * Perform the contraction/expansion based on decisions made by checkResize.
 * Adjustments in contraction size is possible (because compaction might have yielded less then optimal results),
 * therefore allocDescriptor is still passed.
 * @return the actual amount of resize (having IDATA return result will contain valid value only if contract/expand size is half of maxOfUDATA)
 */
IDATA
MM_MemorySubSpaceTarok::performResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	UDATA oldVMState = env->pushVMstate(OMRVMSTATE_GC_PERFORM_RESIZE);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	/* If -Xgc:fvtest=forceTenureResize is specified, then repeat a sequence of 5 expands followed by 5 contracts */	
	if (extensions->fvtest_forceOldResize) {
		UDATA resizeAmount = 0;
		UDATA regionSize = _extensions->regionSize;
		resizeAmount = 2*regionSize;
		if (5 > extensions->fvtest_oldResizeCounter) {
			UDATA expansionSize = MM_Math::roundToCeiling(extensions->heapAlignment, resizeAmount);
			expansionSize = MM_Math::roundToCeiling(regionSize, expansionSize);
			if (canExpand(env, expansionSize)) {
				extensions->heap->getResizeStats()->setLastExpandReason(FORCED_NURSERY_EXPAND);
				_contractionSize = 0;
				_expansionSize = expansionSize;
				extensions->fvtest_oldResizeCounter += 1;
			}
		} else if (10 > extensions->fvtest_oldResizeCounter) {
			UDATA contractionSize = MM_Math::roundToCeiling(extensions->heapAlignment, resizeAmount);
			contractionSize = MM_Math::roundToCeiling(regionSize, contractionSize);
			if (canContract(env, contractionSize)) {
				_contractionSize = contractionSize;
				extensions->heap->getResizeStats()->setLastContractReason(FORCED_NURSERY_CONTRACT);
				_expansionSize = 0;
				extensions->fvtest_oldResizeCounter += 1;
			}
		} 
		
		if (10 <= extensions->fvtest_oldResizeCounter) {
			extensions->fvtest_oldResizeCounter = 0;
		}
	}	
	
	IDATA resizeAmount = 0;

	if (_contractionSize != 0) {
		resizeAmount = -(IDATA)performContract(env, allocDescription);
	} else if (_expansionSize != 0) {
		resizeAmount = performExpand(env);
	}
	
	env->popVMstate(oldVMState);

	return resizeAmount;
}

/**
 * Calculate the contraction/expansion size required (if any). Do not perform anything yet.
 */
void
MM_MemorySubSpaceTarok::checkResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool _systemGC)
{
	UDATA oldVMState = env->pushVMstate(OMRVMSTATE_GC_CHECK_RESIZE);
	if (!timeForHeapContract(env, allocDescription, _systemGC)) {
		timeForHeapExpand(env, allocDescription);
	}
	env->popVMstate(oldVMState);
}

/**
 * Expand the heap by required amount
 * @return
 */
UDATA
MM_MemorySubSpaceTarok::performExpand(MM_EnvironmentBase *env)
{
	UDATA actualExpandAmount;
	
	Trc_MM_MemorySubSpaceTarok_performExpand_Entry(env->getLanguageVMThread(), _expansionSize);

	actualExpandAmount= expand(env, _expansionSize);
	
	_expansionSize = 0;
	
	if (actualExpandAmount > 0 ) {
	
		/* Remember the gc count at the time last expansion. If expand is outside a gc this will be 
		 * number of last gc.
	 	 */ 
		/* Note that the gcCount includes System GCs, PGCs, AFs and GMP increments */
		UDATA gcCount = _extensions->globalVLHGCStats.gcCount;

		_extensions->heap->getResizeStats()->setLastHeapExpansionGCCount(gcCount);
	}	

	Trc_MM_MemorySubSpaceTarok_performExpand_Exit(env->getLanguageVMThread(), actualExpandAmount);
	return actualExpandAmount;
}

/**
 * Determine how much we should attempt to contract heap by and call contract()
 * @return The amount we actually managed to contract the heap
 */
UDATA
MM_MemorySubSpaceTarok::performContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	UDATA contractSize, targetContractSize, maximumContractSize;
	UDATA allocationSize = 0;
	if (NULL != allocDescription) {
		allocationSize = allocDescription->getBytesRequested();
	}
	
	Trc_MM_MemorySubSpaceTarok_performContract_Entry(env->getLanguageVMThread(), allocationSize);
	
	/* Work out the upper limit of the contract size.  We may not be able to contract
	 * by this much as we may not have this much storage free at end of heap in first place
	*/ 
	targetContractSize = _contractionSize;
	
	/* Contract no longer outstanding so reset */
	_contractionSize = 0;
	
	if (targetContractSize == 0 ) {
		Trc_MM_MemorySubSpaceTarok_performContract_Exit1(env->getLanguageVMThread());
		return 0;	
	}	
	
	/* We can only contract within the limits of the last free chunk and we 
	 * need to make sure we don't contract and lose the only chunk of free storage
	 * available to satisfy the allocation request.
	 * The call will adjust for the allocation requirements (if any)
	 */
	maximumContractSize = getAvailableContractionSize(env, allocDescription);  
	
	/* round down to multiple of heap alignment */
	maximumContractSize= MM_Math::roundToFloor(_extensions->heapAlignment, maximumContractSize);
	
	/* Decide by how much to contract */
	if (targetContractSize > maximumContractSize) {
		contractSize= maximumContractSize;
		Trc_MM_MemorySubSpaceTarok_performContract_Event1(env->getLanguageVMThread(), targetContractSize, maximumContractSize, contractSize);
	} else {
		contractSize= targetContractSize;
		Trc_MM_MemorySubSpaceTarok_performContract_Event2(env->getLanguageVMThread(), targetContractSize, maximumContractSize, contractSize);
	}
	
	contractSize = MM_Math::roundToFloor(_extensions->regionSize, contractSize);
	
	if (contractSize == 0 ) {
		Trc_MM_MemorySubSpaceTarok_performContract_Exit2(env->getLanguageVMThread());
		return 0;	
	} else {
		UDATA actualContractSize= contract(env, contractSize);
		if (actualContractSize > 0 ) {
			/* Remember the gc count at the time of last contraction. If contract is outside a gc 
	 		 * this will be number of last gc.
	 		 */
			/* Note that the gcCount includes System GCs, PGCs, AFs and GMP increments */
			UDATA gcCount = _extensions->globalVLHGCStats.gcCount;

			_extensions->heap->getResizeStats()->setLastHeapContractionGCCount(gcCount);
		}	

		Trc_MM_MemorySubSpaceTarok_performContract_Exit3(env->getLanguageVMThread(), actualContractSize);
		return actualContractSize;
	}	
}

/**
 * Determine how much we should attempt to expand subspace by and store the result in _expansionSize
 * @return true if expansion size is non zero
 */
bool
MM_MemorySubSpaceTarok::timeForHeapExpand(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	bool doExpand = false;
	
	if((NULL == _physicalSubArena) || !_physicalSubArena->canExpand(env) || (maxExpansionInSpace(env) == 0 )) {
		/* The PSA or memory sub space cannot be expanded ... we are done */
	} else {
		bool expandToSatisfy = false;
		UDATA sizeInRegionsRequired = 0;

		if (NULL != allocDescription) {
			sizeInRegionsRequired = 1;
			if (allocDescription->isArrayletSpine()) {
				sizeInRegionsRequired += allocDescription->getNumArraylets();
			}
			if (sizeInRegionsRequired > _globalAllocationManagerTarok->getFreeRegionCount()) {
				expandToSatisfy = true;
			}
		}
	
		UDATA sizeInBytesRequired = sizeInRegionsRequired * _heapRegionManager->getRegionSize();
		_expansionSize = calculateExpandSize(env, sizeInBytesRequired, expandToSatisfy);
		doExpand = (0 != _expansionSize);
	}
	return doExpand;
}

/**
 * Determine how much we should attempt to contract subspace by and store the result in _contractionSize
 * 
 * @return true if contraction size is non zero
 */
bool
MM_MemorySubSpaceTarok::timeForHeapContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool systemGC)
{
	Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Entry(env->getLanguageVMThread(), systemGC ? "true" : "false");

	/* If PSA or memory sub space can't be shrunk don't bother trying */
	if ( (NULL == _physicalSubArena) || !_physicalSubArena->canContract(env) || (maxContraction(env) == 0)) {
		Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit1(env->getLanguageVMThread());
		return false;
	}

	/* Don't shrink if we have not met the allocation request
	 * ..we will be expanding soon if possible anyway
	 */
	if (NULL != allocDescription) {
		UDATA sizeInRegionsRequired = 1;
		if (allocDescription->isArrayletSpine()) {
			sizeInRegionsRequired += allocDescription->getNumArraylets();
		}
		UDATA freeRegionCount = _globalAllocationManagerTarok->getFreeRegionCount();
		if (sizeInRegionsRequired >= freeRegionCount) {
			Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit4(env->getLanguageVMThread(), sizeInRegionsRequired, freeRegionCount);
			_contractionSize = 0;
			return false;
		}
	}

	MM_Heap * heap = MM_GCExtensions::getExtensions(_extensions)->getHeap();
	UDATA actualSoftMx = heap->getActualSoftMxSize(env);

	if(0 != actualSoftMx) {
		if(actualSoftMx < getActiveMemorySize()) {
			/* the softmx is less than the currentsize so we're going to attempt an aggressive contract */
			_contractionSize = getActiveMemorySize() - actualSoftMx;
			_extensions->heap->getResizeStats()->setLastContractReason(HEAP_RESIZE);
			return true;
		}
	}
	
	/* Don't shrink if -Xmaxf1.0 specified , i.e max free is 100% */
	if ( _extensions->heapFreeMaximumRatioMultiplier == 100 ) {
		Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit2(env->getLanguageVMThread());
		return false;
	}
	
	/* No need to shrink if we will not be above -Xmaxf after satisfying the allocate */
	UDATA allocSize = allocDescription ? allocDescription->getBytesRequested() : 0;
	
	/* Are we spending too little time in GC ? */
	bool ratioContract = checkForRatioContract(env);
	
	/* How much, if any, do we need to contract by ? */
	_contractionSize = calculateTargetContractSize(env, allocSize, ratioContract);
	
	if (_contractionSize == 0 ) {
		Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit3(env->getLanguageVMThread());
		return false;
	}	
	

	
	/* Don't shrink if we expanded in last extensions->heapContractionStabilizationCount global collections */
	/* Note that the gcCount includes System GCs, PGCs, AFs and GMP increments */
	UDATA gcCount = _extensions->globalVLHGCStats.gcCount;
	if (_extensions->heap->getResizeStats()->getLastHeapExpansionGCCount() + _extensions->heapContractionStabilizationCount > gcCount) {
		Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit5(env->getLanguageVMThread());
		_contractionSize = 0;
		return false;	
	}	
	
	/* Don't shrink if its a system GC and we had less than -Xminf free at 
	 * the start of the garbage collection 
	 */ 
	 if (systemGC) {
	 	UDATA minimumFree = (getActiveMemorySize() / _extensions->heapFreeMinimumRatioDivisor) 
								* _extensions->heapFreeMinimumRatioMultiplier;
		UDATA freeBytesAtSystemGCStart = _extensions->heap->getResizeStats()->getFreeBytesAtSystemGCStart();
		
		if (freeBytesAtSystemGCStart < minimumFree) {
	 		Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit6(env->getLanguageVMThread(), freeBytesAtSystemGCStart, minimumFree);
			_contractionSize = 0;
	 		return false;	
		}	
	 }	
	
	/* Remember reason for contraction for later */
	if (ratioContract) {
		_extensions->heap->getResizeStats()->setLastContractReason(GC_RATIO_TOO_LOW);
	} else {
		_extensions->heap->getResizeStats()->setLastContractReason(FREE_SPACE_GREATER_MAXF);
	}	
		
	Trc_MM_MemorySubSpaceTarok_timeForHeapContract_Exit7(env->getLanguageVMThread(), _contractionSize);
	return true;
}


/**
 * Determine the amount of heap to contract.
 * Calculate the contraction size while factoring in the pending allocate and whether a contract based on
 * percentage of GC time to total time is required.  If there is room to contract, the value is derived from,
 * 1) The heap free ratio multipliers
 * 2) The heap maximum/minimum contraction sizes
 * 3) The heap alignment
 * @note We use the approximate heap size to account for defered work that may during execution free up more memory.
 * @todo Explain what the fudge factors of +5 and +1 mean
 * @return the recommended amount of heap in bytes to contract.
 */
UDATA
MM_MemorySubSpaceTarok::calculateTargetContractSize(MM_EnvironmentBase *env, UDATA allocSize, bool ratioContract)
{
	Trc_MM_MemorySubSpaceTarok_calculateTargetContractSize_Entry(env->getLanguageVMThread(), allocSize, ratioContract ? "true":"false");
	UDATA contractionSize = 0;

	/* If there is not enough memory to satisfy the alloc, don't contract.  If allocSize is greater than the total free memory,
	 * the currentFree value is a large positive number (negative unsigned number calculated above).
	 */
	if (allocSize > getApproximateActiveFreeMemorySize()) {
		contractionSize = 0;
	} else {
		UDATA currentFree = getApproximateActiveFreeMemorySize() - allocSize;
		UDATA currentHeapSize = getActiveMemorySize();
		UDATA maximumFreePercent =  ratioContract ? OMR_MIN(_extensions->heapFreeMinimumRatioMultiplier + 5, _extensions->heapFreeMaximumRatioMultiplier + 1) :
													_extensions->heapFreeMaximumRatioMultiplier + 1;
		UDATA maximumFree = (currentHeapSize / _extensions->heapFreeMaximumRatioDivisor) * maximumFreePercent;

		/* Do we have more free than is desirable ? */
		if (currentFree > maximumFree ) {
			/* How big a heap do we need to leave maximumFreePercent free given current live data */
			UDATA targetHeapSize = ((currentHeapSize - currentFree) / (_extensions->heapFreeMaximumRatioDivisor - maximumFreePercent))
										 * _extensions->heapFreeMaximumRatioDivisor;
			
			if (currentHeapSize < targetHeapSize) {
				/* due to rounding errors, targetHeapSize may actually be larger than currentHeapSize */
				contractionSize = 0;
			} else {
				/* Calculate how much we need to contract by to get to target size.
				 * Note: PSA code will ensure we do not drop below initial heap size
				 */
				contractionSize= currentHeapSize - targetHeapSize;
							
				Trc_MM_MemorySubSpaceTarok_calculateTargetContractSize_Event1(env->getLanguageVMThread(), contractionSize);
				
				/* But we don't contract too quickly or by a trivial amount */	
				UDATA maxContract = (UDATA)(currentHeapSize * _extensions->globalMaximumContraction);
				UDATA minContract = (UDATA)(currentHeapSize * _extensions->globalMinimumContraction);
				UDATA contractionGranule = _extensions->regionSize;
				
				/* If max contraction is less than a single region (minimum contraction granularity) round it up */
				if (maxContract < contractionGranule) {
					maxContract = contractionGranule;
				} else {
					maxContract = MM_Math::roundToCeiling(contractionGranule, maxContract);
				}
				
				contractionSize = OMR_MIN(contractionSize, maxContract);
				
				/* We will contract in multiples of region size. Result may become zero */
				contractionSize = MM_Math::roundToFloor(contractionGranule, contractionSize);
				
				/* Make sure contract is worthwhile, don't want to go to possible expense of a 
				 * compact for a small contraction
				 */
				if (contractionSize < minContract) { 
					contractionSize = 0;
				}	
				
				Trc_MM_MemorySubSpaceTarok_calculateTargetContractSize_Event2(env->getLanguageVMThread(), contractionSize, maxContract);
			}
		} else {
			/* No need to contract as current free less than max */
			contractionSize = 0;
		}	
	}
		
	Trc_MM_MemorySubSpaceTarok_calculateTargetContractSize_Exit1(env->getLanguageVMThread(), contractionSize);
	return contractionSize;
}	


/**
 * Determine how much space we need to expand the heap by on this GC cycle to meet the users specified -Xminf amount
 * @note We use the approximate heap size to account for defered work that may during execution free up more memory.
 * @param expandToSatisfy - if TRUE ensure we epxand heap by at least "byteRequired" bytes
 * @return Number of bytes required or 0 if current free already meets the desired bytes free
 */
UDATA
MM_MemorySubSpaceTarok::calculateExpandSize(MM_EnvironmentBase *env, UDATA bytesRequired, bool expandToSatisfy)
{
	UDATA currentFree, minimumFree, desiredFree;
	UDATA expandSize = 0;
	
	Trc_MM_MemorySubSpaceTarok_calculateExpandSize_Entry(env->getLanguageVMThread(), bytesRequired);
	
	/* How much heap space currently free ? */
	currentFree = getApproximateActiveFreeMemorySize();
	
	/* and how much do we need free after this GC to meet -Xminf ? */
	minimumFree = (getActiveMemorySize() / _extensions->heapFreeMinimumRatioDivisor) * _extensions->heapFreeMinimumRatioMultiplier;
	
	/* The desired free is the sum of these 2 rounded to heapAlignment */
	desiredFree= MM_Math::roundToCeiling(_extensions->heapAlignment, minimumFree + bytesRequired);

	if(desiredFree <= currentFree) {
		/* Only expand if we didn't expand in last _extensions->heapExpansionStabilizationCount global collections */
		/* Note that the gcCount includes System GCs, PGCs, AFs and GMP increments */
		UDATA gcCount = _extensions->globalVLHGCStats.gcCount;

		if (_extensions->heap->getResizeStats()->getLastHeapExpansionGCCount() + _extensions->heapExpansionStabilizationCount <= gcCount )
		{
			/* Determine if its time for a ratio expand ? */
			expandSize = checkForRatioExpand(env,bytesRequired);
		}

		if (expandSize > 0 ) {
			/* Remember reason for expansion for later */
			_extensions->heap->getResizeStats()->setLastExpandReason(GC_RATIO_TOO_HIGH);
		} 
	} else {
		/* Calculate how much we need to expand the heap by in order to meet the 
		 * allocation request and the desired -Xminf amount AFTER expansion 
		 */
		expandSize= ((desiredFree - currentFree) / (100 - _extensions->heapFreeMinimumRatioMultiplier)) * _extensions->heapFreeMinimumRatioDivisor;

		if (expandSize > 0 ) {
			/* Remember reason for contraction for later */
			_extensions->heap->getResizeStats()->setLastExpandReason(FREE_SPACE_LESS_MINF);
		}	
	}

	if (expandToSatisfy){
		// TO DO - If the last free chunk abuts the end of the heap we only need
		// to expand by (bytesRequired - size of last free chunk) to satisfy the 
		// request.
		expandSize = OMR_MAX(bytesRequired, expandSize);
		_extensions->heap->getResizeStats()->setLastExpandReason(EXPAND_DESPERATE);
	}

	if (expandSize > 0) {
		/* Adjust the expand size within the specified minimum and maximum expansion amounts
		 * (-Xmine and -Xmaxe command line options) if expansion is required
		 */ 
		expandSize = adjustExpansionWithinFreeLimits(env, expandSize);
	
		/* and adjust to user increment values (Xmoi) */
		expandSize = adjustExpansionWithinUserIncrement(env, expandSize);
	}
	
	/* Expand size now in range -Xmine =< expandSize <= -Xmaxe */
	
	/* Adjust within -XsoftMx limit */
	if (expandToSatisfy){
		/* we need at least bytesRequired or we will get an OOM */
		expandSize = adjustExpansionWithinSoftMax(env, expandSize, bytesRequired);
	} else {
		/* we are adjusting based on other command line options, so fully respect softmx,
		 * the minimum expand it can allow in this case is 0
		 */
		expandSize = adjustExpansionWithinSoftMax(env, expandSize, 0);
	}
	
	Trc_MM_MemorySubSpaceTarok_calculateExpandSize_Exit1(env->getLanguageVMThread(), desiredFree, currentFree, expandSize);
	return expandSize;
}


UDATA
MM_MemorySubSpaceTarok::calculateCollectorExpandSize(MM_EnvironmentBase *env)
{
	Trc_MM_MemorySubSpaceTarok_calculateCollectorExpandSize_Entry(env->getLanguageVMThread());
	
	/* expand by a single region */
	UDATA expandSize = _heapRegionManager->getRegionSize(); 
	
	/* Adjust within -XsoftMx limit */
	expandSize = adjustExpansionWithinSoftMax(env, expandSize,0);
	
	Trc_MM_MemorySubSpaceTarok_calculateCollectorExpandSize_Exit1(env->getLanguageVMThread(), expandSize);

	return expandSize; 
}

/**
 * Determine if a  expand is required 
 * @note We use the approximate heap size to account for defered work that may during execution free up more memory.
 * @return expand size if ratio expand required or 0 otherwise
 */
UDATA
MM_MemorySubSpaceTarok::checkForRatioExpand(MM_EnvironmentBase *env, UDATA bytesRequired)
{
	Trc_MM_MemorySubSpaceTarok_checkForRatioExpand_Entry(env->getLanguageVMThread(), bytesRequired);
	
	U_32 gcPercentage;
	UDATA currentFree, maxFree;

	/* How many bytes currently free ? */	 
	currentFree = getApproximateActiveFreeMemorySize();
						 
	/* How many bytes free would constitute -Xmaxf at current heap size ? */				 
	maxFree = (UDATA)(((U_64)getActiveMemorySize()  * _extensions->heapFreeMaximumRatioMultiplier)
														 / ((U_64)_extensions->heapFreeMaximumRatioDivisor));
														 
	/* If we have hit -Xmaxf limit already ...return immediately */													 
	if (currentFree >= maxFree) { 
		Trc_MM_MemorySubSpaceTarok_checkForRatioExpand_Exit1(env->getLanguageVMThread());
		return 0;
	}														 
						 
	/* Ask the collector for percentage of time being spent in GC */
	if(NULL != _collector) {
		gcPercentage = _collector->getGCTimePercentage(env);
	} else {
		gcPercentage= _extensions->getGlobalCollector()->getGCTimePercentage(env);
	}
	
	/* Is too much time is being spent in GC? */
	if (gcPercentage < _extensions->heapExpansionGCTimeThreshold) {
		Trc_MM_MemorySubSpaceTarok_checkForRatioExpand_Exit2(env->getLanguageVMThread(), gcPercentage);
		return 0;
	} else { 
		/* 
		 * We are spending too much time in gc and are below -Xmaxf free space so expand to 
		 * attempt to reduce gc time.
		 * 
		 * At this point we already know we have -Xminf storage free. 
		 * 
		 * We expand by HEAP_FREE_RATIO_EXPAND_MULTIPLIER percentage provided this does not take us above
		 * -Xmaxf. If it does we expand up to the -Xmaxf limit.
		 */ 
		UDATA ratioExpandAmount, maxExpandSize;
			
		/* How many bytes (maximum) do we want to expand by ?*/
		ratioExpandAmount =(UDATA)(((U_64)getActiveMemorySize()  * HEAP_FREE_RATIO_EXPAND_MULTIPLIER)
						 / ((U_64)HEAP_FREE_RATIO_EXPAND_DIVISOR));		
						 
		/* If user has set -Xmaxf1.0 then they do not care how much free space we have
		 * so no need to limit expand size here. Expand size will later be checked  
		 * against -Xmaxe value.
		 */
		if (_extensions->heapFreeMaximumRatioMultiplier < 100 ) {					 
			
			/* By how much could we expand current heap without taking us above -Xmaxf bytes in 
			 * resulting new (larger) heap
			 */ 
			maxExpandSize = ((maxFree - currentFree) / (100 - _extensions->heapFreeMaximumRatioMultiplier)) *
								_extensions->heapFreeMaximumRatioDivisor;
				
			ratioExpandAmount = OMR_MIN(maxExpandSize,ratioExpandAmount);
		}	

		/* Round expansion amount UP to heap alignment */
		ratioExpandAmount = MM_Math::roundToCeiling(_extensions->heapAlignment, ratioExpandAmount);	
				
		Trc_MM_MemorySubSpaceTarok_checkForRatioExpand_Exit3(env->getLanguageVMThread(), gcPercentage, ratioExpandAmount);
		return ratioExpandAmount;
	}
}	
	
/**
 * Determine if a ratio contract is required.
 * Calculate the percentage of GC time relative to total execution time, and if this percentage
 * is less than a particular threshold, it is time to contract.
 * @return true if a contraction is desirable, false otherwise.
 */
bool
MM_MemorySubSpaceTarok::checkForRatioContract(MM_EnvironmentBase *env) 
{
	Trc_MM_MemorySubSpaceTarok_checkForRatioContract_Entry(env->getLanguageVMThread());
	
	/* Ask the collector for percentage of time spent in GC */
	U_32 gcPercentage;
	if(NULL != _collector) {
		gcPercentage = _collector->getGCTimePercentage(env);
	} else {
		gcPercentage = _extensions->getGlobalCollector()->getGCTimePercentage(env);
	}
	
	/* If we are spending less than extensions->heapContractionGCTimeThreshold of
	 * our time in gc then we should attempt to shrink the heap
	 */ 	
	if (gcPercentage > 0 && gcPercentage < _extensions->heapContractionGCTimeThreshold) {
		Trc_MM_MemorySubSpaceTarok_checkForRatioContract_Exit1(env->getLanguageVMThread(), gcPercentage);
		return true;
	} else {
		Trc_MM_MemorySubSpaceTarok_checkForRatioContract_Exit2(env->getLanguageVMThread(), gcPercentage);
		return false;
	}			
}


/**
 * Compare the specified expand amount with the specified minimum and maximum expansion amounts
 * (-Xmine and -Xmaxe command line options) and round the amount to within these limits
 * @return Updated expand size
 */		
MMINLINE UDATA		
MM_MemorySubSpaceTarok::adjustExpansionWithinFreeLimits(MM_EnvironmentBase *env, UDATA expandSize)
{		
	UDATA result = expandSize;
	
	if (expandSize > 0 ) { 
		if(_extensions->heapExpansionMinimumSize > 0 ) {
			result = OMR_MAX(_extensions->heapExpansionMinimumSize, expandSize);
		}
		
		if(_extensions->heapExpansionMaximumSize > 0 ) {
			result = OMR_MIN(_extensions->heapExpansionMaximumSize, expandSize);
		}	
	}
	return result;
}

/**
 * Compare the specified expand amount with -XsoftMX value
 * @return Updated expand size
 */		
MMINLINE UDATA		
MM_MemorySubSpaceTarok::adjustExpansionWithinSoftMax(MM_EnvironmentBase *env, UDATA expandSize, UDATA minimumBytesRequired)
{
	MM_Heap * heap = env->getExtensions()->getHeap();
	UDATA actualSoftMx = heap->getActualSoftMxSize(env);
	UDATA activeMemorySize = getActiveMemorySize();
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	
	if (0 != actualSoftMx) {
		if ((minimumBytesRequired != 0) && ((activeMemorySize + minimumBytesRequired) > actualSoftMx)) {
			if (J9_EVENT_IS_HOOKED(MM_GCExtensions::getExtensions(env)->omrHookInterface, J9HOOK_MM_OMR_OOM_DUE_TO_SOFTMX)){
				ALWAYS_TRIGGER_J9HOOK_MM_OMR_OOM_DUE_TO_SOFTMX(MM_GCExtensions::getExtensions(env)->omrHookInterface, env->getOmrVMThread(),
						j9time_hires_clock(), heap->getMaximumMemorySize(), heap->getActiveMemorySize(), MM_GCExtensions::getExtensions(env)->softMx, minimumBytesRequired);
				actualSoftMx = heap->getActualSoftMxSize(env);
			}
		}
		if (actualSoftMx < activeMemorySize) {
			/* if our softmx is smaller than our currentsize, we should be contracting not expanding */
			expandSize = 0;
		} else if((activeMemorySize + expandSize) > actualSoftMx) {
			/* we would go past our -XsoftMx so just expand up to it instead */
			expandSize = actualSoftMx - activeMemorySize;
		}
	}
	return expandSize;
}	
