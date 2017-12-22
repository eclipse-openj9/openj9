
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

#if !defined(EVENT_HEAP_RESIZE_HPP_)
#define EVENT_HEAP_RESIZE_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

/**
 * Stores the data relating to a resizing of the heap.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventHeapResize : public MM_VerboseEvent
{
private:
	/* passed data */
	UDATA _resizeType; /**< the type of resize */
	UDATA _subSpaceType; /**< the type of subspace, old or new */
	UDATA _ratio; /**< the percentage of time being spent in gc */
	UDATA _amount; /**< how many bytes have been requested */
	UDATA _newHeapSize; /**< the heap size following the resize */
	U_64 _timeTaken; /**< the time to resize the heap in ms */
	UDATA _reason; /**< the reason code for the resize */
	
	bool _consumed; /**< flag to indicate if this resize event has been consumed by another */
	
public:
	UDATA getSubSpaceType() { return _subSpaceType; };
	UDATA getAmount() { return _amount; };
	UDATA getNewHeapSize() { return _newHeapSize; };
	U_64 getTimeTaken() { return _timeTaken; };
	UDATA getReason() { return _reason; };
	
	void setConsumed(bool consumed) { _consumed = consumed; };

	static MM_VerboseEvent *newInstance(MM_HeapResizeEvent *event, J9HookInterface** hookInterface);

	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	/* this doesn't return true or false, because some RESIZE events produce
	 * output but some don't (they are consumed as resize events are merged) */
	MMINLINE virtual bool definesOutputRoutine() { return !_consumed; };
	
	MMINLINE virtual bool endsEventChain() { return false; };
		
	MM_VerboseEventHeapResize(MM_HeapResizeEvent *event, J9HookInterface** hookInterface) :
	MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface),
	_resizeType(event->resizeType),
	_subSpaceType(event->subSpaceType),
	_ratio(event->ratio),
	_amount(event->amount),
	_newHeapSize(event->newHeapSize),
	_timeTaken(event->timeTaken),
	_reason(event->reason),
	_consumed(false)
	{};
};

#endif /* EVENT_HEAP_RESIZE_HPP_ */
