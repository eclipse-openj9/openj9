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

#include <stdlib.h>

#include <stdio.h>
#include <limits.h> // or <climits> for CHAR_BIT
#include <string.h> // memcpy

#include "j9cfg.h"
#include "j9.h"

#include "ModronAssertions.h"
#include "AllocateDescription.hpp"
#include "AllocationContextTarok.hpp"
#include "ArrayletLeafIterator.hpp"
#include "AtomicOperations.hpp"
#include "Bits.hpp"
#include "CardListFlushTask.hpp"
#include "CardTable.hpp"
#include "ClassIterator.hpp"
#include "ClassHeapIterator.hpp"
#include "ClassLoaderClassesIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderRememberedSet.hpp"
#include "CompactGroupManager.hpp"
#include "ContinuationObjectBuffer.hpp"
#include "ContinuationObjectList.hpp"
#include "VMHelpers.hpp"
#include "WriteOnceCompactor.hpp"
#include "Debug.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizeListManager.hpp"
#endif /* J9VM_GC_FINALIZATION*/
#include "WriteOnceFixupCardCleaner.hpp"
#include "Heap.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapMapIterator.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapStats.hpp"
#include "InterRegionRememberedSet.hpp"
#include "MarkMap.hpp"
#include "ModronTypes.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MixedObjectIterator.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectModel.hpp"
#include "ParallelDispatcher.hpp"
#include "PacketSlotIterator.hpp"
#include "PointerArrayIterator.hpp"
#include "PointerArrayletInlineLeafIterator.hpp"
#include "RememberedSetCardListCardIterator.hpp"
#include "RootScanner.hpp"
#include "SlotObject.hpp"
#include "SublistPool.hpp"
#include "SublistPuddle.hpp"
#include "ParallelTask.hpp"
#include "VMThreadListIterator.hpp"
#include "VMThreadIterator.hpp"
#include "WorkPacketsIterator.hpp"
#include "WorkPacketsVLHGC.hpp"


#if defined(J9VM_GC_MODRON_COMPACTION)

void
MM_ParallelWriteOnceCompactTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	_compactScheme->compact(env);
}

void
MM_ParallelWriteOnceCompactTask::setup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	if (!env->isMainThread()) {
		env->_cycleState = _cycleState;
	}
	env->_compactVLHGCStats.clear();
}

void
MM_ParallelWriteOnceCompactTask::cleanup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);

	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._compactStats.merge(&env->_compactVLHGCStats);
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._irrsStats.merge(&env->_irrsStats);

	if (!env->isMainThread()) {
		env->_cycleState = NULL;
	}

	env->_lastOverflowedRsclWithReleasedBuffers = NULL;
}

void
MM_ParallelWriteOnceCompactTask::mainSetup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	_compactScheme->setCycleState(_cycleState, _nextMarkMap);
	_compactScheme->mainSetupForGC(env);
#if defined(DEBUG)
	_compactScheme->verifyHeap(env, true);
#endif /* DEBUG */

}

void
MM_ParallelWriteOnceCompactTask::mainCleanup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);

	_compactScheme->clearCycleState();

	MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->resetOverflowedList();
}

MM_WriteOnceCompactor::MM_WriteOnceCompactor(MM_EnvironmentVLHGC *env)
	: _javaVM((J9JavaVM *)env->getLanguageVM())
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _heap(_extensions->getHeap())
	, _dispatcher(_extensions->dispatcher)
	, _regionManager(_heap->getHeapRegionManager())
	, _heapBase(_heap->getHeapBase())
	, _heapTop(_heap->getHeapTop())
	, _cycleState()
	, _nextMarkMap(NULL)
	, _interRegionRememberedSet(NULL)
	, _workListMonitor(NULL)
	, _readyWorkList(NULL)
	, _readyWorkListHighPriority(NULL)
	, _fixupOnlyWorkList(NULL)
	, _rebuildWorkList(NULL)
	, _rebuildWorkListHighPriority(NULL)
	, _threadsWaiting(0)
	, _moveFinished(false)
	, _rebuildFinished(false)
	, _lockCount(0)
	, _compactGroupDestinations(NULL)
	, _regionSize(_extensions->regionSize)
	, _objectAlignmentInBytes(env->getObjectAlignmentInBytes())
{
}

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_WriteOnceCompactor *
MM_WriteOnceCompactor::newInstance(MM_EnvironmentVLHGC *env)
{
	MM_WriteOnceCompactor *compactScheme = (MM_WriteOnceCompactor *)env->getForge()->allocate(sizeof(MM_WriteOnceCompactor), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != compactScheme) {
		new(compactScheme) MM_WriteOnceCompactor(env);
		if (!compactScheme->initialize(env)) {
			compactScheme->kill(env);
			compactScheme = NULL;
		}
	}

	return compactScheme;
}

bool 
MM_WriteOnceCompactor::initialize(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	_interRegionRememberedSet = _extensions->interRegionRememberedSet;
	bool monitorInitialized = (0 == omrthread_monitor_init_with_name(&_workListMonitor, 0, "MM_WriteOnceCompactor::_workListMonitor"));
	bool didAllocateTable = false;
	if (monitorInitialized) {
		UDATA compactGroups = MM_CompactGroupManager::getCompactGroupMaxCount(env);
		_lockCount = compactGroups;
		_compactGroupDestinations = (MM_CompactGroupDestinations *)j9mem_allocate_memory(sizeof(MM_CompactGroupDestinations) * compactGroups, OMRMEM_CATEGORY_MM);
		didAllocateTable = (NULL != _compactGroupDestinations);
		if (didAllocateTable) {
			memset((void *)_compactGroupDestinations, 0, sizeof(MM_CompactGroupDestinations) * compactGroups);
			for (UDATA i = 0; i < compactGroups; i++) {
				_compactGroupDestinations[i].head = NULL;
				_compactGroupDestinations[i].tail = NULL;
				didAllocateTable = didAllocateTable && _compactGroupDestinations[i].lock.initialize(env, &_extensions->lnrlOptions, "MM_WriteOnceCompactor:_compactGroupDestinations[].lock");
			}
		}
	}

	return monitorInitialized && didAllocateTable;
}

void 
MM_WriteOnceCompactor::tearDown(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	if (NULL != _workListMonitor) {
		omrthread_monitor_destroy(_workListMonitor);
		_workListMonitor = NULL;
	}

	if (NULL != _compactGroupDestinations) {
		UDATA compactGroups = MM_CompactGroupManager::getCompactGroupMaxCount(env);
		Assert_MM_true(_lockCount == compactGroups);
		for (UDATA i = 0; i < _lockCount; i++) {
			_compactGroupDestinations[i].lock.tearDown();
		}
		j9mem_free_memory(_compactGroupDestinations);
		_compactGroupDestinations = NULL;
	}
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_WriteOnceCompactor::kill(MM_EnvironmentVLHGC *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

class WriteOnceCompactTableEntry {
private:
	J9Object *_destinationAddress;	/**< The base address of the contiguous area to which the objects in this page are moving. */
	UDATA _growBits;	/**< A bit map of objects in this page which need to grow by a slot when moving (support for new hash) */

private:
	/**
	 * Calculates the offset of the given objectPtr into its _growBits for setting or getting its grow flag.
	 * @param objectPtr[in] The object for which we want to request the growbit offset
	 * @return The bit shift offset for the given objectPtr in the _growBits
	 */
	UDATA
	growBitOffset(J9Object* objectPtr) const
	{
		/**
	 	* Return the bit number in Compressed Mark Map slot responsible for this object
	 	* Each bit in Compressed Mark Map represents twice more heap bytes then regular mark map
	 	* Each page represents number of bytes in heap might be covered by one Compressed Mark Map slot (UDATA)
	 	*/
	 	/* offset in page */
	    UDATA offsetInPage = (UDATA)objectPtr % MM_WriteOnceCompactor::sizeof_page;
	    /* Heap bytes per one bit of Compressed Mark Map */
	    UDATA unitSize = 2 * J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT;
		UDATA growOffset = offsetInPage / unitSize;
		Assert_MM_true(growOffset < (sizeof(UDATA) * BITS_IN_BYTE));
		return growOffset;
	}

public:
	void
	initialize(J9Object* addr)
	{
		Assert_MM_false(0x1 == ((UDATA)addr & 0x1));
		_destinationAddress = (J9Object *)addr;
	}

	/**
	 * Returns the destination address of the first moving object in this page.
	 * @return The address to which the first moving object in this page must be relocated
	 */
	J9Object*
	getAddr()
	{
		/* if the low bit is set, this is a size and not a destination address so just return NULL */
		J9Object *address = NULL;
		if (0x1 != ((UDATA)_destinationAddress & 0x1)) {
			address = _destinationAddress;
		}
		return address;
	}

	/**
	 * Sets the estimated post-move size of this page.  Note that this will assert if we attempt to over-write any existing size or destination address.
	 * @param size The estimated post-move size of this page, in bytes
	 */
	void
	setEstimatedSize(UDATA size)
	{
		Assert_MM_true(NULL == _destinationAddress);
		Assert_MM_false(0x1 == (size & 0x1));
		_destinationAddress = (J9Object *)(size | 0x1);
	}

	/**
	 * Retrieves the stored estimated post-move size of this page.  Note that this will assert if the size has been replaced with a destination address.
	 * @return The estimated post-move size of this page, in bytes
	 */
	UDATA
	getEstimatedSize()
	{
		Assert_MM_true((0x1 == ((UDATA)_destinationAddress & 0x1)) || (NULL == _destinationAddress));
		return (UDATA)_destinationAddress & ~0x1;
	}

	/**
	 * Retrieves the value of the growbit for the given objectPtr.
	 * @param objectPtr[in] The object for which we are retrieving the growbit
	 * @return The value of the growbit
	 */
	bool
	isGrowBitSet(J9Object *objectPtr)
	{
		UDATA offset = growBitOffset(objectPtr);
		return 0 != (_growBits & ((UDATA)1 << offset));
	}
	
	/**
	 * Counts the number of growbits in the parameter object's page which occur before it.
	 * @param object[in] The object for which we are retrieving the count of preceding growbits
	 * @return The number of growbits which have a post-move effect on the given object
	 */
	UDATA
	growBitsInfluencingObject(J9Object *object)
	{
		UDATA offset = growBitOffset(object);
		UDATA mask = ((UDATA)1 << offset) - 1;
		UDATA word = _growBits & mask;
		return MM_Bits::populationCount(word);
	}

	/**
	 * Sets the growbit for the given objectPtr.
	 * @param objectPtr[in] The object for which we are setting the growbit
	 */
	void
	setGrowBit(J9Object *objectPtr)
	{
		UDATA offset = growBitOffset(objectPtr);
		Assert_MM_false(isGrowBitSet(objectPtr));
		_growBits |= ((UDATA)1 << offset);
	}

	/**
	 * Clears the growbit for a given objectPtr.
	 * @param objectPtr[in] The object for which we are clearing the growbit
	 */
	void
	clearGrowBit(J9Object *objectPtr)
	{
		UDATA offset = growBitOffset(objectPtr);
		Assert_MM_true(isGrowBitSet(objectPtr));
		_growBits &= ~((UDATA)1 << offset);
	}

	/**
	 * Reverts the table entry back to an unused state.  This is called in evacuation if we setup forwarding and grow data for some objects in
	 * a page and then determine we are not able to evacuate the page.
	 */
	void
	revert()
	{
		_destinationAddress = NULL;
		_growBits = 0;
	}
};

/**
 * Stores information regarding the old and new locations of a specific object which has moved.
 */
typedef struct J9MM_FixupTuple
{
	J9Object *_oldPointer;	/**< The old object location */
	J9Object *_newPointer;	/**< The new (or "fixed up") location of the same object */

	J9MM_FixupTuple()
		: _oldPointer(NULL)
		, _newPointer(NULL)
	{}
} J9MM_FixupTuple;

/**
 * A cache of J9MM_FixupTuples used by getForwardWrapper to avoid fixup resolution, where possible
 */
typedef struct J9MM_FixupCache
{
	/* Note that this cache could be made more generic than just remembering the fixup data for the previous and next objects but
	 * these names make it easier to understand how we are presently using the cache.
	 */
	J9MM_FixupTuple _previousObject;	/**< The object before the one currently being fixed up */
	J9MM_FixupTuple _nextObject;	/**< The object after the one currently being fixed up */

	J9MM_FixupCache()
		: _previousObject()
		, _nextObject()
	{}
} J9MM_FixupCache;

MMINLINE J9Object *
MM_WriteOnceCompactor::getForwardWrapper(MM_EnvironmentVLHGC *env, J9Object *objectPtr, J9MM_FixupCache *cache)
{
	J9Object *forwarded = NULL;
	if (NULL != cache) {
		if (cache->_previousObject._oldPointer == objectPtr) {
			forwarded = cache->_previousObject._newPointer;
		} else if (cache->_nextObject._oldPointer == objectPtr) {
			forwarded = cache->_nextObject._newPointer;
		}
	}
	if (NULL == forwarded) {
		forwarded = getForwardingPtr(objectPtr);
	}
	return forwarded;
}

void
MM_WriteOnceCompactor::setCycleState(MM_CycleState *cycleState, MM_MarkMap *nextMarkMap)
{
	_cycleState = *cycleState;
	_nextMarkMap = nextMarkMap;
	Assert_MM_true(_cycleState._markMap != _nextMarkMap);
}

void
MM_WriteOnceCompactor::clearCycleState()
{
	_cycleState = MM_CycleStateVLHGC();
	_nextMarkMap = NULL;
}

void
MM_WriteOnceCompactor::mainSetupForGC(MM_EnvironmentVLHGC *env)
{
	_compactTable = (WriteOnceCompactTableEntry*)_nextMarkMap->getMarkBits();
	/* the move work stack is a simple structure shared by all threads so set it up prior to going multi-threaded */
	setupMoveWorkStack(env);
	/* clear the destinations for compact groups in case there was any stale data from the last compact */
	for (UDATA i = 0; i < MM_CompactGroupManager::getCompactGroupMaxCount(env); i++) {
		_compactGroupDestinations[i].head = NULL;
		_compactGroupDestinations[i].tail = NULL;
	}
}

void
MM_WriteOnceCompactor::initRegionCompactDataForCompactSet(MM_EnvironmentVLHGC *env)
{
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldCompact) {
			void *lowAddress = region->getLowAddress();
			region->_compactData._compactDestination = NULL;
			region->_compactData._nextEvacuationCandidate = lowAddress;
			region->_compactData._nextRebuildCandidate = lowAddress;
			region->_compactData._nextMoveEventCandidate = lowAddress;
			region->_compactData._blockedList = NULL;
			region->getUnfinalizedObjectList()->startUnfinalizedProcessing();
			region->getOwnableSynchronizerObjectList()->startOwnableSynchronizerProcessing();
			region->getContinuationObjectList()->startProcessing();
			
			/* clear all reference lists in compacted regions, since the GMP will need to rediscover all of these objects */
			region->getReferenceObjectList()->startWeakReferenceProcessing();
			region->getReferenceObjectList()->startSoftReferenceProcessing();
			region->getReferenceObjectList()->startPhantomReferenceProcessing();
			region->getReferenceObjectList()->resetPriorLists();
		}
	}
}

