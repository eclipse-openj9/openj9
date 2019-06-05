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
 
#include "VerboseEventGCInitialized.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include "j9modron.h"
#include "omrutil.h"

/**
 * Create an new instance of a MM_VerboseEventGCInitialized event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventGCInitialized::newInstance(MM_InitializedEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventGCInitialized *eventObject;
			
	eventObject = (MM_VerboseEventGCInitialized *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventGCInitialized));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventGCInitialized(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventGCInitialized::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventGCInitialized::formattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	UDATA indentLevel = _manager->getIndentLevel();
	char timestamp[32];
	jint i;
	JavaVMInitArgs* vmArgs = static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM->vmArgsArray->actualVMArgs;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(_omrThread->_vm);

	omrstr_ftime(timestamp, sizeof(timestamp), VERBOSEGC_DATE_FORMAT, omrtime_current_time_millis());
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<initialized timestamp=\"%s\" >", timestamp);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"gcPolicy\" value=\"%s\" />", _event.gcPolicy);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"maxHeapSize\" value=\"0x%zx\" />", _event.maxHeapSize);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"initialHeapSize\" value=\"0x%zx\" />", _event.initialHeapSize);
#if defined(OMR_GC_COMPRESSED_POINTERS)
	if (OMRVMTHREAD_COMPRESS_OBJECT_REFERENCES(_omrThread)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"compressedRefs\" value=\"true\" />");
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"compressedRefsDisplacement\" value=\"0x%zx\" />", 0);
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"compressedRefsShift\" value=\"0x%zx\" />", _event.compressedPointersShift);
	} else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	{
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"compressedRefs\" value=\"false\" />");
	}
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"pageSize\" value=\"0x%zx\" />", _event.heapPageSize);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"pageType\" value=\"%s\" />", _event.heapPageType);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"requestedPageSize\" value=\"0x%zx\" />", _event.heapRequestedPageSize);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"requestedPageType\" value=\"%s\" />", _event.heapRequestedPageType);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"gcthreads\" value=\"%zu\" />", _event.gcThreads);

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<system>");
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"physicalMemory\" value=\"%llu\" />", _event.physicalMemory);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"numCPUs\" value=\"%zu\" />", _event.numCPUs);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"architecture\" value=\"%s\" />", _event.architecture);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"os\" value=\"%s\" />", _event.os);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"osVersion\" value=\"%s\" />", _event.osVersion);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "</system>");

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<vmargs>");
	for (i = 0; i < vmArgs->nOptions; ++i) {
		char escapedXMLString[128];
		UDATA optLen = strlen(vmArgs->options[i].optionString);
		UDATA escapeConsumed = escapeXMLString(OMRPORTLIB, escapedXMLString, sizeof(escapedXMLString), vmArgs->options[i].optionString, optLen);
		const char* dots = (escapeConsumed < optLen) ? "..." : "";
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<vmarg name=\"%s%s\" value=\"0x%p\" />", escapedXMLString, dots, vmArgs->options[i].extraInfo);
	}
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "</vmargs>");

	if (extensions->isMetronomeGC()) {
#if defined(J9VM_GC_REALTIME)
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<metronome>");
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"beatsPerMeasure\" value=\"%zu\" />", _event.beat);
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"timeInterval\" value=\"%zu\" />", _event.timeWindow);
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"targetUtilization\" value=\"%zu\" />", _event.targetUtilization);
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"trigger\" value=\"0x%zx\" />", _event.gcTrigger);
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 2, "<attribute name=\"headRoom\" value=\"0x%zx\" />", _event.headRoom);
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "</metronome>");
#endif /* J9VM_GC_REALTIME */
	}
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel + 1, "<attribute name=\"numaNodes\" value=\"%zu\" />", _event.numaNodes);
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</initialized>");
	_manager->setInitializedTime(getTimeStamp());
	agent->endOfCycle(static_cast<J9VMThread*>(_omrThread->_language_vmthread));
}
