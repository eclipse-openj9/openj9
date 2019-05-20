
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

#include "j9.h"
#include "j9cfg.h"

#include "SweepHeapSectioningVLHGC.hpp"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySubSpace.hpp"

/**
 * Return the expected total sweep chunks that will be used in the system.
 * Called during initialization, this routine looks at the maximum size of the heap and expected
 * configuration (generations, regions, etc) and determines the approximate maximum number of chunks
 * that will be required for a sweep at any given time.  It is safe to underestimate the number of chunks,
 * as the sweep sectioning mechanism will compensate, but the the expectation is that by having all
 * chunk memory allocated in one go will keep the data localized and fragment system memory less.
 * @return estimated upper bound number of chunks that will be required by the system.
 */
UDATA
MM_SweepHeapSectioningVLHGC::estimateTotalChunkCount(MM_EnvironmentBase *env)
{
	UDATA totalChunkCountEstimate;

	if(0 == _extensions->parSweepChunkSize) {
		/* -Xgc:sweepchunksize= has NOT been specified, so we set it heuristically.
		 *
		 *                  maxheapsize
		 * chunksize =   ----------------   (rounded up to the nearest 256k)
		 *               threadcount * 32
		 */
		_extensions->parSweepChunkSize = MM_Math::roundToCeiling(256*1024, _extensions->heap->getMaximumMemorySize() / (_extensions->dispatcher->threadCountMaximum() * 32));
	}

	totalChunkCountEstimate = MM_Math::roundToCeiling(_extensions->parSweepChunkSize, _extensions->heap->getMaximumMemorySize()) / _extensions->parSweepChunkSize;

	return totalChunkCountEstimate;
}

/**
 * Walk all segments and calculate the maximum number of chunks needed to represent the current heap.
 * The chunk calculation is done on a per segment basis (no segment can represent memory from more than 1 chunk),
 * and partial sized chunks (ie: less than the chunk size) are reserved for any remaining space at the end of a
 * segment.
 * @return number of chunks required to represent the current heap memory.
 */
UDATA
MM_SweepHeapSectioningVLHGC::calculateActualChunkNumbers() const
{
	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	UDATA regionCount = 0;
	while(regionIterator.nextRegion() != NULL) {
		regionCount += 1;
	}
	
	UDATA regionSize = regionManager->getRegionSize();
	UDATA sweepChunkSize = _extensions->parSweepChunkSize;
	UDATA totalChunkCount = regionCount * (MM_Math::roundToCeiling(sweepChunkSize, regionSize) / sweepChunkSize);

	return totalChunkCount;
}

/**
 * Reset and reassign each chunk to a range of heap memory.
 * Given the current updated listed of chunks and the corresponding heap memory, walk the chunk
 * list reassigning each chunk to an appropriate range of memory.  This will clear each chunk
 * structure and then assign its basic values that connect it to a range of memory (base/top,
 * pool, segment, etc).
 * @return the total number of chunks in the system.
 */
UDATA
MM_SweepHeapSectioningVLHGC::reassignChunks(MM_EnvironmentBase *env)
{
	MM_ParallelSweepChunk *chunk; /* Sweep table chunk (global) */
	MM_ParallelSweepChunk *previousChunk;
	UDATA totalChunkCount;  /* Total chunks in system */

	MM_SweepHeapSectioningIterator sectioningIterator(this);

	totalChunkCount = 0;
	previousChunk = NULL;

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorVLHGC regionIterator(regionManager);
	MM_HeapRegionDescriptorVLHGC *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (!region->_sweepData._alreadySwept && region->hasValidMarkMap()) {
			MM_MemoryPoolBumpPointer *regionPool = (MM_MemoryPoolBumpPointer *)region->getMemoryPool();
			Assert_MM_true(NULL != regionPool);
			/* TODO:  this must be rethought for Tarok since it treats all regions identically but some might require different sweep logic */
			UDATA *heapChunkBase = (UDATA *)region->getLowAddress();  /* Heap chunk base pointer */
			UDATA *regionHighAddress = (UDATA *)region->getHighAddress();

			while (heapChunkBase < regionHighAddress) {
				void *poolHighAddr;
				UDATA *heapChunkTop;
				MM_MemoryPool *pool;

				chunk = sectioningIterator.nextChunk();
				Assert_MM_true(chunk != NULL);  /* Should never return NULL */
				totalChunkCount += 1;

				/* Clear all data in the chunk (including sweep implementation specific information) */
				chunk->clear();

				if(((UDATA)regionHighAddress - (UDATA)heapChunkBase) < _extensions->parSweepChunkSize) {
					/* corner case - we will wrap our address range */
					heapChunkTop = regionHighAddress;
				} else {
					/* normal case - just increment by the chunk size */
					heapChunkTop = (UDATA *)((UDATA)heapChunkBase + _extensions->parSweepChunkSize);
				}

				/* Find out if the range of memory we are considering spans 2 different pools.  If it does,
				 * the current chunk can only be attributed to one, so we limit the upper range of the chunk
				 * to the first pool and will continue the assignment at the upper address range.
				 */
				pool = region->getSubSpace()->getMemoryPool(env, heapChunkBase, heapChunkTop, poolHighAddr);
				if (NULL == poolHighAddr) {
					heapChunkTop = (heapChunkTop > regionHighAddress ? regionHighAddress : heapChunkTop);
				} else {
					/* Yes ..so adjust chunk boundaries */
					assume0(poolHighAddr > heapChunkBase && poolHighAddr < heapChunkTop);
					heapChunkTop = (UDATA *) poolHighAddr;
				}

				/* All values for the chunk have been calculated - assign them */
				chunk->chunkBase = (void *)heapChunkBase;
				chunk->chunkTop = (void *)heapChunkTop;
				chunk->memoryPool = pool;
				chunk->_coalesceCandidate = (heapChunkBase != region->getLowAddress());
				chunk->_previous= previousChunk;
				if(NULL != previousChunk) {
					previousChunk->_next = chunk;
				}

				/* Move to the next chunk */
				heapChunkBase = heapChunkTop;

				/* and remember address of previous chunk */
				previousChunk = chunk;

				assume0((UDATA)heapChunkBase == MM_Math::roundToCeiling(_extensions->heapAlignment,(UDATA)heapChunkBase));
			}
		}
	}

	if(NULL != previousChunk) {
		previousChunk->_next = NULL;
	}

	return totalChunkCount;
}

/**
 * Allocate and initialize a new instance of the receiver.
 * @return pointer to the new instance.
 */
MM_SweepHeapSectioningVLHGC *
MM_SweepHeapSectioningVLHGC::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepHeapSectioningVLHGC *sweepHeapSectioning = (MM_SweepHeapSectioningVLHGC *)env->getForge()->allocate(sizeof(MM_SweepHeapSectioningVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (sweepHeapSectioning) {
		new(sweepHeapSectioning) MM_SweepHeapSectioningVLHGC(env);
		if (!sweepHeapSectioning->initialize(env)) {
			sweepHeapSectioning->kill(env);
			return NULL;
		}
	}
	return sweepHeapSectioning;
}
