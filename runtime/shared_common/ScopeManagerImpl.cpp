/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#include "ScopeManagerImpl.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_ScopeManagerImpl::SH_ScopeManagerImpl()
#if defined(J9SHR_CACHELET_SUPPORT)
	:_allCacheletsStarted(false)
#endif /* J9SHR_CACHELET_SUPPORT */
{
	_htMutexName = "scTableMutex";
}

SH_ScopeManagerImpl::~SH_ScopeManagerImpl()
{
}

/* Creates a new hashtable. Pre-req: portLibrary must be initialized */
J9HashTable* 
SH_ScopeManagerImpl::localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries)
{
	J9HashTable* returnVal;

	Trc_SHR_SMI_localHashTableCreate_Entry(currentThread, initialEntries);
	returnVal = hashTableNew(OMRPORT_FROM_J9PORT(_portlib), J9_GET_CALLSITE(), initialEntries, sizeof(HashEntry), sizeof(char *), 0, J9MEM_CATEGORY_CLASSES, SH_ScopeManagerImpl::scHashFn, SH_ScopeManagerImpl::scHashEqualFn, NULL, (void*)currentThread->javaVM->internalVMFunctions);
	Trc_SHR_SMI_localHashTableCreate_Exit(currentThread, returnVal);
	return returnVal;
}

IDATA
SH_ScopeManagerImpl::localInitializePools(J9VMThread* currentThread)
{
	return 0;
}

void
SH_ScopeManagerImpl::localTearDownPools(J9VMThread* currentThread)
{
}


/* Hash function for hashtable */
UDATA
SH_ScopeManagerImpl::scHashFn(void* item, void *userData)
{
	HashEntry* entryItem = (HashEntry*)item;
	const J9UTF8* utf8 = entryItem->_value;
	J9InternalVMFunctions* internalFunctionTable = (J9InternalVMFunctions*)userData;
	UDATA hashValue = 0;

	Trc_SHR_SMI_scHashFn_Entry(item);
	hashValue = internalFunctionTable->computeHashForUTF8(J9UTF8_DATA(utf8), J9UTF8_LENGTH(utf8));
	Trc_SHR_SMI_scHashFn_Exit(hashValue);
	return hashValue;
}

/* HashEqual function for hashtable */
UDATA
SH_ScopeManagerImpl::scHashEqualFn(void* left, void* right, void *userData)
{
	HashEntry* leftItem = (HashEntry*)left;
	HashEntry* rightItem = (HashEntry*)right;
	const J9UTF8* utf8left = leftItem->_value;
	const J9UTF8* utf8right = rightItem->_value;
	UDATA result;

	Trc_SHR_SMI_scHashEqualFn_Entry(utf8left, utf8right);

	/* PERFORMANCE: Compare key size first as this is the most likely cause for exit */
	if (J9UTF8_LENGTH(utf8left) != J9UTF8_LENGTH(utf8right)) {
		Trc_SHR_SMI_scHashEqualFn_Exit2();
		return 0;
	}
	if (J9UTF8_DATA(utf8left)==NULL || J9UTF8_DATA(utf8right)==NULL) {
		Trc_SHR_SMI_scHashEqualFn_Exit1();
		return 0;
	}
	result = J9UTF8_EQUALS(utf8left, utf8right);
	Trc_SHR_SMI_scHashEqualFn_Exit3(result);
	return result;
}

