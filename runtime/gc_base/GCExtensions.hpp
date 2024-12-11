
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(GCEXTENSIONS_HPP_)
#define GCEXTENSIONS_HPP_

#include <string.h>

#include "arrayCopyInterface.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "omrhookable.h"
#include "mmhook_internal.h"
#include "modronbase.h"
#include "omr.h"
#include "omrmodroncore.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkJavaStats.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "VMThreadListIterator.hpp"
#include "VerboseGCInterface.h"

#if defined(J9VM_GC_MODRON_SCAVENGER)
#include "ScavengerJavaStats.hpp"
#endif /* J9VM_GC_MODRON_SCAVENGER */

class MM_ClassLoaderManager;
class MM_EnvironmentBase;
class MM_HeapMap;
class MM_MemorySubSpace;
class MM_ObjectAccessBarrier;
class MM_OwnableSynchronizerObjectList;
class MM_ContinuationObjectList;
class MM_StringTable;
class MM_UnfinalizedObjectList;
class MM_Wildcard;

#if defined(J9VM_GC_FINALIZATION)
class GC_FinalizeListManager;
#endif /* J9VM_GC_FINALIZATION */

#if defined(J9VM_GC_REALTIME)
class MM_ReferenceObjectList;
#endif /* J9VM_GC_REALTIME */

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
class MM_IdleGCManager;
#endif

#define DEFAULT_SURVIVOR_MINIMUM_FREESIZE 	2048
#define DEFAULT_SURVIVOR_THRESHOLD 			512
#define MAXIMUM_SURVIVOR_MINIMUM_FREESIZE 	524288
#define MAXIMUM_SURVIVOR_THRESHOLD 			8192
#define MINIMUM_SURVIVOR_MINIMUM_FREESIZE 	512
#define MINIMUM_SURVIVOR_THRESHOLD 			512


