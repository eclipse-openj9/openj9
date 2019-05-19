
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

#include "VerboseEventMetronomeGCEnd.hpp"
#include "VerboseEventMetronomeGCStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#if defined(J9VM_GC_REALTIME)

/**
 * Create an new instance of a MM_VerboseEventMetronomeGCEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventMetronomeGCEnd::newInstance(MM_MetronomeIncrementEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventMetronomeGCEnd *eventObject;
	
	eventObject = (MM_VerboseEventMetronomeGCEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventMetronomeGCEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventMetronomeGCEnd(event, hookInterface);
		eventObject->initialize();
	}
	return eventObject;
}

void
MM_VerboseEventMetronomeGCEnd::initialize(void)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	_timeInMilliSeconds = omrtime_current_time_millis();
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventMetronomeGCEnd::consumeEvents(void)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	MM_VerboseEventStream *eventStream = _manager->getEventStream();
	MM_VerboseEvent *eventMetronomeGCStart = NULL, *event;

	eventMetronomeGCStart = getPreviousEvent();
	if((NULL != eventMetronomeGCStart) && (J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START == eventMetronomeGCStart->getEventType()) && (_manager->getPrivateHookInterface() == eventMetronomeGCStart->getHookInterface())) {
		_incrementTime = omrtime_hires_delta(eventMetronomeGCStart->getTimeStamp(), _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	} else {
		/* This GC end event does not have a corresponding GC start event - abort */
		return;
	}
	
	event = (MM_VerboseEvent *)this;
	while(NULL != (event = event->getNextEvent())) {
		/* Return if there is an event of the same type later in the chain - we only
		 * continue if we are the last of this type in the chain */
		if((event->getEventType() == getEventType()) && (event->getHookInterface() == getHookInterface())) {
			return;
		}
	}

	/* We are the last event of this type in the chain */

	_manager->incrementMetronomeHeartbeatCount();
	
	event = eventStream->getHead();
	while(NULL != event) {
		if((J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START == event->getEventType()) && (_manager->getPrivateHookInterface() == event->getHookInterface())) {
			_startIncrementCount += 1;
			UDATA startPriority = ((MM_VerboseEventMetronomeGCStart *)event)->_startPriority;
			_maxStartPriority = OMR_MAX(_maxStartPriority, startPriority);
			_minStartPriority = OMR_MIN(_minStartPriority, startPriority);
			_minExclusiveAccessTime = OMR_MIN(_minExclusiveAccessTime, ((MM_VerboseEventMetronomeGCStart *)event)->_exclusiveAccessTime);
			_meanExclusiveAccessTime += ((MM_VerboseEventMetronomeGCStart *)event)->_exclusiveAccessTime;
			_maxExclusiveAccessTime = OMR_MAX(_maxExclusiveAccessTime, ((MM_VerboseEventMetronomeGCStart *)event)->_exclusiveAccessTime);
			
		}
		if((J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_END == event->getEventType()) && (_manager->getPrivateHookInterface() == event->getHookInterface())) {
			_endIncrementCount += 1;
			
			_maxIncrementTime = OMR_MAX(_maxIncrementTime, ((MM_VerboseEventMetronomeGCEnd *)event)->_incrementTime);
			_meanIncrementTime += ((MM_VerboseEventMetronomeGCEnd *)event)->_incrementTime;
			_minIncrementTime = OMR_MIN(_minIncrementTime, ((MM_VerboseEventMetronomeGCEnd *)event)->_incrementTime);
			
			_maxHeapFree = OMR_MAX(_maxHeapFree, (((MM_VerboseEventMetronomeGCEnd *)event)->_heapFree));
			_meanHeapFree += ((MM_VerboseEventMetronomeGCEnd *)event)->_heapFree;
			_minHeapFree = OMR_MIN(_minHeapFree, (((MM_VerboseEventMetronomeGCEnd *)event)->_heapFree));
			
			_classLoadersUnloadedTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_classLoadersUnloaded;
			_classesUnloadedTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_classesUnloaded;	
			
			_weakReferenceClearCountTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_weakReferenceClearCount;
			_softReferenceClearCountTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_softReferenceClearCount;
			_phantomReferenceClearCountTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_phantomReferenceClearCount;
			_finalizableCountTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_finalizableCount;
			
			_workPacketOverflowCountTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_workPacketOverflowCount;
			_objectOverflowCountTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_objectOverflowCount;
			
			_nonDeterministicSweepTotal += ((MM_VerboseEventMetronomeGCEnd *)event)->_nonDeterministicSweep;
			_nonDeterministicSweepConsecutiveMax = OMR_MAX(_nonDeterministicSweepConsecutiveMax, ((MM_VerboseEventMetronomeGCEnd *)event)->_nonDeterministicSweepConsecutive);
			_nonDeterministicSweepDelayMax = OMR_MAX(_nonDeterministicSweepDelayMax, ((MM_VerboseEventMetronomeGCEnd *)event)->_nonDeterministicSweepDelay);
		}
		event = event->getNextEvent();
	}
	
	/* It's possible to have a differing amount of start and end increment events in the stream
	 * in certain cases, ex: OOMs. For this reason, we must count both the start and end events
	 * and use the appropriate divisor for calculating the mean values.
	 */
	if (0 != _startIncrementCount) {
		_meanExclusiveAccessTime /= _startIncrementCount;
	}
	
	if (0 != _endIncrementCount) {
		_meanIncrementTime /= _endIncrementCount;
		_meanHeapFree /= _endIncrementCount;
	} /* No else required, these are initialized to 0. */
}

