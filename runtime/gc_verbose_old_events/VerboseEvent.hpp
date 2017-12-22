
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
 
#if !defined(VERBOSE_EVENT_HPP_)
#define VERBOSE_EVENT_HPP_
 
#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "Base.hpp"
#include "GCExtensions.hpp"

class MM_EnvironmentBase;
class MM_VerboseEventStream;
class MM_VerboseOutputAgent;
class MM_VerboseManagerOld;

/**
 * Base class for MM_VerboseEvents.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEvent : public MM_Base
{
	/*
	 * Data members
	 */
private:
protected:
	OMR_VMThread *_omrThread;
	MM_GCExtensions *_extensions;
	MM_VerboseManagerOld *_manager;
	
	U_64 _time;
	UDATA _type;
	
	MM_VerboseEvent *_next;
	MM_VerboseEvent *_previous;
	
	J9HookInterface** _hookInterface; /* hook interface the event is registered with */

public:
	
	/*
	 * Function members
	 */
private:
protected:
public:
	MMINLINE MM_VerboseEvent *getNextEvent() { return _next; }
	MMINLINE MM_VerboseEvent *getPreviousEvent() { return _previous; }

	MMINLINE void setNextEvent(MM_VerboseEvent *nextEvent) { _next = nextEvent; }
	MMINLINE void setPreviousEvent(MM_VerboseEvent *previousEvent) { _previous = previousEvent; }
	
	virtual bool definesOutputRoutine() = 0;
	virtual bool endsEventChain() = 0;
	
	virtual bool isAtomic() { return false; } /**< Override to return true if event should be handled away from main event stream */
	
	MMINLINE OMR_VMThread* getThread() { return _omrThread; }
	MMINLINE U_64 getTimeStamp() { return _time; }
	MMINLINE UDATA getEventType() { return _type; }
	MMINLINE J9HookInterface** getHookInterface() { return _hookInterface; }
	
	MMINLINE bool getTimeDeltaInMicroSeconds(U_64 *timeInMicroSeconds, U_64 startTime, U_64 endTime)
	{
		if(endTime < startTime) {
			*timeInMicroSeconds = 0;
			return false;
		}
		OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
		
		*timeInMicroSeconds = omrtime_hires_delta(startTime, endTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
		return true;
	}
	
	static void *create(OMR_VMThread *omrVMThread, UDATA size);
	static void *create(J9VMThread *vmThread, UDATA size);
	virtual void kill(MM_EnvironmentBase *env);
	
	virtual void consumeEvents() = 0;
	virtual void formattedOutput(MM_VerboseOutputAgent *agent) = 0;
		
	MM_VerboseEvent(OMR_VMThread *omrVMThread, U_64 timestamp, UDATA type, J9HookInterface** hookInterface)
		: MM_Base()
		, _omrThread(omrVMThread)
		, _extensions(MM_GCExtensions::getExtensions(omrVMThread))
		, _manager((MM_VerboseManagerOld*)_extensions->verboseGCManager)
		, _time(timestamp)
		, _type(type)
		, _next(NULL)
		, _previous(NULL)
		, _hookInterface(hookInterface)
	{}
};

#endif /* VERBOSE_EVENT_HPP_ */
