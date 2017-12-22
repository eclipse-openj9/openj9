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


#include "j9.h"
#include "j9cfg.h"

#include "GlobalAllocationManagerTarok.hpp"

#include "AllocationContextBalanced.hpp"
#include "AllocationContextTarok.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "VMThreadListIterator.hpp"
#include "Wildcard.hpp"

/* the common context is used for the main thread, specifically, so it is not equivalent to other contexts in the list */
#define COMMON_CONTEXT_INDEX 0


MM_GlobalAllocationManagerTarok*
MM_GlobalAllocationManagerTarok::newInstance(MM_EnvironmentBase *env)
{
	MM_GlobalAllocationManagerTarok *allocationManager = (MM_GlobalAllocationManagerTarok *)env->getForge()->allocate(sizeof(MM_GlobalAllocationManagerTarok), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != allocationManager) {
		allocationManager = new(allocationManager) MM_GlobalAllocationManagerTarok(env);
		if (!allocationManager->initialize(env)) {
			allocationManager->kill(env);
			allocationManager = NULL;	
		}
	}
	return allocationManager;
}

void
MM_GlobalAllocationManagerTarok::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_GlobalAllocationManagerTarok::shouldIdentifyThreadAsCommon(MM_EnvironmentBase *env)
{
	bool result = false;
	
	if (_extensions->tarokAttachedThreadsAreCommon) {
		result = (J9_PRIVATE_FLAGS_ATTACHED_THREAD == ((J9_PRIVATE_FLAGS_ATTACHED_THREAD | J9_PRIVATE_FLAGS_SYSTEM_THREAD) & ((J9VMThread *)env->getLanguageVMThread())->privateFlags));
	}
	
	if (!result) {
		/* determine if the thread's class matches any of the wildcards specified using -XXgc:numaCommonThreadClass= */
		J9Object* threadObject = ((J9VMThread *)env->getLanguageVMThread())->threadObject;
		if (NULL != threadObject) {
			J9Class *threadClass = J9GC_J9OBJECT_CLAZZ(threadObject);
			J9UTF8* classNameUTF8 = J9ROMCLASS_CLASSNAME(threadClass->romClass);
			MM_Wildcard *wildcard = MM_GCExtensions::getExtensions(_extensions)->numaCommonThreadClassNamePatterns;
			while (!result && (NULL != wildcard)) {
				result = wildcard->match((char*)J9UTF8_DATA(classNameUTF8), J9UTF8_LENGTH(classNameUTF8));
				wildcard = wildcard->_next;
			}
		}
	}
	
	return result;
}

bool
MM_GlobalAllocationManagerTarok::acquireAllocationContext(MM_EnvironmentBase *env)
{
	Assert_MM_true(NULL == env->getAllocationContext());
	MM_AllocationContextTarok *context = NULL;
	if ((1 == _managedAllocationContextCount) || shouldIdentifyThreadAsCommon(env)) {
		context = (MM_AllocationContextTarok*)_managedAllocationContexts[COMMON_CONTEXT_INDEX]; 
		/* attached threads get the common context */
		env->setAllocationContext(context);
	} else {
		UDATA index = _nextAllocationContext;
		/* context 0 is the common context so clamp the range of worker contexts above 0 */
		_nextAllocationContext = (index + 1) % (_managedAllocationContextCount - 1);
		UDATA thisIndex = index + 1;
		Assert_MM_true(COMMON_CONTEXT_INDEX != thisIndex);
		context = (MM_AllocationContextTarok*)_managedAllocationContexts[thisIndex]; 
		env->setAllocationContext(context);
		context->setNumaAffinityForThread(env);
	}

	env->setCommonAllocationContext(_managedAllocationContexts[COMMON_CONTEXT_INDEX]);
	
	/* this check is kind of gratuitous but it ensures that the env accepted the context we gave it and that this GAM is correctly initialized */
	return (context == env->getAllocationContext());
}

