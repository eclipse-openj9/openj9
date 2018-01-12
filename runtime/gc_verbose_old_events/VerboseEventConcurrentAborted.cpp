
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

#include "VerboseEventConcurrentAborted.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create an new instance of a MM_VerboseEventConcurrentAborted event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventConcurrentAborted::newInstance(MM_ConcurrentAbortedEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventConcurrentAborted *eventObject;
	
	eventObject = (MM_VerboseEventConcurrentAborted *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventConcurrentAborted));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventConcurrentAborted(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventConcurrentAborted::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventConcurrentAborted::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<con event=\"aborted\" reason=\"%s\" />", getReasonAsString());
}

/**
 * Return the reason for aborted concurrent collect as a string
 * @param reason reason code
 */
const char *
MM_VerboseEventConcurrentAborted::getReasonAsString()
{
	const char *result;

	switch((CollectionAbortReason)_reason) {
	case ABORT_COLLECTION_INSUFFICENT_PROGRESS:
		result = "insufficient progress made";
		break;
	case ABORT_COLLECTION_REMEMBERSET_OVERFLOW:
		result = "remembered set overflow";
		break;
	case ABORT_COLLECTION_SCAVENGE_REMEMBEREDSET_OVERFLOW:
		result = "scavenge remembered set overflow";
		break;
	case ABORT_COLLECTION_PREPARE_HEAP_FOR_WALK:
		result = "prepare heap for walk";
		break;
	default:
		result = "unknown";
	}

	return result;
}



