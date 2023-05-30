/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#include "ContinuationObjectList.hpp"
#include "ReferenceObjectList.hpp"
#include "UnfinalizedObjectList.hpp"

typedef struct MM_HeapRegionDescriptorStandardExtension {
	uintptr_t _maxListIndex; /**< Max index for _*ObjectLists[index] */
	MM_UnfinalizedObjectList *_unfinalizedObjectLists; /**< An array of lists of unfinalized objects in this region */
	MM_OwnableSynchronizerObjectList *_ownableSynchronizerObjectLists; /**< An array of lists of ownable synchronizer objects in this region */
	MM_ContinuationObjectList *_continuationObjectLists; /**< An array of lists of continuation objects in this region */
	MM_ReferenceObjectList *_referenceObjectLists; /**< An array of lists of reference objects (i.e. weak/soft/phantom) in this region */
} MM_HeapRegionDescriptorStandardExtension;

class MM_ConfigurationDelegate
{
/*
 * Member data and types
 */
private:
	uintptr_t _maximumDefaultNumberOfGCThreads;
	const MM_GCPolicy _gcPolicy;
	MM_GCExtensions *_extensions;

protected:
public:

/*
 * Member functions
 */
private:
	MM_HeapRegionDescriptorStandardExtension *
	allocateHeapRegionDescriptorExtension(MM_EnvironmentBase *env, uintptr_t listCount) {
		uintptr_t allocSize = sizeof(MM_HeapRegionDescriptorStandardExtension) + (listCount * (sizeof(MM_UnfinalizedObjectList) + sizeof(MM_OwnableSynchronizerObjectList) + sizeof(MM_ContinuationObjectList) + sizeof(MM_ReferenceObjectList)));

		MM_HeapRegionDescriptorStandardExtension *regionExtension = (MM_HeapRegionDescriptorStandardExtension *)env->getForge()->allocate(allocSize, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());

		if (NULL == regionExtension) {
			return NULL;
		}

		regionExtension->_maxListIndex = listCount;
		regionExtension->_unfinalizedObjectLists = (MM_UnfinalizedObjectList *) ((uintptr_t)regionExtension + sizeof(MM_HeapRegionDescriptorStandardExtension));
		regionExtension->_ownableSynchronizerObjectLists = (MM_OwnableSynchronizerObjectList *) (regionExtension->_unfinalizedObjectLists + listCount);
		regionExtension->_continuationObjectLists = (MM_ContinuationObjectList *) (regionExtension->_ownableSynchronizerObjectLists + listCount);
		regionExtension->_referenceObjectLists = (MM_ReferenceObjectList *) (regionExtension->_continuationObjectLists + listCount);

		return regionExtension;
	}

	void
	initalizeUnfinalizedObjectList(MM_EnvironmentBase *env, MM_UnfinalizedObjectList *list)
	{
		new(list) MM_UnfinalizedObjectList();
		list->setNextList(_extensions->unfinalizedObjectLists);
		list->setPreviousList(NULL);
		if (NULL != _extensions->unfinalizedObjectLists) {
			_extensions->unfinalizedObjectLists->setPreviousList(list);
		}
		_extensions->unfinalizedObjectLists = list;
	}

	void
	initalizeOwnableSynchronizerObjectList(MM_EnvironmentBase *env, MM_OwnableSynchronizerObjectList *list)
	{
		new(list) MM_OwnableSynchronizerObjectList();
		list->setNextList(_extensions->getOwnableSynchronizerObjectLists());
		list->setPreviousList(NULL);
		if (NULL != _extensions->getOwnableSynchronizerObjectLists()) {
			_extensions->getOwnableSynchronizerObjectLists()->setPreviousList(list);
		}
		_extensions->setOwnableSynchronizerObjectLists(list);
	}

	void
	initalizeContinuationObjectList(MM_EnvironmentBase *env, MM_ContinuationObjectList *list)
	{
		new(list) MM_ContinuationObjectList();
		list->setNextList(_extensions->getContinuationObjectLists());
		list->setPreviousList(NULL);
		if (NULL != _extensions->getContinuationObjectLists()) {
			_extensions->getContinuationObjectLists()->setPreviousList(list);
		}
		_extensions->setContinuationObjectLists(list);
	}

