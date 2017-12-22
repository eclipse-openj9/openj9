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

#include "BarrierSynchronization.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "MarkMap.hpp"
#include "MemoryPoolSegregated.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RegionPoolSegregated.hpp"

#include "Scheduler.hpp"

#include "SweepSchemeRealtime.hpp"

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_SweepSchemeRealtime *
MM_SweepSchemeRealtime::newInstance(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC, MM_Scheduler *scheduler, MM_MarkMap *markMap)
{
	MM_SweepSchemeRealtime *instance;

	instance = (MM_SweepSchemeRealtime *)env->getForge()->allocate(sizeof(MM_SweepSchemeRealtime), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != instance) {
		new(instance) MM_SweepSchemeRealtime(env, realtimeGC, scheduler, markMap);
		if (!instance->initialize(env)) {
			instance->kill(env);
			instance = NULL;
		}
	}

	return instance;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_SweepSchemeRealtime::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_SweepSchemeRealtime::preSweep(MM_EnvironmentBase *env)
{
	_realtimeGC->setCollectorSweeping();
	_scheduler->condYieldFromGC(env, _scheduler->beatNanos);

	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	MM_SweepSchemeSegregated::preSweep(env);

	_realtimeGC->allThreadsAllocateUnmarked(env);
	if (ext->concurrentSweepingEnabled) {
		_realtimeGC->setCollectorConcurrentSweeping();
		_scheduler->_barrierSynchronization->releaseExclusiveVMAccess(env, _scheduler->_exclusiveVMAccessRequired);
	}
}

void
MM_SweepSchemeRealtime::postSweep(MM_EnvironmentBase *env)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	if (ext->concurrentSweepingEnabled) {
		_scheduler->_barrierSynchronization->acquireExclusiveVMAccess(env, _scheduler->_exclusiveVMAccessRequired);
		_realtimeGC->setCollectorSweeping(); /* It might have been in ConcurrentSweep mode before. */
	}

	MM_SweepSchemeSegregated::postSweep(env);

	MM_MemoryPoolSegregated *memoryPool = _realtimeGC->_memoryPool;
	ext->gcTrigger = OMR_MAX(ext->gcInitialTrigger, ext->headRoom + memoryPool->getBytesInUse());
}

void
MM_SweepSchemeRealtime::incrementalSweepArraylet(MM_EnvironmentBase *env)
{
	_realtimeGC->setCollectorSweepingArraylets(true);

	MM_SweepSchemeSegregated::incrementalSweepArraylet(env);

	_realtimeGC->setCollectorSweepingArraylets(false);
}


void
MM_SweepSchemeRealtime::yieldFromSweep(MM_EnvironmentBase *env, UDATA yieldSlackTime)
{
	_scheduler->condYieldFromGC(env, yieldSlackTime);
}

UDATA
MM_SweepSchemeRealtime::resetCoalesceFreeRegionCount(MM_EnvironmentBase *env)
{
	_coalesceFreeRegionCount = 0;
	return 100000;
}

bool
MM_SweepSchemeRealtime::updateCoalesceFreeRegionCount(UDATA range)
{
	bool mustYield = false;
	_coalesceFreeRegionCount += range;
	if (_coalesceFreeRegionCount > MAX_REGION_COALESCE) {
		_coalesceFreeRegionCount = 0;
		mustYield = true;
	}
	return mustYield;
}

UDATA
MM_SweepSchemeRealtime::resetSweepSmallRegionCount(MM_EnvironmentBase *env, UDATA sweepSmallRegionsPerIteration)
{
	_sweepSmallRegionCount = 0;
	_yieldSmallRegionCount = sweepSmallRegionsPerIteration >> 3;
	return 50000;
}

bool
MM_SweepSchemeRealtime::updateSweepSmallRegionCount()
{
	bool mustYield = false;
	_sweepSmallRegionCount += 1;
	if (_sweepSmallRegionCount >= _yieldSmallRegionCount) {
		_sweepSmallRegionCount = 0;
		mustYield = true;
	}
	return mustYield;
}

