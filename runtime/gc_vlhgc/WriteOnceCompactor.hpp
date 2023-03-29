
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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#ifndef WRITEONCECOMPACTOR_HPP_
#define WRITEONCECOMPACTOR_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"

#include "CycleState.hpp"
#include "Debug.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "ParallelTask.hpp"
#include "ContinuationObjectBufferVLHGC.hpp"
#include "ContinuationObjectList.hpp"

#if defined(J9VM_GC_MODRON_COMPACTION)

class MM_AllocateDescription;
class MM_WriteOnceCompactor;
class MM_ParallelDispatcher;
class MM_Heap;
class MM_HeapRegionDescriptorVLHGC;
class MM_MemoryPool;
class MM_MemorySubSpace;

struct J9MM_FixupCache;
struct J9MM_FixupTuple;


/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelWriteOnceCompactTask : public MM_ParallelTask
{
private:
	MM_WriteOnceCompactor *_compactScheme;
	MM_CycleState *_cycleState;  /**< Current cycle state used for tasks dispatched */
	MM_MarkMap *_nextMarkMap;	/**< The next mark map which will be passed to the compactor (it uses it as temporary storage for fixup data) */

public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_COMPACT; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

	void mainSetup(MM_EnvironmentBase *env);
	void mainCleanup(MM_EnvironmentBase *env);

	/**
	 * Create an ParallelCompactTask object.
	 */
	MM_ParallelWriteOnceCompactTask(MM_EnvironmentBase *env, MM_ParallelDispatcher *dispatcher, MM_WriteOnceCompactor *compactScheme, MM_CycleState *cycleState, MM_MarkMap *nextMarkMap)
		: MM_ParallelTask(env, dispatcher)
		, _compactScheme(compactScheme)
		, _cycleState(cycleState)
		, _nextMarkMap(nextMarkMap)
	{
		_typeId = __FUNCTION__;
	};
};

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_WriteOnceCompactor : public MM_BaseVirtual
{
	/* Data Members */
private:
	J9JavaVM * const _javaVM;  /**< Cached pointer to the common JavaVM instance */
	MM_GCExtensions * const _extensions;  /**< Cached pointer to the common GCExtensions instance */
	MM_Heap * const _heap;	/**< Cached pointer to the common Heap instance */
	MM_ParallelDispatcher * const _dispatcher;  /**< Cached pointer to the common Dispatcher instance */
	MM_HeapRegionManager * const _regionManager; /**< The region manager which holds the MM_HeapRegionDescriptor instances which manage the properties of the regions */
	void * const _heapBase;	/**< The cached value of the lowest byte address which can be occupied by the heap */
	void * const _heapTop;	/**< The cached value of the lowest byte address after the heap */
	class WriteOnceCompactTableEntry *_compactTable;
	MM_CycleState _cycleState;  /**< Current cycle state information used to formulate receiver state for any operations  */
	MM_MarkMap *_nextMarkMap;  /**< The next mark map (used as temporary storage for fixup data) */
	MM_InterRegionRememberedSet *_interRegionRememberedSet;	/**< A cached pointer to the  inter-region reference tracking  */
	omrthread_monitor_t _workListMonitor;  /**< The monitor used to control work sharing of object movement/fixup tasks */
	MM_HeapRegionDescriptorVLHGC *_readyWorkList;  /**< The root of the list of regions which can have some work done on them */
	MM_HeapRegionDescriptorVLHGC *_readyWorkListHighPriority;  /**< Like _readyWorkList but is higher priority as it only contains regions which are compact destinations (that is, they block other move operations) */
	MM_HeapRegionDescriptorVLHGC *_fixupOnlyWorkList;  /**< The root of the list of regions which must have their cards cleaned in order to fixup references into the compact set */
	MM_HeapRegionDescriptorVLHGC *_rebuildWorkList;  /**< The root of the list of regions which must have their previous mark map extents rebuilt (this list is built as the object movement phase completes) */
	MM_HeapRegionDescriptorVLHGC *_rebuildWorkListHighPriority;	/**< Like _rebuildWorkList but is higher priority as it only contains regions which are compact destinations (that is, they block other rebuild operations) */
	UDATA _threadsWaiting;  /**< The number of threads waiting for work on _workListMonitor */
	bool _moveFinished;  /**< A flag set when all move work is done to allow all threads waiting for work to exit the monitor */
	bool _rebuildFinished;  /**< A flag set when all mark map rebuilding work is done to allow all threads waiting for work to exit the monitor */
	UDATA _lockCount;  /**< Number of locks initialized for compact groups, based on NUMA. Stored for control to make sure that we deallocate same number that we allocated in initialize. */

	class MM_CompactGroupDestinations
	{
		/* Fields */
	public:
		MM_HeapRegionDescriptorVLHGC *head; /**< Compact Group Destinations list header */
		MM_HeapRegionDescriptorVLHGC *tail; /**< Compact Group Destinations list tail */
		MM_LightweightNonReentrantLock lock; /**< Compact Group Destinations list lock */
	protected:
	private:
		/* Methods */
	public:
	protected:
	private:
	};

	MM_CompactGroupDestinations *_compactGroupDestinations; /**< An array of list headers max_age + 1 elements */
	const UDATA _regionSize;	/**< A cache of the region size used in XOR optimizations */
	const UDATA _objectAlignmentInBytes;	/**< Run-time objects alignment in bytes */

protected:
public:
	/*
	 * Page represents space can be marked by one slot(UDATA) of Compressed Mark Map
	 * In Compressed Mark Map one bit represents twice more space then one bit of regular Mark Map
	 * So, sizeof_page should be double of number of bytes represented by one UDATA in Mark Map
	 */
	enum { sizeof_page = 2 * J9MODRON_HEAP_BYTES_PER_HEAPMAP_SLOT };

	/* Member Functions */
private:
	/**
	 * Tail-mark all marked objects in the given region, in _cycleState._markMap.
	 * This is called in parallel but each thread operates on a different region
	 * @param env[in] A GC thread
	 * @param region[in] A region in the compact set
	 * @return The total size of all the objects in region, in bytes, if they were to move
	 */
	UDATA tailMarkObjectsInRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Determines the number of bytes occupied by the object occurring between slidingTarget and oldPointer.
	 * This is used to calculate oldPointer's new location since it will be this number of bytes from a compaction
	 * target address.
	 * @param oldPointer[in] The object pointer being fixed up
	 * @param slidingTarget[in] The base from which to calculate the number of bytes
	 * @return The number of bytes occupied by objects between slidingTarget and oldPointer
	 */
	UDATA bytesAfterSlidingTargetToLocateObject(J9Object *oldPointer, J9Object *slidingTarget) const;

	/**
	 * A wrapper around the getForwardedPtr call which will try to apply its cache, first, and return immediately if the objectPtr hits
	 * in the cache.  If it isn't in the cache, this method calls through to getForwardedPtr.
	 * @note inlined in the implementation so this method MUST be private
	 * @param env[in] A GC thread
	 * @param objectPtr[in] The pointer to look up to see its new, forwarded, location
	 * @param cache[in] The cache to use for fast resolve attempts for objectPtr (may be NULL)
	 * @return The forwarded location of the given objectPtr
	 */
	J9Object *getForwardWrapper(MM_EnvironmentVLHGC *env, J9Object *objectPtr, J9MM_FixupCache *cache);

	/**
	 * Initializes the _compactData fields of regions in the compact set.
	 * Note that this method is executed on only one thread while other initialization is being run by other threads.
	 * @param env[in] A GC thread
	 */
	void initRegionCompactDataForCompactSet(MM_EnvironmentVLHGC *env);

	void saveForwardingPtr(J9Object* objectPtr, J9Object* forwardingPtr);

	void fixupRoots(MM_EnvironmentVLHGC *env);

	/** flush RS Lists for Compact Set and dirty card table
	 */
	void flushRememberedSetIntoCardTable(MM_EnvironmentVLHGC *env);
	/**
	 * Perform fixup for a single object.
	 */
	MMINLINE void preObjectMove(MM_EnvironmentVLHGC* env, J9Object *objectPtr, UDATA *objectSizeAfterMove);
	MMINLINE void postObjectMove(MM_EnvironmentVLHGC* env, J9Object *newLocation, J9Object *objectPtr);
	void fixupMixedObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache);
	void fixupContinuationNativeSlots(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache);
	void fixupContinuationObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache);
	void fixupClassObject(MM_EnvironmentVLHGC* env, J9Object *classObject, J9MM_FixupCache *cache);
	void fixupClassLoaderObject(MM_EnvironmentVLHGC* env, J9Object *classLoaderObject, J9MM_FixupCache *cache);
	void fixupPointerArrayObject(MM_EnvironmentVLHGC* env, J9Object *objectPtr, J9MM_FixupCache *cache);
	void fixupClasses(MM_EnvironmentVLHGC *env);
	void fixupClassLoaders(MM_EnvironmentVLHGC *env);
	void fixupStacks(MM_EnvironmentVLHGC *env);
	void fixupRememberedSet(MM_EnvironmentVLHGC *env);
	void fixupMonitorReferences(MM_EnvironmentVLHGC *env);

	/**
	 * Return the page index for an object.
	 * long int, always positive (in particular, -1 is an invalid value)
	 */
	MMINLINE IDATA pageIndex(J9Object* objectPtr) const
	{
		UDATA markIndex = (UDATA)objectPtr - (UDATA)_heapBase;
		return markIndex / sizeof_page;
	}

	/**
	 * Return the start of a page
	 */
	MMINLINE J9Object* pageStart(UDATA i) const
	{
		return (J9Object*) ((UDATA)_heapBase + (i * sizeof_page));
	}

	void rebuildMarkbits(MM_EnvironmentVLHGC *env);
    
	/**
	 * Rebuild mark bits within the given region
	 *
	 * @param env[in] the current thread
	 * @param region[in] the region of the heap to be rebuilt
	 * @return If the region was fully rebuilt, this will be NULL.  Otherwise, it is the address, in another region, which must be rebuilt before rebuilding this region can proceed
	 */
	void *rebuildMarkbitsInRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region);
    
	/**
	 * Walk all the regions in the compact set and either recycle them (if they are empty) or set their free
	 * list entry to be the free chunk at the end of the region.
	 * Note that this MUST be called by one thread only.
	 * @param env[in] A GC thread
	 */
	void recycleFreeRegionsAndFixFreeLists(MM_EnvironmentVLHGC *env);

	/**
	 * Fix up the spine pointers of any arraylet leaf regions
	 */
	void fixupArrayletLeafRegionSpinePointers(MM_EnvironmentVLHGC *env);
	
	/**
	 * Fix up the data in required arraylet leaf regions, unfinalized lists and ownable synchronizer lists.
	 * @param env[in] the current thread
	 */
	void fixupArrayletLeafRegionContentsAndObjectLists(MM_EnvironmentVLHGC* env);
	
	/**
	 * Identify any arraylet leaf regions which require fix up and tag them using the _shouldFixup flag.
	 * This must be called by only one GC thread.
	 * @param env[in] A GC thread
	 */
	void tagArrayletLeafRegionsForFixup(MM_EnvironmentVLHGC* env);

	/**
	 * Verify all objects in heap
	 */
	void verifyHeap(MM_EnvironmentVLHGC *env, bool beforeCompaction);

	/**
	 * Verify mixed object in heap
	 * @param objectPtr pointer to object for fixup
	 */
	void verifyHeapMixedObject(J9Object* objectPtr);

	/**
	 * Verify array object in heap
	 * @param objectPtr pointer to object for fixup
	 */
	void verifyHeapArrayObject(J9Object* objectPtr);

	/**
	 * Verify object slot
	 * @param object pointer to object slot
	 */
	void verifyHeapObjectSlot(J9Object* object);

	/**
	 * Called when we are compacting while another GC cycle exists but is not currently running.  This function
	 * fixes up its work packets.
	 * Note that this is a parallel fixup (non-empty work packet is the granule of parallelism).
	 * @param env[in] A GC thread
	 * @param packets The work packets to fixup
	 */
	void fixupExternalWorkPackets(MM_EnvironmentVLHGC *env, MM_WorkPacketsVLHGC *packets);

	/**
	 * Called when we are about to compact but another GC cycle is in-flight.  This function clears the other cycles
	 * mark map which underlies the regions we are about to move (since we are going to use that space for fixup data).
	 * Note that this is a parallel fixup (a participating region is the granule of compaction).
	 * @param env[in] A GC thread
	 * @param markMap[in] The next GC cycle's mark map
	 */
	void clearMarkMapCompactSet(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap);

	/**
	 * Called to rebuild the given mark map from references in the given work packets.  Any packet slot which points into
	 * the compact set is marked in the given mark map.
	 * 
	 * @param env[in] The main GC thread
	 * @param packets[in] The work packets for the in-progress GMP cycle
	 * @param markMap[in] The mark map for the in-progress GMP cycle
	 */
	void rebuildNextMarkMapFromPackets(MM_EnvironmentVLHGC *env, MM_WorkPacketsVLHGC *packets, MM_MarkMap *markMap);

	/**
	 * Part of planning refactoring (JAZZ 21595).
	 * This function is analogous to moveObjects.  It is meant to be the non-moving equivalent which only performs planning actions
	 * but does not move objects or write to the heap.
	 * @param env[in] The main GC thread
	 * @param objectCount[in/out] The number of objects the receiver planned to move
	 * @param byteCount[in/out] The number of bytes the receiver planned to move
	 * @param skippedObjectCount[in/out] The number of objects the receiver planned to skip
	 */
	void planCompaction(MM_EnvironmentVLHGC *env, UDATA *objectCount, UDATA *byteCount, UDATA *skippedObjectCount);
	/**
	 * This function plans the compaction for the given subAreaRegion.  This may be an evacuation or an internal sliding compaction,
	 * depending on what other regions are available as compaction targets.
	 * @param env[in] A GC thread
	 * @param subAreaRegion[in] The region for which the receiver must plan a compaction
	 * @param postMoveRegionSizeInBytes[in] The size of all the objects in subAreaRegion, in bytes, if every object it contains were to move
	 * @param objectCount[in/out] The number of objects the receiver planned to move
	 * @param byteCount[in/out] The number of bytes the receiver planned to move
	 * @param skippedObjectCount[in/out] The number of objects the receiver planned to skip
	 */
	void planRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *subAreaRegion, UDATA postMoveRegionSizeInBytes, UDATA *objectCount, UDATA *byteCount, UDATA *skippedObjectCount);

	/**
	 * Return back evacuate extent based on Compact Group Destinations queue
	 * Add the source region to the queue if is finally handled
	 * @param env[in] A GC thread
	 * @param targetSpaceRequired[in] The size of the objects in subAreaRegion still need to be planned, in bytes
	 * @param subAreaRegion[in] The source region is been planning a compaction
	 * @param targetPointer[out] The start address of memory for planning evacuate
	 * @param topEdge[out] The end address of memory for planning evacuate
	 * @return true if the source region planning is satisfied and it has been added to the queue
	 */
	bool getEvacuateExtent(MM_EnvironmentVLHGC *env, UDATA targetSpaceRequired, MM_HeapRegionDescriptorVLHGC *subAreaRegion, void **targetPointer, void **topEdge);

	/**
	 * Part of planning refactoring (JAZZ 21595).
	 * This function is analogous to doCompact.  It is meant to be the non-moving equivalent which only performs planning actions
	 * but does not move objects or write to the heap.
	 * @param env[in] The main GC thread
	 * @param copyDestinationBase[in/out] The base address of object copy destinations.  This is updated to the byte after the space consumed by the copy before this method returns
	 * @param copyDestinationTop[in] The end address of object copy destinations
	 * @param firstTopCopy[in] The first object to try to copy
	 * @param endOfCopyBlock[in] The first byte after the last object which should be copied
	 * @param objectCount[in/out] The number of objects the receiver planned to move
	 * @param byteCount[in/out] The number of bytes the receiver planned to move
	 */
	J9Object *doPlanEvacuate(MM_EnvironmentVLHGC *env, void **copyDestinationBase, void *copyDestinationTop, J9Object *firstToCopy, J9Object *endOfCopyBlock, UDATA *objectCount, UDATA *byteCount);
	/**
	 * Part of planning refactoring (JAZZ 21595).
	 * This function is analogous to doCompact.  It is meant to be the non-moving equivalent which only performs planning actions
	 * but does not move objects or write to the heap.
	 * @param env[in] The main GC thread
	 * @param copyDestinationBase[in] The base address of object copy destinations
	 * @param firstTopCopy[in] The first object to try to copy
	 * @param endOfCopyBlock[in] The first byte after the last object which should be copied
	 * @param objectCount[in/out] The number of objects the receiver planned to move
	 * @param byteCount[in/out] The number of bytes the receiver planned to move
	 */
	void doPlanSlide(MM_EnvironmentVLHGC *env, void *copyDestinationBase, J9Object *firstToCopy, J9Object *endOfCopyBlock, UDATA *objectCount, UDATA *byteCount);

	/**
	 * A helper used by moveObjects to do whatever cleanup is required at the end of a move extent.  This means fixing up
	 * the previous object we moved and clearing out the contents of the previousObjects array.
	 * @param env[in] A GC thread
	 * @param previousObjects[in/out] The previous two objects moved.  The most recent one will be fixed up and then the array will be cleared (MUST HAVE 2 ELEMENTS)
	 */
	void trailingObjectFixup(MM_EnvironmentVLHGC *env, J9MM_FixupTuple *previousObjects);

	/**
	 * Walks the previous mark map to find all objects in regions to be compacted and then, using forwarding pointer data,
	 * finds their new locations and copies them.
	 * @param env[in] The main GC thread
	 */
	void moveObjects(MM_EnvironmentVLHGC *env);

	/**
	 * Evacuates the page based at page according to the stored fixup data built during the planning phase.
	 * @param env[in] A GC thread
	 * @param page[in] The base of the page to evacuate
	 * @param previousTwoObjects[in/out] The fixup data for the previous two objects found in this move extent (may be pre-populated and must be updated by this method)
	 * @param newLocationPtr[in/out] The next destination of moved objects (or NULL if not yet computed).  This must be updated by this method
	 */
	void evacuatePage(MM_EnvironmentVLHGC *env, void *page, J9MM_FixupTuple *previousTwoObjects, J9Object **newLocationPtr);

	/**
	 * Fixes up all the objects in the page based at page using the stored fixup data built during the planning phase.
	 * Note that this page must not be scheduled for moving in this compaction.
	 * @param env[in] A GC thread
	 * @param page[in] The base of the page to fixup
	 */
	void fixupNonMovingPage(MM_EnvironmentVLHGC *env, void *page);

	/**
	 * Determine the size, in bytes, that the objects residing in the page based at page would consume after they are moved.
	 * @param env[in] A GC thread
	 * @param page[in] The base of the page to measure
	 * @return The number of bytes that the objects in page will occupy once moved
	 */
	UDATA movedPageSize(MM_EnvironmentVLHGC *env, void *page);

	/**
	 * Called in the main thread to do the initial population of the move work stack.  This call must be made prior to moveObjects().
	 * @param env[in] The main GC thread
	 */
	void setupMoveWorkStack(MM_EnvironmentVLHGC *env);

	/**
	 * Pops a ready region from the move work stack or the fixup work stack.  Move work is favoured but fixup work will be returned
	 * if any is available.  If no work is available, this method will block until move work becomes available or all work is done (in
	 * which case, NULL will be returned).
	 * The caller must check the type of the region underlying the subarea to see if this needs movement or fixup.
	 * @param env[in] A GC thread
	 * @return A region which either has move work or fixup work to be done
	 */
	MM_HeapRegionDescriptorVLHGC *popWork(MM_EnvironmentVLHGC *env);

	/**
	 * Pushes a region which has just been the subject of some amount of work.  If the entry is finished, it is discarded (from
	 * the move stack but pushed onto the rebuild stack) and not pushed onto the stack.  If some work remains, it is either pushed onto
	 * the ready work stack or pushed onto the blocked stack of the region which must be evacuated (or partially evacuated) before
	 * this item is ready.
	 * @param env[in] A GC thread
	 * @param finishedRegion[in] The region which is being returned to the work stack because further progress is blocked or because it is finished
	 * @param evacuationTarget[in] If we are pushing this because we were blocked, this will be the target we are waiting for (NULL otherwise)
	 * @param evacuationSize If we are pushing this because we were blocked, this will be the size we need at evacuationTarget
	 */
	void pushMoveWork(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *finishedRegion, void *evacuationTarget, UDATA evacuationSize);

	/**
	 * Pops a ready region from the rebuild work stack.  If no work is available, this method will block until move work becomes
	 * available or all work is done (in which case, NULL will be returned).
	 * @param env[in] A GC thread
	 * @return A region which is ready to have some rebuilding work done
	 */
	MM_HeapRegionDescriptorVLHGC *popRebuildWork(MM_EnvironmentVLHGC *env);

	/**
	 * Pushes a region which has just been the subject of some amount of work.  If the entry is finished, it is discarded and
	 * not pushed onto the stack.  If some work remains, it is either pushed onto the ready rebuild work stack or pushed onto the blocked
	 * stack of the region which must be rebuilt (or partially rebuilt) before this item is ready.
	 * @param env[in] A GC thread
	 * @param finishedRegion[in] The region which is being returned to the work stack because further progress is blocked or because it is finished
	 * @param evacuationTarget[in] If we are pushing this because we were blocked, this will be the target we are waiting for (NULL otherwise)
	 */
	void pushRebuildWork(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *finishedRegion, void *evacuationTarget);

	/**
	 * Checks the state of the given card and updates its content based on what state it should transition the old one from, given that we want
	 * the resultant state to also describe that a card from a card list was flushed to it.
	 * Specifically for Write Once collector, if GMP is running CLEAN goes to REMEMBERED_AND_GMP_MUST_SCAN, because
	 * next Mark Map is partially rebuilt (only for objects in Work Packets), and this card may refer to a marked object
	 * for which we did not rebuild mark bit.
	 * @param card[in/out] The card which will be used as the input and output of the state machine function
	 * @param isGlobalMarkPhaseRunning[in] true if GMP is active
	 */
	void writeFlushToCardState(Card *card, bool isGlobalMarkPhaseRunning);

	/**
	 * Rebuilds the given mark map by clearing all the mark bits in the page at pageBase and setting the bits which correspond to
	 * the new locations of the objects in that page.  The page is updated by one thread but, because multiple pages can be copied
	 * into the same mark word, any mark map updates must either be atomic or must be able to prove that only the one thread will
	 * be writing to the word.
	 * @param env[in] A GC thread
	 * @param markMap[in] The mark map to update
	 * @param pageBase[in] The base address of the page to clear and migrate
	 */
	void rebuildMarkMapInMovingPage(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap, void *pageBase);

	/**
	 * Clears the tail markings in the given mark map fragment underlying the page at pageBase.  This update does not require atomics
	 * as the page is a multiple of mark words.
	 * @param env[in] A GC thread
	 * @param markMap[in] The mark map to update
	 * @param pageBase[in] The base address of the page to rebuild
	 */
	void removeTailMarksInPage(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap, void *pageBase);

	void fixupObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, J9MM_FixupCache *cache);

	/**
	 * Called whenever a ownable synchronizer object is fixed up during compact. Places the object on the thread-specific buffer of gc work thread.
	 * @param env -- current thread environment
	 * @param object -- The object of type or subclass of java.util.concurrent.locks.AbstractOwnableSynchronizer.
	 */
	MMINLINE void addOwnableSynchronizerObjectInList(MM_EnvironmentVLHGC *env, J9Object *objectPtr)
	{
		/* if isObjectInOwnableSynchronizerList() return NULL, it means the object isn't in OwnableSynchronizerList,
		 * it could be the constructing object which would be added in the list after the construction finish later. ignore the object to avoid duplicated reference in the list. 
		 */
		if (NULL != _extensions->accessBarrier->isObjectInOwnableSynchronizerList(objectPtr)) {
			((MM_OwnableSynchronizerObjectBufferVLHGC*) env->getGCEnvironment()->_ownableSynchronizerObjectBuffer)->addForOnlyCompactedRegion(env, objectPtr);
		}
	}

