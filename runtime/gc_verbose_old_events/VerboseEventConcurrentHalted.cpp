
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
 
#include "ConcurrentGCStats.hpp"
#include "GCExtensions.hpp"
#include "ScanClassesMode.hpp"
#include "VerboseEventConcurrentHalted.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include "j9modron.h"

/**
 * Create an new instance of a MM_VerboseEventConcurrentHalted event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventConcurrentHalted::newInstance(MM_ConcurrentHaltedEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventConcurrentHalted *eventObject;
	
	eventObject = (MM_VerboseEventConcurrentHalted *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventConcurrentHalted));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventConcurrentHalted(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventConcurrentHalted::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventConcurrentHalted::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
		
#define CONCURRENT_STATUS_BUFFER_LENGTH 32
	char statusBuffer[CONCURRENT_STATUS_BUFFER_LENGTH];
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(_omrThread);
	const char* statusString = MM_ConcurrentGCStats::getConcurrentStatusString(env, _executionMode, statusBuffer, CONCURRENT_STATUS_BUFFER_LENGTH);
#undef CONCURRENT_STATUS_BUFFER_LENGTH
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<con event=\"halted\" mode=\"%s\" state=\"%s\">",
			statusString, getConcurrentStateAsString(_isCardCleaningComplete, _scanClassesMode, _isTracingExhausted));
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<stats tracetarget=\"%zu\">", _traceTarget);
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<traced total=\"%zu\" mutators=\"%zu\" helpers=\"%zu\" percent=\"%zu\" />",
		_tracedTotal,
		_tracedByMutators,
		_tracedByHelpers,
		_traceTarget == 0 ? 0 : (UDATA) ( ( (U_64) _tracedTotal * 100) / (U_64) _traceTarget)
	);
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<cards cleaned=\"%zu\" kickoff=\"%zu\" />",
		_cardsCleaned,
		_cardCleaningThreshold
	);
	
	if(_workStackOverflowOccured) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"concurrent work stack overflow\" count=\"%zu\" />", _workStackOverflowCount);
	}
	
	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</stats>");
	
	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</con>");
}

/**
 * Return the concurrent reason as a string
 * @param isCardCleaningComplete Card cleaning complete condition
 * @param scanClassesMode Current scan classes state
 * $param isTracingExhausted Tracing complete condition
 */
const char *
MM_VerboseEventConcurrentHalted::getConcurrentStateAsString(UDATA isCardCleaningComplete, UDATA scanClassesMode, UDATA isTracingExhausted)
{
	if (0 == isCardCleaningComplete) {
		return "Card cleaning incomplete";
	}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	switch (scanClassesMode) {
		case MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED:
		case MM_ScanClassesMode::SCAN_CLASSES_CURRENTLY_ACTIVE:
			return "Class scanning incomplete";
		case MM_ScanClassesMode::SCAN_CLASSES_COMPLETE:
		case MM_ScanClassesMode::SCAN_CLASSES_DISABLED:
			break;
		default:
			return "Class scanning bad state";
	}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	
	if (0 == isTracingExhausted) {
		return "Tracing incomplete";
	}
	
	return "Complete";
}

