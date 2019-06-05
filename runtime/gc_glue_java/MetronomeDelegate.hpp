/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#if !defined(METRONOMEDELEGATE_HPP_)
#define METRONOMEDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_GC_REALTIME)

#include "BaseNonVirtual.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "RealtimeAccessBarrier.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "Scheduler.hpp"

class MM_HeapRegionDescriptorRealtime;
class MM_RealtimeMarkingSchemeRootMarker;
class MM_RealtimeRootScanner;
class MM_Scheduler;

class MM_MetronomeDelegate : public MM_BaseNonVirtual
{
private:
	MM_GCExtensions *_extensions;
	MM_RealtimeGC *_realtimeGC;
	J9JavaVM *_javaVM;
	MM_Scheduler *_scheduler;
	MM_RealtimeMarkingScheme *_markingScheme;
	UDATA _vmResponsesRequiredForExclusiveVMAccess;  /**< Used to support the (request/wait)ExclusiveVMAccess methods. */
	UDATA _jniResponsesRequiredForExclusiveVMAccess;  /**< Used to support the (request/wait)ExclusiveVMAccess methods. */

public:
	void yieldWhenRequested(MM_EnvironmentBase *env);
	static int J9THREAD_PROC metronomeAlarmThreadWrapper(void* userData);
	static uintptr_t signalProtectedFunction(J9PortLibrary *privatePortLibrary, void* userData);	

	MM_MetronomeDelegate(MM_EnvironmentBase *env) :
		_extensions(MM_GCExtensions::getExtensions(env)),
		_realtimeGC(NULL),
		_javaVM((J9JavaVM*)env->getOmrVM()->_language_vm) {}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
 	bool _unmarkedImpliesClasses; /**< if true the mark bit can be used to check is class alive or not */
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	bool _unmarkedImpliesCleared;
	bool _unmarkedImpliesStringsCleared; /**< If true, we can assume that unmarked strings in the string table will be cleared */
	
#if defined(J9VM_GC_FINALIZATION)	
	bool _finalizationRequired;
#endif /* J9VM_GC_FINALIZATION */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool _dynamicClassUnloadingEnabled;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	bool allocateAndInitializeReferenceObjectLists(MM_EnvironmentBase *env);
	bool allocateAndInitializeUnfinalizedObjectLists(MM_EnvironmentBase *env);
	bool allocateAndInitializeOwnableSynchronizerObjectLists(MM_EnvironmentBase *env);

#if defined(J9VM_GC_FINALIZATION)	
	bool isFinalizationRequired() { return _finalizationRequired; }
#endif /* J9VM_GC_FINALIZATION */

	void masterSetupForGC(MM_EnvironmentBase *env);
	void masterCleanupAfterGC(MM_EnvironmentBase *env);
	void incrementalCollectStart(MM_EnvironmentRealtime *env);
	void incrementalCollect(MM_EnvironmentRealtime *env);
	void doAuxiliaryGCWork(MM_EnvironmentBase *env);
	void clearGCStats();
	void clearGCStatsEnvironment(MM_EnvironmentRealtime *env);
	void mergeGCStats(MM_EnvironmentRealtime *env);
	uintptr_t getSplitArraysProcessed(MM_EnvironmentRealtime *env);
	void reportSyncGCEnd(MM_EnvironmentBase *env);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MMINLINE bool isDynamicClassUnloadingEnabled() { return _dynamicClassUnloadingEnabled; };
	void unloadDeadClassLoaders(MM_EnvironmentBase *env);

