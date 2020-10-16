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

#include "ScopeManagerImpl.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_ScopeManagerImpl::SH_ScopeManagerImpl()
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
		found = (const HashEntry*)hashTableFind(_hashTable, (void*)&searchEntry);
		Trc_SHR_SMI_scTableLookup_HashtableFind(currentThread, found);
		unlockHashTable(currentThread, "scTableLookup");
	} else {
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
	_dataTypesRepresented[1] = TYPE_PREREQ_CACHE;
	_dataTypesRepresented[2] = 0;

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
		const J9UTF8* partition = (const J9UTF8*)_cache->getAddressFromJ9ShrOffset(&(scopedMatch->partitionOffset));
		if ((NULL == partition)
			|| (cachedPartition != partition)
		) {
			Trc_SHR_SMI_validate_ExitFailed2(currentThread);
			return 0;
		}
	} else {
		if (0 != scopedMatch->partitionOffset.offset) {
			Trc_SHR_SMI_validate_ExitFailed3(currentThread);
			return 0;
		}
	}
	if (cachedModContext) {
		const J9UTF8* modContext = (const J9UTF8*)_cache->getAddressFromJ9ShrOffset(&(scopedMatch->partitionOffset));
		if ((NULL == modContext)
			|| (cachedModContext != modContext)
		) {
			Trc_SHR_SMI_validate_ExitFailed4(currentThread);
			return 0;
		}
	} else {
		if (0 != scopedMatch->modContextOffset.offset) {
			Trc_SHR_SMI_validate_ExitFailed5(currentThread);
			return 0;
		}
	}

	Trc_SHR_SMI_validate_ExitOK(currentThread);
	return 1;
}
