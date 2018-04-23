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
extern "C"
{
#include "shrinit.h"
}
#include "SCTestCommon.h"
#include "AbstractMemoryPermission.hpp"
#include "SCStoreTransaction.hpp"
#include "OpenCacheHelper.h"
#include "thrtypes.h"
#include "omr.h"

#define FIND_ATTACHED_DATA_RETRY_COUNT 1
#define MAIN_THREAD_WAIT_TIME 1

#define J9THREAD_PROC

struct SharedAttachedData {
	void *keyAddress;
	J9SharedDataDescriptor data;
};

typedef char* BlockPtr;
static struct SharedAttachedData* dataList4Threads;
static BlockPtr writerCacheMemory;
static UDATA writerCacheSize;

extern "C" {
	IDATA createThread(J9JavaVM *vm, omrthread_t *osThread, J9VMThread **vmThread, omrthread_entrypoint_t entryPoint, void *entryArg);
	int J9THREAD_PROC startReader(void *entryArg);
	int J9THREAD_PROC startWriter(void *entryArg);
	IDATA testAttachedData(J9JavaVM* vm);
}

class AttachedDataTest {
private:
	U_8 nextValue;
	BlockPtr cacheAllocated;
	void populateAttachedDataList(J9JavaVM *vm, SharedAttachedData *dataList);
	IDATA FindAttachedData(J9JavaVM *vm, const void *addressInCache, J9SharedDataDescriptor *expectedData, bool expectCorruptData, I_32 expectedCorruptOffset, const char *caller);

public:
	J9JavaVM *vm;
	omrthread_t osThread;
	J9VMThread *vmThread;
	bool readOnlyCache;
	J9SharedClassPreinitConfig *piConfig, *origPiConfig;
	J9SharedClassConfig *sharedClassConfig, *origSharedClassConfig;
	J9MemorySegmentList *classMemorySegments, *origClassMemorySegments;
	BlockPtr cacheMemory;
	SharedAttachedData *dataList;
	bool threadExited, cancelThread;

	IDATA openTestCache(J9JavaVM* vm, BlockPtr preallocatedCache, UDATA preallocatedCacheSize, I_32 cacheType, U_64 extraRunTimeFlag, U_64 unsetTheseRunTimeFlag);
	IDATA closeTestCache(J9JavaVM *vm, bool freeCache);
	IDATA initializeAttachedData(J9JavaVM *vm);
	IDATA freeAttachedData(J9JavaVM *vm);
	IDATA StoreAttachedDataSuccess(J9JavaVM *vm);
	IDATA StoreAttachedDataFailure(J9JavaVM *vm);
	IDATA FindAttachedDataSuccess(J9JavaVM *vm);
	IDATA FindAttachedDataFailure(J9JavaVM *vm);
	IDATA FindAttachedDataUpdateCountFailure(J9JavaVM *vm);
	IDATA FindAttachedDataCorruptCountFailure(J9JavaVM *vm);
	IDATA ReplaceAttachedData(J9JavaVM *vm);
	IDATA UpdateAttachedDataSuccess(J9JavaVM *vm);
	IDATA UpdateAttachedDataFailure(J9JavaVM *vm);
	IDATA UpdateAttachedUDATASuccess(J9JavaVM *vm);
	IDATA UpdateAttachedUDATAFailure(J9JavaVM *vm);
	IDATA UpdateJitHint(J9JavaVM *vm);
	IDATA CorruptAttachedData(J9JavaVM *vm);
	J9ROMMethod* addMethodToCache(J9JavaVM *vm);

	bool isThreadSuspended(void);

	AttachedDataTest() :
		nextValue(0),
		cacheAllocated(NULL),
		vm(NULL),
		osThread(NULL),
		vmThread(NULL),
		readOnlyCache(false),
		piConfig(NULL),
		origPiConfig(NULL),
		sharedClassConfig(NULL),
		origSharedClassConfig(NULL),
		cacheMemory(NULL),
		dataList(NULL),
		threadExited(false),
		cancelThread(false)
	{
	}
};

bool
AttachedDataTest::isThreadSuspended(void)
{
	return ((osThread->flags & J9THREAD_FLAG_SUSPENDED) ? true : false);
}

J9ROMMethod *
AttachedDataTest::addMethodToCache(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char* testName = "addMethodToCache";
	void * romclassmem ;
	J9ROMClass * romclass;
	J9ROMClass * newromclass = NULL;
	U_32 romclassSize = 0;
	J9ROMMethod *romMethod;
	J9UTF8* romclassName = NULL;
	J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);

	romclass = J9VMJAVALANGOBJECT_OR_NULL(vm)->romClass;
	if (romclass) {
		romclassSize = romclass->romSize;
		romclassName = J9ROMCLASS_CLASSNAME(romclass);
	}

	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	SCStoreTransaction transaction(currentThread, (J9ClassLoader*) vm->systemClassLoader, 0, J9SHR_LOADTYPE_NORMAL, J9UTF8_LENGTH(romclassName), (U_8 *)J9UTF8_DATA(romclassName), false);
	if (transaction.isOK() == false) {
		ERRPRINTF("could not allocate the transaction object");
		return NULL;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	
	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		ERRPRINTF("failure to alloc memory for ROMClass in shared cache\n");
		return NULL;
	} else {
		romclassmem = transaction.getRomClass();
 	}

	if (romclassmem == NULL) {
		ERRPRINTF(("romclassmem == NULL\n"));
		return NULL;
	}
	newromclass = (J9ROMClass*) romclassmem;
	memcpy(newromclass,romclass,romclassSize);
	romMethod = J9ROMCLASS_ROMMETHODS(newromclass);

    if (transaction.updateSharedClassSize(newromclass->romSize) == -1) {
    	ERRPRINTF(("failed to update shared class size\n"));
    	return NULL;
    }

	return (romMethod);
 }

