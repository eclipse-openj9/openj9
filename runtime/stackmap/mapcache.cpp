/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "rommeth.h"
#include "stackmap_api.h"

extern "C" {

void
initializeBasicROMMethodInfo(J9StackWalkState *walkState, J9ROMMethod *romMethod)
{
	J9ROMMethodInfo *romMethodInfo = &walkState->romMethodInfo;
	memset(romMethodInfo, 0, sizeof(*romMethodInfo));
	romMethodInfo->argCount = romMethod->argCount;
	romMethodInfo->tempCount = romMethod->tempCount;
	romMethodInfo->modifiers = romMethod->modifiers;
#if defined(J9MAPCACHE_DEBUG)
	romMethodInfo->flags = J9MAPCACHE_VALID;
#endif /* J9MAPCACHE_DEBUG */
	if (!(romMethod->modifiers & J9AccStatic)) {
		if (J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod))[0] == '<') {
			romMethodInfo->flags |= J9MAPCACHE_METHOD_IS_CONSTRUCTOR;
		}
	}
}

void
populateROMMethodInfo(J9StackWalkState *walkState, J9ROMMethod *romMethod, void *key)
{
	initializeBasicROMMethodInfo(walkState, romMethod);
#if 0
	bool found = false;
	J9Method *method = walkState->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
	omrthread_monitor_t mapCacheMutex = classLoader->mapCacheMutex;

	/* If the mapCacheMutex exists, the caching feature is enabled */
	if (NULL != mapCacheMutex) {
		omrthread_monitor_enter(mapCacheMutex);
		J9HashTable *mapCache = classLoader->romMethodInfoCache;

		/* If the cache exists, check it for this key */
		if (NULL != mapCache) {
			J9ROMMethodInfo exemplar = { 0 };
			exemplar.key = key;
			J9ROMMethodInfo *entry = (J9ROMMethodInfo*)hashTableFind(mapCache, &exemplar);

			if (NULL != entry) {
				/* Cache hit - copy the info */
				*romMethodInfo = *entry;
			} else {
				/* Cache miss - populate the info and cache it */
			}
		}

		omrthread_monitor_exit(mapCacheMutex);
	}
#endif
}

#if 0

/**
 * @brief Map cache hash function
 * @param key J9MapCacheEntry pointer
 * @param userData not used
 * @return hash value
 */
static UDATA
localMapHashFn(void *key, void *userData)
{
	J9MapCacheEntry *entry = (J9MapCacheEntry*)key;

	return (UDATA)entry->key;
}

/**
 * @brief Map cache comparator function
 * @param leftKey J9MapCacheEntry pointer
 * @param rightKey J9MapCacheEntry pointer
 * @param userData not used
 * @return 0 if comparision fails, non-zero if succeess
 */
static UDATA
localMapHashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	J9MapCacheEntry *leftEntry = (J9MapCacheEntry*)leftKey;
	J9MapCacheEntry *rightEntry = (J9MapCacheEntry*)rightKey;

	return (leftEntry->key == rightEntry->key);
}

/**
* @brief Check the given cache for the key. If found, populate the resultArray.
* @param vm the J9JavaVM
* @param classLoader the J9ClassLoader
* @param key the cache key
* @param mapCache the cache to search
* @param resultArrayBase the result array
* @param mapWords the numhber of U_32 in the result array
* @return true on cache hit, false on miss
*/
static bool
checkCache(J9JavaVM *vm, J9ClassLoader *classLoader, void *key, J9HashTable *mapCache, U_32 *resultArrayBase, UDATA mapWords)
{
	bool found = false;
	omrthread_monitor_t mapCacheMutex = classLoader->mapCacheMutex;

	/* If the mapCacheMutex exists, the caching feature is enabled */
	if (NULL != mapCacheMutex) {
		if (mapWords <= J9_MAP_CACHE_SLOTS) {
			omrthread_monitor_enter(mapCacheMutex);

			/* If the cache exists, check it for this key */
			if (NULL != mapCache) {
				J9MapCacheEntry exemplar = { 0 };
				exemplar.key = key;
				J9MapCacheEntry *entry = (J9MapCacheEntry*)hashTableFind(mapCache, &exemplar);

				if (NULL != entry) {
					memcpy(resultArrayBase, &entry->data.bits, sizeof(U_32) * mapWords);
					found = true;
				}
			}

			omrthread_monitor_exit(mapCacheMutex);
		}
	}

	return found;
}

