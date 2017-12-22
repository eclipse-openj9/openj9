
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

#include "GCExtensions.hpp"

#include "hookable_api.h"
#include "j9cfg.h"
#include "j2sever.h"
#include "j9memcategories.h"
#include "j9nongenerated.h"
#include "j9port.h"
#include "util_api.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectModel.hpp"
#include "ReferenceChainWalkerMarkMap.hpp"
#include "SublistPool.hpp"
#include "Wildcard.hpp"

MM_GCExtensions *
MM_GCExtensions::newInstance(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensions *extensions;
	
	/* Avoid using MM_Forge to allocate memory for the extension, since MM_Forge itself has not been created! */
	extensions = static_cast<MM_GCExtensions*>(j9mem_allocate_memory(sizeof(MM_GCExtensions), OMRMEM_CATEGORY_MM));
	if (extensions) {
		/* Initialize all the fields to zero */
		memset((void *)extensions, 0, sizeof(*extensions));

		new(extensions) MM_GCExtensions();
		if (!extensions->initialize(env)) {
			extensions->kill(env);
			return NULL;
		}
	}
	return extensions;
}

void
MM_GCExtensions::kill(MM_EnvironmentBase *env)
{
	/* Avoid using MM_Forge to free memory for the extension, since MM_Forge was not used to allocate the memory */
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	tearDown(env);
	j9mem_free_memory(this);
}

/**
 * Initialize the global GC extensions structure.
 * Clear all values within the extensions structure and call the appropriate initialization routines
 * on all substructures.  Upon completion of this call, the extensions structure is ready for use.
 *
 * @return true if the initialization was successful, false otherwise.
 */
bool
MM_GCExtensions::initialize(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	if (!MM_GCExtensionsBase::initialize(env)) {
		goto failed;
	}

#if defined(OMR_ENV_DATA64)
	if (J2SE_VERSION((J9JavaVM *)getOmrVM()->_language_vm) >= J2SE_19) {
		uint64_t physicalMemory = 0;
		uint64_t memoryLimit = 0;
		uint64_t usableMemory = 0;
		/* Initial physicalMemory as per system call. */
		physicalMemory = j9sysinfo_get_physical_memory();
		if (OMRPORT_LIMIT_LIMITED == j9sysinfo_get_limit(OMRPORT_RESOURCE_ADDRESS_SPACE, &memoryLimit)) {
			/* there is a limit on the memory we can use so take the minimum of this usable amount and the physical memory */
			usableMemory = OMR_MIN(memoryLimit, physicalMemory);
		} else {
			/* if there is no memory limit being imposed on us, we will use physical memory as our max */
			usableMemory = physicalMemory;
		}

		/* extend java default max memory to 25% of physical RAM */
		memoryMax = OMR_MAX(memoryMax, usableMemory / 4);
		/* limit maxheapsize up to MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_3BIT_SHIFT_COMPRESSEDREFS, then can set 3bit compressedrefs as the default */
		memoryMax = OMR_MIN(memoryMax, MAXIMUM_HEAP_SIZE_RECOMMENDED_FOR_3BIT_SHIFT_COMPRESSEDREFS);

		/* Initialize Xmx, Xmdx */
		memoryMax = MM_Math::roundToFloor(heapAlignment, (uintptr_t)memoryMax);
		maxSizeDefaultMemorySpace = memoryMax;
	}
#endif /* OMR_ENV_DATA64 */

#if defined(J9VM_GC_REALTIME)
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
	/* only ref slots, size in bytes: 2 * minObjectSize - header size */
	minArraySizeToSetAsScanned = 2 * (1 << J9VMGC_SIZECLASSES_LOG_SMALLEST) - sizeof(J9IndexableObjectContiguous);
#else /* J9VM_GC_HYBRID_ARRAYLETS */
	/* only ref slots, size in bytes: 2 * minObjectSize - header size) - 1 * sizeof(arraylet pointer) */
	minArraySizeToSetAsScanned = 2 * (1 << J9VMGC_SIZECLASSES_LOG_SMALLEST) - sizeof(J9IndexableObjectDiscontiguous) - sizeof(fj9object_t*);
#endif /* J9VM_GC_HYBRID_ARRAYLETS */

	getJavaVM()->gcCycleOn = 0;
	if (omrthread_monitor_init_with_name(&getJavaVM()->gcCycleOnMonitor, 0, "gcCycleOn")) {
		goto failed;
	}
#endif /* J9VM_GC_REALTIME */

#if defined(J9VM_GC_JNI_ARRAY_CACHE)
	getJavaVM()->jniArrayCacheMaxSize = J9_GC_JNI_ARRAY_CACHE_SIZE;
#endif /* J9VM_GC_JNI_ARRAY_CACHE */

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	getJavaVM()->gcInfo.tlhThreshold = J9_GC_TLH_THRESHOLD;
	getJavaVM()->gcInfo.tlhSize = J9_GC_TLH_SIZE;
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

	/* if tuned for virtualized environment, we compromise a bit of performance for lower footprint */
	if (getJavaVM()->runtimeFlags & J9_RUNTIME_TUNE_VIRTUALIZED) {
		heapFreeMinimumRatioMultiplier = 20;
	}

	padToPageSize = J9_ARE_ALL_BITS_SET(getJavaVM()->runtimeFlags, J9_RUNTIME_AGGRESSIVE);

	if (J9HookInitializeInterface(getHookInterface(), OMRPORT_FROM_J9PORT(PORTLIB), sizeof(hookInterface))) {
		goto failed;
	}
	
	initializeReferenceArrayCopyTable(&referenceArrayCopyTable);
	
	{
		J9InternalVMFunctions const * const vmFuncs = getJavaVM()->internalVMFunctions;
		_asyncCallbackKey = vmFuncs->J9RegisterAsyncEvent(getJavaVM(), memoryManagerAsyncCallbackHandler, getJavaVM());
		_TLHAsyncCallbackKey = vmFuncs->J9RegisterAsyncEvent(getJavaVM(), memoryManagerTLHAsyncCallbackHandler, getJavaVM());
		if ((_asyncCallbackKey < 0) || (_TLHAsyncCallbackKey < 0)) {
			goto failed;
		}
	}

#if defined(J9VM_GC_IDLE_HEAP_MANAGER)
	/* absorbs GC specific idle tuning flags */
	if (J9_IDLE_TUNING_GC_ON_IDLE == (getJavaVM()->vmRuntimeStateListener.idleTuningFlags & J9_IDLE_TUNING_GC_ON_IDLE)) {
		gcOnIdle = true;
	}
	if (J9_IDLE_TUNING_COMPACT_ON_IDLE == (getJavaVM()->vmRuntimeStateListener.idleTuningFlags & J9_IDLE_TUNING_COMPACT_ON_IDLE)) {
		compactOnIdle = true;
	}
	idleMinimumFree = getJavaVM()->vmRuntimeStateListener.idleMinFreeHeap;
#endif /* if defined(J9VM_GC_IDLE_HEAP_MANAGER) */

	return true;

failed:
	tearDown(env);
	return false;
}

