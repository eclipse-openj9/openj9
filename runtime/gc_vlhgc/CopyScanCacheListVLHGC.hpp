
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(COPYSCANCACHELISTVLHGC_HPP_)
#define COPYSCANCACHELISTVLHGC_HPP_

#include "j9.h"
#include "j9consts.h"
#include "modronopt.h"	

#include "BaseVirtual.hpp"
#include "EnvironmentVLHGC.hpp" 
#include "LightweightNonReentrantLock.hpp"

class MM_CopyScanCacheVLHGC;
class MM_CopyScanCacheChunkVLHGC;
 
/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CopyScanCacheListVLHGC : public MM_BaseVirtual
{
public:
private:
	struct CopyScanCacheSublist {
		MM_CopyScanCacheVLHGC *_cacheHead;
		MM_LightweightNonReentrantLock _cacheLock;
	} *_sublists;
	UDATA _sublistCount; /**< the number of lists (split for parallelism). Must be at least 1 */
	MM_CopyScanCacheChunkVLHGC *_chunkHead;
	UDATA _totalEntryCount;
	bool _containsHeapAllocatedChunks;	/**< True if there are heap-allocated scan cache chunks on the _chunkHead list */

private:
	bool appendCacheEntries(MM_EnvironmentVLHGC *env, UDATA cacheEntryCount);
	
	/**
	 * Determine the sublist index for the specified thread.
	 * @return the index (a valid index into _sublists)
	 */
	UDATA getSublistIndex(MM_EnvironmentVLHGC *env) { return env->getEnvironmentId() % _sublistCount; }
	
	/**
	 * Add the specified entry to this list. It is the caller's responsibility to provide synchronization.
	 * @param env[in] the current GC thread
	 * @param cacheEntry[in] the cache entry to add
	 * @param sublist[in] the sublist to push to
	 */
	void pushCacheInternal(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cacheEntry, CopyScanCacheSublist* sublist);
	
	/**
	 * Pop a cache entry from this list. It is the caller's responsibility to provide synchronization.
	 * @param env[in] the current GC thread
	 * @param sublist[in] the sublist to pop from
	 * @return the cache entry, or NULL if the list is empty
	 */
	MM_CopyScanCacheVLHGC *popCacheInternal(MM_EnvironmentVLHGC *env, CopyScanCacheSublist* sublist);

public:
	bool initialize(MM_EnvironmentVLHGC *env);
	virtual void tearDown(MM_EnvironmentVLHGC *env);

	/**
	 * Resizes the number of cache entries.
	 * 
	 * @param env[in] A GC thread
	 * @param totalCacheEntryCount[in] The number of cache entries which this list should be resized to contain
	 */
	bool resizeCacheEntries(MM_EnvironmentVLHGC *env, UDATA totalCacheEntryCount);

	/**
	 * Removes any heap-allocated scan cache chunks from the receiver's chunk list (calls tearDown on them after removing them from the list).
	 * @param env[in] A GC thread
	 */
	void removeAllHeapAllocatedChunks(MM_EnvironmentVLHGC *env);

	/**
	 * Allocates an on-heap scan cache in the bufferLengthInBytes of memory starting at buffer.
	 * @param env[in] A GC thread
	 * @param buffer[in] The base address of the memory extent on which the receiver should allocate a scan cache chunk
	 * @param bufferLengthInBytes[in] The length, in bytes, of the memory extent starting at buffer
	 * @return A new cache entry if the chunk was successfully allocated and inserted into the receiver's chunk list, NULL if a failure occurred
	 */
	MM_CopyScanCacheVLHGC * allocateCacheEntriesInExistingMemory(MM_EnvironmentVLHGC *env, void *buffer, UDATA bufferLengthInBytes);

	/**
	 * Lock the receiver to prevent concurrent modification.
	 * @note that pushCache() and popCache() lock internally, so this is only required for heavyweight operations.
	 */
	void lock();

	/**
	 * Unlock the receiver from concurrent modification.
	 * @note that pushCache() and popCache() lock internally, so this is only required for heavy weight operations.
	 */
	void unlock();
	
	/**
	 * Add the specified entry to this list. It is the caller's responsibility to provide synchronization.
	 * @param env[in] the current GC thread
	 * @param cacheEntry[in] the cache entry to add
	 */
	void pushCacheNoLock(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cacheEntry);
	
	/**
	 * Pop a cache entry from this list. It is the caller's responsibility to provide synchronization.
	 * @param env[in] the current GC thread
	 * @return the cache entry, or NULL if the list is empty
	 */
	MM_CopyScanCacheVLHGC *popCacheNoLock(MM_EnvironmentVLHGC *env);
	
	/**
	 * Add the specified entry to this list.
	 * @param env[in] the current GC thread
	 * @param cacheEntry[in] the cache entry to add
	 */
	void pushCache(MM_EnvironmentVLHGC *env, MM_CopyScanCacheVLHGC *cacheEntry);
	
	/**
	 * Pop a cache entry from this list.
	 * @param env[in] the current GC thread
	 * @return the cache entry, or NULL if the list is empty
	 */
	MM_CopyScanCacheVLHGC *popCache(MM_EnvironmentVLHGC *env);
	
	/**
	 * Determine if the list is empty.
	 * @return true if the list is empty, false otherwise
	 */
	bool isEmpty();
	
	/**
	 * Determine how many caches are in the receiver's lists.
	 * Note that this is not thread safe. The receiver must be locked.
	 * The implementation walks all caches, so this should only be used for debugging.
	 * @return the total number of caches in the receiver's lists
	 */
	UDATA countCaches();
	
	/**
	 * Answer the total number of caches owned by the receiver, whether or not they are currently in the receiver's lists.
	 * @return total number of caches
	 */
	UDATA getTotalCacheCount() { return _totalEntryCount; }

	/**
	 * Create a CopyScanCacheList object.
	 */
	MM_CopyScanCacheListVLHGC();
	
};

#endif /* COPYSCANCACHELISTVLHGC_HPP_ */
