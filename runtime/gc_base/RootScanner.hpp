
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
 * @ingroup GC_Base
 */

#ifndef ROOTSCANNER_HPP_
#define ROOTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "omr.h"

#include "BaseVirtual.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "JVMTIObjectTagTableIterator.hpp"
#include "ModronTypes.hpp"
#include "RootScannerTypes.h"
#include "Task.hpp"
#include "VMClassSlotIterator.hpp"

class GC_SlotObject;
class MM_MemoryPool;
class MM_CollectorLanguageInterfaceImpl;

/**
 * General interface for scanning all object and class slots in the system that are not part of the heap.
 * 
 * MM_RootScanner provides an abstract class that can be specialized to scan particular slots
 * in the system, including all, root specific and clearable slots.  The purpose of the class is
 * to provide a central location for general slot scanners within Modron (e.g., root scanning,
 * all slots do, etc).
 * 
 * There are two levels of specialization for the scanner, structure walking and handling of elements.
 * Structure walking specialization, where the implementer can override the way in which we walk elements,
 * should be done rarely and in only extreme circumstances.  Handling of elements can be specialized for all
 * elements as well as for specific types of structures.
 * 
 * The core routines to be reimplemented are doSlot(J9Object **), doClassSlot(J9Class *) and doClass(J9Class *).
 * All other slot types are forwarded by default to these routines for processing.  To handle structure slots in 
 * specific ways, the slot handler for that type should be overridden.
 * 
 * @ingroup GC_Base
 */
class MM_RootScanner : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	bool _isContinuationListEmpty;
protected:
	MM_EnvironmentBase *_env;
	MM_GCExtensions *_extensions;
	MM_CollectorLanguageInterfaceImpl *_clij;
	J9JavaVM *_javaVM;

	bool _stringTableAsRoot;  /**< Treat the string table as a hard root */
	bool _jniWeakGlobalReferencesTableAsRoot;	/**< Treat JNI Weak References Table as a hard root */
	bool _singleThread;  /**< Should the iterator operate in single threaded mode */

	bool _nurseryReferencesOnly;  /**< Should the iterator only scan structures that currently contain nursery references */
	bool _nurseryReferencesPossibly;  /**< Should the iterator only scan structures that may contain nursery references */
	bool _includeStackFrameClassReferences;  /**< Should the iterator include Classes which have a method running on the stack */
#if defined(J9VM_GC_MODRON_SCAVENGER)		 
	bool _includeRememberedSetReferences;  /**< Should the iterator include references in the Remembered Set (if applicable) */
#endif /* J9VM_GC_MODRON_SCAVENGER */	 	
	bool _classDataAsRoots; /**< Should all classes (and class loaders) be treated as roots. Default true, should set to false when class unloading */
	bool _includeJVMTIObjectTagTables; /**< Should the iterator include the JVMTIObjectTagTables. Default true, should set to false when doing JVMTI object walks */
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
	bool _includeVirtualLargeObjectHeap; /**< Enables scanning of objects that has been allocated at sparse heap. Default is false */
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
	bool _trackVisibleStackFrameDepth; /**< Should the stack walker be told to track the visible frame depth. Default false, should set to true when doing JVMTI walks that report stack slots */

	U_64 _entityStartScanTime; /**< The start time of the scan of the current scanning entity, or 0 if no entity is being scanned.  Defaults to 0. */
	U_64 _entityIncrementStartTime; /**< Start time of current increment with a scan entity (Metronome may have several increment for each entity) */
	U_64 _entityIncrementEndTime; /**< End time of the current increment */
	RootScannerEntity _scanningEntity; /**< The root scanner entity that is currently being scanned. Defaults to RootScannerEntity_None. */ 
	RootScannerEntity _lastScannedEntity; /**< The root scanner entity that was last scanned. Defaults to RootScannerEntity_None. */

	/*
	 * Function members
	 */
private:
	/**
	 * Scan all fields of mixed object
	 * @param objectPtr address of object to scan
	 */
	void scanMixedObject(J9Object *objectPtr);

	/**
	 * Scan all fields of pointer array object
	 * @param objectPtr address of object to scan
	 * @param memoryPool current memory pool
	 * @param manager current region manager
	 * @param memoryType memory type
	 */
	void scanArrayObject(MM_EnvironmentBase *env, J9Object *objectPtr, MM_MemoryPool *memoryPool, MM_HeapRegionManager *manager, uintptr_t memoryType);

	bool isContinuationListEmpty(MM_EnvironmentBase *env);

	/**
	 * Scan all objects from class loader.
	 * @param env thread GC environment
	 * @param classLoader class loader address
	 */
	void scanClassloader(MM_EnvironmentBase *env, J9ClassLoader *classLoader);