void
MM_WriteOnceCompactor::compact(MM_EnvironmentVLHGC *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA objectCount = 0;
	UDATA byteCount = 0;
	UDATA skippedObjectCount = 0;
	UDATA fixupObjectsCount = 0;

	/* use a temp to store the updated time to avoid redundant calls or misleading assignments */
	U_64 timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._setupStartTime = timeTemp;
	env->_compactVLHGCStats._flushStartTime = timeTemp;
	env->_compactVLHGCStats._flushEndTime = timeTemp;

	if (MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType) {
		env->_compactVLHGCStats._flushStartTime = j9time_hires_clock();
		if (NULL != env->_cycleState->_externalCycleState) {
			/* this must be done before the next mark map is cleared */
			rememberClassLoaders(env);
		}
		flushRememberedSetIntoCardTable(env);
		env->_compactVLHGCStats._flushEndTime = j9time_hires_clock();
		/* flushing modifies the Card Table and arraylet leaf tagging reads the table */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
	}

	env->_compactVLHGCStats._leafTaggingStartTime = j9time_hires_clock();
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* tagging arraylet leaf regions can't be usefully parallelized so run in a GC thread */
		tagArrayletLeafRegionsForFixup(env);
	}
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._leafTaggingEndTime = timeTemp;

	env->_compactVLHGCStats._regionCompactDataInitStartTime = timeTemp;
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* initialize _compactData fields before planning starts */
		initRegionCompactDataForCompactSet(env);
	}
	if (_extensions->tarokEnableIncrementalClassGC) {
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			/* clear the class loader remembered sets for regions we'll be rebuilding */
			clearClassLoaderRememberedSetsForCompactSet(env);
		}
	}
	
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._regionCompactDataInitEndTime = timeTemp;

	/* we need to clear the next mark map which underlies the regions we are going to compact since we need to store relocation data there */
	env->_compactVLHGCStats._clearMarkMapStartTime = timeTemp;
	clearMarkMapCompactSet(env, _nextMarkMap);
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._clearMarkMapEndTime = timeTemp;

	/* Clear all outgoing references from any regions which will be compacted.
	 * The fix up stage will rebuild the Remembered Set.
	 */
	env->_compactVLHGCStats._rememberedSetClearingStartTime = timeTemp;
	env->_compactVLHGCStats._rememberedSetClearingEndTime = timeTemp;
	_interRegionRememberedSet->clearFromRegionReferencesForCompact(env);
	env->_compactVLHGCStats._rememberedSetClearingEndTime = j9time_hires_clock();
	/* tail-marking and planning will currently be handled as part of setup but we may want more specific timers in the future */
	/* first, tail-mark the regions in the collection set */
	/* synchronize here so that initialization finishes before tail marking starts (once tail marking is part of planning, this sync point can be re-evaluated) */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
	env->_compactVLHGCStats._planningStartTime = j9time_hires_clock();
	planCompaction(env, &objectCount, &byteCount, &skippedObjectCount);
	env->_compactVLHGCStats._planningEndTime = j9time_hires_clock();
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._setupEndTime = timeTemp;

	env->_compactVLHGCStats._moveStartTime = timeTemp;
	moveObjects(env);
	env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->flush(env);
	/* Note:  moveObjects implicitly synchronizes threads */
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._moveEndTime = timeTemp;

	env->_compactVLHGCStats._fixupStartTime = timeTemp;
	/* TODO:  Change statistics since fixup is no longer completely distinct from move */
	fixupArrayletLeafRegionContentsAndObjectLists(env);
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._fixupEndTime = timeTemp;

	env->_compactVLHGCStats._rootFixupStartTime = timeTemp;
	fixupRoots(env);
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._rootFixupEndTime = timeTemp;

	/* fixup in-progress mark map and work stack.  The mark map fixup must be done by one thread (since it must be low-to-high,
	 * to avoid fixing up twice).  The work packets can be fixed up in parallel, however, since each packet is logically
	 * independent of the others.
	 * In order to better balance the cost of mark map fixup, the first thread in will handle the entire mark map while the
	 * other threads fixup the work packets.
	 */
	MM_CycleState *stateToFixup = _cycleState._externalCycleState;
	env->_compactVLHGCStats._fixupExternalPacketsStartTime = timeTemp;
	if (NULL != stateToFixup) {
		MM_WorkPacketsVLHGC *packets = stateToFixup->_workPackets;
		fixupExternalWorkPackets(env, packets);
	}
	env->_compactVLHGCStats._fixupExternalPacketsEndTime = j9time_hires_clock();
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

	env->_compactVLHGCStats._fixupArrayletLeafStartTime = j9time_hires_clock();

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* now, correct any leaf pages to point at their moved spines - this must be called AFTER (aka:  post-sync) we are sure to have finished fixing up leaf contents since it relies on knowing exactly which version of the spine pointer it sees */
		fixupArrayletLeafRegionSpinePointers(env);
	}

	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._fixupArrayletLeafEndTime = timeTemp;
	env->_compactVLHGCStats._recycleStartTime = timeTemp;
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* see if any regions were completely emptied by the compaction and return them to their parent pool as free
		 * (recycling is single-threaded since it modifies a linked list in the parent memory pool and rebuilding the free
		 * list is not enough work to justify the overhead of parallel synchronization)
		 */
		recycleFreeRegionsAndFixFreeLists(env);
	}
	env->_compactVLHGCStats._recycleEndTime = j9time_hires_clock();

	/* sync because we are about to rebuild the mark maps so the heap must be walkable and we can no longer use the fixup data */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

	/* by this point, fixup is complete so we can begin rebuilding both mark maps */
	env->_compactVLHGCStats._rebuildMarkBitsStartTime = j9time_hires_clock();
	rebuildMarkbits(env);
	env->_compactVLHGCStats._rebuildMarkBitsEndTime = j9time_hires_clock();
	/* rebuilding the mark bits requires that we can use the forwarding data stored in the next mark map so synchronize before clearMarkMapCompactSet destroys this data */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
	env->_compactVLHGCStats._finalClearNextMarkMapStartTime = j9time_hires_clock();
	clearMarkMapCompactSet(env, _nextMarkMap);
	timeTemp = j9time_hires_clock();
	env->_compactVLHGCStats._finalClearNextMarkMapEndTime = timeTemp;
	if (NULL != stateToFixup) {
		MM_WorkPacketsVLHGC *packets = stateToFixup->_workPackets;
		MM_MarkMap *markMap = stateToFixup->_markMap;
		/* sync because we are about to rebuild the next mark map which requires that we have finished clearing it (clearMarkMapCompactSet) */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		env->_compactVLHGCStats._rebuildNextMarkMapStartTime = j9time_hires_clock();
		rebuildNextMarkMapFromPackets(env, packets, markMap);
		rebuildNextMarkMapFromClassLoaders(env);
		env->_compactVLHGCStats._rebuildNextMarkMapEndTime = j9time_hires_clock();
	} else {
		env->_compactVLHGCStats._rebuildNextMarkMapStartTime = timeTemp;
		env->_compactVLHGCStats._rebuildNextMarkMapEndTime = timeTemp;
	}

	env->_compactVLHGCStats._movedObjects = objectCount;
	env->_compactVLHGCStats._movedBytes = byteCount;
	env->_compactVLHGCStats._fixupObjects = fixupObjectsCount;
}

void
MM_WriteOnceCompactor::fixupExternalWorkPackets(MM_EnvironmentVLHGC *env, MM_WorkPacketsVLHGC *packets)
{
	MM_WorkPacketsIterator packetIterator(env, packets);
	MM_Packet *packet = NULL;
	while (NULL != (packet = packetIterator.nextPacket(env))) {
		if (!packet->isEmpty()) {
			/* there is data in this packet so use it */
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_PacketSlotIterator slotIterator(packet);
				J9Object **slot = NULL;
				while (NULL != (slot = slotIterator.nextSlot())) {
					J9Object *pointer = *slot;
					if (PACKET_INVALID_OBJECT != (UDATA)pointer) {
						MM_HeapRegionDescriptorVLHGC *region  = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(pointer);
						if (region->_compactData._shouldCompact) {
							J9Object *newPointer = getForwardingPtr(pointer);
							if (newPointer != pointer) {
								*slot = newPointer;
							}
							slotIterator.resetSplitTagIndexForObject(newPointer, 0 | PACKET_ARRAY_SPLIT_TAG);
						} else {
							Assert_MM_true(_nextMarkMap->isBitSet(pointer));
						}
					}
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::rebuildNextMarkMapFromPackets(MM_EnvironmentVLHGC *env, MM_WorkPacketsVLHGC *packets, MM_MarkMap *markMap)
{
	MM_WorkPacketsIterator packetIterator = MM_WorkPacketsIterator(env, packets);
	MM_Packet *packet = packetIterator.nextPacket(env);
	while (NULL != packet) {
		if (!packet->isEmpty()) {
			/* there is data in this packet so use it */
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				MM_PacketSlotIterator slotIterator = MM_PacketSlotIterator(packet);
				J9Object **slot = slotIterator.nextSlot();
				while (NULL != slot) {
					J9Object *pointer = *slot;
					if (PACKET_INVALID_OBJECT != (UDATA)pointer) {
						MM_HeapRegionDescriptorVLHGC *region  = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(pointer);
						Assert_MM_true(region->containsObjects());
						Assert_MM_true(_cycleState._markMap->isBitSet(pointer));
						Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(pointer, env));
						if (region->_compactData._shouldCompact) {
							_nextMarkMap->atomicSetBit(pointer);
							Assert_MM_false(region->_nextMarkMapCleared);
						} else {
							Assert_MM_true(_nextMarkMap->isBitSet(pointer));
						}
					}
					slot = slotIterator.nextSlot();
				}
			}
		}
		packet = packetIterator.nextPacket(env);
	}
}

void
MM_WriteOnceCompactor::evacuatePage(MM_EnvironmentVLHGC *env, void *page, J9MM_FixupTuple *previousTwoObjects, J9Object **newLocationPtr)
{
	J9Object *newLocation = *newLocationPtr;
	J9MM_FixupTuple oldestObject = previousTwoObjects[0];
	J9MM_FixupTuple oldObject = previousTwoObjects[1];
	bool skipTail = false;
	for (UDATA bias = 0; bias < sizeof_page; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
		void *evacuateBase = (void *)((UDATA)page + bias);
		MM_HeapMapWordIterator markedObjectIterator(_cycleState._markMap, evacuateBase);
		if (skipTail) {
			markedObjectIterator.nextObject();
			skipTail = false;
		}
		J9Object *objectPtr = markedObjectIterator.nextObject();
		if ((NULL == newLocation) && (NULL != objectPtr)) {
			newLocation = getForwardingPtr(objectPtr);
		}
		while (NULL != objectPtr) {
			Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(objectPtr, env));
			/* objectPtr is a head so see if there is a tail in this mark map word and skip it (if not, remember that we need to skip the first bit in the next word) */
			if (NULL == markedObjectIterator.nextObject()) {
				skipTail = true;
			}
			J9Object *nextObject = markedObjectIterator.nextObject();
			UDATA objectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
			J9Object *nextLocation = (J9Object *)((UDATA)newLocation + objectSize);
			if (newLocation != objectPtr) {
				UDATA objectSizeAfterMove = 0;
				preObjectMove(env, objectPtr, &objectSizeAfterMove);

				/* copy objectPtr to newLocation */
				memmove(newLocation, objectPtr, objectSize);

				postObjectMove(env, newLocation, objectPtr);
				nextLocation = (J9Object *)((UDATA)newLocation + objectSizeAfterMove);
			}
			/* fixup this object's fields, whether it moved or not */
			J9Object *toFixup = oldObject._newPointer;
			J9MM_FixupTuple nextFixupTuple;
			nextFixupTuple._oldPointer = objectPtr;
			nextFixupTuple._newPointer = newLocation;
			if (NULL != toFixup) {
				J9MM_FixupCache cache;
				cache._previousObject = oldestObject;
				cache._nextObject = nextFixupTuple;
				fixupObject(env, toFixup, &cache);
			}
			oldestObject = oldObject;
			oldObject = nextFixupTuple;
			/* on to the next object */
			objectPtr = nextObject;
			newLocation = nextLocation;
		}
	}
	*newLocationPtr = newLocation;
	previousTwoObjects[0] = oldestObject;
	previousTwoObjects[1] = oldObject;
}

UDATA
MM_WriteOnceCompactor::movedPageSize(MM_EnvironmentVLHGC *env, void *page)
{
	J9Object *newLocation = NULL;
	UDATA totalMovedSize = 0;
	bool skipTail = false;
	for (UDATA bias = 0; bias < sizeof_page; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
		void *subPageBase = (void *)((UDATA)page + bias);
		MM_HeapMapWordIterator markedObjectIterator(_cycleState._markMap, subPageBase);
		if (skipTail) {
			markedObjectIterator.nextObject();
			skipTail = false;
		}
		J9Object *objectPtr = markedObjectIterator.nextObject();
		if ((NULL == newLocation) && (NULL != objectPtr)) {
			newLocation = getForwardingPtr(objectPtr);
		}
		while (NULL != objectPtr) {
			Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(objectPtr, env));
			/* skip tail */
			if (NULL == markedObjectIterator.nextObject()) {
				skipTail = true;
			}
			J9Object *nextObject = markedObjectIterator.nextObject();
			UDATA objectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
			J9Object *nextLocation = (J9Object *)((UDATA)newLocation + objectSize);
			if (newLocation != objectPtr) {
				UDATA objectSizeAfterMove = _extensions->objectModel.getConsumedSizeInBytesWithHeaderForMove(objectPtr);
				totalMovedSize += objectSizeAfterMove;
			} else {
				totalMovedSize += objectSize;
			}
			/* on to the next object */
			objectPtr = nextObject;
			newLocation = nextLocation;
		}
	}
	return totalMovedSize;
}

void
MM_WriteOnceCompactor::fixupNonMovingPage(MM_EnvironmentVLHGC *env, void *page)
{
	bool skipTail = false;
	for (UDATA bias = 0; bias < sizeof_page; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
		void *subPageBase = (void *)((UDATA)page + bias);
		MM_HeapMapWordIterator markedObjectIterator(_cycleState._markMap, subPageBase);
		if (skipTail) {
			markedObjectIterator.nextObject();
			skipTail = false;
		}
		J9Object *objectPtr = NULL;
		while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
			Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(objectPtr, env));
			/* skip tail */
			if (NULL == markedObjectIterator.nextObject()) {
				skipTail = true;
			}
			fixupObject(env, objectPtr, NULL);
		}
	}
}

MMINLINE void
MM_WriteOnceCompactor::trailingObjectFixup(MM_EnvironmentVLHGC *env, J9MM_FixupTuple *previousObjects)
{
	J9Object *fixup = previousObjects[1]._newPointer;
	previousObjects[1] = J9MM_FixupTuple();
	J9MM_FixupCache cache;
	cache._previousObject = previousObjects[0];
	if (NULL != fixup) {
		fixupObject(env, fixup, &cache);
	}
	previousObjects[0] = J9MM_FixupTuple();
}