const SH_ScopeManagerImpl::HashEntry* 
SH_ScopeManagerImpl::scTableAdd(J9VMThread* currentThread, const ShcItem* item, SH_CompositeCache* cachelet)
{
	const J9UTF8* key = (const J9UTF8*)ITEMDATA(item);
	const HashEntry* returnVal = NULL;
	HashEntry entry(key, cachelet);
	PORT_ACCESS_FROM_PORT(_portlib);
	
	Trc_SHR_SMI_scTableAdd_Entry(currentThread, J9UTF8_LENGTH(key), J9UTF8_DATA(key), item);
	
	if (lockHashTable(currentThread, "scTableAdd")) {
		if ((returnVal = (HashEntry*)hashTableAdd(_hashTable, &entry))==NULL) {
			Trc_SHR_SMI_scTableAdd_Exception1(currentThread);
			M_ERR_TRACE(J9NLS_SHRC_SMI_FAILED_CREATE_HASHTABLE_ENTRY);
		}
		Trc_SHR_SMI_scTableAdd_HashtableAdd(currentThread, returnVal);
		unlockHashTable(currentThread, "scTableAdd");
	} else {
		M_ERR_TRACE(J9NLS_SHRC_SMI_FAILED_ENTER_SCMUTEX);
		Trc_SHR_SMI_scTableAdd_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}

	if (returnVal) {
		Trc_SHR_SMI_scTableAdd_Exit2(currentThread, returnVal);
		return returnVal;
	}
	Trc_SHR_SMI_scTableAdd_Exit2(currentThread, NULL);
	return NULL;
}

/* Hashtable lookup function. Returns value if found, otherwise returns NULL */
const J9UTF8*
SH_ScopeManagerImpl::scTableLookup(J9VMThread* currentThread, const J9UTF8* key)
{
	const HashEntry* found = NULL;
	const J9UTF8* returnVal = NULL;
	HashEntry searchEntry(key, NULL);
	
	Trc_SHR_SMI_scTableLookup_Entry(currentThread, J9UTF8_LENGTH(key), J9UTF8_DATA(key));
	
	if (lockHashTable(currentThread, "scTableLookup")) {
#if defined(J9SHR_CACHELET_SUPPORT)
		if (_isRunningNested) {
			UDATA hint = scHashFn(&searchEntry, currentThread->javaVM->internalVMFunctions);
			
			if (startupHintCachelets(currentThread, hint) == -1) {
				goto exit_lockFailed;
			}
		}
#endif
		found = (const HashEntry*)hashTableFind(_hashTable, (void*)&searchEntry);
		Trc_SHR_SMI_scTableLookup_HashtableFind(currentThread, found);
		unlockHashTable(currentThread, "scTableLookup");
	} else {
#if defined(J9SHR_CACHELET_SUPPORT)
exit_lockFailed:
#endif
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_SMI_FAILED_ENTER_SCMUTEX);
		Trc_SHR_SMI_scTableLookup_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}

	if (found) {
		returnVal = found->_value;
	}
	Trc_SHR_SMI_scTableLookup_Exit2(currentThread, returnVal);
	return returnVal;
}

#if defined(J9SHR_CACHELET_SUPPORT)

/**
 * A hash table do function. Collect hash values of entries in a cachelet.
 */
UDATA
SH_ScopeManagerImpl::scCollectHashOfEntry(void* entry_, void* userData)
{
	HashEntry* entry = (HashEntry*)entry_;
	CacheletHintHashData* data = (CacheletHintHashData*)userData;

	if (data->cachelet == entry->_cachelet) {
		*data->hashSlot++ = scHashFn(entry, data->userData);
	}
	return FALSE; /* don't remove entry */
}

/**
 * A hash table do function. Count number of entries in a cachelet.
 */
UDATA
SH_ScopeManagerImpl::scCountCacheletHashes(void* entry_, void* userData)
{
	HashEntry* entry = (HashEntry*)entry_;
	CacheletHintCountData* data = (CacheletHintCountData*)userData;

	if (data->cachelet == entry->_cachelet) {
		data->hashCount++;
	}
	return FALSE; /* don't remove entry */
}

/**
 * Collect the hash entry hashes into a cachelet hint.
 */
