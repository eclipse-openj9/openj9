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

#if !defined(HEAPREGIONDATAFORALLOCATE_HPP)
#define HEAPREGIONDATAFORALLOCATE_HPP

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "Base.hpp"
#include "MemoryPoolAddressOrderedList.hpp"

class MM_EnvironmentBase;
class MM_EnvironmentVLHGC;
class MM_MemoryPoolAddressOrderedList;
class MM_HeapRegionDescriptorVLHGC;
class MM_AllocationContextTarok;

/**
 * The descriptor type used to manage object allocation within regions
 */
class MM_HeapRegionDataForAllocate : public MM_BaseNonVirtual
{
public:
	MM_HeapRegionDescriptorVLHGC *_nextInList; /**< Used by MM_RegionListTarok to track which descriptors are in the list (next pointer in linked list) */
	MM_HeapRegionDescriptorVLHGC *_previousInList; /**< Used by MM_RegionListTarok to track which descriptors are in the list (previous pointer in linked list) */
	MM_AllocationContextTarok *_owningContext;	/**< A pointer to the allocation context which currently owns (that is, the only one which can allocate from it) this region.  NULL if unowned */
	MM_AllocationContextTarok *_originalOwningContext;	/**< A pointer to the allocation context from which this region was stolen.  NULL if not stolen (either unowned or owned by a context on its native node) */
protected:
private:
	MM_HeapRegionDescriptorVLHGC *_region;
	J9IndexableObject *_spine; /**< If this region contains an arraylet leaf, this points to the spine which owns the leaf */
	MM_HeapRegionDescriptorVLHGC *_nextArrayletLeafRegion; /**< If this region has a spine in it, this is a list of regions which represent the leaves of the region.  If this is a leaf region, it is the next leaf region in the list */
	MM_HeapRegionDescriptorVLHGC *_previousArrayletLeafRegion; /**< If this region has a spine in it, this is NULL.  If this is a leaf region, it is the previous leaf region in the list */
	UDATA _backingStore[((sizeof(MM_MemoryPoolAddressOrderedList) - 1)/sizeof(UDATA)) + 1]; /**< Allocate space for Memory Pool List */
	
public:

	/**
	 * If the receiver is FREE:  Initializes the _backingStore as an MM_MemoryPoolAddressOrderedList and sets the _region->_memoryPool to point at this pool.
	 * If the receiver is ADDRESS_ORDERED_IDLE:  converts the receiver into ADDRESS_ORDERED.
	 * 
	 * @param[in] env - the current env
	 * @param[in] context Allocation Context
	 * 
	 * @return true if the pool initialization succeeded and has been set in _region->_memoryPool or false if the initialization failed
	 * @note Assertion failure if this method is called on a descriptor who already has an initialized memory pool (since that implies an inconsistency in the assumptions of its user(s)).
	 */
	bool taskAsMemoryPool(MM_EnvironmentBase *env, MM_AllocationContextTarok *context);
	
	/**
	 * Converts the receiver into a FREE region, tearing down any memory pool which may exist
	 * @param env[in] The thread requesting the change of task
	 */
	void taskAsFreePool(MM_EnvironmentBase *env);
	
	/**
	 * Converts the receiver from an ADDRESS_ORDERED region into an ADDRESS_ORDERED_IDLE region.
	 * @param env[in] The thread requesting the change of task
	 */
	void taskAsIdlePool(MM_EnvironmentVLHGC *env);
	
	void taskAsArrayletLeaf(MM_EnvironmentBase *env);
	void removeFromArrayletLeafList(MM_EnvironmentVLHGC *env);
	void addToArrayletLeafList(MM_HeapRegionDescriptorVLHGC* spineRegion);
	MM_HeapRegionDescriptorVLHGC *getNextArrayletLeafRegion() { return _nextArrayletLeafRegion; }
	J9IndexableObject *getSpine() { return _spine; }
	void setSpine(J9IndexableObject *spineObject);
	
	/**
	 * @return true if the receiver represents a region which the compact scheme can directly act upon but false if not (ie:  uncommitted or managed by a memory pool type which can't be compacted)
	 */
	bool participatesInCompaction();
	
	MM_HeapRegionDataForAllocate(MM_EnvironmentVLHGC *env);

	bool initialize(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptorVLHGC *descriptor);
	void tearDown(MM_EnvironmentVLHGC *env);

protected:
	
private:
};

#endif /* HEAPREGIONDATAFORALLOCATE_HPP */
