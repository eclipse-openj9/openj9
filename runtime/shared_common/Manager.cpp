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

#include "Manager.hpp"
#include "Managers.hpp"
#include "SharedCache.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "CacheMap.hpp"
#include "AtomicSupport.hpp"

/**
 * Constructor
 */
SH_Manager::SH_Manager()
 : _hashTable(0),
   _cache(0),
   _htMutex(0),
   _htMutexName("hllTableMutex"),
   _portlib(0),
   _htEntries(0),
   _runtimeFlagsPtr(0),
   _verboseFlags(0),
   _state(0)
{
}

/**
 * Destructor
 */
SH_Manager::~SH_Manager()
{
	_state = 0;
}

/* Initialize a new HashLinkedListImpl */
void
SH_Manager::HashLinkedListImpl::initialize(const J9UTF8* key_, const ShcItem* item_, SH_CompositeCache* cachelet_, UDATA hashValue_)
{
	Trc_SHR_M_HashLinkedListImpl_initialize_Entry();

	_key = key_? (U_8*)J9UTF8_DATA(key_): NULL;
	_keySize = key_? (U_16)J9UTF8_LENGTH(key_): 0;
	char *end = getLastDollarSignOfLambdaClassName((const char *)_key, _keySize);
	if (NULL != end) {
		/* if it's a lambda class, we need the part of the class name before the index number so that classes that are the same but have different index numbers can get matched */
		_keySize = (U_16)(end - (char *)_key + 1);
	}
	_item = item_;
	/* Create the required circular link during initialization so
	 * it will be there when the entry is added to the hashtable under
	 * synchronization.
	 */
	_next = this;
	_hashValue = hashValue_;
	localInit(key_, item_, cachelet_, hashValue_);

	Trc_SHR_M_HashLinkedListImpl_initialize_Exit();
}

/** 
 * Static function to create a new link
 * 
 * @param[in] key Name of item to use as a hash key
 * @param[in] item ShcItem in cache
 * @param[in] allocationPool Pool to allocate new object from
 *
 * @return new HashLinkedListImpl created
 */
SH_Manager::HashLinkedListImpl* 
SH_Manager::createLink(const J9UTF8* key, const ShcItem* item, SH_CompositeCache* cachelet, UDATA hashPrimeValue, const J9Pool* allocationPool)
{
	HashLinkedListImpl *newLink, *memPtr;

	Trc_SHR_Assert_True(key != NULL);
	Trc_SHR_M_HashLinkedListImpl_createLink_Entry(J9UTF8_LENGTH(key), J9UTF8_DATA(key), item);
		
	if (!(memPtr = (HashLinkedListImpl*)pool_newElement((J9Pool*)allocationPool))) {
		Trc_SHR_M_HashLinkedListImpl_createLink_Exit1();
		return NULL;
	}
	newLink = localHLLNewInstance(memPtr);
	newLink->initialize(key, item, cachelet, hashPrimeValue);

	Trc_SHR_M_HashLinkedListImpl_createLink_Exit2(newLink);
	return newLink;
}

/* THREADING: Should only be called single-threaded */
void
SH_Manager::notifyManagerInitialized(SH_Managers* managers, const char* managerType)
{
	if (_state != MANAGER_STATE_SHUTDOWN) {
		_managerType = managerType;
		_state = MANAGER_STATE_INITIALIZED;
		managers->addManager(this);
	}
}

/* Destroy the hashtable and linked list entries */
/* THREADING: Must be protected by hashtable mutex */
void
SH_Manager::tearDownHashTable(J9VMThread* currentThread)
{
	Trc_SHR_M_tearDownHashTable_Entry(currentThread, _managerType);

	localTearDownPools(currentThread);
	if (_hashTable) {
		hashTableFree(_hashTable);
		_hashTable = NULL;
	}

	Trc_SHR_M_tearDownHashTable_Exit(currentThread);
}

/**
 * Should be called only after _htEntries has been initialized
 * THREADING: Must be protected by hashtable mutex
 * @retval 0 success
 * @retval -1 error
 */
