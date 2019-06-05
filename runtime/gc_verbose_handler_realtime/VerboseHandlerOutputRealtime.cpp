/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "VerboseHandlerOutputRealtime.hpp"

#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "VerboseHandlerRealtime.hpp"
#include "VerboseManager.hpp"
#include "VerboseWriterChain.hpp"
#include "VerboseHandlerJava.hpp"

#include "gcutils.h"
#include "mmhook.h"
#include "mmprivatehook.h"

MM_VerboseHandlerOutput *
MM_VerboseHandlerOutputRealtime::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env->getOmrVM());

	MM_VerboseHandlerOutputRealtime *verboseHandlerOutput = (MM_VerboseHandlerOutputRealtime *)extensions->getForge()->allocate(sizeof(MM_VerboseHandlerOutputRealtime), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != verboseHandlerOutput) {
		new(verboseHandlerOutput) MM_VerboseHandlerOutputRealtime(extensions);
		if(!verboseHandlerOutput->initialize(env, manager)) {
			verboseHandlerOutput->kill(env);
			verboseHandlerOutput = NULL;
		}
	}
	return verboseHandlerOutput;
}

bool
MM_VerboseHandlerOutputRealtime::initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	_verboseInitTimeStamp = j9time_hires_clock();
	bool initSuccess = MM_VerboseHandlerOutput::initialize(env, manager);

	_mmHooks = J9_HOOK_INTERFACE(MM_GCExtensions::getExtensions(_extensions)->hookInterface);

	return initSuccess;
}

void
MM_VerboseHandlerOutputRealtime::tearDown(MM_EnvironmentBase *env)
{
	MM_VerboseHandlerOutput::tearDown(env);
}

void
MM_VerboseHandlerOutputRealtime::enableVerbose()
{
	MM_VerboseHandlerOutput::enableVerbose();

	/* Cycle */
	(*_mmOmrHooks)->J9HookRegisterWithCallSite(_mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, verboseHandlerCycleStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, verboseHandlerCycleEnd, OMR_GET_CALLSITE(), (void *)this);

	/* trigger */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_START, verboseHandlerTriggerStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END, verboseHandlerTriggerEnd, OMR_GET_CALLSITE(), (void *)this);

	/* increment */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START, verboseHandlerIncrementStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END, verboseHandlerIncrementEnd, OMR_GET_CALLSITE(), (void *)this);

	/* sync gc */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, verboseHandlerSyncGCStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_END, verboseHandlerSyncGCEnd, OMR_GET_CALLSITE(), (void *)this);

	/* mark, class unload, sweep phases */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_START, verboseHandlerMarkStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_END, verboseHandlerMarkEnd, OMR_GET_CALLSITE(), (void *)this);

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_START, verboseHandlerSweepStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, verboseHandlerSweepEnd, OMR_GET_CALLSITE(), (void *)this);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START, verboseHandlerClassUnloadingStart, OMR_GET_CALLSITE(), (void *)this);
	(*_mmHooks)->J9HookRegisterWithCallSite(_mmHooks, J9HOOK_MM_CLASS_UNLOADING_END, verboseHandlerClassUnloadingEnd, OMR_GET_CALLSITE(), (void *)this);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_OUT_OF_MEMORY, verboseHandlerOutOFMemory, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_UTILIZATION_TRACKER_OVERFLOW, verboseHandlerUtilTrackerOverflow, OMR_GET_CALLSITE(), (void *)this);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_NON_MONOTONIC_TIME, verboseHandlerNonMonotonicTime, OMR_GET_CALLSITE(), (void *)this);
}

