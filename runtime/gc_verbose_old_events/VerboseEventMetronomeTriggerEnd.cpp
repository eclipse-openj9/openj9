
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

#include "VerboseEventMetronomeTriggerEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include <string.h>

#if defined(J9VM_GC_REALTIME)

/**
 * Create an new instance of a MM_VerboseEvent_Metronome_Trigger_End event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventMetronomeTriggerEnd::newInstance(MM_MetronomeTriggerEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventMetronomeTriggerEnd *eventObject;
	
	eventObject = (MM_VerboseEventMetronomeTriggerEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventMetronomeTriggerEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventMetronomeTriggerEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventMetronomeTriggerEnd::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventMetronomeTriggerEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);
	MM_VerboseManagerOld *manager = (MM_VerboseManagerOld*) extensions->verboseGCManager;
	char timestamp[32];
	/* Intervals for trigger end event is distance (in time) between the matching (same id) trigger start and this trigger end.
	 * Normally, there are a number of heartbeat or syncGC events in between */
	U_64 timeSinceLastTriggerStart = omrtime_hires_delta(manager->getLastMetronomeTriggerStartTime(), _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	
	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, omrtime_current_time_millis());
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<gc type=\"trigger end\" id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\" />",
		manager->getMetronomeTriggerCount(),
		timestamp,
		timeSinceLastTriggerStart / 1000,
		timeSinceLastTriggerStart % 1000
	);
	manager->setLastMetronomeTriggerEndTime(getTimeStamp());
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}

#endif /* J9VM_GC_REALTIME */
