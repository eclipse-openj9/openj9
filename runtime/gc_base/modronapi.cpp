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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"

#include "omrgcconsts.h"
#include "modronbase.h"

#include "modronapi.hpp"
#include "modronopt.h"
#include "modronnls.h"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapMemorySnapshot.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "GlobalCollector.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "ContinuationObjectBuffer.hpp"
#include "ParallelDispatcher.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemoryPoolLargeObjects.hpp"
#include "VMInterface.hpp"
#include "VMThreadListIterator.hpp"
#include "VMAccess.hpp"

#if defined(J9VM_OPT_CRIU_SUPPORT)
#include "Configuration.hpp"
#include "VerboseManager.hpp"
#include "vmaccess.h"
#include "verbosenls.h"
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

extern "C" {

UDATA
j9gc_modron_global_collect(J9VMThread *vmThread)
{
	return j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC);
}

/**
 * Trigger a global GC with the specified override code
 * 
 * @param[in] vmThread - the current thread
 * @param[in] gcCode - one of: 
 * <ul>
 * 	<li>J9MMCONSTANT_IMPLICIT_GC_DEFAULT - collect due to normal GC activity</li>
 *  <li>J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE - second collect since first collect was insufficient</li>
 * 	<li>J9MMCONSTANT_IMPLICIT_GC_PERCOLATE - collect due to scavenger percolate</li>
 * 	<li>J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES - collect due to scavenger percolate (unloading classes)</li>
 * 	<li>J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE - collect due to aggressive scavenger percolate</li>
 * 	<li>J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC - Java code has requested a System.gc()</li>
 *  <li>J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE - Java code has requested a non-compacting GC via JVM_GCNoCompact</li>
 *  <li>J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY - a native out of memory has occurred -- attempt to recover as much native memory as possible</li>
 *  <li>J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT - a dump agent has requested compaction (usually before producing a heapdump). Note that the dump agent must have already acquired exclusive access before triggering the GC.</li>
 * </ul>
 * @return always returns zero
 */
UDATA
j9gc_modron_global_collect_with_overrides(J9VMThread *vmThread, U_32 gcCode)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if ((J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC == gcCode) || (J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE == gcCode)) {
		if (MM_GCExtensions::getExtensions(env)->disableExplicitGC) {
			return 0;
		}
	}

	/* Prevent thread to respond to safe point; GC has higher priority */
	VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);
	MM_GCExtensions::getExtensions(env)->heap->systemGarbageCollect(env, gcCode);
	VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);

	/* During GC that we just performed, this thread might have temporarily released and required VM access, misleading other threads
	 * that we will not proceed running (like waking up an inspector thread to walk this thread's stack). Hence we have to check again if we are expected to halt.
	 * An exceptional case is if this thread already blocked all other threads globally (for example RAS DUMP acquired exclusive VM access prior to entering here),
	 * in which case we are safe to proceed - with such externally obtained exclusive VM access, we would not have yielded VM access and would not have mislead other threads.
	 * On the other side, while still holding exclusive, blocking would be wrong, leading to a deadlock.
	 */
	if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY) && (0 == vmThread->omrVMThread->exclusiveCount)) {
		vmThread->javaVM->internalVMFunctions->internalReleaseVMAccess(vmThread);
		vmThread->javaVM->internalVMFunctions->internalAcquireVMAccess(vmThread);
	}

	return 0;
}

UDATA
j9gc_modron_local_collect(J9VMThread *vmThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);
	((MM_MemorySpace *)vmThread->omrVMThread->memorySpace)->localGarbageCollect(env, J9MMCONSTANT_IMPLICIT_GC_DEFAULT);
	VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);

	if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY) && (0 == vmThread->omrVMThread->exclusiveCount)) {
		vmThread->javaVM->internalVMFunctions->internalReleaseVMAccess(vmThread);
		vmThread->javaVM->internalVMFunctions->internalAcquireVMAccess(vmThread);
	}

	return 0;
}

UDATA
j9gc_heap_total_memory(J9JavaVM *javaVM)
{
	MM_Heap *heap = MM_GCExtensions::getExtensions(javaVM)->getHeap();
	MM_HeapRegionManager *manager = heap->getHeapRegionManager();
	return manager->getTotalHeapSize();
}

UDATA
j9gc_is_garbagecollection_disabled(J9JavaVM *javaVM)
{
	UDATA ret = 0;
	 if (gc_policy_nogc == MM_GCExtensions::getExtensions(javaVM)->configurationOptions._gcPolicy) {
		 ret = 1;
	 }
	 return ret;
}

/**
 * VM API for determining the amount of free memory available on the heap.
 * The call returns the approximate free memory on the heap available for allocation.  An approximation is used
 * in part because defered work (e.g., concurrent sweep) may be hiding potential free memory.
 * @return The approximate free memory available on the heap.
 */
UDATA 
j9gc_heap_free_memory(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->heap->getApproximateFreeMemorySize();
}

/* -- new APIs for providing information about supported memorypools and garbage collectors by current jvm  -- start */

/**
 * return integer presents all memory pool IDs supported by current jvm. 
 * base on current gcpolicy settings, set supported all memory pool IDs for current jvm
 * possible memorypool ID is defined in omrgcconsts.h, one bit for each possible memorypool, the bit is set if the memory pool is supported by currrent jvm.
 * gc also can decide which memory pools should expose to outside via updating this method.
 */