IDATA
AttachedDataTest::openTestCache(J9JavaVM* vm, BlockPtr preallocatedCache, UDATA preallocatedCacheSize, I_32 cacheType, U_64 extraRunTimeFlag, U_64 unsetTheseRunTimeFlag)
{
	void* memory;
	IDATA rc = 0;
	IDATA cacheMapSize;
	bool cacheHasIntegrity;
	SH_CacheMap *cacheMap = NULL;
	UDATA osPageSize;
	const char* testName = "openTestCache";
	J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);

	PORT_ACCESS_FROM_JAVAVM(vm);

    classMemorySegments = vm->internalVMFunctions->allocateMemorySegmentListWithFlags(vm, 10, MEMORY_SEGMENT_LIST_FLAG_SORT, J9MEM_CATEGORY_CLASSES);
    if (classMemorySegments == NULL) {
            ERRPRINTF("openTestCache: failed to allocate memory segment list");
            rc = FAIL;
            goto done;
    }
    origClassMemorySegments = vm->classMemorySegments;
    vm->classMemorySegments = classMemorySegments;

	piConfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassPreinitConfig");
		rc = FAIL;
		goto done;
	}
	memset(piConfig, 0, sizeof(J9SharedClassPreinitConfig));
	/* j9mmap_get_region_granularity returns 0 on zOS */
	osPageSize = j9mmap_get_region_granularity(NULL);
	piConfig->sharedClassDebugAreaBytes = -1;
	piConfig->sharedClassSoftMaxBytes = -1;
	if (0 == preallocatedCacheSize) {
		/* We will be using UnitTest::cacheSize, so it is necessary to ensure cache size is OS page aligned. */
		if (osPageSize != 0) {
			piConfig->sharedClassCacheSize = ROUND_UP_TO(osPageSize, CACHE_SIZE);
		} else {
			piConfig->sharedClassCacheSize = CACHE_SIZE;
		}
	} else {
		piConfig->sharedClassCacheSize = preallocatedCacheSize;
	}
	piConfig->sharedClassMinJITSize = -1;
	piConfig->sharedClassMaxJITSize = -1;

	origPiConfig = vm->sharedClassPreinitConfig;
	vm->sharedClassPreinitConfig = piConfig;

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		ERRPRINTF("failed to allocate memory for J9SharedClassConfig");
		rc = FAIL;
		goto done;
	}

	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig)+sizeof(J9SharedClassCacheDescriptor));
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
	sharedClassConfig->runtimeFlags &= ~(unsetTheseRunTimeFlag);
	sharedClassConfig->runtimeFlags |= (extraRunTimeFlag);

	sharedClassConfig->verboseFlags |= (J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_JITDATA | J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT);


	/* populate AttachedData APIs */
	sharedClassConfig->storeAttachedData = j9shr_storeAttachedData;
	sharedClassConfig->findAttachedData = j9shr_findAttachedData;
	sharedClassConfig->updateAttachedData = j9shr_updateAttachedData;
	sharedClassConfig->updateAttachedUDATA = j9shr_updateAttachedUDATA;
	sharedClassConfig->freeAttachedDataDescriptor = j9shr_freeAttachedDataDescriptor;
	sharedClassConfig->sharedAPIObject =  initializeSharedAPI(vm);
	if (NULL == sharedClassConfig->sharedAPIObject) {
		ERRPRINTF("failed to allocate sharedAPIObject");
		rc = FAIL;
		goto done;
	}

	UnitTest::unitTest = UnitTest::ATTACHED_DATA_TEST;

	if (NULL == preallocatedCache) {
		if (NULL == cacheMemory) {
			/* We will be using UnitTest::cacheMemory, so it is necessary to ensure cache memory is OS page aligned.
			 * Thats why we allocate an extra page, so that when rounding up, we don't overshoot allocated memory.
			 */
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
			memset(cacheMemory, 0, piConfig->sharedClassCacheSize);
		}
	} else {
		cacheMemory = preallocatedCache;
	}
	UnitTest::cacheMemory = cacheMemory;
	UnitTest::cacheSize = (U_32)piConfig->sharedClassCacheSize;
	
	cacheMapSize = SH_CacheMap::getRequiredConstrBytes(false);
	memory = j9mem_allocate_memory(cacheMapSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == memory) {
		ERRPRINTF("failed to allocate CacheMap object");
		rc = FAIL;
		goto done;
	}
	memset(memory, 0, cacheMapSize);

	cacheMap = SH_CacheMap::newInstance(vm, sharedClassConfig, (SH_CacheMap*)memory, "attacheddatacache", cacheType);

	rc = cacheMap->startup(currentThread, piConfig, "attacheddatacache", NULL, J9SH_DIRPERM_ABSENT, cacheMemory, &cacheHasIntegrity);
	if (0 != rc) {
		ERRPRINTF("openTestCache: CacheMap.startup() failed");
		rc = FAIL;
		goto done;
	}

	sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE;
	sharedClassConfig->sharedClassCache = (void*)cacheMap;

	/* Without unsetting UnitTest::unitTest here, SH_AttachedDataManager::updateDataInCache(),
	 * called by UpdateAttachedDataXXX() and UpdateAttachedUDATAXXX() tests, would mark data as corrupt,
	 * causing these tests to fail.
	 */
	UnitTest::unitTest = UnitTest::NO_TEST;
done:
	if (FAIL == rc) {
		if (NULL != cacheMap) {
			cacheMap->cleanup(vm->mainThread);
			j9mem_free_memory(cacheMap);
			cacheMap = NULL;
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
		if (NULL != cacheAllocated) {
			j9mem_free_memory(cacheAllocated);
			cacheAllocated = NULL;
			cacheMemory = NULL;
		}
        if (NULL != classMemorySegments) {
        	vm->internalVMFunctions->freeMemorySegmentList(vm, classMemorySegments);
            vm->classMemorySegments = origClassMemorySegments;
            classMemorySegments = NULL;
        }
	}
	return rc;
}

IDATA
AttachedDataTest::closeTestCache(J9JavaVM *vm, bool freeCache)
{
	IDATA rc = PASS;
	SH_CacheMap *cacheMap;
	PORT_ACCESS_FROM_JAVAVM(vm);

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	if (NULL != cacheMap) {
		cacheMap->cleanup(vm->mainThread);
		j9mem_free_memory(cacheMap);
		cacheMap = NULL;
	}

	if ((true == freeCache) && (NULL != cacheAllocated)) {
		/* Unprotect the cacheMemory before freeing. Otherwise it can cause crash on linux/ppc 64-bit machine when 
		 * the next call to allocate memory for cache returns same address as current cacheMemory.
		 */
		j9mmap_protect(cacheMemory, piConfig->sharedClassCacheSize, (J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE));
		j9mem_free_memory(cacheAllocated);
		cacheAllocated = NULL;
		cacheMemory = NULL;
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
    if (NULL != classMemorySegments) {
    	vm->internalVMFunctions->freeMemorySegmentList(vm, classMemorySegments);
        vm->classMemorySegments = origClassMemorySegments;
        classMemorySegments = NULL;
    }
	return rc;
}

void
AttachedDataTest::populateAttachedDataList(J9JavaVM *vm, SharedAttachedData *dataList)
{
	I_32 i;
	PORT_ACCESS_FROM_JAVAVM(vm);

	for(i = JIT_PROFILE_OBJECT_BASE; i < JIT_PROFILE_OBJECT_LIMIT; i++) {
		I_32 j;
		dataList[i].keyAddress = addMethodToCache(vm);
		dataList[i].data.length = i*2+10;
		dataList[i].data.address = (U_8 *)j9mem_allocate_memory(dataList[i].data.length, J9MEM_CATEGORY_CLASSES);
		for(j = 0; j < (I_32)dataList[i].data.length; j++) {
			dataList[i].data.address[j] = (U_8)(nextValue++);
		}
		dataList[i].data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
		dataList[i].data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
	}
	for(i = JIT_HINT_OBJECT_BASE; i < JIT_HINT_OBJECT_LIMIT; i++) {
		I_32 j;
		dataList[i].keyAddress = addMethodToCache(vm);
		dataList[i].data.length = i*2+10;
		dataList[i].data.address = (U_8 *)j9mem_allocate_memory(dataList[i].data.length, J9MEM_CATEGORY_CLASSES);
		for(j = 0; j < (I_32)dataList[i].data.length; j++) {
			dataList[i].data.address[j] = (U_8)(nextValue++);
		}
		dataList[i].data.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
		dataList[i].data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
	}
}

IDATA
AttachedDataTest::initializeAttachedData(J9JavaVM *vm)
{
	I_32 rc = PASS;
	const char *testName = "initializeAttachedData";
	PORT_ACCESS_FROM_JAVAVM(vm);

	nextValue = 0;
	dataList = (SharedAttachedData *) j9mem_allocate_memory(sizeof(SharedAttachedData)*NUM_DATA_OBJECTS, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataList) {
		ERRPRINTF("failed to allocate memory for AttachedData list");
		rc = FAIL;
	} else {
		populateAttachedDataList(vm, dataList);
	}

	return rc;
}

IDATA
AttachedDataTest::freeAttachedData(J9JavaVM *vm)
{
	I_32 rc = PASS;
	I_32 i;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL != dataList) {
		for(i = 0; i < NUM_DATA_OBJECTS; i++) {
			if (NULL != dataList[i].data.address) {
				j9mem_free_memory(dataList[i].data.address);
				dataList[i].data.address = NULL;
			}
		}
		j9mem_free_memory(dataList);
		dataList = NULL;
	}

	return rc;
}