/**
* @brief Cache a result in the given cache for the key.
* @param vm the J9JavaVM
* @param classLoader the J9ClassLoader
* @param key the cache key
* @param cacheSlot pointer to the slot holding the cache pointer
* @param resultArrayBase the result array
* @param mapWords the numhber of U_32 in the result array
*/
static void
updateCache(J9JavaVM *vm, J9ClassLoader *classLoader, void *key, J9HashTable **cacheSlot, U_32 *resultArrayBase, UDATA mapWords)
{
	omrthread_monitor_t mapCacheMutex = classLoader->mapCacheMutex;

	/* If the mapCacheMutex exists, the caching feature is enabled */
	if (NULL != mapCacheMutex) {
		if (mapWords <= J9_MAP_CACHE_SLOTS) {
			omrthread_monitor_enter(mapCacheMutex);

			/* Create the cache if necessary - failure is non-fatal */
			J9HashTable *mapCache = *cacheSlot;
			if (NULL == mapCache) {
				mapCache = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary),
						J9_GET_CALLSITE(),
						0,
						sizeof(J9MapCacheEntry),
						sizeof(J9MapCacheEntry *),
						0,
						J9MEM_CATEGORY_VM,
						localMapHashFn,
						localMapHashEqualFn,
						NULL,
						NULL);
				*cacheSlot = mapCache;
			}

			/* If the cache exists, attempt to add and entry for this map - failure is non-fatal */
			if (NULL != mapCache) {
				J9MapCacheEntry entry = { 0 };
				entry.key = key;
				memcpy(&entry.data.bits, resultArrayBase, sizeof(U_32) * mapWords);
				hashTableAdd(mapCache, &entry);
			}

			omrthread_monitor_exit(mapCacheMutex);
		}
	}
}

IDATA
j9cached_StackBitsForPC(UDATA pc, J9ROMClass * romClass, J9ROMMethod * romMethod,
								U_32 * resultArrayBase, UDATA resultArraySize,
								void * userData,
								UDATA * (* getBuffer) (void * userData),
								void (* releaseBuffer) (void * userData),
								J9JavaVM *vm, J9ClassLoader *classLoader)
{
	UDATA mapWords = (resultArraySize + 31) >> 5;
	void *bytecodePC = (void*)(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + pc);
	IDATA rc = 0;

	if (!checkCache(vm, classLoader, bytecodePC, classLoader->stackmapCache, resultArrayBase, mapWords)) {
		/* Cache miss - perform the map and attempt to cache the result if successful */
		rc = j9stackmap_StackBitsForPC(vm->portLibrary, pc, romClass, romMethod,
				resultArrayBase, resultArraySize, userData, getBuffer, releaseBuffer);
		if (0 == rc) {
			updateCache(vm, classLoader, bytecodePC, &classLoader->stackmapCache, resultArrayBase, mapWords);
		}
	}
	return rc;
}

void
j9cached_ArgBitsForPC0(J9ROMClass *romClass, J9ROMMethod *romMethod, U_32 *resultArrayBase, J9JavaVM *vm, J9ClassLoader *classLoader)
{
	UDATA mapWords = (J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + 31) >> 5;

	if (!checkCache(vm, classLoader, (void*)romMethod, classLoader->argsbitsCache, resultArrayBase, mapWords)) {
		/* Cache miss - perform the map and attempt to cache the result */
		j9localmap_ArgBitsForPC0(romClass, romMethod, resultArrayBase);
		updateCache(vm, classLoader, (void*)romMethod, &classLoader->argsbitsCache, resultArrayBase, mapWords);
	}
}

IDATA
j9cached_LocalBitsForPC(J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA pc, U_32 * resultArrayBase,
								void * userData,
								UDATA * (* getBuffer) (void * userData),
								void (* releaseBuffer) (void * userData),
								J9JavaVM *vm, J9ClassLoader * classLoader)
{
	IDATA rc = 0;
	void *bytecodePC = (void*)(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + pc);
	UDATA mapWords = (UDATA) ((J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + 31) >> 5);

	if (!checkCache(vm, classLoader, bytecodePC, classLoader->localmapCache, resultArrayBase, mapWords)) {
		/* Cache miss - perform the map and attempt to cache the result if successful */
		rc = vm->localMapFunction(vm->portLibrary, romClass, romMethod, pc, resultArrayBase, userData, getBuffer, releaseBuffer);
		if (0 == rc) {
			updateCache(vm, classLoader, bytecodePC, &classLoader->localmapCache, resultArrayBase, mapWords);
		}
	}

	return rc;
}

#endif


} /* extern "C" */
