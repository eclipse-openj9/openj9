
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"

#include "MarkMapManager.hpp"

#include "EnvironmentVLHGC.hpp"
#include "GCObjectEvents.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"


MM_MarkMapManager::MM_MarkMapManager(MM_EnvironmentVLHGC *env)
	: MM_BaseNonVirtual()
	, _extensions(MM_GCExtensions::getExtensions(env))
	, _nextMarkMap(NULL)
	, _previousMarkMap(NULL)
	, _deleteEventShadowMarkMap(NULL)
{
	_typeId = __FUNCTION__;
}

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_MarkMapManager *
MM_MarkMapManager::newInstance(MM_EnvironmentVLHGC *env)
{
	MM_MarkMapManager *markMapManager;

	markMapManager = (MM_MarkMapManager *)env->getForge()->allocate(sizeof(MM_MarkMapManager), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != markMapManager) {
		new(markMapManager) MM_MarkMapManager(env);
		if (!markMapManager->initialize(env)) {
			markMapManager->kill(env);
			markMapManager = NULL;
		}
	}

	return markMapManager;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_MarkMapManager::kill(MM_EnvironmentVLHGC *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the receivers internal structures and resources.
 * @return true if initialization is successful, false otherwise.
 */
bool
MM_MarkMapManager::initialize(MM_EnvironmentVLHGC *env)
{
	UDATA maximumPhysicalRange = _extensions->heap->getMaximumPhysicalRange();
	if(NULL == (_nextMarkMap = MM_MarkMap::newInstance(env, maximumPhysicalRange))) {
		goto error_no_memory;
	}
	if(NULL == (_previousMarkMap = MM_MarkMap::newInstance(env, maximumPhysicalRange))) {
		goto error_no_memory;
	}

	_extensions->previousMarkMap = _previousMarkMap;
	
	return true;

error_no_memory:
	return false;
}

/**
 * Free the receivers internal structures and resources.
 */
void
MM_MarkMapManager::tearDown(MM_EnvironmentVLHGC *env)
{
	if(NULL != _nextMarkMap) {
		_nextMarkMap->kill(env);
		_nextMarkMap = NULL;
	}
	if(NULL != _previousMarkMap) {
		_previousMarkMap->kill(env);
		_previousMarkMap = NULL;
	}
	if (NULL != _deleteEventShadowMarkMap) {
		_deleteEventShadowMarkMap->kill(env);
		_deleteEventShadowMarkMap = NULL;
	}
	_extensions->previousMarkMap = NULL;
	
}


bool
MM_MarkMapManager::collectorStartup(MM_GCExtensions *extensions)
{
	bool result = true;
	J9HookInterface** omrHookInterface = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	/* disabling the hook will fail if someone has already hooked or reserved it */
	bool hookInUse = (0 != (*omrHookInterface)->J9HookDisable(omrHookInterface, J9HOOK_MM_OMR_OBJECT_DELETE));
	if (hookInUse) {
		J9JavaVM *javaVM = (J9JavaVM *)extensions->getOmrVM()->_language_vm;
        J9VMThread *currentThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(currentThread);
		_deleteEventShadowMarkMap = MM_MarkMap::newInstance(env, extensions->getHeap()->getMaximumPhysicalRange());
		if (NULL != _deleteEventShadowMarkMap) {
			void *lowAddress = extensions->getHeap()->getHeapBase();
			void *highAddress = extensions->getHeap()->getHeapTop();
			UDATA size = (UDATA)highAddress - (UDATA)lowAddress;
			result = _deleteEventShadowMarkMap->heapAddRange(env, size, lowAddress, highAddress);
		}
		result = result && (NULL != _deleteEventShadowMarkMap);
	}
	return result;
}

/**
 * Adjust internal structures to reflect the change in heap size.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 */
bool
MM_MarkMapManager::heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress)
{
	bool result = _nextMarkMap->heapAddRange(env, size, lowAddress, highAddress);
	if (result) {
		result = _previousMarkMap->heapAddRange(env, size, lowAddress, highAddress);
		if (result) {
			if (NULL != _deleteEventShadowMarkMap) {
				result = _deleteEventShadowMarkMap->heapAddRange(env, size, lowAddress, highAddress);
				if (!result) {
					_previousMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, NULL, NULL);
					_nextMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, NULL, NULL);
				}
			}
		} else {
			_nextMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, NULL, NULL);
		}
	}
	return result;
}

/**
 * Adjust internal structures to reflect the change in heap size.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 *
 */
