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
extern "C"
{
#include "shrinit.h"
}
#include "AbstractMemoryPermission.hpp"
#include "CompositeCacheImpl.hpp"
#include "OSCachemmap.hpp"
#include "OpenCacheHelper.h"

#define ONE_K_BYTES 1024
#define TWO_K_BYTES (2 * ONE_K_BYTES)

#define TEST4_MIN_AOT_SIZE (4 * ONE_K_BYTES)
#define TEST5_MIN_JIT_SIZE (4 * ONE_K_BYTES)

#define TEST7_MIN_AOT_SIZE (5 * ONE_K_BYTES)
#define TEST7_MIN_JIT_SIZE (5 * ONE_K_BYTES)

#define TEST9_MIN_AOT_SIZE (4 * ONE_K_BYTES)
#define TEST9_MIN_JIT_SIZE (4 * ONE_K_BYTES)
#define TEST9_SOFTMX_SIZE (20 * ONE_K_BYTES)

#define ROMCLASS_NAME_LEN 32

#define UPDATEPTR(ca) (((BlockPtr)(ca)) + (ca)->updateSRP)
#define SEGUPDATEPTR(ca) (((BlockPtr)(ca)) + (ca)->segmentSRP)

class TestOSCache : public SH_OSCachemmap {
public:
	void * protectedAddress;
	UDATA protectedLength;

	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	TestOSCache() {
		protectedAddress = NULL;
		protectedLength = 0;
	}

	static UDATA getRequiredConstrBytes(void)
	{
		return sizeof(TestOSCache);
	}
	
	IDATA releaseWriteLock(UDATA lockid)
	{
		return 0;
	}
	
	virtual IDATA acquireWriteLock(UDATA lockid)
	{
		return 0;
	}

	virtual void cleanup()
	{
		return;
	}

	IDATA setRegionPermissions(J9PortLibrary* portLibrary, void *address, UDATA length, UDATA flags) {
		protectedAddress = address;
		protectedLength = length;
		return 0;
	}
	
	void resetTestVars() {
		protectedAddress = NULL;
		protectedLength = 0;
	}
};


IDATA test1(J9JavaVM* vm);
IDATA test2(J9JavaVM* vm);
IDATA test3(J9JavaVM* vm);
IDATA test4(J9JavaVM* vm);
IDATA test5(J9JavaVM* vm);
IDATA test6(J9JavaVM* vm);
IDATA test7(J9JavaVM* vm);
IDATA test8(J9JavaVM* vm);
IDATA test9(J9JavaVM* vm);

extern "C" {

IDATA
testCacheFull(J9JavaVM* vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);
	REPORT_START("testCacheFull");

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

	rc |= test1(vm);
	rc |= test2(vm);
	rc |= test3(vm);
	rc |= test4(vm);
	rc |= test5(vm);
	rc |= test6(vm);
	rc |= test7(vm);
	rc |= test8(vm);
	rc |= test9(vm);

	vm->internalVMFunctions->internalExitVMToJNI(currentThread);

	REPORT_SUMMARY("testCacheFull", rc);
	return rc;
}

} /* extern "C" */

/**
 * This test is for case when the amount of free block bytes is > CC_MIN_SPACE_BEFORE_CACHE_FULL,
 * but the request for allocating a new ROMClass is more than free block bytes.
 * In such case, cache should not be marked full.
 * There is no reserved space for AOT/JIT data.
 *
 * Add a romclass with size > free block bytes > CC_MIN_SPACE_BEFORE_CACHE_FULL.
 * Verify cache is not marked full.
 */
