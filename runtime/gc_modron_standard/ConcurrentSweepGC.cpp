
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"

#if defined(J9VM_GC_CONCURRENT_SWEEP)

#include "ConcurrentSweepGC.hpp"

#include "ConcurrentSweepScheme.hpp"

MM_ConcurrentSweepGC *
MM_ConcurrentSweepGC::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentSweepGC *globalGC;
	
	globalGC = (MM_ConcurrentSweepGC *)env->getForge()->allocate(sizeof(MM_ConcurrentSweepGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (globalGC) {
		new(globalGC) MM_ConcurrentSweepGC(env);
		if (!globalGC->initialize(env)) { 
			globalGC->kill(env);
			globalGC = NULL;
		}
	}
	return globalGC;
}

void
MM_ConcurrentSweepGC::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, U_32 gcCode)
{
	/* Finish off any sweep work that was still in progress */
	MM_ConcurrentSweepScheme *concurrentSweep = (MM_ConcurrentSweepScheme *)_sweepScheme;
	if(concurrentSweep->isConcurrentSweepActive()) {
		concurrentSweep->completeSweep(env, ABOUT_TO_GC);
	}

	MM_ParallelGlobalGC::internalPreCollect(env, subSpace, allocDescription, gcCode);
}

/**
 * Pay the allocation tax for the mutator.
 */
void
MM_ConcurrentSweepGC::payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription)
{
	((MM_ConcurrentSweepScheme *)_sweepScheme)->payAllocationTax(env, baseSubSpace, allocDescription);
}

/**
 * Replenish a pools free lists to satisfy a given allocate.
 * The given pool was unable to satisfy an allocation request of (at least) the given size.  See if there is work
 * that can be done to increase the free stores of the pool so that the request can be met.
 * @note This call is made under the pools allocation lock (or equivalent)
 * @note Base implementation does no work.
 * @return True if the pool was replenished with a free entry that can satisfy the size, false otherwise.
 */
bool
MM_ConcurrentSweepGC::replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, UDATA size)
{
	return _sweepScheme->replenishPoolForAllocate(env, memoryPool, size);
}

#endif /* J9VM_GC_CONCURRENT_SWEEP */