IDATA
AttachedDataTest::StoreAttachedDataSuccess(J9JavaVM *vm)
{
	IDATA rc = PASS;
	UDATA rV = 0;
	I_32 i;
	J9VMThread *currentThread;
	const char *testName = "StoreAttachedDataSuccess";
	PORT_ACCESS_FROM_JAVAVM(vm);

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	for (i = 0; i < NUM_DATA_OBJECTS; i++) {
		rV = (UDATA) vm->sharedClassConfig->storeAttachedData(currentThread, dataList[i].keyAddress, &dataList[i].data, false);
#if defined(J9SHR_CACHELET_SUPPORT)
		if (J9SHR_RESOURCE_STORE_ERROR != rV) {
			ERRPRINTF2("j9shr_storeAttachedData returned incorrect error code for realtime cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rV);
			rc = FAIL;
		} else {
			INFOPRINTF("j9shr_storeAttachedData returned correct error code for realtime cache");
			rc = PASS;
		}
#else
		if (true == readOnlyCache) {
			if (J9SHR_RESOURCE_STORE_ERROR != rV) {
				ERRPRINTF2("j9shr_storeAttachedData returned incorrect code for read only cache, rV: %d for data index: %d\n", rV, i);
				rc = FAIL;
			} else {
				INFOPRINTF2("j9shr_storeAttachedData returned correct error code for read only cache, rV: %d for data index: %d\n", rV, i);
				rc = PASS;
			}
		} else {
			switch(rV) {
			case 0:
				INFOPRINTF1("j9shr_storeAttachedData successfully stored data for data index %d\n\t", i);
				rc = PASS;
				break;
			case J9SHR_RESOURCE_STORE_EXISTS:
			case J9SHR_RESOURCE_STORE_ERROR:
			case J9SHR_RESOURCE_STORE_FULL:
			case J9SHR_RESOURCE_PARAMETER_ERROR:
			default:
				ERRPRINTF2("j9shr_storeAttachedData failed with rc: 0x%x for data index: %d\n", rV, i);
				rc = FAIL;
				break;
			}
		}
#endif
		if (FAIL == rc) {
			break;
		}
	}

	return rc;
}

IDATA
AttachedDataTest::StoreAttachedDataFailure(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	SH_CacheMap *cacheMap;
	SH_CompositeCacheImpl *cc;
	J9SharedCacheHeader *ca;
	SharedAttachedData attachedData;
	const char *testName = "StoreAttachedDataFailure";
	const char *junkData = "junk";

	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	/* create data that will cause J9SHR_RESOURCE_STORE_EXISTS error,
	 * i.e. attached data already exists.
	 */
	attachedData.keyAddress = dataList[0].keyAddress;
	attachedData.data.address = (U_8 *)junkData;
	attachedData.data.length = strlen(junkData)+1;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	UDATA rv = vm->sharedClassConfig->storeAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, false);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rv) {
			ERRPRINTF2("j9shr_storeAttachedData did not return correct error code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rv);
			rc = FAIL;
			goto _exit;
		}
	} else if (J9SHR_RESOURCE_STORE_EXISTS != rv) {
		ERRPRINTF2("j9shr_storeAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_EXISTS, rv);
		rc = FAIL;
		goto _exit;
	}

	/* create data that will cause J9SHR_RESOURCE_STORE_ERROR error,
	 * i.e. addressInCache parameter of storeAttachedData() is not valid.
	 */
	attachedData.keyAddress = (void *)ca;
	attachedData.data.address = (U_8 *)junkData;
	attachedData.data.length = strlen(junkData)+1;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
 	j9tty_printf(PORTLIB, "\t");
	rv = vm->sharedClassConfig->storeAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, false);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rv) {
			ERRPRINTF2("j9shr_storeAttachedData did not return correct error code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rv);
			rc = FAIL;
			goto _exit;
		}
	} else if (J9SHR_RESOURCE_STORE_ERROR != rv) {
		ERRPRINTF2("j9shr_storeAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rv);
		rc = FAIL;
		goto _exit;
	} else {
		j9tty_printf(PORTLIB, "\t");
	}

	/* create data that will cause J9SHR_RESOURCE_PARAMETER_ERROR error,
	 * i.e. J9SharedDataDescriptor->type is invalid.
	 */
	attachedData.keyAddress = dataList[0].keyAddress;
	attachedData.data.address = (U_8 *)junkData;
	attachedData.data.length = strlen(junkData)+1;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_UNKNOWN;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rv = vm->sharedClassConfig->storeAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, false);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rv) {
			ERRPRINTF2("j9shr_storeAttachedData did not return correct error code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rv);
			rc = FAIL;
			goto _exit;
		}
	} else if (J9SHR_RESOURCE_PARAMETER_ERROR != rv) {
		ERRPRINTF2("j9shr_storeAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_PARAMETER_ERROR, rv);
		rc = FAIL;
		goto _exit;
	}

	/* create data that will cause J9SHR_RESOURCE_PARAMETER_ERROR error,
	 * i.e. J9SharedDataDescriptor->flags is invalid.
	 */
	attachedData.keyAddress = dataList[0].keyAddress;
	attachedData.data.address = (U_8 *)junkData;
	attachedData.data.length = strlen(junkData)+1;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS + 100;

	rv = vm->sharedClassConfig->storeAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, false);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rv) {
			ERRPRINTF2("j9shr_storeAttachedData did not return correct error code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rv);
			rc = FAIL;
			goto _exit;
		}
	} else if (J9SHR_RESOURCE_PARAMETER_ERROR != rv) {
		ERRPRINTF2("j9shr_storeAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_PARAMETER_ERROR, rv);
		rc = FAIL;
		goto _exit;
	}

	INFOPRINTF("StoreAttachedDataFailure: j9shr_storeAttachedData returned correct error codes\n\t");
	rc = PASS;

_exit:
	return rc;
}

IDATA
AttachedDataTest::FindAttachedData(J9JavaVM *vm, const void *addressInCache, J9SharedDataDescriptor *expectedData, bool expectCorruptData, I_32 expectedCorruptOffset, const char *caller)
{
	IDATA rc = PASS;
	J9SharedDataDescriptor data;
	IDATA corruptOffset;
	J9VMThread *currentThread;
	const char *testName = "FindAttachedData";
	PORT_ACCESS_FROM_JAVAVM(vm);

	currentThread = vm->internalVMFunctions->currentVMThread(vm);

	memset(&data, 0, sizeof(J9SharedDataDescriptor));
	data.type = expectedData->type;

	rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, addressInCache, &data, &corruptOffset);
#if defined(J9SHR_CACHELET_SUPPORT)
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("FindAttachedData: j9shr_findAttachedData returned incorrect error code for realtime cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
	} else {
		INFOPRINTF("FindAttachedData: j9shr_findAttachedData returned correct error code for realtime cache");
		rc = PASS;
	}
#else
	switch(rc) {
	case J9SHR_RESOURCE_PARAMETER_ERROR:
	case J9SHR_RESOURCE_STORE_ERROR:
		ERRPRINTF2("%s: j9shr_findAttachedData failed with rc: %d", caller, rc);
		rc = FAIL;
		break;
	default:
		if (true == expectCorruptData) {
			if (-1 != corruptOffset) {
				if (NULL != (U_8 *)rc) {
					ERRPRINTF1("%s: j9shr_findAttachedData successfully found corrupt data but did not return NULL", caller);
					rc = FAIL;
				} else {
					if (corruptOffset == expectedCorruptOffset) {
						INFOPRINTF1("%s: j9shr_findAttachedData successfully found corrupt data", caller);
						rc = PASS;
					} else {
						ERRPRINTF3("%s: j9shr_findAttachedData successfully found corrupt data but corruptOffset is incorrect. Expected = %d, got = %d", caller, expectedCorruptOffset, corruptOffset);
						rc = FAIL;
					}
				}
				vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
			} else {
				ERRPRINTF1("%s: j9shr_findAttachedData failed to find corrupt data", caller);
				rc = FAIL;
			}
			break;
		} else if (NULL != expectedData) {
			if (-1 != corruptOffset) {
				vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
				ERRPRINTF1("%s: j9shr_findAttachedData failed, found corrupt data", caller);
				rc = FAIL;
			} else {
				if (NULL != (U_8 *)rc) {
					U_8 *contents = (U_8 *)rc;
					if ((0 != memcmp(contents, expectedData->address, expectedData->length)) || (data.length != expectedData->length)) {
						vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
						ERRPRINTF1("j9shr_findAttachedData found incorrect data", caller);
						rc = FAIL;
						break;
					}
					vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
					INFOPRINTF1("%s: j9shr_findAttachedData successfully found data\n\t", caller);
					rc = PASS;
				} else {
					ERRPRINTF1("%s: j9shr_findAttachedData returned NULL", caller);
					rc = FAIL;
					break;
				}
			}
		} else {
			rc = FAIL;
		}
		break;
	}
#endif
	return rc;
}

IDATA
AttachedDataTest::FindAttachedDataSuccess(J9JavaVM *vm)
{
	IDATA rc = PASS;
	I_32 i;
	const char *testName = "FindAttachedDataSuccess";
	PORT_ACCESS_FROM_JAVAVM(vm);

	for (i = 0; i < NUM_DATA_OBJECTS; i++) {
		rc = FindAttachedData(vm, dataList[i].keyAddress, &(dataList[i].data), false, -1, testName);
		if (FAIL == rc) {
			ERRPRINTF1("failed for data index: %d", i);
			break;
		}
	}
	return rc;
}

