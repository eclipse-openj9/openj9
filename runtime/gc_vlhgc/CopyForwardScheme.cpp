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
#include "j2sever.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include <string.h>

#include "mmhook_internal.h"

#include "CopyForwardScheme.hpp"

#include "AllocateDescription.hpp"
#include "AllocationContextTarok.hpp"
#include "ArrayletLeafIterator.hpp"
#include "AtomicOperations.hpp"
#include "Bits.hpp"
#include "CardCleaner.hpp"
#include "CardListFlushTask.hpp"
#include "CardTable.hpp"
#include "ClassHeapIterator.hpp"
#include "ClassIterator.hpp"
#include "ClassLoaderClassesIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderRememberedSet.hpp"
#include "CopyForwardSchemeTask.hpp"
#include "CompactGroupManager.hpp"
#include "CompactGroupPersistentStats.hpp"
#include "CompressedCardTable.hpp"
#include "CopyForwardCompactGroup.hpp"
#include "CopyForwardGMPCardCleaner.hpp"
#include "CopyForwardNoGMPCardCleaner.hpp"
#include "CopyScanCacheChunkVLHGCInHeap.hpp"
#include "CopyScanCacheListVLHGC.hpp"
#include "CopyScanCacheVLHGC.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentVLHGC.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "ContinuationObjectBufferVLHGC.hpp"
#include "ContinuationObjectList.hpp"
#include "FinalizeListManager.hpp"
#include "ForwardedHeader.hpp"
#include "GlobalAllocationManager.hpp"
#include "Heap.hpp"
#include "HeapMapIterator.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "HotFieldUtil.hpp"
#include "InterRegionRememberedSet.hpp"
#include "MarkMap.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectIteratorState.hpp"
#include "ObjectModel.hpp"
#include "ParallelDispatcher.hpp"
#include "PacketSlotIterator.hpp"
#include "ParallelTask.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "ReferenceObjectList.hpp"
#include "ReferenceStats.hpp"
#include "RegionBasedOverflowVLHGC.hpp"
#include "RootScanner.hpp"
#include "SlotObject.hpp"
#include "StackSlotValidator.hpp"
#include "SublistFragment.hpp"
#include "SublistIterator.hpp"
#include "SublistPool.hpp"
#include "SublistPuddle.hpp"
#include "SublistSlotIterator.hpp"
#include "SurvivorMemoryIterator.hpp"
#include "WorkPacketsIterator.hpp"
#include "WorkPacketsVLHGC.hpp"
#if JAVA_SPEC_VERSION >= 19
#include "ContinuationHelpers.hpp"
#endif /* JAVA_SPEC_VERSION >= 19 */

#define INITIAL_FREE_HISTORY_WEIGHT ((float)0.8)
#define TENURE_BYTES_HISTORY_WEIGHT ((float)0.8)

#define SCAN_CACHES_PER_THREAD 1 /* each thread has 1 scan cache */
#define DEFERRED_CACHES_PER_THREAD 1 /* each thread has 1 deferred cache (hierarchical scan ordering only) */

#define SCAN_TO_COPY_CACHE_MAX_DISTANCE (UDATA_MAX)

/* VM Design 1774: Ideally we would pull these cache line values from the port library but this will suffice for
 * a quick implementation
 */
#if defined(AIXPPC) || defined(LINUXPPC)
#define CACHE_LINE_SIZE 128
#elif defined(J9ZOS390) || (defined(LINUX) && defined(S390))
#define CACHE_LINE_SIZE 256
#else
#define CACHE_LINE_SIZE 64
#endif
/* create macros to interpret the hot field descriptor */
#define HOTFIELD_SHOULD_ALIGN(descriptor) (0x1 == (0x1 & (descriptor)))
#define HOTFIELD_ALIGNMENT_BIAS(descriptor, heapObjectAlignment) (((descriptor) >> 1) * (heapObjectAlignment))

/* give a name to the common context.  Note that this may need to be stored locally and fetched, at start-up,
 * if the common context disappears or becomes defined in a more complicated fashion
 */
#define COMMON_CONTEXT_INDEX 0

/* If scavenger dynamicBreadthFirstScanOrdering and alwaysDepthCopyFirstOffset is enabled, always copy the first offset of each object after the object itself is copied */
#define DEFAULT_HOT_FIELD_OFFSET 1

#define	AllCompressedCardsInWordClean		0
#define AllCompressedCardsInByteClean		0
#define AllCompressedCardsInWordSurvivor	UDATA_MAX
#define AllCompressedCardsInByteSurvivor	U_8_MAX
#define CompressedCardSurvivor				1

MM_CopyForwardScheme::MM_CopyForwardScheme(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *manager)
	: MM_BaseNonVirtual()
	, _javaVM((J9JavaVM *)env->getLanguageVM())
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _regionManager(manager)
	, _interRegionRememberedSet(NULL)
	, _reservedRegionList(NULL)
	, _compactGroupMaxCount(MM_CompactGroupManager::getCompactGroupMaxCount(env))
	, _phantomReferenceRegionsToProcess(0)
	, _minCacheSize(0)
	, _maxCacheSize(0)
	, _dispatcher(_extensions->dispatcher)
	, _cacheFreeList()
	, _cacheScanLists(NULL)
	, _scanCacheListSize(_extensions->_numaManager.getMaximumNodeNumber() + 1)
	, _scanCacheWaitCount(0)
	, _scanCacheMonitor(NULL)
	, _workQueueWaitCountPtr(&_scanCacheWaitCount)
	, _workQueueMonitorPtr(&_scanCacheMonitor)
	, _doneIndex(0)
	, _markMap(NULL)
	, _heapBase(NULL)
	, _heapTop(NULL)
	, _abortFlag(false)
	, _abortInProgress(false)
	, _regionCountCannotBeEvacuated(0)
	, _regionCountReservedNonEvacuated(0)
	, _cacheLineAlignment(0)
	, _clearableProcessingStarted(false)
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	, _dynamicClassUnloadingEnabled(false)
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	, _collectStringConstantsEnabled(false)
	, _tracingEnabled(false)
	, _commonContext(NULL)
	, _compactGroupBlock(NULL)
	, _arraySplitSize(0)
	, _regionSublistContentionThreshold(0)
	, _failedToExpand(false)
	, _shouldScanFinalizableObjects(false)
	, _objectAlignmentInBytes(env->getObjectAlignmentInBytes())
	, _compressedSurvivorTable(NULL)
{
	_typeId = __FUNCTION__;
}

MM_CopyForwardScheme *
MM_CopyForwardScheme::newInstance(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *manager)
{
	MM_CopyForwardScheme *scheme = (MM_CopyForwardScheme *)env->getForge()->allocate(sizeof(MM_CopyForwardScheme), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (scheme) {
		new(scheme) MM_CopyForwardScheme(env, manager);
		if (!scheme->initialize(env)) {
			scheme->kill(env);
			scheme = NULL;
		}
	}
	return scheme;
}

void
MM_CopyForwardScheme::kill(MM_EnvironmentVLHGC *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_CopyForwardScheme::initialize(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	if (!_cacheFreeList.initialize(env)) {
		return false;
	}
	UDATA listsToCreate = _scanCacheListSize;
	UDATA scanListsSizeInBytes = sizeof(MM_CopyScanCacheListVLHGC) * listsToCreate;
	_cacheScanLists = (MM_CopyScanCacheListVLHGC *)env->getForge()->allocate(scanListsSizeInBytes, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _cacheScanLists) {
		return false;
	}
	memset((void*)_cacheScanLists, 0x0, scanListsSizeInBytes);
	for (UDATA i = 0; i < listsToCreate; i++) {
		new(&_cacheScanLists[i]) MM_CopyScanCacheListVLHGC();
		if (!_cacheScanLists[i].initialize(env)) {
			/* if we failed part-way through the list, adjust the _scanCacheListSize since tearDown will otherwise fail to
			 * invoke on the entries in the array which didn't have their constructors called
			 */
			_scanCacheListSize = i + 1;
			return false;
		}
	}
	if (0 != omrthread_monitor_init_with_name(&_scanCacheMonitor, 0, "MM_CopyForwardScheme::cache")) {
		return false;
	}
	
	/* Get the estimated cache count required.  The cachesPerThread argument is used to ensure there are at least enough active
	 * caches for all working threads (threadCount * cachesPerThread)
	 */
	UDATA threadCount = extensions->dispatcher->threadCountMaximum();
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);

	/* Each thread can have a scan cache and compactGroupCount copy caches. In hierarchical, there could also be a deferred cache. */
	UDATA cachesPerThread = SCAN_CACHES_PER_THREAD;
	cachesPerThread += compactGroupCount; /* copy caches */
	switch (_extensions->scavengerScanOrdering) {
	case MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_BREADTH_FIRST:
	case MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_DYNAMIC_BREADTH_FIRST:
		break;
	case MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_HIERARCHICAL:
		cachesPerThread += DEFERRED_CACHES_PER_THREAD;
		break;
	default:
		Assert_MM_unreachable();
		break;
	}
	
	UDATA minCacheCount = threadCount * cachesPerThread;
	
	/* Estimate how many caches we might need to describe the entire heap */
	UDATA heapCaches = extensions->memoryMax / extensions->tlhMaximumSize;

	/* use whichever value is higher */
	UDATA totalCacheCount = OMR_MAX(minCacheCount, heapCaches);

	if (!_cacheFreeList.resizeCacheEntries(env, totalCacheCount)) {
		return false;
	}

	/* Create and initialize the owned region lists to maintain resource for survivor area heap acquisition */
	_reservedRegionList = (MM_ReservedRegionListHeader *)env->getForge()->allocate(sizeof(MM_ReservedRegionListHeader) * _compactGroupMaxCount, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _reservedRegionList) {
		return false;
	}
	
	memset((void *)_reservedRegionList, 0, sizeof(MM_ReservedRegionListHeader) * _compactGroupMaxCount);
	for (UDATA index = 0; index < _compactGroupMaxCount; index++) {
		_reservedRegionList[index]._maxSublistCount = 1;
		_reservedRegionList[index]._sublistCount = 1;
		_reservedRegionList[index]._evacuateRegionCount = 0;
		for (UDATA sublistIndex = 0; sublistIndex < MM_ReservedRegionListHeader::MAX_SUBLISTS; sublistIndex++) {
			_reservedRegionList[index]._sublists[sublistIndex]._head = NULL;
			_reservedRegionList[index]._sublists[sublistIndex]._cacheAcquireCount = 0;
			_reservedRegionList[index]._sublists[sublistIndex]._cacheAcquireBytes = 0;
			if (!_reservedRegionList[index]._sublists[sublistIndex]._lock.initialize(env, &_extensions->lnrlOptions, "MM_CopyForwardScheme:_reservedRegionList[]._sublists[]._lock")) {
				return false;
			}
		}
		_reservedRegionList[index]._freeMemoryCandidates = NULL;
		_reservedRegionList[index]._freeMemoryCandidateCount = 0;
		if (!_reservedRegionList[index]._freeMemoryCandidatesLock.initialize(env, &_extensions->lnrlOptions, "MM_CopyForwardScheme:_reservedRegionList[]._freeMemoryCandidatesLock")) {
			return false;
		}
	}

	/* Set the min/max sizes for copy scan cache allocation when allocating a general purpose area (does not include non-standard sized objects) */
	_minCacheSize = _extensions->tlhMinimumSize;
	_maxCacheSize = _extensions->tlhMaximumSize;

	/* Cached pointer to the inter region remembered set */
	_interRegionRememberedSet = MM_GCExtensions::getExtensions(env)->interRegionRememberedSet;

	_cacheLineAlignment = CACHE_LINE_SIZE;

	/* TODO: how to determine this value? It should be large enough that each thread does
	 * real work, but small enough to give good sharing
	 */
	/* Note: this value should divide evenly into the arraylet leaf size so that each chunk
	 * is a block of contiguous memory
	 */
	_arraySplitSize = 4096;
	
	/* allocate the per-thread, per-compact-group data structures */
	Assert_MM_true(0 != _extensions->gcThreadCount);
	UDATA allocateSize = sizeof(MM_CopyForwardCompactGroup) * _extensions->gcThreadCount * _compactGroupMaxCount;
	_compactGroupBlock = (MM_CopyForwardCompactGroup *)_extensions->getForge()->allocate(allocateSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _compactGroupBlock) {
		return false;
	}
	
	/* Calculate compressed Survivor table size in bytes */
	UDATA compressedSurvivorTableSize = _extensions->heap->getMaximumPhysicalRange() / (CARD_SIZE * BITS_PER_BYTE);
	_compressedSurvivorTable  = (UDATA *)env->getForge()->allocate(compressedSurvivorTableSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _compressedSurvivorTable) {
		return false;
	}

	return true;
}

void
MM_CopyForwardScheme::tearDown(MM_EnvironmentVLHGC *env)
{
	_cacheFreeList.tearDown(env);
	if (NULL != _cacheScanLists) {
		UDATA listCount = _scanCacheListSize;
		for (UDATA i = 0; i < listCount; i++) {
			_cacheScanLists[i].tearDown(env);
		}
		env->getForge()->free(_cacheScanLists);
		_cacheScanLists = NULL;
	}

	if (NULL != _scanCacheMonitor) {
		omrthread_monitor_destroy(_scanCacheMonitor);
		_scanCacheMonitor = NULL;
	}

	if (NULL != _reservedRegionList) {
		for (UDATA index = 0; index < _compactGroupMaxCount; index++) {
			for (UDATA sublistIndex = 0; sublistIndex < MM_ReservedRegionListHeader::MAX_SUBLISTS; sublistIndex++) {
				_reservedRegionList[index]._sublists[sublistIndex]._lock.tearDown();
			}
			_reservedRegionList[index]._freeMemoryCandidatesLock.tearDown();
		}
		env->getForge()->free(_reservedRegionList);
		_reservedRegionList = NULL;
	}
	
	if (NULL != _compactGroupBlock) {
		env->getForge()->free(_compactGroupBlock);
		_compactGroupBlock = NULL;
	}

	if (NULL != _compressedSurvivorTable) {
		env->getForge()->free(_compressedSurvivorTable);
		_compressedSurvivorTable = NULL;
	}
}

MM_AllocationContextTarok *
MM_CopyForwardScheme::getPreferredAllocationContext(MM_AllocationContextTarok *suggestedContext, J9Object *objectPtr)
{
	MM_AllocationContextTarok *preferredContext = suggestedContext;

	if (preferredContext == _commonContext) {
		preferredContext = getContextForHeapAddress(objectPtr);
	} /* no code beyond this point without modifying else statement below */
	return preferredContext;
}

void
MM_CopyForwardScheme::raiseAbortFlag(MM_EnvironmentVLHGC *env)
{
	if (!_abortFlag) {
		bool didSetFlag = false;
		omrthread_monitor_enter(*_workQueueMonitorPtr);
		if (!_abortFlag) {
			_abortFlag = true;
			didSetFlag = true;
			/* if any threads are waiting, notify them so that they can get out of the monitor since nobody else is going to push work for them */
			if (0 != *_workQueueWaitCountPtr) {
				omrthread_monitor_notify_all(*_workQueueMonitorPtr);
			}
		}
		omrthread_monitor_exit(*_workQueueMonitorPtr);

		if (didSetFlag) {
			env->_copyForwardStats._aborted = true;

			Trc_MM_CopyForwardScheme_abortFlagRaised(env->getLanguageVMThread());
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			TRIGGER_J9HOOK_MM_PRIVATE_COPY_FORWARD_ABORT(MM_GCExtensions::getExtensions(env)->privateHookInterface, env->getOmrVMThread(), j9time_hires_clock(), J9HOOK_MM_PRIVATE_COPY_FORWARD_ABORT);
		}
	}
}

/**
 * Clear any global stats associated to the copy forward scheme.
 */
void
MM_CopyForwardScheme::clearGCStats(MM_EnvironmentVLHGC *env)
{
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats.clear();
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._workPacketStats.clear();
}

void
MM_CopyForwardScheme::updateLeafRegions(MM_EnvironmentVLHGC *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->isArrayletLeaf()) {
			J9Object *spineObject = (J9Object *)region->_allocateData.getSpine();
			Assert_MM_true(NULL != spineObject);

			J9Object *updatedSpineObject = updateForwardedPointer(spineObject);
			if (updatedSpineObject != spineObject) {
				MM_HeapRegionDescriptorVLHGC *spineRegion = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(spineObject);
				MM_HeapRegionDescriptorVLHGC *updatedSpineRegion = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(updatedSpineObject);

				Assert_MM_true(spineRegion->_markData._shouldMark);
				Assert_MM_true(spineRegion != updatedSpineRegion);
				Assert_MM_true(updatedSpineRegion->containsObjects());

				/* we need to move the leaf to another region's leaf list since its spine has moved */
				region->_allocateData.removeFromArrayletLeafList();
				region->_allocateData.addToArrayletLeafList(updatedSpineRegion);
				region->_allocateData.setSpine((J9IndexableObject *)updatedSpineObject);
			} else if (!isLiveObject(spineObject)) {
				Assert_MM_true(isObjectInEvacuateMemory(spineObject));
				/* the spine is in evacuate space so the arraylet is dead => recycle the leaf */
				/* remove arraylet leaf from list */
				region->_allocateData.removeFromArrayletLeafList();
				/* recycle */
				region->_allocateData.setSpine(NULL);
				region->getSubSpace()->recycleRegion(env, region);
			}
		}
	}
}

void
MM_CopyForwardScheme::preProcessRegions(MM_EnvironmentVLHGC *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	UDATA ownableSynchronizerCandidates = 0;
	UDATA ownableSynchronizerCountInEden = 0;

	_regionCountCannotBeEvacuated = 0;

	while (NULL != (region = regionIterator.nextRegion())) {
		region->_copyForwardData._survivor = false;
		region->_copyForwardData._freshSurvivor = false;
		if (region->containsObjects()) {
			region->_copyForwardData._initialLiveSet = true;
			region->_copyForwardData._evacuateSet = region->_markData._shouldMark;
			if (region->_markData._shouldMark) {
				region->getUnfinalizedObjectList()->startUnfinalizedProcessing();
				ownableSynchronizerCandidates += region->getOwnableSynchronizerObjectList()->getObjectCount();
				if (region->isEden()) {
					ownableSynchronizerCountInEden += region->getOwnableSynchronizerObjectList()->getObjectCount();
				}
				region->getOwnableSynchronizerObjectList()->startOwnableSynchronizerProcessing();
				region->getContinuationObjectList()->startProcessing();
				Assert_MM_true(region->getRememberedSetCardList()->isAccurate());
				if ((region->_criticalRegionsInUse > 0) || !env->_cycleState->_shouldRunCopyForward || (100 == _extensions->fvtest_forceCopyForwardHybridRatio) || (randomDecideForceNonEvacuatedRegion(_extensions->fvtest_forceCopyForwardHybridRatio))) {
					/* set the region is noEvacuation for copyforward collector */
					region->_markData._noEvacuation = true;
					_regionCountCannotBeEvacuated += 1;
				} else if ((_regionCountReservedNonEvacuated > 0) && region->isEden()){
					_regionCountReservedNonEvacuated -= 1;
					_regionCountCannotBeEvacuated += 1;
					region->_markData._noEvacuation = true;
				} else {
					region->_markData._noEvacuation = false;
				}
			}
		} else {
			region->_copyForwardData._evacuateSet = false;
		}
		
		region->getReferenceObjectList()->resetPriorLists();
		Assert_MM_false(region->_copyForwardData._requiresPhantomReferenceProcessing);
	}

	/* reset _regionCountReservedNonEvacuated */
	_regionCountReservedNonEvacuated = 0;
	/* ideally allocationStats._ownableSynchronizerObjectCount should be equal with ownableSynchronizerCountInEden, 
	 * in case partial constructing ownableSynchronizerObject has been moved during previous PGC, notification for new allocation would happen after gc,
	 * so it is counted for new allocation, but not in Eden region. loose assertion for this special case
	 */
	Assert_MM_true(_extensions->allocationStats._ownableSynchronizerObjectCount >= ownableSynchronizerCountInEden);
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._ownableSynchronizerCandidates = ownableSynchronizerCandidates;
}

void
MM_CopyForwardScheme::postProcessRegions(MM_EnvironmentVLHGC *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	UDATA survivorSetRegionCount = 0;

	while (NULL != (region = regionIterator.nextRegion())) {
		MM_MemoryPool *pool = region->getMemoryPool();
		if (region->_copyForwardData._evacuateSet) {
			if (region->isEden()) {
				static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._edenEvacuateRegionCount += 1;
			} else {
				static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._nonEdenEvacuateRegionCount += 1;
			}
		} else if (region->isFreshSurvivorRegion()) {
			/* check Eden Survivor Regions */
			if (0 == region->getLogicalAge()) {
				static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._edenSurvivorRegionCount += 1;
			} else {
				static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._nonEdenSurvivorRegionCount += 1;
			}
		}

		/* Any region which is part of the survivor set should be set to "shouldMark" to appear as part of the collection set (and be swept, etc) */
		if (region->isSurvivorRegion()) {
			Assert_MM_true(region->containsObjects());
			Assert_MM_false(region->_copyForwardData._evacuateSet);
			Assert_MM_false(region->_markData._shouldMark);
			Assert_MM_false(region->_reclaimData._shouldReclaim);

			/* we do not count non-fresh region, only regions that we acquired as free */
			if (region->isFreshSurvivorRegion()) {
				survivorSetRegionCount += 1;
			} else {
				((MM_MemoryPoolAddressOrderedList *)pool)->resetFirstUnalignedFreeEntry();
			}

			/* store back the remaining memory in the pool as free memory */
			region->_sweepData._alreadySwept = true;
			if (pool->getFreeMemoryAndDarkMatterBytes() == region->getSize()) {
				/* Collector converted this region from FREE/IDLE to ADDRESS_ORDERED, but never ended up using it
				 * (for example allocated some space but lost on forwarding the object). Converting it back to free
				 */
				pool->reset(MM_MemoryPool::any);
				region->getSubSpace()->recycleRegion(env, region);
			} else {
				/* this is non-empty merged region - estimate its age based on compact group */
				setAllocationAgeForMergedRegion(env, region);
			}
		}

		/* Clear any copy forward data */
		region->_copyForwardData._initialLiveSet = false;
		region->_copyForwardData._requiresPhantomReferenceProcessing = false;
		region->_copyForwardData._survivor = false;
		region->_copyForwardData._freshSurvivor = false;

		if (region->_copyForwardData._evacuateSet) {
			Assert_MM_true(region->_sweepData._alreadySwept);
			if (abortFlagRaised() || region->_markData._noEvacuation) {
				if (region->getRegionType() == MM_HeapRegionDescriptor::ADDRESS_ORDERED) {
					region->setRegionType(MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED);
				} else {
					Assert_MM_true(region->getRegionType() == MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED);
				}
				Assert_MM_false(region->_previousMarkMapCleared);
				/* we want to sweep and compact this region since we may have failed to completely evacuate it */
				Assert_MM_true(region->_markData._shouldMark);
				region->_sweepData._alreadySwept = false;
				region->_reclaimData._shouldReclaim = true;
			} else {
				pool->reset(MM_MemoryPool::any);
				region->getSubSpace()->recycleRegion(env, region);
			}
			region->_copyForwardData._evacuateSet = false;
		}
	}

	env->_cycleState->_pgcData._survivorSetRegionCount = survivorSetRegionCount;
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._nonEvacuateRegionCount = _regionCountCannotBeEvacuated;
}

/****************************************
 * Copy and Forward implementation
 ****************************************
 */

bool
MM_CopyForwardScheme::isLiveObject(J9Object *objectPtr)
{
	bool result = true;

	if (NULL != objectPtr) {
		Assert_MM_true(isHeapObject(objectPtr));

		if (!isObjectInSurvivorMemory(objectPtr)) {
			result = _markMap->isBitSet(objectPtr);
		}
	}

	return result;
}


MMINLINE bool
MM_CopyForwardScheme::isObjectInEvacuateMemory(J9Object *objectPtr)
{
	bool result = false;

	if (NULL != objectPtr) {
		result = isObjectInEvacuateMemoryNoCheck(objectPtr);
	}
	return result;
}

MMINLINE bool
MM_CopyForwardScheme::isObjectInEvacuateMemoryNoCheck(J9Object *objectPtr)
{
	bool result = false;

	MM_HeapRegionDescriptorVLHGC *region = NULL;
	region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(objectPtr);
	result = region->_markData._shouldMark;
	return result;
}

MMINLINE bool
MM_CopyForwardScheme::isObjectInSurvivorMemory(J9Object *objectPtr)
{
	bool result = false;

	if (NULL != objectPtr) {
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(objectPtr);
		Assert_MM_true(region->_copyForwardData._initialLiveSet || (!region->_markData._shouldMark && !region->_copyForwardData._initialLiveSet));
		result = region->isFreshSurvivorRegion();
		if (!result && region->isSurvivorRegion()) {
			result = isCompressedSurvivor((void*)objectPtr);
		}
	}
	return result;
}

MMINLINE bool
MM_CopyForwardScheme::isObjectInNurseryMemory(J9Object *objectPtr)
{
	bool result = false;

	if (NULL != objectPtr) {
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(objectPtr);
		result = region->_markData._shouldMark || isObjectInSurvivorMemory(objectPtr);
	}
	return result;
}

MMINLINE void
MM_CopyForwardScheme::reinitCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache, void *base, void *top, UDATA compactGroup)
{
	MM_CopyForwardCompactGroup *compactGroupForMarkData = &(env->_copyForwardCompactGroups[compactGroup]);
	Assert_MM_true(cache == compactGroupForMarkData->_copyCache);
	cache->cacheBase = base;
	cache->cacheAlloc = base;
	cache->scanCurrent = base;
	cache->_hasPartiallyScannedObject = false;
	cache->cacheTop = top;

	/* set the mark map cached values to the initial state */
	/* Count one slot before the base in order to get the true atomic head location.  Regions who do not start on a partial boundary will never see
	 * the slot previous.
	 */
	if (base == _heapBase) {
		/* Going below heap base would be strange - just use _heapTop which won't collide with anything */
		compactGroupForMarkData->_markMapAtomicHeadSlotIndex = _markMap->getSlotIndex((J9Object *)_heapTop);
	} else {
		compactGroupForMarkData->_markMapAtomicHeadSlotIndex = _markMap->getSlotIndex((J9Object *) (((UDATA)base) - _markMap->getObjectGrain()));
	}
	compactGroupForMarkData->_markMapAtomicTailSlotIndex = _markMap->getSlotIndex((J9Object *)top);
	compactGroupForMarkData->_markMapPGCSlotIndex = 0;
	compactGroupForMarkData->_markMapPGCBitMask = 0;
	compactGroupForMarkData->_markMapGMPSlotIndex = 0;
	compactGroupForMarkData->_markMapGMPBitMask = 0;
	
	Assert_MM_true(compactGroup < _compactGroupMaxCount);
	cache->_compactGroup = compactGroup;
	Assert_MM_true(0.0 == cache->_allocationAgeSizeProduct);
	
	MM_HeapRegionDescriptorVLHGC * region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(cache->cacheBase);
	Trc_MM_CopyForwardScheme_reinitCache(env->getLanguageVMThread(), _regionManager->mapDescriptorToRegionTableIndex(region), cache,
			region->getAllocationAgeSizeProduct() / (1024 * 1024) / (1024 * 1024), (double)((UDATA)cache->cacheAlloc - (UDATA)region->getLowAddress()) / (1024 * 1024));

	/* store back the given flags */
	cache->flags = OMR_COPYSCAN_CACHE_TYPE_COPY | (cache->flags & OMR_COPYSCAN_CACHE_MASK_PERSISTENT);
}

MMINLINE void
MM_CopyForwardScheme::reinitArraySplitCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache, J9IndexableObject *array, UDATA nextIndex)
{
	cache->cacheBase = array;
	cache->cacheAlloc = array;
	cache->scanCurrent = array;
	cache->_hasPartiallyScannedObject = false;
	cache->cacheTop = array;
	cache->_arraySplitIndex = nextIndex;

	/* store back the appropriate flags */
	cache->flags = (OMR_COPYSCAN_CACHE_TYPE_SPLIT_ARRAY | OMR_COPYSCAN_CACHE_TYPE_CLEARED) | (cache->flags & OMR_COPYSCAN_CACHE_MASK_PERSISTENT);
}

void
MM_CopyForwardScheme::clearReservedRegionLists(MM_EnvironmentVLHGC *env)
{
	Trc_MM_CopyForwardScheme_clearReservedRegionLists_Entry(env->getLanguageVMThread(), _compactGroupMaxCount);
	
	for (UDATA index = 0; index < _compactGroupMaxCount; index++) {
		Trc_MM_CopyForwardScheme_clearReservedRegionLists_compactGroup(env->getLanguageVMThread(), index, _reservedRegionList[index]._evacuateRegionCount, _reservedRegionList[index]._sublistCount, _reservedRegionList[index]._maxSublistCount, _reservedRegionList[index]._freeMemoryCandidateCount);
		if (0 == _reservedRegionList[index]._freeMemoryCandidateCount) {
			Assert_MM_true(NULL == _reservedRegionList[index]._freeMemoryCandidates);
		} else {
			Assert_MM_true(NULL != _reservedRegionList[index]._freeMemoryCandidates);
		}
		
		for (UDATA sublistIndex = 0; sublistIndex < _reservedRegionList[index]._sublistCount; sublistIndex++) {
			MM_ReservedRegionListHeader::Sublist *regionList = &_reservedRegionList[index]._sublists[sublistIndex];
			MM_HeapRegionDescriptorVLHGC *region = regionList->_head;
	
			while (NULL != region) {
				MM_HeapRegionDescriptorVLHGC *next = region->_copyForwardData._nextRegion;

				releaseRegion(env, regionList, region);
				region = next;
			}
	
			if (0 != regionList->_cacheAcquireCount) {
				Trc_MM_CopyForwardScheme_clearReservedRegionLists_sublist(env->getLanguageVMThread(), index, sublistIndex, regionList->_cacheAcquireCount, regionList->_cacheAcquireBytes, regionList->_cacheAcquireBytes / regionList->_cacheAcquireCount);
			}
			
			regionList->_head = NULL;
			regionList->_cacheAcquireCount = 0;
			regionList->_cacheAcquireBytes = 0;
		}
		_reservedRegionList[index]._sublistCount = 1;
		_reservedRegionList[index]._maxSublistCount = 1;
		_reservedRegionList[index]._evacuateRegionCount = 0;
		_reservedRegionList[index]._freeMemoryCandidates = NULL;
		_reservedRegionList[index]._freeMemoryCandidateCount = 0;
	}
	
	Trc_MM_CopyForwardScheme_clearReservedRegionLists_Exit(env->getLanguageVMThread());
}