	void reportClassUnloadingStart(MM_EnvironmentBase *env);
	void reportClassUnloadingEnd(MM_EnvironmentBase *env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Perform initial cleanup for classloader unloading.  The current thread has exclusive access.
	 * The J9AccClassDying bit is set and J9HOOK_VM_CLASS_UNLOAD is triggered for each class that will be unloaded.
	 * The J9_GC_CLASS_LOADER_DEAD bit is set for each class loader that will be unloaded.
	 * J9HOOK_VM_CLASSES_UNLOAD is triggered if any classes will be unloaded.
	 *
	 * @param env[in] the master GC thread
	 * @param classUnloadCountResult[out] returns the number of classes about to be unloaded
	 * @param anonymousClassUnloadCount[out] returns the number of anonymous classes about to be unloaded
	 * @param classLoaderUnloadCountResult[out] returns the number of class loaders about to be unloaded
	 * @param classLoaderUnloadListResult[out] returns a linked list of class loaders about to be unloaded
	 */
	void processDyingClasses(MM_EnvironmentRealtime *env, UDATA* classUnloadCountResult, UDATA* anonymousClassUnloadCount, UDATA* classLoaderUnloadCountResult, J9ClassLoader** classLoaderUnloadListResult);
	void processUnlinkedClassLoaders(MM_EnvironmentBase *env, J9ClassLoader *deadClassLoaders);
	void updateClassUnloadStats(MM_EnvironmentBase *env, UDATA classUnloadCount, UDATA anonymousClassUnloadCount, UDATA classLoaderUnloadCount);

	/**
	 * Scan classloader for dying classes and add them to the list
	 * @param env[in] the current thread
	 * @param classLoader[in] the list of class loaders to clean up
	 * @param setAll[in] bool if true if all classes must be set dying, if false unmarked classes only
	 * @param classUnloadListStart[in] root of list dying classes should be added to
	 * @param classUnloadCountOut[out] number of classes dying added to the list
	 * @return new root to list of dying classes
	 */
	J9Class *addDyingClassesToList(MM_EnvironmentRealtime *env, J9ClassLoader * classLoader, bool setAll, J9Class *classUnloadListStart, UDATA *classUnloadCountResult);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	void yieldFromClassUnloading(MM_EnvironmentRealtime *env);
	void lockClassUnloadMonitor(MM_EnvironmentRealtime *env);
	void unlockClassUnloadMonitor(MM_EnvironmentRealtime *env);

	UDATA getUnfinalizedObjectListCount(MM_EnvironmentBase *env) { return _extensions->gcThreadCount; }
	UDATA getOwnableSynchronizerObjectListCount(MM_EnvironmentBase *env) { return _extensions->gcThreadCount; }
	UDATA getReferenceObjectListCount(MM_EnvironmentBase *env) { return _extensions->gcThreadCount; }

	void defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace);
	MM_RealtimeAccessBarrier* allocateAccessBarrier(MM_EnvironmentBase *env);
	void enableDoubleBarrier(MM_EnvironmentBase* env);
	void disableDoubleBarrierOnThread(MM_EnvironmentBase* env, OMR_VMThread* vmThread);
	void disableDoubleBarrier(MM_EnvironmentBase* env);