IDATA test1(J9JavaVM* vm) {
	const char* testName = "test1";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 romClassSize = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA oldSegmentPtr = 0;
	UDATA newSegmentPtr = 0;
	UDATA oldUpdatePtr = 0;
	UDATA newUpdatePtr = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	char romClassName[ROMCLASS_NAME_LEN];

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;
	
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	/* Set up segmentSRP such that (updateSRP - segmentSRP) is CC_MIN_SPACE_BEFORE_CACHE_FULL + 1K bytes */
	ca->segmentSRP = ca->updateSRP - (CC_MIN_SPACE_BEFORE_CACHE_FULL + ONE_K_BYTES);

	/* Ensure free space is more than CC_MIN_SPACE_BEFORE_CACHE_FULL */
	if (cc->getFreeBlockBytes() < CC_MIN_SPACE_BEFORE_CACHE_FULL) {
		ERRPRINTF2("Error: Newly created cache does not have enough space. Free bytes: %d, expected to be > %d", \
				cc->getFreeBlockBytes(), CC_MIN_SPACE_BEFORE_CACHE_FULL);
		rc = FAIL;
		goto done;
	}

	INFOPRINTF("updateSRP and segmentSRP is set up at required positions");

	myOSCache->resetTestVars();

	oldSegmentPtr = (UDATA) cc->getSegmentAllocPtr();
	oldUpdatePtr = (UDATA) cc->getMetaAllocPtr();

	/* Add a ROMClass with size more than available bytes */
	romClassSize = CC_MIN_SPACE_BEFORE_CACHE_FULL + (2 * ONE_K_BYTES);
	romClassSize = ROUND_UP_TO(sizeof(U_64), romClassSize);
	j9str_printf(PORTLIB, romClassName, ROMCLASS_NAME_LEN, "%s_DummyClass1", testName);
	if (PASS == cacheHelper.addDummyROMClass(romClassName, romClassSize)) {
		ERRPRINTF("Error: Successfully added dummy class twice the size of cache!");
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Call to add a dummy ROMClass with size more than available bytes failed as expected");
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Cache is not marked as full");
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	newSegmentPtr = (UDATA) cc->getSegmentAllocPtr();
	newUpdatePtr = (UDATA) cc->getMetaAllocPtr();

	if (oldSegmentPtr != newSegmentPtr) {
		ERRPRINTF("Error: Did not expect segmentSRP to change if ROMClass allocation failed");
		rc = FAIL;
		goto done;
	}

	if (oldUpdatePtr != newUpdatePtr) {
		ERRPRINTF("Error: Did not expect updateSRP to change if ROMClass allocation failed");
		rc = FAIL;
		goto done;
	}

	INFOPRINTF("No new data was added to the cache");

	if ((0 != myOSCache->protectedLength) || (NULL != myOSCache->protectedAddress)) {
		ERRPRINTF("Error: Did not expect to mprotect any new pages if ROMClass allocation failed");
		rc = FAIL;
		goto done;
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");
	return rc;
}

/**
 * This test is for case when segmentSRP and updateSRP are on same page.
 * There is no reserved space for AOT/JIT data.
 *
 * Add a ROMClass such that it touches the last free CC_MIN_SPACE_BEFORE_CACHE_FULL block of memory in cache.
 * Verify cache is marked full and dummy data is added to the cache.
 *
 * Reuse the cache and verify cache full flags are set in the cache header.
 *
 * It also verifies that locking the cache unprotects the last metadata page, and unlocking the cache protects it back.
 */
IDATA test2(J9JavaVM* vm) {
	const char* testName = "test2";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result = NULL;
	ShcItem item;
	ShcItem *itemPtr = &item;
	ShcItem *dummyDataItem = NULL;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 romClassSize = 0;
	U_32 itemLen = 0;
	U_32 dummyDataLen = 0;
	U_32 i = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA updatePtrPage = 0;
	UDATA segmentPtrPage = 0;
	UDATA expectedUpdateSRP = 0;
	UDATA requiredFreeBytes = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	U_8 *dummyData = NULL;
	char romClassName[ROMCLASS_NAME_LEN];

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	/**
	 *  <------------ 4K ----------->|<----------- 4K ------------>
	 *   _______________________________________________________________________
	 *  |                            |                             |            |
	 *  |                            |                             |     ~~     |
	 *  |____________________________|_____________________________|____________|
	 *                                                      |<-1K->|
	 *  ^                                                   ^
	 * segmentSRP                                       updateSRP
	 *
	 *                                        Fig 2-1
	 *
	 *
	 *  <------------ 4K ----------->|<----------- 4K ------------>
	 *   _______________________________________________________________________
	 *  |                            |                             |            |
	 *  |                            |                             |     ~~     |
	 *  |____________________________|_____________________________|____________|
	 *                                      |--------------|<-1K->|
	 *                                      ^      |       ^
	 *                               segmentSRP    |    updateSRP
	 *                                             |-> (CC_MIN_SPACE_BEFORE_CACHE_FULL - 1K)
	 *
	 *                                        Fig 2-2
	 *
	 */

	/* Adding data to set up segmentSRP and updateSRP at required positions (as shown in Fig 2-1) */

	expectedUpdateSRP = (UDATA) SEGUPDATEPTR(ca) + 2*pageSizeToUse - ONE_K_BYTES - (UDATA)ca;

	itemLen = (U_32) (ca->updateSRP - expectedUpdateSRP - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	/* Verify updateSRP is set to expected value */
	if (ca->updateSRP != expectedUpdateSRP) {
		ERRPRINTF2("Error: updateSRP is not set to expected value.\n" \
				"current updateSRP: 0x%zx\t expectedUpdateSRP: 0x%zx", ca->updateSRP, expectedUpdateSRP);
	}

	/* Ensure free space is more than CC_MIN_SPACE_BEFORE_CACHE_FULL */
	if (cc->getFreeBlockBytes() < CC_MIN_SPACE_BEFORE_CACHE_FULL) {
		ERRPRINTF2("Error: Cache does not have enough space. Free bytes: %d, expected to be > %d", \
				cc->getFreeBlockBytes(), CC_MIN_SPACE_BEFORE_CACHE_FULL);
		rc = FAIL;
		goto done;
	}

	INFOPRINTF("segmentSRP and updateSRP are set up at required positions");
	/* Now segmentSRP and updateSRP are set at desired positions as shown in Fig 2-1 */

	myOSCache->resetTestVars();

	/* Add a ROMClass such that segmentSRP and updateSRP lie in same page as shown in Fig 2-2 */
	requiredFreeBytes = CC_MIN_SPACE_BEFORE_CACHE_FULL - ONE_K_BYTES;
	romClassSize = (U_32)((ca->updateSRP - requiredFreeBytes) - ca->segmentSRP);
	romClassSize = ROUND_UP_TO(sizeof(U_64), romClassSize);
	j9str_printf(PORTLIB, romClassName, ROMCLASS_NAME_LEN, "%s_DummyClass1", testName);
	if (FAIL == cacheHelper.addDummyROMClass(romClassName, romClassSize)) {
		ERRPRINTF("Error: Failed to add dummy class to the cache.");
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Added a dummy ROMClass to set free block bytes less than CC_MIN_SPACE_BEFORE_CACHE_FULL");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));
	segmentPtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) SEGUPDATEPTR(ca));

	if (cc->getFreeBlockBytes() >= (I_32) (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)) {
		ERRPRINTF2("Error: Free block bytes in cache are more than expected. Free block bytes=%d, expected to be < %d", \
				cc->getFreeBlockBytes(), (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE));
		rc = FAIL;
		goto done;
	}

	if (updatePtrPage != segmentPtrPage) {
		ERRPRINTF2("Error: Expected updateSRP and segmentSRP to be on same page. updateSRP: %lld, segmentSRP: %lld", \
				ca->updateSRP, ca->segmentSRP);
		rc = FAIL;
		goto done;
	}

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Cache is marked as full");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should mprotect one page of data corresponding to metadata (the last data page). */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedLength);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	/* Verify dummy data is added to the cache */
	dummyDataItem = (ShcItem *)cc->getMetaAllocPtr();
	if (TYPE_UNINDEXED_BYTE_DATA != ITEMTYPE(dummyDataItem)) {
		ERRPRINTF2("Error: Did not find expected data type in last metedata item.\n" \
				"Expected type for dummy data = %d, data type found = %d", TYPE_UNINDEXED_BYTE_DATA, ITEMTYPE(dummyDataItem));
		rc = FAIL;
		goto done;
	}

	dummyData = ITEMDATA(dummyDataItem);
	dummyDataLen = ITEMDATALEN(dummyDataItem);

	for (i = 0; i < dummyDataLen; i++) {
		if (J9SHR_DUMMY_DATA_BYTE != dummyData[i]) {
			ERRPRINTF2("Error: Did not find dummy data of expected length. Expected dummy data length: %d bytes, found: %d bytes", \
					dummyDataLen, i);
			rc = FAIL;
			goto done;
		}
	}

	INFOPRINTF("Found dummy data in the cache");

	cacheHelper.closeTestCache(false);

	/* Reuse the cache and verify runtime flags are as expected */
	INFOPRINTF("Reusing the cache");

	myOSCache->resetTestVars();

	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to reuse existing cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Cache is marked as full");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should protect one page of data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected protectLastDataPage to mprotect one page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedLength);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	myOSCache->resetTestVars();

	/* Acquire write mutex and lock the cache. This would result in unprotecting whole of metadata area */
	cc->enterWriteMutex(vm->mainThread, true, testName);
	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the start of metadata area unprotected to be same as updateSRP page. " \
				"Metadata area unprotected from: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Locking the cache unprotected the last metadata page");
	}

	myOSCache->resetTestVars();

	/* Releasing the write mutex should re-protect the metadata area, along with the last metadata page */
	cc->exitWriteMutex(vm->mainThread, testName);
	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the start of metadata area protected to be same as updateSRP page. " \
				"Metadata area protected from: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Unlocking the cache re-protected the last metadata page");
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;

	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");

	return rc;
}

/**
 * This test is for the case when segmentSRP and updateSRP are on different page.
 * There is no reserved space for AOT/JIT data.
 *
 * Set up segmentSRP and updateSRP such that free block bytes is more than CC_MIN_SPACE_BEFORE_CACHE_FULL.
 * Add a ROMClass such that free block bytes goes below (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE),
 * but segmentSRP and updateSRP are on different page.
 * Verify cache is marked full and no dummy data is added to the cache.
 *
 * Two pages will be protected by protectLastUnusedPages().
 *
 * Reuse the cache and verify cache full flags are set in the cache header.
 */
IDATA test3(J9JavaVM* vm) {
	const char* testName = "test3";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	ShcItem item;
	ShcItem *itemPtr = &item;
	ShcItem *lastItem = NULL;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 itemLen = 0;
	U_32 romClassSize = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA updatePtrPage = 0;
	UDATA segmentPtrPage = 0;
	UDATA expectedUpdateSRP = 0;
	UDATA orphanItemSize = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	char romClassName[ROMCLASS_NAME_LEN];

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}
	
	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	/**
	 *  |<------------- 4K ------------>|<------------ 4K ------------>|
	 *   ___________________________________________________________________________
	 *  |                               |                              |            |
	 *  |                               |                              |     ~~     |
	 *  |_______________________________|______________________________|____________|
	 *  |                         |<----X+d---->|
	 *  V                         |<-8->|       V
	 * segmentSRP                           updateSRP
	 *
	 * 	                                    Fig 3-1
	 *
	 *
	 *  |<------------- 4K ------------>|<------------ 4K ------------>|
	 *   ___________________________________________________________________________
	 *  |                               |                              |            |
	 *  |                               |                              |     ~~     |
	 *  |_______________________________|______________________________|____________|
	 *                            |<----X--->|
	 *                            |<-8->|    |
	 *                            V          V
	 *                       segmentSRP    updateSRP
	 *
	 *                                      Fig 3-2
	 *
	 * In above figs:
	 *		'X' equals (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE - sizeof(U_32))
	 *		'd' equals size of metadata required for TYPE_ORPHAN
	 *
	 *
	 */

	/* Adding data to set up segmentSRP and updateSRP at required positions (as shown in Fig 3-1) */
	orphanItemSize = sizeof(ShcItem) + sizeof(ShcItemHdr) + sizeof(OrphanWrapper);

	expectedUpdateSRP = (UDATA) SEGUPDATEPTR(ca) +
						pageSizeToUse -
						(2 * sizeof(U_32)) +
						orphanItemSize +
						(J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE - sizeof(U_32)) -
						(UDATA)ca;

	itemLen = (U_32) (ca->updateSRP - expectedUpdateSRP - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	/* Verify updateSRP is set to expected value */
	if (ca->updateSRP != expectedUpdateSRP) {
		ERRPRINTF2("Error: updateSRP is not set to expected value.\n" \
				"current updateSRP: 0x%zx\t expectedUpdateSRP: 0x%zx", ca->updateSRP, expectedUpdateSRP);
	}

	/* Ensure free space is more than page size */
	if (cc->getFreeBlockBytes() <= (I_32)pageSizeToUse) {
		ERRPRINTF2("Error: updateSRP and segmentSRP are not properly set.\n" \
				"Expected free bytes to be more than: %d\t" \
				"Free bytes available: ", pageSizeToUse, cc->getFreeBlockBytes());
		rc = FAIL;
		goto done;
	}

	INFOPRINTF("updateSRP and segmentSRP is set up at required positions");
	/* Now segmentSRP and updateSRP are set to required position as shown in Fig 3-1 */

	myOSCache->resetTestVars();

	/* Add a ROMClass of size (1 page - 8 bytes). This will bring cache to the state shown in Fig 3-2 */
	romClassSize = (U_32) (pageSizeToUse - (2*sizeof(U_32)));
	romClassSize = ROUND_UP_TO(sizeof(U_64), romClassSize);
	j9str_printf(PORTLIB, romClassName, ROMCLASS_NAME_LEN, "%s_DummyClass1", testName);
	if (FAIL == cacheHelper.addDummyROMClass(romClassName, romClassSize)) {
		ERRPRINTF("Error: Failed to add dummy class to the cache.");
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Added a dummy ROMClass to set free block bytes less than (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));
	segmentPtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) SEGUPDATEPTR(ca));

	if (updatePtrPage == segmentPtrPage) {
		ERRPRINTF2("Error: Expected updateSRP and segmentSRP to be on different page. updateSRP: %p, segmentSRP: %p", \
				UPDATEPTR(ca), SEGUPDATEPTR(ca));
		rc = FAIL;
		goto done;
	}

	if (cc->getFreeBlockBytes() >= (I_32) (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)) {
		ERRPRINTF2("Error: Expected free bytes to be less than (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)\n" \
				"Free bytes available: %d, expected to be < %d", cc->getFreeBlockBytes(), (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE));
		rc = FAIL;
		goto done;
	}

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Cache is marked as full");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if ((UDATA) myOSCache->protectedAddress != segmentPtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as segmentSRP page. " \
				"Last page protected: %p, segmentSRP page: %p", \
				myOSCache->protectedAddress, segmentPtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPage() should mprotect two pages of data:
	 * 	one corresponding to the ROMClass and another corresponding to metadata.
	 */
	if (myOSCache->protectedLength != (2 * pageSizeToUse)) {
		ERRPRINTF1("Error: Expected to mprotect two pages of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedLength);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last two data pages are protected");
	}

	/* Verify that we did not add any dummy data to the cache */
	lastItem = (ShcItem *) UPDATEPTR(ca);
	if (TYPE_UNINDEXED_BYTE_DATA == ITEMTYPE(lastItem)) {
		ERRPRINTF("ERROR: Did not expect last item in cache to be unindexed byte data (dummy data)");
		rc = FAIL;
		goto done;
	}

	INFOPRINTF("Did not find any dummy data");

	cacheHelper.closeTestCache(false);

	/* Reuse the cache and verify runtime flags are as expected */
	INFOPRINTF("Reusing the cache");

	myOSCache->resetTestVars();

	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to reuse existing cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Cache is marked as full");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	segmentPtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) SEGUPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != segmentPtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should protect two pages of data. */
	if (myOSCache->protectedLength != (2 * pageSizeToUse)) {
		ERRPRINTF1("Error: Expected to mprotect two pages of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedLength);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");

	return rc;
}

