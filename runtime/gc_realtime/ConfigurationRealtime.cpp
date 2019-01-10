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

/**
 * @file
 * @ingroup GC_Modron_Metronome
 */

#include "j9.h"
#include "j9cfg.h"
#include "MemorySpacesAPI.h"

#include "ConfigurationRealtime.hpp"

#include "EnvironmentRealtime.hpp"
#include "GlobalAllocationManagerRealtime.hpp"
#include "GlobalCollector.hpp"
#include "GCExtensions.hpp"
#include "HeapVirtualMemory.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "HeapRegionManagerTarok.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpaceFlat.hpp"
#include "MemorySubSpaceGeneric.hpp"
#include "MemorySubSpaceMetronome.hpp"
#include "RegionPoolSegregated.hpp"
#include "PhysicalArenaRegionBased.hpp"
#include "PhysicalSubArenaRegionBased.hpp"
#include "RealtimeGC.hpp"
#include "Scheduler.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SegregatedAllocationTracker.hpp"
#include "SizeClasses.hpp"

#define REALTIME_REGION_SIZE_BYTES 64 * 1024

/* Define realtime arraylet leaf size as the largest small size class we can have */
#define REALTIME_ARRAYLET_LEAF_SIZE_BYTES J9VMGC_SIZECLASSES_MAX_SMALL_SIZE_BYTES

#define J9GC_HASH_SALT_COUNT_METRONOME 1

class MM_MemoryPoolSegregated;

bool
MM_ConfigurationRealtime::initialize(MM_EnvironmentBase *env)
{
	J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	bool success = false;
	if (MM_Configuration::initialize(env)) {
		env->getOmrVM()->_sizeClasses = _delegate.getSegregatedSizeClasses(env);
		if (NULL != env->getOmrVM()->_sizeClasses) {
			extensions->setSegregatedHeap(true);
			extensions->setMetronomeGC(true);

			javaVM->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_USER_REALTIME_ACCESS_BARRIER;
			extensions->arrayletsPerRegion = extensions->regionSize / javaVM->arrayletLeafSize;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			if (!extensions->dynamicClassUnloadingThresholdForced) {
				extensions->dynamicClassUnloadingThreshold = 1;
			}
			if (!extensions->dynamicClassUnloadingKickoffThresholdForced) {
				extensions->dynamicClassUnloadingKickoffThreshold = 0;
			}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
			/* Excessive GC logic does not work with incremental Metronome. */
			if (!extensions->excessiveGCEnabled._wasSpecified) {
				extensions->excessiveGCEnabled._valueSpecified = false;
			}
			success = true;
		}
	}
	return success;
}

void
MM_ConfigurationRealtime::tearDown(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (NULL != extensions->defaultSizeClasses) {
		extensions->defaultSizeClasses->kill(env);
		extensions->defaultSizeClasses = NULL;
	}

	MM_Configuration::tearDown(env);
}

MM_Heap *
MM_ConfigurationRealtime::createHeapWithManager(MM_EnvironmentBase *env, UDATA heapBytesRequested, MM_HeapRegionManager *regionManager)
{
	return MM_HeapVirtualMemory::newInstance(env, env->getExtensions()->heapAlignment, heapBytesRequested, regionManager);
}

