
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

#if !defined(HEAPREGIONDESCRIPTORVLHGC_HPP)
#define HEAPREGIONDESCRIPTORVLHGC_HPP

#include "j9.h"
#include "j9cfg.h"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionDataForAllocate.hpp"
#include "HeapRegionDataForCompactVLHGC.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "ContinuationObjectList.hpp"
#include "ReferenceObjectList.hpp"
#include "RememberedSetCardList.hpp"
#include "UnfinalizedObjectList.hpp"

class MM_EnvironmentVLHGC;

class MM_HeapRegionDescriptorVLHGC : public MM_HeapRegionDescriptor 
{
	/*
	 * Data members
	 */
public:
	MM_HeapRegionDataForAllocate _allocateData;
	enum RefersToCollectionSet {
		noRegion,			/**< This region does not refer to Collection Set */
		anyRegion,			/**< This region does refer to Collection Set */
		overflowedRSCardListRegion /**< This region does refer to Collection Set, but specifically to a region with overflowed RS Card List */
	};
	struct {
		bool _shouldMark;	/**< true if the collector is to mark this region during the collection cycle */
		bool _noEvacuation; /**< true if the region is set that do not copyforward, it is valid if _shouldMark is true. */
		uintptr_t _dynamicMarkCost;	/**< The cost of marking this region (that is, the number of other regions (not including this one) which will need to be scanned - this value is dynamic in that converting the regions which refer to it to scan or mark reduces this number.  It is only valid during GC setup */
		U_8 _overflowFlags;	/**< Used to denote that work packet overflow occurred for an object in this region - bits 0x1 is GMP or global while 0x2 is PGC */
	} _markData;
	struct {
		bool _shouldReclaim; /**< true if the collector is to sweep and/or compact this region during the collection cycle */ 
	} _reclaimData;
	struct {
		bool _alreadySwept;	/**< true if the collector has already swept this region during the last collection increment */
		uintptr_t _lastGCNumber;	/**< initially 0 but set to the GC's collection ID every time it is swept so that the GC can ensure it doesn't over-collect the same set */
	} _sweepData;
	struct {
		bool _initialLiveSet;  /**< true if the region was part of the live set at the start of collection */
		bool _survivorSetAborted;  /**< true if the region was part of the survivor set at the time that an abort had occurred, false otherwise */
		bool _evacuateSet;	/**< true if the region was part of the evacuate set at the beginning of the copy-forward (flag not changed during abort) */
		bool _requiresPhantomReferenceProcessing; /**< Set to true by main thread if this region must be processed during parallel phantom reference processing */
		bool _survivor;
		bool _freshSurvivor;
		MM_HeapRegionDescriptorVLHGC *_nextRegion;  /**< Region list link for compact group resource management during a copyforward operation */
		MM_HeapRegionDescriptorVLHGC *_previousRegion;  /**< Region list link for compact group resource management during a copyforward operation */
	} _copyForwardData;
#if defined (J9VM_GC_MODRON_COMPACTION)
	MM_HeapRegionDataForCompactVLHGC _compactData;
#endif /* defined (J9VM_GC_MODRON_COMPACTION) */
	/* this is only of interest for collectors who can move objects so we ifdef it out, in other cases, to prove that no other specs are absorbing the cost of looking up regions and tracking critical counts */
	uintptr_t volatile _criticalRegionsInUse;	/**< The number of JNI critical regions which are currently in use within the receiver's address range (typically 0).  This value can be modified concurrently so atomics are required */

	bool _previousMarkMapCleared;		/**< if true, previous mark map is guarantied to be cleared; if false it like is not cleared */
	bool _nextMarkMapCleared;			/**< if true, next mark map is guarantied to be cleared; if false it like is not cleared */
	uintptr_t _projectedLiveBytes;			/**< number of bytes in the region projected to be live at the beginning of the current GC. uintptr_t_MAX has a special meaning of 'uninitialized'*/
	uintptr_t _projectedLiveBytesPreviousPGC;   /**< _projectedLiveBytes value from previous PGC; updated just before we apply decay for this PGC */
	IDATA _projectedLiveBytesDeviation;	/**< difference between actual live bytes and projected live bytes. Note: not always update to date and can be negative. */
	MM_HeapRegionDescriptorVLHGC *_compactDestinationQueueNext; /**< pointer to next compact destination region in the queue */
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
	bool _sparseHeapAllocation;		/**< indicates whether this region is related with sparse heap allocation(the first region has reserved for off-heap) */
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

