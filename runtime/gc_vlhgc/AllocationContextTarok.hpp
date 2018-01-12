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
 * @ingroup GC_Modron_Base
 */

#if !defined(ALLOCATIONCONTEXTTAROK_HPP_)
#define ALLOCATIONCONTEXTTAROK_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "modronopt.h"

#include "AllocationContext.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemorySubSpace.hpp"
#include "RegionListTarok.hpp"

class MM_EnvironmentBase;
class MM_HeapRegionDescriptorVLHGC;
class MM_HeapRegionManager;
class MM_MemorySubSpaceTarok;

class MM_AllocationContextTarok : public MM_AllocationContext
{

/* Data members / Types */
public:
	enum AllocationContextType {
		BALANCED = 0                           /**< Represents a regular balanced AC */
		, MULTI_TENANT = 1                     /**< Represents a multi-tenant AC */
	};
protected:
private:
	const UDATA _allocationContextNumber;	/**< A unique 0-indexed number assigned to this context instance (to be used for indexing into meta-data arrays, etc) */
	const AllocationContextType _allocationContextType; /**< The type of this AC, set during instantiation and used for instanceof checks **/

/* Methods */
public:

	/**
	 * @return The context number with which this instance was initialized
	 */
	UDATA getAllocationContextNumber() { return _allocationContextNumber; }

	/**
	 * @return The type of this allocation context.
	 */
	MMINLINE AllocationContextType getAllocationContextType() { return _allocationContextType; }

	/**
	 * Acquire a new region to be placed in the active set of regions from which to allocate.
	 * @param env GC thread.
	 * @return A region descriptor that has been acquired and put into the active to be consumed from rotation.
	 * @note This will immediately move the acquired region from _ownedRegions to _flushedRegions
	 */
	virtual MM_HeapRegionDescriptorVLHGC *collectorAcquireRegion(MM_EnvironmentBase *env) = 0;

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
	virtual void *lockedAllocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType) = 0;
	
	/**
	 * Called when the given region has been deemed empty, but is not a FREE region.  The receiver is responsible for changing the region type and ensuring that it is appropriately stored.
	 * @param env[in] The calling thread (typically the master GC thread)
	 * @param region[in] The region to recycle
	 */
	virtual void recycleRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region) = 0;

	/**
	 * Called during tear down of a subspace to discard any regions which comprise the subspace.
	 * The region should be FREE (not IDLE!) on completion of this call.
	 * @param env[in] The calling thread (typically the master GC thread)
	 * @param region[in] The region to tear down
	 */
	virtual void tearDownRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region) = 0;

	/**
	 * Called during both expansion and during region recycling (if the region was owned by this context, it will come through this path when
	 * recycled).  This method can only accept FREE regions.
	 * @param env[in] The thread which expanded the region or recycled it
	 * @param region[in] The region which should be added to our free list
	 */
	virtual void addRegionToFreeList(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region) = 0;

	/**
	 * Reset region count, used by TGC
	 * @param regionType type of the region that we reset the count for
	 */	
	virtual void resetRegionCount(MM_HeapRegionDescriptor::RegionType regionType) = 0;
	/**
	 * Increment region count, used by TGC
	 * @param regionType type of the region that we update the count for
	 */	
	virtual void incRegionCount(MM_HeapRegionDescriptor::RegionType regionType) = 0;

	/**
	 * Return the region count associated with this AC, used by TGC
	 * @param regionType type of the region that we get the count for
	 * @return region count 
	 */	
	virtual UDATA getRegionCount(MM_HeapRegionDescriptor::RegionType regionType) = 0;

	/**
	 * Reset mutator count, used by TGC
	 */	
	virtual void resetThreadCount() = 0;

	/**
	 * Increment mutator count, used by TGC
	 */		
	virtual void incThreadCount() = 0;

	/**
	 * Return the mutator count associated with this AC, used by TGC
	 * 
	 * @return mutator count 
	 */
	virtual UDATA getThreadCount() = 0;
		
	/**
	 * Return the NUMA node with which this AC is associated, or 0 if none
	 * 
	 * @return associated NUMA node
	 */
	virtual UDATA getNumaNode() = 0;

	/**
	 * Return the amount of free memory currently managed by the context.  This value is always accurate (that is, there is no time when it becomes
	 * out of sync with the actual amount of free memory managed by the context).
	 * 
	 * @return The free memory managed by the receiver, in bytes.
	 */
	virtual UDATA getFreeMemorySize() = 0;
	
	/**
	 * Return the number of free (empty) regions currently managed by the context.   This value is always accurate (that is, there is no time when it becomes
	 * out of sync with the actual number of regions managed by the context).
	 * 
	 * @return The count of free regions managed by the receiver.
	 */
	virtual UDATA getFreeRegionCount() = 0;

	/**
	 * Called to reset the largest free entry in all the MemoryPoolBumpPointer instances in the regions managed by the receiver.
	 */
	virtual void resetLargestFreeEntry() = 0;

	/**
	 * @return The largest free entry out of all the pools managed by the receiver.
	 */
	virtual UDATA getLargestFreeEntry() = 0;

	/**
	 * Called to move the given region directly from the receiver and into the target context instance.  Note that this method can only
	 * be called in one thread on any instance since it directly manipulates lists across contexts.  The caller is responsible for
	 * ensuring that these threading semantics are preserved.
	 * @param region[in] The region to migrate from the receiver
	 * @param newOwner[in] The context to which the given region is being migrated
	 */
	virtual void migrateRegionToAllocationContext(MM_HeapRegionDescriptorVLHGC *region, MM_AllocationContextTarok *newOwner) = 0;

	/**
	 * Called by migrateRegionToAllocationContext on the target context receiving the migrated region.
	 * The receiver will insert the migrated context in its list.
	 *
	 * @param region[in] The region to accept
	 */
	virtual void acceptMigratingRegion(MM_HeapRegionDescriptorVLHGC *region) = 0;

	/**
	 * This helper merely forwards the resetHeapStatistics call on to all the memory pools owned by the receiver.
	 * @param globalCollect[in] True if this was a global collect (blindly passed through)
	 */
	virtual void resetHeapStatistics(bool globalCollect) = 0;

	/**
	 * This helper merely forwards the mergeHeapStats call on to all the memory pools owned by the receiver.
	 * @param heapStats[in/out] The stats structure to receive the merged data (blindly passed through)
	 * @param includeMemoryType[in] The memory space type to use in the merge (blindly passed through)
	 */
	virtual void mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType) = 0;

	/**
	 * Used by TGC to get the count of regions owned by this context.  Differentiates between regions node-local to the context and node-foreign.
	 * @param localCount[out] The number of regions owned by the receiver which are bound to the node in which the receiver logically operates
	 * @param foreignCount[out] The number of regions owned by the receiver which are NOT bound to the node in which the receiver logically operates
	 */
	virtual void getRegionCount(UDATA *localCount, UDATA *foreignCount) = 0;

	/**
	 * Remove the specified region from the flushed regions list.
	 *
	 * @param region[in] The region to remove from the list
	 */
	virtual void removeRegionFromFlushedList(MM_HeapRegionDescriptorVLHGC *region) = 0;

	/**
	 * Set the NUMA affinity for the current thread.
	 * @param env[in] the current thread
	 * @return true upon success, false otherwise
	 */
	virtual bool setNumaAffinityForThread(MM_EnvironmentBase *env) = 0;

	/**
	 * Clear the NUMA affinity of the current thread.
	 * Call the thread library to bind the thread to all eligible CPUs.
	 * @param env[in] the current thread
	 * @see MM_RuntimeExecManager
	 */
	MMINLINE void clearNumaAffinityForThread(MM_EnvironmentBase *env) { env->setNumaAffinity(NULL, 0); }

	/**
	 * Determines whether or not a region should be migrated to the common context.
	 *
	 * @param env[in] The current GC thread
	 * @param region[in] The VLHGC region
	 */
	virtual bool shouldMigrateRegionToCommonContext(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region);