void
MM_VerboseHandlerOutputRealtime::disableVerbose()
{
	MM_VerboseHandlerOutput::disableVerbose();

	/* Cycle */
	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, verboseHandlerCycleStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, verboseHandlerCycleEnd, NULL);

	/* trigger */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_START, verboseHandlerTriggerStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END, verboseHandlerTriggerEnd, NULL);

	/* increment */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START, verboseHandlerIncrementStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END, verboseHandlerIncrementEnd, NULL);

	/* sync gc */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, verboseHandlerSyncGCStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_END, verboseHandlerSyncGCEnd, NULL);

	/* mark, class unload, sweep phases */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_START, verboseHandlerMarkStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_END, verboseHandlerMarkEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_START, verboseHandlerSweepStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, verboseHandlerSweepEnd, NULL);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START, verboseHandlerClassUnloadingStart, NULL);
	(*_mmHooks)->J9HookUnregister(_mmHooks, J9HOOK_MM_CLASS_UNLOADING_END, verboseHandlerClassUnloadingEnd, NULL);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_OUT_OF_MEMORY, verboseHandlerOutOFMemory, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_UTILIZATION_TRACKER_OVERFLOW, verboseHandlerUtilTrackerOverflow, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_NON_MONOTONIC_TIME, verboseHandlerNonMonotonicTime, NULL);
}

bool
MM_VerboseHandlerOutputRealtime::getThreadName(char *buf, UDATA bufLen, OMR_VMThread *vmThread)
{
	return MM_VerboseHandlerJava::getThreadName(buf,bufLen,vmThread);
}

void
MM_VerboseHandlerOutputRealtime::writeVmArgs(MM_EnvironmentBase* env)
{
	MM_VerboseHandlerJava::writeVmArgs(_manager, env, static_cast<J9JavaVM*>(_omrVM->_language_vm));
}

const char *
MM_VerboseHandlerOutputRealtime::getCycleType(UDATA type)
{
	const char* cycleType = NULL;
	switch (type) {
	case OMR_GC_CYCLE_TYPE_DEFAULT:
		cycleType = "default";
		break;
	case OMR_GC_CYCLE_TYPE_GLOBAL:
		cycleType = "global";
		break;
	default:
		cycleType = "unknown";
		break;
	}

	return cycleType;
}

