/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#if !defined(ROMCLASS_RESOURCE_MANAGER_HPP_INCLUDED)
#define ROMCLASS_RESOURCE_MANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "Manager.hpp"
#include "SharedCache.hpp"
#include "j9protos.h"
#include "j9.h"

/**
 * ROMClassResources are items keyed (indexed) by a process-relative address of something in the cache.
 * It should be impossible for two different items to have the same key.
 * 
 * The hint value is the key, so it should be impossible for multiple keys to have the same hint.
 */
class SH_ROMClassResourceManager : public SH_Manager
{
public:
	SH_ROMClassResourceManager();

	~SH_ROMClassResourceManager();

	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet);

	virtual const void* findResource(J9VMThread* currentThread, UDATA resourceKey);

	/**
	 * This function is extremely hot. It provides the a way of directly querying the RRM hashtable without having to go
	 * through findSharedX, which carries much more overhead. This does not get the cache read mutex, it contains no tracepoints
	 * and does the absolute bare minimum required to provide a yes/no answer. Since we do not return the actual location of
	 * the shared resource, it is NOT a substitute for findSharedX. It can only be called once the RRM has started up. 
	 * Note also that this function does not look for recent cache updates, so it may return false when the correct answer is actually true.
	 *
	 * @param [in] currentThread A Java VM Thread
	 * @param [in] resourceKey The ROM address which a resource is associated with
	 *
	 * @return 1 if a resource exists for the given ROM address in the RRM hashtable
	 */
	inline UDATA existsResourceForROMAddress(J9VMThread* currentThread, UDATA resourceKey) {
		HashTableEntry searchKey(resourceKey, 0, NULL);
		HashTableEntry* returnVal = NULL;

		if (omrthread_monitor_enter(_htMutex)==0) {
			returnVal = (HashTableEntry*)hashTableFind(_hashTable, (void*)&searchKey);
			omrthread_monitor_exit(_htMutex);
		}
		return (returnVal != NULL);
	};
	
	virtual UDATA markStale(J9VMThread* currentThread, UDATA resourceKey, const ShcItem* itemInCache);
	
	virtual bool permitAccessToResource(J9VMThread* currentThread);
	
	virtual UDATA getDataBytes();
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual void fixupHintsForSerialization(J9VMThread* vmthread,
			UDATA dataType, UDATA dataLen, U_8* data,
			IDATA deployedOffset, void* serializedROMClassStartAddress);
#endif

	void runExitCode(void) {};

class SH_ResourceDescriptor
{
	public:
		SH_ResourceDescriptor() {}

		virtual ~SH_ResourceDescriptor() {}
		
		/* Return the length in bytes of the resource data represented */
		virtual U_32 getResourceLength() = 0;

		/* Return the type of resource data */
		virtual U_16 getResourceDataSubType() { return J9SHR_RESOURCE_TYPE_UNKNOWN; }

		/* Return the length in bytes of the wrapper struct used to wrap this resource in the cache */
		virtual U_32 getWrapperLength() = 0;

		/* Get the constant representing the resource type */
		virtual U_16 getResourceType() = 0;

		/* Get the byte alignment for the start of the data in the cache */
		virtual U_32 getAlign() = 0;

		/* Given a wrapper struct, return the corresponding cache ShcItem struct */ 
		virtual const ShcItem* wrapperToItem(const void* wrapper) = 0;

		/* Given a wrapper struct, return the resource data */
		virtual const void* unWrap(const void* wrapper) = 0;

		/* Determine the resource length from the wrapper struct */
		virtual UDATA resourceLengthFromWrapper(const void* wrapper) = 0;
		
		/* Given a resource and a cache ShcItem address, populate the ShcItem and write the resource into the cache */
		virtual void writeDataToCache(const ShcItem* newCacheItem, const void* resourceAddress) = 0;

		/* Update the given cache item at the specified offset with the value in data */
		virtual void updateDataInCache(const ShcItem *cacheItem, I_32 updateAtOffset, const J9SharedDataDescriptor* data) {};

