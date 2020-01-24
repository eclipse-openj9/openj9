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
#include "omrcfg.h"
#include "gcutils.h"

#include <string.h>

#include "RealtimeGC.hpp"

#include "AllocateDescription.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentRealtime.hpp"
#include "GlobalAllocationManagerSegregated.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySubSpace.hpp"
#include "modronapicore.hpp"
#include "OMRVMInterface.hpp"
#include "OSInterface.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RealtimeMarkTask.hpp"
#include "RealtimeSweepTask.hpp"
#include "ReferenceChainWalkerMarkMap.hpp"
#include "RememberedSetSATB.hpp"
#include "Scheduler.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SublistFragment.hpp"
#include "SweepSchemeRealtime.hpp"
#include "Task.hpp"
#include "WorkPacketsRealtime.hpp"

/* TuningFork name/version information for gc_staccato */
#define TUNINGFORK_STACCATO_EVENT_SPACE_NAME "com.ibm.realtime.vm.trace.gc.metronome"
#define TUNINGFORK_STACCATO_EVENT_SPACE_VERSION 200

MM_RealtimeGC *
MM_RealtimeGC::newInstance(MM_EnvironmentBase *env)
{
	MM_RealtimeGC *globalGC = (MM_RealtimeGC *)env->getForge()->allocate(sizeof(MM_RealtimeGC), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (globalGC) {
		new(globalGC) MM_RealtimeGC(env);
		if (!globalGC->initialize(env)) {
			globalGC->kill(env);
			globalGC = NULL;
		}
	}
	return globalGC;
}


void
MM_RealtimeGC::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_RealtimeGC::setGCThreadPriority(OMR_VMThread *vmThread, uintptr_t newGCThreadPriority)
{
	if(newGCThreadPriority == (uintptr_t) _currentGCThreadPriority) {
		return;
	}
	
	Trc_MM_GcThreadPriorityChanged(vmThread->_language_vmthread, newGCThreadPriority);
	
	/* Walk through all GC threads and set the priority */
	omrthread_t* gcThreadTable = _sched->getThreadTable();
	for (uintptr_t i = 0; i < _sched->threadCount(); i++) {
		omrthread_set_priority(gcThreadTable[i], newGCThreadPriority);
	}
	_currentGCThreadPriority = (intptr_t) newGCThreadPriority;
}

/**
 * Initialization.
 */
bool
MM_RealtimeGC::initialize(MM_EnvironmentBase *env)
{
	_gcPhase = GC_PHASE_IDLE;
	_extensions->realtimeGC = this;
	_allowGrowth = false;
	
	if (_extensions->gcTrigger == 0) {
		_extensions->gcTrigger = (_extensions->memoryMax / 2);
		_extensions->gcInitialTrigger = (_extensions->memoryMax / 2);
	}

	_extensions->distanceToYieldTimeCheck = 0;

	/* Only SRT passes this check as the commandline option to specify beatMicro is only enabled on SRT */
	if (METRONOME_DEFAULT_BEAT_MICRO != _extensions->beatMicro) {
		/* User-specified quanta time, adjust related parameters */
		_extensions->timeWindowMicro = 20 * _extensions->beatMicro;
		/* Currently all supported SRT platforms - AIX and Linux, can only use HRT for alarm thread implementation.
		 * The default value for HRT period is 1/3 of the default quanta: 1 msec for HRT period and 3 msec quanta,
		 * we will attempt to adjust the HRT period to 1/3 of the specified quanta.
		 */
		uintptr_t hrtPeriodMicro = _extensions->beatMicro / 3;
		if ((hrtPeriodMicro < METRONOME_DEFAULT_HRT_PERIOD_MICRO) && (METRONOME_DEFAULT_HRT_PERIOD_MICRO < _extensions->beatMicro)) {
			/* If the adjusted value is too small for the hires clock resolution, we will use the default HRT period provided that
			 * the default period is smaller than the quanta time specified.
			 * Otherwise we fail to initialize the alarm thread with an error message.
			 */
			hrtPeriodMicro = METRONOME_DEFAULT_HRT_PERIOD_MICRO;
		}
		Assert_MM_true(0 != hrtPeriodMicro);
		_extensions->hrtPeriodMicro = hrtPeriodMicro;

		/* On Windows SRT we still use interrupt-based alarm. Set the interrupt period the same as hires timer period.
		 * We will fail to init the alarm if this is too small a resolution for Windows.
		 */
		_extensions->itPeriodMicro = _extensions->hrtPeriodMicro;

		/* if the pause time user specified is larger than the default value, calculate if there is opportunity
		 * for the GC to do time checking less often inside condYieldFromGC.
		 */
		if (METRONOME_DEFAULT_BEAT_MICRO < _extensions->beatMicro) {
			uintptr_t intervalToSkipYieldCheckMicro = _extensions->beatMicro - METRONOME_DEFAULT_BEAT_MICRO;
			uintptr_t maxInterYieldTimeMicro = INTER_YIELD_MAX_NS / 1000;
			_extensions->distanceToYieldTimeCheck = (U_32)(intervalToSkipYieldCheckMicro / maxInterYieldTimeMicro);
		}
	}

	_osInterface = MM_OSInterface::newInstance(env);
	if (_osInterface == NULL) {
		return false;
	}
	
	_sched = (MM_Scheduler *)_extensions->dispatcher;

	_workPackets = allocateWorkPackets(env);	
	if (_workPackets == NULL) {
		return false;
	}

	_markingScheme = MM_RealtimeMarkingScheme::newInstance(env, this);
	if (NULL == _markingScheme) {
		return false;
	}
	
	if (!_delegate.initialize(env, NULL, NULL)) {
		return false;
	}

	_sweepScheme = MM_SweepSchemeRealtime::newInstance(env, this, _sched, _markingScheme->getMarkMap());
	if(NULL == _sweepScheme) {
		return false;
	}

	if (!_realtimeDelegate.initialize(env)) {
		return false;
	}
	
 	_extensions->sATBBarrierRememberedSet = MM_RememberedSetSATB::newInstance(env, _workPackets);
 	if (NULL == _extensions->sATBBarrierRememberedSet) {
 		return false;
 	}

	_stopTracing = false;

	_sched->collectorInitialized(this);

	return true;
}

/**
 * Initialization.
 */
void
MM_RealtimeGC::tearDown(MM_EnvironmentBase *env)
{
	_delegate.tearDown(env);
	_realtimeDelegate.tearDown(env);

	if(NULL != _sched) {
		_sched->kill(env);
		_sched = NULL;
	}
	
	if(NULL != _osInterface) {
		_osInterface->kill(env);
		_osInterface = NULL;
	}
	
	if(NULL != _workPackets) {
		_workPackets->kill(env);
		_workPackets = NULL;
	}

	if (NULL != _markingScheme) {
		_markingScheme->kill(env);
		_markingScheme = NULL;
 	}
	
	if (NULL != _sweepScheme) {
		_sweepScheme->kill(env);
		_sweepScheme = NULL;
 	}

	if (NULL != _extensions->sATBBarrierRememberedSet) {
		_extensions->sATBBarrierRememberedSet->kill(env);
		_extensions->sATBBarrierRememberedSet = NULL;
	}
}

/**
 * @copydoc MM_GlobalCollector::masterSetupForGC()
 */
void
MM_RealtimeGC::masterSetupForGC(MM_EnvironmentBase *env)
{
	/* Reset memory pools of associated memory spaces */
	env->_cycleState->_activeSubSpace->reset();
	
	_workPackets->reset(env);	
	
	/* Clear the gc stats structure */
	clearGCStats();

	_realtimeDelegate.masterSetupForGC(env);
}

/**
 * @copydoc MM_GlobalCollector::masterCleanupAfterGC()
 */
void
MM_RealtimeGC::masterCleanupAfterGC(MM_EnvironmentBase *env)
{
	_realtimeDelegate.masterCleanupAfterGC(env);
}

/**
 * Thread initialization.
 */
void
MM_RealtimeGC::workerSetupForGC(MM_EnvironmentBase *env)
{	
}

/**
 */
void
MM_RealtimeGC::clearGCStats()
{
	_extensions->globalGCStats.clear();
	_realtimeDelegate.clearGCStats();
}

/**
 */
void
MM_RealtimeGC::mergeGCStats(MM_EnvironmentBase *env)
{
}

uintptr_t
MM_RealtimeGC::verbose(MM_EnvironmentBase *env) {
	return _sched->verbose();
}

/**
 * @note only called by master thread.
 */
void
MM_RealtimeGC::doAuxiliaryGCWork(MM_EnvironmentBase *env)
{
	_realtimeDelegate.doAuxiliaryGCWork(env);
	
	/* Restart the caches for all threads. */
	GC_OMRVMThreadListIterator vmThreadListIterator(_vm);
	OMR_VMThread *walkThread;
	while((walkThread = vmThreadListIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread);
		((MM_SegregatedAllocationInterface *)(walkEnv->_objectAllocationInterface))->restartCache(walkEnv);
	}
	
	mergeGCStats(env);
}

/**
 * Incremental Collector.
 * Employs a double write barrier that saves overwriting (new) values from unscanned threads and
 * also the first (old) value overwritten by all threads (the latter as in a Yuasa barrier).
 * @note only called by master thread.
 */
void
MM_RealtimeGC::incrementalCollect(MM_EnvironmentRealtime *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	masterSetupForGC(env);

	_realtimeDelegate.incrementalCollectStart(env);

	/* Make sure all threads notice GC is ongoing with a barrier. */
	_extensions->globalGCStats.gcCount++;
	if (verbose(env) >= 2) {
		omrtty_printf("RealtimeGC::incrementalCollect\n");
	}
	if (verbose(env) >= 3) {
		omrtty_printf("RealtimeGC::incrementalCollect   setup and root phase\n");
	}
	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_soft_as_weak;
	}

	setCollectorRootMarking();
	
	reportMarkStart(env);
	MM_RealtimeMarkTask markTask(env, _sched, this, _markingScheme, env->_cycleState);
	_sched->run(env, &markTask);
	reportMarkEnd(env);
	

	_realtimeDelegate.incrementalCollect(env);

	/*
	 * Sweeping.
	 */
	reportSweepStart(env);
	MM_RealtimeSweepTask sweepTask(env, _sched, _sweepScheme);
	_sched->run(env, &sweepTask);
	reportSweepEnd(env);

	doAuxiliaryGCWork(env);

	/* Get all components to clean up after themselves at the end of a collect */
	masterCleanupAfterGC(env);

	_sched->condYieldFromGC(env);
	setCollectorIdle();

	if (verbose(env) >= 3) {
		omrtty_printf("RealtimeGC::incrementalCollect   gc complete  %d  MB in use\n", _memoryPool->getBytesInUse() >> 20);
	}
}