IDATA
SH_Manager::initializeHashTable(J9VMThread* currentThread)
{
	IDATA returnVal = 0;
	PORT_ACCESS_FROM_PORT(_portlib);

	Trc_SHR_M_initializeHashTable_Entry(currentThread, _managerType);

	_hashTableGetNumItemsDoFn = (J9HashTableDoFn)SH_Manager::countItemsInList;
	_hashTable = localHashTableCreate(currentThread, _htEntries);
	if (!_hashTable) {
		M_ERR_TRACE(J9NLS_SHRC_M_FAILED_CREATE_HASHTABLE);
		returnVal = -1;
		goto _exit;
	}

	if (localInitializePools(currentThread) == -1) {
		M_ERR_TRACE(J9NLS_SHRC_M_FAILED_CREATE_POOLS);
		tearDownHashTable(currentThread);
		returnVal = -1;
		goto _exit;
	}

_exit :
	Trc_SHR_M_initializeHashTable_Exit(currentThread, returnVal);
	return returnVal;
}

/**
 * Start the initialized SH_Manager
 * 
 * @see Manager.hpp
 * @note should be called only after initialize
 * @param[in] currentThread The current thread
 * @param[in] runtimeFlags_ A pointer to the runtimeFlags in use
 * @param[in] verboseFlags_ The verbose flags in use
 * @param[in] cacheSizeBytes The cache size, in bytes
 *
 * @return 0 for success, -1 for failure, otherwise if it is the wrong state, it returns the current state.
 */
/* THREADING: Can be called multi-threaded. Only one thread should win the compareAndSwap and run startup. */
IDATA
SH_Manager::startup(J9VMThread* currentThread, U_64* runtimeFlags_, UDATA verboseFlags_, UDATA cacheSizeBytes)
{
	UDATA actualState;

	/* Only valid state for entry to this function is MANAGER_STATE_INITIALIZED */
	if (_state != MANAGER_STATE_INITIALIZED) {
		return _state;
	}
	
	Trc_SHR_M_startup_Entry(currentThread, _managerType);

	/* It's possible (albeit unlikely) that multiple threads could have got here. Resolve the race to start up. */
	if ((actualState = VM_AtomicSupport::lockCompareExchange(&(_state), MANAGER_STATE_INITIALIZED, MANAGER_STATE_STARTING)) != MANAGER_STATE_INITIALIZED) {
		Trc_SHR_M_startup_Exit5(currentThread, actualState);
		return actualState;
	}

	_runtimeFlagsPtr = runtimeFlags_;
	_verboseFlags = verboseFlags_;
	_htEntries = getHashTableEntriesFromCacheSize(cacheSizeBytes);

	if (omrthread_monitor_init(&_htMutex, 0)) {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_M_FAILED_CREATE_MUTEX);
		Trc_SHR_M_startup_Exit2(currentThread);
		goto _startupFailed;
	}

	if (_cache->enterLocalMutex(currentThread, _htMutex, "_htMutex", "startup")==0) {
		if (initializeHashTable(currentThread) == -1) {
			Trc_SHR_M_startup_Exit1(currentThread);
			goto _startupFailedWithMutex;
		}
		if (localPostStartup(currentThread) == -1) {
			Trc_SHR_M_startup_Exit3(currentThread);
			goto _startupFailedWithMutex;
		}
		_cache->exitLocalMutex(currentThread, _htMutex, "_htMutex", "startup");
	}

	_state = MANAGER_STATE_STARTED;
	Trc_SHR_M_startup_Exit4(currentThread);
	return 0;

_startupFailedWithMutex :
	_cache->exitLocalMutex(currentThread, _htMutex, "_htMutex", "startup");
_startupFailed :
	cleanup(currentThread);
	_state = MANAGER_STATE_INITIALIZED;
	return -1;	
}

/**
 * Force shutdown of the manager. Manager cannot be restated after shutdown.
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 */
/* THREADING: Must only be called single-threaded and not concurrent with any other functions */
void
SH_Manager::shutDown(J9VMThread* currentThread)
{
	Trc_SHR_M_shutDown_Entry(currentThread, _managerType);
	cleanup(currentThread);
	_state = MANAGER_STATE_SHUTDOWN;
	Trc_SHR_M_shutDown_Entry(currentThread, _managerType);
}
/**
 * Clean-up the SH_Manager on shutdown
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 */
/* THREADING: Must only be called single-threaded and not concurrent with any other functions */
void
SH_Manager::cleanup(J9VMThread* currentThread)
{
	Trc_SHR_M_cleanup_Entry(currentThread, _managerType);

	if ((_state == MANAGER_STATE_STARTED) || (_state == MANAGER_STATE_STARTING)) {
		if (!_htMutex || (_cache->enterLocalMutex(currentThread, _htMutex, "_htMutex", "cleanup")==0)) {
			tearDownHashTable(currentThread);
			localPostCleanup(currentThread);
			_cache->exitLocalMutex(currentThread, _htMutex, "_htMutex", "cleanup");
		}

		if (_htMutex) {
			omrthread_monitor_destroy(_htMutex);
			_htMutex = NULL;
		}
	}

	_state = MANAGER_STATE_INITIALIZED;
	Trc_SHR_M_cleanup_Exit(currentThread);
}

