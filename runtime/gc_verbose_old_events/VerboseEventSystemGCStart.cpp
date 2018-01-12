
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

#include "VerboseEventSystemGCStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_VerboseEventSystemGCStart event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventSystemGCStart::newInstance(MM_SystemGCStartEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventSystemGCStart *eventObject;
	
	eventObject = (MM_VerboseEventSystemGCStart *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventSystemGCStart));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventSystemGCStart(event, hookInterface);
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
MM_VerboseEventSystemGCStart::consumeEvents(void)
{
	/* Increment collection count */
	_manager->incrementSystemGCCount();
	
	/* Consume global data */
	_lastSysTime = _manager->getLastSystemGCTime();
	_sysCollectionCount = _manager->getSystemGCCount();
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventSystemGCStart::formattedOutput(MM_VerboseOutputAgent *agent)
{
	char timestamp[32];
	UDATA indentLevel = _manager->getIndentLevel();
	U_64 timeInMicroSeconds;
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	U_64 prevTime;

	if (1 == _sysCollectionCount) {
		prevTime = _manager->getInitializedTime();
	} else {
		prevTime = _lastSysTime;
	}
	timeInMicroSeconds = omrtime_hires_delta(prevTime, _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);

	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, _timeInMilliSeconds);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<sys id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\">",
		_sysCollectionCount,
		timestamp,
		timeInMicroSeconds / 1000,
		timeInMicroSeconds % 1000
	);
	
	_manager->incrementIndent();
	indentLevel = _manager->getIndentLevel();

	/* output the common GC start info */
	gcStartFormattedOutput(agent);
}
