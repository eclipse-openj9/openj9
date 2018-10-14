
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "Tgc.hpp"
#include "mmhook.h"

#if defined(J9VM_GC_VLHGC)
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "TgcExtensions.hpp"
#include "VMThreadListIterator.hpp"

/**
 * Report NUMA statistics prior to a collection
 */
static void
tgcHookReportNumaStatistics(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent* event = (MM_GlobalGCStartEvent*)eventData;
	J9VMThread* vmThread = (J9VMThread*)event->currentThread->_language_vmthread;
	J9JavaVM *javaVM = vmThread->javaVM;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcNumaExtensions *numaExtensions = &tgcExtensions->_numa;
	
	/*
	 * Initialize the extensions if they haven't been initialized yet
	 */
	if (NULL == numaExtensions->nodeData) {
		UDATA maxNode = extensions->_numaManager.getMaximumNodeNumber();
		UDATA dataSizeInBytes = (maxNode + 1) * sizeof(TgcNumaExtensions::NumaNodeData); 
		MM_Forge *forge = extensions->getForge();
	
		numaExtensions->numaNodes = maxNode;
		numaExtensions->nodeData = (TgcNumaExtensions::NumaNodeData*)forge->allocate(dataSizeInBytes, MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	}
	
	if (NULL != numaExtensions->nodeData) {
		memset(numaExtensions->nodeData, 0, (numaExtensions->numaNodes + 1) * sizeof(TgcNumaExtensions::NumaNodeData));
		
		/*
		 * Count threads
		 */
		GC_VMThreadListIterator threadIterator(vmThread);
		J9VMThread * walkThread = NULL;
		while (NULL != (walkThread = threadIterator.nextVMThread())) {
			MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(walkThread->omrVMThread);
			
			UDATA threadNode = 0;
			UDATA nodeCount = 1;
			if ((0 != omrthread_numa_get_node_affinity(walkThread->osThread, &threadNode, &nodeCount)) || (0 == nodeCount)) {
				threadNode = 0;
			}
			numaExtensions->nodeData[threadNode].threads += 1;
			
			if ((walkThread == vmThread) || (env->getThreadType() == GC_SLAVE_THREAD)) {
				numaExtensions->nodeData[threadNode].gcThreads += 1;
			}
		}
		
		/*
		 * Count regions
		 */
		MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
		GC_HeapRegionIterator regionIterator(regionManager, MM_HeapRegionDescriptor::MANAGED);
		MM_HeapRegionDescriptor *region = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			UDATA regionNode = extensions->_numaManager.getJ9NodeNumber(region->getNumaNode());
			if (region->isCommitted()) {
				numaExtensions->nodeData[regionNode].committedRegions += 1;
			}
			if (region->getRegionType() == MM_HeapRegionDescriptor::FREE) {
				numaExtensions->nodeData[regionNode].freeRegions += 1;
			}
			numaExtensions->nodeData[regionNode].totalRegions += 1;
		}
		
		/*
		 * Report results
		 */
		for (UDATA i = 0; i <= numaExtensions->numaNodes; i++) {
			tgcExtensions->printf(
					"NUMA node %zu has %zu regions (%zu committed, %zu free) %zu threads (%zu GC threads)\n", 
					i, 
					numaExtensions->nodeData[i].totalRegions,
					numaExtensions->nodeData[i].committedRegions,
					numaExtensions->nodeData[i].freeRegions,
					numaExtensions->nodeData[i].threads,
					numaExtensions->nodeData[i].gcThreads);
		}
	}
}


/**
 * Initialize NUMA tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by NUMA tgc tracing.
 */
bool
tgcNumaInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	bool result = true;
	
	tgcExtensions->_numa.numaNodes = 0;
	tgcExtensions->_numa.nodeData = NULL;

	J9HookInterface** hooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookReportNumaStatistics, OMR_GET_CALLSITE(), NULL);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookReportNumaStatistics, OMR_GET_CALLSITE(), NULL);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_LOCAL_GC_END, tgcHookReportNumaStatistics, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* J9VM_GC_VLHGC */
