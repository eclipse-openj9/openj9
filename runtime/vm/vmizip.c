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
#include "vmi.h"
#include "j9.h"
#include "zip_api.h"
/* get the J9ZipCachePool definition so the hookInterface can be used */
#include "../zip/zip_internal.h"
#include "ut_j9vm.h"



#if defined(J9VM_THR_PREEMPTIVE) && defined(J9VM_OPT_SHARED_CLASSES)
#include "omrthread.h"
#define ENTER() omrthread_monitor_enter(omrthread_global_monitor())
#define EXIT() omrthread_monitor_exit(omrthread_global_monitor())
#else
#define ENTER()
#define EXIT()
#endif

/* want to make sure we use the J9 Port library and not the Harmony one */
#undef USING_VMI

I_32 
vmizip_getZipEntryData(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize) 
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_getZipEntryData(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)entry, buffer, bufferSize);
}

I_32 
vmizip_getZipEntryRawData(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize, U_32 offset) 
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_getZipEntryRawData(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)entry, buffer, bufferSize, offset);
}

I_32
vmizip_getZipEntryFromOffset(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, IDATA offset, I_32 flags) 
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_getZipEntryFromOffset(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)entry, offset, (BOOLEAN)((flags & (I_32)ZIP_FLAG_READ_DATA_POINTER) != 0));
}

void
vmizip_resetZipFile(VMInterface * vmi, VMIZipFile * zipFile, IDATA * nextEntryPointer)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	zip_resetZipFile(PORTLIB, (J9ZipFile *)zipFile, nextEntryPointer);
}

I_32 
vmizip_getNextZipEntry(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * zipEntry, IDATA * nextEntryPointer, I_32 flags)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_getNextZipEntry(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)zipEntry, nextEntryPointer, (BOOLEAN)((flags & (I_32)ZIP_FLAG_READ_DATA_POINTER) != 0));
}

I_32 
vmizip_getZipEntry(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, const char *filename, I_32 flags)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	U_32 zipEntryFlags = J9ZIP_OPEN_NO_FLAGS;
	if (J9_ARE_ANY_BITS_SET(flags, ZIP_FLAG_FIND_DIRECTORY)) {
		zipEntryFlags |= J9ZIP_GETENTRY_FIND_DIRECTORY;
	}
	if (J9_ARE_ANY_BITS_SET(flags, ZIP_FLAG_READ_DATA_POINTER)) {
		zipEntryFlags |= J9ZIP_GETENTRY_READ_DATA_POINTER;
	}
	return zip_getZipEntry(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)entry, filename, strlen(filename), zipEntryFlags);
}

I_32
vmizip_getZipEntryWithSize(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, const char *filename, IDATA filenameLength, I_32 flags)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	I_32 zipEntryFlags = J9ZIP_GETENTRY_NO_FLAGS;
	if (J9_ARE_ANY_BITS_SET(flags, ZIP_FLAG_FIND_DIRECTORY)) {
		zipEntryFlags |= J9ZIP_GETENTRY_FIND_DIRECTORY;
	}
	if (J9_ARE_ANY_BITS_SET(flags, ZIP_FLAG_READ_DATA_POINTER)) {
		zipEntryFlags |= J9ZIP_GETENTRY_READ_DATA_POINTER;
	}
	return zip_getZipEntry(PORTLIB, (J9ZipFile *) zipFile, (J9ZipEntry *) entry, filename, filenameLength, zipEntryFlags);
}

I_32 
vmizip_getZipEntryExtraField(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_getZipEntryExtraField(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)entry, buffer, bufferSize);
}

void
vmizip_initZipEntry(VMInterface * vmi, VMIZipEntry * entry)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	zip_initZipEntry(PORTLIB, (J9ZipEntry *)entry);
}

I_32
vmizip_openZipFile(VMInterface * vmi, char *filename, VMIZipFile * vmizipFile, I_32 flags)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	J9JavaVM *vm = j9vmi->javaVM;
	J9ZipFile *zipFile = (J9ZipFile *)vmizipFile;
	J9ZipCachePool *zipCachePool = j9vmi->javaVM->zipCachePool;
	I_32 result = 0;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
#if defined(J9VM_OPT_SHARED_CLASSES)
	JNIEnv *env;
	J9SharedClassConfig* sharedConfig;
#endif

