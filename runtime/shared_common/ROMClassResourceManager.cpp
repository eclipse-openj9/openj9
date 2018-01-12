/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
 * @ingroup Shared_Common
 */

#include "ROMClassResourceManager.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_ROMClassResourceManager::SH_ROMClassResourceManager() :
	_accessPermitted(false), _dataBytes(0)
{
	_htMutexName = "rrmTableMutex";
}

SH_ROMClassResourceManager::~SH_ROMClassResourceManager()
{
}

/* Creates a new hashtable. Pre-req: portLibrary must be initialized */
J9HashTable* 
SH_ROMClassResourceManager::localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries)
{
	J9HashTable* returnVal;

	Trc_SHR_RRM_localHashTableCreate_Entry(currentThread, initialEntries);
	returnVal = hashTableNew(OMRPORT_FROM_J9PORT(_portlib), (char*)_rrmHashTableName, initialEntries, sizeof(HashTableEntry), sizeof(char*), 0, J9MEM_CATEGORY_CLASSES, SH_ROMClassResourceManager::rrmHashFn, SH_ROMClassResourceManager::rrmHashEqualFn, NULL, (void*)currentThread->javaVM->internalVMFunctions);
	_hashTableGetNumItemsDoFn = SH_ROMClassResourceManager::customCountItemsInList;
	Trc_SHR_RRM_localHashTableCreate_Exit(currentThread, returnVal);
	return returnVal;
}

/* Hash function for hashtable */
UDATA
SH_ROMClassResourceManager::rrmHashFn(void* item, void *userData)
{
	HashTableEntry* itemValue = (HashTableEntry*)item;

	UDATA hashValue = UDATA(itemValue->key());

	return hashValue;
}

/* HashEqual function for hashtable */
UDATA
SH_ROMClassResourceManager::rrmHashEqualFn(void* left, void* right, void *userData)
{
	HashTableEntry* leftItem  = (HashTableEntry*)left;
	HashTableEntry* rightItem = (HashTableEntry*)right;

	/* No need to check cachelet as key is guaranteed to be unique */
	UDATA result = (leftItem->key() == rightItem->key());

	return result;
}

SH_ROMClassResourceManager::HashTableEntry::HashTableEntry(UDATA key, const ShcItem* item, SH_CompositeCache* cachelet)
 : _key(key),
   _item(item)
#if defined(J9SHR_CACHELET_SUPPORT)
   ,_cachelet(cachelet)
#endif
{
}

SH_ROMClassResourceManager::HashTableEntry::~HashTableEntry()
{
	/* Nothing to do */
}

SH_ROMClassResourceManager::HashTableEntry* 
SH_ROMClassResourceManager::rrmTableAddHelper(J9VMThread* currentThread, HashTableEntry* newEntry, SH_CompositeCache* cachelet)
{
	HashTableEntry* rc = NULL;

	Trc_SHR_RRM_rrmTableAdd_Entry(currentThread, newEntry->key(), newEntry->item());
	
	if ((rc = (HashTableEntry*)hashTableAdd(_hashTable, newEntry)) == NULL) {
		Trc_SHR_RRM_rrmTableAdd_Exception1(currentThread);
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_RRM_FAILED_CREATE_HASHTABLE_ENTRY);
	}
	Trc_SHR_RRM_rrmTableAdd_HashtableAdd(currentThread, rc);

	Trc_SHR_RRM_rrmTableAdd_Exit2(currentThread, rc);
	return rc;
}

SH_ROMClassResourceManager::HashTableEntry* 
SH_ROMClassResourceManager::rrmTableAdd(J9VMThread* currentThread, const ShcItem* item, SH_CompositeCache* cachelet)
{
	UDATA key = getKeyForItem(item);
	HashTableEntry newItem(key, item, cachelet);
	HashTableEntry* rc = NULL;

	if (lockHashTable(currentThread, _rrmAddFnName)) {
		rc = rrmTableAddHelper(currentThread, &newItem, cachelet);
	
		/* if the item was primed, fill in its value */
		if (rc->item() == NULL) {
			rc->setItem(item);
		}
		unlockHashTable(currentThread, _rrmAddFnName);
	} else {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_RRM_FAILED_ENTER_RRMMUTEX);
		Trc_SHR_RRM_rrmTableAdd_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}
	return rc;
}

#if defined(J9SHR_CACHELET_SUPPORT)

/**
 * Adds a primed entry.
 * This assumes the hash is the key, and all keys are unique.
 */
