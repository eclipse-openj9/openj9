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
#include "ModronAssertions.h"

#include <string.h>

#include "AtomicOperations.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCCode.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "IncrementalParallelTask.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySubSpaceMetronome.hpp"
#include "Metronome.hpp"
#include "MetronomeAlarmThread.hpp"
#include "MetronomeDelegate.hpp"
#include "RealtimeGC.hpp"
#include "OSInterface.hpp"
#include "Scheduler.hpp"
#include "Timer.hpp"
#include "UtilizationTracker.hpp"

/**
 * Initialization.
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
MM_Scheduler*
MM_Scheduler::newInstance(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize)
{
	MM_Scheduler *scheduler = (MM_Scheduler *)env->getForge()->allocate(sizeof(MM_Scheduler), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (scheduler) {
		new(scheduler) MM_Scheduler(env, handler, handler_arg, defaultOSStackSize);
		if (!scheduler->initialize(env)) {
			scheduler->kill(env);
			scheduler = NULL;
		}
	}
	return scheduler;
}

/**
 * Initialization.
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_Scheduler::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
}

/**
 * Teardown
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_Scheduler::tearDown(MM_EnvironmentBase *env)
{
	if (_masterThreadMonitor) {
		omrthread_monitor_destroy(_masterThreadMonitor);
	}
	if (NULL != _threadResumedTable) {
		env->getForge()->free(_threadResumedTable);
		_threadResumedTable = NULL;
	}
	if (NULL != _utilTracker) {
		_utilTracker->kill(env);
	}
	MM_ParallelDispatcher::kill(env);
}

uintptr_t
MM_Scheduler::getParameter(uintptr_t which, char *keyBuffer, I_32 keyBufferSize, char *valueBuffer, I_32 valueBufferSize)
{
	OMRPORT_ACCESS_FROM_OMRVM(_vm);
	switch (which) {
		case 0:	omrstr_printf(keyBuffer, keyBufferSize, "Verbose Level");
			omrstr_printf(valueBuffer, valueBufferSize, "%d", verbose());
			return 1;
		case 1:
		{
			omrstr_printf(keyBuffer, keyBufferSize, "Scheduling Method");
			I_32 len = (I_32)omrstr_printf(valueBuffer, valueBufferSize, "TIME_BASED with ");
			while (_alarmThread == NULL || _alarmThread->_alarm == NULL) {
				/* Wait for GC to finish initializing */
				omrthread_sleep(100);
			}
			_alarmThread->_alarm->describe(OMRPORTLIB, &valueBuffer[len], valueBufferSize - len);
			return 1;
		}
		case 2:
			omrstr_printf(keyBuffer, keyBufferSize, "Time Window");
			omrstr_printf(valueBuffer, valueBufferSize, "%6.2f ms", window * 1.0e3);
			return 1;
		case 3:
			omrstr_printf(keyBuffer, keyBufferSize, "Target Utilization");
			omrstr_printf(valueBuffer, valueBufferSize, "%4.1f%%", _utilTracker->getTargetUtilization() * 1.0e2);
			return 1;
		case 4:
			omrstr_printf(keyBuffer, keyBufferSize, "Beat Size");
			omrstr_printf(valueBuffer, valueBufferSize, "%4.2f ms", beat * 1.0e3);
			return 1;
		case 5:
			omrstr_printf(keyBuffer, keyBufferSize, "Heap Size");
			omrstr_printf(valueBuffer, valueBufferSize, "%6.2f MB",  ((double)(_extensions->memoryMax)) / (1 << 20));
			return 1;
		case 6:
			omrstr_printf(keyBuffer, keyBufferSize, "GC Trigger");
			omrstr_printf(valueBuffer, valueBufferSize, "%6.2f MB",  _extensions->gcTrigger / (double) (1<<20));
			return 1;
		case 7:
			omrstr_printf(keyBuffer, keyBufferSize, "Headroom");
			omrstr_printf(valueBuffer, valueBufferSize, "%5.2f MB",  _extensions->headRoom / (double) (1<<20));
			return 1;
		case 8:
			omrstr_printf(keyBuffer, keyBufferSize, "Number of GC Threads");
			omrstr_printf(valueBuffer, valueBufferSize, "%d", _extensions->gcThreadCount);
			return 1;
		case 9:
			omrstr_printf(keyBuffer, keyBufferSize, "Regionsize");
			omrstr_printf(valueBuffer, valueBufferSize, "%d", _extensions->regionSize);
			return 1;
	}
	return 0;
}

