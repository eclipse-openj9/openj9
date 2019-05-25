
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


#if !defined(GLOBALALLOCATIONMANAGERTAROK_HPP_)
#define GLOBALALLOCATIONMANAGERTAROK_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "modronopt.h"

#include "GlobalAllocationManager.hpp"

#include "AllocationContextBalanced.hpp"
#include "AllocationContextTarok.hpp"
#include "GCExtensions.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemoryPool.hpp"
#include "RuntimeExecManager.hpp"

class MM_AllocationContextBalanced;
class MM_EnvironmentBase;
class MM_HeapRegionDescriptorVLHGC;
class MM_MemorySubSpaceTarok;
class MM_RegionListTarok;

class MM_GlobalAllocationManagerTarok : public MM_GlobalAllocationManager
{
/* Data members & types */
public:
protected:
private:
	MM_AllocationContextBalanced **_perNodeContextSets; /**< an array which is extensions->numaNodes elements long, containing the "first" AllocationContextVLHGC in each corresponding per-node circular list */

	MM_RuntimeExecManager _runtimeExecManager; /**< A helper object used to intercept Runtime.exec() and manage affinity to workaround limitations on Linux */


	friend class MM_RuntimeExecManager;
	
/* Methods */
public:
	static MM_GlobalAllocationManagerTarok *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	virtual bool acquireAllocationContext(MM_EnvironmentBase *env);
	virtual void releaseAllocationContext(MM_EnvironmentBase *env);
	virtual void printAllocationContextStats(MM_EnvironmentBase *env, UDATA eventNum, J9HookInterface** hookInterface);
	/**
	 * Called after the receiver and the subspace have been initialized to request that the receiver build the allocation contexts
	 * it will manager over the course of the run.  The subspace is required because the allocation contexts need to know which
	 * subspace logically "owns" its memory.
	 * @param env[in] The current thread (this is the thread bootstrapping the VM)
	 * @param subspace[in] The subspace which logically owns the memory managed by the contexts the receiver is called to create
	 * @return Whether or not all the contexts were successfully initialized
	 */
	bool initializeAllocationContexts(MM_EnvironmentBase *env, MM_MemorySubSpaceTarok *subspace);
	
	/**
	 * @return The actual free memory size of all the regions managed by the contexts under the receiver
	 */
	UDATA getActualFreeMemorySize();
	/**
	 * @return The approximate free memory size of all the regions managed by the contexts under the receiver
	 */
	UDATA getApproximateFreeMemorySize();
	
	/**
	 * Return the number of free (empty) regions managed by the contexts under the receiver.
	 * 
	 * @return The count of free regions managed by the contexts under the receiver.
	 */
	UDATA getFreeRegionCount();
	
	/**
	 * Forwarded to the all the MemoryPoolBumpPointer instances managed by the contexts under the receiver
	 */
	void resetLargestFreeEntry();
	/**
	 * Expands into the given region by finding an AllocationContextTarok to manage it.  Once this method returns, the given region will
	 * be managed by a context.
	 * @param env[in] The thread performing the expansion
	 * @param region[in] The region which was just expanded
	 */
	void expand(MM_EnvironmentBase *env, MM_HeapRegionDescriptorVLHGC *region);
	/**
	 * @return The largest free entry of all the regions managed by the contexts under the receiver
	 */
	UDATA getLargestFreeEntry();

	/**
	 * This helper merely forwards the resetHeapStatistics call on to all the AllocationContextTarok instances owned by the receiver.
	 * @param globalCollect[in] True if this was a global collect (blindly passed through)
	 */
	void resetHeapStatistics(bool globalCollect);

	/**
	 * This helper merely forwards the mergeHeapStats call on to all the AllocationContextTarok instances owned by the receiver.
	 * @param heapStats[in/out] The stats structure to receive the merged data (blindly passed through)
	 * @param includeMemoryType[in] The memory space type to use in the merge (blindly passed through)
	 */
	void mergeHeapStats(MM_HeapStats *heapStats, UDATA includeMemoryType);

	/**
	 * Get the total number of allocation contexts including the non-managed ones if any.
	 * @return the total allocation context count.
	 */
	MMINLINE UDATA getTotalAllocationContextCount()
	{
		UDATA totalCount = _managedAllocationContextCount;
		return totalCount;
	}
	
	/**
	 * Get the total number of managed allocation contexts, not including the non-managed ones if any.
	 * @return the managed allocation context count.
	 */
	MMINLINE UDATA getManagedAllocationContextCount()
	{
		return _managedAllocationContextCount;
	}

	/**
	 * @param extensions[in] The global GC extensions
	 * @return The ideal number of total allocation contexts (managed + non-managed) with which the receiver assumes it is working
	 */
	static UDATA calculateIdealTotalContextCount(MM_GCExtensions *extensions);

	/**
	 * @param extensions[in] The global GC extensions
	 * @return The ideal number of managed allocation contexts with which the receiver assumes it is working
	 */
	static UDATA calculateIdealManagedContextCount(MM_GCExtensionsBase *extensions);

	virtual MM_AllocationContextTarok *getAllocationContextByIndex(UDATA index) { 

		return (MM_AllocationContextTarok *)MM_GlobalAllocationManager::getAllocationContextByIndex(index);
	}



	/**
	 * Find the allocation context for the specified NUMA node.
	 * @param numaNode the NUMA node to search for
	 * @return the associated context
	 */
	MM_AllocationContextBalanced *getAllocationContextForNumaNode(UDATA numaNode);
	

protected:
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	MM_GlobalAllocationManagerTarok(MM_EnvironmentBase *env)
		: MM_GlobalAllocationManager(env)
		, _perNodeContextSets(NULL)
		, _runtimeExecManager(env)
	{
		_typeId = __FUNCTION__;
	};
private:
	
	/**
	 * Determine if the current thread should be a common thread (non-affinitized) .
	 * 
	 * @param env[in] the current thread
	 * @return true if the thread should be treated as common, false otherwise
	 */
	bool shouldIdentifyThreadAsCommon(MM_EnvironmentBase *env);

};

#endif /* GLOBALALLOCATIONMANAGERTAROK_HPP_ */