MM_HeapRegionDescriptorVLHGC *
MM_CopyForwardScheme::acquireEmptyRegion(MM_EnvironmentVLHGC *env, MM_ReservedRegionListHeader::Sublist *regionList, UDATA compactGroup)
{
	MM_HeapRegionDescriptorVLHGC *newRegion = NULL;

	if (!_failedToExpand) {
		UDATA allocationContextNumber = MM_CompactGroupManager::getAllocationContextNumberFromGroup(env, compactGroup);
		MM_AllocationContextTarok *allocationContext = (MM_AllocationContextTarok *)_extensions->globalAllocationManager->getAllocationContextByIndex(allocationContextNumber);

		newRegion = allocationContext->collectorAcquireRegion(env);

		if (NULL != newRegion) {
			MM_CycleState *cycleState = env->_cycleState;
			MM_CycleState *externalCycleState = env->_cycleState->_externalCycleState;
			
			/* a new region starts as ADDRESS_ORDERED but we will always have valid mark map data for this region so set its type now */
			newRegion->setMarkMapValid();		
			if (newRegion->_previousMarkMapCleared) {
				newRegion->_previousMarkMapCleared = false;
			} else {
				cycleState->_markMap->setBitsForRegion(env, newRegion, true);
			}
			if (NULL != externalCycleState) {
				if (newRegion->_nextMarkMapCleared) {
					newRegion->_nextMarkMapCleared = false;
					if (_extensions->tarokEnableExpensiveAssertions) {
						Assert_MM_true(externalCycleState->_markMap->checkBitsForRegion(env, newRegion));
					}
				} else {
					externalCycleState->_markMap->setBitsForRegion(env, newRegion, true);
				}
			}

			Assert_MM_true(NULL == newRegion->getUnfinalizedObjectList()->getHeadOfList());
			Assert_MM_true(NULL == newRegion->getOwnableSynchronizerObjectList()->getHeadOfList());
			Assert_MM_true(NULL == newRegion->getContinuationObjectList()->getHeadOfList());
			Assert_MM_false(newRegion->_markData._shouldMark);

			/*
			 * set logical age here to have a compact groups working properly
			 * real allocation age will be updated after PGC
			 */
			UDATA logicalRegionAge = MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup);
			newRegion->setAge(0, logicalRegionAge);

			Assert_MM_true(newRegion->getReferenceObjectList()->isSoftListEmpty());
			Assert_MM_true(newRegion->getReferenceObjectList()->isWeakListEmpty());
			Assert_MM_true(newRegion->getReferenceObjectList()->isPhantomListEmpty());

			setRegionAsSurvivor(env, newRegion, true);
			insertRegionIntoLockedList(env, regionList, newRegion);
		} else {
			/* record that we failed to expand so that we stop trying during this collection */
			_failedToExpand = true;
		}
	}

	return newRegion;
}

void
MM_CopyForwardScheme::insertRegionIntoLockedList(MM_EnvironmentVLHGC *env, MM_ReservedRegionListHeader::Sublist *regionList, MM_HeapRegionDescriptorVLHGC *newRegion)
{
	newRegion->_copyForwardData._nextRegion = regionList->_head;
	newRegion->_copyForwardData._previousRegion = NULL;

	if (NULL != regionList->_head) {
		regionList->_head->_copyForwardData._previousRegion = newRegion;
	}

	regionList->_head = newRegion;
}

void
MM_CopyForwardScheme::releaseRegion(MM_EnvironmentVLHGC *env, MM_ReservedRegionListHeader::Sublist *regionList, MM_HeapRegionDescriptorVLHGC *region)
{
	MM_HeapRegionDescriptorVLHGC *next = region->_copyForwardData._nextRegion;
	MM_HeapRegionDescriptorVLHGC *previous = region->_copyForwardData._previousRegion;

	if (NULL != next) {
		next->_copyForwardData._previousRegion = previous;
	}
	if (NULL != previous) {
		previous->_copyForwardData._nextRegion = next;
		Assert_MM_false(previous == previous->_copyForwardData._nextRegion);
	} else {
		regionList->_head = next;
	}
	region->_copyForwardData._nextRegion = NULL;
	region->_copyForwardData._previousRegion = NULL;
}

void *
MM_CopyForwardScheme::reserveMemoryForObject(MM_EnvironmentVLHGC *env, uintptr_t compactGroup, uintptr_t objectSize, MM_LightweightNonReentrantLock** listLock)
{
	MM_AllocateDescription allocDescription(objectSize, 0, false, false);
	uintptr_t sublistCount = _reservedRegionList[compactGroup]._sublistCount;
	Assert_MM_true(sublistCount <= MM_ReservedRegionListHeader::MAX_SUBLISTS);
	uintptr_t sublistIndex = env->getWorkerID() % sublistCount;
	MM_ReservedRegionListHeader::Sublist *regionList = &_reservedRegionList[compactGroup]._sublists[sublistIndex];
	void *result = NULL;

	/* Measure the number of acquires before and after we acquire the lock. If it changed, then there is probably contention on the lock. */
	uintptr_t acquireCountBefore = regionList->_cacheAcquireCount;
	regionList->_lock.acquire();
	uintptr_t acquireCountAfter = regionList->_cacheAcquireCount;
	
	/* 
	 * 1. attempt to use an existing region 
	 */
	MM_HeapRegionDescriptorVLHGC *region = regionList->_head;
	while ((NULL == result) && (NULL != region)) {
		MM_MemoryPool *memoryPool = region->getMemoryPool();
		Assert_MM_true(NULL != memoryPool);
		result = memoryPool->collectorAllocate(env, &allocDescription, false);
		if (NULL != result) {
			break;
		}
		region = region->_copyForwardData._nextRegion;
	}

	/* 
	 * 2. attempt to acquire a region from the free memory candidates list
	 */
	if ((NULL == result) && (NULL != _reservedRegionList[compactGroup]._freeMemoryCandidates)) {
		_reservedRegionList[compactGroup]._freeMemoryCandidatesLock.acquire();
		region = _reservedRegionList[compactGroup]._freeMemoryCandidates;
		MM_HeapRegionDescriptorVLHGC *resultRegion = NULL;
		while ((NULL == result) && (NULL != region)) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);

			((MM_MemoryPoolAddressOrderedList *)memoryPool)->initialFirstUnalignedFreeEntry();
			result = memoryPool->collectorAllocate(env, &allocDescription, false);

			if (NULL !=  result) {
				resultRegion = region;
				break;
			}
			region = region->_copyForwardData._nextRegion;
		}
		if (NULL != result) {
			/* remove this region from the common free memory candidates list and add it to our own sublist */
			Assert_MM_true(NULL != resultRegion);
			removeFreeMemoryCandidate(env, &_reservedRegionList[compactGroup], resultRegion);
			insertRegionIntoLockedList(env, regionList, resultRegion);
			convertFreeMemoryCandidateToSurvivorRegion(env, resultRegion);
		}
		_reservedRegionList[compactGroup]._freeMemoryCandidatesLock.release();
	}

	/* 
	 * 3. attempt to acquire an empty region 
	 */
	if (NULL == result) {
		region = acquireEmptyRegion(env, regionList, compactGroup);
		if (NULL != region) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);
			result = memoryPool->collectorAllocate(env, &allocDescription, false);
			Assert_MM_true(NULL != result);  /* This should not have failed at this point */
		}
	}

	if (NULL != result) {
		regionList->_cacheAcquireCount += 1;
		regionList->_cacheAcquireBytes += allocDescription.getBytesRequested();
		setCompressedSurvivorCards(env, (void*)result, (void*) ((uintptr_t)result + allocDescription.getBytesRequested()));
	}

	regionList->_lock.release();
	*listLock = &regionList->_lock;
	
	Assert_MM_true(acquireCountBefore <= acquireCountAfter);
	if ((NULL != result) && (sublistCount < _reservedRegionList[compactGroup]._maxSublistCount)) {
		uintptr_t acceptableAcquireCountForContention = acquireCountBefore + _regionSublistContentionThreshold;
		if (acceptableAcquireCountForContention < acquireCountAfter) {
			/* contention detected on lock -- attempt to increase the number of sublists */
			MM_AtomicOperations::lockCompareExchange(&_reservedRegionList[compactGroup]._sublistCount, sublistCount, sublistCount + 1);
		}
	}
	
	return result;
}

bool
MM_CopyForwardScheme::reserveMemoryForCache(MM_EnvironmentVLHGC *env, uintptr_t compactGroup, uintptr_t maxCacheSize, void **addrBase, void **addrTop, MM_LightweightNonReentrantLock** listLock)
{
	MM_AllocateDescription allocDescription(maxCacheSize, 0, false, false);
	bool result = false;
	uintptr_t sublistCount = _reservedRegionList[compactGroup]._sublistCount;
	Assert_MM_true(sublistCount <= MM_ReservedRegionListHeader::MAX_SUBLISTS);
	uintptr_t sublistIndex = env->getWorkerID() % sublistCount;
	MM_ReservedRegionListHeader::Sublist *regionList = &_reservedRegionList[compactGroup]._sublists[sublistIndex];

	/* Measure the number of acquires before and after we acquire the lock. If it changed, then there is probably contention on the lock. */
	uintptr_t acquireCountBefore = regionList->_cacheAcquireCount;
	regionList->_lock.acquire();
	uintptr_t acquireCountAfter = regionList->_cacheAcquireCount;

	/* 
	 * 1. attempt to use an existing region 
	 */
	MM_HeapRegionDescriptorVLHGC *region = regionList->_head;
	while ((!result) && (NULL != region)) {
		MM_MemoryPool *memoryPool = region->getMemoryPool();
		Assert_MM_true(NULL != memoryPool);
		
		void *tlhBase = NULL;
		void *tlhTop = NULL;
		result = (NULL != memoryPool->collectorAllocateTLH(env, &allocDescription, maxCacheSize, tlhBase, tlhTop, false));

		MM_HeapRegionDescriptorVLHGC *next = region->_copyForwardData._nextRegion;
		if (result) {
			*addrBase = tlhBase;
			*addrTop = tlhTop;
			break;
		} else {
			releaseRegion(env, regionList, region);
		}
		region = next;
	}

	/* 
	 * 2. attempt to acquire a region from the free memory candidates list
	 */
	if ((!result) && (NULL != _reservedRegionList[compactGroup]._freeMemoryCandidates)) {
		_reservedRegionList[compactGroup]._freeMemoryCandidatesLock.acquire();
		region = _reservedRegionList[compactGroup]._freeMemoryCandidates;
		MM_HeapRegionDescriptorVLHGC *resultRegion = NULL;
		while ((!result) && (NULL != region)) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);

			void *tlhBase = NULL;
			void *tlhTop = NULL;
			((MM_MemoryPoolAddressOrderedList *)memoryPool)->initialFirstUnalignedFreeEntry();
			result = (NULL != memoryPool->collectorAllocateTLH(env, &allocDescription, maxCacheSize, tlhBase, tlhTop, false));
			if (result) {
				*addrBase = tlhBase;
				*addrTop = tlhTop;
				/* remove this region from the common free memory candidates list and add it to our own sublist */
				removeFreeMemoryCandidate(env, &_reservedRegionList[compactGroup], region);
				insertRegionIntoLockedList(env, regionList, region);
				convertFreeMemoryCandidateToSurvivorRegion(env, region);
				break;

			}
			Assert_MM_true(0 == memoryPool->getActualFreeMemorySize());
			resultRegion = region;
			region = region->_copyForwardData._nextRegion;
			removeFreeMemoryCandidate(env, &_reservedRegionList[compactGroup], resultRegion);
		}
		_reservedRegionList[compactGroup]._freeMemoryCandidatesLock.release();
	}

	/* 
	 * 3. attempt to acquire an empty region 
	 */
	if (!result) {
		region = acquireEmptyRegion(env, regionList, compactGroup);
		if (NULL != region) {
			MM_MemoryPool *memoryPool = region->getMemoryPool();
			Assert_MM_true(NULL != memoryPool);

			void *tlhBase = NULL;
			void *tlhTop = NULL;
			/* note that we called alignAllocationPointer on this pool when adding it to our copy-forward destination list so this address won't share a card with non-moving objects */
			result = (NULL != memoryPool->collectorAllocateTLH(env, &allocDescription, maxCacheSize, tlhBase, tlhTop, false));

			Assert_MM_true(result);  /* This should not have failed at this point */

			*addrBase = tlhBase;
			*addrTop = tlhTop;
		}
	}

	if (result) {
		regionList->_cacheAcquireCount += 1;
		regionList->_cacheAcquireBytes += ((uintptr_t)*addrTop) - ((uintptr_t)*addrBase);
		setCompressedSurvivorCards(env, *addrBase, *addrTop);
	}

	regionList->_lock.release();
	*listLock = &regionList->_lock;

	Assert_MM_true(acquireCountBefore <= acquireCountAfter);
	if (result && (sublistCount < _reservedRegionList[compactGroup]._maxSublistCount)) {
		uintptr_t acceptableAcquireCountForContention = acquireCountBefore + _regionSublistContentionThreshold;
		if (acceptableAcquireCountForContention < acquireCountAfter) {
			/* contention detected on lock -- attempt to increase the number of sublists */
			MM_AtomicOperations::lockCompareExchange(&_reservedRegionList[compactGroup]._sublistCount, sublistCount, sublistCount + 1);
		}
	}

	return result;
}

MM_CopyScanCacheVLHGC *
MM_CopyForwardScheme::createScanCacheForOverflowInHeap(MM_EnvironmentVLHGC *env)
{
	bool const compressed = env->compressObjectReferences();
	MM_CopyScanCacheVLHGC * result = NULL;
	
	_cacheFreeList.lock();

	/* check to see if another thread already did this */
	result = _cacheFreeList.popCacheNoLock(env);
	/* find out how many bytes are required to allocate a chunk in the heap */
	UDATA cacheSizeInBytes = MM_CopyScanCacheChunkVLHGCInHeap::bytesRequiredToAllocateChunkInHeap(env);
	/* this we are allocating this in a part of the heap which the copy-forward mechanism will have to walk before it finishes, we need to hide this in a hole so add that header size */
	UDATA bytesToReserve = sizeof(MM_HeapLinkedFreeHeader) + cacheSizeInBytes;
	UDATA suggestedCompactGroup = 0;
	while ((NULL == result) && (suggestedCompactGroup < _compactGroupMaxCount)) {
		MM_LightweightNonReentrantLock *listLock = NULL;
		void *extentBase = reserveMemoryForObject(env, suggestedCompactGroup, bytesToReserve, &listLock);
		if (NULL != extentBase) {
			/* this is not object memory so account for it as free memory while we have the size */
			/* lock the region list for this group and write-back the memory we consumed as free space immediately (this is a rare case so the
			 * lock is an acceptable cost to avoid trying to defer the write-back of the free memory size since this case is unusual)
			 */
			Assert_MM_true(NULL != listLock);
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(extentBase);
			MM_MemoryPool *pool = region->getMemoryPool();
			listLock->acquire();
			pool->incrementDarkMatterBytes(bytesToReserve);
			listLock->release();
			/* save out how much memory we wasted so the caller can account for it */
			memset(extentBase, 0x0, bytesToReserve);
			void *cacheBase = (void *)((MM_HeapLinkedFreeHeader *)extentBase + 1);
			MM_HeapLinkedFreeHeader::fillWithHoles(extentBase, bytesToReserve, compressed);
			result = _cacheFreeList.allocateCacheEntriesInExistingMemory(env, cacheBase, cacheSizeInBytes);
		}
		suggestedCompactGroup += 1;
	}
	
	_cacheFreeList.unlock();

	return result;
}

UDATA
MM_CopyForwardScheme::getDesiredCopyCacheSize(MM_EnvironmentVLHGC *env, UDATA compactGroup)
{
	/* The desired cache size is a fraction of the number of bytes we've copied so far. 
	 * The upper bound on fragmentation is approximately this fraction, with the expected fragmentation about half of the fraction.
	 */
	const double allowableFragmentation = 2.0 * _extensions->tarokCopyForwardFragmentationTarget;
	const double bytesCopiedInCompactGroup = (double)(env->_copyForwardCompactGroups[compactGroup]._edenStats._copiedBytes + env->_copyForwardCompactGroups[compactGroup]._nonEdenStats._copiedBytes);
	UDATA desiredCacheSize = (UDATA)(allowableFragmentation * bytesCopiedInCompactGroup);
	MM_CompactGroupPersistentStats *stats = &(_extensions->compactGroupPersistentStats[compactGroup]);
	UDATA perThreadSurvivalEstimatedSize = (UDATA)(((double)stats->_measuredLiveBytesBeforeCollectInCollectedSet * stats->_historicalSurvivalRate * allowableFragmentation) / (double)env->_currentTask->getThreadCount());
	desiredCacheSize = OMR_MAX(desiredCacheSize, perThreadSurvivalEstimatedSize);
	desiredCacheSize = MM_Math::roundToCeiling(_objectAlignmentInBytes, desiredCacheSize);
	desiredCacheSize = OMR_MIN(desiredCacheSize, _maxCacheSize);
	desiredCacheSize = OMR_MAX(desiredCacheSize, _minCacheSize);
	return desiredCacheSize;
}

MM_CopyScanCacheVLHGC *
MM_CopyForwardScheme::reserveMemoryForCopy(MM_EnvironmentVLHGC *env, J9Object *objectToEvacuate, MM_AllocationContextTarok *reservingContext, uintptr_t objectReserveSizeInBytes)
{
	void *addrBase = NULL;
	void *addrTop = NULL;
	uintptr_t minimumRequiredCacheSize = objectReserveSizeInBytes;
	uintptr_t minimumSingleObjectAllocateSize = _extensions->tlhSurvivorDiscardThreshold;

	Assert_MM_objectAligned(env, objectReserveSizeInBytes);

	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(objectToEvacuate);
	uintptr_t compactGroup = MM_CompactGroupManager::getCompactGroupNumberInContext(env, region, reservingContext);
	MM_CopyForwardCompactGroup *copyForwardCompactGroup = &env->_copyForwardCompactGroups[compactGroup];
	
	Assert_MM_true(compactGroup < _compactGroupMaxCount);

	MM_CopyScanCacheVLHGC *copyCache = copyForwardCompactGroup->_copyCache;

retry:
	/* A survivor copy scan cache exists - check if there is room */
	if ((NULL == copyCache) || (((uintptr_t)copyCache->cacheTop - (uintptr_t)copyCache->cacheAlloc) < minimumRequiredCacheSize)) {
		/* There is no room for current copy cache */
		MM_LightweightNonReentrantLock *listLock = NULL;
		if (minimumRequiredCacheSize < copyForwardCompactGroup->_failedAllocateSize) {
			/* try to use TLH remainder from previous discard */
			if (((uintptr_t)copyForwardCompactGroup->_TLHRemainderTop - (uintptr_t)copyForwardCompactGroup->_TLHRemainderBase) >= minimumRequiredCacheSize) {
				addrBase = copyForwardCompactGroup->_TLHRemainderBase;
				addrTop = copyForwardCompactGroup->_TLHRemainderTop;
				Assert_MM_true(NULL != copyForwardCompactGroup->_TLHRemainderBase);
				Assert_MM_true(NULL != copyForwardCompactGroup->_TLHRemainderTop);
				copyForwardCompactGroup->resetTLHRemainder();

				uintptr_t sublistCount = _reservedRegionList[compactGroup]._sublistCount;
				uintptr_t sublistIndex = env->getWorkerID() % sublistCount;
				MM_ReservedRegionListHeader::Sublist *regionList = &_reservedRegionList[compactGroup]._sublists[sublistIndex];
				listLock = &regionList->_lock;
			} else if (minimumSingleObjectAllocateSize < minimumRequiredCacheSize) {
				addrBase = reserveMemoryForObject(env, compactGroup, minimumRequiredCacheSize, &listLock);

				if (NULL != addrBase) {
					addrTop = (void *)((uintptr_t)addrBase + minimumRequiredCacheSize);
				} else {
					/* failed to allocate - set the threshold to short-circuit future alloc attempts */
					copyForwardCompactGroup->_failedAllocateSize = minimumRequiredCacheSize;
				}
			}  else {
				UDATA desiredCacheSize = getDesiredCopyCacheSize(env, compactGroup);
				desiredCacheSize = OMR_MAX(desiredCacheSize, minimumRequiredCacheSize);
				if (!reserveMemoryForCache(env, compactGroup, desiredCacheSize, &addrBase, &addrTop, &listLock)) {
					/* failed to allocate - set the threshold to short-circut future alloc attempts:
					 * we should never (in this GC) attempt to allocate a cache (TLH) from this compact group
					 */
					copyForwardCompactGroup->_failedAllocateSize = 0;
				}
			}
		}

		if (NULL != copyCache) {
			/* we can't use this cache as a destination so release local cache first. */
			MM_CopyScanCacheVLHGC * stoppedCache = stopCopyingIntoCache(env, compactGroup);
			Assert_MM_true(stoppedCache == copyCache);

			if (copyCache->isCurrentlyBeingScanned()) {
				/* this cache is already being scanned. The scanning thread will add it to the free list when it's finished */
				copyCache = NULL;
			} else {
				/* assert that deferred or scan cache is not this cache */
				Assert_MM_true(copyCache != env->_scanCache);
				Assert_MM_true(copyCache != env->_deferredScanCache);
				/* Either cache is completely scanned or it has never been scanned.
				 * If it has never been scanned, it is here that we should decide if there is scan work to do
				 * and whether to add to the scan list
				 */
				if (copyCache->isScanWorkAvailable()) {
					/* must not have local references still in use before adding to global list */
					Assert_MM_true(copyCache->cacheBase <= copyCache->cacheAlloc);
					Assert_MM_true(copyCache->cacheAlloc <= copyCache->cacheTop);
					Assert_MM_true(copyCache->scanCurrent <= copyCache->cacheAlloc);
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
					env->_copyForwardStats._releaseScanListCount += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
					addCacheEntryToScanCacheListAndNotify(env, copyCache);
					copyCache = NULL;
				} else {
					/* we have decided to stop copying into this cache so ensure that we won't try to keep using it as one (we will allocate a new cache structure if the allocate succeeds) */
					addCacheEntryToFreeCacheList(env, copyCache);
					copyCache = NULL;
				}
			}
		}

		if (NULL != addrBase) {
			/* allocate from reserveMemory or TLHRemainder */
			Assert_MM_true(NULL == copyCache);

			/* If we didn't already have a copy cache structure or dropped it earlier in the call, allocate a new one */
			copyCache = getFreeCache(env);
			if (NULL != copyCache) {
				copyForwardCompactGroup->_copyCache = copyCache;
				copyForwardCompactGroup->_copyCacheLock = listLock;
				reinitCache(env, copyCache, addrBase, addrTop, compactGroup);

				Assert_MM_true(NULL != listLock);
				Assert_MM_true(0 == copyForwardCompactGroup->_freeMemoryMeasured);
			} else {
				/* ensure that we have realized the abort flag (since getFreeCache only returns NULL if it had to abort) */
				Assert_MM_true(abortFlagRaised());
			}
		}

		if (NULL == copyCache) {
			/* Record stats */
			copyForwardCompactGroup->_failedCopiedObjects += 1;
			copyForwardCompactGroup->_failedCopiedBytes += objectReserveSizeInBytes;
		} else {
			Assert_MM_true(NULL != copyCache->cacheAlloc);
			Assert_MM_true(NULL != copyCache->cacheTop);
			Assert_MM_true(NULL != copyCache->cacheBase);
			if (((uintptr_t)addrTop - (uintptr_t)addrBase) < minimumRequiredCacheSize) {
				/* In case of increased tlhSurvivorDiscardThreshold, we may prefer TLH rather then single object allocation
				 * in which case TLH cache may not be large enough to satisfy the allocation.
				 * We'll try again but force single object allocation.
				 * We could have detected earlier in the method the failed TLH allocate, but doing this late gives a chance
				 * for current cache remainder or new cache to be preserved (if sufficiently large) as thread's TLH remainder
				 */
				Assert_MM_true(_extensions->tlhSurvivorDiscardThreshold > _minCacheSize);
				minimumSingleObjectAllocateSize = _minCacheSize;
				addrBase = NULL;
				addrTop = NULL;

				goto retry;
			}
			if (_extensions->tarokEnableExpensiveAssertions) {
				/* verify that the mark map for this range is clear */
				Assert_MM_true(NULL == MM_HeapMapIterator(_extensions, _markMap, (UDATA*)copyCache->cacheAlloc, (UDATA*)copyCache->cacheTop, false).nextObject());
			}
		}
	}

	return copyCache;
}

MMINLINE bool
MM_CopyForwardScheme::copyAndForward(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, volatile j9object_t* objectPtrIndirect, bool leafType)
{
	J9Object *originalObjectPtr = *objectPtrIndirect;
	J9Object *objectPtr = originalObjectPtr;
	bool success = true;

	if ((NULL != objectPtr) && isObjectInEvacuateMemory(objectPtr)) {
		/* Object needs to be copy and forwarded.  Check if the work has already been done */
		MM_ForwardedHeader forwardHeader(objectPtr, _extensions->compressObjectReferences());
		objectPtr = forwardHeader.getForwardedObject();
		
		if (NULL != objectPtr) {
			/* Object has been copied - update the forwarding information and return */
			*objectPtrIndirect = objectPtr;
		} else {
			Assert_GC_true_with_message(env, (UDATA)0x99669966 == _extensions->objectModel.getPreservedClass(&forwardHeader)->eyecatcher, "Invalid class in objectPtr=%p\n", originalObjectPtr);


			objectPtr = copy(env, reservingContext, &forwardHeader, leafType);
			if (NULL == objectPtr) {
				success = false;
			} else if (originalObjectPtr != objectPtr) {
				/* Update the slot */
				*objectPtrIndirect = objectPtr;
			}
		}
	}

	return success;
}

MMINLINE bool
MM_CopyForwardScheme::copyAndForward(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, GC_SlotObject *slotObject, bool leafType)
{
	J9Object *value = slotObject->readReferenceFromSlot();
	J9Object *preservedValue = value;

	bool success = copyAndForward(env, reservingContext, &value, leafType);

	if (success) {
		if(preservedValue != value) {
			slotObject->writeReferenceToSlot(value);
		}
		_interRegionRememberedSet->rememberReferenceForCopyForward(env, objectPtr, value);
	} else {
		Assert_MM_false(_abortInProgress);
		Assert_MM_true(preservedValue == value);
		env->_workStack.push(env, objectPtr);
	}

	return success;
}

MMINLINE bool
MM_CopyForwardScheme::copyAndForward(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, volatile j9object_t* slot)
{
	bool success = copyAndForward(env, reservingContext, slot);

	if (success) {
		_interRegionRememberedSet->rememberReferenceForCopyForward(env, objectPtr, *slot);
	} else {
		Assert_MM_false(_abortInProgress);
		/* Because there is a caller where the slot could be scanned by multiple threads at once, it is possible on failure that
		 * the value of the slot HAS in fact changed (other thread had room to satisfy).  Because of this, we do cannot check if the preserved
		 * slot value would be unchanged (unlike other copyAndForward() implementations).
		 */
		env->_workStack.push(env, objectPtr);
	}

	return success;
}

MMINLINE bool
MM_CopyForwardScheme::copyAndForwardPointerArray(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9IndexableObject *arrayPtr, UDATA startIndex, GC_SlotObject *slotObject)
{
	J9Object *value = slotObject->readReferenceFromSlot();
	J9Object *preservedValue = value;

	bool success = copyAndForward(env, reservingContext, &value);

	if (success) {
		if (preservedValue != value) {
			slotObject->writeReferenceToSlot(value);
		}
		_interRegionRememberedSet->rememberReferenceForCopyForward(env, (J9Object *)arrayPtr, value);
	} else {
		Assert_MM_false(_abortInProgress);
		Assert_MM_true(preservedValue == value);
		/* We push only the current split unit (from startIndex with size of arraySplit size).
		 * This is to avoid duplicate work which would otherwise be created,
		 * if each failed-to-scan-to-completion copy-scan cache had created the work unit till the end of the array
		 */
		void *element1 = (void *)arrayPtr;
		void *element2 = (void *)((startIndex << PACKET_ARRAY_SPLIT_SHIFT) | PACKET_ARRAY_SPLIT_TAG | PACKET_ARRAY_SPLIT_CURRENT_UNIT_ONLY_TAG);
		Assert_MM_true(startIndex == (((UDATA)element2) >> PACKET_ARRAY_SPLIT_SHIFT));
		env->_workStack.push(env, element1, element2);
	}

	return success;
}