void
MM_RealtimeGC::flushCachedFullRegions(MM_EnvironmentBase *env)
{
	/* delegate to the memory pool to perform the flushing of per-context full regions to the region pool */
	_memoryPool->flushCachedFullRegions(env);
}

/**
 * This function is called at the end of tracing when it is safe for threads to stop
 * allocating black and return to allocating white.  It iterates through all the threads
 * and sets their allocationColor to GC_UNMARK.  It also sets the new thread allocation
 * color to GC_UNMARK.
 **/
void
MM_RealtimeGC::allThreadsAllocateUnmarked(MM_EnvironmentBase *env) {
	GC_OMRVMInterface::flushCachesForGC(env);
	GC_OMRVMThreadListIterator vmThreadListIterator(_vm);

	while(OMR_VMThread *aThread = vmThreadListIterator.nextOMRVMThread()) {
		MM_EnvironmentRealtime *threadEnv = MM_EnvironmentRealtime::getEnvironment(aThread);	
		assume0(threadEnv->getAllocationColor() == GC_MARK);
		threadEnv->setAllocationColor(GC_UNMARK);
		threadEnv->setMonitorCacheCleared(FALSE);
	}
	_extensions->newThreadAllocationColor = GC_UNMARK;
}

/****************************************
 * VM Garbage Collection API
 ****************************************
 */
