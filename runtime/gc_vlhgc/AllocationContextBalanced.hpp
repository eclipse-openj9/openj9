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
 * @ingroup GC_Modron_Base
 */

#if !defined(ALLOCATIONCONTEXTBALANCED_HPP_)
#define ALLOCATIONCONTEXTBALANCED_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "modronopt.h"

#include "AllocationContextTarok.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "RegionListTarok.hpp"

class MM_AllocateDescription;
class MM_AllocationContextMultiTenant;
class MM_EnvironmentBase;
class MM_MemorySubSpaceTarok;


class MM_AllocationContextBalanced : public MM_AllocationContextTarok
{
/* Data members / Types */
public:
protected:
private:
	MM_LightweightNonReentrantLock _contextLock; /**< protects updates to the context's region cache */
	MM_LightweightNonReentrantLock _freeListLock; /**< protects updates to the context's free list (managed distinctly from the rest of the context to support region stealing */
	MM_MemorySubSpaceTarok *_subspace; /**< the subspace from which this context allocates and refreshes itself */
	MM_HeapRegionDescriptorVLHGC *_allocationRegion;	/**< The region currently satisfying allocations within this context - also the most recently replenished/stolen region in this context */
	MM_RegionListTarok _nonFullRegions;	/**< Regions which failed to satisfy a large object allocation but which are not yet full.  The _allocationRegion is added to this list when it fails to satisfy an object allocation but this list will quickly be consumed by TLH allocation requests which can soak up all remaining space.  This list is consulted after _allocationRegion but before replenishment or theft. */
	MM_RegionListTarok _discardRegionList; /**< The list of MPBP regions currently privately owned by this context but either too full or too fragmented to be used to satisfy allocations.  These regions must be flushed back to the subspace before a collection */
	MM_RegionListTarok _flushedRegions; /**< The list of MPBP regions which have been flushed from active use, for a GC, and have unknown stats (at any point after the GC, however, these regions could all be migrated to _ownedRegions) */
	MM_RegionListTarok _freeRegions; /**< The list regions which are owned by this context but currently marked as FREE */
	MM_RegionListTarok _idleMPBPRegions; /**< The list regions which are owned by this context, currently marked as BUMP_ALLOCATED_IDLE, but contain no objects (this also implies that they can migrate to other contexts on this node, for free, since the receiver isn't using them) */
	UDATA _freeMemorySize;	/**< The amount of free memory currently managed by the context in the _ownedRegions list only (note that dark matter and small holes are not considered free memory).  This value is always accurate (that is, there is no time when it becomes out of sync with the actual amount of free memory managed by the context). */
	UDATA _regionCount[MM_HeapRegionDescriptor::LAST_REGION_TYPE]; /**< Count of regions that are owned by this AC (accurate only for TGC purposes) */
	UDATA _threadCount;/**< Count of mutator threads that allocate from this AC (accurate only for TGC purposes) */
	UDATA _numaNode; /**< the NUMA node this context is associated with, or 0 if none */
	MM_AllocationContextBalanced *_nextSibling; /**< Instances of the receiver are built into a circular linked-list.  This points to the next adjacent downstream neighbour in this list (may be pointing to this if there is only one context in the node) */
	MM_AllocationContextBalanced *_cachedReplenishPoint;	/**< The sibling context which most recently replenished the receiver */
	MM_AllocationContextBalanced *_stealingCousin;	/**< A context in the "next" node which the receiver will use for spilling memory requests across NUMA nodes.  Points back at the receiver if this is non-NUMA */
	MM_AllocationContextBalanced *_nextToSteal;	/**< A pointer to the next context we will try to steal from (we steal in a round-robin to try to distribute the heap's asymmetry).  Points back at the receiver if this is non-NUMA */
	MM_HeapRegionManager *_heapRegionManager; /**< A cached pointer to the HeapRegionManager */
	UDATA *_freeProcessorNodes;	/**< The array listing all the NUMA node numbers which account for the nodes with processors but no memory plus an empty slot for each context to use (element 0 is used by this context) - this is used when setting affinity */
	UDATA _freeProcessorNodeCount;	/**< The length, in elements, of the _freeProcessorNodes array (always at least 1 after startup) */

/* Methods */
public:
	static MM_AllocationContextBalanced *newInstance(MM_EnvironmentBase *env, MM_MemorySubSpaceTarok *subspace, UDATA numaNode, UDATA allocationContextNumber);
	virtual void flush(MM_EnvironmentBase *env);
	virtual void flushForShutdown(MM_EnvironmentBase *env);

