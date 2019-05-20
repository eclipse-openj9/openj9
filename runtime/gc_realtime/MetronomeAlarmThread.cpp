 
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
 */

#include "omrutil.h"

#include "MetronomeAlarmThread.hpp"
#include "MetronomeDelegate.hpp"
#include "OSInterface.hpp"
#include "RealtimeGC.hpp"
#include "Timer.hpp"

#if defined(LINUX)
#if !defined(J9ZTPF)
#include <sys/signal.h>
#endif /* !defined(J9ZTPF) */
#include <sys/types.h>
#include <linux/unistd.h>
#endif


MM_MetronomeAlarmThread *
MM_MetronomeAlarmThread::newInstance(MM_EnvironmentBase *env)
{
	MM_MetronomeAlarmThread *alarmThread;
	
	alarmThread = (MM_MetronomeAlarmThread *)env->getForge()->allocate(sizeof(MM_MetronomeAlarmThread), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (alarmThread) {
		new(alarmThread) MM_MetronomeAlarmThread(env);
		if (!alarmThread->initialize(env)) {
			alarmThread->kill(env);
			return NULL;
		}
	}
	return alarmThread;
}

void
MM_MetronomeAlarmThread::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_MetronomeAlarmThread::tearDown(MM_EnvironmentBase *env)
{
	/* Shut down the alarm thread, if there is one running */
	omrthread_monitor_enter(_mutex);
	
	/* Set shutdown thread flag */
	_shutdown = true;

	while (_alarmThreadActive == ALARM_THREAD_ACTIVE) {
		omrthread_monitor_wait(_mutex);
	}
		
	omrthread_monitor_exit(_mutex);

	/* Kill the alarm only after the alarm thread is killed */
	if (NULL != _alarm) {
		_alarm->kill(env);
		_alarm = NULL;	
	}

	if (NULL != _mutex) {
		omrthread_monitor_destroy(_mutex);
		_mutex = NULL;	
	}
}

bool 
MM_MetronomeAlarmThread::initialize(MM_EnvironmentBase *env)
{
	if (0 != omrthread_monitor_init_with_name(&_mutex, 0, "Metronome Alarm Thread")) {
		return false;
	}

	/* TODO: The MM_Alarm classes don't quite have the newInstance/initialize
	 * pattern right, so we have to manually call initialize.
	 */
	_alarm = MM_Alarm::factory(env, _scheduler->_osInterface);
	if (!_alarm || !_alarm->initialize(env, this)) {
		return false;	
	}

	return true;
}

/**
 * C entrypoint for the newly created alarm thread.
 */
int J9THREAD_PROC
MM_MetronomeAlarmThread::metronomeAlarmThreadWrapper(void* userData)
{
	return MM_MetronomeDelegate::metronomeAlarmThreadWrapper(userData);
}

/**
 * Fork a separate thread to perform the periodic alarms.
 * Some may instantiate this class without actually calling this method
 * to create a separate thread.
 * @todo Clean up the abstraction so it makes a bit more sense. Probably
 * merging most of the code in this class into MM_Alarm.
 * @return true if the thread was successfully created, false otherwise
 */
bool
MM_MetronomeAlarmThread::startThread(MM_EnvironmentBase *env)
{
	uintptr_t startPriority;
	bool retCode;

	startPriority = J9THREAD_PRIORITY_MAX;

	if (J9THREAD_SUCCESS != createThreadWithCategory(
				&_thread,
				64 * 1024,
				startPriority,
				0,
				MM_MetronomeAlarmThread::metronomeAlarmThreadWrapper,
				this,
				J9THREAD_CATEGORY_SYSTEM_GC_THREAD)) {
		return false;
	}
	
	omrthread_monitor_enter(_mutex);
	while (_alarmThreadActive == ALARM_THREAD_INACTIVE) {
		omrthread_monitor_wait(_mutex);
	}
	retCode = (_alarmThreadActive == ALARM_THREAD_ACTIVE);
	omrthread_monitor_exit(_mutex);
	
	return retCode;
}

/**
 * C++ entrypoint for the newly-created alarm thread.
 */
void 
MM_MetronomeAlarmThread::run(MM_EnvironmentRealtime *env)
{
	omrthread_monitor_enter(_mutex);
	
	_alarmThreadActive = ALARM_THREAD_ACTIVE;
	omrthread_monitor_notify(_mutex);
		
	while (!_shutdown) {
		omrthread_monitor_exit(_mutex);

		_alarm->sleep();

		if (env->getTimer()->hasTimeElapsed(_scheduler->getStartTimeOfCurrentMutatorSlice(), _scheduler->beatNanos)) {
			_scheduler->continueGC(env, TIME_TRIGGER, 0, NULL, true);
		}

		omrthread_monitor_enter(_mutex);
	}
	omrthread_monitor_exit(_mutex);
}