void
MM_VerboseHandlerOutputRealtime::handleInitializedInnerStanzas(J9HookInterface** hook, UDATA eventNum, void* eventData)
{
	MM_InitializedEvent* event = (MM_InitializedEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	handleInitializedRegion(hook, eventNum, eventData);

	writer->formatAndOutput(env, 1, "<metronome>");
	writer->formatAndOutput(env, 2, "<attribute name=\"beatsPerMeasure\" value=\"%zu\" />", event->beat);
	writer->formatAndOutput(env, 2, "<attribute name=\"timeInterval\" value=\"%zu\" />", event->timeWindow);
	writer->formatAndOutput(env, 2, "<attribute name=\"targetUtilization\" value=\"%zu\" />", event->targetUtilization);
	writer->formatAndOutput(env, 2, "<attribute name=\"trigger\" value=\"0x%zx\" />", event->gcTrigger);
	writer->formatAndOutput(env, 2, "<attribute name=\"headRoom\" value=\"0x%zx\" />", event->headRoom);
	writer->formatAndOutput(env, 1, "</metronome>");
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_MetronomeIncrementStartEvent* eventData)
{
	/* if we are in a sync gc cycle, do nothing here and let the syncgc stanza handle things */
	if (!_syncGCTriggered) {
		if (0 == _heartbeatStartTime) {
			_heartbeatStartTime = eventData->timestamp;
		}
		_incrementStartTime = eventData->timestamp;
		_incrementCount++;
		_totalExclusiveAccessTime += eventData->exclusiveAccessTime;
		_maxExclusiveAccessTime = OMR_MAX(eventData->exclusiveAccessTime, _maxExclusiveAccessTime);
		_minExclusiveAccessTime = OMR_MIN(eventData->exclusiveAccessTime, _minExclusiveAccessTime);
	} else {
		/* if we are in a sync GC cycle, only collect the exclusive access time */
		_syncGCExclusiveAccessTime = eventData->exclusiveAccessTime;
	}
}

void
MM_VerboseHandlerOutputRealtime::writeHeartbeatData(MM_EnvironmentBase* env, U_64 timestamp)
{
	/* if we are in a sync gc cycle OR if there is no heartbeat data, nothing to do */
	if (!_syncGCTriggered && 0 != _heartbeatStartTime) {
		MM_VerboseWriterChain* writer = _manager->getWriterChain();
		PORT_ACCESS_FROM_ENVIRONMENT(env);

		char tagTemplate[200];
		getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), "heartbeat", env->_cycleState->_verboseContextID, j9time_current_time_millis());
		enterAtomicReportingBlock();
		writer->formatAndOutput(env, 0, "<gc-op %s>", tagTemplate);

		U_64 maxQuantaTime = j9time_hires_delta(_heartbeatStartTime, _maxIncrementStartTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
		U_64 meanIncrementTime = _totalIncrementTime / _incrementCount;
		const char *gcPhase = NULL;
		if(incrementStartsInPreviousGCPhase()) {
			gcPhase = getGCPhaseAsString(_previousGCPhase);
			_previousGCPhase = _gcPhase;
		} else {
			gcPhase = getGCPhaseAsString(_gcPhase);
		}
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<quanta quantumCount=\"%zu\" quantumType=\"%s\" minTimeMs=\"%llu.%03.3llu\" meanTimeMs=\"%llu.%03.3llu\" maxTimeMs=\"%llu.%03.3llu\" maxTimestampMs=\"%llu.%03.3llu\" />",
			_incrementCount,
			gcPhase,
			_minIncrementTime / 1000,
			_minIncrementTime % 1000,
			meanIncrementTime / 1000,
			meanIncrementTime % 1000,
			_maxIncrementTime / 1000,
			_maxIncrementTime % 1000,
			maxQuantaTime / 1000,
			maxQuantaTime % 1000
		);

		U_64 meanExclusiveAccessTime = _totalExclusiveAccessTime / _incrementCount;
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<exclusiveaccess-info minTimeMs=\"%llu.%03.3llu\" meanTimeMs=\"%llu.%03.3llu\" maxTimeMs=\"%llu.%03.3llu\" />",
			_minExclusiveAccessTime / 1000,
			_minExclusiveAccessTime % 1000,
			meanExclusiveAccessTime / 1000,
			meanExclusiveAccessTime % 1000,
			_maxExclusiveAccessTime / 1000,
			_maxExclusiveAccessTime % 1000
		);

		if ((0 != _classesUnloadedTotal) || (0 != _classLoadersUnloadedTotal)) {
			writer->formatAndOutput(
				env, 1 /*indent*/,
				"<classunload-info classloadersunloaded=\"%zu\" classesunloaded=\"%zu\" anonymousclassesunloaded=\"%zu\" />",
				_classLoadersUnloadedTotal,
				_classesUnloadedTotal,
				_anonymousClassesUnloadedTotal
			);
		}

		if (0 != _softReferenceClearCountTotal) {
			writer->formatAndOutput(
				env, 1, "<references type=\"soft\" cleared=\"%zu\" dynamicThreshold=\"%zu\" maxThreshold=\"%zu\" />",
				_weakReferenceClearCountTotal,
				_dynamicSoftReferenceThreshold,
				_softReferenceThreshold
			);
		}
		if (0 != _weakReferenceClearCountTotal) {
			writer->formatAndOutput(env, 1, "<references type=\"weak\" cleared=\"%zu\" />", _weakReferenceClearCountTotal);
		}
		if (0 != _phantomReferenceClearCountTotal) {
			writer->formatAndOutput(env, 1, "<references type=\"phantom\" cleared=\"%zu\" />", _phantomReferenceClearCountTotal);
		}

		if (0 != _finalizableCountTotal) {
			writer->formatAndOutput(env, 1, "<finalization enqueued=\"%zu\" />", _finalizableCountTotal);
		}

		if ((0 != _workPacketOverflowCountTotal) || (0 != _objectOverflowCountTotal)) {
			writer->formatAndOutput(
				env, 1 /*indent*/,
				"<work-packet-overflow packetCount=\"%zu\" directObjectCount=\"%zu\" />",
				_workPacketOverflowCountTotal,
				_objectOverflowCountTotal
			);
		}

		if (0 != _nonDeterministicSweepTotal) {
			writer->formatAndOutput(
				env, 1 /*indent*/,
				"<nondeterministic-sweep maxTimeMs=\"%llu.%03.3llu\" totalRegions=\"%zu\" maxRegions=\"%zu\" />",
				_nonDeterministicSweepDelayMax / 1000,
				_nonDeterministicSweepDelayMax % 1000,
				_nonDeterministicSweepTotal,
				_nonDeterministicSweepConsecutiveMax
			);
		}

		U_64 meanHeapFree = _totalHeapFree / _incrementCount;
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<free-mem type=\"heap\" minBytes=\"%llu\" meanBytes=\"%llu\" maxBytes=\"%llu\" />",
			_minHeapFree,
			meanHeapFree,
			_maxHeapFree
		);

		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<thread-priority maxPriority=\"%zu\" minPriority=\"%zu\" />",
			_maxStartPriority,
			_minStartPriority
		);

		writer->formatAndOutput(env, 0, "</gc-op>");
		writer->flush(env);
		exitAtomicReportingBlock();
	}
}

