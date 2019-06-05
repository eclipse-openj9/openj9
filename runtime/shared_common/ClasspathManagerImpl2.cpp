/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include "ClasspathManagerImpl2.hpp"
#include "CacheMap.hpp"
#include "j9shrnls.h"
#include "ut_j9shr.h"
#include "vmzipcachehook.h"
#include <string.h>

#define CPLLI_LAST_ITEM_TAG 0x10000
#define IDENTIFIED_START_SIZE 20
#define ID_NOT_SET 0x10000
#define ID_NOT_FOUND 0x20000
#define JAR_LOCKED 2

#define CPM_ZIP_OPEN 1							/* Indicates zip/jar is currently open */
#define CPM_ZIP_FORCE_CHECK_TIMESTAMP 2			/* When a jar/zip is first opened, we should force a check of its timestamp */
#define CPM_ZIP_ONLY_TIMESTAMP_ON_INIT 4		/* Indicates that the timestamp of this container should only be checked once during initialization to support OSGi requirements */
#define CPM_ZIP_CHECKED_INIT_TIMESTAMP 8		/* Indicates that the initialization timestamp has been done */

SH_ClasspathManagerImpl2::SH_ClasspathManagerImpl2()
 : _tsm(0),
   _identifiedMutex(0),
   _linkedListImplPool(0),
   _identifiedClasspaths(0),
   _classpathCount(0), 
   _urlCount(0), 
   _tokenCount(0),
   _allCacheletsStarted(false)
{
   _htMutexName = "cpeTableMutex";
}

SH_ClasspathManagerImpl2::~SH_ClasspathManagerImpl2()
{
}

/** 
 * Create a new CpLinkedListImpl
 * 
 * @param[in] CPEIndex_ The index of the classpath entry that this link represents
 * @param[in] item ShcItem in cache (will be a ClasspathWrapper)
 * @param[in] memForConstructor Memory to build instance into
 *
 * @return new CpLinkedListImpl
 */
SH_ClasspathManagerImpl2::CpLinkedListImpl*
SH_ClasspathManagerImpl2::CpLinkedListImpl::newInstance(I_16 CPEIndex_, const ShcItem* item_, SH_CompositeCache* cachelet_, CpLinkedListImpl* memForConstructor)
{
	CpLinkedListImpl* newCLLI = (CpLinkedListImpl*)memForConstructor;

	Trc_SHR_CMI_CpLinkedListImpl_newInstance_Entry(CPEIndex_, item_);

	new(newCLLI) CpLinkedListImpl();
	newCLLI->initialize(CPEIndex_, item_, cachelet_);

	Trc_SHR_CMI_CpLinkedListImpl_newInstance_Exit(newCLLI);

	return newCLLI;
}

/* Initialize a new CpLinkedListImpl */
void
SH_ClasspathManagerImpl2::CpLinkedListImpl::initialize(I_16 CPEIndex_, const ShcItem* item_, SH_CompositeCache* cachelet_)
{
	Trc_SHR_CMI_CpLinkedListImpl_initialize_Entry();

	_CPEIndex = CPEIndex_;
	_item = item_;
	/* Create the required circular link during initialization so
	 * it will be there when the entry is added to the hashtable under
	 * synchronization.
	 */
	_next = this;
#if defined(J9SHR_CACHELET_SUPPORT)
	_cachelet = cachelet_;
#endif

	Trc_SHR_CMI_CpLinkedListImpl_initialize_Exit();
}

/**
 * Static function that creates a new link and links it to the list given. Overloads the function in SH_Manager.
 *
 * @param[in] addToList The list to link the new entry to
 * @param[in] CPEIndex The index of the classpath entry that this link represents
 * @param[in] item The cache item that this link represents
 * @param[in] doTag Tag the link
 * @param[in] allocationPool The pool used for allocating new links
 *
 * @return A new CpLinkedListImpl or NULL if this fails
 */
SH_ClasspathManagerImpl2::CpLinkedListImpl* 
SH_ClasspathManagerImpl2::CpLinkedListImpl::link(CpLinkedListImpl* addToList, I_16 CPEIndex, const ShcItem* item, bool doTag, SH_CompositeCache* cachelet, J9Pool* allocationPool) 
{	
	CpLinkedListImpl *newLink, *memPtr, *result;

	Trc_SHR_CMI_CpLinkedListImpl_link_Entry(addToList, CPEIndex, item, doTag);

	Trc_SHR_CMI_CpLinkedListImpl_link_PoolNew(allocationPool);

	if (!(memPtr = (CpLinkedListImpl*)pool_newElement(allocationPool))) {
		Trc_SHR_CMI_CpLinkedListImpl_link_Exit1();
		return NULL;
	}
	newLink = CpLinkedListImpl::newInstance(CPEIndex, item, cachelet, memPtr);

	if (doTag) {
		tag(newLink);
	}
	result = (CpLinkedListImpl*)SH_Manager::LinkedListImpl::link(addToList, newLink);
	Trc_SHR_CMI_CpLinkedListImpl_link_Exit2(result);
	return result;
}

/**
 * Searches for a CpLinkedListImpl in the list which belongs to a classpath which is an exact match for the classpath given at the index given
 *
 * Should only ever return one possible entry from the list. Will not return any classpaths which are part-stale.
 * 
 * @param[in] currentThread The current thread
 * @param[in] item The classpath that the link should belong to
 * @param[in] index The index in the classpath that the link should be stored at
 *
 * @return A CpLinkedListImpl that is at the correct index in a matching classpath or NULL if one is not found
 */
SH_ClasspathManagerImpl2::CpLinkedListImpl* 
SH_ClasspathManagerImpl2::CpLinkedListImpl::forCacheItem(J9VMThread* currentThread, ClasspathItem* item, UDATA index) 
{
	CpLinkedListImpl* walk = this;

	Trc_SHR_CMI_CpLinkedListImpl_forCacheItem_Entry(currentThread, index, item);

	do {
		UDATA testIndex = getCPEIndex(walk);
		ClasspathWrapper* testItem = (ClasspathWrapper*)ITEMDATA(walk->_item);

		Trc_SHR_CMI_CpLinkedListImpl_forCacheItem_DoTest(currentThread, walk, testIndex, testItem->staleFromIndex);
		if ((testIndex==index) && 
			(testItem->staleFromIndex == CPW_NOT_STALE) && 
			(ClasspathItem::compare(currentThread->javaVM->internalVMFunctions, ((ClasspathItem*)CPWDATA(testItem)), item))) {		/* Note: Compares whole classpath */

			Trc_SHR_CMI_CpLinkedListImpl_forCacheItem_Exit1(currentThread, walk);
			return walk;
		}
		walk = (CpLinkedListImpl*)walk->_next;
	} while (walk!=this);

	Trc_SHR_CMI_CpLinkedListImpl_forCacheItem_Exit2(currentThread);
	return NULL;
}

/**
 * Static function which tags a link. Used to indicate that a link is the last entry in a classpath.
 *
 * @param[in] item The item to tag
 */
void
SH_ClasspathManagerImpl2::CpLinkedListImpl::tag(CpLinkedListImpl* item) 
{
	Trc_SHR_CMI_CpLinkedListImpl_tag(item);
	item->_CPEIndex |= CPLLI_LAST_ITEM_TAG;
}

/**
 * Static function which returns the CPEIndex field of a link
 * 
 * @note The CPEIndex of a link should never be accessed directly as it needs to be marked to remove the tag
 * @param[in] item The item to get the CPEIndex for
 *
 * @return The CPEIndex value of the link
 */
I_16
SH_ClasspathManagerImpl2::CpLinkedListImpl::getCPEIndex(CpLinkedListImpl* item)
{
	I_16 result = (item->_CPEIndex & ~CPLLI_LAST_ITEM_TAG);
	Trc_SHR_CMI_CpLinkedListImpl_getCPEIndex(result, item);
	return result;
}

SH_ClasspathManagerImpl2::CpLinkedListHdr::CpLinkedListHdr(const char* key_, U_16 keySize_, U_8 isToken_, CpLinkedListImpl* listItem)
 : _isToken(isToken_),
   _flags(0),
   _keySize(keySize_),
   _key(key_),
   _list(listItem)
{
}

SH_ClasspathManagerImpl2::CpLinkedListHdr::~CpLinkedListHdr()
{
}

/** 
 * Create a new SH_ClasspathManagerImpl2
 * 
 * @param[in] vm A Java VM
 * @param[in] cache_ The shared cache that this SH_ClasspathManager will manage
 * @param[in] tsm_ The timestamp manager used by the cache
 * @param[in] memForConstructor Memory to build instance into
 *
 * @return new SH_ClasspathManagerImpl2
 */