/**
 */
void
MM_RealtimeGC::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, U_32 gcCode)
{
	/* Setup the master thread cycle state */
	_cycleState = MM_CycleState();
	env->_cycleState = &_cycleState;
	env->_cycleState->_gcCode = MM_GCCode(gcCode);
	env->_cycleState->_type = _cycleType;
	env->_cycleState->_activeSubSpace = subSpace;

	/* If we are in an excessiveGC level beyond normal then an aggressive GC is
	 * conducted to free up as much space as possible
	 */
	if (!env->_cycleState->_gcCode.isExplicitGC()) {
		if(excessive_gc_normal != _extensions->excessiveGCLevel) {
			/* convert the current mode to excessive GC mode */
			env->_cycleState->_gcCode = MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE);
		}
	}

	/* The minimum free entry size is always re-adjusted at the end of a cycle.
	 * But if the current cycle is triggered due to OOM, at the start of the cycle
	 * set the minimum free entry size to the smallest size class - 16 bytes.
	 */
	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
		_memoryPool->setMinimumFreeEntrySize((1 << J9VMGC_SIZECLASSES_LOG_SMALLEST));
	}

	MM_EnvironmentRealtime *rtEnv = MM_EnvironmentRealtime::getEnvironment(env);
	/* Having heap walkable after the end of GC may be explicitly required through command line option or GC Check*/
	if (rtEnv->getExtensions()->fixHeapForWalk) {
		_fixHeapForWalk = true;
	}
	/* we are about to collect so generate the appropriate cycle start and increment start events */
	reportGCCycleStart(rtEnv);
	_sched->reportStartGCIncrement(rtEnv);
}

