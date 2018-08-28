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

#if !defined(MANAGER_HPP_INCLUDED)
#define MANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "sharedconsts.h"
#include "j9.h"
#include "j9protos.h"
#include "CompositeCacheImpl.hpp"
#include "ManagerHintTable.hpp"

#define M_ERR_TRACE(var) if (_verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var)
#define M_ERR_TRACE2(var, p1, p2) if (_verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var, p1, p2)
#define M_ERR_TRACE_NOTAG(var) if (_verboseFlags) j9nls_printf(PORTLIB, (J9NLS_ERROR | J9NLS_DO_NOT_PRINT_MESSAGE_TAG), var)
#define M_ERR_TRACE2_NOTAG(var, p1, p2) if (_verboseFlags) j9nls_printf(PORTLIB, (J9NLS_ERROR | J9NLS_DO_NOT_PRINT_MESSAGE_TAG), var, p1, p2)
#define M_ERR_TRACE3_NOTAG(var, p1, p2, p3) if (_verboseFlags) j9nls_printf(PORTLIB, (J9NLS_ERROR | J9NLS_DO_NOT_PRINT_MESSAGE_TAG), var, p1, p2, p3)

#define STACK_STRINGBUF_SIZE 512

#define MAX_TYPES_PER_MANAGER 3

class SH_Managers;
class SH_SharedCache;
class SH_CacheMap;

/** 
 * Interface which guarantees basic functions of all SH_Manager classes.
 * 
 * @see ROMClassManager.hpp
 * @see ClasspathManager.hpp
 * @see CompiledMethodManager.hpp
 * @see ScopeMethodManager.hpp
 * 
 * @note TimestampManager.hpp is not an SH_Manager as it does not store any data
 *
 * @ingroup Shared_Common
 */
class SH_Manager
{
public:
	SH_Manager();

	~SH_Manager();

	/**
	 * Abstract linked list class used by SH_Manager classes to store
	 * references to items in the cache.
	 * Each LinkedListImpl points to an entry in the cache.
	 * 
	 * @ingroup Shared_Common
	 */
	class LinkedListImpl
	{
	public:
		const ShcItem* _item;
#if defined(J9SHR_CACHELET_SUPPORT)
		SH_CompositeCache* _cachelet;
#endif
		LinkedListImpl* _next;

		void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

		/*
		 * Static function to link new LinkedListImpl to an existing list.
		 * Returns new LinkedListImpl linked.
		 * 
		 * Parameters:
		 *   addToList		List to add to
		 *   newLink		New link to add
		 */
		static LinkedListImpl* link(LinkedListImpl* addToList, LinkedListImpl* newLink);
	};

	/** 
	 * Extends LinkedListImpl to add a key which can be used in a hashtable
	 * 
	 * @ingroup Shared_Common
	 */
	class HashLinkedListImpl : public LinkedListImpl
	{
	public:
		U_8* _key;
		U_16 _keySize;
		UDATA _hashValue;
		
		void initialize(const J9UTF8* key, const ShcItem* item, SH_CompositeCache* cachelet, UDATA hashPrimeValue);

	protected:
	
		/* Override localInit to perform subclass initialization */
		void localInit(const J9UTF8* key, const ShcItem* item, SH_CompositeCache* cachelet, UDATA hashPrimeValue) {};
	};

	class CountData 
	{
	public :
		UDATA _nonStaleItems;
		UDATA _staleItems;
		SH_SharedCache *_cache;

		explicit CountData(SH_SharedCache *cache)
			: _nonStaleItems(0)
			, _staleItems(0)
			, _cache(cache)
		{
		}
	};

#if defined(J9SHR_CACHELET_SUPPORT)
	class CacheletListItem
	{
	public :
		UDATA _dataType;
		SH_CompositeCacheImpl* cachelet;
		CacheletListItem* next;
	};

	struct CacheletMetadata {
		SH_CompositeCache* cachelet;
		UDATA numHints;
		CacheletWrapper* wrapperAddress;
		CacheletHints* hintsArray;	
	};

	struct CacheletMetadataArray {
		UDATA numMetas;
		CacheletMetadata* metadataArray;
	};
#endif /* J9SHR_CACHELET_SUPPORT */
	
	/* This function must be implemented by the manager subclass - it should store the new item given in its hashtable */
	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet) = 0;
	
	void getNumItems(J9VMThread* currentThread, UDATA* nonStaleItems, UDATA* staleItems);
	
	IDATA reset(J9VMThread* currentThread);
	
	void cleanup(J9VMThread* currentThread);

	void shutDown(J9VMThread* currentThread);

	IDATA startup(J9VMThread* currentThread, U_64* runtimeFlags, UDATA verboseFlags, UDATA cacheSize);

	U_8 getState();
	
#if defined(J9SHR_CACHELET_SUPPORT)
	SH_CompositeCacheImpl* getCacheAreaForData(SH_CacheMap* cm, J9VMThread* currentThread, UDATA dataType, UDATA dataLength);

	void addNewCacheArea(SH_CompositeCacheImpl* newCache);
	
	bool canShareNewCacheletsWithOtherManagers();
#endif

	/* This function gives the manager a chance to perform operations on exit 
	 * This should NOT include freeing any resources */
	virtual void runExitCode(void) = 0;

#if defined(J9SHR_CACHELET_SUPPORT)
	IDATA primeFromHints(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);

	IDATA generateHints(J9VMThread* vmthread, CacheletMetadataArray* metadataArray);
	
	virtual void fixupHintsForSerialization(J9VMThread* vmthread,
			UDATA dataType, UDATA dataLen, U_8* data,
			IDATA deployedOffset, void* serializedROMClassStartAddress) {}
#endif