SH_ClasspathManagerImpl2*
SH_ClasspathManagerImpl2::newInstance(J9JavaVM* vm, SH_SharedCache* cache_, SH_TimestampManager* tsm_, SH_ClasspathManagerImpl2* memForConstructor)
{
	SH_ClasspathManagerImpl2* newCMI = (SH_ClasspathManagerImpl2*)memForConstructor;

	Trc_SHR_CMI_newInstance_Entry(vm, cache_, tsm_);

	new(newCMI) SH_ClasspathManagerImpl2();
	newCMI->initialize(vm, cache_, tsm_, ((BlockPtr)memForConstructor + sizeof(SH_ClasspathManagerImpl2)));

	Trc_SHR_CMI_newInstance_Exit(newCMI);

	return newCMI;
}

/* Initialize the SH_ClasspathManagerImpl2 */
void
SH_ClasspathManagerImpl2::initialize(J9JavaVM* vm, SH_SharedCache* cache_, SH_TimestampManager* tsm_, BlockPtr memForConstructor)
{
	Trc_SHR_CMI_initialize_Entry();

	_cache = cache_;
	_tsm = tsm_;
	_portlib = vm->portLibrary;
	_htMutex = NULL;
	_identifiedMutex = NULL;
	_dataTypesRepresented[0] = TYPE_CLASSPATH;
	_dataTypesRepresented[1] = _dataTypesRepresented[2] = 0;

	notifyManagerInitialized(_cache->managers(), "TYPE_CLASSPATH");

	Trc_SHR_CMI_initialize_Exit();
}

U_32
SH_ClasspathManagerImpl2::getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes)
{
	return (U_32)((cacheSizeBytes / 50000) + 20);
}

/* Should be called only after _htEntries, _runtimeFlagsPtr and _portlib have been initialized */
IDATA
SH_ClasspathManagerImpl2::localInitializePools(J9VMThread* currentThread)
{
	IDATA returnVal = 0;
	PORT_ACCESS_FROM_PORT(_portlib);

	Trc_SHR_CMI_localInitializePools_Entry(currentThread);

	_linkedListImplPool = pool_new(sizeof(SH_ClasspathManagerImpl2::CpLinkedListImpl),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(_portlib));
	if (!_linkedListImplPool) {
		M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_CREATE_LINKEDLISTIMPL);
		returnVal = -1;
		goto _exit;
	}

	if (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING) {
		_identifiedClasspaths = initializeIdentifiedClasspathArray(_portlib, IDENTIFIED_START_SIZE, NULL, 0, 0);
		if (!_identifiedClasspaths) {
			M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_CREATE_IDCPARRAY);
			returnVal = -1;
			goto _exit;
		}
	}

_exit:
	Trc_SHR_CMI_localInitializePools_Exit(currentThread, returnVal);
	return returnVal;
}

/* Destroy the hashtable and linked list entries */
void
SH_ClasspathManagerImpl2::localTearDownPools(J9VMThread* currentThread)
{
	Trc_SHR_CMI_localTearDownPools_Entry(currentThread);

	if (_linkedListImplPool) {
		pool_kill(_linkedListImplPool);
		_linkedListImplPool = NULL;
	}
	if (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING) {
		if (_identifiedClasspaths) {
			freeIdentifiedClasspathArray(_portlib, _identifiedClasspaths);
			_identifiedClasspaths = NULL;
		}
	}
	Trc_SHR_CMI_localTearDownPools_Exit(currentThread);
}

/**
 * Start the initialized SH_ClasspathManager
 * 
 * @see Manager.hpp
 * @note should be called only after initialize
 * @param[in] currentThread The current thread
 *
 * @return 0 for success, -1 for failure
 */
IDATA
SH_ClasspathManagerImpl2::localPostStartup(J9VMThread* currentThread)
{
	Trc_SHR_CMI_localPostStartup_Entry(currentThread);

	if (omrthread_monitor_init(&_identifiedMutex, 0)) {
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_CREATE_IDMUTEX);
		Trc_SHR_CMI_localPostStartup_Exit1(currentThread);
		return -1;
	}

	Trc_SHR_CMI_localPostStartup_Exit2(currentThread);
	return 0;
}

void
SH_ClasspathManagerImpl2::localPostCleanup(J9VMThread* currentThread)
{
	Trc_SHR_CMI_localPostCleanup_Entry(currentThread);

	if (_identifiedMutex) {
		omrthread_monitor_destroy(_identifiedMutex);
		_identifiedMutex = NULL;
	}

	Trc_SHR_CMI_localPostCleanup_Exit(currentThread);
}

/**
 * Returns the number of bytes required to construct this SH_ROMClassManager
 *
 * @return size in bytes
 */
UDATA
SH_ClasspathManagerImpl2::getRequiredConstrBytes(void)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(SH_ClasspathManagerImpl2);
	return reqBytes;
}

/* Make decisions about which entries to actually timestamp
 * Returns 1 if item is stampable and time has changed, 0 if it has not changed, JAR_LOCKED if it was not checked and -1 for error
 */
IDATA
SH_ClasspathManagerImpl2::hasTimestampChanged(J9VMThread* currentThread, ClasspathEntryItem* itemToCheck, CpLinkedListHdr* knownLLH, bool doTryLockJar)
{
	if (getState() != MANAGER_STATE_STARTED) {
		return 0;
	}
	Trc_SHR_CMI_hasTimestampChanged_Entry(currentThread, itemToCheck, doTryLockJar);
	
	/* Only check JARs/Jimage as we check specific classfile timestamps in directories */
	if ((PROTO_JAR == itemToCheck->protocol)
		|| (PROTO_JIMAGE == itemToCheck->protocol)
	) {
		U_16 pathLen = 0;
		const char* itemPath = NULL;
		I_64 newTS;
		CpLinkedListHdr* header;

		if (knownLLH) {
			header = knownLLH;		/* Optimization: Don't need to get mutex and do lookup if header is provided */
		} else {
			itemPath = itemToCheck->getLocation(&pathLen);
			header = cpeTableLookup(currentThread, itemPath, pathLen, 0);			/* 0 = isToken. Certainly won't be a token. */
			if (header == NULL) {
				/* CMVC 131285: This is not expected to happen, but has been seen with concurrent writing 
				 * when called by localValidate_checkAndTimestampManually at the point at which the cache becomes full */ 
				Trc_SHR_CMI_hasTimestampChanged_ExitError(currentThread);
				return -1;
			}
		}
		if ((header->_flags == CPM_ZIP_OPEN) || /* Implies !CPM_ZIP_ONLY_TIMESTAMP_ON_INIT and !CPM_ZIP_FORCE_CHECK_TIMESTAMP */
				(header->_flags & CPM_ZIP_CHECKED_INIT_TIMESTAMP)) {
			Trc_SHR_CMI_hasTimestampChanged_ExitLocked(currentThread, header);
			return JAR_LOCKED;
		}

		newTS = _tsm->checkCPEITimeStamp(currentThread, itemToCheck);

		/* If only timestamping the container once, mark it so that it won't be checked again */
		if (header->_flags & CPM_ZIP_ONLY_TIMESTAMP_ON_INIT) {
			header->_flags &= ~CPM_ZIP_ONLY_TIMESTAMP_ON_INIT;
			header->_flags |= CPM_ZIP_CHECKED_INIT_TIMESTAMP;
		/* If the container has just been opened, force a single check each time, unless CPM_ZIP_ONLY_TIMESTAMP_ON_INIT */
		} else if (header->_flags & CPM_ZIP_FORCE_CHECK_TIMESTAMP) {
			header->_flags &= ~CPM_ZIP_FORCE_CHECK_TIMESTAMP;
		}
		
		if ((newTS == TIMESTAMP_DOES_NOT_EXIST) || (newTS == TIMESTAMP_DISAPPEARED)) {
			UDATA result = (newTS == TIMESTAMP_DISAPPEARED);		/* If resource was there and has gone, timestamp has changed */

			Trc_SHR_CMI_hasTimestampChanged_ExitDoesNotExist(currentThread, result);
			return result;
		} else {
			UDATA result = (newTS != TIMESTAMP_UNCHANGED);

			Trc_SHR_CMI_hasTimestampChanged_Exit(currentThread, newTS, result);
			return result;
		}
	} else {
		Trc_SHR_CMI_hasTimestampChanged_NotJar(currentThread);
	}

	Trc_SHR_CMI_hasTimestampChanged_ExitFalse(currentThread);
	return 0;
}