		/* Update the given cache item at the specified offset with the UDATA value */
		virtual void updateUDATAInCache(const ShcItem *cacheItem, I_32 updateAtOffset, UDATA value) {};

		/* Generate the key to be used for given resource */
		virtual UDATA generateKey(const void *resourceAddress) = 0;

	private:
		/* Placement operator new (<new> is not included) */
		void* operator new(size_t size, void* memoryPtr) {return memoryPtr;}

	protected:	
		/* Delete operator added to avoid linkage with C++ runtime libs 
		 */
		void operator delete(void *) { return; }
	};

protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };
	
	/* Delete operator added to avoid linkage with C++ runtime libs 
	*/
	void operator delete(void *) { return; }

	IDATA localPostStartup(J9VMThread* currentThread) { return 0; };

	void localPostCleanup(J9VMThread* currentThread) {};

	virtual J9HashTable* localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries);

	IDATA localInitializePools(J9VMThread* currentThread) { return 0; };

	void localTearDownPools(J9VMThread* currentThread) {};
	
	HashLinkedListImpl* localHLLNewInstance(HashLinkedListImpl* memForConstructor) { return NULL; };
	
	virtual UDATA getKeyForItem(const ShcItem* cacheItem) = 0;
	
	static void getKeyAndItemForHashtableEntry(void* entry, UDATA* keyPtr, const ShcItem** itemPtr);
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints();
	virtual IDATA createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints);
	virtual IDATA primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);
#endif

	const char* _rrmHashTableName;
	const char* _rrmLookupFnName;
	const char* _rrmAddFnName;
	const char* _rrmRemoveFnName;
	
	bool _accessPermitted;

private:
	UDATA _dataBytes;

	/* Copy prevention */
	SH_ROMClassResourceManager(const SH_ROMClassResourceManager&);
	SH_ROMClassResourceManager& operator=(const SH_ROMClassResourceManager&);

	class HashTableEntry;

	HashTableEntry* rrmTableAdd(J9VMThread* currentThread, const ShcItem* item, SH_CompositeCache* cachelet);
#if defined(J9SHR_CACHELET_SUPPORT)
	HashTableEntry* rrmTableAddPrime(J9VMThread* currentThread, UDATA key, SH_CompositeCache* cachelet);
#endif
	HashTableEntry* rrmTableAddHelper(J9VMThread* currentThread, HashTableEntry* newEntry, SH_CompositeCache* cachelet);
	HashTableEntry* rrmTableLookup(J9VMThread* currentThread, UDATA key);
	UDATA rrmTableRemove(J9VMThread* currentThread, UDATA key);
	static UDATA rrmHashFn(void* item, void* userData);
	static UDATA rrmHashEqualFn(void* left, void* right, void* userData);

	static UDATA customCountItemsInList(void* entry, void* opaque);

#if defined(J9SHR_CACHELET_SUPPORT)
	static UDATA rrmCountCacheletHashes(void* entry, void* userData);
	static UDATA rrmCollectHashOfEntry(void* entry, void* userData);
	IDATA rrmCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints);
#endif

/* Nested class representing an entry in the cache */


class HashTableEntry
{
	public:
		HashTableEntry(UDATA key, const ShcItem* item, SH_CompositeCache* _cachelet);

		~HashTableEntry();

		/* Methods for getting properties of the object */
		UDATA key() const {return _key;}
		const ShcItem* item() const {return _item;}
#if defined(J9SHR_CACHELET_SUPPORT)
		SH_CompositeCache* cachelet() const {return _cachelet;}
#endif
		void setItem(const ShcItem* item) { _item = item; }

	private:
		/* Placement operator new (<new> is not included) */
		void* operator new(size_t size, void* memoryPtr) {return memoryPtr;}

		/* Declared data */
		UDATA _key;
		const ShcItem* _item;
#if defined(J9SHR_CACHELET_SUPPORT)
		SH_CompositeCache* _cachelet;
#endif
	};
};

#endif

