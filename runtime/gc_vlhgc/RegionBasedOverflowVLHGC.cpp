
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

#include "j9cfg.h"
#include "j9.h"
#include "modronopt.h"

#include "RegionBasedOverflowVLHGC.hpp"

#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "Packet.hpp"
#include "Task.hpp"
#include "WorkPackets.hpp"

/**
 * Create a new MM_RegionBasedOverflowVLHGC object
 */
MM_RegionBasedOverflowVLHGC *
MM_RegionBasedOverflowVLHGC::newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets, U_8 overflowFlag)
{
	Assert_MM_true(0 != overflowFlag);
	MM_RegionBasedOverflowVLHGC *overflow = (MM_RegionBasedOverflowVLHGC *)env->getForge()->allocate(sizeof(MM_RegionBasedOverflowVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != overflow) {
		new(overflow) MM_RegionBasedOverflowVLHGC(env,workPackets, overflowFlag);
		if (!overflow->initialize(env)) {
			overflow->kill(env);
			overflow = NULL;
		}
	}
	return overflow;
}

/**
 * Initialize a MM_RegionBasedOverflowVLHGC object.
 * 
 * @return true on success, false otherwise
 */
bool
MM_RegionBasedOverflowVLHGC::initialize(MM_EnvironmentBase *env)
{
	return MM_WorkPacketOverflow::initialize(env);
}

MM_RegionBasedOverflowVLHGC::MM_RegionBasedOverflowVLHGC(MM_EnvironmentBase *env, MM_WorkPackets *workPackets, U_8 overflowFlag)
	: MM_WorkPacketOverflow(env, workPackets)
	,_extensions(MM_GCExtensions::getExtensions(env))
	,_heapRegionManager(_extensions->heapRegionManager)
	,_overflowFlag(overflowFlag)
{
	_typeId = __FUNCTION__;
}

/**
 * Cleanup the resources for a MM_RegionBasedOverflowVLHGC object
 */
void
MM_RegionBasedOverflowVLHGC::tearDown(MM_EnvironmentBase *env)
{
	MM_WorkPacketOverflow::tearDown(env);
}

/**
 * Empty a packet on overflow
 * 
 * Empty a packet to resolve overflow by dirtying the appropriate 
 * cards for each object within a given packet
 * 
 * @param packet - Reference to packet to be emptied
 * @param type - ignored for concurrent collector
 *  
 */
void
MM_RegionBasedOverflowVLHGC::emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type)
{
	MM_EnvironmentVLHGC *envVLHGC = MM_EnvironmentVLHGC::getEnvironment(env);
	void *objectPtr = NULL;

	_overflow = true;
	
	envVLHGC->_workPacketStats.setSTWWorkStackOverflowOccured(true);
	envVLHGC->_workPacketStats.incrementSTWWorkStackOverflowCount();
	envVLHGC->_workPacketStats.setSTWWorkpacketCountAtOverflow(_workPackets->getActivePacketCount());
	
	/* Empty the current packet by overflowing its regions now */
	while(NULL != (objectPtr = packet->pop(envVLHGC))) {
		overflowItemInternal(envVLHGC, objectPtr, type);
	}
	Assert_MM_true(packet->isEmpty());
}

/**
 * Overflow an item
 * 
 * Overflow an item by dirtying the appropriate card
 * 
 * @param item - item to overflow
 * @param type - ignored for concurrent collector
 *  
 */
void
MM_RegionBasedOverflowVLHGC::overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	MM_EnvironmentVLHGC *envVLHGC = MM_EnvironmentVLHGC::getEnvironment(env);
	_overflow = true;
	
	envVLHGC->_workPacketStats.setSTWWorkStackOverflowOccured(true);
	envVLHGC->_workPacketStats.incrementSTWWorkStackOverflowCount();
	envVLHGC->_workPacketStats.setSTWWorkpacketCountAtOverflow(_workPackets->getActivePacketCount());
	
	overflowItemInternal(envVLHGC, item, type);
}

/**
 * Overflow an item
 * 
 * Overflow an item by dirtying the appropriate card
 * 
 * @param item - item to overflow
 * @param type - ignored for concurrent collector
 *  
 */
