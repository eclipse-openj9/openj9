
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

#include "CollectorLanguageInterfaceImpl.hpp"

#include "j9nongenerated.h"
#include "mmhook.h"
#include "mmomrhook_internal.h"
#include "mmprivatehook.h"
#include "mmprivatehook_internal.h"

#include "ArrayObjectModel.hpp"
#include "AsyncCallbackHandler.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassModel.hpp"
#include "Collector.hpp"
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactScheme.hpp"
#include "CompactSchemeCheckMarkRoots.hpp"
#include "CompactSchemeFixupRoots.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentSafepointCallbackJava.hpp"
#include "ConcurrentGC.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#if defined(J9VM_GC_CONCURRENT_SWEEP)
#include "ConcurrentSweepScheme.hpp"
#endif /* J9VM_GC_CONCURRENT_SWEEP */
#include "ConfigurationDelegate.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentStandard.hpp"
#include "ExcessiveGCStats.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "FinalizableReferenceBuffer.hpp"
#include "FinalizerSupport.hpp"
#include "ForwardedHeader.hpp"
#include "FrequentObjectsStats.hpp"
#include "GCExtensions.hpp"
#if defined(J9VM_PROF_EVENT_REPORTING)
#include "GCObjectEvents.hpp"
#endif /* J9VM_PROF_EVENT_REPORTING */
#include "GlobalGCStats.hpp"
#include "GlobalVLHGCStats.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "HeapWalker.hpp"
#include "MarkingScheme.hpp"
#include "MarkingSchemeRootMarker.hpp"
#include "MarkingSchemeRootClearer.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "MixedObjectModel.hpp"
#include "MixedObjectScanner.hpp"
#include "ModronAssertions.h"
#include "ObjectAccessBarrier.hpp"
#include "ObjectAllocationInterface.hpp"
#include "OMRVMInterface.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "ParallelGlobalGC.hpp"
#include "ParallelHeapWalker.hpp"
#include "ParallelSweepScheme.hpp"
#include "PointerArrayObjectScanner.hpp"
#include "ReferenceChainWalkerMarkMap.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "ReferenceObjectList.hpp"
#include "ReferenceObjectScanner.hpp"
#include "ScanClassesMode.hpp"
#include "Scavenger.hpp"
#include "ScavengerStats.hpp"
#include "ScavengerBackOutScanner.hpp"
#include "SlotObject.hpp"
#include "StandardAccessBarrier.hpp"
#include "SublistFragment.hpp"
#include "StringTable.hpp"
#include "Task.hpp"
#include "UnfinalizedObjectBuffer.hpp"
#include "WorkPacketsConcurrent.hpp"
#include "StackSlotValidator.hpp"
#include "VMInterface.hpp"
#include "VMThreadIterator.hpp"
#include "VMThreadListIterator.hpp"
#include "VMThreadStackSlotIterator.hpp"

class MM_AllocationContext;

/**
 * Initialization
 */
MM_CollectorLanguageInterfaceImpl *
MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterfaceImpl *cliJava = NULL;

	cliJava = (MM_CollectorLanguageInterfaceImpl *)env->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != cliJava) {
		new(cliJava) MM_CollectorLanguageInterfaceImpl((J9JavaVM*)env->getLanguageVM());
		if (!cliJava->initialize(env)) {
			cliJava->kill(env);
			cliJava = NULL;
		}
	}

	return cliJava;
}

