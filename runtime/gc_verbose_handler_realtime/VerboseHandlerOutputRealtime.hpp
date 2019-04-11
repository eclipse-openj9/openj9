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

#if !defined(VERBOSEHANDLEROUTPUTREALTIME_HPP_)
#define VERBOSEHANDLEROUTPUTREALTIME_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"
#include "mmhook.h"
#include "mmprivatehook.h"

#include "VerboseHandlerOutput.hpp"
#include "GCExtensions.hpp"

class MM_EnvironmentBase;

class MM_VerboseHandlerOutputRealtime : public MM_VerboseHandlerOutput
{
private:
	U_64 _verboseInitTimeStamp;

	/* Data for increments */
	U_64 _heartbeatStartTime;
	U_64 _incrementStartTime;

	UDATA _incrementCount;

	U_64 _maxIncrementTime;
	U_64 _maxIncrementStartTime;
	U_64 _minIncrementTime;
	U_64 _totalIncrementTime;

	U_64 _maxHeapFree;
	U_64 _minHeapFree;
	U_64 _totalHeapFree;

	UDATA _classLoadersUnloadedTotal; /**< Sum of _classLoadersUnloaded from all events between two heartbeat verbose gc reports */
	UDATA _classesUnloadedTotal; /**< Sum of _classesUnloaded from all events between two heartbeat verbose gc reports */
	UDATA _anonymousClassesUnloadedTotal; /**< Sum of _anonymousClassesUnloaded from all events between two heartbeat verbose gc reports */

	UDATA _weakReferenceClearCountTotal;    /**< Number of weak references between two heartbeats (total of all quantas) */
	UDATA _softReferenceClearCountTotal;    /**< Number of soft references between two heartbeats (total of all quantas) */
	UDATA _softReferenceThreshold; /**< the max soft reference threshold */
	UDATA _dynamicSoftReferenceThreshold; /**< the dynamic soft reference threshold */
	UDATA _phantomReferenceClearCountTotal; /**< Number of phantom references between two heartbeats (total of all quantas) */

	UDATA _finalizableCountTotal;           /**< Number of finalizable objects  between two heartbeats (total of all quantas) */

	UDATA _workPacketOverflowCountTotal; /**< Sum of _workPacketCount from all events between two heartbeat verbose gc reports */
	UDATA _objectOverflowCountTotal; /**< Sum of _objectOverflowCount from all events between two heartbeat verbose gc reports */

	UDATA _nonDeterministicSweepTotal;			/**< Number of non deterministic sweeps between two heartbeats (total of all quantas) */
	UDATA _nonDeterministicSweepConsecutiveMax; /**< The maximum of all longest streaks, between two heartbeats */
	U_64 _nonDeterministicSweepDelayMax;		/**< The longest delay caused by non deterministic sweep among all allocs between two heartbeats */

	U_64 _maxExclusiveAccessTime;		 /**< The maximum time (of all events of the chain) GC master thread spent stopping mutators */
	U_64 _minExclusiveAccessTime;		 /**< The minimum time (of all events of the chain) GC master thread spent stopping mutators */
	U_64 _totalExclusiveAccessTime;		 /**< The average time (of all events of the chain) GC master thread spent stopping mutators */

	UDATA _maxStartPriority; /**< The maximum start priority of all increments in the event chain. Only used for the last event in the chain. */
	UDATA _minStartPriority; /**< The minimum start priority of all increments in the event chain. Only used for the last event in the chain. */

	typedef enum {
		INACTIVE = 0,
		PRE_COLLECT,
		MARK,
		CLASS_UNLOAD,
		SWEEP,
		POST_COLLECT
	} GCPhase;

	GCPhase _gcPhase; /**< Current GC phase. */
	GCPhase _previousGCPhase; /**< The GC phase at the start of the current increment. Different from the current gcPhase if current increment spans across more than one gc phase. */

	/* Data for sync GC*/
	bool _syncGCTriggered; /**< Have we in a sync GC cycle right now? */
	U_64 _syncGCStartTime; /**< The start time of a sync GC */
	GCReason _syncGCReason; /**< The code for cause of the sync GC */
	UDATA _syncGCReasonParameter; /**< Additional info for the sync GC */
	U_64 _syncGCExclusiveAccessTime; /**< The exclusive access time of a sync GC */
	UDATA _syncGCStartHeapFree; /**< The free heap at the start of a sync GC */
	UDATA _syncGCStartImmortalFree; /**< The free immortal space at the start of a sync GC */
	UDATA _syncGCStartClassLoadersUnloaded; /**< The unloaded class loader count at the start of a sync GC */
	UDATA _syncGCStartClassesUnloaded; /**< The unloaded class count at the start of a sync GC */
	UDATA _syncGCStartAnonymousClassesUnloaded; /**< The unloaded anonymous class count at the start of a sync GC */

protected:
	J9HookInterface** _mmHooks;  /**< Pointers to the Hook interface */

public:

private:
	void resetHeartbeatStats()
	{
		_heartbeatStartTime = 0;
		_incrementStartTime = 0;
		_incrementCount = 0;
		_maxIncrementTime = 0;
		_maxIncrementStartTime = 0;
		_minIncrementTime = (U_64)-1;
		_totalIncrementTime = 0;
		_maxHeapFree = 0;
		_minHeapFree = (UDATA)-1;
		_totalHeapFree = 0;
		_classLoadersUnloadedTotal = 0;
		_classesUnloadedTotal = 0;
		_anonymousClassesUnloadedTotal = 0;
		_weakReferenceClearCountTotal = 0;
		_softReferenceClearCountTotal = 0;
		_dynamicSoftReferenceThreshold = 0;
		_softReferenceThreshold = 0;
		_phantomReferenceClearCountTotal = 0;
		_finalizableCountTotal = 0;
		_workPacketOverflowCountTotal = 0;
		_objectOverflowCountTotal = 0;
		_nonDeterministicSweepTotal = 0;
		_nonDeterministicSweepConsecutiveMax = 0;
		_nonDeterministicSweepDelayMax = 0;
		_maxExclusiveAccessTime = 0;
		_minExclusiveAccessTime = (UDATA)-1;
		_totalExclusiveAccessTime = 0;
		_maxStartPriority = 0;
		_minStartPriority = (UDATA)-1;
	}

