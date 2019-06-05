
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

#include "gcutils.h"
#include "modronnls.h"

#include <string.h>

#include "EnvironmentBase.hpp"
#include "VerboseEventAFEnd.hpp"
#include "VerboseEventAFStart.hpp"
#include "VerboseEventClassUnloadingEnd.hpp"
#include "VerboseEventClassUnloadingStart.hpp"
#include "VerboseEventCompactEnd.hpp"
#include "VerboseEventCompactStart.hpp"
#include "VerboseEventCompletedConcurrentSweep.hpp"
#include "VerboseEventConcurrentAborted.hpp"
#include "VerboseEventConcurrentEnd.hpp"
#include "VerboseEventConcurrentFinalCardCleaningEnd.hpp"
#include "VerboseEventConcurrentFinalCardCleaningStart.hpp"
#include "VerboseEventConcurrentHalted.hpp"
#include "VerboseEventConcurrentKickOff.hpp"
#include "VerboseEventConcurrentlyCompletedSweepPhase.hpp"
#include "VerboseEventConcurrentCompleteTracingEnd.hpp"
#include "VerboseEventConcurrentCompleteTracingStart.hpp"
#include "VerboseEventConcurrentRSScanEnd.hpp"
#include "VerboseEventConcurrentRSScanStart.hpp"
#include "VerboseEventConcurrentStart.hpp"
#include "VerboseEventCopyForwardAbortRaised.hpp"
#include "VerboseEventExcessiveGCRaised.hpp"
#include "VerboseEventGCInitialized.hpp"
#include "VerboseEventGlobalGCEnd.hpp"
#include "VerboseEventGlobalGCStart.hpp"
#include "VerboseEventHeapResize.hpp"
#include "VerboseEventLocalGCEnd.hpp"
#include "VerboseEventLocalGCStart.hpp"
#include "VerboseEventMarkEnd.hpp"
#include "VerboseEventMarkStart.hpp"
#include "VerboseEventMetronomeCycleEnd.hpp"
#include "VerboseEventMetronomeCycleStart.hpp"
#include "VerboseEventMetronomeGCEnd.hpp"
#include "VerboseEventMetronomeGCStart.hpp"
#include "VerboseEventMetronomeNonMonotonicTime.hpp"
#include "VerboseEventMetronomeSynchronousGCEnd.hpp"
#include "VerboseEventMetronomeSynchronousGCStart.hpp"
#include "VerboseEventMetronomeTriggerEnd.hpp"
#include "VerboseEventMetronomeTriggerStart.hpp"
#include "VerboseEventMetronomeOutOfMemory.hpp"
#include "VerboseEventMetronomeUtilizationTrackerOverflow.hpp"
#include "VerboseEventPercolateCollect.hpp"
#include "VerboseEventSweepEnd.hpp"
#include "VerboseEventSweepStart.hpp"
#include "VerboseEventSystemGCEnd.hpp"
#include "VerboseEventSystemGCStart.hpp"
#include "VerboseEventTarokIncrementEnd.hpp"
#include "VerboseEventTarokIncrementStart.hpp"
#include "VerboseFileLoggingOutput.hpp"
#include "GCExtensions.hpp"
#include "VerboseOutputAgent.hpp"
#include "VerboseStandardStreamOutput.hpp"
#include "VerboseTraceOutput.hpp"
#include "VerboseManagerOld.hpp"
#include "VerboseEventStream.hpp"


