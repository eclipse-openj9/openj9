
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

#include "VerboseEventGlobalGCStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_VerboseEventGlobalGCStart event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventGlobalGCStart::newInstance(MM_GlobalGCIncrementStartEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventGlobalGCStart *eventObject;
	
	eventObject = (MM_VerboseEventGlobalGCStart *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventGlobalGCStart));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventGlobalGCStart(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventGlobalGCStart::consumeEvents(void)
{
	/* Consume global data */
	_lastGlobalTime = _manager->getLastGlobalGCTime();
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventGlobalGCStart::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	PORT_ACCESS_FROM_JAVAVM(((J9VMThread*)_omrThread->_language_vmthread)->javaVM);
	U_64 timeInMicroSeconds;
	U_64 prevTime;

	if (1 == _globalGCCount) {
		prevTime = _manager->getInitializedTime();
	} else {
		prevTime = _lastGlobalTime;
	}
	timeInMicroSeconds = j9time_hires_delta(prevTime, _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<gc type=\"global\" id=\"%zu\" totalid=\"%zu\" intervalms=\"%llu.%03.3llu\">",
		_globalGCCount,
		_localGCCount + _globalGCCount,
		timeInMicroSeconds / 1000,
		timeInMicroSeconds % 1000
	);
	
	_manager->incrementIndent();
}
