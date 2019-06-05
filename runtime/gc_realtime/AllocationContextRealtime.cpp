
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

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalGCStats.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "MetronomeStats.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RegionPoolSegregated.hpp"
#include "Scheduler.hpp"
#include "SizeClasses.hpp"

#include "AllocationContextRealtime.hpp"

MM_AllocationContextRealtime *
MM_AllocationContextRealtime::newInstance(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool)
{
	MM_AllocationContextRealtime *allocCtxt = (MM_AllocationContextRealtime *)env->getForge()->allocate(sizeof(MM_AllocationContextRealtime), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
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
MM_AllocationContextRealtime::signalSmallRegionDepleted(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	/* If we do not have a region yet, try to trigger a GC */
	MM_GCExtensionsBase *ext = env->getExtensions();
	MM_Scheduler *sched = (MM_Scheduler *)ext->dispatcher;
	sched->checkStartGC((MM_EnvironmentRealtime *) env);
}

bool
MM_AllocationContextRealtime::trySweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass, uintptr_t *sweepCount, U_64 *sweepStartTime)
{
	bool result = false;

#if defined(OMR_GC_REALTIME)
	MM_GCExtensionsBase *ext = env->getExtensions();
	MM_RealtimeGC *realtimeGC = ext->realtimeGC;
	MM_SizeClasses *sizeClasses = ext->defaultSizeClasses;

	uintptr_t nonDeterministicSweepCount = *sweepCount;
	U_64 nonDeterministicSweepStartTime = *sweepStartTime;

	/* Do not attempt this optimization while sweeping arraylets (at least not while the region may contain a spine). */
	if (realtimeGC->shouldPerformNonDeterministicSweep()) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		/* Heuristic (or better, extrapolation): if occupancy is very high, do not retry too many times to sweep.
		 * Still, try at least once no matter how high the occupancy is (even if sharp 1.0), to jump over a local walls (short series of full regions).
		 * The heuristic assumes that the sweep list is (roughly) ordered by occupancy (low occupancy at front).
		 */
		if (nonDeterministicSweepCount <= (sizeClasses->getNumCells(sizeClass) * (1 - _regionPool->getOccupancy(sizeClass)))) {

			if (nonDeterministicSweepCount == 0) {
				nonDeterministicSweepStartTime = omrtime_hires_clock();
			}

			MM_HeapRegionDescriptorSegregated *region = _regionPool->sweepAndAllocateRegionFromSmallSizeClass(env, sizeClass);
			if (region != NULL) {
				/* Count each non determinist sweep */
				ext->globalGCStats.metronomeStats.nonDeterministicSweepCount += 1;
				nonDeterministicSweepCount++;
				/* Find the longest streak (both as count of sweeps and time spent) of consecutive sweeps for one alloc */
				if (nonDeterministicSweepCount > ext->globalGCStats.metronomeStats.nonDeterministicSweepConsecutive) {
					ext->globalGCStats.metronomeStats.nonDeterministicSweepConsecutive = nonDeterministicSweepCount;
				}
				U_64 deltaTime = omrtime_hires_delta(nonDeterministicSweepStartTime, omrtime_hires_clock(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
				if (deltaTime > ext->globalGCStats.metronomeStats.nonDeterministicSweepDelay) {
					ext->globalGCStats.metronomeStats.nonDeterministicSweepDelay = deltaTime;
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

uintptr_t *
MM_AllocationContextRealtime::allocateLarge(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired)
{
	MM_GCExtensionsBase *ext = env->getExtensions();
	MM_Scheduler *sched = (MM_Scheduler *)ext->dispatcher;

	/* See if we should start a GC */
	sched->checkStartGC((MM_EnvironmentRealtime *) env);

	/* Call parent to try to get a large object region */
	uintptr_t *result =  MM_AllocationContextSegregated::allocateLarge(env, sizeInBytesRequired);
//TODO SATB extract into a method
	if ((NULL != result) && (GC_MARK == ((MM_EnvironmentRealtime *) env)->getAllocationColor())) {
		ext->realtimeGC->getMarkingScheme()->mark((omrobjectptr_t)result);
	}

	return result;
}

bool
MM_AllocationContextRealtime::shouldPreMarkSmallCells(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *ext = env->getExtensions();
	bool result = false;

	if (NULL != ext->realtimeGC) {
		MM_EnvironmentRealtime *envRT = (MM_EnvironmentRealtime *) env;
		result = (GC_MARK == envRT->getAllocationColor());
	}

	return result;
}