/**
 */
void
MM_RealtimeGC::setupForGC(MM_EnvironmentBase *env)
{
}	

/**
 * @note only called by master thread.
 */
bool
MM_RealtimeGC::internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);

	incrementalCollect(envRealtime);

	_extensions->heap->resetHeapStatistics(true);
	
	return true;
}

void
MM_RealtimeGC::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	MM_GlobalCollector::internalPostCollect(env, subSpace);
	
	/* Reset fixHeapForWalk for the next cycle, no matter who set it */
	_fixHeapForWalk = false;
	
	/* Check if user overrode the default minimumFreeEntrySize */
	if (_extensions->minimumFreeEntrySize != UDATA_MAX) {
		_memoryPool->setMinimumFreeEntrySize(_extensions->minimumFreeEntrySize);
	} else {
		/* Set it dynamically based on free heap after the end of collection */
		float percentFreeHeapAfterCollect = _extensions->heap->getApproximateActiveFreeMemorySize() * 100.0f / _extensions->heap->getMaximumMemorySize();
		_avgPercentFreeHeapAfterCollect = _avgPercentFreeHeapAfterCollect * 0.8f + percentFreeHeapAfterCollect * 0.2f;
		/* Has percent range changed? (for example from [80,90] down to [70,80]) */
		uintptr_t minFreeEntrySize = (uintptr_t)1 << (((uintptr_t)_avgPercentFreeHeapAfterCollect / 10) + 1);
		if (minFreeEntrySize != _memoryPool->getMinimumFreeEntrySize()) {
			/* Yes, it did => make sure it changed enough (more than 1% up or below the range boundary) to accept it (in the example, 78.9 is ok, but 79.1 is not */
			if ((uintptr_t)_avgPercentFreeHeapAfterCollect % 10 >= 1 && (uintptr_t)_avgPercentFreeHeapAfterCollect % 10 < 9) {
				if (minFreeEntrySize < 16) {
					minFreeEntrySize = 0;
				}
				_memoryPool->setMinimumFreeEntrySize(minFreeEntrySize);
			}
		}
	}

	/*
	 * MM_GC_CYCLE_END is hooked by external components (e.g. JIT), which may cause GC to yield while in the
	 * external callback. Yielding introduces additional METRONOME_INCREMENT_STOP/START verbose events, which must be
	 * processed before the very last METRONOME_INCREMENT_STOP event before the PRIVATE_GC_POST_CYCLE_END event. Otherwise
	 * the METRONOME_INCREMENT_START/END events become out of order and verbose GC will fail.
	 */
	reportGCCycleFinalIncrementEnding(env);

	MM_EnvironmentRealtime *rtEnv = MM_EnvironmentRealtime::getEnvironment(env);
	_sched->reportStopGCIncrement(rtEnv, true);
	_sched->setGCCode(MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_DEFAULT));
	reportGCCycleEnd(rtEnv);
	/*
	 * We could potentially yield during reportGCCycleEnd (e.g. due to JIT callbacks) and the scheduler will only wake up the master if _gcOn is true.
	 * Turn off _gcOn flag at the very last, after cycle end has been reported.
	 */
	_sched->stopGC(rtEnv);
	env->_cycleState->_activeSubSpace = NULL;
}

