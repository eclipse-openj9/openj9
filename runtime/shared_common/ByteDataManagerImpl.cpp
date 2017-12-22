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

#include "ByteDataManagerImpl.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_ByteDataManagerImpl::SH_ByteDataManagerImpl()
{
}

SH_ByteDataManagerImpl::~SH_ByteDataManagerImpl()
{
}

/* Creates a new hashtable. Pre-req: portLibrary must be initialized */
J9HashTable* 
SH_ByteDataManagerImpl::localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries)
{
	J9HashTable* returnVal;

	Trc_SHR_BDMI_localHashTableCreate_Entry(currentThread, initialEntries);
	returnVal = hashTableNew(OMRPORT_FROM_J9PORT(_portlib), J9_GET_CALLSITE(), initialEntries, sizeof(SH_Manager::HashLinkedListImpl), sizeof(char *), 0,  J9MEM_CATEGORY_CLASSES, SH_Manager::hllHashFn, SH_Manager::hllHashEqualFn, NULL, (void*)currentThread->javaVM->internalVMFunctions);
	Trc_SHR_BDMI_localHashTableCreate_Exit(currentThread, returnVal);
	return returnVal;
}

/**
 * Create a new instance of SH_ByteDataManagerImpl
 *
 * @param [in] vm A Java VM
 * @param [in] cache_ The SH_SharedCache that will use this ByteDataManager
 * @param [in] memForConstructor Memory in which to build the instance
 *
 * @return new SH_ByteDataManager
 */	
SH_ByteDataManagerImpl*
SH_ByteDataManagerImpl::newInstance(J9JavaVM* vm, SH_SharedCache* cache_, SH_ByteDataManagerImpl* memForConstructor)
{
	SH_ByteDataManagerImpl* newBDM = (SH_ByteDataManagerImpl*)memForConstructor;

	Trc_SHR_BDMI_newInstance_Entry(vm, cache_);

	new(newBDM) SH_ByteDataManagerImpl();
	newBDM->initialize(vm, cache_, ((BlockPtr)memForConstructor + sizeof(SH_ByteDataManagerImpl)));

	Trc_SHR_BDMI_newInstance_Exit(newBDM);

	return newBDM;
}

/* Initialize the SH_ByteDataManager - should be called before startup */
void
SH_ByteDataManagerImpl::initialize(J9JavaVM* vm, SH_SharedCache* cache_, BlockPtr memForConstructor)
{
	Trc_SHR_BDMI_initialize_Entry();

	_cache = cache_;
	_portlib = vm->portLibrary;
	_htMutex = NULL;
	memset(_indexedBytesByType, 0, sizeof(_indexedBytesByType));
	memset(_numIndexedBytesByType, 0, sizeof(_numIndexedBytesByType));
	_unindexedBytes = 0;
	_dataTypesRepresented[0] = TYPE_BYTE_DATA;
	_dataTypesRepresented[1] = TYPE_UNINDEXED_BYTE_DATA;
	_dataTypesRepresented[2] = TYPE_CACHELET;
	
	notifyManagerInitialized(_cache->managers(), "TYPE_BYTE_DATA");

	Trc_SHR_BDMI_initialize_Exit();
}

/**
 * Returns the number of bytes required to construct this SH_ByteDataManager
 *
 * @return size in bytes
 */
UDATA
SH_ByteDataManagerImpl::getRequiredConstrBytes(void)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(SH_ByteDataManagerImpl);
	return reqBytes;
}

IDATA
SH_ByteDataManagerImpl::localInitializePools(J9VMThread* currentThread)
{
	Trc_SHR_BDMI_localInitializePools_Entry(currentThread);

	_linkedListImplPool = pool_new(sizeof(SH_ByteDataManagerImpl::BdLinkedListImpl),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(_portlib));
	if (!_linkedListImplPool) {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_BDMI_FAILED_CREATE_POOL);
		Trc_SHR_RMI_localInitializePools_ExitFailed(currentThread);
		return -1;
	}

	Trc_SHR_BDMI_localInitializePools_ExitOK(currentThread);
	return 0;
}

