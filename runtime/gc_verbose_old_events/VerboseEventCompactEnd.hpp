
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

#if !defined(EVENT_COMPACT_END_HPP_)
#define EVENT_COMPACT_END_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#if defined(J9VM_GC_MODRON_COMPACTION)

#include "GCExtensions.hpp"
#include "VerboseEvent.hpp"

/**
 * Stores the data relating to the end of a compaction.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventCompactEnd : public MM_VerboseEvent
{
private:
	/* Passed Data */
	UDATA	_movedObjects; /**< the number of objects moved */
	UDATA	_movedBytes; /**< the number of bytes moved */
	UDATA	_compactionReason; /**< the reason code for the compaction */
	UDATA	_compactionPreventedReason; /**< the reason code for why the compaction was not run */
	
public:

	static MM_VerboseEvent *newInstance(MM_CompactEndEvent *event, J9HookInterface** hookInterface);
	
	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine() { return true; };
	MMINLINE virtual bool endsEventChain() { return false; };

	MM_VerboseEventCompactEnd(MM_CompactEndEvent *event, J9HookInterface** hookInterface) :
	MM_VerboseEvent(event->omrVMThread, event->timestamp, event->eventid, hookInterface),
	_movedObjects(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.compactStats._movedObjects),
	_movedBytes(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.compactStats._movedBytes),
	_compactionReason(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.compactStats._compactReason),
	_compactionPreventedReason(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.compactStats._compactPreventedReason)
	{};
};

#endif /* J9VM_GC_MODRON_COMPACTION */
#endif /* EVENT_COMPACT_END_HPP_ */
