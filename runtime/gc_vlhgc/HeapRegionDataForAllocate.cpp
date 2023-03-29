
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

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
#include <sys/mman.h>
#include <errno.h>
#endif /* defined(J9VM_GC_ENABLE_DOUBLE_MAP) */
#include "AllocationContext.hpp"
#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionDataForAllocate.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "InterRegionRememberedSet.hpp"
#include "MarkMap.hpp"
#include "MarkMapManager.hpp"
#include "MemoryPoolAddressOrderedList.hpp"

MM_HeapRegionDataForAllocate::MM_HeapRegionDataForAllocate(MM_EnvironmentVLHGC *env)
	: MM_BaseNonVirtual()
	, _nextInList(NULL)
	, _previousInList(NULL)
	, _owningContext(NULL)
	, _originalOwningContext(NULL)
	, _region(NULL)
	, _nextArrayletLeafRegion(NULL)
	, _previousArrayletLeafRegion(NULL)
{
	_typeId = __FUNCTION__;
}

bool
MM_HeapRegionDataForAllocate::initialize(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptorVLHGC *descriptor)
{
	_region = descriptor;
	return true;
}

void
MM_HeapRegionDataForAllocate::tearDown(MM_EnvironmentVLHGC *env)
{
	MM_MemoryPoolAddressOrderedList *memoryPool = (MM_MemoryPoolAddressOrderedList *)_region->getMemoryPool();
	if (NULL != memoryPool) {
		memoryPool->tearDown(env);
		_region->setMemoryPool(NULL);
	}
	
	_region = NULL;
}

bool
MM_HeapRegionDataForAllocate::taskAsMemoryPool(MM_EnvironmentBase *env, MM_AllocationContextTarok *context)
{
	MM_IncrementalGenerationalGC *globalCollector = ((MM_IncrementalGenerationalGC *)MM_GCExtensions::getExtensions(env)->getGlobalCollector());
	if (globalCollector->isGlobalMarkPhaseRunning()) {
		MM_MarkMap *nextMarkMap = globalCollector->getMarkMapManager()->getGlobalMarkPhaseMap();
		if (_region->_nextMarkMapCleared) {
			_region->_nextMarkMapCleared = false;
			if (MM_GCExtensions::getExtensions(env)->tarokEnableExpensiveAssertions) {
				Assert_MM_true(nextMarkMap->checkBitsForRegion(env, _region));
			}
		} else {
			nextMarkMap->setBitsForRegion(env, _region, true);
		}
	}
	bool regionConverted = false;
	if (MM_HeapRegionDescriptor::FREE == _region->getRegionType()) {
		Assert_MM_true(NULL == _region->getMemoryPool());
		MM_MemoryPoolAddressOrderedList *memoryPool = (MM_MemoryPoolAddressOrderedList*)_backingStore;
		UDATA minimumFreeEntrySize = MM_GCExtensions::getExtensions(env)->getMinimumFreeEntrySize();
		new (memoryPool) MM_MemoryPoolAddressOrderedList(env, minimumFreeEntrySize);
		if (memoryPool->initialize(env)) {
			_region->setMemoryPool(memoryPool);
			_region->setRegionType(MM_HeapRegionDescriptor::ADDRESS_ORDERED);
			_region->_allocateData._owningContext = context;
			regionConverted = true;
		}
	} else if (MM_HeapRegionDescriptor::ADDRESS_ORDERED_IDLE == _region->getRegionType()) {
		_region->setRegionType(MM_HeapRegionDescriptor::ADDRESS_ORDERED);
		regionConverted = true;
	} else {
		Assert_MM_unreachable();
	}
	return regionConverted;
}

void
MM_HeapRegionDataForAllocate::taskAsFreePool(MM_EnvironmentBase *env)
{
	Assert_MM_true(NULL == _spine);
	Assert_MM_true(NULL == _nextArrayletLeafRegion);
	Assert_MM_true(NULL == _previousArrayletLeafRegion);

	MM_MemoryPoolAddressOrderedList *memoryPool = (MM_MemoryPoolAddressOrderedList *)_region->getMemoryPool();
	if (NULL != memoryPool) {
		memoryPool->tearDown(env);
		_region->setMemoryPool(NULL);
	}

	/* Region about to be set free better not to be marked overflowed in GMP or PGC */
	Assert_MM_true(0 == _region->_markData._overflowFlags);

	_region->setRegionType(MM_HeapRegionDescriptor::FREE);
	_region->_allocateData._owningContext = NULL;
	/* set _projectedLiveBytes to 'uninitialized' value. it will be initialized at the beginning of the first PGC */
	_region->_projectedLiveBytes = UDATA_MAX;
	_region->_projectedLiveBytesDeviation = 0;
	_region->setAge(0,0);
	_region->resetAgeBounds();
	_region->_defragmentationTarget = false;
}