SH_ROMClassResourceManager::HashTableEntry* 
SH_ROMClassResourceManager::rrmTableAddPrime(J9VMThread* currentThread, UDATA key, SH_CompositeCache* cachelet)
{
	HashTableEntry newItem(key, NULL, cachelet);
	HashTableEntry* rc = NULL;

	if (lockHashTable(currentThread, _rrmAddFnName)) {
		rc = rrmTableAddHelper(currentThread, &newItem, cachelet);
		unlockHashTable(currentThread, _rrmAddFnName);
	} else {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_RRM_FAILED_ENTER_RRMMUTEX);
		Trc_SHR_RRM_rrmTableAdd_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}
	return rc;
}

#endif /* J9SHR_CACHELET_SUPPORT */

/* Hashtable lookup function. Returns value if found, otherwise returns NULL */
SH_ROMClassResourceManager::HashTableEntry* 
SH_ROMClassResourceManager::rrmTableLookup(J9VMThread* currentThread, UDATA key)
{
	HashTableEntry searchKey(key, 0, NULL);
	HashTableEntry* returnVal = NULL;

	Trc_SHR_RRM_rrmTableLookup_Entry(currentThread, key);

	if (lockHashTable(currentThread, _rrmLookupFnName)) {
		returnVal = (HashTableEntry*)hashTableFind(_hashTable, (void*)&searchKey);
		Trc_SHR_RRM_rrmTableLookup_HashtableFind(currentThread, returnVal);
		
#if defined(J9SHR_CACHELET_SUPPORT)
		if (_isRunningNested && returnVal && (returnVal->item() == NULL)) {
			IDATA rc;
			SH_CompositeCacheImpl* cachelet = (SH_CompositeCacheImpl*)returnVal->cachelet();
			
			/* Make sure the cachelet is started */
			unlockHashTable(currentThread, _rrmLookupFnName);
			/**
			 * @bug _startupMonitor IS A HORRIBLE HACK FOR CMVC 141328. 
			 * THIS WILL NOT WORK FOR NON-READONLY CACHES.
			 * @see SH_CompositeCacheImpl::_startupMonitor
			 */
			if (0 == (rc = cachelet->lockStartupMonitor(currentThread))) {
				if (!cachelet->isStarted()) {
					_cache->startupCachelet(currentThread, cachelet);
				}
				cachelet->unlockStartupMonitor(currentThread);
			} else {
				/* Don't return the unpopulated item. */
				Trc_SHR_RRM_rrmTableLookup_lockStartupMonitorFailed(currentThread, rc, cachelet, returnVal->key());
				returnVal = NULL;
			}
		} else {
			unlockHashTable(currentThread, _rrmLookupFnName);
			if (_isRunningNested && returnVal) {
				Trc_SHR_Assert_True(returnVal->cachelet()->isStarted());
			}
		}
#else
		unlockHashTable(currentThread, _rrmLookupFnName);
#endif
	} else {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_RRM_FAILED_ENTER_RRMMUTEX);
		Trc_SHR_RRM_rrmTableLookup_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}
	
	if (returnVal) {
		Trc_SHR_Assert_True(returnVal->item() != NULL);
	}
	Trc_SHR_RRM_rrmTableLookup_Exit2(currentThread, returnVal);
	return returnVal;
}

/* Hashtable remove function. Returns value if found, otherwise returns NULL
 * Returns 0 on success and 1 on failure */
UDATA 
SH_ROMClassResourceManager::rrmTableRemove(J9VMThread* currentThread, UDATA key)
{
	IDATA retryCount = 0;
	HashTableEntry searchKey(key, 0, NULL);
	UDATA returnVal = 1;

	Trc_SHR_RRM_rrmTableRemove_Entry(currentThread, key);

	while (retryCount < MONITOR_ENTER_RETRY_TIMES) {
		if (_cache->enterLocalMutex(currentThread, _htMutex, _htMutexName, _rrmRemoveFnName)==0) {
			returnVal = hashTableRemove(_hashTable, (void*)&searchKey);
			Trc_SHR_RRM_rrmTableRemove_HashtableRemove(currentThread, returnVal);
			_cache->exitLocalMutex(currentThread, _htMutex, _htMutexName, _rrmRemoveFnName);
			break;
		}
		retryCount++;
	}
	if (retryCount==MONITOR_ENTER_RETRY_TIMES) {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_RRM_FAILED_ENTER_RRMMUTEX);
		Trc_SHR_RRM_rrmTableRemove_Exit1(currentThread, retryCount);
		return 1;
	}

	Trc_SHR_RRM_rrmTableRemove_Exit2(currentThread, returnVal);
	return returnVal;
}

/**
 * Registers a new resource with the SH_ROMClassResourceManager
 *
 * Called when a new resource has been identified in the cache
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 * @param[in] itemInCache The address of the item found in the cache
 * 
 * @return true if successful, false otherwise
 */
