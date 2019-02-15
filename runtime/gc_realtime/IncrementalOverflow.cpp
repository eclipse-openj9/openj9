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

#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapRegionManager.hpp"
#include "HeapRegionDescriptorRealtime.hpp"
#include "IncrementalOverflow.hpp"
#include "Packet.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "WorkPackets.hpp"


/****************************************
 * Initialization
 ****************************************
 */

MM_IncrementalOverflow *
MM_IncrementalOverflow::newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
{
	MM_IncrementalOverflow *overflow;
	
	overflow = (MM_IncrementalOverflow *)env->getForge()->allocate(sizeof(MM_IncrementalOverflow), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (overflow) {
		new(overflow) MM_IncrementalOverflow(env, workPackets);
		if (!overflow->initialize(env)) {
			overflow->kill(env);
			overflow = NULL;
		}
	}
	return overflow;
}

/**
 * Initialize a MM_IncrementalOverflow object.
 * 
 * @return true on success, false otherwise
 */
bool
MM_IncrementalOverflow::initialize(MM_EnvironmentBase *env)
{
	if (!MM_WorkPacketOverflow::initialize(env)) {
		return false;
	}
	
	_extensions = env->getExtensions();

	return true;
}

/**
 * Cleanup the resources for a MM_IncrementalOverflow object
 */
void
MM_IncrementalOverflow::tearDown(MM_EnvironmentBase *env)
{	
	MM_WorkPacketOverflow::tearDown(env);
}

/**
 * Push the region to be overflowed onto the local cache.  If it is already
 * on the cache just return.
 */
MMINLINE void
MM_IncrementalOverflow::pushLocal(MM_EnvironmentBase *env, MM_HeapRegionDescriptorRealtime *region, MM_OverflowType type)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);
	MM_HeapRegionDescriptorRealtime **cache = envRealtime->getOverflowCache();
	
	/* If the local cache is full flush it */
	if (envRealtime->getOverflowCacheUsedCount() >= envRealtime->getExtensions()->overflowCacheCount) {
		flushLocal(envRealtime, type);
	}

	cache[envRealtime->getOverflowCacheUsedCount()] = region;
	envRealtime->incrementOverflowCacheUsedCount();
}

/**
 * Push the region onto the global overflow list.
 * @note this function does not use any locks when manipulating the list.  The caller
 * is responsible for handling the lock.
 */
MMINLINE void
MM_IncrementalOverflow::pushNoLock(MM_EnvironmentBase *env, MM_HeapRegionDescriptorRealtime *region)
{
	if (NULL == region->getNextOverflowRegion()) {
		region->setNextOverflowRegion((MM_HeapRegionDescriptorRealtime *)((uintptr_t)_overflowList | 1));
		_overflowList = region;
	}
}

/**
 * This functions pushes all of the regions from the local cache
 * to the global cache.  If these regions are being overflowed due to the workstack
 * check for yielding before the flush begins.
 */
MMINLINE void
MM_IncrementalOverflow::flushLocal(MM_EnvironmentBase *env, MM_OverflowType type)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);
	uintptr_t currentCount = envRealtime->getOverflowCacheUsedCount();
	MM_HeapRegionDescriptorRealtime **cache = envRealtime->getOverflowCache();
	uintptr_t count = 0;
	
	omrthread_monitor_enter(_overflowListMonitor);
	for (count = 0; count < currentCount; count++) {
		MM_HeapRegionDescriptorRealtime *localRegion = cache[count];
		pushNoLock(env, localRegion);
	}
	omrthread_monitor_exit(_overflowListMonitor);
	envRealtime->resetOverflowCacheUsedCount();
}

/**
 * Push a region onto the global overflow list.
 */
MMINLINE void
MM_IncrementalOverflow::push(MM_EnvironmentBase *env, MM_HeapRegionDescriptorRealtime *region)
{
	omrthread_monitor_enter(_overflowListMonitor);
	
	pushNoLock(env, region);
	
	omrthread_monitor_exit(_overflowListMonitor);
}

/**
 * Pop a region off of the global overflow list.
 */
MMINLINE MM_HeapRegionDescriptorRealtime *
MM_IncrementalOverflow::pop(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorRealtime *region = NULL;
	
	/* The _overflowListMonitor must be held while modifying the overflow list */
	omrthread_monitor_enter(_overflowListMonitor);
	
	if (NULL != _overflowList) {
		region = _overflowList;
		_overflowList = (MM_HeapRegionDescriptorRealtime*)((uintptr_t)region->getNextOverflowRegion() & ~1);
		region->setNextOverflowRegion(NULL);
	}
	
	omrthread_monitor_exit(_overflowListMonitor);
	
	return region;
}

/**
 * Empty the packet by overflowing all if it's items
 * 
 * @param env The current Environment
 * @param packet The packet to empty
 * @param type - ignored by incremental handler
 */
void
MM_IncrementalOverflow::emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type)
{
	void *item;
	
	_extensions->globalGCStats.metronomeStats.incrementWorkPacketOverflowCount();

	while(NULL != (item = packet->pop(env))) {
		overflowItemInternal(env, item, type);
	}
	/* Finished overflowing this region so flush the remaining entries in the local cache */
	flushLocal(env, type);
	
	Assert_MM_true(packet->isEmpty());
	
	_overflowThisGCCycle = 1;
}

/**
 * Fill the packet from overflowed items
 * 
 * @param env The current Environment
 * @param packet The packet to fill
 */
