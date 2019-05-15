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

#include "omr.h"
#include "omrcfg.h"

#include <string.h>

#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "Timer.hpp"
#include "ut_j9mm.h"

#include "UtilizationTracker.hpp"

/**
 * Create a new instance of the UtilizationTracker class.
 */
MM_UtilizationTracker*
MM_UtilizationTracker::newInstance(MM_EnvironmentBase *env, double timeWindow, U_64 maxSliceNano, double targetUtil)
{
	MM_UtilizationTracker *tracker;
	
	tracker = (MM_UtilizationTracker *)env->getForge()->allocate(sizeof(MM_UtilizationTracker), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (tracker) {
		new(tracker) MM_UtilizationTracker(env, timeWindow, maxSliceNano, targetUtil);
		if (!tracker->initialize(env)) {
			tracker->kill(env);		
			tracker = NULL;   			
		}
	}
	return tracker;
}

/**
 * Kill an instance of the UtilizationTracker class.
 */
void
MM_UtilizationTracker::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Teardown an instance of the UtilizationTracker class.
 */
void
MM_UtilizationTracker::tearDown(MM_EnvironmentBase *env)
{
}


/**
 * Initialize an instance of the UtilizationTracker class.
 */
bool
MM_UtilizationTracker::initialize(MM_EnvironmentBase *env)
{
	/* Set up dynamic utilization tracking */
	_nanosLeftInCurrentSlice = 0;
	_timeSliceCursor = 0;
	_totalSlices = UTILIZATION_WINDOW_SIZE;
	_timeSliceDuration[_timeSliceCursor] = _timeWindow;
	_timeSliceIsMutator[_timeSliceCursor++] = 1;
	
	return true;
}

/**
 *  Returns the current utilization target.  This is not necessarily a static quantity.
 */
double
MM_UtilizationTracker::getTargetUtilization()
{
	return _targetUtilization;
}

/**
 * Compacts the timeSlice array to two entries (1 for mutator, 1 for GC) since the
 * array will overflow on the next call to addTimeSlice if we do not.
 * Re-calculates the mutator utilization from the start of the timeSlice array.
 * 
 * @note Synchronization must be provided externally when calling this method.
 */
void
MM_UtilizationTracker::compactTimeSliceWindowAndUpdateCurrentUtil(MM_EnvironmentRealtime *env)
{
	IDATA i = 0;
	double mutatorTime = 0.0;
	
	for (i = 0; i < _totalSlices; i++) {
		if (_timeSliceIsMutator[i]) {
			mutatorTime = mutatorTime + _timeSliceDuration[i];
		}
	}
	
	_currentUtilization = mutatorTime / _timeWindow;
	
	/* Store the total mutator time */
	_timeSliceDuration[0] = mutatorTime;
	_timeSliceIsMutator[0] = true;
	/* Store the difference of _timeWindow and mutator time as the GC time */
	_timeSliceDuration[1] = _timeWindow - mutatorTime;
	_timeSliceIsMutator[1] = false;
	/* Set _timeSliceCursor to 2 since we just compacted the array to 2 entries */
	_timeSliceCursor = 2;
}

/**
 * Adds a time slice.  Calling this method acknowledges that a certain
 * amount of time has elapsed and is being charged to the GC or mutator.
 * 
 * @note Synchronization must be provided externally when calling this method.
 */
U_64
MM_UtilizationTracker::addTimeSlice(MM_EnvironmentRealtime *env, MM_Timer *timer, bool isMutator)
{
	assert1(env->isMasterThread());
	U_64 currentTime = timer->getTimeInNanos();
	double elapsed = 0;
	if (currentTime >= _lastUpdateTime) {
		elapsed = (currentTime - _lastUpdateTime) / 1e9;
	} else {
		elapsed = _timeWindow * (1.0 - _targetUtilization);
		isMutator = false;
	}
	
	_lastUpdateTime = currentTime;
	
	/* Add the new time slice */
	_timeSliceDuration[_timeSliceCursor] = elapsed;
	_timeSliceIsMutator[_timeSliceCursor++] = isMutator;

	/* Slide time window over to the most recent portion. */
	I_32 startCursor = 0;
	double remainder = elapsed;
	while (remainder > _timeSliceDuration[startCursor]) {
		remainder -= _timeSliceDuration[startCursor++];
	}
	_timeSliceDuration[startCursor] -= remainder;

	/* Normalize the active region to the right size by left-shifting. */
	for (I_32 i=startCursor; i<_timeSliceCursor; i++) {
		_timeSliceDuration[i-startCursor] = _timeSliceDuration[i];
		_timeSliceIsMutator[i-startCursor] = _timeSliceIsMutator[i];
	}
	_timeSliceCursor -= startCursor;
	
	/* Check for and report overflow. */
	if (_timeSliceCursor >= _totalSlices) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		Trc_MM_UtilizationTrackerOverFlow(env->getLanguageVMThread(), this, &_timeSliceDuration);
		TRIGGER_J9HOOK_MM_PRIVATE_UTILIZATION_TRACKER_OVERFLOW(env->getExtensions()->privateHookInterface, env->getOmrVMThread(), omrtime_hires_clock(),
		                                               J9HOOK_MM_PRIVATE_UTILIZATION_TRACKER_OVERFLOW, this, &_timeSliceDuration, _timeSliceCursor);
		compactTimeSliceWindowAndUpdateCurrentUtil(env);
	} else {
		updateCurrentUtil(env);
	}
	U_64 excessNanos = (U_64) (1e9 * ((_currentUtilization - _targetUtilization) * _timeWindow));
	_nanosLeftInCurrentSlice = (excessNanos < _maxGCSlice) ? excessNanos : _maxGCSlice;

	return currentTime;
}


/**
 * Re-calculates the mutator utilization from the start of the timeSlice array.
 * Also normalizes the window (writes to the first timeSlice) entry.
 * 
 * @note Synchronization must be provided externally when calling this method.
 */
void
MM_UtilizationTracker::updateCurrentUtil(MM_EnvironmentRealtime *env)
{

	/* Compute time spent in mutator and normalize window.  Normalization is necessary due to roundoff errors. */
	double sum = 0.0;
	double mutTime = 0.0;
	double delta;
		
	for (I_32 i=0; i<_timeSliceCursor; i++) {
		sum += _timeSliceDuration[i];
		if (_timeSliceIsMutator[i]) {
			mutTime += _timeSliceDuration[i];
		}
	}

	delta = _timeWindow - sum;
	_currentUtilization = mutTime / _timeWindow;
		
	_timeSliceDuration[0] += delta;
	assert1(_timeSliceDuration[0] < 1e10);
	
}

/**
 * Returns the current utilization but does not include time elapsed since the last call to addTimeSlice.
 */
double
MM_UtilizationTracker::getCurrentUtil()
{
	return _currentUtilization;
}

/**
 * Returns how many nano-seconds we can run for before we violate maxGCSlice or target utilization. 
 */
I_64
MM_UtilizationTracker::getNanosLeft(MM_EnvironmentRealtime *env, U_64 sliceStartTimeInNanos)
{
	I_64 nanosLeft = J9CONST64(0);
	U_64 nanosUsedInCurrentSlice = env->getTimer()->peekElapsedTime(sliceStartTimeInNanos);
	
	nanosLeft = (I_64) (_nanosLeftInCurrentSlice - nanosUsedInCurrentSlice);
	
	return nanosLeft;
}