IDATA
AttachedDataTest::FindAttachedDataFailure(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	SH_CacheMap *cacheMap;
	SH_CompositeCacheImpl *cc;
	J9SharedCacheHeader *ca;
	IDATA corruptOffset;
	SharedAttachedData attachedData;
	const char *testName = "FindAttachedDataFailure";
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	/* create data that will cause J9SHR_RESOURCE_PARAMETER_ERROR error,
	 * i.e. J9SharedDataDescriptor->flags is invalid.
	 */
	attachedData.keyAddress = dataList[0].keyAddress;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS+100;

	rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, &corruptOffset);
	if (J9SHR_RESOURCE_PARAMETER_ERROR != rc) {
		ERRPRINTF2("j9shr_findAttachedData did not return correct error code \t expected error code: %d, returned: %d", J9SHR_RESOURCE_PARAMETER_ERROR, rc);
		vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &attachedData.data);
		rc = FAIL;
		goto _exit;
	} else if (NULL != attachedData.data.address){
		ERRPRINTF2("j9shr_findAttachedData returned correct error code: %d but data is not NULL: 0x%p", J9SHR_RESOURCE_PARAMETER_ERROR, attachedData.data.address);
		vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &attachedData.data);
		rc = FAIL;
		goto _exit;
	}

	/* create data that will cause J9SHR_RESOURCE_STORE_ERROR error,
	 * i.e. data->address is non NULL and is not sufficient to store the data found.
	 */
	attachedData.keyAddress = dataList[0].keyAddress;
	attachedData.data.length = dataList[0].data.length - 5;
	attachedData.data.address = (U_8 *) j9mem_allocate_memory(attachedData.data.length, J9MEM_CATEGORY_CLASSES);
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, &corruptOffset);
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_findAttachedData did not return correct error code \t expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &attachedData.data);
		rc = FAIL;
		goto _exit;
	}

	/* test for non-existing key */
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	memset(&attachedData, 0, sizeof(SharedAttachedData));
	attachedData.keyAddress = (U_8 *)ca;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
	j9tty_printf(PORTLIB, "\t");
	rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, &corruptOffset);
	if ((NULL != (U_8 *)rc) || (NULL != attachedData.data.address)) {
		ERRPRINTF("j9shr_findAttachedData found data for non-existing key");
		vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &attachedData.data);
		rc = FAIL;
		goto _exit;
	}

	INFOPRINTF("j9shr_findAttachedData returned correct error codes\n\t");
	rc = PASS;

_exit:
	return rc;
}

IDATA
AttachedDataTest::FindAttachedDataUpdateCountFailure(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	IDATA corruptOffset;
	SharedAttachedData attachedData;
	const char *testName = "FindAttachedDataUpdateCountFailure";
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	attachedData.keyAddress = dataList[1].keyAddress;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, &corruptOffset);
	if (J9SHR_RESOURCE_TOO_MANY_UPDATES != rc) {
		ERRPRINTF2("j9shr_findAttachedData did not return correct error code \t expected error code: %d, returned: %d", J9SHR_RESOURCE_TOO_MANY_UPDATES, rc);
		vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &attachedData.data);
		rc = FAIL;
		goto _exit;
	} else {
		INFOPRINTF("FindAttachedDataUpdateCountFailure passed for updateCount test");
		goto _exit;
	}
_exit:
	return rc;
}

IDATA
AttachedDataTest::FindAttachedDataCorruptCountFailure(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	IDATA corruptOffset;
	SharedAttachedData attachedData;
	const char *testName = "FindAttachedDataCorruptCountFailure";
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	attachedData.keyAddress = dataList[2].keyAddress;
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, &corruptOffset);
	if (0 != rc) {
		ERRPRINTF2("j9shr_findAttachedData did not return correct error code \t expected error code: %d, returned: %d", 0, rc);
		vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &attachedData.data);
		rc = FAIL;
	} else {
		if (-1 != corruptOffset) {
			INFOPRINTF("j9shr_findAttachedData successfully found corrupt data");
			rc = PASS;
		} else {
			ERRPRINTF("j9shr_findAttachedData returned NULL but data is not marked as corrupt");
			rc = FAIL;
		}
	}
	if (PASS == rc) {
		INFOPRINTF("FindAttachedDataCorruptCountFailure passed for corruptCount test");
		goto _exit;
	}
_exit:
	return rc;
}

IDATA
AttachedDataTest::ReplaceAttachedData(J9JavaVM *vm)
{
	IDATA rc = PASS, i;
	J9VMThread *currentThread;
	SharedAttachedData attachedData;
	const char *testName = "ReplaceAttachedData";
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	attachedData.keyAddress = dataList[0].keyAddress;
	attachedData.data.length = 10;
	attachedData.data.address = (U_8 *)j9mem_allocate_memory(attachedData.data.length, J9MEM_CATEGORY_CLASSES);
	if (NULL == attachedData.data.address) {
		ERRPRINTF("failed to allocate memory for the data");
		rc = FAIL;
		goto _exit;
	}

	for( i = 0; i < (I_32)attachedData.data.length; i++) {
		attachedData.data.address[i] = (U_8)nextValue++;
	}
	attachedData.data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
	attachedData.data.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

	/* pass true to forceReplace parameter of j9shr_storeAttachedData */
	rc = (IDATA) vm->sharedClassConfig->storeAttachedData(currentThread, attachedData.keyAddress, &attachedData.data, true);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF2("j9shr_storeAttachedData returned incorrect code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
			rc = FAIL;
		} else {
			INFOPRINTF1("j9shr_storeAttachedData returned correct error code for read only cache, rc: %d", rc);
			rc = PASS;
		}
	} else {
		switch(rc) {
		case 0:
			INFOPRINTF("j9shr_storeAttachedData successfully replaced existing data\n\t");
			rc = PASS;
			break;
		case J9SHR_RESOURCE_STORE_EXISTS:
		case J9SHR_RESOURCE_STORE_ERROR:
		case J9SHR_RESOURCE_STORE_FULL:
		case J9SHR_RESOURCE_PARAMETER_ERROR:
		default:
			ERRPRINTF1("j9shr_storeAttachedData failed to replace data with rc: %d", rc);
			rc = FAIL;
			break;
		}
	}

	if ((false == readOnlyCache) && (PASS == rc)) {
		rc = FindAttachedData(vm, attachedData.keyAddress, &attachedData.data, false, -1, testName);
		if (FAIL == rc) {
			ERRPRINTF("failed to find replaced data");
			goto _exit;
		}
	}

_exit:
	if (NULL != attachedData.data.address) {
		j9mem_free_memory(attachedData.data.address);
	}
	return rc;
}

IDATA
AttachedDataTest::UpdateAttachedDataSuccess(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	SharedAttachedData attachedData;
	I_32 updateOffset, bytesToUpdate;
	U_8 *updateData;
	const char *testName = "UpdateAttachedDataSuccess";
	PORT_ACCESS_FROM_JAVAVM(vm);

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	updateData = (U_8 *) j9mem_allocate_memory(dataList[1].data.length, J9MEM_CATEGORY_CLASSES);
	if (NULL == updateData) {
		ERRPRINTF("failed to allocate memory for update data");
		rc = FAIL;
		goto _exit;
	}

	memcpy(updateData, dataList[1].data.address, dataList[1].data.length);

	updateOffset = (I_32)dataList[1].data.length/2;
	bytesToUpdate = (I_32)(dataList[1].data.length - updateOffset);
	while(bytesToUpdate > 0) {
		*(updateData+updateOffset+bytesToUpdate-1) = nextValue++;
		attachedData.data.length++;
		bytesToUpdate--;
	}

	attachedData.keyAddress = dataList[1].keyAddress;
	attachedData.data.address = (U_8 *)(updateData+updateOffset);
	attachedData.data.type = dataList[1].data.type;
	attachedData.data.flags = dataList[1].data.flags;

	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(currentThread, attachedData.keyAddress, updateOffset, &attachedData.data);
#if defined(J9SHR_CACHELET_SUPPORT)
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_updateAttachedData returned incorrect code for realtime cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
	} else {
		INFOPRINTF("j9shr_updateAttachedData returned correct code for realtime cache");
		rc = PASS;
	}
