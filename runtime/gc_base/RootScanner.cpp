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
#if defined(J9VM_OPT_JVMTI)
#include "jvmtiInternal.h"
#endif /* defined(J9VM_OPT_JVMTI) */

#include "RootScanner.hpp"

#include "ClassIterator.hpp"
#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "FinalizeListManager.hpp"
#endif /* J9VM_GC_FINALIZATION*/
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySpace.hpp"
#include "MixedObjectIterator.hpp"
#include "ModronTypes.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectModel.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "ContinuationObjectList.hpp"
#include "VMHelpers.hpp"
#include "ParallelDispatcher.hpp"
#include "PointerArrayIterator.hpp"
#include "SlotObject.hpp"
#include "StringTable.hpp"
#include "StringTableIncrementalIterator.hpp"
#include "Task.hpp"
#include "UnfinalizedObjectList.hpp"
#include "VMClassSlotIterator.hpp"
#include "VMInterface.hpp"
#include "VMThreadListIterator.hpp"
#include "VMThreadIterator.hpp"

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doClassLoader(J9ClassLoader *classLoader)
{
	doSlot(J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(classLoader));

	scanModularityObjects(classLoader);
}

void
MM_RootScanner::scanModularityObjects(J9ClassLoader * classLoader)
{
	if (NULL != classLoader->moduleHashTable) {
		J9HashTableState moduleWalkState;
		J9Module **modulePtr = (J9Module**)hashTableStartDo(classLoader->moduleHashTable, &moduleWalkState);
		while (NULL != modulePtr) {
			J9Module * const module = *modulePtr;

			doSlot(&module->moduleObject);
			if (NULL != module->moduleName) {
				doSlot(&module->moduleName);
			}
			if (NULL != module->version) {
				doSlot(&module->version);
			}
			modulePtr = (J9Module**)hashTableNextDo(&moduleWalkState);
		}

		if (classLoader == _javaVM->systemClassLoader) {
			doSlot(&_javaVM->unamedModuleForSystemLoader->moduleObject);
		}
	}
}

/**
 * @todo Provide function documentation
 */
MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanWeakReferencesComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}


/**
 * @todo Provide function documentation
 */
MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanSoftReferencesComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}

/**
 * @todo Provide function documentation
 */
MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanPhantomReferencesComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}

#if defined(J9VM_GC_FINALIZATION)
void
MM_RootScanner::doUnfinalizedObject(J9Object *objectPtr, MM_UnfinalizedObjectList *list)
{
	/* This function needs to be overridden if the default implementation of scanUnfinalizedObjects
	 * is used.
	 */
	Assert_MM_unreachable();
}

/**
 * @todo Provide function documentation
 */
MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_RootScanner::doOwnableSynchronizerObject(J9Object *objectPtr, MM_OwnableSynchronizerObjectList *list)
{
	/* This function needs to be overridden if the default implementation of scanOwnableSynchronizerObjects
	 * is used.
	 */
	Assert_MM_unreachable();
}

void
MM_RootScanner::doContinuationObject(J9Object *objectPtr, MM_ContinuationObjectList *list)
{
	Assert_MM_unreachable();
}


MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanOwnableSynchronizerObjectsComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}

MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanContinuationObjectsComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
{
	J9ThreadAbstractMonitor *monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
	doSlot((J9Object **)&monitor->userData);
}

MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanMonitorReferencesComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}


/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doMonitorLookupCacheSlot(j9objectmonitor_t *slotPtr)
{
	if (0 != *slotPtr) {
		*slotPtr = 0;
	}
}


/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doJNIWeakGlobalReference(J9Object **slotPtr)
{
	doSlot(slotPtr);
}

#if defined(J9VM_GC_MODRON_SCAVENGER)
/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doRememberedSetSlot(J9Object **slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator)
{
	doSlot(slotPtr);
}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator)
{
	doSlot(slotPtr);
}

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
void
MM_RootScanner::doObjectInVirtualLargeObjectHeap(J9Object *objectPtr, bool *sparseHeapAllocation)
{
	/* No need to call doSlot() here since there's nothing to update */
}
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