bool
SH_ROMClassResourceManager::storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet)
{
	HashTableEntry* entry = NULL;

	/* Need to allow storeNew to work even if accessPermitted == false. 
	 * This is because we need to keep the hashtables up to date incase accessPermitted becomes true */
	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}
	Trc_SHR_RRM_storeNew_Entry(currentThread, itemInCache);

	if (!_cache->isStale(itemInCache)) {
		entry = rrmTableAdd(currentThread, itemInCache, cachelet);
		/* If there was an existing stale entry, remove it */
		_dataBytes += ITEMDATALEN(itemInCache);
		if (entry && _cache->isStale(entry->item())) {
			UDATA key = getKeyForItem(entry->item());
			
			rrmTableRemove(currentThread, key);
			entry = rrmTableAdd(currentThread, itemInCache, cachelet);
		}
		if (!entry) {
			Trc_SHR_RRM_storeNew_ExitFalse(currentThread);
			return false;
		}
	}

	Trc_SHR_RRM_storeNew_ExitTrue(currentThread);
 	return true;
}

/**
 * Look for a resource for the given ROM address
 *
 * @param[in] currentThread The current thread
 * @param[in] resourceKey An address in the ROMClass which a resource might be associated with
 *
 * @return The resource data if found, or null
 */
const void*
SH_ROMClassResourceManager::findResource(J9VMThread* currentThread, UDATA resourceKey)
{
	const void* returnVal = NULL;
	HashTableEntry* entry = NULL;
	
	if (!_accessPermitted) {
		return NULL;
	}

	Trc_SHR_RRM_find_Entry_V2(currentThread, resourceKey);

	if ((entry = rrmTableLookup(currentThread, resourceKey))) {
		const ShcItem* item = entry->item();
		returnVal = (const void*)ITEMDATA(item);
	}
	
	Trc_SHR_RRM_find_Exit(currentThread, returnVal);

	return returnVal;
}

UDATA
SH_ROMClassResourceManager::markStale(J9VMThread* currentThread, UDATA resourceKey, const ShcItem* itemInCache)
{
	UDATA returnVal = 1;

	if (!_accessPermitted) {
		return 0;
	}

	Trc_SHR_RRM_markStale_Entry(currentThread, resourceKey, itemInCache);
	
	if ((returnVal = rrmTableRemove(currentThread, resourceKey))==0) {
		_cache->markItemStale(currentThread, itemInCache, false);
	}
	
	Trc_SHR_RRM_markStale_Exit(currentThread, returnVal);

	return returnVal;
}

bool 
SH_ROMClassResourceManager::permitAccessToResource(J9VMThread* currentThread)
{
	return _accessPermitted;
}

UDATA
SH_ROMClassResourceManager::customCountItemsInList(void* entry, void* opaque)
{
	HashTableEntry* node = (HashTableEntry*)entry;
	SH_Manager::CountData* countData = (SH_Manager::CountData*)opaque;
	
	if (countData->_cache->isStale(node->item())) {
		++(countData->_staleItems);
	} else {
		++(countData->_nonStaleItems);
	}
	return 0;
}

void
SH_ROMClassResourceManager::getKeyAndItemForHashtableEntry(void* entry, UDATA* key, const ShcItem** item)
{
	HashTableEntry* found = (HashTableEntry*)entry;
	
	*key = found->key();
	*item = found->item();
}

UDATA
SH_ROMClassResourceManager::getDataBytes()
{
	return _dataBytes;
}

#if defined(J9SHR_CACHELET_SUPPORT)

bool 
SH_ROMClassResourceManager::canCreateHints()
{
	return false;
}

/**
 * A hash table do function. Count number of entries in a cachelet.
 */
UDATA
SH_ROMClassResourceManager::rrmCountCacheletHashes(void* entry, void* userData)
{
	HashTableEntry* node = (HashTableEntry*)entry;
	CacheletHintCountData* data = (CacheletHintCountData*)userData;

	if (data->cachelet == node->cachelet()) {
		data->hashCount++;
	}
	return FALSE; /* don't remove entry */
}

/**
 * A hash table do function
 */
UDATA
SH_ROMClassResourceManager::rrmCollectHashOfEntry(void* entry, void* userData)
{
	HashTableEntry* node = (HashTableEntry*)entry;
	CacheletHintHashData* data = (CacheletHintHashData*)userData;

	if (data->cachelet == node->cachelet()) {
		*data->hashSlot++ = rrmHashFn(entry, NULL);
	}
	return FALSE; /* don't remove entry */
}

/**
 * Collect the hash entry hashes into a cachelet hint.
 */
