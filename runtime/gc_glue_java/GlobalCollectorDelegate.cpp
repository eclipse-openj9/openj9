/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9nonbuilder.h"
#include "j9nongenerated.h"
#include "mmprivatehook.h"
#include "ModronAssertions.h"
#include "omrgcconsts.h"
#include "omrhookable.h"

#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "ClassUnloadStats.hpp"
#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "FinalizerSupport.hpp"
#include "FrequentObjectsStats.hpp"
#include "GCExtensionsBase.hpp"
#include "GCObjectEvents.hpp"
#include "GlobalCollectorDelegate.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "MarkingDelegate.hpp"
#include "MarkingScheme.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectModel.hpp"
#include "ParallelGlobalGC.hpp"
#include "ParallelHeapWalker.hpp"
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
#include "ReadBarrierVerifier.hpp"
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
#include "ReferenceChainWalkerMarkMap.hpp"
#include "ReferenceObjectList.hpp"
#include "ScavengerJavaStats.hpp"
#include "StandardAccessBarrier.hpp"
#include "VMThreadListIterator.hpp"

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
/**
 * Function to fix single object on the heap but only if its class is marked as dying.  Other dead objects
 * who still have valid classes (that is, classes not about to be unloaded) are ignored (see CMVC 122959
 * for the rationale behind this function).
 */
static void
fixObjectIfClassDying(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData)
{
	/* Check to see if the object's class is being unloaded. If so, it can't be left as dark matter so abandon it */
	uintptr_t classFlags = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(object));
	if (0 != (classFlags & J9AccClassDying)) {
		MM_MemorySubSpace *memorySubSpace = region->getSubSpace();
		uintptr_t deadObjectByteSize = MM_GCExtensions::getExtensions(omrVMThread)->objectModel.getConsumedSizeInBytesWithHeader(object);
		memorySubSpace->abandonHeapChunk(object, ((U_8*)object) + deadObjectByteSize);
		/* the userdata is a counter of dead objects fixed up so increment it here as a uintptr_t */
		*((uintptr_t *)userData) += 1;
	}
}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

bool
MM_GlobalCollectorDelegate::initialize(MM_EnvironmentBase *env, MM_GlobalCollector *globalCollector, MM_MarkingScheme *markingScheme)
{
	_markingScheme = markingScheme;
	_globalCollector = globalCollector;
	_javaVM = (J9JavaVM*)env->getLanguageVM();
	_extensions = MM_GCExtensions::getExtensions(env);

	/* This delegate is used primarily by MM_ParallelGlobalGC but is declared in base MM_GlobalCollector
	 * class which is base class for MM_IncrementalGlobalGC (balanced) and MM_RealtimeGC (realtime). The
	 * only MM_GlobalCollector methods (postCollect and isTimeForGlobalGCKickoff) that use this
	 * delegate do not use _globalCollector or _markingScheme, so these are required to be NULLed for the
	 * balanced and realtime GC policies (for safety), and not NULL for standard GC policies.
	 */
	Assert_MM_true((NULL != _globalCollector) == _extensions->isStandardGC());
	Assert_MM_true((NULL != _markingScheme) == _extensions->isStandardGC());

	/* Balanced and realtime polices will instantiate their own access barrier */
	if (_extensions->isStandardGC()) {

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
		if (1 == _extensions->fvtest_enableReadBarrierVerification) {
			_extensions->accessBarrier = MM_ReadBarrierVerifier::newInstance(env);
		} else
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
		{
			_extensions->accessBarrier = MM_StandardAccessBarrier::newInstance(env);
		}

		if (NULL == _extensions->accessBarrier) {
			return false;
		}
	}

	return true;
}

void
MM_GlobalCollectorDelegate::tearDown(MM_EnvironmentBase *env)
{
	if (_extensions->isStandardGC() && (NULL != _extensions->accessBarrier)) {
		_extensions->accessBarrier->kill(env);
		_extensions->accessBarrier = NULL;
	}
}