	/**
	 * Ideally, this would only be understood by sub-classes which know about TLH allocation but we will use runtime assertions to ensure this is safe, for now
	 */
	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_ObjectAllocationInterface *objectAllocationInterface, bool shouldCollectOnFailure);
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure);
#if defined(J9VM_GC_ARRAYLETS)
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure);
#endif /* defined(J9VM_GC_ARRAYLETS) */

	/**
	 * Acquire a new region to be placed in the active set of regions from which to allocate.
	 * @param env GC thread.
	 * @return A region descriptor that has been acquired and put into the active to be consumed from rotation.
	 * @note This will immediately move the acquired region from _ownedRegions to _flushedRegions
	 */
	virtual MM_HeapRegionDescriptorVLHGC *collectorAcquireRegion(MM_EnvironmentBase *env);

	/**
	 * @See MM_AllocationContext::allocate
	 */
	virtual void *allocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType);

	/**
	 * Perform an allocate of the given allocationType and return it.  Note that the receiver can assume that either the context is locked or the calling thread has exclusive.
	 *
	 * @param[in] env The calling thread
	 * @param[in] objectAllocationInterface The allocation interface through which the original allocation call was initiated (only used by TLH allocations, can be NULL in other cases)
	 * @param[in] allocateDescription The description of the requested allocation
	 * @param[in] allocationType The type of allocation to perform
	 *
	 * @return The result of the allocation (NULL on failure)
	 */
	virtual void *lockedAllocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType);

	/**
	 * Called when the given region has been deemed empty, but is not a FREE region.  The receiver is responsible for changing the region type and ensuring that it is appropriately stored.
	 * @param env[in] The calling thread (typically the master GC thread)
	 * @param region[in] The region to recycle
	 */
	virtual void recycleRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Called during tear down of a subspace to discard any regions which comprise the subspace.
	 * The region should be FREE (not IDLE!) on completion of this call.
	 * @param env[in] The calling thread (typically the master GC thread)
	 * @param region[in] The region to tear down
	 */
	virtual void tearDownRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Called during both expansion and during region recycling (if the region was owned by this context, it will come through this path when
	 * recycled).  This method can only accept FREE regions.
	 * @param env[in] The thread which expanded the region or recycled it
	 * @param region[in] The region which should be added to our free list
	 */
	virtual void addRegionToFreeList(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Reset region count, used by TGC
	 * @param regionType type of the region that we reset the count for
	 */
	virtual void resetRegionCount(MM_HeapRegionDescriptor::RegionType regionType) { _regionCount[regionType] = 0; }
	/**
	 * Increment region count, used by TGC
	 * @param regionType type of the region that we update the count for
	 */
	virtual void incRegionCount(MM_HeapRegionDescriptor::RegionType regionType) { _regionCount[regionType] += 1; }

	/**
	 * Return the region count associated with this AC, used by TGC
	 * @param regionType type of the region that we get the count for
	 * @return region count
	 */
	virtual UDATA getRegionCount(MM_HeapRegionDescriptor::RegionType regionType) { return _regionCount[regionType]; }
	/**
	 * Reset mutator count, used by TGC
	 */
	virtual void resetThreadCount() { _threadCount = 0; }

	/**
	 * Increment mutator count, used by TGC
	 */
	virtual void incThreadCount() { _threadCount += 1; }

	/**
	 * Return the mutator count associated with this AC, used by TGC
	 *
	 * @return mutator count
	 */
	virtual UDATA getThreadCount() { return _threadCount; }

	/**
	 * Return the NUMA node with which this AC is associated, or 0 if none
	 *
	 * @return associated NUMA node
	 */
	virtual UDATA getNumaNode() { return _numaNode; }

	/**
	 * Return the amount of free memory currently managed by the context.  This value is always accurate (that is, there is no time when it becomes
	 * out of sync with the actual amount of free memory managed by the context).
	 *
	 * @return The free memory managed by the receiver, in bytes.
	 */
	virtual UDATA getFreeMemorySize();

	/**
	 * Return the number of free (empty) regions currently managed by the context.   This value is always accurate (that is, there is no time when it becomes
	 * out of sync with the actual number of regions managed by the context).
	 *
	 * @return The count of free regions managed by the receiver.
	 */
	virtual UDATA getFreeRegionCount();

	/**
	 * Sets the downstream sibling in the circularly linked-list of contexts on a given node.
	 * @param sibling[in] The next downstream neighbour in the list (might be this if there is only one context on the node)
	 */
	void setNextSibling(MM_AllocationContextBalanced *sibling);

	/**
	 * @return The next region in the circular linked list of contexts on this node (could be this if there is only one context on the node)
	 */
	MM_AllocationContextBalanced *getNextSibling() { return _nextSibling; }

	/**
	 * Sets the _stealingCousin instance variable (can only be called once!).
	 * @param cousin[in] The context onto which the receiver will spill failed allocates within its node (must not be NULL)
	 */
	void setStealingCousin(MM_AllocationContextBalanced *cousin);

	/**
	 * @return The first context onto which the receiver will spill allocation failures within its own node
	 */
	MM_AllocationContextBalanced *getStealingCousin() { return _stealingCousin; }

	/**
	 * Called to reset the largest free entry in all the MemoryPoolBumpPointer instances in the regions managed by the receiver.
	 */
	virtual void resetLargestFreeEntry();

	/**
	 * @return The largest free entry out of all the pools managed by the receiver.
	 */
	virtual UDATA getLargestFreeEntry();

	/**
	 * Called to move the given region directly from the receiver and into the target context instance.  Note that this method can only
	 * be called in one thread on any instance since it directly manipulates lists across contexts.  The caller is responsible for
	 * ensuring that these threading semantics are preserved.
	 * @param region[in] The region to migrate from the receiver
	 * @param newOwner[in] The context to which the given region is being migrated
	 */
	virtual void migrateRegionToAllocationContext(MM_HeapRegionDescriptorVLHGC *region, MM_AllocationContextTarok *newOwner);

	/**
	 * Called by migrateRegionToAllocationContext on the target context receiving the migrated region.
	 * The receiver will insert the migrated context in its list.
	 *
	 * @param region[in] The region to accept
	 */
	virtual void acceptMigratingRegion(MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * This helper merely forwards the resetHeapStatistics call on to all the memory pools owned by the receiver.
	 * @param globalCollect[in] True if this was a global collect (blindly passed through)
	 */
	virtual void resetHeapStatistics(bool globalCollect);

	/**
	 * This helper merely forwards the mergeHeapStats call on to all the memory pools owned by the receiver.
	 * @param heapStats[in/out] The stats structure to receive the merged data (blindly passed through)
	 * @param includeMemoryType[in] The memory space type to use in the merge (blindly passed through)
	 */
	virtual void mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType);

	/**
	 * Used by TGC to get the count of regions owned by this context.  Differentiates between regions node-local to the context and node-foreign.
	 * @param localCount[out] The number of regions owned by the receiver which are bound to the node in which the receiver logically operates
	 * @param foreignCount[out] The number of regions owned by the receiver which are NOT bound to the node in which the receiver logically operates
	 */
	virtual void getRegionCount(UDATA *localCount, UDATA *foreignCount);

	/**
	 * Remove the specified region from the flushed regions list.
	 *
	 * @param region[in] The region to remove from the list
	 */
	virtual void removeRegionFromFlushedList(MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * @See MM_AllocationContextTarok::setNumaAffinityForThread
	 */
	virtual bool setNumaAffinityForThread(MM_EnvironmentBase *env);

	
protected:
	virtual void tearDown(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	MM_AllocationContextBalanced(MM_MemorySubSpaceTarok *subspace, UDATA numaNode, UDATA allocationContextNumber)
		: MM_AllocationContextTarok(allocationContextNumber, MM_AllocationContextTarok::BALANCED)
		, _subspace(subspace)
		, _allocationRegion(NULL)
		, _nonFullRegions()
		, _discardRegionList()
		, _flushedRegions()
		, _freeRegions()
		, _idleMPBPRegions()
		, _freeMemorySize(0)
		, _threadCount(0)
		, _numaNode(numaNode)
		, _nextSibling(NULL)
		, _cachedReplenishPoint(NULL)
		, _stealingCousin(NULL)
		, _nextToSteal(NULL)
		, _heapRegionManager(NULL)
		, _freeProcessorNodes(NULL)
		, _freeProcessorNodeCount(0)
	{
		_typeId = __FUNCTION__;
	}

	virtual void *lockedReplenishAndAllocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType);
	virtual MM_HeapRegionDescriptorVLHGC * selectRegionForContraction(MM_EnvironmentBase *env);
	
private:

	/**
	 * Locks the "common" _contextLock (as opposed to the _freeListLock).
	 */
	void lockCommon();
	/**
	 * Locks the "common" _contextLock (as opposed to the _freeListLock).
	 */
	void unlockCommon();

	/**
	 * Tries to acquire an MPBP region from the receiver (either from an existing, but idle, BUMP_ALLOCATED or by converting a free region).
	 * If the receiver does not have a valid candidate it will check its siblings and cousins for one.
	 * The region that this method returns will have been removed from the receiver's management and its owning context is set to requestingContext.
	 *
	 * @note Caller must hold this context's @ref _contextLock
	 *
	 * @param env[in] The thread attempting the allocation
	 * @param subspace[in] The subSpace to which the allocated pool must be attached
	 * @param requestingContext[in] The context requesting a region from the receiver
	 * @return The region or NULL if there were none available in the heap
	 */
	MM_HeapRegionDescriptorVLHGC *acquireMPBPRegionFromHeap(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_AllocationContextTarok *requestingContext);

	/**
	 * Tries to acquire a FREE region from the receiver (either from an existing FREE region or by converting an idle region).
	 * If the receiver does not have a valid candidate it will check its siblings and cousins for one.
	 * The region that this method returns will have been removed from the receiver's management and its owning context is set to requestingContext.
	 *
	 * @note Caller must hold this context's @ref _contextLock
	 *
	 * @param env[in] The thread attempting the allocation
	 * @return The region or NULL if there were none available in the heap
	 */
	MM_HeapRegionDescriptorVLHGC *acquireFreeRegionFromHeap(MM_EnvironmentBase *env);
	
	/**
	 * Returns a region descriptor of BUMP_ALLOCATED type on the node where the receiver is resident.  The region may not have been found
	 * in the receiver (it may have been managed by a sibling) but it will be placed under the management of the given requestingContext
	 * before this call returns.
	 * @note The caller must own the receiver's _contextLock
	 * @param env[in] The thread requesting the region
	 * @param subSpace[in] The subSpace to which the allocated pool must be attached
	 * @param requestingContext[in] The context which is requesting the region and asking this context to search its node
	 * @return A region of type BUMP_ALLOCATED (or NULL if no BUMP_ALLOCATED regions could be found or created on the receiver's node)
	 */
	MM_HeapRegionDescriptorVLHGC *acquireMPBPRegionFromNode(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocationContextTarok *requestingContext);

	/**
	 * Returns a region descriptor of FREE type on the node where the receiver is resident.  The region may not have been found in the
	 * receiver (it may have been managed by a sibling) but it will not be under any context's management, when it is returned and the
	 * caller accepts responsibility for managing it.
	 * @note The caller must own the receiver's _contextLock
	 * @param env[in] The thread requesting the free region
	 * @return A region of type FREE (or NULL if no FREE regions could be found or created on the receiver's node)
	 */
	MM_HeapRegionDescriptorVLHGC *acquireFreeRegionFromNode(MM_EnvironmentBase *env);

	/**
	 * Tries to acquire an MPBP region from the receiver (either from an existing, but idle, BUMP_ALLOCATED or by converting a free region).
	 * The region that this method returns will have been removed from the receiver's management and its owning context is set to requestingContext.
	 *
	 * @param env[in] The thread attempting the allocation
	 * @param subSpace[in] The subSpace to which the allocated pool must be attached
	 * @param requestingContext[in] The context requesting a region from the receiver
	 * @return The region or NULL if there were none available in the receiver
	 */
	MM_HeapRegionDescriptorVLHGC *acquireMPBPRegionFromContext(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocationContextTarok *requestingContext);

	/**
	 * Tries to acquire a FREE region from the receiver (either from an existing FREE region or by converting an idle region).
	 * The region that this method returns will have been removed from the receiver's management and its owning context is set to requestingContext.
	 *
	 * @param env[in] The thread attempting the allocation
	 * @return The region or NULL if there were none available in the receiver
	 */
	MM_HeapRegionDescriptorVLHGC *acquireFreeRegionFromContext(MM_EnvironmentBase *env);

	/**
	 * Perform a TLH allocation.  Note that the receiver can assume that either the context is locked or the calling thread has exclusive.
	 *
	 * @param[in] env The calling thread
	 * @param[in] allocateDescription The allocation to perform
	 * @param[in] objectAllocationInterface The interface through which the allocation request was initiated (required to initialize the TLH)
	 *
	 * @return The base pointer of the allocated TLH (or NULL, if the allocation failed)
	 */
	void *lockedAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_ObjectAllocationInterface *objectAllocationInterface);
	/**
	 * Allocate an object.  Note that the receiver can assume that either the context is locked or the calling thread has exclusive.
	 *
	 * @param[in] env The calling thread
	 * @param[in] allocateDescription The allocation to perform
	 *
	 * @return The address of the allocated object (or NULL, if the allocation failed)
	 */
	void *lockedAllocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription);

