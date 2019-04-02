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

#if !defined(CONFIGURATIONREALTIME_HPP_)
#define CONFIGURATIONREALTIME_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "Configuration.hpp"
#include "EnvironmentRealtime.hpp"

class MM_GlobalCollector;
class MM_Heap;
class MM_GlobalAllocationManagerSegregated;

class MM_ConfigurationRealtime : public MM_Configuration
{
/* Data members / Types */
public:

protected:
	static const uintptr_t REALTIME_REGION_SIZE_BYTES = (64 * 1024);
	static const uintptr_t REALTIME_ARRAYLET_LEAF_SIZE_BYTES = OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES;

private:
	
/* Methods */
public:
	virtual MM_GlobalCollector *createGlobalCollector(MM_EnvironmentBase *env);
	virtual MM_Heap *createHeapWithManager(MM_EnvironmentBase *env, uintptr_t heapBytesRequested, MM_HeapRegionManager *regionManager);
	virtual MM_HeapRegionManager *createHeapRegionManager(MM_EnvironmentBase *env);
	virtual MM_MemorySpace *createDefaultMemorySpace(MM_EnvironmentBase *env, MM_Heap *heap, MM_InitializationParameters *parameters);
	virtual J9Pool *createEnvironmentPool(MM_EnvironmentBase *env);
	virtual MM_Dispatcher *createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);
	
	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace);
	
	MM_ConfigurationRealtime(MM_EnvironmentBase *env)
		: MM_Configuration(env, gc_policy_metronome, mm_regionAlignment, REALTIME_REGION_SIZE_BYTES, REALTIME_ARRAYLET_LEAF_SIZE_BYTES, gc_modron_wrtbar_satb, gc_modron_allocation_type_segregated)
	{
		_typeId = __FUNCTION__;
	};

	static MM_Configuration *newInstance(MM_EnvironmentBase *env);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	virtual MM_EnvironmentBase *allocateNewEnvironment(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread);
	virtual bool initializeEnvironment(MM_EnvironmentBase *env);

private:
};


#endif /* CONFIGURATIONREALTIME_HPP_ */