#if defined(J9VM_OPT_SHARED_CLASSES)
	ENTER();
	if (flags & ZIP_FLAG_OPEN_CACHE) {
		BOOLEAN readCacheData = FALSE;
		if (!vm->zipCachePool) {
			/* This is a synchronization hole with dynload.c and any other 
			 * threads that call this function */
			zipCachePool = zipCachePool_new(PORTLIB, vm);
			vm->zipCachePool = zipCachePool;
		}
		/* open the zip file but do not call zip_readCacheData().
		 * we need to search data in shared class cache before reading it from disk.
		 */
		result = zip_openZipFile(PORTLIB, filename, zipFile, vm->zipCachePool, J9ZIP_OPEN_NO_FLAGS);
		if (result) {
			if (zipCachePool) {
				TRIGGER_J9HOOK_VM_ZIP_LOAD(zipCachePool->hookInterface, PORTLIB, zipCachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_OPEN, (U_8*)filename, result);
			}
		} else {
			readCacheData = !result;
		}
		sharedConfig = vm->sharedClassConfig;
		/* bootstrap jars are controlled by the J9VM_ZERO_SHAREBOOTZIPCACHE flag */
		if ((0 == result)
			&& (NULL != sharedConfig)
			&& ((flags & ZIP_FLAG_BOOTSTRAP) && (vm->zeroOptions & J9VM_ZERO_SHAREBOOTZIPCACHE))
			&& !zipCache_isCopied(zipFile->cache)
		) {
			result = (*((JavaVM *)vm))->GetEnv((JavaVM *)vm, (void **) &env, JNI_VERSION_1_2);
			if (result == JNI_OK) {
				J9VMThread *vmThread = (J9VMThread *) env;
				J9SharedDataDescriptor descriptor;
				IDATA numElem;
				void *cacheData;
				const char *uniqueId = zipCache_uniqueId(zipFile->cache);
				if (uniqueId) {
					UDATA idLength = strlen(uniqueId);
					numElem = sharedConfig->findSharedData(vmThread, uniqueId, idLength, J9SHR_DATA_TYPE_ZIPCACHE, TRUE, &descriptor, NULL);
					if (numElem == 0) {
						/* None found - add to the cache */
						result = zip_readCacheData(PORTLIB, zipFile);
						readCacheData = FALSE;
						if (!result) {
							memset(&descriptor, 0, sizeof(descriptor));
							descriptor.length = zipCache_cacheSize(zipFile->cache);
							descriptor.address = j9mem_allocate_memory(descriptor.length, OMRMEM_CATEGORY_VM);
							if (descriptor.address) {
								if (zipCache_copy(zipFile->cache, descriptor.address, descriptor.length)) {
									descriptor.type = J9SHR_DATA_TYPE_ZIPCACHE;
									descriptor.flags = J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE;
									cacheData = (J9ZipCache *)sharedConfig->storeSharedData(vmThread, uniqueId, idLength, &descriptor);
									if (cacheData) {
										Trc_VM_createdSharedZipCache(env, uniqueId, zipFile->filename);
										zipCache_useCopiedCache(zipFile->cache, cacheData);
									} else {
										Trc_VM_sharedZipCacheStoreSharedDataFailed(env, uniqueId, zipFile->filename);
									}
								} else {
									Trc_VM_sharedZipCacheCopyFailure(env, zipFile->filename);
								}
								j9mem_free_memory(descriptor.address);
							} else {
								Trc_VM_sharedZipCacheAllocFailure(env, descriptor.length, zipFile->filename);
							}
						}
					} else {
						if (numElem == 1) {
							/* Found the zip cache data, use it */
							cacheData = (J9ZipCache *)descriptor.address;
							if (NULL != cacheData) {
								Trc_VM_useSharedZipCache(env, uniqueId, zipFile->filename);
								zipCache_useCopiedCache(zipFile->cache, cacheData);
								readCacheData = FALSE;
								if (zipCachePool) {
									TRIGGER_J9HOOK_VM_ZIP_LOAD(zipCachePool->hookInterface, PORTLIB, zipCachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_OPEN, (U_8*)filename, 0);
								}
							}
						}
						if ((numElem == 1 && !descriptor.address) || numElem != 1) {
							Trc_VM_sharedZipCacheFindSharedDataFailed(env, numElem, uniqueId, zipFile->filename);
						}
					}
					j9mem_free_memory((void *)uniqueId);
				} else {
					/* Failure to allocate the unique id, the number of bytes is unknown */
					Trc_VM_sharedZipCacheAllocFailure(env, -5, zipFile->filename);
				}
			} else {
				Trc_VM_sharedZipCacheNoEnvFailure(result, zipFile->filename);
			}
		}
		if (readCacheData) {
			result = zip_readCacheData(PORTLIB, zipFile);
		}
	}
	else {
		result = zip_openZipFile(PORTLIB, filename, zipFile, NULL, J9ZIP_OPEN_NO_FLAGS);
	}
	EXIT();
	return result;