#else
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF2("j9shr_updateAttachedData returned incorrect code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
			rc = FAIL;
		} else {
			INFOPRINTF1("j9shr_updateAttachedData returned correct error code for read only cache, rc: %d", rc);
			rc = PASS;
		}
	} else {
		switch(rc) {
		case 0:
			INFOPRINTF("j9shr_updateAttachedData successfully updated the data\n\t");
			rc = PASS;
			break;
		case J9SHR_RESOURCE_STORE_ERROR:
		case J9SHR_RESOURCE_PARAMETER_ERROR:
		default:
			ERRPRINTF1("j9shr_updateAttachedData failed to update the data with rc: %d", rc);
			rc = FAIL;
			break;
		}
	}

	if ((false == readOnlyCache) && (PASS == rc)) {
		attachedData.data.address = updateData;
		attachedData.data.length = dataList[1].data.length;
		rc = FindAttachedData(vm, attachedData.keyAddress, &attachedData.data, false, -1, testName);
		if (FAIL == rc) {
			ERRPRINTF("failed to find updated data");
		}
	}
#endif

_exit:
	if (NULL != updateData) {
		j9mem_free_memory(updateData);
	}
	return rc;
}
IDATA
AttachedDataTest::UpdateJitHint(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	SharedAttachedData attachedData;
	I_32 updateOffset, bytesToUpdate;
	U_8 *updateData;
	const char *testName = "UpdateJitHint";
	PORT_ACCESS_FROM_JAVAVM(vm);

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	updateData = (U_8 *) j9mem_allocate_memory(dataList[JIT_HINT_OBJECT_BASE].data.length, J9MEM_CATEGORY_CLASSES);
	if (NULL == updateData) {
		ERRPRINTF("failed to allocate memory for update data");
		rc = FAIL;
		goto _exit;
	}

	memcpy(updateData, dataList[JIT_HINT_OBJECT_BASE].data.address, dataList[JIT_HINT_OBJECT_BASE].data.length);

	updateOffset = (I_32)dataList[JIT_HINT_OBJECT_BASE].data.length/2;
	bytesToUpdate = (I_32)(dataList[JIT_HINT_OBJECT_BASE].data.length - updateOffset);
	while(bytesToUpdate > 0) {
		*(updateData+updateOffset+bytesToUpdate-1) = nextValue++;
		attachedData.data.length++;
		bytesToUpdate--;
	}

	attachedData.keyAddress = dataList[JIT_HINT_OBJECT_BASE].keyAddress;
	attachedData.data.address = (U_8 *)(updateData+updateOffset);
	attachedData.data.type = dataList[JIT_HINT_OBJECT_BASE].data.type;
	attachedData.data.flags = dataList[JIT_HINT_OBJECT_BASE].data.flags;

	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(currentThread, attachedData.keyAddress, updateOffset, &attachedData.data);
#if defined(J9SHR_CACHELET_SUPPORT)
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_updateAttachedData returned incorrect code for realtime cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
	} else {
		INFOPRINTF("j9shr_updateAttachedData returned correct code for realtime cache");
		rc = PASS;
	}
#else
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF2("j9shr_updateAttachedData returned incorrect code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
			rc = FAIL;
		} else {
			INFOPRINTF1("j9shr_updateAttachedData returned correct error code for read only cache, rc: %d", rc);
			rc = PASS;
		}
	} else {
		switch(rc) {
		case 0:
			INFOPRINTF("j9shr_updateAttachedData successfully updated the data\n\t");
			rc = PASS;
			break;
		case J9SHR_RESOURCE_STORE_ERROR:
		case J9SHR_RESOURCE_PARAMETER_ERROR:
		default:
			ERRPRINTF1("j9shr_updateAttachedData failed to update the data with rc: %d", rc);
			rc = FAIL;
			break;
		}
	}

	if ((false == readOnlyCache) && (PASS == rc)) {
		attachedData.data.address = updateData;
		attachedData.data.length = dataList[JIT_HINT_OBJECT_BASE].data.length;
		rc = FindAttachedData(vm, attachedData.keyAddress, &attachedData.data, false, -1, testName);
		if (FAIL == rc) {
			ERRPRINTF("failed to find updated data");
		}
	}
#endif

_exit:
	if (NULL != updateData) {
		j9mem_free_memory(updateData);
	}
	return rc;
}

IDATA
AttachedDataTest::UpdateAttachedDataFailure(J9JavaVM *vm)
{
	IDATA rc = PASS, i;
	J9VMThread *currentThread;
	SH_CacheMap *cacheMap;
	SH_CompositeCacheImpl *cc;
	J9SharedCacheHeader *ca;
	SharedAttachedData attachedData;
	I_32 updateOffset;
	U_8 *updateData;
	const char *testName = "UpdateAttachedDataFailure";
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	memset(&attachedData, 0, sizeof(SharedAttachedData));

	updateData = (U_8 *) j9mem_allocate_memory(dataList[1].data.length/2, J9MEM_CATEGORY_CLASSES);
	if (NULL == updateData) {
		ERRPRINTF("UpdateAttachedDataFailure: failed to allocate memory for updatedata");
		rc = FAIL;
		goto _exit;
	}
	for(i = 0; i < (IDATA)dataList[1].data.length/2; i++) {
		*(updateData+i) = nextValue++;
	}

	/* ensure updateOffset + size of updateData is more than dataList[1].data.length */
	updateOffset = (I_32)((dataList[1].data.length - dataList[1].data.length/2) + 1);

	attachedData.keyAddress = dataList[1].keyAddress;
	attachedData.data.address = updateData;
	attachedData.data.length = dataList[1].data.length/2;
	attachedData.data.type = dataList[1].data.type;
	attachedData.data.flags = dataList[1].data.flags;

	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(currentThread, attachedData.keyAddress, updateOffset, &attachedData.data);
	/* no need to handle read-only cache case, as it also returns same error code */
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_updateAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
		goto _exit;
	}

	/* flags parameter is invalid */
	attachedData.keyAddress = dataList[1].keyAddress;
	attachedData.data.address = updateData;
	attachedData.data.length = dataList[1].data.length/2;
	attachedData.data.type = dataList[1].data.type;
	attachedData.data.flags = (UDATA)-1;
	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(currentThread, attachedData.keyAddress, 0, &attachedData.data);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF1("j9shr_storeAttachedData returned incorrect code for read only cache, rc: %d", rc);
			rc = FAIL;
			goto _exit;
		}
	} else {
		if (J9SHR_RESOURCE_PARAMETER_ERROR != rc) {
			ERRPRINTF2("j9shr_updateAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_PARAMETER_ERROR, rc);
			rc = FAIL;
			goto _exit;
		}
	}

	/* test for updating data for a non-existing key */
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();

	attachedData.keyAddress = (U_8 *)ca;
	attachedData.data.address = updateData;
	attachedData.data.length = dataList[1].data.length/2;
	attachedData.data.type = dataList[1].data.type;
	attachedData.data.flags = dataList[1].data.flags;
	j9tty_printf(PORTLIB, "\t");
	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(currentThread, attachedData.keyAddress, 0, &attachedData.data);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF1("j9shr_storeAttachedData returned incorrect code for read only cache, rc: %d", rc);
			rc = FAIL;
			goto _exit;
		}
	} else if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF2("j9shr_updateAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
			rc = FAIL;
			goto _exit;
	}

	INFOPRINTF("j9shr_updateAttachedData returned correct error codes\n\t");
	rc = PASS;
_exit:
	if (NULL != updateData) {
		j9mem_free_memory(updateData);
	}
	return rc;
}