void
MM_WriteOnceCompactor::moveObjects(MM_EnvironmentVLHGC *env)
{
	MM_CardTable *cardTable = _extensions->cardTable;
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = popWork(env))) {
		if (region->_compactData._shouldCompact) {
			void *startAddress = region->_compactData._nextEvacuationCandidate;
			/* assume that this is page aligned - this could be changed in the future but it simplifies things, for now */
			Assert_MM_true(0 == ((UDATA)startAddress & (sizeof_page-1)));
			void *high = region->getHighAddress();
			Assert_MM_true(startAddress < high);
			void *page = startAddress;
			void *earlyExit = NULL;
			void *evacuationTarget = NULL;
			UDATA evacuationSize = 0;
			MM_HeapRegionDescriptorVLHGC *existingTargetRegion = NULL;
			/* since we typically work on one destination region (existingTargetRegion), cache the high address and update it only when the destination region changes */
			void *targetRegionHighAddress = NULL;
			void *endOfEvacuatedTarget = NULL;
			J9MM_FixupTuple previousObjects[2];
			previousObjects[0] = J9MM_FixupTuple();
			previousObjects[1] = J9MM_FixupTuple();
			J9Object *newLocation = NULL;
			for (page = startAddress; (NULL == earlyExit) && (page < high); page = (void *)((UDATA)page + sizeof_page)) {
				/* ensure that we can evacuate this page */
				IDATA evacuateIndex = pageIndex((J9Object *)page);
				void *target = _compactTable[evacuateIndex].getAddr();
				if (NULL != target) {
					MM_HeapRegionDescriptorVLHGC *newTargetRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(target);
					if (existingTargetRegion != newTargetRegion) {
						trailingObjectFixup(env, previousObjects);
						newLocation = NULL;
						existingTargetRegion = newTargetRegion;
						targetRegionHighAddress = existingTargetRegion->getHighAddress();
						endOfEvacuatedTarget = existingTargetRegion->_compactData._nextEvacuationCandidate;
					}
					if (region == existingTargetRegion) {
						/* sliding within the region */
						evacuatePage(env, page, previousObjects, &newLocation);
					} else {
						/* for now, we will state that there should be two pages at the target (although we could go tighter than this, so long as it is enough to account for object growth) */
						UDATA movedSize = movedPageSize(env, page);
						void *postMoveEnd = (void *)((UDATA)target + movedSize);
						/* moving these objects into the target region must not overflow it */
						Assert_MM_true(postMoveEnd <= targetRegionHighAddress);
						if ((postMoveEnd <= endOfEvacuatedTarget) || (targetRegionHighAddress == endOfEvacuatedTarget)) {
							/* do the evacuation */
							evacuatePage(env, page, previousObjects, &newLocation);
						} else {
							evacuationTarget = target;
							evacuationSize = movedSize;
							earlyExit = page;
						}
					}
				} else {
					/* nothing in this page moved so just fixup the objects where they are */
					fixupNonMovingPage(env, page);
					trailingObjectFixup(env, previousObjects);
					newLocation = NULL;
				}
			}
			trailingObjectFixup(env, previousObjects);
			region->_compactData._nextEvacuationCandidate = (NULL == earlyExit) ? page : earlyExit;
			pushMoveWork(env, region, evacuationTarget, evacuationSize);
			/* since we just moved some objects in this region, clear the card table under them since they are now invalid */
			void *endOfExtent = (NULL == earlyExit) ? high : earlyExit;
			Card *base = cardTable->heapAddrToCardAddr(env, startAddress);
			Card *top = cardTable->heapAddrToCardAddr(env, endOfExtent);
			memset(base, CARD_CLEAN, (UDATA)top - (UDATA)base);
		} else if ((MM_CycleState::CT_GLOBAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType) && (region->_criticalRegionsInUse > 0)) {
			/* in the case of a global collection, mark will have avoided updating the RSCL but this region has pinned objects so the entire region must be walked for fixup */
			void *lowAddress = region->getLowAddress();
			void *highAddress = region->getHighAddress();
			for (void *cardToScan = lowAddress; cardToScan < highAddress; cardToScan = (void *)((UDATA)cardToScan + CARD_SIZE)) {
				/* clear the card table under the region - we will rebuild it as we fixup */
				Card *base = cardTable->heapAddrToCardAddr(env, cardToScan);
				*base = CARD_CLEAN;
				fixupObjectsInRange(env, cardToScan, (void *)((UDATA)cardToScan + CARD_SIZE), false);
			}
		} else {
			/* there is some fixup work to do while we wait for move work to become available.  Clean cards for this subarea */
			MM_WriteOnceFixupCardCleaner cardCleaner(this, env->_cycleState, _regionManager);
			cardTable->cleanCardsInRegion(env, &cardCleaner, region);
		}
	}
}

/* Save forwarding information for relocated object objectPtr.
 *
 * The new location of the first relocated object on a page
 * is forwardingPtr.  The new location of the following objects
 * is deduced from the forwardingPtr and the sequential number
 * of the object on the page.
 *
 * We need to keep some state in the caller's local variables
 * page and counter.
 * Objects are counted on the page for hints.  The first
 * object (count=0) does not need a hint, because its
 * address is directly stored in the addr field.  Depending
 * on the available space, one or more hints are kept for
 * the following objects.
 */
MMINLINE void
MM_WriteOnceCompactor::saveForwardingPtr(J9Object *objectPtr, J9Object *forwardingPtr)
{
	Assert_MM_true(NULL != forwardingPtr);
	IDATA page = pageIndex(objectPtr);
	void *destination = _compactTable[page].getAddr();
	if (NULL == destination) {
		_compactTable[page].initialize(forwardingPtr);
	} else {
		/* ensure that we aren't trying to re-forward a page to a lower address than it was already targetting */
		Assert_MM_true((void *)forwardingPtr > destination);
	}
}

UDATA
MM_WriteOnceCompactor::tailMarkObjectsInRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
{
	Assert_MM_true(region->_compactData._shouldCompact);
	/* walk the objects in this page (following this object) in their previous mark map and tail-mark them to denote their sizes */
	void *base = region->getLowAddress();
	void *top = region->getHighAddress();
	/* now perform the tail-mark */
	MM_HeapMapIterator iterator(_extensions, _cycleState._markMap, (UDATA *)base, (UDATA *)top, false);
	J9Object *walk = NULL;
	J9Object *tailToMark = NULL;
	UDATA estimatedSize = 0;
	UDATA estimatedPageSize = 0;
	IDATA previousPageIndex = -1;
	while (NULL != (walk = iterator.nextObject())) {
		Assert_MM_mustBeClass(J9GC_J9OBJECT_CLAZZ(walk, env));
		IDATA walkPageIndex = pageIndex(walk);
		if (NULL != tailToMark) {
			Assert_MM_true(tailToMark < walk);
			_cycleState._markMap->setBit(tailToMark);
			tailToMark = NULL;
		}
		if (walkPageIndex != previousPageIndex) {
			if (0 != estimatedPageSize) {
				/* we are skipping to a new page so write our total estimated size of the page */
				_compactTable[previousPageIndex].setEstimatedSize(estimatedPageSize);
				estimatedPageSize = 0;
			}
			previousPageIndex = walkPageIndex;
		}
		UDATA objectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(walk);
		UDATA postMoveSize = _extensions->objectModel.getConsumedSizeInBytesWithHeaderForMove(walk);
		estimatedSize += postMoveSize;
		estimatedPageSize += postMoveSize;
		J9Object *tail = (J9Object *)((UDATA)walk + objectSize - J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT);
		Assert_MM_true(NULL == tailToMark);
		Assert_MM_true(tail > base);
		if (objectSize != postMoveSize) {
			Assert_MM_true(_objectAlignmentInBytes == (postMoveSize - objectSize));
			_compactTable[walkPageIndex].setGrowBit(walk);
		}
		if (tail < pageStart(walkPageIndex + 1)) {
			tailToMark = tail;
		}
	}
	if (NULL != tailToMark) {
		_cycleState._markMap->setBit(tailToMark);
	}
	if (0 != estimatedPageSize) {
		_compactTable[previousPageIndex].setEstimatedSize(estimatedPageSize);
	}
	if (estimatedSize > region->getSize()) {
		/* make sure that the compact destination doesn't overflow - we can't compact this region so just make the destination the top of the region */
		region->_compactData._compactDestination = top;
	} else {
		region->_compactData._compactDestination = (void *)((UDATA)base + estimatedSize);
	}
	return estimatedSize;
}

J9Object*
MM_WriteOnceCompactor::getForwardingPtr(J9Object* objectPtr) const
{
	J9Object *forwardPointer = NULL;
	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *) _regionManager->tableDescriptorForAddress(objectPtr);
	if (!region->_compactData._shouldCompact) {
		forwardPointer = objectPtr;
	} else {
		IDATA oldPageIndex = pageIndex(objectPtr);
		J9Object* targetBaseAddress = _compactTable[oldPageIndex].getAddr();
		if (NULL == targetBaseAddress) {
			/* objects in this page did not move so return the original */
			forwardPointer = objectPtr;
		} else {
			/* by this point, we know that objects in this page have moved in some way (might have only been the case of compaction within the page but movement, nonetheless) */
			IDATA newPageIndex = pageIndex(targetBaseAddress);
			/* we can't over-write ourself */
			Assert_MM_true(objectPtr != targetBaseAddress);
			if ((newPageIndex == oldPageIndex) && (targetBaseAddress >= objectPtr)) {
				/* this only happens when internally compacting a page - we are to ignore the forwarding pointer since it implies that this object was before the sliding objects and did not move */
				forwardPointer = objectPtr;
			} else {
				/* sliding compact or not, this object moved */
				bool isSliding = (oldPageIndex == newPageIndex);
				if (isSliding) {
					/* this is a sliding compact within one page but the object is after the forwarding pointer so it is sliding down */
					UDATA bytes = bytesAfterSlidingTargetToLocateObject(objectPtr, targetBaseAddress);
					forwardPointer = (J9Object *)((UDATA)targetBaseAddress + bytes);
				} else {
					/* this is an inter-page compaction */
					void *base = pageStart(oldPageIndex);
					UDATA totalSize = bytesAfterSlidingTargetToLocateObject(objectPtr, (J9Object *)base);
					forwardPointer = (J9Object *)((UDATA)targetBaseAddress + totalSize);
				}
			}
		}
	}
	Assert_MM_true(NULL != forwardPointer);
	return forwardPointer;
}

/* this is the combined value of the number of bits which represent live objects in a tail-marked mark map byte:
 * -high 4 bits represent the number of bits which represent live data assuming that we ARE NOT already inside an object
 * -low 4 bits represent the number of bits which represent live data assuming that we ARE already inside an object
 */
static U_8 combinedByteValueLookupTable[] = {0x8, 0x81, 0x72, 0x28, 0x63, 0x37, 0x28, 0x83, 0x54, 0x46, 0x37, 0x74, 0x28, 0x83, 0x74, 0x48, 0x45, 0x55, 0x46, 0x65, 0x37, 0x74, 0x65, 0x57, 0x28, 0x83, 0x74, 0x48, 0x65, 0x57, 0x48, 0x85, 0x36, 0x64, 0x55, 0x56, 0x46, 0x65, 0x56, 0x66, 0x37, 0x74, 0x65, 0x57, 0x56, 0x66, 0x57, 0x76, 0x28, 0x83, 0x74, 0x48, 0x65, 0x57, 0x48, 0x85, 0x56, 0x66, 0x57, 0x76, 0x48, 0x85, 0x76, 0x68, 0x27, 0x73, 0x64, 0x47, 0x55, 0x56, 0x47, 0x75, 0x46, 0x65, 0x56, 0x66, 0x47, 0x75, 0x66, 0x67, 0x37, 0x74, 0x65, 0x57, 0x56, 0x66, 0x57, 0x76, 0x47, 0x75, 0x66, 0x67, 0x57, 0x76, 0x67, 0x77, 0x28, 0x83, 0x74, 0x48, 0x65, 0x57, 0x48, 0x85, 0x56, 0x66, 0x57, 0x76, 0x48, 0x85, 0x76, 0x68, 0x47, 0x75, 0x66, 0x67, 0x57, 0x76, 0x67, 0x77, 0x48, 0x85, 0x76, 0x68, 0x67, 0x77, 0x68, 0x87, 0x18, 0x82, 0x73, 0x38, 0x64, 0x47, 0x38, 0x84, 0x55, 0x56, 0x47, 0x75, 0x38, 0x84, 0x75, 0x58, 0x46, 0x65, 0x56, 0x66, 0x47, 0x75, 0x66, 0x67, 0x38, 0x84, 0x75, 0x58, 0x66, 0x67, 0x58, 0x86, 0x37, 0x74, 0x65, 0x57, 0x56, 0x66, 0x57, 0x76, 0x47, 0x75, 0x66, 0x67, 0x57, 0x76, 0x67, 0x77, 0x38, 0x84, 0x75, 0x58, 0x66, 0x67, 0x58, 0x86, 0x57, 0x76, 0x67, 0x77, 0x58, 0x86, 0x77, 0x78, 0x28, 0x83, 0x74, 0x48, 0x65, 0x57, 0x48, 0x85, 0x56, 0x66, 0x57, 0x76, 0x48, 0x85, 0x76, 0x68, 0x47, 0x75, 0x66, 0x67, 0x57, 0x76, 0x67, 0x77, 0x48, 0x85, 0x76, 0x68, 0x67, 0x77, 0x68, 0x87, 0x38, 0x84, 0x75, 0x58, 0x66, 0x67, 0x58, 0x86, 0x57, 0x76, 0x67, 0x77, 0x58, 0x86, 0x77, 0x78, 0x48, 0x85, 0x76, 0x68, 0x67, 0x77, 0x68, 0x87, 0x58, 0x86, 0x77, 0x78, 0x68, 0x87, 0x78, 0x88};

/* the number of bits representing live data in a mark map byte with value offset, assuming we ARE NOT already in an object*/
#define BITS_OCCUPIED(offset) (combinedByteValueLookupTable[offset] >> 4)
/* the number of bits representing live data in a mark map byte with value offset, assuming we ARE already in an object*/
#define BITS_NEGATE_OCCUPIED(offset) (combinedByteValueLookupTable[offset] & 0xf)
/* the number of bits set in a tail-marked mark map word with the value offset */
#define BITS_SET(offset) (BITS_OCCUPIED(offset) + BITS_NEGATE_OCCUPIED(offset) - 8)
	
UDATA
MM_WriteOnceCompactor::bytesAfterSlidingTargetToLocateObject(J9Object *oldPointer, J9Object *slidingTarget) const
{
	UDATA totalSize = 0;
	IDATA oldPageIndex = pageIndex(oldPointer);
	IDATA slidingIndex = pageIndex(slidingTarget);
	void *base = pageStart(oldPageIndex);
	/* we need to know if we have seen an even or odd number of bits, to know if each mark map byte starts inside an object, or not */
	UDATA bitsCounted = 0;
	U_8 *heapMapBits = (U_8 *)_cycleState._markMap->getHeapMapBits();
	/* initialize the map pointer to the first mark map word in the page.  We will skip ahead by one slot each time through this loop */
	UDATA *mapPointer = (UDATA *) (heapMapBits + (((UDATA)base - (UDATA)_heapBase) / J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT));
	/* we will count the number of live bytes represented by each word of the mark map since it is our native unit of computation so loop over the page, heap map word at a time */
	for (UDATA bias = 0; (((UDATA)base + bias) < (UDATA)oldPointer) && (bias < sizeof_page); bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
		void *baseAddress = (void *)((UDATA)base + bias);
		/* read this mark map word and we will modify it, in place, for our purposes */
		UDATA mapWord = *mapPointer;
		/* advance the pointer to the next mark map word */
		mapPointer += 1;
		void *next = (void *)((UDATA)baseAddress + J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP);
		if ((slidingTarget > baseAddress) && (pageIndex((J9Object *)baseAddress) == slidingIndex)) {
			/* mask off the early part of the word since we don't want to count the size consumed before the base address we are using */
			if (slidingTarget < next) {
				/* if the target is in this part of the word, we need to partially mask the word */
				UDATA heapWordStart = (UDATA)slidingTarget - (UDATA)baseAddress;
				UDATA bitCount = heapWordStart / J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT;
				UDATA mask = ((UDATA)1 << bitCount) - 1;
				mapWord = mapWord & (~mask);
			} else {
				/* the target is in the next part of the word so ignore all bits in this part */
				mapWord = 0;
			}
		}
		if ((oldPointer < next) && (oldPointer >= baseAddress)) {
			/* mask off the high part of the word since we don't want to count the size consumed after the object we are looking up */
			UDATA heapWordOffset = (UDATA)oldPointer - (UDATA)baseAddress;
			UDATA objectBitCount = heapWordOffset / J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT;
			UDATA bitExtend = ((UDATA)1 << objectBitCount);
			UDATA mask = bitExtend - 1;
			mapWord = mapWord & mask;
		}
		/* now, count the number of bytes consumed in each byte of the mark map, after we have masked off the parts in which we aren't interested */
		/* extract a copy of the map word which we will change, from iteration to iteration, of the loop */
		UDATA mutableWord = mapWord;
		for (UDATA i = 0 ; i < sizeof(UDATA); i++) {
			U_8 byteValue = (U_8)(mutableWord & 0xFF);
			UDATA bitCount = 0;
			if (0 == (bitsCounted % 2)) {
				bitCount = BITS_OCCUPIED(byteValue);
			} else {
				bitCount = BITS_NEGATE_OCCUPIED(byteValue);
			}
			UDATA size = bitCount * J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT;
			bitsCounted += BITS_SET(byteValue);
			totalSize += size;
			mutableWord = mutableWord >> 8;
		}
	}
	if (totalSize > 0) {
		/* if there were objects before this, account for the number of objects which have grown (each object needs to be aligned
		 * so add object alignment as the "per-object growth quantity" even though the hash is likely smaller than this)
		 */
		UDATA growingSize = _objectAlignmentInBytes * _compactTable[oldPageIndex].growBitsInfluencingObject(oldPointer);
		totalSize += growingSize;
	}
	return totalSize;
}