void
MM_RealtimeGC::reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	MM_CommonGCData commonData;
	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		omrgc_condYieldFromGC
	);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportSyncGCStart(MM_EnvironmentBase *env, GCReason reason, uintptr_t reasonParameter)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uintptr_t approximateFreeFreeMemorySize;
#if defined(OMR_GC_DYNAMIC_CLASS_UNLOADING)
	MM_ClassUnloadStats *classUnloadStats = &_extensions->globalGCStats.classUnloadStats;
#endif /* defined(OMR_GC_DYNAMIC_CLASS_UNLOADING) */
	
	approximateFreeFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize();
	
	Trc_MM_SynchGCStart(env->getLanguageVMThread(),
		reason,
		getGCReasonAsString(reason),
		reasonParameter,
		approximateFreeFreeMemorySize,
		0
	);

#if defined(OMR_GC_DYNAMIC_CLASS_UNLOADING)
	uintptr_t classLoaderUnloadedCount = isCollectorIdle()?0:classUnloadStats->_classLoaderUnloadedCount;
	uintptr_t classesUnloadedCount = isCollectorIdle()?0:classUnloadStats->_classesUnloadedCount;
	uintptr_t anonymousClassesUnloadedCount = isCollectorIdle()?0:classUnloadStats->_anonymousClassesUnloadedCount;
#else /* defined(OMR_GC_DYNAMIC_CLASS_UNLOADING) */
	uintptr_t classLoaderUnloadedCount = 0;
	uintptr_t classesUnloadedCount = 0;
	uintptr_t anonymousClassesUnloadedCount = 0;
#endif /* defined(OMR_GC_DYNAMIC_CLASS_UNLOADING) */
	
	/* If master thread was blocked at end of GC, waiting for a new GC cycle,
	 * globalGCStats are not cleared yet. Thus, if we haven't started GC yet,
	 * just report 0s for classLoaders unloaded count */
	TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START(_extensions->privateHookInterface,
		env->getOmrVMThread(), omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, reason, reasonParameter,
		approximateFreeFreeMemorySize,
		0,
		classLoaderUnloadedCount,
		classesUnloadedCount,
		anonymousClassesUnloadedCount
	);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportSyncGCEnd(MM_EnvironmentBase *env)
{
	_realtimeDelegate.reportSyncGCEnd(env);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportGCCycleStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	/* Let VM know that GC cycle is about to start. JIT, in particular uses it,
	 * to not compile while GC cycle is on.
	 */
	omrthread_monitor_enter(env->getOmrVM()->_gcCycleOnMonitor);
	env->getOmrVM()->_gcCycleOn = 1;

	uintptr_t approximateFreeMemorySize = _memoryPool->getApproximateFreeMemorySize();

	Trc_MM_CycleStart(env->getLanguageVMThread(), env->_cycleState->_type, approximateFreeMemorySize);

	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_START,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type
	);
	omrthread_monitor_exit(env->getOmrVM()->_gcCycleOnMonitor);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_RealtimeGC::reportGCCycleEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	omrthread_monitor_enter(env->getOmrVM()->_gcCycleOnMonitor);

	uintptr_t approximateFreeMemorySize = _memoryPool->getApproximateFreeMemorySize();

	Trc_MM_CycleEnd(env->getLanguageVMThread(), env->_cycleState->_type, approximateFreeMemorySize);

	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		_extensions->globalGCStats.fixHeapForWalkReason,
		_extensions->globalGCStats.fixHeapForWalkTime
	);

	/* If GC cycle just finished, and trigger start was previously generated, generate trigger end now */
	if (_memoryPool->getBytesInUse() < _extensions->gcInitialTrigger) {
		_previousCycleBelowTrigger = true;
		TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END(_extensions->privateHookInterface,
			env->getOmrVMThread(), omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END
		);
	}
	
	/* Let VM (JIT, in particular) GC cycle is finished. Do a monitor notify, to
	 * unblock parties that waited for the cycle to complete
	 */
	env->getOmrVM()->_gcCycleOn = 0;
	omrthread_monitor_notify_all(env->getOmrVM()->_gcCycleOnMonitor);
	
	omrthread_monitor_exit(env->getOmrVM()->_gcCycleOnMonitor);
}

