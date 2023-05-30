
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(COPYFORWARDCOMPACTGROUP_HPP_)
#define COPYFORWARDCOMPACTGROUP_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "GCExtensions.hpp"
#include "MemoryPool.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"

class MM_CopyScanCacheVLHGC;
class MM_LightweightNonReentrantLock;

/**
 * Structure used to record per-thread data for each compact group in CopyForwardScheme.
 * @ingroup GC_Modron_Standard
 */
class MM_CopyForwardCompactGroup
{
/* data members */
private:
	MM_HeapRegionManager *_regionManager;  /**< Region manager for the heap instance */
protected:
public:
	MM_CopyScanCacheVLHGC *_copyCache;  /**< The copy cache in this compact group for the owning thread */
	MM_LightweightNonReentrantLock *_copyCacheLock; /**< The lock associated with the list this copy cache belongs to */
	void *_TLHRemainderBase;  /**< base pointers of the last unused TLH copy cache, that might be reused  on next copy refresh */
	void *_TLHRemainderTop;	  /**< top pointers of the last unused TLH copy cache, that might be reused  on next copy refresh */
	void *_DFCopyBase;	/**< The base address of the inlined "copy cache" used by CopyForwardSchemeDepthFirst */
	void *_DFCopyAlloc;	/**< The alloc pointer of the inlined "copy cache" used by CopyForwardSchemeDepthFirst */
	void *_DFCopyTop;	/**< The top address of the inlined "copy cache" used by CopyForwardSchemeDepthFirst */
	UDATA _failedAllocateSize;		/**< Smallest size of allocate request that we failed in this compact group */
	
	/* statistics */
	struct MM_CopyForwardCompactGroupStats {
		UDATA _copiedObjects; /**< The number of objects copied into survivor regions for this compact group by the owning thread (i.e. objects copied TO or WITHIN this compact group) */
		UDATA _copiedBytes; /**< The number of bytes copied into survivor regions for this compact group by the owning thread */
		UDATA _liveObjects; /**< The number of live objects found in evacuate regions for this compact group by the owning thread (i.e. objects copied FROM or WITHIN this compact group) */
		UDATA _liveBytes; /**< The number of live bytes found in evacuate regions for this compact group by the owning thread */
		UDATA _scannedObjects; /**< The number of objects scanned in abort recovery/nonEvacuated regions in this compact group */
		UDATA _scannedBytes; /**< The number of objects scanned in abort recovery/nonEvacuated regions in this compact group */
	};
	
	MM_CopyForwardCompactGroupStats _edenStats; /**< Data about objects from Eden regions */
	MM_CopyForwardCompactGroupStats _nonEdenStats; /**< Data about objects from non-Eden regions */

	UDATA _failedCopiedObjects; /**< The number of objects which the owning thread failed to copy in this compact group */
	UDATA _failedCopiedBytes; /**< The number of bytes which the owning thread failed to copy in this compact group */
	UDATA _freeMemoryMeasured;	/**< The number of bytes which were wasted while this thread was working with the given _copyCache (due to alignment, etc) which is needs to save back into the pool when giving up the cache */
	UDATA _discardedBytes; /**< The number of bytes discarded in survivor regions for this compact group by the owning thread due to incompletely filled copy caches and other inefficiencies */
	UDATA _TLHRemainderCount; /**< number of TLHRemainders has been created for the compact group during the copyforward */
	U_64 _allocationAge; /**< Average allocation age for this compact group */
	
	/* Mark map rebuilding cache values for the active MM_CopyScanCacheVLHGC associated with this group */
	UDATA _markMapAtomicHeadSlotIndex;  /**< Slot index of head which requires atomic update into the mark map (if any) */
	UDATA _markMapAtomicTailSlotIndex;  /**< Slot index of tail which requires atomic update into the mark map (if any) */
	UDATA _markMapPGCSlotIndex;  /**< Cached slot index for previous (PGC) mark map */
	UDATA _markMapPGCBitMask;  /**< Cached bit map mask being build for previous (PGC) mark map */
	UDATA _markMapGMPSlotIndex;  /**< Cached slot index for next (GMP) mark map */
	UDATA _markMapGMPBitMask;  /**< Cached bit map mask being build for previous (GMP) mark map */
	
