
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(MEMORYSUBSPACETAROK_HPP_)
#define MEMORYSUBSPACETAROK_HPP_

#include "j9cfg.h"

#include "MemorySubSpace.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_HeapStats;
class MM_MemoryPool;
class MM_MemorySpace;
class MM_ObjectAllocationInterface;
class MM_AllocationContext;
class MM_AllocationContextTarok;
class MM_GlobalAllocationManagerTarok;
class MM_RegionListTarok;
class MM_HeapRegionManager;
class MM_PhysicalSubArena;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_MemorySubSpaceTarok : public MM_MemorySubSpace
{
/* data members */
private:
	MM_GlobalAllocationManagerTarok *_globalAllocationManagerTarok;	/**< Provides the API for accessing information about the underlying regions (owned by the AllocationContextTarok instances) */
	bool _allocateAtSafePointOnly;
	volatile UDATA _bytesRemainingBeforeTaxation;	/**< The number of bytes which this subspace can use for allocation before triggering an early allocation failure */

	MM_HeapRegionManager *_heapRegionManager;	/**< Stored so that we can resolve the table descriptor for given addresses when asked for a pool corresponding to a specific address */
	MM_LightweightNonReentrantLock _expandLock; /**< Most of the common expand code is not multi-threaded safe (since it used in standard collectors on alloc path fail path which is single threaded)  */

protected:
public:
	
/* function members */
private:
	bool initialize(MM_EnvironmentBase *env);
	UDATA adjustExpansionWithinFreeLimits(MM_EnvironmentBase *env, UDATA expandSize);
	UDATA adjustExpansionWithinSoftMax(MM_EnvironmentBase *env, UDATA expandSize, UDATA minimumBytesRequired);
	UDATA checkForRatioExpand(MM_EnvironmentBase *env, UDATA bytesRequired);	
	bool checkForRatioContract(MM_EnvironmentBase *env);
	UDATA calculateExpandSize(MM_EnvironmentBase *env, UDATA bytesRequired, bool expandToSatisfy);
	
	/**
	 * Determine how much space we need to expand the heap by on this GC cycle to meet the collector's requirement.
	 * @param env[in] the current thread  
	 * @return Number of bytes required
	 */
	UDATA calculateCollectorExpandSize(MM_EnvironmentBase *env);
	UDATA calculateTargetContractSize(MM_EnvironmentBase *env, UDATA allocSize, bool ratioContract);
	bool timeForHeapContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool systemGC);
	bool timeForHeapExpand(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);	
	UDATA performExpand(MM_EnvironmentBase *env);
	UDATA performContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	/**
	 * This function is inherited but we can't implement it with this signature.
	 * Overridden as private to prevent invocation, and implemented as Assert_MM_unreachable()
	 */
	virtual UDATA collectorExpand(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);

	/**
	 * Attempt to allocate the described entity (object, TLH or leaf), replenishing the allocate region if necessary.
	 *
	 * @param env[in] the current thread
	 * @param context[in] the allocation context
	 * @param objectAllocationInterface[in] the interface to use in allocation
	 * @param allocateDescription[in] information about the allocation
	 * @param allocationType[in] the type of entity being allocated
	 * 
	 * @return the entity or NULL on failure 
	 */
	void* lockedAllocate(MM_EnvironmentBase *env, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType);
	
protected:
	virtual void tearDown(MM_EnvironmentBase *env);
	
public:
	static MM_MemorySubSpaceTarok *newInstance(MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_GlobalAllocationManagerTarok *gamt, bool usesGlobalCollector, UDATA minimumSize, UDATA initialSize, UDATA maximumSize, UDATA memoryType, U_32 objectFlags);
	
	virtual const char *getName() { return MEMORY_SUBSPACE_NAME_GENERIC; }
	virtual const char *getDescription() { return MEMORY_SUBSPACE_DESCRIPTION_GENERIC; }

	virtual MM_MemoryPool *getMemoryPool();
	virtual MM_MemoryPool *getMemoryPool(void *addr);
	virtual MM_MemoryPool *getMemoryPool(UDATA size);
	virtual MM_MemoryPool *getMemoryPool(MM_EnvironmentBase *env, 
										void *addrBase, void *addrTop, 
										void * &highAddr);
	virtual UDATA getMemoryPoolCount();
	virtual UDATA getActiveMemoryPoolCount();
	
	virtual UDATA getActiveMemorySize();
	virtual UDATA getActiveMemorySize(UDATA includeMemoryType);
	
	virtual UDATA getActualFreeMemorySize();
	virtual UDATA getApproximateFreeMemorySize();
	
	virtual UDATA getActualActiveFreeMemorySize();
	virtual UDATA getActualActiveFreeMemorySize(UDATA includememoryType);
	virtual UDATA getApproximateActiveFreeMemorySize();
	virtual UDATA getApproximateActiveFreeMemorySize(UDATA includememoryType);
	
	virtual UDATA getActiveLOAMemorySize(UDATA includememoryType);
	virtual UDATA getApproximateActiveFreeLOAMemorySize();
	virtual UDATA getApproximateActiveFreeLOAMemorySize(UDATA includememoryType);
		
	virtual	void mergeHeapStats(MM_HeapStats *heapStats);
	virtual	void mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType);
	virtual void resetHeapStatistics(bool globalCollect);	
	virtual MM_AllocationFailureStats *getAllocationFailureStats();

	virtual void *allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace);
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
#if defined(J9VM_GC_ARRAYLETS)
	virtual UDATA largestDesirableArraySpine()
	{
		return _extensions->getOmrVM()->_arrayletLeafSize;
	}

	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