protected:
	/* Family of yielding methods to be overridden by incremental scanners such
	 * as the RealtimeRootScanner. The default implementations of these do
	 * nothing. 
	 */
	
	/**
	 * Root scanning methods that have been incrementalized are responsible for
	 * calling this method periodically during class scan to check when they should
	 * yield.
	 * @return true if the GC should yield, false otherwise
	 */
	virtual bool shouldYieldFromClassScan(uintptr_t timeSlackNanoSec = 0);

	/**
	 * Root scanning methods that have been incrementalized are responsible for
	 * calling this method periodically during string scan to check when they should
	 * yield.
	 * @return true if the GC should yield, false otherwise
	 */
	virtual bool shouldYieldFromStringScan();

	/**
	 * Root scanning methods that have been incrementalized are responsible for
	 * calling this method periodically during monitor scan to check when they should
	 * yield.
	 * @return true if the GC should yield, false otherwise
	 */
	virtual bool shouldYieldFromMonitorScan();

	/**
	 * Root scanning methods that have been incrementalized are responsible for
	 * calling this method periodically to check when they should yield.
	 * @return true if the GC should yield, false otherwise
	 */
	virtual bool shouldYield();

	/**
	 * Root scanning methods that have been incrementalized should call this method
	 * after determining that it should yield and after releasing locks held during
	 * root processing.  Upon returning from this method, the root scanner is
	 * responsible for re-acquiring necessary locks.  The integrity of iterators
	 * and the semantics of atomicity are not ensured and must be examined on a
	 * case-by-case basis.
	 */
	virtual void yield();

	/**
	 * Two in one method which amounts to yield if shouldYield is true.
	 * @return true if yielded, false otherwise
	 */
	virtual bool condYield(U_64 timeSlackNanoSec = 0);
	
	virtual void flushRequiredNonAllocationCaches(MM_EnvironmentBase *envModron){}
	
	/**
	 * Sets the currently scanned root entity to scanningEntity. This is mainly for
	 * debug purposes.
	 * @param scanningEntity The entity for which scanning has begun.
	 */
	MMINLINE void
	reportScanningStarted(RootScannerEntity scanningEntity)
	{
		/* Ensures reportScanningEnded was called previously. */
		assume0(RootScannerEntity_None == _scanningEntity);
		_scanningEntity = scanningEntity;
		
		if (_extensions->rootScannerStatsEnabled) {
			OMRPORT_ACCESS_FROM_OMRVM(_javaVM->omrVM);
			_entityStartScanTime = omrtime_hires_clock();	
			_entityIncrementStartTime = _entityStartScanTime;
		}
	}
	
	MMINLINE void updateScanStats(uint64_t endTime)
	{
		if (_entityIncrementStartTime >= endTime) {
			/* overflow */
 			_env->_rootScannerStats._entityScanTime[_scanningEntity] += 1;
 		} else {
			uint64_t duration = endTime - _entityIncrementStartTime;
			_env->_rootScannerStats._entityScanTime[_scanningEntity] += duration;
			if (duration > _env->_rootScannerStats._maxIncrementTime) {
				_env->_rootScannerStats._maxIncrementTime = duration;
				_env->_rootScannerStats._maxIncrementEntity = _scanningEntity;
			}
		}
	}	
	
	/**
	 * Sets the currently scanned root entity to None and sets the last scanned root
	 * entity to scannedEntity. This is mainly for debug purposes.
	 * @param scannedEntity The entity for which scanning has ended.
	 */
	MMINLINE void
	reportScanningEnded(RootScannerEntity scannedEntity)
	{
		/* Ensures scanning ended for the currently scanned entity. */
		Assert_MM_true(_scanningEntity == scannedEntity);
		
		if (_extensions->rootScannerStatsEnabled) {
			OMRPORT_ACCESS_FROM_OMRVM(_javaVM->omrVM);
 			uint64_t entityEndScanTime = omrtime_hires_clock();
			
			_env->_rootScannerStats._statsUsed = true;
			_extensions->rootScannerStatsUsed = true;
							
			updateScanStats(entityEndScanTime);
 			
 			_entityStartScanTime = 0;
 			/* In theory, it would be cleaner to reset _entityIncrementStartTime to 0, but sometimes increments could be dis-associated from any entity.
 			    For example sync barrier between two entities. Since they can suspend, they will try to report the duration of the increment,
 			    but the start point 0 would lead to invalid (too long duration). Hence, we set the start point to the current time, just in case it 
 			    is used be non-tracked increments.
 			 */
			_entityIncrementStartTime = entityEndScanTime;
 		}

		_lastScannedEntity = _scanningEntity;
 		_scanningEntity = RootScannerEntity_None;
	}

	/**
	 * Scans Modularity objects that are owned by a given class loader
	 * @param classLoader The class loader in question
	 */
	void scanModularityObjects(J9ClassLoader * classLoader);

