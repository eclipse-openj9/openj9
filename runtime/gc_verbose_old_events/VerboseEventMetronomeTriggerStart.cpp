
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "VerboseEventMetronomeTriggerStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include <string.h>

#if defined(J9VM_GC_REALTIME)

/**
 * Create an new instance of a MM_VerboseEventMetronomeTriggerStart event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventMetronomeTriggerStart::newInstance(MM_MetronomeTriggerStartEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventMetronomeTriggerStart *eventObject;
	
	eventObject = (MM_VerboseEventMetronomeTriggerStart *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventMetronomeTriggerStart));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventMetronomeTriggerStart(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventMetronomeTriggerStart::consumeEvents(void)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);
	MM_VerboseManagerOld *manager = (MM_VerboseManagerOld*) extensions->verboseGCManager;

	manager->incrementMetronomeTriggerCount();	

}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventMetronomeTriggerStart::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);
	MM_VerboseManagerOld *manager = (MM_VerboseManagerOld*) extensions->verboseGCManager;
	char timestamp[32];
	/* Intervalms for trigger start is the distance (in time) from previous trigger end and this trigger start.
	 * There should be no heartbeat neither syncGC/OOM events in between the two, since no GC should be happening in between.
	 * SyncGC due to explicit GC are possible between trigger end and trigger start */
	U_64 timeSinceLastTriggerEnd;
	U_64 prevTime;

	if (1 == manager->getMetronomeTriggerCount()) {
		prevTime = manager->getInitializedTime();
	} else {
		prevTime = manager->getLastMetronomeTriggerEndTime();
	}
	timeSinceLastTriggerEnd = omrtime_hires_delta(prevTime, _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	
	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, omrtime_current_time_millis());
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<gc type=\"trigger start\" id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\" />",
		manager->getMetronomeTriggerCount(),
		timestamp,
		timeSinceLastTriggerEnd / 1000,
		timeSinceLastTriggerEnd % 1000
	);
	manager->setLastMetronomeTriggerStartTime(getTimeStamp());
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}

#endif /* J9VM_GC_REALTIME */