void
MM_HeapRegionDataForAllocate::taskAsIdlePool(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true( (MM_HeapRegionDescriptor::ADDRESS_ORDERED == _region->getRegionType()) || (MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED == _region->getRegionType()) );

	/* Region about to be set free better not to be marked overflowed in GMP or PGC */
	Assert_MM_true(0 == _region->_markData._overflowFlags);

	/* This could be a first PGC after a GMP (that did not build RS list for this region).
	 * PGC swept this region (it's empty), but RS list is stale - we should clear it
	 */
	MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->clearReferencesToRegion(env, _region);
	_region->setRegionType(MM_HeapRegionDescriptor::ADDRESS_ORDERED_IDLE);
	/* set _projectedLiveBytes to 'uninitialized' value. it will be initialized at the beginning of the first PGC */
	_region->_projectedLiveBytes = UDATA_MAX;
	_region->_projectedLiveBytesDeviation = 0;
	_region->setAge(0,0);
	_region->resetAgeBounds();
	_region->_defragmentationTarget = false;

	/* When a region becomes idle ensure that identity hash salt for this region gets updated */
	J9IdentityHashData *salts = ((J9JavaVM *)env->getLanguageVM())->identityHashData;
	UDATA heapDelta = (UDATA)_region->getLowAddress() - salts->hashData1;
	UDATA index = heapDelta >> salts->hashData3;
	Assert_MM_true(index >= 0);
	Assert_MM_true(index < salts->hashData4);
	MM_GCExtensions::getExtensions(env)->updateIdentityHashDataForSaltIndex(index);
}

void
MM_HeapRegionDataForAllocate::taskAsArrayletLeaf(MM_EnvironmentBase *env)
{
	Assert_MM_true(NULL == _nextArrayletLeafRegion);
	Assert_MM_true(NULL == _previousArrayletLeafRegion);
	Assert_MM_true(MM_HeapRegionDescriptor::FREE == _region->getRegionType());

	/* Region about to be used as arraylet leaf better not to be marked overflowed in GMP or PGC */
	Assert_MM_true(0 == _region->_markData._overflowFlags);

	_spine = NULL;
	_region->setRegionType(MM_HeapRegionDescriptor::ARRAYLET_LEAF);
}

/**
 * Remove the receiver's region from the list of arraylet leaves it currently belongs to.
 * Once this function completes, the region is in an invalid state and must be freed
 * or assigned to a new list immediately.
 */
void 
MM_HeapRegionDataForAllocate::removeFromArrayletLeafList(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(_region->isArrayletLeaf());
	
	MM_HeapRegionDescriptorVLHGC *next = _nextArrayletLeafRegion;
	MM_HeapRegionDescriptorVLHGC *previous = _previousArrayletLeafRegion;
	
	Assert_MM_true(NULL != previous);

	/**
	 * Restore/Recommit arraylet leaves that have been previously decommitted.
	 */
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	const UDATA arrayletLeafSize = env->getOmrVM()->_arrayletLeafSize;
	void *leafAddress = _region->getLowAddress();
	extensions->heap->commitMemory(leafAddress, arrayletLeafSize);

	previous->_allocateData._nextArrayletLeafRegion = next;
	if (NULL != next) {
		Assert_MM_true(next->isArrayletLeaf());
		next->_allocateData._previousArrayletLeafRegion = previous;
	}

	_nextArrayletLeafRegion = NULL;
	_previousArrayletLeafRegion = NULL;
}

/**
 * Add the receiver's region to the specified spine region's list of arraylet leaves
 */
void 
MM_HeapRegionDataForAllocate::addToArrayletLeafList(MM_HeapRegionDescriptorVLHGC* newSpineRegion)
{
	Assert_MM_true(_region->isArrayletLeaf());
	Assert_MM_true(NULL != newSpineRegion);
	Assert_MM_true(newSpineRegion->containsObjects());
	Assert_MM_true(NULL == newSpineRegion->_allocateData._spine);
	Assert_MM_true(NULL == _nextArrayletLeafRegion);
	Assert_MM_true(NULL == _previousArrayletLeafRegion);
	
	_nextArrayletLeafRegion = newSpineRegion->_allocateData._nextArrayletLeafRegion;
	if (NULL != _nextArrayletLeafRegion) {
		Assert_MM_true(_nextArrayletLeafRegion->isArrayletLeaf());
		_nextArrayletLeafRegion->_allocateData._previousArrayletLeafRegion = _region;
	}
	newSpineRegion->_allocateData._nextArrayletLeafRegion = _region;
	_previousArrayletLeafRegion = newSpineRegion;
}

void 
MM_HeapRegionDataForAllocate::setSpine(J9IndexableObject *spineObject)
{
	Assert_MM_true(_region->isArrayletLeaf());
	_spine = spineObject;
}

bool
MM_HeapRegionDataForAllocate::participatesInCompaction()
{
	return _region->containsObjects();
}