public:
	/**
	 * Determine whether running method classes in stack frames should be walked.
	 * @return boolean determining whether running method classes in stack frames should be walked
	 */
	MMINLINE bool isStackFrameClassWalkNeeded() {
		if (_nurseryReferencesOnly || _nurseryReferencesPossibly) {
			return false;
		}
		return 	_includeStackFrameClassReferences;
	}

	/** 
	 * Maintain start/end increment times when scan is suspended. Add the diff (duration) to scan entity time. 
	 * Also maintain max increment duration and its entity
	 */	
	MMINLINE void
	reportScanningSuspended()
	{
		if (_extensions->rootScannerStatsEnabled) {
			OMRPORT_ACCESS_FROM_OMRVM(_javaVM->omrVM);
			_entityIncrementEndTime = omrtime_hires_clock();
			
			updateScanStats(_entityIncrementEndTime);
		}
	}

	/** 
	 * Maintain start/end increment times, when scan is resumed.
	 */	
	MMINLINE void
	reportScanningResumed()
	{
		if (_extensions->rootScannerStatsEnabled) {
			OMRPORT_ACCESS_FROM_OMRVM(_javaVM->omrVM);
			_entityIncrementStartTime = omrtime_hires_clock();
			_entityIncrementEndTime = 0;	
		}
	}

	MM_RootScanner(MM_EnvironmentBase *env, bool singleThread = false)
		: MM_BaseVirtual()
		, _isContinuationListEmpty(false)
		, _env(env)
		, _extensions(MM_GCExtensions::getExtensions(env))
		, _clij((MM_CollectorLanguageInterfaceImpl *)_extensions->collectorLanguageInterface)
		, _javaVM((J9JavaVM *)env->getOmrVM()->_language_vm)
		, _stringTableAsRoot(true)
		, _jniWeakGlobalReferencesTableAsRoot(false)
		, _singleThread(singleThread)
		, _nurseryReferencesOnly(false)
		, _nurseryReferencesPossibly(false)
		, _includeStackFrameClassReferences(true)
#if defined(J9VM_GC_MODRON_SCAVENGER)		 
		, _includeRememberedSetReferences(_extensions->scavengerEnabled ? true : false)
#endif /* J9VM_GC_MODRON_SCAVENGER */
		, _classDataAsRoots(true)
		, _includeJVMTIObjectTagTables(true)
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
		, _includeVirtualLargeObjectHeap(_extensions->indexableObjectModel.isVirtualLargeObjectHeapEnabled())
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
		, _trackVisibleStackFrameDepth(false)
		, _entityStartScanTime(0)
		, _entityIncrementStartTime(0)
		, _entityIncrementEndTime(0)		
		, _scanningEntity(RootScannerEntity_None)
		, _lastScannedEntity(RootScannerEntity_None)
	{
		_typeId = __FUNCTION__;
		
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		_entityIncrementStartTime = omrtime_hires_clock();

	}

	/**
	 * Return codes from root scanner complete phase calls that are allowable by implementers.
	 */
	typedef enum {
		complete_phase_OK = 0,  /**< Continue scanning */
		complete_phase_ABORT,  /**< Abort all further scanning */
	} CompletePhaseCode;

	/** Set whether the string table should be treated as a hard root or not */
	void setStringTableAsRoot(bool stringTableAsRoot) {
		_stringTableAsRoot = stringTableAsRoot;
	}

#if defined(J9VM_GC_MODRON_SCAVENGER)
	/** Set whether the iterator will only scan structures which contain nursery references */
	void setNurseryReferencesOnly(bool nurseryReferencesOnly) {
		_nurseryReferencesOnly = nurseryReferencesOnly;
	}

	/** Set whether the iterator will only scan structures which may contain nursery references */
	void setNurseryReferencesPossibly(bool nurseryReferencesPossibly) {
		_nurseryReferencesPossibly = nurseryReferencesPossibly;
	}

	/** Set whether the iterator will scan the remembered set references (if applicable to the scan type) */
	void setIncludeRememberedSetReferences(bool includeRememberedSetReferences) {
		_includeRememberedSetReferences = includeRememberedSetReferences;
	}
