/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * @ingroup ZipSupport
 * @brief Zip Support for Java VM
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "j9port.h"
#include "zipsup.h"
#include "pool_api.h"
#include "omrutil.h"
#include "zip_internal.h"
#include "j9memcategories.h"


void zipCachePool_doFindHandler (J9ZipCachePoolEntry *entry, J9ZipCachePool *zcp);
void zipCachePool_doKillHandler (J9ZipCachePoolEntry *entry, J9ZipCachePool *zcp);


/**
 * Add a new cache to the pool with reference count of 1.
 * 
 * When reference count reaches zero 	the pool will automatically be freed.
 *
 * @param[in] zcp the zip cache pool that is being added to.
 * @param[in] zipCache the zip cache being added.
 *
 * @return TRUE if successful, FALSE otherwise.
 *
 * @note A cache may only reside in one pool (read: multiple VMs may not share caches with each other). 
*/

BOOLEAN zipCachePool_addCache(J9ZipCachePool *zcp, J9ZipCache *zipCache)
{
	J9ZipCachePoolEntry *entry;

	if (!zcp || !zipCache)  return FALSE;

	MUTEX_ENTER(zcp->mutex);

	entry = pool_newElement(zcp->pool);
	if (!entry)  {
		MUTEX_EXIT(zcp->mutex);
		return FALSE;
	}

	zipCache->cachePool = zcp;
	zipCache->cachePoolEntry = entry;

	entry->cache = zipCache;
	entry->referenceCount = 1;

	MUTEX_EXIT(zcp->mutex);
	return TRUE;
}



/** 
 * Increment the reference count of a cache in the pool.
 * 
 * @note Result is undefined if the cache is not actually in the pool!
 *
 * @param[in] zcp the zip cache pool that is being added to.
 * @param[in] the zip cache being added.
 *
 * @return TRUE if successful, FALSE otherwise.
*/

BOOLEAN zipCachePool_addRef(J9ZipCachePool *zcp, J9ZipCache *zipCache)
{
	J9ZipCachePoolEntry *entry;

	if (!zcp || !zipCache)  return FALSE;

	MUTEX_ENTER(zcp->mutex);

	entry = (J9ZipCachePoolEntry *)zipCache->cachePoolEntry;
	if (!entry)  {
		MUTEX_EXIT(zcp->mutex);
		return FALSE;
	}

	entry->referenceCount++;

	MUTEX_EXIT(zcp->mutex);
	return TRUE;
}



/**
 * Scans the pool for a cache with matching zipFileName, zipFileSize and zipTimeStamp.
 *
 * The reference count is incremented and the cache is returned if a match is found. 
 *
 * @param[in] zcp the zip cache pool to search
 * @param[in] zipFileName the name to test for match
 * @param[in] zipFileNameLength the length of zipFileName
 * @param[in] zipFileSize the size to test for match
 * @param[in] zipTimeStamp the time stamp to test for match 
 *
 * @return the matching zip cache
 * @return NULL if no match is found.
 */

J9ZipCache * zipCachePool_findCache(J9ZipCachePool *zcp, char const *zipFileName, IDATA zipFileNameLength, IDATA zipFileSize, I_64 zipTimeStamp)
{
	J9ZipCache *zipCache;
	J9ZipCachePoolEntry *entry;

	if (!zcp || !zipFileName)  return NULL;

	MUTEX_ENTER(zcp->mutex);

	/* Find a suitable cache */
	zcp->desiredCache = NULL;
	zcp->zipFileName = zipFileName;
	zcp->zipFileSize = zipFileSize;
	zcp->zipTimeStamp = zipTimeStamp;
	zcp->zipFileNameLength = zipFileNameLength;

	pool_do(zcp->pool, (void(*)(void*,void*))zipCachePool_doFindHandler, zcp);
	zipCache = zcp->desiredCache;

	if (zipCache)  {
		entry = (J9ZipCachePoolEntry *)zipCache->cachePoolEntry;
		entry->referenceCount++;
	}

	MUTEX_EXIT(zcp->mutex);
	return zipCache;
}



/** 
 * Deletes a pool containing shareable zip caches.
 *
 * @param[in] zcp the zip cache pool that is being deleted
 *
 * @return none
 *
 * @note Warning: This also deletes remaining caches in the pool, regardless of their reference counts!
 * 
 */