void
MM_WriteOnceCompactor::fixupObjectsInRange(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress, bool rememberedObjectsOnly)
{
	/* we only support fixing exactly one card at a time */
	Assert_MM_true(0 == ((UDATA)lowAddress & (J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP - 1)));
	Assert_MM_true(((UDATA)lowAddress + CARD_SIZE) == (UDATA)highAddress);

	if (rememberedObjectsOnly) {
		for (UDATA bias = 0; bias < CARD_SIZE; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
			void *fixupAddress = (void *)((UDATA)lowAddress + bias);
			MM_HeapMapWordIterator markedObjectIterator(_cycleState._markMap, fixupAddress);
			J9Object *fromObject = NULL;
			while (NULL != (fromObject = markedObjectIterator.nextObject())) {
				/* this object needs to be re-scanned (to update next mark map and RS) */
				if (_extensions->objectModel.isRemembered(fromObject)) {
					fixupObject(env, fromObject, NULL);
				}
			}
		}
	} else {
		for (UDATA bias = 0; bias < CARD_SIZE; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
			void *fixupAddress = (void *)((UDATA)lowAddress + bias);
			MM_HeapMapWordIterator markedObjectIterator(_cycleState._markMap, fixupAddress);
			J9Object *fromObject = NULL;
			while (NULL != (fromObject = markedObjectIterator.nextObject())) {
				/* this object needs to be re-scanned (to update next mark map and RS) */
				fixupObject(env, fromObject, NULL);
			}
		}
	}
}

MMINLINE void
MM_WriteOnceCompactor::preObjectMove(MM_EnvironmentVLHGC* env, J9Object *objectPtr, UDATA *objectSizeAfterMove)
{
	*objectSizeAfterMove = _extensions->objectModel.getConsumedSizeInBytesWithHeaderForMove(objectPtr);
	env->preObjectMoveForCompact(objectPtr);
}

MMINLINE void
MM_WriteOnceCompactor::postObjectMove(MM_EnvironmentVLHGC* env, J9Object *newLocation, J9Object *objectPtr)
{
	/* the object has just moved so we can't trust the remembered bit.  Clear it and it will be re-added, during fixup, if required */
	if (_extensions->objectModel.isRemembered(newLocation)) {
		_extensions->objectModel.clearRemembered(newLocation);
	}

	env->postObjectMoveForCompact(newLocation, objectPtr);
}

void
MM_WriteOnceCompactor::fixupMixedObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache)
{
	/* object may have moved so ensure that its class loader knows where it is */
	_extensions->classLoaderRememberedSet->rememberInstance(env, objectPtr);
	
	GC_MixedObjectIterator it(_javaVM->omrVM, objectPtr);
	while (GC_SlotObject* slotObject = it.nextSlot()) {
		J9Object *pointer = slotObject->readReferenceFromSlot();
		if (NULL != pointer) {
			J9Object *forwardedPtr = getForwardWrapper(env, pointer, cache);
			if (pointer != forwardedPtr) {
				slotObject->writeReferenceToSlot(forwardedPtr);
			}
			_interRegionRememberedSet->rememberReferenceForCompact(env, objectPtr, forwardedPtr);
		}
	}
}

void
MM_WriteOnceCompactor::doStackSlot(MM_EnvironmentVLHGC *env, J9Object *fromObject, J9Object** slot)
{
	J9Object *pointer = *slot;
	if (NULL != pointer) {
		J9Object *forwardedPtr = getForwardingPtr(pointer);
		if (pointer != forwardedPtr) {
			*slot = forwardedPtr;
		}
		_interRegionRememberedSet->rememberReferenceForCompact(env, fromObject, forwardedPtr);
	}
}

/**
 * @todo Provide function documentation
 */
void
stackSlotIteratorForWriteOnceCompactor(J9JavaVM *javaVM, J9Object **slotPtr, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	StackIteratorData4WriteOnceCompactor *data = (StackIteratorData4WriteOnceCompactor *)localData;
	data->writeOnceCompactor->doStackSlot(data->env, data->fromObject, slotPtr);
}

void
MM_WriteOnceCompactor::fixupContinuationNativeSlots(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache)
{
	J9VMThread *currentThread = (J9VMThread *)env->getLanguageVMThread();
	/* In sliding compaction we must fix slots exactly once. Since we will fixup stack slots of
	 * mounted Virtual threads later during root fixup, we will skip it during this heap fixup pass
	 * (hence passing true for scanOnlyUnmounted parameter).
	 */
	const bool isConcurrentGC = false;
	const bool isGlobalGC = MM_CycleState::CT_GLOBAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType;
	const bool beingMounted = false;
	if (MM_GCExtensions::needScanStacksForContinuationObject(currentThread, objectPtr, isConcurrentGC, isGlobalGC, beingMounted)) {
		StackIteratorData4WriteOnceCompactor localData;
		localData.writeOnceCompactor = this;
		localData.env = env;
		localData.fromObject = objectPtr;

		GC_VMThreadStackSlotIterator::scanContinuationSlots(currentThread, objectPtr, (void *)&localData, stackSlotIteratorForWriteOnceCompactor, false, false);
	}
}

void
MM_WriteOnceCompactor::fixupContinuationObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache)
{
	fixupContinuationNativeSlots(env, objectPtr, cache);
	fixupMixedObject(env, objectPtr, cache);
}

#if defined (J9ZOS39064)
/* Temporary fix CMVC 173946. Due to a compiler defect (385201) the JVM throws assertions on z/OS 
 * if this function is optimized at -O3. At -O2 the function works, but only because the problem is
 * being hidden. At -O0 the defect is completely avoided. Use a pragma to override the optimization 
 * level. Once our build machines all have a PTF with the fix we can remove this workaround.
 */
#pragma option_override(MM_WriteOnceCompactor::fixupClassObject, "opt(level, 0)")
#endif
void
MM_WriteOnceCompactor::fixupClassObject(MM_EnvironmentVLHGC* env, J9Object *classObject, J9MM_FixupCache *cache)
{
	fixupMixedObject(env, classObject, cache);

	J9Class *classPtr = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), classObject);
	if (NULL != classPtr) {
		volatile j9object_t *slotPtr = NULL;

		do {
			Assert_MM_mustBeClass(classPtr);
			Assert_MM_true(0 == (J9CLASS_FLAGS(classPtr) & J9AccClassDying));

			/*
			 * Scan J9Class internals using general iterator
			 * - scan statics fields
			 * - scan call sites
			 * - scan MethodTypes
			 * - scan VarHandle MethodTypes
			 * - scan constants pool objects
			 */
			GC_ClassIterator classIterator(env, classPtr, false);
			while (NULL != (slotPtr = classIterator.nextSlot())) {
				J9Object *originalObject = *slotPtr;
				if (NULL != originalObject) {
					J9Object *forwardedObject = getForwardWrapper(env, originalObject, cache);
					*slotPtr = forwardedObject;
					_interRegionRememberedSet->rememberReferenceForCompact(env, classObject, forwardedObject);
				}
			}

			Assert_MM_true(classObject == getForwardWrapper(env, J9VM_J9CLASS_TO_HEAPCLASS(classPtr), cache));
			classPtr->classObject = (j9object_t)classObject;

			classPtr = classPtr->replacedClass;
		} while (NULL != classPtr);
	}
}

void
MM_WriteOnceCompactor::fixupClassLoaderObject(MM_EnvironmentVLHGC* env, J9Object *classLoaderObject, J9MM_FixupCache *cache)
{
	fixupMixedObject(env, classLoaderObject, cache);
	
	J9ClassLoader *classLoader = J9VMJAVALANGCLASSLOADER_VMREF((J9VMThread*)env->getLanguageVMThread(), classLoaderObject);
	if (NULL != classLoader) {
		Assert_MM_true(classLoaderObject == getForwardWrapper(env, classLoader->classLoaderObject, cache));
		classLoader->classLoaderObject = classLoaderObject;

		/*
		 * Fixup modules
		 */
		if (NULL != classLoader->moduleHashTable) {
			J9HashTableState walkState;
			J9Module **modulePtr = (J9Module **)hashTableStartDo(classLoader->moduleHashTable, &walkState);

			volatile j9object_t * slotPtr = NULL;
			J9Object* originalObject = NULL;

			while (NULL != modulePtr) {
				J9Module * const module = *modulePtr;

				slotPtr = &module->moduleObject;

				originalObject = *slotPtr;
				J9Object* forwardedObject = getForwardWrapper(env, originalObject, cache);
				*slotPtr = forwardedObject;
				_interRegionRememberedSet->rememberReferenceForCompact(env, classLoaderObject, forwardedObject);

				slotPtr = &module->moduleName;

				originalObject = *slotPtr;
				if (NULL != originalObject) {
					J9Object* forwardedObject = getForwardWrapper(env, originalObject, cache);
					*slotPtr = forwardedObject;
					_interRegionRememberedSet->rememberReferenceForCompact(env, classLoaderObject, forwardedObject);
				}

				slotPtr = &module->version;

				originalObject = *slotPtr;
				if (NULL != originalObject) {
					J9Object* forwardedObject = getForwardWrapper(env, originalObject, cache);
					*slotPtr = forwardedObject;
					_interRegionRememberedSet->rememberReferenceForCompact(env, classLoaderObject, forwardedObject);
				}

				modulePtr = (J9Module**)hashTableNextDo(&walkState);
			}

			if (classLoader == _javaVM->systemClassLoader) {
				slotPtr = &_javaVM->unamedModuleForSystemLoader->moduleObject;

				originalObject = *slotPtr;
				J9Object* forwardedObject = getForwardWrapper(env, originalObject, cache);
				*slotPtr = forwardedObject;
				_interRegionRememberedSet->rememberReferenceForCompact(env, classLoaderObject, forwardedObject);
			}
		}

		/* We can't fixup remembered set for defined and referenced classes here since we 
		 * can't tell if they've been fixed up yet. This is left for the final fixup pass.
		 */
	}
}

void
MM_WriteOnceCompactor::fixupPointerArrayObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache)
{
	uintptr_t const referenceSize = env->compressObjectReferences() ? sizeof(uint32_t) : sizeof(uintptr_t);
	/* object may have moved so ensure that its class loader knows where it is */
	_extensions->classLoaderRememberedSet->rememberInstance(env, objectPtr);

	/* arraylet leaves are walked separately in fixupArrayletLeafRegionContentsAndObjectLists(), to increase parallelism. Just walk the spine */
	GC_ArrayObjectModel *indexableObjectModel = &_extensions->indexableObjectModel;
	GC_ArrayletObjectModel::ArrayLayout layout = indexableObjectModel->getArrayLayout((J9IndexableObject *)objectPtr);

	if (GC_ArrayletObjectModel::InlineContiguous == layout) {
		/* For offheap we need a special check for the case of a partially offheap allocated array that caused the current GC
		 * (its dataAddr field will still be NULL), with offheap heap we will fixup camouflaged discontiguous arrays) - DM, like default
		 * balanced, wants to fixup only truly contiguous arrays
		 */
		if (indexableObjectModel->isArrayletDataAdjacentToHeader((J9IndexableObject *)objectPtr)
#if defined(J9VM_ENV_DATA64)
		 || (indexableObjectModel->isVirtualLargeObjectHeapEnabled() && (NULL != indexableObjectModel->getDataAddrForContiguous((J9IndexableObject *)objectPtr)))
#endif /* defined(J9VM_ENV_DATA64) */
		 ) {
			uintptr_t elementsToWalk = indexableObjectModel->getSizeInElements((J9IndexableObject *)objectPtr);
			GC_PointerArrayIterator it(_javaVM, objectPtr);
			uintptr_t previous = 0;
			for (uintptr_t i = 0; i < elementsToWalk; i++) {
				GC_SlotObject *slotObject = it.nextSlot();
				Assert_MM_true(NULL != slotObject);
				J9Object *pointer = slotObject->readReferenceFromSlot();
				if (NULL != pointer) {
					J9Object *forwardedPtr = getForwardWrapper(env, pointer, cache);
					slotObject->writeReferenceToSlot(forwardedPtr);
					_interRegionRememberedSet->rememberReferenceForCompact(env, objectPtr, forwardedPtr);
				}
				uintptr_t address = (uintptr_t)slotObject->readAddressFromSlot();
				Assert_MM_true((0 == previous) || ((address + referenceSize) == previous));
				previous = address;
			}
			/* if this is a contiguous array, we must have exhausted the iterator */
			Assert_MM_true(NULL == it.nextSlot());
		}
	} else if (GC_ArrayletObjectModel::Discontiguous == layout) {
		/* do nothing - no inline pointers */
	} else if (GC_ArrayletObjectModel::Hybrid == layout) {
		uintptr_t numArraylets = indexableObjectModel->numArraylets((J9IndexableObject *)objectPtr);
		/* hybrid layouts always have at least one arraylet pointer in any configuration */
		Assert_MM_true(numArraylets > 0);
		/* ensure that the array has been initialized */
		if (NULL != GC_PointerArrayIterator(_javaVM, objectPtr).nextSlot()) {
			/* find the size of the inline component of the spine */
			uintptr_t totalElementCount = indexableObjectModel->getSizeInElements((J9IndexableObject *)objectPtr);
			uintptr_t externalArrayletCount = indexableObjectModel->numExternalArraylets((J9IndexableObject *)objectPtr);
			uintptr_t fullLeafSizeInBytes = _javaVM->arrayletLeafSize;
			uintptr_t elementsPerFullLeaf = fullLeafSizeInBytes / referenceSize;
			uintptr_t elementsToWalk = totalElementCount - (externalArrayletCount * elementsPerFullLeaf);
			uintptr_t previous = 0;
			
			GC_PointerArrayletInlineLeafIterator iterator(_javaVM, objectPtr);
			GC_SlotObject *slotObject = NULL;
			while (NULL != (slotObject = iterator.nextSlot())) {
				Assert_MM_true(elementsToWalk > 0);
				elementsToWalk -= 1;
				J9Object* pointer = slotObject->readReferenceFromSlot();
				if (NULL != pointer) {
					J9Object* forwardedPtr = getForwardWrapper(env, pointer, cache);
					slotObject->writeReferenceToSlot(forwardedPtr);
					_interRegionRememberedSet->rememberReferenceForCompact(env, objectPtr, forwardedPtr);
				}
				
				uintptr_t address = (uintptr_t)slotObject->readAddressFromSlot();
				Assert_MM_true((0 == previous) || ((address + referenceSize) == previous));
				previous = address;
			}
			Assert_MM_true(0 == elementsToWalk);
		}
	} else {
		Assert_MM_unreachable();
	}
}

