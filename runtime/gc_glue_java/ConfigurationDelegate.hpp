
/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#ifndef CONFIGURATIONDELEGATE_HPP_
#define CONFIGURATIONDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9nonbuilder.h"
#include "omrgcconsts.h"
#include "sizeclasses.h"

#include "ClassLoaderManager.hpp"
#include "ConcurrentGC.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "GlobalAllocationManager.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ObjectAllocationInterface.hpp"
#include "StringTable.hpp"


#include "OwnableSynchronizerObjectList.hpp"
#include "ReferenceObjectList.hpp"
#include "UnfinalizedObjectList.hpp"

typedef struct MM_HeapRegionDescriptorStandardExtension {
	uintptr_t _maxListIndex; /**< Max index for _*ObjectLists[index] */
	MM_UnfinalizedObjectList *_unfinalizedObjectLists; /**< An array of lists of unfinalized objects in this region */
	MM_OwnableSynchronizerObjectList *_ownableSynchronizerObjectLists; /**< An array of lists of ownable synchronizer objects in this region */
	MM_ReferenceObjectList *_referenceObjectLists; /**< An array of lists of reference objects (i.e. weak/soft/phantom) in this region */
} MM_HeapRegionDescriptorStandardExtension;

class MM_ConfigurationDelegate
{
/*
 * Member data and types
 */
private:
	static const uintptr_t _maximumDefaultNumberOfGCThreads = 64;
	const MM_GCPolicy _gcPolicy;

protected:
public:

/*
 * Member functions
 */
private:
protected:

public:
	bool
	initialize(MM_EnvironmentBase* env, MM_GCWriteBarrierType writeBarrierType, MM_GCAllocationType allocationType)
	{
		/* sync J9 VM arraylet size with OMR VM */
		OMR_VM* omrVM = env->getOmrVM();
		J9JavaVM* javaVM = (J9JavaVM*)omrVM->_language_vm;
		javaVM->arrayletLeafSize = omrVM->_arrayletLeafSize;
		javaVM->arrayletLeafLogSize = omrVM->_arrayletLeafLogSize;

		/* set write barrier for J9 VM -- catch -Xgc:alwayscallwritebarrier first */
		MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(javaVM);
		if (extensions->alwaysCallWriteBarrier) {
			writeBarrierType = gc_modron_wrtbar_always;
		}

		Assert_MM_true(gc_modron_wrtbar_illegal != writeBarrierType);
		javaVM->gcWriteBarrierType = writeBarrierType;

		if (extensions->alwaysCallReadBarrier) {
			javaVM->gcReadBarrierType = gc_modron_readbar_always;
		} else {
			if (extensions->isConcurrentScavengerEnabled()) {
				javaVM->gcReadBarrierType = gc_modron_readbar_range_check;
			} else {
				javaVM->gcReadBarrierType = gc_modron_readbar_none;
			}
		}

		/* set allocation type for J9 VM */
		javaVM->gcAllocationType = allocationType;

		if (!extensions->dynamicClassUnloadingSet) {
			extensions->dynamicClassUnloading = MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_ON_CLASS_LOADER_CHANGES;
		}

		/* Enable string constant collection by default if we support class unloading */
		extensions->collectStringConstants = true;

		/*
		 *  note that these are the default thresholds but Realtime Configurations override these values, in their initialize methods
		 * (hence it is key for them to call their super initialize, first)
		 */
#define DYNAMIC_CLASS_UNLOADING_THRESHOLD			6
#define DYNAMIC_CLASS_UNLOADING_KICKOFF_THRESHOLD	80000

		if (!extensions->dynamicClassUnloadingThresholdForced) {
			extensions->dynamicClassUnloadingThreshold = DYNAMIC_CLASS_UNLOADING_THRESHOLD;
		}
		if (!extensions->dynamicClassUnloadingKickoffThresholdForced) {
			extensions->dynamicClassUnloadingKickoffThreshold = DYNAMIC_CLASS_UNLOADING_KICKOFF_THRESHOLD;
		}
		return true;
	}

	void
	tearDown(MM_EnvironmentBase* env)
	{
		J9JavaVM* vm = (J9JavaVM*)env->getOmrVM()->_language_vm;
		MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);

		if (NULL != vm->identityHashData) {
			env->getForge()->free(vm->identityHashData);
			vm->identityHashData = NULL;
		}

		if (NULL != extensions->classLoaderManager) {
			extensions->classLoaderManager->kill(env);
			extensions->classLoaderManager = NULL;
		}

