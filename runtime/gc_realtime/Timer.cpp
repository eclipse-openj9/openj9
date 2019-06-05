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
#include "omrutilbase.h"

#include "EnvironmentRealtime.hpp"
#include "OSInterface.hpp"

#include "Timer.hpp"

/**
 * Create a new instance of the MM_Timer class.
 * @param timerDesc String describing the relative timer, should be static!
 */
MM_Timer*
MM_Timer::newInstance(MM_EnvironmentBase* env, MM_OSInterface* osInterface)
{
	MM_Timer* timer;
	
	timer = (MM_Timer*)env->getForge()->allocate(sizeof(MM_Timer), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != timer) {
		new(timer) MM_Timer();
		if (!timer->initialize(env, osInterface)) {
			timer->kill(env);
			timer = NULL;
		}
	}
	return timer;
}

/**
 * Kill an instance of the MM_Timer class.
 */
void
MM_Timer::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Teardown an instance of the MM_Timer class.
 */
void
MM_Timer::tearDown(MM_EnvironmentBase *env)
{
}


/**
 * Initialize an instance of the MM_Timer class.
 */
bool
MM_Timer::initialize(MM_EnvironmentBase* env, MM_OSInterface* osInterface)
{
	_osInterface = osInterface;
	reset();
	return true;
}

/**
 * Get the actual time in nanoseconds.
 */
U_64
MM_Timer::getTimeInNanos()
{
	return _osInterface->nanoTime();
}

/**
 * Reset the timer to the current time in nanoseconds
 */
void
MM_Timer::reset()
{
	rebaseTime();
}

/**
 * Get the approximate time in nanoseconds using the TSC.
 * If too much time as elapsed, or time has gone backwards since 
 * the last call to this function the Timer will rebase the time to
 * the actual time in nanoseconds.
 */
U_64
MM_Timer::nanoTime()
{
#if defined(AIXPPC) || defined(WIN32)
	return getTimeInNanos();
#else
	U_64 delta = J9CONST64(0);
	U_64 timeInNanos = _sytemTimeBase;
	U_64 ticks = getTimebase();
	
	if (ticks > _tickBase) {
		delta = ticks - _tickBase;
		if (delta > CLOCK_SWITCH_TICK_THRESHOLD) {
			timeInNanos = rebaseTime();
		} else {
			timeInNanos = _sytemTimeBase + ((1000 * delta) / _osInterface->_ticksPerMicroSecond);
		}
	} else {
		timeInNanos = rebaseTime();
	}
	
	return timeInNanos;
#endif /* AIXPPC */
}

/**
 * Return the elapsed time since timerBase
 * @param timerBase - The time to get elapsed from
 */
U_64
MM_Timer::peekElapsedTime(U_64 timerBase)
{
	U_64 currentTime = nanoTime();
	U_64 delta = J9CONST64(0);
	
	if (currentTime > timerBase) {
		delta = currentTime - timerBase;
	}
	
	return delta;
}

/**
 * Rebase the timer with the current TSC value and the actual
 * time in nanoseconds.
 */
U_64
MM_Timer::rebaseTime()
{
	_sytemTimeBase = _osInterface->nanoTime();
	_tickBase = getTimebase();
	
	return _sytemTimeBase;
}

/**
 * Has timeToWaitInNanos time elapsed since the last call to reset().
 */
bool
MM_Timer::hasTimeElapsed(U_64 startTimeInNanos, U_64 timeToWaitInNanos)
{
	U_64 currentTimeInNanos = nanoTime();
	U_64 elapsedTimeInNanos = J9CONST64(0);
	if (currentTimeInNanos > startTimeInNanos) {
		elapsedTimeInNanos = currentTimeInNanos - startTimeInNanos;
	}
	if (elapsedTimeInNanos > timeToWaitInNanos) {
		return true;
	}
	return false;
}
