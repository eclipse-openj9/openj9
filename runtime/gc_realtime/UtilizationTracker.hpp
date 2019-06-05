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

#if !defined(UTILIZATIONTRACKER_HPP_)
#define UTILIZATIONTRACKER_HPP_

#include "Base.hpp"
#include "Metronome.hpp"

class MM_EnvironmentBase;
class MM_Metronome;
class MM_RealtimeGC;
class MM_Scheduler;


/**
 * This class tracks the current mutator utilization by remembering what executed (and the interleaving)
 * in the last time window.  When gang-scheduling (aka Metronome) there will be only one instance globally.
 *
 * @todo Potentially make the backing data-structure dynamic or dependent on the time-window size.
 *
 * @ingroup GC_Metronome
 */
class MM_UtilizationTracker : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	I_32 _totalSlices;                                    /**< How big is the time slice array?  */
	I_32 _timeSliceCursor;                                /**< How much of the time slice array is active? */
	double _timeWindow;                                   /**< How big of a time window in seconds is being considered?  Typically, 10ms. */
	double _targetUtilization;                            /**< The minimum target utilization we are trying to maintain.  Typically, 70%. */
	U_64 _maxGCSlice;                                     /**< The maximum we would like the GC to run at a time.  Typically, 500us. */
	U_64 _nanosLeftInCurrentSlice;                        /**< How many nanos are left in the current time slice before we violate either _maxGCSlice or utilization target. */
	double _currentUtilization;                           /**< The current utilization excluding the current incomplete time slice. */
	U_64 _lastUpdateTime;
	
	double _timeSliceDuration[UTILIZATION_WINDOW_SIZE];   /**< How long is the time slice in seconds? */
	bool _timeSliceIsMutator[UTILIZATION_WINDOW_SIZE];    /**< Is this time slice time spent in the mutator. */
	
protected:
public:
	
	/*
	 * Function members
	 */
private:
	void updateCurrentUtil(MM_EnvironmentRealtime *env);
	void compactTimeSliceWindowAndUpdateCurrentUtil(MM_EnvironmentRealtime *env);

protected:
public:

	static MM_UtilizationTracker *newInstance(MM_EnvironmentBase *env, double timeWindow, U_64 maxGCSlice, double targetUtil);
	bool initialize(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
	
	double getTargetUtilization();
	U_64 addTimeSlice(MM_EnvironmentRealtime *env, MM_Timer *timer, bool isMutator);
	double getCurrentUtil();
	I_64 getNanosLeft(MM_EnvironmentRealtime *env, U_64 sliceStartTimeInNanos);

	MM_UtilizationTracker(MM_EnvironmentBase *env, double timeWindow, U_64 maxGCSlice, double targetUtil)
		: MM_BaseVirtual()
		, _timeSliceCursor(0)
		, _timeWindow(timeWindow)
		, _targetUtilization(targetUtil)
		, _maxGCSlice(maxGCSlice)
		, _currentUtilization(1.0)
		, _lastUpdateTime(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* UTILIZATIONTRACKER_HPP_ */
