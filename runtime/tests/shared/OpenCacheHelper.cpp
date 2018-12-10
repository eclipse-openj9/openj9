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

#include "OpenCacheHelper.h"
#include "SCStoreTransaction.hpp"


IDATA
OpenCacheHelper::openTestCache(I_32 cacheType, I_32 cacheSize, const char *cacheName,
								bool inMemoryCache, BlockPtr cacheBuffer,
								char *cacheDir, const char *cacheDirPermStr,
								U_64 extraRunTimeFlag,
								U_64 unsetTheseRunTimeFlag,
								IDATA unitTest,
								UDATA extraVerboseFlag ,
								const char * testName,
								bool startupWillFail,
								bool doCleanupOnFail,
								J9SharedClassPreinitConfig * cacheDetails,
								J9SharedClassConfig *sharedclassconfig)
{
	IDATA rc = 0;
	IDATA cacheObjectSize;
	bool cacheHasIntegrity;
	void* memory;
	SH_CompositeCacheImpl *cc;
	UDATA cacheDirPerm;

	PORT_ACCESS_FROM_JAVAVM(vm);

	INFOPRINTF("Opening test cache");

	if (NULL == cacheName) {
		this->cacheName = DEFAULT_CACHE_NAME;
	} else {
		this->cacheName = cacheName;
	}

	this->cacheDir = cacheDir;
	this->inMemoryCache = inMemoryCache;
	this->cacheType = cacheType;

	if(NULL == cacheDetails) {
		piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
		if (NULL == piConfig) {
			ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
			rc = FAIL;
			goto done;
		}
		memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));

		piConfig->sharedClassDebugAreaBytes = -1;
		piConfig->sharedClassMinAOTSize = -1;
		piConfig->sharedClassMaxAOTSize = -1;
		piConfig->sharedClassMinJITSize = -1;
		piConfig->sharedClassMaxJITSize = -1;
		piConfig->sharedClassSoftMaxBytes = -1;

		if (osPageSize != 0) {
			piConfig->sharedClassCacheSize = ROUND_UP_TO(osPageSize, cacheSize);
		} else {
			piConfig->sharedClassCacheSize = cacheSize;
		}
	} else {
		piConfig = cacheDetails;
	}
	origPiConfig = vm->sharedClassPreinitConfig;
	vm->sharedClassPreinitConfig = piConfig;

	if(NULL != sharedclassconfig){
		sharedClassConfig = sharedclassconfig;
	} else {
		sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
		if (NULL == sharedClassConfig) {
			ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
			rc = FAIL;
			goto done;
		}
		memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig)+sizeof(J9SharedClassCacheDescriptor));
	}
	origSharedClassConfig = vm->sharedClassConfig;
	vm->sharedClassConfig = sharedClassConfig;

	sharedClassConfig->cacheDescriptorList = (J9SharedClassCacheDescriptor*)((UDATA)sharedClassConfig + sizeof(J9SharedClassConfig));
	sharedClassConfig->cacheDescriptorList->next = sharedClassConfig->cacheDescriptorList;
	sharedClassConfig->softMaxBytes = -1;
	sharedClassConfig->minAOT = -1;
	sharedClassConfig->maxAOT = -1;
	sharedClassConfig->minJIT = -1;
	sharedClassConfig->maxJIT = -1;

	sharedClassConfig->runtimeFlags = getDefaultRuntimeFlags();
	sharedClassConfig->runtimeFlags |= extraRunTimeFlag;
	vm->sharedCacheAPI->runtimeFlags |= extraRunTimeFlag;
	sharedClassConfig->runtimeFlags &= ~(unsetTheseRunTimeFlag);

	sharedClassConfig->verboseFlags |= extraVerboseFlag;

	sharedClassConfig->sharedAPIObject = initializeSharedAPI(vm);
	if (NULL == sharedClassConfig->sharedAPIObject) {
		ERRPRINTF("failed to allocate sharedAPIObject");
		rc = FAIL;
		goto done;
	}

	sharedClassConfig->getJavacoreData = j9shr_getJavacoreData;
	sharedClassConfig->storeCompiledMethod = j9shr_storeCompiledMethod;

	UnitTest::unitTest = unitTest;
	cacheObjectSize = SH_CacheMap::getRequiredConstrBytes(false);
	memory = j9mem_allocate_memory(cacheObjectSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == memory) {
		ERRPRINTF("failed to allocate CacheMap object");
		rc = FAIL;
		goto done;
	}
	memset(memory, 0, cacheObjectSize);

	cacheMap = SH_CacheMap::newInstance(vm, sharedClassConfig, (SH_CacheMap*)memory, cacheName, cacheType);

	sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE;
	sharedClassConfig->sharedClassCache = (void*)cacheMap;
	if (true == inMemoryCache) {
		if (NULL == cacheBuffer) {
			if (NULL == cacheMemory) {
				cacheAllocated = (BlockPtr)j9mem_allocate_memory(piConfig->sharedClassCacheSize + osPageSize, J9MEM_CATEGORY_CLASSES);
				if (NULL == cacheAllocated) {
					ERRPRINTF("failed to allocate CacheMap object");
					rc = FAIL;
					goto done;
				}
				if (osPageSize != 0) {
					cacheMemory = (BlockPtr) ROUND_UP_TO(osPageSize, (UDATA)cacheAllocated);
				} else {
					cacheMemory = cacheAllocated;
				}

				/*zero out new cache memory*/
				memset(cacheMemory, 0, piConfig->sharedClassCacheSize);
			} else {
				/* use existing cacheMemory */
			}
		} else {
			cacheMemory = cacheBuffer;
		}
	} else {
		cacheAllocated = cacheMemory = NULL;
	}

	if (UnitTest::MINMAX_TEST == UnitTest::unitTest) {
		cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
		cc->_osPageSize = 0;
	}

	if ((UnitTest::PROTECT_NEW_ROMCLASS_DATA_TEST == UnitTest::unitTest)
		|| (UnitTest::CACHE_FULL_TEST == UnitTest::unitTest)
		|| (UnitTest::PROTECTA_SHARED_CACHE_DATA_TEST == UnitTest::unitTest)
	) {
		cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
		cc->_oscache = oscache;
		cc->_osPageSize = osPageSize;
		cc->_doSegmentProtect = doSegmentProtect;
	}

	if (cacheDir == NULL) {
		cacheDirPerm = J9SH_DIRPERM_ABSENT;
	} else {
		cacheDirPerm = convertPermToDecimal(vm, cacheDirPermStr);
		if ((UDATA)-1 == cacheDirPerm) {
			ERRPRINTF("Invalid cache dir permission\n");
			rc = FAIL;
			goto done;
		}
	}
	
	if (J9_ARE_ALL_BITS_SET(sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS)) {
		if (J9SH_DIRPERM_ABSENT == cacheDirPerm) {
			cacheDirPerm = J9SH_DIRPERM_ABSENT_GROUPACCESS;
		}
	}
	rc = cacheMap->startup(vm->mainThread, piConfig, this->cacheName, this->cacheDir, cacheDirPerm, cacheMemory, &cacheHasIntegrity);
	if (true == startupWillFail) {
		if (0 == rc) {
			ERRPRINTF("CacheMap.startup() passed when a fail was expected");
			rc = FAIL;
			goto done;
		}
	} else {
		if (0 != rc) {
			ERRPRINTF("CacheMap.startup() failed when a pass was expected");
			rc = FAIL;
			goto done;
		}
	}
	UnitTest::unitTest = UnitTest::NO_TEST;