#if defined(J9SHR_CACHELET_SUPPORT)
	IDATA freeHintData(J9VMThread* vmthread, CacheletMetadataArray* metadataArray);
#endif

#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints();
#endif

	bool isDataTypeRepresended(UDATA type);

protected:
	J9HashTable* _hashTable;
	SH_SharedCache* _cache;
	omrthread_monitor_t _htMutex;
	const char* _htMutexName;
	J9PortLibrary* _portlib;
	U_32 _htEntries;
	U_64* _runtimeFlagsPtr;
	UDATA _verboseFlags;
	J9HashTableDoFn _hashTableGetNumItemsDoFn;
#if defined(J9SHR_CACHELET_SUPPORT)
	CacheletListItem* _cacheletListHead;
	CacheletListItem* _cacheletListTail;
	J9Pool* _cacheletListPool;
	bool _shareNewCacheletsWithOtherManagers;
#endif
	bool _isRunningNested;
		/* Indicates that this manager is aware that the shared cache contains cachelets.
		 * The cache may or may not be growable (i.e. chained).
		 * NOT equivalent to whether -Xshareclasses:nested has been specified.
		 * NOT equivalent to SH_CacheMap::_runningNested.
		 */
	UDATA _dataTypesRepresented[MAX_TYPES_PER_MANAGER];
#if defined(J9SHR_CACHELET_SUPPORT)
	ManagerHintTable _hints;
#endif
	
	
	/* Functions which must be implemented by manager subclasses */

	/* This function gives the manager an opportunity to run code at the end of the startup routine */
	virtual IDATA localPostStartup(J9VMThread* currentThread) = 0;

	/* This function gives the manager an opportunity to run code at the end of the cleanup routine */
	virtual void localPostCleanup(J9VMThread* currentThread) = 0;

	/* This function is where the Manager should create its local hashtable */
	virtual J9HashTable* localHashTableCreate(J9VMThread* currentThread, U_32 htEntries) = 0;

	/* This function is where the Manager can optionally create any other resources it needs, such as pools */
	virtual IDATA localInitializePools(J9VMThread* currentThread) = 0;

	/* This function is where the Manager declares code which frees the resources created in localInitializePools */
	virtual void localTearDownPools(J9VMThread* currentThread) = 0;
	
	/* This function should return an appropriate number of hashtable for the manager based on the cache size given */
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes) = 0;
	
	/* This function should be implemented if the Manager uses a HashLinkedListImpl and should 
	 * simply return a new instance of the subclass into the memory provided*/
	virtual HashLinkedListImpl* localHLLNewInstance(HashLinkedListImpl* memForConstructor) = 0;
	
	/* This function should be called by the sub-class when it has finished initializing */
	void notifyManagerInitialized(SH_Managers* managers, const char* managerType);

	/* This function creates a new link in a linked list */
	HashLinkedListImpl* createLink(const J9UTF8* key, const ShcItem* item, SH_CompositeCache* cachelet, UDATA hashPrimeValue, const J9Pool* allocationPool);

	/* This function is the lookup entry point for a manager linked list hashtable */
	HashLinkedListImpl* hllTableLookup(J9VMThread* currentThread, const char* name, U_16 nameLen, bool allowCacheletStartup);

	/* This function is the update entry point for a manager linked list hashtable */
	HashLinkedListImpl* hllTableUpdate(J9VMThread* currentThread, const J9Pool* linkPool, const J9UTF8* key, const ShcItem* item, SH_CompositeCache* cachelet);

	/* Synchronize access to _hashTable */
	bool lockHashTable(J9VMThread* currentThread, const char* funcName);
	void unlockHashTable(J9VMThread* currentThread, const char* funcName);

	static UDATA hllHashFn(void* item, void *userData);
	static UDATA hllHashEqualFn(void* left, void* right, void *userData);

#if defined(J9SHR_CACHELET_SUPPORT)

	struct CacheletHintCountData {
		SH_CompositeCache* cachelet;
		UDATA hashCount;
	};

	static UDATA hllCollectHashOfEntry(void* entry, void* userData);
	
	struct CacheletHintHashData {
		SH_CompositeCache* cachelet;
		UDATA *hashSlot;
		void* userData;
	};

	static UDATA hllCountCacheletHashes(void* entry, void* userData);
	IDATA hllCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints);
#endif
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual IDATA primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);
	virtual IDATA createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints);
	IDATA startupHintCachelets(J9VMThread* currentThread, UDATA hint);
#endif

private:
	UDATA _state;
#if defined(J9SHR_CACHELET_SUPPORT)
	bool _allCacheletsStarted;
#endif
	
	const char* _managerType;

	IDATA initializeHashTable(J9VMThread* currentThread);

	void tearDownHashTable(J9VMThread* currentThread);

	HashLinkedListImpl* hllTableAdd(J9VMThread* currentThread, const J9Pool* linkPool, const J9UTF8* key, const ShcItem* item, UDATA hashPrimeValue, SH_CompositeCache* cachelet, HashLinkedListImpl** addToList);
	HashLinkedListImpl* hllTableLookupHelper(J9VMThread* currentThread, U_8* key, U_16 keySize, UDATA hashValue, SH_CompositeCache* cachelet);

	static UDATA countItemsInList(void* node, void* countData);

#if defined(J9SHR_CACHELET_SUPPORT)
	bool isCacheletInList(SH_CompositeCache* cachelet);
#endif

	static UDATA generateHash(J9InternalVMFunctions* internalFunctionTable, U_8* key, U_16 keySize);

#if defined(J9SHR_CACHELET_SUPPORT)
	J9Pool* getCacheletListPool(void); 
#endif
};

#endif /* !defined(MANAGER_HPP_INCLUDED) */