	bool _defragmentationTarget;		/**< indicates whether this region should be considered for defragmentation, currently this means the region has been GMPed but not collected yet */

protected:
	MM_GCExtensions * _extensions;

private:
	U_64 _allocationAge; /**< allocation age (number of bytes allocated since the last attempted allocation) */
	U_64 _lowerAgeBound; /**< lowest possible age of any object in this region */
	U_64 _upperAgeBound; /**< highest possible age of any object in this region */
	double _allocationAgeSizeProduct; /**< sum of (age * size) products for each object in the region. used for age merging math in survivor regions */
	uintptr_t _age; /**< logical allocation age (number of GC cycles since the last attempted allocation) */
	MM_RememberedSetCardList _rememberedSetCardList; /**< remembered set card list */
	MM_RememberedSetCard *_rsclBufferPool;			 /**< RSCL Buffer pool owned by this region (Buffers can still be shared among other regions) */
	
	MM_HeapRegionDescriptorVLHGC *_dynamicSelectionNext;  /**< Linked list pointer used during dynamic set selection (NOTE: Valid only during dynamic set selection) */

	MM_UnfinalizedObjectList _unfinalizedObjectList; /**< A list of unfinalized objects in this region */
	MM_OwnableSynchronizerObjectList _ownableSynchronizerObjectList; /**< A list of ownable synchronizer objects in this region */
	MM_ContinuationObjectList _continuationObjectList; /**< A list of continuation objects in this region */
	MM_ReferenceObjectList _referenceObjectList; /**< A list of reference objects (i.e. weak/soft/phantom) in this region */
	
	/*
	 * Function members
	 */
public:
	MM_HeapRegionDescriptorVLHGC(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress);

	/**
	 * Get Allocation Age - return back allocation age
	 * @return allocation age value
	 */
	MMINLINE U_64 getAllocationAge() { return _allocationAge; }

	MMINLINE U_64 getLowerAgeBound() { return _lowerAgeBound; }
	MMINLINE U_64 getUpperAgeBound() { return _upperAgeBound; }

	/**
	 * get current value of (age * size) product used for survivor regions
	 * @return allocationAgeSizeProduct value
	 */
	MMINLINE double getAllocationAgeSizeProduct()
	{
		 return _allocationAgeSizeProduct;
	}

	/**
	 * set current value of (age * size) product used for survivor regions (used for setting its initial value)
	 */
	MMINLINE void setAllocationAgeSizeProduct(double allocationAgeSizeProduct)
	{
		 _allocationAgeSizeProduct = allocationAgeSizeProduct;
	}


	/**
	 * increment atomically allocation (age * size) product
	 * @param allocationAgeSizeProduct age to increment with
	 * @return new allocationAgeSizeProduct value
	 */
	MMINLINE double atomicIncrementAllocationAgeSizeProduct(double allocationAgeSizeProduct)
	{
		 return MM_AtomicOperations::addDouble(&_allocationAgeSizeProduct, allocationAgeSizeProduct);
	}

	/**
	 *	Get Logical Age - return back logical age in range 0-tarokRegionMaxAge
	 *	@return return age value
	 */
	MMINLINE uintptr_t getLogicalAge() { return _age; }

	/**
	 * Set Age - set both allocation and logical age
	 * @param allocationAge allocation age to set
	 * @param logicalAge logical age to set
	 */
	MMINLINE void setAge(U_64 allocationAge, uintptr_t logicalAge)
	{
		_allocationAge = allocationAge;
		_age = logicalAge;
	}

	/**
	 * Update lowest and higher object age bounds (propageted info from just filled-up copy cache)
	 */
	MMINLINE void updateAgeBounds(U_64 lowerAgeBound, U_64 upperAgeBound) {
		/* ideally, these should be atomic updates, but bounds are anyway just an approximation */
		_lowerAgeBound = OMR_MIN(_lowerAgeBound, lowerAgeBound);
		_upperAgeBound = OMR_MAX(_upperAgeBound, upperAgeBound);
	}