/**
 * Clears the hashtable data in the SH_Manager
 *
 * @see Manager.hpp
 * @warning Should always be called with local or cache write mutex held
 * @param[in] currentThread The current thread
 *
 * @return 0 for success, -1 for failure
 */
/* THREADING: Must only be called single-threaded and not concurrent with any other functions */
IDATA 
SH_Manager::reset(J9VMThread* currentThread)
{
	IDATA returnVal = 0;
	
	Trc_SHR_M_reset_Entry(currentThread, _managerType);

	if (_state == MANAGER_STATE_STARTED) {
		if (_cache->enterLocalMutex(currentThread, _htMutex, "_htMutex", "reset")==0) {
			tearDownHashTable(currentThread);
			if (initializeHashTable(currentThread) == -1) {
				returnVal = -1;
			}
			_cache->exitLocalMutex(currentThread, _htMutex, "_htMutex", "reset");
		}
	}

	Trc_SHR_M_reset_Exit(currentThread, returnVal);

	return returnVal;
}

/**
 * Returns the state of the manager
 *
 * @see Manager.hpp
 */
U_8
SH_Manager::getState()
{
	return (U_8)_state;
} 

/* Stored new entry in circular linked list */
/* THREADING: Completely thread-safe */
SH_Manager::LinkedListImpl* 
SH_Manager::LinkedListImpl::link(LinkedListImpl* addToList, LinkedListImpl* newLink)
{
	Trc_SHR_M_LinkListImpl_link_Entry(newLink, addToList);

	if (!addToList || (addToList==newLink)) {
		/* no action required here since newLink is initialized with a circular _next link */
		Trc_SHR_M_LinkListImpl_link_Exit1(newLink);
		return newLink;
	}
	newLink->_next = addToList->_next;
	VM_AtomicSupport::writeBarrier();
	addToList->_next = newLink;

	Trc_SHR_M_LinkListImpl_link_Exit2(newLink, addToList);
	return newLink;
}

/* Creates a new link and adds it to the list specified in addToList
 * Returns new link if successfully added, otherwise NULL.
 * Sets addToList as the list to link the new link to 
 */
SH_Manager::HashLinkedListImpl* 
SH_Manager::hllTableAdd(J9VMThread* currentThread, const J9Pool* linkPool, const J9UTF8* key, const ShcItem* item, UDATA hashPrimeValue, SH_CompositeCache* cachelet, HashLinkedListImpl** addToList)
{
	HashLinkedListImpl* newItem;
	IDATA retryCount = 0;
	PORT_ACCESS_FROM_PORT(_portlib);
	
	Trc_SHR_Assert_True(key != NULL);
	Trc_SHR_M_hllTableAdd_Entry(currentThread, J9UTF8_LENGTH(key), J9UTF8_DATA(key), item);
	
	if ((key != NULL) || (item != NULL)) {
		/* Just to be certain */
		hashPrimeValue = 0;
	}
	if (!(newItem = createLink(key, item, cachelet, hashPrimeValue, linkPool))) {
		M_ERR_TRACE(J9NLS_SHRC_M_FAILED_CREATE_LINKEDLISTITEM);
		Trc_SHR_M_hllTableAdd_Exit1(currentThread);
		return NULL;
	}

	while (retryCount < MONITOR_ENTER_RETRY_TIMES) {
		if (_cache->enterLocalMutex(currentThread, _htMutex, "hllTableMutex", "hllTableAdd")==0) {
			HashLinkedListImpl** rc;

			/* This call will not actually add the new item if there is already an entry of the same key in the hashtable. Instead, the value returned
				by hashTableAdd is passed back as the addToList parameter. The value returned by this function should then be linked to addToList */
			if ((rc = (HashLinkedListImpl**)hashTableAdd(_hashTable, &newItem))==NULL) {
				Trc_SHR_M_hllTableAdd_Exception1(currentThread);
				M_ERR_TRACE(J9NLS_SHRC_M_FAILED_CREATE_HASHTABLE_ENTRY);
				newItem = NULL;		/* Return null, but must exit mutex first */
			} else {
				Trc_SHR_M_hllTableAdd_HashtableAdd(currentThread, rc);
				*addToList = *rc;
			}

			_cache->exitLocalMutex(currentThread, _htMutex, "hllTableMutex", "hllTableAdd");
			break;
		}
		retryCount++;
	}
	if (retryCount==MONITOR_ENTER_RETRY_TIMES) {
		M_ERR_TRACE(J9NLS_SHRC_M_FAILED_ENTER_HTMUTEX);
		Trc_SHR_M_hllTableAdd_Exit3(currentThread, retryCount);
		return NULL;
	}
	Trc_SHR_M_hllTableAdd_Exit4(currentThread, newItem);
	return newItem;
}

