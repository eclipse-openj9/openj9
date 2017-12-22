
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

#include "VerboseEventClassUnloadingEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"
#include "VerboseEventClassUnloadingStart.hpp"

/**
 * Create an new instance of a MM_VerboseEventClassUnloadingEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventClassUnloadingEnd::newInstance(MM_ClassUnloadingEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventClassUnloadingEnd *eventObject;
	
	eventObject = (MM_VerboseEventClassUnloadingEnd *)MM_VerboseEvent::create((OMR_VMThread*)event->currentThread->omrVMThread, sizeof(MM_VerboseEventClassUnloadingEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventClassUnloadingEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */	
void
MM_VerboseEventClassUnloadingEnd::consumeEvents(void)
{
	MM_VerboseEventStream *eventStream = _manager->getEventStream();
	MM_VerboseEventClassUnloadingStart *event = NULL;
	
	if (NULL != (event = (MM_VerboseEventClassUnloadingStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_CLASS_UNLOADING_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
		_classUnloadingStartTime = event->getTimeStamp();
	} else {
		//Stream is corrupted, what now?
	}
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventClassUnloadingEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	U_64 timeInMicroSeconds;
	
	if (!getTimeDeltaInMicroSeconds(&timeInMicroSeconds, _classUnloadingStartTime, _time)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in classloadersunloaded timetakenms\" />");
	}
	
	if (_extensions->verboseExtensions) {
		U_64 setupTime, scanTime, postTime;

		getTimeDeltaInMicroSeconds(&setupTime, 0, _cleanUpClassLoadersStartTime);
		getTimeDeltaInMicroSeconds(&scanTime, 0, _cleanUpClassLoadersTime);
		getTimeDeltaInMicroSeconds(&postTime, 0, _cleanUpClassLoadersEndTime);

		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<classunloading classloaders=\"%zu\" classes=\"%zu\" timevmquiescems=\"%llu.%03.3llu\" setup=\"%llu.%03.3llu\" scan=\"%llu.%03.3llu\" post=\"%llu.%03.3llu\" totalms=\"%llu.%03.3llu\" />",
			_classLoadersUnloadedCount,
			_classesUnloadedCount,
			_classUnloadMutexQuiesceTime / 1000,
			_classUnloadMutexQuiesceTime % 1000,
			setupTime / 1000,
			setupTime % 1000,
			scanTime / 1000,
			scanTime % 1000,
			postTime / 1000,
			postTime % 1000,
			timeInMicroSeconds / 1000, 
			timeInMicroSeconds % 1000
		);
	} else {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<classunloading classloaders=\"%zu\" classes=\"%zu\" timevmquiescems=\"%llu.%03.3llu\" timetakenms=\"%llu.%03.3llu\" />",
			_classLoadersUnloadedCount,
			_classesUnloadedCount,
			_classUnloadMutexQuiesceTime / 1000,
			_classUnloadMutexQuiesceTime % 1000,
			timeInMicroSeconds / 1000, 
			timeInMicroSeconds % 1000
		);
	}
}