protected:
	MM_AllocationContextTarok(UDATA allocationContextNumber, AllocationContextType allocationContextType)
		: MM_AllocationContext()
		, _allocationContextNumber(allocationContextNumber)
		, _allocationContextType(allocationContextType)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Replenish the receiver to satisfy the given allocation description and type and then re-attempt the allocation, returning the result.
	 * @note The calling env must own the receiver's _contextLock or have exclusive VM access
	 * 
	 * @param env[in] A GC thread (must own either _contextLock or have exclusive access)
	 * @param objectAllocationInterface[in] If this is a TLH allocation request, this will be the interface used to initialize the TLH (is NULL in non-TLH allocations)
	 * @param allocateDescription[in] A description of the allocate
	 * @param allocationType[in] The type of the allocation
	 * @return The base address of the allocation or NULL in the case of failure
	 */
	virtual void *lockedReplenishAndAllocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType) = 0;

	/**
	 * Select a region for contraction from the receiver. 
	 * The returned region will be region type FREE.
	 * @param env the environment
	 * @return the region to contract, or NULL if none available
	 */
	virtual MM_HeapRegionDescriptorVLHGC * selectRegionForContraction(MM_EnvironmentBase *env) = 0;
	
private:

/*
* Friends
*/
	/* the MSST is currently responsible for replenishing the ACT so let it reach in to update the regions in the cache */
	friend class MM_MemorySubSpaceTarok;
	
};

#endif /* ALLOCATIONCONTEXTTAROK_HPP */
