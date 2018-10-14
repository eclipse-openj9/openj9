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

#if !defined(SCOPEMANAGERIMPL_H_INCLUDED)
#define SCOPEMANAGERIMPL_H_INCLUDED

/* @ddr_namespace: default */
#include "ScopeManager.hpp"
#include "SharedCache.hpp"
#include "j9protos.h"
#include "j9.h"

/**
 * Implementation of SH_ScopeManager
 *
 * The Scope manager has the job of indexing, locating and comparing Scopes in the shared class cache.
 *
 * @ingroup Shared_Common
 */
class SH_ScopeManagerImpl : public SH_ScopeManager
{
public:
	SH_ScopeManagerImpl();

	~SH_ScopeManagerImpl();

	static SH_ScopeManagerImpl* newInstance(J9JavaVM* vm, SH_SharedCache* cache, SH_ScopeManagerImpl* memForConstructor);

	static UDATA getRequiredConstrBytes(void);

	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet);

	virtual const J9UTF8* findScopeForUTF(J9VMThread* currentThread, const J9UTF8* localScope);

	virtual IDATA validate(J9VMThread* currentThread, const J9UTF8* partition, const J9UTF8* modContext, const ShcItem* item);

	void runExitCode(void) {};	

protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; }

	IDATA localPostStartup(J9VMThread* currentThread) { return 0; }
	void localPostCleanup(J9VMThread* currentThread) {}
	virtual J9HashTable* localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries);
	IDATA localInitializePools(J9VMThread* currentThread);
	void localTearDownPools(J9VMThread* currentThread);
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes);	
	HashLinkedListImpl* localHLLNewInstance(HashLinkedListImpl* memForConstructor) { return NULL; }

#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints() { return true; }
	virtual IDATA createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints);
	virtual IDATA primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);
#endif

private:
	struct HashEntry {
		const J9UTF8* _value;
#if defined(J9SHR_CACHELET_SUPPORT)
		SH_CompositeCache* _cachelet;
#endif

		HashEntry(const J9UTF8* value_, SH_CompositeCache* cachelet_):
			_value(value_)
#if defined(J9SHR_CACHELET_SUPPORT)
			,_cachelet(cachelet_)
#endif
		 {}
	};

	void initialize(J9JavaVM* vm, SH_SharedCache* cache, BlockPtr memForConstructor);

	static UDATA scHashFn(void* item, void *userData);
	static UDATA scHashEqualFn(void* left, void* right, void *userData);
	const J9UTF8* scTableLookup(J9VMThread* currentThread, const J9UTF8* key);
	const HashEntry* scTableAdd(J9VMThread* currentThread, const ShcItem* item, SH_CompositeCache* cachelet);

#if defined(J9SHR_CACHELET_SUPPORT)
	IDATA scCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints);
	static UDATA scCollectHashOfEntry(void* entry, void* userData);
	static UDATA scCountCacheletHashes(void* entry, void* userData);

	bool _allCacheletsStarted;
#endif /* J9SHR_CACHELET_SUPPORT */
};

#endif /* SCOPEMANAGERIMPL_H_INCLUDED */

