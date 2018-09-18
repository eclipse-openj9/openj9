
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(GLOBALMARKINGSCHEME_HPP_)
#define GLOBALMARKINGSCHEME_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseVirtual.hpp"

#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "ModronTypes.hpp"
#include "WorkStack.hpp"
#include "HeapRegionManager.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "ParallelTask.hpp"

/**
 * @}
 */
class MM_CardCleaner;
class MM_Collector;
class MM_CycleState;
class MM_Dispatcher;
class MM_GlobalGCStats;
class MM_GlobalMarkingScheme;
class MM_GlobalMarkingSchemeRootMarker;
class MM_GlobalMarkingSchemeRootClearer;
class MM_InterRegionRememberedSet;
class MM_MarkMap;
class MM_ReferenceStats;
class MM_RootScanner;
class MM_SublistPool;
class MM_WorkPacketsVLHGC;


enum MarkAction {
	MARK_ALL = 1,
	MARK_INIT,
	MARK_ROOTS,
	MARK_SCAN,
	MARK_COMPLETE
};

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelGlobalMarkTask : public MM_ParallelTask
{
private:
	MM_GlobalMarkingScheme *_markingScheme;
	const MarkAction _action; /**< specify which action Mark Setup to do*/
	bool _timeLimitWasHit;	/**< true if any requests to check the time came in after we had hit our time threshold */
	const I_64 _timeThreshold;	/**< The millisecond which represents the end of this increment */
	MM_CycleState *_cycleState; /**< Current cycle state information */
	
public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_MARK; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	
	virtual void masterSetup(MM_EnvironmentBase *env);
	virtual void masterCleanup(MM_EnvironmentBase *env);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	virtual bool shouldYieldFromTask(MM_EnvironmentBase *env);

	/**
	 * @return True if this task hit its timeout limit (does not check if NOW is after the time limit, though)
	 */
	MMINLINE bool didTimeout() const { return _timeLimitWasHit; }

	/**
	 * Create a ParallelMarkTask object.
	 */
	MM_ParallelGlobalMarkTask(MM_EnvironmentBase *env,
			MM_Dispatcher *dispatcher, 
			MM_GlobalMarkingScheme *markingScheme, 
			MarkAction action,
			I_64 timeThreshold,
			MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_markingScheme(markingScheme)
		,_action(action)
		,_timeLimitWasHit(false)
		,_timeThreshold(timeThreshold)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};


class MM_ConcurrentGlobalMarkTask : public MM_ParallelGlobalMarkTask
{
	/* Data Members */
private:
	UDATA const _bytesToScan;	/**< The number of bytes that this must scan before it will stop trying to do more work */
	UDATA volatile _bytesScanned;	/**< The number of bytes scanned by this */
	bool _didReturnEarly;	/**< True if any thread in this task returned before the mark phase completed (either due to a scanning limit, a timing limit, or an external termination request) */
	volatile bool * const _forceExit;	/**< Shared state concurrently updated by an external thread to force the receiver to cause all threads to yield (by setting the destination of the pointer to true) */
protected:
public:

	/* Member Functions */
private:
protected:
public:
	virtual UDATA getVMStateID() 
	{ 
		return OMRVMSTATE_GC_CONCURRENT_MARK_TRACE;
	}

	UDATA getBytesScanned()
	{
		return _bytesScanned;
	}
	bool didReturnEarly()
	{
		return _didReturnEarly;
	}
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	virtual bool shouldYieldFromTask(MM_EnvironmentBase *env);

	/**
	 * Create a MM_ConcurrentGlobalMarkTask object.
	 */
	MM_ConcurrentGlobalMarkTask(MM_EnvironmentBase *env,
			MM_Dispatcher *dispatcher, 
			MM_GlobalMarkingScheme *markingScheme, 
			MarkAction action,
			UDATA bytesToScan,
			volatile bool *forceExit,
			MM_CycleState *cycleState) :
		MM_ParallelGlobalMarkTask(env, dispatcher, markingScheme, action, I_64_MAX, cycleState)
		, _bytesToScan(bytesToScan)
		, _bytesScanned(0)
		, _didReturnEarly(false)
		, _forceExit(forceExit)
	{
		_typeId = __FUNCTION__;
	};
};


/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_GlobalMarkingScheme : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	J9JavaVM *const _javaVM;
	MM_GCExtensions *const _extensions;
	void *_heapBase;
	void *_heapTop;
	MM_MarkMap *_markMap;
	UDATA _arraySplitSize;
	MM_HeapRegionManager *const _heapRegionManager;	/**< This HRM is retained since it is needed to quickly resolve the table region of a given object */
	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool _dynamicClassUnloadingEnabled;  /**< Local cached value from cycle state for performance reasons (TODO: Reevaluate) */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	MM_InterRegionRememberedSet *_interRegionRememberedSet;	/**< A cached pointer to the  inter-region reference tracking  */
	const bool _collectStringConstantsEnabled;
	const UDATA _regionSize;	/**< Cached copy of the region size used for short-circuiting region matching checks with the XOR-and-compare */

	/**
	 * Codes used to indicate why an object is being scanned
	 */
	enum ScanReason {
		SCAN_REASON_PACKET = 1, /**< Indicates the object being scanned came from a work packet */
		SCAN_REASON_DIRTY_CARD = 2, /**< Indicates the object being scanned was found in a dirty card */
		SCAN_REASON_OVERFLOWED_REGION = 3, /**< Indicates the object being scanned was in an overflowed region */
	};
	
	/* 
	 * Function members
	 */
private:

	/**
	 * The appropriate remembered object handling for the reference between from and to
	 * @note This must be private as it is inlined in the implementation and will not generate a symbol
	 * @param env[in] The GC thread
	 * @param from[in] The object this reference points from (not NULL)
	 * @param to[in] The object this reference points to (could be NULL)
	 */
	void rememberReferenceIfRequired(MM_EnvironmentVLHGC *env, J9Object *from, J9Object *to);

	bool markObjectNoCheck(MM_EnvironmentVLHGC *env, J9Object *objectPtr, bool leafType = false);

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
	 * Process the list of reference objects recorded in the specified list.
	 * References with unmarked referents are cleared and optionally enqueued.
	 * SoftReferences have their ages incremented.
	 * @param env[in] the current thread
	 * @param headOfList[in] the first object in the linked list 
	 * @param referenceStats copy forward stats substructure to be updated
	 */
	void processReferenceList(MM_EnvironmentVLHGC *env, J9Object* headOfList, MM_ReferenceStats *referenceStats);

#if defined(J9VM_GC_FINALIZATION)
	/**
	 * Scan all unfinalized objects in the collection set.
	 * @param env[in] the current thread
	 */
	void scanUnfinalizedObjects(MM_EnvironmentVLHGC *env);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * Scan all ownable synchronizer objects in the collection set.
	 * @param env[in] the current thread
	 */
	void scanOwnableSynchronizerObjects(MM_EnvironmentVLHGC *env);

	bool isMarked(J9Object *objectPtr);

	/**
	 * Handling of Work Packets overflow case
	 * Active STW Card Based Overflow Handler only.
	 * For other types of STW Overflow Handlers always return false
	 * @param env[in] The master GC thread
	 * @return true if overflow flag is set
	 */
	bool handleOverflow(MM_EnvironmentVLHGC *env);

	/**
	 * Update scan stats: object and byte counts, separately for each of reasons. Object count is just incremented by 1, and byte count is incremented by provided byte count.
	 * @param env[in] the current thread
	 * @param bytesScanned[in] count of bytes scanned (typically object size, but could be less in case of split array scanning)
	 * @param reason[in] reason to scan (dirty card, packet,...)
	 */
	MMINLINE void updateScanStats(MM_EnvironmentVLHGC *env, UDATA bytesScanned, ScanReason reason);

	/**
	 * Scan the specified object. The caller is responsible for recording the time
	 * taken to scan the object in MM_MarkVLHGCStats::_scanTime
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to scan. Must be a non-NULL object on the heap.
	 * @param reason a code indicating why the object is being scanned
	 */
	void scanObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason);

	/**
	 * Scan the specified object which has already been marked.
	 * 1. Mark any objects it points to, optionally including it's class
	 * 2. Enqueue those objects for scanning, if necessary
	 * 3. Update the remembered set
	 * 
	 * @param env[in] the current thread
	 * @param objectPtr[in] the mixed object to scan
 	 * @param reason a code indicating why the object is being scanned
	 */
	void scanMixedObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason);
	
	/**
	 * Scan the specified instance of java.lang.Class.
	 * 1. Scan the object itself, as in scanMixedObject()
	 * 2. Mark and enqueue any objects in static fields
	 * 3. Mark and enqueue any objects in the constant pool
	 * 4. Also scan any replaced (how-swapped) versions of the class
	 * @note This may safely be called for initialized or uninitialized class objects.
	 * 
	 * @param env[in] the current thread
	 * @param objectPtr[in] an instance of java.lang.Class
	 * @param reason a code indicating why the object is being scanned
	 */
	void scanClassObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason);
	
	/**
	 * Scan the specified instance of java.lang.ClassClassLoader, or one of its subclasses.
	 * 1. Scan the object itself, as in scanMixedObject()
	 * 2. Mark and enqueue any classes found in the class table
	 * @note This may safely be called for initialized or uninitialized class loader objects.
	 * 
	 * @param env[in] the current thread
	 * @param objectPtr[in] an instance of java.lang.ClassLoader or one of its subclasses
	 * @param reason a code indicating why the object is being scanned
	 */
	void scanClassLoaderObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason);
	
	/**
	 * Scan the slots of a J9ClassLoader object.
	 * Mark all relevant slots values found in the object.  This includes the java.lang.ClassLoader object.
	 *
	 * Should only be used by J9ClassLoaders which may not have a java.lang.ClassLoader object (system and application).  Everyone else should
	 * go through scanClassLoaderObjectSlots().
	 * @param env current GC thread.
	 * @param objectPtr current object being scanned.
	 * @param reason a code indicating why the object is being scanned
	 */
	void scanClassLoaderSlots(MM_EnvironmentVLHGC *env, J9ClassLoader *classLoaderObject, ScanReason reason = SCAN_REASON_PACKET);

	/**
	 * Scan the specified instance of java.lang.ref.Reference, or one of its subclasses.
	 * 1. Scan the object itself, as in scanMixedObject()
	 * 2. Except, use special handling for the weak referent field
	 * @note The reference object must be on one of the reference object queues.
	 * 
	 * @param env[in] the current thread
	 * @param objectPtr[in] an instance of a reference object
	 * @param reason a code indicating why the object is being scanned
	 */
	void scanReferenceMixedObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, ScanReason reason);
	
	/**
	 * Scan the specified portion of the specified object array.
	 * If the function does not scan to the end of the array, the remaining portion of the
	 * array is enqueued for further scanning.
	 * 
	 * @param env[in] the current thread
	 * @param objectPtr[in] an object array
	 * @param indexArg the index to start scanning from
	 * @param reason a code indicating why the object is being scanned
	 */
	UDATA scanPointerArrayObjectSplit(MM_EnvironmentVLHGC *env, J9IndexableObject *objectPtr, UDATA indexArg, ScanReason reason);

	/**
	 * Scan the specified object array.
	 * If the function does not scan to the end of the array, the remaining portion of the
	 * array is enqueued for further scanning.
	 * 
	 * @param env[in] the current thread
	 * @param objectPtr[in] an object array
	 * @param reason a code indicating why the object is being scanned
	 */
	void scanPointerArrayObject(MM_EnvironmentVLHGC *env, J9IndexableObject *objectPtr, ScanReason reason);

	MMINLINE bool isCollectStringConstantsEnabled() { return _collectStringConstantsEnabled; };

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MMINLINE bool isDynamicClassUnloadingEnabled() { return _dynamicClassUnloadingEnabled; };
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	/**
	 * Rescan all objects in the specified overflowed region.
	 */
	void cleanRegion(MM_EnvironmentVLHGC *env, MM_HeapRegionDescriptorVLHGC *region, U_8 flagToClean);
		
	/**
	 * Clean cards for the purpose of finding "roots" into the collection set.
	 * @param env A GC thread involved in the cleaning.
	 * @param cardCleaner The cleaner to be used
	 */
	void cleanCardTableForGlobalCollect(MM_EnvironmentVLHGC *env, MM_CardCleaner *cardCleaner);

	/*
	 * Initialize (clear) next mark map regions, that are not already cleared
	 * @param env[in] GC thread
	 */
	void initializeMarkMap(MM_EnvironmentVLHGC *env);

	/**
	 * If dynamic class unloading is enabled, mark the class of objectPtr and remember the instance.
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object whose class should be marked
	 */
	MMINLINE void markObjectClass(MM_EnvironmentVLHGC *env, J9Object *objectPtr);

