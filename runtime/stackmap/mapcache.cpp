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
				0,
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
getROMMethodInfoCommon(J9StackWalkState *walkState, J9ClassLoader *classLoader, J9ROMMethod *romMethod)
{
	J9ROMMethodInfo *romMethodInfo = &walkState->romMethodInfo;
	J9JavaVM *vm = walkState->javaVM;
	J9Method *method = walkState->method;
	J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;
	if (romMethodInfo->argCount <= (J9_ARGBITS_CACHE_SLOTS * 32)) {
		j9localmap_ArgBitsForPC0(
				romClass,
				romMethod,
				romMethodInfo->argbits);
			romMethodInfo->flags |= J9MAPCACHE_ARGBITS_CACHED;
	}

	updateROMMethodInfoCache(vm, classLoader, romMethodInfo);
}

static void
getROMMethodInfoForBytecodePCInternal(J9StackWalkState *walkState, J9ClassLoader *classLoader, UDATA pcOffset, void *bytecodePC, UDATA numberOfLocals, UDATA pendingCount, J9ROMMethod *romMethod)
{
	J9ROMMethodInfo *romMethodInfo = &walkState->romMethodInfo;
	J9JavaVM *vm = walkState->javaVM;
	J9Method *method = walkState->method;
	J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;

	//memset(romMethodInfo, 0, sizeof(*romMethodInfo));
	initializeBasicROMMethodInfo(walkState, romMethod);
	romMethodInfo->key = bytecodePC;

	if (numberOfLocals <= (J9_LOCALMAP_CACHE_SLOTS * 32)) {
		IDATA rc = vm->localMapFunction(
				vm->portLibrary,
				romClass,
				romMethod,
				pcOffset,
				romMethodInfo->localmap,
				NULL,
				NULL,
				NULL);
		if (0 == rc) {
			romMethodInfo->flags |= J9MAPCACHE_LOCALMAP_CACHED;
		}
	}

	if ((pendingCount != 0) && (pendingCount <= (J9_STACKMAP_CACHE_SLOTS * 32))) {
		IDATA rc = j9stackmap_StackBitsForPC(
				vm->portLibrary,
				pcOffset,
				romClass,
				romMethod,
				romMethodInfo->stackmap,
				pendingCount,
				NULL,
				NULL,
				NULL);
		if (0 == rc) {
			romMethodInfo->flags |= J9MAPCACHE_STACKMAP_CACHED;
		}
	}

	getROMMethodInfoCommon(walkState, classLoader, romMethod);
}

void
getROMMethodInfoForROMMethod(J9StackWalkState *walkState, J9ROMMethod *romMethod)
{
	J9ROMMethodInfo *romMethodInfo = &walkState->romMethodInfo;
	J9JavaVM *vm = walkState->javaVM;
	J9Method *method = walkState->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
	void *key = (void *)romMethod;

	if (checkROMMethodInfoCache(classLoader, key, romMethodInfo)) {
		return;
	}

	memset(romMethodInfo, 0, sizeof(*romMethodInfo));
	romMethodInfo->key = key;
	initializeBasicROMMethodInfo(walkState, romMethod);

	getROMMethodInfoCommon(walkState, classLoader, romMethod);
}

void
getROMMethodInfoForBytecodeFrame(J9StackWalkState *walkState)
{
	//if (NULL == walkState->method) {
	//	initializeBasicROMMethodInfo(walkState, NULL);
	//	walkState->romMethodInfo.key = NULL;
	//	return;
	//}

	J9ROMMethodInfo *romMethodInfo = &walkState->romMethodInfo;
	J9Method *method = walkState->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

	UDATA pcOffset = (UDATA)(walkState->pc - J9_BYTECODE_START_FROM_RAM_METHOD(method));

	if ((pcOffset <= J9SF_MAX_SPECIAL_FRAME_TYPE) || (pcOffset >= (UDATA)J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod))) {
		initializeBasicROMMethodInfo(walkState, romMethod);
		romMethodInfo->key = (void *)romMethod;
		return;
	}

	void *bytecodePC = (void *)(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + pcOffset);

	if (checkROMMethodInfoCache(classLoader, bytecodePC, romMethodInfo)) {
		return;
	}

	UDATA argCount = romMethod->argCount;
	UDATA tempCount = romMethod->tempCount;
	UDATA numberOfLocals = argCount + tempCount;
	UDATA localsSize = numberOfLocals;

	if (romMethod->modifiers & J9AccSynchronized) {
		localsSize += 1;
	} else if (((romMethod->modifiers) & (J9AccMethodObjectConstructor | J9AccEmptyMethod)) == J9AccMethodObjectConstructor) {
		localsSize += 1;
	}

	UDATA *bp = walkState->arg0EA - localsSize;
	UDATA *unwindSP;

	if (bp == walkState->j2iFrame) {
		unwindSP = (UDATA *)((U_8 *)bp - sizeof(J9SFJ2IFrame) + sizeof(UDATA));
	} else {
		unwindSP = (UDATA *)((U_8 *)bp - sizeof(J9SFStackFrame) + sizeof(UDATA));
	}

	UDATA pendingCount = (UDATA)(unwindSP - walkState->walkSP);

	getROMMethodInfoForBytecodePCInternal(
		walkState,
		classLoader,
		pcOffset,
		bytecodePC,
		numberOfLocals,
		pendingCount,
		romMethod);
}

void
getROMMethodInfoForOSRFrame(J9StackWalkState *walkState, J9OSRFrame *osrFrame)
{
	J9ROMMethodInfo *romMethodInfo = &walkState->romMethodInfo;

	J9Method *method = osrFrame->method;
	J9ClassLoader *classLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

	void *bytecodePC = (void *)(osrFrame->bytecodePCOffset + J9_BYTECODE_START_FROM_RAM_METHOD(method));

	if (checkROMMethodInfoCache(classLoader, bytecodePC, romMethodInfo)) {
		return;
	}

	UDATA numberOfLocals = osrFrame->numberOfLocals;
	if ((romMethod->modifiers & J9AccSynchronized) || (((romMethod->modifiers) & (J9AccMethodObjectConstructor | J9AccEmptyMethod))== J9AccMethodObjectConstructor)) {
		numberOfLocals -= 1;
	}
	getROMMethodInfoForBytecodePCInternal(
		walkState,
		classLoader,
		osrFrame->bytecodePCOffset,
		bytecodePC,
		numberOfLocals,
		osrFrame->pendingStackHeight,
		romMethod);
}

} /* extern "C" */