	void
	initalizeReferenceObjectList(MM_EnvironmentBase *env, MM_ReferenceObjectList *list)
	{
		new(list) MM_ReferenceObjectList();
	}
protected:

public:
	bool
	initialize(MM_EnvironmentBase *env, MM_GCWriteBarrierType writeBarrierType, MM_GCAllocationType allocationType)
	{
		/* sync J9 VM arraylet size with OMR VM */
		OMR_VM *omrVM = env->getOmrVM();
		J9JavaVM *javaVM = (J9JavaVM*)omrVM->_language_vm;
		javaVM->arrayletLeafSize = omrVM->_arrayletLeafSize;
		javaVM->arrayletLeafLogSize = omrVM->_arrayletLeafLogSize;

		/* set write barrier for J9 VM -- catch -Xgc:alwayscallwritebarrier first */
		_extensions = MM_GCExtensions::getExtensions(javaVM);
		if (_extensions->alwaysCallWriteBarrier) {
			writeBarrierType = gc_modron_wrtbar_always;
		}

		Assert_MM_true(gc_modron_wrtbar_illegal != writeBarrierType);
		javaVM->gcWriteBarrierType = writeBarrierType;

		if (_extensions->alwaysCallReadBarrier) {
			/* AlwaysCallReadBarrier takes precedence over other read barrier types */
			javaVM->gcReadBarrierType = gc_modron_readbar_always;
		} else if (_extensions->isScavengerEnabled() && _extensions->isConcurrentScavengerEnabled()) {
			javaVM->gcReadBarrierType = gc_modron_readbar_range_check;
		} else if (_extensions->isVLHGC() && _extensions->isConcurrentCopyForwardEnabled()) {
			javaVM->gcReadBarrierType = gc_modron_readbar_region_check;
		} else {
			javaVM->gcReadBarrierType = gc_modron_readbar_none;
		}

		/* set allocation type for J9 VM */
		javaVM->gcAllocationType = allocationType;

		if (!_extensions->dynamicClassUnloadingSet) {
			_extensions->dynamicClassUnloading = MM_GCExtensions::DYNAMIC_CLASS_UNLOADING_ON_CLASS_LOADER_CHANGES;
		}

		/* Enable string constant collection by default if we support class unloading */
		_extensions->collectStringConstants = true;

		/*
		 *  note that these are the default thresholds but Realtime Configurations override these values, in their initialize methods
		 * (hence it is key for them to call their super initialize, first)
		 */
#define DYNAMIC_CLASS_UNLOADING_THRESHOLD			6
#define DYNAMIC_CLASS_UNLOADING_KICKOFF_THRESHOLD	80000

		if (!_extensions->dynamicClassUnloadingThresholdForced) {
			_extensions->dynamicClassUnloadingThreshold = DYNAMIC_CLASS_UNLOADING_THRESHOLD;
		}
		if (!_extensions->dynamicClassUnloadingKickoffThresholdForced) {
			_extensions->dynamicClassUnloadingKickoffThreshold = DYNAMIC_CLASS_UNLOADING_KICKOFF_THRESHOLD;
		}
		return true;
	}

	void
	tearDown(MM_EnvironmentBase *env)
	{
		J9JavaVM *vm = (J9JavaVM *)env->getOmrVM()->_language_vm;

		if (NULL != vm->identityHashData) {
			env->getForge()->free(vm->identityHashData);
			vm->identityHashData = NULL;
		}

		if (NULL != _extensions->classLoaderManager) {
			_extensions->classLoaderManager->kill(env);
			_extensions->classLoaderManager = NULL;
		}

		if (NULL != _extensions->stringTable) {
			_extensions->stringTable->kill(env);
			_extensions->stringTable = NULL;
		}
	}

	OMR_SizeClasses *getSegregatedSizeClasses(MM_EnvironmentBase *env)
	{
		J9JavaVM *javaVM = (J9JavaVM*)env->getOmrVM()->_language_vm;
		return (OMR_SizeClasses *)(javaVM->realtimeSizeClasses);
	}

