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

/**
 * @file
 * @ingroup GC_Metronome
 */

#if !defined(SCHEDULER_HPP_)
#define SCHEDULER_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "Base.hpp"
#include "GCCode.hpp"
#include "GCExtensionsBase.hpp"
#include "Metronome.hpp"
#include "ParallelDispatcher.hpp"
#include "YieldCollaborator.hpp"

class MM_OSInterface;
class MM_EnvironmentBase;
class MM_EnvironmentRealtime;
class MM_MemorySubSpaceMetronome;
class MM_Metronome;
class MM_RealtimeGC;
class MM_MetronomeAlarmThread;
class MM_Timer;
class MM_UtilizationTracker;

#define METRONOME_GC_ON 1
#define METRONOME_GC_OFF 0

/**
 * @todo Provide class documentation
 * @ingroup GC_Metronome
 */
class MM_Scheduler : public MM_ParallelDispatcher
{
	/*
	 * Data members
	 */
private:
	U_64 _mutatorStartTimeInNanos; /**< Time in nanoseconds when the mutator slice started.  This is updated at increment end and when a GC quantum is skipped due to shouldMutatorDoubleBeat */
	U_64 _incrementStartTimeInNanos; /**< Time in nanoseconds when the last gc increment started */
	MM_GCCode _gcCode; /**< The gc code that will be used for the next GC cycle.  If this is modified during a collect it will be unused.  This variable is reset at the end of every cycle to the default collection type */

protected:
public:
	bool _isInitialized; /**< Set to true when all threads have been started */
	volatile uintptr_t _sharedBarrierState;

	MM_YieldCollaborator *_yieldCollaborator;

	/*
	 * Cached value for shouldGCYield()
	 */
	volatile bool _shouldGCYield;

	/* When utilization is over 50%, multiple consecutive GC beats is not allowed.
	 * Double-beating is allowed between 33% and 50% utilization and so on.
	 * We must keep track of the number of consecutive beats dynamically to ensure this.
	 */
	I_32 _currentConsecutiveBeats;

	bool *_threadResumedTable; /**< Used to keep track of threads taken out of the suspended state when wakeUpThreads is called */

	bool _masterThreadMustShutDown; /**< Set when the master thread must shutdown */
	
	bool _exclusiveVMAccessRequired; /**< This flag is used by the master thread to see if it needs to get exclusive vm access */

	MM_MetronomeAlarmThread *_alarmThread;
	MM_EnvironmentRealtime *_threadWaitingOnMasterThreadMonitor;
	uintptr_t _mutatorCount;

	MM_RealtimeGC *_gc;
	OMR_VM *_vm;
	MM_GCExtensionsBase *_extensions;
 	bool _doSchedulingBarrierEvents;

	U_32 _gcOn;      /* Are we in some long GC cycle? */
	typedef enum {
		MUTATOR = 0,  /* master blocked on a monitor, mutators running */
		WAKING_GC,
		STOP_MUTATOR, /* master thread awake - waiting for mut threads to reach safe point */
		AWAIT_GC, /* waiting for other gc threads to reach initial barrier */
		RUNNING_GC,
		WAKING_MUTATOR, /* master thread still awake, mutators running */
		NUM_MODES
	} Mode;
	Mode _mode;
	uintptr_t _gcPhaseSet;
	/* requests (typically by a mutator) to complete GC synchronously */
	bool _completeCurrentGCSynchronously;
	/* copy of the request made by Master Thread at the beginning of very next GC increment */
	bool _completeCurrentGCSynchronouslyMasterThreadCopy;
	GCReason _completeCurrentGCSynchronouslyReason;
	uintptr_t _completeCurrentGCSynchronouslyReasonParameter;
	/* monitor used to suspend/resume master thread,
	 * but also to ensure atomic access/change of _completeCurrentGCSynchronously/_mode */
	omrthread_monitor_t _masterThreadMonitor;

	MM_OSInterface *_osInterface;

	/* Params generated from command-line options from mmparse */
	double window;
	double beat;
	U_64 beatNanos;
	double _staticTargetUtilization;

	MM_UtilizationTracker* _utilTracker;

	/*
	 * Function members
	 */
private:

protected:
	/**
	 * Overrides of functionality in MM_ParallelDispatcher
	 * @{
	 */
	virtual uintptr_t getThreadPriority();

	/**
	 * @copydoc MM_ParallelDispatcher::useSeparateMasterThread()
	 * Because Metronome requires all GC threads to begin an increment
	 * at the same location where they left off, it uses a separate
	 * thread as the GC master thread, instead of using one of the
	 * mutator threads.
	 */
	virtual bool useSeparateMasterThread() { return true; }

	virtual void wakeUpThreads(uintptr_t count);
	void wakeUpSlaveThreads(uintptr_t count);

	virtual void slaveEntryPoint(MM_EnvironmentBase *env);
	virtual void masterEntryPoint(MM_EnvironmentBase *env);

	bool internalShouldGCYield(MM_EnvironmentRealtime *env, U_64 timeSlack);

	/** @} */

public:
	void pushYieldCollaborator(MM_YieldCollaborator *yieldCollaborator) {
		_yieldCollaborator = yieldCollaborator->push(_yieldCollaborator);
	}
	void popYieldCollaborator() {
		_yieldCollaborator = _yieldCollaborator->pop();
	}

	void shutDownSlaveThreads();
	void shutDownMasterThread();
	void startGCIfTimeExpired(MM_EnvironmentBase *env);

	virtual bool condYieldFromGCWrapper(MM_EnvironmentBase *env, U_64 timeSlack = 0);