void
MM_GlobalAllocationManagerTarok::releaseAllocationContext(MM_EnvironmentBase *env)
{
	/* just disassociate the env from this context */
	env->setAllocationContext(NULL);
}

bool
MM_GlobalAllocationManagerTarok::initializeAllocationContexts(MM_EnvironmentBase *env, MM_MemorySubSpaceTarok *subspace)
{
	UDATA allocationSize = sizeof(MM_AllocationContextBalanced *) * _managedAllocationContextCount;
	MM_AllocationContextBalanced **contexts = (MM_AllocationContextBalanced **)env->getForge()->allocate(allocationSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == contexts) {
		return false;
	}
	memset(contexts, 0, allocationSize);
	_managedAllocationContexts = (MM_AllocationContext **)contexts;

	UDATA affinityLeaderCount = 0;
	J9MemoryNodeDetail const* affinityLeaders = _extensions->_numaManager.getAffinityLeaders(&affinityLeaderCount);
	Assert_MM_true((1 + affinityLeaderCount) == _managedAllocationContextCount);
	UDATA forceNode = _extensions->fvtest_tarokForceNUMANode;

	/* create the array of contexts indexed by node */
	UDATA maximumNodeNumberOwningMemory = 0;
	if (UDATA_MAX == forceNode) {
		for (UDATA i = 0; i < affinityLeaderCount; i++) {
			maximumNodeNumberOwningMemory = OMR_MAX(maximumNodeNumberOwningMemory, affinityLeaders[i].j9NodeNumber);
		}
	} else {
		maximumNodeNumberOwningMemory = forceNode;
	}
	UDATA owningByNodeSize = sizeof(MM_AllocationContextBalanced *) * (maximumNodeNumberOwningMemory + 1);
	_perNodeContextSets = (MM_AllocationContextBalanced **)env->getForge()->allocate(owningByNodeSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _perNodeContextSets) {
		return false;
	}
	memset(_perNodeContextSets, 0x0, owningByNodeSize);

	/* create the common context */
	MM_AllocationContextBalanced *commonContext = MM_AllocationContextBalanced::newInstance(env, subspace, 0, COMMON_CONTEXT_INDEX);
	if (NULL == commonContext) {
		return false;
	}
	contexts[COMMON_CONTEXT_INDEX] = commonContext;
	commonContext->setNextSibling(commonContext);
	_perNodeContextSets[0] = commonContext;
	Assert_MM_true(0 == COMMON_CONTEXT_INDEX);
	UDATA nextContextIndex = 1;

	/* create affinity leader contexts */
	for (UDATA i = 0; i < affinityLeaderCount; i++) {
		UDATA numaNode = 0;
		if (UDATA_MAX == forceNode) {
			numaNode = affinityLeaders[i].j9NodeNumber;
		} else {
			numaNode = forceNode;
		}
		MM_AllocationContextBalanced *context = MM_AllocationContextBalanced::newInstance(env, subspace, numaNode, nextContextIndex);
		if (NULL == context) {
			return false;
		}
		/* sibling relationship is for lock splitting but not currently in use so just short-circuit it */
		context->setNextSibling(context);
		_perNodeContextSets[numaNode] = context;
		/* every context is the cousin of the one before it */
		context->setStealingCousin(contexts[nextContextIndex - 1]);
		contexts[nextContextIndex] = context;
		nextContextIndex += 1;
	}

	commonContext->setStealingCousin(contexts[nextContextIndex - 1]);
	_nextAllocationContext = (1 == _managedAllocationContextCount) ? 0 : (_extensions->fvtest_tarokFirstContext % (_managedAllocationContextCount - 1));


	return true;
}

bool
MM_GlobalAllocationManagerTarok::initialize(MM_EnvironmentBase *env)
{
	bool result = MM_GlobalAllocationManager::initialize(env);
	if (result) {
		_managedAllocationContextCount = calculateIdealManagedContextCount(_extensions);
	}
	if (result) {
		result = _runtimeExecManager.initialize(env);
	}
	
	if (result) {
		Assert_MM_true((UDATA_MAX / (getTotalAllocationContextCount() + 1)) > _extensions->tarokRegionMaxAge);
	}


	return result;
}