UDATA
j9gc_allsupported_memorypools(J9JavaVM *javaVM)
{
	UDATA memPools = 0;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
		memPools |= J9_GC_MANAGEMENT_POOL_JAVAHEAP;
	} else {
		OMR_VM *vm = extensions->getOmrVM();
		switch (vm->gcPolicy)
		{
		case J9_GC_POLICY_GENCON :
			memPools |= J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE;
			memPools |= J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR;
		case J9_GC_POLICY_OPTAVGPAUSE :
		case J9_GC_POLICY_OPTTHRUPUT :
			if (extensions->largeObjectArea) {
				memPools |= J9_GC_MANAGEMENT_POOL_TENURED_SOA;
				memPools |= J9_GC_MANAGEMENT_POOL_TENURED_LOA;
			} else {
				memPools |= J9_GC_MANAGEMENT_POOL_TENURED;
			}
			break;
		case J9_GC_POLICY_NOGC :
			memPools |= J9_GC_MANAGEMENT_POOL_TENURED;
			break;
		case J9_GC_POLICY_BALANCED :
				memPools |= J9_GC_MANAGEMENT_POOL_REGION_OLD;
				memPools |= J9_GC_MANAGEMENT_POOL_REGION_EDEN;
				memPools |= J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR;
				memPools |= J9_GC_MANAGEMENT_POOL_REGION_RESERVED;
			break;
		case J9_GC_POLICY_METRONOME :
			memPools |= J9_GC_MANAGEMENT_POOL_JAVAHEAP;
			break;
		default :
			break;
		}
	}
	return memPools;
}

/**
 * return integer presents all garbage collector IDs(one bit for each possible garbage collector) supported by current jvm.
 */
UDATA
j9gc_allsupported_garbagecollectors(J9JavaVM *javaVM)
{
	UDATA collectors = 0;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	OMR_VM *vm = extensions->getOmrVM();
	switch (vm->gcPolicy)
	{
	case J9_GC_POLICY_GENCON :
		collectors |= J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE;
	case J9_GC_POLICY_OPTAVGPAUSE :
	case J9_GC_POLICY_OPTTHRUPUT :
	case J9_GC_POLICY_METRONOME :
		collectors |= J9_GC_MANAGEMENT_COLLECTOR_GLOBAL;
		break;
	case J9_GC_POLICY_BALANCED :
		collectors |= J9_GC_MANAGEMENT_COLLECTOR_PGC;
		collectors |= J9_GC_MANAGEMENT_COLLECTOR_GGC;
		break;
	case J9_GC_POLICY_NOGC :
		collectors |= J9_GC_MANAGEMENT_COLLECTOR_EPSILON;
		break;
	default :
		break;
	}
	return collectors;
}

/**
 * retrieve memory pool name
 */
const char *
j9gc_pool_name(J9JavaVM *javaVM, UDATA poolID)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	const char *name = NULL;
	switch (poolID) {
	case J9_GC_MANAGEMENT_POOL_TENURED_SOA :
		name = J9_GC_MANAGEMENT_POOL_NAME_TENURED_SOA;
		break;
	case J9_GC_MANAGEMENT_POOL_TENURED_LOA :
		name = J9_GC_MANAGEMENT_POOL_NAME_TENURED_LOA;
		break;
	case J9_GC_MANAGEMENT_POOL_TENURED :
		name = J9_GC_MANAGEMENT_POOL_NAME_TENURED;
		break;
	case J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE :
		name = J9_GC_MANAGEMENT_POOL_NAME_NURSERY_ALLOCATE;
		break;
	case J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR :
		name = J9_GC_MANAGEMENT_POOL_NAME_NURSERY_SURVIVOR;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_OLD :
		name = J9_GC_MANAGEMENT_POOL_NAME_BALANCED_OLD;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_EDEN :
		name = J9_GC_MANAGEMENT_POOL_NAME_BALANCED_EDEN;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR :
		name = J9_GC_MANAGEMENT_POOL_NAME_BALANCED_SURVIVOR;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_RESERVED :
		name = J9_GC_MANAGEMENT_POOL_NAME_BALANCED_RESERVED;
		break;
	case J9_GC_MANAGEMENT_POOL_JAVAHEAP :
		if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
			name = J9_GC_MANAGEMENT_POOL_NAME_HEAP_OLD;
		} else {
			name = J9_GC_MANAGEMENT_POOL_NAME_HEAP;
		}
		break;
	default:
		name = NULL;
		break;
	}
	return name;
}

/**
 * retrieve garbage collector name
 */
const char *
j9gc_garbagecollector_name(J9JavaVM *javaVM, UDATA gcID)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	const char *name = NULL;
	switch (gcID) {
	case J9_GC_MANAGEMENT_COLLECTOR_EPSILON :
		if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
			name = J9_GC_MANAGEMENT_GC_NAME_GLOBAL_OLD;
		} else {
			name = J9_GC_MANAGEMENT_GC_NAME_EPSILON;
		}
		break;
	case J9_GC_MANAGEMENT_COLLECTOR_GLOBAL :
		if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
			name = J9_GC_MANAGEMENT_GC_NAME_GLOBAL_OLD;
		} else {
			name = J9_GC_MANAGEMENT_GC_NAME_GLOBAL;
		}
		break;
	case J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE :
		if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
			name = J9_GC_MANAGEMENT_GC_NAME_LOCAL_OLD;
		} else {
			name = J9_GC_MANAGEMENT_GC_NAME_SCAVENGE;
		}
		break;
	case J9_GC_MANAGEMENT_COLLECTOR_PGC :
		if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
			name = J9_GC_MANAGEMENT_GC_NAME_LOCAL_OLD;
		} else {
			name = J9_GC_MANAGEMENT_GC_NAME_PGC;
		}
		break;
	case J9_GC_MANAGEMENT_COLLECTOR_GGC :
		if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
			name = J9_GC_MANAGEMENT_GC_NAME_GLOBAL_OLD;
		} else {
			name = J9_GC_MANAGEMENT_GC_NAME_GGC;
		}
		break;
	default:
		name = NULL;
		break;
	}
	return name;
}