/**
 * @Perform operation on the given string cache table slot.
 * @String table cache contains cached entries of string table, it's
 * @a subset of string table entries.
 */
void
MM_RootScanner::doStringCacheTableSlot(J9Object **slotPtr)
{
	doSlot(slotPtr);
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doVMClassSlot(J9Class *classPtr)
{
	doClassSlot(classPtr);
}

/**
 * General object field slot handler to be reimplemented by specializing class. This handler is called
 * for every reference through an instance field.
 * Implementation for slotObject input format
 * @param slotObject Input field for scan in slotObject format
 */
void
MM_RootScanner::doFieldSlot(GC_SlotObject *slotObject)
{
	J9Object *object = slotObject->readReferenceFromSlot();
	doSlot(&object);
	slotObject->writeReferenceToSlot(object);
}

#if defined(J9VM_OPT_JVMTI)
/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
{
	doSlot(slotPtr);
}
#endif /* J9VM_OPT_JVMTI */

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanClasses(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_Classes);

	GC_SegmentIterator segmentIterator(_javaVM->classMemorySegments, MEMORY_TYPE_RAM_CLASS);

	while (J9MemorySegment *segment = segmentIterator.nextSegment()) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
			J9Class *clazz = NULL;
			while (NULL != (clazz = classHeapIterator.nextClass())) {
				doClass(clazz);
				if (shouldYieldFromClassScan(100000)) {
					yield();
				}
			}
		}
	}

	condYield();

	reportScanningEnded(RootScannerEntity_Classes);
}

/**
 * Scan through the slots in the VM containing known classes
 */
void
MM_RootScanner::scanVMClassSlots(MM_EnvironmentBase *env)
{
	/* VM Class Slot Iterator */
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_VMClassSlots);

		GC_VMClassSlotIterator classSlotIterator(_javaVM);
		J9Class *classPtr;

		while (NULL != (classPtr = classSlotIterator.nextSlot())) {
			doVMClassSlot(classPtr);
		}

		reportScanningEnded(RootScannerEntity_VMClassSlots);
	}
}

/**
 * General class slot handler to be reimplemented by specializing class.
 * This handler is called for every reference to a J9Class.
 */