/* Hashtable lookup function. Returns value if found, otherwise returns NULL 
 * TODO: allowCacheletStart is a way to prevent recursive calls to hllTableLookup by reuniteOrphan.
 * However, it's possible for an orphan to be in a different cachelet to a ROMClass pointer, so need to work this out.
 */
SH_Manager::HashLinkedListImpl* 
SH_Manager::hllTableLookup(J9VMThread* currentThread, const char* name, U_16 nameLen, bool allowCacheletStartup)
{
	HashLinkedListImpl* result = NULL;

	Trc_SHR_M_hllTableLookup_Entry(currentThread, nameLen, name);

	if (lockHashTable(currentThread, "hllTableLookup")) {
		result = hllTableLookupHelper(currentThread, (U_8*)name, nameLen, 0, NULL);
		unlockHashTable(currentThread, "hllTableLookup");
	} else {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_M_FAILED_ENTER_HTMUTEX);
		Trc_SHR_M_hllTableLookup_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}

	Trc_SHR_M_hllTableLookup_Exit2(currentThread, result);
	return result;
}

/**
 * Construct a search entry and do hashTableFind()
 */
SH_Manager::HashLinkedListImpl*
SH_Manager::hllTableLookupHelper(J9VMThread* currentThread, U_8* key, U_16 keySize, UDATA hashValue, SH_CompositeCache* cachelet)
{
	HashLinkedListImpl dummy;
	HashLinkedListImpl* dummyPtr = &dummy;
	HashLinkedListImpl** p_result;

	dummyPtr->_key = key;
	dummyPtr->_keySize = keySize;
	dummyPtr->_hashValue = hashValue;

	p_result = (HashLinkedListImpl**)hashTableFind(_hashTable, (void*)&dummyPtr);

	return (p_result? *p_result : NULL);
}

/* Creates a new link, adds it to the hashtable and links it to the correct list.
 * Returns the newly created link or NULL if there was an error */
SH_Manager::HashLinkedListImpl* 
SH_Manager::hllTableUpdate(J9VMThread* currentThread, const J9Pool* linkPool, const J9UTF8* key, const ShcItem* item, SH_CompositeCache* cachelet)
{
	HashLinkedListImpl* newLink = NULL;
	HashLinkedListImpl* addToList = NULL;
	HashLinkedListImpl* returnVal = NULL;
	
	Trc_SHR_M_hllTableUpdate_Entry(currentThread, J9UTF8_LENGTH(key), J9UTF8_DATA(key), item);

	/**
	 * @bug Incorrect synchronization of hashtable. Another thread could walk the linked list 
	 * as we're modifying it. Unlikely to occur because most callers require the VM class segment mutex.
	 */
	if (!(newLink = hllTableAdd(currentThread, linkPool, key, item, 0, cachelet, &addToList))) {
		Trc_SHR_M_hllTableUpdate_Exit1(currentThread);
		return NULL;
	}

	returnVal = (HashLinkedListImpl*)SH_Manager::LinkedListImpl::link(addToList, newLink);
	Trc_SHR_M_hllTableUpdate_Exit2(currentThread, returnVal);
	return returnVal;
}

/**
 * @param currentThread - the currentThread or NULL when called to collect javacore data
 */