/**
 * check if the memory pool is managed by the garbage collector
 */
UDATA
j9gc_is_managedpool_by_collector(J9JavaVM *javaVM, UDATA gcID, UDATA poolID)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
		if (J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE == gcID) {
			/* for BackCompatible mode scavenge does not try to reclaim memory from the whole heap, so we do not mark JavaHeap managed by scavenge */
			return 0;
		}
		return 1;
	}
	UDATA managedPools = 0;
	switch (gcID) {
	case J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE :
		managedPools |= J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE;
		managedPools |= J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR;
		break;
	case J9_GC_MANAGEMENT_COLLECTOR_PGC :
	case J9_GC_MANAGEMENT_COLLECTOR_GLOBAL :
	case J9_GC_MANAGEMENT_COLLECTOR_GGC :
	case J9_GC_MANAGEMENT_COLLECTOR_EPSILON :
	default:
		managedPools = j9gc_allsupported_memorypools(javaVM);
		break;
	}

	if (0 != (poolID & managedPools)) {
		return 1;
	} else {
		return 0;
	}

}


/**
 * check if the memory pool supports usage threshold
 */
UDATA
j9gc_is_usagethreshold_supported(J9JavaVM *javaVM, UDATA poolID)
{
	UDATA supported = 0;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
		supported = 1;
	} else {
		switch (poolID) {
		case J9_GC_MANAGEMENT_POOL_TENURED_SOA :
		case J9_GC_MANAGEMENT_POOL_TENURED_LOA :
		case J9_GC_MANAGEMENT_POOL_TENURED :
		case J9_GC_MANAGEMENT_POOL_REGION_OLD :
		case J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR :
		case J9_GC_MANAGEMENT_POOL_JAVAHEAP :
			supported = 1;
			break;
		default:
			break;
		}
	}
	return supported;
}

/**
 * check if the memory pool supports collection usage threshold
 */
UDATA
j9gc_is_collectionusagethreshold_supported(J9JavaVM *javaVM, UDATA poolID)
{
	UDATA supported = 0;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	if (extensions->_HeapManagementMXBeanBackCompatibilityEnabled) {
		supported = 1;
	} else {
		switch (poolID) {
		case J9_GC_MANAGEMENT_POOL_TENURED_SOA :
		case J9_GC_MANAGEMENT_POOL_TENURED_LOA :
		case J9_GC_MANAGEMENT_POOL_TENURED :
		case J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE :
		case J9_GC_MANAGEMENT_POOL_REGION_OLD :
		case J9_GC_MANAGEMENT_POOL_REGION_EDEN :
		case J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR :
		case J9_GC_MANAGEMENT_POOL_JAVAHEAP :
			supported = 1;
			break;
		default:
			break;
		}
	}
	return supported;
}

/**
 * check if the garbage collector is local collector
 */
UDATA
j9gc_is_local_collector(J9JavaVM *javaVM, UDATA gcID)
{
	UDATA local = 0;
	if ((J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE == gcID) || (J9_GC_MANAGEMENT_COLLECTOR_PGC == gcID)) {
		local = 1;
	}

	return local;
}

/**
 * get garbage collector id from internal cycle type
 */
UDATA
j9gc_get_collector_id(OMR_VMThread *omrVMThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	UDATA cycle_type = env->_cycleState->_type;

	UDATA id = 0;
	switch (cycle_type) {
	case OMR_GC_CYCLE_TYPE_GLOBAL :
		id = J9_GC_MANAGEMENT_COLLECTOR_GLOBAL;
		break;
	case OMR_GC_CYCLE_TYPE_SCAVENGE :
		id = J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE;
		break;
	case OMR_GC_CYCLE_TYPE_VLHGC_PARTIAL_GARBAGE_COLLECT :
		id = J9_GC_MANAGEMENT_COLLECTOR_PGC;
		break;
	case OMR_GC_CYCLE_TYPE_VLHGC_GLOBAL_MARK_PHASE :
		id = J9_GC_MANAGEMENT_COLLECTOR_GGC;
		break;
	case OMR_GC_CYCLE_TYPE_VLHGC_GLOBAL_GARBAGE_COLLECT :
		id = J9_GC_MANAGEMENT_COLLECTOR_GGC;
		break;
	case OMR_GC_CYCLE_TYPE_EPSILON :
		id = J9_GC_MANAGEMENT_COLLECTOR_EPSILON;
		break;
	default :
		break;
	}
	return id;
}

/**
 * retrieve total memory sizes and free memory sizes for the memory pools
 * array size = J9VM_MAX_HEAP_MEMORYPOOL_COUNT
 * index of array is the order of supported memory pool IDs (low bit comes first)
 * parm[in]		javaVM
 * parm[in]		poolIDs		the memory pool IDs
 * parm[out]	totals		array of total memory sizes
 * parm[out]	frees		array of free memory sizes
 */