void
MM_RootScanner::doClassSlot(J9Class *classPtr)
{
	/* ignore class slots by default */
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doStackSlot(J9Object **slotPtr, void *walkState, const void *stackLocation)
{
	/* ensure that this isn't a slot pointing into the gap (only matters for split heap VMs) */
	if (!_extensions->heap->objectIsInGap(*slotPtr)) {
		doSlot(slotPtr);
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator)
{
	doSlot(slotPtr);
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::doJNIGlobalReferenceSlot(J9Object **slotPtr, GC_JNIGlobalReferenceIterator *jniGlobalReferenceIterator)
{
	doSlot(slotPtr);
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
/**
 * Scan all classes which will never be unloaded (those that
 * were loaded either by the system class loader or the application
 * class loader).
 */
void
MM_RootScanner::scanPermanentClasses(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_PermanentClasses);

	/* Do systemClassLoader */
	scanClassloader(env, _javaVM->systemClassLoader);

	/* Do applicationClassLoader */
	scanClassloader(env, _javaVM->applicationClassLoader);

	/* Do extensionClassLoader */
	scanClassloader(env, _javaVM->extensionClassLoader);

	condYield();

	reportScanningEnded(RootScannerEntity_PermanentClasses);
}

/**
 * Scan all objects from class loader.
 */
void
MM_RootScanner::scanClassloader(MM_EnvironmentBase *env, J9ClassLoader *classLoader)
{
	J9MemorySegment *segment = NULL;
	J9Class *clazz = NULL;

	if (NULL != classLoader) {
		GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
		while (NULL != (segment = segmentIterator.nextSegment())) {
			if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
				while (NULL != (clazz = classHeapIterator.nextClass())) {
					doClass(clazz);
					if (shouldYieldFromClassScan(100000)) {
						yield();
					}
				}
			}
		}

		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			scanModularityObjects(classLoader);
		}
	}
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

/**
 * Checkpoint signalling completion of scanning of class data (classes and classloaders).
 * This checkpoint is called once for any larger scanning phase
 * which includes class scanning.
 * @see scanClasses
 * @see scanPermanentClasses
 */
MM_RootScanner::CompletePhaseCode
MM_RootScanner::scanClassesComplete(MM_EnvironmentBase *env)
{
	return complete_phase_OK;
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanClassLoaders(MM_EnvironmentBase *env)
{
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_ClassLoaders);

		J9ClassLoader *classLoader;

		GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
		while ((classLoader = classLoaderIterator.nextSlot()) != NULL) {
			doClassLoader(classLoader);
		}

		reportScanningEnded(RootScannerEntity_ClassLoaders);
	}
}

/**
 * @todo Provide function documentation
 */
void
stackSlotIterator(J9JavaVM *javaVM, J9Object **slot, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	StackIteratorData *data = (StackIteratorData *)localData;
	data->rootScanner->doStackSlot(slot, walkState, stackLocation);
}

/**
 * @todo Provide function documentation
 *
 * This function iterates through all the threads, calling scanOneThread on each one that
 * should be scanned.  The scanOneThread function scans exactly one thread and returns
 * either true (if it took an action that requires the thread list iterator to return to
 * the beginning) or false (if the thread list iterator should just continue with the next
 * thread).
 */
void
MM_RootScanner::scanThreads(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_Threads);

	/* TODO This assumes that while exclusive vm access is held the thread
	 * list is also locked.
	 */

	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	StackIteratorData localData;

	localData.rootScanner = this;
	localData.env = env;

	while (J9VMThread *walkThread = vmThreadListIterator.nextVMThread()) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			if (scanOneThread(env, walkThread, (void *) &localData)) {
				vmThreadListIterator.reset(_javaVM->mainThread);
			}
		}
	}

	reportScanningEnded(RootScannerEntity_Threads);
}

/**
 * This function scans exactly one thread for potential roots.  It is designed as
 *    an overridable subroutine of the primary functions scanThreads and scanSingleThread.
 * @param walkThead the thread to be scanned
 * @param localData opaque data to be passed to the stack walker callback function.
 *   The root scanner fixes that callback function to the stackSlotIterator function
 *   defined above
 * @return true if the thread scan included an action that requires the thread list
 *   iterator to begin over again at the beginning, false otherwise.  This implementation
 *   always returns false but an overriding implementation may take actions that require
 *   a true return (for example, RealtimeRootScanner returns true if it yielded after the
 *   scan, allowing other threads to run and perhaps be created or terminated).
 **/
bool
MM_RootScanner::scanOneThread(MM_EnvironmentBase *env, J9VMThread *walkThread, void *localData)
{
	GC_VMThreadIterator vmThreadIterator(walkThread);

	while (J9Object **slot = vmThreadIterator.nextSlot()) {
		doVMThreadSlot(slot, &vmThreadIterator);
	}

	J9VMThread *currentThread = (J9VMThread *)env->getOmrVMThread()->_language_vmthread;
	/* In a case this thread is a carrier thread, and a virtual thread is mounted, we will scan virtual thread's stack.
	 * If virtual thread is not mounted, or this is just a regular thread, this will scan its own stack. */
	GC_VMThreadStackSlotIterator::scanSlots(currentThread, walkThread, localData, stackSlotIterator, isStackFrameClassWalkNeeded(), _trackVisibleStackFrameDepth);

#if JAVA_SPEC_VERSION >= 19
	if (NULL != walkThread->currentContinuation)
	{
		/* At this point we know that a virtual thread is mounted. We previously scanned its stack,
		 * and now we will scan carrier's stack, that continuation struct is currently pointing to. */
		GC_VMThreadStackSlotIterator::scanSlots(currentThread, walkThread, walkThread->currentContinuation, localData, stackSlotIterator, isStackFrameClassWalkNeeded(), _trackVisibleStackFrameDepth);
	}
#endif /* JAVA_SPEC_VERSION >= 19 */
	return false;
}