IDATA
AttachedDataTest::UpdateAttachedUDATASuccess(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	I_32 updateOffset;
	UDATA updateData;
	I_32 align = sizeof(UDATA);
    const char* testName = "UpdateAttachedUDATASuccess";
	PORT_ACCESS_FROM_JAVAVM(vm);

	currentThread = vm->internalVMFunctions->currentVMThread(vm);

	updateData = (UDATA)nextValue++;
	/* make sure updataOffset is UDATA aligned */
	updateOffset = ((I_32)dataList[1].data.length/2 > align) ? (I_32)((dataList[1].data.length/2) & (~(align-1))) : 0;

	rc = (IDATA) vm->sharedClassConfig->updateAttachedUDATA(currentThread, dataList[1].keyAddress, dataList[1].data.type, updateOffset, updateData);
#if defined(J9SHR_CACHELET_SUPPORT)
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_updateAttachedUDATA returned incorrect code for realtime cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
	} else {
		INFOPRINTF("j9shr_updateAttachedUDATA returned correct code for realtime cache");
		rc = PASS;
	}
#else
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF2("j9shr_updateAttachedUDATA returned incorrect code for read only cache, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
			rc = FAIL;
		} else {
			INFOPRINTF1("j9shr_updateAttachedUDATA returned correct error code for read only cache, rc: %d", rc);
			rc = PASS;
		}
	} else {
		switch(rc) {
		case 0:
			INFOPRINTF("j9shr_updateAttachedUDATA successfully updated the data\n\t");
			rc = PASS;
			break;
		case J9SHR_RESOURCE_STORE_ERROR:
		case J9SHR_RESOURCE_PARAMETER_ERROR:
		default:
			ERRPRINTF1("j9shr_updateAttachedUDATA failed to update the data with rc: %d", rc);
			rc = FAIL;
			break;
		}
	}

	if ((false == readOnlyCache) && (PASS == rc)) {
		J9SharedDataDescriptor data;
		UDATA *contents;
		IDATA corruptOffset;

		memset(&data, 0, sizeof(J9SharedDataDescriptor));
		data.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;

		rc = (IDATA) vm->sharedClassConfig->findAttachedData(currentThread, dataList[1].keyAddress, &data, &corruptOffset);
		switch(rc) {
		case 0:
		case J9SHR_RESOURCE_PARAMETER_ERROR:
		case J9SHR_RESOURCE_STORE_ERROR:
			ERRPRINTF1("j9shr_findAttachedUDATA failed to find updated value with rc: %d", rc);
			rc = FAIL;
			break;
		default:
			if (-1 != corruptOffset) {
				ERRPRINTF("j9shr_findAttachedUDATA found corrupt data");
				vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
				rc = FAIL;
				break;
			}
			contents = (UDATA *)((U_8 *)rc + updateOffset);
			if (*contents != updateData) {
				ERRPRINTF("j9shr_findAttachedUDATA failed to find updated value");
				vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
				rc = FAIL;
				break;
			}
			vm->sharedClassConfig->freeAttachedDataDescriptor(currentThread, &data);
			INFOPRINTF("j9shr_findAttachedUDATA successfully found the updated data\n\t");
			rc = PASS;
			break;
		}
	}
#endif

	return rc;
}

IDATA
AttachedDataTest::UpdateAttachedUDATAFailure(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	SH_CacheMap *cacheMap;
	SH_CompositeCacheImpl *cc;
	J9SharedCacheHeader *ca;
	I_32 updateOffset;
	UDATA updateData;
	const char *testName="UpdateAttachedUDATAFailure";
	I_32 align = sizeof(UDATA);
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif

	currentThread = vm->internalVMFunctions->currentVMThread(vm);

	updateData = (UDATA)nextValue++;

	updateOffset = ((I_32)dataList[1].data.length/2 > align) ? (I_32)((dataList[1].data.length/2) & (~(align-1))) : 0;
	/* make sure updateOffset is not UDATA aligned */
	updateOffset += 1;
	rc = (IDATA) vm->sharedClassConfig->updateAttachedUDATA(currentThread, dataList[1].keyAddress, dataList[1].data.type, updateOffset, updateData);
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_updateAttachedUDATA did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
		goto _exit;
	}

	/* updateOffset is UDATA aligned but out of range */
	updateOffset = (I_32)(dataList[1].data.length & (~(align-1)));
	if ((updateOffset & align) != 0) {
		updateOffset += align;
	}
	rc = (IDATA) vm->sharedClassConfig->updateAttachedUDATA(currentThread, dataList[1].keyAddress, dataList[1].data.type, updateOffset, updateData);
	if (J9SHR_RESOURCE_STORE_ERROR != rc) {
		ERRPRINTF2("j9shr_updateAttachedUDATA did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
		rc = FAIL;
		goto _exit;
	}

	/* test for updating data for a non-existing key */
	cacheMap = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	cc = (SH_CompositeCacheImpl *)cacheMap->getCompositeCacheAPI();
	ca = cc->getCacheHeaderAddress();
	j9tty_printf(PORTLIB, "\t");
	rc = (IDATA) vm->sharedClassConfig->updateAttachedUDATA(currentThread, (U_8 *)ca, J9SHR_ATTACHED_DATA_TYPE_JITPROFILE, 0, updateData);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF1("j9shr_storeAttachedData returned incorrect code for read only cache, rc: %d", rc);
			rc = FAIL;
			goto _exit;
		}
	} else if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF2("j9shr_updateAttachedData did not return correct error code, expected error code: %d, returned: %d", J9SHR_RESOURCE_STORE_ERROR, rc);
			rc = FAIL;
			goto _exit;
	}
	INFOPRINTF("j9shr_updateAttachedUDATA returned correct error codes");
	rc = PASS;

_exit:
	return rc;
}

IDATA
AttachedDataTest::CorruptAttachedData(J9JavaVM *vm)
{
	IDATA rc = PASS;
	J9VMThread *currentThread;
	J9SharedDataDescriptor data;
	const char *testName = "CorruptAttachedData";
	const char *content = "corrupt";
	I_32 updateAtOffset = 1;
	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9SHR_CACHELET_SUPPORT)
	return PASS;
#endif
	INFOPRINTF("Running CorruptAttachedData\n\t");
	currentThread = vm->internalVMFunctions->currentVMThread(vm);

	memset(&data, 0, sizeof(J9SharedDataDescriptor));
	data.address = (U_8 *) j9mem_allocate_memory(strlen(content) + 1, J9MEM_CATEGORY_CLASSES);
	if (NULL == data.address) {
		ERRPRINTF("failed to allocate memory for update data");
		rc = FAIL;
		goto _exit;
	}
	strncpy((char *)data.address, content, strlen(content));
	data.address[strlen(content)] = '\0';
	data.length = strlen(content) + 1;
	data.type = dataList[2].data.type;
	data.flags = dataList[2].data.flags;
	j9tty_printf(PORTLIB, "\t");
	/* Set unitTest so that SH_AttachedDataManager::updateDataInCache() can mark data as corrupt */
	UnitTest::unitTest = UnitTest::ATTACHED_DATA_TEST;

	rc = (IDATA) vm->sharedClassConfig->updateAttachedData(currentThread, dataList[2].keyAddress, updateAtOffset, &data);
	if (true == readOnlyCache) {
		if (J9SHR_RESOURCE_STORE_ERROR != rc) {
			ERRPRINTF1("j9shr_storeAttachedData returned incorrect code for read only cache, rc: %d", rc);
			rc = FAIL;
		} else {
			INFOPRINTF1("j9shr_storeAttachedData returned correct error code for read only cache, rc: %d\n\t", rc);
			rc = PASS;
		}
	} else {
		switch(rc) {
		case 0:
			rc = PASS;
			break;
		case J9SHR_RESOURCE_STORE_ERROR:
		case J9SHR_RESOURCE_PARAMETER_ERROR:
		default:
			ERRPRINTF1("j9shr_updateAttachedData failed to update the data with rc: %d", rc);
			rc = FAIL;
			break;
		}
	}

	UnitTest::unitTest = UnitTest::NO_TEST;

	j9mem_free_memory(data.address);
	data.address = NULL;

	if ((false == readOnlyCache) && (PASS == rc)) {
		j9tty_printf(PORTLIB, "\t");
		rc = FindAttachedData(vm, dataList[2].keyAddress, &data, true, updateAtOffset, testName);
		if (FAIL == rc) {
			ERRPRINTF("failed to find updated corrupt data");
		}
	}

_exit:
	return rc;
}