	void resetSyncGCStats()
	{
		_syncGCTriggered = false;
		_syncGCStartTime = 0;
		_syncGCReason = UNKOWN_REASON;
		_syncGCReasonParameter = 0;
		_syncGCExclusiveAccessTime = 0;
		_syncGCStartHeapFree = 0;
		_syncGCStartImmortalFree = 0;
		_syncGCStartClassLoadersUnloaded = 0;
		_syncGCStartClassesUnloaded = 0;
		_syncGCStartAnonymousClassesUnloaded = 0;
	}

	const char *
	getGCPhaseAsString(GCPhase gcPhase)
	{
		switch(gcPhase) {
			case PRE_COLLECT:
				return "precollect";
			case MARK:
				return "mark";
			case CLASS_UNLOAD:
				return "classunload";
			case SWEEP:
				return "sweep";
			case POST_COLLECT:
				return "postcollect";
			default:
				return "unknown";
		}
	}

	/**
	 * Answers whether the current increment spans more than one GC phase.
	 */	
	bool
	incrementStartsInPreviousGCPhase()
	{
		return (_gcPhase != _previousGCPhase);
	}

protected:

	/**
	 * Answer a string representation of a given cycle type.
	 * @param[IN] cycle type
	 * @return string representing the human readable "type" of the cycle.
	 */	
	virtual const char *getCycleType(UDATA type);
	virtual void handleInitializedInnerStanzas(J9HookInterface** hook, UDATA eventNum, void* eventData);

	MM_VerboseHandlerOutputRealtime(MM_GCExtensions *extensions) :
		MM_VerboseHandlerOutput(extensions),
		_verboseInitTimeStamp(0),
		_heartbeatStartTime(0),
		_incrementStartTime(0),
		_incrementCount(0),
		_maxIncrementTime(0),
		_maxIncrementStartTime(0),
		_minIncrementTime((U_64)-1),
		_totalIncrementTime(0),
		_maxHeapFree(0),
		_minHeapFree((UDATA)-1),
		_totalHeapFree(0),
		_classLoadersUnloadedTotal(0),
		_classesUnloadedTotal(0),
		_anonymousClassesUnloadedTotal(0),
		_weakReferenceClearCountTotal(0),
		_softReferenceClearCountTotal(0),
		_softReferenceThreshold(0),
		_dynamicSoftReferenceThreshold(0),
		_phantomReferenceClearCountTotal(0),
		_finalizableCountTotal(0),
		_workPacketOverflowCountTotal(0),
		_objectOverflowCountTotal(0),
		_nonDeterministicSweepTotal(0),
		_nonDeterministicSweepConsecutiveMax(0),
		_nonDeterministicSweepDelayMax(0),
		_maxExclusiveAccessTime(0),
		_minExclusiveAccessTime((UDATA)-1),
		_totalExclusiveAccessTime(0),
		_maxStartPriority(0),
		_minStartPriority((UDATA)-1),
		_gcPhase(INACTIVE),
		_previousGCPhase(INACTIVE),
		_syncGCTriggered(false),
		_syncGCStartTime(0),
		_syncGCReason(UNKOWN_REASON),
		_syncGCReasonParameter(0),
		_syncGCExclusiveAccessTime(0),
		_syncGCStartHeapFree(0),
		_syncGCStartImmortalFree(0),
		_syncGCStartClassLoadersUnloaded(0),
		_syncGCStartClassesUnloaded(0),
		_syncGCStartAnonymousClassesUnloaded(0),
		_mmHooks(NULL)
	{};

	virtual bool initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual bool getThreadName(char *buf, UDATA bufLen, OMR_VMThread *vmThread);
	virtual void writeVmArgs(MM_EnvironmentBase* env);

public:
	static MM_VerboseHandlerOutput *newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager);

	void handleCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData);
	void handleCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData);

	void handleEvent(MM_MetronomeIncrementStartEvent* eventData);
	void handleEvent(MM_MetronomeIncrementEndEvent* eventData);
	void handleEvent(MM_MetronomeSynchronousGCStartEvent* eventData);
	void handleEvent(MM_MetronomeSynchronousGCEndEvent* eventData);
	void handleEvent(MM_MarkStartEvent* eventData);
	void handleEvent(MM_MarkEndEvent* eventData);
	void handleEvent(MM_SweepStartEvent* eventData);
	void handleEvent(MM_SweepEndEvent* eventData);
	void handleEvent(MM_ClassUnloadingStartEvent* eventData);
	void handleEvent(MM_ClassUnloadingEndEvent* eventData);
	void handleEvent(MM_OutOfMemoryEvent* eventData);
	void handleEvent(MM_UtilizationTrackerOverflowEvent* eventData);
	void handleEvent(MM_NonMonotonicTimeEvent* eventData);

	void writeHeartbeatData(MM_EnvironmentBase* env, U_64 timestamp);
	void writeHeartbeatDataAndResetHeartbeatStats(MM_EnvironmentBase* env, U_64 timestamp);

	virtual void enableVerbose();
	virtual void disableVerbose();
};

#endif /* VERBOSEHANDLEROUTPUTREALTIME_HPP_ */