/* Creates a new hashtable. Pre-req: portLibrary must be initialized */
J9HashTable* 
SH_ClasspathManagerImpl2::localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries)
{
	J9HashTable* returnVal;

	Trc_SHR_CMI_localHashTableCreate_Entry(currentThread, initialEntries);
	returnVal = hashTableNew(OMRPORT_FROM_J9PORT(_portlib), J9_GET_CALLSITE(), initialEntries, sizeof(SH_ClasspathManagerImpl2::CpLinkedListHdr), sizeof(char *), 0, J9MEM_CATEGORY_CLASSES, SH_ClasspathManagerImpl2::cpeHashFn, SH_ClasspathManagerImpl2::cpeHashEqualFn, NULL, (void*)currentThread->javaVM->internalVMFunctions);
	Trc_SHR_CMI_localHashTableCreate_Exit(currentThread, returnVal);
	return returnVal;
}

/* Hash function for hashtable */
UDATA
SH_ClasspathManagerImpl2::cpeHashFn(void* item, void *userData)
{
	CpLinkedListHdr* itemValue = (CpLinkedListHdr*)item;
	J9InternalVMFunctions* internalFunctionTable = (J9InternalVMFunctions*)userData;
	UDATA hashValue;

	Trc_SHR_CMI_cpeHashFn_Entry(item);
	hashValue = (internalFunctionTable->computeHashForUTF8((U_8*)itemValue->_key, itemValue->_keySize) + itemValue->_isToken);
	Trc_SHR_CMI_cpeHashFn_Exit(hashValue);
	return hashValue;
}

/**
 * HashEqual function for hashtable
 * 
 * @param[in] left a match candidate from the hashtable
 * @param[in] right search key 
 * 
 * @retval 0 not equal
 * @retval non-zero equal 
 */
UDATA
SH_ClasspathManagerImpl2::cpeHashEqualFn(void* left, void* right, void *userData)
{
	CpLinkedListHdr* leftItem = (CpLinkedListHdr*)left;
	CpLinkedListHdr* rightItem = (CpLinkedListHdr*)right;
	UDATA result;

	Trc_SHR_CMI_cpeHashEqualFn_Entry(leftItem, rightItem);

	/* PERFORMANCE: Hot function. This is likely to mostly be the first point of failure. */
	if (leftItem->_keySize != rightItem->_keySize) {
		Trc_SHR_CMI_cpeHashEqualFn_Exit2();
		return 0;
	}
	if (leftItem->_key==NULL || rightItem->_key==NULL) {
		Trc_SHR_CMI_cpeHashEqualFn_Exit1();
		return 0;
	}
	if (leftItem->_isToken != rightItem->_isToken) {
		Trc_SHR_CMI_cpeHashEqualFn_Exit3();
		return 0;
	}
	result = J9UTF8_DATA_EQUALS((U_8*)leftItem->_key, leftItem->_keySize, (U_8*)rightItem->_key, rightItem->_keySize);
	Trc_SHR_CMI_cpeHashEqualFn_Exit4(result);
	return result;
}

SH_ClasspathManagerImpl2::CpLinkedListHdr* 
SH_ClasspathManagerImpl2::cpeTableAddHeader(J9VMThread* currentThread, const char* name, U_16 nameLen, CpLinkedListImpl* newItem, U_8 isToken)
{
	CpLinkedListHdr* rc = NULL;
	IDATA retryCount = 0;

	CpLinkedListHdr header(name, nameLen, isToken, newItem);

	while (retryCount < MONITOR_ENTER_RETRY_TIMES) {
		if (_cache->enterLocalMutex(currentThread, _htMutex, "cpeTableMutex", "cpeTableAddHeader")==0) {
			Trc_SHR_CMI_cpeTableAdd_HashtableAdd(currentThread);
			rc = (CpLinkedListHdr*)hashTableAdd(_hashTable, &header);
			if (rc == NULL) {
				PORT_ACCESS_FROM_PORT(_portlib);
				M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_CREATE_HASHTABLE_ENTRY);
			}
			_cache->exitLocalMutex(currentThread, _htMutex, "cpeTableMutex", "cpeTableAddHeader");
			if (rc == NULL) {
				return NULL;
			}
			break;
		}
		retryCount++;
	}
	return rc;
}

/* Creates a new header and a new linked list entry, which is added to the header. The new header is then added to the hashtable.
 * CPEIndex and doTag are parameters stored by the link and name and isToken are parameters used by the header. 
 * Returns a pointer to the new link, or NULL if there was an error */
SH_ClasspathManagerImpl2::CpLinkedListImpl* 
SH_ClasspathManagerImpl2::cpeTableAdd(J9VMThread* currentThread, const char* name, U_16 nameLen, I_16 CPEIndex, const ShcItem* item, U_8 isToken, bool doTag, SH_CompositeCache* cachelet)
{
	CpLinkedListImpl* newItem = NULL;

	Trc_SHR_CMI_cpeTableAdd_Entry(currentThread, nameLen, name, CPEIndex, item, isToken, doTag);

	/* Note that it is legitimate for item to be NULL if we're being notified about a container opening which we have no cache data for yet */
	if (item != NULL) {
		if (!(newItem = CpLinkedListImpl::link(NULL, CPEIndex, item, doTag, cachelet, _linkedListImplPool))) {
			PORT_ACCESS_FROM_PORT(_portlib);
			M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_CREATE_LINKEDLISTITEM);
			Trc_SHR_CMI_cpeTableAdd_Exit1(currentThread);
			return NULL;
		}
	}

	if (cpeTableAddHeader(currentThread, name, nameLen, newItem, isToken) == NULL) {
		Trc_SHR_CMI_cpeTableAdd_Exit3(currentThread);
		return NULL;
	}
	
	Trc_SHR_CMI_cpeTableAdd_Exit5(currentThread, newItem);
	return newItem;
}

/* Finds a header entry in the hashtable, based on the data given. Will return NULL if the header is not found or if there is an error. */
SH_ClasspathManagerImpl2::CpLinkedListHdr* 
SH_ClasspathManagerImpl2::cpeTableLookup(J9VMThread* currentThread, const char* key, U_16 keySize, U_8 isToken)
{
	CpLinkedListHdr dummy(key, keySize, isToken, NULL);
	CpLinkedListHdr* returnVal = NULL;

	Trc_SHR_CMI_cpeTableLookup_Entry(currentThread, keySize, key, isToken);

	if (lockHashTable(currentThread, "cpeTableLookup")) {

#if defined(J9SHR_CACHELET_SUPPORT)
		if (_isRunningNested) {
			UDATA hint = cpeHashFn(&dummy, currentThread->javaVM->internalVMFunctions);
			
			if (startupHintCachelets(currentThread, hint) == -1) {
				goto exit_lockFailed;
			}
		}
#endif
		returnVal = cpeTableLookupHelper(currentThread, &dummy);
		unlockHashTable(currentThread, "cpeTableLookup");
	} else {
#if defined(J9SHR_CACHELET_SUPPORT)
exit_lockFailed:
#endif
		PORT_ACCESS_FROM_PORT(_portlib);
		M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_ENTER_CPEMUTEX);
		Trc_SHR_CMI_cpeTableLookup_Exit1(currentThread, MONITOR_ENTER_RETRY_TIMES);
		return NULL;
	}

	Trc_SHR_CMI_cpeTableLookup_Exit2(currentThread, returnVal);
	return returnVal;
}

/* Finds a header entry in the hashtable, based on the data given. Will return NULL if the header is not found or if there is an error. */
SH_ClasspathManagerImpl2::CpLinkedListHdr* 
SH_ClasspathManagerImpl2::cpeTableLookupHelper(J9VMThread* currentThread, const char* key, U_16 keySize, U_8 isToken)
{
	CpLinkedListHdr dummy(key, keySize, isToken, NULL); 
	CpLinkedListHdr* returnVal = NULL;

	returnVal = cpeTableLookupHelper(currentThread, &dummy);

	return returnVal;
}

SH_ClasspathManagerImpl2::CpLinkedListHdr* 
SH_ClasspathManagerImpl2::cpeTableLookupHelper(J9VMThread* currentThread, CpLinkedListHdr* searchItem)
{
	CpLinkedListHdr* returnVal = NULL;

	returnVal = (CpLinkedListHdr*)hashTableFind(_hashTable, (void*)searchItem);
	Trc_SHR_CMI_cpeTableLookup_HashtableFind(currentThread, returnVal);

	return returnVal;
}