UDATA
j9gc_pools_memory(J9JavaVM *javaVM, UDATA poolIDs, UDATA *totals, UDATA *frees, BOOLEAN gcEnd)
{
	UDATA supportedIDs = j9gc_allsupported_memorypools(javaVM);
	if (0 == poolIDs) {
		poolIDs = supportedIDs;
	}
	MM_GCExtensionsBase *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_HeapRegionManager *manager = extensions->getHeap()->getHeapRegionManager();
	MM_HeapMemorySnapshot snapShot;
	manager->getHeapMemorySnapshot(extensions, &snapShot, (TRUE == gcEnd));
	UDATA idx = 0;
	
	for (UDATA count = 0, mask = 1; count < J9_GC_MANAGEMENT_MAX_POOL; count++, mask <<= 1)
	{
		if (0 != (poolIDs & mask)) {
			switch (poolIDs & mask) {
			case J9_GC_MANAGEMENT_POOL_TENURED :
				totals[idx] = snapShot._totalTenuredSize;
				frees[idx] = snapShot._freeTenuredSize;
				break;
			case J9_GC_MANAGEMENT_POOL_TENURED_SOA :
				totals[idx] = snapShot._totalTenuredSOASize;
				frees[idx] = snapShot._freeTenuredSOASize;
				break;
			case J9_GC_MANAGEMENT_POOL_TENURED_LOA :
				totals[idx] = snapShot._totalTenuredLOASize;
				frees[idx] = snapShot._freeTenuredLOASize;
				break;
			case J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE :
				totals[idx] = snapShot._totalNurseryAllocateSize;
				frees[idx] = snapShot._freeNurseryAllocateSize;
				break;
			case J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR :
				totals[idx] = snapShot._totalNurserySurvivorSize;
				frees[idx] = snapShot._freeNurserySurvivorSize;
				break;
			case J9_GC_MANAGEMENT_POOL_REGION_OLD :
				totals[idx] = snapShot._totalRegionOldSize;
				frees[idx] = snapShot._freeRegionOldSize;
				break;
			case J9_GC_MANAGEMENT_POOL_REGION_EDEN :
				totals[idx] = snapShot._totalRegionEdenSize;
				frees[idx] = snapShot._freeRegionEdenSize;
				break;
			case J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR :
				totals[idx] = snapShot._totalRegionSurvivorSize;
				frees[idx] = snapShot._freeRegionSurvivorSize;
				break;
			case J9_GC_MANAGEMENT_POOL_REGION_RESERVED :
				totals[idx] = snapShot._totalRegionReservedSize;
				frees[idx] = snapShot._freeRegionReservedSize;
				break;
			case J9_GC_MANAGEMENT_POOL_JAVAHEAP :
				totals[idx] = snapShot._totalHeapSize;
				frees[idx] = snapShot._freeHeapSize;
				break;
			default :
				totals[idx] = 0;
				frees[idx] = 0;
				break;
			}
		}
		if (0 != (supportedIDs & mask)) {
			idx += 1;
		}
	}
	return poolIDs;
}

/**
 * retrieve the maximum memory size for the memory pools 
 */
UDATA
j9gc_pool_maxmemory(J9JavaVM *javaVM, UDATA poolID)
{
	UDATA maxsize = 0;
	MM_GCExtensionsBase *extensions = MM_GCExtensions::getExtensions(javaVM);

	switch (poolID) {
	case J9_GC_MANAGEMENT_POOL_TENURED_SOA :
		{
			MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
			MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
			MM_MemoryPoolLargeObjects *memoryPool = (MM_MemoryPoolLargeObjects *) tenureMemorySubspace->getMemoryPool();
			UDATA loaSize = (UDATA) (memoryPool->getLOARatio() * extensions->maxOldSpaceSize);
			loaSize = MM_Math::roundToCeiling(extensions->heapAlignment, loaSize);
			maxsize = extensions->maxOldSpaceSize - loaSize;
		}
		break;
	case J9_GC_MANAGEMENT_POOL_TENURED_LOA :
		{
			MM_MemorySpace *defaultMemorySpace = extensions->heap->getDefaultMemorySpace();
			MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
			MM_MemoryPoolLargeObjects *memoryPool = (MM_MemoryPoolLargeObjects *) tenureMemorySubspace->getMemoryPool();
			UDATA loaSize = (UDATA) (memoryPool->getLOARatio() * extensions->maxOldSpaceSize);
			loaSize = MM_Math::roundToCeiling(extensions->heapAlignment, loaSize);
			maxsize = loaSize;
		}
		break;
	case J9_GC_MANAGEMENT_POOL_TENURED :
		maxsize = extensions->maxOldSpaceSize;
		break;
	case J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE :
		{
			double tiltRatio = ((double)(extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW) - extensions->heap->getActiveSurvivorMemorySize(MEMORY_TYPE_NEW))) / (double) extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW);
			UDATA allocateSize = (UDATA)(tiltRatio * extensions->maxNewSpaceSize);
			allocateSize = MM_Math::roundToCeiling(extensions->heapAlignment, allocateSize);
			maxsize = allocateSize;
		}
		break;
	case J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR :
		{
			double tiltRatio = ((double)(extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW) - extensions->heap->getActiveSurvivorMemorySize(MEMORY_TYPE_NEW))) / (double) extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW);
			UDATA allocateSize = (UDATA)(tiltRatio * extensions->maxNewSpaceSize);
			allocateSize = MM_Math::roundToCeiling(extensions->heapAlignment, allocateSize);
			maxsize = extensions->maxNewSpaceSize - allocateSize;
		}
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_OLD :
		maxsize = extensions->memoryMax;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_EDEN :
		maxsize = extensions->tarokIdealEdenMaximumBytes;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR :
		maxsize = extensions->memoryMax;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_RESERVED :
		maxsize = extensions->memoryMax;
		break;
	case J9_GC_MANAGEMENT_POOL_JAVAHEAP :
		maxsize = extensions->memoryMax;
		break;
	default:
		break;
	}
	return maxsize;
}

