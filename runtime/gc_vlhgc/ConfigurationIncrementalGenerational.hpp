
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

#if !defined(CONFIGURATIONTAROK_HPP_)
#define CONFIGURATIONTAROK_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Configuration.hpp"

class MM_GlobalCollector;
class MM_Heap;
class MM_HeapRegionManager;
class MM_MemoryPool;
class MM_MemorySubSpaceTarok;


class MM_ConfigurationIncrementalGenerational : public MM_Configuration
{
/* Data members / Types */
public:
protected:
private:
	static const uintptr_t _tarokMinimumRegionSizeInBytes = (512 * 1024);

/* Methods */
public:
	static MM_Configuration *newInstance(MM_EnvironmentBase *env);

	virtual MM_GlobalCollector *createGlobalCollector(MM_EnvironmentBase *env);
	virtual MM_Heap *createHeapWithManager(MM_EnvironmentBase *env, UDATA heapBytesRequested, MM_HeapRegionManager *regionManager);
	virtual MM_HeapRegionManager *createHeapRegionManager(MM_EnvironmentBase *env);
	virtual J9Pool *createEnvironmentPool(MM_EnvironmentBase *env);
	virtual MM_MemorySpace *createDefaultMemorySpace(MM_EnvironmentBase *env, MM_Heap *heap, MM_InitializationParameters *parameters);
	virtual void cleanUpClassLoader(MM_EnvironmentBase *env, J9ClassLoader* classLoader);
	virtual void prepareParameters(OMR_VM *omrVM, UDATA minimumSpaceSize, UDATA minimumNewSpaceSize, UDATA initialNewSpaceSize,
						    UDATA maximumNewSpaceSize, UDATA minimumTenureSpaceSize, UDATA initialTenureSpaceSize, UDATA maximumTenureSpaceSize,
						    UDATA memoryMax, UDATA tenureFlags, MM_InitializationParameters *parameters);

	/**
	 * Constructor sets default arraylet leaf size to 0. This will be updated to match effective region size
	 * after the effective region size has been established in MM_GCExtensionsBase::regionSize.
	 */
	MM_ConfigurationIncrementalGenerational(MM_EnvironmentBase *env)
#if defined(J9VM_GC_COMBINATION_SPEC)
		: MM_Configuration(env, gc_policy_balanced, mm_regionAlignment, calculateDefaultRegionSize(env), 0, gc_modron_wrtbar_cardmark_incremental, gc_modron_allocation_type_tlh)
#else
		: MM_Configuration(env, gc_policy_balanced, mm_regionAlignment, calculateDefaultRegionSize(env), 0, gc_modron_wrtbar_cardmark, gc_modron_allocation_type_tlh)
#endif /* J9VM_GC_COMBINATION_SPEC */
	{
		_typeId = __FUNCTION__;
	}

	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace);

protected:
	virtual MM_EnvironmentBase *allocateNewEnvironment(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread);
	virtual bool initializeEnvironment(MM_EnvironmentBase *env);
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * Once the region size is calculated each configuration needs to verify that
	 * is is valid.
	 *
	 * @param env[in] - the current environment
	 * @param regionSize[in] - the current regionSize to verify
	 * @return valid - is the regionSize valid
	 */
	virtual bool verifyRegionSize(MM_EnvironmentBase *env, UDATA regionSize);
	
	/**
	 * Initializes the NUMAManager.
	 *
	 * lpnguyen TODO: move this out of configuration as we should not have to "configure" NUMA.  The only reason this is here is
	 * because ConfigurationIncrementalGenerational.hpp will disable physical NUMA if it would create too many ACs and ideal AC calculation
	 * requires configuration to be done (regionSize set up).
	 *
	 * @param env[in] - the current environment
	 */
	virtual bool initializeNUMAManager(MM_EnvironmentBase *env);

private:
	static UDATA
	calculateDefaultRegionSize(MM_EnvironmentBase *env)
	{
		UDATA regionSize = 0;

		MM_GCExtensionsBase *extensions = env->getExtensions();
		UDATA regionCount = extensions->memoryMax / _tarokMinimumRegionSizeInBytes;
		/* try to select region size such that the resulting region count is in the range of [1024, 2048] */
		if (regionCount < 1024 || regionCount > 2048) {
			regionSize = OMR_MAX(extensions->memoryMax / 1024, _tarokMinimumRegionSizeInBytes);
		} else {
			regionSize = _tarokMinimumRegionSizeInBytes;
		}

		return regionSize;
	}
};

#endif /* CONFIGURATIONTAROK_HPP_ */