#endif /* J9VM_GC_MODRON_SCAVENGER */

	/** Set whether the iterator will scan stack frames for the classes of running methods */
	void setIncludeStackFrameClassReferences(bool includeStackFrameClassReferences) {
		_includeStackFrameClassReferences = includeStackFrameClassReferences;
	}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/** Set whether all classes and their classloaders (or only permanent ones) are considered roots */
	void setClassDataAsRoots(bool classDataAsRoots) {
		_classDataAsRoots = classDataAsRoots;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	bool getClassDataAsRoots() {
		return _classDataAsRoots;
	}
#if defined(J9VM_OPT_JVMTI)
	/** Set whether the iterator will scan the JVMTIObjectTagTables (if applicable to the scan type) */
	void setIncludeJVMTIObjectTagTables(bool includeJVMTIObjectTagTables) {
		_includeJVMTIObjectTagTables = includeJVMTIObjectTagTables;
	}
#endif /* J9VM_OPT_JVMTI */

	/** Set whether the iterator will scan the JVMTIObjectTagTables (if applicable to the scan type) */
	void setTrackVisibleStackFrameDepth(bool trackVisibleStackFrameDepth) {
		 _trackVisibleStackFrameDepth = trackVisibleStackFrameDepth;
	}

	bool getTrackVisibleStackFrameDepth() {
		return _trackVisibleStackFrameDepth;
	}

	/** General object slot handler to be reimplemented by specializing class. This handler is called for every reference to a J9Object. */
	virtual void doSlot(J9Object **slotPtr) = 0;

	/** General class slot handler to be reimplemented by specializing class. This handler is called for every reference to a J9Class. */
	virtual void doClassSlot(J9Class *classPtr);

	/** General class handler to be reimplemented by specializing class. This handler is called once per class. */
	virtual void doClass(J9Class *clazz) = 0;

	/**
	 * Scan object field
	 * @param slotObject for field
	 */
	virtual void doFieldSlot(GC_SlotObject *slotObject);
	
	virtual void scanRoots(MM_EnvironmentBase *env);
	virtual void scanClearable(MM_EnvironmentBase *env);
	virtual void scanAllSlots(MM_EnvironmentBase *env);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	virtual void scanRememberedSet(MM_EnvironmentBase *env);
#endif /* J9VM_GC_MODRON_SCAVENGER */
	
	virtual void scanClasses(MM_EnvironmentBase *env);
	virtual void scanVMClassSlots(MM_EnvironmentBase *env);
	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	void scanPermanentClasses(MM_EnvironmentBase *env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	virtual CompletePhaseCode scanClassesComplete(MM_EnvironmentBase *env);

	virtual bool scanOneThread(MM_EnvironmentBase *env, J9VMThread *walkThread, void *localData);
	
	virtual void scanClassLoaders(MM_EnvironmentBase *env);
	virtual void scanThreads(MM_EnvironmentBase *env);
	virtual void scanSingleThread(MM_EnvironmentBase *env, J9VMThread *walkThread);
#if defined(J9VM_GC_FINALIZATION)
	virtual void scanFinalizableObjects(MM_EnvironmentBase *env);
	virtual void scanUnfinalizedObjects(MM_EnvironmentBase *env);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * @todo Provide function documentation
	 *
	 * @NOTE this can only be used as a READ-ONLY version of the ownableSynchronizerObjectList.
	 * If you need to modify elements with-in the list you will need to provide your own functionality.
	 * See MM_MarkingScheme::scanUnfinalizedObjects(MM_EnvironmentStandard *env) as an example
	 * which modifies elements within the list.
	 */
	virtual void scanOwnableSynchronizerObjects(MM_EnvironmentBase *env);
	/**
	 * scanContinuationObjects() is for removing continuation objects, which are dead or last unmounted,
	 * from the continuation lists within a scope of current GC(for example just for Nursery in Scavenge).
	 */
	virtual void scanContinuationObjects(MM_EnvironmentBase *env);
	/**
	 * iterateAllContinuationObjects() is for iterating all live continuation objects in the heap and doing any processing
	 * that other parties may register through a hook [J9HOOK_MM_WALKCONTINUATION].
	 */
	virtual void iterateAllContinuationObjects(MM_EnvironmentBase *env) {}

	virtual void scanStringTable(MM_EnvironmentBase *env);
	void scanJNIGlobalReferences(MM_EnvironmentBase *env);
	virtual void scanJNIWeakGlobalReferences(MM_EnvironmentBase *env);

    virtual void scanMonitorReferences(MM_EnvironmentBase *env);
    virtual CompletePhaseCode scanMonitorReferencesComplete(MM_EnvironmentBase *env);
	virtual void scanMonitorLookupCaches(MM_EnvironmentBase *env);

#if defined(J9VM_OPT_JVMTI)
	void scanJVMTIObjectTagTables(MM_EnvironmentBase *env);
#endif /* J9VM_OPT_JVMTI */

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
	/**
	 * Scans each heap region for arraylet leaves that contains a non-NULL
	 * contiguous address due to off-heap allocation. This address points to the contiguous representation
	 * of the arraylet associated with this leaf. Only arraylets that have been off-heap
	 * allocated or double-mapped will contain such a contiguous address, otherwise the
	 * address will be NULL.
	 *
	 * @param env thread GC Environment
	 */
	void scanObjectsInVirtualLargeObjectHeap(MM_EnvironmentBase *env);
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

	virtual void doClassLoader(J9ClassLoader *classLoader);

	virtual void scanWeakReferenceObjects(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanWeakReferencesComplete(MM_EnvironmentBase *env);

	virtual void scanSoftReferenceObjects(MM_EnvironmentBase *env);
	virtual void scanPhantomReferenceObjects(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanSoftReferencesComplete(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanPhantomReferencesComplete(MM_EnvironmentBase *env);

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(J9Object *objectPtr) = 0;
	/**
	 * Handle an unfinalized object
	 * 
	 * @param objectPtr the unfinalized object
	 * @param list the list which this object belongs too.
	 * 
	 * @NOTE If a subclass does not override this function to provide an implementation it must override
	 * scanUnfinalizedObjects so that this function is not called.
	 */
	virtual void doUnfinalizedObject(J9Object *objectPtr, MM_UnfinalizedObjectList *list);
	virtual CompletePhaseCode scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * During clearabble processing, invoke specific processing/checks after last known phase that scans (and copies) objects.
	 */
	virtual void completedObjectScanPhasesCheckpoint() {}

	/**
	 * @todo Provide function documentation
	 */
	virtual void doOwnableSynchronizerObject(J9Object *objectPtr, MM_OwnableSynchronizerObjectList *list);
	virtual void doContinuationObject(J9Object *objectPtr, MM_ContinuationObjectList *list);
	
	/**
	 * @todo Provide function documentation
	 */	
	virtual CompletePhaseCode scanOwnableSynchronizerObjectsComplete(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanContinuationObjectsComplete(MM_EnvironmentBase *env);

	virtual void doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator);
	virtual void doMonitorLookupCacheSlot(j9objectmonitor_t* slotPtr);

	virtual void doJNIWeakGlobalReference(J9Object **slotPtr);
	virtual void doJNIGlobalReferenceSlot(J9Object **slotPtr, GC_JNIGlobalReferenceIterator *jniGlobalReferenceIterator);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	virtual void doRememberedSetSlot(J9Object **slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator);
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

#if defined(J9VM_OPT_JVMTI)
	virtual void doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator);
#endif /* J9VM_OPT_JVMTI */

	virtual void doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator);
	virtual void doStringCacheTableSlot(J9Object **slotPtr);
	virtual void doVMClassSlot(J9Class *classPtr);
	virtual void doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
	/**
	 * Frees the region used for off-heap allocation associated to the objectPtr (arraylet spine) if the objectPtr
	 * is not live.
	 *
	 * @param objectPtr[in] indexable object's spine
	 */
	virtual void doObjectInVirtualLargeObjectHeap(J9Object *objectPtr, bool *sparseHeapAllocation);
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
	
	/**
	 * Called for each object stack slot. Subclasses may override.
	 * 
	 * @param slotPtr[in/out] a pointer to the stack slot or a copy of the stack slot.
	 * @param walkState[in] the J9StackWalkState
	 * @param stackLocation[in] the actual pointer to the stack slot. Use only for reporting. 
	 */
	virtual void doStackSlot(J9Object **slotPtr, void *walkState, const void *stackLocation);
};

typedef struct StackIteratorData {
    MM_RootScanner *rootScanner;
    MM_EnvironmentBase *env;
} StackIteratorData;

#endif /* ROOTSCANNER_HPP_ */
