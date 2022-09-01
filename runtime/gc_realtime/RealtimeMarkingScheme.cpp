/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#include "omr.h"

#include "CycleState.hpp"
#include "EnvironmentRealtime.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RealtimeMarkTask.hpp"

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_RealtimeMarkingScheme *
MM_RealtimeMarkingScheme::newInstance(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC)
{
	MM_RealtimeMarkingScheme *instance;
	
	instance = (MM_RealtimeMarkingScheme *)env->getForge()->allocate(sizeof(MM_RealtimeMarkingScheme), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (instance) {
		new(instance) MM_RealtimeMarkingScheme(env, realtimeGC);
		if (!instance->initialize(env)) { 
			instance->kill(env);
			instance = NULL;
		}
	}

	return instance;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_RealtimeMarkingScheme::kill(MM_EnvironmentBase *env)
{
	tearDown(env); 
	env->getForge()->free(this);
}

/**
 * Intialize the RealtimeMarkingScheme instance.
 * 
 */
bool
MM_RealtimeMarkingScheme::initialize(MM_EnvironmentBase *env)
{
	if (!MM_SegregatedMarkingScheme::initialize(env)) {
		return false;
	}

	_scheduler = _realtimeGC->_sched;

	return true;
}

/**
 * Teardown the RealtimeMarkingScheme instance.
 * 
 */
void
MM_RealtimeMarkingScheme::tearDown(MM_EnvironmentBase *env)
{
	MM_SegregatedMarkingScheme::tearDown(env);
}

void
MM_RealtimeMarkingScheme::markLiveObjectsInit(MM_EnvironmentBase *env, bool initMarkMap)
{
	MM_EnvironmentRealtime *realtimeEnv = MM_EnvironmentRealtime::getEnvironment(env);
	realtimeEnv->getWorkStack()->reset(realtimeEnv, _realtimeGC->_workPackets);

	/* These are thread-local stats that should probably be moved
	 * into the MM_MarkStats structure.
	 */
	realtimeEnv->resetScannedCounters();

	/* The write barrier must be enabled before any scanning begins. The double barrier will
	 * be enabled for the duration of the thread scans. It gets disabled on a per thread basis
	 * as the threads get scanned. It also gets "disabled" on a global basis once all threads
	 * are scanned.
	 */
	if (realtimeEnv->_currentTask->synchronizeGCThreadsAndReleaseMain(realtimeEnv, UNIQUE_ID)) {
		_realtimeGC->enableWriteBarrier(realtimeEnv);
		_realtimeGC->enableDoubleBarrier(realtimeEnv);
		/* BEN TODO: Ragged barrier here */
		realtimeEnv->_currentTask->releaseSynchronizedGCThreads(realtimeEnv);
	}
}

void
MM_RealtimeMarkingScheme::markLiveObjectsRoots(MM_EnvironmentBase *env)
{
	MM_EnvironmentRealtime *realtimeEnv = MM_EnvironmentRealtime::getEnvironment(env);
	MM_MetronomeDelegate *metronomeDelegate = _realtimeGC->getRealtimeDelegate();

	metronomeDelegate->markLiveObjectsRoots(realtimeEnv);
	_scheduler->condYieldFromGC(realtimeEnv);
}

void
MM_RealtimeMarkingScheme::markLiveObjectsScan(MM_EnvironmentBase *env)
{
	MM_EnvironmentRealtime *realtimeEnv = MM_EnvironmentRealtime::getEnvironment(env);
	MM_MetronomeDelegate *metronomeDelegate = _realtimeGC->getRealtimeDelegate();

	/* Heap Marking and barrier processing. Cannot delay barrier processing until the end.*/	
	_realtimeGC->completeMarking(realtimeEnv);

	metronomeDelegate->markLiveObjectsScan(realtimeEnv);

	if (realtimeEnv->_currentTask->synchronizeGCThreadsAndReleaseMain(realtimeEnv, UNIQUE_ID)) {
		metronomeDelegate->setUnmarkedImpliesCleared();
		realtimeEnv->_currentTask->releaseSynchronizedGCThreads(realtimeEnv);
	}
}

void
MM_RealtimeMarkingScheme::markLiveObjectsComplete(MM_EnvironmentBase *env)
{
	MM_EnvironmentRealtime *realtimeEnv = MM_EnvironmentRealtime::getEnvironment(env);
	MM_MetronomeDelegate *metronomeDelegate = _realtimeGC->getRealtimeDelegate();

	metronomeDelegate->markLiveObjectsComplete(realtimeEnv);

	_scheduler->condYieldFromGC(realtimeEnv);

	/* Do a final tracing phase to complete the marking phase. It should not be possible for any thread,
	 * including NHRT's, to add elements to the rememberedSet between the end of this completeMarking() call and when
	 * we disable the write barrier since the entire live set will be completed.
	 */
	_realtimeGC->completeMarking(realtimeEnv);

	metronomeDelegate->checkReferenceBuffer(realtimeEnv);

	if (realtimeEnv->_currentTask->synchronizeGCThreadsAndReleaseMain(realtimeEnv, UNIQUE_ID)) {
		metronomeDelegate->unsetUnmarkedImpliesCleared();

		/* This is the symmetric call to the enabling of the write barrier that happens at the top of this method. */
		_realtimeGC->disableWriteBarrier(realtimeEnv);
		/* BEN TODO: Ragged barrier here */

		/* reset flag "overflow happened this GC cycle" */
		_realtimeGC->_workPackets->getIncrementalOverflowHandler()->resetOverflowThisGCCycle();

		Assert_MM_true(_realtimeGC->_workPackets->isAllPacketsEmpty());

		realtimeEnv->_currentTask->releaseSynchronizedGCThreads(realtimeEnv);
	}
}

/**
 * If maxCount == MAX_UNIT, we run till work stack is empty and we return true, if at least one
 * object is marked.
 * Otherwise, mark up to maxCount of objects. If we reached the limit return false, which means we are
 * not finished yet.
 */
bool
MM_RealtimeMarkingScheme::incrementalCompleteScan(MM_EnvironmentRealtime *env, uintptr_t maxCount)
{
	uintptr_t item;
	uintptr_t count = 0, countSinceLastYieldCheck = 0;
	uintptr_t scannedPointersSumSinceLastYieldCheck = 0;
	GC_ObjectScannerState objectScannerState;
  
	while(0 != (item = (uintptr_t)env->getWorkStack()->pop(env))) {
		uintptr_t scannedPointers = scanObject(env, item, &objectScannerState);

		countSinceLastYieldCheck += 1;
		scannedPointersSumSinceLastYieldCheck += scannedPointers;
		
		if (((countSinceLastYieldCheck * 2) + scannedPointersSumSinceLastYieldCheck) > _extensions->traceCostToCheckYield) {
			_scheduler->condYieldFromGC(env);
			
			scannedPointersSumSinceLastYieldCheck = 0;
			countSinceLastYieldCheck = 0;
		}
		
		if (++count >= maxCount) {
			return false;
		}
	}
	
	if (maxCount == MAX_UINT) {
		return (count != 0);
	} else {
		return true;
	}
}

uintptr_t
MM_RealtimeMarkingScheme::scanObject(MM_EnvironmentRealtime *env, uintptr_t item, GC_ObjectScannerState *objectScannerState)
{
	uintptr_t scannedPointers = 0;
	uintptr_t sizeToDo = UDATA_MAX;
	GC_ObjectScanner *objectScanner = NULL;
	omrobjectptr_t objectPtr = ITEM_TO_OBJECT(item);
	if (IS_ITEM_ARRAYLET((uintptr_t)item)) {
		uintptr_t referenceSize = env->compressObjectReferences() ? sizeof(uint32_t) : sizeof(uintptr_t);
		uintptr_t sizeInElements = env->getOmrVM()->_arrayletLeafSize / referenceSize;
		sizeToDo = env->getOmrVM()->_arrayletLeafSize;
		objectScanner = GC_PointerArrayObjectScanner::newInstance(env, objectPtr, ITEM_TO_ARRAYLET(item), objectScannerState, GC_ObjectScanner::indexableObjectNoSplit, sizeInElements);
	} else {
		objectScanner = _delegate.getObjectScanner(env, objectPtr, objectScannerState, SCAN_REASON_PACKET, &sizeToDo);
	}
	if (NULL != objectScanner) {
		bool isLeafSlot = false;
		GC_SlotObject *slotObject;
#if defined(OMR_GC_LEAF_BITS)
		while (NULL != (slotObject = objectScanner->getNextSlot(&isLeafSlot))) {
#else /* OMR_GC_LEAF_BITS */
		while (NULL != (slotObject = objectScanner->getNextSlot())) {
#endif /* OMR_GC_LEAF_BITS */
			fixupForwardedSlot(slotObject);
			inlineMarkObjectNoCheck(env, slotObject->readReferenceFromSlot(), isLeafSlot);
			scannedPointers += 1;
		}
	}

	env->incScannedObjects();

	return scannedPointers;
}

uintptr_t
MM_RealtimeMarkingScheme::setupPointerArrayScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason, uintptr_t *sizeToDo, uintptr_t *sizeInElementsToDo, fomrobject_t **basePtr, uintptr_t *flags)
{
	uintptr_t startIndex = 0;
	MM_RealtimeGC *realtimeGC = _extensions->realtimeGC;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(realtimeGC->getRealtimeDelegate()->isDynamicClassUnloadingEnabled()) {
		realtimeGC->getRealtimeDelegate()->markClassOfObject(MM_EnvironmentRealtime::getEnvironment(env), (J9Object *)objectPtr);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


	bool isContiguous = _extensions->indexableObjectModel.isInlineContiguousArraylet((J9IndexableObject*)objectPtr);

	/* Very small arrays cannot be set as scanned (no scanned bit in Mark Map reserved for them) */
	bool canSetAsScanned = (isContiguous
			&& (_extensions->minArraySizeToSetAsScanned <= _extensions->indexableObjectModel.arrayletSize((J9IndexableObject*)objectPtr, 0)));

	if (canSetAsScanned && realtimeGC->getMarkingScheme()->isScanned((J9Object *)objectPtr)) {
		/* Already scanned by ref array copy optimization */
		return 0;
	}

	/* If NUA is enabled, separate path for contiguous arrays */
	uintptr_t sizeInElements = _extensions->indexableObjectModel.getSizeInElements((J9IndexableObject*)objectPtr);
	bool const compressed = env->compressObjectReferences();
	uintptr_t referenceSize = compressed ? sizeof(uint32_t) : sizeof(uintptr_t);
	fomrobject_t *startPtr = 0;

	if (isContiguous || (0 == sizeInElements)) {
		fj9object_t *startScanPtr = (fj9object_t *)_extensions->indexableObjectModel.getDataPointerForContiguous((J9IndexableObject*)objectPtr);
		startPtr = startScanPtr;
	} else {
		sizeInElements = 0;
		fj9object_t *arrayoid = _extensions->indexableObjectModel.getArrayoidPointer((J9IndexableObject*)objectPtr);
		uintptr_t numArraylets = _extensions->indexableObjectModel.numArraylets((J9IndexableObject*)objectPtr);
		for (uintptr_t i = 0; i < numArraylets; i++) {
			uintptr_t arrayletSize = _extensions->indexableObjectModel.arrayletSize((J9IndexableObject*)objectPtr, i);
			/* need to check leaf pointer because this can be a partially allocated arraylet (not all leafs are allocated) */
			GC_SlotObject slotObject(env->getOmrVM(), GC_SlotObject::addToSlotAddress(arrayoid, i, compressed));
			fj9object_t *startScanPtr = (fj9object_t*) (slotObject.readReferenceFromSlot());
			if (NULL != startScanPtr) {
				if (i == (numArraylets - 1)) {
					if (canSetAsScanned) {
						realtimeGC->getMarkingScheme()->setScanAtomic((J9Object *)objectPtr);
					}
					startPtr = startScanPtr;
					sizeInElements = arrayletSize / referenceSize;
				} else {
					env->getWorkStack()->push(env, (void *)ARRAYLET_TO_ITEM(startScanPtr));
				}
			}

		}
	}

	*sizeInElementsToDo = sizeInElements;
	*sizeToDo = sizeInElements * referenceSize;
	*basePtr = startPtr;
	*flags = GC_ObjectScanner::indexableObjectNoSplit;

	return startIndex;
}

