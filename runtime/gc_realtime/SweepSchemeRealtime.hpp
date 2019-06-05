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

#if !defined(SWEEPSCHEMEREALTIME_HPP_)
#define SWEEPSCHEMEREALTIME_HPP_

#include "omr.h"

#include "RealtimeGC.hpp"
#include "SweepSchemeSegregated.hpp"

class MM_EnvironmentBase;
class MM_MarkMap;
class MM_Scheduler;

class MM_SweepSchemeRealtime : public MM_SweepSchemeSegregated
{
	/*
	 * Data members
	 */
private:
	MM_RealtimeGC *_realtimeGC; /**< The RealtimeGC that this sweepScheme is associated with */
	MM_Scheduler *_scheduler;
	uintptr_t _coalesceFreeRegionCount;
	uintptr_t _sweepSmallRegionCount;
	uintptr_t _yieldSmallRegionCount;

protected:
public:

	/*
	 * Function members
	 */
private:
	virtual void yieldFromSweep(MM_EnvironmentBase *env, uintptr_t yieldSlackTime = 0);

	virtual uintptr_t resetCoalesceFreeRegionCount(MM_EnvironmentBase *env);
	virtual bool updateCoalesceFreeRegionCount(uintptr_t range);

	virtual uintptr_t resetSweepSmallRegionCount(MM_EnvironmentBase *env, uintptr_t yieldSmallRegionCount);
	virtual bool updateSweepSmallRegionCount();

protected:
	virtual void preSweep(MM_EnvironmentBase *env);
	virtual void postSweep(MM_EnvironmentBase *env);

	virtual void incrementalSweepArraylet(MM_EnvironmentBase *env);

	MM_SweepSchemeRealtime(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC, MM_Scheduler *scheduler, MM_MarkMap *markMap) :
		MM_SweepSchemeSegregated(env, markMap)
		,_realtimeGC(realtimeGC)
		,_scheduler(scheduler)
		,_coalesceFreeRegionCount(0)
		,_sweepSmallRegionCount(0)
		,_yieldSmallRegionCount(0)
	{
		_typeId = __FUNCTION__;
	};

public:
	static MM_SweepSchemeRealtime *newInstance(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC, MM_Scheduler *scheduler, MM_MarkMap *markMap);
	void kill(MM_EnvironmentBase *env);

	void
	sweep(MM_EnvironmentBase *env)
	{
		MM_SweepSchemeSegregated::sweep(env, _realtimeGC->_memoryPool, _realtimeGC->isFixHeapForWalk());
	}
};

#endif /* SWEEPSCHEMEREALTIME_HPP_ */