void
MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_CollectorLanguageInterfaceImpl::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_CollectorLanguageInterfaceImpl::tearDown(MM_EnvironmentBase *env)
{
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_masterSetupForGC(MM_EnvironmentBase * envBase)
{
	/* Remember the candidates of OwnableSynchronizerObject before
	 * clearing scavenger statistics
	 */
	/* = survived ownableSynchronizerObjects in nursery space during previous scavenger
	 * + the new OwnableSynchronizerObject allocations
	 */
	UDATA ownableSynchronizerCandidates = 0;
	ownableSynchronizerCandidates += _extensions->scavengerJavaStats._ownableSynchronizerNurserySurvived;
	ownableSynchronizerCandidates += _extensions->allocationStats._ownableSynchronizerObjectCount;

	/* Clear the global java-only gc statistics */
	_extensions->scavengerJavaStats.clear();

	/* set the candidates of ownableSynchronizerObject for gc verbose report */
	_extensions->scavengerJavaStats._ownableSynchronizerCandidates = ownableSynchronizerCandidates;

	/* correspondent lists will be build this scavenge */
	_scavenger_shouldScavengeSoftReferenceObjects = false;
	_scavenger_shouldScavengeWeakReferenceObjects = false;
	_scavenger_shouldScavengePhantomReferenceObjects = false;

	/* Record whether finalizable processing is required in this scavenge */
	_scavenger_shouldScavengeFinalizableObjects = _extensions->finalizeListManager->isFinalizableObjectProcessingRequired();
	_scavenger_shouldScavengeUnfinalizedObjects = false;

	private_scavenger_setupForOwnableSynchronizerProcessing(MM_EnvironmentStandard::getEnvironment(envBase));

	return;
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *envBase)
{
	/* clear thread-local java-only gc stats */
	envBase->getGCEnvironment()->_scavengerJavaStats.clear();
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful)
{
	Assert_GC_true_with_message2(envBase, _extensions->scavengerJavaStats._ownableSynchronizerCandidates >= _extensions->scavengerJavaStats._ownableSynchronizerTotalSurvived,
			"[MM_CollectorLanguageInterfaceImpl::scavenger_reportScavengeEnd]: _extensions->scavengerJavaStats: _ownableSynchronizerCandidates=%zu < _ownableSynchronizerTotalSurvived=%zu\n", _extensions->scavengerJavaStats._ownableSynchronizerCandidates, _extensions->scavengerJavaStats._ownableSynchronizerTotalSurvived);

	if (!scavengeSuccessful) {
		/* for backout case, the ownableSynchronizerObject lists is restored before scavenge, so all of candidates are survived */
		_extensions->scavengerJavaStats._ownableSynchronizerTotalSurvived = _extensions->scavengerJavaStats._ownableSynchronizerCandidates;

		_extensions->scavengerJavaStats._ownableSynchronizerNurserySurvived = _extensions->scavengerJavaStats._ownableSynchronizerCandidates;
	}
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase * envBase)
{
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);

	MM_ScavengerJavaStats *finalGCJavaStats = &_extensions->scavengerJavaStats;
	MM_ScavengerJavaStats *scavJavaStats = &env->getGCEnvironment()->_scavengerJavaStats;

	finalGCJavaStats->_unfinalizedCandidates += scavJavaStats->_unfinalizedCandidates;
	finalGCJavaStats->_unfinalizedEnqueued += scavJavaStats->_unfinalizedEnqueued;

	finalGCJavaStats->_ownableSynchronizerCandidates += scavJavaStats->_ownableSynchronizerCandidates;
	finalGCJavaStats->_ownableSynchronizerTotalSurvived += scavJavaStats->_ownableSynchronizerTotalSurvived;
	finalGCJavaStats->_ownableSynchronizerNurserySurvived += scavJavaStats->_ownableSynchronizerNurserySurvived;

	finalGCJavaStats->_weakReferenceStats.merge(&scavJavaStats->_weakReferenceStats);
	finalGCJavaStats->_softReferenceStats.merge(&scavJavaStats->_softReferenceStats);
	finalGCJavaStats->_phantomReferenceStats.merge(&scavJavaStats->_phantomReferenceStats);
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase * envBase)
{
#if defined(J9VM_GC_FINALIZATION)
	/* Alert the finalizer if work needs to be done */
	if(this->scavenger_getFinalizationRequired()) {
		omrthread_monitor_enter(_javaVM->finalizeMasterMonitor);
		_javaVM->finalizeMasterFlags |= J9_FINALIZE_FLAGS_MASTER_WAKE_UP;
		omrthread_monitor_notify_all(_javaVM->finalizeMasterMonitor);
		omrthread_monitor_exit(_javaVM->finalizeMasterMonitor);
	}
#endif
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase)
{
	/* Once a scavenge has been completed successfully ensure that
	 * identity hash salt for the nursery gets updated
	 */
	/* Temporarily disabling the update of salt for Concurrent Scavenger. Since salt is changed at the end of a GC cycle,
	 * object that are allocated and hashed during a cycle would have different hash value before and after the cycle end.
	 */
	if (!_extensions->isConcurrentScavengerEnabled()) {
		_extensions->updateIdentityHashDataForSaltIndex(J9GC_HASH_SALT_NURSERY_INDEX);
	}
}

