/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "GlobalGCStats.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "MemoryPoolAggregatedCellList.hpp"
#include "MetronomeStats.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RegionPoolSegregated.hpp"
#include "Scheduler.hpp"
#include "SizeClasses.hpp"
#include "SweepSchemeRealtime.hpp"

#include "AllocationContextRealtime.hpp"

MM_AllocationContextRealtime *
MM_AllocationContextRealtime::newInstance(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool)
{
	MM_AllocationContextRealtime *allocCtxt = (MM_AllocationContextRealtime *)env->getForge()->allocate(sizeof(MM_AllocationContextRealtime), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (allocCtxt) {
		new(allocCtxt) MM_AllocationContextRealtime(env, gam, regionPool);
		if (!allocCtxt->initialize(env)) {
			allocCtxt->kill(env);
			allocCtxt = NULL;
		}
	}
	return allocCtxt;
}

bool
MM_AllocationContextRealtime::initialize(MM_EnvironmentBase *env)
{
	if (!MM_AllocationContextSegregated::initialize(env)) {
		return false;
	}

	return true;
}

void
MM_AllocationContextRealtime::tearDown(MM_EnvironmentBase *env)
{
	MM_AllocationContextSegregated::tearDown(env);
}

void
MM_AllocationContextRealtime::signalSmallRegionDepleted(MM_EnvironmentBase *env, UDATA sizeClass)
{
	/* If we do not have a region yet, try to trigger a GC */
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	MM_Scheduler *sched = (MM_Scheduler *)ext->dispatcher;
	sched->checkStartGC((MM_EnvironmentRealtime *) env);
}

bool
MM_AllocationContextRealtime::trySweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, UDATA sizeClass, UDATA *sweepCount, U_64 *sweepStartTime)
{
	bool result = false;

#if defined(J9VM_GC_REALTIME)
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	MM_RealtimeGC *realtimeGC = ext->realtimeGC;
	MM_SizeClasses *sizeClasses = MM_GCExtensions::getExtensions(env)->defaultSizeClasses;

	UDATA nonDeterministicSweepCount = *sweepCount;
	U_64 nonDeterministicSweepStartTime = *sweepStartTime;

	/* Do not attempt this optimization while sweeping arraylets (at least not while the region may contain a spine). */
	if (realtimeGC->shouldPerformNonDeterministicSweep()) {
		PORT_ACCESS_FROM_ENVIRONMENT(env);
		/* Heuristic (or better, extrapolation): if occupancy is very high, do not retry too many times to sweep.
		 * Still, try at least once no matter how high the occupancy is (even if sharp 1.0), to jump over a local walls (short series of full regions).
		 * The heuristic assumes that the sweep list is (roughly) ordered by occupancy (low occupancy at front).
		 */
		if (nonDeterministicSweepCount <= (sizeClasses->getNumCells(sizeClass) * (1 - _regionPool->getOccupancy(sizeClass)))) {

			if (nonDeterministicSweepCount == 0) {
				nonDeterministicSweepStartTime = j9time_hires_clock();
			}

			MM_HeapRegionDescriptorSegregated *region = _regionPool->sweepAndAllocateRegionFromSmallSizeClass(env, sizeClass);
			if (region != NULL) {
				/* Count each non determinist sweep */
				MM_GCExtensions::getExtensions(env)->globalGCStats.metronomeStats.nonDeterministicSweepCount += 1;
				nonDeterministicSweepCount++;
				/* Find the longest streak (both as count of sweeps and time spent) of consecutive sweeps for one alloc */
				if (nonDeterministicSweepCount > MM_GCExtensions::getExtensions(env)->globalGCStats.metronomeStats.nonDeterministicSweepConsecutive) {
					MM_GCExtensions::getExtensions(env)->globalGCStats.metronomeStats.nonDeterministicSweepConsecutive = nonDeterministicSweepCount;
				}
				U_64 deltaTime = j9time_hires_delta(nonDeterministicSweepStartTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
				if (deltaTime > MM_GCExtensions::getExtensions(env)->globalGCStats.metronomeStats.nonDeterministicSweepDelay) {
					MM_GCExtensions::getExtensions(env)->globalGCStats.metronomeStats.nonDeterministicSweepDelay = deltaTime;
				}
				MM_AtomicOperations::storeSync();
				_smallRegions[sizeClass] = region;
				/* Don't update bytesAllocated because unswept regions are still considered to be in use */
				result = true;
			}
		}
	}
#endif

	return result;
}

UDATA *
MM_AllocationContextRealtime::allocateLarge(MM_EnvironmentBase *env, UDATA sizeInBytesRequired)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	MM_Scheduler *sched = (MM_Scheduler *)ext->dispatcher;

	/* See if we should start a GC */
	sched->checkStartGC((MM_EnvironmentRealtime *) env);

	/* Call parent to try to get a large object region */
	UDATA *result =  MM_AllocationContextSegregated::allocateLarge(env, sizeInBytesRequired);
//TODO SATB extract into a method
	if ((NULL != result) && (GC_MARK == ((MM_EnvironmentRealtime *) env)->getAllocationColor())) {
		ext->realtimeGC->getMarkingScheme()->mark((omrobjectptr_t)result);
	}

	return result;
}

bool
MM_AllocationContextRealtime::shouldPreMarkSmallCells(MM_EnvironmentBase *env)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	bool result = false;

	if (NULL != ext->realtimeGC) {
		MM_EnvironmentRealtime *envRT = (MM_EnvironmentRealtime *) env;
		result = (GC_MARK == envRT->getAllocationColor());
	}

	return result;
}
