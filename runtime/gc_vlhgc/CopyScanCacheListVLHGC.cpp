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
#include "j9port.h"

#include "CopyScanCacheListVLHGC.hpp"
#include "CopyScanCacheChunkVLHGC.hpp"
#include "CopyScanCacheChunkVLHGCInHeap.hpp"
#include "CopyScanCacheVLHGC.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"

MM_CopyScanCacheListVLHGC::MM_CopyScanCacheListVLHGC()
	: MM_BaseVirtual()
	, _sublists(NULL)
	, _sublistCount(0)
	, _chunkHead(NULL)
	, _totalEntryCount(0)
	, _containsHeapAllocatedChunks(false)
{
	_typeId = __FUNCTION__;
}

bool
MM_CopyScanCacheListVLHGC::initialize(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	_sublistCount = extensions->packetListSplit;
	Assert_MM_true(0 < _sublistCount);
	
	UDATA sublistBytes = sizeof(CopyScanCacheSublist) * _sublistCount;
	_sublists = (CopyScanCacheSublist *)extensions->getForge()->allocate(sublistBytes, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL == _sublists) {
		_sublistCount = 0;
		return false;
	}

	memset((void *)_sublists, 0, sublistBytes);
	for (UDATA i = 0; i < _sublistCount; i++) {
		if (!_sublists[i]._cacheLock.initialize(env, &extensions->lnrlOptions, "MM_CopyScanCacheListVLHGC:_sublists[]._cacheLock")) {
			return false;
		}
	}
	
	return true;
}

void
MM_CopyScanCacheListVLHGC::tearDown(MM_EnvironmentVLHGC *env)
{
	/* Free the memory allocated for the caches */
	while (NULL != _chunkHead) {
		MM_CopyScanCacheChunkVLHGC *_next = _chunkHead->getNext();
		_chunkHead->kill(env);
		_chunkHead = _next;
	}
	
	if (NULL != _sublists) {
		for (UDATA i = 0; i < _sublistCount; i++) {
			_sublists[i]._cacheLock.tearDown();
		}
		env->getForge()->free(_sublists);
		_sublists = NULL;
		_sublistCount = 0;
	}
}

bool
MM_CopyScanCacheListVLHGC::appendCacheEntries(MM_EnvironmentVLHGC *env, UDATA cacheEntryCount)
{
	CopyScanCacheSublist *cacheList = &_sublists[getSublistIndex(env)];
	MM_CopyScanCacheChunkVLHGC *chunk = MM_CopyScanCacheChunkVLHGC::newInstance(env, cacheEntryCount, &cacheList->_cacheHead, _chunkHead);
	if(NULL != chunk) {
		_chunkHead = chunk;
		_totalEntryCount += cacheEntryCount;
	}
	
	return NULL != chunk;
}

bool
MM_CopyScanCacheListVLHGC::resizeCacheEntries(MM_EnvironmentVLHGC *env, UDATA totalCacheEntryCount)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(env);
	
	/* If -Xgc:fvtest=scanCacheCount has been specified, then restrict the number of scan caches to n.
	 * Stop all future resizes from having any effect. */
	if (0 != ext->fvtest_scanCacheCount) {
		if (0 == _totalEntryCount) {
			totalCacheEntryCount = ext->fvtest_scanCacheCount;
			return appendCacheEntries(env, totalCacheEntryCount);
		} else {
			return true;
		}
	}
	
	if (totalCacheEntryCount > _totalEntryCount) {
		/* TODO: consider appending at least by some minimum entry count
		 * or having total entry count a multiple of let say 16 or so */
		return appendCacheEntries(env, totalCacheEntryCount - _totalEntryCount);
	}

	/* downsizing is non-trivial with current list/chunk implementation since
	 * the free caches are scattered across the chunks and cross reference themselves */	
	
	return true;
}

void
MM_CopyScanCacheListVLHGC::removeAllHeapAllocatedChunks(MM_EnvironmentVLHGC *env)
{
	if (_containsHeapAllocatedChunks) {
		/* execute if allocation in heap occur this Local GC */
		/*
		 * Walk caches list first to remove all references to heap allocated caches
		 */
		for (UDATA i = 0; i < _sublistCount; i++) {
			CopyScanCacheSublist *cacheList = &_sublists[i];

			MM_CopyScanCacheVLHGC *previousCache = NULL;
			MM_CopyScanCacheVLHGC *cache = cacheList->_cacheHead;
	
			while(cache != NULL) {
				if (0 != (cache->flags & J9VM_MODRON_SCAVENGER_CACHE_TYPE_HEAP)) {
					/* this cache is heap allocated - remove it from list */
					if (NULL == previousCache) {
						/* remove first element */
						cacheList->_cacheHead = (MM_CopyScanCacheVLHGC *)cache->next;
					} else {
						/* remove middle element */
						previousCache->next = cache->next;
					}
				} else {
					/* not heap allocated - just skip */
					previousCache = cache;
				}
				cache = (MM_CopyScanCacheVLHGC *)cache->next;
			}
		}

		/*
		 *  Walk caches chunks list and release all heap allocated
		 */
		MM_CopyScanCacheChunkVLHGC *previousChunk = NULL;
		MM_CopyScanCacheChunkVLHGC *chunk = _chunkHead;

		while(chunk != NULL) {
			MM_CopyScanCacheChunkVLHGC *nextChunk = chunk->getNext();

			if (0 != (chunk->getBase()->flags & J9VM_MODRON_SCAVENGER_CACHE_TYPE_HEAP)) {
				/* this chunk is heap allocated - remove it from list */
				if (NULL == previousChunk) {
					/* still be a first element */
					_chunkHead = nextChunk;
				} else {
					/* remove middle element */
					previousChunk->setNext(nextChunk);
				}

				/* release heap allocated chunk */
				chunk->kill(env);

			} else {
				/* not heap allocated - just skip */
				previousChunk = chunk;
			}
			chunk = nextChunk;
		}

		/* clear flag - no more heap allocated caches */
		_containsHeapAllocatedChunks = false;
	}
}