MMINLINE bool
MM_CopyForwardScheme::copyAndForwardObjectClass(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr)
{
	bool success = true;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_extensions->classLoaderRememberedSet->rememberInstance(env, objectPtr);
	if (isDynamicClassUnloadingEnabled()) {
		j9object_t classObject = (j9object_t)J9GC_J9OBJECT_CLAZZ(objectPtr, env)->classObject;
		Assert_MM_true(J9_INVALID_OBJECT != classObject);
		if (copyAndForward(env, reservingContext, &classObject)) {
			/* we don't need to update anything with the new address of the class object since objectPtr points at the immobile J9Class */
		} else {
			/* we failed to copy (and, therefore, mark) the class so we need to scan this object again */
			Assert_MM_false(_abortInProgress);
			env->_workStack.push(env, objectPtr);
			success = false;
		}
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	return success;
}

/**
 * Cleanup after CopyForward work is complete.
 * This should only be called once per collection by the main thread.
 */
void
MM_CopyForwardScheme::mainCleanupForCopyForward(MM_EnvironmentVLHGC *env)
{
	/* make sure that we have dropped any remaining references to any on-heap scan caches which we would have allocated if we hit overflow */
	_cacheFreeList.removeAllHeapAllocatedChunks(env);
	
	if (_extensions->tarokEnableExpensiveAssertions) {
		/* ensure that all managed caches have been returned to the free list */
		Assert_MM_true(_cacheFreeList.getTotalCacheCount() == _cacheFreeList.countCaches());
	}

	Assert_MM_true(static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._ownableSynchronizerCandidates >= static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._ownableSynchronizerSurvived);
}

/**
 * Initialize the copy forward scheme for a garbage collection.
 * Initialize all internal values to start a garbage collect. This should only be
 * called once per collection by the main thread.
 */
void
MM_CopyForwardScheme::mainSetupForCopyForward(MM_EnvironmentVLHGC *env)
{
	clearAbortFlag();
	_abortInProgress = false;
	_clearableProcessingStarted = false;
	_failedToExpand = false;
	_phantomReferenceRegionsToProcess = 0;

	/* Sort all hot fields for all classes as dynamicBreadthFirstScanOrdering is enabled */
	if (MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_DYNAMIC_BREADTH_FIRST == _extensions->scavengerScanOrdering) {
		MM_HotFieldUtil::sortAllHotFieldData(_javaVM, _extensions->globalVLHGCStats.gcCount);
	}

	/* Cache of the mark map */
	_markMap = env->_cycleState->_markMap;

	/* Cache heap ranges for fast "valid object" checks (this can change in an expanding heap situation, so we refetch every cycle) */
	_heapBase = _extensions->heap->getHeapBase();
	_heapTop = _extensions->heap->getHeapTop();

	/* Record any special action for clearing / unloading this cycle */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	_dynamicClassUnloadingEnabled = env->_cycleState->_dynamicClassUnloadingEnabled;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	_collectStringConstantsEnabled = _extensions->collectStringConstants;

	/* ensure heap base is aligned to region size */
	UDATA heapBase = (UDATA)_extensions->heap->getHeapBase();
	UDATA regionSize = _regionManager->getRegionSize();
	Assert_MM_true((0 != regionSize) && (0 == (heapBase % regionSize)));
	
	/* Reinitialize the _doneIndex */
	_doneIndex = 0;

	/* Context 0 is currently our "common destination context" */
	_commonContext = (MM_AllocationContextTarok *)_extensions->globalAllocationManager->getAllocationContextByIndex(0);
	
	/* We don't want to split too aggressively so take the base2 log of our thread count as our current contention trigger.
	 * Note that this number could probably be improved upon but log2 "seemed" to make sense for contention measurement and
	 * provided a measurable performance benefit in the tests we were running.
	 */
	_regionSublistContentionThreshold = MM_Math::floorLog2(_extensions->dispatcher->activeThreadCount());

	_interRegionRememberedSet->setupForPartialCollect(env);
	
	/* Record whether finalizable processing is required in this copy-forward collection */
	_shouldScanFinalizableObjects = _extensions->finalizeListManager->isFinalizableObjectProcessingRequired();

	cleanCompressedSurvivorCardTable(env);
}

/**
 * Per worker thread pre-gc initialization.
 */
void
MM_CopyForwardScheme::workerSetupForCopyForward(MM_EnvironmentVLHGC *env)
{
	/* Reset the copy caches */
	Assert_MM_true(NULL == env->_scanCache);
	Assert_MM_true(NULL == env->_deferredScanCache);

	/* install this thread's compact group structures */
	Assert_MM_true(NULL == env->_copyForwardCompactGroups);
	Assert_MM_true(NULL != _compactGroupBlock);
	env->_copyForwardCompactGroups = &_compactGroupBlock[env->getWorkerID() * _compactGroupMaxCount];

	for (UDATA compactGroup = 0; compactGroup < _compactGroupMaxCount; compactGroup++) {
		env->_copyForwardCompactGroups[compactGroup].initialize(env);
	}

	Assert_MM_true(NULL == env->_lastOverflowedRsclWithReleasedBuffers);
}

/**
 * Merge any per thread GC stats into the main stat structure.
 */
void
MM_CopyForwardScheme::mergeGCStats(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CopyForwardStats *localStats = &env->_copyForwardStats;
	MM_CompactGroupPersistentStats *persistentStats = _extensions->compactGroupPersistentStats;

	/* the following statistics are only updated at the merge point */
	Assert_MM_true(0 == localStats->_copyObjectsTotal);
	Assert_MM_true(0 == localStats->_copyBytesTotal);
	Assert_MM_true(0 == localStats->_copyDiscardBytesTotal);
	Assert_MM_true(0 == localStats->_copyObjectsEden);
	Assert_MM_true(0 == localStats->_copyBytesEden);
	Assert_MM_true(0 == localStats->_copyDiscardBytesEden);
	Assert_MM_true(0 == localStats->_copyObjectsNonEden);
	Assert_MM_true(0 == localStats->_copyBytesNonEden);
	Assert_MM_true(0 == localStats->_copyDiscardBytesNonEden);

	/* sum up the per-compact group data before entering the lock */
	for (UDATA compactGroupNumber = 0; compactGroupNumber < _compactGroupMaxCount; compactGroupNumber++) {
		MM_CopyForwardCompactGroup *compactGroup = &env->_copyForwardCompactGroups[compactGroupNumber];
		UDATA totalCopiedBytes = compactGroup->_edenStats._copiedBytes + compactGroup->_nonEdenStats._copiedBytes;
		UDATA totalLiveBytes = compactGroup->_edenStats._liveBytes + compactGroup->_nonEdenStats._liveBytes;

		localStats->_copyObjectsTotal += compactGroup->_edenStats._copiedObjects + compactGroup->_nonEdenStats._copiedObjects;
		localStats->_copyBytesTotal += totalCopiedBytes;
		localStats->_scanObjectsTotal += compactGroup->_edenStats._scannedObjects + compactGroup->_nonEdenStats._scannedObjects;
		localStats->_scanBytesTotal += compactGroup->_edenStats._scannedBytes + compactGroup->_nonEdenStats._scannedBytes;

		localStats->_copyObjectsEden += compactGroup->_edenStats._copiedObjects;
		localStats->_copyBytesEden += compactGroup->_edenStats._copiedBytes;
		localStats->_scanObjectsEden += compactGroup->_edenStats._scannedObjects;
		localStats->_scanBytesEden += compactGroup->_edenStats._scannedBytes;

		localStats->_copyObjectsNonEden += compactGroup->_nonEdenStats._copiedObjects;
		localStats->_copyBytesNonEden += compactGroup->_nonEdenStats._copiedBytes;
		localStats->_scanObjectsNonEden += compactGroup->_nonEdenStats._scannedObjects;
		localStats->_scanBytesNonEden += compactGroup->_nonEdenStats._scannedBytes;

		localStats->_copyDiscardBytesTotal += compactGroup->_discardedBytes;
		localStats->_TLHRemainderCount += compactGroup->_TLHRemainderCount;

		if (0 == MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroupNumber)) {
			localStats->_copyDiscardBytesEden += compactGroup->_discardedBytes;
		} else {
			localStats->_copyDiscardBytesNonEden += compactGroup->_discardedBytes;
		}
		
		/* use an atomic since other threads may be doing this at the same time */
		if (0 != totalLiveBytes) {
			MM_AtomicOperations::add(&persistentStats[compactGroupNumber]._measuredBytesCopiedFromGroupDuringCopyForward, totalLiveBytes);
		}

		if (0 != totalCopiedBytes) {
			MM_AtomicOperations::add(&persistentStats[compactGroupNumber]._measuredBytesCopiedToGroupDuringCopyForward, totalCopiedBytes);
			MM_AtomicOperations::addU64(&persistentStats[compactGroupNumber]._measuredAllocationAgeToGroupDuringCopyForward, compactGroup->_allocationAge);
		}

		if (0 != (totalCopiedBytes + compactGroup->_discardedBytes)) {
			Trc_MM_CopyForwardScheme_mergeGCStats_efficiency(env->getLanguageVMThread(), compactGroupNumber, totalCopiedBytes, compactGroup->_discardedBytes, (double)(compactGroup->_discardedBytes) / (double)(totalCopiedBytes + compactGroup->_discardedBytes));
		}
	}

	/* Protect the merge with the mutex (this is done by multiple threads in the parallel collector) */
	omrthread_monitor_enter(_extensions->gcStatsMutex);
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats.merge(localStats);
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._workPacketStats.merge(&env->_workPacketStats);
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._irrsStats.merge(&env->_irrsStats);
	omrthread_monitor_exit(_extensions->gcStatsMutex);
	
	/* record the thread-specific parallelism stats in the trace buffer. This partially duplicates info in -Xtgc:parallel */ 
	Trc_MM_CopyForwardScheme_parallelStats(
		env->getLanguageVMThread(),
		(U_32)env->getWorkerID(),
		(U_32)j9time_hires_delta(0, env->_copyForwardStats._workStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)j9time_hires_delta(0, env->_copyForwardStats._completeStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)j9time_hires_delta(0, env->_copyForwardStats._syncStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)j9time_hires_delta(0, env->_copyForwardStats._irrsStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)env->_copyForwardStats._workStallCount,
		(U_32)env->_copyForwardStats._completeStallCount,
		(U_32)env->_copyForwardStats._syncStallCount,
		(U_32)env->_copyForwardStats._irrsStallCount,
		env->_copyForwardStats._acquireFreeListCount,
		env->_copyForwardStats._releaseFreeListCount,
		env->_copyForwardStats._acquireScanListCount,
		env->_copyForwardStats._releaseScanListCount,
		env->_copyForwardStats._copiedArraysSplit);
			
	if (env->_copyForwardStats._aborted) {
		Trc_MM_CopyForwardScheme_parallelStatsForAbort(
			env->getLanguageVMThread(),
			(U_32)env->getWorkerID(),
			(U_32)j9time_hires_delta(0, env->_workPacketStats._workStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
			(U_32)j9time_hires_delta(0, env->_workPacketStats._completeStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
			(U_32)j9time_hires_delta(0, env->_copyForwardStats._markStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
			(U_32)j9time_hires_delta(0, env->_copyForwardStats._abortStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
			(U_32)env->_workPacketStats._workStallCount,
			(U_32)env->_workPacketStats._completeStallCount,
			(U_32)env->_copyForwardStats._markStallCount,
			(U_32)env->_copyForwardStats._abortStallCount,
			env->_workPacketStats.workPacketsAcquired,
			env->_workPacketStats.workPacketsReleased,
			env->_workPacketStats.workPacketsExchanged,
			env->_copyForwardStats._markedArraysSplit);
	}
}

void
MM_CopyForwardScheme::copyForwardPreProcess(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	/* stats management */
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._startTime = j9time_hires_clock();
	/* Clear the gc statistics */
	clearGCStats(env);
	
	/* Perform any pre copy forwarding changes to the region set */
	preProcessRegions(env);

	if (0 != _regionCountCannotBeEvacuated) {
		/* need to run Hybrid mode, reuse InputListMonitor for both workPackets and ScanCopyCache */
		_workQueueMonitorPtr = env->_cycleState->_workPackets->getInputListMonitorPtr();
		_workQueueWaitCountPtr = env->_cycleState->_workPackets->getInputListWaitCountPtr();
	}
	/* Perform any main-specific setup */
	mainSetupForCopyForward(env);
}

void
MM_CopyForwardScheme::copyForwardPostProcess(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	mainCleanupForCopyForward(env);
	
	/* Record the completion time of the copy forward cycle */
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._copyForwardStats._endTime = j9time_hires_clock();

	updateLeafRegions(env);

	/* We used memory from the ACs for survivor space - make sure it doesn't hang around as allocation space */
	clearReservedRegionLists(env);
	_extensions->globalAllocationManager->flushAllocationContexts(env);

	copyForwardCompletedSuccessfully(env);

	if (_extensions->tarokEnableExpensiveAssertions) {
		/* Verify the result of the copy forward operation (heap integrity, etc) */
		verifyCopyForwardResult(MM_EnvironmentVLHGC::getEnvironment(env));
	}

	if (0 != _regionCountCannotBeEvacuated) {
		_workQueueMonitorPtr = &_scanCacheMonitor;
		_workQueueWaitCountPtr = &_scanCacheWaitCount;
	}

	/* Do any final work to regions in order to release them back to the main collector implementation */
	postProcessRegions(env);

	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_abortFlagRaisedDuringPGC = copyForwardCompletedSuccessfully(env);
}

#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
void
MM_CopyForwardScheme::concurrentCopyForwardCollectionSet(MM_EnvironmentVLHGC *env)
{
	/* isConcurrentCycleInProgress() tells us if this is the first PGC increment or not. If it is
	 * we'll call copyForwardPreProcess(). isConcurrentCycleInProgress state/value will get updated
	 * preventing copyForwardPreProcess from being called in subsequent increments. For initial increment,
	 * isConcurrentCycleInProgress will change from false to true causing only preProcess step to
	 * be performed */
	if (!isConcurrentCycleInProgress())
	{
		copyForwardPreProcess(env);
	}

	/* Perform the copy forward. This step will update the isConcurrentCycleInProgress state/value.
	 * Note: The following is temporary as this will be updated to call concurrent copy forward state machine */
	MM_CopyForwardSchemeTask copyForwardTask(env, _dispatcher, this, env->_cycleState);
	_dispatcher->run(env, &copyForwardTask);

	/* isConcurrentCycleInProgress() tells us if this is the last PGC increment or not. If this is the
	 * last increment, copyForwardPreProcess state/value would have been updated from from true to false,
	 * which will cause the following copyForwardPostProcess step to be performed */
	if (!isConcurrentCycleInProgress())
	{
		copyForwardPostProcess(env);
	}
}
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */

void
MM_CopyForwardScheme::copyForwardCollectionSet(MM_EnvironmentVLHGC *env)
{
	copyForwardPreProcess(env);

	/* And perform the copy forward */
	MM_CopyForwardSchemeTask copyForwardTask(env, _dispatcher, this, env->_cycleState);
	_dispatcher->run(env, &copyForwardTask);

	copyForwardPostProcess(env);
}

/**
 * Determine whether a copy forward that has been started did complete successfully.
 * @return true if the copyForward completed successfully, false otherwise.
 */
bool
MM_CopyForwardScheme::copyForwardCompletedSuccessfully(MM_EnvironmentVLHGC *env)
{
	return !abortFlagRaised();
}

/****************************************
 * Copy-Scan Cache management
 ****************************************
 * TODO: move all the CopyScanCache methods into the CopyScanCache class.
 */

/* getFreeCache makes the assumption that there will be at least 1 entry on the scan list if there are no entries on the free list.
 * This requires that there be at (N * _cachesPerThread) scan cache entries, where N is the number of threads (Main + workers)
 */
MM_CopyScanCacheVLHGC *
MM_CopyForwardScheme::getFreeCache(MM_EnvironmentVLHGC *env)
{
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	env->_copyForwardStats._acquireFreeListCount += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	/* Check the free list */
	MM_CopyScanCacheVLHGC *cache = _cacheFreeList.popCache(env);
	if (NULL != cache) {
		return cache;
	}

	/* No thread can use more than _cachesPerThread cache entries at 1 time (flip, tenure, scan, large, possibly deferred)
	 * So long as (N * _cachesPerThread) cache entries exist,
	 * the head of the scan list will contain a valid entry */
	env->_copyForwardStats._scanCacheOverflow = true;

	if (NULL == cache) {
		/* we couldn't get a free cache so we must be in an overflow scenario.  Try creating new cache structures on the heap */
		cache = createScanCacheForOverflowInHeap(env);
		if (NULL == cache) {
			/* we couldn't overflow so we have no choice but to abort the copy-forward */
			raiseAbortFlag(env);
		}
	}
	/* Overflow or abort was hit so alert other threads that are waiting */
	omrthread_monitor_enter(*_workQueueMonitorPtr);
	if (0 != *_workQueueWaitCountPtr) {
		omrthread_monitor_notify(*_workQueueMonitorPtr);
	}
	omrthread_monitor_exit(*_workQueueMonitorPtr);
	return cache;
}

void
MM_CopyForwardScheme::addCacheEntryToFreeCacheList(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *newCacheEntry)
{
	_cacheFreeList.pushCache(env, newCacheEntry);
}

void
MM_CopyForwardScheme::addCacheEntryToScanCacheListAndNotify(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *newCacheEntry)
{
	UDATA numaNode = _regionManager->tableDescriptorForAddress(newCacheEntry->scanCurrent)->getNumaNode();
	_cacheScanLists[numaNode].pushCache(env, newCacheEntry);
	if (0 != *_workQueueWaitCountPtr) {
		/* Added an entry to the scan list - notify any other threads that a new entry has appeared on the list */
		omrthread_monitor_enter(*_workQueueMonitorPtr);
		omrthread_monitor_notify(*_workQueueMonitorPtr);
		omrthread_monitor_exit(*_workQueueMonitorPtr);
	}
}

void
MM_CopyForwardScheme::flushCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache)
{
	Assert_MM_false(cache->isSplitArray());
	if (0 == (cache->flags & OMR_COPYSCAN_CACHE_TYPE_COPY)) {
		if (0 == (cache->flags & OMR_COPYSCAN_CACHE_TYPE_CLEARED)) {
			clearCache(env, cache); 
		}	
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		env->_copyForwardStats._releaseFreeListCount += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
		addCacheEntryToFreeCacheList(env, cache);
	}
}

bool
MM_CopyForwardScheme::clearCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache)
{
	uintptr_t discardSize = (uintptr_t)cache->cacheTop - (uintptr_t)cache->cacheAlloc;
	Assert_MM_true(0 == (cache->flags & OMR_COPYSCAN_CACHE_TYPE_CLEARED));
	Assert_MM_false(cache->isSplitArray());
	bool remainderCreated = false;
	
	UDATA compactGroup = cache->_compactGroup;
	Assert_MM_true(compactGroup < _compactGroupMaxCount);
	MM_CopyForwardCompactGroup *compactGroupForMarkData = &(env->_copyForwardCompactGroups[compactGroup]);

	if (0 < discardSize) {
		if ((discardSize < env->getExtensions()->tlhSurvivorDiscardThreshold) ||
			(discardSize <= ((uintptr_t)compactGroupForMarkData->_TLHRemainderTop - (uintptr_t)compactGroupForMarkData->_TLHRemainderBase))) {
			/* Abandon the current entry in the cache */
			compactGroupForMarkData->discardTLHRemainder(env, cache->cacheAlloc, cache->cacheTop);
		} else {
			/* Abandon the current TLHRemainder if one exists */
			compactGroupForMarkData->discardTLHRemainder(env);
			remainderCreated = true;
			compactGroupForMarkData->setTLHRemainder(cache->cacheAlloc, cache->cacheTop);
		}
	}

	/* Broadcast details of that portion of memory within which objects have been allocated */
	TRIGGER_J9HOOK_MM_PRIVATE_CACHE_CLEARED(_extensions->privateHookInterface, env->getOmrVMThread(), env->_cycleState->_activeSubSpace,
									cache->cacheBase, cache->cacheAlloc, cache->cacheTop);

	cache->flags |= OMR_COPYSCAN_CACHE_TYPE_CLEARED;

	return remainderCreated;
}

MM_CopyScanCacheVLHGC *
MM_CopyForwardScheme::stopCopyingIntoCache(MM_EnvironmentVLHGC *env, UDATA compactGroup)
{
	MM_CopyScanCacheVLHGC *copyCache = env->_copyForwardCompactGroups[compactGroup]._copyCache;
	MM_LightweightNonReentrantLock *copyCacheLock = env->_copyForwardCompactGroups[compactGroup]._copyCacheLock;

	if (NULL != copyCache) {
		Assert_MM_false(copyCache->isSplitArray());
		UDATA wastedMemory = env->_copyForwardCompactGroups[compactGroup]._freeMemoryMeasured;
		env->_copyForwardCompactGroups[compactGroup]._freeMemoryMeasured = 0;
		
		MM_HeapRegionDescriptorVLHGC * region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(copyCache->cacheBase);

		/* atomically add (age * usedBytes) product from this cache to the regions product */
		double newAllocationAgeSizeProduct = region->atomicIncrementAllocationAgeSizeProduct(copyCache->_allocationAgeSizeProduct);
		region->updateAgeBounds(copyCache->_lowerAgeBound, copyCache->_upperAgeBound);

		/* Return any remaining memory to the pool */
		discardRemainingCache(env, copyCache, copyCacheLock, wastedMemory);

		Trc_MM_CopyForwardScheme_stopCopyingIntoCache(env->getLanguageVMThread(), _regionManager->mapDescriptorToRegionTableIndex(region), copyCache,
				(double)(newAllocationAgeSizeProduct - copyCache->_allocationAgeSizeProduct) / (1024 * 1024) / (1024 * 1024), (double)((UDATA)copyCache->cacheAlloc - (UDATA)region->getLowAddress()) / (1024 * 1024),
				(double)copyCache->_allocationAgeSizeProduct / (1024 * 1024) / (1024 * 1024), (double)copyCache->_objectSize / (1024 * 1024), (double)newAllocationAgeSizeProduct / (1024 * 1024) / (1024 * 1024));

		copyCache->_allocationAgeSizeProduct = 0.0;
		copyCache->_objectSize = 0;
		copyCache->_lowerAgeBound = U_64_MAX;
		copyCache->_upperAgeBound = 0;

		/* Push any cached mark map data out */
		flushCacheMarkMap(env, copyCache);
		/* Update a region's projected live bytes from copy cache*/
		updateProjectedLiveBytesFromCopyScanCache(env, copyCache);
		/* Clear the current entry in the cache */
		clearCache(env, copyCache);
		/* This is no longer a copy cache */
		copyCache->flags &= ~OMR_COPYSCAN_CACHE_TYPE_COPY;
		/* drop this cache from the env */
		env->_copyForwardCompactGroups[compactGroup]._copyCache = NULL;
		env->_copyForwardCompactGroups[compactGroup]._copyCacheLock = NULL;
	}
	return copyCache;
}

void
MM_CopyForwardScheme::updateProjectedLiveBytesFromCopyScanCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache)
{
	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(cache->cacheBase);
	Assert_MM_true(region->isSurvivorRegion());
	UDATA consumedBytes = (UDATA) cache->cacheAlloc - (UDATA) cache->cacheBase;
	MM_AtomicOperations::add(&region->_projectedLiveBytes, consumedBytes);
}

void
MM_CopyForwardScheme::discardRemainingCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache, MM_LightweightNonReentrantLock *cacheLock, UDATA wastedMemory)
{
	Assert_MM_false(cache->isSplitArray());
	if (0 != wastedMemory) {
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(cache->cacheBase);
		MM_MemoryPool *pool = region->getMemoryPool();
		pool->incrementDarkMatterBytes(wastedMemory);
	}
}

void
MM_CopyForwardScheme::addCopyCachesToFreeList(MM_EnvironmentVLHGC *env)
{
	for (UDATA index = 0; index < _compactGroupMaxCount; index++) {
		MM_CopyScanCacheVLHGC * copyCache = stopCopyingIntoCache(env, index);
		if (NULL != copyCache) {
			addCacheEntryToFreeCacheList(env, copyCache);
		}
	}
}

J9Object *
MM_CopyForwardScheme::updateForwardedPointer(J9Object *objectPtr)
{
	J9Object *forwardPtr;

	if (isObjectInEvacuateMemory(objectPtr)) {
		MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
		forwardPtr = forwardedHeader.getForwardedObject();
		if (forwardPtr != NULL) {
			return forwardPtr;
		}
	}

	return objectPtr;
}

MMINLINE MM_AllocationContextTarok *
MM_CopyForwardScheme::getContextForHeapAddress(void *address)
{
	return ((MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(address))->_allocateData._owningContext;
}

J9Object *
MM_CopyForwardScheme::copy(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_ForwardedHeader* forwardedHeader, bool leafType)
{
	bool const compressed = env->compressObjectReferences();
	J9Object *result = NULL;
	J9Object *object = forwardedHeader->getObject();
	uintptr_t objectCopySizeInBytes = 0;
	uintptr_t objectReserveSizeInBytes = 0;

	bool noEvacuation = false;
	if (0 != _regionCountCannotBeEvacuated) {
		noEvacuation = isObjectInNoEvacuationRegions(env, object);
	}

	if (_abortInProgress || noEvacuation) {
		/* Once threads agreed that abort is in progress or the object is in noEvacuation region, only mark/push should be happening, no attempts even to allocate/copy */

		if (_markMap->atomicSetBit(object)) {
			Assert_MM_false(MM_ForwardedHeader(object, compressed).isForwardedPointer());
			/* don't need to push leaf object in work stack */
			if (!leafType) {
				env->_workStack.push(env, object);
			}
		}

		result = object;
	} else {
		uintptr_t hotFieldsDescriptor = 0;
		uintptr_t hotFieldsAlignment = 0;
		uintptr_t *hotFieldPadBase = NULL;
		uintptr_t hotFieldPadSize = 0;
		MM_CopyScanCacheVLHGC *copyCache = NULL;
		void *newCacheAlloc = NULL;
		GC_ObjectModel *objectModel = &_extensions->objectModel;
		
		/* Object is in the evacuate space but not forwarded. */
		objectModel->calculateObjectDetailsForCopy(env, forwardedHeader, &objectCopySizeInBytes, &objectReserveSizeInBytes, &hotFieldsDescriptor);

		Assert_MM_objectAligned(env, objectReserveSizeInBytes);

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		/* adjust the reserved object's size if we are aligning hot fields and this class has a known hot field */
		if (_extensions->scavengerAlignHotFields && HOTFIELD_SHOULD_ALIGN(hotFieldsDescriptor)) {
			/* set the descriptor field if we should be aligning (since assuming that 0 means no is not safe) */
			hotFieldsAlignment = hotFieldsDescriptor;
			/* for simplicity, add the maximum padding we could need (and back off after allocation) */
			objectReserveSizeInBytes += (_cacheLineAlignment - _objectAlignmentInBytes);
			Assert_MM_objectAligned(env, objectReserveSizeInBytes);
		}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

		reservingContext = getPreferredAllocationContext(reservingContext, object);

		copyCache = reserveMemoryForCopy(env, object, reservingContext, objectReserveSizeInBytes);

		/* Check if memory was reserved successfully */
		if (NULL == copyCache) {
			raiseAbortFlag(env);
			Assert_MM_true(NULL == result);
		} else {
			Assert_MM_false(copyCache->isSplitArray());

			/* Memory has been reserved */
			uintptr_t destinationCompactGroup = copyCache->_compactGroup;
			J9Object *destinationObjectPtr = (J9Object *)copyCache->cacheAlloc;
			Assert_MM_true(NULL != destinationObjectPtr);

			/* now correct for the hot field alignment */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
			if (0 != hotFieldsAlignment) {
				uintptr_t remainingInCacheLine = _cacheLineAlignment - ((uintptr_t)destinationObjectPtr % _cacheLineAlignment);
				uintptr_t alignmentBias = HOTFIELD_ALIGNMENT_BIAS(hotFieldsAlignment, _objectAlignmentInBytes);
				/* do alignment only if the object cannot fit in the remaining space in the cache line */
				if ((remainingInCacheLine < objectCopySizeInBytes) && (alignmentBias < remainingInCacheLine)) {
					hotFieldPadSize = ((remainingInCacheLine + _cacheLineAlignment) - (alignmentBias % _cacheLineAlignment)) % _cacheLineAlignment;
					hotFieldPadBase = (uintptr_t *)destinationObjectPtr;
					/* now fix the object pointer so that the hot field is aligned */
					destinationObjectPtr = (J9Object *)((uintptr_t)destinationObjectPtr + hotFieldPadSize);
				}
				/* and update the reserved size so that we "un-reserve" the extra memory we said we might need.  This is done by
				 * removing the excess reserve since we already accounted for the hotFieldPadSize by bumping the destination pointer
				 * and now we need to revert to the amount needed for the object allocation and its array alignment so the rest of
				 * the method continues to function without needing to know about this extra alignment calculation
				 */
				objectReserveSizeInBytes = objectReserveSizeInBytes - (_cacheLineAlignment - _objectAlignmentInBytes);
			}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

			/* and correct for the double array alignment */
			newCacheAlloc = (void *)((uintptr_t)destinationObjectPtr + objectReserveSizeInBytes);

			/* Try to swap the forwarding pointer to the destination copy array into the source object */
			J9Object *originalDestinationObjectPtr = destinationObjectPtr;
			destinationObjectPtr = forwardedHeader->setForwardedObject(destinationObjectPtr);
			Assert_MM_true(NULL != destinationObjectPtr);
			if (destinationObjectPtr == originalDestinationObjectPtr) {
				/* Succeeded in forwarding the object - copy and adjust the age value */

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
				if (NULL != hotFieldPadBase) {
					/* lay down a hole (XXX:  This assumes that we are using AOL (address-ordered-list)) */
					MM_HeapLinkedFreeHeader::fillWithHoles(hotFieldPadBase, hotFieldPadSize, compressed);
				}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

				memcpy((void *)destinationObjectPtr, forwardedHeader->getObject(), objectCopySizeInBytes);

				forwardedHeader->fixupForwardedObject(destinationObjectPtr);

				if (objectModel->isIndexable(destinationObjectPtr)) {
					_extensions->indexableObjectModel.fixupInternalLeafPointersAfterCopy((J9IndexableObject *)destinationObjectPtr, (J9IndexableObject *)forwardedHeader->getObject());

					/* Updates internal data address of indexable objects. Every indexable object have a void *dataAddr
					 * that always points to the array data. It will always point to the address right after the header,
					 * in case of contiguous data it will point to the data itself, and in case of discontiguous
					 * arraylet it will point to the first arrayiod. dataAddr is only updated if dataAddr points to data
					 * within heap. */
					_extensions->indexableObjectModel.fixupDataAddr(destinationObjectPtr);
				}

				objectModel->fixupHashFlagsAndSlot(forwardedHeader, destinationObjectPtr);

				/* Update any mark maps and transfer card table data as appropriate for a successful copy */
				updateMarkMapAndCardTableOnCopy(env, forwardedHeader->getObject(), destinationObjectPtr, copyCache);

				/* Move the cache allocate pointer to reflect the consumed memory */
				Assert_MM_true(copyCache->cacheAlloc <= copyCache->cacheTop);

				if (_tracingEnabled) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Cache alloc: %p newAlloc: %p origO: %p copyO: %p\n", copyCache->cacheAlloc, newCacheAlloc, forwardedHeader->getObject(), destinationObjectPtr);
				}

				copyCache->cacheAlloc = newCacheAlloc;
				Assert_MM_true(copyCache->cacheAlloc <= copyCache->cacheTop);

				/* Update the stats */
				if (hotFieldPadSize > 0) {
					/* account for this as free memory */
					env->_copyForwardCompactGroups[destinationCompactGroup]._freeMemoryMeasured += hotFieldPadSize;
				}
				MM_HeapRegionDescriptorVLHGC * sourceRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(object);
				uintptr_t sourceCompactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, sourceRegion);
				if (sourceRegion->isEden()) {
					env->_copyForwardCompactGroups[sourceCompactGroup]._edenStats._liveObjects += 1;
					env->_copyForwardCompactGroups[sourceCompactGroup]._edenStats._liveBytes += objectCopySizeInBytes;
					env->_copyForwardCompactGroups[destinationCompactGroup]._edenStats._copiedObjects += 1;
					env->_copyForwardCompactGroups[destinationCompactGroup]._edenStats._copiedBytes += objectCopySizeInBytes;
				} else {
					env->_copyForwardCompactGroups[sourceCompactGroup]._nonEdenStats._liveObjects += 1;
					env->_copyForwardCompactGroups[sourceCompactGroup]._nonEdenStats._liveBytes += objectCopySizeInBytes;
					env->_copyForwardCompactGroups[destinationCompactGroup]._nonEdenStats._copiedObjects += 1;
					env->_copyForwardCompactGroups[destinationCompactGroup]._nonEdenStats._copiedBytes += objectCopySizeInBytes;
				}
				copyCache->_allocationAgeSizeProduct += ((double)objectReserveSizeInBytes * (double)sourceRegion->getAllocationAge());
				copyCache->_objectSize += objectReserveSizeInBytes;
				copyCache->_lowerAgeBound = OMR_MIN(copyCache->_lowerAgeBound, sourceRegion->getLowerAgeBound());
				copyCache->_upperAgeBound = OMR_MAX(copyCache->_upperAgeBound, sourceRegion->getUpperAgeBound());

#if defined(J9VM_GC_LEAF_BITS)
				if (_extensions->tarokEnableLeafFirstCopying) {
					copyLeafChildren(env, reservingContext, destinationObjectPtr);
				}
#endif /* J9VM_GC_LEAF_BITS */
				/* depth copy the hot fields of an object if scavenger dynamicBreadthFirstScanOrdering is enabled */
				depthCopyHotFields(env, objectModel->getPreservedClass(forwardedHeader), destinationObjectPtr, reservingContext);
			}
			/* return value for updating the slot */
			result = destinationObjectPtr;
		}
	}

	return result;
}

