/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#if !defined(YIELDCOLLABORATOR_HPP_)
#define YIELDCOLLABORATOR_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "EnvironmentBase.hpp"

class MM_YieldCollaborator : public MM_BaseNonVirtual {
	
public:
	
	enum ResumeEvent {
		fromYield = 1, /* master notifies slaves to start working */
		synchedThreads = 2,  /* one GC thread notifies other threads, it's the last one to reach blocking or yield point */
		notifyMaster = 3, /* notify only master that all other GC threads synched or yielded */
		newPacket = 4 /* notify a GC thread that a new packeted is created */
	};
	
	/* This is just a debug info.
	   For example, if ever a deadlock occurs, and some threads are trying to use barrier synchronization,
	   and the current collaborator is WorkPacketsRealtime, we have a problem.
	 */
	enum CollaboratorID {
		IncrementalParallelTask = 1,
		WorkPacketsRealtime = 2
	};
	
private:
	
	MM_YieldCollaborator *_prev;
	
	omrthread_monitor_t *_mutex;
	volatile uintptr_t *_count;
	volatile uintptr_t _yieldIndex;
	volatile uintptr_t _yieldCount;
	ResumeEvent _resumeEvent;

public:
	
	void setResumeEvent(ResumeEvent resumeEvent) { _resumeEvent = resumeEvent; }
	ResumeEvent getResumeEvent() { return _resumeEvent; }	
	uintptr_t getYieldCount() const { return _yieldCount; }
	
	MM_YieldCollaborator *push(MM_YieldCollaborator *prev) {
		_prev = prev;
		_yieldCount = 0;
		return this;
	}
	MM_YieldCollaborator *pop() {
		_yieldCount = 0;
		return _prev;
	}
	
	/* master notifies slaves it's time to start a new GC quantum */
	void resumeSlavesFromYield(MM_EnvironmentBase *env);
	/* used by either slaves to yield or master to wait for slaves to yield (or reach sync barrier) */	
	void yield(MM_EnvironmentBase *env);
	
	MM_YieldCollaborator(omrthread_monitor_t *mutex, volatile UDATA *count, CollaboratorID collaboratorID) :
		_prev(NULL),
		_mutex(mutex),
		_count(count),
		_yieldIndex(0),
		_yieldCount(0)
	{
		_typeId = __FUNCTION__;
	}
	
};

#endif /* YIELDCOLLABORATOR_HPP_ */
