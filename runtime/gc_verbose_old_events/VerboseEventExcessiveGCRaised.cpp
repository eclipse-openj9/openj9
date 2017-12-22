
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
 
#include "VerboseEventExcessiveGCRaised.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include "j9modron.h"

/**
 * Create an new instance of a MM_VerboseEventExcessiveGCRaised event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventExcessiveGCRaised::newInstance(MM_ExcessiveGCRaisedEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventExcessiveGCRaised *eventObject;
			
	eventObject = (MM_VerboseEventExcessiveGCRaised *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventExcessiveGCRaised));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventExcessiveGCRaised(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventExcessiveGCRaised::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventExcessiveGCRaised::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	
	switch(_excessiveGCLevel) {	
	case excessive_gc_aggressive:
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"excessive gc activity detected, will attempt aggressive gc\" />");
		break;
	case excessive_gc_fatal:
	case excessive_gc_fatal_consumed:
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"excessive gc activity detected, will fail on allocate\" />");
		break;
	default:
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"excessive gc activity detected, unknown level: %d \" />", _excessiveGCLevel);
		break;
	}	
}
