
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(COPYFORWARDSCHEME_HPP_)
#define COPYFORWARDSCHEME_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "BaseNonVirtual.hpp"

#include "CopyScanCacheListVLHGC.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "ModronTypes.hpp"

class GC_SlotObject;
class MM_AllocationContextTarok;
class MM_CardCleaner;
class MM_CopyForwardCompactGroup;
class MM_CopyForwardGMPCardCleaner;
class MM_CopyForwardNoGMPCardCleaner;
class MM_CopyForwardVerifyScanner;
class MM_Dispatcher;
class MM_HeapRegionManager;
class MM_HeapRegionDescriptorVLHGC;
class MM_InterRegionRememberedSet;
class MM_MarkMap;
class MM_MemoryPoolBumpPointer;
class MM_ReferenceStats;

/* Forward declaration of classes defined within the cpp */
class MM_CopyForwardSchemeAbortScanner;
class MM_CopyForwardSchemeRootScanner;
class MM_CopyForwardSchemeRootClearer;
class MM_CopyForwardSchemeTask;
class MM_ScavengerForwardedHeader;

/**
 * Copy Forward scheme used for highly mobile partial collection operations.
 * @ingroup GC_Modron_Standard
 */
class MM_CopyForwardScheme : public MM_BaseNonVirtual
{
private:
	J9JavaVM *_javaVM;
	MM_GCExtensions *_extensions;

	enum ScanReason {
		SCAN_REASON_NONE = 0, /**< Indicates there is no item for scan */
		SCAN_REASON_PACKET = 1, /**< Indicates the object being scanned came from a work packet */
		SCAN_REASON_COPYSCANCACHE = 2,
		SCAN_REASON_DIRTY_CARD = 3, /**< Indicates the object being scanned was found in a dirty card */
		SCAN_REASON_OVERFLOWED_REGION = 4, /**< Indicates the object being scanned was in an overflowed region */
	};

	MM_HeapRegionManager *_regionManager;  /**< Region manager for the heap instance */
	MM_InterRegionRememberedSet *_interRegionRememberedSet;	/**< A cached pointer to the inter-region reference tracking mechanism */

	class MM_ReservedRegionListHeader {
		/* Fields */
	public:
		enum { MAX_SUBLISTS = 8 }; /* arbitrary value for maximum split */
		struct Sublist {
			MM_HeapRegionDescriptorVLHGC *_head;  /**< Head of the reserved regions list */
			MM_LightweightNonReentrantLock _lock;  /**< Lock for reserved regions list */
			volatile UDATA _cacheAcquireCount; /**< The number of times threads acquired caches from this list */ 
			UDATA _cacheAcquireBytes; /**< The number of bytes acquired for caches from this list */
		} _sublists[MAX_SUBLISTS];
		UDATA _evacuateRegionCount; /**< The number of evacuate regions in this group */
		UDATA _maxSublistCount; /**< The highest value _sublistCount is permitted to reach in this group */
		volatile UDATA _sublistCount; /**< The number of active sublists in this list */
		MM_HeapRegionDescriptorVLHGC *_tailCandidates; /**< A linked list of regions in this compact group which have empty tails */
		MM_LightweightNonReentrantLock _tailCandidatesLock; /**< Lock to protect _tailCandidates */
		UDATA _tailCandidateCount; /**< The number of regions in the _tailCandidates list */
	protected:
	private:
		/* Methods */
	public:
	protected:
	private:
	};
	MM_ReservedRegionListHeader *_reservedRegionList;  /**< List of reserved regions during a copy forward operation */
	UDATA _compactGroupMaxCount;  /**< The maximum number of compact groups that exist in the system */

	UDATA _phantomReferenceRegionsToProcess; /**< A count, for verification purposes, of the number of regions to be processed for phantom references */

	UDATA _minCacheSize;  /**< Minimum size in bytes that will be returned as a general purpose cache area */
	UDATA _maxCacheSize;  /**< Maximum size in bytes that will be requested for a general purpose cache area */

	MM_Dispatcher *_dispatcher;
	
	MM_CopyScanCacheListVLHGC _cacheFreeList;  /**< Caches which are not bound to heap memory and available to be populated */
	MM_CopyScanCacheListVLHGC *_cacheScanLists;  /**< An array of per-node caches which contains objects still to be scanned (1+node_count elements in array)*/
	UDATA _scanCacheListSize;	/**< The number of entries in _cacheScanLists */
	volatile UDATA _scanCacheWaitCount;	/**< The number of threads currently sleeping on _scanCacheMonitor, awaiting scan cache work */
	omrthread_monitor_t _scanCacheMonitor;	/**< Used when waiting on work on any of the _cacheScanLists */

	volatile UDATA* _workQueueWaitCountPtr;	/**< The number of threads currently sleeping on *_workQueueMonitorPtr, awaiting scan cache work or work from packets*/
	omrthread_monitor_t* _workQueueMonitorPtr;	/**< Used when waiting on work on any of the _cacheScanLists or workPackets*/

	volatile UDATA _doneIndex;	/**< Incremented when _cacheScanLists are empty and we want all threads to fall out of *_workQueueMonitorPtr */

	MM_MarkMap *_markMap;  /**< Cached reference to the previous mark map */

	void *_heapBase;  /**< Cached base pointer of heap */
	void *_heapTop;  /**< Cached top pointer of heap */

	volatile bool _abortFlag;  /**< Flag indicating whether the current copy forward cycle should be aborted due to insufficient heap to complete */
	bool _abortInProgress;  /**< Flag indicating that the copy forward mechanism is now operating in abort mode, which is attempting to secure integrity of the heap to continue execution */

	UDATA _regionCountCannotBeEvacuated; /**<The number of regions, which can not be copyforward in collectionSet */
	UDATA _regionCountReservedNonEvacuated; /** the number of regions need to set Mark only in order to try to avoid abort case */

	UDATA _cacheLineAlignment; /**< The number of bytes per cache line which is used to determine which boundaries in memory represent the beginning of a cache line */

	bool _clearableProcessingStarted;  /**< Flag indicating that clearable processing had been started during this cycle (used for abort purposes) */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool _dynamicClassUnloadingEnabled;  /**< Local cached value from cycle state for performance reasons (TODO: Reevaluate) */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	bool _collectStringConstantsEnabled;  /**< Local cached value which determines whether string constants are roots */

