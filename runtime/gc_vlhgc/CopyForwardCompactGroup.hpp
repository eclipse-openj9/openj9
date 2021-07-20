
/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#if !defined(COPYFORWARDCOMPACTGROUP_HPP_)
#define COPYFORWARDCOMPACTGROUP_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"
#include "Math.hpp"

#include "GCExtensions.hpp"
#include "CompactGroupManager.hpp"
#include "MemoryPool.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"

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
protected:
public:
	MM_CopyScanCacheVLHGC *_copyCache;  /**< The copy cache in this compact group for the owning thread */
	MM_LightweightNonReentrantLock *_copyCacheLock; /**< The lock associated with the list this copy cache belongs to */
	void *_TLHRemainderBase;  /**< base pointers of the last unused TLH copy cache, that might be reused  on next copy refresh */
	void *_TLHRemainderTop;	  /**< top pointers of the last unused TLH copy cache, that might be reused  on next copy refresh */
	uintptr_t _gcIDForTLHRemainder;  /**< the gc cycle when preserving the TLHRemainder. */
	bool _preservedTLHRemainder;
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
	uintptr_t _preparedRemainderBytes;
	uintptr_t _preservedRemainderBytes;
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
		_preparedRemainderBytes = 0;
		_preservedRemainderBytes = 0;
		_allocationAge = 0;
		_markMapAtomicHeadSlotIndex = 0;
		_markMapAtomicTailSlotIndex = 0;
		_markMapPGCSlotIndex = 0;
		_markMapPGCBitMask = 0;
		_markMapGMPSlotIndex = 0;
		_markMapGMPBitMask = 0;
	}

