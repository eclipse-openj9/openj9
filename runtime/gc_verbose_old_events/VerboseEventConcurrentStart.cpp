
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
 
#include "VerboseEventConcurrentStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_VerboseEventConcurrentStart event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventConcurrentStart::newInstance(MM_ConcurrentCollectionStartEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventConcurrentStart *eventObject;
	
	eventObject = (MM_VerboseEventConcurrentStart *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventConcurrentStart));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventConcurrentStart(event, hookInterface);
		eventObject->initialize();
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventConcurrentStart::consumeEvents(void)
{
	/* Increment collection count */
	_manager->incrementConcurrentGCCount();
	
	/* Consume global data */
	_lastConTime = _manager->getLastConcurrentGCTime();
	_conCollectionCount = _manager->getConcurrentGCCount();
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventConcurrentStart::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	char timestamp[32];
	UDATA indentLevel = _manager->getIndentLevel();
	U_64 timeInMicroSeconds;
	U_64 prevTime;
	const char* cardCleaningReasonString = "";

	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, _timeInMilliSeconds);

	if (1 == _conCollectionCount) {
		prevTime = _manager->getInitializedTime();
	} else {
		prevTime = _lastConTime;
	}
	timeInMicroSeconds = omrtime_hires_delta(prevTime, _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<con event=\"collection\" id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\">",
		_conCollectionCount,
		timestamp,
		timeInMicroSeconds / 1000,
		timeInMicroSeconds % 1000
	);
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();

	/* output the common GC start info */
	gcStartFormattedOutput(agent);

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<stats tracetarget=\"%zu\">", _traceTarget);
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<traced total=\"%zu\" mutators=\"%zu\" helpers=\"%zu\" percent=\"%zu\" />",
		_tracedTotal,
		_tracedByMutators,
		_tracedByHelpers,
		_traceTarget == 0 ? 0 : (UDATA) ( ( (U_64) _tracedTotal * 100) / (U_64) _traceTarget)
	);

	switch (_cardCleaningReason) {
	case TRACING_COMPLETED:
		cardCleaningReasonString = "tracing completed";
		break;
	case CARD_CLEANING_THRESHOLD_REACHED:
		cardCleaningReasonString = "card cleaning threshold reached";
		break;
	default:
		cardCleaningReasonString = "unknown";
		break;
	}
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<cards cleaned=\"%zu\" kickoff=\"%zu\" reason=\"%s\" />",
		_cardsCleaned,
		_cardCleaningPhase1Threshold,
		cardCleaningReasonString
	);
	
	if(_workStackOverflowOccured) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"concurrent work stack overflow\" count=\"%zu\" />", _workStackOverflowCount);
	}

	if (_extensions->verboseExtensions) {
		/* 
		 * CMVC 119942
		 * 
		 * output stats about how many threads were scanned vs. how many were found at kickoff.
		 * The number actually scanned will usually be lower than the number found at kickoff
		 * because attached threads (like the JIT and the GC helper threads) won't typically be
		 * checking for async events
		 */
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<threads kickoff=\"%zu\" scanned=\"%zu\" />", _threadsToScanCount, _threadsScannedCount);
	}
	
	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</stats>");
}