	bool _tracingEnabled;  /**< Temporary variable to enable tracing of activity */
	bool _cacheTracingEnabled;  /**< Temporary variable to enable tracing of activity */
	MM_AllocationContextTarok *_commonContext;	/**< The common context is used as an opaque token to represent cases where we don't want to relocate objects during NUMA-aware copy-forward since relocating to the common context is currently disabled */
	MM_CopyForwardCompactGroup *_compactGroupBlock; /**< A block of MM_CopyForwardCompactGroup structs which is subdivided among the GC threads */ 
	UDATA _arraySplitSize; /**< The number of elements to be scanned in each array chunk (this determines the degree of parallelization) */

	UDATA _regionSublistContentionThreshold	/**< The number of threads which must be contending on the same region sublist for us to decide that another sublist should be created to alleviate contention (reset at the beginning of every CopyForward task) */;

	volatile bool _failedToExpand; /**< Record if we've failed to expand in this collection already, in order to avoid repeated expansion attempts */
	bool _shouldScanFinalizableObjects; /**< Set to true at the beginning of a collection if there are any pending finalizable objects */
	const UDATA _objectAlignmentInBytes;	/**< Run-time objects alignment in bytes */

protected:
public:
private:

	/* Temporary verification functions */
	void verifyDumpObjectDetails(MM_EnvironmentVLHGC *env, const char *title, J9Object *object);
	void verifyCopyForwardResult(MM_EnvironmentVLHGC *env);
	bool verifyIsPointerInSurvivor(MM_EnvironmentVLHGC *env, J9Object *object);
	bool verifyIsPointerInEvacute(MM_EnvironmentVLHGC *env, J9Object *object);
	void verifyObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr);
	void verifyMixedObjectSlots(MM_EnvironmentVLHGC *env, J9Object *objectPtr);
	void verifyReferenceObjectSlots(MM_EnvironmentVLHGC *env, J9Object *objectPtr);
	void verifyPointerArrayObjectSlots(MM_EnvironmentVLHGC *env, J9Object *objectPtr);
	void verifyClassObjectSlots(MM_EnvironmentVLHGC *env, J9Object *classObject);
	void verifyClassLoaderObjectSlots(MM_EnvironmentVLHGC *env, J9Object *classLoaderObject);
	void verifyExternalState(MM_EnvironmentVLHGC *env);
	friend class MM_CopyForwardVerifyScanner;

	/**
	 * Called to retire a copy cache once a thread no longer wants to use it as a copy destination.
	 * @param env[in] A GC thread
	 * @param compactGroup The index of the compact group whose thread-local copy cache should be deleted
	 * @return the copy cache which was affected
	 */
	MM_CopyScanCacheVLHGC * stopCopyingIntoCache(MM_EnvironmentVLHGC *env, UDATA compactGroup);

	/*
	 * Called to update a region's projected live bytes with the bytes copied by the copy scan cache.
	 * @param env[in] A GC thread
	 * @param cache[in] The copy cache to update a region's live bytes from
	 */
	void updateProjectedLiveBytesFromCopyScanCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache);

	/**
	 * Discard any remaining memory in the specified copy cache, returning it to the pool if possible, and updating
	 * memory statistics. The implementation will attempt to return any unused portion of the cache to the owning pool 
	 * if possible (and will internally lock to corresponding region list to ensure that the allocation pointer can be 
	 * updated safely).  The copy cache flag is unset.  Additionally, the top of the cache will be updated to reflect 
	 * that no further allocation is possible if the excess at the top of the cache was successfully returned to the 
	 * pool.
	 * @param env[in] A GC thread
	 * @param cache[in] The copy cache to retire
	 * @param cacheLock[in] The lock associated with the cache's owning list
	 * @param wastedMemory[in] The number of bytes of memory which couldn't be used in the allocated portion of the copy cache and should be considered free memory when flushed back to the pool
	 */
	void discardRemainingCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache, MM_LightweightNonReentrantLock *cacheLock, UDATA wastedMemory);
	
	/**
	 * Determine whether the object pointer is found within the heap proper.
	 * @return Boolean indicating if the object pointer is within the heap boundaries.
	 */
	MMINLINE bool isHeapObject(J9Object* objectPtr)
	{
		return ((_heapBase <= (U_8 *)objectPtr) && (_heapTop > (U_8 *)objectPtr));
	}

 	/**
	 * Determine whether the object is live relying on survivor/evacuate region flags and mark map. null object is considered marked.
	 * @return Boolean true if object is live
	 */
	bool isLiveObject(J9Object *objectPtr);

	/**
	 * Determine whether string constants should be treated as clearable.
	 * @return Boolean indicating if strings constants are clearable.
	 */
	MMINLINE bool isCollectStringConstantsEnabled() { return _collectStringConstantsEnabled; };

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Determine whether class unloading is enabled this collection cycle.
	 * @return Boolean indicating if class unloading is enabled this cycle.
	 */
	MMINLINE bool isDynamicClassUnloadingEnabled() { return _dynamicClassUnloadingEnabled; };
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	/**
	 * Determine whether the copy forward abort flag is raised or not.
	 * @return boolean indicating the start of the copy forward abort flag.
	 */
	MMINLINE bool abortFlagRaised() { return _abortFlag; }

	/**
	 * Clear the abort copy forward flag.
	 */
	MMINLINE void clearAbortFlag() { _abortFlag = false; }

	/**
	 * Raise the abort copy forward flag.
	 * @param env GC thread.
	 */
	MMINLINE void raiseAbortFlag(MM_EnvironmentVLHGC *env);

	/**
	 * Reinitializes the cache with the given base address, top address, as a copy cache.  Also updates the mark map cache values in the env's
	 * CopyForwardCompactGroup for the given cache.
	 * @param env[in] The thread taking ownership of the cache
	 * @param cache[in] cache to be reinitialized
	 * @param base[in] base address of cache
	 * @param top[in] top address of cache
	 * @param compactGroup the compact group to which the cache belongs
	 */
	MMINLINE void reinitCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache, void *base, void *top, UDATA compactGroup);

	/**
	 * Reinitializes the cache with the array and next scan index as an array split cache.
	 * @param env[in] The thread taking ownership of the cache
	 * @param cache[in] cache to be reinitialized
	 * @param array[in] The array to scan
	 * @param nextIndex[in] The next index in the array to scan
	 */
	MMINLINE void reinitArraySplitCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache, J9IndexableObject *array, UDATA nextIndex);
	
	MM_CopyScanCacheVLHGC *getFreeCache(MM_EnvironmentVLHGC *env);
	/**
	 * Adds the given newCacheEntry to the free cache list.
	 * @note The implementation will lock the list prior to modification.
	 * @param env current GC thread.
	 * @param newCacheEntry[in] The entry to add to cacheList
	 */
	void addCacheEntryToFreeCacheList(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *newCacheEntry);
	/**
	 * Adds the given newCacheEntry to the scan cache list and will notify any waiting threads if the list was empty prior to adding newCacheEntry.
	 * @note The implementation will lock the list prior to modification.
	 * @param env current GC thread.
	 * @param newCacheEntry[in] The entry to add to cacheList
	 */
	void addCacheEntryToScanCacheListAndNotify(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *newCacheEntry);
	
	void flushCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache);
	void clearCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache);

	/**
	 * add OwnableSynchronizerObject in the buffer if reason == SCAN_REASON_COPYSCANCACHE || SCAN_REASON_PACKET
	 * and call scanMixedObjectSlots().
	 *
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr current object being scanned.
	 * @param reason to scan (dirty card, packet, scan cache, overflow)
	 */
	MMINLINE void scanOwnableSynchronizerObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason);

	/**
	 * Called whenever a ownable synchronizer object is scaned during CopyForwardScheme. Places the object on the thread-specific buffer of gc work thread.
	 * @param env -- current thread environment
	 * @param object -- The object of type or subclass of java.util.concurrent.locks.AbstractOwnableSynchronizer.
	 */
	MMINLINE void addOwnableSynchronizerObjectInList(MM_EnvironmentVLHGC *env, j9object_t object);

	/**
	 * Scan the slots of a mixed object.
	 * Copy and forward all relevant slots values found in the object.
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr current object being scanned.
	 * @param reason to scan (dirty card, packet, scan cache, overflow)
	 */
	void scanMixedObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason);
	/**
	 * Scan the slots of a reference mixed object.
	 * Copy and forward all relevant slots values found in the object.
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr current object being scanned.
	 * @param reason to scan (dirty card, packet, scan cache, overflow)	 *
	 */
	void  scanReferenceObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason);
	
	/**
	 * Called by the root scanner to scan all WeakReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanWeakReferenceObjects(MM_EnvironmentVLHGC *env);

	/**
	 * Called by the root scanner to scan all SoftReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanSoftReferenceObjects(MM_EnvironmentVLHGC *env);

	/**
	 * Called by the root scanner to scan all PhantomReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanPhantomReferenceObjects(MM_EnvironmentVLHGC *env);
	
	/**
	 * Set region as survivor (and its survivor base address). Also set the special state if region was created by phantom reference processing.
	 * @param env[in] the current thread
	 * @param region[in] region to set as survivor
	 * @param survivorBase[in] survivor base address (all object below this address, if different than low region address are not part of survivor)
	 */
	void setRegionAsSurvivor(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region, void* survivorBase);

	/**
	 * Process the list of reference objects recorded in the specified list.
	 * References with unmarked referents are cleared and optionally enqueued.
	 * SoftReferences have their ages incremented.
	 * @param env[in] the current thread
	 * @param region[in] the region all the objects in the list belong to
	 * @param headOfList[in] the first object in the linked list 
	 * @param referenceStats copy forward stats substructure to be updated
	 */
	void processReferenceList(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC* region, J9Object* headOfList, MM_ReferenceStats *referenceStats);
	
	/**
	 * Walk the list of reference objects recorded in the specified list, changing them to the REMEMBERED state.
	 * @param env[in] the current thread
	 * @param headOfList[in] the first object in the linked list 
	 */
	void rememberReferenceList(MM_EnvironmentVLHGC *env, J9Object* headOfList);

	/**
	 * Walk all regions in the evacuate set in parallel, remembering any objects on their reference 
	 * lists so that they may be restored at the end of the PGC.
	 * @param env[in] the current thread
	 */
	void rememberReferenceListsFromExternalCycle(MM_EnvironmentVLHGC *env);

	/**
	 * Call rememberReferenceList() for each of the weak, soft and phantom lists in referenceObjectList, then reset them.
	 * @param env[in] the current thread
	 * @param region[in] region which lists to remember and reset
	 */
	void rememberAndResetReferenceLists(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Partially scan the slots of a pointer array object.
	 * This is only called during abort phase (workstack driven). Otherwise, scanPointerArrayObjectSlotsSplit() is used.
	 * Create a new packet work item to allow other threads to complete scanning of the array while this scan proceeds.
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param arrayPtr current array being scanned.
	 * @param reason to scan (dirty card, packet, scan cache, overflow)
	 */
	void scanPointerArrayObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9IndexableObject *arrayPtr, ScanReason reason);

	/**
	 * Partially scan the slots of a pointer array object.
	 * Copy and forward all relevant slots values found in the object.
	 * Create a new scan cache to allow other threads to complete scanning of the array while this scan proceeds.
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param arrayPtr current array being scanned.
	 * @param startIndex the index to start from
	 * @param currentSplitUnitOnly[in] do only current array split work unit; do not push the rest of array on the work stack
	 * @return number of slots scanned
	 */
	UDATA scanPointerArrayObjectSlotsSplit(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9IndexableObject *arrayPtr, UDATA startIndex, bool currentSplitUnitOnly = false);
	
	/**
	 * For the given object and current starting index, determine the number of slots being scanned now, and create a work unit for the rest of the array.
	 * Works for both copy-scan cache and workstack driven phases.
	 * @param env[in] the current thread
	 * @param arrayPtr[in] current array object being scanned.
	 * @param startIndex[in] starting index for the current split scan
	 * @param currentSplitUnitOnly[in] do only current array split work unit; do not push the rest of array on the work stack*
	 * @return the number of the slots to be scanned for the current split
	 */
	UDATA createNextSplitArrayWorkUnit(MM_EnvironmentVLHGC *env, J9IndexableObject *arrayPtr, UDATA startIndex, bool currentSplitUnitOnly = false);

	/**
	 * Scan the slots of a java.lang.Class object.
	 * Copy and forward all relevant slots values found in the object, as well
	 * as those found in the backing J9Class structure.
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr current object being scanned.
	 * @param reason to scan (dirty card, packet, scan cache, overflow)
	 */
	void scanClassObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *classObject, ScanReason reason = SCAN_REASON_COPYSCANCACHE);
	/**
	 * Scan the slots of a java.lang.ClassLoader object.
	 * Copy and forward all relevant slots values found in the object, as well
	 * as those found in the backing J9ClassLoader structure.
	 * @param env current GC thread.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr current object being scanned.
	 * @param reason to scan (dirty card, packet, scan cache, overflow)
	 */
	void scanClassLoaderObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *classLoaderObject, ScanReason reason = SCAN_REASON_COPYSCANCACHE);

	/**
	 * Determine whether the object is in evacuate memory or not.
	 * @param objectPtr[in] Object pointer whose properties are being examined.
	 * @return true if the object is found in evacuate memory, false otherwise.
	 */
	MMINLINE bool isObjectInEvacuateMemory(J9Object *objectPtr);
	MMINLINE bool isObjectInEvacuateMemoryNoCheck(J9Object *objectPtr);

	/**
	 * Determine whether the object is in survivor memory or not.
	 * @param objectPtr[in] Object pointer whose properties are being examined.
	 * @return true if the object is found in survivor memory, false otherwise.
	 */
	MMINLINE bool isObjectInSurvivorMemory(J9Object *objectPtr);

	/**
	 * Determine whether the object is in evacuate/survivor memory or not.
	 * @param objectPtr[in] Object pointer whose properties are being examined.
	 * @return true if the object is found in evacuate/survivor memory, false otherwise.
	 */
	MMINLINE bool isObjectInNurseryMemory(J9Object *objectPtr);

	/**
	 * @param doesObjectNeedHash[out]		True, if object need to store hashcode in hashslot
	 * @param isObjectGrowingHashSlot[out]	True, if object need to grow size for hashslot
	 */
	MMINLINE void calculateObjectDetailsForCopy(MM_ScavengerForwardedHeader* forwardedHeader, UDATA *objectCopySizeInBytes, UDATA *objectReserveSizeInBytes, bool *doesObjectNeedHash, bool *isObjectGrowingHashSlot);

	/**
	 * Remove any remaining regions from the reserved allocation list.
	 * @param env GC thread.
	 */
	void clearReservedRegionLists(MM_EnvironmentVLHGC *env);

	/**
	 * Acquire a region for use as a survivor area during copy and forward.
	 * @param env GC thread.
	 * @param regionList Internally managed region list to which the region should be assigned to.
	 * @param compactGroup Compact group for acquired region
	 * @return the newly acquired region or NULL if no region was available
	 */
	MM_HeapRegionDescriptorVLHGC *acquireRegion(MM_EnvironmentVLHGC *env, MM_ReservedRegionListHeader::Sublist *regionList, UDATA compactGroup);

	/**
	 * Inserts newRegion into the given regionList.  The implementation assumes that the calling thread can modify regionList without locking
	 * it so the callsite either needs to have locked the list or be single-threaded.
	 * @param env[in] The GC thread
	 * @param regionList[in] The region list to which the implementation will add newRegion (call site must ensure that no other thread is able to view this until the method returns)
	 * @param newRegion[in] The region which the implementation must add to regionList (the implementation assumes that newRegion is not already in a region list)
	 */
	void insertRegionIntoLockedList(MM_EnvironmentVLHGC *env, MM_ReservedRegionListHeader::Sublist *regionList, MM_HeapRegionDescriptorVLHGC *newRegion);
	
	/**
	 * Release a region as it is deemed to be full and no longer useful.
	 * @param env GC thread.
	 * @param regionList Internally managed region list from which the region should be removed from.
	 * @param region The region to release.
	 */
	void releaseRegion(MM_EnvironmentVLHGC *env, MM_ReservedRegionListHeader::Sublist *regionList, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * Insert the specified tail candidate into the tail candidate list.  The implementation assumes that the calling thread can modify 
	 * regionList without locking it so the callsite either needs to have locked the list or be single-threaded.
	 * @param env[in] The GC thread
	 * @param regionList[in] The region list to which tailRegion should be added as a a tail candidate
	 * @param tailRegion[in] The region to add
	 */
	void insertTailCandidate(MM_EnvironmentVLHGC* env, MM_ReservedRegionListHeader* regionList, MM_HeapRegionDescriptorVLHGC *tailRegion);

	/**
	 * Remove the specified tail candidate from the tail candidate list.  The implementation assumes that the calling thread can modify 
	 * regionList without locking it so the callsite either needs to have locked the list or be single-threaded.
	 * @param env[in] The GC thread
	 * @param regionList[in] The region list which tailRegion belongs to
	 * @param tailRegion[in] The region to remove
	 */
	void removeTailCandidate(MM_EnvironmentVLHGC* env, MM_ReservedRegionListHeader* regionList, MM_HeapRegionDescriptorVLHGC *tailRegion);
	
	/**
	 * Reserve memory for an object to be copied to survivor space.
	 * @param env[in] GC thread.
	 * @param compactGroup The compact group number that the object should be associated with.
	 * @param objectSize Size in bytes to be reserved (precisely, no more or less) for the object to be copied.
	 * @param listLock[out] Returns the lock associated with the returned memory
	 * @return a pointer to the storage reserved for allocation, or NULL on failure.
	 */
	void *reserveMemoryForObject(MM_EnvironmentVLHGC *env, UDATA compactGroup, UDATA objectSize, MM_LightweightNonReentrantLock** listLock);

	/**
	 * Reserve memory for a general cache to be used as the copy destination of a survivor space.
	 * @param env[in] GC thread.
	 * @param compactGroup The compact group number that the cache should be associated with.
	 * @param maxCacheSize The max (give or take) size of the cache being requested.
	 * @param addrBase[out] Location to store the base address of the cache that is acquired.
	 * @param addrTop[out] local to store the top address of the cache that is acquired.
	 * @param listLock[out] Returns the lock associated with the returned memory
	 * @return true if the cache was allocated, false otherwise.
	 */
	bool reserveMemoryForCache(MM_EnvironmentVLHGC *env, UDATA compactGroup, UDATA maxCacheSize, void **addrBase, void **addrTop, MM_LightweightNonReentrantLock** listLock);

	/**
	 * Creates a new chunk of scan caches by using heap memory and attaches them to the free cache list.
	 * @param env[in] A GC thread
	 * @return A new cache entry if the allocation of scan caches succeeded, NULL otherwise
	 */
	MM_CopyScanCacheVLHGC * createScanCacheForOverflowInHeap(MM_EnvironmentVLHGC *env);

	/**
	 * Reserve the specified number of bytes to accommodate an object copy.
	 * @param env The GC thread requesting the heap memory to be allocated.
	 * @param objectToEvacuate Object being copied.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectReserveSizeInBytes Amount of bytes to be reserved (can be greater than the original object size) for copying.
	 * @return a CopyScanCache which contains the reserved memory or NULL if the reserve was not successful.
	 */
	MMINLINE MM_CopyScanCacheVLHGC *reserveMemoryForCopy(MM_EnvironmentVLHGC *env, J9Object *objectToEvacuate, MM_AllocationContextTarok *reservingContext, UDATA objectReserveSizeInBytes);

	void flushCaches(MM_CopyScanCacheVLHGC *cache);
	
	/**
	 * Called at the end of the copy forward operation to flush any remaining data from copy caches and return them to the free list.
	 * @param env[in] the thread whose copy caches should be flushed
	 */
	void addCopyCachesToFreeList(MM_EnvironmentVLHGC *env);

	J9Object *updateForwardedPointer(J9Object *objectPtr);

	/**
	 * Checks to see if there is any scan work in the given list.
	 * @param scanCacheList[in] The list to check
	 * @return True if there is work available in the given list
	 */
	bool isScanCacheWorkAvailable(MM_CopyScanCacheListVLHGC *scanCacheList);
	/**
	 * Checks to see if there is any scan work in any of the lists.
	 * @return True if any scan lists contain work
	 */
	bool isAnyScanCacheWorkAvailable();

	/**
	 * Checks to see if there is any scan work in any of scanCacheLists or workPackets
	 * it is only for CopyForwardHybrid mode
	 * @return True if there is any scan work
	 */
	bool isAnyScanWorkAvailable(MM_EnvironmentVLHGC *env);

	/**
	 * Return the next available survivor (destination) copy scan cache that has work available for scanning.
	 * @param env GC thread.
	 * @return a copy scan cache to be scanned, or NULL if none are available.
	 */
	MM_CopyScanCacheVLHGC *getSurvivorCacheForScan(MM_EnvironmentVLHGC *env);

	/**
	 * Tries to find next scan work from both scanCache and workPackets
	 * return SCAN_REASON_NONE if there is no scan work
	 * @param env[in] The GC thread
	 * @param preferredNumaNode[in] The NUMA node number where the caller would prefer to find a scan cache
	 * @return possible return value(SCAN_REASON_NONE, SCAN_REASON_COPYSCANCACHE, SCAN_REASON_PACKET)
	 */
	ScanReason getNextWorkUnit(MM_EnvironmentVLHGC *env, UDATA preferredNumaNode);

	/**
	 * @param env[in] The GC thread
	 * @param preferredNumaNode[in] The NUMA node number where the caller would prefer to find a scan cache
	 * @return possible return value(SCAN_REASON_NONE, SCAN_REASON_COPYSCANCACHE, SCAN_REASON_PACKET)
	 */
	ScanReason getNextWorkUnitNoWait(MM_EnvironmentVLHGC *env, UDATA preferredNumaNode);

	/**
	 * Tries to find a scan cache from the specified NUMA node or return SCAN_REASON_NONE if there was no work available on that node
	 * @return possible return value(SCAN_REASON_NONE, SCAN_REASON_COPYSCANCACHE)
	 */
	ScanReason getNextWorkUnitOnNode(MM_EnvironmentVLHGC *env, UDATA numaNode);

	/**
	 * Complete scanning in Copy-Forward fashion (consume&produce CopyScanCaches)
	 * If abort happens midway through all produced work is pushed on Marking WorkStack
	 * @param env[in] The GC thread
	 */
	void completeScan(MM_EnvironmentVLHGC *env);
	/**
	 * Complete scanning in Marking fashion (consume&produce on WorkStack)
	 * @param env[in] The GC thread
	 */
	void completeScanForAbort(MM_EnvironmentVLHGC *env);
	/**
	 * Rescan all objects in the specified overflowed region.
	 */
	void cleanRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region, U_8 flagToClean);
	/**
	 * Handling of Work Packets overflow case
	 * Active STW Card Based Overflow Handler only.
	 * For other types of STW Overflow Handlers always return false
	 * @param env[in] The master GC thread
	 * @return true if overflow flag is set
	 */
	bool handleOverflow(MM_EnvironmentVLHGC *env);

	/**
	 * Check if Work Packets overflow
	 * @return true if overflow flag is set
	 */
	bool isWorkPacketsOverflow(MM_EnvironmentVLHGC *env);

	void completeScanCache(MM_EnvironmentVLHGC *env);
	/**
	 * complete scan works from _workStack
	 * only for CopyForward Hybrid mode
	 */
	void completeScanWorkPacket(MM_EnvironmentVLHGC *env);

	/**
	 * Scans all the slots of the given object. Used only in copy-scan cache driven phase.
	 * @param env[in] the current thread
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr[in] current object being scanned
 	 * @param reason[in] reason to scan (dirty card, packet, scan cache, overflow)
 	 */
	MMINLINE void scanObject(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, ScanReason reason);

	/**
	 * Update scan for abort phase (workstack phase)
	 * @param env[in] the current thread
	 * @param objectPtr[in] current object being scanned.
	 * @param reason[in] reason to scan (dirty card, packet, scan cache, overflow)
	 */
	MMINLINE void updateScanStats(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason);
	
	MMINLINE UDATA scanToCopyDistance(MM_CopyScanCacheVLHGC* cache);
	MMINLINE bool bestCacheForScanning(MM_CopyScanCacheVLHGC* copyCache, MM_CopyScanCacheVLHGC** scanCache);

	/**
	 * Calculates whether to change the current cache being scanned for a copy cache, deciding
	 * between one of the two copy caches available, using bestCacheForScanning.
	 *
	 * On entry, nextScanCache should contain the current scan cache. On exit, it may contain
	 * the updated scan cache to use next.
	 *
	 * @param nextScanCache the current scan cache, which may be updated on exit.
	 * @return true if the nextScanCache has been updated with the best cache to scan.
	 */
	MMINLINE bool aliasToCopyCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC** nextScanCache);
	
	/**
	 * Scans the slots of a MixedObject, remembering objects as required. Scanning is interrupted
	 * as soon as there is a copy cache that is preferred to the current scan cache. This is returned
	 * in nextScanCache.
	 *
	 * @note <an iterator>.nextSlot() is one of the hottest methods in scavenging, so
	 * it needs to be inlined as much as possible, but we also need to store that iterator state in the
	 * copy/scan cache. Using the local (ie stack or register) instance of the class (through the default
	 * constructor) allows the compiler to inline the virtual method. Using a pointer from the copy/scan
	 * cache to the abstract class ObjectIterator would not allow the compiler to inline. Hence the
	 * Iterator member functions restore and save.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param scanCache current cache being scanned
	 * @param objectPtr current object being scanned
	 * @param hasPartiallyScannedObject whether current object has been partially scanned already
	 * @param nextScanCache the updated scanCache after re-aliasing.
	 * @return whether current object is still only partially scanned
	 */
	MMINLINE bool incrementalScanMixedObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
		bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache);
	/**
	 * Scans the slots of a java.lang.Class object, remembering objects as required.
	 * This scan will visit both the object itself (as a mixed object) as well as the backing J9Class structure.
	 * Scanning is interrupted as soon as there is a copy cache that is preferred to the current scan cache.
	 * This is returned in nextScanCache.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @note The current implementation will not interrupt, completing the scan of the object before returning.
	 * @see MM_CopyForwardScheme:incrementalScanMixedObjectSlots
	 */
	MMINLINE bool incrementalScanClassObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
		bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache);
	/**
	 * Scans the slots of a java.lang.ClassLoader object, remembering objects as required.
	 * This scan will visit both the object itself (as a mixed object) as well as the backing J9ClassLoader structure.
	 * Scanning is interrupted as soon as there is a copy cache that is preferred to the current scan cache.
	 * This is returned in nextScanCache.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @note The current implementation will not interrupt, completing the scan of the object before returning.
	 * @see MM_CopyForwardScheme:incrementalScanMixedObjectSlots
	 */
	MMINLINE bool incrementalScanClassLoaderObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
			bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache);
	/**
	 * Scans the slots of a PointerArrayObject, remembering objects as required. Scanning is interrupted
	 * as soon as there is a copy cache that is preferred to the current scan cache. This is returned
	 * in nextScanCache.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @see MM_CopyForwardScheme:incrementalScanMixedObjectSlots
	 */
	MMINLINE bool incrementalScanPointerArrayObjectSlots(MM_EnvironmentVLHGC *env, 	MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
		bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache);
	/**
	 * Scans the slots of a ReferenceObject, remembering objects as required. Scanning is interrupted
	 * as soon as there is a copy cache that is preferred to the current scan cache. This is returned
	 * in nextScanCache.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @see MM_CopyForwardScheme:incrementalScanMixedObjectSlots
	 */
	MMINLINE bool incrementalScanReferenceObjectSlots(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_CopyScanCacheVLHGC* scanCache, J9Object *objectPtr,
		bool hasPartiallyScannedObject, MM_CopyScanCacheVLHGC** nextScanCache);
	 /**
	 * Scans the objects to scan in the env->_scanCache.
	 * Slots are scanned until there is an opportunity to alias the scan cache to a copy cache. When
	 * this happens, scanning is interrupted and the present scan cache is either pushed onto the scan
	 * list or "deferred" in thread-local storage. Deferring is done to reduce contention due to the
	 * increased need to change scan cache. If the cache is scanned completely without interruption
	 * the cache is flushed at the end.
	 */
	void incrementalScanCacheBySlot(MM_EnvironmentVLHGC *env);