/* Updates the hashtable with new data. Creates a new link and will also create a new header if required.
 * ASSUMPTION: There cannot be 2 URLs with the same name, but different protocols. This would not make sense.
 * However, there can be a token and a URL with the same name. These are treated as entirely different entries. 
 * Returns the new link created, or NULL if there is an error. */
SH_ClasspathManagerImpl2::CpLinkedListImpl* 
SH_ClasspathManagerImpl2::cpeTableUpdate(J9VMThread* currentThread, const char* name, U_16 nameLen, 
		I_16 CPEIndex, const ShcItem* item, U_8 isToken, bool doTag,
		SH_CompositeCache* cachelet)
{
	CpLinkedListHdr* addToList = NULL;
	CpLinkedListImpl* returnVal = NULL;

	Trc_SHR_CMI_cpeTableUpdate_Entry(currentThread, nameLen, name, CPEIndex, item, isToken);

	addToList = cpeTableLookupHelper(currentThread, name, nameLen, isToken);

	if (addToList==NULL) {
		returnVal = cpeTableAdd(currentThread, name, nameLen, CPEIndex, item, isToken, doTag, cachelet);
	} else {
		returnVal = CpLinkedListImpl::link(addToList->_list, CPEIndex, item, doTag, cachelet, _linkedListImplPool);
		if (addToList->_list == NULL) {
			addToList->_list = returnVal;
		}
	}

	Trc_SHR_CMI_cpeTableUpdate_Exit(currentThread, returnVal);

	return returnVal;
}

/* This function entirely resets the classpath cache. The mechanism currently uses runtimeFlags
 * so that the JNI natives can tell the cache that a change has potentially invalidated the classpath cache.
 * Assumes that identifiedClasspaths has been initialized and exists.
 * THREADING: This function must only be called with _identifiedMutex already held 
 * Assumes that identifiedClasspaths has been initialized and exists.
 */
UDATA
SH_ClasspathManagerImpl2::testForClasspathReset(J9VMThread* currentThread)
{
	J9JavaVM* vm = currentThread->javaVM;

	if (getState() != MANAGER_STATE_STARTED) {
		return 1;
	}
	Trc_SHR_CMI_testForClasspathReset_Entry(currentThread);

	if ((*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_DO_RESET_CLASSPATH_CACHE) && (_identifiedClasspaths != NULL)) {
		UDATA currentSize = _identifiedClasspaths->size;

		*_runtimeFlagsPtr &= ~J9SHR_RUNTIMEFLAG_DO_RESET_CLASSPATH_CACHE;
		freeIdentifiedClasspathArray(vm->portLibrary, _identifiedClasspaths);
		_identifiedClasspaths = NULL;
		_identifiedClasspaths = initializeIdentifiedClasspathArray(vm->portLibrary, currentSize, NULL, 0, 0);

		/* If re-allocation fails, don't bomb out. Just disable classpath cacheing to ensure we don't crash. */
		if (!_identifiedClasspaths) {
			*_runtimeFlagsPtr &= ~J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING;
		}

		Trc_SHR_CMI_testForClasspathReset_ExitReset(currentThread);
		return 0;
	}

	Trc_SHR_CMI_testForClasspathReset_ExitDoNothing(currentThread);
	return 1;
}

/* Private function used exclusively by update(...)
 * Looks for identified classpath in identified array. Takes a local classpath. Returns a ClasspathWrapper or null.
 * Note that since the modified context does not change for a single JVM, it is irrelevant to classpath cacheing.
 */
ClasspathWrapper*
SH_ClasspathManagerImpl2::localUpdate_FindIdentified(J9VMThread* currentThread, ClasspathItem* localCP) 
{
	ClasspathWrapper* found = NULL;

	Trc_SHR_CMI_localUpdate_FindIdentified_Entry(currentThread, localCP);

	if (_cache->enterLocalMutex(currentThread, _identifiedMutex, "identifiedMutex", "localUpdate_FindIdentified")==0) {
		if (testForClasspathReset(currentThread)) {
			found = (ClasspathWrapper*)getIdentifiedClasspath(currentThread, _identifiedClasspaths, localCP->getHelperID(), localCP->getItemsAdded(), NULL, 0, NULL);
		}
		_cache->exitLocalMutex(currentThread, _identifiedMutex, "identifiedMutex", "localUpdate_FindIdentified");
	} /* If monitor_enter fails, just do manual match */

	Trc_SHR_CMI_localUpdate_FindIdentified_Exit(currentThread, found);
	return found;
}

/* Private function used exclusively by both update(...) and validate(...)
 * Associates a classpath in the cache with a classpath from a local classloader by adding it to the identified array.
 * Note that since the modified context does not change for a single JVM, it is irrelevant to classpath cacheing.
 * Returns 0 if ok or -1 for failure.
 */
IDATA
SH_ClasspathManagerImpl2::local_StoreIdentified(J9VMThread* currentThread, ClasspathItem* localCP, ClasspathWrapper* cpInCache) 
{
	Trc_SHR_CMI_local_StoreIdentified_Entry(currentThread, localCP, cpInCache);

	if (_cache->enterLocalMutex(currentThread, _identifiedMutex, "identifiedMutex", "local_StoreIdentified")==0) {
		if (testForClasspathReset(currentThread)) {
			setIdentifiedClasspath(currentThread, &_identifiedClasspaths, localCP->getHelperID(), localCP->getItemsAdded(), NULL, 0, cpInCache);
		}
		_cache->exitLocalMutex(currentThread, _identifiedMutex, "identifiedMutex", "local_StoreIdentified");
		if ((_identifiedClasspaths == NULL) || (_identifiedClasspaths->size == 0)) {
			/* Now that we can choose to make the identified array NULL if we exceed a number of classpaths, this is no longer an error condition */
			/* CMI_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_CREATE_IDCPARRAY); */
			*_runtimeFlagsPtr &= ~J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING;
			Trc_SHR_CMI_local_StoreIdentified_Exit1(currentThread);
			return -1;
		}
	}

	Trc_SHR_CMI_local_StoreIdentified_Exit2(currentThread);
	return 0;
}

/* Private function used exclusively by update(...) 
 * Attempts to match a local classpath with a classpath in the cache.
 * Returns a ClasspathWrapper if a match is found, or null.
 */
ClasspathWrapper*
SH_ClasspathManagerImpl2::localUpdate_CheckManually(J9VMThread* currentThread, ClasspathItem* cp, CpLinkedListHdr** knownLLH) 
{
	CpLinkedListHdr* known = NULL;
	ClasspathWrapper* found = NULL;
	U_16 pathLen = 0;
	const char* path = NULL;

	Trc_SHR_CMI_localUpdate_CheckManually_Entry(currentThread, cp);

	path = cp->itemAt(0)->getLocation(&pathLen);
	known = cpeTableLookup(currentThread, path, pathLen, (cp->getType()==CP_TYPE_TOKEN));
	if (known && known->_list) {
		CpLinkedListImpl* cpInCache = NULL;

		Trc_SHR_CMI_localUpdate_CheckManually_FoundKnown(currentThread, known);
		cpInCache = (known->_list)->forCacheItem(currentThread, cp, 0);
		if (cpInCache) {
			/* 	forCacheItem only returns an identical classpath, so we have now found our classpath */
			found = (ClasspathWrapper*)ITEMDATA(cpInCache->_item);
		}
		*knownLLH = known;
	}	

	Trc_SHR_CMI_localUpdate_CheckManually_Exit(currentThread, found);
	return found;
}	

/**
 * Main entry point for a store call. Looks for a classpath in the cache which exactly matches the caller's classpath
 * and validates that every entry upto and including the entry at cpeIndex has not changed.
 *
 * May detect a stale classpath and mark the classpath stale, so any caller of this function should be aware
 * that the result of calling it could be a stale mark.
 * Only returns a classpath if it is an exact match for the caller's classpath and if it is proved to be non-stale.
 *
 * @see ClasspathManager.hpp
 * @warning Must always be called with write mutex held
 * param[in] currentThread The current thread
 * param[in] localCP The ClasspathItem from the caller classloader
 * param[in] cpeIndex The index in localCP that that ROMClass was loaded from
 * param[out] foundCP A ClasspathItem in the cache that exactly matches localCP and has been proved non-stale
 *
 * @return 0 for success, non-zero for failure
 */