#if defined(J9VM_GC_LEAF_BITS)
void
MM_CopyForwardScheme::copyLeafChildren(MM_EnvironmentVLHGC* env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr)
{
	J9Class *clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, env);
	if (GC_ObjectModel::SCAN_MIXED_OBJECT == _extensions->objectModel.getScanType(clazz)) {
		UDATA instanceLeafDescription = (UDATA)J9GC_J9OBJECT_CLAZZ(objectPtr, env)->instanceLeafDescription;
		/* For now we only support leaf children in small objects. If the leaf description isn't immediate, ignore it to keep the code simple. */
		if (1 == (instanceLeafDescription & 1)) {
			bool const compressed = env->compressObjectReferences();
			fj9object_t* scanPtr = _extensions->mixedObjectModel.getHeadlessObject(objectPtr);
			UDATA leafBits = instanceLeafDescription >> 1;
			while (0 != leafBits) {
				if (1 == (leafBits & 1)) {
					/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
					GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
					/* pass leaf flag into copy method for optimizing abort case and hybrid case (don't need to push leaf object in work stack) */
					copyAndForward(env, reservingContext, objectPtr, &slotObject, true);
				}
				leafBits >>= 1;
				scanPtr = GC_SlotObject::addToSlotAddress(scanPtr, 1, compressed);
			}
		}
	}
}
#endif /* J9VM_GC_LEAF_BITS */

MMINLINE void
MM_CopyForwardScheme::depthCopyHotFields(MM_EnvironmentVLHGC *env, J9Class *clazz, J9Object *destinationObjectPtr, MM_AllocationContextTarok *reservingContext) {
	/* depth copy the hot fields of an object up to a depth specified by depthCopyMax */
	J9ClassHotFieldsInfo* hotFieldsInfo = clazz->hotFieldsInfo;
	if (env->_hotFieldCopyDepthCount < _extensions->depthCopyMax && NULL != hotFieldsInfo) {
		U_8 hotFieldOffset = hotFieldsInfo->hotFieldOffset1;
		if (U_8_MAX != hotFieldOffset) {
			copyHotField(env, destinationObjectPtr, hotFieldOffset, reservingContext);
			U_8 hotFieldOffset2 = hotFieldsInfo->hotFieldOffset2;
			if (U_8_MAX !=hotFieldOffset2) {
				copyHotField(env, destinationObjectPtr, hotFieldOffset2, reservingContext);
				U_8 hotFieldOffset3 = hotFieldsInfo->hotFieldOffset3;
				if (U_8_MAX != hotFieldOffset3) {
					copyHotField(env, destinationObjectPtr, hotFieldOffset3, reservingContext);
				}
			}
		} else if ((_extensions->alwaysDepthCopyFirstOffset) && (false == _extensions->objectModel.isIndexable(destinationObjectPtr))) {
			copyHotField(env, destinationObjectPtr, DEFAULT_HOT_FIELD_OFFSET, reservingContext);
		}
	}	
}

MMINLINE void
MM_CopyForwardScheme::copyHotField(MM_EnvironmentVLHGC *env, J9Object *destinationObjectPtr, U_8 offset, MM_AllocationContextTarok *reservingContext) {
	bool const compressed = _extensions->compressObjectReferences();
	GC_SlotObject hotFieldObject(_javaVM->omrVM, GC_SlotObject::addToSlotAddress((fomrobject_t*)((uintptr_t)destinationObjectPtr), offset, compressed));
	omrobjectptr_t objectPtr = hotFieldObject.readReferenceFromSlot();							
	if (isObjectInEvacuateMemory(objectPtr)) {
		/* Hot field needs to be copy and forwarded.  Check if the work has already been done */
		MM_ForwardedHeader forwardHeaderHotField(objectPtr, compressed);
		if (!forwardHeaderHotField.isForwardedPointer()) {
			env->_hotFieldCopyDepthCount += 1;
			copy(env, reservingContext, &forwardHeaderHotField);
			env->_hotFieldCopyDepthCount -= 1;
		}
	}
}

void
MM_CopyForwardScheme::flushCacheMarkMap(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache)
{
	MM_CopyForwardCompactGroup *compactGroup = &(env->_copyForwardCompactGroups[cache->_compactGroup]);
	Assert_MM_true(cache == compactGroup->_copyCache);
	Assert_MM_false(UDATA_MAX == compactGroup->_markMapPGCSlotIndex);  /* Safety check from flushing to see if somehow the cache is being resurrected */
	Assert_MM_false(UDATA_MAX == compactGroup->_markMapGMPSlotIndex);  /* Safety check from flushing to see if somehow the cache is being resurrected */
	Assert_MM_false(cache->isSplitArray());

	if (0 != compactGroup->_markMapPGCBitMask) {
		UDATA pgcFlushSlotIndex = compactGroup->_markMapPGCSlotIndex;
		if ((pgcFlushSlotIndex == compactGroup->_markMapAtomicHeadSlotIndex) || (pgcFlushSlotIndex == compactGroup->_markMapAtomicTailSlotIndex)) {
			_markMap->atomicSetSlot(pgcFlushSlotIndex, compactGroup->_markMapPGCBitMask);
		} else {
			_markMap->setSlot(pgcFlushSlotIndex, compactGroup->_markMapPGCBitMask);
		}

		/* We set the slot index to an invalid value to assert on later if seen */
		compactGroup->_markMapPGCSlotIndex = UDATA_MAX;
		compactGroup->_markMapPGCBitMask = 0;
	}

	if (NULL != env->_cycleState->_externalCycleState) {
		if (0 != compactGroup->_markMapGMPBitMask) {
			UDATA gmpFlushSlotIndex = compactGroup->_markMapGMPSlotIndex;
			if ((gmpFlushSlotIndex == compactGroup->_markMapAtomicHeadSlotIndex) || (gmpFlushSlotIndex == compactGroup->_markMapAtomicTailSlotIndex)) {
				env->_cycleState->_externalCycleState->_markMap->atomicSetSlot(gmpFlushSlotIndex, compactGroup->_markMapGMPBitMask);
			} else {
				env->_cycleState->_externalCycleState->_markMap->setSlot(gmpFlushSlotIndex, compactGroup->_markMapGMPBitMask);
			}

			/* We set the slot index to an invalid value to assert on later if seen */
			compactGroup->_markMapGMPSlotIndex = UDATA_MAX;
			compactGroup->_markMapGMPBitMask = 0;
		}
	}

	compactGroup->_markMapAtomicHeadSlotIndex = 0;
	compactGroup->_markMapAtomicTailSlotIndex = 0;
}

void
MM_CopyForwardScheme::updateMarkMapCache(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap, J9Object *object,
		UDATA *slotIndexIndirect, UDATA *bitMaskIndirect, UDATA atomicHeadSlotIndex, UDATA atomicTailSlotIndex)
{
	UDATA slotIndex = 0;
	UDATA bitMask = 0;

	markMap->getSlotIndexAndMask(object, &slotIndex, &bitMask);

	if (*slotIndexIndirect != slotIndex) {
		if (0 != *bitMaskIndirect) {
			UDATA flushSlotIndex = *slotIndexIndirect;
			if ((flushSlotIndex == atomicHeadSlotIndex) || (flushSlotIndex == atomicTailSlotIndex)) {
				markMap->atomicSetSlot(flushSlotIndex, *bitMaskIndirect);
			} else {
				markMap->setSlot(flushSlotIndex, *bitMaskIndirect);
			}
		}
		*slotIndexIndirect = slotIndex;
		*bitMaskIndirect = bitMask;
	} else {
		*bitMaskIndirect |= bitMask;
	}
}

void
MM_CopyForwardScheme::updateMarkMapAndCardTableOnCopy(MM_EnvironmentVLHGC *env, J9Object *srcObject, J9Object *dstObject, MM_CopyScanCacheVLHGC *dstCache)
{
	MM_CopyForwardCompactGroup *destinationGroup = &(env->_copyForwardCompactGroups[dstCache->_compactGroup]);
	Assert_MM_true(dstCache == destinationGroup->_copyCache);
	Assert_MM_false(UDATA_MAX == destinationGroup->_markMapPGCSlotIndex);  /* Safety check from flushing to see if somehow the cache is being resurrected */
	Assert_MM_false(UDATA_MAX == destinationGroup->_markMapGMPSlotIndex);  /* Safety check from flushing to see if somehow the cache is being resurrected */
	Assert_MM_false(dstCache->isSplitArray());
	
	updateMarkMapCache(env, _markMap, dstObject, &destinationGroup->_markMapPGCSlotIndex, &destinationGroup->_markMapPGCBitMask, destinationGroup->_markMapAtomicHeadSlotIndex, destinationGroup->_markMapAtomicTailSlotIndex);

	/* If there is an external cycle in progress, see if any information needs to be migrated */
	if (NULL != env->_cycleState->_externalCycleState) {
		MM_MarkMap *externalMap = env->_cycleState->_externalCycleState->_markMap;

		if (externalMap->isBitSet(srcObject)) {
			/* The external cycle has already visited the live object - move the mark map and card information across */
			updateMarkMapCache(env, externalMap, dstObject, &destinationGroup->_markMapGMPSlotIndex, &destinationGroup->_markMapGMPBitMask, destinationGroup->_markMapAtomicHeadSlotIndex, destinationGroup->_markMapAtomicTailSlotIndex);

			MM_CardTable *cardTable = _extensions->cardTable;
			Card *card = cardTable->heapAddrToCardAddr(env, srcObject);
			Card srcCardState = *card;

			switch (srcCardState) {
			case CARD_GMP_MUST_SCAN:
			case CARD_DIRTY:
			{
				Card *dstCard = cardTable->heapAddrToCardAddr(env, dstObject);
				if(CARD_GMP_MUST_SCAN != *dstCard) {
					*dstCard = CARD_GMP_MUST_SCAN;
				}
				break;
			}
			case CARD_PGC_MUST_SCAN:
			case CARD_CLEAN:
				/* do nothing */
				break;

			/* Unexpected states */
			case CARD_REMEMBERED:
				Assert_GC_true_with_message4(env, false, "Unexpected Card state CARD_REMEMBERED %u for Card %p, srcObject %p, dstObject %p\n", CARD_REMEMBERED, card, srcObject, dstObject);
				break;
			case CARD_REMEMBERED_AND_GMP_SCAN:
				Assert_GC_true_with_message4(env, false, "Unexpected Card state CARD_REMEMBERED_AND_GMP_SCAN %u for Card %p, srcObject %p, dstObject %p\n", CARD_REMEMBERED_AND_GMP_SCAN, card, srcObject, dstObject);
				break;
			case CARD_MARK_COMPACT_TRANSITION:
				Assert_GC_true_with_message4(env, false, "Unexpected Card state CARD_MARK_COMPACT_TRANSITION %u for Card %p, srcObject %p, dstObject %p\n", CARD_MARK_COMPACT_TRANSITION, card, srcObject, dstObject);
				break;
			case CARD_INVALID:
				Assert_GC_true_with_message4(env, false, "Unexpected Card state CARD_INVALID %u for Card %p, srcObject %p, dstObject %p\n", CARD_INVALID, card, srcObject, dstObject);
				break;
			default:
				Assert_GC_true_with_message4(env, false, "Unexpected Card state UNKNOWN %u for Card %p, srcObject %p, dstObject %p\n", srcCardState, card, srcObject, dstObject);
			}
		}
	}
}

/****************************************
 * Object scan and copy routines
 ****************************************
 */
MMINLINE void
MM_CopyForwardScheme::scanOwnableSynchronizerObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason)
{
	if (SCAN_REASON_COPYSCANCACHE == reason) {
		addOwnableSynchronizerObjectInList(env, objectPtr);
	} else if (SCAN_REASON_PACKET == reason) {
		if (isObjectInEvacuateMemoryNoCheck(objectPtr)) {
			addOwnableSynchronizerObjectInList(env, objectPtr);
		}
	}
	scanMixedObjectSlots(env, reservingContext, objectPtr, reason);
}

void
MM_CopyForwardScheme::doStackSlot(MM_EnvironmentVLHGC *env, J9Object *fromObject, J9Object **slotPtr, J9StackWalkState *walkState, const void *stackLocation)
{
	if (isHeapObject(*slotPtr)) {
		/* heap object - validate and copyforward */
		Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::COULD_BE_FORWARDED, *slotPtr, stackLocation, walkState).validate(env));
		J9VMThread *thread = ((J9StackWalkState *)walkState)->currentThread;
		MM_AllocationContextTarok *reservingContext = (MM_AllocationContextTarok *)MM_EnvironmentVLHGC::getEnvironment(thread)->getAllocationContext();
		copyAndForward(MM_EnvironmentVLHGC::getEnvironment(env), reservingContext, fromObject, slotPtr);
	} else if (NULL != *slotPtr) {
		/* stack object - just validate */
		Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, *slotPtr, stackLocation, walkState).validate(env));
	}
}

/**
 * @todo Provide function documentation
 */
void
stackSlotIteratorForCopyForwardScheme(J9JavaVM *javaVM, J9Object **slotPtr, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	StackIteratorData4CopyForward *data = (StackIteratorData4CopyForward *)localData;
	MM_CopyForwardScheme *copyForwardScheme = data->copyForwardScheme;

	copyForwardScheme->doStackSlot(data->env, data->fromObject, slotPtr, walkState, stackLocation);
}

MMINLINE void
MM_CopyForwardScheme::scanContinuationNativeSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason)
{
	J9VMThread *currentThread = (J9VMThread *)env->getLanguageVMThread();
	const bool isConcurrentGC = false;
	const bool isGlobalGC = false;
	const bool beingMounted = false;
	if (MM_GCExtensions::needScanStacksForContinuationObject(currentThread, objectPtr, isConcurrentGC, isGlobalGC, beingMounted)) {
		StackIteratorData4CopyForward localData;
		localData.copyForwardScheme = this;
		localData.env = env;
		localData.fromObject = objectPtr;
		/* check _includeStackFrameClassReferences, _trackVisibleStackFrameDepth  */
		bool stackFrameClassWalkNeeded = false;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		stackFrameClassWalkNeeded = isDynamicClassUnloadingEnabled();
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		GC_VMThreadStackSlotIterator::scanContinuationSlots(currentThread, objectPtr, (void *)&localData, stackSlotIteratorForCopyForwardScheme, stackFrameClassWalkNeeded, false);
	}
}

MMINLINE void
MM_CopyForwardScheme::scanContinuationObject(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason)
{
	scanContinuationNativeSlots(env, reservingContext, objectPtr, reason);
	scanMixedObjectSlots(env, reservingContext, objectPtr, reason);
}

/**
 *  Iterate the slot reference and parse and pass leaf bit of the reference to copy forward
 *  to avoid to push leaf object to work stack in case the reference need to be marked instead of copied.
 */
MMINLINE bool
MM_CopyForwardScheme::iterateAndCopyforwardSlotReference(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr) {
	bool success = true;
	fj9object_t *endScanPtr;
	UDATA *descriptionPtr;
	UDATA descriptionBits;
	UDATA descriptionIndex;
#if defined(J9VM_GC_LEAF_BITS)
	UDATA *leafPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr, env)->instanceLeafDescription;
	UDATA leafBits;
#endif /* J9VM_GC_LEAF_BITS */
	bool const compressed = env->compressObjectReferences();

	/* Object slots */
	volatile fj9object_t* scanPtr = _extensions->mixedObjectModel.getHeadlessObject(objectPtr);
	UDATA objectSize = _extensions->mixedObjectModel.getSizeInBytesWithHeader(objectPtr);

	endScanPtr = (fj9object_t*)(((U_8 *)objectPtr) + objectSize);
	descriptionPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr, env)->instanceDescription;

	if (((UDATA)descriptionPtr) & 1) {
		descriptionBits = ((UDATA)descriptionPtr) >> 1;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits = ((UDATA)leafPtr) >> 1;
#endif /* J9VM_GC_LEAF_BITS */
	} else {
		descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
	}
	descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;

	while (success && (scanPtr < endScanPtr)) {
		/* Determine if the slot should be processed */
		if (descriptionBits & 1) {
			GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);

		/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
#if defined(J9VM_GC_LEAF_BITS)
			success = copyAndForward(env, reservingContext, objectPtr, &slotObject, 1 == (leafBits & 1));
#else /* J9VM_GC_LEAF_BITS */
			success = copyAndForward(env, reservingContext, objectPtr, &slotObject);
#endif /* J9VM_GC_LEAF_BITS */
		}
		descriptionBits >>= 1;
#if defined(J9VM_GC_LEAF_BITS)
		leafBits >>= 1;
#endif /* J9VM_GC_LEAF_BITS */
		if (descriptionIndex-- == 0) {
			descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
			leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
			descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
		}
		scanPtr = GC_SlotObject::addToSlotAddress((fomrobject_t*)scanPtr, 1, compressed);
	}
	return success;
}

void
MM_CopyForwardScheme::scanMixedObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason)
{
	if (_tracingEnabled) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		j9tty_printf(PORTLIB, "@%p\n", objectPtr);
	}

	bool success = copyAndForwardObjectClass(env, reservingContext, objectPtr);

	if (success) {
		/* Iteratoring and copyforwarding  the slot reference with leaf bit */
		success = iterateAndCopyforwardSlotReference(env, reservingContext, objectPtr);
	}

	updateScanStats(env, objectPtr, reason);
}

void
MM_CopyForwardScheme::scanReferenceObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason)
{
	bool success = copyAndForwardObjectClass(env, reservingContext, objectPtr);

	I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr);

	/* if the reference isn't part of the collection set, treat it as a strong reference */
	bool isReferenceInCollectionSet = isObjectInNurseryMemory(objectPtr);
	bool isReferenceCleared = (GC_ObjectModel::REF_STATE_CLEARED == referenceState) || (GC_ObjectModel::REF_STATE_ENQUEUED == referenceState);
	bool referentMustBeMarked = isReferenceCleared || !isReferenceInCollectionSet;
	bool referentMustBeCleared = false;
	if (isReferenceInCollectionSet) {
		UDATA referenceObjectOptions = env->_cycleState->_referenceObjectOptions;
		UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(objectPtr, env)) & J9AccClassReferenceMask;
		switch (referenceObjectType) {
		case J9AccClassReferenceWeak:
			referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_weak)) ;
			break;
		case J9AccClassReferenceSoft:
			referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_soft));
			referentMustBeMarked = referentMustBeMarked || (
				((0 == (referenceObjectOptions & MM_CycleState::references_soft_as_weak))
				&& ((UDATA)J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, objectPtr) < _extensions->getDynamicMaxSoftReferenceAge())));
			break;
		case J9AccClassReferencePhantom:
			referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_phantom));
			break;
		default:
			Assert_MM_unreachable();
		}
	}
	
	GC_SlotObject referentPtr(_javaVM->omrVM, J9GC_J9VMJAVALANGREFERENCE_REFERENT_ADDRESS(env, objectPtr));

	/* Iterating and copyforwarding regular reference slots, except the special (soft) referent slot. Not making use of leaf bit optimization,
	 * sacrificing minor performance to avoid code complication. Could optimize later, if/when using ObjectScanner */
	GC_MixedObjectIterator mixedObjectIterator(_javaVM->omrVM, objectPtr);
	GC_SlotObject *slotObject = NULL;
	while (success && (NULL != (slotObject = mixedObjectIterator.nextSlot()))) {
		if ((slotObject->readAddressFromSlot() != referentPtr.readAddressFromSlot()) || referentMustBeMarked) {
			/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
			success = copyAndForward(env, reservingContext, objectPtr, slotObject);
		}
	}

	if (SCAN_REASON_OVERFLOWED_REGION == reason) {
		/* handled when we empty packet to overflow */
	} else {
		if (referentMustBeCleared) {
			Assert_MM_true(isReferenceInCollectionSet);
			referentPtr.writeReferenceToSlot(NULL);
			if (!isReferenceCleared) {
				J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr) = GC_ObjectModel::REF_STATE_CLEARED;
			}
		} else if (isReferenceInCollectionSet) {
			if (!isReferenceCleared) {
				if (success) {
					env->getGCEnvironment()->_referenceObjectBuffer->add(env, objectPtr);
				}
			}
		}
	}

	updateScanStats(env, objectPtr, reason);
}

UDATA
MM_CopyForwardScheme::createNextSplitArrayWorkUnit(MM_EnvironmentVLHGC *env, J9IndexableObject *arrayPtr, UDATA startIndex, bool currentSplitUnitOnly)
{
	UDATA sizeInElements = _extensions->indexableObjectModel.getSizeInElements(arrayPtr);
	UDATA slotsToScan = 0;

	if (sizeInElements > 0) {
		Assert_MM_true(startIndex < sizeInElements);
		slotsToScan = sizeInElements - startIndex;

		if (slotsToScan > _arraySplitSize) {
			slotsToScan = _arraySplitSize;

			/* immediately make the next chunk available for another thread to start processing */
			UDATA nextIndex = startIndex + slotsToScan;
			Assert_MM_true(nextIndex < sizeInElements);

			bool noEvacuation = false;
			if (0 != _regionCountCannotBeEvacuated) {
				noEvacuation = isObjectInNoEvacuationRegions(env, (J9Object *) arrayPtr);
			}

			if (abortFlagRaised() || noEvacuation) {
				if (!currentSplitUnitOnly) {
					/* work stack driven */
					env->_workStack.push(env, (void *)arrayPtr, (void *)((nextIndex << PACKET_ARRAY_SPLIT_SHIFT) | PACKET_ARRAY_SPLIT_TAG));
					env->_workStack.flushOutputPacket(env);
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
					env->_copyForwardStats._markedArraysSplit += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
				}
			} else {
				Assert_MM_false(currentSplitUnitOnly);
				/* copy-scan cache driven */
				MM_CopyScanCacheVLHGC *splitCache = getFreeCache(env);
				if (NULL != splitCache) {
					reinitArraySplitCache(env, splitCache, arrayPtr, nextIndex);
					addCacheEntryToScanCacheListAndNotify(env, splitCache);
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
					env->_copyForwardStats._copiedArraysSplit += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
				} else {
					Assert_MM_true(_abortFlag);
					void *element1 = (void *)arrayPtr;
					void *element2 = (void *)((nextIndex << PACKET_ARRAY_SPLIT_SHIFT) | PACKET_ARRAY_SPLIT_TAG);
					Assert_MM_true(nextIndex == (((UDATA)element2) >> PACKET_ARRAY_SPLIT_SHIFT));
					env->_workStack.push(env, element1, element2);
					env->_workStack.flushOutputPacket(env);
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
					env->_copyForwardStats._markedArraysSplit += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
					Trc_MM_CopyForwardScheme_scanPointerArrayObjectSlotsSplit_failedToAllocateCache(env->getLanguageVMThread(), sizeInElements);
				}
			}
		}
	}

	return slotsToScan;
}
UDATA
MM_CopyForwardScheme::scanPointerArrayObjectSlotsSplit(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9IndexableObject *arrayPtr, UDATA startIndex, bool currentSplitUnitOnly)
{
	if (_tracingEnabled) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		j9tty_printf(PORTLIB, "#");
	}

	/* there's no harm in remembering the array multiple times, so do this for each split chunk */
	bool success = copyAndForwardObjectClass(env, reservingContext, (J9Object *)arrayPtr);

	UDATA slotsToScan = createNextSplitArrayWorkUnit(env, arrayPtr, startIndex, currentSplitUnitOnly);

	if (slotsToScan > 0) {
		/* TODO: this iterator scans the array backwards - change it to forward, and optimize it since we can guarantee the range will be contiguous */
		GC_PointerArrayIterator pointerArrayIterator(_javaVM, (J9Object *)arrayPtr);
		pointerArrayIterator.setIndex(startIndex + slotsToScan);

		for (UDATA scanCount = 0; success && (scanCount < slotsToScan); scanCount++) {
			GC_SlotObject *slotObject = pointerArrayIterator.nextSlot();
			if (NULL == slotObject) {
				/* this can happen if the array is only partially allocated */
				break;
			}

			/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
			success = copyAndForwardPointerArray(env, reservingContext, arrayPtr, startIndex, slotObject);
		}
	}

	return slotsToScan;
}

void
MM_CopyForwardScheme::scanClassObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *classObject, ScanReason reason)
{
	scanMixedObjectSlots(env, reservingContext, classObject, reason);

	J9Class *classPtr = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), classObject);

	if (NULL != classPtr) {
		volatile j9object_t * slotPtr = NULL;
		bool success = true;

		do {
			/*
			 * Scan J9Class internals using general iterator
			 * - scan statics fields
			 * - scan call sites
			 * - scan MethodTypes
			 * - scan VarHandle MethodTypes
			 * - scan constants pool objects
			 */
			GC_ClassIterator classIterator(env, classPtr, false);
			while (success && (NULL != (slotPtr = classIterator.nextSlot()))) {
				/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
				success = copyAndForward(env, reservingContext, classObject, slotPtr);
			}

			/*
			 * Usually we don't care about class to class references because its can be marked as a part of alive classloader or find in Hash Table
			 * However we need to scan them for case of Anonymous classes. Its are unloaded on individual basis so it is important to reach each one
			 */
			if (OMR_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(classPtr), J9ClassIsAnonymous)) {
				GC_ClassIteratorClassSlots classSlotIterator(_javaVM, classPtr);
				J9Class *classPtr;
				while (success && (NULL != (classPtr = classSlotIterator.nextSlot()))) {
					slotPtr = &(classPtr->classObject);
					/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
					success = copyAndForward(env, reservingContext, classObject, slotPtr);
				}
			}

			if (success) {
				/* we can safely ignore any classes referenced by the constant pool, since
				 * these are guaranteed to be referenced by our class loader
				 * except anonymous case handled above
				 */
				/* By scanning the class object, we've committed to it either being in a card external to the collection set, or that it is already part of a copied set and
				 * being scanned through the copy/scan cache.  In either case, a simple pointer forward update is all that is required.
				 */
				classPtr->classObject = (j9object_t)updateForwardedPointer((J9Object *)classPtr->classObject);
				Assert_MM_true(isLiveObject((J9Object *)classPtr->classObject));
			}
			classPtr = classPtr->replacedClass;
		} while (success && (NULL != classPtr));
	}
}

void
MM_CopyForwardScheme::scanClassLoaderObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *classLoaderObject, ScanReason reason)
{
	scanMixedObjectSlots(env, reservingContext, classLoaderObject, reason);

	J9ClassLoader *classLoader = J9VMJAVALANGCLASSLOADER_VMREF((J9VMThread*)env->getLanguageVMThread(), classLoaderObject);
	if (NULL != classLoader) {
		/* By scanning the class loader object, we've committed to it either being in a card external to the collection set, or that it is already part of a copied set and
		 * being scanned through the copy/scan cache.  In either case, a simple pointer forward update is all that is required.
		 */
		classLoader->classLoaderObject = updateForwardedPointer((J9Object *)classLoader->classLoaderObject);
		Assert_MM_true(isLiveObject((J9Object *)classLoader->classLoaderObject));

		/* No lock is required because this only runs under exclusive access */
		/* (NULL == classLoader->classHashTable) is true ONLY for DEAD class loaders */
		Assert_MM_true(NULL != classLoader->classHashTable);

		/* Do this for all classloaders except anonymous */
		if (0 == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER)) {
			GC_ClassLoaderClassesIterator iterator(_extensions, classLoader);
			J9Class *clazz = NULL;
			bool success = true;
			while (success && (NULL != (clazz = iterator.nextClass()))) {
				Assert_MM_true(NULL != clazz->classObject);
				/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
				success = copyAndForward(env, reservingContext, classLoaderObject, (J9Object **)&(clazz->classObject));
			}

			if (NULL != classLoader->moduleHashTable) {
				J9HashTableState walkState;
				J9Module **modulePtr = (J9Module **)hashTableStartDo(classLoader->moduleHashTable, &walkState);
				while (success && (NULL != modulePtr)) {
					J9Module * const module = *modulePtr;
					success = copyAndForward(env, reservingContext, classLoaderObject, (J9Object **)&(module->moduleObject));
					if (success) {
						if (NULL != module->moduleName) {
							success = copyAndForward(env, reservingContext, classLoaderObject, (J9Object **)&(module->moduleName));
						}
					}
					if (success) {
						if (NULL != module->version) {
							success = copyAndForward(env, reservingContext, classLoaderObject, (J9Object **)&(module->version));
						}
					}
					modulePtr = (J9Module**)hashTableNextDo(&walkState);
				}

				if (success && (classLoader == _javaVM->systemClassLoader)) {
					success = copyAndForward(env, reservingContext, classLoaderObject, (J9Object **)&(_javaVM->unamedModuleForSystemLoader->moduleObject));
				}
			}
		}
	}
}	

/****************************************
 * Scan completion routines
 ****************************************
 */
bool
MM_CopyForwardScheme::isScanCacheWorkAvailable(MM_CopyScanCacheListVLHGC *scanCacheList)
{
	return !scanCacheList->isEmpty();
}

bool
MM_CopyForwardScheme::isAnyScanCacheWorkAvailable()
{
	bool result = false;
	UDATA nodeLists = _scanCacheListSize;
	for (UDATA i = 0; (!result) && (i < nodeLists); i++) {
		result = isScanCacheWorkAvailable(&_cacheScanLists[i]);
	}
	return result;
}

