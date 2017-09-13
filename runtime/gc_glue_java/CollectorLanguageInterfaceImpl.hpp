
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef COLLECTORLANGUAGEINTERFACEJAVA_HPP_
#define COLLECTORLANGUAGEINTERFACEJAVA_HPP_

#include "j9.h"
#include "CollectorLanguageInterface.hpp"
#include "CompactScheme.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapWalker.hpp"
#include "MarkingScheme.hpp"
#include "ScanClassesMode.hpp"

class GC_ObjectScanner;
class GC_VMThreadIterator;
class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MemorySubSpaceSemiSpace;
class MM_Scavenger;
class MM_ParallelSweepScheme;

#if defined(J9VM_GC_MODRON_CONCURRENT_MARK) && !defined(OMR_GC_MODRON_CONCURRENT_MARK)
#error "J9VM_GC_MODRON_CONCURRENT_MARK in j9cfg.h requires OMR_GC_MODRON_CONCURRENT_MARK in omrcfg.h"
#endif /* defined(J9VM_GC_MODRON_CONCURRENT_MARK) && !defined(OMR_GC_MODRON_CONCURRENT_MARK) */

/**
 * Class representing a collector language interface.  This implements the API between the OMR
 * functionality and the language being implemented.
 * @ingroup GC_Base
 */
class MM_CollectorLanguageInterfaceImpl : public MM_CollectorLanguageInterface {
private:
protected:
	OMR_VM *_omrVM;
	J9JavaVM *_javaVM;
	MM_GCExtensions *_extensions;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_ScanClassesMode _concurrentGC_scanClassesMode; /** Support for dynamic class unloading in concurrent mark (ConcurrentGC) */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	volatile bool _scavenger_shouldScavengeFinalizableObjects; /**< Set to true at the beginning of a collection if there are any pending finalizable objects */
	volatile bool _scavenger_shouldScavengeUnfinalizedObjects; /**< Set to true at the beginning of a collection if there are any unfinalized objects */
	volatile bool _scavenger_shouldScavengeSoftReferenceObjects; /**< Set to true if there are any SoftReference objects discovered */
	volatile bool _scavenger_shouldScavengeWeakReferenceObjects; /**< Set to true if there are any WeakReference objects discovered */
	volatile bool _scavenger_shouldScavengePhantomReferenceObjects; /**< Set to true if there are any PhantomReference objects discovered */
#if defined(J9VM_GC_FINALIZATION)
	bool _scavenger_finalizationRequired; /**< Scavenger variable used to determine if finalization should be triggered */
#endif /* J9VM_GC_FINALIZATION */

public:
private:
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	MM_CollectorLanguageInterfaceImpl(J9JavaVM *vm)
		: MM_CollectorLanguageInterface()
		,_omrVM(vm->omrVM)
		,_javaVM(vm)
		,_extensions(MM_GCExtensions::getExtensions(vm))
		,_scavenger_shouldScavengeFinalizableObjects(false)
		,_scavenger_shouldScavengeUnfinalizedObjects(false)
		,_scavenger_shouldScavengeSoftReferenceObjects(false)
		,_scavenger_shouldScavengeWeakReferenceObjects(false)
		,_scavenger_shouldScavengePhantomReferenceObjects(false)
#if defined(J9VM_GC_FINALIZATION)
		,_scavenger_finalizationRequired(false)
#endif /* J9VM_GC_FINALIZATION */
	{
		_typeId = __FUNCTION__;
	}

public:
	static MM_CollectorLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	static MM_CollectorLanguageInterfaceImpl *getInterface(MM_CollectorLanguageInterface *cli) { return (MM_CollectorLanguageInterfaceImpl *)cli; }

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	/**
	 * Used to pass marking scheme and thread environment to stack slot iterator.
	 *
	 * @see concurrentGC_scanThread()
	 */
	typedef struct markSchemeStackIteratorData {
		MM_MarkingScheme *markingScheme;
		MM_EnvironmentBase *env;
	} markSchemeStackIteratorData;

	virtual void concurrentGC_processItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);
	virtual MM_ConcurrentSafepointCallback* concurrentGC_createSafepointCallback(MM_EnvironmentBase *env);
	virtual omrsig_handler_fn concurrentGC_getProtectedSignalHandler(void **signalHandlerArg);
	virtual void concurrentGC_concurrentInitializationComplete(MM_EnvironmentStandard *env);
	virtual bool concurrentGC_forceConcurrentKickoff(MM_EnvironmentBase *env, uintptr_t gcCode, uintptr_t *languagekickoffReason);
	virtual uintptr_t concurrentGC_getNextTracingMode(uintptr_t executionMode);
	virtual uintptr_t concurrentGC_collectRoots(MM_EnvironmentStandard *env, uintptr_t concurrentStatus, bool *collectedRoots, bool *paidTax);
	virtual void concurrentGC_signalThreadsToTraceStacks(MM_EnvironmentStandard *env);
	virtual void concurrentGC_signalThreadsToDirtyCards(MM_EnvironmentStandard *env);
	virtual void concurrentGC_signalThreadsToStopDirtyingCards(MM_EnvironmentStandard *env);
	virtual void concurrentGC_kickoffCardCleaning(MM_EnvironmentStandard *env);
	virtual void concurrentGC_flushRegionReferenceLists(MM_EnvironmentBase *env);
	virtual void concurrentGC_flushThreadReferenceBuffer(MM_EnvironmentBase *env);
	virtual bool concurrentGC_isThreadReferenceBufferEmpty(MM_EnvironmentBase *env);
	virtual bool concurrentGC_startConcurrentScanning(MM_EnvironmentStandard *env, uintptr_t *bytesTraced, bool *collectedRoots);
	virtual void concurrentGC_concurrentScanningStarted(MM_EnvironmentStandard *env, bool isConcurrentScanningComplete);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	virtual bool concurrentGC_isConcurrentScanningComplete(MM_EnvironmentBase *env) {return !_concurrentGC_scanClassesMode.isPendingOrActiveMode();}
	virtual uintptr_t concurrentGC_reportConcurrentScanningMode(MM_EnvironmentBase *env) {return _concurrentGC_scanClassesMode.getScanClassesMode();}