void 
MM_Scheduler::showParameters(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	omrtty_printf("****************************************************************************\n");
	for (uintptr_t which=0; ; which++) {
		char keyBuffer[256], valBuffer[256];
		uintptr_t rc = getParameter(which, keyBuffer, sizeof(keyBuffer), valBuffer, sizeof(valBuffer));
		if (rc == 0) { break; }
		if (rc == 1) { omrtty_printf("%s: %s\n", keyBuffer, valBuffer); }
	}
	omrtty_printf("****************************************************************************\n");
}

void 
MM_Scheduler::initializeForVirtualSTW(MM_GCExtensionsBase *ext)
{
	ext->gcInitialTrigger = (uintptr_t) - 1;
 	ext->gcTrigger = ext->gcInitialTrigger;
 	ext->targetUtilizationPercentage = 0;
}

/**
 * Initialization.
 * @todo Provide method documentation
 * @ingroup GC_Metronome methodGroup
 */
bool
MM_Scheduler::initialize(MM_EnvironmentBase *env)
{
	if (!MM_ParallelDispatcher::initialize(env)) {
		return false;
	}

	/* Show GC parameters here before we enter real execution */
	window = _extensions->timeWindowMicro / 1e6;
	beat = _extensions->beatMicro / 1e6;
	beatNanos = (U_64) (_extensions->beatMicro * 1e3);
	_staticTargetUtilization = _extensions->targetUtilizationPercentage / 1e2;
	_utilTracker = MM_UtilizationTracker::newInstance(env, window, beatNanos, _staticTargetUtilization);
	if (NULL == _utilTracker) {
		goto error_no_memory;
	}

	
	/* Set up the table used for keeping track of which threads were resumed from suspended */
	_threadResumedTable = (bool*)env->getForge()->allocate(_threadCountMaximum * sizeof(bool), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _threadResumedTable) {
		goto error_no_memory;
	}
	memset(_threadResumedTable, false, _threadCountMaximum * sizeof(bool));

	if (omrthread_monitor_init_with_name(&_masterThreadMonitor, 0, "MasterThread")) {
		return false;
	}

	return true;

error_no_memory:
	return false;
}

void
MM_Scheduler::collectorInitialized(MM_RealtimeGC *gc) {
	_gc = gc;
	_osInterface = _gc->_osInterface;
}

void
MM_Scheduler::checkStartGC(MM_EnvironmentRealtime *env)
{
	uintptr_t bytesInUse = _gc->_memoryPool->getBytesInUse();
	if (isInitialized() && !isGCOn() && (bytesInUse > _extensions->gcTrigger)) {
		startGC(env);
	}
}

/* Races with other startGC's are ok
 *
 */
void
MM_Scheduler::startGC(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	if (verbose() >= 3) {
		omrtty_printf("GC request: %d Mb in use\n", _gc->_memoryPool->getBytesInUse() >> 20);
	}

	if (METRONOME_GC_OFF == MM_AtomicOperations::lockCompareExchangeU32(&_gcOn, METRONOME_GC_OFF, METRONOME_GC_ON)) {
		if (_gc->isPreviousCycleBelowTrigger()) {
			_gc->setPreviousCycleBelowTrigger(false);
			TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_START(_extensions->privateHookInterface,
				env->getOmrVMThread(), omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_START
			);
		}
	}
}

/* External synchronization to make sure this does not race with startGC
 */
void
MM_Scheduler::stopGC(MM_EnvironmentBase *env)
{
	_gcOn = METRONOME_GC_OFF;
}

bool
MM_Scheduler::isGCOn()
{
	return (METRONOME_GC_ON == _gcOn);
}