#else
	if (flags & ZIP_FLAG_OPEN_CACHE) {
		J9ZipCache *cache = NULL;
		if (NULL == vm->zipCachePool) {
			/* This is a synchronization hole with dynload.c and any other 
			 * threads that call this function */
			zipCachePool = zipCachePool_new(PORTLIB, vm);
			vm->zipCachePool = zipCachePool;
		}
		result = zip_openZipFile(PORTLIB, filename, zipFile, vm->zipCachePool, J9ZIP_OPEN_READ_CACHE_DATA);
	} else {
		result = zip_openZipFile(PORTLIB, filename, zipFile, NULL, J9ZIP_OPEN_NO_FLAGS);
	}
	return result;

#endif
}

void
vmizip_freeZipEntry(VMInterface * vmi, VMIZipEntry * entry)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	zip_freeZipEntry(PORTLIB, (J9ZipEntry *)entry);
}

I_32
vmizip_closeZipFile(VMInterface * vmi, VMIZipFile * zipFile)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_releaseZipFile(PORTLIB, (J9ZipFile *)zipFile);
}

I_32
vmizip_getZipEntryComment(VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize) 
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM (j9vmi->javaVM);
	return zip_getZipEntryComment(PORTLIB, (J9ZipFile *)zipFile, (J9ZipEntry *)entry, buffer, bufferSize);
}


I_32
vmizip_getZipComment(VMInterface * vmi, VMIZipFile * zipFile, U_8 ** comment, UDATA * commentSize)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	return zip_getZipComment(PORTLIB, (J9ZipFile *)zipFile, comment, commentSize);
}

void
vmizip_freeZipComment(VMInterface * vmi, U_8 * commentString)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	PORT_ACCESS_FROM_JAVAVM(j9vmi->javaVM);
	zip_freeZipComment(PORTLIB, commentString);
}


IDATA
vmizipCache_enumGetDirName(void *handle, char *nameBuf, UDATA nameBufSize)
{
	return zipCache_enumGetDirName(handle, nameBuf, nameBufSize);
}

IDATA
vmizipCache_enumNew(void * zipCache, char *directoryName, void **handle)
{
	return zipCache_enumNew((J9ZipCache *)zipCache, directoryName, handle);
}
	
IDATA 
vmizipCache_enumElement(void *handle, char *nameBuf, UDATA nameBufSize, UDATA * offset)
{
	return zipCache_enumElement(handle, nameBuf, nameBufSize, offset);
}

void
vmizipCache_enumKill(void *handle)
{
	zipCache_enumKill(handle);
}

J9HookInterface**
vmizip_getZipHookInterface(VMInterface * vmi)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	J9JavaVM *vm = (J9JavaVM *)j9vmi->javaVM;
	return zip_getVMZipCachePoolHookInterface(vm->zipCachePool);
}


struct VMIZipFunctionTable VMIZipLibraryTable = {
	vmizip_closeZipFile,
	vmizip_freeZipEntry,
	vmizip_getNextZipEntry,
	vmizip_getZipEntry,
	vmizip_getZipEntryComment,
	vmizip_getZipEntryData,
	vmizip_getZipEntryExtraField,
	vmizip_getZipEntryFromOffset,
	vmizip_getZipEntryRawData,
	vmizip_initZipEntry,
	vmizip_openZipFile,
	vmizip_resetZipFile,
	vmizipCache_enumElement,
	vmizipCache_enumGetDirName,
	vmizipCache_enumKill,
	vmizipCache_enumNew,
	vmizip_getZipComment,
	vmizip_freeZipComment,
	vmizip_getZipEntryWithSize,
	/* J9 specific, keep these at the end */
	vmizip_getZipHookInterface,
    NULL
};
