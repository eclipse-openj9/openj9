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
#include "OSCachemmap.hpp"
#include "OpenCacheHelper.h"

class FakeOSCache : public SH_OSCachemmap {
public:
	void * protectedAddress;
	UDATA protectedLength;

	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	FakeOSCache() {
		protectedAddress = NULL;
		protectedLength = 0;
	}

	static UDATA getRequiredConstrBytes(void)
	{
		return sizeof(FakeOSCache);
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


IDATA testProtectNewROMClassData_test1(J9JavaVM* vm);

extern "C" {

IDATA
testProtectNewROMClassData(J9JavaVM* vm)
{
	IDATA rc = PASS;

	PORT_ACCESS_FROM_JAVAVM(vm);
	REPORT_START("testProtectNewROMClassData");

	rc |= testProtectNewROMClassData_test1(vm);

	REPORT_SUMMARY("testProtectNewROMClassData", rc);
	return rc;
}

}

IDATA testProtectNewROMClassData_test1(J9JavaVM* vm) {
	IDATA rc = PASS;
	J9SharedCacheHeader *ca;
	SH_CompositeCacheImpl *cc;
	SH_CacheMap *cacheMap;
	const char* testName = "testProtectNewROMClassData_test1";
	OpenCacheHelper cachehelper(vm);
	U_32 romClassSize = 0;
	const int pageSizeToUse = 4096;
	UDATA changeInCCRomClassProtectEnd = 0;
	U_64 protectRunTimeFlags=(J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW);
	U_64 unsetProtectRunTimeFlags = J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES;
	UDATA* updateCountAddress = 0;
	UDATA oldUpdateCount = 0;
	BlockPtr oldProtectEnd = NULL;
	void * oldProtectedAddress;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/*Set the _oscache to a dummy class so how much memory is mprotected can be monitored*/
	char * OSCacheMem;
	FakeOSCache *myfakeoscache;

	vm->internalVMFunctions->internalEnterVMFromJNI(vm->mainThread);

	if (NULL == (OSCacheMem = (char *)j9mem_allocate_memory(FakeOSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF("Failed to allocate memory for testProtectNewROMClassData_test1");
		rc = FAIL;
		goto done;
	}
	myfakeoscache = new((void *)OSCacheMem) FakeOSCache();
	cachehelper.setOscache(myfakeoscache);
	cachehelper.setOsPageSize(pageSizeToUse);
	cachehelper.setDoSegmentProtect(true);

	if (cachehelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, CACHE_SIZE, NULL, true, NULL, NULL, NULL, protectRunTimeFlags, unsetProtectRunTimeFlags, UnitTest::PROTECT_NEW_ROMCLASS_DATA_TEST) == FAIL) {
		ERRPRINTF("Failed to open cache for testProtectNewROMClassData_test1");
		rc = FAIL;
		goto done;
	}
	UnitTest::unitTest = UnitTest::PROTECT_NEW_ROMCLASS_DATA_TEST;

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	/*Back up original values in the cache header and composite cache*/
	updateCountAddress = WSRP_GET(ca->updateCountPtr, UDATA*);
	oldUpdateCount = cc->_oldUpdateCount;
	oldProtectEnd = cc->getRomClassProtectEnd();
	
	/*Test 1: Test protecting a new class. Store a class 2.5 pages in size*/
	romClassSize = (pageSizeToUse * 2) + (pageSizeToUse/2);
	if (cachehelper.addDummyROMClass("testProtectNewROMClassData_test1_DummyClass1", romClassSize) != PASS) {
		ERRPRINTF("Adding dummy class to the cache has failed");
		rc = FAIL;
		goto done;
	}

	if (myfakeoscache->protectedLength != (romClassSize - (romClassSize % pageSizeToUse))) {
		ERRPRINTF2("After calling addDummyROMClass() the incorrect size was protected. Expected %u, but %u was protected", 
			(romClassSize - (romClassSize % pageSizeToUse)), myfakeoscache->protectedLength);
		rc = FAIL;
		goto done;
	}
	INFOPRINTF("Correctly protected a newly added ROMClasses");

	/* Test 2: Test protecting after discovering a cache update
	 * To do this modify the cache header and composite cache to simulate
	 * the discovery of a cache updated
	 */
	*updateCountAddress += 1;
	cc->setRomClassProtectEnd(oldProtectEnd);
	cc->_oldUpdateCount = oldUpdateCount;
	myfakeoscache->resetTestVars();
		
	/* ::doneReadUpdates() is responsible for mprotected new rom classes added by other JVMs.
	 * The function is called instead of SH_CacheMap::readCache() because we would have to tweak the meta data as well 
	 */
	cc->doneReadUpdates(vm->mainThread, 1);

	if (myfakeoscache->protectedLength != (romClassSize - (romClassSize % pageSizeToUse))) {
		ERRPRINTF2("After calling doneReadUpdates() the incorrect size was protected. Expected %u, but %u was protected", 
			(romClassSize - (romClassSize % pageSizeToUse)), myfakeoscache->protectedLength);
		rc = FAIL;
		goto done;
	}
	
	changeInCCRomClassProtectEnd = ((UDATA)(cc->getRomClassProtectEnd()) - (UDATA)oldProtectEnd);
	if (changeInCCRomClassProtectEnd != romClassSize) {
		ERRPRINTF2("After calling doneReadUpdates() verification of getRomClassProtectEnd() has failed." \
				   " Expected a change of %u bytes, however the change was %u", 
					romClassSize,
					changeInCCRomClassProtectEnd);
		rc = FAIL;
		goto done;
	}
	INFOPRINTF("Correctly protected ROMClasses after a cache update");
	
	/* Test 3: Test reopening the cache protects the correct amount of 
	 * memory during ::startup()
	 */ 
	cachehelper.closeTestCache(false);
	oldProtectedAddress = myfakeoscache->protectedAddress;
	myfakeoscache->resetTestVars();
	if (cachehelper.openTestCache(J9PORT_SHR_CACHE_TYPE_PERSISTENT, CACHE_SIZE, NULL, true, NULL, NULL, NULL, protectRunTimeFlags, unsetProtectRunTimeFlags, UnitTest::PROTECT_NEW_ROMCLASS_DATA_TEST) == FAIL) {
		ERRPRINTF("Failed to open cache for test 3");
		rc = FAIL;
		goto done;
	}
	if (myfakeoscache->protectedLength != (romClassSize - (romClassSize % pageSizeToUse))) {
		ERRPRINTF2("After calling openTestCache() the incorrect size was protected. Expected %u, but %u was protected", 
			 (romClassSize - (romClassSize % pageSizeToUse)), myfakeoscache->protectedLength);
		rc = FAIL;
		goto done;
	}
	if (myfakeoscache->protectedAddress != oldProtectedAddress) {
		ERRPRINTF2("After calling openTestCache() the protected address was incorrect (Expected %p, but protect %p)",
				oldProtectedAddress, myfakeoscache->protectedAddress);
		rc = FAIL;
		goto done;
	}
	INFOPRINTF("Correctly protected existing ROMClasses when opening an existing cache");
	UnitTest::unitTest = UnitTest::PROTECT_NEW_ROMCLASS_DATA_TEST;
	

	/* Test 4: Add and protect a 2nd ROMClass
	 */
	myfakeoscache->resetTestVars();
	romClassSize = (pageSizeToUse) + (pageSizeToUse/2);
	if (cachehelper.addDummyROMClass("testProtectNewROMClassData_test1_DummyClass2", romClassSize) != PASS) {
		ERRPRINTF("Adding dummy class to the cache has failed");
		rc = FAIL;
		goto done;
	}

	/*Two new 4096 pages should be protected b/c the previously added class was 2.5 pages in size.*/
	if (myfakeoscache->protectedLength != (pageSizeToUse*2)) {
		ERRPRINTF2("After calling addDummyROMClass() the incorrect size was protected. Expected %u, but %u was protected",
			(pageSizeToUse*2), myfakeoscache->protectedLength);
		rc = FAIL;
		goto done;
	}
	/*The protected address should start 2 pages ahead b/c the 1st rom class is 2.5 pages in size*/
	if ( (UDATA)myfakeoscache->protectedAddress != ((UDATA)oldProtectedAddress + (pageSizeToUse*2))) {
		ERRPRINTF2("After calling openTestCache() the protected address was incorrect (Expected %p, but protect %p)",
				oldProtectedAddress, ((UDATA)oldProtectedAddress + (pageSizeToUse/2)));
		rc = FAIL;
		goto done;
	}
	INFOPRINTF("Correctly protected a 2nd newly added ROMClasses");

done:
	cachehelper.closeTestCache(true);
	if (NULL != OSCacheMem) {
		j9mem_free_memory(OSCacheMem);
	}
	vm->internalVMFunctions->internalExitVMToJNI(vm->mainThread);
	UnitTest::unitTest = UnitTest::NO_TEST;
	return rc;
}