extern "C" {

/**
 * Generate a verbosegc event object
 * This function is called whenever a hook is received (all hooked events point at this function). A
 * different userData will be passed for each event type - this function pointer is then dereferenced
 * to instantiate the correct type.
 *
 * @param eventData Pointer to the event-specific structure containing the data for this hook.
 * @param userData void* function pointer to the newInstance() method of the event type for this hook.
 */
void
generateVerbosegcEvent(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_VerboseEventStream *eventStream;

	/* call the function pointer - this will call the newInstance()
	 * method that corresponds to the event type we have received. */
	MM_VerboseEvent *event = ((MM_VerboseEvent *(*)(void *, J9HookInterface**))userData)(eventData,hook);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->getThread());

	if (NULL != event) {
		MM_VerboseManagerOld *verboseManager = (MM_VerboseManagerOld*) MM_GCExtensions::getExtensions(event->getThread()->_vm)->verboseGCManager;
		eventStream = verboseManager->getEventStreamForEvent(event);

		eventStream->chainEvent(env, event);
		if (event->endsEventChain()) {
			eventStream->processStream(env);
		}
	}
}

}

/**
 * Create a new MM_VerboseManagerOld instance.
 * @return Pointer to the new MM_VerboseManagerOld.
 */
MM_VerboseManagerOld *
MM_VerboseManagerOld::newInstance(MM_EnvironmentBase *env, OMR_VM* vm)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(vm);
	
	MM_VerboseManagerOld *verboseManager = (MM_VerboseManagerOld *)extensions->getForge()->allocate(sizeof(MM_VerboseManagerOld), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (verboseManager) {
		new(verboseManager) MM_VerboseManagerOld(vm);
		if(!verboseManager->initialize(env)) {
			verboseManager->kill(env);
			verboseManager = NULL;
		}
	}
	return verboseManager;
}

/**
 * Kill the MM_VerboseManagerOld instance.
 * Tears down the related structures and frees any storage.
 */
void
MM_VerboseManagerOld::kill(MM_EnvironmentBase *env)
{
	tearDown(env);

	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	extensions->getForge()->free(this);
}

/**
 * Initializes the MM_VerboseManagerOld instance.
 */
bool
MM_VerboseManagerOld::initialize(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	_mmHooks = J9_HOOK_INTERFACE(extensions->hookInterface);
	_mmPrivateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	_omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	
	/* Create the Event Stream */
	if(NULL == (_eventStream = MM_VerboseEventStream::newInstance(env, this))) {
		return false;
	}
	
	_lastOutputTime = j9time_hires_clock();
	
	return true;
}

/**
 * Tear down the structures managed by the MM_VerboseManagerOld.
 * Tears down the event stream and output agents.
 */
void
MM_VerboseManagerOld::tearDown(MM_EnvironmentBase *env)
{
	disableVerboseGC();

	if(NULL != _eventStream) {
		_eventStream->kill(env);
		_eventStream = NULL;
	}
		
	MM_VerboseOutputAgent *agent = _agentChain;
	_agentChain = NULL;
	
	while(NULL != agent) {
		MM_VerboseOutputAgent *nextAgent = agent->getNextAgent();
		agent->kill(env);
		agent = nextAgent;
	}
}

/**
 * Get the event stream which the given event should be added to.
 */
MM_VerboseEventStream *
MM_VerboseManagerOld::getEventStreamForEvent(MM_VerboseEvent *event)
{
	MM_VerboseEventStream *eventStream;
	
	if(event->isAtomic()) {
		/* Use a thread-local event-stream */
		eventStream = MM_VerboseEventStream::newInstance(MM_EnvironmentBase::getEnvironment(event->getThread()), this);
		if(NULL == eventStream) {
			/* Error - use the master event stream and hope for the best */
			eventStream = _eventStream;
		} else {
			/* This is a one time event stream for an atomic event - when the event
			 * stream is done being processed, it should be disposed of immediately.
			 */
			eventStream->setDisposable(true);
		}
	} else {
		/* Use the master event-stream */
		eventStream = _eventStream;
	}
	
	return eventStream;
}

void
MM_VerboseManagerOld::closeStreams(MM_EnvironmentBase *env)
{
	MM_VerboseOutputAgent *agent = _agentChain;
	while(NULL != agent) {
		agent->closeStream(env);
		agent = agent->getNextAgent();
	}
}

/**
 * Adds an output agent to the output agent chain.
 * @param agent Pointer to the agent to be added.
 */
