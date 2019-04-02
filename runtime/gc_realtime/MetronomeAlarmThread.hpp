
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

#if !defined(METRONOMEALARMTHREAD_HPP_)
#define METRONOMEALARMTHREAD_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "ProcessorInfo.hpp"
#include "MetronomeAlarm.hpp"
#include "EnvironmentRealtime.hpp"

class MM_Timer;

class MM_MetronomeAlarmThread : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	MM_Alarm *_alarm;
	omrthread_monitor_t _mutex;
	volatile bool _shutdown;
	enum AlarmThreadActive {
			ALARM_THREAD_INACTIVE,
			ALARM_THREAD_ACTIVE,
			ALARM_THREAD_SHUTDOWN
	};
	volatile  AlarmThreadActive _alarmThreadActive;
	MM_Scheduler *_scheduler;

protected:
public:
	omrthread_t _thread; /**< Underlying port-library thread */
	
	/*
	 * Function members
	 */
private:
	static int J9THREAD_PROC metronomeAlarmThreadWrapper(void* userData);

protected:
	void tearDown(MM_EnvironmentBase *env);
	bool initialize (MM_EnvironmentBase *env);

public:
	static MM_MetronomeAlarmThread *newInstance(MM_EnvironmentBase *env); 
	virtual void kill(MM_EnvironmentBase *env);

	MM_Scheduler* getScheduler() const { return _scheduler; }

	bool startThread(MM_EnvironmentBase *env);
	virtual void run(MM_EnvironmentRealtime *env);
	
	MM_MetronomeAlarmThread(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		, _alarm(NULL)
		, _mutex(NULL)
		, _shutdown(false)
		, _alarmThreadActive(ALARM_THREAD_INACTIVE)
		, _scheduler((MM_Scheduler *)(MM_GCExtensionsBase::getExtensions(env->getOmrVM())->dispatcher))
		, _thread(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
	/*
	 * Friends
	 */
	friend class MM_Scheduler;
	friend class MM_MetronomeDelegate;
};

#endif /* METRONOMEALARMTHREAD_HPP_ */