bool
MM_Scheduler::continueGC(MM_EnvironmentRealtime *env, GCReason reason, uintptr_t resonParameter, OMR_VMThread *thr, bool doRequestExclusiveVMAccess)
{
	uintptr_t gcPriority = 0;
	bool didGC = true;

	assert1(isInitialized());
	if (!isGCOn()) {
		return false;
	}

	if (_extensions->trackMutatorThreadCategory) {
		/* This thread is doing GC work, account for the time spent into the GC bucket */
		omrthread_set_category(omrthread_self(), J9THREAD_CATEGORY_SYSTEM_GC_THREAD, J9THREAD_TYPE_SET_GC);
	}

	_gc->getRealtimeDelegate()->preRequestExclusiveVMAccess(thr);

	/* Wake up only the master thread -- it is responsible for
	 * waking up any slaves.
	 * Make sure _completeCurrentGCSynchronously and _mode are atomically changed.
	 */
	omrthread_monitor_enter(_masterThreadMonitor);
	switch(reason) {
		case OUT_OF_MEMORY_TRIGGER:
			/* For now we assume that OUT_OF_MEMORY trigger means perform
		 	* a synchronous GC, but maybe we want a mode where we try one
		 	* more time slice before degrading to synchronous.
		 	*/
		 	if(!_extensions->synchronousGCOnOOM) {
		 		break;
		 	}
		 	/* fall through */
		case SYSTEM_GC_TRIGGER:
			/* System garbage collects, if not disabled through the usual command lines,
		 	* force a synchronous GC
		 	*/
		 	_completeCurrentGCSynchronously = true;
		 	_completeCurrentGCSynchronouslyReason = reason;
		 	_completeCurrentGCSynchronouslyReasonParameter = resonParameter;

			break;
		default: /* WORK_TRIGGER or TIME_TRIGGER */ {
			if(_threadWaitingOnMasterThreadMonitor != NULL) {
				/* Check your timer again incase another thread beat you to checking for shouldMutatorDoubleBeat */
				if (env->getTimer()->hasTimeElapsed(getStartTimeOfCurrentMutatorSlice(), beatNanos)) {
					if (shouldMutatorDoubleBeat(_threadWaitingOnMasterThreadMonitor, env->getTimer())) {
						/*
						 * Since the mutator should double beat signal the mutator threads to update their
						 * timer with the current time.
						 */
						setStartTimeOfCurrentMutatorSlice(env->getTimer()->getTimeInNanos());
						didGC = false;
						goto exit;
					}
				} else {
					didGC = false;
					goto exit;
				}
			}
			break;
		}
	}
	if(_threadWaitingOnMasterThreadMonitor == NULL) {
		/*
 		* The gc thread(s) are already awake and collecting (otherwise, the master
 		* gc thread would be waiting on the monitor).
 		* This also means that the application threads are already sleeping.
 		* So there is no need to put the application threads to sleep or to
 		* awaken the gc thread(s).  However we return true to indicate that
 		* garbage collection is indeed taking place as requested.
 		*/
 		goto exit;
	}

	/* At this point master thread is blocked and cannot change _gcOn flag anymore.
	 * Check the flag again, since there is (a small) chance it may have changed since the last check
	 * (master thread, driven by mutators' could have finished the GC cycle)
	 */
	if (!isGCOn()) {
		didGC = false;
		goto exit;
	}

	_exclusiveVMAccessRequired = doRequestExclusiveVMAccess;

	_mode = WAKING_GC;

	if (_exclusiveVMAccessRequired) {
		/* initiate the request for exclusive VM access; this function does not wait for exclusive access to occur,
		 * that will be done by the master gc thread when it resumes activity after the masterThreadMonitor is notified
		 * We do not block. It's best effort. If the request is success full TRUE is returned via requested flag.
		 */
		if (FALSE == _gc->getRealtimeDelegate()->requestExclusiveVMAccess(_threadWaitingOnMasterThreadMonitor, FALSE /* do not block */, &gcPriority)) {
			didGC = false;
			goto exit;
		}
		_gc->setGCThreadPriority(env->getOmrVMThread(), gcPriority);
	}
	
	omrthread_monitor_notify(_masterThreadMonitor);
	/* set the waiting thread to NULL while we are in the _masterThreadMonitor so that nobody else will notify the waiting thread */
	_threadWaitingOnMasterThreadMonitor = NULL;

exit:
	if (_extensions->trackMutatorThreadCategory) {
		/* Done doing GC, reset the category back to the old one */
		omrthread_set_category(omrthread_self(), 0, J9THREAD_TYPE_SET_GC);
	}

	omrthread_monitor_exit(_masterThreadMonitor);
	_gc->getRealtimeDelegate()->postRequestExclusiveVMAccess(thr);

	return didGC;
}

uintptr_t
MM_Scheduler::getTaskThreadCount(MM_EnvironmentBase *env)
{
	if (env->_currentTask == NULL) {
		return 1;
	}
	return env->_currentTask->getThreadCount();
}