#endif /* J9VM_GC_ARRAYLETS */
	
	
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */
	
	virtual void setAllocateAtSafePointOnly(MM_EnvironmentBase *env, bool safePoint) { _allocateAtSafePointOnly = safePoint; };
	
	/* Calls for internal collection routines */
	virtual void *collectorAllocate(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	virtual void *collectorAllocateTLH(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription, UDATA maximumBytesRequired, void * &addrBase, void * &addrTop);
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

	/**
	 * Expand the heap by a single region.
	 * Called during a garbage collection operation, typically to avoid aborting.
	 * @param env[in] the current thread
	 * @return the number of bytes expanded
	 */
	UDATA collectorExpand(MM_EnvironmentBase *env);

	virtual UDATA adjustExpansionWithinUserIncrement(MM_EnvironmentBase *env, UDATA expandSize);
	virtual UDATA maxExpansionInSpace(MM_EnvironmentBase *env);
	virtual bool expanded(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, MM_HeapRegionDescriptor *region, bool canCoalesce);
	virtual UDATA getAvailableContractionSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);	

	virtual void checkResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription = NULL, bool _systemGC = false);
	virtual IDATA performResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription = NULL);

	virtual void abandonHeapChunk(void *addrBase, void *addrTop);

	virtual MM_MemorySubSpace *getDefaultMemorySubSpace();
	virtual MM_MemorySubSpace *getTenureMemorySubSpace();

	virtual void reset();
	virtual void rebuildFreeList(MM_EnvironmentBase *env);
	
	virtual void resetLargestFreeEntry();
	virtual void recycleRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region);
	virtual UDATA findLargestFreeEntry(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription);

	virtual bool completeFreelistRebuildRequired(MM_EnvironmentBase *env);
	
	virtual void addExistingMemory(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, UDATA size, void *lowAddress, void *highAddress, bool canCoalesce);
	virtual void *removeExistingMemory(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, UDATA size, void *lowAddress, void *highAddress);

	virtual bool isActive();

	virtual void *lockedReplenishAndAllocate(MM_EnvironmentBase *env, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType);

	/**
	 * Called by IncrementalGenerationalGC to permit the subspace to do more work before the next taxation.
	 */
	void setBytesRemainingBeforeTaxation(UDATA remaining);

	/**
	 * @return the number of bytes which can still be allocated by this subspace before triggering taxation 
	 */
	MMINLINE UDATA getBytesRemainingBeforeTaxation() { return _bytesRemainingBeforeTaxation; }
	
	/**
	 * @see MM_MemorySubSpace::selectRegionForContraction()
	 */
	virtual MM_HeapRegionDescriptor * selectRegionForContraction(MM_EnvironmentBase *env, UDATA numaNode);

	/**
	 * Called when an allocation context replenishment request fails in order to invoke a GC.
	 * 
	 * @param[in] env The current thread
	 * @param[in] replenishingSpace The subspace which was initially asked to fill the replenishment request and which will be aksed to replenish it once the GC is complete
	 * @param[in] context The allocation context which the sender failed to replenish
	 * @param[in] allocateDescription The allocation request which initiated the allocation failure
	 * @param[in] allocationType The type of allocation request we eventually must satisfy
	 * 
	 * @return The result of the allocation attempted under exclusive
	 */
	void *replenishAllocationContextFailed(MM_EnvironmentBase *env, MM_MemorySubSpace *replenishingSpace, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType);

	/**
	 * Attempt to consume the specified number of bytes from the subspace's taxation threshold.
	 * @param env[in] the current thread
	 * @param bytesToConsume the number of bytes about to be allocated
	 * @return true if the bytes were available, false if the threshold has been reached
	 */
	bool consumeFromTaxationThreshold(MM_EnvironmentBase *env, UDATA bytesToConsume);

	/**
	 * Create a MemorySubSpaceGeneric object
	 */
	MM_MemorySubSpaceTarok(MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_GlobalAllocationManagerTarok *gamt, MM_HeapRegionManager *heapRegionManager, bool usesGlobalCollector, UDATA minimumSize, UDATA initialSize, UDATA maximumSize, UDATA memoryType, U_32 objectFlags)
		: MM_MemorySubSpace(env, NULL, physicalSubArena, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryType, objectFlags)
		, _globalAllocationManagerTarok(gamt)
		, _allocateAtSafePointOnly(false)
		, _bytesRemainingBeforeTaxation(0)
		, _heapRegionManager(heapRegionManager)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* MEMORYSUBSPACETAROK_HPP_ */