	static MM_HeapRegionDescriptorStandardExtension *
	getHeapRegionDescriptorStandardExtension(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
	{
		MM_HeapRegionDescriptorStandardExtension *regionExtension = NULL;
		if (env->getExtensions()->isStandardGC()) {
			regionExtension = (MM_HeapRegionDescriptorStandardExtension *)region->_heapRegionDescriptorExtension;
		}
		return regionExtension;
	}

	bool
	initializeHeapRegionDescriptorExtension(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
	{
		if (_extensions->isStandardGC()) {
			uintptr_t listCount = _extensions->gcThreadCount;

			MM_HeapRegionDescriptorStandardExtension *regionExtension = allocateHeapRegionDescriptorExtension(env, listCount);
			if (NULL == regionExtension) {
				return false;
			}

			for (uintptr_t list = 0; list < listCount; list++) {
				initalizeUnfinalizedObjectList(env, &regionExtension->_unfinalizedObjectLists[list]);
				initalizeOwnableSynchronizerObjectList(env, &regionExtension->_ownableSynchronizerObjectLists[list]);
				initalizeContinuationObjectList(env, &regionExtension->_continuationObjectLists[list]);
				initalizeReferenceObjectList(env, &regionExtension->_referenceObjectLists[list]);
			}

			region->_heapRegionDescriptorExtension = regionExtension;
		}

		return true;
	}

	void
	teardownHeapRegionDescriptorExtension(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
	{
		if (env->getExtensions()->isStandardGC()) {
			if (NULL != region->_heapRegionDescriptorExtension) {
				env->getForge()->free(region->_heapRegionDescriptorExtension);
				region->_heapRegionDescriptorExtension = NULL;
			}
		}
	}

	bool
	heapInitialized(MM_EnvironmentBase *env)
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

		J9JavaVM *javaVM = (J9JavaVM*)env->getOmrVM()->_language_vm;
		uintptr_t size = offsetof(J9IdentityHashData, hashSaltTable) + (sizeof(U_32) * hashSaltCount);
		javaVM->identityHashData = (J9IdentityHashData*)env->getForge()->allocate(size, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
		bool result = NULL != javaVM->identityHashData;
		if (result) {
			J9IdentityHashData *hashData = javaVM->identityHashData;
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
	getInitialNumberOfPooledEnvironments(MM_EnvironmentBase *env)
	{
		return 0;
	}

	bool
	environmentInitialized(MM_EnvironmentBase *env)
	{
		J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
		OMR_VM *omrVM = env->getOmrVM();

		/* only assign this parent list if we are going to use it */
		if (_extensions->isStandardGC()) {
			vmThread->gcRememberedSet.parentList = &_extensions->rememberedSet;
		}

		_extensions->accessBarrier->initializeForNewThread(env);

		if ((_extensions->isConcurrentMarkEnabled()) && (!_extensions->usingSATBBarrier())) {
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
			vmThread->cardTableVirtualStart = (U_8*)j9gc_incrementalUpdate_getCardTableVirtualStart(omrVM);
			vmThread->cardTableShiftSize = j9gc_incrementalUpdate_getCardTableShiftValue(omrVM);
			MM_ConcurrentGC *concurrentGC = (MM_ConcurrentGC *)_extensions->getGlobalCollector();
			if (!_extensions->optimizeConcurrentWB || (CONCURRENT_OFF < concurrentGC->getConcurrentGCStats()->getExecutionMode())) {
				vmThread->privateFlags |= J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
			}
#else
			Assert_MM_unreachable();
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
		} else if (_extensions->isVLHGC()) {
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

		if (_extensions->fvtest_disableInlineAllocation) {
			env->_objectAllocationInterface->disableCachedAllocations(env);
		}

		return true;
	}

	bool canCollectFragmentationStats(MM_EnvironmentBase *env)
	{
		J9JavaVM *javaVM = (J9JavaVM*)env->getOmrVM()->_language_vm;
		/* processing estimate Fragmentation only is on non startup stage to avoid startup regression(fragmentation during startup is not meaningful for the estimation)
		   it is only for jit mode(for int mode javaVM->phase is always J9VM_PHASE_NOT_STARTUP) */
		return (J9VM_PHASE_NOT_STARTUP == javaVM->phase);
	}

	uintptr_t getMaxGCThreadCount(MM_EnvironmentBase *env)
	{
		return _maximumDefaultNumberOfGCThreads;
	}

	void setMaxGCThreadCount(MM_EnvironmentBase *env, uintptr_t maxGCThreads)
	{
		_maximumDefaultNumberOfGCThreads = maxGCThreads;
	}

	MM_GCPolicy getGCPolicy() { return _gcPolicy; }

	/**
	 * Constructor.
	 */
	MM_ConfigurationDelegate(MM_GCPolicy gcPolicy) :
		_maximumDefaultNumberOfGCThreads(64)
		, _gcPolicy(gcPolicy)
	{}
};

#endif /* CONFIGURATIONDELEGATE_HPP_ */