	/* New methods */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool doClassTracing(MM_EnvironmentRealtime* env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	bool doTracing(MM_EnvironmentRealtime* env);

	/*
	 * These functions are used by classic Metronome gang-scheduling and
	 * also transitionally used for the parts of realtime that have not
	 * been made concurrent.
	 */
	void preRequestExclusiveVMAccess(OMR_VMThread *threadRequestingExclusive);
	void postRequestExclusiveVMAccess(OMR_VMThread *threadRequestingExclusive);
	uintptr_t requestExclusiveVMAccess(MM_EnvironmentBase *env, uintptr_t block, uintptr_t *gcPriority);
	void waitForExclusiveVMAccess(MM_EnvironmentBase *env, bool waitRequired);
	void acquireExclusiveVMAccess(MM_EnvironmentBase *env, bool waitRequired);
	void releaseExclusiveVMAccess(MM_EnvironmentBase *env, bool releaseRequired);

	void markLiveObjectsRoots(MM_EnvironmentRealtime *env);
	void markLiveObjectsScan(MM_EnvironmentRealtime *env);
	void markLiveObjectsComplete(MM_EnvironmentRealtime *env);
	void checkReferenceBuffer(MM_EnvironmentRealtime *env);
	void setUnmarkedImpliesCleared();
	void unsetUnmarkedImpliesCleared();
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MMINLINE void
	markClassOfObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
	{
		markClassNoCheck(env, J9GC_J9OBJECT_CLAZZ(objectPtr));
	}

	MMINLINE bool
	markClassNoCheck(MM_EnvironmentRealtime *env, J9Class *clazz)
	{
		bool result = false;
		if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassIsAnonymous)) {
			/*
			 * If class is anonymous it's classloader will not be rescanned
			 * so class object should be marked directly
			 */
			result = _markingScheme->markObject(env, clazz->classObject);
		} else {
			result = _markingScheme->markObject(env, clazz->classLoader->classLoaderObject);
		}
		return result;
	}
	bool markClass(MM_EnvironmentRealtime *env, J9Class *objectPtr);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	MMINLINE uintptr_t
	scanPointerArraylet(MM_EnvironmentRealtime *env, fomrobject_t *arraylet)
	{
		fomrobject_t *startScanPtr = arraylet;
		fomrobject_t *endScanPtr = startScanPtr + (_javaVM->arrayletLeafSize / sizeof(fj9object_t));
		return scanPointerRange(env, startScanPtr, endScanPtr);
	}

	MMINLINE uintptr_t
	scanObject(MM_EnvironmentRealtime *env, omrobjectptr_t objectPtr)
	{
		UDATA pointersScanned = 0;
		switch(_extensions->objectModel.getScanType(objectPtr)) {
		case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
		case GC_ObjectModel::SCAN_MIXED_OBJECT:
		case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		case GC_ObjectModel::SCAN_CLASS_OBJECT:
		case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
			pointersScanned = scanMixedObject(env, objectPtr);
			break;
		case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
			pointersScanned = scanPointerArrayObject(env, (J9IndexableObject *)objectPtr);
			break;
		case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
			pointersScanned = scanReferenceMixedObject(env, objectPtr);
			break;
		case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		   pointersScanned = 0;
		   break;
		default:
			Assert_MM_unreachable();
		}
		
		return pointersScanned;
	}

	MMINLINE UDATA
	scanPointerRange(MM_EnvironmentRealtime *env, fj9object_t *startScanPtr, fj9object_t *endScanPtr)
	{
		fj9object_t *scanPtr = startScanPtr;
		UDATA pointerFieldBytes = (UDATA)(endScanPtr - scanPtr);
		UDATA pointerField = pointerFieldBytes / sizeof(fj9object_t);
		while(scanPtr < endScanPtr) {
			GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
			_markingScheme->markObject(env, slotObject.readReferenceFromSlot());
			scanPtr++;
		}

		env->addScannedBytes(pointerFieldBytes);
		env->addScannedPointerFields(pointerField);

		return pointerField;
	}

	MMINLINE UDATA
	scanMixedObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
	{
		/* Object slots */

		fj9object_t *scanPtr = _extensions->mixedObjectModel.getHeadlessObject(objectPtr);
		UDATA objectSize = _extensions->mixedObjectModel.getSizeInBytesWithHeader(objectPtr);
		fj9object_t *endScanPtr = (fj9object_t*)(((U_8 *)objectPtr) + objectSize);
		UDATA *descriptionPtr;
		UDATA descriptionBits;
		UDATA descriptionIndex;
#if defined(J9VM_GC_LEAF_BITS)
		UDATA *leafPtr;
		UDATA leafBits;
#endif /* J9VM_GC_LEAF_BITS */
		
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if(isDynamicClassUnloadingEnabled()) {
			markClassOfObject(env, objectPtr);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		descriptionPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
		leafPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceLeafDescription;
#endif /* J9VM_GC_LEAF_BITS */

		if(((UDATA)descriptionPtr) & 1) {
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

		UDATA pointerFields = 0;
		while(scanPtr < endScanPtr) {
			/* Determine if the slot should be processed */
			if(descriptionBits & 1) {
				pointerFields++;
				GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
#if defined(J9VM_GC_LEAF_BITS)
				_markingScheme->markObject(env, slotObject.readReferenceFromSlot(), 1 == (leafBits & 1));
#else /* J9VM_GC_LEAF_BITS */
				_markingScheme->markObject(env, slotObject.readReferenceFromSlot(), false);
#endif /* J9VM_GC_LEAF_BITS */
			}
			descriptionBits >>= 1;
#if defined(J9VM_GC_LEAF_BITS)
			leafBits >>= 1;
#endif /* J9VM_GC_LEAF_BITS */
			if(descriptionIndex-- == 0) {
				descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
				leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
				descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
			}
			scanPtr += 1;
		}

		env->addScannedBytes(objectSize);
		env->addScannedPointerFields(pointerFields);
		env->incScannedObjects();

		return pointerFields;
	}

	MMINLINE UDATA
	scanPointerArrayObject(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr)
	{
		UDATA pointerFields = 0;
		
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if(isDynamicClassUnloadingEnabled()) {
			markClassOfObject(env, (J9Object *)objectPtr);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		bool isContiguous = _extensions->indexableObjectModel.isInlineContiguousArraylet(objectPtr);

		/* Very small arrays cannot be set as scanned (no scanned bit in Mark Map reserved for them) */
		bool canSetAsScanned = (isContiguous
				&& (_extensions->minArraySizeToSetAsScanned <= _extensions->indexableObjectModel.arrayletSize(objectPtr, 0)));

		if (canSetAsScanned && _markingScheme->isScanned((J9Object *)objectPtr)) {
			/* Already scanned by ref array copy optimization */
			return pointerFields;
		}

#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		/* if NUA is enabled, separate path for contiguous arrays */
		UDATA sizeInElements = _extensions->indexableObjectModel.getSizeInElements(objectPtr);
		if (isContiguous || (0 == sizeInElements)) {
			fj9object_t *startScanPtr = (fj9object_t *)_extensions->indexableObjectModel.getDataPointerForContiguous(objectPtr);
			fj9object_t *endScanPtr = startScanPtr + sizeInElements;
			pointerFields += scanPointerRange(env, startScanPtr, endScanPtr);
		} else {
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
			fj9object_t *arrayoid = _extensions->indexableObjectModel.getArrayoidPointer(objectPtr);
			UDATA numArraylets = _extensions->indexableObjectModel.numArraylets(objectPtr);
			for (UDATA i=0; i<numArraylets; i++) {
				UDATA arrayletSize = _extensions->indexableObjectModel.arrayletSize(objectPtr, i);
				/* need to check leaf pointer because this can be a partially allocated arraylet (not all leafs are allocated) */
				GC_SlotObject slotObject(_javaVM->omrVM, &arrayoid[i]);
				fj9object_t *startScanPtr = (fj9object_t*) (slotObject.readReferenceFromSlot());
				if (NULL != startScanPtr) {
					fj9object_t *endScanPtr = startScanPtr + arrayletSize / sizeof(fj9object_t);
					if (i == (numArraylets - 1)) {
						pointerFields += scanPointerRange(env, startScanPtr, endScanPtr);
						if (canSetAsScanned) {
							_markingScheme->setScanAtomic((J9Object *)objectPtr);
						}
					} else {
						env->getWorkStack()->push(env, (void *)ARRAYLET_TO_ITEM(startScanPtr));
					}
				}
			}
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		}
#endif /* J9VM_GC_HYBRID_ARRAYLETS */

		/* check for yield if we've actually scanned a leaf */
		if (0 != pointerFields) {
			_scheduler->condYieldFromGC(env);
		}

		env->incScannedObjects();

		return pointerFields;
	}

	MMINLINE UDATA
	scanReferenceMixedObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
	{
		fj9object_t *scanPtr = _extensions->mixedObjectModel.getHeadlessObject(objectPtr);
		UDATA objectSize = _extensions->mixedObjectModel.getSizeInBytesWithHeader(objectPtr);
		fj9object_t *endScanPtr = (fj9object_t*)(((U_8 *)objectPtr) + objectSize);
		UDATA *descriptionPtr;
		UDATA descriptionBits;
		UDATA descriptionIndex;
#if defined(J9VM_GC_LEAF_BITS)
		UDATA *leafPtr;
		UDATA leafBits;
#endif /* J9VM_GC_LEAF_BITS */
		
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if(isDynamicClassUnloadingEnabled()) {
			markClassOfObject(env, objectPtr);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		
		descriptionPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
		leafPtr = (UDATA *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceLeafDescription;
#endif /* J9VM_GC_LEAF_BITS */

		if(((UDATA)descriptionPtr) & 1) {
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

		I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr);
		UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(objectPtr)) & J9AccClassReferenceMask;
		UDATA referenceObjectOptions = env->_cycleState->_referenceObjectOptions;
		bool isReferenceCleared = (GC_ObjectModel::REF_STATE_CLEARED == referenceState) || (GC_ObjectModel::REF_STATE_ENQUEUED == referenceState);
		bool referentMustBeMarked = isReferenceCleared;
		bool referentMustBeCleared = false;

		switch (referenceObjectType) {
		case J9AccClassReferenceWeak:
			referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_weak));
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

		GC_SlotObject referentPtr(_javaVM->omrVM, &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr));

		if (referentMustBeCleared) {
			/* Discovering this object at this stage in the GC indicates that it is being resurrected. Clear its referent slot. */
			referentPtr.writeReferenceToSlot(NULL);
			/* record that the reference has been cleared if it's not already in the cleared or enqueued state */
			if (!isReferenceCleared) {
				J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr) = GC_ObjectModel::REF_STATE_CLEARED;
			}
		} else {
			/* we don't need to process cleared or enqueued references */
			if (!isReferenceCleared) {
				env->getGCEnvironment()->_referenceObjectBuffer->add(env, objectPtr);
			}
		}
		
		UDATA pointerFields = 0;
		while(scanPtr < endScanPtr) {
			/* Determine if the slot should be processed */
			if((descriptionBits & 1) && ((scanPtr != referentPtr.readAddressFromSlot()) || referentMustBeMarked)) {
				pointerFields++;
				GC_SlotObject slotObject(_javaVM->omrVM, scanPtr);
#if defined(J9VM_GC_LEAF_BITS)
				_markingScheme->markObject(env, slotObject.readReferenceFromSlot(), 1 == (leafBits & 1));
#else /* J9VM_GC_LEAF_BITS */
				_markingScheme->markObject(env, slotObject.readReferenceFromSlot(), false);
#endif /* J9VM_GC_LEAF_BITS */
			}
			descriptionBits >>= 1;
#if defined(J9VM_GC_LEAF_BITS)
			leafBits >>= 1;
#endif /* J9VM_GC_LEAF_BITS */
			if(descriptionIndex-- == 0) {
				descriptionBits = *descriptionPtr++;
#if defined(J9VM_GC_LEAF_BITS)
				leafBits = *leafPtr++;
#endif /* J9VM_GC_LEAF_BITS */
				descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
			}
			scanPtr++;
		}
		
		env->addScannedBytes(objectSize);
		env->addScannedPointerFields(pointerFields);
		env->incScannedObjects();
		
		return pointerFields;
	}

#if defined(J9VM_GC_FINALIZATION)
	void scanUnfinalizedObjects(MM_EnvironmentRealtime *env);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * Wraps the MM_RootScanner::scanOwnableSynchronizerObjects method to disable yielding during the scan
	 * then yield after scanning.
	 * @see MM_RootScanner::scanOwnableSynchronizerObjects()
	 */
	void scanOwnableSynchronizerObjects(MM_EnvironmentRealtime *env);

private:
	/**
	 * Called by the root scanner to scan all WeakReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanWeakReferenceObjects(MM_EnvironmentRealtime *env);
	/**
	 * Process the list of reference objects recorded in the specified list.
	 * References with unmarked referents are cleared and optionally enqueued.
	 * SoftReferences have their ages incremented.
	 * @param env[in] the current thread
	 * @param region[in] the region all the objects in the list belong to
	 * @param headOfList[in] the first object in the linked list
	 */
	void processReferenceList(MM_EnvironmentRealtime *env, MM_HeapRegionDescriptorRealtime* region, J9Object* headOfList, MM_ReferenceStats *referenceStats);

	/**
	 * Called by the root scanner to scan all SoftReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanSoftReferenceObjects(MM_EnvironmentRealtime *env);

	/**
	 * Called by the root scanner to scan all PhantomReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanPhantomReferenceObjects(MM_EnvironmentRealtime *env);

	/*
	 * Friends
	 */
	friend class MM_RealtimeGC;
	friend class MM_RealtimeMarkingSchemeRootClearer;
};

#endif /* defined(J9VM_GC_REALTIME) */

#endif /* defined(METRONOMEDELEGATE_HPP_) */