void
MM_Scheduler::waitForMutatorsToStop(MM_EnvironmentRealtime *env)
{
	/* assumption: only master enters this */
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	/* we need to record how long it took to wait for the mutators to stop */
	U_64 exclusiveAccessTime = omrtime_hires_clock();

	/* The time before acquiring exclusive VM access is charged to the mutator but the time
	 * during the acquisition is conservatively charged entirely to the GC. */
	_utilTracker->addTimeSlice(env, env->getTimer(), true);
	omrthread_monitor_enter(_masterThreadMonitor);
	/* If master GC thread gets here without anybody requesting exclusive access for us
	 * (possible in a shutdown scenario after we kill alarm thread), the thread will request
	 * exclusive access for itself.
	 * requestExclusiveVMAccess is invoked atomically with _mode being set to WAKING_GC
	 * under masterThreadMonitor (see continueGC). Therefore, we check here if mode is not
	 * WAKING_GC, and only then we request exclusive assess for ourselves.
	 * TODO: This approach is just to fix some timing holes in shutdown. Consider removing this
	 * "if" statement and fix alarm thread not to die before requesting exclusive access for us.
	 */
	if (_masterThreadMustShutDown && _mode != WAKING_GC) {
		uintptr_t gcPriority = 0;
		_gc->getRealtimeDelegate()->requestExclusiveVMAccess(env, TRUE /* block */, &gcPriority);
		_gc->setGCThreadPriority(env->getOmrVMThread(), gcPriority);
	}
	/* Avoid another attempt to start up GC increment */
	_mode = STOP_MUTATOR;
	omrthread_monitor_exit(_masterThreadMonitor);

	_gc->getRealtimeDelegate()->waitForExclusiveVMAccess(env, _exclusiveVMAccessRequired);

	_mode = RUNNING_GC;

	_extensions->globalGCStats.metronomeStats._microsToStopMutators = omrtime_hires_delta(exclusiveAccessTime, omrtime_hires_clock(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
}

void
MM_Scheduler::startMutators(MM_EnvironmentRealtime *env) {
	_mode = WAKING_MUTATOR;
	_gc->getRealtimeDelegate()->releaseExclusiveVMAccess(env, _exclusiveVMAccessRequired);
}

void
MM_Scheduler::startGCTime(MM_EnvironmentRealtime *env, bool isDoubleBeat)
{
	if (env->isMasterThread()) {
		setStartTimeOfCurrentGCSlice(_utilTracker->addTimeSlice(env, env->getTimer(), false));
	}
}

void
MM_Scheduler::stopGCTime(MM_EnvironmentRealtime *env)
{
	if (env->isMasterThread()) {
		setStartTimeOfCurrentMutatorSlice(_utilTracker->addTimeSlice(env, env->getTimer(), false));
	}
}

bool
MM_Scheduler::shouldGCDoubleBeat(MM_EnvironmentRealtime *env)
{
	double targetUtilization = _utilTracker->getTargetUtilization();
	if (targetUtilization <= 0.0) {
		return true;
	}
	I_32 maximumAllowedConsecutiveBeats = (I_32) (1.0 / targetUtilization);
	if (_currentConsecutiveBeats >= maximumAllowedConsecutiveBeats) {
		return false;
	}
	/* Note that shouldGCDoubleBeat is only called by the master thread, this means we
	 * can call addTimeSlice without checking for isMasterThread() */
	_utilTracker->addTimeSlice(env, env->getTimer(), false);
	double excessTime = (_utilTracker->getCurrentUtil() - targetUtilization) * window;
	double excessBeats = excessTime / beat;
	return (excessBeats >= 2.0);
}

bool
MM_Scheduler::shouldMutatorDoubleBeat(MM_EnvironmentRealtime *env, MM_Timer *timer)
{
	_utilTracker->addTimeSlice(env, timer, true);

	/* The call to currentUtil will modify the timeSlice array, so calls to shouldMutatorDoubleBeat
	 * must be protected by a mutex (which is indeed currently the case) */
	double curUtil = _utilTracker->getCurrentUtil();
	double excessTime = (curUtil - _utilTracker->getTargetUtilization()) * window;
	double excessBeats = excessTime / beat;
	return (excessBeats <= 1.0);
}

void
MM_Scheduler::reportStartGCIncrement(MM_EnvironmentRealtime *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	if(_completeCurrentGCSynchronously) {
		_completeCurrentGCSynchronouslyMasterThreadCopy = true;
		U_64 exclusiveAccessTimeMicros = 0;
		U_64 meanExclusiveAccessIdleTimeMicros = 0;

		Trc_MM_SystemGCStart(env->getLanguageVMThread(),
			_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
			_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
			_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
			_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
			(_extensions-> largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 ),
			(_extensions-> largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 )
		);

		exclusiveAccessTimeMicros = omrtime_hires_delta(0, env->getExclusiveAccessTime(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		meanExclusiveAccessIdleTimeMicros = omrtime_hires_delta(0, env->getMeanExclusiveAccessIdleTime(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		Trc_MM_ExclusiveAccess(env->getLanguageVMThread(),
			(U_32)(exclusiveAccessTimeMicros / 1000),
			(U_32)(exclusiveAccessTimeMicros % 1000),
			(U_32)(meanExclusiveAccessIdleTimeMicros / 1000),
			(U_32)(meanExclusiveAccessIdleTimeMicros % 1000),
			env->getExclusiveAccessHaltedThreads(),
			env->getLastExclusiveAccessResponder(),
			env->exclusiveAccessBeatenByOtherThread());

		_gc->reportSyncGCStart(env, _completeCurrentGCSynchronouslyReason, _completeCurrentGCSynchronouslyReasonParameter);
	}

	/* GC start/end are reported at each GC increment,
	 *  not at the beginning/end of a GC cycle,
	 *  since no Java code is supposed to run between those two events */
	_extensions->globalGCStats.metronomeStats.clearStart();
	_gc->reportGCStart(env);
	TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START(_extensions->privateHookInterface, env->getOmrVMThread(), omrtime_hires_clock(), J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START, _extensions->globalGCStats.metronomeStats._microsToStopMutators);

	_currentConsecutiveBeats = 1;
	startGCTime(env, false);

	_gc->flushCachesForGC(env);
}

void
MM_Scheduler::reportStopGCIncrement(MM_EnvironmentRealtime *env, bool isCycleEnd)
{
	/* assumption: only master enters this */

	stopGCTime(env);

	/* This can not be combined with the reportGCCycleEnd below as it has to happen before
	 * the incrementEnd event is triggered.
	 */
	if (isCycleEnd) {
		if (_completeCurrentGCSynchronously) {
			/* The requests for Sync GC made at the very end of
			 * GC cycle might not had a chance to make the local copy
			 */
			if (_completeCurrentGCSynchronouslyMasterThreadCopy) {
				Trc_MM_SystemGCEnd(env->getLanguageVMThread(),
					_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
					_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
					_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
					_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
					(_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 ),
					(_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 )
				);
				_gc->reportSyncGCEnd(env);
				_completeCurrentGCSynchronouslyMasterThreadCopy = false;
			}
			_completeCurrentGCSynchronously = false;
			_completeCurrentGCSynchronouslyReason = UNKOWN_REASON;
		}
	}

	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	TRIGGER_J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END(_extensions->privateHookInterface, env->getOmrVMThread(), omrtime_hires_clock(), J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	);

	/* GC start/end are reported at each GC increment,
	 *  not at the beginning/end of a GC cycle,
	 *  since no Java code is supposed to run between those two events */
	_gc->reportGCEnd(env);
	_extensions->globalGCStats.metronomeStats.clearEnd();
}

void
MM_Scheduler::restartMutatorsAndWait(MM_EnvironmentRealtime *env)
{
	startMutators(env);

	omrthread_monitor_enter(_masterThreadMonitor);
	/* Atomically change mode to MUTATOR and set threadWaitingOnMasterThreadMonitor
	 * (only after the master is fully stoped, we switch from WAKING_MUTATOR to MUTATOR) */
	_mode = MUTATOR;
	_threadWaitingOnMasterThreadMonitor = env;

	/* If we're shutting down, we don't want to wait. Note that this is safe
	 * since on shutdown, the only mutator thread left is the thread that is
	 * doing the shutdown.
	 */
	if (!_masterThreadMustShutDown) {
		omrthread_monitor_wait(_masterThreadMonitor);
		/* Master is awoken to either do another increment of GC or
		 * to shutdown (but never both)
		 */
		Assert_MM_true((isGCOn() && !_masterThreadMustShutDown) || (!_gcOn &&_masterThreadMustShutDown));
	}
	omrthread_monitor_exit(_masterThreadMonitor);
}

bool
MM_Scheduler::shouldGCYield(MM_EnvironmentRealtime *env, U_64 timeSlack)
{
	return internalShouldGCYield(env, timeSlack);
}

/**
 * Test whether it's time for the GC to yield, and whether yielding is currently enabled.
 *  To enhance the generality of methods that may call this method, the call may occur on 
 *  a non-GC thread, in which case this method does nothing.
 * @param timeSlack a slack factor to apply to time-based scheduling
 * @param location the phase of the GC during which this call is occurring (for tracing: in
 *    some cases may be approximate).
 * @return true if the GC thread should yield, false otherwise
 */
MMINLINE bool
MM_Scheduler::internalShouldGCYield(MM_EnvironmentRealtime *env, U_64 timeSlack)
{
	if (_completeCurrentGCSynchronouslyMasterThreadCopy) {
		/* If we have degraded to a synchronous GC, don't yield until finished */
		return false;
	}
	/* Be harmless when called indirectly on mutator thread */
	if (env->getThreadType() == MUTATOR_THREAD) {
		return false;
	}
	/* The GC does not have to yield when ConcurrentTracing or ConcurrentSweeping is
	 * enabled since the GC is not holding exclusive access.
	 */
	if (_gc->isCollectorConcurrentTracing() || _gc->isCollectorConcurrentSweeping()) {
		return false;
	}

	/* If at least one thread thinks we should yield, than all should yield.
	 * Discrepancy may happen due different timeSlack that GC threads may have */
	if (_shouldGCYield) {
		return true;
	}

	if (env->hasDistanceToYieldTimeCheck()) {
		return false;
	}

	I_64 nanosLeft = _utilTracker->getNanosLeft(env, getStartTimeOfCurrentGCSlice());
	if (nanosLeft > 0) {
		if ((U_64)nanosLeft > timeSlack) {
			return false;
		}
	}
	_shouldGCYield = true;
	return true;
}

bool
MM_Scheduler::condYieldFromGCWrapper(MM_EnvironmentBase *env, U_64 timeSlack)
{
	return condYieldFromGC(env, timeSlack);
}

/**
 * Test whether it's time for the GC to yield, and whether yielding is currently enabled, and
 *  if appropriate actually do the yielding.   To enhance the generality of methods that may
 *  call this method, the call may occur on a non-GC thread, in which case this method does
 *  nothing.
 * @param location the phase of the GC during which this call is occurring (for tracing: in
 *    some cases may be approximate).
 * @param timeSlack a slack factor to apply to time-based scheduling
 * @return true if yielding actually occurred, false otherwise
 */
bool
MM_Scheduler::condYieldFromGC(MM_EnvironmentBase *envBase, U_64 timeSlack)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);

	if (env->getYieldDisableDepth() > 0) {
		return false;
	}
	if (!internalShouldGCYield(env, timeSlack)) {
		return false;
	}

	yieldFromGC(env, true);

	env->resetCurrentDistanceToYieldTimeCheck();

	return true;
}

void MM_Scheduler::yieldFromGC(MM_EnvironmentRealtime *env, bool distanceChecked)
{
	assert(!_gc->isCollectorConcurrentTracing());
	assert(!_gc->isCollectorConcurrentSweeping());
	if (env->isMasterThread()) {
		if (_yieldCollaborator) {
			/* wait for slaves to yield/sync */
			_yieldCollaborator->yield(env);
		}

		_sharedBarrierState = shouldGCDoubleBeat(env);

		if (_sharedBarrierState) {
			_currentConsecutiveBeats += 1;
			startGCTime(env, true);
		} else {
			reportStopGCIncrement(env);
			env->reportScanningSuspended();
			Assert_MM_true(isGCOn());
			restartMutatorsAndWait(env);
			waitForMutatorsToStop(env);
			env->reportScanningResumed();
			reportStartGCIncrement(env);
			_shouldGCYield = false;
		}

		if (_yieldCollaborator) {
			_yieldCollaborator->resumeSlavesFromYield(env);
		}

	} else {
		/* Slave only running here. _yieldCollaborator instance exists for sure */
		env->reportScanningSuspended();
		_yieldCollaborator->yield(env);
		env->reportScanningResumed();
	}
}

void
MM_Scheduler::prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task, uintptr_t threadCount)
{
	omrthread_monitor_enter(_slaveThreadMutex);
	_slaveThreadsReservedForGC = true;

	task->setSynchronizeMutex(_synchronizeMutex);

	for (uintptr_t index=0; index < threadCount; index++) {
		_statusTable[index] = slave_status_reserved;
		_taskTable[index] = task;
	}

	wakeUpThreads(threadCount);
	omrthread_monitor_exit(_slaveThreadMutex);

	pushYieldCollaborator(((MM_IncrementalParallelTask *)task)->getYieldCollaborator());
}

void
MM_Scheduler::completeTask(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		popYieldCollaborator();
	}
	MM_ParallelDispatcher::completeTask(env);
}

bool
MM_Scheduler::startUpThreads()
{
	OMRPORT_ACCESS_FROM_OMRVM(_vm);
	MM_EnvironmentRealtime env(_vm);

	if (_extensions->gcThreadCount > _osInterface->getNumbersOfProcessors()) {
		omrtty_printf("Please specify fewer GC threads than the number of physical processors.\n");
		return false;
	}

	/* Start up the GC threads */
	if (!MM_ParallelDispatcher::startUpThreads()) {
		return false;
	}

	/* At this point, all GC threads have signalled that they are ready.
	 * However, because Metronome uses omrthread_suspend/omrthread_resume to stop and
	 * start threads, there is a race: the thread may have been preempted after
	 * signalling but before suspending itself. An alternative may be to use
	 * omrthread_park/unpark.
	 */
	_isInitialized = true;

	/* Now that the GC threads are started, it is safe to start the alarm thread */
	_alarmThread = MM_MetronomeAlarmThread::newInstance(&env);
	if (_alarmThread == NULL) {
		omrtty_printf("Unable to initialize alarm thread for time-based GC scheduling\n");
		omrtty_printf("Most likely cause is non-supported version of OS\n");
		return false;
	}

	if (verbose() >= 1) {
		showParameters(&env);
	}

	return true;
}

/**
 * @copydoc MM_ParallelDispatcher::recomputeActiveThreadCount()
 * This function is called at the start of a complete GC cycle to calculate the number of
 * GC threads to use for the cycle.
 */
void
MM_Scheduler::recomputeActiveThreadCount(MM_EnvironmentBase *env)
{
	_activeThreadCount = _threadCount;
}

/**
 * @copydoc MM_ParallelDispatcher::getThreadPriority()
 */
uintptr_t
MM_Scheduler::getThreadPriority()
{
	/* this is the priority that the threads are started with */
	return J9THREAD_PRIORITY_USER_MAX + 1;
}

/**
 * @copydoc MM_MetronomeDispatcher::slaveEntryPoint()
 */
void
MM_Scheduler::slaveEntryPoint(MM_EnvironmentBase *envModron)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envModron);

	uintptr_t slaveID = env->getSlaveID();

	setThreadInitializationComplete(env);

	omrthread_monitor_enter(_slaveThreadMutex);

	while(slave_status_dying != _statusTable[slaveID]) {
		/* Wait for a task to be dispatched to the slave thread */
		while(slave_status_waiting == _statusTable[slaveID]) {
			omrthread_monitor_wait(_slaveThreadMutex);
		}

		if(slave_status_reserved == _statusTable[slaveID]) {
			/* Found a task to dispatch to - do prep work for dispatch */
			acceptTask(env);
			omrthread_monitor_exit(_slaveThreadMutex);

			env->_currentTask->run(env);

			omrthread_monitor_enter(_slaveThreadMutex);
			/* Returned from task - do clean up work from dispatch */
			completeTask(env);
		}
	}
	omrthread_monitor_exit(_slaveThreadMutex);
}

