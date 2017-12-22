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

#if !defined(CLASSPATHMANAGERIMPL_H_INCLUDED)
#define CLASSPATHMANAGERIMPL_H_INCLUDED

/* @ddr_namespace: default */
#include "ClasspathManager.hpp"
#include "TimestampManager.hpp"
#include "classpathcache.h"
#include "j9.h"
#include "j9protos.h"

/**
 * Implementation of SH_ClasspathManager
 *
 * The Classpath manager has the job of indexing, locating and comparing Classpaths in the shared class cache.
 * Classpath entries are indexed (keyed) by entry name. Each entry maps to a list of Classpaths in which
 * the entry appears.
 * 
 * A hint may map to multiple Classpath entries.
 
 * @ingroup Shared_Common
 */
class SH_ClasspathManagerImpl2 : public SH_ClasspathManager
{
public:
	typedef char* BlockPtr;

	SH_ClasspathManagerImpl2();

	~SH_ClasspathManagerImpl2();

	static SH_ClasspathManagerImpl2* newInstance(J9JavaVM* vm, SH_SharedCache* cache, SH_TimestampManager* tsm, SH_ClasspathManagerImpl2* memForConstructor);

	static UDATA getRequiredConstrBytes(void);

	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet);

	virtual IDATA update(J9VMThread* currentThread, ClasspathItem* cp, I_16 cpeIndex, ClasspathWrapper** foundCP);

	virtual IDATA validate(J9VMThread* currentThread, ROMClassWrapper* foundROMClass, ClasspathItem* compareTo, IDATA confirmedEntries, I_16* foundAtIndex, ClasspathEntryItem** staleItem);

	virtual void notifyClasspathEntryStateChange(J9VMThread* currentThread, const J9UTF8* path, UDATA newState);

	virtual void markClasspathsStale(J9VMThread* currentThread, ClasspathEntryItem* cpei);

	virtual void setTimestamps(J9VMThread* currentThread, ClasspathWrapper* cpw);

	virtual bool isStale(ClasspathWrapper* cpw);

	virtual bool touchForClassFiles(J9VMThread* currentThread, const char* className, UDATA classNameLen, ClasspathItem* cp, I_16 toIndex);

	virtual void getNumItemsByType(UDATA* numClasspaths, UDATA* numURLs, UDATA* numTokens);

	void runExitCode(void) {};	

protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	virtual IDATA localPostStartup(J9VMThread* currentThread);

	virtual void localPostCleanup(J9VMThread* currentThread);

	virtual J9HashTable* localHashTableCreate(J9VMThread* currentThread, U_32 initialEntries);

	virtual IDATA localInitializePools(J9VMThread* currentThread);

	virtual void localTearDownPools(J9VMThread* currentThread);
		
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes);	

	HashLinkedListImpl* localHLLNewInstance(HashLinkedListImpl* memForConstructor) { return NULL; };
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints() { return true; }
	virtual IDATA createHintsForCachelet(J9VMThread* vmthread, SH_CompositeCache* cachelet, CacheletHints* hints);
	virtual IDATA primeHashtables(J9VMThread* vmthread, SH_CompositeCache* cachelet, U_8* hintsData, UDATA datalength);
#endif

private:
	SH_TimestampManager* _tsm;
	omrthread_monitor_t _identifiedMutex;
	J9Pool* _linkedListImplPool;
	struct J9ClasspathByIDArray* _identifiedClasspaths;
	UDATA _classpathCount, _urlCount, _tokenCount;
	bool _allCacheletsStarted;
	
	/** 
	 * Circular linked list for storing local classpath entry information.
	 * Local hashtable (cpeTable) has an entry for each known classpath entry.
	 *   Key is the string name of the classpath entry.
	 *   Value is a CpLinkedListHdr which contains a CpLinkedListImpl list.
	 * A CpLinkedListImpl is a reference to a classpath in the cache along
	 * with the classpath entry index of the Key.
	 * Thus, given a classpath entry, it is quick and easy to walk all known
	 * classpaths in the cache which contain that entry.
	 * 
	 * @ingroup Shared_Common
	 */