/**
 * Tear down the global GC extensions structure and all sub structures.
 */
void
MM_GCExtensions::tearDown(MM_EnvironmentBase *env)
{
	J9InternalVMFunctions const * const vmFuncs = getJavaVM()->internalVMFunctions;
	vmFuncs->J9UnregisterAsyncEvent(getJavaVM(), _TLHAsyncCallbackKey);
	_TLHAsyncCallbackKey = -1;
	vmFuncs->J9UnregisterAsyncEvent(getJavaVM(), _asyncCallbackKey);
	_asyncCallbackKey = -1;

#if defined(J9VM_GC_REALTIME)
	if (getJavaVM()->gcCycleOnMonitor) {
		omrthread_monitor_destroy(getJavaVM()->gcCycleOnMonitor);
		getJavaVM()->gcCycleOnMonitor = (omrthread_monitor_t) NULL;
	}
#endif

#if defined(J9VM_GC_MODRON_TRACE) && !defined(J9VM_GC_REALTIME)
	tgcTearDownExtensions(getJavaVM());
#endif /* J9VM_GC_MODRON_TRACE && !defined(J9VM_GC_REALTIME) */

	MM_Wildcard *wildcard = numaCommonThreadClassNamePatterns;
	while (NULL != wildcard) {
		MM_Wildcard *nextWildcard = wildcard->_next;
		wildcard->kill(this);
		wildcard = nextWildcard;
	}
	numaCommonThreadClassNamePatterns = NULL;
	
	J9HookInterface** tmpHookInterface = getHookInterface();
	if((NULL != tmpHookInterface) && (NULL != *tmpHookInterface)){
		(*tmpHookInterface)->J9HookShutdownInterface(tmpHookInterface);
		*tmpHookInterface = NULL; /* avoid issues with double teardowns */
	}

	MM_GCExtensionsBase::tearDown(env);
}

void
MM_GCExtensions::identityHashDataAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace* subspace, UDATA size, void* lowAddress, void* highAddress)
{
	J9IdentityHashData* hashData = getJavaVM()->identityHashData;
	if (J9_IDENTITY_HASH_SALT_POLICY_STANDARD == hashData->hashSaltPolicy) {
		if (MEMORY_TYPE_NEW == (subspace->getTypeFlags() & MEMORY_TYPE_NEW)) {
			if ((UDATA)lowAddress < hashData->hashData1) {
				hashData->hashData1 = (UDATA)lowAddress;
			}
			if ((UDATA)highAddress > hashData->hashData2) {
				hashData->hashData2 = (UDATA)highAddress;
			}
		}
	}
}

void
MM_GCExtensions::identityHashDataRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace* subspace, UDATA size, void* lowAddress, void* highAddress)
{
	J9IdentityHashData* hashData = getJavaVM()->identityHashData;
	if (J9_IDENTITY_HASH_SALT_POLICY_STANDARD == hashData->hashSaltPolicy) {
		if (MEMORY_TYPE_NEW == (subspace->getTypeFlags() & MEMORY_TYPE_NEW)) {
			if ((UDATA)lowAddress > hashData->hashData1) {
				hashData->hashData1 = (UDATA)lowAddress;
			}
			if ((UDATA)highAddress < hashData->hashData2) {
				hashData->hashData2 = (UDATA)highAddress;
			}
		}
	}
}

void
MM_GCExtensions::updateIdentityHashDataForSaltIndex(UDATA index)
{
	getJavaVM()->identityHashData->hashSaltTable[index] = (U_32)convertValueToHash(getJavaVM(), getJavaVM()->identityHashData->hashSaltTable[index]);
}