MM_CopyScanCacheVLHGC *
MM_CopyScanCacheListVLHGC::allocateCacheEntriesInExistingMemory(MM_EnvironmentVLHGC *env, void *buffer, UDATA bufferLengthInBytes)
{
	CopyScanCacheSublist *cacheList = &_sublists[getSublistIndex(env)];
	MM_CopyScanCacheVLHGC * result = NULL;
	MM_CopyScanCacheChunkVLHGCInHeap *chunk = MM_CopyScanCacheChunkVLHGCInHeap::newInstance(env, buffer, bufferLengthInBytes, &cacheList->_cacheHead, _chunkHead);
	if (NULL != chunk) {
		_chunkHead = chunk;
		_containsHeapAllocatedChunks = true;
		
		result = popCacheInternal(env, cacheList);
		Assert_MM_true(NULL != result);
	}
	return result;
}

void 
MM_CopyScanCacheListVLHGC::lock()
{
	for (UDATA i = 0; i < _sublistCount; ++i) {
		_sublists[i]._cacheLock.acquire();
	}
}

void 
MM_CopyScanCacheListVLHGC::unlock()
{
	for (UDATA i = 0; i < _sublistCount; ++i) {
		_sublists[i]._cacheLock.release();
	}
}

void 
MM_CopyScanCacheListVLHGC::pushCacheInternal(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cacheEntry, CopyScanCacheSublist* sublist)
{
	Assert_MM_true(NULL != cacheEntry);
	Assert_MM_true(NULL == cacheEntry->next);

	MM_CopyScanCacheVLHGC *oldCacheEntry = sublist->_cacheHead;
	cacheEntry->next = oldCacheEntry;
	sublist->_cacheHead = cacheEntry;
}

MM_CopyScanCacheVLHGC *
MM_CopyScanCacheListVLHGC::popCacheInternal(MM_EnvironmentVLHGC *env, CopyScanCacheSublist* sublist)
{
	MM_CopyScanCacheVLHGC *cache = sublist->_cacheHead;
	if (NULL != cache) {
		sublist->_cacheHead = (MM_CopyScanCacheVLHGC *)cache->next;
		cache->next = NULL;
	}
	return cache;
}


void
MM_CopyScanCacheListVLHGC::pushCacheNoLock(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cacheEntry)
{
	CopyScanCacheSublist *cacheList = &_sublists[getSublistIndex(env)];
	pushCacheInternal(env, cacheEntry, cacheList);
}

void
MM_CopyScanCacheListVLHGC::pushCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cacheEntry)
{
	CopyScanCacheSublist *cacheList = &_sublists[getSublistIndex(env)];
	cacheList->_cacheLock.acquire();
	pushCacheInternal(env, cacheEntry, cacheList);
	cacheList->_cacheLock.release();
}

MM_CopyScanCacheVLHGC *
MM_CopyScanCacheListVLHGC::popCacheNoLock(MM_EnvironmentVLHGC *env)
{
	UDATA indexStart = getSublistIndex(env);
	MM_CopyScanCacheVLHGC *cache = NULL;

	for (UDATA i = 0; (i < _sublistCount) && (NULL == cache); ++i) {
		UDATA index = (i + indexStart) % _sublistCount;
		CopyScanCacheSublist *cacheList = &_sublists[index];
		cache = popCacheInternal(env, cacheList); 
	}
	
	return cache;
}

MM_CopyScanCacheVLHGC *
MM_CopyScanCacheListVLHGC::popCache(MM_EnvironmentVLHGC *env)
{
	UDATA indexStart = getSublistIndex(env);
	MM_CopyScanCacheVLHGC *cache = NULL;
	for (UDATA i = 0; (i < _sublistCount) && (NULL == cache); ++i) {
		UDATA index = (i + indexStart) % _sublistCount;
		CopyScanCacheSublist *cacheList = &_sublists[index];
		if (NULL != cacheList->_cacheHead) {
			cacheList->_cacheLock.acquire();
			cache = popCacheInternal(env, cacheList); 
			cacheList->_cacheLock.release();
		}
	}
	return cache;
}

bool
MM_CopyScanCacheListVLHGC::isEmpty()
{
	MM_CopyScanCacheVLHGC *cache = NULL;
	for (UDATA i = 0; (i < _sublistCount) && (NULL == cache); ++i) {
		cache = _sublists[i]._cacheHead;
	}
	return NULL == cache;
}

UDATA
MM_CopyScanCacheListVLHGC::countCaches()
{
	UDATA count = 0;
	for (UDATA i = 0; i < _sublistCount; ++i) {
		MM_CopyScanCacheVLHGC *cache = _sublists[i]._cacheHead;
		while (NULL != cache) {
			count++;
			cache = (MM_CopyScanCacheVLHGC *)cache->next;
		}
	}
	return count;
}
