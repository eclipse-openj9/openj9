
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

#include "VerboseEventMetronomeCycleEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include <string.h>

#if defined(J9VM_GC_REALTIME)

/**
 * Create an new instance of a MM_VerboseEventMetronomeCycleEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventMetronomeCycleEnd::newInstance(MM_GCPostCycleEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventMetronomeCycleEnd *eventObject;
	
	eventObject = (MM_VerboseEventMetronomeCycleEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventMetronomeCycleEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventMetronomeCycleEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventMetronomeCycleEnd::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventMetronomeCycleEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);
	MM_VerboseManagerBase *manager = extensions->verboseGCManager;
	char timestamp[32];

	/* Intervalms for cycle end event is distance (in time) between the matching (same id) cycle start and this cycle end.
	 * Normally, there are a number of heartbeat or syncGC events in between */
	U_64 timeSinceLastCycleStart = omrtime_hires_delta(manager->getLastMetronomeCycleStartTime(), _time, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	
	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, omrtime_current_time_millis());
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<gc type=\"cycle end\" id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\" heapfreebytes=\"%zu\" />",
		manager->getMetronomeCycleCount(),
		timestamp,
		timeSinceLastCycleStart / 1000,
		timeSinceLastCycleStart % 1000,
		_heapFree
	);
	manager->setLastMetronomeCycleEndTime(getTimeStamp());
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}

#endif /* J9VM_GC_REALTIME */