void
MM_VerboseHandlerOutputRealtime::writeHeartbeatDataAndResetHeartbeatStats(MM_EnvironmentBase* env, U_64 timestamp)
{
	writeHeartbeatData(env, timestamp);
	resetHeartbeatStats();
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_MetronomeIncrementEndEvent* eventData)
{
	/* process only if we are in a heartbeat */
	if (0 != _heartbeatStartTime) {
		MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(eventData->currentThread);
		MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env->getOmrVM());

		PORT_ACCESS_FROM_ENVIRONMENT(env);

		U_64 incrementTime = j9time_hires_delta(_incrementStartTime, eventData->timestamp, J9PORT_TIME_DELTA_IN_MICROSECONDS);
		_totalIncrementTime += incrementTime;
		if (incrementTime > _maxIncrementTime) {
			_maxIncrementTime = incrementTime;
			_maxIncrementStartTime = _incrementStartTime;
		}
		/* reset increment start time */
		_incrementStartTime = 0;
		_minIncrementTime = OMR_MIN(incrementTime, _minIncrementTime);

		_classLoadersUnloadedTotal += extensions->globalGCStats.metronomeStats.classLoaderUnloadedCount;
		_classesUnloadedTotal += extensions->globalGCStats.metronomeStats.classesUnloadedCount;
		_anonymousClassesUnloadedTotal += extensions->globalGCStats.metronomeStats.anonymousClassesUnloadedCount;

		_weakReferenceClearCountTotal += extensions->markJavaStats._weakReferenceStats._cleared;
		_softReferenceClearCountTotal += extensions->markJavaStats._softReferenceStats._cleared;
		_softReferenceThreshold = extensions->getMaxSoftReferenceAge();
		_dynamicSoftReferenceThreshold = extensions->getDynamicMaxSoftReferenceAge();
		_phantomReferenceClearCountTotal += extensions->markJavaStats._phantomReferenceStats._cleared;

		_finalizableCountTotal += extensions->markJavaStats._unfinalizedEnqueued;

		_workPacketOverflowCountTotal += extensions->globalGCStats.metronomeStats.getWorkPacketOverflowCount();
		_objectOverflowCountTotal += extensions->globalGCStats.metronomeStats.getObjectOverflowCount();

		_nonDeterministicSweepTotal += extensions->globalGCStats.metronomeStats.nonDeterministicSweepCount;
		_nonDeterministicSweepConsecutiveMax = OMR_MAX(_nonDeterministicSweepConsecutiveMax, extensions->globalGCStats.metronomeStats.nonDeterministicSweepConsecutive);
		_nonDeterministicSweepDelayMax = OMR_MAX(_nonDeterministicSweepDelayMax, extensions->globalGCStats.metronomeStats.nonDeterministicSweepDelay);

		_maxHeapFree = OMR_MAX(_maxHeapFree, _extensions->heap->getApproximateActiveFreeMemorySize());
		_totalHeapFree += _extensions->heap->getApproximateActiveFreeMemorySize();
		_minHeapFree = OMR_MIN(_minHeapFree, _extensions->heap->getApproximateActiveFreeMemorySize());

		UDATA startPriority = omrthread_get_priority(eventData->currentThread->_os_thread);
		_maxStartPriority = OMR_MAX(_maxStartPriority, startPriority);
		_minStartPriority = OMR_MIN(_minStartPriority, startPriority);

		U_64 timeSinceHeartbeatStart = j9time_hires_delta(_heartbeatStartTime, eventData->timestamp, J9PORT_TIME_DELTA_IN_MICROSECONDS);
		if (((timeSinceHeartbeatStart / 1000) >= extensions->verbosegcCycleTime)
			|| (incrementStartsInPreviousGCPhase())) {
			writeHeartbeatDataAndResetHeartbeatStats(env, eventData->timestamp);
		}
	}
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_MetronomeSynchronousGCStartEvent* eventData)
{
	/* we hit a sync GC, flush heartbeat data collected so far */
	writeHeartbeatDataAndResetHeartbeatStats(MM_EnvironmentBase::getEnvironment(eventData->currentThread), eventData->timestamp);

	_syncGCTriggered = true;
	_syncGCStartTime = eventData->timestamp;
	_syncGCReason = (GCReason)eventData->reason;
	_syncGCReasonParameter = eventData->reasonParameter;
	_syncGCStartHeapFree = eventData->heapFree;
	_syncGCStartImmortalFree = eventData->immortalFree;
	_syncGCStartClassLoadersUnloaded = eventData->classLoadersUnloaded;
	_syncGCStartClassesUnloaded = eventData->classesUnloaded;
	_syncGCStartAnonymousClassesUnloaded = eventData->anonymousClassesUnloaded;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_MetronomeSynchronousGCEndEvent* eventData)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(eventData->currentThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	enterAtomicReportingBlock();

	U_64 syncGCTimeDuration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&syncGCTimeDuration, _syncGCStartTime, eventData->timestamp);

	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), "syncgc", env->_cycleState->_verboseContextID, syncGCTimeDuration, j9time_current_time_millis());

	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}

	writer->formatAndOutput(env, 0, "<gc-op %s>", tagTemplate);

	const char *hookReason = getGCReasonAsString(_syncGCReason);
	if (_syncGCReason == OUT_OF_MEMORY_TRIGGER) {
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<syncgc-info reason=\"%s\" totalBytesRequested=\"%zu\" exclusiveaccessTimeMs=\"%llu.%03.3llu\" threadPriority=\"%zu\" />",
			hookReason,
			_syncGCReasonParameter,
			_syncGCExclusiveAccessTime / 1000,
			_syncGCExclusiveAccessTime % 1000,
			omrthread_get_priority(eventData->currentThread->_os_thread)
		);
	} else {
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<syncgc-info reason=\"%s\" exclusiveaccessTimeMs=\"%llu.%03.3llu\" threadPriority=\"%zu\" />",
			hookReason,
			_syncGCExclusiveAccessTime / 1000,
			_syncGCExclusiveAccessTime % 1000,
			omrthread_get_priority(eventData->currentThread->_os_thread)
		);
	}

	writer->formatAndOutput(env, 1, "<free-mem-delta type=\"heap\" bytesBefore=\"%zu\" bytesAfter=\"%zu\" />", _syncGCStartHeapFree, eventData->heapFree);

	if ((0 != eventData->workPacketOverflowCount) || (0 != eventData->objectOverflowCount)) {
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<work-packet-overflow packetCount=\"%zu\" directObjectCount=\"%zu\" />",
			eventData->workPacketOverflowCount,
			eventData->objectOverflowCount
		);
	}

	if ((_syncGCStartClassesUnloaded != eventData->classesUnloaded) || (_syncGCStartClassLoadersUnloaded != eventData->classLoadersUnloaded)) {
		writer->formatAndOutput(
			env, 1 /*indent*/,
			"<classunload-info classloadersunloaded=\"%zu\" classesunloaded=\"%zu\" anonymousclassesunloaded=\"%zu\" />",
			eventData->classLoadersUnloaded - _syncGCStartClassLoadersUnloaded,
			eventData->classesUnloaded - _syncGCStartClassesUnloaded,
			eventData->anonymousClassesUnloaded - _syncGCStartAnonymousClassesUnloaded
		);
	}

	if (0 != eventData->softReferenceClearCount) {
		writer->formatAndOutput(
			env, 1,
			"<references type=\"soft\" cleared=\"%zu\" dynamicThreshold=\"%zu\" maxThreshold=\"%zu\" />",
			eventData->softReferenceClearCount,
			eventData->dynamicSoftReferenceThreshold,
			eventData->softReferenceThreshold
		);
	}
	if (0 != eventData->weakReferenceClearCount) {
		writer->formatAndOutput(env, 1, "<references type=\"weak\" cleared=\"%zu\" />",eventData->weakReferenceClearCount);
	}
	if (0 != eventData->phantomReferenceClearCount) {
		writer->formatAndOutput(env, 1, "<references type=\"phantom\" cleared=\"%zu\" />",eventData->phantomReferenceClearCount);
	}

	if (0 != eventData->finalizableCount) {
		writer->formatAndOutput(env, 1,"<finalization enqueued=\"%zu\" />", eventData->finalizableCount);
	}

	writer->formatAndOutput(env, 0, "</gc-op>");
	writer->flush(env);
	exitAtomicReportingBlock();

	resetSyncGCStats();
}