/**
 * This function scans exactly one thread for potential roots.
 * @param walkThead the thread to be scanned
 **/
void
MM_RootScanner::scanSingleThread(MM_EnvironmentBase *env, J9VMThread *walkThread)
{
	StackIteratorData localData;
	localData.rootScanner = this;
	localData.env = env;
	scanOneThread(env, walkThread, &localData);
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanFinalizableObjects(MM_EnvironmentBase *env)
{
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_FinalizableObjects);

		GC_FinalizeListManager *finalizeListManager = _extensions->finalizeListManager;
		{
			/* walk finalizable objects loaded by the system class loader */
			j9object_t systemObject = finalizeListManager->peekSystemFinalizableObject();
			while (NULL != systemObject) {
				doFinalizableObject(systemObject);
				systemObject = finalizeListManager->peekNextSystemFinalizableObject(systemObject);
			}
		}

		{
			/* walk finalizable objects loaded by all other class loaders*/
			j9object_t defaultObject = finalizeListManager->peekDefaultFinalizableObject();
			while (NULL != defaultObject) {
				doFinalizableObject(defaultObject);
				defaultObject = finalizeListManager->peekNextDefaultFinalizableObject(defaultObject);
			}
		}

		{
			/* walk reference objects */
			j9object_t referenceObject = finalizeListManager->peekReferenceObject();
			while (NULL != referenceObject) {
				doFinalizableObject(referenceObject);
				referenceObject = finalizeListManager->peekNextReferenceObject(referenceObject);
			}
		}

		reportScanningEnded(RootScannerEntity_FinalizableObjects);
	}
}
#endif /* J9VM_GC_FINALIZATION */

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanJNIGlobalReferences(MM_EnvironmentBase *env)
{
	/* JNI Global References */
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_JNIGlobalReferences);

		GC_JNIGlobalReferenceIterator jniGlobalReferenceIterator(_javaVM->jniGlobalReferences);
		J9Object **slot;

		while ((slot = (J9Object **)jniGlobalReferenceIterator.nextSlot()) != NULL) {
			doJNIGlobalReferenceSlot(slot, &jniGlobalReferenceIterator);
		}

		reportScanningEnded(RootScannerEntity_JNIGlobalReferences);
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanStringTable(MM_EnvironmentBase *env)
{
	MM_StringTable *stringTable = _extensions->getStringTable();
	reportScanningStarted(RootScannerEntity_StringTable);
	bool isMetronomeGC = _extensions->isMetronomeGC();

	/* String Table */
	for (uintptr_t tableIndex = 0; tableIndex < stringTable->getTableCount(); tableIndex++) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {

			if (isMetronomeGC) {
				GC_StringTableIncrementalIterator stringTableIterator(stringTable->getTable(tableIndex));
				J9Object **slot = NULL;
				
				stringTableIterator.disableTableGrowth();
				while (stringTableIterator.nextIncrement()) {
					while (NULL != (slot = (J9Object **)stringTableIterator.nextSlot())) {
						doStringTableSlot(slot, &stringTableIterator);
					}
					if (shouldYieldFromStringScan()) {
						yield();
					}
				}
				stringTableIterator.enableTableGrowth();
			} else {
				GC_StringTableIterator stringTableIterator(stringTable->getTable(tableIndex));
				J9Object **slot = NULL;
				while (NULL != (slot = (J9Object **)stringTableIterator.nextSlot())) {
					doStringTableSlot(slot, &stringTableIterator);
				}
			}
		}
	}

	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		j9object_t *stringCacheTable = stringTable->getStringInternCache();
		uintptr_t cacheTableIndex = 0;
		for (; cacheTableIndex < MM_StringTable::getCacheSize(); cacheTableIndex++) {
			doStringCacheTableSlot(&stringCacheTable[cacheTableIndex]);
		}
	}

	reportScanningEnded(RootScannerEntity_StringTable);
}