#if defined(J9VM_GC_FINALIZATION)
	/**
	 * Scan all unfinalized objects in the collection set.
	 * @param env[in] the current thread
	 */
	void scanUnfinalizedObjects(MM_EnvironmentVLHGC *env);

	void scanFinalizableObjects(MM_EnvironmentVLHGC *env);

	/**
	 * Helper method called by scanFinalizableObjects to scan
	 * a list of finalizable objects.
	 *
	 * @param env[in] the current thread
	 * @param headObject[in] the object at the head of the list
	 */
	void scanFinalizableList(MM_EnvironmentVLHGC *env, j9object_t headObject);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * Clear the cycle's mark map for all regions that are part of the evacuate set.
	 * @param env GC thread.
	 */
	void clearMarkMapForPartialCollect(MM_EnvironmentVLHGC *env);
	/**
	 * Clear the cycle's card table for all regions that are part of the evacuate set, if GMP in progress.
	 * Called only, if regions are completely evacuated (no abort)
	 * @param env GC thread.
	 */
	void clearCardTableForPartialCollect(MM_EnvironmentVLHGC *env);

	void workThreadGarbageCollect(MM_EnvironmentVLHGC *env);

	/**
	 * Update the given slot to point at the new location of the object, after copying the object if it was not already.
	 * Attempt to copy (either flip or tenure) the object and install a forwarding pointer at the new location. The object
	 * may have already been copied. In either case, update the slot to point at the new location of the object.
	 *
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr[in] Object being scanned.
	 * @param slotObject the slot to be copied or updated
	 * @return true if copy succeeded (or no copying was involved)
	 */
	MMINLINE bool copyAndForward(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, volatile j9object_t* slot);

	/**
	 * Update the given slot to point at the new location of the object, after copying the object if it was not already.
	 * Attempt to copy (either flip or tenure) the object and install a forwarding pointer at the new location. The object
	 * may have already been copied. In either case, update the slot to point at the new location of the object.
	 *
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr[in] Object being scanned.
	 * @param slot the slot to be copied or updated
	 * @param leafType true if slotObject is leaf Object
	 * @return true if copy succeeded (or no copying was involved)
	 */
	MMINLINE bool copyAndForward(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr, GC_SlotObject *slotObject, bool leafType = false);

	/**
	 * Update the given slot to point at the new location of the object, after copying the object if it was not already.
	 * Attempt to copy (either flip or tenure) the object and install a forwarding pointer at the new location. The object
	 * may have already been copied. In either case, update the slot to point at the new location of the object.
	 *
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr[in] Array object being scanned.
	 * @param startIndex[in] First index of the sub (split)array being scanned.
	 * @param slotObject the slot to be copied or updated
	 * @return true if copy succeeded (or no copying was involved)
	 */
	MMINLINE bool copyAndForwardPointerArray(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9IndexableObject *arrayPtr, UDATA startIndex, GC_SlotObject *slotObject);


	/**
	 * Update the given slot to point at the new location of the object, after copying the object if it was not already.
	 * Attempt to copy (either flip or tenure) the object and install a forwarding pointer at the new location. The object
	 * may have already been copied. In either case, update the slot to point at the new location of the object.
	 *
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtrIndirect the slot to be copied or updated
	 * @param leafType true if the slot is leaf Object
	 * @return true if copy succeeded (or no copying was involved)
	 */
	MMINLINE bool copyAndForward(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, volatile j9object_t* objectPtrIndirect, bool leafType = false);

	/**
	 * If class unloading is enabled, copy the specified object's class object and remember the object as an instance.
	 * @param env[in] the current thread
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr[in] the object whose class should be copied
	 * @return true if copy succeeded (or no copying was involved)
	 */
	MMINLINE bool copyAndForwardObjectClass(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr);

	/**
	 * Answer the new location of an object after it has been copied and forwarded.
	 * Attempt to copy and forward the given object header between the evacuate and survivor areas.  If the object has already
	 * been copied, or the copy is successful, return the updated information.  If the copy is not successful due to insufficient
	 * heap memory, return the original object pointer and raise the "abort" flag.
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param leafType true if the object is leaf object, default = false
	 * @note This routine can set the abort flag for a copy forward.
	 * @note This will respect any alignment requirements due to hot fields etc.
	 * @return an object pointer representing the new location of the object, or the original object pointer on failure.
	 */
	J9Object *copy(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, MM_ScavengerForwardedHeader* forwardedHeader, bool leafType = false);