void
MM_GlobalCollectorDelegate::masterThreadGarbageCollectStarted(MM_EnvironmentBase *env)
{
	/* Clear the java specific mark stats */
	_extensions->markJavaStats.clear();

#if defined(J9VM_GC_MODRON_COMPACTION)
	_criticalSectionCount = MM_StandardAccessBarrier::getJNICriticalRegionCount(_extensions);
#endif /* J9VM_GC_MODRON_COMPACTION */

#if defined(J9VM_GC_MODRON_SCAVENGER)
	if (_extensions->scavengerEnabled) {
		/* clear scavenger stats for correcting the ownableSynchronizerObjects stats, only in generational gc */
		_extensions->scavengerJavaStats.clearOwnableSynchronizerCounts();
	}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

#if defined(J9VM_GC_FINALIZATION)
	/* this should not be set by the GC since it is used by components in order to record that they performed some operation which will require that we do some finalization */
	_finalizationRequired = false;
#endif /* J9VM_GC_FINALIZATION */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool forceUnloading = false;

	/* Set the dynamic class unloading flag based on command line and runtime state */
	switch (_extensions->dynamicClassUnloading) {
		case MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER:
			_extensions->runtimeCheckDynamicClassUnloading = false;
			forceUnloading = false;
			break;
		case MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_ALWAYS:
			_extensions->runtimeCheckDynamicClassUnloading = true;
			forceUnloading = true;
			break;
		case MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_ON_CLASS_LOADER_CHANGES:
			forceUnloading = env->_cycleState->_gcCode.isAggressiveGC();
			_extensions->runtimeCheckDynamicClassUnloading = forceUnloading || _extensions->classLoaderManager->isTimeForClassUnloading(env);
			break;
		default:
			break;
	}

	if (_extensions->runtimeCheckDynamicClassUnloading) {
		/* request collector enter classUnloadMutex if possible (if forceUnloading is set - always)*/
		_extensions->runtimeCheckDynamicClassUnloading = enterClassUnloadMutex(env, forceUnloading);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
}

void
MM_GlobalCollectorDelegate::masterThreadGarbageCollectFinished(MM_EnvironmentBase *env, bool compactedThisCycle)
{
	/* Check that all reference object lists are empty:
	 * lists must be processed at Mark and nothing should be flushed after
	 */
	UDATA listCount = _extensions->gcThreadCount;
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(_extensions->heap->getHeapRegionManager());
	while(NULL != (region = regionIterator.nextRegion())) {
		/* check all lists for regions, they should be empty */
		MM_HeapRegionDescriptorStandardExtension *regionExtension =  MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (UDATA i = 0; i < listCount; i++) {
			MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[i];
			Assert_MM_true(list->isWeakListEmpty());
			Assert_MM_true(list->isSoftListEmpty());
			Assert_MM_true(list->isPhantomListEmpty());
		}
	}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_MarkingDelegate::clearClassLoadersScannedFlag(env);

	/* If we allowed class unloading during this gc, we must release the classUnloadMutex */
	if (_extensions->runtimeCheckDynamicClassUnloading) {
		exitClassUnloadMutex(env);
	}

	/* make sure that we are going to get at least some number of bytes back since this will otherwise waste time in monitor operations and potentially get exclusive in order to do nothing */
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	uintptr_t reclaimableMemory = _extensions->classLoaderManager->reclaimableMemory();
	if (reclaimableMemory > 0) {
		if (!compactedThisCycle) {
			bool isExplicitGC = env->_cycleState->_gcCode.isExplicitGC();
			if (isExplicitGC || (reclaimableMemory > _extensions->deadClassLoaderCacheSize)) {
				/* fix the heap */
				Trc_MM_DoFixHeapForUnload_Entry(vmThread, MEMORY_TYPE_RAM);
				MM_ParallelGlobalGC *parallelGlobalCollector = (MM_ParallelGlobalGC *)_globalCollector;
				UDATA fixedObjectCount = parallelGlobalCollector->fixHeapForWalk(env, MEMORY_TYPE_RAM, FIXUP_CLASS_UNLOADING, fixObjectIfClassDying);
				if (0 < fixedObjectCount) {
					Trc_MM_DoFixHeapForUnload_Exit(vmThread, fixedObjectCount);
				} else {
					Trc_MM_DoFixHeapForUnload_ExitNotNeeded(vmThread);
				}
				/* now flush the cached segments */
				Trc_MM_FlushUndeadSegments_Entry(vmThread, isExplicitGC ? "SystemGC" : "Dead Class Loader Cache Full");
				_extensions->classLoaderManager->flushUndeadSegments(env);
				Trc_MM_FlushUndeadSegments_Exit(vmThread);
			}
		} else {
#if defined(J9VM_GC_MODRON_COMPACTION)
			Trc_MM_FlushUndeadSegments_Entry(vmThread, "Compaction");
			_extensions->classLoaderManager->flushUndeadSegments(env);
			Trc_MM_FlushUndeadSegments_Exit(vmThread);
#else
			Assert_MM_unreachable();
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
		}
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
}

void
MM_GlobalCollectorDelegate::postMarkProcessing(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if (_extensions->runtimeCheckDynamicClassUnloading != 0) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		OMR_VMThread *vmThread = env->getOmrVMThread();
		Trc_MM_ClassUnloadingStart((J9VMThread *)vmThread->_language_vmthread);
		TRIGGER_J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START(
			_extensions->privateHookInterface,
			vmThread,
			j9time_hires_clock(),
			J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START);

		unloadDeadClassLoaders(env);

		MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
		Trc_MM_ClassUnloadingEnd((J9VMThread *)vmThread->_language_vmthread,
								classUnloadStats->_classLoaderUnloadedCount,
								classUnloadStats->_classesUnloadedCount);
		TRIGGER_J9HOOK_MM_CLASS_UNLOADING_END(
			_extensions->hookInterface,
			(J9VMThread *)vmThread->_language_vmthread,
			j9time_hires_clock(),
			J9HOOK_MM_CLASS_UNLOADING_END,
			classUnloadStats->_endTime - classUnloadStats->_startTime,
			classUnloadStats->_classLoaderUnloadedCount,
			classUnloadStats->_classesUnloadedCount,
			classUnloadStats->_classUnloadMutexQuiesceTime,
			classUnloadStats->_endSetupTime - classUnloadStats->_startSetupTime,
			classUnloadStats->_endScanTime - classUnloadStats->_startScanTime,
			classUnloadStats->_endPostTime - classUnloadStats->_startPostTime);

		/* If there was dynamic class unloading checks during the run, record the new number of class
		 * loaders last seen during a DCU pass
		 */
		_extensions->classLoaderManager->setLastUnloadNumOfClassLoaders();
		_extensions->classLoaderManager->setLastUnloadNumOfAnonymousClasses();
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

#if defined(J9VM_GC_FINALIZATION)
	if (_finalizationRequired) {
		/* Signal the finalizer */
		omrthread_monitor_enter(_javaVM->finalizeMasterMonitor);
		_javaVM->finalizeMasterFlags |= J9_FINALIZE_FLAGS_MASTER_WAKE_UP;
		omrthread_monitor_notify_all(_javaVM->finalizeMasterMonitor);
		omrthread_monitor_exit(_javaVM->finalizeMasterMonitor);
	}
#endif /* J9VM_GC_FINALIZATION */
}

bool
MM_GlobalCollectorDelegate::isAllowUserHeapWalk()
{
	/* Enable  only if required for debugging. */
	return (0 != (_javaVM->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK));
}

void
MM_GlobalCollectorDelegate::prepareHeapForWalk(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/* Clear the appropriate flags of all classLoaders */
	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	J9ClassLoader *classLoader;
	while((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		classLoader->gcFlags &= ~J9_GC_CLASS_LOADER_SCANNED;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
}

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
void
MM_GlobalCollectorDelegate::poisonSlots(MM_EnvironmentBase *env)
{
	((MM_ReadBarrierVerifier *)_extensions->accessBarrier)->poisonSlots(env);
}

void
MM_GlobalCollectorDelegate::healSlots(MM_EnvironmentBase *env)
{
	((MM_ReadBarrierVerifier *)_extensions->accessBarrier)->healSlots(env);
}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

bool
MM_GlobalCollectorDelegate::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress)
{
	if(NULL != _extensions->referenceChainWalkerMarkMap) {
		return _extensions->referenceChainWalkerMarkMap->heapAddRange(env, size, lowAddress, highAddress);
	}
	return true;
}

bool
MM_GlobalCollectorDelegate::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	if (NULL != _extensions->referenceChainWalkerMarkMap) {
		return _extensions->referenceChainWalkerMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return true;
}

bool
MM_GlobalCollectorDelegate::isTimeForGlobalGCKickoff()
{
	bool result = false;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	uintptr_t numClassLoaderBlocks = pool_numElements(_javaVM->classLoaderBlocks);
	uintptr_t numAnonymousClasses = _javaVM->anonClassCount;

	Trc_MM_GlobalCollector_isTimeForGlobalGCKickoff_Entry(
		_extensions->dynamicClassUnloading,
		numClassLoaderBlocks,
		_extensions->dynamicClassUnloadingKickoffThreshold,
		_extensions->classLoaderManager->getLastUnloadNumOfClassLoaders());

	Trc_MM_GlobalCollector_isTimeForGlobalGCKickoff_anonClasses(
		numAnonymousClasses,
		_extensions->classLoaderManager->getLastUnloadNumOfAnonymousClasses(),
		_extensions->classUnloadingAnonymousClassWeight
	);

	Assert_MM_true(numAnonymousClasses >= _extensions->classLoaderManager->getLastUnloadNumOfAnonymousClasses());

	if ((0 != _extensions->dynamicClassUnloadingKickoffThreshold) && (_extensions->dynamicClassUnloading != MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_NEVER)) {
		uintptr_t recentlyLoaded = (uintptr_t) ((numAnonymousClasses - _extensions->classLoaderManager->getLastUnloadNumOfAnonymousClasses()) * _extensions->classUnloadingAnonymousClassWeight);
		/* todo aryoung: getLastUnloadNumOfClassLoaders() includes the class loaders which
		 * were unloaded but still required finalization when the last classUnloading occured.
		 * This means that the threshold check is wrong when there are classes which require finalization.
		 * Temporarily make sure that we do not create a negative recently loaded.
		 */
		if (numClassLoaderBlocks > _extensions->classLoaderManager->getLastUnloadNumOfClassLoaders()) {
			recentlyLoaded += (numClassLoaderBlocks - _extensions->classLoaderManager->getLastUnloadNumOfClassLoaders());
		}
		result = recentlyLoaded >= _extensions->dynamicClassUnloadingKickoffThreshold;
	}

	Trc_MM_GlobalCollector_isTimeForGlobalGCKickoff_Exit(result ? "true" : "false");
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	return result;
}

void
MM_GlobalCollectorDelegate::postCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace)
{
	/* update the dynamic soft reference age based on the free space in the oldest generation after this collection so we know how many to clear next time */
	MM_Heap* heap = (MM_Heap*)_extensions->heap;
	uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	uintptr_t freeSize = heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
	double percentFree = ((double)freeSize) / ((double)heapSize);
	_extensions->dynamicMaxSoftReferenceAge = (uintptr_t)(percentFree * (double)(_extensions->maxSoftReferenceAge));
	Assert_MM_true(_extensions->dynamicMaxSoftReferenceAge <= _extensions->maxSoftReferenceAge);
}

#if defined(J9VM_GC_MODRON_COMPACTION)
CompactPreventedReason
MM_GlobalCollectorDelegate::checkIfCompactionShouldBePrevented(MM_EnvironmentBase *env)
{
	CompactPreventedReason reason = COMPACT_PREVENTED_NONE;
	/* If there are active JNI critical regions then objects can't be moved. */

	if (0 < _criticalSectionCount) {
		reason = COMPACT_PREVENTED_CRITICAL_REGIONS;
	}

	return reason;
}
#endif /* J9VM_GC_MODRON_COMPACTION */


#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
bool
MM_GlobalCollectorDelegate::enterClassUnloadMutex(MM_EnvironmentBase *env, bool force)
{
	bool result = true;

	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
	if (force) {
		classUnloadStats->_classUnloadMutexQuiesceTime = _extensions->classLoaderManager->enterClassUnloadMutex(env);
	} else {
		classUnloadStats->_classUnloadMutexQuiesceTime = J9CONST64(0);
		result = _extensions->classLoaderManager->tryEnterClassUnloadMutex(env);
	}

	return result;
}

void
MM_GlobalCollectorDelegate::exitClassUnloadMutex(MM_EnvironmentBase *env)
{
	_extensions->classLoaderManager->exitClassUnloadMutex(env);
}

void
MM_GlobalCollectorDelegate::unloadDeadClassLoaders(MM_EnvironmentBase *env)
{
	Trc_MM_ParallelGlobalGC_unloadDeadClassLoaders_entry(env->getLanguageVMThread());
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;

	/* The list of classLoaders to be unloaded by cleanUpClassLoadersEnd is rooted in unloadLink */

	/* set the vmState whilst we're unloading classes */
	UDATA vmState = env->pushVMstate(OMRVMSTATE_GC_CLEANING_METADATA);

	/* Count the classes we're unloading and perform class-specific clean up work for each unloading class.
	 * If we're unloading any classes, perform common class-unloading clean up.
	 */
	classUnloadStats->_startTime = j9time_hires_clock();
	classUnloadStats->_startSetupTime = classUnloadStats->_startTime;

	J9ClassLoader *classLoadersUnloadedList = _extensions->classLoaderManager->identifyClassLoadersToUnload(env, _markingScheme->getMarkMap(), classUnloadStats);
	_extensions->classLoaderManager->cleanUpClassLoadersStart(env, classLoadersUnloadedList, _markingScheme->getMarkMap(), classUnloadStats);

	classUnloadStats->_endSetupTime = j9time_hires_clock();
	classUnloadStats->_startScanTime = classUnloadStats->_endSetupTime;

	/* The list of classLoaders to be unloaded by cleanUpClassLoadersEnd is rooted in unloadLink */
	J9ClassLoader *unloadLink = NULL;
	J9MemorySegment *reclaimedSegments = NULL;
	_extensions->classLoaderManager->cleanUpClassLoaders(env, classLoadersUnloadedList, &reclaimedSegments, &unloadLink, &_finalizationRequired);

	/* Free the class memory segments associated with dead classLoaders, unload (free) the dead classLoaders that don't
	 * require finalization, and perform any final clean up after the dead classLoaders are gone.
	 */
	classUnloadStats->_endScanTime = j9time_hires_clock();
	classUnloadStats->_startPostTime = classUnloadStats->_endScanTime;

	/* enqueue all the segments we just salvaged from the dead class loaders for delayed free (this work was historically attributed in the unload end operation so it goes after the timer start) */
	_extensions->classLoaderManager->enqueueUndeadClassSegments(reclaimedSegments);
	_extensions->classLoaderManager->cleanUpClassLoadersEnd(env, unloadLink);

	classUnloadStats->_endPostTime = j9time_hires_clock();
	classUnloadStats->_endTime = classUnloadStats->_endPostTime;

	env->popVMstate(vmState);

	Trc_MM_ParallelGlobalGC_unloadDeadClassLoaders_exit(env->getLanguageVMThread());
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