/**
 * retrieve the free, total and maximum memory size for the memory pools
 * parm[in]  javaVM
 * parm[in]  poolID the memory pool ID
 * parm[out] total  total memory size
 * parm[out] free   free memory size
 * return           max memory size
 */
UDATA
j9gc_pool_memoryusage(J9JavaVM *javaVM, UDATA poolID, UDATA *free, UDATA *total)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_HeapRegionManager *manager = extensions->getHeap()->getHeapRegionManager();
	MM_HeapMemorySnapshot snapShot;
	manager->getHeapMemorySnapshot(extensions, &snapShot, FALSE);

	switch (poolID) {
	case J9_GC_MANAGEMENT_POOL_TENURED :
		*total = snapShot._totalTenuredSize;
		*free = snapShot._freeTenuredSize;
		break;
	case J9_GC_MANAGEMENT_POOL_TENURED_SOA :
		*total = snapShot._totalTenuredSOASize;
		*free = snapShot._freeTenuredSOASize;
		break;
	case J9_GC_MANAGEMENT_POOL_TENURED_LOA :
		*total = snapShot._totalTenuredLOASize;
		*free = snapShot._freeTenuredLOASize;
		break;
	case J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE :
		*total = snapShot._totalNurseryAllocateSize;
		*free = snapShot._freeNurseryAllocateSize;
		break;
	case J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR :
		*total = snapShot._totalNurserySurvivorSize;
		*free = snapShot._freeNurserySurvivorSize;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_OLD :
		*total = snapShot._totalRegionOldSize;
		*free = snapShot._freeRegionOldSize;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_EDEN :
		*total = snapShot._totalRegionEdenSize;
		*free = snapShot._freeRegionEdenSize;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR :
		*total = snapShot._totalRegionSurvivorSize;
		*free = snapShot._freeRegionSurvivorSize;
		break;
	case J9_GC_MANAGEMENT_POOL_REGION_RESERVED :
		*total = snapShot._totalRegionReservedSize;
		*free = snapShot._freeRegionReservedSize;
		break;
	case J9_GC_MANAGEMENT_POOL_JAVAHEAP :
		*total = snapShot._totalHeapSize;
		*free = snapShot._freeHeapSize;
		break;
	default :
		*total = 0;
		*free = 0;
		break;
	}
	return j9gc_pool_maxmemory(javaVM, poolID);
}

/**
 * retrieve the gc action
 */
const char *
j9gc_get_gc_action(J9JavaVM *javaVM, UDATA gcID)
{
	const char *ret = NULL;
	if (1 == j9gc_is_local_collector(javaVM, gcID)) {
		ret = "end of minor GC";
	} else {
		ret = "end of major GC";
	}
	return ret;
}

/**
 * retrieve the cause of current gc, only be called during gc-end callback
 */
const char *
j9gc_get_gc_cause(OMR_VMThread *omrVMthread)
{
	const char *ret = NULL;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMthread);
	switch (env->_cycleState->_gcCode.getCode())
	{
		case J9MMCONSTANT_IMPLICIT_GC_DEFAULT :
			ret = "collect due to normal GC activity";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE :
			ret = "second collect since first collect was insufficient";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE :
			ret = "excessive";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE :
			ret = "collect due to scavanger percolate";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES :
			ret = "collect due to scavanger percolate(unloading classes)";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE :
			ret = "collect due to aggressive scavanger percolate";
			break;
		case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC :
			ret = "Java code has requested a System.gc()";
			break;
		case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE :
			ret = "Java code has requested a non-compacting GC";
			break;
		case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY :
			ret = "a native out of memory has occurred";
			break;
		case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT :
			ret = "a dump agent has requested compaction";
			break;
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
		case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
			ret = "collect due to JVM becomes idle";
			break;
#endif
		case J9MMCONSTANT_EXPLICIT_GC_PREPARE_FOR_CHECKPOINT:
			ret = "collect due to checkpoint";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT :
			ret = "concurrent collection must be completed";
			break;
		case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS :
		default :
			ret = "unknown";
			break;
	}
	return ret;
}
/* -- new APIs for providing information about supported memorypools and garbage collectors by current jvm  -- end */

/**
 * API for the jit to call to find out the maximum allocation size, including the 
 * object header, that is guaranteed not to overflow the address range.
 */
UDATA
j9gc_get_overflow_safe_alloc_size(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->overflowSafeAllocSize;
}

/**
 * API to set a "softmx" value (generally done through j.l.management)
 * @return 0 on success, 1 otherwise.
 */
UDATA
j9gc_set_softmx(J9JavaVM *javaVM, UDATA newsoftMx)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	
	/* make sure the parameter is heap aligned */
	UDATA softMx = MM_Math::roundToFloor(extensions->heapAlignment, newsoftMx);

	if ((softMx > extensions->memoryMax) || (softMx < extensions->initialMemorySize)) {
		/* you can only set a softmx between -Xms and -Xmx */
		return 1;
	}
	
	extensions->softMx = softMx;
	return 0;
}

/**
 * API to return the current "softmx" value (generally done through j.l.management)
 * @return the current softmx - 0 if one has not been set
 */
UDATA
j9gc_get_softmx(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->softMx;
}

/**
 * API to return the initial memory size (-Xms)
 * to interested parties
 */
UDATA
j9gc_get_initial_heap_size(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->initialMemorySize;
}

/**
 * API to return the maximum heap size (-Xmx)
 * For RTJ, it also includes non-collectible heap memory
 * to interested parties
 */
UDATA
j9gc_get_maximum_heap_size(J9JavaVM *javaVM)
{
	UDATA size = MM_GCExtensions::getExtensions(javaVM)->memoryMax;
	return size;
}