IDATA
SH_ClasspathManagerImpl2::update(J9VMThread* currentThread, ClasspathItem* localCP, I_16 cpeIndex, ClasspathWrapper** foundCP) 
{
	I_16 i;
	ClasspathWrapper* found = NULL;
	bool foundIdentified = false;
	CpLinkedListHdr* knownLLH = NULL;

	if (getState() != MANAGER_STATE_STARTED) {
		return -1;
	}
	Trc_SHR_CMI_Update_Entry(currentThread, localCP, cpeIndex);

	/* Search local cache of known "identified" classpaths */
	if (localCP->getType()==CP_TYPE_CLASSPATH && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING)) {
		found = localUpdate_FindIdentified(currentThread, localCP);
	}

	/* If not found an "identified" classpath, do a full search */
	if (found) {
		foundIdentified = true;
	} else {
		found = localUpdate_CheckManually(currentThread, localCP, &knownLLH);
	}

	/* If classpath was found, walk classpath upto and including cpeIndex, checking timestamps of timestampable entries.
		Do not check whole classpath as this will get very expensive... also, post-cpeIndex staleness doesn't matter */
	if (found && (localCP->getType()!=CP_TYPE_TOKEN) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS)) {
		for (i=0; i<=cpeIndex; i++) {
			ClasspathEntryItem* foundItem = ((ClasspathItem*)CPWDATA(found))->itemAt(i);
			IDATA rc = hasTimestampChanged(currentThread, foundItem, knownLLH, true);

			if ((rc == 1) && (_cache->markStale(currentThread, foundItem, true))) {
				Trc_SHR_CMI_Update_Exit3(currentThread);
				PORT_ACCESS_FROM_PORT(_portlib);
				M_ERR_TRACE(J9NLS_SHRC_CMI_FAILED_MARKSTALE_UPDATE);
				return -1;
			} else if (rc == -1) {
				Trc_SHR_CMI_Update_Exit4(currentThread);
				return -1;
			}
		}
	}

	/* Classpath could have become stale above! */
	if (!found || isStale(found)) {
		*foundCP = NULL;			/* Tells caller to create new classpath */
	} else {
		*foundCP = found;
	}

	/* If we found the classpath in the cache, it wasn't stale and it hasn't already been identified, add it to the identified array */
	if ((*foundCP && !foundIdentified) && (localCP->getType()==CP_TYPE_CLASSPATH) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING)) {
		if (local_StoreIdentified(currentThread, localCP, *foundCP)==-1) {
			Trc_SHR_CMI_Update_Exit1(currentThread);
			return -1;		/* Error already reported */
		}
	}

	Trc_SHR_CMI_Update_Exit2(currentThread, (*foundCP));
	return 0;		/* Indicates that there were no errors */
}

/* Private function used exclusively by validate(...)
 * Looks for identified classpath in identified array.
 * Note that since the modified context does not change for a single JVM, it is irrelevant to classpath cacheing.
 * Returns helperID of identified classpath if one is found. Else returns ID_NOT_FOUND.
 */
IDATA
SH_ClasspathManagerImpl2::localValidate_FindIdentified(J9VMThread* currentThread, ClasspathWrapper* cpInCache, IDATA walkFromID) 
{
	IDATA identifiedID = ID_NOT_FOUND;

	Trc_SHR_CMI_localValidate_FindIdentified_Entry(currentThread, cpInCache);

	if (_cache->enterLocalMutex(currentThread, _identifiedMutex, "identifiedMutex", "localValidate_FindIdentified")==0) {
		if (testForClasspathReset(currentThread)) {
			identifiedID = getIDForIdentified(_portlib, _identifiedClasspaths, cpInCache, walkFromID);
		}
		_cache->exitLocalMutex(currentThread, _identifiedMutex, "identifiedMutex", "localValidate_FindIdentified");
	} /* If monitor_enter fails, just do manual match */

	if (identifiedID==ID_NOT_FOUND) {
		Trc_SHR_CMI_localValidate_FindIdentified_ExitNotFound(currentThread);
	} else {
		Trc_SHR_CMI_localValidate_FindIdentified_ExitFound(currentThread, identifiedID);
	}
	return identifiedID;
}

/* Private function used exclusively by validate(...)
 * Attempts to "validate" a classpath in the cache against a classpath from a caller classloader.
 * cpInCache is the cache classpath and testCPIndex is the index of the test ROMClass in that classpath.
 * May try to "identify" a classpath if it seems worthwhile. Sets *addToIdentified=true if it successfully matches the classpath with a classpath in the cache.
 * As part of the validation, timestamps appropriate entries and sets *staleItem if it finds a stale item.
 * Returns the index in the caller's classpath (compareTo) that the ROMClass is found at, or -1 if not found.
 */
I_16 
SH_ClasspathManagerImpl2::localValidate_CheckAndTimestampManually(J9VMThread* currentThread, ClasspathWrapper* cpInCache, I_16 testCPIndex, ClasspathItem* compareTo, IDATA foundIdentified, bool* addToIdentified, ClasspathEntryItem** staleItem) 
{
	ClasspathItem* testCPI = NULL;
	ClasspathEntryItem* testItem = NULL;
	I_16 indexInCompare = 0;
	I_16 walkToIndex;
	bool doTryIdentify;

	Trc_SHR_CMI_localValidate_CheckAndTimestampManually_Entry(currentThread, cpInCache, compareTo, testCPIndex);

	testCPI = (ClasspathItem*)CPWDATA(cpInCache);
	testItem = testCPI->itemAt(testCPIndex);			/* testItem is classpath entry the ROMClass was stored against */
	*addToIdentified = false;
	*staleItem = NULL;

	/* ClasspathEntryItem of stored romclass should exist in compareTo
	 * If so, then if the index of the testItem in compareTo is greater than that in the cpInCache,
	 * then there is clearly more items to the left of it in the cpInCache, so we know to fail
	 */
	indexInCompare = compareTo->find(currentThread->javaVM->internalVMFunctions, testItem);
	if ((indexInCompare < 0) || (indexInCompare > testCPIndex)) {
		if (!(foundIdentified & (ID_NOT_FOUND | ID_NOT_SET)) && (compareTo->getType()==CP_TYPE_CLASSPATH) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING)) {
			Trc_SHR_CMI_localValidate_CheckAndTimestampManually_RegisterFailed(currentThread);
			registerFailedMatch(currentThread, _identifiedClasspaths, compareTo->getHelperID(), foundIdentified, testCPIndex, NULL, 0);
		}
		Trc_SHR_CMI_localValidate_CheckAndTimestampManually_Exit1(currentThread, indexInCompare);
		return -1;
	}

	/* If token, this is all we need to prove success. Will only ever have one entry. */
	if (compareTo->getType()==CP_TYPE_TOKEN) {
		if (indexInCompare==0) {
			Trc_SHR_CMI_localValidate_CheckAndTimestampManually_ExitTokenFound(currentThread);
			return 0;
		} else {
			Trc_SHR_CMI_localValidate_CheckAndTimestampManually_ExitTokenNotFound(currentThread); 
			return -1;
		}
	}

	/* If this is an as-yet unidentified classpath, is it worth us trying to identify it? 
	 * Yes if classpaths have same hashcode and number of items, if testItem is at same index in both and if classpath is not part-stale 
	 * Note: Hashcode is no guarantee of equality.
	 */
	if (compareTo->getType()==CP_TYPE_CLASSPATH 
			&& (testCPI->getType()==CP_TYPE_CLASSPATH)
			&& (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING) 
			&& (indexInCompare == testCPIndex) 
			&& (compareTo->getItemsAdded() == testCPI->getItemsAdded())
			&& (compareTo->getHashCode() == testCPI->getHashCode()) && !isStale(cpInCache)) {
		/* Try to identify - walk whole classpath to see if it is the same */
		doTryIdentify = true;
		walkToIndex = (compareTo->getItemsAdded()-1);
	} else {
		/* Not going to identify - walk only those ClasspathEntryItems to left of indexInCompare */
		doTryIdentify = false;
		walkToIndex = indexInCompare;
	}
	Trc_SHR_CMI_localValidate_CheckAndTimestampManually_DoTryIdentifySet(currentThread, doTryIdentify, walkToIndex);

	/* Walk the test classpath, checking consistency with the real one */
	for (I_16 i=walkToIndex; i>=0; i--) {
		ClasspathEntryItem* walk = compareTo->itemAt(i);
		I_16 indexInTestCP = 0;
		U_16 pathLen = 0;
		const char* cpeiPath = walk->getPath(&pathLen);

		Trc_SHR_CMI_localValidate_CheckAndTimestampManually_TestEntry(currentThread, pathLen, cpeiPath, i);
		if (doTryIdentify) {
			/* We expect to find walk at index i in cpInCache */
			indexInTestCP = testCPI->find(currentThread->javaVM->internalVMFunctions, walk, i);
			if (indexInTestCP != i) {
				/* We failed to identify the classpath. Revert to normal "match" mode. */
				doTryIdentify = false;
			}
			Trc_SHR_CMI_localValidate_CheckAndTimestampManually_DoTryIdentify(currentThread, indexInTestCP);
		} 
		/* Not "else" because if doTryIdentify = false above, need to re-find indexInTestCP */
		if (!doTryIdentify) {
			if (i > indexInCompare) {
				/* Once we have failed to identify, skip classpath entries which don't matter */
				Trc_SHR_CMI_localValidate_CheckAndTimestampManually_SkippingEntry(currentThread, i);
				continue;
			}
			/* Searches the cpInCache upto testCPIndex for testItem */
			indexInTestCP = testCPI->find(currentThread->javaVM->internalVMFunctions, walk, testCPIndex);
		}
		if ((indexInTestCP < 0)) {
			Trc_SHR_CMI_localValidate_CheckAndTimestampManually_Exit2(currentThread);
			return -1;
		} else if (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS) {
			/* Check the timestamp of classpath entry IN THE CACHE */
			ClasspathEntryItem* foundItem = testCPI->itemAt(indexInTestCP);
			IDATA rc = hasTimestampChanged(currentThread, foundItem, NULL, (compareTo->getHelperID()==0));
			/* THREADING: If we find that the timestamp has changed between us finding the class
			 * and running this check, store the stale item (markStale happens later) and return -1
			 * which will cause the FIND to fail. 
			 */
			/* doTryLockJar = false for non-bootstrap because we are trying to load from the cache, not from disk, so JARs may not yet be locked by the classloader.
				Bootstrap loader is always helperID==0 and it DOES lock its jars, so this is safe. */
			if (rc == 1) {
				Trc_SHR_CMI_localValidate_CheckAndTimestampManually_DetectedStaleCPEI(currentThread, foundItem);
				if (!(*staleItem)) {
					*staleItem = foundItem;
				}
				/* Ensure we don't try to store this classpath as "identified" if we've just discovered it is stale! */ 
				doTryIdentify = false;
				/* Only fail the find if the stale entry is indexInCompare or earlier */
				if (i <= indexInCompare) {
					Trc_SHR_CMI_localValidate_CheckAndTimestampManually_Exit3(currentThread);
					return -1;
				}
			} else if (rc == -1) {
				Trc_SHR_CMI_localValidate_CheckAndTimestampManually_Exit4(currentThread);
				return -1;
			}
		}
	}
	*addToIdentified = doTryIdentify;
	Trc_SHR_CMI_localValidate_CheckAndTimestampManually_ExitSuccess(currentThread, indexInCompare, (*addToIdentified), (*staleItem));
	return indexInCompare;
}