	/**
	 * Increment lowest and higher object age bounds (when doing region aging)
	 */
	MMINLINE void incrementAgeBounds(uintptr_t increment) {
		_lowerAgeBound += increment;
		_upperAgeBound += increment;
	}

	/**
	 * Set age bounds from newly allocated region
	 */
	MMINLINE void setAgeBounds(U_64 lowerAgeBound, U_64 upperAgeBound) {
		_lowerAgeBound = lowerAgeBound;
		_upperAgeBound = upperAgeBound;
	}

	/**
	 * Reset age bounds for IDLE/FREE regions
	 */
	MMINLINE void resetAgeBounds() {
		_lowerAgeBound = U_64_MAX;
		_upperAgeBound = 0;
	}


	/**
	 * Reset Age
	 * Supports both old and bytes-allocated-based aging systems
	 * @param env[in] the current thread
	 * @param allocationAge allocation age to set
	 */
	void resetAge(MM_EnvironmentVLHGC *env, U_64 allocationAge);

	/**
	 * Is Eden - return true if region is Eden
	 * By definition Eden space is allocated since last PGC and has not been marked yet
	 * So for regions contains objects we can base on region type and it is only case we need
	 * @return true if it is a Eden region
	 */
	MMINLINE bool isEden()
	{
		/* this implementation return correct answer for regions contains objects only */
		Assert_MM_true(containsObjects());
		return (ADDRESS_ORDERED == getRegionType());
	}

	/**
	 * Check is it an arraylet leaf
	 * @return true if region type is an arraylet leaf
	 */
	bool isArrayletLeaf()
	{
		return (ARRAYLET_LEAF == getRegionType());
	}

	MM_RememberedSetCardList *getRememberedSetCardList() { return &_rememberedSetCardList; }
	
	MM_HeapRegionDescriptorVLHGC *getDynamicSelectionNext() { return _dynamicSelectionNext; }
	void setDynamicSelectionNext(MM_HeapRegionDescriptorVLHGC *region) { _dynamicSelectionNext = region; }

	/**
	 * Get the amount of bytes we expect to be reclaimed after a collection. i.e. bytes occupied - projected live bytes.
	 * This will not include free space in the tail.
	 */
	uintptr_t getProjectedReclaimableBytes();

	/**
	 * Fetch the list of unfinalized objects within this region.
	 * @return the list
	 */
	MM_UnfinalizedObjectList *getUnfinalizedObjectList() { return &_unfinalizedObjectList; }

	/**
	 * Fetch the list of ownable synchronizer objects within this region.
	 * @return the list
	 */
	MM_OwnableSynchronizerObjectList *getOwnableSynchronizerObjectList() { return &_ownableSynchronizerObjectList; }
	
	/**
	 * Fetch the list of continuation objects within this region.
	 * @return the list
	 */
	MM_ContinuationObjectList *getContinuationObjectList() { return &_continuationObjectList; }
	/**
	 * Fetch the list of reference objects within this region.
	 * @return the list
	 */
	MM_ReferenceObjectList *getReferenceObjectList() { return &_referenceObjectList; }

	MM_RememberedSetCard *getRsclBufferPool() { return _rsclBufferPool; }

	/**
	 * @return True if the receiver was a fresh survivor region (is freshly allocated from a free region) in the copy-forward which just finished
	 */
	MMINLINE bool isFreshSurvivorRegion() { return _copyForwardData._freshSurvivor; }

	/**
	 * @return True if region is in survivor set (there is explicit flag)
	 */
	MMINLINE bool isSurvivorRegion() { return _copyForwardData._survivor; }

	/**
	 * Allocate supporting resources (large enough to justify not to preallocate them for all regions at the startup) when region is being committed.
	 * For VLHGC region, the resource is RSCL Buffer pool
	 * @param env[in] of a GC thread
	 * @return true if allocation is successful
	 */
	virtual bool allocateSupportingResources(MM_EnvironmentBase *env);


	bool initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager);
	void tearDown(MM_EnvironmentBase *env);
	
	static bool initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress);
	static void destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor);
	
protected:
private:
	
	/*
	 * Friends
	 */
	friend class MM_InterRegionRememberedSet;	/* to access _rsclBufferPool */
};

#endif /* HEAPREGIONDESCRIPTORVLHGC_HPP */