/**
 * Java CollectorLanguageInterface will require a global gc if:
 *   1. There are classes that should be dynamically unloaded. Classes can only be collected by the globalGC.
 *   2. JNI critical sections require that objects cannot be moved.
 */
bool
MM_CollectorLanguageInterfaceImpl::scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason * percolateReason, U_32 * percolateType)
{
	bool shouldPercolate = false;

	if (private_scavenger_shouldPercolateGarbageCollect_classUnloading(envBase)) {
		*percolateReason = UNLOADING_CLASSES;
		*percolateType = J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES;
		shouldPercolate = true;
	} else if (private_scavenger_shouldPercolateGarbageCollect_activeJNICriticalRegions(envBase)) {
		Trc_MM_Scavenger_percolate_activeJNICritical(envBase->getLanguageVMThread());
		*percolateReason = CRITICAL_REGIONS;
		*percolateType = J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS;
		shouldPercolate = true;
	}

	return shouldPercolate;
}

GC_ObjectScanner *
MM_CollectorLanguageInterfaceImpl::scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
{
#if defined(OMR_GC_MODRON_SCAVENGER_STRICT)
	Assert_MM_true((GC_ObjectScanner::scanHeap == flags) ^ (GC_ObjectScanner::scanRoots == flags));
#endif /* defined(OMR_GC_MODRON_SCAVENGER_STRICT) */

	GC_ObjectScanner *objectScanner = NULL;
	switch(_extensions->objectModel.getScanType(objectPtr)) {
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, allocSpace, flags);
		break;
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		if (GC_ObjectScanner::isHeapScan(flags)) {
			I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr);
			bool isReferenceCleared = (GC_ObjectModel::REF_STATE_CLEARED == referenceState) || (GC_ObjectModel::REF_STATE_ENQUEUED == referenceState);
			bool isObjectInNewSpace = _extensions->scavenger->isObjectInNewSpace(objectPtr);
			bool shouldScavengeReferenceObject = isObjectInNewSpace && !isReferenceCleared;
			bool referentMustBeMarked = isReferenceCleared || !isObjectInNewSpace;
			bool referentMustBeCleared = false;

			J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(objectPtr);
			UDATA referenceObjectOptions = env->_cycleState->_referenceObjectOptions;
			UDATA referenceObjectType = J9CLASS_FLAGS(clazzPtr) & J9_JAVA_CLASS_REFERENCE_MASK;
			switch (referenceObjectType) {
			case J9_JAVA_CLASS_REFERENCE_WEAK:
				referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_weak));
				if (!referentMustBeCleared && shouldScavengeReferenceObject && !_scavenger_shouldScavengeWeakReferenceObjects) {
					_scavenger_shouldScavengeWeakReferenceObjects = true;
				}
				break;
			case J9_JAVA_CLASS_REFERENCE_SOFT:
				referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_soft));
				referentMustBeMarked = referentMustBeMarked || ((0 == (referenceObjectOptions & MM_CycleState::references_soft_as_weak))
					&& ((UDATA)J9GC_J9VMJAVALANGSOFTREFERENCE_AGE(env, objectPtr) < _extensions->getDynamicMaxSoftReferenceAge())
				);
				if (!referentMustBeCleared && shouldScavengeReferenceObject && !_scavenger_shouldScavengeSoftReferenceObjects) {
					_scavenger_shouldScavengeSoftReferenceObjects = true;
				}
				break;
			case J9_JAVA_CLASS_REFERENCE_PHANTOM:
				referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_phantom));
				if (!referentMustBeCleared && shouldScavengeReferenceObject && !_scavenger_shouldScavengePhantomReferenceObjects) {
					_scavenger_shouldScavengePhantomReferenceObjects = true;
				}
				break;
			default:
				Assert_MM_unreachable();
			}

			GC_SlotObject referentPtr(env->getOmrVM(), &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr));
			if (referentMustBeCleared) {
				/* Discovering this object at this stage in the GC indicates that it is being resurrected. Clear its referent slot. */
				referentPtr.writeReferenceToSlot(NULL);
				/* record that the reference has been cleared if it's not already in the cleared or enqueued state */
				if (!isReferenceCleared) {
					J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr) = GC_ObjectModel::REF_STATE_CLEARED;
				}
			} else if (shouldScavengeReferenceObject) {
				env->getGCEnvironment()->_referenceObjectBuffer->add(env, objectPtr);
			}

			fomrobject_t *referentSlotAddress = referentMustBeMarked ? NULL : referentPtr.readAddressFromSlot();
			objectScanner = GC_ReferenceObjectScanner::newInstance(env, objectPtr, referentSlotAddress, allocSpace, flags);
		} else {
			objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, allocSpace, flags);
		}
		break;
	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		if (!_extensions->isConcurrentScavengerEnabled()) {
			if (GC_ObjectScanner::isHeapScan(flags)) {
				private_scavenger_addOwnableSynchronizerObjectInList(env, objectPtr);
			}
		}
		objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, allocSpace, flags);
		break;
	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		{
			uintptr_t splitAmount = 0;
			if (!GC_ObjectScanner::isIndexableObjectNoSplit(flags)) {
				splitAmount = _extensions->scavenger->getArraySplitAmount(env, _extensions->indexableObjectModel.getSizeInElements((omrarrayptr_t)objectPtr));
			}
			objectScanner = GC_PointerArrayObjectScanner::newInstance(env, objectPtr, allocSpace, flags, splitAmount);
		}
		break;
	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		break;
	default:
		Assert_GC_true_with_message(env, false, "Bad scan type for object pointer %p\n", objectPtr);
	}

	return objectScanner;
}

