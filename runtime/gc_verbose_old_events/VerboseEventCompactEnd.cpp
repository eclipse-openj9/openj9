
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
 
#include "VerboseEventCompactEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

#include "j9modron.h"
#include "gcutils.h"

#if defined(J9VM_GC_MODRON_COMPACTION)

/**
 * Create an new instance of a MM_VerboseEventCompactEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventCompactEnd::newInstance(MM_CompactEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventCompactEnd *eventObject;
			
	eventObject = (MM_VerboseEventCompactEnd *)MM_VerboseEvent::create(event->omrVMThread, sizeof(MM_VerboseEventCompactEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventCompactEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventCompactEnd::consumeEvents(void)
{
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventCompactEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	
	if(COMPACT_PREVENTED_NONE == _compactionPreventedReason) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<compaction movecount=\"%zu\" movebytes=\"%zu\" reason=\"%s\" />",
			_movedObjects,
			_movedBytes,
			getCompactionReasonAsString((CompactReason)_compactionReason));
	} else {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"compaction prevented due to %s\" />",
			getCompactionPreventedReasonAsString((CompactPreventedReason)_compactionPreventedReason));
	}
}

#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