/**
 * @copydoc MM_ParallelDispatcher::masterEntryPoint()
 */
void
MM_Scheduler::masterEntryPoint(MM_EnvironmentBase *envModron)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envModron);

	setThreadInitializationComplete(env);

	omrthread_monitor_enter(_masterThreadMonitor);
	_threadWaitingOnMasterThreadMonitor = env;
	omrthread_monitor_wait(_masterThreadMonitor);
	omrthread_monitor_exit(_masterThreadMonitor);

	/* We want to execute the body of the do-while (run a gc) if a shutdown has
	 * been requested at the same time as the first gc. In other words, we want
	 * a gc to complete before shutting down.
	 *
	 * We however do not want to execute a gc if it hasn't been requested. The
	 * outer while loop guarantees this. It is a while loop (as opposed to an
	 * if) to cover the case of simultaneous gc/shutdown while waiting in
	 * stopGCIntervalAndWait. Again, we want to complete the gc in that case.
	 */
	while (isGCOn()) {
		do {
			/* Before starting a new GC, recompute the number of threads to use */
			recomputeActiveThreadCount(env);
			waitForMutatorsToStop(env);
			/* note that the cycle and increment start events will be posted from MM_RealtimeGC::internalPreCollect */
			_gc->_memorySubSpace->collect(env, _gcCode);
			restartMutatorsAndWait(env);

		/* We must also check for the _masterThreadMustShutDown flag since if we
		 * try to shutdown while we're in a stopGCIntervalAndWait, the GC will
		 * continue potentially changing the status of the master thread
		 */
		} while ((slave_status_dying != _statusTable[env->getSlaveID()] && !_masterThreadMustShutDown));
	}
	/* TODO: tear down the thread before exiting */
}