void MM_CollectorLanguageInterfaceImpl::scavenger_flushReferenceObjects(MM_EnvironmentStandard *env)
{
	env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	bool shouldBeRemembered = false;

	J9Class *classPtr = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), objectPtr);
	Assert_MM_true(NULL != classPtr);
	J9Class *classToScan = classPtr;
	do {
		volatile omrobjectptr_t *slotPtr = NULL;
		GC_ClassStaticsIterator classStaticsIterator(env, classToScan);
		while((slotPtr = classStaticsIterator.nextSlot()) != NULL) {
			shouldBeRemembered = _extensions->scavenger->copyObjectSlot(env, slotPtr) || shouldBeRemembered;
		}
		shouldBeRemembered = _extensions->scavenger->copyObjectSlot(env, (omrobjectptr_t *)&(classToScan->classObject)) || shouldBeRemembered;

		classToScan = classToScan->replacedClass;
	} while (NULL != classToScan);

	return shouldBeRemembered;
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	J9Class *classToScan = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), objectPtr);
	Assert_MM_true(NULL != classToScan);

	/* Check if Class Object should be remembered */
	omrobjectptr_t classObjectPtr = (omrobjectptr_t)classToScan->classObject;
	if (_extensions->scavenger->isObjectInNewSpace(classObjectPtr)) {
		 Assert_MM_false(_extensions->scavenger->isObjectInEvacuateMemory(classObjectPtr));
		 return true;
	}

	/* Iterate though Class Statics */
	do {
		omrobjectptr_t *slotPtr = NULL;
		GC_ClassStaticsIterator classStaticsIterator(env, classToScan);
		while(NULL != (slotPtr = (omrobjectptr_t*)classStaticsIterator.nextSlot())) {
			omrobjectptr_t objectPtr = *slotPtr;
			if (NULL != objectPtr){
				if (_extensions->scavenger->isObjectInNewSpace(objectPtr)){
					Assert_MM_false(_extensions->scavenger->isObjectInEvacuateMemory(objectPtr));
					return true;
				}
			}
		}
		classToScan = classToScan->replacedClass;
	} while (NULL != classToScan);

	return false;
}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_fixupIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), objectPtr);
	Assert_MM_true(NULL != clazz);
	J9Class *classToScan = clazz;
	do {
		volatile omrobjectptr_t *slotPtr = NULL;
		GC_ClassStaticsIterator classStaticsIterator(env, classToScan);
		while((slotPtr = classStaticsIterator.nextSlot()) != NULL) {
			_extensions->scavenger->fixupSlotWithoutCompression(slotPtr);
		}
		_extensions->scavenger->fixupSlotWithoutCompression((omrobjectptr_t *)&(classToScan->classObject));
		classToScan = classToScan->replacedClass;
	} while (NULL != classToScan);
}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