void
MM_WriteOnceCompactor::fixupObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache)
{
	J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, env);
	Assert_MM_mustBeClass(clazz);
	switch(_extensions->objectModel.getScanType(clazz)) {
	case GC_ObjectModel::SCAN_MIXED_OBJECT_LINKED:
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		fixupMixedObject(env, objectPtr, cache);
		break;

	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		addOwnableSynchronizerObjectInList(env, objectPtr);
		fixupMixedObject(env, objectPtr, cache);
		break;
	case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
		fixupContinuationObject(env, objectPtr, cache);
		break;
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
		fixupClassObject(env, objectPtr, cache);
		break;
		
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		fixupClassLoaderObject(env, objectPtr, cache);
		break;
		
	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		fixupPointerArrayObject(env, objectPtr, cache);
		break;

	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		/* nothing to do */
		break;

	default:
		Trc_MM_WriteOnceCompactor_fixupObject_invalid(env->getLanguageVMThread(), objectPtr, cache);
		Assert_MM_unreachable();
	}
}

void
MM_WriteOnceCompactor::writeFlushToCardState(Card *card, bool isGlobalMarkPhaseRunning)
{
	Card fromState = *card;
	Card toState = CARD_INVALID;
	switch(fromState) {
	case CARD_CLEAN:
		if (isGlobalMarkPhaseRunning) {
			/* This card may refer an object for which we do not rebuild next mark bits via Work Packet fixup so ensure that the GMP has to see it */
			toState = CARD_REMEMBERED_AND_GMP_SCAN;
		} else {
			toState = CARD_REMEMBERED;
		}
		break;
	case CARD_GMP_MUST_SCAN:
		toState = CARD_REMEMBERED_AND_GMP_SCAN;
		break;
	case CARD_PGC_MUST_SCAN:
		if (isGlobalMarkPhaseRunning) {
			/* This card may refer an object for which we do not rebuild next mark bits via Work Packet fixup so ensure that the GMP has to see it */
			toState = CARD_DIRTY;
		} else {
			/* do nothing */
		}
		break;
	case CARD_REMEMBERED_AND_GMP_SCAN:
		if (isGlobalMarkPhaseRunning) {
			/* This card may refer an object for which we do not rebuild next mark bits via Work Packet fixup so ensure that the GMP has to see it */
			/* do nothing */
		} else {
			toState = CARD_REMEMBERED;
		}
		break;
	case CARD_REMEMBERED:
		if (isGlobalMarkPhaseRunning) {
			/* This card may refer an object for which we do not rebuild next mark bits via Work Packet fixup so ensure that the GMP has to see it */
			toState = CARD_REMEMBERED_AND_GMP_SCAN;
		} else {
			/* do nothing */
		}
		break;
	case CARD_DIRTY:
		/* do nothing */
		break;
	default:
		Assert_MM_unreachable();
		break;
	}
	if (CARD_INVALID != toState) {
		*card = toState;
	}
}

void
MM_WriteOnceCompactor::flushRememberedSetIntoCardTable(MM_EnvironmentVLHGC *env)
{
	/* TODO: share this code with CardListFlushTask (for Marking) */
	/* This function has knowledge of the collection set, which is only valid during a PGC */
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);

	/* flush RS Lists for CollectionSet and dirty card table */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (NULL != region->getMemoryPool()) {
			if (region->_compactData._shouldCompact) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					Assert_MM_true(region->getRememberedSetCardList()->isAccurate());
					UDATA card = 0;
					GC_RememberedSetCardListCardIterator rsclCardIterator(region->getRememberedSetCardList());
					while (0 != (card = rsclCardIterator.nextReferencingCard(env))) {
						/* For Marking purposes we do not need to track references within Collection Set */
						MM_HeapRegionDescriptorVLHGC *targetRegion = _interRegionRememberedSet->tableDescriptorForRememberedSetCard(card);
						if ((!targetRegion->_compactData._shouldCompact) && (targetRegion->containsObjects())) {
							Card *cardAddress = _interRegionRememberedSet->rememberedSetCardToCardAddr(env, card);
							writeFlushToCardState(cardAddress, NULL != env->_cycleState->_externalCycleState);
						}
					}

					/* Clear remembered references to each region in Collection Set (completely clear RS Card List for those regions
					 * and appropriately update RSM). We are about to rebuild those references in PGC.
					 */
					_interRegionRememberedSet->clearReferencesToRegion(env, region);
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::tagArrayletLeafRegionsForFixup(MM_EnvironmentVLHGC *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		Assert_MM_false(region->_compactData._shouldFixup);
		if (region->isArrayletLeaf()) {
			Assert_MM_false(region->_compactData._shouldCompact);
			J9IndexableObject *spineObject = region->_allocateData.getSpine();
			Assert_MM_true(NULL != spineObject);
			if (_extensions->objectModel.isObjectArray(spineObject)) {
				MM_HeapRegionDescriptorVLHGC* spineRegion = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(spineObject);
				if (MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION != env->_cycleState->_collectionType) {
					/* global collection - must fix up */
					region->_compactData._shouldFixup = true;
				} else if (spineRegion->_compactData._shouldCompact) {
					/* the spine object is moving, so this leaf will need to be fixed up */
					region->_compactData._shouldFixup = true;
				} else  {
					Card * spineCard = _extensions->cardTable->heapAddrToCardAddr(env, spineObject);
					switch (*spineCard) {
					case CARD_CLEAN:
					case CARD_GMP_MUST_SCAN:
						/* clean card - no fixup required */
						break;
					case CARD_REMEMBERED:
					case CARD_REMEMBERED_AND_GMP_SCAN:
					case CARD_DIRTY:
					case CARD_PGC_MUST_SCAN:
						/* the spine object is in a dirty card, so this leaf will need to be fixed up */
						region->_compactData._shouldFixup = true;
						break;
					default:
						Assert_MM_unreachable();
					}
				}
			}
		}
	}
}

class MM_WriteOnceCompactFixupRoots : public MM_RootScanner {
private:
	MM_WriteOnceCompactor * const _compactScheme;
	void * const _heapBase;
	void * const _heapTop;

public:
	MM_WriteOnceCompactFixupRoots(MM_EnvironmentVLHGC *env, MM_WriteOnceCompactor* compactScheme, void *heapBase, void *heapTop)
		: MM_RootScanner(env)
		, _compactScheme(compactScheme)
		, _heapBase(heapBase)
		, _heapTop(heapTop)
	{
		_typeId = __FUNCTION__;
		setIncludeStackFrameClassReferences(false);
	}

	/**
	 * Scan all slots which contain references into the heap.
	 */
	void
	scanAllSlots(MM_EnvironmentBase *env)
	{
		scanVMClassSlots(env);
		scanThreads(env);
#if defined(J9VM_GC_FINALIZATION)
		scanFinalizableObjects(env);
#endif /* J9VM_GC_FINALIZATION */
		scanJNIGlobalReferences(env);
		scanStringTable(env);
		scanMonitorReferences(env);
		scanJNIWeakGlobalReferences(env);
#if defined(J9VM_OPT_JVMTI)
		scanJVMTIObjectTagTables(env);
#endif /* J9VM_OPT_JVMTI */

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	if (_includeDoubleMap) {
		scanDoubleMappedObjects(env);
	}
#endif /* defined(J9VM_GC_ENABLE_DOUBLE_MAP) */

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	if (_includeVirtualLargeObjectHeap) {
		scanObjectsInVirtualLargeObjectHeap(env);
	}
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

	}
	
	virtual void doSlot(J9Object** slot)
	{
		J9Object *pointer = *slot;
		if ((pointer >= _heapBase) && (pointer < _heapTop)) {
			J9Object *newPointer = _compactScheme->getForwardingPtr(pointer);
			if (pointer != newPointer) {
				*slot = newPointer;
			}
		}
	}

	virtual void doClass(J9Class *clazz)
	{
		/* classes are fixed up as part of normal object fixup */
		Assert_MM_unreachable();
	}

	virtual void doClassLoader(J9ClassLoader *classLoader)
	{
		/* class loaders are fixed up as part of normal object fixup */
		Assert_MM_unreachable();
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t object) {
		Assert_MM_unreachable();
	}
	virtual void scanFinalizableObjects(MM_EnvironmentBase *env) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			reportScanningStarted(RootScannerEntity_FinalizableObjects);
			_compactScheme->fixupFinalizableObjects(MM_EnvironmentVLHGC::getEnvironment(env));
			reportScanningEnded(RootScannerEntity_FinalizableObjects);
		}
	}
#endif /* J9VM_GC_FINALIZATION */
};

void
MM_WriteOnceCompactor::fixupRoots(MM_EnvironmentVLHGC *env)
{
	MM_WriteOnceCompactFixupRoots rootScanner(env, this, _heapBase, _heapTop);
	rootScanner.scanAllSlots(env);
	
	/* Now we need to update any RSM entries for classloader-to-class references.
	 * These couldn't be done in the first pass since we don't have any unique slots to operate on.
	 */
	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	J9ClassLoader *classLoader = NULL;
	while ((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			/* TODO: we could optimize this by only examining class loaders in fixup regions */
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				if (NULL != classLoader->classLoaderObject) {
					Assert_MM_true(NULL != classLoader->classHashTable);
					J9Object *classLoaderObject = classLoader->classLoaderObject;
					GC_ClassLoaderClassesIterator iterator(_extensions, classLoader);
					J9Class *clazz = NULL;
					while (NULL != (clazz = iterator.nextClass())) {
						J9Object * classObject = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
						Assert_MM_true(NULL != classObject);
						_interRegionRememberedSet->rememberReferenceForCompact(env, classLoaderObject, classObject);
					}
				} else { 
					/* Only system/app classloaders can have a null classloader object (only during early bootstrap) */
					Assert_MM_true((classLoader == _javaVM->systemClassLoader)
							|| (classLoader == _javaVM->applicationClassLoader)
							|| (classLoader == _javaVM->extensionClassLoader));
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::rebuildMarkbits(MM_EnvironmentVLHGC *env)
{
	/* we need to ensure that pages will be a multiple of mark words */
	Assert_MM_true(0 == (sizeof_page % J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP));
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = popRebuildWork(env))) {
		Assert_MM_true(region->_compactData._shouldCompact);
		void *target = rebuildMarkbitsInRegion(env, region);
		pushRebuildWork(env, region, target);
	}
	/* all threads will implicitly synchronize in the above loop and only exit once all work is done so assert that they all see work being completed */
	Assert_MM_true(NULL == _rebuildWorkList);
}


void *
MM_WriteOnceCompactor::rebuildMarkbitsInRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region)
{
	/* first, look up where we are supposed to start rebuilding (_nextRebuildCandidate) */
	void *startAddress = region->_compactData._nextRebuildCandidate;
	/* assume that this is page aligned - this could be changed in the future but it simplifies things, for now */
	Assert_MM_true(0 == ((UDATA)startAddress & (sizeof_page-1)));
	/* get the high address of the region since that is how far we intend to attempt to rebuild */
	J9Object *high = (J9Object *)region->getHighAddress();
	/* obviously, the starting address has to come first (protects against stale data and ensures that we didn't just get a region which is already done) */
	Assert_MM_true(startAddress < high);
	/* the page we are presently trying to rebuild "from" */
	void *page = NULL;
	/* earlyExit is used in case we run into blocking point (that is, we find a page whose destination's mark bits have not yet been moved to their new location) */
	void *earlyExit = NULL;
	/* evacuationTarget is used in the case where earlyExit is used - it represents the destination address upon which we are blocked */
	void *evacuationTarget = NULL;
	MM_MarkMap *markMap = _cycleState._markMap;
	/* rebuild one [source] page at a time until we are blocked at a destination or we reach the end of the region */
	for (page = startAddress; (NULL == earlyExit) && (page < high); page = (void *)((UDATA)page + sizeof_page)) {
		/* ensure that we can evacuate this page */
		IDATA evacuateIndex = pageIndex((J9Object *)page);
		void *target = _compactTable[evacuateIndex].getAddr();
		if (NULL != target) {
			/* this page has a real destination so see if it is sliding within this region or evacuating to another */
			MM_HeapRegionDescriptorVLHGC *targetRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(target);
			if (region == targetRegion) {
				/* we are sliding objects within a page */
				rebuildMarkMapInMovingPage(env, markMap, page);
			} else {
				/* we are evacuating so check the state of the target region and ensure that it isn't still waiting to evacuate the mark bits in the location where we want to write ours */
				void *rebuildMarkerInTarget = targetRegion->_compactData._nextRebuildCandidate;
				/* for now, we will state that there should be at least two pages at the target (although we could go tighter than this, so long as it is enough to account for object growth) */
				if ((((UDATA)target + (2*sizeof_page)) <= (UDATA)rebuildMarkerInTarget) || (rebuildMarkerInTarget == targetRegion->getHighAddress())) {
					rebuildMarkMapInMovingPage(env, markMap, page);
				} else {
					/* something is in the way so we will have to pick this up later */
					evacuationTarget = target;
					earlyExit = page;
				}
			}
		} else {
			/* this page didn't move so just clean up the tail marking */
			removeTailMarksInPage(env, markMap, page);
		}
	}
	if (NULL == earlyExit) {
		/* we are finished so ensure that page points at the end of the region, we have no evacuationTarget, and then record that we are done */
		Assert_MM_true(page == high);
		Assert_MM_true(NULL == evacuationTarget);
		region->_compactData._nextRebuildCandidate = page;
	} else {
		/* something was in the way so we need to record the address we will pick up from and then return the address we were blocked on */
		Assert_MM_true(NULL != evacuationTarget);
		region->_compactData._nextRebuildCandidate = earlyExit;
	}
	return evacuationTarget;
}

class MM_WriteOnceCompactorCheckMarkRoots : public MM_RootScanner {
private:
public:
	MM_WriteOnceCompactorCheckMarkRoots(MM_EnvironmentVLHGC *env) :
		MM_RootScanner(env, true)
	{
		_typeId = __FUNCTION__;
	}

	virtual void doSlot(J9Object** slot)
	{
	}

	virtual void doClass(J9Class *clazz)
	{
		GC_ClassIterator classIterator(_env, clazz);
		while (volatile j9object_t * slot = classIterator.nextSlot()) {
			/* discard volatile since we must be in stop-the-world mode */
			doSlot((j9object_t*)slot);
		}
	}

	virtual void doClassLoader(J9ClassLoader *classLoader)
	{
		if (J9_GC_CLASS_LOADER_DEAD != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			doSlot(&classLoader->classLoaderObject);
		}
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t object) {
		Assert_MM_unreachable();
	}
#endif /* J9VM_GC_FINALIZATION */
};

void
MM_WriteOnceCompactor::verifyHeapObjectSlot(J9Object* object)
{
	if ((object >= _heap->getHeapBase()) && (object < _heap->getHeapTop())) {
		Assert_MM_true (_cycleState._markMap->isBitSet(object));
	}
}

void
MM_WriteOnceCompactor::verifyHeapMixedObject(J9Object* objectPtr)
{
	GC_MixedObjectIterator it(_javaVM->omrVM, objectPtr);
	while (GC_SlotObject* slotObject = it.nextSlot()) {
		verifyHeapObjectSlot(slotObject->readReferenceFromSlot());
	}
}

void
MM_WriteOnceCompactor::verifyHeapArrayObject(J9Object* objectPtr)
{
	GC_PointerArrayIterator it(_javaVM, objectPtr);
	while (GC_SlotObject* slotObject = it.nextSlot()) {
		verifyHeapObjectSlot(slotObject->readReferenceFromSlot());
	}
}

void
MM_WriteOnceCompactor::verifyHeap(MM_EnvironmentVLHGC *env, bool beforeCompaction) {

	MM_WriteOnceCompactorCheckMarkRoots rootChecker(env);
	rootChecker.scanAllSlots(env);

	/* Check that the heap alignment is as compaction expects it. Compaction
	 * expects that the heap will split into a whole number of pages where
	 * each pages maps to 2 mark bit slots so heap alignment needs to be a multiple
	 * of the compaction page size
	 */
	assume0(_extensions->heapAlignment % (2 * J9BITS_BITS_IN_SLOT * sizeof(UDATA)) == 0);

	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		void *lowAddress = region->getLowAddress();
		void *highAddress = region->getHighAddress();
		MM_HeapMapIterator markedObjectIterator(_extensions,
												 _cycleState._markMap,
												(UDATA *)lowAddress,
												(UDATA *)highAddress);

		while (J9Object* objectPtr = markedObjectIterator.nextObject()) {
			switch(_extensions->objectModel.getScanType(objectPtr)) {
			case GC_ObjectModel::SCAN_MIXED_OBJECT_LINKED:
			case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
			case GC_ObjectModel::SCAN_MIXED_OBJECT:
			case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
			case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
			case GC_ObjectModel::SCAN_CLASS_OBJECT:
			case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
			case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
				verifyHeapMixedObject(objectPtr);
				break;

			case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
				verifyHeapArrayObject(objectPtr);
				break;

			case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
				/* nothing to do */
				break;

			default:
				Assert_MM_unreachable();
			}
		}
	}
}

