
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

#if !defined(EVENT_METRONOME__SYNCHRONOUS_GC_END_HPP_)
#define EVENT_METRONOME__SYNCHRONOUS_GC_END_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

#if defined(J9VM_GC_REALTIME)

class MM_VerboseEventMetronomeSynchronousGCEnd : public MM_VerboseEvent
{
private:
	GCReason _reason;
	UDATA _reasonParameter;	
	char _timestamp[32];		
	
	UDATA _heapFreeBefore;        /**< heap free memory at the time of matching Metronome_Synchronous_GC_Start event */
	UDATA _heapFreeAfter;	      /**< heap free memory at the time of this event */
	
	U_64 _startTime;  			  /**< timestamp of matching Metronome_Synchronous_GC_Start event */
	
	UDATA _classLoadersUnloadedStart; /**< Number of classLoaders unloaded just just before SyncGC started, but only for the current GC cycle */
	UDATA _classesUnloadedStart;      /**< Number of classes unloaded just just before SyncGC started, but only for the current GC cycle */
	UDATA _classLoadersUnloadedEnd;   /**< Number of classLoaders unloaded at the end of the current GC cycle */
	UDATA _classesUnloadedEnd;        /**< Number of classes unloaded at the end of the current GC cycle */
	
	UDATA _weakReferenceClearCount;    /**< number of weak references */
	UDATA _softReferenceClearCount;    /**< number of soft references */
	UDATA _softReferenceThreshold; /**< the max soft reference threshold */
	UDATA _dynamicSoftReferenceThreshold; /**< the dynamic soft reference threshold */
	UDATA _phantomReferenceClearCount; /**< number of phantom references */
	UDATA _finalizableCount;		   /**< number of finalizable objects */
	
	UDATA _workPacketOverflowCount; /**< number of workpackets that overflowed during this collect */
	UDATA _objectOverflowCount; /**< number of workpackets that overflowed during this collect */
	UDATA _gcThreadPriority; /**< Priority of the gc thread at the end of the synchronous GC. Note that the start of the synch GC is reported by the java thread and that's why the start priority isn't taken. */
	
	void initialize(MM_MetronomeSynchronousGCEndEvent *event);
	
public:	
	static MM_VerboseEvent *newInstance(MM_MetronomeSynchronousGCEndEvent *event, J9HookInterface** hookInterface);

	virtual void consumeEvents(void);
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine(void) { return true; }
	MMINLINE virtual bool endsEventChain(void) { return true; }
	
	MM_VerboseEventMetronomeSynchronousGCEnd(MM_MetronomeSynchronousGCEndEvent *event, J9HookInterface** hookInterface) :
		MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface),
		_heapFreeBefore(0),
		_heapFreeAfter(event->heapFree),
		_startTime(0),
		_classLoadersUnloadedStart(0),
		_classesUnloadedStart(),
		_classLoadersUnloadedEnd(event->classLoadersUnloaded),
		_classesUnloadedEnd(event->classesUnloaded),
		_weakReferenceClearCount(event->weakReferenceClearCount),
		_softReferenceClearCount(event->softReferenceClearCount),
		_softReferenceThreshold(event->softReferenceThreshold),
		_dynamicSoftReferenceThreshold(event->dynamicSoftReferenceThreshold),
		_phantomReferenceClearCount(event->phantomReferenceClearCount),
		_finalizableCount(event->finalizableCount),	
		_workPacketOverflowCount(event->workPacketOverflowCount),
		_objectOverflowCount(event->objectOverflowCount),
		_gcThreadPriority(0)
	{}
};

#endif /* J9VM_GC_REALTIME */

#endif /* EVENT_METRONOME__SYNCHRONOUS_GC_END_HPP_ */