bool
MM_CopyForwardScheme::isAnyScanWorkAvailable(MM_EnvironmentVLHGC *env)
{
	return (isAnyScanCacheWorkAvailable() || ((0 != _regionCountCannotBeEvacuated) && !abortFlagRaised() && env->_workStack.inputPacketAvailableFromWorkPackets(env)));
}

MM_CopyScanCacheVLHGC *
MM_CopyForwardScheme::getSurvivorCacheForScan(MM_EnvironmentVLHGC *env)
{
	MM_CopyScanCacheVLHGC *cache = NULL;

	for (UDATA index = 0; index < _compactGroupMaxCount; index++) {
		cache = env->_copyForwardCompactGroups[index]._copyCache;
		if ((NULL != cache) && cache->isScanWorkAvailable()) {
			return cache;
		}
	}

	return NULL;
}

MM_CopyForwardScheme::ScanReason
MM_CopyForwardScheme::getNextWorkUnit(MM_EnvironmentVLHGC *env, UDATA preferredNumaNode)
{
	env->_scanCache = NULL;
	ScanReason ret = SCAN_REASON_NONE;

	MM_CopyScanCacheVLHGC *cache = NULL;
	/* Preference is to use survivor copy cache */
	if (NULL != (cache = getSurvivorCacheForScan(env))) {
		env->_scanCache = cache;
		ret = SCAN_REASON_COPYSCANCACHE;
		return ret;
	}

	if (NULL != env->_deferredScanCache) {
		/* there is deferred scanning to do from partial depth first scanning */
		cache = (MM_CopyScanCacheVLHGC *)env->_deferredScanCache;
		env->_deferredScanCache = NULL;
		env->_scanCache = cache;
		ret = SCAN_REASON_COPYSCANCACHE;
		return ret;
	}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	env->_copyForwardStats._acquireScanListCount += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	bool doneFlag = false;
	volatile UDATA doneIndex = _doneIndex;

	while ((SCAN_REASON_NONE == ret) && !doneFlag) {
		if (SCAN_REASON_NONE == (ret = getNextWorkUnitNoWait(env, preferredNumaNode))) {
			omrthread_monitor_enter(*_workQueueMonitorPtr);
			*_workQueueWaitCountPtr += 1;

			if (doneIndex == _doneIndex) {
				if ((*_workQueueWaitCountPtr == env->_currentTask->getThreadCount()) && !isAnyScanWorkAvailable(env)) {
					*_workQueueWaitCountPtr = 0;
					_doneIndex += 1;
					omrthread_monitor_notify_all(*_workQueueMonitorPtr);
				} else {
					while (!isAnyScanWorkAvailable(env) && (doneIndex == _doneIndex)) {
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
						PORT_ACCESS_FROM_ENVIRONMENT(env);
						U_64 waitEndTime, waitStartTime;
						waitStartTime = j9time_hires_clock();
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
						omrthread_monitor_wait(*_workQueueMonitorPtr);
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
						waitEndTime = j9time_hires_clock();
						if (doneIndex == _doneIndex) {
							env->_copyForwardStats.addToWorkStallTime(waitStartTime, waitEndTime);
						} else {
							env->_copyForwardStats.addToCompleteStallTime(waitStartTime, waitEndTime);
						}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
					}
				}
			}

			/* Set the local done flag and if we are done and the waiting count is 0 (last thread) exit */
			doneFlag = (doneIndex != _doneIndex);
			if (!doneFlag) {
				*_workQueueWaitCountPtr -= 1;
			}
			omrthread_monitor_exit(*_workQueueMonitorPtr);
		}
	}

	return ret;
}

MM_CopyForwardScheme::ScanReason
MM_CopyForwardScheme::getNextWorkUnitOnNode(MM_EnvironmentVLHGC *env, UDATA numaNode)
{
	ScanReason ret = SCAN_REASON_NONE;

	MM_CopyScanCacheVLHGC *cache = _cacheScanLists[numaNode].popCache(env);
	if (NULL != cache) {
		/* Check if there are threads waiting that should be notified because of pending entries */
		if ((0 != *_workQueueWaitCountPtr) && isScanCacheWorkAvailable(&_cacheScanLists[numaNode])) {
			omrthread_monitor_enter(*_workQueueMonitorPtr);
			if (0 != *_workQueueWaitCountPtr) {
				omrthread_monitor_notify(*_workQueueMonitorPtr);
			}
			omrthread_monitor_exit(*_workQueueMonitorPtr);
		}
		env->_scanCache = cache;
		ret = SCAN_REASON_COPYSCANCACHE;
	}

	return ret;
}

MM_CopyForwardScheme::ScanReason
MM_CopyForwardScheme::getNextWorkUnitNoWait(MM_EnvironmentVLHGC *env, UDATA preferredNumaNode)
{
	UDATA nodeLists = _scanCacheListSize;
	ScanReason ret = SCAN_REASON_NONE;
	/* local node first */
	ret = getNextWorkUnitOnNode(env, preferredNumaNode);
	if (SCAN_REASON_NONE == ret) {
		/* we failed to find a scan cache on our preferred node */
		if (COMMON_CONTEXT_INDEX != preferredNumaNode) {
			/* try the common node */
			ret = getNextWorkUnitOnNode(env, COMMON_CONTEXT_INDEX);
		}
		/* now try the remaining nodes */
		UDATA nextNode = (preferredNumaNode + 1) % nodeLists;
		while ((SCAN_REASON_NONE == ret) && (nextNode != preferredNumaNode)) {
			if (COMMON_CONTEXT_INDEX != nextNode) {
				ret = getNextWorkUnitOnNode(env, nextNode);
			}
			nextNode = (nextNode + 1) % nodeLists;
		}
	}
	if (SCAN_REASON_NONE == ret && (0 != _regionCountCannotBeEvacuated) && !abortFlagRaised()) {
		if (env->_workStack.retrieveInputPacket(env)) {
			ret = SCAN_REASON_PACKET;
		}
	}
	return ret;
}

/**
 * Calculates distance from the allocation pointer to the scan pointer for the given cache.
 * 
 * If the allocation pointer is less than or equal to the scan pointer, or the cache is NULL
 * the distance is set to the maximum unsigned UDATA, SCAN_TO_COPY_CACHE_MAX_DISTANCE.
 * @return distance calculated.
 */
MMINLINE UDATA
MM_CopyForwardScheme::scanToCopyDistance(MM_CopyScanCacheVLHGC* cache)
{
	if (cache == NULL) {
		return SCAN_TO_COPY_CACHE_MAX_DISTANCE;
	}
	IDATA distance = ((UDATA) cache->cacheAlloc) - ((UDATA) cache->scanCurrent);
	UDATA udistance;
	if (distance <= 0) {
		udistance = SCAN_TO_COPY_CACHE_MAX_DISTANCE;
	} else {
		udistance = distance;
	}
	return udistance;
}

/**
 * For a given copyCache and scanCache (which may or may not also be a copy cache), return the
 * best cache for scanning of these two caches.
 * 
 * If the copyCache has work to scan, and the scanCache is not a copy cache, then the copyCache is
 * the better one. If they are both copy caches (it is assumed the scanCache in this context has
 * work to scan), then the one with the shorter scanToCopyDistance is the better one to scan.
 * 
 * @param copyCache the candidate copy cache
 * @param scanCache the current best scan cache, which may be updated.
 * @return true if the scanCache has been updated with the best cache to scan.
 */
MMINLINE bool
MM_CopyForwardScheme::bestCacheForScanning(MM_CopyScanCacheVLHGC* copyCache, MM_CopyScanCacheVLHGC** scanCache)
{
	if (!copyCache->isScanWorkAvailable()) {
		return false;
	}
	if (0 == ((*scanCache)->flags & OMR_COPYSCAN_CACHE_TYPE_COPY)) {
		*scanCache = copyCache;
		return true;
	}
	if (scanToCopyDistance(copyCache) < scanToCopyDistance(*scanCache)) {
		*scanCache = copyCache;
		return true;
	}
	return false;
}

MMINLINE bool
MM_CopyForwardScheme::aliasToCopyCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC** nextScanCache)
{
	bool interruptScanning = false;

	Assert_MM_unimplemented();
#if 0
	/* VMDESIGN 1359.
	 * Only alias the _survivorCopyScanCache IF there are 0 threads waiting.  If the current thread is the only producer and
	 * it aliases it's survivor cache then it will be the only thread able to consume.  This will alleviate the stalling issues
	 * described in the above mentioned design.
	 */
	if (0 == *_workQueueWaitCountPtr) {
		interruptScanning = bestCacheForScanning(env->_survivorCopyScanCache, nextScanCache) || interruptScanning;
	}
#endif /* 0 */

	return interruptScanning;
}

MMINLINE void
MM_CopyForwardScheme::scanObject(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason)
{
	J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, env);
	Assert_MM_mustBeClass(clazz);
	switch (_extensions->objectModel.getScanType(clazz)) {
	case GC_ObjectModel::SCAN_MIXED_OBJECT_LINKED:
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
		scanMixedObjectSlots(env, reservingContext, objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		scanOwnableSynchronizerObjectSlots(env, reservingContext, objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
		scanContinuationObject(env, reservingContext, objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		scanReferenceObjectSlots(env, reservingContext, objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
		scanClassObjectSlots(env, reservingContext, objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		scanClassLoaderObjectSlots(env, reservingContext, objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		scanPointerArrayObjectSlots(env, reservingContext, (J9IndexableObject *)objectPtr, reason);
		break;
	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		if (SCAN_REASON_DIRTY_CARD != reason) {
			/* since we copy arrays in the non-aborting case, count them as scanned in the abort case for symmetry */
			updateScanStats(env, objectPtr, reason);
		}
		break;
	default:
		Trc_MM_CopyForwardScheme_scanObject_invalid(env->getLanguageVMThread(), objectPtr, reason);
		Assert_MM_unreachable();
	}
}

MMINLINE void
MM_CopyForwardScheme::updateScanStats(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason)
{
	bool noEvacuation = false;
	if (0 != _regionCountCannotBeEvacuated) {
		noEvacuation = isObjectInNoEvacuationRegions(env, objectPtr);
	}

	if (SCAN_REASON_DIRTY_CARD == reason) {
		UDATA objectSize = _extensions->objectModel.getSizeInBytesWithHeader(objectPtr);
		env->_copyForwardStats._objectsCardClean += 1;
		env->_copyForwardStats._bytesCardClean += objectSize;
	} else if (abortFlagRaised() || noEvacuation) {
		UDATA objectSize = _extensions->objectModel.getSizeInBytesWithHeader(objectPtr);
		Assert_MM_false(SCAN_REASON_DIRTY_CARD == reason);
		MM_HeapRegionDescriptorVLHGC * region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(objectPtr);
		UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
		if (region->isEden()) {
			env->_copyForwardCompactGroups[compactGroup]._edenStats._liveObjects += 1;
			env->_copyForwardCompactGroups[compactGroup]._edenStats._liveBytes += objectSize;
			env->_copyForwardCompactGroups[compactGroup]._edenStats._scannedObjects += 1;
			env->_copyForwardCompactGroups[compactGroup]._edenStats._scannedBytes += objectSize;
		} else {
			env->_copyForwardCompactGroups[compactGroup]._nonEdenStats._liveObjects += 1;
			env->_copyForwardCompactGroups[compactGroup]._nonEdenStats._liveBytes += objectSize;
			env->_copyForwardCompactGroups[compactGroup]._nonEdenStats._scannedObjects += 1;
			env->_copyForwardCompactGroups[compactGroup]._nonEdenStats._scannedBytes += objectSize;
		}
	}

	/* else:
	 * if not abort, object is copied and stats are updated through copy method
	 * if abort, object is both copied and scanned, but we do not report those stats
	 */
}


void
MM_CopyForwardScheme::scanPointerArrayObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9IndexableObject *arrayPtr, ScanReason reason)
{
	UDATA index = 0;
	bool currentSplitUnitOnly = false;
	
	/*	only _abortInProgress==true or noEvacuation==true case are expected here, but we should handle all of exception cases(such as abortFlagRaised() case) */
	if (SCAN_REASON_PACKET == reason) {
		UDATA peekValue = (UDATA)env->_workStack.peek(env);
		if ((PACKET_ARRAY_SPLIT_TAG == (peekValue & PACKET_ARRAY_SPLIT_TAG))) {
			UDATA workItem = (UDATA)env->_workStack.pop(env);
			index = workItem >> PACKET_ARRAY_SPLIT_SHIFT;
			currentSplitUnitOnly = ((PACKET_ARRAY_SPLIT_CURRENT_UNIT_ONLY_TAG == (peekValue & PACKET_ARRAY_SPLIT_CURRENT_UNIT_ONLY_TAG)));
		}
	}
	if (0 == index) {
		/* make sure we only record stats for the object once -- note that this means we might
		 * attribute the scanning cost to the wrong thread, but that's not really important
		 */
		updateScanStats(env, (J9Object *)arrayPtr, reason);
	}
	
	scanPointerArrayObjectSlotsSplit(env, reservingContext, arrayPtr, index, currentSplitUnitOnly);
}

/**
 * Scans all the objects to scan in the env->_scanCache and flushes the cache at the end.
 */
void
MM_CopyForwardScheme::completeScanCache(MM_EnvironmentVLHGC *env)
{
	MM_CopyScanCacheVLHGC *scanCache = (MM_CopyScanCacheVLHGC *)env->_scanCache;

	/* mark that cache is in use as a scan cache */
	scanCache->setCurrentlyBeingScanned();
	if (scanCache->isSplitArray()) {
		/* a scan cache can't be a split array and have generic work available */
		Assert_MM_false(scanCache->isScanWorkAvailable());
		MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(scanCache->scanCurrent);
		J9IndexableObject *arrayObject = (J9IndexableObject *)scanCache->scanCurrent;
		UDATA nextIndex = scanCache->_arraySplitIndex;
		Assert_MM_true(0 != nextIndex);
		scanPointerArrayObjectSlotsSplit(env, reservingContext, arrayObject, nextIndex);
		scanCache->clearSplitArray();
	} else if (scanCache->isScanWorkAvailable()) {
		/* we want to perform a NUMA-aware analogue to "hierarchical scanning" so this scan cache should pull other objects into its node */
		MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(scanCache->scanCurrent);
		do {
			GC_ObjectHeapIteratorAddressOrderedList heapChunkIterator(
				_extensions,
				(J9Object *)scanCache->scanCurrent,
				(J9Object *)scanCache->cacheAlloc, false);
			/* Advance the scan pointer to the top of the cache to signify that this has been scanned */
			scanCache->scanCurrent = scanCache->cacheAlloc;
			/* Scan the chunk for all live objects */
			J9Object *objectPtr = NULL;
			while((objectPtr = heapChunkIterator.nextObject()) != NULL) {
				scanObject(env, reservingContext, objectPtr, SCAN_REASON_COPYSCANCACHE);
			}
		} while(scanCache->isScanWorkAvailable());

	}
	/* mark cache as no longer in use for scanning */
	scanCache->clearCurrentlyBeingScanned();
	/* Done with the cache - build a free list entry in the hole, release the cache to the free list (if not used), and continue */
	flushCache(env, scanCache);
}

MMINLINE bool
MM_CopyForwardScheme::incrementalScanMixedObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
	bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache)
{
	GC_MixedObjectIterator mixedObjectIterator(_javaVM->omrVM);

	if (!hasPartiallyScannedObject) {
		/* finished previous object, step up for next one */
		mixedObjectIterator.initialize(_javaVM->omrVM, objectPtr);
	} else {
		/* retrieve partial scan state of cache */
		mixedObjectIterator.restore(&(scanCache->_objectIteratorState));
	}
	GC_SlotObject *slotObject;
	bool success = true;
	while (success && ((slotObject = mixedObjectIterator.nextSlot()) != NULL)) {
		/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
		success = copyAndForward(env, reservingContext, objectPtr, slotObject);

		/* interrupt scanning this cache if it should be aliased or re-aliased */
		if (aliasToCopyCache(env, nextScanCache)) {
			/* save scan state of cache */
			mixedObjectIterator.save(&(scanCache->_objectIteratorState));
			return true;
		}
	}

	return false;
}

MMINLINE bool
MM_CopyForwardScheme::incrementalScanClassObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
	bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache)
{
	/* NOTE: An incremental scan solution should be provided here.  For now, just use a full scan and ignore any hierarchical needs. */
	scanClassObjectSlots(env, reservingContext, objectPtr);
	return false;
}

MMINLINE bool
MM_CopyForwardScheme::incrementalScanClassLoaderObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
	bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache)
{
	/* NOTE: An incremental scan solution should be provided here.  For now, just use a full scan and ignore any hierarchical needs. */
	scanClassLoaderObjectSlots(env, reservingContext, objectPtr);
	return false;
}

MMINLINE bool
MM_CopyForwardScheme::incrementalScanPointerArrayObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
	bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache)
{
	GC_PointerArrayIterator pointerArrayIterator(_javaVM);

	if (!hasPartiallyScannedObject) {
		/* finished previous object, step up for next one */
		pointerArrayIterator.initialize(_javaVM, objectPtr);
	} else {
		/* retrieve partial scan state of cache */
		pointerArrayIterator.restore(&(scanCache->_objectIteratorState));
	}

 	GC_SlotObject *slotObject = NULL;
 	bool success = true;

	while (success && ((slotObject = pointerArrayIterator.nextSlot()) != NULL)) {
		/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
		success = copyAndForward(env, reservingContext, objectPtr, slotObject);

		/* interrupt scanning this cache if it should be aliased or re-aliased */
		if (aliasToCopyCache(env, nextScanCache)) {
			/* save scan state of cache */
			pointerArrayIterator.save(&(scanCache->_objectIteratorState));
			return true;
		}
	}

	return false;
}

MMINLINE bool
MM_CopyForwardScheme::incrementalScanReferenceObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
	bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache)
{
	GC_MixedObjectIterator mixedObjectIterator(_javaVM->omrVM);
	fj9object_t *referentPtr = J9GC_J9VMJAVALANGREFERENCE_REFERENT_ADDRESS(env, objectPtr);
	bool referentMustBeMarked = false;

	if (!hasPartiallyScannedObject) {
		/* finished previous object, step up for next one */
		mixedObjectIterator.initialize(_javaVM->omrVM, objectPtr);
	} else {
		/* retrieve partial scan state of cache */
		mixedObjectIterator.restore(&(scanCache->_objectIteratorState));
	}

	if (J9AccClassReferenceSoft == (J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(objectPtr, env)) & J9AccClassReferenceMask)) {
		/* Object is a Soft Reference: mark it if not expired */
		U_32 age = J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, objectPtr);
		referentMustBeMarked = age < _extensions->getDynamicMaxSoftReferenceAge();
	}
	
	GC_SlotObject *slotObject;
	bool success = true;
	while (success && ((slotObject = mixedObjectIterator.nextSlot()) != NULL)) {
		if (((fj9object_t *)slotObject->readAddressFromSlot() != referentPtr) || referentMustBeMarked) {
			/* Copy/Forward the slot reference and perform any inter-region remember work that is required */
			success = copyAndForward(env, reservingContext, objectPtr, slotObject);

			/* interrupt scanning this cache if it should be aliased or re-aliased */
			if (aliasToCopyCache(env, nextScanCache)) {
				/* save scan state of cache */
				mixedObjectIterator.save(&(scanCache->_objectIteratorState));
				return true;
			}
		}
	}

	return false;
}

void
MM_CopyForwardScheme::incrementalScanCacheBySlot(MM_EnvironmentVLHGC *env)
{
	MM_CopyScanCacheVLHGC* scanCache = (MM_CopyScanCacheVLHGC *)env->_scanCache;
	J9Object *objectPtr;
	MM_CopyScanCacheVLHGC* nextScanCache = scanCache;
	
	nextCache:
	/* mark that cache is in use as a scan cache */
	scanCache->setCurrentlyBeingScanned();
	bool hasPartiallyScannedObject = scanCache->_hasPartiallyScannedObject;
	if (scanCache->isScanWorkAvailable()) {
		/* we want to perform a NUMA-aware analogue to "hierarchical scanning" so this scan cache should pull other objects into its node */
		MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(env->_scanCache->scanCurrent);
		do {
			void *cacheAlloc = scanCache->cacheAlloc;
			GC_ObjectHeapIteratorAddressOrderedList heapChunkIterator(
				_extensions, 
				(J9Object *)scanCache->scanCurrent,
				(J9Object *)cacheAlloc,
				false);
	
			/* Scan the chunk for live objects, incrementally slot by slot */
			while ((objectPtr = heapChunkIterator.nextObject()) != NULL) {
				/* retrieve scan state of the scan cache */
				switch(_extensions->objectModel.getScanType(objectPtr)) {
				case GC_ObjectModel::SCAN_MIXED_OBJECT_LINKED:
				case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
				case GC_ObjectModel::SCAN_MIXED_OBJECT:
				case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
				case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
					hasPartiallyScannedObject = incrementalScanMixedObjectSlots(env, reservingContext, scanCache, objectPtr, hasPartiallyScannedObject, &nextScanCache);
					break;
				case GC_ObjectModel::SCAN_CLASS_OBJECT:
					hasPartiallyScannedObject = incrementalScanClassObjectSlots(env, reservingContext, scanCache, objectPtr, hasPartiallyScannedObject, &nextScanCache);
					break;
				case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
					hasPartiallyScannedObject = incrementalScanClassLoaderObjectSlots(env, reservingContext, scanCache, objectPtr, hasPartiallyScannedObject, &nextScanCache);
					break;
				case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
					hasPartiallyScannedObject = incrementalScanPointerArrayObjectSlots(env, reservingContext, scanCache, objectPtr, hasPartiallyScannedObject, &nextScanCache);
					break;
				case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
					hasPartiallyScannedObject = incrementalScanReferenceObjectSlots(env, reservingContext, scanCache, objectPtr, hasPartiallyScannedObject, &nextScanCache);
					break;
				case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
					continue;
					break;
				default:
					Assert_MM_unreachable();
				}
						
				/* object was not completely scanned in order to interrupt scan */
				if (hasPartiallyScannedObject) {
					/* interrupt scan, save scan state of cache before deferring */
					scanCache->scanCurrent = objectPtr;
					scanCache->_hasPartiallyScannedObject = true;
					/* Only save scan cache if it is not a copy cache, and then don't add to scanlist - this
					 * can cause contention, just defer to later time on same thread
					 * if deferred cache is occupied, then queue current scan cache on scan list
					 */
					scanCache->clearCurrentlyBeingScanned();	
					if (0 == (scanCache->flags & OMR_COPYSCAN_CACHE_TYPE_COPY)) {
						if (NULL == env->_deferredScanCache) {
							env->_deferredScanCache = scanCache;
						} else {
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
							env->_copyForwardStats._releaseScanListCount += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
							addCacheEntryToScanCacheListAndNotify(env, scanCache);
						}
					}
					env->_scanCache = scanCache = nextScanCache;
					goto nextCache;
				}
			}
			/* Advance the scan pointer for the objects that were scanned */
			scanCache->scanCurrent = cacheAlloc;
		} while (scanCache->isScanWorkAvailable());
	}
	/* although about to flush this cache, the flush occurs only if the cache is not in use
	 * hence we still need to store the state of current scanning */
	scanCache->_hasPartiallyScannedObject = false;
	/* mark cache as no longer in use for scanning */
	scanCache->clearCurrentlyBeingScanned();		
	/* Done with the cache - build a free list entry in the hole, release the cache to the free list (if not used), and continue */
	flushCache(env, scanCache);
}

void
MM_CopyForwardScheme::cleanOverflowedRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region, U_8 flagToClean)
{
	Assert_MM_true(region->containsObjects());
	/* do we need to clean this region? */
	U_8 flags = region->_markData._overflowFlags;
	if (flagToClean == (flags & flagToClean)) {
		/* Region must be cleaned */
		/* save back the new flags, first, in case we re-overflow in another thread (or this thread) */
		U_8 newFlags = flags & ~flagToClean;
		region->_markData._overflowFlags = newFlags;
		/* Force our write of the overflow flags from our cache and ensure that we have no stale mark map data before we walk */
		MM_AtomicOperations::sync();
		if (region->_copyForwardData._evacuateSet || region->isFreshSurvivorRegion()) {
			 cleanOverflowInRange(env, (UDATA *)region->getLowAddress(), (UDATA *)region->getHighAddress());
		} else if (region->isSurvivorRegion()) {
			GC_SurvivorMemoryIterator survivorIterator(env, region, _compressedSurvivorTable);
			while (survivorIterator.next()) {
				 cleanOverflowInRange(env, (UDATA *)survivorIterator.getCurrentLow(), (UDATA *)survivorIterator.getCurrentHigh());
			}
		}		
	}
}

bool
MM_CopyForwardScheme::isWorkPacketsOverflow(MM_EnvironmentVLHGC *env)
{
	MM_WorkPackets *packets = (MM_WorkPackets *)(env->_cycleState->_workPackets);
	bool result = false;
	if (packets->getOverflowFlag()) {
		result = true;
	}
	return result;
}

bool
MM_CopyForwardScheme::handleOverflow(MM_EnvironmentVLHGC *env)
{
	MM_WorkPackets *packets = (MM_WorkPackets *)(env->_cycleState->_workPackets);
	bool result = false;

	if (packets->getOverflowFlag()) {
		result = true;
		if (((MM_CopyForwardSchemeTask*)env->_currentTask)->synchronizeGCThreadsAndReleaseMainForMark(env, UNIQUE_ID)) {
			packets->clearOverflowFlag();
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		/* our overflow handling mechanism is to set flags in the region descriptor so clean those regions */
		U_8 flagToRemove = MM_RegionBasedOverflowVLHGC::overflowFlagForCollectionType(env, env->_cycleState->_collectionType);
		GC_HeapRegionIteratorVLHGC regionIterator = GC_HeapRegionIteratorVLHGC(_regionManager);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			if (region->containsObjects()) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					cleanOverflowedRegion(env, region, flagToRemove);
				}
			}
		}
		((MM_CopyForwardSchemeTask*)env->_currentTask)->synchronizeGCThreadsForMark(env, UNIQUE_ID);
	}
	return result;
}

void
MM_CopyForwardScheme::completeScanForAbort(MM_EnvironmentVLHGC *env)
{
	/* From this point on, no copying should happen - reservingContext is irrelevant */
	MM_AllocationContextTarok *reservingContext = _commonContext;

	J9Object *objectPtr = NULL;
	do {
		while (NULL != (objectPtr = (J9Object *)env->_workStack.pop(env))) {
			do {
				Assert_MM_false(MM_ForwardedHeader(objectPtr, _extensions->compressObjectReferences()).isForwardedPointer());
				scanObject(env, reservingContext, objectPtr, SCAN_REASON_PACKET);

				objectPtr = (J9Object *)env->_workStack.popNoWait(env);
			} while (NULL != objectPtr);
		}
		((MM_CopyForwardSchemeTask*)env->_currentTask)->synchronizeGCThreadsForMark(env, UNIQUE_ID);
	} while (handleOverflow(env));
}

void
MM_CopyForwardScheme::completeScanWorkPacket(MM_EnvironmentVLHGC *env)
{
	MM_AllocationContextTarok *reservingContext = _commonContext;
	J9Object *objectPtr = NULL;

	while (NULL != (objectPtr = (J9Object *)env->_workStack.popNoWaitFromCurrentInputPacket(env))) {
		Assert_MM_false(MM_ForwardedHeader(objectPtr, _extensions->compressObjectReferences()).isForwardedPointer());
		scanObject(env, reservingContext, objectPtr, SCAN_REASON_PACKET);
	}
}