#if defined(J9VM_GC_ARRAYLETS)
	void updateInternalLeafPointersAfterCopy(J9IndexableObject *destinationPtr, J9IndexableObject *sourcePtr);
#endif /* J9VM_GC_ARRAYLETS */
	
	/**
	 * Push any remaining cached mark map data out before the copy scan cache is released.
	 * @param env GC thread.
	 * @param cache Location of cached mark map data to push out.
	 */
	void flushCacheMarkMap(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cache);

	/**
	 * Update the mark map information (PGC or GMP) for a given copy scan cache.
	 * @param env GC thread.
	 * @param markMap Mark map to update (if necessary).
	 * @param object Heap object reference whose bit in the mark map should be updated.
	 * @param slotIndexIndirect Indirect pointer to the cached slot index to examine and update.
	 * @param bitMaskIndirect Indirect pointer to the cached bit mask to use for updating.
	 * @param atomicHeadSlotIndex Slot index of mark map where the update must be atomic.
	 * @param atomicTailSlotIndex Slot index of mark map where the update must be atomic.
	 */
	MMINLINE void updateMarkMapCache(MM_EnvironmentVLHGC *env, MM_MarkMap *markMap, J9Object *object,
			UDATA *slotIndexIndirect, UDATA *bitMaskIndirect, UDATA atomicHeadSlotIndex, UDATA atomicTailSlotIndex);

	/**
	 * Update any mark maps and transfer card table data as appropriate for a successful copy.
	 * @param env GC thread.
	 * @param srcObject Source object that was copy forwarded.
	 * @param dstObject Destination object that was copy forwarded to.
	 * @param dstCache Destination copy scan cache for the forwarded object.
	 */
	MMINLINE void updateMarkMapAndCardTableOnCopy(MM_EnvironmentVLHGC *env, J9Object *srcObject, J9Object *dstObject, MM_CopyScanCacheVLHGC *dstCache);

	/**
	 * Determine whether a copy forward cycle that has been started did complete successfully.
	 * @return true if the copy forward cycle completed successfully, false otherwise.
	 */
	bool copyForwardCompletedSuccessfully(MM_EnvironmentVLHGC *env);

	/**
	 * Used by the logic which relocates objects to different NUMA nodes by looking up the context which owns a given "source" address in the heap.
	 * @param address[in] A heap address for which the receiver will look up the owning context (must be in the heap)
	 * @return The allocation context which owns the region within which address exists
	 */
	MMINLINE MM_AllocationContextTarok *getContextForHeapAddress(void *address);

	/**
	 * Determine the desired copy cache size for the specified compact group for the current thread.
	 * The size is chosen to balance the opposing problems of fragmentation and contention.
	 * 
	 * @param env[in] the current thread
	 * @param compactGroup the compact group to calculate the size for
	 * @return the desired copy cache size, in bytes 
	 */
	UDATA getDesiredCopyCacheSize(MM_EnvironmentVLHGC *env, UDATA compactGroup);

	/**
	 * Align the specified memory pool so that it can be used for survivor objects.
	 * Specifically, make sure that we can't be copying objects into the area covered by a card which is 
	 * meant to describe objects which were already in the region.
	 * @param env[in] the current thread
	 * @param memoryPool[in] the memoryPool to align
	 * @return the number of bytes lost by aligning the pool
	 */
	UDATA alignMemoryPool(MM_EnvironmentVLHGC *env, MM_MemoryPoolBumpPointer *memoryPool);
	
	/**
	 * Set the specified tail candidate region to be a survivor region.
	 * 1. Set its _survivorBase to survivorBase (essentially set survivorSet to true).
	 * 2. Update its free memory in preparation for copying objects into it
	 * 3. Prepare its reference lists for processing
	 * The caller is responsible for calling rememberAndResetReferenceLists() on the list once it's released all locks.
	 * @param env[in] the current thread
	 * @param region[in] the tail region to convert
	 * @param survivorBase the lowest address in the region where survivor objects can be found
	 */
	void convertTailCandidateToSurvivorRegion(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region, void* survivorBase);

	/**
	 * Scan the root set, copying-and-forwarding any objects found.
	 * @param env[in] the current thread 
	 */
	void scanRoots(MM_EnvironmentVLHGC* env);
	
