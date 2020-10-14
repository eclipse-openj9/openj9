/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
		unlockHashTable(currentThread, _rrmLookupFnName);
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
		/* If there was an existing entry, remove it */
		_dataBytes += ITEMDATALEN(itemInCache);
		const ShcItem* itemInHashTable = NULL;
		if (NULL != entry) {
			itemInHashTable = entry->item();
			if (NULL != itemInHashTable) {
				if (itemInCache != itemInHashTable) {
					/* found an existing item under the same key, remove it from the hashTable */
					UDATA key = getKeyForItem(itemInHashTable);
					rrmTableRemove(currentThread, key);
					Trc_SHR_RRM_storeNew_existingEntryRemoved(currentThread, itemInHashTable);
					entry = rrmTableAdd(currentThread, itemInCache, cachelet);
				}
			}
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