/**
 * This test is for the case when free block bytes is exactly equal to (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE).
 * There is no reserved space for AOT/JIT data.
 *
 * Add data to the cache to set up free block bytes equal to (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE).
 * At this point verify dummy data is added to the cache and all the runtime cache full flags are set.
 */
IDATA test4(J9JavaVM* vm) {
	const char* testName = "test4";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	ShcItem item;
	ShcItem *itemPtr = &item;
	ShcItem *dummyDataItem = NULL;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 itemLen = 0;
	U_32 romClassSize = 0;
	U_32 dummyDataLen = 0;
	U_32 i = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA expectedUpdateSRP = 0;
	UDATA freeBlockBytes = 0;
	UDATA orphanItemSize = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	U_8 *dummyData = NULL;
	char romClassName[ROMCLASS_NAME_LEN];

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	/**
	 *  |<------------- 4K ------------>|<------------ 4K ------------>|
	 *   ___________________________________________________________________________
	 *  |                               |                              |            |
	 *  |                               |                              |     ~~     |
	 *  |_______________________________|______________________________|____________|
	 *                                  |<--X+d-->|
	 *  ^                                         ^
	 * segmentSRP                             updateSRP
	 *
	 *
	 *                                    Fig 4-1
	 *
	 *  |<------------- 4K ------------>|<------------ 4K ------------>|
	 *   ___________________________________________________________________________
	 *  |                               |                              |            |
	 *  |                               |                              |     ~~     |
	 *  |_______________________________|______________________________|____________|
	 *                                  |<--X-->|
	 *                                  ^       ^
	 *                           segmentSRP   updateSRP
	 *
	 *                                    Fig 4-2
	 *  In above fig:
	 *  	'X' equals (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)
	 *		'd' equals size of metadata required for TYPE_ORPHAN
	 *
	 */

	/* Adding data to set up segmentSRP and updateSRP at required positions (as shown in Fig 3-1) */
	orphanItemSize = sizeof(ShcItem) + sizeof(ShcItemHdr) + sizeof(OrphanWrapper);

	expectedUpdateSRP = (UDATA) SEGUPDATEPTR(ca) +
						pageSizeToUse +
						orphanItemSize +
						(J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE) -
						(UDATA)ca;

	itemPtr = &item;
	itemLen = (U_32) (ca->updateSRP - expectedUpdateSRP - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	/* Verify updateSRP is set to expected value */
	if (ca->updateSRP != expectedUpdateSRP) {
		ERRPRINTF2("Error: updateSRP is not set to expected value.\n" \
				"current updateSRP: 0x%zx\t expectedUpdateSRP: 0x%zx", ca->updateSRP, expectedUpdateSRP);
	}

	freeBlockBytes = cc->getFreeBlockBytes();

	/* Ensure free space is more than page size */
	if (freeBlockBytes <= pageSizeToUse) {
		ERRPRINTF2("Error: updateSRP and segmentSRP are not properly set.\n" \
				"Expected free block bytes to be more than: %d\t" \
				"Free block bytes available: ", pageSizeToUse, freeBlockBytes);
		rc = FAIL;
		goto done;
	}

	INFOPRINTF("updateSRP and segmentSRP is set up at required positions");
	/* Now segmentSRP and updateSRP are desired positions as shown in Fig 3-1 */

	/* Add a ROMClass to set segmentSRP and updateSRP to same value as shown in Fig 3-2 */
	romClassSize = (U_32) pageSizeToUse;
	romClassSize = ROUND_UP_TO(sizeof(U_64), romClassSize);
	j9str_printf(PORTLIB, romClassName, ROMCLASS_NAME_LEN, "%s_DummyClass1", testName);
	if (FAIL == cacheHelper.addDummyROMClass(romClassName, romClassSize)) {
		ERRPRINTF("Error: Failed to add dummy class to the cache.");
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Added a dummy ROMClass to set available block bytes equal to (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected block space to be marked full. runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Block space is marked as full");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	freeBlockBytes = cc->getFreeBlockBytes();
	if (freeBlockBytes != J9SHR_MIN_GAP_BEFORE_METADATA) {
		ERRPRINTF2("Error: Expected free block bytes to be equal to (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)\n" \
				"Free block bytes available: %d, expected to be %d", freeBlockBytes, J9SHR_MIN_GAP_BEFORE_METADATA);
		rc = FAIL;
		goto done;
	}

	/* Verify dummy data is added to the cache */
	dummyDataItem = (ShcItem *)cc->getMetaAllocPtr();
	if (TYPE_UNINDEXED_BYTE_DATA != ITEMTYPE(dummyDataItem)) {
		ERRPRINTF2("Error: Did not find expected data type in last metedata item.\n" \
				"Expected type for dummy data = %d, data type found = %d", TYPE_UNINDEXED_BYTE_DATA, ITEMTYPE(dummyDataItem));
		rc = FAIL;
		goto done;
	}

	dummyData = ITEMDATA(dummyDataItem);
	dummyDataLen = ITEMDATALEN(dummyDataItem);

	for (i = 0; i < dummyDataLen; i++) {
		if (J9SHR_DUMMY_DATA_BYTE != dummyData[i]) {
			ERRPRINTF2("Error: Did not find dummy data of expected length. Expected dummy data length: %d bytes, found: %d bytes", \
					dummyDataLen, i);
			rc = FAIL;
			goto done;
		}
	}

	INFOPRINTF("Found dummy data in the cache");

	cacheHelper.closeTestCache(false);

	/* Reuse the cache and verify runtime flags are as expected */
	INFOPRINTF("Reusing the cache");

	cacheHelper.setOsPageSize(pageSizeToUse);

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST) == FAIL) {
		ERRPRINTF1("Failed to reuse existing cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected block space to be marked full on startup. runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Block space is marked as full");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	INFOPRINTF("End\n");
	cacheHelper.closeTestCache(true);
	return rc;
}

/**
 * This test reserves TEST4_MIN_AOT_SIZE space for AOT data.
 * It performs following actions:
 * 	- Consumes up all available block bytes.
 * 	- Allocate (TEST4_MIN_AOT_SIZE - 1K) of AOT data. This should pass.
 * 	- Allocate 2K of AOT data. This should fail.
 * 	- Reopen the cache.
 * 	- Verify runtime cache full flags are as expected.
 * 	- Allocate 0.5K of AOT data. This should fail.
 */
IDATA test5(J9JavaVM* vm) {
	const char* testName = "test5";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	J9SharedClassPreinitConfig *piConfig = NULL;
	J9SharedClassConfig *sharedClassConfig = NULL;
	ShcItem item;
	ShcItem *itemPtr = &item;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 itemLen = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA updatePtrPage = 0;
	UDATA freeBlockBytes = 0;
	UDATA dataLength = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	U_8 *dataStart = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinAOTSize = TEST4_MIN_AOT_SIZE;
	piConfig->sharedClassMaxAOTSize = -1;
	piConfig->sharedClassSoftMaxBytes = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeCompiledMethod = j9shr_storeCompiledMethod;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;
	
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	freeBlockBytes = cc->getFreeBlockBytes();

	/* Add enough metadata to the cache to mark it as full */
	itemLen = (U_32) (freeBlockBytes - (J9SHR_MIN_GAP_BEFORE_METADATA + ONE_K_BYTES) - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	INFOPRINTF("Added metadata to the cache to consume free block bytes");

	/* Verify flags for BLOCK and JIT space are set */
	if ((J9SHR_AVAILABLE_SPACE_FULL| J9SHR_BLOCK_SPACE_FULL | J9SHR_JIT_SPACE_FULL) != (ca->cacheFullFlags & J9SHR_ALL_CACHE_FULL_BITS)) {
		ERRPRINTF1("Error: Expected (J9SHR_AVAILABLE_SPACE_FULL | J9SHR_BLOCK_SPACE_FULL | J9SHR_JIT_SPACE_FULL) to be bet in cache header." \
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL flag is not set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (ca->minAOT != cc->getAvailableReservedAOTBytes(vm->mainThread)) {
		ERRPRINTF2("Error: Expected available reserved AOT bytes to be equal to minAOT\n" \
				"minAOT=%d, available reserved AOT bytes=%d", ca->minAOT, cc->getAvailableReservedAOTBytes(vm->mainThread));
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	/* At this stage, we only have reserved AOT space (= TEST4_MIN_AOT_SIZE bytes) available in cache */
	memset(itemPtr, 0, sizeof(ShcItem));
	/* Allocate AOT data of size (TEST4_MIN_AOT_SIZE - 1K) bytes */
	dataLength = (TEST4_MIN_AOT_SIZE - ONE_K_BYTES) - sizeof(CompiledMethodWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for AOT data");
		rc = FAIL;
		goto done;
	}
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), dataStart, dataLength, NULL, 0, true);
	j9mem_free_memory(dataStart);
	if (true == cc->isAddressInCache(result)) {
		INFOPRINTF("Successfully allocated 3K of AOT data");
	} else if (NULL == result) {
		ERRPRINTF("Error: storeCompiledMethod returned NULL");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		rc = FAIL;
		goto done;
	}

	if (J9SHR_AOT_SPACE_FULL == (ca->cacheFullFlags & J9SHR_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_AOT_SPACE_FULL to be set in cache header"\
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is not set");
	}

	/* Now, allocate AOT data of size 2K bytes. This is expected to fail, as only 1K of space is left for AOT data */
	dataLength = 2 * ONE_K_BYTES - sizeof(CompiledMethodWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for AOT data");
		rc = FAIL;
		goto done;
	}
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), dataStart, dataLength, NULL, 0, true);
	j9mem_free_memory(dataStart);
	if (J9SHR_RESOURCE_STORE_FULL != (UDATA) result) {
		if (true == cc->isAddressInCache(result)) {
			ERRPRINTF1("Error: Did not expect AOT data to be added to the cache. result=%p", result);
		} else if (NULL == result) {
			ERRPRINTF("Error: storeCompiledMethod returned NULL");
		} else {
			ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		}
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF1("Adding AOT data failed with expected error code=%d", (UDATA) result);
	}

	if (J9SHR_AOT_SPACE_FULL != (ca->cacheFullFlags & J9SHR_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_AOT_SPACE_FULL to be set in cache header" \
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		ERRPRINTF1("Error: Available reserved AOT bytes: %d\n", cc->getAvailableReservedAOTBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	}

	/* Verify J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL flag is set */
	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		ERRPRINTF1("Error: Available reserved AOT bytes: %d\n", cc->getAvailableReservedAOTBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should mprotect only 1 page corresponding to AOT data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedAddress);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	cacheHelper.closeTestCache(false);

	/* Reuse the cache and verify runtime flags are as expected */
	INFOPRINTF("Reusing the cache");

	myOSCache->resetTestVars();

	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinAOTSize = TEST4_MIN_AOT_SIZE;
	piConfig->sharedClassMaxAOTSize = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeCompiledMethod = j9shr_storeCompiledMethod;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to reuse existing cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should protect only 1 page of data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedAddress);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	myOSCache->resetTestVars();

	/* Try to add some AOT data again. It should fail again with J9SHR_RESOURCE_STORE_FULL */
	dataLength = (ONE_K_BYTES/2) - sizeof(CompiledMethodWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for AOT data");
		rc = FAIL;
		goto done;
	}
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), dataStart, dataLength, NULL, 0, true);
	j9mem_free_memory(dataStart);
	if (J9SHR_RESOURCE_STORE_FULL != (UDATA) result) {
		if (true == cc->isAddressInCache(result)) {
			ERRPRINTF1("Error: Did not expect AOT data to be added to the cache. result=%p", result);
		} else if (NULL == result) {
			ERRPRINTF("Error: storeCompiledMethod returned NULL");
		} else {
			ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		}
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF1("Adding AOT data failed with expected error code=%d", (UDATA) result);
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");

	return rc;
}

/**
 * This test is similar to test4.
 * This test reserves TEST5_MIN_JIT_SIZE bytes for JIT data.
 * It performs following actions:
 * 	- Consumes up all available block bytes.
 * 	- Allocate (TEST5_MIN_JIT_SIZE-1K) of JIT data. This should pass. Only 1K of free reserve JIT space is left.
 * 	- Allocate 2K of JIT data. This should fail, and set the cache full flags for JIT space.
 * 	- Reopen the cache.
 * 	- Verify runtime cache full flags are as expected.
 * 	- Allocate 0.5K of JIT data. This should fail.
 */
IDATA test6(J9JavaVM* vm) {
	const char* testName = "test6";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	J9SharedClassPreinitConfig *piConfig = NULL;
	J9SharedClassConfig *sharedClassConfig = NULL;
	ShcItem item;
	ShcItem *itemPtr = &item;
	J9SharedDataDescriptor dataDescriptor;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 itemLen = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA updatePtrPage = 0;
	UDATA freeBlockBytes = 0;
	UDATA dataLength = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	U_8 *dataStart = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinJITSize = TEST5_MIN_JIT_SIZE;
	piConfig->sharedClassMaxJITSize = -1;
	piConfig->sharedClassSoftMaxBytes = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;
	
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	freeBlockBytes = cc->getFreeBlockBytes();

	/* Add enough metadata to the cache mark it as full */
	itemLen = (U_32) (freeBlockBytes - (J9SHR_MIN_GAP_BEFORE_METADATA + ONE_K_BYTES) - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	INFOPRINTF("Added metadata to the cache to consume free block bytes");

	/* Verify flags for BLOCK and AOT space are marked full */
	if ((J9SHR_AVAILABLE_SPACE_FULL| J9SHR_BLOCK_SPACE_FULL | J9SHR_AOT_SPACE_FULL) != (ca->cacheFullFlags & J9SHR_ALL_CACHE_FULL_BITS)) {
		ERRPRINTF1("Error: Expected (J9SHR_AVAILABLE_SPACE_FULL| J9SHR_BLOCK_SPACE_FULL | J9SHR_AOT_SPACE_FULL) to be bet in cache header." \
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is not set");
	}

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (ca->minJIT != cc->getAvailableReservedJITBytes(vm->mainThread)) {
		ERRPRINTF2("Error: Expected available reserved JIT bytes to be equal to minJIT\n" \
				"minAOT=%d, available reserved JIT bytes=%d", ca->minJIT, cc->getAvailableReservedJITBytes(vm->mainThread));
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	/* At this stage, we only have reserved JIT space (= TEST5_MIN_JIT_SIZE bytes) available in cache */

	/* Allocate JIT data of size (TEST5_MIN_JIT_SIZE-1K) bytes */
	dataLength = (TEST5_MIN_JIT_SIZE - ONE_K_BYTES) - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	j9mem_free_memory(dataStart);
	if (0 == rc) {
		INFOPRINTF1("Successfully allocated %d bytes of JIT data", dataLength);
		rc = PASS;
	} else {
		ERRPRINTF1("Error: Adding JIT data to cache failed with error code = %d", rc);
		rc = FAIL;
		goto done;
	}

	if (J9SHR_JIT_SPACE_FULL == (ca->cacheFullFlags & J9SHR_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_JIT_SPACE_FULL to be set in cache header"\
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is not set");
	}

	/* Now, allocate JIT data of size 2K bytes. This is expected to fail, as only 1K of space is left for JIT data */
	dataLength = 2 * ONE_K_BYTES - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	j9mem_free_memory(dataStart);
	if (J9SHR_RESOURCE_STORE_FULL != rc) {
		if (0 == rc) {
			ERRPRINTF("Error: Did not expect JIT data to be added to the cache");
		} else {
			ERRPRINTF1("Error: Adding JIT data to cache failed with unexpected error code = %d", rc);
		}
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF1("Adding JIT data failed with expected error code of result=%d", rc);
		rc = PASS;
	}

	if (J9SHR_JIT_SPACE_FULL != (ca->cacheFullFlags & J9SHR_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_JIT_SPACE_FULL to be set in cache header"\
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		ERRPRINTF1("Error: Available reserved JIT bytes: %d\n", cc->getAvailableReservedJITBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	}

	/* Verify J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL flag is set */
	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		ERRPRINTF1("Error: Available reserved JIT bytes: %d\n", cc->getAvailableReservedJITBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should mprotect only 1 page corresponding to AOT data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedAddress);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	cacheHelper.closeTestCache(false);

	/* Reuse the cache and verify runtime flags are as expected */
	INFOPRINTF("Reusing the cache");

	myOSCache->resetTestVars();

	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinJITSize = TEST5_MIN_JIT_SIZE;
	piConfig->sharedClassMaxJITSize = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to reuse existing cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should protect only 1 page of data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedLength);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	myOSCache->resetTestVars();

	/* Try to add some JIT data again. It should fail again with J9SHR_RESOURCE_STORE_FULL. */
	dataLength = (ONE_K_BYTES/2) - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	j9mem_free_memory(dataStart);
	if (J9SHR_RESOURCE_STORE_FULL != rc) {
		if (0 == rc) {
			ERRPRINTF("Error: Did not expect JIT data to be added to the cache");
		} else {
			ERRPRINTF1("Error: Adding JIT data to cache failed with unexpected error code = %d", rc);
		}
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF1("Adding JIT data failed with expected error code of result=%d", rc);
		rc = PASS;
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");

	return rc;
}

/**
 * This test adds JIT data to the last CC_MIN_SPACE_BEFORE_CACHE_FULL block of memory in the cache.
 * This should mark the cache full and protect last metadata page.
 * Cache is re-opened and the JIT data is updated.
 * It then verifies that last metadata page is re-protected.
 */
IDATA test7(J9JavaVM* vm) {
	const char* testName = "test7";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	J9SharedClassPreinitConfig *piConfig = NULL;
	J9SharedClassConfig *sharedClassConfig = NULL;
	ShcItem item;
	ShcItem *itemPtr = &item;
	J9SharedDataDescriptor dataDescriptor;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result;

	I_32 cacheSize = (1 * 1024 * 1024);
	U_32 itemLen = 0;
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	UDATA updatePtrPage = 0;
	UDATA oldFreeBlockBytes = 0;
	UDATA dataLength = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	U_8 *dataStart = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinJITSize =-1;
	piConfig->sharedClassMaxJITSize = -1;
	piConfig->sharedClassSoftMaxBytes = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;
	sharedClassConfig->updateAttachedData = j9shr_updateAttachedData;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;
	
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	oldFreeBlockBytes = cc->getFreeBlockBytes();

	/* Add enough metadata to the leave (CC_MIN_SPACE_BEFORE_CACHE_FULL + 1K) space in the cache */
	itemLen = (U_32) (oldFreeBlockBytes - CC_MIN_SPACE_BEFORE_CACHE_FULL - ONE_K_BYTES - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	INFOPRINTF("Added metadata to the cache to consume free block bytes");

	/* Allocate JIT data of size 2K bytes. This should mark cache as full. */
	dataLength = (2 * ONE_K_BYTES) - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	j9mem_free_memory(dataStart);
	if (0 == rc) {
		INFOPRINTF("Successfully allocated 3K of JIT data");
		rc = PASS;
	} else {
		ERRPRINTF1("Error: Adding JIT data to cache failed with error code = %d", rc);
		rc = FAIL;
		goto done;
	}

	/* Verify cache is marked full */
	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should mprotect only 1 page corresponding to AOT data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedAddress);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	cacheHelper.closeTestCache(false);

	/* Reuse the cache and verify runtime flags are as expected */
	INFOPRINTF("Reusing the cache");

	myOSCache->resetTestVars();

	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinJITSize =-1;
	piConfig->sharedClassMaxJITSize = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;
	sharedClassConfig->updateAttachedData = j9shr_updateAttachedData;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to reuse existing cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (false == cc->isCacheMarkedFull(vm->mainThread)) {
		ERRPRINTF1("Error: Expected J9SHR_ALL_CACHE_FULL_BITS to be bet in cache header. cacheFullFlags: 0x%zx", \
				ca->cacheFullFlags);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

	/* Call to protectLastUnusedPages() should protect only 1 page of data. */
	if (myOSCache->protectedLength != pageSizeToUse) {
		ERRPRINTF1("Error: Expected to mprotect 1 page of data. " \
				"Amount of data mprotected: 0x%zx", myOSCache->protectedLength);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("Last data page is protected");
	}

	myOSCache->resetTestVars();

	/* Update previously stored JIT data and ensure last metadata page remains protected after the update */
	dataLength = (2 * ONE_K_BYTES) - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(vm->mainThread, SEGUPDATEPTR(ca), 0, &dataDescriptor);
	j9mem_free_memory(dataStart);
	if (0 == rc) {
		INFOPRINTF("Successfully updated JIT data");
		rc = PASS;
	} else {
		ERRPRINTF1("Error: Updating JIT data to cache failed with error code = %d", rc);
		rc = FAIL;
		goto done;
	}

	/* Ensure last metadata page is protected after updating JIT data */

	updatePtrPage = ROUND_DOWN_TO(pageSizeToUse, (UDATA) UPDATEPTR(ca));

	if ((UDATA) myOSCache->protectedAddress != updatePtrPage) {
		ERRPRINTF2("Error: Expected the last data page mprotected to be same as updateSRP page. " \
				"Last page protected: %p, updateSRP page: %p", \
				myOSCache->protectedAddress, updatePtrPage);
		rc = FAIL;
		goto done;
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");

	return rc;
}

/**
 * This test is added for defect CMVC 193446.
 * AOT data should not consume space reserved for JIT data.
 * Same goes for JIT data.
 *
 * This test works as follows:
 * Reserve TEST7_MIN_AOT_SIZE bytes of AOT and TEST7_MIN_JIT_SIZE bytes of JIT space.
 * Performs following actions:
 * 	1) Except J9SHR_MIN_GAP_BEFORE_METADATA + 1K, consume all free block bytes. This also sets the flags for BLOCK space full.
 * 		Break up of free space:
 * 		Free block bytes = (J9SHR_MIN_GAP_BEFORE_METADATA + 1K) (not usable to store block data as the flags for BLOCK space full are set).
 * 		Reserved AOT = TEST7_MIN_AOT_SIZE
 * 		Reserve JIT = TEST7_MIN_JIT_SIZE
 * 	2) Allocate (TEST7_MIN_AOT_SIZE - 2K) of AOT data. This should pass.
 * 		Break up of free space:
 * 		Free block bytes = (J9SHR_MIN_GAP_BEFORE_METADATA + 1K) (not usable to store block data as the flags for BLOCK space full are set).
 * 		Reserved AOT = 2K
 * 		Reserve JIT = 5K
 * 	3) Allocate AOT data > (available reserved AOT bytes + free block bytes).
 * 		This should fail and set the cache header flag and runtime flag for AOT space full.
 * 		Break up of free space: same as in (2) above.
 * 	4) Allocate (TEST7_MIN_JIT_SIZE - 2K) of JIT data. This should pass.
 * 		Break up of free space:
 * 		Free block bytes = (J9SHR_MIN_GAP_BEFORE_METADATA + 1K) (not usable to store block data as the flags for BLOCK space full are set).
 * 		Reserved AOT = 2K
 * 		Reserve JIT = 2K
 * 	5) Allocate JIT data > (available reserved JIT bytes + free block bytes).
 * 		This should fail and set the cache header flag and runtime flag for JIT space full.
 * 		Break of free space: same as in (4) above.
 */
IDATA test8(J9JavaVM* vm) {
	const char* testName = "test8";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	J9SharedClassPreinitConfig *piConfig = NULL;
	J9SharedClassConfig *sharedClassConfig = NULL;
	J9SharedDataDescriptor dataDescriptor;
	ShcItem item;
	ShcItem *itemPtr = &item;
	OpenCacheHelper cacheHelper(vm);
	BlockPtr result;

	U_64 unsetRuntimeFlags = J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES;
	UDATA pageSizeToUse = 4096;
	UDATA oldFreeBlockBytes = 0;
	UDATA dataLength = 0;
	U_32 itemLen = 0;
	I_32 cacheSize = (1 * 1024 * 1024);
	I_32 availableReservedAOTBytes = 0;
	I_32 availableReservedJITBytes = 0;
	IDATA rc = PASS;
	U_8 *osCacheMem = NULL;
	U_8 *dataStart = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	if (NULL == (osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF1("Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	cacheHelper.setOsPageSize(pageSizeToUse);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinAOTSize = TEST7_MIN_AOT_SIZE;
	piConfig->sharedClassMaxAOTSize = -1;
	piConfig->sharedClassMinJITSize = TEST7_MIN_JIT_SIZE;
	piConfig->sharedClassMaxJITSize = -1;
	piConfig->sharedClassSoftMaxBytes = -1;
	piConfig->sharedClassCacheSize = cacheSize;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeCompiledMethod = j9shr_storeCompiledMethod;
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, 0, unsetRuntimeFlags,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;
	
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	oldFreeBlockBytes = cc->getFreeBlockBytes();

	/* Add enough metadata to the cache to mark it as full */
	itemLen = (U_32) (oldFreeBlockBytes - (J9SHR_MIN_GAP_BEFORE_METADATA + ONE_K_BYTES) - (sizeof(ShcItem) + sizeof(ShcItemHdr)));
	cc->initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

	cc->enterWriteMutex(vm->mainThread, false, testName);
	result = cc->allocateBlock(vm->mainThread, &item, SHC_WORDALIGN, 0);
	if (NULL == result) {
		ERRPRINTF1("Error: Did not expect allocateBlock to fail but it returned result=%p", result);
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->commitUpdate(vm->mainThread, false);
	cc->exitWriteMutex(vm->mainThread, testName);

	INFOPRINTF("Added metadata to the cache to consume free block bytes");

	/* Verify flags for BLOCK space is set */
	if (J9_ARE_NO_BITS_SET(ca->cacheFullFlags, J9SHR_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_BLOCK_SPACE_FULL to be bet in cache header." \
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("BLOCK_SPACE_FULL is set");
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL flag is not set");
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is not set");
	}

	cc->enterWriteMutex(vm->mainThread, false, testName);
	if (ca->minAOT != cc->getAvailableReservedAOTBytes(vm->mainThread)) {
		ERRPRINTF2("Error: Expected available reserved AOT bytes to be equal to minAOT\n" \
				"minAOT=%d, available reserved AOT bytes=%d", ca->minAOT, cc->getAvailableReservedAOTBytes(vm->mainThread));
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}

	if (ca->minJIT != cc->getAvailableReservedJITBytes(vm->mainThread)) {
		ERRPRINTF2("Error: Expected available reserved JIT bytes to be equal to minJIT\n" \
				"minJIT=%d, available reserved JIT bytes=%d", ca->minJIT, cc->getAvailableReservedJITBytes(vm->mainThread));
		cc->exitWriteMutex(vm->mainThread, testName);
		rc = FAIL;
		goto done;
	}
	cc->exitWriteMutex(vm->mainThread, testName);

	/* At this stage, we have TEST7_MIN_AOT_SIZE of reserved AOT space available in cache */

	memset(itemPtr, 0, sizeof(ShcItem));
	/* Allocate AOT data of size (TEST7_MIN_AOT_SIZE - 2K) bytes */
	dataLength = (TEST7_MIN_AOT_SIZE - TWO_K_BYTES) - sizeof(CompiledMethodWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for AOT data");
		rc = FAIL;
		goto done;
	}
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), dataStart, dataLength, NULL, 0, true);
	j9mem_free_memory(dataStart);
	if (true == cc->isAddressInCache(result)) {
		INFOPRINTF1("Successfully allocated %d of AOT data", dataLength);
	} else if (NULL == result) {
		ERRPRINTF("Error: storeCompiledMethod returned NULL");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		rc = FAIL;
		goto done;
	}

	if (J9SHR_AOT_SPACE_FULL == (ca->cacheFullFlags & J9SHR_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_AOT_SPACE_FULL to be set in cache header"\
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is not set");
	}

	/* Now, allocate AOT data of size (available reserved AOT bytes + free block bytes + 1K) bytes.
	 * This is expected to fail, as only (available reserved AOT bytes + free block bytes) of space is left for AOT data
	 */
	cc->enterWriteMutex(vm->mainThread, false, testName);
	availableReservedAOTBytes = cc->getAvailableReservedAOTBytes(vm->mainThread);
	cc->exitWriteMutex(vm->mainThread, testName);

	dataLength = availableReservedAOTBytes + cc->getFreeBlockBytes() + ONE_K_BYTES - sizeof(CompiledMethodWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for AOT data");
		rc = FAIL;
		goto done;
	}
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), dataStart, dataLength, NULL, 0, true);
	j9mem_free_memory(dataStart);
	if (J9SHR_RESOURCE_STORE_FULL != (UDATA) result) {
		if (true == cc->isAddressInCache(result)) {
			ERRPRINTF1("Error: Did not expect AOT data to be added to the cache. result=%p", result);
		} else if (NULL == result) {
			ERRPRINTF("Error: storeCompiledMethod returned NULL");
		} else {
			ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		}
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF1("Adding AOT data failed with expected error code=%d", (UDATA) result);
	}

	if (J9SHR_AOT_SPACE_FULL != (ca->cacheFullFlags & J9SHR_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_AOT_SPACE_FULL to be set in cache header" \
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		ERRPRINTF1("Error: Available reserved AOT bytes: %d\n", cc->getAvailableReservedAOTBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	}

	/* Verify J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL flag is set */
	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		ERRPRINTF1("Error: Available reserved AOT bytes: %d\n", cc->getAvailableReservedAOTBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("AOT_SPACE_FULL is set");
	}

	/* At this stage, we have TEST7_MIN_JIT_SIZE of reserved JIT space available in cache */

	/* Allocate JIT data of size (TEST7_MIN_JIT_SIZE - 2K) bytes */
	dataLength = (TEST7_MIN_JIT_SIZE - TWO_K_BYTES) - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	j9mem_free_memory(dataStart);
	if (0 == rc) {
		INFOPRINTF1("Successfully allocated %d of JIT data", dataLength);
		rc = PASS;
	} else {
		ERRPRINTF1("Error: Adding JIT data to cache failed with error code = %d", rc);
		rc = FAIL;
		goto done;
	}

	if (J9SHR_JIT_SPACE_FULL == (ca->cacheFullFlags & J9SHR_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_JIT_SPACE_FULL to be set in cache header"\
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		rc = FAIL;
		goto done;
	}

	if (0 != (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Did not expect J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is not set");
	}

	/* Now, allocate JIT data of size (available reserved JIT bytes + free block bytes + 1K) bytes.
	 * This is expected to fail, as only (available reserved JIT bytes + free block bytes) of space is left for JIT data.
	 */

	cc->enterWriteMutex(vm->mainThread, false, testName);
	availableReservedJITBytes = cc->getAvailableReservedJITBytes(vm->mainThread);
	cc->exitWriteMutex(vm->mainThread, testName);

	dataLength = availableReservedJITBytes + cc->getFreeBlockBytes() + ONE_K_BYTES - sizeof(AttachedDataWrapper);
	dataStart = (U_8 *) j9mem_allocate_memory(dataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataStart) {
		ERRPRINTF("failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = dataStart;
	dataDescriptor.length = dataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	j9mem_free_memory(dataStart);
	if (J9SHR_RESOURCE_STORE_FULL != rc) {
		if (0 == rc) {
			ERRPRINTF("Error: Did not expect JIT data to be added to the cache");
		} else {
			ERRPRINTF1("Error: Adding JIT data to cache failed with unexpected error code = %d", rc);
		}
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF1("Adding JIT data failed with expected error code of result=%d", rc);
		rc = PASS;
	}

	if (J9SHR_JIT_SPACE_FULL != (ca->cacheFullFlags & J9SHR_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_JIT_SPACE_FULL to be set in cache header"\
					"cacheFullFlags: 0x%zx", ca->cacheFullFlags);
		ERRPRINTF1("Error: Available reserved JIT bytes: %d\n", cc->getAvailableReservedJITBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	}

	/* Verify J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL flag is set */
	if (0 == (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: Expected J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL to be set in runtimeFlags: 0x%llx", \
				vm->sharedClassConfig->runtimeFlags);
		ERRPRINTF1("Error: Available reserved JIT bytes: %d\n", cc->getAvailableReservedJITBytes(vm->mainThread));
		rc = FAIL;
		goto done;
	} else {
		INFOPRINTF("JIT_SPACE_FULL is set");
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}

	INFOPRINTF("End\n");

	return rc;
}


IDATA test9(J9JavaVM* vm)
{
	const char* testName = "test9";
	J9SharedCacheHeader *ca = NULL;
	SH_CompositeCacheImpl *cc = NULL;
	SH_CacheMap *cacheMap = NULL;
	J9SharedClassPreinitConfig *piConfig = NULL;
	J9SharedClassConfig *sharedClassConfig = NULL;
	U_32 romClassSize = 0;
	U_32 romClassSize2 = ONE_K_BYTES;
	J9SharedDataDescriptor dataDescriptor;
	OpenCacheHelper cacheHelper(vm);
	TestOSCache *myOSCache = NULL;
	BlockPtr result = NULL;
	U_32 unstoredBytes = 0;

	I_32 cacheSize = (1024 * 1024);
	IDATA rc = PASS;
	UDATA pageSizeToUse = 4096;
	U_32 usedBytes = 0;
	UDATA jitDataLength = 0;
	UDATA aotDataLength = 0;
	U_64 protectRunTimeFlags = getDefaultRuntimeFlags();
	U_8 *osCacheMem = NULL;
	U_8 *aotDataStart = NULL;
	U_8* jitDataStart = NULL;
	char romClassName[ROMCLASS_NAME_LEN];

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Starting");

	osCacheMem = (U_8 *)j9mem_allocate_memory(TestOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if (NULL == osCacheMem) {
		ERRPRINTF1("Error: Failed to allocate memory for %s", testName);
		rc = FAIL;
		goto done;
	}
	myOSCache = new((void *)osCacheMem) TestOSCache();
	cacheHelper.setOscache(myOSCache);
	cacheHelper.setOsPageSize(pageSizeToUse);
	cacheHelper.setDoSegmentProtect(true);

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("Error: failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassMinAOTSize =TEST9_MIN_AOT_SIZE;
	piConfig->sharedClassMaxAOTSize = -1;
	piConfig->sharedClassMinJITSize =TEST9_MIN_JIT_SIZE;
	piConfig->sharedClassMaxJITSize = -1;
	piConfig->sharedClassSoftMaxBytes = TEST9_SOFTMX_SIZE;
	piConfig->sharedClassCacheSize = cacheSize;
	
	romClassSize2 = ROUND_UP_TO(sizeof(U_64), romClassSize2);

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("Error: failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;
	sharedClassConfig->storeCompiledMethod = j9shr_storeCompiledMethod;

	if (cacheHelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, cacheSize,
			NULL, true, NULL, NULL, NULL, protectRunTimeFlags, 0,
			UnitTest::CACHE_FULL_TEST, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			"openTestCache", false, true, piConfig, sharedClassConfig) == FAIL) {
		ERRPRINTF1("Failed to open cache for %s", testName);
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::CACHE_FULL_TEST;

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();


	/* Phase 1: 1) Test the case that when available space is less than CC_MIN_SPACE_BEFORE_CACHE_FULL, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL will be set (cache is soft full).
	 * 			2) Test the case that when cache is soft full, JIT data can still be added into the reserved JIT data space.
	 * 			3) Test the case that when cache is soft full, AOT data can still be added into the reserved AOT space.
	 */

	/* Try adding a ROMClass so that the cache becomes soft full*/
	usedBytes = cc->getUsedBytes();
	/* Make CC_MIN_SPACE_BEFORE_CACHE_FULL > free available space > CC_MIN_SPACE_BEFORE_CACHE_FULL - romClassSize2, so that increasing the softmx according to the
	 * unstoredBytes in phase 3 can take effect. Otherwise, it has no effect as free available space can still be less than CC_MIN_SPACE_BEFORE_CACHE_FULL. */
	romClassSize = (TEST9_SOFTMX_SIZE - usedBytes - CC_MIN_SPACE_BEFORE_CACHE_FULL + romClassSize2/2);
	romClassSize = ROUND_UP_TO(sizeof(U_64), romClassSize);
	j9str_printf(PORTLIB, romClassName, ROMCLASS_NAME_LEN, "%s_DummyClass1", testName);
	if (PASS == cacheHelper.addDummyROMClass(romClassName, romClassSize)) {
		INFOPRINTF("Successfully added dummy class1 into the cache");
	} else {
		ERRPRINTF("Error: Failed to add dummy class1 into the cache");
		rc = FAIL;
		goto done;
	}

	if (J9_ARE_ANY_BITS_SET(vm->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
		INFOPRINTF("Cache is soft full");
	} else {
		ERRPRINTF("Error: J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL should be set");
		rc = FAIL;
		goto done;
	}

	/* Try adding JIT data of size 3K bytes into the cache. This should success because of the 4K reserved JIT space even when cache is soft full. */
	jitDataLength = (3 * ONE_K_BYTES) - sizeof(AttachedDataWrapper);
	jitDataStart = (U_8 *) j9mem_allocate_memory(jitDataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == jitDataStart) {
		ERRPRINTF("Error: Failed to allocate memory for JIT data");
		rc = FAIL;
		goto done;
	}
	dataDescriptor.address = jitDataStart;
	dataDescriptor.length = jitDataLength;
	dataDescriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
	dataDescriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	if (0 == rc) {
		INFOPRINTF("Successfully added JIT data into the resvered JIT space");
	} else {
		ERRPRINTF1("Error: Adding JIT data to cache failed with error code = %d", rc);
		rc = FAIL;
		goto done;
	}

	/* Try adding AOT data of size 3K bytes into the cache. This should success because of the 4K reserved AOT space even when cache is soft full. */
	aotDataLength = (3 * ONE_K_BYTES) - sizeof(CompiledMethodWrapper);
	aotDataStart = (U_8 *) j9mem_allocate_memory(aotDataLength, J9MEM_CATEGORY_CLASSES);
	if (NULL == aotDataStart) {
		ERRPRINTF("Error: failed to allocate memory for AOT data");
		rc = FAIL;
		goto done;
	}

	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), aotDataStart, aotDataLength, NULL, 0, true);
	if (true == cc->isAddressInCache(result)) {
		INFOPRINTF("Successfully added AOT data into the reserved AOT space");
	} else if (NULL == result) {
		ERRPRINTF("Error: storeCompiledMethod returned NULL");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		rc = FAIL;
		goto done;
	}

	/* Phase 2: 1) Try adding a rom class when the cache is soft full, should fail.
	 * 			2) Try adding JIT data when the cache is soft full (free reserved JIT data space is not big enough), should fail.
	 * 			3) Try adding AOT data when the cache is soft full (free reserved AOT space is not big enough), should fail.
	 */

	/* Try adding a ROMClass when the cache is soft full */
	j9str_printf(PORTLIB, romClassName, ROMCLASS_NAME_LEN, "%s_DummyClass2", testName);
	if (FAIL == cacheHelper.addDummyROMClass(romClassName, romClassSize2)) {
		INFOPRINTF("Dummy class2 is not added into the cache as the cache is soft full");
	} else {
		ERRPRINTF("Error: Dummy class2 should not be added into the cache as the cache is soft full");
		rc = FAIL;
		goto done;
	}

	cc->getUnstoredBytes(&unstoredBytes, NULL, NULL);
	if ((romClassSize2 + SHC_WORDALIGN + sizeof(ShcItem) + sizeof(ShcItemHdr) + sizeof(OrphanWrapper)) != unstoredBytes) {
		ERRPRINTF2("Error: unstoredBytes is %u, but should be %u", unstoredBytes, romClassSize2 + SHC_WORDALIGN + sizeof(ShcItem) + sizeof(ShcItemHdr) + sizeof(OrphanWrapper));
		rc = FAIL;
		goto done;
	}
	
	INFOPRINTF1("unstoredBytes is = %u", unstoredBytes);

	/* Try adding JIT data of size 3K bytes into the cache. This should fail because the cache is soft full and reserved JIT data space is not big enough. */
	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	if (J9SHR_RESOURCE_STORE_FULL == rc) {
		INFOPRINTF("Cache is soft full the reserved JIT data space is not big enough. JIT data is not stored into the cache as expected");
	} else if (0 == rc) {
		ERRPRINTF("Error: JIT data is not expected to be added into the cache as the cache is soft full and reserved JIT data space is not big enough");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: storeAttachedData failed with error code %d", rc);
		rc = FAIL;
		goto done;
	}

	/* Try adding AOT data of size 3K bytes into the cache. This should fail because the cache is soft full and reserved aot space is not big enough. */
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), aotDataStart, aotDataLength, NULL, 0, true);
	if ((BlockPtr)J9SHR_RESOURCE_STORE_FULL == result) {
		INFOPRINTF("Cache is soft full and the reserved AOT space is not big enough. AOT data is not stored into the cache as expected\n");
	} else if (true == cc->isAddressInCache(result)) {
		ERRPRINTF("Error: AOT data is not expected to be added into the cache as the cache is soft full and the reserved AOT space is not big enough\n");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: storeCompiledMethod failed with return result %p\n", result);
		rc = FAIL;
		goto done;
	}

	if (!J9_ARE_ALL_BITS_SET(vm->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL | J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF("Error: J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL | J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL is not set\n");
		rc = FAIL;
		goto done;
	}


	/* Phase 3: 1) Test the case that rom class can be added into the cache again after softmx is increased.
	 * 			2) Adjust maxJIT to a value than no more JIT data can be added into the cache.
	 * 			3) Adjust maxAOT to a value than no more AOT data can be added into the cache.
	 */

	/* adjust the softmx limit, maxJIT and maxAOT */
	vm->sharedClassConfig->maxAOT = TEST9_MIN_AOT_SIZE;
	vm->sharedClassConfig->maxJIT = TEST9_MIN_JIT_SIZE;
	vm->sharedClassConfig->softMaxBytes = TEST9_SOFTMX_SIZE + unstoredBytes;
	cc->tryAdjustMinMaxSizes(vm->mainThread);

	if (J9_ARE_ANY_BITS_SET(vm->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL | J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL | J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF1("Error: no cache full flags should be set, used bytes is %d", cc->getUsedBytes());
		rc = FAIL;
		goto done;
	}

	if (PASS == cacheHelper.addDummyROMClass(romClassName, romClassSize2)) {
		INFOPRINTF("successfully added dummy class2 into the cache after the softmx is increased");
	} else {
		ERRPRINTF("Error: Failed to add dummy class2 into the cache after the softmx is increased");
		rc = FAIL;
		goto done;
	}
	
	vm->sharedClassConfig->softMaxBytes = 2 * TEST9_SOFTMX_SIZE;
	cc->tryAdjustMinMaxSizes(vm->mainThread);
	/* increase softMaxBytes to a big value so that it is the setting of maxAOT/maxJIT that prevents AOT/JIT data being added. */

	/* Try adding JIT data of size 3K bytes into the cache. This should fail because of the 4K setting of max JIT space. */
	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	if (J9SHR_RESOURCE_STORE_FULL == rc) {
		INFOPRINTF("JIT data is not stored into the cache because of the setting of max JIT space");
	} else if (0 == rc) {
		ERRPRINTF("Error: JIT data is not expected to be added into the cache due to the setting of max JIT space");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: storeAttachedData failed with error code %d", rc);
		rc = FAIL;
		goto done;
	}

	/* Try adding AOT data of size 3K bytes into the cache. This should fail because of the 4K setting of max AOT space. */
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), aotDataStart, aotDataLength, NULL, 0, true);
	if ((BlockPtr)J9SHR_RESOURCE_STORE_FULL == result) {
		INFOPRINTF("AOT data is not stored into the cache because of the setting of max AOT space\n");
	} else if (true == cc->isAddressInCache(result)) {
		ERRPRINTF("Error: AOT data is not expected to be added into the cache due to the setting of max AOT space\n");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: storeCompiledMethod failed with return result %p\n", result);
		rc = FAIL;
		goto done;
	}


	/* Phase 4: 1) Test the case the JIT data can be added into the cache again after maxJIT has been increased.
	 * 			2) Test the case the AOT data can be added into the cache again after maxAOT has been increased.
	 */

	/* adjust the maxAOT and maxJIT */
	vm->sharedClassConfig->maxAOT = 2 * TEST9_MIN_AOT_SIZE;
	vm->sharedClassConfig->maxJIT = 2 * TEST9_MIN_JIT_SIZE;
	cc->tryAdjustMinMaxSizes(vm->mainThread);


	if (J9_ARE_ANY_BITS_SET(vm->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL | J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
		ERRPRINTF("Error: J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL or J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL should not be set");
		rc = FAIL;
		goto done;
	}

	/* Try adding JIT data of size 3K bytes into the cache. This should success because maxJIT has been increased. */
	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(vm->mainThread, SEGUPDATEPTR(ca), &dataDescriptor, true);
	if (0 == rc) {
		INFOPRINTF("Successfully added JIT data into the cache as maxJIT has been increased");
	} else {
		ERRPRINTF1("Error: Adding JIT data to cache failed with error code = %d", rc);
		rc = FAIL;
		goto done;
	}

	/* Try adding AOT data of size 3K bytes into the cache. This should success as maxAOT has been increased*/
	result = (BlockPtr) vm->sharedClassConfig->storeCompiledMethod(vm->mainThread, (J9ROMMethod *) SEGUPDATEPTR(ca), aotDataStart, aotDataLength, NULL, 0, true);
	if (true == cc->isAddressInCache(result)) {
		INFOPRINTF("Successfully added AOT data into the cache as maxAOT has been increased");
	} else if (NULL == result) {
		ERRPRINTF("Error: storeCompiledMethod returned NULL");
		rc = FAIL;
		goto done;
	} else {
		ERRPRINTF1("Error: Adding AOT data to cache failed with error code = %d", (UDATA) result);
		rc = FAIL;
		goto done;
	}

done:
	UnitTest::unitTest = UnitTest::NO_TEST;
	/* closeTestCache() will free piConfig and sharedClassConfig */
	cacheHelper.closeTestCache(true);
	if (NULL != osCacheMem) {
		j9mem_free_memory(osCacheMem);
	}
	if (NULL != jitDataStart) {
		j9mem_free_memory(jitDataStart);
	}
	if (NULL != aotDataStart) {
		j9mem_free_memory(aotDataStart);
	}

	INFOPRINTF("End\n");

	return rc;
}
