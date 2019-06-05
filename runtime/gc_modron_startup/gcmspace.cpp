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
 * @ingroup GC_Modron_Startup
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "modronopt.h"
#include "gcmspace.h"

#include <string.h>

#include "Configuration.hpp"
#if defined(J9VM_GC_REALTIME)
#include "EnvironmentRealtime.hpp"
#else
#include "EnvironmentBase.hpp"
#endif /* J9VM_GC_REALTIME */
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "ObjectAllocationInterface.hpp"

#include "mmhook_internal.h"

extern "C" {
/**
 * Shim for low level Memory Spaces allocation which only exists to create the correct fake env for internalAllocateMemorySpaceWithMaximumWithEnv.
 * Provided sizes are already adjusted (aligned etc).
 * @return the pointer to the list (pool) of memory spaces allocated
 */
MM_MemorySpace *
internalAllocateMemorySpaceWithMaximum(J9JavaVM * javaVM, UDATA minimumSpaceSize, UDATA minimumNewSpaceSize, UDATA initialNewSpaceSize, UDATA maximumNewSpaceSize, UDATA minimumTenureSpaceSize, UDATA initialTenureSpaceSize, UDATA maximumTenureSpaceSize, UDATA memoryMax, UDATA baseAddress, UDATA tenureFlags)
{
	MM_MemorySpace *result = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	if (extensions->isMetronomeGC()) {
#if defined(J9VM_GC_REALTIME)
		MM_EnvironmentRealtime env(javaVM->omrVM);
		result = internalAllocateMemorySpaceWithMaximumWithEnv(&env, javaVM, minimumSpaceSize, minimumNewSpaceSize, initialNewSpaceSize, maximumNewSpaceSize, minimumTenureSpaceSize, initialTenureSpaceSize, maximumTenureSpaceSize, memoryMax, baseAddress, tenureFlags);
#endif /* J9VM_GC_REALTIME */
	} else {
		MM_EnvironmentBase env(javaVM->omrVM);
		result = internalAllocateMemorySpaceWithMaximumWithEnv(&env, javaVM, minimumSpaceSize, minimumNewSpaceSize, initialNewSpaceSize, maximumNewSpaceSize, minimumTenureSpaceSize, initialTenureSpaceSize, maximumTenureSpaceSize, memoryMax, baseAddress, tenureFlags);
	}
	return result;
}

/**
 * Low level Memory Spaces allocation.
 * Provided sizes are already adjusted (aligned etc).
 * @return the pointer to the list (pool) of memory spaces allocated
 */
MM_MemorySpace *
internalAllocateMemorySpaceWithMaximumWithEnv(MM_EnvironmentBase *env, J9JavaVM *javaVM, UDATA minimumSpaceSize, UDATA minimumNewSpaceSize, UDATA initialNewSpaceSize, UDATA maximumNewSpaceSize, UDATA minimumTenureSpaceSize, UDATA initialTenureSpaceSize, UDATA maximumTenureSpaceSize, UDATA memoryMax, UDATA baseAddress, UDATA tenureFlags)
{
	MM_MemorySpace *memorySpace = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_Heap *heap = extensions->heap;

	/* create set of parameters and clean them */
	MM_InitializationParameters parameters;	
	MM_Configuration *configuration = extensions->configuration;

	/*
	 * This function uses goto! All function-scope variables must be declared above
	 * this comment. Failure to do so will cause compile errors on GNU and IBM compilers.
	 */
	configuration->prepareParameters(javaVM->omrVM, minimumSpaceSize, minimumNewSpaceSize, initialNewSpaceSize, maximumNewSpaceSize,
					 minimumTenureSpaceSize, initialTenureSpaceSize, maximumTenureSpaceSize,
					 memoryMax, tenureFlags, &parameters);
	
	memorySpace = configuration->createDefaultMemorySpace(env, heap, &parameters);
	
	if(NULL == memorySpace) {
		goto cleanup;
	}

	if (baseAddress) {
		if(!memorySpace->inflate(env)) {
			goto cleanup;
		}
	} else {
		/* TODO: remove this branch, once physical memory is created with new API */
		/* defer inflation if base address is fixed */
		if ((MEMORY_TYPE_FIXED & tenureFlags) != MEMORY_TYPE_FIXED) {
			if(!memorySpace->inflate(env)) {
				goto cleanup;
			}
		}
	}

	TRIGGER_J9HOOK_MM_PRIVATE_HEAP_NEW(
		MM_GCExtensions::getExtensions(javaVM)->privateHookInterface,
		env->getOmrVMThread(),
		memorySpace);

	if (NULL == heap->getDefaultMemorySpace()) {
		heap->setDefaultMemorySpace(memorySpace);
	}

	return memorySpace;

cleanup:

	/* TODO: Cleanup for the memorySpace class (if necessary) */
	return NULL;
}

} /* extern "C" */