void
MM_VerboseHandlerOutputRealtime::handleCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData)
{
	MM_VerboseHandlerOutput::handleCycleStart(hook, eventNum, eventData);
	/* set the gc phase to mark at the start of the cycle */
	_previousGCPhase = _gcPhase = PRE_COLLECT;
}

void
MM_VerboseHandlerOutputRealtime::handleCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData)
{
	MM_GCPostCycleEndEvent* cycleEndEvent = (MM_GCPostCycleEndEvent*)eventData;
	writeHeartbeatDataAndResetHeartbeatStats(MM_EnvironmentBase::getEnvironment(cycleEndEvent->currentThread), cycleEndEvent->timestamp);
	MM_VerboseHandlerOutput::handleCycleEnd(hook, eventNum, eventData);
	/* set the gc phase to inactive at the end of the cycle */
	_previousGCPhase = _gcPhase = INACTIVE;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_MarkStartEvent* eventData)
{
	_previousGCPhase = _gcPhase = MARK;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_MarkEndEvent* eventData)
{
	_previousGCPhase = MARK;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_ClassUnloadingStartEvent* eventData)
{
	_gcPhase = CLASS_UNLOAD;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_ClassUnloadingEndEvent* eventData)
{
	if (!incrementStartsInPreviousGCPhase()) {
		_previousGCPhase = CLASS_UNLOAD;
	}
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_SweepStartEvent* eventData)
{
	_gcPhase = SWEEP;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_SweepEndEvent* eventData)
{
	if (!incrementStartsInPreviousGCPhase()) {
		_previousGCPhase = SWEEP;
	}
	_gcPhase = POST_COLLECT;
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_OutOfMemoryEvent* eventData)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(eventData->currentThread);
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), j9time_current_time_millis());

	enterAtomicReportingBlock();
	writer->formatAndOutput(
		env, 0 /*indent*/,
		"<out-of-memory %s memorySpaceName=\"%s\" memorySpaceAddress=\"%p\" />",
		tagTemplate,
		eventData->memorySpaceString,
		eventData->memorySpace
	);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_UtilizationTrackerOverflowEvent* eventData)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(eventData->currentThread);
	writeHeartbeatDataAndResetHeartbeatStats(env, eventData->timestamp);
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), j9time_current_time_millis());

	enterAtomicReportingBlock();
	writer->formatAndOutput(
		env, 0 /*indent*/,
		"<utilization-tracker-overflow %s utilizationTrackerAddress=\"%p\" timeSliceDurationArrayAddress=\"%p\" timeSliceCursor=\"%zu\" />",
		tagTemplate,
		eventData->utilizationTrackerAddress,
		eventData->timeSliceDurationArrayAddress,
		eventData->timeSliceCursor
	);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputRealtime::handleEvent(MM_NonMonotonicTimeEvent* eventData)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(eventData->currentThread);
	writeHeartbeatDataAndResetHeartbeatStats(env, eventData->timestamp);
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), j9time_current_time_millis());

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<non-monotonic-time timerDescription=\"%s\" %s />", eventData->timerDesc, tagTemplate);
	writer->flush(env);
	exitAtomicReportingBlock();
}