/**
 * API to return a string representing the current GC mode.
 * Examples of the string returned are "optthruput", and "gencon".
 */
const char *
j9gc_get_gcmodestring(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->gcModeString;
}

/**
 * API to return the size of an object, in bytes, including the header
 * taking into account object alignment and minimum object size.
 * @param objectPtr Pointer to an object
 * @return The consumed heap size of an object, in bytes, including the header
 */ 
UDATA
j9gc_get_object_size_in_bytes(J9JavaVM *javaVM, j9object_t objectPtr)
{
	return MM_GCExtensions::getExtensions(javaVM)->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
}

/**
 * API to return the total footprint of an object, in bytes, including the header
 * taking into account object alignment and minimum object size. The total footprint
 * also includes the size of any external leaves of arraylets.
 *
 * @param objectPtr Pointer to an object
 * @return The total footprint of an object, in bytes
 */
UDATA
j9gc_get_object_total_footprint_in_bytes(J9JavaVM *javaVM, j9object_t objectPtr)
{
	return MM_GCExtensions::getExtensions(javaVM)->objectModel.getTotalFootprintInBytes(objectPtr);
}

/**
 * Called whenever the allocation threshold values or enablement state changes.
 * 
 * @parm[in] currentThread The current VM Thread
 */
void
j9gc_allocation_threshold_changed(J9VMThread *currentThread)
{
#if defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(currentThread);
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	IDATA const key = extensions->_TLHAsyncCallbackKey;
	vmFuncs->J9SignalAsyncEvent(vm, NULL, key);
	vmFuncs->J9CancelAsyncEvent(vm, currentThread, key);
	memoryManagerTLHAsyncCallbackHandler(currentThread, key, (void*)vm);
#endif /* defined(J9VM_GC_THREAD_LOCAL_HEAP) || defined(J9VM_GC_SEGREGATED_HEAP) */
}

/**
 * Set the allocation sampling interval to trigger a J9HOOK_MM_OBJECT_ALLOCATION_SAMPLING event
 * 
 * Examples:
 * 	To trigger an event whenever 4K objects have been allocated:
 *		j9gc_set_allocation_sampling_interval(vm, (UDATA)4096);
 *	To trigger an event for every object allocation:
 *		j9gc_set_allocation_sampling_interval(vm, (UDATA)0);
 *	To disable allocation sampling
 *		j9gc_set_allocation_sampling_interval(vm, UDATA_MAX);
 * The initial MM_GCExtensionsBase::objectSamplingBytesGranularity value is UDATA_MAX.
 * 
 * @parm[in] vm The J9JavaVM
 * @parm[in] samplingInterval The allocation sampling interval.
 */
void 
j9gc_set_allocation_sampling_interval(J9JavaVM *vm, UDATA samplingInterval)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vm);
	if (0 == samplingInterval) {
		/* avoid (env->_traceAllocationBytes) % 0 which could be undefined. */
		samplingInterval = 1;
	}

	if (samplingInterval != extensions->objectSamplingBytesGranularity) {
		extensions->objectSamplingBytesGranularity = samplingInterval;
		J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);
		j9gc_allocation_threshold_changed(currentThread);
	}
}

/**
 * Sets the allocation threshold (VMDESIGN 2006) to trigger a J9HOOK_MM_ALLOCATION_THRESHOLD event
 * whenever an object is allocated on the heap whose is between the lower bound and the upper bound
 * of the allocation threshold range.
 * 
 * Examples:
 * 	To trigger an event whenever an object is allocated between 1K and 4K:
 *		j9gc_set_allocation_threshold(vmThread, (UDATA)1024, (UDATA)4096);
 *	To trigger an event for all object allocates greater than 32K:
 *		j9gc_set_allocation_threshold(vmThread, (UDATA)(32*1024), UDATA_MAX);
 *	To trigger an event for all object allocates:
 *		j9gc_set_allocation_threshold(vmThread, (UDATA)0, UDATA_MAX);
 * 
 * @parm[in] vmThread The current VM Thread
 * @parm[in] low The lower bound of the allocation threshold range.
 * @parm[in] high The upper bound of the allocation threshold range.
 */
void 
j9gc_set_allocation_threshold(J9VMThread *vmThread, UDATA low, UDATA high)
{
	J9JavaVM *vm = vmThread->javaVM;
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(vm);
	Trc_MM_AllocationThreshold_setAllocationThreshold_Entry(vmThread,low,high,ext->lowAllocationThreshold,ext->highAllocationThreshold);
	
	Assert_MM_true( low <= high );
	
	ext->lowAllocationThreshold = low;
	ext->highAllocationThreshold = high;
	j9gc_allocation_threshold_changed(vmThread);

	Trc_MM_AllocationThreshold_setAllocationThreshold_Exit(vmThread);
}
/**
 * @param[in] vmThread the vmThread we are querying about
 * @return number of bytes allocated by thread.
 */
UDATA
j9gc_get_bytes_allocated_by_thread(J9VMThread *vmThread)
{
	return MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread)->_objectAllocationInterface->getAllocationStats()->bytesAllocated();
}

/**
 * @param[in] vmThread the vmThread we are querying about
 * @param[out] cumulativeValue pointer to a variable where to store cumulative number of bytes allocated by a thread since the start of VM
 * @return false if the value just rolled over or if cumulativeValue pointer is null, otherwise true
 */
BOOLEAN
j9gc_get_cumulative_bytes_allocated_by_thread(J9VMThread *vmThread, UDATA *cumulativeValue)
{
	return MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread)->_objectAllocationInterface->getAllocationStats()->bytesAllocatedCumulative(cumulativeValue);
}