void
MM_WriteOnceCompactor::recycleFreeRegionsAndFixFreeLists(MM_EnvironmentVLHGC *env)
{
	/* try walking the regions and recycling any regions with free pools */
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldCompact) {
			MM_MemoryPool *regionPool = region->getMemoryPool();
			Assert_MM_true(NULL != regionPool);
			Assert_MM_true(region->isCommitted());
			void *currentFreeBase = region->_compactData._compactDestination;
			regionPool->reset(MM_MemoryPool::forCompact);

			if (region->getLowAddress() == currentFreeBase) {
				/* if we are recycling a region, we better not have tried to make it a compact destination, migrating contexts */
				Assert_MM_true(NULL == region->_compactData._previousContext);
				/* cards are already cleaned when objects in a region move so we can safely recycle it (see moveObjects) */
				region->getSubSpace()->recycleRegion(env, region);
				/* mark bits will be cleared during the rebuilding phase */
			} else {
				static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._compactStats._survivorRegionCount += 1;

				if (NULL != region->_compactData._previousContext) {
					/* we migrated this region into a new context so ask its previous owner to migrate the contexts' views of the region's ownership to make the meta-structures consistent */
					region->_compactData._previousContext->migrateRegionToAllocationContext(region, region->_allocateData._owningContext);
					region->_compactData._previousContext = NULL;
				}
				void *highAddress = region->getHighAddress();
				UDATA currentFreeSize = (NULL == currentFreeBase) ? 0 : ((UDATA)highAddress - (UDATA)currentFreeBase);
				void *byteAfterFreeEntry = (void *)((UDATA)currentFreeBase + currentFreeSize);

				regionPool->reset(MM_MemoryPool::forCompact);
				if (currentFreeSize > regionPool->getMinimumFreeEntrySize()) {
					regionPool->recycleHeapChunk(env, currentFreeBase, byteAfterFreeEntry);
					regionPool->updateMemoryPoolStatistics(env, currentFreeSize, 1, currentFreeSize);
				} else {
					regionPool->abandonHeapChunk(currentFreeBase, byteAfterFreeEntry);
					regionPool->updateMemoryPoolStatistics(env, 0, 0, 0);
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::fixupArrayletLeafRegionSpinePointers(MM_EnvironmentVLHGC *env)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	
	while (NULL != (region = regionIterator.nextRegion())) {
		J9IndexableObject *spine = region->_allocateData.getSpine();
		
		if (NULL != spine) {
			Assert_MM_true(region->isArrayletLeaf());
			/* see if this spine has moved */
			J9IndexableObject *newSpine = (J9IndexableObject *)getForwardingPtr((J9Object *)spine);
			if (newSpine != spine) {
				MM_HeapRegionDescriptorVLHGC *spineRegion = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(spine);
				MM_HeapRegionDescriptorVLHGC *newSpineRegion = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(newSpine);

				/* Note that the previous spine region may be recycled while we are fixing up this region (recycling is done in parallel with
				 * this method) so we can't assert anything about the state of the previous region.
				 */
				Assert_MM_true( newSpineRegion->containsObjects() );
				if (spineRegion != newSpineRegion) {
					/* we need to move the leaf to another region's leaf list since its spine has moved */
					region->_allocateData.removeFromArrayletLeafList(env);
					region->_allocateData.addToArrayletLeafList(newSpineRegion);
				}
				region->_allocateData.setSpine(newSpine);
			}
		}
	}
}

void
MM_WriteOnceCompactor::fixupArrayletLeafRegionContentsAndObjectLists(MM_EnvironmentVLHGC* env)
{
	bool const compressed = env->compressObjectReferences();
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldFixup) {  
			Assert_MM_true(region->isArrayletLeaf());
			J9Object* spineObject = (J9Object*)region->_allocateData.getSpine();
			Assert_MM_true(NULL != spineObject);

			/* spine objects get fixed up later in fixupArrayletLeafRegionSpinePointers(), after a sync point */
			spineObject = getForwardingPtr(spineObject);

			fj9object_t* slotPointer = (fj9object_t*)region->getLowAddress();
			fj9object_t* endOfLeaf = (fj9object_t*)region->getHighAddress();
			while (slotPointer < endOfLeaf) {
				/* TODO: 4096 elements is an arbitrary number */
				fj9object_t* endPointer = GC_SlotObject::addToSlotAddress(slotPointer, 4096, compressed);
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					while (slotPointer < endPointer) {
						GC_SlotObject slotObject(_javaVM->omrVM, slotPointer);
						J9Object *pointer = slotObject.readReferenceFromSlot();
						if (NULL != pointer) {
							J9Object *forwardedPtr = getForwardingPtr(pointer);
							slotObject.writeReferenceToSlot(forwardedPtr);
							_interRegionRememberedSet->rememberReferenceForCompact(env, spineObject, forwardedPtr);
						}
						slotPointer = GC_SlotObject::addToSlotAddress(slotPointer, 1, compressed);
					}
				}
				slotPointer = endPointer;
			}
				
			/* prove we didn't miss anything at the end */
			Assert_MM_true(slotPointer == endOfLeaf);
		} else if (region->_compactData._shouldCompact) {
			if (!region->getUnfinalizedObjectList()->wasEmpty()) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					J9Object *pointer = region->getUnfinalizedObjectList()->getPriorList();
					while (NULL != pointer) {
						Assert_MM_true(region->isAddressInRegion(pointer));
						J9Object* forwardedPtr = getForwardingPtr(pointer);

						/* read the next link out of the moved copy of the object before we add it to the buffer */
						pointer = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);

						/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
						env->getGCEnvironment()->_unfinalizedObjectBuffer->add(env, forwardedPtr);
					}
				}
			}
			if (!region->getContinuationObjectList()->wasEmpty()) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					J9Object *pointer = region->getContinuationObjectList()->getPriorList();
					while (NULL != pointer) {
						Assert_MM_true(region->isAddressInRegion(pointer));
						J9Object* forwardedPtr = getForwardingPtr(pointer);

						/* read the next link out of the moved copy of the object before we add it to the buffer */
						pointer = _extensions->accessBarrier->getContinuationLink(forwardedPtr);

						/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
						env->getGCEnvironment()->_continuationObjectBuffer->add(env, forwardedPtr);
					}
				}
			}
		}
	}

	/* restore everything to a flushed state before exiting */
	env->getGCEnvironment()->_unfinalizedObjectBuffer->flush(env);
	env->getGCEnvironment()->_continuationObjectBuffer->flush(env);
}

void
MM_WriteOnceCompactor::clearMarkMapCompactSet(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap)
{
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldCompact) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				markMap->setBitsForRegion(env, region, true);
				Assert_MM_true((NULL == env->_cycleState->_externalCycleState) || !region->_nextMarkMapCleared);
			}
		}
	}
}