#if defined(J9VM_GC_LEAF_BITS)
	/**
	 * Copy any leaf children of the specified object.
	 * @param env[in] the current thread
	 * @param reservingContext[in] The context to which we would prefer to copy any objects discovered in this method
	 * @param objectPtr[in] the object whose children should be copied
	 */
	void copyLeafChildren(MM_EnvironmentVLHGC* env, MM_AllocationContextTarok *reservingContext, J9Object* objectPtr);
#endif /* J9VM_GC_LEAF_BITS */

	/**
	 * Calculate estimation for allocation age based on compact group and set it to the merged region
	 * @param[in] env The current thread
	 * @param[in] region The region to adjust age to
	 */
	void setAllocationAgeForMergedRegion(MM_EnvironmentVLHGC* env, MM_HeapRegionDescriptorVLHGC *region);

	/**
	 * check if the Object in jni critical region
	 */
	bool isObjectInNoEvacuationRegions(MM_EnvironmentVLHGC *env, J9Object *objectPtr);

	bool randomDecideForceNonEvacuatedRegion(UDATA ratio);

	/**
	 *  Iterate the slot reference and parse and pass leaf bit of the reference to copy forward
	 *  to avoid to push leaf object to work stack in case the reference need to be marked instead of copied.
	 */
	MMINLINE bool iterateAndCopyforwardSlotReference(MM_EnvironmentVLHGC *env, MM_AllocationContextTarok *reservingContext, J9Object *objectPtr);