/**
 * If there is an ongoing GC cycle complete it
 */
void
MM_Scheduler::completeCurrentGCSynchronously(MM_EnvironmentRealtime *env)
{
	omrthread_monitor_enter(_vm->_gcCycleOnMonitor);	
	if (_vm->_gcCycleOn || isGCOn()) {
		_completeCurrentGCSynchronously = true;
		_completeCurrentGCSynchronouslyReason = VM_SHUTDOWN;

		/* wait till get notified by master that the cycle is finished */
		omrthread_monitor_wait(_vm->_gcCycleOnMonitor);
	}
	omrthread_monitor_exit(_vm->_gcCycleOnMonitor);
}

/**
 * @copydoc MM_ParallelDispatcher::wakeUpThreads()
 */
void
MM_Scheduler::wakeUpThreads(uintptr_t count)
{
	assert1(count > 0);

	/* Resume the master thread */
	omrthread_monitor_enter(_masterThreadMonitor);
	omrthread_monitor_notify(_masterThreadMonitor);
	omrthread_monitor_exit(_masterThreadMonitor);

	if (count > 1) {
		wakeUpSlaveThreads(count - 1);
	}
}

/**
 * Wakes up `count` slave threads. This function will actually busy wait until
 * `count` number of slaves have been resumed from the suspended state.
 *
 * @param count Number of slave threads to wake up
 */
