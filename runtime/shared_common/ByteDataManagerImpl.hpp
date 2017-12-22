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

/**
 * @file
 * @ingroup Shared_Common
 */

#if !defined(BYTEDATAMANAGERIMPL_H_INCLUDED)
#define BYTEDATAMANAGERIMPL_H_INCLUDED

/* @ddr_namespace: default */
#include "ByteDataManager.hpp"
#include "SharedCache.hpp"
#include "j9protos.h"
#include "j9.h"

/**
 * Implementation of SH_ByteDataManager
 *
 * The Byte Data manager has the job of indexing and locating and Byte Data in the shared class cache.
 *
 * @ingroup Shared_Common
 */
class SH_ByteDataManagerImpl : public SH_ByteDataManager
{
public:
	typedef char* BlockPtr;

	SH_ByteDataManagerImpl();

	~SH_ByteDataManagerImpl();

	static SH_ByteDataManagerImpl* newInstance(J9JavaVM* vm, SH_SharedCache* cache, SH_ByteDataManagerImpl* memForConstructor);

	static UDATA getRequiredConstrBytes(void);

	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet);

	virtual ByteDataWrapper* findSingleEntry(J9VMThread* currentThread, const char* key, UDATA keylen, UDATA dataType, U_16 jvmID, UDATA* dataLen);

	virtual void markAllStaleForKey(J9VMThread* currentThread, const char* key, UDATA keylen);

	virtual IDATA find(J9VMThread* currentThread, const char* key, UDATA keylen, UDATA limitDataType, UDATA includePrivateData, J9SharedDataDescriptor* firstItem, const J9Pool* descriptorPool);
	
	virtual UDATA acquirePrivateEntry(J9VMThread* currentThread, const J9SharedDataDescriptor* data);

	virtual UDATA releasePrivateEntry(J9VMThread* currentThread, const J9SharedDataDescriptor* data);
	
	virtual void runExitCode(void);

	virtual UDATA getNumOfType(UDATA type);

	virtual UDATA getDataBytesForType(UDATA type);

	virtual UDATA getUnindexedDataBytes();
	
protected:

	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	IDATA localPostStartup(J9VMThread* currentThread) { return 0; };

	void localPostCleanup(J9VMThread* currentThread) {};

	virtual J9HashTable* localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries);

	virtual IDATA localInitializePools(J9VMThread* currentThread);

	virtual void localTearDownPools(J9VMThread* currentThread);
	
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes);
	
	HashLinkedListImpl* localHLLNewInstance(HashLinkedListImpl* memForConstructor) {
		BdLinkedListImpl* newBLL = (BdLinkedListImpl*)memForConstructor;
		return (HashLinkedListImpl*) new(newBLL) BdLinkedListImpl();
	}
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints() { return true; }
	virtual IDATA createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints);
	virtual IDATA primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);
#endif

private:
	J9Pool* _linkedListImplPool;
	UDATA _unindexedBytes;
	UDATA _indexedBytesByType[J9SHR_DATA_TYPE_MAX+1];
	UDATA _numIndexedBytesByType[J9SHR_DATA_TYPE_MAX+1];

	void initialize(J9JavaVM* vm, SH_SharedCache* cache, BlockPtr memForConstructor);
	
	void setDescriptorFields(const ByteDataWrapper* wrapper, J9SharedDataDescriptor* descriptor);

	static UDATA htReleasePrivateEntry(void *entry, void *opaque);
	
	/**
	 * Circular linked list for storing local Byte data information.
	 *
	 * Local hashtable (_bdTable) has an entry for each known key.
	 * Key is the string key being used
	 * Value is a BdLinkedListImpl list of data entries stored under that name
	 * A BdLinkedListImpl is a reference to a data entry in the cache.
	 * Thus, given a data key, it is quick and easy to walk all known
	 * data entries in the cache stored under that name.
	 *
	 * @ingroup Shared_Common
	 */
	class BdLinkedListImpl : public SH_Manager::HashLinkedListImpl
	{
	protected:
		void localInit(const J9UTF8* key, const ShcItem* item) {};
	};
};

#endif /* BYTEDATAMANAGERIMPL_H_INCLUDED */