void
SH_ByteDataManagerImpl::localTearDownPools(J9VMThread* currentThread)
{
	Trc_SHR_BDMI_localTearDownPools_Entry(currentThread);

	if (_linkedListImplPool) {
		pool_kill(_linkedListImplPool);
		_linkedListImplPool = NULL;
	}

	Trc_SHR_BDMI_localTearDownPools_Exit(currentThread);
}

U_32
SH_ByteDataManagerImpl::getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes)
{
	return (U_32)((cacheSizeBytes / 100000) + 20);	
}

/**
 * Registers new cached Byte Data with the SH_ByteDataManager
 *
 * Called when new Byte Data has been identified in the cache
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 * @param[in] itemInCache The address of the item found in the cache
 * 
 * @return true if successful, false otherwise
 */
bool 
SH_ByteDataManagerImpl::storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet)
{

	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}
	Trc_SHR_BDMI_storeNew_Entry(currentThread, itemInCache);

	if (itemInCache->dataType == TYPE_BYTE_DATA) {
		ByteDataWrapper *bdw = (ByteDataWrapper*)ITEMDATA(itemInCache);
		const J9UTF8* key = (const J9UTF8*)BDWTOKEN(bdw);
		UDATA type = BDWTYPE(bdw);

		if (type <= J9SHR_DATA_TYPE_MAX) {
			_indexedBytesByType[type] += ITEMDATALEN(itemInCache);
			_numIndexedBytesByType[type] += 1;
		} else {
			/* Unidentified types */
			_indexedBytesByType[J9SHR_DATA_TYPE_UNKNOWN] += ITEMDATALEN(itemInCache);
			_numIndexedBytesByType[J9SHR_DATA_TYPE_UNKNOWN] += 1;
		}
		if (!hllTableUpdate(currentThread, _linkedListImplPool, key, itemInCache, cachelet)) {
			Trc_SHR_BDMI_storeNew_ExitFalse(currentThread);
			return false;
		}
	} else {
		_unindexedBytes += ITEMDATALEN(itemInCache);
	}

	Trc_SHR_BDMI_storeNew_ExitTrue(currentThread);
 	return true;
}

/**
 * For each key, datatype and jvmID there should be a maximum of one data entry.
 * This function returns that data entry if it exists, ignoring whether that data is private or not.
 * 
 * @param[in] currentThread  The current thread
 * @param[in] key  The UTF8 key against which the data was stored
 * @param[in] keylen  The length of the key
 * @param[in] dataType  The type of data required
 * @param[in] jvmID  The ID of the JVM that the data will belong to, or 0 if this does not apply
 * @param[out] dataLen  The length of the data entry returned
 * 
 * @return  The number of data elements found or -1 in the case of error
 * 
 * THREADING: Must be called with cache read mutex held
 */
ByteDataWrapper*
SH_ByteDataManagerImpl::findSingleEntry(J9VMThread* currentThread, const char* key, UDATA keylen, UDATA dataType, U_16 jvmID, UDATA* dataLen)
{
	BdLinkedListImpl *found, *walk;

	if (getState() != MANAGER_STATE_STARTED) {
		return NULL;
	}
	
	Trc_SHR_BDMI_findSingleEntry_Entry(currentThread, keylen, key, dataType, jvmID);

	if ((found = (BdLinkedListImpl*)hllTableLookup(currentThread, key, (U_16)keylen, true))) {
		walk = found;
		do {
			const ShcItem* item = walk->_item;
			ByteDataWrapper* wrapper = (ByteDataWrapper*)ITEMDATA(item);

			if (!_cache->isStale(item) && 
					(dataType == (UDATA)(wrapper->dataType)) && 
					(wrapper->privateOwnerID == jvmID)) {
				if (dataLen) {
					*dataLen = wrapper->dataLength;
				}
				Trc_SHR_BDMI_findSingleEntry_ExitFound(currentThread, wrapper);
				return wrapper;
			} 
			walk = (BdLinkedListImpl*)walk->_next;
		} while (walk != found);
	}
	
	Trc_SHR_BDMI_findSingleEntry_ExitNotFound(currentThread);
	return NULL;
}