void
MM_CopyForwardScheme::completeScan(MM_EnvironmentVLHGC *env)
{
	UDATA nodeOfThread = 0;

	/* if we aren't using NUMA, we don't want to check the thread affinity since we will have only one list of scan caches */
	if (_extensions->_numaManager.isPhysicalNUMASupported()) {
		nodeOfThread = env->getNumaAffinity();
		Assert_MM_true(nodeOfThread <= _extensions->_numaManager.getMaximumNodeNumber());
	}
	ScanReason scanReason = SCAN_REASON_NONE;
	while(SCAN_REASON_NONE != (scanReason = getNextWorkUnit(env, nodeOfThread))) {
		if (SCAN_REASON_COPYSCANCACHE == scanReason) {
			Assert_MM_true(env->_scanCache->cacheBase <= env->_scanCache->cacheAlloc);
			Assert_MM_true(env->_scanCache->cacheAlloc <= env->_scanCache->cacheTop);
			Assert_MM_true(env->_scanCache->scanCurrent <= env->_scanCache->cacheAlloc);

			switch (_extensions->scavengerScanOrdering) {
			case MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_BREADTH_FIRST:
			case MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_DYNAMIC_BREADTH_FIRST:
				completeScanCache(env);
				break;
			case MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_HIERARCHICAL:
				incrementalScanCacheBySlot(env);
				break;
			default:
				Assert_MM_unreachable();
				break;
			} /* end of switch on type of scan order */
		} else if (SCAN_REASON_PACKET == scanReason) {
			completeScanWorkPacket(env);
		}
	}

	/* flush Mark Map caches before we start draining Work Stack (in case of Abort) */
	addCopyCachesToFreeList(env);

	if (((MM_CopyForwardSchemeTask*)env->_currentTask)->synchronizeGCThreadsAndReleaseMainForAbort(env, UNIQUE_ID)) {
		if (abortFlagRaised()) {
			_abortInProgress = true;
		}
		/* using abort case to handle work packets overflow during copyforwardHybrid */
		if (!_abortInProgress && (0 != _regionCountCannotBeEvacuated) && isWorkPacketsOverflow(env)) {
			_abortInProgress = true;
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	if (_abortInProgress) {
		completeScanForAbort(env);
	}
}

MMINLINE void
MM_CopyForwardScheme::addOwnableSynchronizerObjectInList(MM_EnvironmentVLHGC *env, j9object_t object)
{
	if (NULL != _extensions->accessBarrier->isObjectInOwnableSynchronizerList(object)) {
		env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->add(env, object);
		env->_copyForwardStats._ownableSynchronizerSurvived += 1;
	}
}

#if defined(J9VM_GC_FINALIZATION)
void
MM_CopyForwardScheme::scanUnfinalizedObjects(MM_EnvironmentVLHGC *env)
{
	/* ensure that all clearable processing is complete up to this point since this phase resurrects objects */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

	GC_FinalizableObjectBuffer buffer(_extensions);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_copyForwardData._evacuateSet && !region->getUnfinalizedObjectList()->wasEmpty()) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				J9Object *pointer = region->getUnfinalizedObjectList()->getPriorList();
				while (NULL != pointer) {
					bool finalizable = false;
					env->_copyForwardStats._unfinalizedCandidates += 1;

					Assert_MM_true(region->isAddressInRegion(pointer));

					/* NOTE: it is safe to read from the forwarded object since either:
					 * 1. it was copied before unfinalized processing began, or
					 * 2. it was copied by this thread.
					 */
					MM_ForwardedHeader forwardedHeader(pointer, _extensions->compressObjectReferences());
					J9Object *forwardedPtr = forwardedHeader.getForwardedObject();
					if (NULL == forwardedPtr) {
						if (_markMap->isBitSet(pointer)) {
							forwardedPtr = pointer;
						} else {
							Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedHeader));
							/* TODO:  Use the context for the finalize thread */
							MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(pointer);
							forwardedPtr = copy(env, reservingContext, &forwardedHeader);
							finalizable = true;

							if (NULL == forwardedPtr) {
								/* We failed to copy the object. This must have caused an abort. This will be dealt with in scanUnfinalizedObjectsComplete */ 
								Assert_MM_false(_abortInProgress);
								Assert_MM_true(abortFlagRaised());
								forwardedPtr = pointer;
							}
						}
					}

					J9Object *next = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);
					if (finalizable) {
						/* object was not previously marked -- it is now finalizable so push it to the local buffer */
						env->_copyForwardStats._unfinalizedEnqueued += 1;
						buffer.add(env, forwardedPtr);
						env->_cycleState->_finalizationRequired = true;
					} else {
						env->getGCEnvironment()->_unfinalizedObjectBuffer->add(env, forwardedPtr);
					}

					pointer = next;
				}

				/* Flush the local buffer of finalizable objects to the global list.
				 * This is done once per region to ensure that multi-tenant lists
				 * only contain objects from the same allocation context
				 */
				buffer.flush(env);
			}
		}
	}

	/* restore everything to a flushed state before exiting */
	env->getGCEnvironment()->_unfinalizedObjectBuffer->flush(env);
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_CopyForwardScheme::scanContinuationObjects(MM_EnvironmentVLHGC *env)
{
#if JAVA_SPEC_VERSION >= 19
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_copyForwardData._evacuateSet && !region->getContinuationObjectList()->wasEmpty()) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				J9Object *pointer = region->getContinuationObjectList()->getPriorList();

				while (NULL != pointer) {
					env->_copyForwardStats._continuationCandidates += 1;
					Assert_MM_true(region->isAddressInRegion(pointer));

					/* NOTE: it is safe to read from the forwarded object since either:
					 * 1. it was copied before continuation processing began, or
					 * 2. it was copied by this thread.
					 */
					MM_ForwardedHeader forwardedHeader(pointer, _extensions->compressObjectReferences());
					J9Object *forwardedPtr = forwardedHeader.getForwardedObject();
					if ((NULL == forwardedPtr) || VM_ContinuationHelpers::isFinished(*VM_ContinuationHelpers::getContinuationStateAddress((J9VMThread *)env->getLanguageVMThread() , forwardedPtr))) {
						if (_markMap->isBitSet(pointer)) {
							forwardedPtr = pointer;
						}
					}

					J9Object *next = _extensions->accessBarrier->getContinuationLink(pointer);
					if (NULL == forwardedPtr) {
						/* object was not previously marked, clean up */
						env->_copyForwardStats._continuationCleared += 1;
						_extensions->releaseNativesForContinuationObject(env, pointer);
					} else {
						env->getGCEnvironment()->_continuationObjectBuffer->add(env, forwardedPtr);
					}
					pointer = next;
				}
			}
		}
	}

	/* restore everything to a flushed state before exiting */
	env->getGCEnvironment()->_continuationObjectBuffer->flush(env);
#endif /* JAVA_SPEC_VERSION >= 19 */
}

void
MM_CopyForwardScheme::cleanCardTable(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);
	if (NULL != env->_cycleState->_externalCycleState) {
		/* A GMP is in progress */
		MM_CopyForwardGMPCardCleaner cardCleaner(this);
		cleanCardTableForPartialCollect(env, &cardCleaner);
	} else {
		/* No GMP is in progress so we can clear more aggressively */
		MM_CopyForwardNoGMPCardCleaner cardCleaner(this);
		cleanCardTableForPartialCollect(env, &cardCleaner);
	}
}

void
MM_CopyForwardScheme::cleanCardTableForPartialCollect(MM_EnvironmentVLHGC *env, MM_CardCleaner *cardCleaner)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 cleanStartTime = j9time_hires_clock();

	bool gmpIsRunning = (NULL != env->_cycleState->_externalCycleState);
	MM_CardTable* cardTable = _extensions->cardTable;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		/* Don't include survivor regions as we scan - they don't need to be processed and this will throw off the work unit indices */
		if (region->containsObjects() && region->_copyForwardData._initialLiveSet) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				if (!region->_markData._shouldMark) {
					/* this region isn't part of the collection set, so it may have dirty or remembered cards in it. */
					cardTable->cleanCardsInRegion(env, cardCleaner, region);
				} else {
					/* this region is part of the collection set, so just change its dirty cards to clean (or GMP_MUST_SCAN) */
					void *low = region->getLowAddress();
					void *high = region->getHighAddress();
					Card *card = cardTable->heapAddrToCardAddr(env, low);
					Card *toCard = cardTable->heapAddrToCardAddr(env, high);

					while (card < toCard) {
						Card fromState = *card;
						switch(fromState) {
						case CARD_PGC_MUST_SCAN:
							*card = CARD_CLEAN;
							break;
						case CARD_GMP_MUST_SCAN:
							/* This can only occur if a GMP is currently active, no transition is required */
							Assert_MM_true(gmpIsRunning);
							break;
						case CARD_DIRTY:
							if (gmpIsRunning) {
								*card = CARD_GMP_MUST_SCAN;
							} else {
								*card = CARD_CLEAN;
							}
							break;
						case CARD_CLEAN:
							/* do nothing */
							break;
						case CARD_REMEMBERED:
							/* card state valid if left over during aborted card cleaning */
							*card = CARD_CLEAN;
							break;
						case CARD_REMEMBERED_AND_GMP_SCAN:
							/* card state valid if left over during aborted card cleaning */
							Assert_MM_true(gmpIsRunning);
							*card = CARD_GMP_MUST_SCAN;
							break;
						default:
							Assert_MM_unreachable();
						}
						card += 1;
					}
				}
			}
		}
	}

	U_64 cleanEndTime = j9time_hires_clock();
	env->_cardCleaningStats.addToCardCleaningTime(cleanStartTime, cleanEndTime);
}

void
MM_CopyForwardScheme::updateOrDeleteObjectsFromExternalCycle(MM_EnvironmentVLHGC *env)
{
	/* this function has knowledge of the collection set, which is only valid during a PGC */
	Assert_MM_true(NULL != env->_cycleState->_externalCycleState);

	MM_MarkMap *externalMarkMap = env->_cycleState->_externalCycleState->_markMap;
	Assert_MM_true(externalMarkMap != _markMap);

	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_markData._shouldMark) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				Assert_MM_true(region->_copyForwardData._initialLiveSet);
				Assert_MM_false(region->isSurvivorRegion());
				Assert_MM_true(region->containsObjects());

				if (abortFlagRaised() || region->_markData._noEvacuation) {
					/* Walk the mark map range for the region and fixing mark bits to be the subset of the current mark map.
					 * (Those bits that are cleared have been moved and their bits are already set).
					 */
					UDATA currentExternalIndex = externalMarkMap->getSlotIndex((J9Object *)region->getLowAddress());
					UDATA topExternalIndex = externalMarkMap->getSlotIndex((J9Object *)region->getHighAddress());
					UDATA currentIndex = _markMap->getSlotIndex((J9Object *)region->getLowAddress());

					while (currentExternalIndex < topExternalIndex) {
						UDATA slot = externalMarkMap->getSlot(currentExternalIndex);
						if(0 != slot) {
							externalMarkMap->setSlot(currentExternalIndex, slot & _markMap->getSlot(currentIndex));
						}
						currentExternalIndex += 1;
						currentIndex += 1;
					}
				} else {
					Assert_MM_false(region->_nextMarkMapCleared);
					externalMarkMap->setBitsForRegion(env, region, true);
				}
			}
		}
	}

	/* Mark map processing must be completed before we move to work packets */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

	/* Clear or update references on external cycle work packets, depending on whether the reference has been forwarded or not */
	UDATA totalCount = 0;
	UDATA deletedCount = 0;
	UDATA preservedCount = 0;
	MM_WorkPacketsIterator packetIterator(env, env->_cycleState->_externalCycleState->_workPackets);
	MM_Packet *packet = NULL;
	while (NULL != (packet = packetIterator.nextPacket(env))) {
		if (!packet->isEmpty()) {
			/* there is data in this packet so use it */
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_PacketSlotIterator slotIterator(packet);
				J9Object **slot = NULL;
				while (NULL != (slot = slotIterator.nextSlot())) {
					J9Object *object = *slot;
					Assert_MM_true(NULL != object);
					if (PACKET_INVALID_OBJECT != (UDATA)object) {
						totalCount += 1;
						if (isLiveObject(object)) {
							Assert_MM_true(externalMarkMap->isBitSet(object));
							Assert_MM_true(_markMap->isBitSet(object));
							Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(object, env));
						} else {
							Assert_MM_true(isObjectInEvacuateMemory(object));
							J9Object *forwardedObject = updateForwardedPointer(object);
							if (externalMarkMap->isBitSet(forwardedObject)) {
								Assert_MM_true(_markMap->isBitSet(forwardedObject));
								Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(forwardedObject, env));
								preservedCount += 1;
								*slot = forwardedObject;
							} else {
								/* this object failed to survive the PGC cycle */
								Assert_MM_true(!_markMap->isBitSet(forwardedObject));
								deletedCount += 1;
								slotIterator.resetSplitTagIndexForObject(object, PACKET_INVALID_OBJECT);
								*slot = (J9Object *)PACKET_INVALID_OBJECT;
							}
						}
					}
				}
			}
		}
	}

	Trc_MM_CopyForwardScheme_deleteDeadObjectsFromExternalCycle(env->getLanguageVMThread(), totalCount, deletedCount, preservedCount);
}

bool
MM_CopyForwardScheme::scanObjectsInRange(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress, bool rememberedObjectsOnly)
{
	/* we only support scanning exactly one card at a time */
	Assert_MM_true(0 == ((UDATA)lowAddress & (J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP - 1)));
	Assert_MM_true(((UDATA)lowAddress + CARD_SIZE) == (UDATA)highAddress);
	/* card cleaning is done after stack processing so any objects we copy should be copied into the node which refers to them, even from cards */
	MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(lowAddress);

	if (rememberedObjectsOnly) {
		for (UDATA bias = 0; bias < CARD_SIZE; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
			void *scanAddress = (void *)((UDATA)lowAddress + bias);
			MM_HeapMapWordIterator markedObjectIterator(_markMap, scanAddress);
			J9Object *fromObject = NULL;
			while (NULL != (fromObject = markedObjectIterator.nextObject())) {
				/* this object needs to be re-scanned (to update next mark map and RSM) */
				if (_extensions->objectModel.isRemembered(fromObject)) {
					scanObject(env, reservingContext, fromObject, SCAN_REASON_DIRTY_CARD);

				}
			}
		}
	} else {
		for (UDATA bias = 0; bias < CARD_SIZE; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
			void *scanAddress = (void *)((UDATA)lowAddress + bias);
			MM_HeapMapWordIterator markedObjectIterator(_markMap, scanAddress);
			J9Object *fromObject = NULL;
			while (NULL != (fromObject = markedObjectIterator.nextObject())) {
				/* this object needs to be re-scanned (to update next mark map and RSM) */
				scanObject(env, reservingContext, fromObject, SCAN_REASON_DIRTY_CARD);
			}
		}
	}
	/* we can only clean the card if we haven't raised the abort flag since we might have aborted in this thread
	 * while processing the card while another thread copied an object that this card referred to.  We need to
	 * make sure that we re-clean this card in abort processing, in that case, so don't clean the card.
	 * If an abort _is_ already in progress, however, no objects can be copied so we are safe to clean this card
	 * knowing that all its objects have correct references.
	 */
	return _abortInProgress || !abortFlagRaised();
}


/**
 * The root set scanner for MM_CopyForwardScheme.
 * @copydoc MM_RootScanner
 * @ingroup GC_Modron_Standard
 */
class MM_CopyForwardSchemeRootScanner : public MM_RootScanner
{
private:
	MM_CopyForwardScheme *_copyForwardScheme;  /**< Local reference back to the copy forward scheme driving the collection */

private:
	virtual void doSlot(J9Object **slotPtr) {
		if (NULL != *slotPtr) {
			/* we don't have the context of this slot so just relocate the object into the same node where we found it */
			MM_AllocationContextTarok *reservingContext = _copyForwardScheme->getContextForHeapAddress(*slotPtr);
			_copyForwardScheme->copyAndForward(MM_EnvironmentVLHGC::getEnvironment(_env), reservingContext, slotPtr);
		}
	}

	virtual void doStackSlot(J9Object **slotPtr, void *walkState, const void* stackLocation) {
		if (_copyForwardScheme->isHeapObject(*slotPtr)) {
			/* heap object - validate and mark */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::COULD_BE_FORWARDED, *slotPtr, stackLocation, walkState).validate(_env));
			/* we know that threads are bound to nodes so relocalize this object into the node of the thread which directly references it */
			J9VMThread *thread = ((J9StackWalkState *)walkState)->currentThread;
			MM_AllocationContextTarok *reservingContext = (MM_AllocationContextTarok *)MM_EnvironmentVLHGC::getEnvironment(thread)->getAllocationContext();
			_copyForwardScheme->copyAndForward(MM_EnvironmentVLHGC::getEnvironment(_env), reservingContext, slotPtr);
		} else if (NULL != *slotPtr) {
			/* stack object - just validate */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, *slotPtr, stackLocation, walkState).validate(_env));
		}
	}

	virtual void doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator) {
		if (_copyForwardScheme->isHeapObject(*slotPtr)) {
			/* we know that threads are bound to nodes so relocalize this object into the node of the thread which directly references it */
			J9VMThread *thread = vmThreadIterator->getVMThread();
			MM_AllocationContextTarok *reservingContext = (MM_AllocationContextTarok *)MM_EnvironmentVLHGC::getEnvironment(thread)->getAllocationContext();
			_copyForwardScheme->copyAndForward(MM_EnvironmentVLHGC::getEnvironment(_env), reservingContext, slotPtr);
		} else if (NULL != *slotPtr) {
			Assert_MM_true(vmthreaditerator_state_monitor_records == vmThreadIterator->getState());
		}
	}

	virtual void doClass(J9Class *clazz) {
		/* Should never try to scan J9Class structures - these are handled by j.l.c and class loader references on the heap */
		Assert_MM_unreachable();
	}

	virtual void doClassLoader(J9ClassLoader *classLoader) {
		if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			/* until we decide if class loaders should be common, just relocate this object back into its existing node */
			MM_AllocationContextTarok *reservingContext = _copyForwardScheme->getContextForHeapAddress(classLoader->classLoaderObject);
			_copyForwardScheme->copyAndForward(MM_EnvironmentVLHGC::getEnvironment(_env), reservingContext, &classLoader->classLoaderObject);
		}
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t object) {
		Assert_MM_unreachable();
	}

	virtual void scanFinalizableObjects(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_FinalizableObjects);
		/* synchronization can be expensive so skip it if there's no work to do */
		if (_copyForwardScheme->_shouldScanFinalizableObjects) {
			if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
				_copyForwardScheme->scanFinalizableObjects(MM_EnvironmentVLHGC::getEnvironment(env));
				env->_currentTask->releaseSynchronizedGCThreads(env);
			}
		} else {
			/* double check that there really was no work to do */
			Assert_MM_true(!MM_GCExtensions::getExtensions(env)->finalizeListManager->isFinalizableObjectProcessingRequired());
		}
		reportScanningEnded(RootScannerEntity_FinalizableObjects);
	}
#endif /* J9VM_GC_FINALIZATION */

public:
	MM_CopyForwardSchemeRootScanner(MM_EnvironmentVLHGC *env, MM_CopyForwardScheme *copyForwardScheme) :
		MM_RootScanner(env),
		_copyForwardScheme(copyForwardScheme)
	{
		_typeId = __FUNCTION__;
	};

	/**
	 * Scan all root set references from the VM into the heap.
	 * For all slots that are hard root references into the heap, the appropriate slot handler will be called.
	 */
	void
	scanRoots(MM_EnvironmentBase *env)
	{
		/* threads and their stacks tell us more about NUMA affinity than anything else so ensure that we scan them first and process all scan caches that they produce before proceeding */
		scanThreads(env);
		_copyForwardScheme->completeScan(MM_EnvironmentVLHGC::getEnvironment(env));

		Assert_MM_true(_classDataAsRoots == !_copyForwardScheme->isDynamicClassUnloadingEnabled());
		if (_classDataAsRoots) {
			/* The classLoaderObject of a class loader might be in the nursery, but a class loader
			 * can never be in the remembered set, so include class loaders here.
			 */
			scanClassLoaders(env);
		}

#if defined(J9VM_GC_FINALIZATION)
		scanFinalizableObjects(env);
#endif /* J9VM_GC_FINALIZATION */
		scanJNIGlobalReferences(env);

		if (_stringTableAsRoot){
			scanStringTable(env);
		}
	}
};

/**
 * The clearable root set scanner for MM_CopyForwardScheme.
 * @copydoc MM_RootScanner
 * @ingroup GC_Modron_Standard
 */
class MM_CopyForwardSchemeRootClearer : public MM_RootScanner
{
private:
	MM_CopyForwardScheme *_copyForwardScheme;

private:
	virtual void doSlot(J9Object **slotPtr) {
		Assert_MM_unreachable();  /* Should not have gotten here - how do you clear a generic slot? */
	}

	virtual void doClass(J9Class *clazz) {
		Assert_MM_unreachable();  /* Should not have gotten here - how do you clear a class? */
	}

	virtual void scanSoftReferenceObjects(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_SoftReferenceObjects);
		_copyForwardScheme->scanSoftReferenceObjects(MM_EnvironmentVLHGC::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_SoftReferenceObjects);
	}
	
	virtual CompletePhaseCode scanSoftReferencesComplete(MM_EnvironmentBase *env) {
		/* do nothing -- no new objects could have been discovered by soft reference processing */
		return complete_phase_OK;
	}

	virtual void scanWeakReferenceObjects(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_WeakReferenceObjects);
		_copyForwardScheme->scanWeakReferenceObjects(MM_EnvironmentVLHGC::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_WeakReferenceObjects);
	}

	virtual CompletePhaseCode scanWeakReferencesComplete(MM_EnvironmentBase *env) {
		/* No new objects could have been discovered by soft / weak reference processing,
		 * but we must complete this phase prior to unfinalized processing to ensure that
		 * finalizable referents get cleared */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		return complete_phase_OK;
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void scanUnfinalizedObjects(MM_EnvironmentBase *env) {
		/* allow the scheme to handle this, since it knows which regions are interesting */
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
		_copyForwardScheme->scanUnfinalizedObjects(MM_EnvironmentVLHGC::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}

	virtual CompletePhaseCode scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_UnfinalizedObjectsComplete);
		/* ensure that all unfinalized processing is complete before we start marking additional objects */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

		bool wasAbortAlreadyInProgress = _copyForwardScheme->_abortInProgress;
		_copyForwardScheme->completeScan(MM_EnvironmentVLHGC::getEnvironment(env));
		
		if (!wasAbortAlreadyInProgress && _copyForwardScheme->_abortInProgress) {
			/* an abort occurred during unfinalized processing: there could be unscanned or unforwarded objects on the finalizable list */
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				/* since we know we're in abort handling mode and won't be copying any of these objects we don't need to synchronize here */
				_copyForwardScheme->scanFinalizableObjects(MM_EnvironmentVLHGC::getEnvironment(env));
			}
			_copyForwardScheme->completeScanForAbort(MM_EnvironmentVLHGC::getEnvironment(env));
		}
		reportScanningEnded(RootScannerEntity_UnfinalizedObjectsComplete);
		return complete_phase_OK;
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual void scanOwnableSynchronizerObjects(MM_EnvironmentBase *env) {
		/* allow the scheme to handle this, since it knows which regions are interesting */
		/* empty, move ownable synchronizer processing in copy-continuous phase */
	}

	virtual void scanContinuationObjects(MM_EnvironmentBase *env) {
		/* allow the scheme to handle this, since it knows which regions are interesting */
		reportScanningStarted(RootScannerEntity_ContinuationObjects);
		_copyForwardScheme->scanContinuationObjects(MM_EnvironmentVLHGC::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_ContinuationObjects);
	}

	virtual void iterateAllContinuationObjects(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_ContinuationObjectsComplete);
		MM_ContinuationObjectBufferVLHGC::iterateAllContinuationObjects(env);
		reportScanningEnded(RootScannerEntity_ContinuationObjectsComplete);
	}

	virtual void scanPhantomReferenceObjects(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_PhantomReferenceObjects);
		_copyForwardScheme->scanPhantomReferenceObjects(MM_EnvironmentVLHGC::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_PhantomReferenceObjects);
	}

	virtual CompletePhaseCode scanPhantomReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);

		reportScanningStarted(RootScannerEntity_PhantomReferenceObjectsComplete);
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		Assert_MM_true(MM_CycleState::references_clear_phantom == (env->_cycleState->_referenceObjectOptions & MM_CycleState::references_clear_phantom));

		/* phantom reference processing may resurrect objects - scan them now */
		_copyForwardScheme->completeScan(env);

		reportScanningEnded(RootScannerEntity_PhantomReferenceObjectsComplete);
		return complete_phase_OK;
	}

	virtual void doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator) {
		J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
		MM_EnvironmentVLHGC::getEnvironment(_env)->_copyForwardStats._monitorReferenceCandidates += 1;
		J9Object *objectPtr = (J9Object *)monitor->userData;
		if (!_copyForwardScheme->isLiveObject(objectPtr)) {
			Assert_MM_true(_copyForwardScheme->isObjectInEvacuateMemory(objectPtr));
			MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
			J9Object *forwardPtr = forwardedHeader.getForwardedObject();
			if (NULL != forwardPtr) {
				monitor->userData = (UDATA)forwardPtr;
			} else {
				Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedHeader));
				monitorReferenceIterator->removeSlot();
				MM_EnvironmentVLHGC::getEnvironment(_env)->_copyForwardStats._monitorReferenceCleared += 1;
				/* We must call objectMonitorDestroy (as opposed to omrthread_monitor_destroy) when the
				 * monitor is not internal to the GC
				 */
				static_cast<J9JavaVM*>(_omrVM->_language_vm)->internalVMFunctions->objectMonitorDestroy(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9VMThread *)_env->getLanguageVMThread(), (omrthread_monitor_t)monitor);
			}
		}
	}

	virtual CompletePhaseCode scanMonitorReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_MonitorReferenceObjectsComplete);
		((J9JavaVM *)env->getLanguageVM())->internalVMFunctions->objectMonitorDestroyComplete((J9JavaVM *)env->getLanguageVM(), (J9VMThread *)env->getLanguageVMThread());
		reportScanningEnded(RootScannerEntity_MonitorReferenceObjectsComplete);
		return complete_phase_OK;
	}

	virtual void doJNIWeakGlobalReference(J9Object **slotPtr) {
		J9Object *objectPtr = *slotPtr;
		if (!_copyForwardScheme->isLiveObject(objectPtr)) {
			Assert_MM_true(_copyForwardScheme->isObjectInEvacuateMemory(objectPtr));
			MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
			*slotPtr = forwardedHeader.getForwardedObject();
		}
	}

	virtual void doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator) {
		J9Object *objectPtr = *slotPtr;
		MM_EnvironmentVLHGC::getEnvironment(_env)->_copyForwardStats._stringConstantsCandidates += 1;
		if (!_copyForwardScheme->isLiveObject(objectPtr)) {
			Assert_MM_true(_copyForwardScheme->isObjectInEvacuateMemory(objectPtr));
			MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
			objectPtr = forwardedHeader.getForwardedObject();
			if (NULL == objectPtr) {
				Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedHeader));
				MM_EnvironmentVLHGC::getEnvironment(_env)->_copyForwardStats._stringConstantsCleared += 1;
				stringTableIterator->removeSlot();
			} else {
				*slotPtr = objectPtr;
			}
		}
	}

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	virtual void doDoubleMappedObjectSlot(J9Object *objectPtr, struct J9PortVmemIdentifier *identifier) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(_env);
		env->_copyForwardStats._doubleMappedArrayletsCandidates += 1;
		if (!_copyForwardScheme->isLiveObject(objectPtr)) {
			Assert_MM_true(_copyForwardScheme->isObjectInEvacuateMemory(objectPtr));
			MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
			objectPtr = forwardedHeader.getForwardedObject();
			if (NULL == objectPtr) {
				Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedHeader));
				env->_copyForwardStats._doubleMappedArrayletsCleared += 1;
				OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
				omrvmem_release_double_mapped_region(identifier->address, identifier->size, identifier);
			}
		}
	}
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */

	/**
	 * @Clear the string table cache slot if the object is not marked
	 */
	virtual void doStringCacheTableSlot(J9Object **slotPtr) {
		J9Object *objectPtr = *slotPtr;
		if (!_copyForwardScheme->isLiveObject(objectPtr)) {
			Assert_MM_true(_copyForwardScheme->isObjectInEvacuateMemory(objectPtr));
			MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
			*slotPtr = forwardedHeader.getForwardedObject();
		}
	}

#if defined(J9VM_OPT_JVMTI)
	virtual void doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
	{
		J9Object *objectPtr = *slotPtr;
		if (!_copyForwardScheme->isLiveObject(objectPtr)) {
			Assert_MM_true(_copyForwardScheme->isObjectInEvacuateMemory(objectPtr));
			MM_ForwardedHeader forwardedHeader(objectPtr, _extensions->compressObjectReferences());
			*slotPtr = forwardedHeader.getForwardedObject();
		}
	}
#endif /* J9VM_OPT_JVMTI */

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t object) {
		Assert_MM_unreachable();
	}
#endif /* J9VM_GC_FINALIZATION */

public:
	MM_CopyForwardSchemeRootClearer(MM_EnvironmentVLHGC *env, MM_CopyForwardScheme *copyForwardScheme) :
		MM_RootScanner(env),
		_copyForwardScheme(copyForwardScheme)
	{
		_typeId = __FUNCTION__;
	};
};

void
MM_CopyForwardScheme::clearMarkMapForPartialCollect(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	/* Walk the collection set to determine what ranges of the mark map should be cleared */
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_copyForwardData._evacuateSet) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				/* we start with an assumption that abort will occur, so we set _previousMarkMapCleared to false.
				 * if not, the region will be recycled, in which moment the flag will turn to true
				 */
				if (region->_previousMarkMapCleared) {
					region->_previousMarkMapCleared = false;
					if (_extensions->tarokEnableExpensiveAssertions) {
						Assert_MM_true(_markMap->checkBitsForRegion(env, region));
					}
				/* TODO: need to handle region->hasValidMarkMap() case for optimum performance */
				/* consider remembering where the last allocated object is, to minimize clearing for regions with low occupancy (indeed, regions with low occupancy are rather good candidates for evacuation). */
//				} else if (region->hasValidMarkMap()) {
				} else {
					_markMap->setBitsForRegion(env, region, true);
				}
			}
		}
	}
}

void
MM_CopyForwardScheme::clearCardTableForPartialCollect(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);
	bool gmpIsRunning = (NULL != env->_cycleState->_externalCycleState);

	if (gmpIsRunning) {
		/* Walk the collection set to determine what ranges of the mark map should be cleared */
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
		MM_CardTable *cardTable = _extensions->cardTable;
		while (NULL != (region = regionIterator.nextRegion())) {
			if (region->_copyForwardData._evacuateSet && !region->_markData._noEvacuation) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					void *low = region->getLowAddress();
					void *high = region->getHighAddress();
					Card *lowCard = cardTable->heapAddrToCardAddr(env, low);
					Card *highCard = cardTable->heapAddrToCardAddr(env, high);
					UDATA cardRangeSize = (UDATA)highCard - (UDATA)lowCard;
					memset(lowCard, CARD_CLEAN, cardRangeSize);
				}
			}
		}
	}
}