void
MM_IncrementalOverflow::fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet)
{
	MM_EnvironmentRealtime *envRealtime = MM_EnvironmentRealtime::getEnvironment(env);
	MM_HeapRegionDescriptorRealtime* region;
	MM_RealtimeGC *realtimeGC = envRealtime->getExtensions()->realtimeGC;
#if defined(OMR_GC_ARRAYLETS)
	MM_RealtimeMarkingScheme *markingScheme = realtimeGC->getMarkingScheme();
#endif /* defined(OMR_GC_ARRAYLETS) */
	bool roomLeft = true;
	
	while (roomLeft && ((region = pop(envRealtime)) != NULL)) {
#if defined(OMR_GC_ARRAYLETS)
		if (region->isArraylet()) {
			uintptr_t arrayletIndex = 0;
			uintptr_t arrayletLeafLogSize = envRealtime->getOmrVM()->_arrayletLeafLogSize;
			uintptr_t arrayletsPerRegion = envRealtime->getExtensions()->arrayletsPerRegion;
			
			while (arrayletIndex < arrayletsPerRegion) {
				if (region->isArrayletUsed(arrayletIndex)) {
					/* Only return arraylets if they contain pointers (scan stack can't deal with base type
					 * leaves) and whose spine is marked to prevent the arraylet leaf elements from being kept
					 * alive (more floating garbage)
					 */
					omrobjectptr_t arrayletParent = (omrobjectptr_t)region->getArrayletParent(arrayletIndex);
					if (_extensions->objectModel.isObjectArray(arrayletParent) && markingScheme->isMarked(arrayletParent)) {
						uintptr_t* arraylet = region->getArraylet(arrayletIndex, arrayletLeafLogSize);
						if (packet->isFull(envRealtime)) {
							push(envRealtime, region);
							roomLeft = false;
							break;
						}
						packet->push(envRealtime, (void *)ARRAYLET_TO_ITEM(arraylet));
					}
				}
				arrayletIndex += 1;
				realtimeGC->_sched->condYieldFromGC(env);
			}
		} else 
#endif /* defined(OMR_GC_ARRAYLETS) */		
		if (region->isCanonical()) {
			if (region->isSmall()) {
				/* A small region is divided into cells.
				 * Iterate over the cells returning the objects contained within inuse cells
				 */
				uintptr_t numCells = region->getNumCells();
				uintptr_t cellSize = region->getCellSize();
				uintptr_t *baseAddress = (uintptr_t *)region->getLowAddress();
				uintptr_t cellIndex = 0;
				while(cellIndex < numCells) {
					omrobjectptr_t object = (omrobjectptr_t)((uintptr_t)baseAddress + (cellIndex * cellSize));
					if (!_extensions->objectModel.isDeadObject(object)) {
						if(_extensions->objectModel.isOverflowBitSet(object)) {
							if (packet->isFull(envRealtime)) {
								push(envRealtime, region);
								roomLeft = false;
								break;
							}
							if(_extensions->objectModel.atomicClearOverflowBit(object)) {
								packet->push(envRealtime, (void *)OBJECT_TO_ITEM(object));
							}
						}
						cellIndex += 1;
					} else {
						cellIndex += (MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(object)->getSize() / cellSize);
					}
					
					realtimeGC->_sched->condYieldFromGC(env);
				}
				/* ran off end of region */
			} else if (region->isLarge()) {
				/* A region that is both large and canonical contains exactly 1 object */
				uintptr_t *cell = (uintptr_t *)region->getLowAddress();
				omrobjectptr_t object = (omrobjectptr_t)cell;
				if (_extensions->objectModel.isOverflowBitSet(object)) {
					if (packet->isFull(envRealtime)) {
						push(envRealtime, region);
						roomLeft = false;
						break;
					}
				    if (_extensions->objectModel.atomicClearOverflowBit(object)) {
						packet->push(envRealtime, (void *)OBJECT_TO_ITEM(object));
					}
				}
				realtimeGC->_sched->condYieldFromGC(env);
			}
		}
	}
}

/**
 * Overflow the item.  To overflow incremental items the object
 * and the object's region have to both be marked as overflowed.
 * 
 * @param env The current Environment
 * @param item The item to overflow
 * @param type - ignored by incremental handler
 */
void 
MM_IncrementalOverflow::overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	_extensions->globalGCStats.metronomeStats.incrementObjectOverflowCount();

	overflowItemInternal(env, item, type);
	flushLocal(env, type);
	
	_overflowThisGCCycle = 1;
}

/**
 * Overflow the item.  To overflow incremental items the object
 * and the object's region have to both be marked as overflowed.
 * 
 * @param env The current Environment
 * @param item The item to overflow
 * @param type - ignored by incremental handler
 */
MMINLINE void 
MM_IncrementalOverflow::overflowItemInternal(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	MM_HeapRegionManager *manager = _extensions->heap->getHeapRegionManager();
	MM_HeapRegionDescriptorRealtime* region;
	
	if (!IS_ITEM_ARRAYLET((uintptr_t)item)) {
		/* Set overflow bit in the object's flags */
		omrobjectptr_t objectPtr = ITEM_TO_OBJECT((uintptr_t)item);
		if (!_extensions->objectModel.atomicSetOverflowBit(objectPtr)) {
			return;
		}
	}

	/* Arraylets are not individually marked, only the region they belong to */
	/* TODO: Consider marking individual arraylet. For example, tag low bit of its spine back pointer),
	 * or even introduce arraylet flags in its region. */

	/* For both arraylets and objects, mark the region as containing overflow objects */
	region = (MM_HeapRegionDescriptorRealtime *)manager->tableDescriptorForAddress(item);
	pushLocal(env, region, type);
}

void
MM_IncrementalOverflow::reset(MM_EnvironmentBase *env)
{
	/* the _overflowListMonitor must be held while modifying the overflow list */
	omrthread_monitor_enter(_overflowListMonitor);
	_overflowList = NULL;
	omrthread_monitor_exit(_overflowListMonitor);
}