protected:
	virtual bool initialize(MM_EnvironmentVLHGC *env);
	virtual void tearDown(MM_EnvironmentVLHGC *env);
	MMINLINE bool isHeapObject(J9Object* objectPtr)
	{
		return ((_heapBase <= (U_8 *)objectPtr) && (_heapTop > (U_8 *)objectPtr));
	}

public:
	static MM_GlobalMarkingScheme *newInstance(MM_EnvironmentVLHGC *env); 
	virtual void kill(MM_EnvironmentVLHGC *env);
	
	void masterSetupForGC(MM_EnvironmentVLHGC *env);
	void masterCleanupAfterGC(MM_EnvironmentVLHGC *env);
	void workerSetupForGC(MM_EnvironmentVLHGC *env);

	/**
	 * Configures the marking scheme with data it requires before it can be used.
	 * @param markMap The mark map in which to mark objects
	 * @param dynamicClassUnloadingEnabled true if we want to attempt class unloading
	 */
	void setCachedState(MM_MarkMap *markMap, bool dynamicClassUnloadingEnabled);

	/**
	 * Clears the cycle state components which the receiver had cached at the beginning of the cycle in setCachedState.
	 */
	void clearCachedState();

	/**
	 *  Initialization for Mark
	 *  Actual startup for Mark procedure
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsInit(MM_EnvironmentVLHGC *env);

	/**
	 *  Mark Roots
	 *  Create Root Scanner and Mark all roots including classes and classloaders if dynamic class unloading is enabled
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsRoots(MM_EnvironmentVLHGC *env);

	/**
	 *  Scan (complete)
	 *  Process all work packets from queue, including classes and work generated during this phase.
	 *  On return, all live objects will be marked, although some unreachable objects (such as finalizable 
	 *  objects) may still be revived by by subsequent stages.   
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsScan(MM_EnvironmentVLHGC *env);
	
	/**
	 *  Final Mark services including scanning of Clearables
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsComplete(MM_EnvironmentVLHGC *env);

	/**
	 * Synchronizes all threads and calls handleOverflow, continuing to do so until overflow is finally resolved.
	 * @param env[in] A GC thread
	 */
	void resolveOverflow(MM_EnvironmentVLHGC *env);

	bool markObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr, bool leafType = false);

	void completeScan(MM_EnvironmentVLHGC *env);
	
	bool heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	/**
	 * Scans the marked objects in the [lowAddress..highAddress) range.  If rememberedObjectsOnly is set, we will further constrain
	 * this scanned set by only scanning objects in the range which have the OBJECT_HEADER_REMEMBERED bit set.  The receiver uses
	 * its _markMap to determine which objects in the range are marked.
	 * 
	 * @param env[in] A GC thread (note that this method could be called by multiple threads, in parallel, but on disjoint address ranges)
	 * @param lowAddress[in] The heap address where the receiver will begin walking objects
	 * @param highAddress[in] The heap address after the last address we will potentially find a live object
	 */
	void scanObjectsInRange(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress);

	/**
	 * Flush any thread-specific buffers at the end of a GC increment.
	 * @param env[in] the current thread
	 */
	void flushBuffers(MM_EnvironmentVLHGC *env);
	
	/**
	 * Create a GlobalMarkingScheme object.
	 */
	MM_GlobalMarkingScheme(MM_EnvironmentVLHGC *env)
		: _javaVM((J9JavaVM *)env->getLanguageVM())
		, _extensions(MM_GCExtensions::getExtensions(env))
		, _heapBase(NULL)
		, _heapTop(NULL)
		, _markMap(NULL)
		, _heapRegionManager(_extensions->getHeap()->getHeapRegionManager())
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		, _dynamicClassUnloadingEnabled(false)
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		, _interRegionRememberedSet(NULL)
		, _collectStringConstantsEnabled(_extensions->collectStringConstants)
		, _regionSize(_extensions->regionSize)
	{
		_typeId = __FUNCTION__;
	}
	
	/*
	 * Friends
	 */
	friend class MM_GlobalMarkingSchemeRootMarker;
	friend class MM_GlobalMarkingSchemeRootClearer;
};

#endif /* GLOBALMARKINGSCHEME_HPP_ */