#else
	virtual bool concurrentGC_isConcurrentScanningComplete(MM_EnvironmentBase *env) {return true;}
	virtual uintptr_t concurrentGC_reportConcurrentScanningMode(MM_EnvironmentBase *env) {return 0;}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	virtual void concurrentGC_scanThread(MM_EnvironmentBase *env);
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#if defined(OMR_GC_MODRON_COMPACTION)
	virtual void compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env);
	virtual void compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme);
	virtual void compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env);
	virtual void compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap);
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_SCAVENGER)
	virtual void scavenger_masterSetupForGC(MM_EnvironmentBase *env);
	virtual void scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env);
	virtual void scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful);
	virtual void scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase);
	virtual void scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase);
	virtual void scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase);
	virtual bool scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, U_32 *gcCode);
	virtual GC_ObjectScanner *scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags);
	virtual void scavenger_flushReferenceObjects(MM_EnvironmentStandard *env);
	virtual bool scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	virtual bool scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	virtual void scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	virtual void scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env);
	virtual void scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject);
#if defined (J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
	virtual void scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew);
#endif /* J9VM_INTERP_COMPRESSED_OBJECT_HEADER */

	void scavenger_setShouldScavengeUnfinalizedObjects(bool shouldScavenge) { _scavenger_shouldScavengeUnfinalizedObjects = shouldScavenge; }

	volatile bool scavenger_getShouldScavengeFinalizableObjects() { return _scavenger_shouldScavengeFinalizableObjects; }
	volatile bool scavenger_getShouldScavengeUnfinalizedObjects() { return _scavenger_shouldScavengeUnfinalizedObjects; }
	volatile bool scavenger_getShouldScavengeSoftReferenceObjects() { return _scavenger_shouldScavengeSoftReferenceObjects; }
	volatile bool scavenger_getShouldScavengeWeakReferenceObjects() { return _scavenger_shouldScavengeWeakReferenceObjects; }
	volatile bool scavenger_getShouldScavengePhantomReferenceObjects() { return _scavenger_shouldScavengePhantomReferenceObjects; }
	
#if defined(J9VM_GC_FINALIZATION)
	void scavenger_setFinalizationRequired(bool req) { _scavenger_finalizationRequired = req; }
	bool scavenger_getFinalizationRequired(void) { return _scavenger_finalizationRequired; }
#endif /* J9VM_GC_FINALIZATION */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	virtual void scavenger_switchConcurrentForThread(MM_EnvironmentBase *env);
	virtual void scavenger_fixupIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#endif /* OMR_GC_MODRON_SCAVENGER */

protected:
private:

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	void private_concurrentGC_collectJNIRoots(MM_EnvironmentStandard *env, bool *completedJNIRoots);
	void private_concurrentGC_collectClassRoots(MM_EnvironmentStandard *env, bool *completedClassRoots, bool *classesMarkedAsRoots);
	void private_concurrentGC_collectFinalizableRoots(MM_EnvironmentStandard *env, bool *completedFinalizableRoots);
	void private_concurrentGC_collectStringRoots(MM_EnvironmentStandard *env, bool *completedStringRoots, bool *collectedStringConstants);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	uintptr_t private_concurrentGC_concurrentClassMark(MM_EnvironmentStandard *env, bool *completedClassMark);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	void private_concurrentGC_doVMThreadSlot(MM_EnvironmentBase *env, omrobjectptr_t *slotPtr, GC_VMThreadIterator *vmThreadIterator);
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	
	/*
	 * Compactor, Private
	 */

	void private_compactScheme_verifyHeapMixedObject(omrobjectptr_t objectPtr, MM_MarkMap *markMap);
	void private_compactScheme_verifyHeapObjectSlot(omrobjectptr_t object, MM_MarkMap *markMap);
	void private_compactScheme_verifyHeapArrayObject(omrobjectptr_t objectPtr, MM_MarkMap *markMap);

	void private_compactScheme_setupForOwnableSynchronizerProcessing(MM_EnvironmentBase *env);

	/*
	 * ParallelScavenger, Private
	 */

	void private_scavenger_addOwnableSynchronizerObjectInList(MM_EnvironmentStandard *env, omrobjectptr_t object);
	void private_scavenger_setupForOwnableSynchronizerProcessing(MM_EnvironmentStandard *env);

	/*
	 * Scavenger Collector, Private
	 */

	/**
	 * Decide if GC percolation should occur for class unloading.
	 * The class unloading occurs after a user defined number of cycles.
	 */
	bool private_scavenger_shouldPercolateGarbageCollect_classUnloading(MM_EnvironmentBase *envBase);

	/**
	 * Decide if GC percolation should occur due to active JNI critical
	 * regions.  Active regions require that objects do not move, which
	 * prevents scavenging. Percolate collect instead.
	 */
	bool private_scavenger_shouldPercolateGarbageCollect_activeJNICriticalRegions(MM_EnvironmentBase *envBase);
};

#endif /* COLLECTORLANGUAGEINTERFACEJAVA_HPP_ */