IDATA
SH_ROMClassResourceManager::rrmCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	CacheletHintHashData hashes;
	CacheletHintCountData counts;

	/* count hashes in this cachelet */
	counts.cachelet = cachelet;
	counts.hashCount = 0;
	hashTableForEachDo(_hashTable, rrmCountCacheletHashes, (void*)&counts);

	/* allocate hash array */
	hints->length = counts.hashCount * sizeof(UDATA);
	if (hints->length == 0) {
		hints->data = NULL;
		return 0;
	}
	hints->data = (U_8*)j9mem_allocate_memory(hints->length, J9MEM_CATEGORY_CLASSES);
	if (!hints->data) {
		/* TODO trace alloc failure */
		hints->length = 0;
		return -1;
	}

	/* collect hashes */
	hashes.cachelet = cachelet;
	hashes.hashSlot = (UDATA*)hints->data;
	hashTableForEachDo(_hashTable, rrmCollectHashOfEntry, (void*)&hashes);
	/* TODO trace hints written */
	return 0;
}

void
SH_ROMClassResourceManager::fixupHintsForSerialization(J9VMThread* vmthread,
			UDATA dataType, UDATA dataLen, U_8* data,
			IDATA deployedOffset, void* serializedROMClassStartAddress) 
{
	UDATA *hashSlot = (UDATA*)data;
	UDATA hintCount = dataLen / sizeof(UDATA);
	
	Trc_SHR_Assert_True(dataType == _dataTypesRepresented[0]);

	while (hintCount-- > 0) {
		UDATA temp = *hashSlot; /* this is an absolute address in the original supercache */
		
		/* The address may be in a different cachelet, but
		 * all cachelets in a supercache get the same deployed offset.
		 */
		temp += deployedOffset; /* convert to absolute address in the deployed supercache */ 
		temp -= (UDATA)serializedROMClassStartAddress; /* convert to offset from deployed supercache start */

#ifdef DEBUG
		fprintf(stderr, "\torig=%p, new abs=%p rel=%p\n", *hashSlot, *hashSlot + deployedOffset, temp);
#endif

		*hashSlot = temp;
		++hashSlot;
	}
}

/**
 * Walk the managed items hashtable in this cachelet. Allocate and populate an array
 * of hints, one for each hash entry.
 *
 * @param[in] self a data type manager
 * @param[in] vmthread the current VMThread
 * @param[out] hints a CacheletHints structure. This function fills in its
 * contents.
 *
 * @retval 0 success
 * @retval -1 failure
 */
IDATA
SH_ROMClassResourceManager::createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	Trc_SHR_Assert_True(hints != NULL);

	/* hints->dataType should have been set by the caller */
	Trc_SHR_Assert_True(hints->dataType == _dataTypesRepresented[0]);
	
	return rrmCollectHashes(vmthread, cachelet, hints);
}

/**
 * add a (_hashValue, cachelet) entry to the hash table
 * only called with hints of the right data type 
 */
IDATA
SH_ROMClassResourceManager::primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA dataLength)
{
	UDATA* hashSlot = (UDATA*)hintsData;
	UDATA hintCount = 0;
	UDATA segmentBase = 0;

	if ((dataLength == 0) || (hintsData == NULL)) {
		return 0;
	}
	
#ifdef DEBUG
	fprintf(stderr, "primeFromHints: supercache start=%p end=%p\n",
		((SH_CompositeCacheImpl*)cachelet)->getParent()->getFirstROMClassAddress(true),
		((SH_CompositeCacheImpl*)cachelet)->getParent()->getCacheEndAddress());
#endif

	/* The hint is supposed to be an absolute cache address. It's stored in
	 * the cache as an offset. Need to relocate the hint when priming the hashtable.
	 */
/* TODO: remove the entire function when J9SHR_CACHELET_SUPPORT is not defined */
	segmentBase = (UDATA)((SH_CompositeCacheImpl*)cachelet)->getParent()->getFirstROMClassAddress(true);
	hintCount = dataLength / sizeof(UDATA);
	while (hintCount-- > 0) {
		UDATA temp = *hashSlot;
		
		temp += segmentBase;

		Trc_SHR_RRM_primeHashtables_addingHint(vmthread, cachelet, temp);
		if (NULL == rrmTableAddPrime(vmthread, temp, cachelet)) {
			/* If we failed to finish priming the hints, just give up.
			 * Stuff should still work, just suboptimally.
			 */
			Trc_SHR_RRM_primeHashtables_failedToPrimeHint(vmthread, this, cachelet, *hashSlot);
			break;
		}
		++hashSlot;
	}
	
	return 0;
}

#endif /* J9SHR_CACHELET_SUPPORT */