void
MM_VerboseManagerOld::chainOutputAgent(MM_VerboseOutputAgent *agent)
{
	agent->setNextAgent(_agentChain);
	_agentChain = agent;
}

void
MM_VerboseManagerOld::enableVerboseGC()
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrVM);
	
	if (!_hooksAttached){
		(*_omrHooks)->J9HookRegisterWithCallSite(_omrHooks, J9HOOK_MM_OMR_INITIALIZED, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventGCInitialized::newInstance);
		
		if (extensions->isMetronomeGC()) {
			enableVerboseGCRealtime();
		} else {
			enableVerboseGCNonRealtime();
		}
		
		if (extensions->isVLHGC()) {
			enableVerboseGCVLHGC();
		}

		_hooksAttached = true;
	}
}


void
MM_VerboseManagerOld::disableVerboseGC()
{
	if (_hooksAttached){
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrVM);
		(*_omrHooks)->J9HookUnregister(_omrHooks, J9HOOK_MM_OMR_INITIALIZED, generateVerbosegcEvent, NULL);
		
		if (extensions->isMetronomeGC()) {
			disableVerboseGCRealtime();
		} else {
			disableVerboseGCNonRealtime();
		}

		if (extensions->isVLHGC()) {
			disableVerboseGCVLHGC();
		}
		
		_hooksAttached = false;
		_indentationLevel = 0;
	}
}

void 
MM_VerboseManagerOld::enableVerboseGCRealtime()
{
#if defined(J9VM_GC_REALTIME)
	/* These are the hooks metronome is interested in
	 * TODO: this is a hack - we need a pluggable way to define different verbosegc systems (eg j2se, metronome, gclite)
	 */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeGCStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeGCEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeSynchronousGCStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeSynchronousGCEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeTriggerStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeTriggerEnd::newInstance);
	(*_omrHooks)->J9HookRegisterWithCallSite(_omrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeCycleStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeCycleEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_OUT_OF_MEMORY, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeOutOfMemory::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_UTILIZATION_TRACKER_OVERFLOW, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeUtilizationTrackerOverflow::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_NON_MONOTONIC_TIME, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMetronomeNonMonotonicTime::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventTarokIncrementStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventTarokIncrementEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_ABORT, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventCopyForwardAbortRaised::newInstance);
#endif /* J9VM_GC_REALTIME */
}

void 
MM_VerboseManagerOld::disableVerboseGCRealtime()
{
#if defined(J9VM_GC_REALTIME)
	/* Unregister metronome verbosegc hooks */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_METRONOME_TRIGGER_END, generateVerbosegcEvent, NULL);
	(*_omrHooks)->J9HookUnregister(_omrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_OUT_OF_MEMORY, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_UTILIZATION_TRACKER_OVERFLOW, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_NON_MONOTONIC_TIME, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_ABORT, generateVerbosegcEvent, NULL);
#endif /* J9VM_GC_REALTIME */
}

void
MM_VerboseManagerOld::enableVerboseGCNonRealtime()
{
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventGlobalGCStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventGlobalGCEnd::newInstance);

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMarkStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventMarkEnd::newInstance);

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventSweepStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventSweepEnd::newInstance);
#if defined(J9VM_GC_MODRON_COMPACTION)
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COMPACT_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventCompactStart::newInstance);
	(*_omrHooks)->J9HookRegisterWithCallSite(_omrHooks, J9HOOK_MM_OMR_COMPACT_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventCompactEnd::newInstance);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
#if defined(J9VM_GC_MODRON_SCAVENGER)
	(*_omrHooks)->J9HookRegisterWithCallSite(_omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventLocalGCStart::newInstance);
	(*_omrHooks)->J9HookRegisterWithCallSite(_omrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventLocalGCEnd::newInstance);
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventSystemGCStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventSystemGCEnd::newInstance);

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventAFStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventAFEnd::newInstance);

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_HEAP_RESIZE, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventHeapResize::newInstance);

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_KICKOFF, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentKickOff::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_ABORTED, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentAborted::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_HALTED, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentHalted::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentFinalCardCleaningStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentFinalCardCleaningEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentCompleteTracingStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentCompleteTracingEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentRSScanStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentRSScanEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentEnd::newInstance);

