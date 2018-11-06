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

#if !defined(ROMCLASSMANAGERIMPL_H_INCLUDED)
#define ROMCLASSMANAGERIMPL_H_INCLUDED

/* @ddr_namespace: default */
#include "ROMClassManager.hpp"
#include "TimestampManager.hpp"
#include "j9.h"
#include "j9protos.h"

/**
 * Implementation of SH_ROMClassManager
 *
 * The ROMClass manager has the job of indexing, locating and comparing ROMClasses in the shared class cache.
 * 
 * ROMClasses are keyed (indexed) by class name, a UTF8 string.
 * The hashtable is a HashLinkedList (HLL) table, implemented in SH_Manager. Each entry in the HLL table
 * is a linked list of ROMClasses that have the same name, but different or unknown classpaths.
 * 
 * Multiple class names may have the same hint value.
 * 
 * @ingroup Shared_Common
 */
class SH_ROMClassManagerImpl : public SH_ROMClassManager
{
public:
	SH_ROMClassManagerImpl();

	~SH_ROMClassManagerImpl();

	static SH_ROMClassManagerImpl* newInstance(J9JavaVM* vm, SH_SharedCache* cache, SH_TimestampManager* tsm, SH_ROMClassManagerImpl* memForConstructor);

	static UDATA getRequiredConstrBytes(void);

	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet);

	virtual UDATA locateROMClass(J9VMThread* currentThread, const char* path, U_16 pathLen, ClasspathItem* cp, I_16 cpeIndex, IDATA confirmedEntries, IDATA callerHelperID, 
					const J9ROMClass* cachedROMClass, const J9UTF8* partition, const J9UTF8* modContext, LocateROMClassResult* result);

	virtual const J9ROMClass* findNextExisting(J9VMThread* currentThread, void * &findNextIterator, void * &firstFound, U_16 classnameLength, const char* classnameData);

	virtual UDATA existsClassForName(J9VMThread* currentThread, const char* path, UDATA pathLen);

	void runExitCode(void) {};	

protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	IDATA localPostStartup(J9VMThread* currentThread) { return 0; };

	void localPostCleanup(J9VMThread* currentThread) {};

	virtual J9HashTable* localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries);

	virtual IDATA localInitializePools(J9VMThread* currentThread);

	virtual void localTearDownPools(J9VMThread* currentThread);
	
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes);	

	HashLinkedListImpl* localHLLNewInstance(HashLinkedListImpl* memForConstructor) {
		return new(memForConstructor) HashLinkedListImpl();
	}

#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints();
	virtual IDATA createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints);
	virtual IDATA primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);
#endif
	
private:
	SH_TimestampManager* _tsm;
	
	/**
	 * Circular linked list for storing local ROMClass information.
	 *
	 * Local hashtable (_hashTable) has an entry for each known ROMClass name.
	 * Key is the fully-qualified string name of the ROMClass
	 * Value is a HashLinkedListImpl list of ROMClasses stored under that name
	 * A HashLinkedListImpl is a reference to a ROMClass in the cache.
	 * Thus, given a ROMClass name, it is quick and easy to walk all known
	 * ROMClasses in the cache stored under that name.
	 *
	 * @ingroup Shared_Common
	 */
	J9Pool* _linkedListImplPool;


	bool checkTimestamp(J9VMThread* currentThread, const char* path, UDATA pathLen, ROMClassWrapper* wrapper, const ShcItem* item);

	bool reuniteOrphan(J9VMThread* currentThread, const char* romClassName, UDATA nameLen, const ShcItem* item, const J9ROMClass* romClassPtr);

	void initialize(J9JavaVM* vm, SH_SharedCache* cache, SH_TimestampManager* tsm, BlockPtr memForConstructor);

	static UDATA customCountItemsInList(void* node, void* countData);
};

#endif /* ROMCLASSMANAGERIMPL_H_INCLUDED */