protected:

	MM_CopyForwardScheme(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *manager);
	
	/**
	 * Initialize the receivers internal support structures and state.
	 * @param env[in] Main thread.
	 */
	bool initialize(MM_EnvironmentVLHGC *env);
	
	/**
	 * Clean up the receivers internal support structures and state.
	 * @param env[in] Main thread.
	 */
	void tearDown(MM_EnvironmentVLHGC *env);

	void masterSetupForCopyForward(MM_EnvironmentVLHGC *env);
	void masterCleanupForCopyForward(MM_EnvironmentVLHGC *env);
	void workerSetupForCopyForward(MM_EnvironmentVLHGC *env);

	void clearGCStats(MM_EnvironmentVLHGC *env);

	/**
	 * Merge the current threads copy forward stats into the global copy forward stats.
	 */
	void mergeGCStats(MM_EnvironmentVLHGC *env);

#if defined(J9VM_GC_ARRAYLETS)
	/**
	 * After successful copy forward cycle, update leaf region base pointers to newly copied spine locations and
	 * recycle any with spines remaining in evacuate space.
	 */
	void updateLeafRegions(MM_EnvironmentVLHGC *env);
#endif /* J9VM_GC_ARRAYLETS */

	/**
	 * Before a copy forward operation, perform any pre processing required on regions.
	 */
	void preProcessRegions(MM_EnvironmentVLHGC *env);

	/**
	 * After successful copy forward operation, perform any post processing required on regions.
	 */
	void postProcessRegions(MM_EnvironmentVLHGC *env);

	/**
	 * Clean cards for the purpose of finding "roots" into the collection set.
	 * @param env A GC thread involved in the cleaning.
	 */
	void cleanCardTable(MM_EnvironmentVLHGC *env);

	/**
	 * Helper routine to clean cards during a partial collection for the purpose of finding "roots" into the collection set.
	 * @param env A GC thread involved in the cleaning.
	 * @param cardCleaner Card handling instance to manage state transition and actions.
	 */
	void cleanCardTableForPartialCollect(MM_EnvironmentVLHGC *env, MM_CardCleaner *cardCleaner);

	/**
	 * Update or delete any external cycle object data that was involved in the collection set.
	 * This routine will clear the external mark map for the collection set (if successful) or adjust the map on moved objects (abort) and
	 * remove any dead objects from external cycle work packets.  It will also update (forward) any work packet pointers which have survived
	 * the collection.  The responsibility of setting the external cycle mark map survivor bits and updating cards lies with object copying.
	 */
	void updateOrDeleteObjectsFromExternalCycle(MM_EnvironmentVLHGC *env);

	/**
	 * Scans the marked objects in the [lowAddress..highAddress) range.  If rememberedObjectsOnly is set, we will further constrain
	 * this scanned set by only scanning objects in the range which have the OBJECT_HEADER_REMEMBERED bit set.  The receiver uses
	 * its _markMap to determine which objects in the range are marked.
	 *
	 * @param env[in] A GC thread (note that this method could be called by multiple threads, in parallel, but on disjoint address ranges)
	 * @param lowAddress[in] The heap address where the receiver will begin walking objects
	 * @param highAddress[in] The heap address after the last address we will potentially find a live object
	 * @param rememberedObjectsOnly If true, the receiver only scans remembered live objects in the range.  If false, it scans all live objects in the range
	 * @return true if all the objects in the given range were scanned without causing an abort
	 */
	bool scanObjectsInRange(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress, bool rememberedObjectsOnly);

	/**
	 * Checks whether the suggestedContext passed in is a preferred allocation context for
	 * object relocation. If so the same context is returned if not the object's original context
	 * is returned.
	 * @param[in] suggestedContext The allocation context we intended to copy the object into
	 * @param[in] objectPtr A pointer to the object being copied
	 * @return The reservingContext or the object's owning context if the suggestedContext is not a preferred object relocation context
	 */
	MMINLINE MM_AllocationContextTarok *getPreferredAllocationContext(MM_AllocationContextTarok *suggestedContext, J9Object *objectPtr);

public:

	static MM_CopyForwardScheme *newInstance(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *manager);
	void kill(MM_EnvironmentVLHGC *env);

	/**
	 * Run a copy forward collection operation on the already determined collection set.
	 * @param env[in] Main thread.
	 * @return Flag indicating if the copy forward collection was successful or not.
	 */
	bool copyForwardCollectionSet(MM_EnvironmentVLHGC *env);

	/**
	 * Return true if CopyForward is running under Hybrid mode
	 */
	bool isHybrid(MM_EnvironmentVLHGC *env)
	{
		return (0 != _regionCountCannotBeEvacuated);
	}

	void setReservedNonEvacuatedRegions(UDATA regionCount)
	{
		_regionCountReservedNonEvacuated = regionCount;
	}

	friend class MM_CopyForwardGMPCardCleaner;
	friend class MM_CopyForwardNoGMPCardCleaner;
	friend class MM_CopyForwardSchemeTask;
	friend class MM_CopyForwardSchemeRootScanner;
	friend class MM_CopyForwardSchemeRootClearer;
	friend class MM_CopyForwardSchemeAbortScanner;
};

#endif /* COPYFORWARDSCHEME_HPP_ */
