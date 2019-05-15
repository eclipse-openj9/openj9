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

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCCode.hpp"
#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"
#include "MetronomeDelegate.hpp"
#include "Scheduler.hpp"
#include "RealtimeGC.hpp"

#include "MemorySubSpaceMetronome.hpp"

/**
 * Allocation.
 * @todo Provide class documentation
 */
void *
MM_MemorySubSpaceMetronome::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	void *result = NULL;
	
	if (shouldCollectOnFailure) {
		result = allocateMixedObjectOrArraylet((MM_EnvironmentRealtime *)env, allocDescription, mixedObject);
	} else {
		/* not setting these object flags causes a failure working with empty arrays but where this is set should probably be hoisted into the caller */
		allocDescription->setObjectFlags(getObjectFlags());
		result = _memoryPoolSegregated->allocateObject(env, allocDescription);
	}
	return result;
}

#if defined(OMR_GC_ARRAYLETS)
/**
 * Allocate an arraylet leaf.
 */
void *
MM_MemorySubSpaceMetronome::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	omrarrayptr_t spine = allocDescription->getSpine();
	/* spine object refers to the barrier-safe "object" reference, as opposed to the internal "heap address" represented by the spine variable */
	omrobjectptr_t spineObject = (omrobjectptr_t)spine;
	void *leaf = NULL;
	if(env->saveObjects(spineObject)) {
		leaf = allocateMixedObjectOrArraylet(env, allocDescription, arrayletLeaf);
		env->restoreObjects(&spineObject);
		spine = (omrarrayptr_t)spineObject;
		allocDescription->setSpine(spine);
	}
	return leaf;
}
#endif /* OMR_GC_ARRAYLETS */

void
MM_MemorySubSpaceMetronome::collectOnOOM(MM_EnvironmentBase *env, MM_GCCode gcCode, MM_AllocateDescription *allocDescription)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);
	MM_GCExtensionsBase *ext = envRealtime->getExtensions();
	MM_Scheduler *sched = (MM_Scheduler *)ext->dispatcher;
	
	if (sched->isInitialized()) {
		sched->startGC(envRealtime);
		sched->setGCCode(gcCode);
		sched->continueGC(envRealtime, OUT_OF_MEMORY_TRIGGER, allocDescription->getBytesRequested(), env->getOmrVMThread(), true);
	}
	/* TODO CRGTMP remove call to yieldWhenRequested since continueGC blocks */
	ext->realtimeGC->getRealtimeDelegate()->yieldWhenRequested(envRealtime);
}

/**
 * TODO: this is temporary as a way to avoid duplication of (badly written) code in MemorySubSpaceMetronome::allocate. 
 * We will specifically fix this allocate method in a separate design.
 */
void *
MM_MemorySubSpaceMetronome::allocateMixedObjectOrArraylet(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, AllocateType allocType)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);
	void *result;
	
	allocDescription->setObjectFlags(getObjectFlags());
	
	result = allocate(envRealtime, allocDescription, allocType);
	if (NULL != result) {
		return result;
	}
	
	/* We failed to allocate the object so do a synchronous GC.  This may complete the current GC */
	collectOnOOM(envRealtime, MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_DEFAULT), allocDescription);
	
	/* We completed a GC so try the allocate again */
	result = allocate(envRealtime, allocDescription, allocType);
	if (NULL != result) {
		return result;
	}
	
	/* Still failed to allocate so try another synchronous GC */
	collectOnOOM(envRealtime, MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_DEFAULT), allocDescription);
	
	/* We completed a GC so try the allocate again */
	result = allocate(envRealtime, allocDescription, allocType);
	if (NULL != result) {
		return result;
	}
	
	/* Still failed to allocate so try an aggressive synchronous GC */
	collectOnOOM(envRealtime, MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE), allocDescription);
	
	result = allocate(envRealtime, allocDescription, allocType);
	if (NULL != result) {
		return result;
	}
	
	return NULL;
}

/**
 * On a request for systemGC metronome will initiate a GC, 
 * but doesn't wait for it to complete.
 */
void 
MM_MemorySubSpaceMetronome::systemGarbageCollect(MM_EnvironmentBase *env, U_32 gcCode) {
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);
	MM_Scheduler *sched = (MM_Scheduler *)envRealtime->getExtensions()->dispatcher;

	if (sched->isInitialized()) {
		MM_GCExtensionsBase *ext = env->getExtensions();
		ext->realtimeGC->setFixHeapForWalk(true);
		sched->startGC(envRealtime);
		sched->setGCCode(MM_GCCode(gcCode));
		/* if we were triggered by rasdump, then the caller has already acquired exclusive VM access */
		sched->continueGC(envRealtime, SYSTEM_GC_TRIGGER, 0, envRealtime->getOmrVMThread(), J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT != gcCode);
		/* TODO CRGTMP remove this call since continueGC blocks */
		ext->realtimeGC->getRealtimeDelegate()->yieldWhenRequested(envRealtime);
	}
}

/**
 * @todo Provide class documentation
 * @ingroup GC_Metronome methodGroup
 */
void
MM_MemorySubSpaceMetronome::collect(MM_EnvironmentBase *env, MM_GCCode gcCode)
{
	_collector->garbageCollect(env, this, NULL, gcCode.getCode(), NULL, NULL, NULL);
}

/**
 * Initialization.
 * @todo Provide class documentation
 */
MM_MemorySubSpaceMetronome *
MM_MemorySubSpaceMetronome::newInstance(
		MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemoryPool *memoryPool,
		bool usesGlobalCollector, UDATA minimumSize, UDATA initialSize, UDATA maximumSize)
{
	MM_MemorySubSpaceMetronome *memorySubSpace;
	
	memorySubSpace = (MM_MemorySubSpaceMetronome *)env->getForge()->allocate(sizeof(MM_MemorySubSpaceMetronome), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != memorySubSpace) {
		new(memorySubSpace) MM_MemorySubSpaceMetronome(env, physicalSubArena, memoryPool, usesGlobalCollector, minimumSize, initialSize, maximumSize);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	}
	return memorySubSpace;
}

/**
 * Initialization.
 * @todo Provide class documentation
 */
bool
MM_MemorySubSpaceMetronome::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemorySubSpaceSegregated::initialize(env)) {
		return false;
	}

	/* TODO: Not valid for multiple memory spaces */
	MM_RealtimeGC *realtimeGC = (MM_RealtimeGC *)_collector;
	realtimeGC->setMemoryPool(_memoryPoolSegregated);
	realtimeGC->setMemorySubSpace(this);

	return true;
}