/**
 * Scan the weak reference list.
 * @note Extra locking for NHRTs can be omitted, because it is impossible for
 * an NHRT to create a reference object that will live longer than its referent;
 * thus reference objects created by NHRTs do not need to go on the list.
 */
void
MM_RootScanner::scanWeakReferenceObjects(MM_EnvironmentBase *env)
{
}

/**
 * Scan the soft reference list.
 * @note Extra locking for NHRTs can be omitted, because it is impossible for
 * an NHRT to create a reference object that will live longer than its referent;
 * thus reference objects created by NHRTs do not need to go on the list.
 */
void
MM_RootScanner::scanSoftReferenceObjects(MM_EnvironmentBase *env)
{
}

/**
 * Scan the phantom reference list.
 * @note Extra locking for NHRTs can be omitted, because it is impossible for
 * an NHRT to create a reference object that will live longer than its referent;
 * thus reference objects created by NHRTs do not need to go on the list.
 */
void
MM_RootScanner::scanPhantomReferenceObjects(MM_EnvironmentBase *env)
{
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 *
 * @NOTE this can only be used as a READ-ONLY version of the unfinalizedObjectList.
 * If you need to modify elements with-in the list you will need to provide your own functionality.
 * See MM_MarkingScheme::scanUnfinalizedObjects(MM_EnvironmentStandard *env) as an example
 * which modifies elements within the list.
 */
void
MM_RootScanner::scanUnfinalizedObjects(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_UnfinalizedObjects);

	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_UnfinalizedObjectList *unfinalizedObjectList = _extensions->unfinalizedObjectLists;
	while (NULL != unfinalizedObjectList) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			J9Object *objectPtr = unfinalizedObjectList->getHeadOfList();
			while (NULL != objectPtr) {
				doUnfinalizedObject(objectPtr, unfinalizedObjectList);
				objectPtr = barrier->getFinalizeLink(objectPtr);
			}
		}
		unfinalizedObjectList = unfinalizedObjectList->getNextList();
	}

	reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_RootScanner::scanOwnableSynchronizerObjects(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_OwnableSynchronizerObjects);

	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = _extensions->getOwnableSynchronizerObjectLists();
	while (NULL != ownableSynchronizerObjectList) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			J9Object *objectPtr = ownableSynchronizerObjectList->getHeadOfList();
			while (NULL != objectPtr) {
				doOwnableSynchronizerObject(objectPtr, ownableSynchronizerObjectList);
				objectPtr = barrier->getOwnableSynchronizerLink(objectPtr);
			}
		}
		ownableSynchronizerObjectList = ownableSynchronizerObjectList->getNextList();
	}

	reportScanningEnded(RootScannerEntity_OwnableSynchronizerObjects);
}

void
MM_RootScanner::scanContinuationObjects(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_ContinuationObjects);

	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_ContinuationObjectList *continuationObjectList = _extensions->getContinuationObjectLists();
	while (NULL != continuationObjectList) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			J9Object *objectPtr = continuationObjectList->getHeadOfList();
			while (NULL != objectPtr) {
				doContinuationObject(objectPtr, continuationObjectList);
				objectPtr = barrier->getContinuationLink(objectPtr);
			}
		}
		continuationObjectList = continuationObjectList->getNextList();
	}

	reportScanningEnded(RootScannerEntity_ContinuationObjects);
}

/**
 * Scan the per-thread object monitor lookup caches.
 * Note that this is not a root since the cache contains monitors from the global monitor table
 * which will be scanned by scanMonitorReferences. It should be scanned first, however, since
 * scanMonitorReferences may destroy monitors that appear in caches.
 */
