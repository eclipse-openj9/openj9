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

#include "EnvironmentBase.hpp"
#include "MetronomeAlarmThread.hpp"
#include "OSInterface.hpp"
#include "Scheduler.hpp"

#include "MetronomeAlarm.hpp"

#if defined(LINUX)
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* #if defined(LINUX) */
#if defined(LINUX) && !defined(J9ZTPF)
#include <linux/rtc.h>
#include <sys/syscall.h>
#include <sys/signal.h>
#elif defined(J9ZTPF)
#include <signal.h>
#endif /* defined(LINUX) && !defined(J9ZTPF) */

MM_HRTAlarm *
MM_HRTAlarm::newInstance(MM_EnvironmentBase *env)
{
	MM_HRTAlarm * alarm;
	
	alarm = (MM_HRTAlarm *)env->getForge()->allocate(sizeof(MM_HRTAlarm), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (alarm) {
		new(alarm) MM_HRTAlarm();
	}
	return alarm;
}

MM_RTCAlarm *
MM_RTCAlarm::newInstance(MM_EnvironmentBase *env)
{
	MM_RTCAlarm * alarm;
	
	alarm = (MM_RTCAlarm *)env->getForge()->allocate(sizeof(MM_RTCAlarm), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (alarm) {
		new(alarm) MM_RTCAlarm();
	}
	return alarm;
}

MM_ITAlarm *
MM_ITAlarm::newInstance(MM_EnvironmentBase *env)
{
	MM_ITAlarm * alarm;
	
	alarm = (MM_ITAlarm *)env->getForge()->allocate(sizeof(MM_ITAlarm), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (alarm) {
		new(alarm) MM_ITAlarm();
	}
	return alarm;
}

/**
 * MM_Alarm::newInstance - create a new alarm suitable for the system.
 */
MM_Alarm*
MM_Alarm::factory(MM_EnvironmentBase *env, MM_OSInterface* osInterface)
{
	MM_Alarm *alarm = NULL;
	
	if (osInterface->hiresTimerAvailable()) {
		alarm = MM_HRTAlarm::newInstance(env);
	} else if (osInterface->itTimerAvailable()) {
		alarm = MM_ITAlarm::newInstance(env);
	}
	return alarm;
}


/**
 * initialize
 * Initialize the High Resolution Timer for use by Metronome. 
 * @return true if the high resolution timer was successfully initialized, false
 *  otherwise
 */
bool
MM_HRTAlarm::initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread)
{
	_extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	return alarmThread->startThread(env);
}

/**
 * initialize
 * Initialize the Linux real-time clock for use by Metronome. On 
 * non-Linux OS's, it is an error to be working with RTCAlarm's
 * This code has OS-specific code in it, but it will disappear in time
 * when we have HRT Alarm's running everywhere...
 * @return true if the real-time clock was successfully initialized, false
 *  otherwise
 */

bool
MM_RTCAlarm::initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread)
{
	_extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
#if defined(LINUX) && !defined(J9ZTPF)
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	RTCfd = open("/dev/rtc", O_RDONLY);
	if (RTCfd == -1) {
		if (_extensions->verbose >= 2) {
			omrtty_printf("Unable to open /dev/rtc\n");
		}
		goto error;
	}
	if ((ioctl(RTCfd, RTC_IRQP_SET, _extensions->RTC_Frequency) == -1)) {
		if (_extensions->verbose >= 2) {
			omrtty_printf("Unable to set IRQP for /dev/rtc\n");
		}
		goto error;
	}
	if (ioctl(RTCfd, RTC_IRQP_READ, &_extensions->RTC_Frequency)) {
		if (_extensions->verbose >= 2) {
			omrtty_printf("Unable to read IRQP for /dev/rtc\n");
		}
		goto error;			
	}
	if (ioctl(RTCfd, RTC_PIE_ON, 0) == -1) {
		if (_extensions->verbose >= 2) {
			omrtty_printf("Unable to enable PIE for /dev/rtc\n");
		}
		goto error;
	}

	return alarmThread->startThread(env);

error:
	if (_extensions->verbose >= 1) {
		omrtty_printf("Unable to use /dev/rtc for time-based scheduling\n");
	}
	return false;	
#else	
	return false;
#endif /* defined(LINUX) && !defined(J9ZTPF) */
}

void MM_ITAlarm::alarm_handler(MM_MetronomeAlarmThread *alarmThread) {
	MM_Scheduler *sched = alarmThread->getScheduler();
	sched->_osInterface->maskSignals();
	omrthread_resume(alarmThread->_thread);	
}

#if defined(WIN32)
static void CALLBACK
itAlarmThunk(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	MM_ITAlarm::alarm_handler((MM_MetronomeAlarmThread *)dwUser);
}
#endif /*WIN32*/


/**
 * Initialize the interval timer alarm. This timer is not
 * used except as a backup if HRT and RTC alarms aren't available.
 * This code has OS-specific code in it, but it will disappear in time
 * when we have HRT Alarm's running everywhere...
 * @return true if the real-time clock was successfully initialized, false
 *  otherwise
 */
bool
MM_ITAlarm::initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread)
{
	_extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	if (!alarmThread->startThread(env)) {
		return false;
	}
	
#if defined(WIN32)
	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		return false;
	}
	UINT uDesiredPeriod = (UINT)(_extensions->itPeriodMicro / 1000);
	UINT uPeriod = (uDesiredPeriod < tc.wPeriodMin) ? tc.wPeriodMin : uDesiredPeriod;
	UINT uDelay = uPeriod;
	if (timeBeginPeriod(uPeriod) != TIMERR_NOERROR) {
		/* Error beginning higher frequency period  */
		return false;
	}
	_uTimerId = (UINT)timeSetEvent(uDelay, uPeriod, itAlarmThunk, (DWORD_PTR)alarmThread, TIME_PERIODIC);
	if (_uTimerId == 0) {
		/* Error creating timer */
		timeEndPeriod(uPeriod);
		return false;
	}

	return true;