extern "C" {

IDATA createThread(J9JavaVM *vm, omrthread_t *osThread, J9VMThread **vmThread, omrthread_entrypoint_t entryPoint, void *entryArg) {
	const char *testName = "createThread";
	IDATA rc = PASS;
	J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);
	/* Start thread in suspended state */
	rc = omrthread_create_ex(osThread, J9THREAD_ATTR_DEFAULT, 1 /* suspended */, entryPoint, entryArg);
	if (0 != rc) {
		ERRPRINTF("Failed to create OS thread");
		rc = FAIL;
		goto end;
	}

	*vmThread = vm->internalVMFunctions->allocateVMThread(vm, *osThread, 0, currentThread->omrVMThread->memorySpace, NULL);
	if (NULL == (*vmThread)) {
		ERRPRINTF("Failed to allocate VM thread");
		rc = FAIL;
		goto end;
	}

end:
	return rc;
}

int J9THREAD_PROC
startReader(void *entryArg) {
	const char *testName = "startReader";
	IDATA rc = PASS;
	AttachedDataTest *adt = (AttachedDataTest *)entryArg;
	J9JavaVM *vm = adt->vm;
	J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

	/* writer thread must have failed, no need to proceed */
	if (true == adt->cancelThread) {
		goto _exitCloseCache;
	}

    /* Reader thread needs to use same cache memory as writer thread */
    rc = adt->openTestCache(vm, writerCacheMemory, writerCacheSize, J9PORT_SHR_CACHE_TYPE_PERSISTENT, J9SHR_RUNTIMEFLAG_ENABLE_READONLY, 0);
    if (FAIL == rc) {
    	ERRPRINTF("openTestCache failed");
    	goto _exitCloseCache;
    }

	UnitTest::unitTest = UnitTest::ATTACHED_DATA_UPDATE_COUNT_TEST;

	/* writer and reader thread should share same data list */
	adt->dataList = dataList4Threads;

	rc = adt->FindAttachedDataUpdateCountFailure(vm);
	if (FAIL == rc) {
		ERRPRINTF("FindAttachedDataReadOnlyFailure failed");
		goto _exitCloseCache;
	}

	INFOPRINTF("Failed to find data due to too many updates\n");

	UnitTest::unitTest = UnitTest::ATTACHED_DATA_CORRUPT_COUNT_TEST;

	rc = adt->FindAttachedDataCorruptCountFailure(vm);
	if (FAIL == rc) {
		ERRPRINTF("FindAttachedDataCorruptCountFailure failed");
		goto _exitCloseCache;
	}

	INFOPRINTF("Successfully found corrupt data\n");

_exitCloseCache:
	adt->closeTestCache(vm, false);
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	UnitTest::unitTest = UnitTest::NO_TEST;
	vm->internalVMFunctions->threadCleanup(currentThread, 0);
	INFOPRINTF("Reader thread exited\n");
	adt->threadExited = true;
	return (int)rc;
}

int J9THREAD_PROC
startWriter(void *entryArg) {
	const char *testName = "startWriter";
	IDATA rc = PASS;
	AttachedDataTest *adt = (AttachedDataTest *)entryArg;
	J9JavaVM *vm = adt->vm;
	J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);

	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

    rc = adt->openTestCache(vm, NULL, 0, J9PORT_SHR_CACHE_TYPE_PERSISTENT, 0, 0);
    if (FAIL == rc) {
    	ERRPRINTF("openTestCache failed");
    	goto _exitCloseCache;
    }
    writerCacheMemory = adt->cacheMemory;
    writerCacheSize = adt->piConfig->sharedClassCacheSize;

	rc = adt->initializeAttachedData(vm);
	if (FAIL == rc) {
		ERRPRINTF("initializeAttachedData failed");
		goto _exitCloseCache;
	}

	/* writer and reader thread should share same data list, so save this for reader to use
	 * Currently reader thread always reads after writer thread populates
	 */
	dataList4Threads = adt->dataList;

	rc = adt->StoreAttachedDataSuccess(vm);
	if (FAIL == rc) {
		ERRPRINTF("StoreAttachedDataSuccess failed");
		goto _exitCloseCache;
	}

	omrthread_suspend();

	if (true == adt->cancelThread) {
		/* reader thread must have failed, no need to proceed */
		goto _exitCloseCache;
	}

	rc = adt->UpdateAttachedDataSuccess(vm);
	if (FAIL == rc) {
		ERRPRINTF("UpdateAttachedDataSuccess failed");
		goto _exitCloseCache;
	}
	INFOPRINTF("Successfully updated data\n\t");
	omrthread_suspend();

	if (true == adt->cancelThread) {
		/* reader thread must have failed, no need to proceed */
		goto _exitCloseCache;
	}

	rc = adt->UpdateAttachedDataSuccess(vm);
	if (FAIL == rc) {
		ERRPRINTF("UpdateAttachedDataSuccess failed");
		goto _exitCloseCache;
	}
	INFOPRINTF("Successfully updated data\n\t");

	omrthread_suspend();

	rc = adt->CorruptAttachedData(vm);
	if (FAIL == rc) {
		ERRPRINTF("UpdateAttachedDataSuccess failed");
		goto _exitCloseCache;
	}
	INFOPRINTF("Successfully corrupted data\n\t");

	omrthread_suspend();

_exitCloseCache:
	adt->closeTestCache(vm, true);
	vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	vm->internalVMFunctions->threadCleanup(currentThread, 0);
	INFOPRINTF("Writer thread exited\n");
	adt->threadExited = true;
	return (int)rc;
}