bool
SH_Manager::lockHashTable(J9VMThread* currentThread, const char* funcName)
{
	IDATA retryCount = MONITOR_ENTER_RETRY_TIMES;
	
	while (retryCount-- > 0) {
		/* WARNING - currentThread can be NULL */
		if (_cache->enterLocalMutex(currentThread, _htMutex, _htMutexName, funcName) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * @param currentThread - the currentThread or NULL when called to collect javacore data
 */
void
SH_Manager::unlockHashTable(J9VMThread* currentThread, const char* funcName)
{
	/* WARNING - currentThread can be NULL */
	_cache->exitLocalMutex(currentThread, _htMutex, _htMutexName, funcName);
}

UDATA
SH_Manager::generateHash(J9InternalVMFunctions* internalFunctionTable, U_8* key, U_16 keySize)
{
	UDATA hashValue = 0;

	/* Longer class names will have very common package names, so optimize these out */
	if (keySize < 16) {
		hashValue = internalFunctionTable->computeHashForUTF8(key, keySize);
	} else if (keySize < 24) {
		hashValue = internalFunctionTable->computeHashForUTF8(key+10, keySize-10);
	} else {
		hashValue = internalFunctionTable->computeHashForUTF8(key+18, keySize-18);
	}
	return hashValue;
}

/* Hash function for hashtable */
UDATA
SH_Manager::hllHashFn(void* item, void *userData)
{
	HashLinkedListImpl* itemValue = *((HashLinkedListImpl**)item);
	J9InternalVMFunctions* internalFunctionTable = (J9InternalVMFunctions*)userData;
	UDATA hashValue = 0;

	Trc_SHR_M_hllHashFn_Entry(item);
	if (itemValue->_hashValue) {
		hashValue = itemValue->_hashValue;
	} else { 
		hashValue = generateHash(internalFunctionTable, itemValue->_key, itemValue->_keySize);
		itemValue->_hashValue = hashValue;
	}
	Trc_SHR_M_hllHashFn_Exit(hashValue);
	return hashValue;
}

/**
 * HashEqual function for hashtable
 * @param[in] left a match candidate from the hashtable
 * @param[in] right search key 
 */
UDATA
SH_Manager::hllHashEqualFn(void* left, void* right, void *userData)
{
	HashLinkedListImpl* leftItem = *((HashLinkedListImpl**)left);
	HashLinkedListImpl* rightItem = *((HashLinkedListImpl**)right);
	UDATA result;

	Trc_SHR_M_hllHashEqualFn_Entry(leftItem, rightItem);

	/* PERFORMANCE: Compare key size first as this is the most likely cause for exit */
	if (leftItem->_keySize != rightItem->_keySize) {
		Trc_SHR_M_hllHashEqualFn_Exit2();
		return 0;
	}
	if (leftItem->_key == NULL || rightItem->_key == NULL) {
		Trc_SHR_M_hllHashEqualFn_Exit1();
		return 0;
	}
	result = J9UTF8_DATA_EQUALS(leftItem->_key, leftItem->_keySize, rightItem->_key, rightItem->_keySize);
	Trc_SHR_M_hllHashEqualFn_Exit3(result);
	return result;
}

/**
 * @param currentThread - the currentThread or NULL when called to collect javacore data
 */
void 
SH_Manager::getNumItems(J9VMThread* currentThread, UDATA* nonStaleItems, UDATA* staleItems)
{
	if (_hashTable && _hashTableGetNumItemsDoFn) {
		CountData countData(_cache);

		/* WARNING - currentThread can be NULL */
		if (lockHashTable(currentThread, "getNumItems")) {
			hashTableForEachDo(_hashTable, _hashTableGetNumItemsDoFn, &countData);
			unlockHashTable(currentThread, "getNumItems");
		}
		*nonStaleItems = countData._nonStaleItems;
		*staleItems = countData._staleItems;
	} else {
		*nonStaleItems = *staleItems = 0;
	}
}

/**
 * Default hashtable item counter.
 * Default implementation works only for managers with one entry per key 
 * To force a different implementation, replace _hashTableGetNumItemsDoFn
 */
UDATA
SH_Manager::countItemsInList(void* entry, void* opaque)
{
	SH_Manager::LinkedListImpl* node = *(SH_Manager::LinkedListImpl**)entry;
	SH_Manager::CountData* countData = (SH_Manager::CountData*)opaque;
	
	if (countData->_cache->isStale(node->_item)) {
		++(countData->_staleItems);
	} else {
		++(countData->_nonStaleItems);
	}
	return 0;
}

bool
SH_Manager::isDataTypeRepresended(UDATA type)
{
	for (int i=0; i<MAX_TYPES_PER_MANAGER; i+=1) {
		if (_dataTypesRepresented[i] == type) {
			return true;
		}
	}
	return false;
}

