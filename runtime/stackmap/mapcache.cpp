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

static UDATA
romMethodInfoHashFn(void *key, void *userData)
{
	J9ROMMethodInfo *entry = (J9ROMMethodInfo *)key;
	return (UDATA)entry->key;
}

static UDATA
romMethodInfoEqualFn(void *leftKey, void *rightKey, void *userData)
{
	J9ROMMethodInfo *left = (J9ROMMethodInfo *)leftKey;
	J9ROMMethodInfo *right = (J9ROMMethodInfo *)rightKey;
	return (left->key == right->key);
}

static bool
checkROMMethodInfoCache(J9ClassLoader *classLoader, void *key, J9ROMMethodInfo *outInfo)
{
	bool found = false;
	omrthread_monitor_t mapCacheMutex = classLoader->mapCacheMutex;

	if (NULL != mapCacheMutex) {
		omrthread_monitor_enter(mapCacheMutex);

		J9HashTable *cache = classLoader->romMethodInfoCache;
		if (NULL != cache) {
			J9ROMMethodInfo exemplar = {0};
			exemplar.key = key;
			J9ROMMethodInfo *entry = (J9ROMMethodInfo *)hashTableFind(cache, &exemplar);
			if (NULL != entry) {
				*outInfo = *entry;
				found = true;
			}
		}

		omrthread_monitor_exit(mapCacheMutex);
	}

	return found;
}

static void
updateROMMethodInfoCache(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMMethodInfo *info)
{
	omrthread_monitor_t mapCacheMutex = classLoader->mapCacheMutex;
	if (NULL == mapCacheMutex) {
		return; /* caching disabled */
	}

	omrthread_monitor_enter(mapCacheMutex);

	J9HashTable *cache = classLoader->romMethodInfoCache;
	if (NULL == cache) {
		/* Create the cache if it doesn't exist */
		cache = hashTableNew(
				OMRPORT_FROM_J9PORT(vm->portLibrary),
				J9_GET_CALLSITE(),
				0,
				sizeof(J9ROMMethodInfo),
				sizeof(J9ROMMethodInfo *),
				0,
				J9MEM_CATEGORY_VM,
				romMethodInfoHashFn,
				romMethodInfoEqualFn,
				NULL,
				NULL);
		classLoader->romMethodInfoCache = cache;
	}

	if (NULL != cache) {
		hashTableAdd(cache, info);
	}

	omrthread_monitor_exit(mapCacheMutex);
}

static void
populateROMMethodInfo(J9StackWalkState *walkState, J9ROMMethod *romMethod, void *key, UDATA pc, bool computeStackAndLocals)
{
	J9Method *method = walkState->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
	J9JavaVM *vm = walkState->javaVM;

	J9ROMMethodInfo newInfo = {0};
	newInfo.key = key;
	J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;

	/* Always compute argument bits */
	j9localmap_ArgBitsForPC0(romClass, romMethod, newInfo.argbits);

	if (computeStackAndLocals) {
		/* Compute stack map for this PC */
		j9stackmap_StackBitsForPC(
				vm->portLibrary,
				pc,
				romClass,
				romMethod,
				newInfo.stackmap,
				J9_STACKMAP_CACHE_SLOTS << 5,
				NULL,
				NULL,
				NULL);

		/* Compute local variable map for this PC */
		vm->localMapFunction(
				vm->portLibrary,
				romClass,
				romMethod,
				pc,
				newInfo.localmap,
				NULL,
				NULL,
				NULL);
	}

	/* Copy metadata */
	newInfo.modifiers = romMethod->modifiers;
	newInfo.argCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	newInfo.tempCount = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);

	/* Insert into cache */
	updateROMMethodInfoCache(vm, classLoader, &newInfo);

	/* Reflect into current walkState */
	walkState->romMethodInfo = newInfo;
}

void
getROMMethodInfoForROMMethod(J9StackWalkState *walkState, J9ROMMethod *romMethod)
{
	J9Method *method = walkState->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;

	void *key = (void *)romMethod;
	J9ROMMethodInfo tmp = {0};

	/* Check cache first */
	if (!checkROMMethodInfoCache(classLoader, key, &tmp)) {
		/* Cache miss or not populated */
		initializeBasicROMMethodInfo(walkState, romMethod);
		populateROMMethodInfo(walkState, romMethod, key, 0, false);
	} else {
		/* Cache hit */
		walkState->romMethodInfo = tmp;
	}
}

/* Bytecode PC: compute argbits + stackmap + localmap */
void
getROMMethodInfoForBytecodePC(J9StackWalkState *walkState, J9ROMMethod *romMethod, UDATA pc)
{
	if (pc <= J9SF_MAX_SPECIAL_FRAME_TYPE || pc >= (UDATA)J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod)) {
		return;
	}

	J9Method *method = walkState->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;

	void *key = (void *)( (uintptr_t)J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + (uintptr_t)pc );
	J9ROMMethodInfo tmp = {0};

	/* Check cache first */
	if (!checkROMMethodInfoCache(classLoader, key, &tmp)) {
		/* Cache miss or not populated */
		initializeBasicROMMethodInfo(walkState, romMethod);
		populateROMMethodInfo(walkState, romMethod, key, pc, true);
	} else {
		/* Cache hit */
		walkState->romMethodInfo = tmp;
	}
}


} /* extern "C" */