IDATA
SH_ScopeManagerImpl::scCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	CacheletHintHashData hashes;
	CacheletHintCountData counts;

	/* count hashes in this cachelet */
	counts.cachelet = cachelet;
	counts.hashCount = 0;
	hashTableForEachDo(_hashTable, scCountCacheletHashes, (void*)&counts);
	/* TODO trace hints written. Note this runs after trace engine has shut down. */
	
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
	hashes.userData = (void*)currentThread->javaVM->internalVMFunctions;
	hashTableForEachDo(_hashTable, scCollectHashOfEntry, (void*)&hashes);
	/* TODO trace hints written */

	return 0;
}

#endif /* J9SHR_CACHELET_SUPPORT */

/**
 * Create a new instance of SH_ScopeManagerImpl
 *
 * @param [in] vm A Java VM
 * @param [in] cache_ The SH_SharedCache that will use this ScopeManager
 * @param [in] memForConstructor Memory in which to build the instance
 *
 * @return new SH_ScopeManager
 */	
SH_ScopeManagerImpl*
SH_ScopeManagerImpl::newInstance(J9JavaVM* vm, SH_SharedCache* cache_, SH_ScopeManagerImpl* memForConstructor)
{
	SH_ScopeManagerImpl* newSCM = (SH_ScopeManagerImpl*)memForConstructor;

	Trc_SHR_SMI_newInstance_Entry(vm, cache_);

	new(newSCM) SH_ScopeManagerImpl();
	newSCM->initialize(vm, cache_, ((BlockPtr)memForConstructor + sizeof(SH_ScopeManagerImpl)));

	Trc_SHR_SMI_newInstance_Exit(newSCM);

	return newSCM;
}

/* Initialize the SH_ScopeManager - should be called before startup */
void
SH_ScopeManagerImpl::initialize(J9JavaVM* vm, SH_SharedCache* cache_, BlockPtr memForConstructor)
{
	Trc_SHR_SMI_initialize_Entry();

	_cache = cache_;
	_portlib = vm->portLibrary;
	_htMutex = NULL;
	_dataTypesRepresented[0] = TYPE_SCOPE;
	_dataTypesRepresented[1] = _dataTypesRepresented[2] = 0;

	notifyManagerInitialized(_cache->managers(), "TYPE_SCOPE");

	Trc_SHR_SMI_initialize_Exit();
}

/**
 * Returns the number of bytes required to construct this SH_ScopeManager
 *
 * @return size in bytes
 */
UDATA
SH_ScopeManagerImpl::getRequiredConstrBytes(void)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(SH_ScopeManagerImpl);
	return reqBytes;
}

U_32
SH_ScopeManagerImpl::getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes)
{
	return (U_32)((cacheSizeBytes / 100000) + 20);	
}

/**
 * Registers a new cached Scope with the SH_ScopeManager
 *
 * Called when a new Scope has been identified in the cache
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 * @param[in] itemInCache The address of the item found in the cache
 * 
 * @return true if successful, false otherwise
 */
bool 
SH_ScopeManagerImpl::storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet)
{
	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}
	Trc_SHR_SMI_storeNew_Entry(currentThread, itemInCache);

	if (!scTableAdd(currentThread, itemInCache, cachelet)) {
		Trc_SHR_SMI_storeNew_ExitFalse(currentThread);
		return false;
	}

	Trc_SHR_SMI_storeNew_ExitTrue(currentThread);
 	return true;
}

/**
 * Find a cached scope for a given J9UTF8 local scope
 *
 * @param[in] currentThread  The current VM thread
 * @param[in] localScope  A local J9UTF8 struct
 *
 * @return A cached J9UTF8 scope if it is found
 */
const J9UTF8*
SH_ScopeManagerImpl::findScopeForUTF(J9VMThread* currentThread, const J9UTF8* localScope)
{
	const J9UTF8* result = NULL;

	if (getState() != MANAGER_STATE_STARTED) {
		return NULL;
	}

	Trc_SHR_SMI_findScopeForUTF_Entry(currentThread, localScope);

	if (localScope) {
		result = scTableLookup(currentThread, localScope);
	}

	Trc_SHR_SMI_findScopeForUTF_Exit(currentThread, result);

	return result;
}

