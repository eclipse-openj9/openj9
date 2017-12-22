
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

#include "VerboseEventHeapResize.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include "gcutils.h"
#include "j9modron.h"

/**
 * Create an new instance of a MM_VerboseEventHeapResize event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventHeapResize::newInstance(MM_HeapResizeEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventHeapResize *eventObject;
	
	eventObject = (MM_VerboseEventHeapResize *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventHeapResize));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventHeapResize(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventHeapResize::consumeEvents(void)
{
	MM_VerboseEventHeapResize *event = NULL;
	
	if(_consumed) {
		return;
	}
	
	MM_VerboseEvent *nextEvent = getNextEvent();
	while(NULL != nextEvent) {
		UDATA eventType = nextEvent->getEventType();
		J9HookInterface** hookInterface = nextEvent->getHookInterface();

		/* external event */
		if(hookInterface == _manager->getHookInterface())
		{
#if defined(J9VM_GC_MODRON_SCAVENGER)
			switch(eventType) {

			case J9HOOK_MM_OMR_LOCAL_GC_END:
				/* it's the end event, so we can just exit */
				return;
			default:
				/* a normal event - carry on looking for more RESIZE events to merge */
				break;
			}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

		} else { /* internal event */
			switch(eventType) {

			case J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_END:
			case J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END:
			case J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_END:
			case J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END:
				/* it's the end event, so we can just exit */
				return;
			
			case J9HOOK_MM_PRIVATE_HEAP_RESIZE:
				event = (MM_VerboseEventHeapResize *)nextEvent;
				/* we have a resize event, is it compatible? */
				if((_subSpaceType == event->getSubSpaceType()) && (_reason == event->getReason())) {
					/* the event is compatible so merge */
					_amount += event->getAmount();
					_newHeapSize = event->getNewHeapSize();
					_timeTaken += event->getTimeTaken();

					event->setConsumed(true);
				}
				break;
				
			default:
				/* a normal event - carry on looking for more RESIZE events to merge */
				break;
			}

		}
		
		nextEvent = nextEvent->getNextEvent();
	}
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventHeapResize::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	U_64 timeInMicroSeconds = omrtime_hires_delta(0, _timeTaken, J9PORT_TIME_DELTA_IN_MICROSECONDS);

	switch(_resizeType) {
		case HEAP_EXPAND:
			if(_amount) {
				if (0 == _ratio) {
					agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<expansion type=\"%s\" amount=\"%zu\" newsize=\"%zu\" timetaken=\"%llu.%03.3llu\" reason=\"%s\" />",
						(_subSpaceType == MEMORY_TYPE_OLD) ? "tenured" : "nursery",
						_amount,
						_newHeapSize,
						timeInMicroSeconds / 1000,
						timeInMicroSeconds % 1000,
						getExpandReasonAsString((ExpandReason)_reason)
					);
				} else {
					agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<expansion type=\"%s\" amount=\"%zu\" newsize=\"%zu\" timetaken=\"%llu.%03.3llu\" reason=\"%s\" gctimepercent=\"%zu\" />",
						(_subSpaceType == MEMORY_TYPE_OLD) ? "tenured" : "nursery",
						_amount,
						_newHeapSize,
						timeInMicroSeconds / 1000,
						timeInMicroSeconds % 1000,
						getExpandReasonAsString((ExpandReason)_reason),
						_ratio
					);
				}	
			} else {
				agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<expansion type=\"%s\" result=\"failed\" />",
						(_subSpaceType == MEMORY_TYPE_OLD) ? "tenured" : "nursery");
			}
			break;

		case HEAP_CONTRACT:
			if(_amount) {
				if (0 == _ratio) {
					agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<contraction type=\"%s\" amount=\"%zu\" newsize=\"%zu\" timetaken=\"%llu.%03.3llu\" reason=\"%s\" />",
						(_subSpaceType == MEMORY_TYPE_OLD) ? "tenured" : "nursery",
						_amount,
						_newHeapSize,
						timeInMicroSeconds / 1000,
						timeInMicroSeconds % 1000,
						getContractReasonAsString((ContractReason)_reason)
					);
				} else {
					agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<contraction type=\"%s\" amount=\"%zu\" newsize=\"%zu\" timetaken=\"%llu.%03.3llu\" reason=\"%s\" gctimepercent=\"%zu\" />",
						(_subSpaceType == MEMORY_TYPE_OLD) ? "tenured" : "nursery",
						_amount,
						_newHeapSize,
						timeInMicroSeconds / 1000,
						timeInMicroSeconds % 1000,
						getContractReasonAsString((ContractReason)_reason),
						_ratio
					);
				}		
			} else {
				agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<contraction type=\"%s\" result=\"failed\" />",
					(_subSpaceType == MEMORY_TYPE_OLD) ? "tenured" : "nursery");
			}
			break;
		}
}