IDATA
testFindReadOnly(J9JavaVM *vm) {
	const char *testName = "testFindReadOnly";
	IDATA rc = PASS;
	I_32 retryCount = 0;
	AttachedDataTest adtReader, adtWriter;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Setup writer thread */
	adtWriter.vm = (J9JavaVM *) j9mem_allocate_memory(sizeof(J9JavaVM), J9MEM_CATEGORY_CLASSES);
	if (NULL != adtWriter.vm) {
		memcpy(adtWriter.vm, vm, sizeof(J9JavaVM));
	} else {
		ERRPRINTF("Failed to allocate memory for writer VM");
	}

	/* Setup reader thread */
	adtReader.vm = (J9JavaVM *) j9mem_allocate_memory(sizeof(J9JavaVM), J9MEM_CATEGORY_CLASSES);
	if (NULL != adtReader.vm) {
		memcpy(adtReader.vm, vm, sizeof(J9JavaVM));
	} else {
		ERRPRINTF("Failed to allocate memory for reader VM");
	}

	/* Starts writer thread in suspended state */
	rc = createThread(adtWriter.vm, &adtWriter.osThread, &adtWriter.vmThread, startWriter, &adtWriter);
	if (FAIL == rc) {
		ERRPRINTF("createThread failed");
		goto _exit;
	}

	/* Starts reader thread in suspended state */
	rc = createThread(adtReader.vm, &adtReader.osThread, &adtReader.vmThread, startReader, &adtReader);
	if (FAIL == rc) {
		ERRPRINTF("createThread failed");
		goto _exit;
	}

	/* Resume writer thread to populate the cache with attached data */
	omrthread_resume(adtWriter.osThread);

	/* Wait for writer thread to be suspended */
	while (adtWriter.isThreadSuspended() == false) {
		/* If writer thread exited without getting suspended, stop reader thread as well */
		if (true == adtWriter.threadExited) {
			adtReader.cancelThread = true;
			/* Give reader thread a chance to exit gracefully */
			omrthread_resume(adtReader.osThread);
			rc = FAIL;
			goto _exit;
		}
		omrthread_sleep(MAIN_THREAD_WAIT_TIME);
	}

	while (retryCount <= FIND_ATTACHED_DATA_RETRY_COUNT) {

		/* Resume reader thread to call findAttachedData() */
		omrthread_resume(adtReader.osThread);

		/* Wait for reader thread to be suspended */
		while (adtReader.isThreadSuspended() == false) {
			/* If reader thread exited without getting suspended, stop writer thread as well */
			if (true == adtReader.threadExited) {
				adtWriter.cancelThread = true;
				/* Give writer thread a chance to exit gracefully */
				omrthread_resume(adtWriter.osThread);
				rc = FAIL;
				goto _exit;
			}
			omrthread_sleep(MAIN_THREAD_WAIT_TIME);
		}

		/* Resume writer thread to update stored data in cache, causing reader's attempt to find data to fail */
		omrthread_resume(adtWriter.osThread);

		/* Wait for writer thread to be suspended */
		while (adtWriter.isThreadSuspended() == false) {
			/* If writer thread exited without getting suspended, stop reader thread as well */
			if (true == adtWriter.threadExited) {
				adtReader.cancelThread = true;
				/* Give reader thread a chance to exit gracefully.
				 * Clear UnitTest::unitTest so that reader thread can exit without suspending itself again.
				 */
				UnitTest::unitTest = UnitTest::NO_TEST;
				omrthread_resume(adtReader.osThread);
				rc = FAIL;
				goto _exit;
			}
			omrthread_sleep(MAIN_THREAD_WAIT_TIME);
		}

		retryCount++;
	}

	/* Resume reader thread.
	 * It should exit from findAttachedData() with J9SHR_RESOURCE_TOO_MANY_UPDATES as updateCountTryCount exceeds FIND_ATTACHED_DATA_RETRY_COUNT.
	 */
	omrthread_resume(adtReader.osThread);

	/* Wait for reader thread to be suspended */
	while (adtReader.isThreadSuspended() == false) {
		/* If reader thread exited without getting suspended, stop writer thread as well */
		if (true == adtReader.threadExited) {
			adtWriter.cancelThread = true;
			/* Give writer thread a chance to exit gracefully */
			omrthread_resume(adtWriter.osThread);
			rc = FAIL;
			goto _exit;
		}
		omrthread_sleep(MAIN_THREAD_WAIT_TIME);
	}

	/* Resume writer thread to corrupt data */
	omrthread_resume(adtWriter.osThread);

	/* Wait for writer thread to be suspended */
	while (adtWriter.isThreadSuspended() == false) {
		/* If writer thread exited without getting suspended, stop reader thread as well */
		if (true == adtWriter.threadExited) {
			adtReader.cancelThread = true;
			/* Give reader thread a chance to exit gracefully.
			 * Clear UnitTest::unitTest so that reader thread can exit without suspending itself again.
			 */
			UnitTest::unitTest = UnitTest::NO_TEST;
			omrthread_resume(adtReader.osThread);
			rc = FAIL;
			goto _exit;
		}
		omrthread_sleep(MAIN_THREAD_WAIT_TIME);
	}

	/* Resume reader thread to detect corrupt data and exit. */
	omrthread_resume(adtReader.osThread);

	while (false == adtReader.threadExited) {
		/* wait for reader thread to exit */
		omrthread_sleep(MAIN_THREAD_WAIT_TIME);
	}

	/* Resume writer thread to clean up the cache. */
	omrthread_resume(adtWriter.osThread);

	while (false == adtWriter.threadExited) {
		/* wait for writer thread to exit */
		omrthread_sleep(MAIN_THREAD_WAIT_TIME);
	}

_exit:
	if(NULL != adtWriter.vm) {
		j9mem_free_memory(adtWriter.vm);
	}
	if(NULL != adtReader.vm) {
		j9mem_free_memory(adtReader.vm);
	}

	return rc;
}

IDATA
testAttachedData(J9JavaVM* vm)
{
	IDATA rc = PASS;
	I_32 cacheType, i;
	bool readOnly;
	const char *testName = "testAttachedData";
	J9VMThread *currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);

	REPORT_START("AttachedDataTest");

	currentThread = vm->internalVMFunctions->currentVMThread(vm);
	vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

	for(i = 0; i < 4; i++) {
		U_64 extraRuntimeFlag = 0;
		/* Currently not in use, but can be used in future to unset runtime flags if required */
		U_64 unsetTheseRunTimeFlag = 0;
		AttachedDataTest adt;
		cacheType = 0;

		switch(i) {
		case 0:
#if !(defined(J9ZOS390))
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			readOnly = false;
#endif
			break;
		case 1:
#if !defined(J9SHR_CACHELET_SUPPORT)
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			readOnly = false;
#endif
			break;
		case 2:
#if !(defined(J9ZOS390))
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			readOnly = true;
#endif
			break;
		case 3:
#if !defined(J9SHR_CACHELET_SUPPORT)
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			readOnly = true;
#endif
			break;
		}

		if (0 != cacheType) {
			INFOPRINTF2("Running test with cacheType: %d and readOnly: %d\n\t", cacheType, readOnly);

			rc = adt.openTestCache(vm, NULL, 0, cacheType, extraRuntimeFlag, unsetTheseRunTimeFlag);
			if (FAIL == rc) {
				ERRPRINTF("openTestCache failed");
				goto _exitCloseCache;
			}

			rc = adt.initializeAttachedData(vm);
			if (FAIL == rc) {
				ERRPRINTF("initializeAttachedData failed");
				goto _exitClearData;
			}

			rc = adt.StoreAttachedDataSuccess(vm);
			if (FAIL == rc) {
				ERRPRINTF("StoreAttachedDataSuccess failed");
				goto _exitClearData;
			}

			if (true == readOnly) {
				/* close read-write cache and open it again as read-only */
				adt.closeTestCache(vm, false);
				extraRuntimeFlag |= J9SHR_RUNTIMEFLAG_ENABLE_READONLY;
				rc = adt.openTestCache(vm, NULL, 0, cacheType, extraRuntimeFlag, unsetTheseRunTimeFlag);
				if (FAIL == rc) {
					ERRPRINTF("openTestCache failed");
					goto _exitCloseCache;
				}

				adt.readOnlyCache = readOnly;
				rc = adt.StoreAttachedDataSuccess(vm);
				if (FAIL == rc) {
					ERRPRINTF("StoreAttachedDataSuccess failed");
					goto _exitClearData;
				}
			}

			rc = adt.StoreAttachedDataFailure(vm);
			if (FAIL == rc) {
				ERRPRINTF("StoreAttachedDataFailure failed");
				goto _exitClearData;
			}

			rc = adt.FindAttachedDataSuccess(vm);
			if (FAIL == rc) {
				ERRPRINTF("FindAttachedDataSuccess failed");
				goto _exitClearData;
			}

			rc = adt.FindAttachedDataFailure(vm);
			if (FAIL == rc) {
				ERRPRINTF("FindAttachedDataFailure failed");
				goto _exitClearData;
			}

			rc = adt.ReplaceAttachedData(vm);
			if (FAIL == rc) {
				ERRPRINTF("ReplaceAttachedData failed");
				goto _exitClearData;
			}

			rc = adt.UpdateAttachedDataSuccess(vm);
			if (FAIL == rc) {
				ERRPRINTF("UpdateAttachedDataSuccess failed");
				goto _exitClearData;
			}

			rc = adt.UpdateJitHint(vm);
			if (FAIL == rc) {
				ERRPRINTF("UpdateAttachedDataSuccess failed");
				goto _exitClearData;
			}

			rc = adt.UpdateAttachedDataFailure(vm);
			if (FAIL == rc) {
				ERRPRINTF("UpdateAttachedDataFailure failed");
				goto _exitClearData;
			}

			rc = adt.UpdateAttachedUDATASuccess(vm);
			if (FAIL == rc) {
				ERRPRINTF("UpdateAttachedUDATASuccess failed");
				goto _exitClearData;
			}

			rc = adt.UpdateAttachedUDATAFailure(vm);
			if (FAIL == rc) {
				ERRPRINTF("UpdateAttachedUDATAFailure failed");
				goto _exitClearData;
			}

			rc = adt.CorruptAttachedData(vm);
			if (FAIL == rc) {
				ERRPRINTF("CorruptAttachedData failed");
				goto _exitClearData;
			}

_exitClearData:
			adt.freeAttachedData(vm);

_exitCloseCache:
			adt.closeTestCache(vm, true);

			if (FAIL == rc) {
				break;
			}
		}
	}

	rc = testFindReadOnly(vm);
	if (FAIL == rc) {
		ERRPRINTF("testFindReadOnly failed");
	}

	UnitTest::unitTest = UnitTest::NO_TEST;
	UnitTest::cacheSize = 0;
	UnitTest::cacheMemory = NULL;

	vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	REPORT_SUMMARY("AttachedDataTest", rc);
	return rc;

}

} /* extern "C" */