void
MM_RootScanner::scanMonitorLookupCaches(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_MonitorLookupCaches);
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	while (J9VMThread *walkThread = vmThreadListIterator.nextVMThread()) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			j9objectmonitor_t *objectMonitorLookupCache = walkThread->objectMonitorLookupCache;
			uintptr_t cacheIndex = 0;
			for (; cacheIndex < J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE; cacheIndex++) {
				doMonitorLookupCacheSlot(&objectMonitorLookupCache[cacheIndex]);
			}
		}
	}
	reportScanningEnded(RootScannerEntity_MonitorLookupCaches);
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanMonitorReferences(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_MonitorReferences);

	J9ObjectMonitor *objectMonitor = NULL;
	J9MonitorTableListEntry *monitorTableList = _javaVM->monitorTableList;
	while (NULL != monitorTableList) {
		J9HashTable *table = monitorTableList->monitorTable;
		if (NULL != table) {
			if(_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				GC_HashTableIterator iterator(table);
				while (NULL != (objectMonitor = (J9ObjectMonitor *)iterator.nextSlot())) {
					doMonitorReference(objectMonitor, &iterator);
				}
			}
		}
		monitorTableList = monitorTableList->next;
	}

	reportScanningEnded(RootScannerEntity_MonitorReferences);
}

/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanJNIWeakGlobalReferences(MM_EnvironmentBase *env)
{
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_JNIWeakGlobalReferences);

		GC_JNIWeakGlobalReferenceIterator jniWeakGlobalReferenceIterator(_javaVM->jniWeakGlobalReferences);
		J9Object **slot;

		while ((slot = (J9Object **)jniWeakGlobalReferenceIterator.nextSlot()) != NULL) {
			doJNIWeakGlobalReference(slot);
		}

		reportScanningEnded(RootScannerEntity_JNIWeakGlobalReferences);
	}
}

#if defined(J9VM_GC_MODRON_SCAVENGER)
/**
 * @todo Provide function documentation
 */
void
MM_RootScanner::scanRememberedSet(MM_EnvironmentBase *env)
{
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_RememberedSet);

		MM_SublistPuddle *puddle;
		J9Object **slotPtr;
		GC_RememberedSetIterator rememberedSetIterator(&_extensions->rememberedSet);

		while ((puddle = rememberedSetIterator.nextList()) != NULL) {
			GC_RememberedSetSlotIterator rememberedSetSlotIterator(puddle);
			while ((slotPtr = (J9Object **)rememberedSetSlotIterator.nextSlot()) != NULL) {
				doRememberedSetSlot(slotPtr, &rememberedSetSlotIterator);
			}
		}

		reportScanningEnded(RootScannerEntity_RememberedSet);
	}
}
#endif /* J9VM_GC_MODRON_SCAVENGER */

#if defined(J9VM_OPT_JVMTI)
/**
 * Scan the JVMTI tag tables for object references
 */
void
MM_RootScanner::scanJVMTIObjectTagTables(MM_EnvironmentBase *env)
{
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		reportScanningStarted(RootScannerEntity_JVMTIObjectTagTables);

		J9JVMTIData *jvmtiData = J9JVMTI_DATA_FROM_VM(_javaVM);
		J9JVMTIEnv *jvmtiEnv = NULL;
		J9Object **slotPtr = NULL;
		if (NULL != jvmtiData) {
			/* TODO: When JVMTI is supported in RTSJ, this structure needs to be locked
			 * when it is being scanned
			 */
			GC_JVMTIObjectTagTableListIterator objectTagTableList(jvmtiData->environments);
			while (NULL != (jvmtiEnv = (J9JVMTIEnv *)objectTagTableList.nextSlot())) {
				if (NULL != jvmtiEnv->objectTagTable) {
					GC_JVMTIObjectTagTableIterator objectTagTableIterator(jvmtiEnv->objectTagTable);
					while (NULL != (slotPtr = (J9Object **)objectTagTableIterator.nextSlot())) {
						doJVMTIObjectTagSlot(slotPtr, &objectTagTableIterator);
					}
				}
			}
		}

		reportScanningEnded(RootScannerEntity_JVMTIObjectTagTables);
	}
}
#endif /* J9VM_OPT_JVMTI */

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
void
MM_RootScanner::scanObjectsInVirtualLargeObjectHeap(MM_EnvironmentBase *env)
{
	if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		GC_HeapRegionIteratorVLHGC regionIterator(_extensions->heap->getHeapRegionManager());
		MM_HeapRegionDescriptorVLHGC *region = NULL;
		reportScanningStarted(RootScannerEntity_virtualLargeObjectHeapObjects);
		while (NULL != (region = regionIterator.nextRegion())) {
			if (region->isArrayletLeaf()) {
				if (region->_sparseHeapAllocation) {
					J9Object *spineObject = (J9Object *)region->_allocateData.getSpine();
					Assert_MM_true(NULL != spineObject);
					doObjectInVirtualLargeObjectHeap(spineObject, &region->_sparseHeapAllocation);
				}
			}
		}
		reportScanningEnded(RootScannerEntity_virtualLargeObjectHeapObjects);
	}
}
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

