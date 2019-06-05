
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

#include "VerboseEventMetronomeSynchronousGCEnd.hpp"
#include "VerboseEventMetronomeSynchronousGCStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"
#include "gcutils.h"

#include <string.h>

#if defined(J9VM_GC_REALTIME)


/**
 * Create an new instance of a MM_VerboseEventMetronomeSynchronousGCEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventMetronomeSynchronousGCEnd::newInstance(MM_MetronomeSynchronousGCEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventMetronomeSynchronousGCEnd *eventObject;
	
	eventObject = (MM_VerboseEventMetronomeSynchronousGCEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventMetronomeSynchronousGCEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventMetronomeSynchronousGCEnd(event, hookInterface);
		eventObject->initialize(event);
	}
	return eventObject;
}

void
MM_VerboseEventMetronomeSynchronousGCEnd::initialize(MM_MetronomeSynchronousGCEndEvent *event)
{
	_reason = UNKOWN_REASON;
	_timestamp[0] = 0;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventMetronomeSynchronousGCEnd::consumeEvents(void)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);
	MM_VerboseManagerOld *manager = (MM_VerboseManagerOld*) extensions->verboseGCManager;
	MM_VerboseEventStream *eventStream = manager->getEventStream();	
	MM_VerboseEventMetronomeSynchronousGCStart *eventSyncGCStart = NULL;
	
	manager->incrementMetronomeSynchGCCount();		

	/* Find previous (matching) SyncGC start event */
	if (NULL != (eventSyncGCStart = (MM_VerboseEventMetronomeSynchronousGCStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_METRONOME_SYNCHRONOUS_GC_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
		/* Copy over all relevant attributes from SyncGC start event, to be used later in formattedOutput */
		_heapFreeBefore = eventSyncGCStart->getHeapFree();
		_startTime = eventSyncGCStart->getTimeStamp();
		strncpy(_timestamp, eventSyncGCStart->getTimestamp(), 32);		
		_reason = eventSyncGCStart->getReason();
		_reasonParameter = eventSyncGCStart->getReasonParameter();
		_classLoadersUnloadedStart = eventSyncGCStart->getClassLoadersUnloaded();
		_classesUnloadedStart = eventSyncGCStart->getClassesUnloaded();
		_gcThreadPriority = omrthread_get_priority(_omrThread->_os_thread);
	} else {
		//Stream is corrupted, what now?
	}	
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventMetronomeSynchronousGCEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);
	MM_VerboseManagerOld* manager = (MM_VerboseManagerOld*) extensions->verboseGCManager;
	
	U_64 timeSinceLastEvent;
	
	/* Get the timestamp of any of previous heartbeat or trigger event, needed to calculate intervalms.
	 * Intervalms reported will be between that previous event and the mayching syncGC start event (not end event!) */
	if (manager->getLastMetronomeTime()) {
		/* SyncGC due to OOM is preceded by a trigger start event, so that lastMetronomeTime is for sure non 0 */
		timeSinceLastEvent = omrtime_hires_delta(manager->getLastMetronomeTime(), _startTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	} else {
		/* SyncGC due to explicit GC may happen before any trigger start event, so lastMetronomeTime may or may not be 0 */
		timeSinceLastEvent = 0;
	}
	
	U_64 syncGCTimeDuration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&syncGCTimeDuration, _startTime, getTimeStamp());
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<gc type=\"synchgc\" id=\"%zu\" timestamp=\"%s\" intervalms=\"%llu.%03.3llu\">",
		manager->getMetronomeSynchGCCount(),
		_timestamp,  /* This is the timestamp of matching SyncGC start event! */
		timeSinceLastEvent / 1000,
		timeSinceLastEvent % 1000
	);
	manager->incrementIndent();
	
	const char *hookReason = getGCReasonAsString(_reason);

	if (_reason == OUT_OF_MEMORY_TRIGGER) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<details reason=\"%s\" requested_bytes=\"%zu\" />", hookReason, _reasonParameter);
	} else {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<details reason=\"%s\" />", hookReason);
	}

	if (!deltaTimeSuccess) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<duration timems=\"%llu.%03.3llu\" />", syncGCTimeDuration / 1000, syncGCTimeDuration % 1000);
	if ((0 != _workPacketOverflowCount) || (0 != _objectOverflowCount)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), _manager->getIndentLevel(), "<warning details=\"overflow occured\" packetCount=\"%zu\" directObjectCount=\"%zu\" />",
			_workPacketOverflowCount,
			_objectOverflowCount
		);
	}
	if (_classLoadersUnloadedEnd != _classLoadersUnloadedStart) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<classunloading classloaders=\"%zu\" classes=\"%zu\" />",
			_classLoadersUnloadedEnd - _classLoadersUnloadedStart,
			_classesUnloadedEnd - _classesUnloadedStart
		);	
	}
	if ((0 != _weakReferenceClearCount) || (0 != _softReferenceClearCount) || (0 != _phantomReferenceClearCount)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<refs_cleared soft=\"%zu\" threshold=\"%zu\" maxThreshold=\"%zu\" weak=\"%zu\" phantom=\"%zu\" />",
			_softReferenceClearCount,
			_dynamicSoftReferenceThreshold,
			_softReferenceThreshold,
			_weakReferenceClearCount,
			_phantomReferenceClearCount			
		);	
	}
	if (0 != _finalizableCount) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<finalization objectsqueued=\"%zu\" />",
			_finalizableCount
		);
	}	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<heap freebytesbefore=\"%zu\" />", _heapFreeBefore);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<heap freebytesafter=\"%zu\" />", _heapFreeAfter);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "<synchronousgcpriority value=\"%zu\" />", _gcThreadPriority);
	manager->decrementIndent();
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), manager->getIndentLevel(), "</gc>");
	manager->setLastMetronomeSynchGCTime(getTimeStamp());
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}

#endif /* J9VM_GC_REALTIME */