#if defined(J9VM_GC_CONCURRENT_SWEEP)
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENTLY_COMPLETED_SWEEP_PHASE, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventConcurrentlyCompletedSweepPhase::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COMPLETED_CONCURRENT_SWEEP, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventCompletedConcurrentSweep::newInstance);
#endif /* J9VM_GC_CONCURRENT_SWEEP */

	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventClassUnloadingStart::newInstance);
	(*_mmHooks)->J9HookRegisterWithCallSite(_mmHooks, J9HOOK_MM_CLASS_UNLOADING_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventClassUnloadingEnd::newInstance);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_PERCOLATE_COLLECT, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventPercolateCollect::newInstance);
#endif /* J9VM_GC_MODRON_SCAVENGER */

	(*_omrHooks)->J9HookRegisterWithCallSite(_omrHooks, J9HOOK_MM_OMR_EXCESSIVEGC_RAISED, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventExcessiveGCRaised::newInstance);
}

void
MM_VerboseManagerOld::disableVerboseGCNonRealtime()
{
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_END, generateVerbosegcEvent, NULL);

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_END, generateVerbosegcEvent, NULL);

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, generateVerbosegcEvent, NULL);
#if defined(J9VM_GC_MODRON_COMPACTION)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COMPACT_START, generateVerbosegcEvent, NULL);
	(*_omrHooks)->J9HookUnregister(_omrHooks, J9HOOK_MM_OMR_COMPACT_END, generateVerbosegcEvent, NULL);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
#if defined(J9VM_GC_MODRON_SCAVENGER)
	(*_omrHooks)->J9HookUnregister(_omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, generateVerbosegcEvent, NULL);
	(*_omrHooks)->J9HookUnregister(_omrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, generateVerbosegcEvent, NULL);
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_END, generateVerbosegcEvent, NULL);

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END, generateVerbosegcEvent, NULL);

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_HEAP_RESIZE, generateVerbosegcEvent, NULL);

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_KICKOFF, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_ABORTED, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_HALTED, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END, generateVerbosegcEvent, NULL);

#if defined(J9VM_GC_CONCURRENT_SWEEP)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENTLY_COMPLETED_SWEEP_PHASE, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COMPLETED_CONCURRENT_SWEEP, generateVerbosegcEvent, NULL);
#endif /* J9VM_GC_CONCURRENT_SWEEP */

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START, generateVerbosegcEvent, NULL);
	(*_mmHooks)->J9HookUnregister(_mmHooks, J9HOOK_MM_CLASS_UNLOADING_END, generateVerbosegcEvent, NULL);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_PERCOLATE_COLLECT, generateVerbosegcEvent, NULL);
#endif /* J9VM_GC_MODRON_SCAVENGER */

	(*_omrHooks)->J9HookUnregister(_omrHooks, J9HOOK_MM_OMR_EXCESSIVEGC_RAISED, generateVerbosegcEvent, NULL);
}

void
MM_VerboseManagerOld::enableVerboseGCVLHGC()
{
#if defined(J9VM_GC_VLHGC)
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_START, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventTarokIncrementStart::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_END, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventTarokIncrementEnd::newInstance);
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_ABORT, generateVerbosegcEvent, OMR_GET_CALLSITE(), (void *)MM_VerboseEventCopyForwardAbortRaised::newInstance);
#endif /* defined(J9VM_GC_VLHGC) */

}

void
MM_VerboseManagerOld::disableVerboseGCVLHGC()
{
#if defined(J9VM_GC_VLHGC)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_START, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_TAROK_INCREMENT_END, generateVerbosegcEvent, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_ABORT, generateVerbosegcEvent, NULL);
#endif /* defined(J9VM_GC_VLHGC) */
}