done:
	if ((FAIL == rc) && (true == doCleanupOnFail)) {
		if (NULL != cacheMap) {
			cacheMap->cleanup(vm->mainThread);
			j9mem_free_memory(cacheMap);
		}
		if (NULL != sharedClassConfig) {
			vm->sharedClassConfig = origSharedClassConfig;
			j9mem_free_memory(sharedClassConfig);
		}
		if (NULL != piConfig) {
			vm->sharedClassPreinitConfig = origPiConfig;
			j9mem_free_memory(piConfig);
		}
		if (NULL != cacheAllocated) {
			j9mem_free_memory(cacheAllocated);
			cacheAllocated = cacheMemory = NULL;
		}

	}
	INFOPRINTF("Done opening test cache");
	return rc;
}


IDATA
OpenCacheHelper::closeTestCache(bool freeCache)
{
	SH_CacheMap *cachemap;
	IDATA rc = PASS;
	const char *testName = "closeTestCache";
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL == vm->sharedClassConfig) {
		ERRPRINTF("sharedClassConfig is null\n");
		return FAIL;
	}

	cachemap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	if (NULL != cachemap) {
		cachemap->cleanup(vm->mainThread);
		j9mem_free_memory(cachemap);
	}

	if (false == inMemoryCache) {
		rc = j9shr_destroySharedCache(vm, cacheDir, cacheName, cacheType, false);
	}

	if ((true == freeCache) && (NULL != cacheAllocated)) {
		/* Unprotect the cacheMemory before freeing. Otherwise it can cause crash on Linux platform
		 * when the next call to allocate memory returns same address as current cacheMemory.
		 */
		j9mmap_protect(cacheMemory, piConfig->sharedClassCacheSize, (J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE));
		j9mem_free_memory(cacheAllocated);
		cacheAllocated = cacheMemory = NULL;
	}

	if (NULL != sharedClassConfig) {
		vm->sharedClassConfig = origSharedClassConfig;
		j9mem_free_memory(sharedClassConfig);
		sharedClassConfig = NULL;
	}
	if (NULL != piConfig) {
		vm->sharedClassPreinitConfig = origPiConfig;
		j9mem_free_memory(piConfig);
		piConfig = NULL;
	}

	if ((inMemoryCache == false) && (J9SH_DESTROYED_ALL_CACHE != rc)) {
		ERRPRINTF("Failed to destroy corrupt cache\n");
		j9tty_printf(PORTLIB, "\tj9shr_destroySharedCache()=%d\n", rc);
		rc = FAIL;
	}
	return rc;
}

