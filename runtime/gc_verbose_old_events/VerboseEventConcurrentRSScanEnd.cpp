
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
 
#include "VerboseEventConcurrentRSScanEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"
#include "VerboseEventConcurrentRSScanStart.hpp"

/**
 * Create an new instance of a MM_VerboseEventConcurrentRSScanEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventConcurrentRSScanEnd::newInstance(MM_ConcurrentRememberedSetScanEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventConcurrentRSScanEnd *eventObject;
	
	eventObject = (MM_VerboseEventConcurrentRSScanEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventConcurrentRSScanEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventConcurrentRSScanEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventConcurrentRSScanEnd::consumeEvents(void)
{
	MM_VerboseEventStream *eventStream = _manager->getEventStream();
	MM_VerboseEventConcurrentRSScanStart *event = NULL;
	
	if (NULL != (event = (MM_VerboseEventConcurrentRSScanStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
		_conRSScanStartTime = event->getTimeStamp();
		_workStackOverflowCountStart = event->getWorkStackOverflowCount();
	} else {
		//Stream is corrupted, what now?
	}
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventConcurrentRSScanEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	U_64 timeInMicroSeconds;

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<con event=\"remembered set scan\">");
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();

	if (!getTimeDeltaInMicroSeconds(&timeInMicroSeconds, _conRSScanStartTime, _time)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in stats timetakenms\" />");
	}
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<stats objectsfound=\"%zu\" traced=\"%zu\" timetakenms=\"%llu.%03.3llu\" />",
		_objectsFound,
		_bytesTraced,
		timeInMicroSeconds / 1000,
		timeInMicroSeconds % 1000
	);

	/* Has overflow count gone up since start of final card cleaning. 
	 * If so we have had further work stack overflows
	 */
	if(_workStackOverflowCount > _workStackOverflowCountStart) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"concurrent work stack overflow\" count=\"%zu\" />", 
			_workStackOverflowCount);
	}

	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</con>");
}