/**
 * Finds an agent of a given type in the event chain.
 * @param type Indicates the type of agent to return.
 * @return Pointer to an agent of the specified type.
 */
MM_VerboseOutputAgent *
MM_VerboseManagerOld::findAgentInChain(AgentType type)
{
	MM_VerboseOutputAgent *agent = _agentChain;
	
	while (agent){
		if (type == agent->getType()){
			return agent;
		}
		agent = agent->getNextAgent();
	}
	
	return NULL;
}

AgentType
MM_VerboseManagerOld::parseAgentType(MM_EnvironmentBase *env, char *filename, UDATA fileCount, UDATA iterations)
{
	if(NULL == filename) {
		return STANDARD_STREAM;
	}

	if(!strcmp(filename, "stderr") || !strcmp(filename, "stdout")) {
		return STANDARD_STREAM;
	}

	if(!strcmp(filename, "trace")) {
		return TRACE;
	}

	if(!strcmp(filename, "hook")) {
		return HOOK;
	}

	return FILE_LOGGING;
}

/**
 * Counts the number of output agents currently enabled.
 * @return the number of current output agents.
 */
UDATA
MM_VerboseManagerOld::countActiveOutputHandlers()
{
	MM_VerboseOutputAgent *agent = _agentChain;
	UDATA count = 0;
	
	while(agent) {
		if(agent->isActive()) {
			count += 1;
		}
		agent = agent->getNextAgent();
	}
	
	return count;
}

/**
 * Walks the output agent chain disabling the agents.
 */
void
MM_VerboseManagerOld::disableAgents()
{
	MM_VerboseOutputAgent *agent = _agentChain;
	
	while(agent) {
		agent->isActive(false);
		agent = agent->getNextAgent();
	}
}

/**
 * Configures verbosegc according to the parameters passed.
 * @param filename The name of the file or output stream to log to.
 * @param fileCount The number of files to log to.
 * @param iterations The number of gc cycles to log to each file.
 * @return true on success, false on failure
 */
bool
MM_VerboseManagerOld::configureVerboseGC(OMR_VM *omrVM, char *filename, UDATA fileCount, UDATA iterations)
{
	MM_EnvironmentBase env(omrVM);

	MM_VerboseOutputAgent *agent;

	disableAgents();

	AgentType type = parseAgentType(&env, filename, fileCount, iterations);

	agent = findAgentInChain(type);
	if(NULL != agent) {
		agent->reconfigure(&env, filename, fileCount, iterations);
	} else {
		switch(type) {
		case STANDARD_STREAM:
			agent = MM_VerboseStandardStreamOutput::newInstance(&env, filename);
			break;

		case TRACE:
			agent = MM_VerboseTraceOutput::newInstance(&env);
			break;

		case FILE_LOGGING:
			agent = MM_VerboseFileLoggingOutput::newInstance(&env, filename, fileCount, iterations);
			if (NULL == agent) {
				agent = findAgentInChain(STANDARD_STREAM);
				if (NULL != agent) {
					agent->isActive(true);
					return true;
				}
				/* if we failed to create a file stream and there is no stderr agenttry to create a stderr agent */
				agent = MM_VerboseStandardStreamOutput::newInstance(&env, NULL);
			}
			break;

		case HOOK:
			/* The hook method is only available to new verbose */
			return false;

		default:
			return false;
		}
	
		if(NULL == agent) {
			return false;
		}
	
		chainOutputAgent(agent);
	}

	agent->isActive(true);

	return true;
}

/**
 * Passes the event stream to each output agent in the output agent chain.
 */
void
MM_VerboseManagerOld::passStreamToOutputAgents(MM_EnvironmentBase *env, MM_VerboseEventStream *stream)
{
	MM_VerboseOutputAgent *agent = _agentChain;
	
	while(NULL != agent) {
		if(agent->isActive()) {
			agent->processEventStream(env, stream);
		}
		agent = agent->getNextAgent();
	}
}