void
MM_WriteOnceCompactor::planCompaction(MM_EnvironmentVLHGC *env, UDATA *objectCount, UDATA *byteCount, UDATA *skippedObjectCount)
{
	UDATA regionSize = _regionManager->getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldCompact) {
			if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				Assert_MM_true(0 == region->_criticalRegionsInUse);
				/* tail-mark this region */
				UDATA postMoveRegionSizeInBytes = tailMarkObjectsInRegion(env, region);
				/* Store this information for projected live bytes stats purposes */
				region->_compactData._projectedLiveBytesRatio = (double) region->_projectedLiveBytes / (double) postMoveRegionSizeInBytes;
				/* only build a plan for regions which aren't dense or likely to grow to be larger than a region */
				if (postMoveRegionSizeInBytes < regionSize) {
					planRegion(env, region, postMoveRegionSizeInBytes, objectCount, byteCount, skippedObjectCount);
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::planRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *subAreaRegion, UDATA postMoveRegionSizeInBytes, UDATA *objectCount, UDATA *byteCount, UDATA *skippedObjectCount)
{
	/* when we first try to plan a region, it is not yet a destination */
	subAreaRegion->_compactData._isCompactDestination = false;
	subAreaRegion->_compactData._vineDepth = 0;

	void *sourceRegionBase = subAreaRegion->getLowAddress();
	void *sourceRegionTop = subAreaRegion->getHighAddress();
	void *copyStart = sourceRegionBase;
	UDATA targetSpaceRequired = postMoveRegionSizeInBytes;
	
	while (targetSpaceRequired > 0) {
		void *topEdge = NULL;
		void *targetPointer = NULL;
		bool final = getEvacuateExtent(env, targetSpaceRequired, subAreaRegion, &targetPointer, &topEdge);
		if (NULL == targetPointer) {
			/* the candidate is non-existent so install ourselves as the candidate and do a sliding compact */
			Assert_MM_true(final);
			Assert_MM_true(NULL == topEdge);
			UDATA bytesConsumedInMove = 0;
			doPlanSlide(env, sourceRegionBase, (J9Object *)copyStart, (J9Object *)sourceRegionTop, objectCount, &bytesConsumedInMove);
			*byteCount += bytesConsumedInMove;
			Assert_MM_true(bytesConsumedInMove <= targetSpaceRequired);
			targetSpaceRequired = 0;
		} else {
			/* evacuate into the given candidate */
			Assert_MM_true(NULL != topEdge);
			/* we consumed the space so do the planning and see if we need to keep trying to evacuate */
			void *freeChunk = targetPointer;
			UDATA bytesConsumedInMove = 0;
			copyStart = doPlanEvacuate(env, &freeChunk, topEdge, (J9Object *)copyStart, (J9Object *)sourceRegionTop, objectCount, &bytesConsumedInMove);
			*byteCount += bytesConsumedInMove;
			Assert_MM_true(bytesConsumedInMove <= targetSpaceRequired);
			Assert_MM_true(freeChunk <= topEdge);
			targetSpaceRequired -= bytesConsumedInMove;

			if (0 == targetSpaceRequired) {
				/* we have fully evacuated the region */
				Assert_MM_true(final);
				Assert_MM_true(NULL == copyStart);
			} else {
				Assert_MM_true(!final);
				Assert_MM_true(NULL != copyStart);
			}
		}
	}
}

bool
MM_WriteOnceCompactor::getEvacuateExtent(MM_EnvironmentVLHGC *env, UDATA targetSpaceRequired, MM_HeapRegionDescriptorVLHGC *subAreaRegion,
												void **targetPointer, void **topEdge)
{
	Assert_MM_true(targetSpaceRequired > 0);
	UDATA compactGroupIndex = MM_CompactGroupManager::getCompactGroupNumber(env, subAreaRegion);
	void *sourceRegionBase = subAreaRegion->getLowAddress();
	void *sourceRegionTop = subAreaRegion->getHighAddress();

	void *base = NULL;
	void *top = NULL;
	bool final = true;

	_compactGroupDestinations[compactGroupIndex].lock.acquire();

	if (NULL == _compactGroupDestinations[compactGroupIndex].head) {
		/* destination regions queue is empty - add this regions to queue to be a destination while sliding */
		subAreaRegion->_compactData._compactDestination = (void *)((UDATA)sourceRegionBase + targetSpaceRequired);

		Assert_MM_true(NULL != subAreaRegion->_compactData._compactDestination);
		Assert_MM_true(subAreaRegion->_compactData._compactDestination <= sourceRegionTop);
		Assert_MM_true(NULL == _compactGroupDestinations[compactGroupIndex].tail);

		subAreaRegion->_compactDestinationQueueNext = NULL;
		_compactGroupDestinations[compactGroupIndex].head = subAreaRegion;
		_compactGroupDestinations[compactGroupIndex].tail = subAreaRegion;
	} else {
		/* take first destination from the queue */
		MM_HeapRegionDescriptorVLHGC *destinationRegion = _compactGroupDestinations[compactGroupIndex].head;
		base = destinationRegion->_compactData._compactDestination;
		top = destinationRegion->getHighAddress();
		if ((void *)((UDATA)top - targetSpaceRequired) >= base) {
			/* it is enough space in destination region to take everything from source */
			top = (void *)((UDATA)base + targetSpaceRequired);
			/* source region would be fully evacuated */
			subAreaRegion->_compactData._compactDestination = sourceRegionBase;
			subAreaRegion->_projectedLiveBytes = 0;
			/* add source region to the end of the queue */
			subAreaRegion->_compactDestinationQueueNext = NULL;
			_compactGroupDestinations[compactGroupIndex].tail->_compactDestinationQueueNext = subAreaRegion;
			_compactGroupDestinations[compactGroupIndex].tail = subAreaRegion;
		} else {
			final = false;
		}

		destinationRegion->_compactData._compactDestination = top;

		if (top == destinationRegion->getHighAddress()) {
			/* all free space in destination region would be consumed - take it out from the queue */
			_compactGroupDestinations[compactGroupIndex].head = destinationRegion->_compactDestinationQueueNext;
			if (_compactGroupDestinations[compactGroupIndex].tail == destinationRegion) {
				/* removing last region from the queue */
				Assert_MM_true(NULL == _compactGroupDestinations[compactGroupIndex].head);
				_compactGroupDestinations[compactGroupIndex].tail = NULL;
			}
			destinationRegion->_compactDestinationQueueNext = NULL;
		}

		destinationRegion->_compactData._isCompactDestination = true;
		subAreaRegion->_compactData._vineDepth = OMR_MAX(subAreaRegion->_compactData._vineDepth, destinationRegion->_compactData._vineDepth + 1);
		/*
		 * the number of bytes allocated by objects in the region (top - base) is not very accurate here,
		 * it includes tail of the memory chunk may be not fully consumed
		 */
		destinationRegion->_projectedLiveBytes += (UDATA)subAreaRegion->_compactData._projectedLiveBytesRatio * ((UDATA)top - (UDATA)base);
	}

	_compactGroupDestinations[compactGroupIndex].lock.release();


	*targetPointer = base;
	*topEdge = top;
	return final;
}

J9Object*
MM_WriteOnceCompactor::doPlanEvacuate(MM_EnvironmentVLHGC *env, void **copyDestinationBase, void *copyDestinationTop, J9Object *firstToCopy, J9Object *endOfCopyBlock, UDATA *objectCount, UDATA *byteCount)
{
	/* we use the MM_HeapMapWordIterator in this function so ensure that it is safe (can't be tested during init since assertions don't work at that point) */
	Assert_MM_true(0 == (sizeof_page % J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP));
	/* the compact destination starts at the deadObject we were given but will be bumped along as we compact.  We need to save this back to deadObject, at the end, since it is an in/out parameter */
	J9Object *compactDestination = (J9Object *)*copyDestinationBase;
	UDATA compactDestinationSize = (UDATA)copyDestinationTop - (UDATA)compactDestination;
	UDATA movedObjectCount = 0;
	UDATA movedByteCount = 0;

	/* ensure that we are actually evacuating a region if we specified evacuate */
	Assert_MM_true(_regionManager->tableDescriptorForAddress(firstToCopy) != _regionManager->tableDescriptorForAddress(compactDestination));

	IDATA startPage = pageIndex(firstToCopy);
	IDATA finishPage = pageIndex(endOfCopyBlock);
	J9Object *firstObjectNotEvacuated = NULL;
	for (IDATA page = startPage; (NULL == firstObjectNotEvacuated) && (page < finishPage); page++) {
		UDATA estimatedSizeOfPage = _compactTable[page].getEstimatedSize();
		void *baseOfPage = pageStart(page);
		if (0 == estimatedSizeOfPage) {
			/* do nothing since this page is empty */
		} else if (estimatedSizeOfPage <= compactDestinationSize) {
			/* we are going to copy this page so find the first object for relocation data, count the number of objects, and account for the size of the page */
			/* first, find the object */
			J9Object *firstObject = NULL;
			for (UDATA bias = 0; (NULL == firstObject) && (bias < sizeof_page); bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
				void *baseAddress = (void *)((UDATA)baseOfPage + bias);
				MM_HeapMapWordIterator pagePieceIterator(_cycleState._markMap, baseAddress);
				firstObject = pagePieceIterator.nextObject();
			}
			Assert_MM_true(NULL != firstObject);
			/* now, look up how many objects there are */
			UDATA *markMapBits = (UDATA *)_cycleState._markMap->getMarkBits();
			UDATA startIndex = _cycleState._markMap->getSlotIndex(firstObject);
			UDATA endIndex = _cycleState._markMap->getSlotIndex(pageStart(page + 1));
			UDATA bitCount = 0;
			for (UDATA markMapIndex = startIndex; markMapIndex < endIndex; markMapIndex++) {
				bitCount += MM_Bits::populationCount(markMapBits[markMapIndex]);
			}
			/* this page will fit so commit the update to the destination geometry */
			saveForwardingPtr(firstObject, compactDestination);
			compactDestination = (J9Object *)((UDATA)compactDestination + estimatedSizeOfPage);
			compactDestinationSize -= estimatedSizeOfPage;
			movedObjectCount += (bitCount + 1) / 2;
			movedByteCount += estimatedSizeOfPage;
		} else {
			/* we can't copy this page so see if this page contains the first object which we failed to move */
			for (UDATA bias = 0; (NULL == firstObjectNotEvacuated) && (bias < sizeof_page); bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
				void *baseAddress = (void *)((UDATA)baseOfPage + bias);
				MM_HeapMapWordIterator pagePieceIterator(_cycleState._markMap, baseAddress);
				firstObjectNotEvacuated = pagePieceIterator.nextObject();
			}
		}
	}

	if (movedByteCount > 0) {
		/* we can also now write back our updated stats - these won't have changed if we didn't move any objects */
		if (compactDestinationSize == 0) {
			*copyDestinationBase = copyDestinationTop;
		} else {
			*copyDestinationBase = (void *)compactDestination;
		}
		/* update our stat for the number of objects moved */
		(*objectCount) += movedObjectCount;
		/* update our stat for the number of bytes copied */
		(*byteCount) += movedByteCount;
	}

	return firstObjectNotEvacuated;
}

void
MM_WriteOnceCompactor::doPlanSlide(MM_EnvironmentVLHGC *env, void *copyDestinationBase, J9Object *firstToCopy, J9Object *endOfCopyBlock, UDATA *objectCount, UDATA *byteCount)
{
	/* we use the MM_HeapMapWordIterator in this function so ensure that it is safe (can't be tested during init since assertions don't work at that point) */
	Assert_MM_true(0 == (sizeof_page % J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP));
	/* the compact destination starts at the deadObject we were given but will be bumped along as we compact.  We need to save this back to deadObject, at the end, since it is an in/out parameter */
	J9Object *compactDestination = (J9Object *)copyDestinationBase;
	UDATA movedObjectCount = 0;
	UDATA movedByteCount = 0;

	MM_HeapMapIterator markedObjectIterator(_extensions, _cycleState._markMap, (UDATA *)firstToCopy, (UDATA *)pageStart(pageIndex(endOfCopyBlock)), false);

	J9Object* objectPtr = NULL;
	J9Object* nextObject = NULL;
	IDATA previousPageIndex = -1;
	UDATA estimatedPageSize = 0;
	UDATA previousPageSize = 0;
	for (objectPtr = markedObjectIterator.nextObject(); NULL != objectPtr; objectPtr = nextObject) {
		nextObject = markedObjectIterator.nextObject();
		IDATA objectIndex = pageIndex(objectPtr);
		IDATA nextIndex = pageIndex(nextObject);
		if (objectIndex != previousPageIndex) {
			Assert_MM_true(estimatedPageSize == previousPageSize);
			previousPageIndex = objectIndex;
			estimatedPageSize = _compactTable[objectIndex].getEstimatedSize();
			previousPageSize = 0;
		}
		UDATA postMoveObjectSize = 0;
		/* if there is a compact page boundary between objectPtr and nextObject, nextObject is a head pointer.  Otherwise, it is a tail */
		if (objectIndex == nextIndex) {
			J9Object *tail = nextObject;
			nextObject = markedObjectIterator.nextObject();
			postMoveObjectSize = (UDATA)tail - (UDATA)objectPtr + J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT + (_compactTable[objectIndex].isGrowBitSet(objectPtr) ? _objectAlignmentInBytes : 0);
		} else {
			postMoveObjectSize = estimatedPageSize - previousPageSize;
		}
		Assert_MM_true(0 != postMoveObjectSize);

		if(compactDestination == objectPtr) {
			/* this object didn't move - update the new destination */
			if (_compactTable[objectIndex].isGrowBitSet(objectPtr)) {
				_compactTable[objectIndex].clearGrowBit(objectPtr);
				estimatedPageSize -= _objectAlignmentInBytes;
				postMoveObjectSize -= _objectAlignmentInBytes;
			}
		} else {
			/* save out the forwarding pointer if the object moved (otherwise, there is no point and we would confuse an assertion in getForwardingPtr which assures that an object never moves on top of itself) */
			Assert_MM_true(objectPtr > compactDestination);
			saveForwardingPtr(objectPtr, compactDestination);
			
			/* since the object moved, update our stats */
			movedObjectCount += 1;
			movedByteCount += postMoveObjectSize;
		}
		/* update our next destination to after where this object will be moved */
		compactDestination = (J9Object*)((UDATA)compactDestination + postMoveObjectSize);
		previousPageSize += postMoveObjectSize;
	}
	/* if the destination, after planning every object, extends beyond the end of the extent we were sliding then we are going to corrupt the next extent (which would be the next region) and this region should never have been compacted */
	Assert_MM_true(compactDestination <= endOfCopyBlock);

	if (movedByteCount > 0) {
		/* update our stat for the number of objects moved */
		(*objectCount) += movedObjectCount;
		/* update our stat for the number of bytes copied */
		(*byteCount) += movedByteCount;
	}
}

void
MM_WriteOnceCompactor::setupMoveWorkStack(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(env->isMainThread());
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	MM_HeapRegionDescriptorVLHGC *endOfQueue = NULL;
	MM_HeapRegionDescriptorVLHGC *endOfFixupQueue = NULL;
	Assert_MM_true(0 == _threadsWaiting);
	_moveFinished = false;
	_rebuildFinished = false;
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldCompact) {
			if (NULL == endOfQueue) {
				endOfQueue = region;
				_readyWorkListHighPriority = region;
			} else {
				endOfQueue->_compactData._nextInWorkList = region;
				endOfQueue = region;
			}
		} else if (region->containsObjects()) {
			/* non-compacted regions which do contain objects need to have their cards cleaned */
			if (NULL == endOfFixupQueue) {
				endOfFixupQueue = region;
				_fixupOnlyWorkList = region;
			} else {
				endOfFixupQueue->_compactData._nextInWorkList = region;
				endOfFixupQueue = region;
			}
		}
	}
}

MM_HeapRegionDescriptorVLHGC *
MM_WriteOnceCompactor::popWork(MM_EnvironmentVLHGC *env)
{
	omrthread_monitor_enter(_workListMonitor);
	while ((NULL == _readyWorkListHighPriority) && (NULL == _readyWorkList) && (NULL == _fixupOnlyWorkList) && !_moveFinished) {
		_threadsWaiting += 1;
		if (env->_currentTask->getThreadCount() == _threadsWaiting) {
			_moveFinished = true;
			if (_extensions->tarokEnableExpensiveAssertions) {
				/* verify that move didn't discard or otherwise lose any regions.  Every region in the compact set should now be in the rebuild work list */
				MM_HeapRegionDescriptorVLHGC *region = NULL;
				GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
				UDATA compactRegions = 0;
				while (NULL != (region = regionIterator.nextRegion())) {
					if (region->_compactData._shouldCompact) {
						compactRegions += 1;
					}
				}
				region = _rebuildWorkList;
				UDATA listRegions = 0;
				while (NULL != region) {
					listRegions += 1;
					Assert_MM_true(NULL == region->_compactData._blockedList);
					region = region->_compactData._nextInWorkList;
				}
				Assert_MM_true(compactRegions == listRegions);
			}
			omrthread_monitor_notify_all(_workListMonitor);
		} else {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			U_64 waitStartTime = j9time_hires_clock();

			omrthread_monitor_wait(_workListMonitor);

			U_64 waitEndTime = j9time_hires_clock();
			env->_compactVLHGCStats.addToMoveStallTime(waitStartTime, waitEndTime);
		}
		Assert_MM_true(_threadsWaiting > 0);
		_threadsWaiting -= 1;
	}
	MM_HeapRegionDescriptorVLHGC *region = popNextRegionFromWorkStack(&_readyWorkListHighPriority);
	if (NULL == region) {
		region = popNextRegionFromWorkStack(&_readyWorkList);
		if (NULL == region) {
			region = popNextRegionFromWorkStack(&_fixupOnlyWorkList);
			/* if we are about to return empty-handed, we better be done */
			if (NULL == region) {
				Assert_MM_true(_moveFinished);
			}
		}
	}
	omrthread_monitor_exit(_workListMonitor);
	return region;
}

void
MM_WriteOnceCompactor::pushMoveWork(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *finishedRegion, void *evacuationTarget, UDATA evacuationSize)
{
	/* this should not be part of a waiting work list if we are actively using it */
	Assert_MM_true(NULL == finishedRegion->_compactData._nextInWorkList);
	omrthread_monitor_enter(_workListMonitor);
	/* drop this element from the work list if it is done */
	void *nextEvacuationCandidate = finishedRegion->_compactData._nextEvacuationCandidate;
	if (nextEvacuationCandidate < finishedRegion->getHighAddress()) {
		/* we aren't finished with this entry but we must have returned it here, for some reason,
		 * so look up which entry is blocking it and enqueue it on that blocked list (unless that
		 * entry has since become complete - in which case we put the entry back in the ready list)
		 */
		/* if we setting this regions aside and it isn't completely finished, we must have specified the target upon which we were blocked */
		Assert_MM_true(NULL != evacuationTarget);
		/* look up the region into which we were attempting to evacuate objects */
		MM_HeapRegionDescriptorVLHGC *targetRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(evacuationTarget);
		/* we can't push a region which is blocked on itself */
		Assert_MM_true(targetRegion != finishedRegion);
		/* look up how far into the target region the evacuation has progressed (since it is possible that it proceeded past our blocking point before we picked up the monitor) */
		void *targetProgress = targetRegion->_compactData._nextEvacuationCandidate;
		if (targetProgress < (void *)((UDATA)evacuationTarget + evacuationSize)) {
			/* if the target region still hasn't progressed past the end of the extent we would require (our target plus the number of bytes we need), then put finishedRegion on its blocked list */
			Assert_MM_true(targetProgress != targetRegion->getHighAddress());
			finishedRegion->_compactData._nextInWorkList = targetRegion->_compactData._blockedList;
			targetRegion->_compactData._blockedList = finishedRegion;
		} else {
			/* the target has proceeded far enough while we acquired the monitor that it is no longer blocked so add it to the work stack */
			pushRegionOntoWorkStack(&_readyWorkList, &_readyWorkListHighPriority, finishedRegion);
		}
	} else {
		/* we are going to drop this entry, since it is fully evacuated, but first make sure that we enqueue all the items from its blocked list */
		MM_HeapRegionDescriptorVLHGC *blocked = finishedRegion->_compactData._blockedList;
		while (NULL != blocked) {
			MM_HeapRegionDescriptorVLHGC *next = blocked->_compactData._nextInWorkList;
			pushRegionOntoWorkStack(&_readyWorkList, &_readyWorkListHighPriority, blocked);
			blocked = next;
		}
		finishedRegion->_compactData._blockedList = NULL;
		/* now that it is dropped from the move queue, add it to the rebuild queue */
		finishedRegion->_compactData._nextInWorkList = _rebuildWorkList;
		_rebuildWorkList = finishedRegion;
	}
	if (((NULL != _readyWorkListHighPriority) || (NULL != _readyWorkList)) && (_threadsWaiting > 0)) {
		/* there is work so wake up the next thread waiting */
		omrthread_monitor_notify(_workListMonitor);
	}
	omrthread_monitor_exit(_workListMonitor);
}

MM_HeapRegionDescriptorVLHGC *
MM_WriteOnceCompactor::popRebuildWork(MM_EnvironmentVLHGC *env)
{
	omrthread_monitor_enter(_workListMonitor);
	while ((NULL == _rebuildWorkListHighPriority) && (NULL == _rebuildWorkList) && !_rebuildFinished) {
		_threadsWaiting += 1;
		if (env->_currentTask->getThreadCount() == _threadsWaiting) {
			_rebuildFinished = true;
			if (_extensions->tarokEnableExpensiveAssertions) {
				/* ensure that none of the regions in the compact set are still in a work list and that no regions are blocked */
				MM_HeapRegionDescriptorVLHGC *region = NULL;
				GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
				while (NULL != (region = regionIterator.nextRegion())) {
					if (region->_compactData._shouldCompact) {
						Assert_MM_true(NULL == region->_compactData._nextInWorkList);
						Assert_MM_true(NULL == region->_compactData._blockedList);
					}
				}
			}
			omrthread_monitor_notify_all(_workListMonitor);
		} else {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			U_64 waitStartTime = j9time_hires_clock();

			omrthread_monitor_wait(_workListMonitor);

			U_64 waitEndTime = j9time_hires_clock();
			env->_compactVLHGCStats.addToRebuildStallTime(waitStartTime, waitEndTime);
		}
		Assert_MM_true(_threadsWaiting > 0);
		_threadsWaiting -= 1;
	}
	MM_HeapRegionDescriptorVLHGC *region = popNextRegionFromWorkStack(&_rebuildWorkListHighPriority);
	if (NULL == region) {
		region = popNextRegionFromWorkStack(&_rebuildWorkList);
		/* if we are about to return empty-handed, we better be done */
		if (NULL == region) {
			Assert_MM_true(_rebuildFinished);
		}
	}
	omrthread_monitor_exit(_workListMonitor);
	return region;
}

