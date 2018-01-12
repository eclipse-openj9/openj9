
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
 
#include "j9modron.h"

#include "Debug.hpp"
#include "GCExtensions.hpp"
#include "ScanClassesMode.hpp"
#include "VerboseEventConcurrentKickOff.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_VerboseEventConcurrentKickOff event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventConcurrentKickOff::newInstance(MM_ConcurrentKickoffEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventConcurrentKickOff *eventObject;
	
	eventObject = (MM_VerboseEventConcurrentKickOff *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventConcurrentKickOff));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventConcurrentKickOff(event, hookInterface);
		eventObject->initialize();
	}
	return eventObject;
}

void
MM_VerboseEventConcurrentKickOff::initialize(void)
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
MM_VerboseEventConcurrentKickOff::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventConcurrentKickOff::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	char timestamp[32];
	UDATA indentLevel = _manager->getIndentLevel();
	
	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, _timeInMilliSeconds);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<con event=\"kickoff\" timestamp=\"%s\">", timestamp);
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<kickoff reason=\"%s\" />", getKickoffReasonAsString(_kickOffReason, _languageKickOffReason));

	if(static_cast<J9JavaVM*>(_omrThread->_vm->_language_vm)->memoryManagerFunctions->j9gc_scavenger_enabled(static_cast<J9JavaVM*>(_omrThread->_vm->_language_vm))) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<stats tenurefreebytes=\"%zu\" nurseryfreebytes=\"%zu\" tracetarget=\"%zu\" kickoff=\"%zu\"  />",
			_tenureFreeBytes,
			_nurseryFreeBytes,
			_traceTarget,
			_kickOffThreshold); 
	} else {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<stats tenurefreebytes=\"%zu\" tracetarget=\"%zu\" kickoff=\"%zu\" />",
			_tenureFreeBytes,
			_traceTarget,
			_kickOffThreshold);
	}
	
	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</con>");
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}

/**
 * Return the reason for kickoff as a string
 * @param mode reason code
 */
const char *
MM_VerboseEventConcurrentKickOff::getKickoffReasonAsString(UDATA reason, UDATA languageReason)
{
	if ((ConcurrentKickoffReason) reason == LANGUAGE_DEFINED_REASON) {

		switch (languageReason) {
			case FORCED_UNLOADING_CLASSES:
				return "Unloading of classes requested";
			case NO_LANGUAGE_KICKOFF_REASON:
				/* Should never be the case */
				assume(0,"No reason set for kickoff");
			default:
				return "unknown";
		}

	} else {

		switch((ConcurrentKickoffReason)reason) {
			case KICKOFF_THRESHOLD_REACHED:
				return "Kickoff threshold reached";
			case NEXT_SCAVENGE_WILL_PERCOLATE:
				return "Next scavenge will percolate";
			case NO_KICKOFF_REASON:
				/* Should never be the case */
				assume(0,"No reason set for kickoff");
			default:
				return "unknown";
		}

	}
}

