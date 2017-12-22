
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

#if !defined(EVENT_CON_FCC_END_HPP_)
#define EVENT_CON_FCC_END_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

/**
 * Stores the data relating to the end of  concurrent final card cleaning.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventConcurrentFinalCardCleaningEnd : public MM_VerboseEvent
{
private:
	/* Passed Data */
	UDATA	_finalcleanedCards;	/**< the number of cards cleaned in final card cleaning */
	UDATA	_bytesTraced; /**< he number of bytes traced during card cleaning */
	UDATA	_workStackOverflowCount; /**< the current count of concurrent work stack overflows */
	
	/* Consumed Data */
	U_64	_conFinalCCStartTime; /**< timestamp of Final card cleaning start */
	UDATA	_workStackOverflowCountStart; /**< the count of concurrent work stacks overflows at start of final card clean */
public:

	static MM_VerboseEvent *newInstance(MM_ConcurrentCollectionCardCleaningEndEvent *event, J9HookInterface** hookInterface);
	
	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine() { return true; };
	MMINLINE virtual bool endsEventChain() { return false; };

	MM_VerboseEventConcurrentFinalCardCleaningEnd(MM_ConcurrentCollectionCardCleaningEndEvent *event, J9HookInterface** hookInterface) :
	MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface),
	_finalcleanedCards(event->finalcleanedCards),
	_bytesTraced(event->bytesTraced),
	_workStackOverflowCount(event->workStackOverflowCount)
	{};
};

#endif /* EVENT_CON_FCC_END_HPP_ */