/**
 * Given a scoped ROMClass, tests whether its partition and modContext match the ones provided
 *
 * @param[in] currentThread  The current VM thread
 * @param[in] partition  A partition to test
 * @param[in] modContext  A modification context to test
 * @param[in] match  An ShcItem struct from the cache to test, which must be TYPE_SCOPED_ROMCLASS
 *
 * @return  -1 for an error, 0 if test fails and 1 if test succeeds.
 */
IDATA
SH_ScopeManagerImpl::validate(J9VMThread* currentThread, const J9UTF8* partition, const J9UTF8* modContext, const ShcItem* match)
{
	const J9UTF8 *cachedPartition, *cachedModContext;
	ScopedROMClassWrapper* scopedMatch = NULL;

	if (getState() != MANAGER_STATE_STARTED) {
		return -1;
	}
	Trc_SHR_SMI_validate_Entry(currentThread, partition, modContext);

	if (ITEMTYPE(match) != TYPE_SCOPED_ROMCLASS) {
		Trc_SHR_SMI_validate_Exit1(currentThread);
		return (partition==NULL && modContext==NULL) ? 1 : 0;
	}
	cachedPartition = findScopeForUTF(currentThread, partition);
	cachedModContext = findScopeForUTF(currentThread, modContext);
	scopedMatch = (ScopedROMClassWrapper*)ITEMDATA(match);
	if (cachedPartition) {
		if (!(scopedMatch->partitionOffset && (cachedPartition==(const J9UTF8*)RCWPARTITION(scopedMatch)))) {
			Trc_SHR_SMI_validate_ExitFailed2(currentThread);
			return 0;
		}
	} else 
	if (scopedMatch->partitionOffset) {
		Trc_SHR_SMI_validate_ExitFailed3(currentThread);
		return 0;
	}
	if (cachedModContext) {
		if (!(scopedMatch->modContextOffset && (cachedModContext==(const J9UTF8*)RCWMODCONTEXT(scopedMatch)))) {
			Trc_SHR_SMI_validate_ExitFailed4(currentThread);
			return 0;
		}
	} else
	if (scopedMatch->modContextOffset) {
		Trc_SHR_SMI_validate_ExitFailed5(currentThread);
		return 0;
	}

	Trc_SHR_SMI_validate_ExitOK(currentThread);
	return 1;
}


#if defined(J9SHR_CACHELET_SUPPORT)

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
SH_ScopeManagerImpl::createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	Trc_SHR_Assert_True(hints != NULL);

	/* hints->dataType should have been set by the caller */
	Trc_SHR_Assert_True(hints->dataType == _dataTypesRepresented[0]);
	
	return scCollectHashes(vmthread, cachelet, hints);
}

/**
 * add a (_hashValue, cachelet) entry to the hash table
 * only called with hints of the right data type 
 *
 * each hint is a UDATA-length hash of a string
 * 
 * This method is not threadsafe.
 */
IDATA
SH_ScopeManagerImpl::primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA dataLength)
{
	UDATA* hashSlot = (UDATA*)hintsData;
	UDATA hintCount = 0;

	if ((dataLength == 0) || (hintsData == NULL)) {
		return 0;
	}

	hintCount = dataLength / sizeof(UDATA);
	while (hintCount-- > 0) {
		Trc_SHR_SMI_primeHashtables_addingHint(vmthread, cachelet, *hashSlot);
		if (!_hints.addHint(vmthread, *hashSlot, cachelet)) {
			/* If we failed to finish priming the hints, just give up.
			 * Stuff should still work, just suboptimally.
			 */
			Trc_SHR_SMI_primeHashtables_failedToPrimeHint(vmthread, this, cachelet, *hashSlot);
			break;
		}
		++hashSlot;
	}
	
	return 0;
}

#endif /* J9SHR_CACHELET_SUPPORT */