#if defined(J9VM_GC_FINALIZATION)
	void fixupFinalizableObjects(MM_EnvironmentVLHGC *env);

	/**
	 * Helper method used by @ref fixupFinalizeObjects to fixup a list
	 * of finalizable objects starting at the specified headObject
	 * @param env[in] A GC thread
	 * @param headObject[in] The head of the finalizable list to fixup
	 */
	void fixupFinalizableList(MM_EnvironmentVLHGC *env, j9object_t headObject);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * At the beginning of a compaction, record any class loaders in the compact set which are marked in the next mark map.
	 * The next mark map must still be valid.
	 * @param env[in] A GC thread
	 */
	void rememberClassLoaders(MM_EnvironmentVLHGC *env);
	
	/**
	 * At the end of a compaction, restore the next mark bits for any class loaders in the compact set.
	 * @param env[in] A GC thread
	 */
	void rebuildNextMarkMapFromClassLoaders(MM_EnvironmentVLHGC *env);
	
	/**
	 * At the beginning of a compaction, clear the remembered sets of all class loaders for all regions in the compaction set.
	 * These will be rebuilt as we relocate objects.
	 * @param env[in] A GC thread 
	 */
	void clearClassLoaderRememberedSetsForCompactSet(MM_EnvironmentVLHGC *env);
	
	/**
	 * Create a WriteOnceCompactor object.
	 */
	MM_WriteOnceCompactor(MM_EnvironmentVLHGC *env);

