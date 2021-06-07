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

#include "CardTable.hpp"
#include "GCExtensionsBase.hpp"
#include "MemoryManager.hpp"
#include "HeapRegionManagerVLHGC.hpp"
#include "HeapMemorySnapshot.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIteratorVLHGC.hpp"

MM_HeapRegionManagerVLHGC::MM_HeapRegionManagerVLHGC(MM_EnvironmentBase *env, UDATA regionSize, UDATA tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
	: MM_HeapRegionManagerTarok(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor)
{
	_typeId = __FUNCTION__;
}

MM_HeapRegionManagerVLHGC *
MM_HeapRegionManagerVLHGC::newInstance(MM_EnvironmentBase *env, UDATA regionSize, UDATA tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
{
	MM_HeapRegionManagerVLHGC *regionManager = (MM_HeapRegionManagerVLHGC *)env->getForge()->allocate(sizeof(MM_HeapRegionManagerVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != regionManager) {
		new(regionManager) MM_HeapRegionManagerVLHGC(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor);
		if (!regionManager->initialize(env)) {
			regionManager->kill(env);
			regionManager = NULL;
		}
	}
	return regionManager;
}

bool 
MM_HeapRegionManagerVLHGC::initialize(MM_EnvironmentBase *env)
{
	return MM_HeapRegionManagerTarok::initialize(env);
}

void 
MM_HeapRegionManagerVLHGC::tearDown(MM_EnvironmentBase *env)
{
	MM_HeapRegionManagerTarok::tearDown(env);
}

bool
MM_HeapRegionManagerVLHGC::enableRegionsInTable(MM_EnvironmentBase *env, MM_MemoryHandle *handle)
{
	bool result = true;
	MM_GCExtensionsBase *extensions= env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;
	void *lowHeapEdge = memoryManager->getHeapBase(handle);
	void *highHeapEdge = memoryManager->getHeapTop(handle);
	
	/* ask for the number of affinity leaders since that will ensure that is how we will logically split up the regions - we only use physical details for binding */
	UDATA nodeCount = 0;
	J9MemoryNodeDetail const* affinityLeaders = extensions->_numaManager.getAffinityLeaders(&nodeCount);
	UDATA nodeToBind = 0;
	if (nodeCount > 0) {
		nodeToBind = affinityLeaders[0].j9NodeNumber;
	}

	UDATA forceNode = extensions->fvtest_tarokForceNUMANode;
	if (0 == forceNode) {
		/* force interleave */
		nodeToBind = 0;
		nodeCount = 1;
	} else if (UDATA_MAX != forceNode) {
		/* a specific node */
		nodeToBind = forceNode;
		nodeCount = 1;
	}

	MM_CardTable *cardTable = extensions->cardTable;
	Assert_MM_true(NULL != cardTable);
	bool hasPhysicalNUMASupport = extensions->_numaManager.isPhysicalNUMASupported();
	if (nodeCount > 1) {
		/* get the card table since we need to set its affinity, as well */
		UDATA heapRemaining = memoryManager->getMaximumSize(handle);
		/* because page size and region size are both powers of 2, the max is equivalent to the lowest-common-multiple */
		UDATA granularity = OMR_MAX(memoryManager->getPageSize(handle), getRegionSize());
		UDATA heapAddress = (UDATA)memoryManager->getHeapBase(handle);
		for (UDATA nextNodeIndex = 1; nextNodeIndex <= nodeCount; nextNodeIndex++) {
			UDATA nodesRemaining = nodeCount + 1 - nextNodeIndex;
			UDATA thisNodeSize = MM_Math::roundToCeiling(granularity, heapRemaining / nodesRemaining);
			if (0 < thisNodeSize) {
				void *topOfExtent = (void*)(heapAddress + thisNodeSize);
				if (topOfExtent > highHeapEdge) {
					/* adjust topOfExtent to top heap address even it would not be aligned to page size
					 * setNumaAffinity() will aligned it itself
					 */
					topOfExtent = highHeapEdge;
					/* adjust thisNodeSize as well */
					thisNodeSize = (UDATA)topOfExtent - heapAddress;
				}
				if (hasPhysicalNUMASupport) {
					result = memoryManager->setNumaAffinity(handle, nodeToBind, (void*)heapAddress, thisNodeSize);
					if (result) {
						/* also set the memory affinity for the corresponding CardTable extent */
						result = cardTable->setNumaAffinityCorrespondingToHeapRange(env, nodeToBind, (void*)heapAddress, topOfExtent);
					}
				} else {
					result = true;
				}
				if (result) {
					setNodeAndLinkRegions(env, (void*)heapAddress, topOfExtent, nodeToBind);
					heapAddress += thisNodeSize;
					heapRemaining -= thisNodeSize;
					if (nodeCount > nextNodeIndex) {
						nodeToBind = affinityLeaders[nextNodeIndex].j9NodeNumber;
					} else {
						Assert_MM_true(nextNodeIndex == nodeCount);
					}
				} else {
					break;
				}
			}
		}
	} else {
		if ((hasPhysicalNUMASupport) && (nodeToBind > 0)) {
			UDATA nodeSize = (UDATA)highHeapEdge - (UDATA)lowHeapEdge;
			result = memoryManager->setNumaAffinity(handle, nodeToBind, lowHeapEdge, nodeSize);
			if (result) {
				/* also set the memory affinity for the corresponding CardTable extent */
				result = cardTable->setNumaAffinityCorrespondingToHeapRange(env, nodeToBind, lowHeapEdge, highHeapEdge);
			}
		} else {
			result = true;
		}
		if (result) {
			setNodeAndLinkRegions(env, lowHeapEdge, highHeapEdge, nodeToBind);
		}
	}

	return result;
}

MM_HeapMemorySnapshot*
MM_HeapRegionManagerVLHGC::getHeapMemorySnapshot(MM_GCExtensionsBase *extensions, MM_HeapMemorySnapshot* snapshot, bool gcEnd)
{
	MM_Heap *heap = extensions->getHeap();
	snapshot->_totalHeapSize = heap->getActiveMemorySize();
	snapshot->_freeHeapSize = heap->getApproximateFreeMemorySize();

	MM_IncrementalGenerationalGC *collector = (MM_IncrementalGenerationalGC*)extensions->getGlobalCollector();
	snapshot->_totalRegionEdenSize = collector->getCurrentEdenSizeInBytes(NULL);
	snapshot->_freeRegionEdenSize = 0;
	snapshot->_totalRegionOldSize = 0;
	snapshot->_freeRegionOldSize = 0;
	snapshot->_totalRegionSurvivorSize = 0;
	snapshot->_freeRegionSurvivorSize = 0;

	UDATA regionSize = getRegionSize();
	GC_HeapRegionIteratorVLHGC regionIterator(this);

	MM_HeapRegionDescriptorVLHGC *region = NULL;
	UDATA age = 0;
	UDATA free = 0;
	UDATA allocateEdenTotal = 0;


	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->isFreeOrIdle()) {
			snapshot->_totalRegionReservedSize += regionSize;
		} else {
			if (region->containsObjects()) {
				MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
				Assert_MM_true(NULL != memoryPool);
				free = memoryPool->getAllocatableBytes();
			} else {
				Assert_MM_true(region->isArrayletLeaf());
				free = 0;
			}
			age = region->getLogicalAge();
			/* increment logic age for regions happens after getHeapMemorySnapshot(),
			 * which is triggered by gcEnd event, so at gcEnd, counts any of regions with logic age = 0 as survivor regions.
			 * TODO: increment logic age for regions should be called before gcEnd */
			if ((0 == age) && !gcEnd) {
				allocateEdenTotal += regionSize;
				snapshot->_freeRegionEdenSize += free;
			} else if (extensions->tarokRegionMaxAge == age) {
				snapshot->_totalRegionOldSize += regionSize;
				snapshot->_freeRegionOldSize += free;

			} else {
				snapshot->_totalRegionSurvivorSize += regionSize;
				snapshot->_freeRegionSurvivorSize += free;
			}
		}

	}

	/* there would be a case that mutators use more than assigned regions, correct totalRegionEdenSize to avoid inconsistent Exception */
	if (allocateEdenTotal > snapshot->_totalRegionEdenSize) {
		snapshot->_totalRegionEdenSize = allocateEdenTotal;
	}

	snapshot->_totalRegionReservedSize -= (snapshot->_totalRegionEdenSize - allocateEdenTotal);
	snapshot->_freeRegionEdenSize += (snapshot->_totalRegionEdenSize - allocateEdenTotal);
	snapshot->_freeRegionReservedSize = snapshot->_totalRegionReservedSize;
	return snapshot;
}