void
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)env->getLanguageVMThread(), objectPtr);
	Assert_MM_true(NULL != clazz);
	J9Class *classToScan = clazz;
	do {
		volatile omrobjectptr_t *slotPtr = NULL;
		GC_ClassStaticsIterator classStaticsIterator(env, classToScan);
		while((slotPtr = classStaticsIterator.nextSlot()) != NULL) {
			_extensions->scavenger->backOutFixSlotWithoutCompression(slotPtr);
		}
		_extensions->scavenger->backOutFixSlotWithoutCompression((omrobjectptr_t *)&(classToScan->classObject));
		classToScan = classToScan->replacedClass;
	} while (NULL != classToScan);
}

void
MM_CollectorLanguageInterfaceImpl:: scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env)
{
	GC_SegmentIterator classSegmentIterator(((J9JavaVM*)_omrVM->_language_vm)->classMemorySegments, MEMORY_TYPE_RAM_CLASS);
	J9MemorySegment *classMemorySegment;
	while(NULL != (classMemorySegment = classSegmentIterator.nextSegment())) {
		J9Class *classPtr;
		GC_ClassHeapIterator classHeapIterator((J9JavaVM*)_omrVM->_language_vm, classMemorySegment);
		while(NULL != (classPtr = classHeapIterator.nextClass())) {
			omrobjectptr_t classObject = J9VM_J9CLASS_TO_HEAPCLASS(classPtr);
			if(_extensions->objectModel.isRemembered(classObject)) {
				_extensions->scavenger->backOutObjectScan(env, classObject);
			}
		}
	}
}

/**
 * Reverse a forwarded object.
 *
 * Backout of the forwarding operation, restoring the original object to its original state.
 *
 * @note this operation is not thread safe
 *
 * @param[in] env the environment for the current thread
 */
void
MM_CollectorLanguageInterfaceImpl::scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *originalForwardedHeader)
{
	if (originalForwardedHeader->isForwardedPointer()) {
		omrobjectptr_t objectPtr = originalForwardedHeader->getObject();
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrVM);
		omrobjectptr_t fwdObjectPtr = originalForwardedHeader->getForwardedObject();
		J9Class *forwardedClass = J9GC_J9OBJECT_CLAZZ(fwdObjectPtr);
		Assert_MM_mustBeClass(forwardedClass);
		UDATA forwardedFlags = J9GC_J9OBJECT_FLAGS_FROM_CLAZZ(fwdObjectPtr);
		/* If object just has been moved (this scavenge) we should undo hash flags and set hashed/not moved */
		if (OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS == (forwardedFlags & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS))) {
			forwardedFlags &= ~OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS;
			forwardedFlags |= OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS;
		}
		extensions->objectModel.setObjectClassAndFlags(objectPtr, forwardedClass, forwardedFlags);

#if defined (J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
		/* Restore destroyed overlapped slot in the original object. This slot might need to be reversed
		 * as well or it may be already reversed - such fixup will be completed at in a later pass.
		 */
		originalForwardedHeader->restoreDestroyedOverlap();