MM_MemorySpace *
MM_ConfigurationRealtime::createDefaultMemorySpace(MM_EnvironmentBase *envBase, MM_Heap *heap, MM_InitializationParameters *parameters)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_MemoryPoolSegregated *memoryPool = NULL;
	MM_MemorySubSpaceMetronome *memorySubSpaceMetronome = NULL;
	MM_PhysicalSubArenaRegionBased *physicalSubArena = NULL;
	MM_PhysicalArenaRegionBased *physicalArena = NULL;
	MM_RegionPoolSegregated *regionPool = NULL;
	
	if(NULL == (extensions->defaultSizeClasses = MM_SizeClasses::newInstance(env))) {
		return NULL;
	}
	
	regionPool = MM_RegionPoolSegregated::newInstance(env, extensions->heapRegionManager);
	if (NULL == regionPool) {
		return NULL;
	}	
	
	extensions->globalAllocationManager = MM_GlobalAllocationManagerRealtime::newInstance(env, regionPool);
	if(NULL == extensions->globalAllocationManager) {
		return NULL;
	}
	
	if(NULL == (memoryPool = MM_MemoryPoolSegregated::newInstance(env, regionPool, MINIMUM_FREE_CHUNK_SIZE, (MM_GlobalAllocationManagerSegregated*)extensions->globalAllocationManager))) {
		return NULL;
	}
	
	if(NULL == (physicalSubArena = MM_PhysicalSubArenaRegionBased::newInstance(env, heap))) {
		memoryPool->kill(env);
		return NULL;
	}
	
	memorySubSpaceMetronome = MM_MemorySubSpaceMetronome::newInstance(env, physicalSubArena, memoryPool, true, parameters->_minimumSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumSpaceSize);
	if(NULL == memorySubSpaceMetronome) {
		return NULL;
	}
	
	if(NULL == (physicalArena = MM_PhysicalArenaRegionBased::newInstance(env, heap))) {
		memorySubSpaceMetronome->kill(env);
		return NULL;
	}
	
	return MM_MemorySpace::newInstance(env, heap, physicalArena, memorySubSpaceMetronome, parameters, MEMORY_SPACE_NAME_METRONOME, MEMORY_SPACE_DESCRIPTION_METRONOME);
}


MM_EnvironmentBase *
MM_ConfigurationRealtime::allocateNewEnvironment(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread)
{
	return MM_EnvironmentRealtime::newInstance(extensions, omrVMThread);
}

J9Pool *
MM_ConfigurationRealtime::createEnvironmentPool(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	
	uintptr_t numberOfElements = getConfigurationDelegate()->getInitialNumberOfPooledEnvironments(env);
	/* number of elements, pool flags = 0, 0 selects default pool configuration (at least 1 element, puddle size rounded to OS page size) */
	return pool_new(sizeof(MM_EnvironmentRealtime), numberOfElements, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(PORTLIB));
}

bool 
MM_ConfigurationRealtime::initializeEnvironment(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	if (MM_Configuration::initializeEnvironment(env)) {
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
		if (extensions->globalAllocationManager->acquireAllocationContext(env)) {
			MM_MemoryPoolSegregated *memoryPool = (MM_MemoryPoolSegregated *)extensions->heap->getDefaultMemorySpace()->getDefaultMemorySubSpace()->getMemoryPool();
			env->_allocationTracker = memoryPool->createAllocationTracker(env);
			return (NULL != env->_allocationTracker);
		}
	}
	
		return false;
	}
	
void 
MM_ConfigurationRealtime::defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace)
{
	MM_Configuration::defaultMemorySpaceAllocated(extensions, defaultMemorySpace);

	J9JavaVM* vm = (J9JavaVM *)extensions->getOmrVM()->_language_vm;
	
	vm->heapBase = extensions->heap->getHeapBase();
	vm->heapTop = extensions->heap->getHeapTop();
}

MM_HeapRegionManager *
MM_ConfigurationRealtime::createHeapRegionManager(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	UDATA descriptorSize = sizeof(MM_HeapRegionDescriptorRealtime) + sizeof(UDATA *) * extensions->arrayletsPerRegion;
	
	MM_HeapRegionManagerTarok *heapRegionManager = MM_HeapRegionManagerTarok::newInstance(env, extensions->regionSize, descriptorSize, MM_HeapRegionDescriptorRealtime::initializer, MM_HeapRegionDescriptorRealtime::destructor);
	return heapRegionManager;
}

MM_Dispatcher *
MM_ConfigurationRealtime::createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, UDATA defaultOSStackSize)
{
	return MM_Scheduler::newInstance(env, handler, handler_arg, defaultOSStackSize);
}