/**
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
bool
MM_RealtimeGC::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool result = _markingScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);

	if (result) {
		if(NULL != _extensions->referenceChainWalkerMarkMap) {
			result = _extensions->referenceChainWalkerMarkMap->heapAddRange(env, size, lowAddress, highAddress);
			if (!result) {
				/* Expansion of Reference Chain Walker Mark Map has failed
				 * Marking Scheme expansion must be reversed
				 */
				_markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
			}
		}
	}
	return result;
}

/**
 */
bool
MM_RealtimeGC::heapRemoveRange(
	MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress,
	void *lowValidAddress, void *highValidAddress)
{
	bool result = _markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	if(NULL != _extensions->referenceChainWalkerMarkMap) {
		result = result && _extensions->referenceChainWalkerMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return result;
}

/**
 */
bool
MM_RealtimeGC::collectorStartup(MM_GCExtensionsBase* extensions)
{
	((MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager)->setSweepScheme(_sweepScheme);
	((MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager)->setMarkingScheme(_markingScheme);
	return true;
}

/**
 */	
void
MM_RealtimeGC::collectorShutdown(MM_GCExtensionsBase *extensions)
{
}

/**
 * Factory method for creating the work packets structure.
 * 
 * @return the WorkPackets to be used for this Collector.
 */
MM_WorkPacketsRealtime*
MM_RealtimeGC::allocateWorkPackets(MM_EnvironmentBase *env)
{
	return MM_WorkPacketsRealtime::newInstance(env);
}

/**
 * Calls the Scheduler's yielding API to determine if the GC should yield.
 * @return true if the GC should yield, false otherwise
 */
bool
MM_RealtimeGC::shouldYield(MM_EnvironmentBase *env)
{
	return _sched->shouldGCYield(MM_EnvironmentRealtime::getEnvironment(env), 0);
}

/**
 * Yield from GC by calling the Scheduler's API.
 */
void
MM_RealtimeGC::yield(MM_EnvironmentBase *env)
{
	_sched->yieldFromGC(MM_EnvironmentRealtime::getEnvironment(env));
}

/**
 * Yield only if the Scheduler deems yielding should occur at the time of the
 * call to this method.
 */
bool
MM_RealtimeGC::condYield(MM_EnvironmentBase *env, U_64 timeSlackNanoSec)
{
	return _sched->condYieldFromGC(MM_EnvironmentRealtime::getEnvironment(env), timeSlackNanoSec);
}

bool
MM_RealtimeGC::isMarked(void *objectPtr)
{
	return _markingScheme->isMarked((omrobjectptr_t)(objectPtr));
}

void
MM_RealtimeGC::reportMarkStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_START);
}

void
MM_RealtimeGC::reportMarkEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkEnd(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_END);
}

void
MM_RealtimeGC::reportSweepStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_START);
}

void
MM_RealtimeGC::reportSweepEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepEnd(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_END);
}

void
MM_RealtimeGC::reportGCStart(MM_EnvironmentBase *env)
{
	uintptr_t scavengerCount = 0;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_GlobalGCStart(env->getLanguageVMThread(), _extensions->globalGCStats.gcCount);

	TRIGGER_J9HOOK_MM_OMR_GLOBAL_GC_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GLOBAL_GC_START,
		_extensions->globalGCStats.gcCount,
		scavengerCount,
		env->_cycleState->_gcCode.isExplicitGC() ? 1 : 0,
		env->_cycleState->_gcCode.isAggressiveGC() ? 1: 0,
		_bytesRequested);
}