IDATA
OpenCacheHelper::addDummyROMClass(const char *romClassName, U_32 dummyROMClassSize) {
	J9RomClassRequirements sizes;
	J9ROMClass *romClass;
	IDATA rc = PASS;
	U_32 count;
	PORT_ACCESS_FROM_JAVAVM(vm);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romClassName), (U_8 *)romClassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "addDummyROMClass: could not allocate the transaction object\n");
		rc = FAIL;
		goto done;
	}

	memset(&sizes, 0, sizeof(J9RomClassRequirements));
	sizes.romClassSizeFullSize = dummyROMClassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;

	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		rc = FAIL;
		goto done;
	} else {
		romClass = (J9ROMClass *)transaction.getRomClass();
	}

	if (romClass == NULL) {
		rc = FAIL;
		goto done;
	}

	romClass->romSize = dummyROMClassSize;

	/* fill ROMClass contents with some garbage */
	count = 4;
	while (count < dummyROMClassSize-4) {
		memset((U_8 *)romClass+count, count%128, sizeof(char));
		count++;
	}

	/* Set ROMClass.intermediateClassData to NULL, it can't be set to garbage
	 * as there is as assert in j9shr_classStoreTransaction_stop() to verify its value */
	romClass->intermediateClassData = 0;
	romClass->intermediateClassDataLength = 0;

	setRomClassName(romClass, romClassName);

	if (transaction.updateSharedClassSize(romClass->romSize) == -1) {
		rc = FAIL;
		goto done;
	}
done:
	return rc;
}

void
OpenCacheHelper::setRomClassName(J9ROMClass * rc, const char * name)
{
	J9UTF8 * romClassNameLocation = NULL;
	romClassNameLocation = (J9UTF8 *) (((IDATA) rc) + sizeof(J9ROMClass));
	J9UTF8_SET_LENGTH(romClassNameLocation, (U_16)strlen(name));
	strcpy((char *) (J9UTF8_DATA(romClassNameLocation)), name);
	NNSRP_SET(rc->className, romClassNameLocation);
}