/**
 * For all data entries stored under a given key, mark them stale, except for private entries owned by other JVMs
 *
 * @param[in] currentThread  The current thread 
 * @param[in] key  The UTF8 key against which the data was stored
 * @param[in] keylen  The length of the key
 * 
 * THREADING: Must be called with cache write mutex held
 */
void
SH_ByteDataManagerImpl::markAllStaleForKey(J9VMThread* currentThread, const char* key, UDATA keylen)
{
	BdLinkedListImpl *found, *walk;

	if (getState() != MANAGER_STATE_STARTED) {
		return;
	}

	Trc_SHR_BDMI_markAllStaleForKey_Entry(currentThread, keylen, key);

	if ((found = (BdLinkedListImpl*)hllTableLookup(currentThread, key, (U_16)keylen, true))) {
		U_16 jvmID = _cache->getCompositeCacheAPI()->getJVMID();
		
		walk = found;
		do {
			const ShcItem* item = walk->_item;
			ByteDataWrapper* wrapper = (ByteDataWrapper*)ITEMDATA(item);
			
			if (((wrapper->privateOwnerID == 0) || (wrapper->privateOwnerID == jvmID)) && !_cache->isStale(item)) {
				_cache->markItemStale(currentThread, item, false);
			}
			walk = (BdLinkedListImpl*)walk->_next;
		} while (walk != found);
	}

	Trc_SHR_BDMI_markAllStaleForKey_Exit(currentThread);
}

void
SH_ByteDataManagerImpl::setDescriptorFields(const ByteDataWrapper* wrapper, J9SharedDataDescriptor* descriptor)
{
	Trc_SHR_BDMI_setDescriptorFields_Event(wrapper, descriptor);

	descriptor->address = (U_8*)BDWDATA(wrapper);
	descriptor->length = wrapper->dataLength;
	descriptor->type = (UDATA)wrapper->dataType;
	descriptor->flags = 0;
	if (wrapper->privateOwnerID) {
		descriptor->flags |= J9SHRDATA_IS_PRIVATE;
		if (wrapper->privateOwnerID != _cache->getCompositeCacheAPI()->getJVMID()) {
			descriptor->flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
		}
	}
} 

/**
 * Retrieves data in the cache which has been stored against "key" which is a UTF8 string.
 * Populates descriptorPool with J9SharedDataDescriptors describing data elements. Returns the number of elements found.
 * The data returned can include private data of other JVMs or data of different types stored under the same key.
 * 
 * @param[in] currentThread  The current thread
 * @param[in] key  The UTF8 key against which the data was stored
 * @param[in] keylen  The length of the key
 * @param[in] limitDataType  Optional. If used, only data of the type constant specified is returned. If 0, all data stored under that key is returned
 * @param[in] includePrivate  If non-zero, will also add private data of other JVMs stored under that key into the array
 * @param[out] firstItem  If non-NULL, the fields are set to the first entry found
 * @param[out] descriptorPool  Populated with the results. Pool is not cleaned by this function. Can be NULL.
 * 
 * @return  The number of data elements found or -1 in the case of error
 *
 * THREADING: Must be called with cache read mutex held
 */