/**
 * Tear down a GAM instance
 */
void
MM_GlobalAllocationManagerTarok::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _managedAllocationContexts) {
		for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
			if (NULL != _managedAllocationContexts[i]) {
				_managedAllocationContexts[i]->kill(env);
				_managedAllocationContexts[i] = NULL;
			}
		}
	
		env->getForge()->free(_managedAllocationContexts);
		_managedAllocationContexts = NULL;
	}


	if (NULL != _perNodeContextSets) {
		env->getForge()->free(_perNodeContextSets);
		_perNodeContextSets = NULL;
	}

	_runtimeExecManager.tearDown(env);

	
	MM_GlobalAllocationManager::tearDown(env);
}

/**
 * Print current counters for AC region count and resets the counters afterwards
 */	
void
MM_GlobalAllocationManagerTarok::printAllocationContextStats(MM_EnvironmentBase *env, UDATA eventNum, J9HookInterface** hookInterface)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA totalRegionCount = 0;
	const char *eventName = "     ";
	J9HookInterface** externalHookInterface = MM_GCExtensions::getExtensions(env)->getOmrHookInterface();
	
	if ((eventNum == J9HOOK_MM_OMR_GLOBAL_GC_START) && (hookInterface == externalHookInterface)) {
		eventName = "GCSTART";
	} else if ((eventNum == J9HOOK_MM_OMR_GLOBAL_GC_END) && (hookInterface == externalHookInterface)) {
		eventName = "GCEND  ";
	} else {
		Assert_MM_unreachable();
	}
	
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		MM_AllocationContextTarok *ac = (MM_AllocationContextTarok *)_managedAllocationContexts[i];		
		ac->resetRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED);
		ac->resetRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED_IDLE);
		ac->resetRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED);
		ac->resetThreadCount();
	}	

	GC_VMThreadListIterator threadIterator((J9JavaVM *)env->getLanguageVM());
	J9VMThread * walkThread = NULL;
	while (NULL != (walkThread = threadIterator.nextVMThread())) {
		MM_EnvironmentBase *envThread = MM_EnvironmentBase::getEnvironment(walkThread->omrVMThread);
		if ((envThread->getThreadType() == MUTATOR_THREAD)) {
			((MM_AllocationContextTarok *)envThread->getAllocationContext())->incThreadCount();
		}
	}
	
	GC_HeapRegionIteratorVLHGC regionIterator(MM_GCExtensions::getExtensions(env)->heapRegionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (NULL != region->getMemoryPool()) {
			region->_allocateData._owningContext->incRegionCount(region->getRegionType());
		}
	}	
	
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		MM_AllocationContextTarok *ac = (MM_AllocationContextTarok *)_managedAllocationContexts[i];
		UDATA acRegionCount = ac->getRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED);
		acRegionCount += ac->getRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED_IDLE);
		acRegionCount += ac->getRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED);
		totalRegionCount += acRegionCount;
		UDATA localCount = 0;
		UDATA foreignCount = 0;
		ac->getRegionCount(&localCount, &foreignCount);

		j9tty_printf(PORTLIB, "AC %3d %s MPAOL regionCount %5d (AO %5d AO_IDLE %5d AO_MARKED %5d) mutatorCount %3d numaNode %d (%d local, %d foreign)\n",
				i, eventName, acRegionCount, ac->getRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED),
				ac->getRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED_IDLE), ac->getRegionCount(MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED),
				ac->getThreadCount(), ac->getNumaNode(), localCount, foreignCount);
	}

	j9tty_printf(PORTLIB, "AC sum %s MPAOL regionCount %5d (total %d) \n", eventName, totalRegionCount, MM_GCExtensions::getExtensions(env)->heapRegionManager->getTableRegionCount());
}