#define J9_MAXIMUM_TLH_SIZE (1024 * 1024)
#if defined(J9VM_GC_BATCH_CLEAR_TLH)
#define J9_MAXIMUM_TLH_SIZE_BATCH_CLEAR (128 * 1024)
#endif /* defined(J9VM_GC_BATCH_CLEAR_TLH) */

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_GCExtensions : public MM_GCExtensionsBase {
private:
	MM_OwnableSynchronizerObjectList* ownableSynchronizerObjectLists; /**< The global linked list of ownable synchronizer object lists. */
	MM_ContinuationObjectList* continuationObjectLists; /**< The global linked list of continuation object lists. */
public:
	MM_StringTable* stringTable; /**< top level String Table structure (internally organized as a set of hash sub-tables */

	void* gcchkExtensions;

	void* tgcExtensions;
	J9MemoryManagerVerboseInterface verboseFunctionTable;

#if defined(J9VM_GC_FINALIZATION)
	intptr_t finalizeCycleInterval;
	intptr_t finalizeCycleLimit;
#endif /* J9VM_GC_FINALIZATION */

	MM_HookInterface hookInterface;

	bool collectStringConstants;

	MM_MarkJavaStats markJavaStats;
#if defined(J9VM_GC_MODRON_SCAVENGER)
	MM_ScavengerJavaStats scavengerJavaStats;
#endif /* J9VM_GC_MODRON_SCAVENGER */
	MM_ContinuationStats continuationStats;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	enum DynamicClassUnloading {
		DYNAMIC_CLASS_UNLOADING_NEVER,
		DYNAMIC_CLASS_UNLOADING_ON_CLASS_LOADER_CHANGES,
		DYNAMIC_CLASS_UNLOADING_ALWAYS
	};
	DynamicClassUnloading dynamicClassUnloading;

	bool dynamicClassUnloadingSet; /**< is true if value for dynamicClassUnloading was specified in command line */

	uintptr_t runtimeCheckDynamicClassUnloading; /**< set to true if class unloading is to be performed for the current GC cycle */
	bool dynamicClassUnloadingKickoffThresholdForced; /**< true if classUnloadingKickoffThreshold is specified in java options. */
	bool dynamicClassUnloadingThresholdForced; /**< true if classUnloadingThresholdForced is specified in java options. */
	uintptr_t dynamicClassUnloadingKickoffThreshold; /**< the threshold to kickoff a concurrent global GC from a scavenge */
	uintptr_t dynamicClassUnloadingThreshold; /**< the threshold to trigger class unloading during a global GC */
	double classUnloadingAnonymousClassWeight; /**< The weight factor to apply to anonymous classes for threshold comparisons */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	U_32 _stringTableListToTreeThreshold; /**< Threshold at which we start using trees instead of lists for collision resolution in the String table */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool fvtest_forceFinalizeClassLoaders;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	uintptr_t maxSoftReferenceAge; /**< The fixed age specified as the soft reference threshold which acts as our baseline for the dynamicMaxSoftReferenceAge */
	uintptr_t dynamicMaxSoftReferenceAge; /**< The age which represents the clearing age of soft references for a globalGC cycle.  At the end of a GC cycle, it will be updated for the following cycle by taking the percentage of free heap in the oldest generation as a fraction of the maxSoftReferenceAge */
#if defined(J9VM_GC_FINALIZATION)
	GC_FinalizeListManager* finalizeListManager;
#endif /* J9VM_GC_FINALIZATION */

	J9ReferenceArrayCopyTable referenceArrayCopyTable;

#if defined(J9VM_GC_REALTIME)
	MM_ReferenceObjectList* referenceObjectLists; /**< A global array of lists of reference objects (i.e. weak/soft/phantom) */
#endif /* J9VM_GC_REALTIME */
	MM_ObjectAccessBarrier* accessBarrier;

#if defined(J9VM_GC_FINALIZATION)
	uintptr_t finalizeMainPriority; /**< cmd line option to set finalize main thread priority */
	uintptr_t finalizeWorkerPriority; /**< cmd line option to set finalize worker thread priority */
#endif /* J9VM_GC_FINALIZATION */

	MM_ClassLoaderManager* classLoaderManager; /**< Pointer to the gc's classloader manager to process classloaders/classes */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	uintptr_t deadClassLoaderCacheSize; /**< threshold after which we flush class segments (not done for every class unloading, since it requires heap walk) */
#endif /*defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	MM_UnfinalizedObjectList* unfinalizedObjectLists; /**< The global linked list of unfinalized object lists. */
	
	uintptr_t objectListFragmentCount; /**< the size of Local Object Buffer(per gc thread), used by referenceObjectBuffer, UnfinalizedObjectBuffer and OwnableSynchronizerObjectBuffer */

	MM_Wildcard* numaCommonThreadClassNamePatterns; /**< A linked list of thread class names which should be associated with the common context */

	class UserSpecifiedParameters {
	private:
	protected:
	public:
		MM_UserSpecifiedParameterUDATA _Xmn; /**< Initial value of -Xmn specified by the user */
		MM_UserSpecifiedParameterUDATA _Xmns; /**< Initial value of -Xmns specified by the user */
		MM_UserSpecifiedParameterUDATA _Xmnx; /**< Initial value of -Xmnx specified by the user */
		MM_UserSpecifiedParameterUDATA _Xmx; /**< Initial value of -Xmx specified by the user */
#if defined(J9VM_OPT_CRIU_SUPPORT)
		MM_UserSpecifiedParameterUDATA _checkpointGCThreads; /**< Initial value of -XX:CheckpointGCThreads specified by the user */
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	private:
	protected:
	public:
		UserSpecifiedParameters()
			: _Xmn()
			, _Xmns()
			, _Xmnx()
			, _Xmx()
#if defined(J9VM_OPT_CRIU_SUPPORT)
			, _checkpointGCThreads()
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
		{
		}
	};

	UserSpecifiedParameters userSpecifiedParameters; /**< A collection of user-speicifed parameters */

	bool tlhMaximumSizeSpecified; /**< true, if tlhMaximumSize specified by a command line option */

	MM_UserSpecifiedParameterBool virtualLargeObjectHeap; /**< off heap option */

	bool dynamicHeapAdjustmentForRestore; /**< If set to true, the default heuristic-calculated softmx is prioritized over the user-specified values. */
	/**
	 * Values for com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY
	 * must hava the same values as J9_JIT_STRING_DEDUP_POLICY_DISABLED, J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER and J9_JIT_STRING_DEDUP_POLICY_FAVOUR_HIGHER.
	 */
	enum JitStringDeDupPolicy {
		J9_JIT_STRING_DEDUP_POLICY_DISABLED = 0,
		J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER = 1,
		J9_JIT_STRING_DEDUP_POLICY_FAVOUR_HIGHER = 2,
		J9_JIT_STRING_DEDUP_POLICY_UNDEFINED = 3,
	};
	JitStringDeDupPolicy stringDedupPolicy;

	intptr_t _asyncCallbackKey; /**< the key for async callback used in Concurrent Marking for threads to scan their own stacks */
	intptr_t _TLHAsyncCallbackKey; /**< the key for async callback used to support instrumentable allocations */

	bool _HeapManagementMXBeanBackCompatibilityEnabled;

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	MM_IdleGCManager* idleGCManager; /**< Manager which registers for VM Runtime State notification & manages free heap on notification */
#endif

	double maxRAMPercent; /**< Value of -XX:MaxRAMPercentage specified by the user */
	double initialRAMPercent; /**< Value of -XX:InitialRAMPercentage specified by the user */
	uintptr_t minimumFreeSizeForSurvivor; /**< minimum free size can be reused by collector as survivor, for balanced GC only */
	uintptr_t freeSizeThresholdForSurvivor; /**< if average freeSize(freeSize/freeCount) of the region is smaller than the Threshold, the region would not be reused by collector as survivor, for balanced GC only */
	bool recycleRemainders; /**< true if need to recycle TLHRemainders at the end of PGC, for balanced GC only */

	bool forceGPFOnHeapInitializationError; /**< if set causes GPF generation on heap initialization error */

	enum ContinuationListOption {
		disable_continuation_list = 0,
		enable_continuation_list = 1,
		verify_continuation_list = 2,
	};
	ContinuationListOption continuationListOption;

	enum TimingAddContinuationInList {
		onCreated = 0,
		onStarted = 1,
	};
	TimingAddContinuationInList timingAddContinuationInList;
	bool testContainerMemLimit; /**< if set simulates a container with memory limit set - for GC testing only*/
	double testRAMSizePercentage; /**< a percentage to increase/decrease usablePhysicalMemory - for GC testing only, only applies to CRIU restore VM */
protected:
private:
protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

public:
	static MM_GCExtensions* newInstance(MM_EnvironmentBase* env);
	virtual void kill(MM_EnvironmentBase* env);
	/**
	 * Compute the default max heap size for java based on physical memory and other metrics.
	 * Note this function is used to adjust the max heap size during gc initialization,
	 * and to adjust the soft max value during the gc restore initialization in case the
	 * available memory in the environment reduces
	 * @param[in] enableOriginalJDK8HeapSizeCompatibilityOption Boolean value indicates if
	 * we enable original heap size compatibility option for JDK under JDK8
	 * @return Computed max heap size
	 */
	uintptr_t computeDefaultMaxHeapForJava(bool enableOriginalJDK8HeapSizeCompatibilityOption);

	MMINLINE J9HookInterface** getHookInterface() { return J9_HOOK_INTERFACE(hookInterface); };

	/**
	 * Fetch the global string table instance.
	 * @return the string table
	 */
	MMINLINE MM_StringTable* getStringTable() { return stringTable; }

	MMINLINE uintptr_t getDynamicMaxSoftReferenceAge()
	{
		return dynamicMaxSoftReferenceAge;
	}

	MMINLINE uintptr_t getMaxSoftReferenceAge()
	{
		return maxSoftReferenceAge;
	}

	virtual void identityHashDataAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress);
	virtual void identityHashDataRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress);

	void updateIdentityHashDataForSaltIndex(uintptr_t index);

	/**
	 * Set Tenure address range
	 * @param base low address of Old subspace range
	 * @param size size of Old subspace in bytes
	 */
	virtual void setTenureAddressRange(void* base, uintptr_t size)
	{
		_tenureBase = base;
		_tenureSize = size;

		/* todo: dagar move back to MemorySubSpaceGeneric addTenureRange() and removeTenureRange() once
		 * heapBaseForBarrierRange0 heapSizeForBarrierRange0 can be removed from J9VMThread
		 *
		 * setTenureAddressRange() can be removed fromo GCExtensions.hpp and made inline again
		 */
		GC_OMRVMThreadListIterator omrVMThreadListIterator(_omrVM);
		while (OMR_VMThread* walkThread = omrVMThreadListIterator.nextOMRVMThread()) {
			walkThread->lowTenureAddress = heapBaseForBarrierRange0;
			walkThread->highTenureAddress = (void*)((uintptr_t)heapBaseForBarrierRange0 + heapSizeForBarrierRange0);
			walkThread->heapBaseForBarrierRange0 = heapBaseForBarrierRange0;
			walkThread->heapSizeForBarrierRange0 = heapSizeForBarrierRange0;
		}

		GC_VMThreadListIterator vmThreadListIterator((J9JavaVM*)_omrVM->_language_vm);
		while (J9VMThread* walkThread = vmThreadListIterator.nextVMThread()) {
			walkThread->lowTenureAddress = heapBaseForBarrierRange0;
			walkThread->highTenureAddress = (void*)((uintptr_t)heapBaseForBarrierRange0 + heapSizeForBarrierRange0);
			walkThread->heapBaseForBarrierRange0 = heapBaseForBarrierRange0;
			walkThread->heapSizeForBarrierRange0 = heapSizeForBarrierRange0;
		}
	}


	static MM_GCExtensions* getExtensions(OMR_VM* omrVM) { return static_cast<MM_GCExtensions*>(MM_GCExtensionsBase::getExtensions(omrVM)); }
	static MM_GCExtensions* getExtensions(OMR_VMThread* omrVMThread) { return static_cast<MM_GCExtensions*>(MM_GCExtensionsBase::getExtensions(omrVMThread->_vm)); }
	static MM_GCExtensions* getExtensions(J9JavaVM* javaVM) { return static_cast<MM_GCExtensions*>(MM_GCExtensionsBase::getExtensions(javaVM->omrVM)); }
	static MM_GCExtensions* getExtensions(J9VMThread* vmThread) { return getExtensions(vmThread->javaVM); }
	static MM_GCExtensions* getExtensions(MM_GCExtensionsBase* ext) { return static_cast<MM_GCExtensions*>(ext); }
	static MM_GCExtensions* getExtensions(MM_EnvironmentBase* env) { return getExtensions(env->getExtensions()); }

	MMINLINE J9JavaVM* getJavaVM() {return static_cast<J9JavaVM*>(_omrVM->_language_vm);}
	
	/**
	 * Return ownable synchronizer object lists by first ensuring that the lists are in a consistent state (e.g., during concurrent gc).
	 * This should be used by any external consumer (non-GC consuming the list)     
	 * @param vmThread The current J9VMThread thread (used to invoke j9gc apis if required)
	 * @return Linked list of ownable synchronizer objects
	 */
	MM_OwnableSynchronizerObjectList* getOwnableSynchronizerObjectListsExternal(J9VMThread *vmThread);
	MMINLINE MM_OwnableSynchronizerObjectList* getOwnableSynchronizerObjectLists() { return ownableSynchronizerObjectLists; }
	MMINLINE void setOwnableSynchronizerObjectLists(MM_OwnableSynchronizerObjectList* newOwnableSynchronizerObjectLists) { ownableSynchronizerObjectLists = newOwnableSynchronizerObjectLists; }

	MM_ContinuationObjectList* getContinuationObjectListsExternal(J9VMThread *vmThread);
	MMINLINE MM_ContinuationObjectList* getContinuationObjectLists() { return continuationObjectLists; }
	MMINLINE void setContinuationObjectLists(MM_ContinuationObjectList* newContinuationObjectLists) { continuationObjectLists = newContinuationObjectLists; }

	void releaseNativesForContinuationObject(MM_EnvironmentBase* env, j9object_t objectPtr);

	/**
	 * Prints error message and forces GPF if -XXgc:forceGPFOnHeapInitializationError is provided
	 * @param[in] vm the current J9JavaVM
	 * @param[in] errorMessage error message set by GC heap initialization if available
	 */
	void handleInitializeHeapError(J9JavaVM *vm, const char *errorMessage);

	/**
	 * Check if we need to scan the java stack for the Continuation Object
	 * Used during main scan phase of GC (object graph traversal) or heap object iteration (in sliding compact).
	 * Not meant to be used during root scanning (neither strong roots nor weak roots)!
	 *
	 * @param[in] vmThread the current J9VMThread
	 * @param[in] continuationObject the continuation object
	 * @param[in] isGlobalGC
	 * @return true if we need to scan the java stack
	 */
	static bool needScanStacksForContinuationObject(J9VMThread *vmThread, j9object_t objectPtr, bool isConcurrentGC, bool isGlobalGC, bool beingMounted);
	/*
	 *
	 * param[in] checkConcurrentState can be J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_LOCAL or J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_GLOBAL
	 *
	 *  There is no need scanning before continuation is started or after continuation is finished.
	 *  If WinningConcurrentGCScan set J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_LOCAL or J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_GLOBAL in the state base on checkConcurrentState
	 *	If low tagging(CONCURRENT_SCAN_LOCAL or CONCURRENT_SCAN_GLOBAL) failed due to either
	 *
	 *   a carrier thread winning to mount, we don't need to do anything, since it will be compensated by pre/post mount actions
	 *   another GC thread winning to scan(CONCURRENT_SCAN_LOCAL and CONCURRENT_SCAN_GLOBAL are irrelevant and independent), again don't do anything, and let the winning thread do the work, instead
	 */
	static bool tryWinningConcurrentGCScan(ContinuationState volatile *continuationStatePtr, bool isGlobalGC, bool beingMounted);

	/**
	 * clear CONCURRENTSCANNING flag for LocalConcurrentScanning or for GlobalConcurrentScanning base on isGlobalGC,
	 * if all CONCURRENTSCANNING bits are cleared and the continuation mounting is blocked by concurrent scanning, notify it.
	 */
	MMINLINE static void exitConcurrentGCScan(ContinuationState volatile *continuationStatePtr, bool isGlobalGC);

	static void exitContinuationConcurrentGCScan(J9VMThread *vmThread, j9object_t continuationObject, bool isGlobalGC);