void
MM_CopyForwardScheme::workThreadGarbageCollect(MM_EnvironmentVLHGC *env)
{
	/* GC init (set up per-invocation values) */
	workerSetupForCopyForward(env);

	env->_workStack.prepareForWork(env, env->_cycleState->_workPackets);

	/* pre-populate the _reservedRegionList with the flushed regions */
	/* this is a simple operation, so do it in one GC thread */
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager, MM_HeapRegionDescriptor::MANAGED);
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			if (region->containsObjects()) {
				UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
				if (region->_markData._shouldMark) {
					_reservedRegionList[compactGroup]._evacuateRegionCount += 1;
				} else {
					Assert_MM_true(MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED == region->getRegionType());
					MM_MemoryPool *pool = region->getMemoryPool();
					/* only add regions with pools which could possibly satisfy a TLH allocation */
					if ((pool->getActualFreeMemorySize() >= pool->getMinimumFreeEntrySize()) &&
						((pool->getActualFreeMemorySize()/pool->getActualFreeEntryCount()) >= _extensions->freeSizeThresholdForSurvivor)
						) {
						Assert_MM_true(pool->getActualFreeMemorySize() < region->getSize());
						Assert_MM_false(region->isSurvivorRegion());
						insertFreeMemoryCandidate(env, &_reservedRegionList[compactGroup], region);
					}
				}
			}
		}
		
		/* initialize the maximum number of sublists for each compact group; ensure that we try to produce fewer survivor regions than evacuate regions */
		for (UDATA index = 0; index < _compactGroupMaxCount; index++) {
			UDATA evacuateCount = _reservedRegionList[index]._evacuateRegionCount;
			/* Arbitrarily set the max to half the evacuate count. This means that, if it's possible, we'll use no more than half as many survivor regions as there were evacuate regions */
			UDATA maxSublistCount = evacuateCount / 2;
			maxSublistCount = OMR_MAX(maxSublistCount, 1);
			maxSublistCount = OMR_MIN(maxSublistCount, MM_ReservedRegionListHeader::MAX_SUBLISTS);
			_reservedRegionList[index]._maxSublistCount = maxSublistCount;
		}
	}

	/* another thread clears the class loader remembered set */
	if (_extensions->tarokEnableIncrementalClassGC) {
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			MM_ClassLoaderRememberedSet *classLoaderRememberedSet = _extensions->classLoaderRememberedSet;
			classLoaderRememberedSet->resetRegionsToClear(env);
			MM_HeapRegionDescriptorVLHGC *region = NULL;
			GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
			while (NULL != (region = regionIterator.nextRegion())) {
				if (region->_markData._shouldMark) {
					classLoaderRememberedSet->prepareToClearRememberedSetForRegion(env, region);
				}
			}
			classLoaderRememberedSet->clearRememberedSets(env);
		}
	}


	/* We want to clear all out-going references from the nursery set since those regions
	 * will be walked and their precise out-going references will be used to reconstruct the RS
	 */
	_interRegionRememberedSet->clearFromRegionReferencesForCopyForward(env);

	clearMarkMapForPartialCollect(env);

	if (NULL != env->_cycleState->_externalCycleState) {
		rememberReferenceListsFromExternalCycle(env);
	}
	((MM_CopyForwardSchemeTask*)env->_currentTask)->synchronizeGCThreadsForInterRegionRememberedSet(env, UNIQUE_ID);

	/*  Enable dynamicBreadthFirstScanOrdering depth copying if dynamicBreadthFirstScanOrdering is enabled */
	env->enableHotFieldDepthCopy();
	
	/* scan roots before cleaning the card table since the roots give us more concrete NUMA recommendations */
	scanRoots(env);

	cleanCardTable(env);
	
	completeScan(env);

	/* TODO: check if abort happened during root scanning/cardTable clearing (and optimize in any other way) */
	if (abortFlagRaised()) {
		Assert_MM_true(_abortInProgress);
		/* rescan to fix up root slots, but also to complete scanning of roots that we miss to mark/push in original root scanning */
		scanRoots(env);

		cleanCardTable(env);
		
		completeScan(env);
	}
	/*  Disable dynamicBreadthFirstScanOrdering depth copying after root scanning and main phase of PGC cycle */
	env->disableHotFieldDepthCopy();

	/* ensure that all buffers have been flushed before we start reference processing */
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
	
	UDATA preservedGcReadBarrierType = 0;
	if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
		_clearableProcessingStarted = true;

		/* During clearable pass, GC threads can access clearable slots other than the one they are directly processing.
		 * Such other slots could still point to fowarded objects and forwarded pointer needs to be
		 * resolved (at least in thread local sense) to be able to access the object.
		 * An example of that is string comparator, that may be used when removing
		 * an entry from the string table, as part of AVL rebalancing.
		 * String comparator happens to be used also in  the context of mutator thread when adding new elements,
		 * and it already uses Read Barrier (to support concurrent evacuating GCs).
		 * That read barrier will do exactly what we need for our clearable pass (well it will do more,
		 * not just locally resolve FP, but even fix the slot, but it's correct for this pass, too). We just need
		 * to enable the RB, if not already enabled.
		 */
		preservedGcReadBarrierType = _javaVM->gcReadBarrierType;
		_javaVM->gcReadBarrierType = J9_GC_READ_BARRIER_TYPE_ALWAYS;

		/* Soft and weak references resurrected by finalization need to be cleared immediately since weak and soft processing has already completed.
		 * This has to be set before unfinalizable (and phantom) processing, because it can copy object to a non-fresh region, in which case we do
		 * not want to put GMP refs to REMEMBERED state (we want have a chance to put it back to INITIAL state).
		 */
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_soft;
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_weak;
		/* since we need a sync point here anyway, use this opportunity to determine which regions contain weak and soft references or unfinalized objects */
		/* (we can't do phantom references yet because unfinalized processing may find more of them) */
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
		while (NULL != (region = regionIterator.nextRegion())) {
			if (region->isSurvivorRegion() || region->_copyForwardData._evacuateSet) {
				region->getReferenceObjectList()->startSoftReferenceProcessing();
				region->getReferenceObjectList()->startWeakReferenceProcessing();
			}
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	MM_CopyForwardSchemeRootClearer rootClearer(env, this);
	rootClearer.setStringTableAsRoot(!isCollectStringConstantsEnabled());
	rootClearer.scanClearable(env);

	/* Clearable must not uncover any new work */
	Assert_MM_true(NULL == env->_workStack.popNoWait(env));

	if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
		_javaVM->gcReadBarrierType = preservedGcReadBarrierType;
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	if (!abortFlagRaised()) {
		clearCardTableForPartialCollect(env);
	}

	/* make sure that we aren't leaving any stale scan work behind */
	Assert_MM_false(isAnyScanCacheWorkAvailable());

	if (NULL != env->_cycleState->_externalCycleState) {
		updateOrDeleteObjectsFromExternalCycle(env);
	}

	env->_workStack.flush(env);
	/* flush the buffer after clearable phase --- cmvc 198798 */
	/* flush ownable synchronizer object buffer after rebuild the ownableSynchronizerObjectList during main scan phase */
	env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->flush(env);

	abandonTLHRemainders(env);

	/* No matter what happens, always sum up the gc stats */
	mergeGCStats(env);

	env->_copyForwardCompactGroups = NULL;

	return ;
}

void
MM_CopyForwardScheme::scanRoots(MM_EnvironmentVLHGC* env)
{
	MM_CopyForwardSchemeRootScanner rootScanner(env, this);
	rootScanner.setStringTableAsRoot(!isCollectStringConstantsEnabled());
	rootScanner.setClassDataAsRoots(!isDynamicClassUnloadingEnabled());
	rootScanner.setIncludeStackFrameClassReferences(isDynamicClassUnloadingEnabled());

	rootScanner.scanRoots(env);

	/* Mark root set classes */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (isDynamicClassUnloadingEnabled()) {
		/* A single thread processes all class loaders, marking any loader which has instances outside of the collection set. */
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			bool foundSystemClassLoader = false;
			bool foundApplicationClassLoader = false;
			bool foundAnonymousClassLoader = false;

			MM_ClassLoaderRememberedSet *classLoaderRememberedSet = _extensions->classLoaderRememberedSet;
			GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
			J9ClassLoader *classLoader = NULL;

			while (NULL != (classLoader = classLoaderIterator.nextSlot())) {
				if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
					if (OMR_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER)) {
						foundAnonymousClassLoader = true;
						/* Anonymous classloader should be scanned on level of classes every time */
						GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
						J9MemorySegment *segment = NULL;
						while (NULL != (segment = segmentIterator.nextSegment())) {
							GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
							J9Class *clazz = NULL;
							while (NULL != (clazz = classHeapIterator.nextClass())) {
								if (classLoaderRememberedSet->isClassRemembered(env, clazz)) {
									MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(clazz->classObject);
									copyAndForward(env, reservingContext, &clazz->classObject);
								}
							}
						}
					} else {
						if (classLoaderRememberedSet->isRemembered(env, classLoader)) {
							foundSystemClassLoader = foundSystemClassLoader || (classLoader == _javaVM->systemClassLoader);
							foundApplicationClassLoader = foundApplicationClassLoader || (classLoader == _javaVM->applicationClassLoader);
							if (NULL != classLoader->classLoaderObject) {
								/* until we decide if class loaders should be common, just relocate this object back into its existing node */
								MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(classLoader->classLoaderObject);
								copyAndForward(env, reservingContext, &classLoader->classLoaderObject);
							} else {
								/* Only system/app classloaders can have a null classloader object (only during early bootstrap) */
								Assert_MM_true((classLoader == _javaVM->systemClassLoader) || (classLoader == _javaVM->applicationClassLoader));

								/* We will never find the object for this class loader during scanning, so scan its class table immediately */
								GC_ClassLoaderClassesIterator iterator(_extensions, classLoader);
								J9Class *clazz = NULL;
								bool success = true;
	
								while (success && (NULL != (clazz = iterator.nextClass()))) {
									Assert_MM_true(NULL != clazz->classObject);
									MM_AllocationContextTarok *clazzContext = getContextForHeapAddress(clazz->classObject);
									/* Copy/Forward the slot reference*/
									success = copyAndForward(env, clazzContext, (J9Object **)&(clazz->classObject));
								}

								if (NULL != classLoader->moduleHashTable) {
									J9HashTableState walkState;
									J9Module **modulePtr = (J9Module **)hashTableStartDo(classLoader->moduleHashTable, &walkState);
									while (success && (NULL != modulePtr)) {
										J9Module * const module = *modulePtr;
										success = copyAndForward(env, getContextForHeapAddress(module->moduleObject), (J9Object **)&(module->moduleObject));
										if (success) {
											if (NULL != module->moduleName) {
												success = copyAndForward(env, getContextForHeapAddress(module->moduleName), (J9Object **)&(module->moduleName));
											}
										}
										if (success) {
											if (NULL != module->version) {
												success = copyAndForward(env, getContextForHeapAddress(module->version), (J9Object **)&(module->version));
											}
										}
										modulePtr = (J9Module**)hashTableNextDo(&walkState);
									}

									if (success && (classLoader == _javaVM->systemClassLoader)) {
										success = copyAndForward(env, getContextForHeapAddress(_javaVM->unamedModuleForSystemLoader->moduleObject), (J9Object **)&(_javaVM->unamedModuleForSystemLoader->moduleObject));
									}
								}
							}
						}
					}
				}
			}

			/* verify that we found the permanent class loaders in the above loop */
			Assert_MM_true(NULL != _javaVM->systemClassLoader);
			Assert_MM_true(foundSystemClassLoader);
			Assert_MM_true( (NULL == _javaVM->applicationClassLoader) || foundApplicationClassLoader );
			Assert_MM_true(NULL != _javaVM->anonClassLoader);
			Assert_MM_true(foundAnonymousClassLoader);
		}
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
}

void
MM_CopyForwardScheme::verifyDumpObjectDetails(MM_EnvironmentVLHGC *env, const char *title, J9Object *object)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	j9tty_printf(PORTLIB, "%s: %p\n", title, object);

	if (NULL != object) {
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(object);

		j9tty_printf(PORTLIB, "\tregion:%p base:%p top:%p regionProperties:%u\n",
				region,
				region->getLowAddress(),
				region->getHighAddress(),
				region->getRegionProperties()
				);

		j9tty_printf(PORTLIB, "\t\tbitSet:%c externalBitSet:%c shouldMark:%c initialLiveSet:%c survivorSet:%c freshSurvivorSet:%c age:%zu\n",
				_markMap->isBitSet(object) ? 'Y' : 'N',
				(NULL == env->_cycleState->_externalCycleState) ? 'N' : (env->_cycleState->_externalCycleState->_markMap->isBitSet(object) ? 'Y' : 'N'),
				region->_markData._shouldMark ? 'Y' : 'N',
				region->_copyForwardData._initialLiveSet ? 'Y' : 'N',
				region->isSurvivorRegion() ? 'Y' : 'N',
				region->isFreshSurvivorRegion() ? 'Y' : 'N',
				region->getLogicalAge()
		);
	}
}

class MM_CopyForwardVerifyScanner : public MM_RootScanner
{
public:
protected:
private:
	MM_CopyForwardScheme *_copyForwardScheme;  /**< Local reference back to the copy forward scheme driving the collection */

private:
	void verifyObject(J9Object **slotPtr)
	{
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(_env);

		J9Object *objectPtr = *slotPtr;
		if (!_copyForwardScheme->_abortInProgress && !_copyForwardScheme->isObjectInNoEvacuationRegions(env, objectPtr) && _copyForwardScheme->verifyIsPointerInEvacute(env, objectPtr)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Root slot points into evacuate!  Slot %p dstObj %p. RootScannerEntity=%zu\n", slotPtr, objectPtr, (UDATA)_scanningEntity);
			Assert_MM_unreachable();
		}
	}

	virtual void doSlot(J9Object **slotPtr) {
		verifyObject(slotPtr);
	}

	virtual void doStackSlot(J9Object **slotPtr, void *walkState, const void* stackLocation) {
		if (_copyForwardScheme->isHeapObject(*slotPtr)) {
			/* heap object - validate and mark */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::COULD_BE_FORWARDED, *slotPtr, stackLocation, walkState).validate(_env));
			verifyObject(slotPtr);
			Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ_THREAD(*slotPtr, ((J9StackWalkState*)walkState)->walkThread));
		} else if (NULL != *slotPtr) {
			/* stack object - just validate */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, *slotPtr, stackLocation, walkState).validate(_env));
		}
	}

	virtual void doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator) {
		if (_copyForwardScheme->isHeapObject(*slotPtr)) {
			verifyObject(slotPtr);
			Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ_THREAD(*slotPtr, vmThreadIterator->getVMThread()));
		} else if (NULL != *slotPtr) {
			Assert_MM_true(vmthreaditerator_state_monitor_records == vmThreadIterator->getState());
			Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ_THREAD(*slotPtr, vmThreadIterator->getVMThread()));
		}
	}

	virtual void doClass(J9Class *clazz) {
		J9Object *classObject = (J9Object *)clazz->classObject;
		if (NULL != classObject) {
			if (_copyForwardScheme->isDynamicClassUnloadingEnabled() && !_copyForwardScheme->isLiveObject(classObject)) {
				/* don't verify garbage collected classes */
			} else {
				_copyForwardScheme->verifyClassObjectSlots(MM_EnvironmentVLHGC::getEnvironment(_env), classObject);
			}
		}
	}
	
	virtual void doClassLoader(J9ClassLoader *classLoader) {
		J9Object *classLoaderObject = J9GC_J9CLASSLOADER_CLASSLOADEROBJECT(classLoader);
		if (NULL != classLoaderObject) {
			if (_copyForwardScheme->isDynamicClassUnloadingEnabled() && !_copyForwardScheme->isLiveObject(classLoaderObject)) {
				/* don't verify garbage collected class loaders */
			} else {
				verifyObject(J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(classLoader));
			}
		}
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void doUnfinalizedObject(J9Object *objectPtr, MM_UnfinalizedObjectList *list) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(_env);

		if (!_copyForwardScheme->_abortInProgress && !_copyForwardScheme->isObjectInNoEvacuationRegions(env, objectPtr) && _copyForwardScheme->verifyIsPointerInEvacute(env, objectPtr)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Unfinalized object list points into evacuate!  list %p object %p\n", list, objectPtr);
			Assert_MM_unreachable();
		}
	}
#endif /* J9VM_GC_FINALIZATION */

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t objectPtr) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(_env);

		if (!_copyForwardScheme->_abortInProgress && !_copyForwardScheme->isObjectInNoEvacuationRegions(env, objectPtr) && _copyForwardScheme->verifyIsPointerInEvacute(env, objectPtr)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Finalizable object in evacuate!  object %p\n", objectPtr);
			Assert_MM_unreachable();
		}
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual void doOwnableSynchronizerObject(J9Object *objectPtr, MM_OwnableSynchronizerObjectList *list) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(_env);

		if (!_copyForwardScheme->_abortInProgress && !_copyForwardScheme->isObjectInNoEvacuationRegions(env, objectPtr) && _copyForwardScheme->verifyIsPointerInEvacute(env, objectPtr)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "OwnableSynchronizer object list points into evacuate!  list %p object %p\n", list, objectPtr);
			Assert_MM_unreachable();
		}
	}

	virtual void doContinuationObject(J9Object *objectPtr, MM_ContinuationObjectList *list) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(_env);

		if (!_copyForwardScheme->_abortInProgress && !_copyForwardScheme->isObjectInNoEvacuationRegions(env, objectPtr) && _copyForwardScheme->verifyIsPointerInEvacute(env, objectPtr)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Continuation object list points into evacuate!  list %p object %p\n", list, objectPtr);
			Assert_MM_unreachable();
		}
	}

public:
	MM_CopyForwardVerifyScanner(MM_EnvironmentVLHGC *env, MM_CopyForwardScheme *copyForwardScheme) :
		MM_RootScanner(env, true),
		_copyForwardScheme(copyForwardScheme)
	{
		_typeId = __FUNCTION__;
	};

protected:
private:

};

void
MM_CopyForwardScheme::verifyCopyForwardResult(MM_EnvironmentVLHGC *env)
{
	/* Destination regions verifying their integrity */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->isArrayletLeaf()) {
			J9Object *spineObject = (J9Object *)region->_allocateData.getSpine();
			Assert_MM_true(NULL != spineObject);
			/* the spine must be marked if it was copied as a live object or if we aborted the copy-forward */
			/* otherwise, it must not be forwarded (since that would imply that the spine survived but the pointer wasn't updated) */
			if (!_markMap->isBitSet(spineObject)) {
				MM_ForwardedHeader forwardedSpine(spineObject, _extensions->compressObjectReferences());
				if (forwardedSpine.isForwardedPointer()) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Spine pointer is not marked and is forwarded (leaf region's pointer to spine not updated)!  Region %p Spine %p (should be %p)\n", region, spineObject, forwardedSpine.getForwardedObject());
					verifyDumpObjectDetails(env, "spineObject", spineObject);
					Assert_MM_unreachable();
				}
			}
		} else {
			if (region->containsObjects()) {
				if (region->isSurvivorRegion()) {
					if (region->isFreshSurvivorRegion()) {
						verifyChunkSlotsAndMapSlotsInRange(env, (UDATA *)region->getLowAddress(), (UDATA *)region->getHighAddress());
					} else {
						/* iterating from isCompressedSurvivor */
						GC_SurvivorMemoryIterator survivorIterator(env, region, _compressedSurvivorTable);
						while (survivorIterator.next()) {
							verifyChunkSlotsAndMapSlotsInRange(env, (UDATA *)survivorIterator.getCurrentLow(), (UDATA *)survivorIterator.getCurrentHigh());
						}
					}
				}

				if (region->_copyForwardData._initialLiveSet) {
					/* iterating from isNotCompressedSurvivor */
					GC_SurvivorMemoryIterator survivorIterator(env, region, _compressedSurvivorTable, false);
					while (survivorIterator.next()) {
						verifyObjectsInRange(env, (UDATA *)survivorIterator.getCurrentLow(), (UDATA *)survivorIterator.getCurrentHigh());
					}
				}
			}
		}
	}

	MM_CopyForwardVerifyScanner scanner(env, this);
	scanner.scanAllSlots(env);

	if (NULL != env->_cycleState->_externalCycleState) {
		verifyExternalState(env);
	}
}

void
MM_CopyForwardScheme::verifyObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr)
{
	J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, env);
	Assert_MM_mustBeClass(clazz);
	switch (_extensions->objectModel.getScanType(clazz)) {
	case GC_ObjectModel::SCAN_MIXED_OBJECT_LINKED:
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
	case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
		verifyMixedObjectSlots(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
		verifyClassObjectSlots(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		verifyClassLoaderObjectSlots(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		verifyPointerArrayObjectSlots(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		verifyReferenceObjectSlots(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		/* nothing to do */
		break;
	default:
		Assert_MM_unreachable();
	}
}

void
MM_CopyForwardScheme::verifyMixedObjectSlots(MM_EnvironmentVLHGC *env, J9Object *objectPtr)
{
	GC_MixedObjectIterator mixedObjectIterator(_javaVM->omrVM, objectPtr);
	GC_SlotObject *slotObject = NULL;

	while (NULL != (slotObject = mixedObjectIterator.nextSlot())) {
		J9Object *dstObject = slotObject->readReferenceFromSlot();
		if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Mixed object slot points to evacuate!  srcObj %p slot %p dstObj %p\n", objectPtr, slotObject->readAddressFromSlot(), dstObject);
			verifyDumpObjectDetails(env, "srcObj", objectPtr);
			verifyDumpObjectDetails(env, "dstObj", dstObject);
			Assert_MM_unreachable();
		}
		if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Mixed object slot points to unmarked object!  srcObj %p slot %p dstObj %p\n", objectPtr, slotObject->readAddressFromSlot(), dstObject);
			verifyDumpObjectDetails(env, "srcObj", objectPtr);
			verifyDumpObjectDetails(env, "dstObj", dstObject);
			Assert_MM_unreachable();
		}
	}
}

void
MM_CopyForwardScheme::verifyReferenceObjectSlots(MM_EnvironmentVLHGC *env, J9Object *objectPtr)
{
	fj9object_t referentToken = J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr);
	J9Object *referentPtr = _extensions->accessBarrier->convertPointerFromToken(referentToken);
	if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, referentPtr) && verifyIsPointerInEvacute(env, referentPtr)) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		j9tty_printf(PORTLIB, "RefMixed referent slot points to evacuate!  srcObj %p dstObj %p\n", objectPtr, referentPtr);
		Assert_MM_unreachable();
	}
	if ((NULL != referentPtr) && !_markMap->isBitSet(referentPtr)) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		j9tty_printf(PORTLIB, "RefMixed referent slot points to unmarked object!  srcObj %p dstObj %p\n", objectPtr, referentPtr);
		verifyDumpObjectDetails(env, "srcObj", objectPtr);
		verifyDumpObjectDetails(env, "referentPtr", referentPtr);
		Assert_MM_unreachable();
	}

	GC_MixedObjectIterator mixedObjectIterator(_javaVM->omrVM, objectPtr);
	GC_SlotObject *slotObject = NULL;

	while (NULL != (slotObject = mixedObjectIterator.nextSlot())) {
		J9Object *dstObject = slotObject->readReferenceFromSlot();
		if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "RefMixed object slot points to evacuate!  srcObj %p slot %p dstObj %p\n", objectPtr, slotObject->readAddressFromSlot(), dstObject);
			Assert_MM_unreachable();
		}
		if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "RefMixed object slot points to unmarked object!  srcObj %p slot %p dstObj %p\n", objectPtr, slotObject->readAddressFromSlot(), dstObject);
			verifyDumpObjectDetails(env, "srcObj", objectPtr);
			verifyDumpObjectDetails(env, "dstPtr", dstObject);
			Assert_MM_unreachable();
		}
	}
}

void
MM_CopyForwardScheme::verifyPointerArrayObjectSlots(MM_EnvironmentVLHGC *env, J9Object *objectPtr)
{
	GC_PointerArrayIterator pointerArrayIterator(_javaVM, objectPtr);
	GC_SlotObject *slotObject = NULL;

	while ((slotObject = pointerArrayIterator.nextSlot()) != NULL) {
		J9Object *dstObject = slotObject->readReferenceFromSlot();
		if(!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Pointer array slot points to evacuate!  srcObj %p slot %p dstObj %p\n", objectPtr, slotObject->readAddressFromSlot(), dstObject);
			Assert_MM_unreachable();
		}
		if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Pointer array slot points to unmarked object!  srcObj %p slot %p dstObj %p\n", objectPtr, slotObject->readAddressFromSlot(), dstObject);
			verifyDumpObjectDetails(env, "srcObj", objectPtr);
			verifyDumpObjectDetails(env, "dstObj", dstObject);
			Assert_MM_unreachable();
		}
	}
}

void
MM_CopyForwardScheme::verifyClassObjectSlots(MM_EnvironmentVLHGC *env, J9Object *classObject)
{
	verifyMixedObjectSlots(env, classObject);

	J9Class *classPtr = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), classObject);

	if (NULL != classPtr) {
		volatile j9object_t * slotPtr = NULL;

		do {
			/*
			 * scan static fields
			 */
			GC_ClassStaticsIterator classStaticsIterator(env, classPtr);
			while (NULL != (slotPtr = classStaticsIterator.nextSlot())) {
				J9Object *dstObject = *slotPtr;
				if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class static slot points to evacuate!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					Assert_MM_unreachable();
				}
				if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class static slot points to unmarked object!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					verifyDumpObjectDetails(env, "classObject", classObject);
					verifyDumpObjectDetails(env, "dstObj", dstObject);
					Assert_MM_unreachable();
				}
			}

			/*
			 * scan call sites
			 */
			GC_CallSitesIterator callSitesIterator(classPtr);
			while (NULL != (slotPtr = callSitesIterator.nextSlot())) {
				J9Object *dstObject = *slotPtr;
				if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class call site slot points to evacuate!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					Assert_MM_unreachable();
				}
				if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class call site slot points to unmarked object!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					verifyDumpObjectDetails(env, "classObject", classObject);
					verifyDumpObjectDetails(env, "dstObj", dstObject);
					Assert_MM_unreachable();
				}
			}

			/*
			 * scan MethodTypes
			 */
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
			GC_MethodTypesIterator methodTypesIterator(classPtr->romClass->invokeCacheCount, classPtr->invokeCache);
#else /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
			GC_MethodTypesIterator methodTypesIterator(classPtr->romClass->methodTypeCount, classPtr->methodTypes);
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

			while (NULL != (slotPtr = methodTypesIterator.nextSlot())) {
				J9Object *dstObject = *slotPtr;
				if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class MethodType slot points to evacuate!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					Assert_MM_unreachable();
				}
				if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class MethodType slot points to unmarked object!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					verifyDumpObjectDetails(env, "classObject", classObject);
					verifyDumpObjectDetails(env, "dstObj", dstObject);
					Assert_MM_unreachable();
				}
			}

			/*
			 * scan VarHandle MethodTypes
			 */
#if defined(J9VM_OPT_METHOD_HANDLE)
			GC_MethodTypesIterator varHandleMethodTypesIterator(classPtr->romClass->varHandleMethodTypeCount, classPtr->varHandleMethodTypes);
			while (NULL != (slotPtr = varHandleMethodTypesIterator.nextSlot())) {
				J9Object *dstObject = *slotPtr;
				if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class MethodType slot points to evacuate!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					Assert_MM_unreachable();
				}
				if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class MethodType slot points to unmarked object!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					verifyDumpObjectDetails(env, "classObject", classObject);
					verifyDumpObjectDetails(env, "dstObj", dstObject);
					Assert_MM_unreachable();
				}
			}
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

			/*
			 * scan constant pool objects
			 */
			/* we can safely ignore any classes referenced by the constant pool, since
			 * these are guaranteed to be referenced by our class loader
			 */
			GC_ConstantPoolObjectSlotIterator constantPoolIterator(_javaVM, classPtr);
			while (NULL != (slotPtr = constantPoolIterator.nextSlot())) {
				J9Object *dstObject = *slotPtr;
				if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, dstObject) && verifyIsPointerInEvacute(env, dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class CP slot points to evacuate!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					Assert_MM_unreachable();
				}
				if ((NULL != dstObject) && !_markMap->isBitSet(dstObject)) {
					PORT_ACCESS_FROM_ENVIRONMENT(env);
					j9tty_printf(PORTLIB, "Class CP slot points to unmarked object!  srcObj %p J9Class %p slot %p dstObj %p\n", classObject, classPtr, slotPtr, dstObject);
					verifyDumpObjectDetails(env, "classObject", classObject);
					verifyDumpObjectDetails(env, "dstObj", dstObject);
					Assert_MM_unreachable();
				}
			}
			classPtr = classPtr->replacedClass;
		} while (NULL != classPtr);
	}
}

void
MM_CopyForwardScheme::verifyClassLoaderObjectSlots(MM_EnvironmentVLHGC *env, J9Object *classLoaderObject)
{
	verifyMixedObjectSlots(env, classLoaderObject);

	J9ClassLoader *classLoader = J9VMJAVALANGCLASSLOADER_VMREF((J9VMThread*)env->getLanguageVMThread(), classLoaderObject);
	if ((NULL != classLoader) && (0 == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER))) {
		/* No lock is required because this only runs under exclusive access */
		/* (NULL == classLoader->classHashTable) is true ONLY for DEAD class loaders */
		Assert_MM_true(NULL != classLoader->classHashTable);
		GC_ClassLoaderClassesIterator iterator(_extensions, classLoader);
		J9Class *clazz = NULL;
		while (NULL != (clazz = iterator.nextClass())) {
			if (!_abortInProgress && !isObjectInNoEvacuationRegions(env, (J9Object *)clazz->classObject) && verifyIsPointerInEvacute(env, (J9Object *)clazz->classObject)) {
				PORT_ACCESS_FROM_ENVIRONMENT(env);
				j9tty_printf(PORTLIB, "Class loader table class object points to evacuate!  srcObj %p clazz %p clazzObj %p\n", classLoaderObject, clazz, clazz->classObject);
				Assert_MM_unreachable();
			}
			if ((NULL != clazz->classObject) && !_markMap->isBitSet((J9Object *)clazz->classObject)) {
				PORT_ACCESS_FROM_ENVIRONMENT(env);
				j9tty_printf(PORTLIB, "Class loader table class object points to unmarked object!  srcObj %p clazz %p clazzObj %p\n", classLoaderObject, clazz, clazz->classObject);
				verifyDumpObjectDetails(env, "classLoaderObject", classLoaderObject);
				verifyDumpObjectDetails(env, "classObject", (J9Object *)clazz->classObject);
				Assert_MM_unreachable();
			}
		}
	}
}

void
MM_CopyForwardScheme::verifyExternalState(MM_EnvironmentVLHGC *env)
{
	/* this function has knowledge of the collection set, which is only valid during a PGC */
	Assert_MM_true(NULL != env->_cycleState->_externalCycleState);

	MM_MarkMap *externalMarkMap = env->_cycleState->_externalCycleState->_markMap;
	Assert_MM_true(externalMarkMap != _markMap);

	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			if (region->_markData._shouldMark) {
				Assert_MM_true(region->_copyForwardData._initialLiveSet);

				if (_abortInProgress || region->_markData._noEvacuation) {
					MM_HeapMapIterator mapIterator(_extensions, externalMarkMap, (UDATA *)region->getLowAddress(), (UDATA *)region->getHighAddress(), false);
					J9Object *objectPtr = NULL;

					while (NULL != (objectPtr = mapIterator.nextObject())) {
						Assert_MM_true(_markMap->isBitSet(objectPtr));
					}
				} else {
					/* Evacuate space - make sure the GMP mark map is clear */
					UDATA lowIndex = externalMarkMap->getSlotIndex((J9Object *)region->getLowAddress());
					UDATA highIndex = externalMarkMap->getSlotIndex((J9Object *)region->getHighAddress());

					for (UDATA slotIndex = lowIndex; slotIndex < highIndex; slotIndex++) {
						Assert_MM_true(0 == externalMarkMap->getSlot(slotIndex));
					}
				}
			} else if (region->isSurvivorRegion()) {
				/* Survivor space - check that anything marked in the GMP map is also marked in the PGC map */
				if (region->isFreshSurvivorRegion()) {
					checkConsistencyGMPMapAndPGCMap(env, region, (UDATA *)region->getLowAddress(), (UDATA *)region->getHighAddress());
				} else {
					/* iterating from isCompressedSurvivor */
					GC_SurvivorMemoryIterator survivorIterator(env, region, _compressedSurvivorTable);
					while (survivorIterator.next()) {
						checkConsistencyGMPMapAndPGCMap(env, region, (UDATA *)survivorIterator.getCurrentLow(), (UDATA *)survivorIterator.getCurrentHigh());
					}
				}
			}
		}
	}

	/* Check that no object in the work packets appears in the evacuate space.
	 * If it appears in survivor, verify that both map bits are set.
	 */
	MM_WorkPacketsIterator packetIterator(env, env->_cycleState->_externalCycleState->_workPackets);
	MM_Packet *packet = NULL;
	while (NULL != (packet = packetIterator.nextPacket(env))) {
		if (!packet->isEmpty()) {
			/* there is data in this packet so use it */
			MM_PacketSlotIterator slotIterator(packet);
			J9Object **slot = NULL;
			while (NULL != (slot = slotIterator.nextSlot())) {
				J9Object *object = *slot;
				Assert_MM_true(NULL != object);
				if (PACKET_INVALID_OBJECT != (UDATA)object) {
					Assert_MM_false(!_abortInProgress && !isObjectInNoEvacuationRegions(env, object) && verifyIsPointerInEvacute(env, object));
					Assert_MM_true(!verifyIsPointerInSurvivor(env, object) || (_markMap->isBitSet(object) && externalMarkMap->isBitSet(object)));
				}
			}
		}
	}
}