protected:
	virtual bool initialize(MM_EnvironmentVLHGC *env);
	virtual void tearDown(MM_EnvironmentVLHGC *env);

public:
	static MM_WriteOnceCompactor *newInstance(MM_EnvironmentVLHGC *env);
	void kill(MM_EnvironmentVLHGC *env);

	void mainSetupForGC(MM_EnvironmentVLHGC *env);
	void compact(MM_EnvironmentVLHGC *env);

	J9Object *getForwardingPtr(J9Object *objectPtr) const;
	
	/**
	 * Fixes the marked objects in the [lowAddress..highAddress) range.  If rememberedObjectsOnly is set, we will further constrain
	 * this scanned set by only scanning objects in the range which have the OBJECT_HEADER_REMEMBERED bit set.  The receiver uses
	 * its _markMap to determine which objects in the range are marked.
	 *
	 * @param env[in] A GC thread (note that this method could be called by multiple threads, in parallel, but on disjoint address ranges)
	 * @param lowAddress[in] The heap address where the receiver will begin walking objects
	 * @param highAddress[in] The heap address after the last address we will potentially find a live object
	 * @param rememberedObjectsOnly If true, the receiver only fixes remembered live objects in the range.  If false, it fixes all live objects in the range
	 */
	void fixupObjectsInRange(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress, bool rememberedObjectsOnly);

	/**
	 * Set the current cycle state information.
	 */
	void setCycleState(MM_CycleState *cycleState, MM_MarkMap *nextMarkMap);

	/**
	 * Clear the current cycle state information.
	 */
	void clearCycleState();

	/**
	 * A helper method to pop a region from the given work stack base and update the base to its new base value.
	 * @param workStackBase[in/out] The work stack from which the region must be popped.  This is a reference parameter which will be updated before the method returns
	 * @return The next region on the given work stack or NULL if the stack is empty
	 */
	MM_HeapRegionDescriptorVLHGC *popNextRegionFromWorkStack(MM_HeapRegionDescriptorVLHGC **workStackBase);

	/**
	 * A helper method to push a region onto one of the given work stacks, based on whether or not it has its _isCompactDestination flag set (true implies high priority).  Updates
	 * the stack base of the stack receiving the region before returning.
	 * @param workStackBase[in/out] The "low priority" work stack base.  This reference parameter will be updated before the function returns is region is low priority
	 * @param workStackBaseHighPriority[in/out] The "high priority" work stack base.  This reference parameter will be updated before the function returns is region is high priority
	 */
	void pushRegionOntoWorkStack(MM_HeapRegionDescriptorVLHGC **workStackBase, MM_HeapRegionDescriptorVLHGC **workStackBaseHighPriority, MM_HeapRegionDescriptorVLHGC *region);
	void doStackSlot(MM_EnvironmentVLHGC *env, J9Object *fromObject, J9Object** slot);

	friend class MM_WriteOnceCompactFixupRoots;
	friend class MM_ParallelWriteOnceCompactTask;
};

#endif /* J9VM_GC_MODRON_COMPACTION */

typedef struct StackIteratorData4WriteOnceCompactor {
	MM_WriteOnceCompactor *writeOnceCompactor;
	MM_EnvironmentVLHGC *env;
	J9Object *fromObject;
} StackIteratorData4WriteOnceCompactor;

#endif /* COMPACTSCHEMEVLHGC_HPP_ */