#if defined(J9VM_OPT_CRIU_SUPPORT)
	MMINLINE virtual bool reinitializationInProgress() { return (NULL != ((J9JavaVM*)_omrVM->_language_vm)->checkpointState.checkpointThread); }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	/**
	 * Create a GCExtensions object
	 */
	MM_GCExtensions()
		: MM_GCExtensionsBase()
		, ownableSynchronizerObjectLists(NULL)
		, continuationObjectLists(NULL)
		, stringTable(NULL)
		, gcchkExtensions(NULL)
		, tgcExtensions(NULL)
#if defined(J9VM_GC_FINALIZATION)
		, finalizeCycleInterval(J9_FINALIZABLE_INTERVAL)  /* 1/2 second */
		, finalizeCycleLimit(0)  /* 0 seconds (i.e. no time limit) */
#endif /* J9VM_GC_FINALIZATION */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		, dynamicClassUnloadingSet(false)
		, dynamicClassUnloadingKickoffThresholdForced(false)
		, dynamicClassUnloadingThresholdForced(false)
		, dynamicClassUnloadingKickoffThreshold(0)
		, dynamicClassUnloadingThreshold(0)
		, classUnloadingAnonymousClassWeight(1.0)
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		, _stringTableListToTreeThreshold(1024)
		, maxSoftReferenceAge(32)