		if (NULL != extensions->stringTable) {
			extensions->stringTable->kill(env);
			extensions->stringTable = NULL;
		}
	}

	OMR_SizeClasses *getSegregatedSizeClasses(MM_EnvironmentBase* env)
	{
		J9JavaVM* javaVM = (J9JavaVM*)env->getOmrVM()->_language_vm;
		return (OMR_SizeClasses*)(javaVM->realtimeSizeClasses);
	}

	static MM_HeapRegionDescriptorStandardExtension *
	getHeapRegionDescriptorStandardExtension(MM_EnvironmentBase* env, MM_HeapRegionDescriptor *region)
	{
		MM_HeapRegionDescriptorStandardExtension *regionExtension = NULL;
		if (env->getExtensions()->isStandardGC()) {
			regionExtension = (MM_HeapRegionDescriptorStandardExtension *)region->_heapRegionDescriptorExtension;
		}
		return regionExtension;
	}

	bool
	initializeHeapRegionDescriptorExtension(MM_EnvironmentBase* env, MM_HeapRegionDescriptor *region)
	{
		MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);

		if (extensions->isStandardGC()) {
			uintptr_t listCount = extensions->gcThreadCount;
			uintptr_t allocSize = sizeof(MM_HeapRegionDescriptorStandardExtension) + (listCount * (sizeof(MM_UnfinalizedObjectList) + sizeof(MM_OwnableSynchronizerObjectList) + sizeof(MM_ReferenceObjectList)));
			MM_HeapRegionDescriptorStandardExtension *regionExtension = (MM_HeapRegionDescriptorStandardExtension *)env->getForge()->allocate(allocSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
			if (NULL == regionExtension) {
				return false;
			}

			regionExtension->_maxListIndex = listCount;
			regionExtension->_unfinalizedObjectLists = (MM_UnfinalizedObjectList *) ((uintptr_t)regionExtension + sizeof(MM_HeapRegionDescriptorStandardExtension));
			regionExtension->_ownableSynchronizerObjectLists = (MM_OwnableSynchronizerObjectList *) (regionExtension->_unfinalizedObjectLists + listCount);
			regionExtension->_referenceObjectLists = (MM_ReferenceObjectList *) (regionExtension->_ownableSynchronizerObjectLists + listCount);

			for (uintptr_t list = 0; list < listCount; list++) {
				new(&regionExtension->_unfinalizedObjectLists[list]) MM_UnfinalizedObjectList();
				regionExtension->_unfinalizedObjectLists[list].setNextList(extensions->unfinalizedObjectLists);
				regionExtension->_unfinalizedObjectLists[list].setPreviousList(NULL);
				if (NULL != extensions->unfinalizedObjectLists) {
					extensions->unfinalizedObjectLists->setPreviousList(&regionExtension->_unfinalizedObjectLists[list]);
				}
				extensions->unfinalizedObjectLists = &regionExtension->_unfinalizedObjectLists[list];

				new(&regionExtension->_ownableSynchronizerObjectLists[list]) MM_OwnableSynchronizerObjectList();
				regionExtension->_ownableSynchronizerObjectLists[list].setNextList(extensions->ownableSynchronizerObjectLists);
				regionExtension->_ownableSynchronizerObjectLists[list].setPreviousList(NULL);
				if (NULL != extensions->ownableSynchronizerObjectLists) {
					extensions->ownableSynchronizerObjectLists->setPreviousList(&regionExtension->_ownableSynchronizerObjectLists[list]);
				}
				extensions->ownableSynchronizerObjectLists = &regionExtension->_ownableSynchronizerObjectLists[list];

				new(&regionExtension->_referenceObjectLists[list]) MM_ReferenceObjectList();
			}

			region->_heapRegionDescriptorExtension = regionExtension;
		}

		return true;
	}

	void
	teardownHeapRegionDescriptorExtension(MM_EnvironmentBase* env, MM_HeapRegionDescriptor *region)
	{
		if (env->getExtensions()->isStandardGC()) {
			if (NULL != region->_heapRegionDescriptorExtension) {
				env->getForge()->free(region->_heapRegionDescriptorExtension);
				region->_heapRegionDescriptorExtension = NULL;
			}
		}
	}

	bool
	heapInitialized(MM_EnvironmentBase* env)
	{
		MM_Heap *heap = env->getExtensions()->getHeap();
		MM_HeapRegionManager *heapRegionManager = heap->getHeapRegionManager();

		uintptr_t hashSaltCount = 0;
		uintptr_t hashSaltPolicy = J9_IDENTITY_HASH_SALT_POLICY_NONE;

		switch (_gcPolicy) {
		case gc_policy_optthruput:
		case gc_policy_nogc:
		case gc_policy_optavgpause:
		case gc_policy_gencon:
			hashSaltCount = 1;
			hashSaltPolicy = J9_IDENTITY_HASH_SALT_POLICY_STANDARD;
			break;
		case gc_policy_metronome:
			break;
		case gc_policy_balanced:
			hashSaltCount = heapRegionManager->getTableRegionCount();
			hashSaltPolicy = J9_IDENTITY_HASH_SALT_POLICY_REGION;
			break;
		default:
			Assert_MM_unreachable();
			break;
		}

		J9JavaVM* javaVM = (J9JavaVM*)env->getOmrVM()->_language_vm;
		uintptr_t size = offsetof(J9IdentityHashData, hashSaltTable) + (sizeof(U_32) * hashSaltCount);
		javaVM->identityHashData = (J9IdentityHashData*)env->getForge()->allocate(size, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
		bool result = NULL != javaVM->identityHashData;
		if (result) {
			J9IdentityHashData* hashData = javaVM->identityHashData;
			hashData->hashData1 = UDATA_MAX;
			hashData->hashData2 = 0;
			hashData->hashData3 = 0;
			hashData->hashData4 = 0;
			hashData->hashSaltPolicy = hashSaltPolicy;
			switch (hashSaltPolicy) {
			case J9_IDENTITY_HASH_SALT_POLICY_NONE:
				break;
			case J9_IDENTITY_HASH_SALT_POLICY_STANDARD:
				javaVM->identityHashData->hashSaltTable[J9GC_HASH_SALT_NURSERY_INDEX] = (U_32)convertValueToHash(javaVM, 1421595292 ^ (U_32)(uintptr_t)javaVM);
				break;
			case J9_IDENTITY_HASH_SALT_POLICY_REGION:
				for (uintptr_t index = 0; index < hashSaltCount; index++) {
					javaVM->identityHashData->hashSaltTable[index] = (U_32)convertValueToHash(javaVM, 1421595292 ^ (U_32)(uintptr_t)heapRegionManager->physicalTableDescriptorForIndex(index));
				}
				hashData->hashData1 = (uintptr_t)heap->getHeapBase();
				hashData->hashData2 = (uintptr_t)heap->getHeapTop();
				hashData->hashData3 = heapRegionManager->getRegionShift();
				hashData->hashData4 = hashSaltCount;
				break;
			default:
				result = false;
				break;
			}
		}
		return result;
	}

	uint32_t
	getInitialNumberOfPooledEnvironments(MM_EnvironmentBase* env)
	{
		return 0;
	}

	bool
	environmentInitialized(MM_EnvironmentBase* env)
	{
		MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
		J9VMThread* vmThread = (J9VMThread *)env->getLanguageVMThread();
		OMR_VM *omrVM = env->getOmrVM();

		/* only assign this parent list if we are going to use it */
		if (extensions->isStandardGC()) {
			vmThread->gcRememberedSet.parentList = &extensions->rememberedSet;
		}

		extensions->accessBarrier->initializeForNewThread(env);

		if (extensions->isConcurrentMarkEnabled()) {
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
			vmThread->cardTableVirtualStart = (U_8*)j9gc_incrementalUpdate_getCardTableVirtualStart(omrVM);
			vmThread->cardTableShiftSize = j9gc_incrementalUpdate_getCardTableShiftValue(omrVM);
			MM_ConcurrentGC *concurrentGC = (MM_ConcurrentGC *)extensions->getGlobalCollector();
			if (!extensions->optimizeConcurrentWB || (CONCURRENT_OFF < concurrentGC->getConcurrentGCStats()->getExecutionMode())) {
				vmThread->privateFlags |= J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
			}
#else
			Assert_MM_unreachable();
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
		} else if (extensions->isVLHGC()) {
#if defined(OMR_GC_VLHGC)
			vmThread->cardTableVirtualStart = (U_8 *)j9gc_incrementalUpdate_getCardTableVirtualStart(omrVM);
			vmThread->cardTableShiftSize = j9gc_incrementalUpdate_getCardTableShiftValue(omrVM);
#else
			Assert_MM_unreachable();
#endif /* OMR_GC_VLHGC */
		} else {
			vmThread->cardTableVirtualStart = (U_8*)NULL;
			vmThread->cardTableShiftSize = 0;
		}

		if (extensions->fvtest_disableInlineAllocation) {
			env->_objectAllocationInterface->disableCachedAllocations(env);
		}

		return true;
	}

	bool canCollectFragmentationStats(MM_EnvironmentBase *env)
	{
		J9JavaVM* javaVM = (J9JavaVM*)env->getOmrVM()->_language_vm;
		/* processing estimate Fragmentation only is on non startup stage to avoid startup regression(fragmentation during startup is not meaningful for the estimation)
		   it is only for jit mode(for int mode javaVM->phase is always J9VM_PHASE_NOT_STARTUP) */
		return (J9VM_PHASE_NOT_STARTUP == javaVM->phase);
	}

	uintptr_t getMaxGCThreadCount(MM_EnvironmentBase* env)
	{
		return _maximumDefaultNumberOfGCThreads;
	}

	MM_GCPolicy getGCPolicy() { return _gcPolicy; }

	/**
	 * Constructor.
	 */
	MM_ConfigurationDelegate(MM_GCPolicy gcPolicy)
		: _gcPolicy(gcPolicy)
	{}
};

#endif /* CONFIGURATIONDELEGATE_HPP_ */
