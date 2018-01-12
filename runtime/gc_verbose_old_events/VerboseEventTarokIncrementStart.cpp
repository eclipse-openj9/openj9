
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
#include "VerboseEventTarokIncrementStart.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_Verbose_AF_End event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventTarokIncrementStart::newInstance(MM_TarokIncrementStartEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventTarokIncrementStart *eventObject = (MM_VerboseEventTarokIncrementStart *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventTarokIncrementStart));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventTarokIncrementStart(event, hookInterface);
		eventObject->initialize();
	}
	return eventObject;
}

bool
MM_VerboseEventTarokIncrementStart::definesOutputRoutine()
{
	return true;
}

bool
MM_VerboseEventTarokIncrementStart::endsEventChain()
{
	return false;
}

void
MM_VerboseEventTarokIncrementStart::consumeEvents()
{
	_lastIncrementEndTime = _manager->getLastTarokIncrementEndTime();
	_manager->setLastTarokIncrementStartTime(_time);
}

void
MM_VerboseEventTarokIncrementStart::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	char timestamp[32];
	UDATA indentLevel = _manager->getIndentLevel();
	UDATA incrementCount = _incrementID;
	U_64 prevTime = (0 == incrementCount) ? _manager->getInitializedTime() : _lastIncrementEndTime;
	U_64 timeInMicroSeconds = omrtime_hires_delta(prevTime, _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, _timeInMilliSeconds);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<increment id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\">",
		incrementCount,
		timestamp,
		timeInMicroSeconds / 1000,
		timeInMicroSeconds % 1000
	);
	_manager->incrementIndent();

	/* output the common GC start info */
	gcStartFormattedOutput(agent);
}