	void initialize(MM_EnvironmentVLHGC *env) {
		_copyCache = NULL;
		_copyCacheLock = NULL;
		_TLHRemainderBase = NULL;
		_TLHRemainderTop = NULL;
		_DFCopyBase = NULL;
		_DFCopyAlloc = NULL;
		_DFCopyTop = NULL;
		_failedAllocateSize = UDATA_MAX;
		_edenStats._copiedObjects = 0;
		_edenStats._copiedBytes = 0;
		_edenStats._liveObjects = 0;
		_edenStats._liveBytes = 0;
		_edenStats._scannedObjects = 0;
		_edenStats._scannedBytes = 0;
		_nonEdenStats._copiedObjects = 0;
		_nonEdenStats._copiedBytes = 0;
		_nonEdenStats._liveObjects = 0;
		_nonEdenStats._liveBytes = 0;
		_nonEdenStats._scannedObjects = 0;
		_nonEdenStats._scannedBytes = 0;
		_failedCopiedObjects = 0;
		_failedCopiedBytes = 0;
		_freeMemoryMeasured = 0;
		_discardedBytes = 0;
		_TLHRemainderCount = 0;
		_allocationAge = 0;
		_markMapAtomicHeadSlotIndex = 0;
		_markMapAtomicTailSlotIndex = 0;
		_markMapPGCSlotIndex = 0;
		_markMapPGCBitMask = 0;
		_markMapGMPSlotIndex = 0;
		_markMapGMPBitMask = 0;
		_regionManager = MM_GCExtensions::getExtensions(env)->heapRegionManager;
	}

/* function members */
private:
protected:
public:
	MMINLINE void resetTLHRemainder()
	{
		_TLHRemainderBase = NULL;
		_TLHRemainderTop = NULL;
	}

	MMINLINE void setTLHRemainder(void* base, void* top)
	{
		_TLHRemainderBase = base;
		_TLHRemainderTop = top;
		_TLHRemainderCount += 1;
	}

	MMINLINE uintptr_t getTLHRemainderSize()
	{
		return ((uintptr_t)_TLHRemainderTop - (uintptr_t)_TLHRemainderBase);
	}

	MMINLINE void discardTLHRemainder(MM_EnvironmentVLHGC* env)
	{
		if (NULL != _TLHRemainderBase) {
			discardTLHRemainder(env, _TLHRemainderBase, _TLHRemainderTop);
			resetTLHRemainder();
		} else {
			Assert_MM_true(NULL == _TLHRemainderTop);
		}
	}

	MMINLINE void discardTLHRemainder(MM_EnvironmentVLHGC* env, void* base, void* top)
	{
		/* make it a walkable hole */
		env->_cycleState->_activeSubSpace->abandonHeapChunk(base, top);
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(base);
		discardHeapChunk(base, top, region);
	}

	MMINLINE void recycleTLHRemainder(MM_EnvironmentVLHGC* env)
	{
		if (NULL != _TLHRemainderBase) {
			/* make it a walkable hole */
			env->_cycleState->_activeSubSpace->abandonHeapChunk(_TLHRemainderBase, _TLHRemainderTop);
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_regionManager->tableDescriptorForAddress(_TLHRemainderBase);
			region->getMemoryPool()->recycleHeapChunk(env, _TLHRemainderBase, _TLHRemainderTop);
			resetTLHRemainder();
		} else {
			Assert_MM_true(NULL == _TLHRemainderTop);
		}
	}

	MMINLINE void discardHeapChunk(void* base, void* top, MM_HeapRegionDescriptorVLHGC* region)
	{
		uintptr_t discardSize = ((uintptr_t)top - (uintptr_t)base);
		_discardedBytes += discardSize;
		region->getMemoryPool()->incrementDarkMatterBytesAtomic(discardSize);
	}
};

#endif /* COPYFORWARDCOMPACTGROUP_HPP_ */