/**
 * Return information about the total CPU time consumed by GC threads, as well
 * as the number of GC threads. The time for the main and worker threads is
 * reported separately, with the worker threads returned as a total.
 * 
 * @parm[in] vm The J9JavaVM
 * @parm[out] mainCpuMillis The amount of CPU time spent in the GC by the main thread, in milliseconds
 * @parm[out] workerCpuMillis The amount of CPU time spent in the GC by the all worker threads, in milliseconds
 * @parm[out] maxThreads The maximum number of GC worker threads
 * @parm[out] currentThreads The number of GC worker threads that participated in the last collection
 */
void
j9gc_get_CPU_times(J9JavaVM *javaVM, U_64 *mainCpuMillis, U_64 *workerCpuMillis, U_32 *maxThreads, U_32 *currentThreads)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	GC_VMThreadListIterator iterator(javaVM);
	U_64 mainMillis = 0;
	U_64 workerMillis = 0;
	U_64 workerNanos = 0;
	J9VMThread *vmThread = iterator.nextVMThread();
	while (NULL != vmThread) {
		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
		if (!env->isMainThread()) {
			/* For a large number of worker threads and very long runs, a sum of 
			 * nanos might overflow a U_64. Sum the millis and nanos separately.
			 */
			workerMillis += env->_workerThreadCpuTimeNanos / 1000000;
			workerNanos += env->_workerThreadCpuTimeNanos % 1000000;
		}
		vmThread = iterator.nextVMThread();
	}

	/* Adjust the total millis by the nanos, rounding up. */
	workerMillis += workerNanos / 1000000;
	if ((workerNanos % 1000000) > 500000) {
		workerMillis += 1;
	}

	/* Adjust the main millis by the nanos, rounding up. */
	mainMillis = extensions->_mainThreadCpuTimeNanos / 1000000;
	if ((extensions->_mainThreadCpuTimeNanos % 1000000) > 500000) {
		mainMillis += 1;
	}
	
	/* Store the results */
	*mainCpuMillis = mainMillis;
	*workerCpuMillis = workerMillis;
	*maxThreads = (U_32)extensions->dispatcher->threadCountMaximum();	
	*currentThreads = (U_32)extensions->dispatcher->activeThreadCount();
}

J9HookInterface**
j9gc_get_private_hook_interface(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	return extensions->getPrivateHookInterface();
}

UDATA
ownableSynchronizerObjectCreated(J9VMThread *vmThread, j9object_t object)
{
	Assert_MM_true(NULL != object);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->add(env, object);
	MM_ObjectAllocationInterface *objectAllocation = env->_objectAllocationInterface;
	if (NULL != objectAllocation) {
		objectAllocation->getAllocationStats()->_ownableSynchronizerObjectCount += 1;
	}
	return 0;
}

UDATA
continuationObjectCreated(J9VMThread *vmThread, j9object_t object)
{
	Assert_MM_true(NULL != object);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if (MM_GCExtensions::onCreated == MM_GCExtensions::getExtensions(env)->timingAddContinuationInList) {
		addContinuationObjectInList(env, object);
	}
	MM_ObjectAllocationInterface *objectAllocation = env->_objectAllocationInterface;

	if (NULL != objectAllocation) {
		objectAllocation->getAllocationStats()->_continuationObjectCount += 1;
	}
	return 0;
}

UDATA
continuationObjectStarted(J9VMThread *vmThread, j9object_t object)
{
	Assert_MM_true(NULL != object);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	if (MM_GCExtensions::onStarted == MM_GCExtensions::getExtensions(env)->timingAddContinuationInList) {
		addContinuationObjectInList(env, object);
	}
	return 0;
}

UDATA
continuationObjectFinished(J9VMThread *vmThread, j9object_t object)
{
	Assert_MM_true(NULL != object);
	return 0;
}

void
addContinuationObjectInList(MM_EnvironmentBase *env, j9object_t object)
{
	if (MM_GCExtensions::disable_continuation_list != MM_GCExtensions::getExtensions(env)->continuationListOption) {
		env->getGCEnvironment()->_continuationObjectBuffer->add(env, object);
	}
}

void
j9gc_notifyGCOfClassReplacement(J9VMThread *vmThread, J9Class *oldClass, J9Class *newClass, UDATA isFastHCR)
{
	Assert_MM_true(NULL != newClass);
	Assert_MM_true(NULL != oldClass);
	Assert_MM_true(newClass != oldClass);

	/* must be call under exclusive only */
    if (J9_ARE_ANY_BITS_SET(vmThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT)) {
        Assert_MM_true(0 != vmThread->safePointCount);
    } else {
        Assert_MM_mustHaveExclusiveVMAccess(vmThread->omrVMThread);
    }

	/* classes should not be unloaded */
	Assert_MM_true(!J9_ARE_ANY_BITS_SET(oldClass->classDepthAndFlags, J9AccClassDying));
	Assert_MM_true(!J9_ARE_ANY_BITS_SET(newClass->classDepthAndFlags, J9AccClassDying));

	/*
	 * class->gcLink slot can be not NULL in two cases:
	 *  - link of list of dying classes for unloaded classes
	 *  - have installed Remembered Set (balanced only)
	 *  Both of this cases are illegal for new class
	 */
	Assert_MM_true(NULL == newClass->gcLink);

	if (NULL != oldClass->gcLink) {
		/* Remembered Set for class is using in balanced only */
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread->javaVM);
		Assert_MM_true(extensions->isVLHGC());

		/* Remembered Set for class is using for anonymous classes only */
		Assert_MM_true(J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(oldClass), J9ClassIsAnonymous));
		Assert_MM_true(J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(newClass), J9ClassIsAnonymous));

		/*
		 * gcLink in Balanced would refer to Remembered Set information (low tagged value or pointer to malloc'ed bit array)
		 * Class replacement can not introduce new roots so Remembered Set is good enough
		 * In case of Fast HCR old class still be an actual class after replacement so RS pointer is in correct place already
		 * Otherwise RS pointer should be moved from old class to new class. An old class RS pointer can be set to NULL. There are no instances of it left.
		 */
		if (0 == isFastHCR) {
			newClass->gcLink = oldClass->gcLink;
			oldClass->gcLink = NULL;
		}
	}
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
void
j9gc_prepare_for_checkpoint(J9VMThread *vmThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_VerboseManagerBase *verboseGCManager = extensions->verboseGCManager;

	/* trigger a GC to disclaim memory */
	j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC);
	j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_PREPARE_FOR_CHECKPOINT);

	if (NULL != verboseGCManager) {
		verboseGCManager->prepareForCheckpoint(env);
	}

	/* Threads being terminated may trigger a hook that may acquire exclusive VM access via JVMTI callback */
	releaseVMAccess(vmThread);
	extensions->dispatcher->prepareForCheckpoint(env, extensions->checkpointGCthreadCount);
	acquireVMAccess(vmThread);
}