void
MM_RegionBasedOverflowVLHGC::overflowItemInternal(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	MM_EnvironmentVLHGC *envVLHGC = MM_EnvironmentVLHGC::getEnvironment(env);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(envVLHGC);
	J9Object *objectPtr = (J9Object *)item;

	/* If element is an array split tag then ignore it */
	if ((PACKET_INVALID_OBJECT != (UDATA)item) && (PACKET_ARRAY_SPLIT_TAG !=  (((UDATA)objectPtr) & PACKET_ARRAY_SPLIT_TAG))) {
		/* Check reference is within the heap */
		void *heapBase = extensions->heap->getHeapBase();
		void *heapTop = extensions->heap->getHeapTop();
		Assert_MM_true((item >= heapBase) && (item <  heapTop));

	   	/* ..and dirty its card if it is */	
		Assert_MM_true(NULL != envVLHGC->_cycleState);
		Assert_MM_true(NULL != envVLHGC->_cycleState->_markMap);
		/* Removed for JAZZ 46465 since CopyForwardDepthFirst needs to use this overflow mechanism but its mark map caches out
		 * out of sync with the global mark map so it isn't safe to check that an overflow item is marked:
		 * Assert_MM_true(envVLHGC->_cycleState->_markMap->isBitSet(objectPtr));
		 */
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)_heapRegionManager->tableDescriptorForAddress(item);
		/* ensure that we have flushed the updated mark map bit from our cache and that we see any other updates to the overflow flags for this region */
		MM_AtomicOperations::sync();

		/*
		 * CMVC 176903:
		 * Regions with containsObjects flag OFF can not be marked overflowed
		 */
		Assert_MM_true(region->containsObjects());

		U_8 existingFlags = region->_markData._overflowFlags;
		if (0 == (existingFlags & _overflowFlag)) {
			/* this flag isn't yet set so set it and re-save the value to the region */
			U_8 newFlags = existingFlags | _overflowFlag;
			region->_markData._overflowFlags = newFlags;
		}
		
		GC_ObjectModel::ScanType scantype = extensions->objectModel.getScanType(objectPtr);

		if (GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT == scantype) {
			/* since we popped this object from the work packet, it is our responsibility to record it in the list of reference objects */
			/* we know that the object must be in the collection set because it was on a work packet */
			/* we don't need to process cleared or enqueued references */ 
			I_32 referenceState = J9GC_J9VMJAVALANGREFERENCE_STATE(envVLHGC, objectPtr);
			if ((GC_ObjectModel::REF_STATE_INITIAL == referenceState) || (GC_ObjectModel::REF_STATE_REMEMBERED == referenceState)) {
				env->getGCEnvironment()->_referenceObjectBuffer->add(envVLHGC, objectPtr);

				bool referentMustBeCleared = false;
				UDATA referenceObjectOptions = envVLHGC->_cycleState->_referenceObjectOptions;
				UDATA referenceObjectType = J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(objectPtr)) & J9AccClassReferenceMask;
				switch (referenceObjectType) {
				case J9AccClassReferenceWeak:
					referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_weak)) ;
					break;
				case J9AccClassReferenceSoft:
					referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_soft));
					break;
				case J9AccClassReferencePhantom:
					referentMustBeCleared = (0 != (referenceObjectOptions & MM_CycleState::references_clear_phantom));
					break;
				default:
					Assert_MM_unreachable();
				}

				if (referentMustBeCleared) {
					GC_SlotObject referentPtr(envVLHGC->getOmrVM(), &J9GC_J9VMJAVALANGREFERENCE_REFERENT(envVLHGC, objectPtr));
					referentPtr.writeReferenceToSlot(NULL);
					J9GC_J9VMJAVALANGREFERENCE_STATE(envVLHGC, objectPtr) = GC_ObjectModel::REF_STATE_CLEARED;
				}
			}
		} else if ((OMR_GC_CYCLE_TYPE_VLHGC_PARTIAL_GARBAGE_COLLECT == envVLHGC->_cycleState->_type) && (GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT == scantype)) {
			/* JAZZ 63834 handle new ownableSynchronizer processing in overflowed case
		 	 * new processing currently only for CopyForwardScheme collector
		 	 */
			if (isEvacuateRegion(region) && (NULL != _extensions->accessBarrier->isObjectInOwnableSynchronizerList(objectPtr))) {
				/* To avoid adding duplication item (abort case the object need to be rescan and interregions remembered objects) 
				 * To avoid adding constructing object 
				 */
				envVLHGC->getGCEnvironment()->_ownableSynchronizerObjectBuffer->add(envVLHGC, objectPtr);
				if (envVLHGC->_cycleState->_shouldRunCopyForward) {
					envVLHGC->_copyForwardStats._ownableSynchronizerSurvived += 1;
				} else {
					envVLHGC->_markVLHGCStats._ownableSynchronizerSurvived += 1;
				}
			}
		}
	}
}

/**
 * Fill a packet from overflow list
 * 
 * @param packet - Reference to packet to be filled.
 * 
 * @note No-op in this overflow handler. We will never fill a packet from
 * the card table.
 */
void
MM_RegionBasedOverflowVLHGC::fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet)
{

}

void
MM_RegionBasedOverflowVLHGC::reset(MM_EnvironmentBase *env)
{

}

bool
MM_RegionBasedOverflowVLHGC::isEmpty()
{
	return true;
}

U_8
MM_RegionBasedOverflowVLHGC::overflowFlagForCollectionType(MM_EnvironmentBase *env, MM_CycleState::CollectionType collectionType)
{
	U_8 flag = 0;
	switch (collectionType) {
	case MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION:
		flag = OVERFLOW_PARTIAL_COLLECT;
		break;
	case MM_CycleState::CT_GLOBAL_GARBAGE_COLLECTION:
	case MM_CycleState::CT_GLOBAL_MARK_PHASE:
		flag = OVERFLOW_GLOBAL_COLLECT;
		break;
	default:
		Assert_MM_unreachable();
	}
	return flag;
}
