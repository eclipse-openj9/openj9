

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

#include "VerboseEventTarokIncrementEnd.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_Verbose_AF_End event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventTarokIncrementEnd::newInstance(MM_TarokIncrementEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventTarokIncrementEnd *eventObject = (MM_VerboseEventTarokIncrementEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventTarokIncrementEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventTarokIncrementEnd(event, hookInterface);
		eventObject->initialize();
	}
	return eventObject;
}

bool
MM_VerboseEventTarokIncrementEnd::definesOutputRoutine()
{
	return true;
}

bool
MM_VerboseEventTarokIncrementEnd::endsEventChain()
{
	return true;
}

void
MM_VerboseEventTarokIncrementEnd::consumeEvents()
{
	_lastIncrementStartTime = _manager->getLastTarokIncrementStartTime();
	_manager->setLastTarokIncrementEndTime(_time);
}

void
MM_VerboseEventTarokIncrementEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	/* output the common GC start info */
	gcEndFormattedOutput(agent);
	U_64 timeInMicroSeconds = 0;

	UDATA indentLevel = _manager->getIndentLevel();
	if (!getTimeDeltaInMicroSeconds(&timeInMicroSeconds, _lastIncrementStartTime, _time + _exclusiveAccessTime)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in time totalms\" />");
	}
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<time totalms=\"%llu.%03.3llu\" />",
		timeInMicroSeconds / 1000, 
		timeInMicroSeconds % 1000
	);

	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</increment>");
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
	
}