#endif /* defined (J9VM_INTERP_COMPRESSED_OBJECT_HEADER) */

		MM_ObjectAccessBarrier* barrier = extensions->accessBarrier;

		/* If the object states are mismatched, the reference object was removed from the reference list.
		 * This is a non-reversable operation. Adjust the state of the original object and its referent field.
		 */
		if ((J9CLASS_FLAGS(forwardedClass) & J9_JAVA_CLASS_REFERENCE_MASK)) {
			I_32 forwadedReferenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(env, fwdObjectPtr);
			J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr) = forwadedReferenceState;
			GC_SlotObject referentSlotObject(_omrVM, &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, fwdObjectPtr));
			if (NULL == referentSlotObject.readReferenceFromSlot()) {
				GC_SlotObject slotObject(_omrVM, &J9GC_J9VMJAVALANGREFERENCE_REFERENT(env, objectPtr));
				slotObject.writeReferenceToSlot(NULL);
			}
			/* Copy back the reference link */
			barrier->setReferenceLink(objectPtr, barrier->getReferenceLink(fwdObjectPtr));
		}

		/* Copy back the finalize link */
		fomrobject_t *finalizeLinkAddress = barrier->getFinalizeLinkAddress(fwdObjectPtr);
		if (NULL != finalizeLinkAddress) {
			barrier->setFinalizeLink(objectPtr, barrier->convertPointerFromToken(*finalizeLinkAddress));
		}
	}
}

#if defined (J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *originalForwardedHeader, MM_MemorySubSpaceSemiSpace *subSpaceNew)
{
	/* Nothing to do if overlap slot is not an object reference, and the destroyed slot for indexable objects is the size */
	if ((0 != originalForwardedHeader->getPreservedOverlap())
			&& !_extensions->objectModel.isIndexable(_extensions->objectModel.getPreservedClass(originalForwardedHeader))
	) {
		/* Check the first description bit */
		bool isObjectSlot = false;
		omrobjectptr_t objectPtr = originalForwardedHeader->getObject();
		uintptr_t *descriptionPtr = (uintptr_t *)J9GC_J9OBJECT_CLAZZ(objectPtr)->instanceDescription;
		if (0 != (((uintptr_t)descriptionPtr) & 1)) {
			isObjectSlot = 0 != (1 & (((uintptr_t)descriptionPtr) >> 1));
		} else {
			isObjectSlot = 0 != (1 & *descriptionPtr);
		}

		if (isObjectSlot) {
			/* Get the uncompressed reference from the slot */
			MM_ObjectAccessBarrier* barrier = _extensions->accessBarrier;
			omrobjectptr_t survivingCopyAddress = barrier->convertPointerFromToken(originalForwardedHeader->getPreservedOverlap());
			/* Check if the address we want to read is aligned (since mis-aligned reads may still be less than a top address but extend beyond it) */
			if (0 == ((uintptr_t)survivingCopyAddress & (_extensions->getObjectAlignmentInBytes() - 1))) {
				/* Ensure that the address we want to read is within part of the heap which could contain copied objects (tenure or survivor) */
				void *topOfObject = (void *)((uintptr_t *)survivingCopyAddress + 1);
				if (subSpaceNew->isObjectInNewSpace(survivingCopyAddress, topOfObject) || _extensions->isOld(survivingCopyAddress, topOfObject)) {
					/* if the slot points to a reverse-forwarded object, restore the original location (in evacuate space) */
					MM_ForwardedHeader reverseForwardedHeader(survivingCopyAddress);
					if (reverseForwardedHeader.isReverseForwardedPointer()) {
						/* first slot must be fixed up */
						originalForwardedHeader->restoreDestroyedOverlap((uint32_t)barrier->convertTokenFromPointer(reverseForwardedHeader.getReverseForwardedPointer()));
					}
				}
			}
		}
	}
}
#endif /* defined (J9VM_INTERP_COMPRESSED_OBJECT_HEADER) */

void
MM_CollectorLanguageInterfaceImpl::private_scavenger_addOwnableSynchronizerObjectInList(MM_EnvironmentStandard *env, omrobjectptr_t object)
{
	omrobjectptr_t link = MM_GCExtensions::getExtensions(_extensions)->accessBarrier->isObjectInOwnableSynchronizerList(object);
	/* if isObjectInOwnableSynchronizerList() return NULL, it means the object isn't in OwanbleSynchronizerList,
	 * it could be the constructing object which would be added in the list after the construction finish later. ignore the object to avoid duplicated reference in the list. */
	if (NULL != link) {
		/* this method expects the caller (scanObject) never pass the same object twice, which could cause circular loop when walk through the list.
		 * the assertion partially could detect duplication case */
		Assert_MM_false(_extensions->scavenger->isObjectInNewSpace(link));
		env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->add(env, object);
		env->getGCEnvironment()->_scavengerJavaStats._ownableSynchronizerTotalSurvived += 1;
		if (_extensions->scavenger->isObjectInNewSpace(object)) {
			env->getGCEnvironment()->_scavengerJavaStats._ownableSynchronizerNurserySurvived += 1;
		}
	}
}