bool
MM_CopyForwardScheme::verifyIsPointerInSurvivor(MM_EnvironmentVLHGC *env, J9Object *object)
{
	if (NULL == object) {
		return false;
	}
	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->physicalTableDescriptorForAddress(object);
	bool result = region->isFreshSurvivorRegion();
	if (!result && region->isSurvivorRegion()) {
		result = isCompressedSurvivor((void*)object);
	}
	return result;
}

bool
MM_CopyForwardScheme::verifyIsPointerInEvacute(MM_EnvironmentVLHGC *env, J9Object *object)
{
	if (NULL == object) {
		return false;
	}

	MM_HeapRegionDescriptorVLHGC *region = NULL;
	region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->physicalTableDescriptorForAddress(object);
	return region->_markData._shouldMark;
}

void
MM_CopyForwardScheme::verifyObjectsInRange(MM_EnvironmentVLHGC *env, UDATA *lowAddress, UDATA *highAddress)
{
	MM_HeapMapIterator iterator(_extensions, _markMap, lowAddress, highAddress, false);
	J9Object *objectPtr = NULL;
	while (NULL != (objectPtr = (iterator.nextObject()))) {
		verifyObject(env, objectPtr);
	}
}

void
MM_CopyForwardScheme::verifyChunkSlotsAndMapSlotsInRange(MM_EnvironmentVLHGC *env, UDATA *lowAddress, UDATA *highAddress)
{
	MM_HeapMapIterator mapIterator(_extensions, _markMap, lowAddress, highAddress, false);
	GC_ObjectHeapIteratorAddressOrderedList heapChunkIterator(_extensions, (J9Object *)lowAddress, (J9Object *)highAddress, false);
	J9Object *objectPtr = NULL;

	while (NULL != (objectPtr = heapChunkIterator.nextObject())) {
		J9Object *mapObjectPtr = mapIterator.nextObject();

		if (objectPtr != mapObjectPtr) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "ChunkIterator and mapIterator did not match up during walk of survivor space! ChunkSlot %p MapSlot %p\n", objectPtr, mapObjectPtr);
			Assert_MM_unreachable();
			break;
		}
		verifyObject(env, objectPtr);
	}
	if (NULL != mapIterator.nextObject()) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		j9tty_printf(PORTLIB, "Survivor space mapIterator did not end when the chunkIterator did!\n");
		Assert_MM_unreachable();
	}
}

void
MM_CopyForwardScheme:: cleanOverflowInRange(MM_EnvironmentVLHGC *env, UDATA *lowAddress, UDATA *highAddress)
{
	/* At this point, no copying should happen, so that reservingContext is irrelevant */
	MM_AllocationContextTarok *reservingContext = _commonContext;
	MM_HeapMapIterator objectIterator = MM_HeapMapIterator(MM_GCExtensions::getExtensions(env), env->_cycleState->_markMap, lowAddress, highAddress);

	J9Object *object = NULL;
	while (NULL != (object = objectIterator.nextObject())) {
		scanObject(env, reservingContext, object, SCAN_REASON_OVERFLOWED_REGION);
	}
}

void
MM_CopyForwardScheme::checkConsistencyGMPMapAndPGCMap(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region, UDATA *lowAddress, UDATA *highAddress)
{
	MM_MarkMap *externalMarkMap = env->_cycleState->_externalCycleState->_markMap;
	MM_HeapMapIterator mapIterator(_extensions, externalMarkMap, lowAddress, highAddress, false);
	J9Object *objectPtr = NULL;

	while (NULL != (objectPtr = mapIterator.nextObject())) {
		Assert_MM_true(_markMap->isBitSet(objectPtr));
		Assert_MM_true(objectPtr >= region->getLowAddress());
		Assert_MM_true(objectPtr < region->getHighAddress());
	}
}

void
MM_CopyForwardScheme::scanWeakReferenceObjects(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());
	
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if ((region->isSurvivorRegion() || region->_copyForwardData._evacuateSet) && !region->getReferenceObjectList()->wasWeakListEmpty()) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				processReferenceList(env, region, region->getReferenceObjectList()->getPriorWeakList(), &env->_copyForwardStats._weakReferenceStats);
			}
		}
	}
	
	/* processReferenceList() may have pushed remembered references back onto the buffer if a GMP is active */
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
}

void
MM_CopyForwardScheme::scanSoftReferenceObjects(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());

	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if ((region->isSurvivorRegion() || region->_copyForwardData._evacuateSet) && !region->getReferenceObjectList()->wasSoftListEmpty()) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				processReferenceList(env, region, region->getReferenceObjectList()->getPriorSoftList(), &env->_copyForwardStats._softReferenceStats);
			}
		}
	}

	/* processReferenceList() may have pushed remembered references back onto the buffer if a GMP is active */
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
}

void
MM_CopyForwardScheme::scanPhantomReferenceObjects(MM_EnvironmentVLHGC *env)
{
	/* unfinalized processing may discover more phantom reference objects */
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);

	if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
		Assert_MM_true(0 == _phantomReferenceRegionsToProcess);
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_phantom;
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
		while (NULL != (region = regionIterator.nextRegion())) {
			Assert_MM_true(region->getReferenceObjectList()->wasPhantomListEmpty());
			Assert_MM_false(region->_copyForwardData._requiresPhantomReferenceProcessing);
			if (region->isSurvivorRegion() || region->_copyForwardData._evacuateSet) {
				region->getReferenceObjectList()->startPhantomReferenceProcessing();
				if (!region->getReferenceObjectList()->wasPhantomListEmpty()) {
					region->_copyForwardData._requiresPhantomReferenceProcessing = true;
					_phantomReferenceRegionsToProcess += 1;
				}
			}
		}
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	UDATA phantomReferenceRegionsProcessed = 0;
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_copyForwardData._requiresPhantomReferenceProcessing) {
			Assert_MM_true(region->isSurvivorRegion() || region->_copyForwardData._evacuateSet);
			Assert_MM_false(region->getReferenceObjectList()->wasPhantomListEmpty());
			phantomReferenceRegionsProcessed += 1;
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				processReferenceList(env, region, region->getReferenceObjectList()->getPriorPhantomList(), &env->_copyForwardStats._phantomReferenceStats);
			}
		}
	}

	Assert_MM_true(_phantomReferenceRegionsToProcess == phantomReferenceRegionsProcessed);

	/* processReferenceList() may have pushed remembered references back onto the buffer if a GMP is active */
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
}

void
MM_CopyForwardScheme::processReferenceList(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region, J9Object *headOfList, MM_ReferenceStats *referenceStats)
{
	/* no list can possibly contain more reference objects than there are bytes in a region. */
	const UDATA maxObjects = _regionManager->getRegionSize();
	UDATA objectsVisited = 0;
	GC_FinalizableReferenceBuffer buffer(_extensions);
	bool const compressed = env->compressObjectReferences();

	J9Object *referenceObj = headOfList;
	while (NULL != referenceObj) {
		Assert_MM_true(isLiveObject(referenceObj));

		objectsVisited += 1;
		referenceStats->_candidates += 1;

		Assert_MM_true(region->isAddressInRegion(referenceObj));
		Assert_MM_true(objectsVisited < maxObjects);

		J9Object *nextReferenceObj = _extensions->accessBarrier->getReferenceLink(referenceObj);

		GC_SlotObject referentSlotObject(_extensions->getOmrVM(), J9GC_J9VMJAVALANGREFERENCE_REFERENT_ADDRESS(env, referenceObj));
		J9Object *referent = referentSlotObject.readReferenceFromSlot();
		if (NULL != referent) {
			UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(referenceObj, env)) & J9AccClassReferenceMask;
			
			/* update the referent if it's been forwarded */
			MM_ForwardedHeader forwardedReferent(referent, compressed);
			if (forwardedReferent.isForwardedPointer()) {
				referent = forwardedReferent.getForwardedObject();
				referentSlotObject.writeReferenceToSlot(referent);
			} else {
				Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedReferent));
			}
			
			if (isLiveObject(referent)) {
				if (J9AccClassReferenceSoft == referenceObjectType) {
					U_32 age = J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, referenceObj);
					if (age < _extensions->getMaxSoftReferenceAge()) {
						/* Soft reference hasn't aged sufficiently yet - increment the age */
						J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, referenceObj) = age + 1;
					}
				}
				_interRegionRememberedSet->rememberReferenceForMark(env, referenceObj, referent);
			} else {
				Assert_MM_true(isObjectInEvacuateMemory(referent));
				/* transition the state to cleared */
				I_32 previousState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj);
				Assert_MM_true((GC_ObjectModel::REF_STATE_INITIAL == previousState) || (GC_ObjectModel::REF_STATE_REMEMBERED == previousState));

				referenceStats->_cleared += 1;
				J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj) = GC_ObjectModel::REF_STATE_CLEARED;
				referentSlotObject.writeReferenceToSlot(NULL);

				/* Check if the reference has a queue */
				if (0 != J9GC_J9VMJAVALANGREFERENCE_QUEUE(env, referenceObj)) {
					/* Reference object can be enqueued onto the finalizable list */
					referenceStats->_enqueued += 1;
					buffer.add(env, referenceObj);
					env->_cycleState->_finalizationRequired = true;
				}
			}
		}

		switch (J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj)) {
		case GC_ObjectModel::REF_STATE_REMEMBERED:
			Assert_MM_true(NULL != env->_cycleState->_externalCycleState);
			/* This reference object was on a list of GMP reference objects at the start of the cycle. Restore it to its original condition. */
			J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj) = GC_ObjectModel::REF_STATE_INITIAL;
			env->getGCEnvironment()->_referenceObjectBuffer->add(env, referenceObj);
			break;
		case GC_ObjectModel::REF_STATE_CLEARED:
			break;
		case GC_ObjectModel::REF_STATE_INITIAL:
			/* if the object isn't in nursery space it should have been REMEMBERED */
			Assert_MM_true(isObjectInNurseryMemory(referenceObj));
			break;
		case GC_ObjectModel::REF_STATE_ENQUEUED:
			/* this object shouldn't have been on the list */
			Assert_MM_unreachable();
			break;
		default:
			Assert_MM_unreachable();
			break;
		}

		referenceObj = nextReferenceObj;
	}
	buffer.flush(env);
}

void
MM_CopyForwardScheme::rememberReferenceList(MM_EnvironmentVLHGC *env, J9Object *headOfList)
{
	Assert_MM_true((NULL == headOfList) || (NULL != env->_cycleState->_externalCycleState));
	/* If phantom reference processing has already started this list will never be processed */
	Assert_MM_true(0 == _phantomReferenceRegionsToProcess);

	J9Object *referenceObj = headOfList;
	while (NULL != referenceObj) {
		J9Object *next = _extensions->accessBarrier->getReferenceLink(referenceObj);
		I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj);
		switch (referenceState) {
		case  GC_ObjectModel::REF_STATE_INITIAL:
			/* The reference object was on a list of GMP reference objects at the start of the cycle. Remember this */
			J9GC_J9VMJAVALANGREFERENCE_STATE(env, referenceObj) = GC_ObjectModel::REF_STATE_REMEMBERED;
			if (!isObjectInEvacuateMemory(referenceObj)) {
				Assert_MM_true(_markMap->isBitSet(referenceObj));
				Assert_MM_true(!isObjectInNurseryMemory(referenceObj));
				env->getGCEnvironment()->_referenceObjectBuffer->add(env, referenceObj);
			}
			break;
		case GC_ObjectModel::REF_STATE_CLEARED:
			/* The reference object was cleared (probably by an explicit call to the clear() Java API).
			 * No need to remember it, since it's already in its terminal state.
			 */
			break;
		case GC_ObjectModel::REF_STATE_ENQUEUED:
			/* The reference object was enqueued. This could have happened either
			 * 1) during previous GC (+ finalization), in which case it has been removed from the list at GC time or
			 * 2) in Java through explicit enqueue(), in which case it may still be in the list
			 * Explicit enqueue() will clear reference queue field. So, if we still see it in the list, the queue must be null.
			 * This GC will rebuild the list, after which the reference must not be on the list anymore.			 * 
			 */
            Assert_MM_true(0 == J9GC_J9VMJAVALANGREFERENCE_QUEUE(env, referenceObj));
			break;
		case GC_ObjectModel::REF_STATE_REMEMBERED:
			/* The reference object must not already be remembered */
		default:
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "rememberReferenceList referenceObj=%p, referenceState=%zu\n", referenceObj, referenceState);
			Assert_MM_unreachable();
		}
		referenceObj = next;
	}
}

void
MM_CopyForwardScheme::rememberReferenceListsFromExternalCycle(MM_EnvironmentVLHGC *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_markData._shouldMark) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				rememberAndResetReferenceLists(env, region);
			}
		}
	}
}

void
MM_CopyForwardScheme::rememberAndResetReferenceLists(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
{
	MM_ReferenceObjectList *referenceObjectList = region->getReferenceObjectList();
	UDATA referenceObjectOptions = env->_cycleState->_referenceObjectOptions;

	if (0 == (referenceObjectOptions & MM_CycleState::references_clear_weak)) {
		referenceObjectList->startWeakReferenceProcessing();
		J9Object *headOfList = referenceObjectList->getPriorWeakList();
		if (NULL != headOfList) {
			Trc_MM_CopyForwardScheme_rememberAndResetReferenceLists_rememberWeak(env->getLanguageVMThread(), region, headOfList);
			rememberReferenceList(env, headOfList);
		}
	}

	if (0 == (referenceObjectOptions & MM_CycleState::references_clear_soft)) {
		referenceObjectList->startSoftReferenceProcessing();
		J9Object *headOfList = referenceObjectList->getPriorSoftList();
		if (NULL != headOfList) {
			Trc_MM_CopyForwardScheme_rememberAndResetReferenceLists_rememberSoft(env->getLanguageVMThread(), region, headOfList);
			rememberReferenceList(env, headOfList);
		}
	}

	if (0 == (referenceObjectOptions & MM_CycleState::references_clear_phantom)) {
		referenceObjectList->startPhantomReferenceProcessing();
		J9Object *headOfList = referenceObjectList->getPriorPhantomList();
		if (NULL != headOfList) {
			Trc_MM_CopyForwardScheme_rememberAndResetReferenceLists_rememberPhantom(env->getLanguageVMThread(), region, headOfList);
			rememberReferenceList(env, headOfList);
		}
	}

	referenceObjectList->resetPriorLists();
}

#if defined(J9VM_GC_FINALIZATION)
void
MM_CopyForwardScheme::scanFinalizableObjects(MM_EnvironmentVLHGC *env)
{
	GC_FinalizeListManager * finalizeListManager = _extensions->finalizeListManager;

	/* If we're copying objects this code must be run single-threaded and we should only be here if work is actually required */
	/* This function is also used during abort; these assertions aren't applicable to that case because objects can't be copied during abort */
	Assert_MM_true(_abortInProgress || env->_currentTask->isSynchronized());
	Assert_MM_true(_abortInProgress || _shouldScanFinalizableObjects);
	Assert_MM_true(_abortInProgress || finalizeListManager->isFinalizableObjectProcessingRequired());

	/* walk finalizable objects loaded by the system class loader */
	j9object_t systemObject = finalizeListManager->resetSystemFinalizableObjects();
	if (NULL != systemObject) {
		scanFinalizableList(env, systemObject);
	}

	/* walk finalizable objects loaded by the all other class loaders */
	j9object_t defaultObject = finalizeListManager->resetDefaultFinalizableObjects();
	if (NULL != defaultObject) {
		scanFinalizableList(env, defaultObject);
	}



	{
		/* walk reference objects */
		GC_FinalizableReferenceBuffer referenceBuffer(_extensions);
		j9object_t referenceObject = finalizeListManager->resetReferenceObjects();
		while (NULL != referenceObject) {
			j9object_t next = NULL;
			if (!isLiveObject(referenceObject)) {
				Assert_MM_true(isObjectInEvacuateMemory(referenceObject));
				MM_ForwardedHeader forwardedHeader(referenceObject, _extensions->compressObjectReferences());
				if (!forwardedHeader.isForwardedPointer()) {
					Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedHeader));
					next = _extensions->accessBarrier->getReferenceLink(referenceObject);

					MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(referenceObject);
					J9Object *copyObject = copy(env, reservingContext, &forwardedHeader);
					if ( (NULL == copyObject) || (referenceObject == copyObject) ) {
						referenceBuffer.add(env, referenceObject);
					} else {
						/* It's only safe to copy objects on the finalizable list if we're in single threaded mode */
						Assert_MM_true(!_abortInProgress);
						referenceBuffer.add(env, copyObject);
					}
				} else {
					J9Object *forwardedPtr =  forwardedHeader.getForwardedObject();
					Assert_MM_true(NULL != forwardedPtr);
					next = _extensions->accessBarrier->getReferenceLink(forwardedPtr);
					referenceBuffer.add(env, forwardedPtr);
				}
			} else {
				next = _extensions->accessBarrier->getReferenceLink(referenceObject);
				referenceBuffer.add(env, referenceObject);
			}

			referenceObject = next;
		}
		referenceBuffer.flush(env);
	}
}

void
MM_CopyForwardScheme::scanFinalizableList(MM_EnvironmentVLHGC *env, j9object_t headObject)
{
	GC_FinalizableObjectBuffer objectBuffer(_extensions);

	while (NULL != headObject) {
		j9object_t next = NULL;

		if (!isLiveObject(headObject)) {
			Assert_MM_true(isObjectInEvacuateMemory(headObject));
			MM_ForwardedHeader forwardedHeader(headObject, _extensions->compressObjectReferences());
			if (!forwardedHeader.isForwardedPointer()) {
				Assert_MM_mustBeClass(_extensions->objectModel.getPreservedClass(&forwardedHeader));
				next = _extensions->accessBarrier->getFinalizeLink(headObject);

				MM_AllocationContextTarok *reservingContext = getContextForHeapAddress(headObject);
				J9Object *copyObject = copy(env, reservingContext, &forwardedHeader);
				if ( (NULL == copyObject) || (headObject == copyObject) ) {
					objectBuffer.add(env, headObject);
				} else {
					/* It's only safe to copy objects on the finalizable list if we're in single threaded mode */
					Assert_MM_true(!_abortInProgress);
					objectBuffer.add(env, copyObject);
				}
			} else {
				J9Object *forwardedPtr =  forwardedHeader.getForwardedObject();
				Assert_MM_true(NULL != forwardedPtr);
				next = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);
				objectBuffer.add(env, forwardedPtr);
			}
		} else {
			next = _extensions->accessBarrier->getFinalizeLink(headObject);
			objectBuffer.add(env, headObject);
		}

		headObject = next;
	}

	objectBuffer.flush(env);
}
#endif /* J9VM_GC_FINALIZATION */

void 
MM_CopyForwardScheme::removeFreeMemoryCandidate(MM_EnvironmentVLHGC* env, MM_ReservedRegionListHeader* regionList, MM_HeapRegionDescriptorVLHGC *region)
{
	Assert_MM_true(NULL != regionList->_freeMemoryCandidates);
	Assert_MM_true(0 < regionList->_freeMemoryCandidateCount);

	regionList->_freeMemoryCandidateCount -= 1;

	MM_HeapRegionDescriptorVLHGC *next = region->_copyForwardData._nextRegion;
	MM_HeapRegionDescriptorVLHGC *previous = region->_copyForwardData._previousRegion;
	if (NULL != next) {
		next->_copyForwardData._previousRegion = previous;
	}
	if (NULL != previous) {
		previous->_copyForwardData._nextRegion = next;
		Assert_MM_true(previous != previous->_copyForwardData._nextRegion);
	} else {
		Assert_MM_true(region == regionList->_freeMemoryCandidates);
		regionList->_freeMemoryCandidates = next;
	}
}

void
MM_CopyForwardScheme::insertFreeMemoryCandidate(MM_EnvironmentVLHGC* env, MM_ReservedRegionListHeader* regionList, MM_HeapRegionDescriptorVLHGC *region)
{
	region->_copyForwardData._nextRegion = regionList->_freeMemoryCandidates;
	region->_copyForwardData._previousRegion = NULL;
	if (NULL != regionList->_freeMemoryCandidates) {
		regionList->_freeMemoryCandidates->_copyForwardData._previousRegion = region;
	}
	regionList->_freeMemoryCandidates = region;
	regionList->_freeMemoryCandidateCount += 1;
}

void
MM_CopyForwardScheme::convertFreeMemoryCandidateToSurvivorRegion(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region)
{
	Trc_MM_CopyForwardScheme_convertFreeMemoryCandidateToSurvivorRegion_Entry(env->getLanguageVMThread(), region);
	Assert_MM_true(NULL != region);
	Assert_MM_true(MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED == region->getRegionType());
	Assert_MM_false(region->isSurvivorRegion());
	Assert_MM_false(region->isFreshSurvivorRegion());

	setRegionAsSurvivor(env, region, false);

	/* TODO: Remembering does not really have to be done under a lock, but dual (prev, current) list implementation indirectly forces us to do it this way. */
	rememberAndResetReferenceLists(env, region);

	Trc_MM_CopyForwardScheme_convertFreeMemoryCandidateToSurvivorRegion_Exit(env->getLanguageVMThread());
}

void
MM_CopyForwardScheme::setRegionAsSurvivor(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region, bool freshSurvivor)
{
	UDATA usedBytes = region->getSize() - region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();

	/* convert allocation age into (usedBytes * age) multiple. it will be converted back to pure age at the end of GC.
	 * in the mean time as caches are allocated from the region, the age will be merged
	 */
	double allocationAgeSizeProduct = (double)usedBytes * (double)region->getAllocationAge();

	Trc_MM_CopyForwardScheme_setRegionAsSurvivor(env->getLanguageVMThread(), _regionManager->mapDescriptorToRegionTableIndex(region), MM_CompactGroupManager::getCompactGroupNumber(env, region),
	    (double)region->getAllocationAge() / (1024 * 1024), (double)usedBytes / (1024 * 1024), allocationAgeSizeProduct / (1024 * 1024) / (1024 * 1024));

	Assert_MM_true(0.0 == region->getAllocationAgeSizeProduct());
	region->setAllocationAgeSizeProduct(allocationAgeSizeProduct);
	if (freshSurvivor) {
		region->resetAgeBounds();
	}

	/* update the pool so it only knows about the free memory occurring before survivor base.  We will add whatever we don't use at the end of the copy-forward */
	Assert_MM_false(region->_copyForwardData._requiresPhantomReferenceProcessing);
	region->_copyForwardData._survivor = true;
	region->_copyForwardData._freshSurvivor = freshSurvivor;
}

void
MM_CopyForwardScheme::setAllocationAgeForMergedRegion(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region)
{
	UDATA compactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
	UDATA usedBytes = region->getSize() - region->getMemoryPool()->getFreeMemoryAndDarkMatterBytes();

	Assert_MM_true(0 != usedBytes);

	/* convert allocation age product (usedBytes * age) back to pure age */
	U_64 newAllocationAge = (U_64)(region->getAllocationAgeSizeProduct() / (double)usedBytes);

	Trc_MM_CopyForwardScheme_setAllocationAgeForMergedRegion(env->getLanguageVMThread(), _regionManager->mapDescriptorToRegionTableIndex(region), compactGroup,
	    region->getAllocationAgeSizeProduct() / (1024 * 1024) / (1024 * 1024), (double)usedBytes / (1024 * 1024), (double)newAllocationAge / (1024 * 1024),
	    (double)region->getLowerAgeBound() / (1024 * 1024), (double)region->getUpperAgeBound() / (1024 * 1024));

	if (_extensions->tarokAllocationAgeEnabled) {
		Assert_MM_true(newAllocationAge < _extensions->compactGroupPersistentStats[compactGroup]._maxAllocationAge);
		Assert_MM_true((MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup) == 0) || (newAllocationAge >= _extensions->compactGroupPersistentStats[compactGroup - 1]._maxAllocationAge));
	}

	UDATA logicalAge = 0;
	if (_extensions->tarokAllocationAgeEnabled) {
		logicalAge = MM_CompactGroupManager::calculateLogicalAgeForRegion(env, newAllocationAge);
	} else {
		logicalAge = MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup);
	}

	region->setAge(newAllocationAge, logicalAge);
	/* reset aging auxiliary datea for future usage */
	region->setAllocationAgeSizeProduct(0.0);

}

bool
MM_CopyForwardScheme::isObjectInNoEvacuationRegions(MM_EnvironmentVLHGC *env, J9Object *objectPtr)
{
	if ((NULL == objectPtr) || (0 == _regionCountCannotBeEvacuated)) {
		return false;
	}
	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(objectPtr);
	return region->_markData._noEvacuation;
}

bool
MM_CopyForwardScheme::randomDecideForceNonEvacuatedRegion(UDATA ratio) {
	bool ret = false;
	if ((0 < ratio) && (ratio <= 100)) {
		ret = ((UDATA)(rand() % 100) <= (UDATA)(ratio - 1));
	}
	return ret;
}

MMINLINE bool
MM_CopyForwardScheme::isCompressedSurvivor(void *heapAddr)
{
	UDATA compressedCardOffset = ((UDATA)heapAddr - (UDATA)_heapBase) / CARD_SIZE;
	UDATA compressedCardIndex = compressedCardOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA compressedSurvivorWord = _compressedSurvivorTable[compressedCardIndex];
	bool isSurvivor = false;

	if (AllCompressedCardsInWordClean != compressedSurvivorWord) {
		UDATA bit = compressedCardOffset % COMPRESSED_CARDS_PER_WORD;
		isSurvivor = (CompressedCardSurvivor == ((compressedSurvivorWord >> bit) & 1));
	}
	return isSurvivor;
}

/**
 * compressedSurvivorTable bit to Card Table, it is for identifying if live object is in survivor memory in current PGC
 *  setCompressedSurvivorCards() are called for requiring free memory from region and preparing preserved TLHRemainders.
 */
MMINLINE void
MM_CopyForwardScheme::setCompressedSurvivorCards(MM_EnvironmentVLHGC *env, void *startHeapAddress, void *endHeapAddress)
{
	UDATA compressedCardStartOffset = ((UDATA)startHeapAddress - (UDATA)_heapBase) / CARD_SIZE;
	UDATA compressedCardStartIndex = compressedCardStartOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA compressedCardEndOffset = (((UDATA)endHeapAddress - (UDATA)_heapBase) + (CARD_SIZE -1))/ CARD_SIZE;
	UDATA compressedCardEndIndex = compressedCardEndOffset / COMPRESSED_CARDS_PER_WORD;
	UDATA mask = 1;
	UDATA endOfWord = ((UDATA)1) << (COMPRESSED_CARDS_PER_WORD - 1);
	UDATA compressedSurvivorWord = AllCompressedCardsInWordClean;

	UDATA *compressedSurvivor = &_compressedSurvivorTable[compressedCardStartIndex];

	UDATA shiftStart = compressedCardStartOffset % COMPRESSED_CARDS_PER_WORD;
	mask = mask << shiftStart;
	UDATA offset = compressedCardStartOffset;
	UDATA idx = compressedCardStartIndex;
	if (idx == compressedCardEndIndex) {
		endOfWord = ((UDATA)1) << ((compressedCardEndOffset - 1) % COMPRESSED_CARDS_PER_WORD);
	}
	while (offset < compressedCardEndOffset) {
		/* invert bit */
		compressedSurvivorWord ^= mask;

		if (mask == endOfWord) {
			/*only first and last word need atomic update, non-atomic for intermediate ones */
			if ((idx != compressedCardStartIndex) && (idx != compressedCardEndIndex)) {
				*compressedSurvivor |= compressedSurvivorWord;
			} else {
				/* atomic update */
				volatile UDATA *localAddr = compressedSurvivor;
				UDATA oldValue = *localAddr;
				UDATA newValue = oldValue | compressedSurvivorWord;
				if (newValue != oldValue) {
					while ((MM_AtomicOperations::lockCompareExchange(localAddr, oldValue, newValue)) != oldValue) {
						oldValue = *localAddr;
						newValue = oldValue | compressedSurvivorWord;
					}
				}
			}
			compressedSurvivor++;
			compressedSurvivorWord = AllCompressedCardsInWordClean;
			idx += 1;
			if (idx == compressedCardEndIndex) {
				endOfWord = ((UDATA)1) << ((compressedCardEndOffset - 1) % COMPRESSED_CARDS_PER_WORD);
			}
			mask = 1;
		} else {
			/* mask for next bit to handle */
			mask = mask << 1;
		}
		offset += 1;
	}
}

MMINLINE void
MM_CopyForwardScheme::cleanCompressedSurvivorCardTable(MM_EnvironmentVLHGC *env)
{
	UDATA compressedSurvivorTableSize = _extensions->heap->getMaximumPhysicalRange() / (CARD_SIZE * BITS_PER_BYTE);
	memset((void*)_compressedSurvivorTable, AllCompressedCardsInByteClean, compressedSurvivorTableSize);
}

void
MM_CopyForwardScheme::abandonTLHRemainders(MM_EnvironmentVLHGC *env)
{
	for (UDATA compactGroup = 0; compactGroup < _compactGroupMaxCount; compactGroup++) {
		MM_CopyForwardCompactGroup *copyForwardCompactGroup = &env->_copyForwardCompactGroups[compactGroup];
		if (_extensions->recycleRemainders) {
			if ((MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroup) >= _extensions->tarokNurseryMaxAge._valueSpecified) &&
				(copyForwardCompactGroup->getTLHRemainderSize() >= _extensions->minimumFreeSizeForSurvivor)) {
				copyForwardCompactGroup->recycleTLHRemainder(env);
			} else {
				copyForwardCompactGroup->discardTLHRemainder(env);
			}
		} else {
			copyForwardCompactGroup->discardTLHRemainder(env);
		}
	}
}