class CpLinkedListImpl : public SH_Manager::LinkedListImpl
{
	public:
		static CpLinkedListImpl* newInstance(I_16 CPEIndex, const ShcItem* item, SH_CompositeCache* cachelet, CpLinkedListImpl* memForConstructor);
		static CpLinkedListImpl* link(CpLinkedListImpl* addToList, I_16 CPEIndex, const ShcItem* item, bool doTag, SH_CompositeCache* cachelet, J9Pool* allocationPool);
		CpLinkedListImpl* forCacheItem(J9VMThread* currentThread, ClasspathItem* item, UDATA index);
		static void tag(CpLinkedListImpl* item);
		static I_16 getCPEIndex(CpLinkedListImpl* item);
	private:
		I_16 _CPEIndex;
		void initialize(I_16 CPEIndex_, const ShcItem* item_, SH_CompositeCache* cachelet_);
	};

	/** 
	 * Header for storing CpLinkedListImpl list.
	 * Contains local data about classpath entry (Key).
	 *   isLocked is set to true if the classpath entry is locked by the
	 *   classloader and therefore does not need timestamp checking.
	 * isToken is important. A Token with "/foo/bar" is different to a URL with "/foo/bar".
	 *
	 * @ingroup Shared_Common
	 */
	class CpLinkedListHdr {
	public:
		CpLinkedListHdr(const char* key_, U_16 keySize_, U_8 isToken_, CpLinkedListImpl* listItem);

		~CpLinkedListHdr();

		U_8 _isToken;
		U_8 _flags;
		U_16 _keySize;
		const char* _key;
		CpLinkedListImpl* _list;
	};

	static UDATA cpeHashFn(void* item, void *userData);
	static UDATA cpeHashEqualFn(void* left, void* right, void *userData);
	
	CpLinkedListHdr* cpeTableLookup(J9VMThread* currentThread, const char* key, U_16 keySize, U_8 isToken);
	CpLinkedListHdr* cpeTableLookupHelper(J9VMThread* currentThread, const char* key, U_16 keySize, U_8 isToken);
	CpLinkedListHdr* cpeTableLookupHelper(J9VMThread* currentThread, CpLinkedListHdr* searchItem);

	CpLinkedListHdr* cpeTableAddHeader(J9VMThread* currentThread, const char* name, U_16 nameLen, CpLinkedListImpl* newItem, U_8 isToken);
	CpLinkedListImpl* cpeTableAdd(J9VMThread* currentThread, const char* name, U_16 nameLen, I_16 CPEIndex, const ShcItem* item, U_8 isToken, bool doTag, SH_CompositeCache* cachelet);

	CpLinkedListImpl* cpeTableUpdate(J9VMThread* currentThread, const char* name, U_16 nameLen, I_16 CPEIndex, const ShcItem* item, U_8 isToken, bool doTag, SH_CompositeCache* cachelet);
	
	static UDATA cpeCollectHashOfEntry(void* entry, void* userData);
	static UDATA cpeCountCacheletHashes(void* entry, void* userData);
	IDATA cpeCollectHashes(J9VMThread* currentThread, SH_CompositeCache* cachelet, CacheletHints* hints);

	UDATA testForClasspathReset(J9VMThread* currentThread);

	ClasspathWrapper* localUpdate_FindIdentified(J9VMThread* currentThread, ClasspathItem* cp);

	IDATA localValidate_FindIdentified(J9VMThread* currentThread, ClasspathWrapper* testCP, IDATA walkFromID);

	IDATA local_StoreIdentified(J9VMThread* currentThread, ClasspathItem* cp, ClasspathWrapper* foundCP);

	ClasspathWrapper* localUpdate_CheckManually(J9VMThread* currentThread, ClasspathItem* cp, CpLinkedListHdr** knownLLH);

	I_16 localValidate_CheckAndTimestampManually(J9VMThread* currentThread, ClasspathWrapper* cpInCache, I_16 testCPIndex, 
			ClasspathItem* compareTo, IDATA foundIdentified, bool* addToIdentified, ClasspathEntryItem** staleItem);

	IDATA hasTimestampChanged(J9VMThread* currentThread, ClasspathEntryItem* itemToCheck, CpLinkedListHdr* knownLLH, bool doTryLockJar);
	
	bool isIdentifiedClasspathUpdated(ClasspathItem* cpInCache, I_16 cpeIndex, IDATA stopAtIndex);

	void initialize(J9JavaVM* vm, SH_SharedCache* cache, SH_TimestampManager* tsm, BlockPtr memForConstructor);
};

#endif /* !defined(CLASSPATHMANAGERIMPL_H_INCLUDED) */