/* function members */
private:
protected:
public:
	MMINLINE void setPreservedTLHRemainder()
	{
		_preservedTLHRemainder = true;
	}

	MMINLINE void resetPreservedTLHRemainder()
	{
		_preservedTLHRemainder = false;
	}

	MMINLINE bool isPreservedTLHRemainder()
	{
		return _preservedTLHRemainder;
	}

	MMINLINE void resetTLHRemainder()
	{
		_TLHRemainderBase = NULL;
		_TLHRemainderTop = NULL;
		_gcIDForTLHRemainder = 0;
		resetPreservedTLHRemainder();
	}

	MMINLINE void setTLHRemainder(void *base, void *top)
	{
		_TLHRemainderBase = base;
		_TLHRemainderTop = top;
		_TLHRemainderCount += 1;
		resetPreservedTLHRemainder();
	}

	MMINLINE void moveTLHRemainder(MM_CopyForwardCompactGroup *dest)
	{
		dest->_TLHRemainderBase = _TLHRemainderBase;
		dest->_TLHRemainderTop = _TLHRemainderTop;
		dest->_gcIDForTLHRemainder = _gcIDForTLHRemainder;
		resetTLHRemainder();
	}

	MMINLINE uintptr_t getTLHRemainderSize()
	{
		return ((uintptr_t)_TLHRemainderTop - (uintptr_t)_TLHRemainderBase);
	}

	/**
	 * @return true if TLHRemainder has been aligned.
	 * @return false if TLHRemainder is too small to use after alignment
	 */
	MMINLINE bool alignTLHRemainder(uintptr_t tlhSurvivorDiscardThreshold)
	{
		bool aligned = false;
		if (_TLHRemainderBase) {
			void *newAlignedTLHRemainderBase = (void*) MM_Math::roundToCeilingCard((uintptr_t) _TLHRemainderBase);
			void *newAlignedTLHRemainderTop =  (void*) MM_Math::roundToFloorCard((uintptr_t) _TLHRemainderTop);
			if (((uintptr_t)newAlignedTLHRemainderTop - (uintptr_t)newAlignedTLHRemainderBase) >= tlhSurvivorDiscardThreshold) {
				/* TLHRemainder need to be card aligned for being reuse cross PGCs */
				_TLHRemainderBase = newAlignedTLHRemainderBase;
				_TLHRemainderTop = newAlignedTLHRemainderTop;
				aligned = true;
			}
			/* else aligned TLHRemainder is smaller than tlhSurvivorDiscardThreshold, need to be reset */
		}
		return aligned;
	}

	/**
	 * abandonTLHRemainder
	 * make TLHRemainder walkable hole and preserve it for next PGCs
	 * count remainderBytes as darkMatterBytes in related memoryPool for avoiding to affect scheduling, estimating, projecting process before the next PGC
	 */
	MMINLINE void abandonTLHRemainder(MM_EnvironmentVLHGC* env)
	{
		if (NULL != _TLHRemainderBase) {
			Assert_MM_true(NULL != _TLHRemainderTop);
			/* make it a walkable hole */
			env->_cycleState->_activeSubSpace->abandonHeapChunk(_TLHRemainderBase, _TLHRemainderTop);
			MM_GCExtensions *extensions =MM_GCExtensions::getExtensions(env);
			/* can discard the remainder in Nursery region here, but it is also be part of collection set in next PGC, so discard it at beginning of next PGC */
			/* MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)extensions->heapRegionManager->tableDescriptorForAddress(_TLHRemainderBase);
		    if (age < extensions->tarokNurseryMaxAge._valueSpecified) {
				discardHeapChunk();
				resetTLHRemainder();
			} else {} */

			_gcIDForTLHRemainder = extensions->globalVLHGCStats.gcCount;
			_preservedRemainderBytes += getTLHRemainderSize();
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)extensions->heapRegionManager->tableDescriptorForAddress(_TLHRemainderBase);
			incrementDarkMatterBytesForRemainder(region);
		} else {
			Assert_MM_true(NULL == _TLHRemainderTop);
		}
	}

	MMINLINE void discardTLHRemainder(MM_EnvironmentVLHGC* env)
	{
		if (NULL != _TLHRemainderBase) {
			/* make it a walkable hole */
			env->_cycleState->_activeSubSpace->abandonHeapChunk(_TLHRemainderBase, _TLHRemainderTop);
			MM_GCExtensions *extensions =MM_GCExtensions::getExtensions(env);
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)extensions->heapRegionManager->tableDescriptorForAddress(_TLHRemainderBase);
			discardHeapChunk(_TLHRemainderBase, _TLHRemainderTop, region);
			resetTLHRemainder();
		} else {
			Assert_MM_true(NULL == _TLHRemainderTop);
		}
	}

	MMINLINE void recycleTLHRemainder(MM_EnvironmentVLHGC* env)
	{
		if (NULL != _TLHRemainderBase) {
			/* make it a walkable hole */
			env->_cycleState->_activeSubSpace->abandonHeapChunk(_TLHRemainderBase, _TLHRemainderTop);
			MM_GCExtensions *extensions =MM_GCExtensions::getExtensions(env);
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)extensions->heapRegionManager->tableDescriptorForAddress(_TLHRemainderBase);
			MM_MemoryPoolAddressOrderedList *pool = (MM_MemoryPoolAddressOrderedList *)region->getMemoryPool();

//			PORT_ACCESS_FROM_ENVIRONMENT(env);
//			U_64 startTime = 0;
//
//			UDATA samplingCount = MM_AtomicOperations::add(&extensions->samplingCount, 1);
//			if (0 == (samplingCount % 100)) {
//				startTime = j9time_hires_clock();
//			}

			pool->recycleHeapChunk(env, _TLHRemainderBase, _TLHRemainderTop);
//
//			if (0 != startTime) {
//				j9tty_printf(PORTLIB, "Sampling recycleTLHRemainder size=%zu, time=%zu us\n",
//						getTLHRemainderSize(), j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS));
//			}
			resetTLHRemainder();
		} else {
			Assert_MM_true(NULL == _TLHRemainderTop);
		}
	}

	MMINLINE void decrementDarkMatterBytesForRemainder(MM_HeapRegionDescriptorVLHGC *region)
	{
		region->getMemoryPool()->decrementDarkMatterBytes(getTLHRemainderSize(), true);
	}

	MMINLINE void incrementDarkMatterBytesForRemainder(MM_HeapRegionDescriptorVLHGC *region)
	{
		region->getMemoryPool()->incrementDarkMatterBytes(getTLHRemainderSize(), true);
	}


	MMINLINE void discardHeapChunk()
	{
		_discardedBytes = getTLHRemainderSize();
	}

	MMINLINE void discardHeapChunk(void *base, void *top, MM_HeapRegionDescriptorVLHGC *region)
	{
		uintptr_t discardSize = ((uintptr_t)top - (uintptr_t)base);
		_discardedBytes += discardSize;
		region->getMemoryPool()->incrementDarkMatterBytes(discardSize, true);
	}

	/**
	 * check and prepare TLHRemainder ready for using by collector
	 *  1, reset TLHRemainder if it is invalid.
	 *  2, reset TLHRemainder if it is in collection set
	 *  3, align TLHRemainder with Card
	 *  4, move TLHRemainder from compactGroup to regionCompactGroup (region age is incremented at every end of PGC or jump to tenure if the region is overflow)
	 *  if there are more than one TLHRemainder in the same regionCompactGroup, keep the largest TLHRemainder, reset others.
	 */
	MMINLINE void prepareTLHRemainder(MM_EnvironmentVLHGC* env, uintptr_t compactGroup)
	{
		if (NULL != _TLHRemainderBase) {
			MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
			bool needResetTLHRemainder = !extensions->preserveRemainders;
			MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)extensions->heapRegionManager->tableDescriptorForAddress(_TLHRemainderBase);
			if (needResetTLHRemainder || region->_markData._shouldMark || !region->containsObjects() || (_gcIDForTLHRemainder <= region->_copyForwardData._lastGcIDForReclaim)) {
				/* the region is part of collection set or has been reclaimed(swept/compacted/freed), reset TLHRemainder related with the region */
				needResetTLHRemainder = true;
			} else {
				/* align TLHRemainder with card */
				needResetTLHRemainder = !alignTLHRemainder(extensions->tlhSurvivorDiscardThreshold);
				if (!needResetTLHRemainder) {
					uintptr_t regionCompactGroup = MM_CompactGroupManager::getCompactGroupNumber(env, region);
					if (regionCompactGroup != compactGroup) {
						/* matching to region compact group, if there is already one remainder for the region compact group, pick bigger remainder */
						MM_CopyForwardCompactGroup *dest = &(env->_copyForwardCompactGroups[regionCompactGroup]);
						if (getTLHRemainderSize() > dest->getTLHRemainderSize()) {
							if (0 != dest->getTLHRemainderSize()) {

								dest->discardHeapChunk();
							}
							moveTLHRemainder(dest);
						} else {
							needResetTLHRemainder = true;
						}
					}
				}
			}

			if (needResetTLHRemainder) {
				discardHeapChunk();
				resetTLHRemainder();
			}
		}
	}
};

#endif /* COPYFORWARDCOMPACTGROUP_HPP_ */