IDATA
SH_ByteDataManagerImpl::find(J9VMThread* currentThread, const char* key, UDATA keylen, UDATA limitDataType, UDATA includePrivateData, J9SharedDataDescriptor* firstItem, const J9Pool* descriptorPool)
{
	BdLinkedListImpl *found, *walk;
	IDATA resultCntr = 0;
	UDATA setOptItem = FALSE;

	if (getState() != MANAGER_STATE_STARTED) {
		return -1;
	}

	Trc_SHR_BDMI_find_Entry(currentThread, keylen, key, limitDataType, includePrivateData, firstItem, descriptorPool);

	if ((found = (BdLinkedListImpl*)hllTableLookup(currentThread, key, (U_16)keylen, true))) {
		walk = found;
		do {
			const ShcItem* item = walk->_item;
			ByteDataWrapper* wrapper = (ByteDataWrapper*)ITEMDATA(item);
			J9SharedDataDescriptor* newPoolEntry; 

			if (!_cache->isStale(item) &&
				(!limitDataType || (limitDataType == (UDATA)wrapper->dataType)) &&
				(includePrivateData || (!includePrivateData && (wrapper->privateOwnerID == 0)))) {
				if (descriptorPool && (newPoolEntry = (J9SharedDataDescriptor*)pool_newElement((J9Pool*)descriptorPool))) {
					setDescriptorFields(wrapper, newPoolEntry);
				}
				if (!setOptItem && firstItem) {
					setDescriptorFields(wrapper, firstItem);
					setOptItem = TRUE;
				}
				++resultCntr;
			} 
			walk = (BdLinkedListImpl*)walk->_next;
		} while (walk != found);
	}	

	Trc_SHR_BDMI_find_Exit(currentThread, resultCntr);
		
	return resultCntr;
}

/**
 * If a JVM has finished using a piece of private data and wants to allow another JVM to acquire it, the data entry must be released.
 * This is done automatically when a JVM shuts down, but can also be achieved explicitly using this function.
 * The data descriptor passed to this function must have been returned by findSharedData with the 
 * J9SHRDATA_IS_PRIVATE flag set and the J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM flag not set.
 * A JVM can only release data entries which it is currently has "in use"
 * If the function succeeds, the data is released and can be acquired by any JVM using acquirePrivateSharedData
 * 
 * @param[in] currentThread  The current thread
 * @param[in] data  A data descriptor that was obtained from calling findSharedData
 * 
 * @return 1 if the data was successfully released or 0 otherwise
 * THREADING: No need to get the cache write mutex as we only modify data private to this JVM 
 */
UDATA
SH_ByteDataManagerImpl::releasePrivateEntry(J9VMThread* currentThread, const J9SharedDataDescriptor* data)
{
	ByteDataWrapper* wrapper;

	Trc_SHR_BDMI_releasePrivateEntry_Entry(currentThread, data);

	if (!data || (data->flags & J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM) || !(data->flags & J9SHRDATA_IS_PRIVATE) || (data->flags & J9SHRDATA_USE_READWRITE)) {
		Trc_SHR_BDMI_releasePrivateEntry_ExitNoop(currentThread);
		return 0;
	}
	/* Note that we cannot find the ByteDataWrapper from readWrite data descriptor */
	wrapper = (ByteDataWrapper*)((BlockPtr)(data->address) - sizeof(ByteDataWrapper));
	
	if (wrapper->privateOwnerID == (U_16)_cache->getCompositeCacheAPI()->getJVMID()) {		/* Double-check */
		wrapper->inPrivateUse = 0;
		Trc_SHR_BDMI_releasePrivateEntry_ExitSuccess(currentThread, wrapper);
		return 1;
	}
	Trc_SHR_BDMI_releasePrivateEntry_ExitFailed(currentThread, wrapper);
	return 0;
}
 
/* THREADING: No need to get the cache write mutex as we only modify data private to this JVM */ 
UDATA
SH_ByteDataManagerImpl::htReleasePrivateEntry(void *entry, void *opaque)
{
	BdLinkedListImpl* found = *((BdLinkedListImpl**)entry);
	BdLinkedListImpl* walk = found;
	UDATA opaqueUDATA = (UDATA)opaque;

	do {
		ByteDataWrapper* wrapper = (ByteDataWrapper*)ITEMDATA(walk->_item);

		if (wrapper->privateOwnerID == (U_16)opaqueUDATA) {
			wrapper->inPrivateUse = 0;
		}
		walk = (BdLinkedListImpl*)walk->_next;
	} while (walk != found);

	return FALSE;
}

/**
 * Attempts to acquire a private data entry that used to be private to a JVM which has now shut down
 * The data descriptor passed to this function must have been returned by find(..) with the 
 * J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM flag set.
 * 
 * @param[in] currentThread  The current thread
 * @param[in] data  A data descriptor that was obtained from calling find(..)
 * 
 * @return 1 if the data was successfully made private or 0 otherwise
 * 
 * THREADING: Must be called with cache write mutex held
 */