bool
MM_VerboseEventMetronomeGCEnd::definesOutputRoutine()
{
	MM_VerboseEvent *event = (MM_VerboseEvent *)this;
	MM_VerboseEvent *previousEvent;

	previousEvent = getPreviousEvent();
	if((NULL == previousEvent) || (J9HOOK_MM_PRIVATE_METRONOME_INCREMENT_START != previousEvent->getEventType()) || (_manager->getPrivateHookInterface() != previousEvent->getHookInterface())) {
		/* This event does not have a corresponding start event, so we abort */
		return false;
	}

	while(NULL != (event = event->getNextEvent())) {
		/* Return false if there is an event of the same type later in the chain - we only
		 * define our output routine if we are the last of this type */
		if((event->getEventType() == getEventType()) && (event->getHookInterface() == getHookInterface())) {
			return false;
		}
	}
	return true;
}

bool
MM_VerboseEventMetronomeGCEnd::endsEventChain()
{
	/* We end the event chain when elapsed enough time (more than extensions->verbosegcCycleTime)
	 * since any of (whichever is the last one) trigger start, previous heartbeat or syncGC events */
	U_64 microSecondsSinceLastEvent = J9CONST64(0);
	if (getTimeDeltaInMicroSeconds(&microSecondsSinceLastEvent, _manager->getLastMetronomeTime(), _time)) {
		return ((microSecondsSinceLastEvent / 1000) >= _extensions->verbosegcCycleTime); 
	}
	
	/* If we detected time went backwards, don't output verbose. */
	return false;
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventMetronomeGCEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	char timestamp[32];
	/* Intervalms is reported as distance (in time) between this event and any of (whichever is the last one)
	 * trigger start, previous heartbeat or syncGC events */
	U_64 timeSinceLastEvent = omrtime_hires_delta(_manager->getLastMetronomeTime(), _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);

	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, _timeInMilliSeconds);
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<gc type=\"heartbeat\" id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\">",
		_manager->getMetronomeHeartbeatCount(),
		timestamp,  /* Reported timestamp is for the last event in the chain */
		timeSinceLastEvent / 1000,
		timeSinceLastEvent % 1000
	);
	_manager->incrementIndent();
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<summary quantumcount=\"%zu\">",
		_endIncrementCount
	);
	_manager->incrementIndent();
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<quantum minms=\"%llu.%03.3llu\" meanms=\"%llu.%03.3llu\" maxms=\"%llu.%03.3llu\" />",
		_minIncrementTime / 1000,
		_minIncrementTime % 1000,
		_meanIncrementTime / 1000,
		_meanIncrementTime % 1000,
		_maxIncrementTime / 1000,
		_maxIncrementTime % 1000
	);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<exclusiveaccess minms=\"%llu.%03.3llu\" meanms=\"%llu.%03.3llu\" maxms=\"%llu.%03.3llu\" />",
		_minExclusiveAccessTime / 1000,
		_minExclusiveAccessTime % 1000,
		_meanExclusiveAccessTime / 1000,
		_meanExclusiveAccessTime % 1000,
		_maxExclusiveAccessTime / 1000,
		_maxExclusiveAccessTime % 1000
	);		 
	if (0 != _classLoadersUnloadedTotal) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<classunloading classloaders=\"%zu\" classes=\"%zu\" />",
			_classLoadersUnloadedTotal,
			_classesUnloadedTotal
		);	
	}
	if ((0 != _weakReferenceClearCountTotal) || (0 != _softReferenceClearCountTotal) || (0 != _phantomReferenceClearCountTotal)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<refs_cleared soft=\"%zu\" threshold=\"%zu\" maxThreshold=\"%zu\" weak=\"%zu\" phantom=\"%zu\" />",
			_softReferenceClearCountTotal,
			_dynamicSoftReferenceThreshold,
			_softReferenceThreshold,
			_weakReferenceClearCountTotal,
			_phantomReferenceClearCountTotal			
		);	
	}
	if (0 != _finalizableCountTotal) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<finalization objectsqueued=\"%zu\" />",
			_finalizableCountTotal
		);
	}
	
	if ((0 != _workPacketOverflowCountTotal) || (0 != _objectOverflowCount)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<warning details=\"overflow occured\" packetCount=\"%zu\" directObjectCount=\"%zu\" />",
			_workPacketOverflowCountTotal,
			_objectOverflowCountTotal
		);
	}
	if (0 != _nonDeterministicSweepTotal) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<nondeterministicsweep  maxms=\"%llu.%03.3llu\" totalregions=\"%zu\" maxregions=\"%zu\" />",
			_nonDeterministicSweepDelayMax / 1000,
			_nonDeterministicSweepDelayMax % 1000,
			_nonDeterministicSweepTotal,
			_nonDeterministicSweepConsecutiveMax
		);
	}	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<heap minfree=\"%zu\" meanfree=\"%llu\" maxfree=\"%zu\" />",
		_minHeapFree,
		_meanHeapFree,
		_maxHeapFree
	);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<gcthreadpriority max=\"%zu\" min=\"%zu\" />",
		_maxStartPriority,
		_minStartPriority
	);
	
	_manager->decrementIndent();
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "</summary>");
	_manager->decrementIndent();
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "</gc>");
	_manager->setLastMetronomeHeartbeatTime(_time);
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}

#endif /* J9VM_GC_REALTIME */