UDATA
MM_GlobalAllocationManagerTarok::getActualFreeMemorySize()
{
	UDATA freeMemory = 0;
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		freeMemory += ((MM_AllocationContextTarok *)_managedAllocationContexts[i])->getFreeMemorySize();
	}
	return freeMemory;
}

UDATA
MM_GlobalAllocationManagerTarok::getFreeRegionCount()
{
	UDATA freeRegions = 0;
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		freeRegions += ((MM_AllocationContextTarok *)_managedAllocationContexts[i])->getFreeRegionCount();
	}
	return freeRegions;
}

UDATA
MM_GlobalAllocationManagerTarok::getApproximateFreeMemorySize()
{
	return getActualFreeMemorySize();
}

void
MM_GlobalAllocationManagerTarok::resetLargestFreeEntry()
{
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		((MM_AllocationContextTarok *)_managedAllocationContexts[i])->resetLargestFreeEntry();
	}
}
void
MM_GlobalAllocationManagerTarok::expand(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region)
{
	/* we can only work on committed, free regions */
	Assert_MM_true(region->isCommitted());
	Assert_MM_true(MM_HeapRegionDescriptor::FREE == region->getRegionType());

	/* find the node to which this region has been bound */
	UDATA nodeNumber = region->getNumaNode();
	MM_AllocationContextBalanced *targetContext = _perNodeContextSets[nodeNumber];
	targetContext->addRegionToFreeList(env, region);
	/* now "rotate the wheel" so that the next expansion into this node will be distributed to the next context */
	_perNodeContextSets[nodeNumber] = targetContext->getNextSibling();
}
UDATA
MM_GlobalAllocationManagerTarok::getLargestFreeEntry()
{
	UDATA largest = 0;
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		UDATA candidate = ((MM_AllocationContextTarok *)_managedAllocationContexts[i])->getLargestFreeEntry();
		largest = OMR_MAX(largest, candidate);
	}
	return largest;
}
void
MM_GlobalAllocationManagerTarok::resetHeapStatistics(bool globalCollect)
{
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		((MM_AllocationContextTarok *)_managedAllocationContexts[i])->resetHeapStatistics(globalCollect);
	}
}
void
MM_GlobalAllocationManagerTarok::mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType)
{
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		((MM_AllocationContextTarok *)_managedAllocationContexts[i])->mergeHeapStats(heapStats, includeMemoryType);
	}
}

UDATA
MM_GlobalAllocationManagerTarok::calculateIdealTotalContextCount(MM_GCExtensions *extensions)
{
	UDATA totalCount = calculateIdealManagedContextCount(extensions);
	return totalCount;
	
}

UDATA
MM_GlobalAllocationManagerTarok::calculateIdealManagedContextCount(MM_GCExtensionsBase *extensions)
{
	UDATA affinityLeaderCount = extensions->_numaManager.getAffinityLeaderCount();
	UDATA desiredAllocationContextCount = 1 + affinityLeaderCount;
	UDATA regionCount = extensions->memoryMax / extensions->regionSize;
	/* heuristic -- ACs are permitted to waste up to 1/8th of the heap in slack regions. This number may need to be adjusted */ 
	UDATA maxAllocationContextCount = regionCount / 8;
	return OMR_MAX(1, OMR_MIN(desiredAllocationContextCount, maxAllocationContextCount));
}


MM_AllocationContextBalanced *
MM_GlobalAllocationManagerTarok::getAllocationContextForNumaNode(UDATA numaNode)
{
	MM_AllocationContextBalanced * result = NULL;
	for (UDATA i = 0; i < _managedAllocationContextCount; i++) {
		MM_AllocationContextBalanced *allocationContext = (MM_AllocationContextBalanced*)_managedAllocationContexts[i];
		if (allocationContext->getNumaNode() == numaNode) {
			result = allocationContext;
			break;
		}
	}
	Assert_MM_true(NULL != result);
	return result;
}