void
MM_CollectorLanguageInterfaceImpl::private_scavenger_setupForOwnableSynchronizerProcessing(MM_EnvironmentStandard *env)
{
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heapRegionManager);
	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
			MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[i];
			if ((MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW))) {
				list->startOwnableSynchronizerProcessing();
			} else {
				list->backupList();
			}
		}
	}
}

bool
MM_CollectorLanguageInterfaceImpl::private_scavenger_shouldPercolateGarbageCollect_classUnloading(MM_EnvironmentBase *envBase)
{
	bool shouldGCPercolate = false;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_Collector *globalCollector = _extensions->getGlobalCollector();
	shouldGCPercolate = globalCollector->isTimeForGlobalGCKickoff();
#endif

	return shouldGCPercolate;
}

bool
MM_CollectorLanguageInterfaceImpl::private_scavenger_shouldPercolateGarbageCollect_activeJNICriticalRegions(MM_EnvironmentBase *envBase)
{
	bool shouldGCPercolate = false;

	UDATA activeCriticalRegions = MM_StandardAccessBarrier::getJNICriticalRegionCount(_extensions);

	/* percolate if any active regions */
	shouldGCPercolate = (0 != activeCriticalRegions);
	return shouldGCPercolate;
}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_switchConcurrentForThread(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	J9VMThread *vmThread = (J9VMThread *)env->getOmrVMThread()->_language_vmthread;

	if (_extensions->scavenger->isConcurrentInProgress()) {
		Assert_MM_true(NULL != _extensions->scavenger);
		void* gsBase = _extensions->scavenger->getEvacuateBase();
		void* gsTop = _extensions->scavenger->getEvacuateTop();

		/* Concurrent Scavenger Page start address must be initialized */
		Assert_MM_true(_extensions->getConcurrentScavengerPageStartAddress() != (void *)UDATA_MAX);

		/* Nursery should fit selected Concurrent Scavenger Page */
		Assert_MM_true(gsBase >= _extensions->getConcurrentScavengerPageStartAddress());
		Assert_MM_true(gsTop <= (void *)((uintptr_t)_extensions->getConcurrentScavengerPageStartAddress() + _extensions->getConcurrentScavengerPageSectionSize() * CONCURRENT_SCAVENGER_PAGE_SECTIONS));

		uintptr_t sectionCount = ((uintptr_t)gsTop - (uintptr_t)gsBase) / _extensions->getConcurrentScavengerPageSectionSize();
		uintptr_t startOffsetInBits = ((uintptr_t)gsBase - (uintptr_t)_extensions->getConcurrentScavengerPageStartAddress()) / _extensions->getConcurrentScavengerPageSectionSize();
		uint64_t bitMask = (((uint64_t)1 << sectionCount) - 1) << (CONCURRENT_SCAVENGER_PAGE_SECTIONS - (sectionCount + startOffsetInBits));

		if (_extensions->isDebugConcurrentScavengerPageAlignment()) {
			void* nurseryBase = OMR_MIN(gsBase, _extensions->scavenger->getSurvivorBase());
			void* nurseryTop = OMR_MAX(gsTop, _extensions->scavenger->getSurvivorTop());
			void* pageBase = _extensions->getConcurrentScavengerPageStartAddress();
			void* pageTop = (void *)((uintptr_t)pageBase + _extensions->getConcurrentScavengerPageSectionSize() * CONCURRENT_SCAVENGER_PAGE_SECTIONS);

			j9tty_printf(PORTLIB, "%p: Nursery [%p,%p] Evacuate [%p,%p] GS [%p,%p] Section size 0x%zx, sections %lu bit offset %lu bit mask 0x%zx\n",
					vmThread, nurseryBase, nurseryTop, gsBase, gsTop, pageBase, pageTop, _extensions->getConcurrentScavengerPageSectionSize() , sectionCount, startOffsetInBits, bitMask);
		}

		vmThread->privateFlags |= J9_PRIVATE_FLAGS_CONCURRENT_SCAVENGER_ACTIVE;
		j9gs_enable(&vmThread->gsParameters, _extensions->getConcurrentScavengerPageStartAddress(), _extensions->getConcurrentScavengerPageSectionSize(), bitMask);
    } else {
		j9gs_disable(&vmThread->gsParameters);
		vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_CONCURRENT_SCAVENGER_ACTIVE;
    }
}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_CollectorLanguageInterfaceImpl::private_compactScheme_setupForOwnableSynchronizerProcessing(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->getHeap()->getHeapRegionManager());
	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
			MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[i];
			list->startOwnableSynchronizerProcessing();
		}
	}
}