/**
 * Main entry point for a find call. Compares the classpath of a found ROMClass to that of the caller classloader
 * to determine whether the ROMClass should be returned by the find call.
 * 
 * Note that the classpath of the foundROMClass and that of the caller classloader do not have to be an exact match.
 * We know when this is called that the classpath entry for the romclass we have found cannot be stale,
 * however, since we only hold the read mutex, it could theoretically become stale at any time.
 * Note also that the function will also return 0 if the entries in compareTo are not confirmed upto and including foundAtIndex
 *
 * @see ClasspathManager.hpp
 * @note Should be called with the read mutex held
 * @param[in] currentThread The current thread
 * @param[in] foundROMClass The ROMClass that has been found in the cache
 * @param[in] compareTo The caller's classpath. This MUST be a local classpath, not a cached classpath
 * @param[in] confirmedEntries The entries in the classpath which have been confirmed. Only applies to TYPE_CLASSPATH, otherwise should be -1.
 * @param[out] foundAtIndex Set to the index in the caller's classpath that foundROMClass is found at. If not found, it will be -1.
 * @param[out] staleItem Set if a stale classpath entry is found. It is then the caller's responsibility to do a stale mark as we only own the read mutex.
 * 
 * @return 	1 only if the ROMClass found is the same ROMClass which would be found on the filesystem using the given classpath.
 * @return 0 if classpaths do not match, if a stale entry is detected or if the classpath has not been confirmed to the required index
 * @return -1 in the case of an error.
 */
IDATA 
SH_ClasspathManagerImpl2::validate(J9VMThread* currentThread, ROMClassWrapper* foundROMClass, ClasspathItem* compareTo, IDATA confirmedEntries, I_16* foundAtIndex, ClasspathEntryItem** staleItem) 
{
	ClasspathWrapper* testCP = NULL;
	I_16 testCPIndex = -2;
	ClasspathItem* testCPI = NULL;
	I_16 localFoundAtIndex = -1;
	bool addToIdentified = false;
	IDATA result = -2;
	IDATA foundIdentified = ID_NOT_SET;

	Trc_SHR_CMI_validate_Entry(currentThread, foundROMClass, compareTo, confirmedEntries);
	
	if (getState() != MANAGER_STATE_STARTED) {
		/* trace exception is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */
		Trc_SHR_CMI_validate_ManagerInNotStartedState_Exception(currentThread, foundROMClass, compareTo, confirmedEntries);
		Trc_SHR_CMI_validate_ExitError(currentThread);		
		return -1;
	}
	
	testCP = (ClasspathWrapper*)RCWCLASSPATH(foundROMClass);
	testCPIndex = foundROMClass->cpeIndex;
	testCPI = (ClasspathItem*)CPWDATA(testCP);

	/* Neither of these scenarios should happen. This means that compareTo is not local */
	if (compareTo==testCPI) {
		/* trace exception is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */
		Trc_SHR_CMI_validate_ExitSameInCache_Exception(currentThread, foundROMClass, compareTo, confirmedEntries);
		Trc_SHR_CMI_validate_ExitSameInCache(currentThread);
		return 1;
	} else 
	if (compareTo->isInCache()) {
		/* trace exception is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */
		Trc_SHR_CMI_validate_ExitNotSameInCache_Exception(currentThread, foundROMClass, compareTo, confirmedEntries);
		Trc_SHR_CMI_validate_ExitNotSameInCache(currentThread);
		return -1;
	}

	/* See if we have already identified testCP - is it the same classpath as our caller classloader's? */
	if ((compareTo->getType()==CP_TYPE_CLASSPATH) && (testCPI->getType()==CP_TYPE_CLASSPATH) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING)) {
		IDATA compareToID = compareTo->getHelperID();
		IDATA prevMatch = ID_NOT_FOUND;

		foundIdentified = -1;

		/* Multiple ClassLoaders could have the same classpath, so search until the matching helperID is found */
		do {
			if ((foundIdentified = localValidate_FindIdentified(currentThread, testCP, foundIdentified+1)) != ID_NOT_FOUND) {
				prevMatch = foundIdentified;
			}
		} while ((foundIdentified != ID_NOT_FOUND) && (foundIdentified != compareToID));

		/* This means we have a positive match - testCP is the same classpath as that of the caller classloader */
		if (foundIdentified==compareToID) {
			localFoundAtIndex = testCPIndex;
		} else {
			/* At this point, foundIdentified will always be ID_NOT_FOUND. This means that testCP is not exactly the same classpath as the caller
			 * classloader's, but we have identified it and it could still be a valid match. Test to see if we have tried this match before and failed it. */
			foundIdentified = prevMatch;
			if (foundIdentified != ID_NOT_FOUND) {
				if (hasMatchFailedBefore(currentThread, _identifiedClasspaths, compareToID, foundIdentified, testCPIndex, NULL, 0)) {
					/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */
					Trc_SHR_CMI_validate_ExitFailedBefore_Event(currentThread, foundROMClass, compareTo, confirmedEntries);
					Trc_SHR_CMI_validate_ExitFailedBefore(currentThread);
					goto _done;
				}
			}
		}
	}

	/* At this point, we still have work to do:
	 * foundIdentified==compareToID: We have either found that testCP and compareTo match exactly - we need to simply timestamp check.
	 * foundIdentified!=NULL: TestCP is a different classpath to the caller's which needs to be tested to see if it is a valid match.
	 * foundIdentified==NULL: TestCP has not been identified. It may be identified later, or there may not be a helper with that classpath in the VM.
	*/
	if (localFoundAtIndex == -1) {
		/* We didn't find an exact classpath match. Walk the classpaths and check for validity. Note that the timestamp check is incorporated for efficiency. */
		localFoundAtIndex = localValidate_CheckAndTimestampManually(currentThread, testCP, testCPIndex, compareTo, foundIdentified, &addToIdentified, staleItem);
	} else {
		/* If we found an identified classpath, check timestamps on the entries from jarsLockedToIndex upto and including localFoundAtIndex */
		if ((compareTo->getType()!=CP_TYPE_TOKEN) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS)) {
			I_16 jarsLockedToIndex = compareTo->getJarsLockedToIndex();
			I_16 i = jarsLockedToIndex>=0 ? jarsLockedToIndex : 0;
			
			for (; i<=localFoundAtIndex; i++) {
				ClasspathEntryItem* foundItem = testCPI->itemAt(i);
				IDATA tsChangedVal;

				/* doTryLockJar = false for non-bootstrap because we are trying to load from the cache, not from disk, so JARs may not yet be locked by the classloader.
					Bootstrap loader is always helperID==0 and it DOES lock its jars, so this is safe. */
				tsChangedVal = hasTimestampChanged(currentThread, foundItem, NULL, (compareTo->getHelperID()==0));
				if (tsChangedVal==JAR_LOCKED && i==jarsLockedToIndex+1) {
					jarsLockedToIndex = i;
				} else if (tsChangedVal==1) {
					*staleItem = foundItem;
					if (_cache->getCompositeCacheAPI()->isRunningReadOnly()) {
						/* If we're running in read-only mode, disable sharing for this local classpath as it's too expensive to keep checking */
						compareTo->flags |= MARKED_STALE_FLAG;
					}
					
					/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
					Trc_SHR_CMI_validate_ExitStaleItem_Event(currentThread, foundROMClass, compareTo, confirmedEntries); 
					Trc_SHR_CMI_validate_ExitStaleItem(currentThread);
					return 0;			/* fail the find */
				} else if (tsChangedVal == -1) {
					/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
					Trc_SHR_CMI_validate_ExitError2_Event(currentThread, foundROMClass, compareTo, confirmedEntries);
					Trc_SHR_CMI_validate_ExitError2(currentThread);
					return -1;		/* Error already reported */
				}
			}
			compareTo->setJarsLockedToIndex(jarsLockedToIndex);
		}
	}

	/* If the classpath has just been identified by localValidate_CheckAndTimestampManually, store it in the identified array */
	if (addToIdentified && (compareTo->getType()==CP_TYPE_CLASSPATH) && (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING)) {
		/* Identification succeeded. Store identified classpath. */
		if (local_StoreIdentified(currentThread, compareTo, testCP)==-1) {
			/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
			Trc_SHR_CMI_validate_ExitError_Event(currentThread, foundROMClass, compareTo, confirmedEntries);
			Trc_SHR_CMI_validate_ExitError(currentThread);
			return -1;		/* Error already reported */
		}
	}