#if defined(J9VM_GC_ARRAYLETS)
	/**
	 * Allocate an arraylet leaf.  Note that the receiver can assume that either the context is locked or the calling thread has exclusive.
	 * NOTE:  The returned arraylet leaf is UNINITIALIZED MEMORY (since it can't be zeroed under lock) so the caller must zero it before it
	 * can be seen by the collector or user code.
	 *
	 * @param[in] env The calling thread
	 * @param[in] allocateDescription The allocation to perform
	 * @param[in] freeRegionForArrayletLeaf The region to use for the allocation
	 *
	 * @return The address of the leaf
	 */
	void *lockedAllocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_HeapRegionDescriptorVLHGC *freeRegionForArrayletLeaf);
#endif /* defined(J9VM_GC_ARRAYLETS) */

	/**
	 * Common implementation for flush() and flushForShutdown()
	 */
	void flushInternal(MM_EnvironmentBase *env);

	/**
	 * Replenishes the receiver's active region list.  Called by lockedReplenishAndAllocate in the cases where we aren't allocating a leaf.
	 * @note The calling env must own the receiver's _contextLock or have exclusive VM access
	 *
	 * @param env[in] A GC thread (must own either _contextLock or have exclusive access)
	 * @param payTax Flag indicating whether taxation restrictions should apply
	 * @return region descriptor that was used to replenish the active region, or NULL if no descriptor was found.
	 */
	MM_HeapRegionDescriptorVLHGC *internalReplenishActiveRegion(MM_EnvironmentBase *env, bool payTax);

	/**
	 * A helper which increments the given count of local and foreign regions based on the location of the given region.
	 * A region is considered "local" if it is bound to the node managed by the receiver.
	 * @param region[in] A region to check
	 * @param localCount[in/out] The number of regions owned by the receiver which are bound to the node in which the receiver logically operates
	 * @param foreignCount[in/out] The number of regions owned by the receiver which are NOT bound to the node in which the receiver logically operates
	 */
	void accountForRegionLocation(MM_HeapRegionDescriptorVLHGC *region, UDATA *localCount, UDATA *foreignCount);

	/**
	 * A helper counts the regions in a specific region list and differentiates between node-local and node-foreign regions
	 * @param localCount[out] The number of regions owned by the receiver which are bound to the node in which the receiver logically operates
	 * @param foreignCount[out] The number of regions owned by the receiver which are NOT bound to the node in which the receiver logically operates
	 */
	void countRegionsInList(MM_RegionListTarok *list, UDATA *localCount, UDATA *foreignCount);

	/**
	 * Helper for collectorAcquireRegion
	 * @param env The current GC thread.
	 * @return A region descriptor that has been acquired and put into the active to be consumed from rotation.
	 */
	MM_HeapRegionDescriptorVLHGC *internalCollectorAcquireRegion(MM_EnvironmentBase *env);

};

#endif /* ALLOCATIONCONTEXTBALANCED_HPP_ */
