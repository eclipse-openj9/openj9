
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
#include "ModronAssertions.h"

#include "HeapRegionDescriptorVLHGC.hpp"

#include "AllocationContextTarok.hpp"
#include "CompactGroupManager.hpp"
#include "CompactGroupPersistentStats.hpp"
#include "EnvironmentVLHGC.hpp"
#include "InterRegionRememberedSet.hpp"

MM_HeapRegionDescriptorVLHGC::MM_HeapRegionDescriptorVLHGC(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress)
	: MM_HeapRegionDescriptor(env, lowAddress, highAddress)
	, _allocateData(env)
#if defined (J9VM_GC_MODRON_COMPACTION)
	, _compactData(env)
#endif /* defined (J9VM_GC_MODRON_COMPACTION) */
	, _criticalRegionsInUse(0)
	,_previousMarkMapCleared(false)
	,_nextMarkMapCleared(false)
	,_projectedLiveBytes(0)
	,_projectedLiveBytesPreviousPGC(0)
	,_projectedLiveBytesDeviation(0)
	,_compactDestinationQueueNext(NULL)
#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	,_sparseHeapAllocation(false)
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
	,_defragmentationTarget(false)
	,_extensions(MM_GCExtensions::getExtensions(env))
	,_allocationAge(0)
	,_allocationAgeSizeProduct(0.0)
	,_age(0)
	,_rememberedSetCardList()
	,_rsclBufferPool(NULL)
	,_dynamicSelectionNext(NULL)
{
	_typeId = __FUNCTION__;
}

bool 
MM_HeapRegionDescriptorVLHGC::initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager)
{
	if (!MM_HeapRegionDescriptor::initialize(env, regionManager)) {
		return false;
	}
	
	if (!_allocateData.initialize((MM_EnvironmentVLHGC*)env, regionManager, this)) {
		return false;
	}

	_markData._shouldMark = false;
	_markData._noEvacuation = false;
	_markData._dynamicMarkCost = 0;
	_markData._overflowFlags = 0x0;
	_reclaimData._shouldReclaim = false;
	_sweepData._alreadySwept = true;
	_sweepData._lastGCNumber = 0;
	_copyForwardData._initialLiveSet = false;
	_copyForwardData._survivorSetAborted = false;
	_copyForwardData._evacuateSet = false;
	_copyForwardData._requiresPhantomReferenceProcessing = false;
	_copyForwardData._survivor = false;
	_copyForwardData._freshSurvivor = false;
	_copyForwardData._nextRegion = NULL;
	_copyForwardData._previousRegion = NULL;

#if defined (J9VM_GC_MODRON_COMPACTION)
	if (!_compactData.initialize((MM_EnvironmentVLHGC*)env, regionManager, this)) {
		return false;
	}
#endif /* defined (J9VM_GC_MODRON_COMPACTION) */

	/* add our unfinalized list to the global list (no locking - assumes single threaded initialization) */
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	_unfinalizedObjectList.setNextList(extensions->unfinalizedObjectLists);
	_unfinalizedObjectList.setPreviousList(NULL);
	if (NULL != extensions->unfinalizedObjectLists) {
		extensions->unfinalizedObjectLists->setPreviousList(&_unfinalizedObjectList);
	}
	extensions->unfinalizedObjectLists = &_unfinalizedObjectList;

	/* add our ownable synchronizer list to the global list (no locking - assumes single threaded initialization) */
	_ownableSynchronizerObjectList.setNextList(extensions->getOwnableSynchronizerObjectLists());
	_ownableSynchronizerObjectList.setPreviousList(NULL);
	if (NULL != extensions->getOwnableSynchronizerObjectLists()) {
		extensions->getOwnableSynchronizerObjectLists()->setPreviousList(&_ownableSynchronizerObjectList);
	}
	extensions->setOwnableSynchronizerObjectLists(&_ownableSynchronizerObjectList);
	
	/* add our continuation list to the global list (no locking - assumes single threaded initialization) */
	_continuationObjectList.setNextList(extensions->getContinuationObjectLists());
	_continuationObjectList.setPreviousList(NULL);
	if (NULL != extensions->getContinuationObjectLists()) {
		extensions->getContinuationObjectLists()->setPreviousList(&_continuationObjectList);
	}
	extensions->setContinuationObjectLists(&_continuationObjectList);

	return true;
}

void 
MM_HeapRegionDescriptorVLHGC::tearDown(MM_EnvironmentBase *env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);

#if defined (J9VM_GC_MODRON_COMPACTION)
	_compactData.tearDown((MM_EnvironmentVLHGC*)env);
#endif /* defined (J9VM_GC_MODRON_COMPACTION) */
	
	_allocateData.tearDown((MM_EnvironmentVLHGC*)env);
	
	if (NULL != _rsclBufferPool) {
		extensions->getForge()->free(_rsclBufferPool);
		_rsclBufferPool = NULL;
	}

	_rememberedSetCardList.tearDown(extensions);
	extensions->unfinalizedObjectLists = NULL;
	extensions->setOwnableSynchronizerObjectLists(NULL);
	extensions->setContinuationObjectLists(NULL);

	MM_HeapRegionDescriptor::tearDown(env);
}

bool 
MM_HeapRegionDescriptorVLHGC::initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress)
{
	new((MM_HeapRegionDescriptorVLHGC*)descriptor) MM_HeapRegionDescriptorVLHGC((MM_EnvironmentVLHGC*)env, lowAddress, highAddress);
	return ((MM_HeapRegionDescriptorVLHGC*)descriptor)->initialize(env, regionManager);
}

void 
MM_HeapRegionDescriptorVLHGC::destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor)
{
	((MM_HeapRegionDescriptorVLHGC*)descriptor)->tearDown((MM_EnvironmentVLHGC*)env);
}

bool
MM_HeapRegionDescriptorVLHGC::allocateSupportingResources(MM_EnvironmentBase *env)
{
	return MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->allocateRegionBuffers((MM_EnvironmentVLHGC *)env, this);
}

uintptr_t
MM_HeapRegionDescriptorVLHGC::getProjectedReclaimableBytes() 
{
	uintptr_t regionSize = _extensions->regionSize;
	uintptr_t consumedBytes = regionSize - getMemoryPool()->getFreeMemoryAndDarkMatterBytes();
	uintptr_t projectedReclaimableBytes = consumedBytes - _projectedLiveBytes;
	return projectedReclaimableBytes;
}

void 
MM_HeapRegionDescriptorVLHGC::resetAge(MM_EnvironmentVLHGC *env, U_64 allocationAge)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	uintptr_t logicalAge = 0;
	if (extensions->tarokAllocationAgeEnabled) {
		logicalAge = MM_CompactGroupManager::calculateLogicalAgeForRegion(env, allocationAge);
	}
	setAge(allocationAge, logicalAge);

	U_64 maxAllocationAge = extensions->compactGroupPersistentStats[logicalAge]._maxAllocationAge;
	U_64 minAllocationAge = 0;

	if (logicalAge > 0) {
		minAllocationAge = extensions->compactGroupPersistentStats[logicalAge - 1]._maxAllocationAge;
	}

	setAgeBounds(minAllocationAge, maxAllocationAge);
}

