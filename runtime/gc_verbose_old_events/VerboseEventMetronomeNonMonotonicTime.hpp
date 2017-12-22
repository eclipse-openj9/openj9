
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

#if !defined(EVENT_METRONOME_NON_MONOTONIC_TIME_HPP_)
#define EVENT_METRONOME_NON_MONOTONIC_TIME_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

#if defined(J9VM_GC_REALTIME)

/**
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventMetronomeNonMonotonicTime : public MM_VerboseEvent
{
private:
	U_64 _timeInMilliSeconds;
	const char* _timerDescription;
	
	void initialize(MM_NonMonotonicTimeEvent *event);
	
public:	
	static MM_VerboseEvent *newInstance(MM_NonMonotonicTimeEvent *event, J9HookInterface** hookInterface);

	virtual void consumeEvents(void);
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine(void) { return true; }
	MMINLINE virtual bool endsEventChain(void) { return true; }
	
	virtual bool isAtomic() { return true; }
	
	MM_VerboseEventMetronomeNonMonotonicTime(MM_NonMonotonicTimeEvent *event, J9HookInterface** hookInterface) :
		MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface),
		_timerDescription(event->timerDesc)
	{}
};

#endif /* J9VM_GC_REALTIME */

#endif /* EVENT_METRONOME_NON_MONOTONIC_TIME_HPP_ */