void
MM_Scheduler::wakeUpSlaveThreads(uintptr_t count)
{
	omrthread_monitor_notify_all(_slaveThreadMutex);
}

/**
 * @copydoc MM_ParallelDispatcher::shutDownThreads()
 */
void
MM_Scheduler::shutDownThreads()
{
	/* This will stop threads from requesting another GC cycle to start*/
	_isInitialized = false;

	/* If the GC is currently in a Cycle complete it before we shutdown */
	completeCurrentGCSynchronously();

	/* Don't kill the master thread before the alarm thread since the alarm thread
	 * may still refer to the master thread if a continueGC happens to occur during
	 * shutdown.
	 */
	shutDownSlaveThreads();

	/* Don't kill the alarm thread until after the GC slave threads, since it may
	 * be needed to drive a final synchronous GC */
	if (_alarmThread) {
		MM_EnvironmentBase env(_vm);
		_alarmThread->kill(&env);
		_alarmThread = NULL;
	}

	/* Now that the alarm and trace threads are killed, we can shutdown the master thread */
	shutDownMasterThread();
}

/**
 * Signals the slaves to shutdown, will block until they are all shutdown.
 *
 * @note Assumes all threads are live before the function is called (ie: this
 * must be called before shutDownMasterThread)
 */
void
MM_Scheduler::shutDownSlaveThreads()
{
	/* If _threadShutdownCount is 1, only the master must shutdown, if 0,
	 * no shutdown required (happens when args passed to java are invalid
	 * so the vm doesn't actually start up, ex: -Xgc:threads=5 on a 4-way
	 * box)
	 */
	if (_threadShutdownCount <= 1) {
		return;
	}

	omrthread_monitor_enter(_slaveThreadMutex);

	for (uintptr_t threadIndex = 1; threadIndex < _threadCountMaximum; threadIndex++) {
		_statusTable[threadIndex] = slave_status_dying;
	}

	_threadCount = 1;

	wakeUpSlaveThreads(_threadShutdownCount - 1);

	omrthread_monitor_exit(_slaveThreadMutex);

	/* -1 because the thread shutdown count includes the master thread */
	omrthread_monitor_enter(_dispatcherMonitor);

	while (1 != _threadShutdownCount) {
		omrthread_monitor_wait(_dispatcherMonitor);
	}

	omrthread_monitor_exit(_dispatcherMonitor);
}

