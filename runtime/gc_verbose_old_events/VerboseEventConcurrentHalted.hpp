
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

#if !defined(EVENT_CON_HALTED_HPP_)
#define EVENT_CON_HALTED_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

/**
 * Stores the data relating to an halted concurrent collection.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventConcurrentHalted : public MM_VerboseEvent
{
private:
	/* Passed Data */
	UDATA	_executionMode; /**< the concurrent execution mode at the time of the halt event */
	UDATA	_traceTarget; /**< the targetted number of bytes to be concurrently traced */
	UDATA	_tracedTotal; /**< the number of bytes concurrently traced */
	UDATA	_tracedByMutators; /**< the number of bytes traced by mutators */
	UDATA	_tracedByHelpers; /**< the number of bytes traced by helper threads */
	UDATA	_cardsCleaned; /**< the number of cards cleaned */
	UDATA	_cardCleaningThreshold; /**< the number of free bytes at which we wish to start the card cleaning phase */
	UDATA	_workStackOverflowOccured; /**< flag to indicate if workstack overflow has occured */
	UDATA	_workStackOverflowCount; /**< the number of times concurrent work stacks have overflowed */
	UDATA	_isCardCleaningComplete;
	UDATA	_scanClassesMode;
	UDATA	_isTracingExhausted;
	
public:

	const char *getConcurrentStatusAsString(UDATA mode);
	const char *getConcurrentStateAsString(UDATA isCardCleaningComplete, UDATA scanClassesMode, UDATA isTracingExhausted);

	static MM_VerboseEvent *newInstance(MM_ConcurrentHaltedEvent *event, J9HookInterface** hookInterface);
	
	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine() { return true; };
	MMINLINE virtual bool endsEventChain() { return false; };
		
	MM_VerboseEventConcurrentHalted(MM_ConcurrentHaltedEvent *event, J9HookInterface** hookInterface) :
	MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface),
	_executionMode(event->executionMode),
	_traceTarget(event->traceTarget),
	_tracedTotal(event->tracedTotal),
	_tracedByMutators(event->tracedByMutators),
	_tracedByHelpers(event->tracedByHelpers),
	_cardsCleaned(event->cardsCleaned),
	_cardCleaningThreshold(event->cardCleaningThreshold),
	_workStackOverflowOccured(event->workStackOverflowOccured),
	_workStackOverflowCount(event->workStackOverflowCount),
	_isCardCleaningComplete(event->isCardCleaningComplete),
	_scanClassesMode(event->scanClassesMode),
	_isTracingExhausted(event->isTracingExhausted)
	{};
};

#endif /* EVENT_CON_HALTED_HPP_ */