BOOLEAN
j9gc_reinitialize_for_restore(J9VMThread *vmThread, const char **nlsMsgFormat)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();
	J9JavaVM *vm = vmThread->javaVM;
	J9MemoryManagerVerboseInterface *mmFuncTable = (J9MemoryManagerVerboseInterface *)vm->memoryManagerFunctions->getVerboseGCFunctionTable(vm);

	Assert_MM_true(NULL != extensions->getGlobalCollector());
	Assert_MM_true(NULL != extensions->configuration);

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (!gcReinitializeDefaultsForRestore(vmThread)) {
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_GC_FAILED_TO_REINITIALIZE_PARSING_RESTORE_OPTIONS, NULL);
		goto _error;
	}

	extensions->configuration->reinitializeForRestore(env);

	if (!extensions->getGlobalCollector()->reinitializeForRestore(env)) {
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_GC_FAILED_TO_INSTANTIATE_GLOBAL_GARBAGE_COLLECTOR, NULL);
		goto _error;
	}

	/* The checkpoint thread must release VM access for the duration of dispatcher->reinitializeForRestore,
	 * since new GC threads could be started and the startup/attach of a new GC thread involves allocation and may trigger GC. */
	releaseVMAccess(vmThread);
	if (!extensions->dispatcher->reinitializeForRestore(env)) {
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_GC_FAILED_TO_INSTANTIATE_TASK_DISPATCHER, NULL);
		acquireVMAccess(vmThread);
		goto _error;
	}
	acquireVMAccess(vmThread);

	if (!mmFuncTable->checkOptsAndInitVerbosegclog(vm, vm->checkpointState.restoreArgsList)) {
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VERB_FAILED_TO_INITIALIZE, NULL);
		goto _error;
	}

	TRIGGER_J9HOOK_MM_OMR_REINITIALIZED( extensions->omrHookInterface, vmThread->omrVMThread, j9time_hires_clock());

	return true;

_error:
	return false;
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

/* JAZZ 90354 Temporarily move obsolete GC table exported functions, to be removed shortly. */
/* These calls remain, due to legacy symbol names - see Jazz 13097 for more information */
#if defined (OMR_GC_MODRON_CONCURRENT_MARK) || defined (J9VM_GC_VLHGC)
#include "j9comp.h"
#include "omr.h"
#include "CardTable.hpp"

UDATA
j9gc_concurrent_getCardSize(J9JavaVM *javaVM)
{
	return j9gc_incrementalUpdate_getCardSize(javaVM->omrVM);
}

UDATA
j9gc_concurrent_getHeapBase(J9JavaVM *javaVM)
{
	return j9gc_incrementalUpdate_getHeapBase(javaVM->omrVM);
}

uintptr_t
j9gc_incrementalUpdate_getCardTableVirtualStart(OMR_VM *omrVM)
{
	uintptr_t start = 0;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	/* make sure that the table is initialized */
	if (NULL != extensions->cardTable) {
		start = (uintptr_t) extensions->cardTable->getCardTableVirtualStart();
	}
	return start;
}

uintptr_t
j9gc_incrementalUpdate_getCardSize(OMR_VM *omrVM)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	/* make sure that the table is initialized */
	if (NULL != extensions->cardTable) {
		return CARD_SIZE;
	} else {
		return 0;
	}
}

uintptr_t
j9gc_incrementalUpdate_getCardTableShiftValue(OMR_VM *omrVM)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	/* make sure that the table is initialized */
	if (NULL != extensions->cardTable) {
		return CARD_SIZE_SHIFT;
	} else {
		return 0;
	}
}

/**
 * Get the base of the region of heap which is described by the card table.
 * Note that other heap sections (e.g. RTJ scopes) may be outside of this heap
 * range. Also note that there is no guarantee that the entire heap is
 * accessible. This value should ONLY be used for card table calculations.
 *
 * @return the heap address corresponding to the first card in the card table
 */
uintptr_t
j9gc_incrementalUpdate_getHeapBase(OMR_VM *omrVM)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	/* make sure that the table is initialized */
	if (NULL != extensions->cardTable) {
		return (uintptr_t) extensions->cardTable->getHeapBase();
	} else {
		return 0;
	}
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK || J9VM_GC_VLHGC */

} /* extern "C" */