/**
 * Signals the master to shutdown, will block until the thread is shutdown.
 *
 * @note Assumes the master thread is the last gc thread left (ie: this must be
 * called after shutDownSlaveThreads)
 */
void
MM_Scheduler::shutDownMasterThread()
{
	omrthread_monitor_enter(_slaveThreadMutex);
	_statusTable[0] = slave_status_dying;
	omrthread_monitor_exit(_slaveThreadMutex);

	/* Note: Calling wakeUpThreads at this point would be unsafe since there is
	 * more than 1 location where the master thread could be waiting and the one
	 * in stopGcIntervalAndWait [which ultimately gets invoked by the
	 * condYieldFromGC] assumes that a request for exclusive VM access on behalf
	 * of the master has been made. Blindly notifying the monitor [as
	 * wakeUpThreads does] would cause the master thread to wait for exclusive
	 * access without requesting for it first, causing a hang.
	 */
	omrthread_monitor_enter(_masterThreadMonitor);
	_masterThreadMustShutDown = true;
	omrthread_monitor_notify(_masterThreadMonitor);
	omrthread_monitor_exit(_masterThreadMonitor);

	omrthread_monitor_enter(_dispatcherMonitor);
	while (0 != _threadShutdownCount) {
		omrthread_monitor_wait(_dispatcherMonitor);
	}
	omrthread_monitor_exit(_dispatcherMonitor);
}

/**
 * Check to see if it is time to do the next GC increment.  If beatNanos time
 * has elapsed since the end of the last GC increment then start the next
 * increment now.
 */
void
MM_Scheduler::startGCIfTimeExpired(MM_EnvironmentBase *envModron)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envModron);
	if (isInitialized() && isGCOn() && env->getTimer()->hasTimeElapsed(getStartTimeOfCurrentMutatorSlice(), beatNanos)) {
		continueGC(env, TIME_TRIGGER, 0, env->getOmrVMThread(), true);
	}
}

uintptr_t
MM_Scheduler::incrementMutatorCount()
{
	return MM_AtomicOperations::add(&_mutatorCount, 1);
}

extern "C" {

void
j9gc_startGCIfTimeExpired(OMR_VMThread* vmThread)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(vmThread);
	MM_Scheduler *scheduler = (MM_Scheduler *)env->getExtensions()->dispatcher;
	scheduler->startGCIfTimeExpired(env);
}

}
