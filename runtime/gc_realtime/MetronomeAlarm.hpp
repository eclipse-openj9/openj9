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
 
#if !defined(METRONOME_ALARM_HPP_)
#define METRONOME_ALARM_HPP_ 1

/* @ddr_namespace: default */
/**
 * @file
 * @ingroup GC_Metronome
 */

class MM_EnvironmentBase;
class MM_ProcessorInfo;
class MM_MetronomeAlarmThread;

#include "omr.h"
#include "omrcfg.h"

#include "Base.hpp"
#include "GCExtensionsBase.hpp"

#if defined(WIN32)
#include "omrmutex.h"
#endif /* WIN32 */

#if defined(LINUX)
#include <signal.h>
#endif /* LINUX */

 /**
 * MM_Alarm
 * A hi-resolution alarm that Metronome can use to gain control periodically
 * This is an abstract class - you need to create a concrete alarm which is currently
 *  one of:
 *  MM_HRTAlarm (hi-resolution timer alarm - available on some Linux and AIX variants)
 *  MM_RTCAlarm (RTC device driver alarm - available on some Linux systems when run as root or sudo)
 *  MM_ITAlarm (interval timer alarm - default if nothing else available - not very high resolution)
 * Use the factory service createAlarm() to create the right initial alarm for the system
 */
class MM_Alarm : protected MM_BaseVirtual
{	
private:
protected:
	MM_Alarm()
	{
		_typeId = __FUNCTION__;
	}
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_GCExtensionsBase *_extensions;

public:
	virtual void kill(MM_EnvironmentBase *envModron);

	virtual bool initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread)=0;
	static MM_Alarm * factory(MM_EnvironmentBase *env, MM_OSInterface* osInterface);
	virtual void describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize)=0;

	virtual void sleep()=0;
	virtual void wakeUp(MM_MetronomeAlarmThread *) {};
};

class MM_HRTAlarm : public MM_Alarm
{
public:
	static MM_HRTAlarm * newInstance(MM_EnvironmentBase *env);

	MM_HRTAlarm() : MM_Alarm()
	{
		_typeId = __FUNCTION__;
	}
	virtual void sleep();
	virtual void describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize);
	virtual bool initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread);
};

class MM_RTCAlarm : public MM_Alarm
{
private:
#if defined(LINUX) && !defined(J9ZTPF)
	IDATA RTCfd;
#endif /* defined(LINUX) && !defined(J9ZTPF) */

public:
	static MM_RTCAlarm * newInstance(MM_EnvironmentBase *env);

	MM_RTCAlarm() : MM_Alarm()
	{
		_typeId = __FUNCTION__;
	}
	virtual void sleep();
	virtual void describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize);
	virtual bool initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread);
};

class MM_ITAlarm : public MM_Alarm
{
private:
#if defined(WIN32)
	UINT _uTimerId;
#endif /* WIN32 */
protected:
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	static MM_ITAlarm * newInstance(MM_EnvironmentBase *env);

	MM_ITAlarm() : MM_Alarm()
	{
		_typeId = __FUNCTION__;
	}
	virtual void sleep();
	virtual void describe(OMRPortLibrary* port, char *buffer, I_32 bufferSize);
	virtual bool initialize(MM_EnvironmentBase *env, MM_MetronomeAlarmThread* alarmThread);
	static void alarm_handler(MM_MetronomeAlarmThread *);
	virtual void wakeUp(MM_MetronomeAlarmThread *alarmThread) { alarm_handler(alarmThread); }

#if defined(LINUX) 
	static MM_MetronomeAlarmThread *alarmHandlerArgument;  /**< signal handlers do not get user-supplied args	 */
#endif /* LINUX */
};

#endif /* METRONOME_ALARM_HPP_ */