	uintptr_t incrementMutatorCount();

	uintptr_t getParameter(uintptr_t which, char *keyBuffer, I_32 keyBufferSize, char *valueBuffer, I_32 valueBufferSize);
	void showParameters(MM_EnvironmentBase *env);

	/**
	 * Set scheduling parameters on argument extensions object to
	 * simulate Stop-The-World GC.  We simulate STW by running time-based,
	 * with MMU, etc. parameters chosen so that the GC will not yield to the
	 * mutator until an entire GC cycle has completed.
	 */
	static void initializeForVirtualSTW(MM_GCExtensionsBase *ext);

	bool isInitialized() { return _isInitialized; }

	static MM_Scheduler *newInstance(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);
	bool initialize(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	virtual bool startUpThreads();
	virtual void shutDownThreads();

	virtual void prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task, uintptr_t threadCount);
	virtual void completeTask(MM_EnvironmentBase *env);

	void checkStartGC(MM_EnvironmentRealtime *env);
	void startGC(MM_EnvironmentBase *env);              /* Call when some space threshold is triggered. */
	void stopGC(MM_EnvironmentBase *env);
	bool isGCOn();
	bool shouldGCDoubleBeat(MM_EnvironmentRealtime *env);
	bool shouldMutatorDoubleBeat(MM_EnvironmentRealtime *env, MM_Timer *timer);
	void reportStartGCIncrement(MM_EnvironmentRealtime *env);
	void reportStopGCIncrement(MM_EnvironmentRealtime *env, bool isCycleEnd = false);
	void restartMutatorsAndWait(MM_EnvironmentRealtime *env);
	bool shouldGCYield(MM_EnvironmentRealtime *env, U_64 timeSlack);
	bool condYieldFromGC(MM_EnvironmentBase *env, U_64 timeSlack = 0);
	/* Low-level yielding from inside the Scheduler */
	void yieldFromGC(MM_EnvironmentRealtime *env, bool distanceChecked = false);
	void waitForMutatorsToStop(MM_EnvironmentRealtime *env);
	void startMutators(MM_EnvironmentRealtime *env);
	bool continueGC(MM_EnvironmentRealtime *, GCReason reason, uintptr_t reasonParameter, OMR_VMThread *_vmThread, bool doRequestExclusiveVMAccess);  /* Non-blocking and typicallly called by an alarm handler.  Returns 1 if we did resume GC (non-recursive). */
	void setGCPriority(MM_EnvironmentBase *env, uintptr_t priority); /* sets the priority for all gc threads */
	void completeCurrentGCSynchronously(MM_EnvironmentRealtime *env = NULL);

	uintptr_t verbose() { return _extensions->verbose; }
	uintptr_t debug() { return _extensions->debug; }

	virtual void recomputeActiveThreadCount(MM_EnvironmentBase *env);

	/* Time and Work Statistics */
	void startGCTime(MM_EnvironmentRealtime *env, bool isDoubleBeat);
	void stopGCTime(MM_EnvironmentRealtime *env);
	U_64 getStartTimeOfCurrentMutatorSlice() {return _mutatorStartTimeInNanos;}
	void setStartTimeOfCurrentMutatorSlice(U_64 time) {_mutatorStartTimeInNanos = time;}
	U_64 getStartTimeOfCurrentGCSlice() {return _incrementStartTimeInNanos;}
	void setStartTimeOfCurrentGCSlice(U_64 time) {_incrementStartTimeInNanos = time;}

	uintptr_t getActiveThreadCount() { return _activeThreadCount; }
	uintptr_t getTaskThreadCount(MM_EnvironmentBase *env);
	void setGCCode(MM_GCCode gcCode) {_gcCode = gcCode;}

	void collectorInitialized(MM_RealtimeGC *gc);

	MM_Scheduler(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize) :
		MM_ParallelDispatcher(env, handler, handler_arg, defaultOSStackSize),
		_mutatorStartTimeInNanos(J9CONST64(0)),
		_incrementStartTimeInNanos(J9CONST64(0)),
		_gcCode(J9MMCONSTANT_IMPLICIT_GC_DEFAULT),
		_isInitialized(false),
		_yieldCollaborator(NULL),
		_shouldGCYield(false),
		_currentConsecutiveBeats(0),
		_threadResumedTable(NULL),
		_masterThreadMustShutDown(false),
		_exclusiveVMAccessRequired(true),
		_alarmThread(NULL),
		_threadWaitingOnMasterThreadMonitor(NULL),
		_mutatorCount(0),
		_gc(NULL),
		_vm(env->getOmrVM()),
		_extensions(MM_GCExtensionsBase::getExtensions(_vm)),
		_doSchedulingBarrierEvents(false),
		_gcOn(METRONOME_GC_OFF),
		_mode(MUTATOR),
		_gcPhaseSet(GC_PHASE_IDLE),
		_completeCurrentGCSynchronously(false),
		_completeCurrentGCSynchronouslyMasterThreadCopy(false),
		_completeCurrentGCSynchronouslyReason(UNKOWN_REASON),
		_completeCurrentGCSynchronouslyReasonParameter(0),
		_masterThreadMonitor(NULL),
		_osInterface(NULL),
		window(),
		beat(),
		beatNanos(),
		_staticTargetUtilization(),
		_utilTracker(NULL)
	{
		_typeId = __FUNCTION__;
	}

	/*
	 * Friends
	 */
	friend class MM_EnvironmentRealtime;
};

#endif /* SCHEDULER_HPP_ */