void
MM_CollectorLanguageInterfaceImpl::private_compactScheme_verifyHeapObjectSlot(omrobjectptr_t objectPtr, MM_MarkMap *markMap)
{
	if ((objectPtr >= _extensions->getHeap()->getHeapBase()) && (objectPtr < _extensions->getHeap()->getHeapTop())) {
		Assert_MM_true (markMap->isBitSet(objectPtr));
	}
}

void
MM_CollectorLanguageInterfaceImpl::private_compactScheme_verifyHeapMixedObject(omrobjectptr_t objectPtr, MM_MarkMap* markMap)
{
	GC_MixedObjectIterator it(_omrVM, objectPtr);
	while (GC_SlotObject* slotObject = it.nextSlot()) {
		private_compactScheme_verifyHeapObjectSlot(slotObject->readReferenceFromSlot(), markMap);
	}
}

void
MM_CollectorLanguageInterfaceImpl::private_compactScheme_verifyHeapArrayObject(omrobjectptr_t objectPtr, MM_MarkMap* markMap)
{
	GC_PointerContiguousArrayIterator it(_omrVM, objectPtr);
	while (GC_SlotObject* slotObject = it.nextSlot()) {
		private_compactScheme_verifyHeapObjectSlot(slotObject->readReferenceFromSlot(), markMap);
	}
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap)
{
	MM_CompactSchemeCheckMarkRoots rootChecker(MM_EnvironmentStandard::getEnvironment(env));
	rootChecker.scanAllSlots(env);

	/* Check that the heap alignment is as compaction expects it. Compaction
	 * expects that the heap will split into a whole number of pages where
	 * each pages maps to 2 mark bit slots so heap alignment needs to be a multiple
	 * of the compaction page size
	 */
	assume0(_extensions->heapAlignment % (2 * J9BITS_BITS_IN_SLOT * sizeof(UDATA)) == 0);

	MM_HeapRegionManager *regionManager = env->getExtensions()->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);
	MM_HeapRegionDescriptorStandard *region = NULL;

	while(NULL != (region = regionIterator.nextRegion())) {
		void *lowAddress = region->getLowAddress();
		void *highAddress = region->getHighAddress();
		MM_HeapMapIterator markedObjectIterator(_extensions, markMap, (UDATA *)lowAddress, (UDATA *)highAddress);

		omrobjectptr_t objectPtr = NULL;
		while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
			switch(_extensions->objectModel.getScanType(objectPtr)) {
			case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
			case GC_ObjectModel::SCAN_MIXED_OBJECT:
			case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
			case GC_ObjectModel::SCAN_CLASS_OBJECT:
			case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
			case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
				private_compactScheme_verifyHeapMixedObject(objectPtr, markMap);
				break;

			case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
				private_compactScheme_verifyHeapArrayObject(objectPtr, markMap);
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
MM_CollectorLanguageInterfaceImpl::compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme)
{
	MM_CompactSchemeFixupRoots rootScanner(env, compactScheme);
	rootScanner.scanAllSlots(env);
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env)
{
	/* flush ownable synchronizer object buffer after rebuild the ownableSynchronizerObjectList during fixupObjects */
	env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->flush(env);
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env)
{
	private_compactScheme_setupForOwnableSynchronizerProcessing(env);
}

#endif /* OMR_GC_MODRON_COMPACTION */