bool
MM_MarkMapManager::heapRemoveRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	bool result = _nextMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	result = result && _previousMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	if (NULL != _deleteEventShadowMarkMap) {
		result = result && _deleteEventShadowMarkMap->heapRemoveRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return result;
}

MM_MarkMap *
MM_MarkMapManager::savePreviousMarkMapForDeleteEvents(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(NULL != _deleteEventShadowMarkMap);
	UDATA *targetBits = _deleteEventShadowMarkMap->getHeapMapBits();
	UDATA *sourceBits = _previousMarkMap->getHeapMapBits();
	
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;
	while((region = regionIterator.nextRegion()) != NULL) {
		if (region->hasValidMarkMap()) {
			void *base = region->getLowAddress();
			void *top = region->getHighAddress();
			UDATA baseIndex = _previousMarkMap->getSlotIndex((J9Object*)base);
			UDATA topIndex = _previousMarkMap->getSlotIndex((J9Object*)top);
			UDATA bytesToCopy = (topIndex - baseIndex) * sizeof(UDATA);
			memcpy(&targetBits[baseIndex], &sourceBits[baseIndex], bytesToCopy);
		}
	}
	
	return _deleteEventShadowMarkMap;
}

void
MM_MarkMapManager::reportDeletedObjects(MM_EnvironmentVLHGC *env, MM_MarkMap *oldMap, MM_MarkMap *newMap)
{
	Assert_MM_true(NULL != _deleteEventShadowMarkMap);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;
	while((region = regionIterator.nextRegion()) != NULL) {
		MM_MemorySubSpace *memorySubSpace = region->getSubSpace();
		void *base = region->getLowAddress();
		void *top = region->getHighAddress();
		if (region->hasValidMarkMap()) {
			/* we can't use the GC_ObjectHeapBufferedIterator since we need to control which mark map we are walking */
			MM_HeapMapIterator iterator(extensions, oldMap, (UDATA *)base, (UDATA *)top, false);
			J9Object *object = NULL;
			while (NULL != (object = iterator.nextObject())) {
				if (!newMap->isBitSet(object)) {
					/* this object died during the collect */
					TRIGGER_J9HOOK_MM_OMR_OBJECT_DELETE(env->getExtensions()->omrHookInterface, env->getOmrVMThread(), object, memorySubSpace);
				}
			}
		} else if (region->containsObjects()) {
			GC_ObjectHeapIteratorAddressOrderedList iterator(extensions, (J9Object *)region->getLowAddress(), (J9Object *)((MM_MemoryPoolBumpPointer *)region->getMemoryPool())->getAllocationPointer(), false, false);
			J9Object *object = NULL;
			while (NULL != (object = iterator.nextObject())) {
				if (!newMap->isBitSet(object)) {
					/* this object wasn't marked during the collect */
					TRIGGER_J9HOOK_MM_OMR_OBJECT_DELETE(env->getExtensions()->omrHookInterface, env->getOmrVMThread(), object, memorySubSpace);
				}
			}
		}
	}
}

void
MM_MarkMapManager::verifyNextMarkMapSubsetOfPrevious(MM_EnvironmentVLHGC *env)
{
	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager);
	MM_HeapRegionDescriptorVLHGC *region = regionIterator.nextRegion();
	while (NULL != region) {
		/* TODO: check for exactly MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED once this is correctly set */
		if (region->containsObjects()) {
			MM_HeapMapIterator nextMapWalker(_extensions, _nextMarkMap, (UDATA *)region->getLowAddress(), (UDATA *)region->getHighAddress());
			J9Object *object = nextMapWalker.nextObject();
			while (NULL != object) {
				bool doesMatch = _previousMarkMap->isBitSet(object);
				Assert_MM_true(doesMatch);
				object = nextMapWalker.nextObject();
			}
		}
		region = regionIterator.nextRegion();
	}
}

void 
MM_MarkMapManager::swapMarkMaps() 
{
	MM_MarkMap *tempMarkMap = _nextMarkMap;
	_nextMarkMap = _previousMarkMap;
	_previousMarkMap = tempMarkMap;
	_extensions->previousMarkMap = _previousMarkMap;

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	UDATA regionCount = regionManager->getTableRegionCount();
	for (UDATA i = 0; i < regionCount; i++) {
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)regionManager->mapRegionTableIndexToDescriptor(i);
		bool temp = region->_previousMarkMapCleared;
		region->_previousMarkMapCleared = region->_nextMarkMapCleared;
		region->_nextMarkMapCleared = temp;
	}
}