/**
 * Scan all root set references from the VM into the heap.
 * For all slots that are hard root references into the heap, the appropriate slot handler will be called.
 * @note This includes all references to classes.
 */
void
MM_RootScanner::scanRoots(MM_EnvironmentBase *env)
{
	if (_classDataAsRoots || _nurseryReferencesOnly || _nurseryReferencesPossibly) {
		/* The classLoaderObject of a class loader might be in the nursery, but a class loader
		 * can never be in the remembered set, so include class loaders here.
		 */
		scanClassLoaders(env);
	}

	if (!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (_classDataAsRoots) {
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
			scanClasses(env);
			/* We are scanning all classes, no need to include stack frame references */
			setIncludeStackFrameClassReferences(false);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		} else {
			scanPermanentClasses(env);
			setIncludeStackFrameClassReferences(true);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		if (complete_phase_ABORT == scanClassesComplete(env)) {
			return ;
		}
	}

	scanThreads(env);
#if defined(J9VM_GC_FINALIZATION)
	scanFinalizableObjects(env);
#endif /* J9VM_GC_FINALIZATION */
	scanJNIGlobalReferences(env);

	if (_jniWeakGlobalReferencesTableAsRoot) {
		/* JNI Weak Global References table should be scanned as a hard root */
		scanJNIWeakGlobalReferences(env);
	}

/* In the RT configuration, We can skip scanning the string table because
   all interned strings are in immortal memory and will not move. */
	if (_stringTableAsRoot && (!_nurseryReferencesOnly && !_nurseryReferencesPossibly)){
		scanStringTable(env);
	}
}

/**
 * Scan all clearable root set references from the VM into the heap.
 * For all slots that are clearable root references into the heap, the appropriate slot handler will be
 * called.
 * @note This includes all references to classes.
 */
void
MM_RootScanner::scanClearable(MM_EnvironmentBase *env)
{
	/* This may result in more marking, if aged soft references could not be enqueued. */
	scanSoftReferenceObjects(env);
	if (complete_phase_ABORT == scanSoftReferencesComplete(env)) {
		return ;
	}

	scanWeakReferenceObjects(env);
	if (complete_phase_ABORT == scanWeakReferencesComplete(env)) {
		return ;
	}

#if defined(J9VM_GC_FINALIZATION)
	/* This may result in more marking, but any weak references
	 * must be cleared before, or at the same time as, the referent
	 * is moved to the finalizable list.
	 */
	scanUnfinalizedObjects(env);
	if (complete_phase_ABORT == scanUnfinalizedObjectsComplete(env)) {
		return ;
	}
#endif /* J9VM_GC_FINALIZATION */

	if (!_jniWeakGlobalReferencesTableAsRoot) {
		/* Skip Clearable phase if it was treated as a hard root already */
		scanJNIWeakGlobalReferences(env);
	}

	scanPhantomReferenceObjects(env);
	if (complete_phase_ABORT == scanPhantomReferencesComplete(env)) {
		return ;
	}

	/* Just completed last phase that may have done any object scanning (or copied objects if a copying collector).
	 * Collectors have a chance to invoke specific processing and checks.
	 */
	completedObjectScanPhasesCheckpoint();

	/*
	 * clear the following private references once resurrection is completed 
	 */
	scanMonitorLookupCaches(env);
	scanMonitorReferences(env);
	if(complete_phase_ABORT == scanMonitorReferencesComplete(env)) {
		return ;
	}

	if (!_stringTableAsRoot && (!_nurseryReferencesOnly && !_nurseryReferencesPossibly)) {
		scanStringTable(env);
	}

	scanOwnableSynchronizerObjects(env);
	scanContinuationObjects(env);
#if JAVA_SPEC_VERSION >= 19
	J9JITConfig *jitConfig = _javaVM->jitConfig;
	if ((NULL != jitConfig) && (NULL != jitConfig->methodsToDelete)) {
		iterateAllContinuationObjects(env);
	}
#endif /* JAVA_SPEC_VERSION >= 19 */

#if defined(J9VM_GC_MODRON_SCAVENGER)
	/* Remembered set is clearable in a generational system -- if an object in old
	 * space dies, and it pointed to an object in new space, it needs to be removed
	 * from the remembered set.
	 * This must after any other marking might occur, e.g. phantom references.
	 */
	if (_includeRememberedSetReferences && !_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		scanRememberedSet(env);
	}
#endif /* J9VM_GC_MODRON_SCAVENGER */

#if defined(J9VM_OPT_JVMTI)
	if (_includeJVMTIObjectTagTables) {
		scanJVMTIObjectTagTables(env);
	}
#endif /* J9VM_OPT_JVMTI */

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	if (_includeVirtualLargeObjectHeap) {
		scanObjectsInVirtualLargeObjectHeap(env);
	}
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
}

/**
 * Scan all slots which contain references into the heap.
 * @note this includes class references.
 */
void
MM_RootScanner::scanAllSlots(MM_EnvironmentBase *env)
{
	if (!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		scanClasses(env);
		scanVMClassSlots(env);
	}

	scanClassLoaders(env);

	scanThreads(env);
#if defined(J9VM_GC_FINALIZATION)
	scanFinalizableObjects(env);
#endif /* J9VM_GC_FINALIZATION */
	scanJNIGlobalReferences(env);

	if (!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		scanStringTable(env);
	}

	scanWeakReferenceObjects(env);
	scanSoftReferenceObjects(env);
	scanPhantomReferenceObjects(env);

#if defined(J9VM_GC_FINALIZATION)
	scanUnfinalizedObjects(env);
#endif /* J9VM_GC_FINALIZATION */

	scanMonitorReferences(env);
	scanJNIWeakGlobalReferences(env);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	if (_includeRememberedSetReferences && !_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
		scanRememberedSet(env);
	}
#endif /* J9VM_GC_MODRON_SCAVENGER */
	
#if defined(J9VM_OPT_JVMTI)
	if (_includeJVMTIObjectTagTables) {
		scanJVMTIObjectTagTables(env);
	}
#endif /* J9VM_OPT_JVMTI */

#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	if (_includeVirtualLargeObjectHeap) {
		scanObjectsInVirtualLargeObjectHeap(env);
	}
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */

	scanOwnableSynchronizerObjects(env);
	scanContinuationObjects(env);
}

bool
MM_RootScanner::shouldYieldFromClassScan(uintptr_t timeSlackNanoSec)
{
	return false;
}

bool
MM_RootScanner::shouldYieldFromStringScan()
{
	return false;
}

bool
MM_RootScanner::shouldYieldFromMonitorScan()
{
	return false;
}

bool
MM_RootScanner::shouldYield()
{
	return false;
}

void
MM_RootScanner::yield()
{
}

bool
MM_RootScanner::condYield(U_64 timeSlackNanoSec)
{
	bool yielded = shouldYield();

	if (yielded) {
		yield();
	}

	return yielded;
}