_done:
	/* If confirmedEntries has been set and "found at index" is for an unconfirmed entry, then the match is not valid */
	if ((confirmedEntries >= 0) && (localFoundAtIndex >= confirmedEntries)) {
		/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683  */
		Trc_SHR_CMI_validate_ExitNotConfirmed_Event(currentThread, localFoundAtIndex, confirmedEntries, foundROMClass, compareTo);
		Trc_SHR_CMI_validate_ExitNotConfirmed(currentThread, localFoundAtIndex, confirmedEntries);
		localFoundAtIndex = -1;
	}

	*foundAtIndex = localFoundAtIndex;
	result = (localFoundAtIndex != -1);

	/* no level 1 trace event here due to performance problem stated in CMVC 155318/157683 */
	Trc_SHR_CMI_validate_ExitDone(currentThread, result, localFoundAtIndex);
	return result;
}

/**
 * Initializes timestamp values for all entries in a new classpath
 *
 * @see ClasspathManager.hpp
 * @warning Must be called with cache write mutex held
 * param[in] currentThread The current thread
 * param[in] wrapperInCache The new classpath to initialize
 */
void 
SH_ClasspathManagerImpl2::setTimestamps(J9VMThread* currentThread, ClasspathWrapper* wrapperInCache)
{
	ClasspathItem* cpi = NULL;

	Trc_SHR_CMI_setTimestamps_Entry(currentThread, wrapperInCache);

	cpi = (ClasspathItem*)CPWDATA(wrapperInCache);
	for (I_16 i=0; i<cpi->getItemsAdded(); i++) {
		ClasspathEntryItem* current = cpi->itemAt(i);
		I_64 newTimeStamp = 0;

		/* Only store timestamps for JARs. For classfiles in directories, only timestamp the specific class */
		if ((PROTO_JAR == current->protocol)
			|| (PROTO_JIMAGE == current->protocol)
		) {
			newTimeStamp = _tsm->checkCPEITimeStamp(currentThread, current);
		}
		if ((newTimeStamp != TIMESTAMP_UNCHANGED) && (newTimeStamp != TIMESTAMP_DISAPPEARED)) {
			U_16 cpeiPathLen = 0;
			const char* cpeiPath = current->getPath(&cpeiPathLen);
			Trc_SHR_CMI_setTimestamps_NewTimestamp(currentThread, cpeiPathLen, cpeiPath, newTimeStamp);

			current->timestamp = newTimeStamp;
		}
	}

	Trc_SHR_CMI_setTimestamps_Exit(currentThread);
}

/**
 * Registers a new cached Classpath with the SH_ClasspathManager
 *
 * Called when a new Classpath has been identified in the cache
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 * @param[in] itemInCache The address of the item found in the cache
 * 
 * @return true if successful, false otherwise
 */
bool 
SH_ClasspathManagerImpl2::storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet)
{
	ClasspathWrapper* wrapperInCache = NULL;
	ClasspathItem* cpi = NULL;

	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}
	Trc_SHR_CMI_storeNew_Entry(currentThread, itemInCache);

	wrapperInCache = (ClasspathWrapper*)ITEMDATA(itemInCache);
	cpi = (ClasspathItem*)CPWDATA(wrapperInCache);
	for (I_16 i=0; i<cpi->getItemsAdded(); i++) {
		bool isLastItem = (i==(cpi->getItemsAdded()-1));
		U_16 pathLen = 0;
		const char* path = cpi->itemAt(i)->getLocation(&pathLen);
		U_8 isToken = (cpi->getType()==CP_TYPE_TOKEN);

		if (!cpeTableUpdate(currentThread, path, pathLen, i, itemInCache, isToken, isLastItem, cachelet)) {
			Trc_SHR_CMI_storeNew_ExitFalse(currentThread);
			return false;
		}
	}
	if (cpi->getType()==CP_TYPE_CLASSPATH) {
		++_classpathCount;
	} else 
	if (cpi->getType()==CP_TYPE_URL) {
		++_urlCount;
	} else 
	if (cpi->getType()==CP_TYPE_TOKEN) {
		++_tokenCount;
	}

	Trc_SHR_CMI_storeNew_ExitTrue(currentThread);
	return true;
}

/**
 * Marks all classpaths containing a given classpath entry stale
 *
 * The "staleFromIndex" of each ClasspathItem is set to the index of cpei in that classpath.
 * After this function is called, the caller should test all ROMClasses in the cache to see if they now
 * point to a stale classpath entry.
 *
 * @see ClasspathManager.hpp
 * @warning Must only be called with the cache write mutex AND a full cache lock (no readers).
 * @param[in] currentThread The current thread
 * @param[in] cpei The stale ClasspathEntryItem
 */ 
void 
SH_ClasspathManagerImpl2::markClasspathsStale(J9VMThread* currentThread, ClasspathEntryItem* cpei) 
{
	CpLinkedListImpl *cpToMark = NULL;
	CpLinkedListImpl *walk = NULL;
	U_16 pathLen = 0;
	const char* path = NULL;
	CpLinkedListHdr* header = NULL;

	path = cpei->getLocation(&pathLen);
	Trc_SHR_CMI_markClasspathsStale_Entry(currentThread, pathLen, path);

	header = cpeTableLookup(currentThread, path, pathLen, 0);		/* 0 == isToken. We will never mark token cpei stale */
	if (header) {
		cpToMark = header->_list;
	} else {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}

	if (cpToMark) {
		walk = cpToMark;
		do {
			ClasspathWrapper* cpw = (ClasspathWrapper*)ITEMDATA(walk->_item);
	
			if (*_runtimeFlagsPtr & J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING) {
				/* No need to get local identifiedMutex as we already have cache mutex 
				 * Note also that identifiedClasspath contains reference to cpw in cache, so
				 * if it is cleared, there is no memory we need to free */
				clearIdentifiedClasspath(_portlib, _identifiedClasspaths, cpw);
			}
			cpw->staleFromIndex = CpLinkedListImpl::getCPEIndex(walk);
			Trc_SHR_CMI_markClasspathsStale_SetStaleFromIndex(currentThread, cpw->staleFromIndex, walk);
			walk = (CpLinkedListImpl*)walk->_next;
		} while (cpToMark!=walk);
	}

	Trc_SHR_CMI_markClasspathsStale_Exit(currentThread);
}