void
MM_RealtimeGC::reportGCEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uintptr_t approximateNewActiveFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW);
	uintptr_t newActiveMemorySize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW);
	uintptr_t approximateOldActiveFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
	uintptr_t oldActiveMemorySize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	uintptr_t approximateLoaActiveFreeMemorySize = (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 );
	uintptr_t loaActiveMemorySize = (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 );

	/* not including LOA in total (already accounted by OLD */
	uintptr_t approximateTotalActiveFreeMemorySize = approximateNewActiveFreeMemorySize + approximateOldActiveFreeMemorySize;
	uintptr_t totalActiveMemorySizeTotal = newActiveMemorySize + oldActiveMemorySize;


	Trc_MM_GlobalGCEnd(env->getLanguageVMThread(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		approximateTotalActiveFreeMemorySize,
		totalActiveMemorySizeTotal
	);

	/* these are assigned to temporary variable out-of-line since some preprocessors get confused if you have directives in macros */
	uintptr_t approximateActiveFreeMemorySize = 0;
	uintptr_t activeMemorySize = 0;

	TRIGGER_J9HOOK_MM_OMR_GLOBAL_GC_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GLOBAL_GC_END,
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		approximateNewActiveFreeMemorySize,
		newActiveMemorySize,
		approximateOldActiveFreeMemorySize,
		oldActiveMemorySize,
		(_extensions-> largeObjectArea ? 1 : 0),
		approximateLoaActiveFreeMemorySize,
		loaActiveMemorySize,
		/* We can't just ask the heap for everything of type FIXED, because that includes scopes as well */
		approximateActiveFreeMemorySize,
		activeMemorySize,
		_extensions->globalGCStats.fixHeapForWalkReason,
		_extensions->globalGCStats.fixHeapForWalkTime
	);
}

/**
 * Enables the write barrier, this should be called at the beginning of the mark phase.
 */
void
MM_RealtimeGC::enableWriteBarrier(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	extensions->sATBBarrierRememberedSet->restoreGlobalFragmentIndex(env);
}

/**
 * Disables the write barrier, this should be called at the end of the mark phase.
 */
void
MM_RealtimeGC::disableWriteBarrier(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	extensions->sATBBarrierRememberedSet->preserveGlobalFragmentIndex(env);
}

void
MM_RealtimeGC::flushRememberedSet(MM_EnvironmentRealtime *env)
{
	if (_workPackets->inUsePacketsAvailable(env)) {
		_workPackets->moveInUseToNonEmpty(env);
		_extensions->sATBBarrierRememberedSet->flushFragments(env);
	}
}

/**
 * Perform the tracing phase.  For tracing to be complete the work stack and rememberedSet
 * have to be empty and class tracing has to complete without marking any objects.
 * 
 * If concurrentMarkingEnabled is true then tracing is completed concurrently.
 */
void
MM_RealtimeGC::completeMarking(MM_EnvironmentRealtime *env)
{
	
	do {
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			flushRememberedSet(env);
			if (_extensions->concurrentTracingEnabled) {
				setCollectorConcurrentTracing();
				_realtimeDelegate.releaseExclusiveVMAccess(env, _sched->_exclusiveVMAccessRequired);
			} else {
				setCollectorTracing();
			}
					
			_moreTracingRequired = false;
			
			/* From this point on the Scheduler collaborates with WorkPacketsRealtime on yielding.
			 * Strictly speaking this should be done first thing in incrementalCompleteScan().
			 * However, it would require another synchronizeGCThreadsAndReleaseMaster barrier.
			 * So we are just reusing the existing one.
			 */
			_sched->pushYieldCollaborator(_workPackets->getYieldCollaborator());
			
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		
		if(_markingScheme->incrementalCompleteScan(env, MAX_UINT)) {
			_moreTracingRequired = true;
		}

		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			/* restore the old Yield Collaborator */
			_sched->popYieldCollaborator();
			
			if (_extensions->concurrentTracingEnabled) {
				_realtimeDelegate.acquireExclusiveVMAccess(env, _sched->_exclusiveVMAccessRequired);
				setCollectorTracing();
			}
			_moreTracingRequired |= _realtimeDelegate.doTracing(env);

			/* the workStack and rememberedSet use the same workPackets
			 * as backing store.  If all packets are empty this means the
			 * workStack and rememberedSet processing are both complete.
			 */
			_moreTracingRequired |= !_workPackets->isAllPacketsEmpty();
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	} while(_moreTracingRequired);
}

void
MM_RealtimeGC::enableDoubleBarrier(MM_EnvironmentBase* env)
{
	_realtimeDelegate.enableDoubleBarrier(env);
}

void
MM_RealtimeGC::disableDoubleBarrierOnThread(MM_EnvironmentBase* env, OMR_VMThread *vmThread)
{
	_realtimeDelegate.disableDoubleBarrierOnThread(env, vmThread);
}

void
MM_RealtimeGC::disableDoubleBarrier(MM_EnvironmentBase* env)
{
	_realtimeDelegate.disableDoubleBarrier(env);
}