void
MM_WriteOnceCompactor::pushRebuildWork(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *finishedRegion, void *evacuationTarget)
{
	/* rebuildMarkbitsInRegion assumes that one source page could consume more than one but less than 2 destination pages so allow 2 pages of clearance */
	UDATA evacuationSize = (2 * sizeof_page);
	/* this should not be part of a waiting work list if we are actively using it */
	Assert_MM_true(NULL == finishedRegion->_compactData._nextInWorkList);
	omrthread_monitor_enter(_workListMonitor);
	/* drop this element from the work list if it is done */
	void *nextRebuildCandidate = finishedRegion->_compactData._nextRebuildCandidate;
	if (nextRebuildCandidate < finishedRegion->getHighAddress()) {
		/* we aren't finished with this entry but we must have returned it here, for some reason,
		 * so look up which entry is blocking it and enqueue it on that blocked list (unless that
		 * entry has since become complete - in which case we put the entry back in the ready list)
		 */
		/* if we aren't finished rebuilding this region, it must be blocked on something, so ensure that it has a non-NULL target */
		Assert_MM_true(NULL != evacuationTarget);
		/* look up the region upon which we are blocked */
		MM_HeapRegionDescriptorVLHGC *targetRegion = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(evacuationTarget);
		/* we can't push an entry which is blocked on itself */
		Assert_MM_true(targetRegion != finishedRegion);
		/* see where the progress of the target is in terms of what it will be rebuilding next (it might have progressed past evacuationTarget
		 * before we got the monitor so we might be able to return this work immediately to the ready stack)
		 */
		void *targetProgress = targetRegion->_compactData._nextRebuildCandidate;
		/* targetHighEdge is a conservative estimate of the end of the heap area for which we might need to update the mark map for at least one page of movement (obviously must be capped at the top of the region) */
		void *targetHighEdge = OMR_MIN(targetRegion->getHighAddress(), (void *)((UDATA)evacuationTarget + evacuationSize));
		/* determine if we are actually blocked on our target or if it has progressed since we thought we had to stop */
		if (targetProgress < targetHighEdge) {
			/* we are actually blocked on our target so just make sure it isn't finished (which would mean we stored an invalid target) and then add us to its blocked queue */
			Assert_MM_true(targetProgress != targetRegion->getHighAddress());
			finishedRegion->_compactData._nextInWorkList = targetRegion->_compactData._blockedList;
			targetRegion->_compactData._blockedList = finishedRegion;
		} else {
			/* we can immediately resume work on this region so just push it on the work stack */
			pushRegionOntoWorkStack(&_rebuildWorkList, &_rebuildWorkListHighPriority, finishedRegion);
		}
	} else {
		/* we are going to drop this entry, since it is fully rebuilt, but first make sure that we enqueue all the items from its blocked list */
		MM_HeapRegionDescriptorVLHGC *blocked = finishedRegion->_compactData._blockedList;
		while (NULL != blocked) {
			MM_HeapRegionDescriptorVLHGC *next = blocked->_compactData._nextInWorkList;
			pushRegionOntoWorkStack(&_rebuildWorkList, &_rebuildWorkListHighPriority, blocked);
			blocked = next;
		}
		finishedRegion->_compactData._blockedList = NULL;
	}
	if (((NULL != _rebuildWorkListHighPriority) || (NULL != _rebuildWorkList)) && (_threadsWaiting > 0)) {
		/* there is work so wake up the next thread waiting */
		omrthread_monitor_notify(_workListMonitor);
	}
	omrthread_monitor_exit(_workListMonitor);
}

void
MM_WriteOnceCompactor::rebuildMarkMapInMovingPage(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap, void *pageBase)
{
	/* the page starts on a head but each J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP may not so remember whether we are expecting a tail or head bit */
	bool skipTail = false;
	IDATA index = pageIndex((J9Object *)pageBase);
	J9Object *targetBase = _compactTable[index].getAddr();
	J9Object *originalTarget = targetBase;
	IDATA originalTargetIndex = pageIndex(originalTarget);
	UDATA slotIndex = markMap->getSlotIndex((J9Object *)pageBase);
	J9Object *previousHead = NULL;
	bool useAtomicFlush = true;
	UDATA targetWordIndex = UDATA_MAX;
	UDATA targetWord = 0;
	for (UDATA bias = 0; bias < sizeof_page; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
		void *baseAddress = (void *)((UDATA)pageBase + bias);
		/* read the word and clear it in case the objects moved into this mark word.  We will manually feed this mark word into the iterator so we won't be modifying the data under the iterator */
		UDATA markWord = markMap->getSlot(slotIndex);
		markMap->setSlot(slotIndex, 0);
		/* walk the objects in the mark word we read */
		MM_HeapMapWordIterator iterator(markWord, baseAddress);
		J9Object *object = NULL;
		if (skipTail) {
			J9Object *tail = iterator.nextObject();
			if (NULL != tail) {
				UDATA size = ((UDATA)tail - (UDATA)previousHead) + (_compactTable[index].isGrowBitSet(previousHead) ? _objectAlignmentInBytes : 0);
				targetBase = (J9Object *)((UDATA)targetBase + size + J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT);
				skipTail = false;
			}
		}
		while (NULL != (object = iterator.nextObject())) {
			previousHead = object;
			if ((object <= originalTarget) && (pageIndex(object) == originalTargetIndex)) {
				targetBase = object;
			}
			J9Object *newLocation = targetBase;
			Assert_MM_false(markMap->isBitSet(newLocation));
			UDATA newTargetSlotIndex = 0;
			UDATA newTargetSlotMask = 0;
			markMap->getSlotIndexAndMask(newLocation, &newTargetSlotIndex, &newTargetSlotMask);
			Assert_MM_true(0 == (newTargetSlotMask & markMap->getSlot(newTargetSlotIndex)));
			if (newTargetSlotIndex != targetWordIndex) {
				if ((UDATA_MAX != targetWordIndex) && (0 != targetWord)) {
					/* write back the word */
					if (useAtomicFlush) {
						markMap->atomicSetSlot(targetWordIndex, targetWord);
						useAtomicFlush = false;
					} else {
						markMap->setSlot(targetWordIndex, targetWord);
					}
					targetWord = 0;
				}
				targetWordIndex = newTargetSlotIndex;
			}
			targetWord |= newTargetSlotMask;
			/* skip the tail */
			J9Object *tail = iterator.nextObject();
			if (NULL != tail) {
				UDATA size = ((UDATA)tail - (UDATA)previousHead) + (_compactTable[index].isGrowBitSet(previousHead) ? _objectAlignmentInBytes : 0);
				targetBase = (J9Object *)((UDATA)targetBase + size + J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT);
			}
			skipTail = (NULL == tail);
		}
		slotIndex += 1;
	}
	if (0 != targetWord) {
		markMap->atomicSetSlot(targetWordIndex, targetWord);
	}
}
void
MM_WriteOnceCompactor::removeTailMarksInPage(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap, void *pageBase)
{
	UDATA slotIndex = markMap->getSlotIndex((J9Object *)pageBase);
	bool isHead = true;
	for (UDATA bias = 0; bias < sizeof_page; bias += J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP) {
		UDATA markMapWord = markMap->getSlot(slotIndex);
		UDATA newWord = 0;
		while (0 != markMapWord) {
			UDATA leadingZeroes = MM_Bits::leadingZeroes(markMapWord);
			UDATA bit = ((UDATA)1 << leadingZeroes);
			markMapWord &= ~bit;
			if (isHead) {
				newWord |= bit;
			}
			isHead = !isHead;
		}
		markMap->setSlot(slotIndex, newWord);
		slotIndex += 1;
	}
}

MM_HeapRegionDescriptorVLHGC *
MM_WriteOnceCompactor::popNextRegionFromWorkStack(MM_HeapRegionDescriptorVLHGC **workStackBase)
{
	MM_HeapRegionDescriptorVLHGC *region = *workStackBase;
	if (NULL != region) {
		*workStackBase = region->_compactData._nextInWorkList;
		region->_compactData._nextInWorkList = NULL;
		if ((NULL != *workStackBase) && (_threadsWaiting > 0)) {
			/* there is still more work so wake someone up */
			omrthread_monitor_notify(_workListMonitor);
		}
	}
	return region;
}

void
MM_WriteOnceCompactor::pushRegionOntoWorkStack(MM_HeapRegionDescriptorVLHGC **workStackBase, MM_HeapRegionDescriptorVLHGC **workStackBaseHighPriority, MM_HeapRegionDescriptorVLHGC *region)
{
	if (region->_compactData._isCompactDestination) {
		region->_compactData._nextInWorkList = *workStackBase;
		*workStackBase = region;
	} else {
		region->_compactData._nextInWorkList = *workStackBaseHighPriority;
		*workStackBaseHighPriority = region;
	}
}

#if defined(J9VM_GC_FINALIZATION)
void
MM_WriteOnceCompactor::fixupFinalizableObjects(MM_EnvironmentVLHGC *env)
{
	GC_FinalizeListManager * finalizeListManager = _extensions->finalizeListManager;

	/* walk finalizable objects loaded by the system class loader */
	j9object_t systemObject = finalizeListManager->resetSystemFinalizableObjects();
	if (NULL != systemObject) {
		fixupFinalizableList(env, systemObject);
	}

	/* walk finalizable objects loaded by the all other class loaders */
	j9object_t defaultObject = finalizeListManager->resetDefaultFinalizableObjects();
	if (NULL != defaultObject) {
		fixupFinalizableList(env, defaultObject);
	}


	{
		/* walk reference objects */
		GC_FinalizableReferenceBuffer referenceBuffer(_extensions);
		j9object_t referenceObject = finalizeListManager->resetReferenceObjects();
		while (NULL != referenceObject) {
			J9Object* forwardedPtr = getForwardingPtr(referenceObject);
			/* read the next link out of the moved copy of the object before we add it to the buffer */
			referenceObject = _extensions->accessBarrier->getReferenceLink(forwardedPtr);
			/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
			referenceBuffer.add(env, forwardedPtr);
		}
		referenceBuffer.flush(env);
	}
}

void
MM_WriteOnceCompactor::fixupFinalizableList(MM_EnvironmentVLHGC *env, j9object_t headObject)
{
	GC_FinalizableObjectBuffer objectBuffer(_extensions);

	while (NULL != headObject) {
		J9Object* forwardedPtr = getForwardingPtr(headObject);
		/* read the next link out of the moved copy of the object before we add it to the buffer */
		headObject = _extensions->accessBarrier->getFinalizeLink(forwardedPtr);
		/* store the object in this thread's buffer. It will be flushed to the appropriate list when necessary. */
		objectBuffer.add(env, forwardedPtr);
	}

	objectBuffer.flush(env);
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_WriteOnceCompactor::rememberClassLoaders(MM_EnvironmentVLHGC *env)
{
	/* TODO: ideally we should have per region lists of class loaders and call this from clearMarkMapCompactSet(), improving its parallelism */
	Assert_MM_true(NULL != env->_cycleState->_externalCycleState);
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
		J9ClassLoader *classLoader = NULL;
		while ((classLoader = classLoaderIterator.nextSlot()) != NULL) {
			if (J9_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER)) {
				/* Anonymous classloader should be scanned on level of classes */
				GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
				J9MemorySegment *segment = NULL;
				while (NULL != (segment = segmentIterator.nextSegment())) {
					GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
					J9Class *clazz = NULL;
					while (NULL != (clazz = classHeapIterator.nextClass())) {
						Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));
						Assert_MM_true(!J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassGCScanned));
						J9Object* classObject = clazz->classObject;
						Assert_MM_true(NULL != classObject);
						MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(classObject);
						if (region->_compactData._shouldCompact && _nextMarkMap->isBitSet(classObject)) {
							J9CLASS_EXTENDED_FLAGS_SET(clazz, J9ClassGCScanned);
						}
					}
				}
			} else {
				Assert_MM_true(0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_REMEMBERED));
				J9Object* classLoaderObject = classLoader->classLoaderObject;
				if (NULL != classLoaderObject) {
					MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(classLoaderObject);
					if (region->_compactData._shouldCompact && _nextMarkMap->isBitSet(classLoaderObject)) {
						classLoader->gcFlags |= J9_GC_CLASS_LOADER_REMEMBERED;
					}
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::rebuildNextMarkMapFromClassLoaders(MM_EnvironmentVLHGC *env)
{
	/* TODO: ideally we should have per region lists of class loaders and parallelize this better */
	Assert_MM_true(NULL != env->_cycleState->_externalCycleState);
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
		J9ClassLoader *classLoader = NULL;
		while ((classLoader = classLoaderIterator.nextSlot()) != NULL) {
			if (J9_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER)) {
				/* Anonymous classloader should be scanned on level of classes */
				GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
				J9MemorySegment *segment = NULL;
				while (NULL != (segment = segmentIterator.nextSegment())) {
					GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
					J9Class *clazz = NULL;
					while (NULL != (clazz = classHeapIterator.nextClass())) {
						Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));
						if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassGCScanned)) {
							J9Object* classObject = clazz->classObject;
							Assert_MM_true(NULL != classObject);
							_nextMarkMap->atomicSetBit(classObject);
							/* dirty card to ensure that this class loader is rescanned so its classes get marked */
							_extensions->cardTable->dirtyCardWithValue(env, classObject, CARD_GMP_MUST_SCAN);
							J9CLASS_EXTENDED_FLAGS_CLEAR(clazz, J9ClassGCScanned);
						}
					}
				}
			} else {
				if (J9_GC_CLASS_LOADER_REMEMBERED == (classLoader->gcFlags & J9_GC_CLASS_LOADER_REMEMBERED)) {
					J9Object* classLoaderObject = classLoader->classLoaderObject;
					Assert_MM_true(NULL != classLoaderObject);
					_nextMarkMap->atomicSetBit(classLoaderObject);
					/* dirty card to ensure that this class loader is rescanned so its classes get marked */
					_extensions->cardTable->dirtyCardWithValue(env, classLoaderObject, CARD_GMP_MUST_SCAN);
					classLoader->gcFlags &= ~J9_GC_CLASS_LOADER_REMEMBERED;
				}
			}
		}
	}
}

void
MM_WriteOnceCompactor::clearClassLoaderRememberedSetsForCompactSet(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(_extensions->tarokEnableIncrementalClassGC);
	MM_ClassLoaderRememberedSet *classLoaderRememberedSet = _extensions->classLoaderRememberedSet;
	classLoaderRememberedSet->resetRegionsToClear(env);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	GC_HeapRegionIteratorVLHGC regionIterator(_regionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->_compactData._shouldCompact) {
			classLoaderRememberedSet->prepareToClearRememberedSetForRegion(env, region);
		}
	}
	classLoaderRememberedSet->clearRememberedSets(env);
}

#endif /* J9VM_GC_MODRON_COMPACTION */