#else
	/* write code if we want this path on other platforms */
	return false;
#endif /* WIN32 */
}

void
MM_RTCAlarm::sleep()
{
#if defined(LINUX) && !defined(J9ZTPF)
	uintptr_t data;
	ssize_t readAmount = read(RTCfd, &data, sizeof(data));
	if (readAmount == -1) {
		perror("blocking read failed");
	}
#endif /* defined(LINUX) && !defined(J9ZTPF) */
}

void
MM_HRTAlarm::sleep()
{
	omrthread_nanosleep(_extensions->hrtPeriodMicro * 1000);
}

void
MM_ITAlarm::sleep()
{
	omrthread_suspend();
}

void MM_RTCAlarm::describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize) {
	OMRPORT_ACCESS_FROM_OMRPORT(port);
 	omrstr_printf(buffer, bufferSize, "RTC  (Period = %.2f us Frequency = %d Hz)", 1.0/_extensions->RTC_Frequency, _extensions->RTC_Frequency);
}

void MM_HRTAlarm::describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize) {
	OMRPORT_ACCESS_FROM_OMRPORT(port);
	 omrstr_printf(buffer, bufferSize, "High Resolution Timer (Period = %d us)", _extensions->hrtPeriodMicro);
}

void MM_ITAlarm::describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize) {
	OMRPORT_ACCESS_FROM_OMRPORT(port);
	omrstr_printf(buffer, bufferSize, "Interval Timer (Period = %d us)", _extensions->itPeriodMicro);
}

/**
 * kill
 * teardown the environment and free up storage associated with the object
 */
void
MM_Alarm::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * tearDown
 */
void
MM_Alarm::tearDown(MM_EnvironmentBase *env)
{
}

/**
 * tearDown
 */
void
MM_ITAlarm::tearDown(MM_EnvironmentBase *env)
{
#if defined(WIN32)
	if (_uTimerId != 0) {
		UINT uPeriod = (UINT)(_extensions->itPeriodMicro / 1000);
		timeKillEvent(_uTimerId);
		timeEndPeriod(uPeriod);
	}
#endif
	MM_Alarm::tearDown(env);
}