UDATA
SH_ByteDataManagerImpl::acquirePrivateEntry(J9VMThread* currentThread, const J9SharedDataDescriptor* data)
{
	ByteDataWrapper* wrapper;

	Trc_SHR_BDMI_acquirePrivateEntry_Entry(currentThread, data);

	if (!data || !(data->flags & J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM) || (data->flags & J9SHRDATA_USE_READWRITE)) {
		Trc_SHR_BDMI_acquirePrivateEntry_ExitNoop(currentThread);
		return 0;
	}
	/* Note that we cannot find the ByteDataWrapper from readWrite data descriptor */
	wrapper = (ByteDataWrapper*)((BlockPtr)(data->address) - sizeof(ByteDataWrapper));
	/* Note that during shutdown, a JVM does not get the write mutex when releasing private data
	 * This does not cause any problems here though as we test for inPrivateUse before setting it */
	if ((wrapper->inPrivateUse == 0) && (wrapper->privateOwnerID != 0)) {
		wrapper->inPrivateUse = 1;
		wrapper->privateOwnerID = _cache->getCompositeCacheAPI()->getJVMID();
		Trc_SHR_BDMI_acquirePrivateEntry_ExitSuccess(currentThread, wrapper);
		return 1;
	}
	Trc_SHR_BDMI_acquirePrivateEntry_ExitFailed(currentThread, wrapper);
	return 0;
}

void 
SH_ByteDataManagerImpl::runExitCode(void)
{
	/* Release private data entries for this JVM */
	if (getState() != MANAGER_STATE_STARTED) {
		return;
	}
	hashTableForEachDo(_hashTable, htReleasePrivateEntry, (void*)((UDATA)_cache->getCompositeCacheAPI()->getJVMID()));
}

UDATA 
SH_ByteDataManagerImpl::getNumOfType(UDATA type)
{
	if (type > J9SHR_DATA_TYPE_MAX) {
		Trc_SHR_BDMI_getNumOfType_Error(type);
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _numIndexedBytesByType[type];
}

UDATA 
SH_ByteDataManagerImpl::getDataBytesForType(UDATA type)
{
	if (type > J9SHR_DATA_TYPE_MAX) {
		Trc_SHR_BDMI_getDataBytesForType_Error(type);
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _indexedBytesByType[type];
}

UDATA 
SH_ByteDataManagerImpl::getUnindexedDataBytes()
{
	return _unindexedBytes;
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
SH_ByteDataManagerImpl::createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	Trc_SHR_Assert_True(hints != NULL);

	/* hints->dataType should have been set by the caller */
	Trc_SHR_Assert_True(hints->dataType == _dataTypesRepresented[0]);
	
	return hllCollectHashes(vmthread, cachelet, hints);
}

/**
 * add a (_hashValue, cachelet) entry to the hash table
 * only called with hints of the right data type 
 *
 * each hint is a UDATA-length hash of a string
 */
IDATA
SH_ByteDataManagerImpl::primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA dataLength)
{
	UDATA* hashSlot = (UDATA*)hintsData;
	UDATA hintCount = 0;

	if ((dataLength == 0) || (hintsData == NULL)) {
		return 0;
	}

	hintCount = dataLength / sizeof(UDATA);
	while (hintCount-- > 0) {
		Trc_SHR_BDMI_primeHashtables_addingHint(vmthread, cachelet, *hashSlot);
		if (!_hints.addHint(vmthread, *hashSlot, cachelet)) {
			/* If we failed to finish priming the hints, just give up.
			 * Stuff should still work, just suboptimally.
			 */
			Trc_SHR_BDMI_primeHashtables_failedToPrimeHint(vmthread, this, cachelet, *hashSlot);
			break;
		}
		++hashSlot;
	}
	
	return 0;
}

#endif /* J9SHR_CACHELET_SUPPORT */
