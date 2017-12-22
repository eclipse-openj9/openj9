
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

#if !defined(EVENT_METRONOME_GC_END_HPP_)
#define EVENT_METRONOME_GC_END_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

#if defined(J9VM_GC_REALTIME)

/**
 * Stores the data relating to the end of a global garbage collection.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventMetronomeGCEnd : public MM_VerboseEvent
{
	/*
	 * Data members
	 */
private:
	UDATA _startIncrementCount;
	UDATA _endIncrementCount;
	
	U_64 _maxIncrementTime;
	U_64 _minIncrementTime;
	U_64 _meanIncrementTime;
	
	UDATA _maxHeapFree;
	UDATA _minHeapFree;
	U_64 _meanHeapFree;

	UDATA _classLoadersUnloaded; /**< Snapshot of classloader unloaded count for each reported event (ie each Metronome GC increment) */
	UDATA _classLoadersUnloadedTotal; /**< Sum of _classLoadersUnloaded from all events between two heartbeat verbose gc reports */
	UDATA _classesUnloaded; /**< Snapshot of classes unloaded count for each reported event (ie each Metronome GC increment) */
	UDATA _classesUnloadedTotal; /**< Sum of _classesUnloaded from all events between two heartbeat verbose gc reports */
	
	UDATA _weakReferenceClearCount;    		/**< Number of weak references cleared between two GC quantas */
	UDATA _weakReferenceClearCountTotal;    /**< Number of weak references between two heartbeats (total of all quantas) */	
	UDATA _softReferenceClearCount;    		/**< Number of soft references cleared between two GC quantas */
	UDATA _softReferenceClearCountTotal;    /**< Number of soft references between two heartbeats (total of all quantas) */
	UDATA _softReferenceThreshold; /**< the max soft reference threshold */
	UDATA _dynamicSoftReferenceThreshold; /**< the dynamic soft reference threshold */
	UDATA _phantomReferenceClearCount;    	/**< Number of phantom references cleared between two GC quantas */
	UDATA _phantomReferenceClearCountTotal; /**< Number of phantom references between two heartbeats (total of all quantas) */	
	UDATA _finalizableCount;           		/**< Number of finalizable objects between two GC quantas*/
	UDATA _finalizableCountTotal;           /**< Number of finalizable objects  between two heartbeats (total of all quantas) */
	
	UDATA _workPacketOverflowCount; /**< Number of work packets that have been overflowed */
	UDATA _workPacketOverflowCountTotal; /**< Sum of _workPacketCount from all events between two heartbeat verbose gc reports */
	UDATA _objectOverflowCount; /**< Number of single objects that have been overflowed */
	UDATA _objectOverflowCountTotal; /**< Sum of _objectOverflowCount from all events between two heartbeat verbose gc reports */
		
	UDATA _nonDeterministicSweep;	  			/**< Number of non deterministic sweeps between two GC quantas */
	UDATA _nonDeterministicSweepTotal;			/**< Number of non deterministic sweeps between two heartbeats (total of all quantas) */
	UDATA _nonDeterministicSweepConsecutive;	/**< The longest streak of consecutive sweeps for one allocation, among all allocations between two quantas */
	UDATA _nonDeterministicSweepConsecutiveMax; /**< The maximum of all longest streaks, between two heartbeats */
	U_64 _nonDeterministicSweepDelay;			/**< The longest delay caused by non deterministic sweep among all allocs between two GC quantas */
	U_64 _nonDeterministicSweepDelayMax;		/**< The longest delay caused by non deterministic sweep among all allocs between two heartbeats */
	
	U_64 _maxExclusiveAccessTime;		 /**< The maximum time (of all events of the chain) GC master thread spent stopping mutators */
	U_64 _minExclusiveAccessTime;		 /**< The minimum time (of all events of the chain) GC master thread spent stopping mutators */
	U_64 _meanExclusiveAccessTime;		 /**< The average time (of all events of the chain) GC master thread spent stopping mutators */
	
	UDATA _maxStartPriority; /**< The maximum start priority of all increments in the event chain. Only used for the last event in the chain. */
	UDATA _minStartPriority; /**< The minimum start priority of all increments in the event chain. Only used for the last event in the chain. */

	I_64 _timeInMilliSeconds;
	
protected:
public:
	U_64 _incrementTime;
	UDATA _heapFree;
	
	/*
	 * Function members
	 */
private:	
	void initialize(void);
protected:
public:
	static MM_VerboseEvent *newInstance(MM_MetronomeIncrementEndEvent *event, J9HookInterface** hookInterface);
	
	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine();
	MMINLINE virtual bool endsEventChain();
	
	MM_VerboseEventMetronomeGCEnd(MM_MetronomeIncrementEndEvent *event, J9HookInterface** hookInterface)
		: MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface)
		, _startIncrementCount(0)
		, _endIncrementCount(0)
		, _maxIncrementTime(0)
		, _minIncrementTime(U_64_MAX)
		, _meanIncrementTime(0)
		, _maxHeapFree(0)
		, _minHeapFree(UDATA_MAX)
		, _meanHeapFree(0)
		, _classLoadersUnloaded(event->classLoadersUnloaded)
		, _classLoadersUnloadedTotal(0)
		, _classesUnloaded(event->classesUnloaded)
		, _classesUnloadedTotal(0)
		, _weakReferenceClearCount(event->weakReferenceClearCount)
		, _weakReferenceClearCountTotal(0)
		, _softReferenceClearCount(event->softReferenceClearCount)
		, _softReferenceClearCountTotal(0)
		, _softReferenceThreshold(event->softReferenceThreshold)
		, _dynamicSoftReferenceThreshold(event->dynamicSoftReferenceThreshold)
		, _phantomReferenceClearCount(event->phantomReferenceClearCount)
		, _phantomReferenceClearCountTotal(0)
		, _finalizableCount(event->finalizableCount)
		, _finalizableCountTotal(0)
		, _workPacketOverflowCount(event->workPacketOverflowCount)
		, _workPacketOverflowCountTotal(0)
		, _objectOverflowCount(event->objectOverflowCount)
		, _objectOverflowCountTotal(0)
		, _nonDeterministicSweep(event->nonDeterministicSweepCount)
		, _nonDeterministicSweepTotal(0)
		, _nonDeterministicSweepConsecutive(event->nonDeterministicSweepConsecutive)
		, _nonDeterministicSweepConsecutiveMax(0)		
		, _nonDeterministicSweepDelay(event->nonDeterministicSweepDelay)
		, _nonDeterministicSweepDelayMax(0)
		, _maxExclusiveAccessTime(0)
		, _minExclusiveAccessTime(UDATA_MAX)
		, _meanExclusiveAccessTime(0)			
		, _maxStartPriority(0)
		, _minStartPriority(UDATA_MAX)
		, _incrementTime(0)
		, _heapFree(event->heapFree)
	{}
};

#endif /* J9VM_GC_REALTIME */

#endif /* EVENT_METRONOME_GC_END_HPP_ */