/**
 * Tests to see if a cached classpath is stale
 * 
 * @see ClasspathManager.hpp
 * @param[in] cpw The cached classpath to test
 *
 * @return true if the classpath is stale, false otherwise
 */
bool 
SH_ClasspathManagerImpl2::isStale(ClasspathWrapper* cpw) 
{
	bool result = (cpw->staleFromIndex != CPW_NOT_STALE);
	Trc_SHR_CMI_isStale(cpw, result);
	return result;
}

/**
 * Scans all directory entries in a classpath for the existence of a .class file of a given fully qualified name
 *
 * @see ClasspathManager.hpp
 * @param[in] currentThread The current thread
 * @param[in] className The fully-qualified name of the class to search for
 * @param[in] classNameLen The length of the class name
 * @param[in] cp The classpath to search
 * @param[in] toIndex Search upto (but not including) the given index
 *
 * @return true If a classfile has been found and there was no error, false otherwise
 */
bool
SH_ClasspathManagerImpl2::touchForClassFiles(J9VMThread* currentThread, const char* className, UDATA classNameLen, ClasspathItem* cp, I_16 toIndex) 
{
	I_16 i;

	PORT_ACCESS_FROM_PORT(currentThread->javaVM->portLibrary);

	Trc_SHR_CMI_touchForClassFiles_Entry(currentThread, classNameLen, className, cp, toIndex);

	/* If we are just using jars, no need to check */
	if (cp->getFirstDirIndex()==-1 || cp->getFirstDirIndex()>toIndex) {
		Trc_SHR_CMI_touchForClassFiles_ExitFalse1(currentThread);
		return false;
	}
	for (i=0; i<toIndex; i++) {
		ClasspathEntryItem* current = cp->itemAt(i);
		if (current->protocol == PROTO_DIR) {
			char pathBuf[SHARE_PATHBUF_SIZE];
			char* pathBufPtr = (char*)pathBuf;
			bool doFreeBuffer = false;

			/* If stack buffer not big enough, doFreeBuffer is set to true */
			if (SH_CacheMap::createPathString(currentThread, _cache->getSharedClassConfig(), &pathBufPtr, SHARE_PATHBUF_SIZE, current, className, classNameLen, &doFreeBuffer)) {
				/* Error reported in function */
				Trc_SHR_CMI_touchForClassFiles_ExitError(currentThread);
				return false;
			}
			if (j9file_attr(pathBuf)>=0) {
				Trc_SHR_CMI_touchForClassFiles_ExitTrue(currentThread, pathBufPtr);
				return true;
			}
			if (doFreeBuffer) {
				Trc_SHR_CMI_touchForClassFiles_FreeBuffer(currentThread, pathBufPtr);
				j9mem_free_memory(pathBufPtr);
			}
		}
	}
	Trc_SHR_CMI_touchForClassFiles_ExitFalse2(currentThread);
	return false;
}

void 
SH_ClasspathManagerImpl2::notifyClasspathEntryStateChange(J9VMThread* currentThread, const J9UTF8* path, UDATA newState)
{
	CpLinkedListHdr* header;
	const char* pathChar = (const char*)J9UTF8_DATA(path);
	U_16 pathLen = J9UTF8_LENGTH(path);

	Trc_SHR_CMI_notifyClasspathEntryStateChange_Entry(currentThread, pathLen, pathChar, newState);

	if (newState == 0) {
		Trc_SHR_CMI_notifyClasspathEntryStateChange_ExitNoop(currentThread);
		return;
	}
	
	header = cpeTableLookup(currentThread, pathChar, pathLen, 0);			/* 0 = isToken. Certainly won't be a token. */
	if (header == NULL) {
		header = cpeTableAddHeader(currentThread, pathChar, pathLen, NULL, 0);
	}
	
	if (header) {
		UDATA oldFlags = header->_flags;
		
		switch(newState) {
		case J9ZIP_STATE_OPEN :
			/* CMVC 201090: Note that this step clears out any existing flags on the container. */
			header->_flags = (CPM_ZIP_OPEN | CPM_ZIP_FORCE_CHECK_TIMESTAMP);
			break;
		case J9ZIP_STATE_CLOSED :
			header->_flags &= ~CPM_ZIP_OPEN;
			break;
		case J9ZIP_STATE_IGNORE_STATE_CHANGES :
			header->_flags |= CPM_ZIP_ONLY_TIMESTAMP_ON_INIT;
			break;
		case J9ZIP_STATE_RESET :
			break;
		default :
			break;
		}
		Trc_SHR_CMI_notifyClasspathEntryStateChange_FlagsEvent(currentThread, header, oldFlags, header->_flags);
	}
	Trc_SHR_CMI_notifyClasspathEntryStateChange_Exit(currentThread);
}

/* Note that getNumItems will return the number of known classpath containers, 
 * but cannot return the correct number of classpaths. This is because classpaths are indexed by container. */ 
void
SH_ClasspathManagerImpl2::getNumItemsByType(UDATA* numClasspaths, UDATA* numURLs, UDATA* numTokens)
{
	*numClasspaths = _classpathCount;
	*numURLs = _urlCount;
	*numTokens = _tokenCount;
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
SH_ClasspathManagerImpl2::createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	Trc_SHR_Assert_True(hints != NULL);

	/* hints->dataType should have been set by the caller */
	Trc_SHR_Assert_True(hints->dataType == _dataTypesRepresented[0]);
	
	return cpeCollectHashes(vmthread, cachelet, hints);
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
SH_ClasspathManagerImpl2::primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA dataLength)
{
	UDATA* hashSlot = (UDATA*)hintsData;
	UDATA hintCount = 0;

	if ((dataLength == 0) || (hintsData == NULL)) {
		return 0;
	}

	hintCount = dataLength / sizeof(UDATA);
	while (hintCount-- > 0) {
		Trc_SHR_CMI_primeHashtables_addingHint(vmthread, cachelet, *hashSlot);
		if (!_hints.addHint(vmthread, *hashSlot, cachelet)) {
			/* If we failed to finish priming the hints, just give up.
			 * Stuff should still work, just suboptimally.
			 */
			Trc_SHR_CMI_primeHashtables_failedToPrimeHint(vmthread, this, cachelet, *hashSlot);
			break;
		}
		++hashSlot;
	}
	
	return 0;
}

/**
 * A hash table do function. Collect hash values of entries in a cachelet.
 */
UDATA
SH_ClasspathManagerImpl2::cpeCollectHashOfEntry(void* entry, void* userData)
{
	CpLinkedListHdr* node = (CpLinkedListHdr*)entry;
	CpLinkedListImpl* item = node->_list;
	CacheletHintHashData* data = (CacheletHintHashData*)userData;

	if (item) {
		do {
			if (data->cachelet == item->_cachelet) {
				*data->hashSlot++ = cpeHashFn(node, data->userData);
				break;
			}
			item = (CpLinkedListImpl*)item->_next;
		} while (item != node->_list);
	}
	return FALSE; /* don't remove entry */
}

/**
 * A hash table do function. Count number of entries in a cachelet.
 */
UDATA
SH_ClasspathManagerImpl2::cpeCountCacheletHashes(void* entry, void* userData)
{
	CpLinkedListHdr* node = (CpLinkedListHdr*)entry;
	CpLinkedListImpl* item = node->_list;
	CacheletHintCountData* data = (CacheletHintCountData*)userData;

	if (item) {
		do {
			if (data->cachelet == item->_cachelet) {
				data->hashCount++;
				break;
			}
			item = (CpLinkedListImpl*)item->_next;
		} while (item != node->_list);
	}
	return FALSE; /* don't remove entry */
}

/**
 * Collect the hash entry hashes into a cachelet hint.
 */
IDATA
SH_ClasspathManagerImpl2::cpeCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	CacheletHintHashData hashes;
	CacheletHintCountData counts;

	/* count hashes in this cachelet */
	counts.cachelet = cachelet;
	counts.hashCount = 0;
	hashTableForEachDo(_hashTable, cpeCountCacheletHashes, (void*)&counts);
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
	hashTableForEachDo(_hashTable, cpeCollectHashOfEntry, (void*)&hashes);
	/* TODO trace hints written */

	return 0;
}

#endif /* J9SHR_CACHELET_SUPPORT */