#if defined(J9VM_GC_FINALIZATION)
		, finalizeMainPriority(J9THREAD_PRIORITY_NORMAL)
		, finalizeWorkerPriority(J9THREAD_PRIORITY_NORMAL)
#endif /* J9VM_GC_FINALIZATION */
		, classLoaderManager(NULL)
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		, deadClassLoaderCacheSize(1024 * 1024) /* default is one MiB */
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
		, unfinalizedObjectLists(NULL)
		, objectListFragmentCount(0)
		, numaCommonThreadClassNamePatterns(NULL)
		, userSpecifiedParameters()
		, tlhMaximumSizeSpecified(false)
		, virtualLargeObjectHeap()
		, dynamicHeapAdjustmentForRestore(false)
		, stringDedupPolicy(J9_JIT_STRING_DEDUP_POLICY_UNDEFINED)
		, _asyncCallbackKey(-1)
		, _TLHAsyncCallbackKey(-1)
		, _HeapManagementMXBeanBackCompatibilityEnabled(false)
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
		, idleGCManager(NULL)
#endif
		, maxRAMPercent(-1.0) /* this would get overwritten by user specified value */
		, initialRAMPercent(0.0) /* this would get overwritten by user specified value */
		, minimumFreeSizeForSurvivor(DEFAULT_SURVIVOR_MINIMUM_FREESIZE)
		, freeSizeThresholdForSurvivor(DEFAULT_SURVIVOR_THRESHOLD)
		, recycleRemainders(true)
		, forceGPFOnHeapInitializationError(false)
		, continuationListOption(enable_continuation_list)
		, timingAddContinuationInList(onCreated)
		, testContainerMemLimit(false)
		, testRAMSizePercentage(-1.0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* GCEXTENSIONS_HPP_ */