void zipCachePool_kill(J9ZipCachePool *zcp)
{
	j9memFree_fptr_t memFree;
	void* userData;
	
	if (!zcp)  return;
	
	zip_shutdownZipCachePoolHookInterface(zcp);
	
	pool_do(zcp->pool, (void(*)(void*,void*))zipCachePool_doKillHandler, zcp);

	MUTEX_DESTROY(zcp->mutex);

	/* Grab the memFree and userData out of the pool BEFORE we destroy it. */
	memFree = zcp->pool->memFree;
	userData = zcp->pool->userData;
	pool_kill(zcp->pool);
	if (zcp->workBuffer != NULL) {
		memFree(userData, zcp->workBuffer, 0);
	}
	memFree(userData, zcp, 0);
}



/**
 * Creates a pool to hold shareable zip caches with their reference counts. 
 * This should be called once per VM. 
 * 
 * @param[in] portLib the port library
 *
 * @return a zip cache pool or NULL if one cannot be created
 *
*/

J9ZipCachePool *zipCachePool_new(J9PortLibrary * portLib, void * userData)
{
	PORT_ACCESS_FROM_PORT(portLib);

	J9ZipCachePool *p = j9mem_allocate_memory(sizeof(*p), J9MEM_CATEGORY_VM_JCL);
	J9ZipCachePool *toReturn = NULL;

	if (p != NULL) {
		p->userData = userData;
		p->allocateWorkBuffer = TRUE;
		p->workBuffer = NULL;
		if (MUTEX_INIT(p->mutex)) {
			p->pool = pool_new(sizeof(J9ZipCachePoolEntry),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_VM_JCL, POOL_FOR_PORT(portLib));
			if (p->pool) {
				if (zip_initZipCachePoolHookInterface(portLib, p) == 0) {
					/* All initialization worked so set up to return the pointer */
					toReturn = p;
				}
			}
			if (NULL == toReturn) {
				/* pool discovery failed so give up the mutex */
				MUTEX_DESTROY(p->mutex);
			}
		}
		if (NULL == toReturn)
		{
			/* something went wrong so free the memory */
			j9mem_free_memory(p);
		}
	}
	return toReturn;
}



/**
 * Decrements the reference count of a cache in the pool.
 * If the reference count reaches 0, the cache is removed from the pool and @ref zipCache_kill is called on it. 
 *
 * @param[in] zcp the zip cache pool
 * @param[in] zipCache the zip cache whose count is being decremented.
 *
 * @return TRUE if the cache was destroyed
 * @return FALSE if the cache is still in the pool.
 *
 */

BOOLEAN zipCachePool_release(J9ZipCachePool *zcp, J9ZipCache *zipCache)
{
	J9ZipCachePoolEntry *entry;

	if (!zcp || !zipCache)  return FALSE;

	MUTEX_ENTER(zcp->mutex);

	entry = (J9ZipCachePoolEntry *)zipCache->cachePoolEntry;
	if (!entry)  {
		/* What the..? */
		MUTEX_EXIT(zcp->mutex);
		return FALSE;
	}
	
	if (--entry->referenceCount != 0)  {
		MUTEX_EXIT(zcp->mutex);
		return FALSE;
	}

	/* Reference count is zero, get rid of the cache */
	zipCache_kill(entry->cache);
	pool_removeElement(zcp->pool, entry);

	MUTEX_EXIT(zcp->mutex);
	return TRUE;
}



void zipCachePool_doFindHandler(J9ZipCachePoolEntry *entry, J9ZipCachePool *zcp)
{

	if (zcp->desiredCache)  return;  /* already done */

	if (!zipCache_isSameZipFile(entry->cache, (IDATA) zcp->zipTimeStamp, zcp->zipFileSize, zcp->zipFileName, zcp->zipFileNameLength)) {
		return;
	}

	/* Looks like we have a match. */
	zcp->desiredCache = entry->cache;
}



void zipCachePool_doKillHandler(J9ZipCachePoolEntry *entry, J9ZipCachePool *zcp)
{
	zipCache_kill(entry->cache);
}




