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

#include "sharedconsts.h"

#if defined(J9SHR_CACHELET_SUPPORT)

/**
 * @file
 * @ingroup Shared_Common
 */

#include <string.h>
#include "j9cfg.h"
#include "j9port.h"
#include "pool_api.h"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "util_api.h"

#include "OSCachevmem.hpp"
#include "UnitTest.hpp"

SH_OSCachevmem::SH_OSCachevmem(J9PortLibrary* portLibrary, const char* cachedirname, const char* cacheName, J9SharedClassPreinitConfig* piconfig,
		IDATA numLocks, UDATA createFlag, UDATA verboseFlags, U_64 runtimeFlags, I_32 openMode, J9PortShcVersion* versionData, SH_OSCacheInitializer* initializer)
{
	initialize(portLibrary, NULL, OSCACHE_CURRENT_CACHE_GEN);
	startup(cachedirname, J9SH_DIRPERM_ABSENT, cacheName, piconfig, numLocks, createFlag, verboseFlags, runtimeFlags, openMode, 0, versionData, initializer, SHR_STARTUP_REASON_NORMAL);
}

void
SH_OSCachevmem::initialize(J9PortLibrary* portLibrary, char* memForConstructor, UDATA generation)
{	
	commonInit(portLibrary, generation);
	_fileHandle = -1;
	_writeLockCounter = 0;
	_headerStart = NULL;
	_dataStart = NULL;
	_dataLength = 0;
	for (UDATA i = 0; i < J9SH_OSCACHE_VMEM_LOCK_COUNT; i++) {
		_lockMutex[i] = NULL;
	}
	_corruptionCode = NO_CORRUPTION;
	_corruptValue = NO_CORRUPTION;
}

bool
SH_OSCachevmem::startup(const char* ctrlDirName, UDATA cacheDirPerm, const char* cacheName, J9SharedClassPreinitConfig* piconfig, IDATA numLocks,
		UDATA createFlag, UDATA verboseFlags, U_64 runtimeFlags, I_32 openMode, UDATA storageKeyTesting, J9PortShcVersion* versionData, SH_OSCacheInitializer* initializer, UDATA reason)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	U_32 readWriteBytes = (U_32)((piconfig->sharedClassReadWriteBytes > 0) ? piconfig->sharedClassReadWriteBytes : 0);
	struct J9FileStat statBuf;
	IDATA errorCode = J9SH_OSCACHE_FAILURE;
	bool initMutex = true;
	LastErrorInfo lastErrorInfo;

	Trc_SHR_OSC_Vmem_startup_Entry(cacheName, ctrlDirName, piconfig->sharedClassCacheSize, numLocks, createFlag, verboseFlags, openMode);
	
	/* pretend to be a persistent cache so the correct filename is created */
	versionData->cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
	if (commonStartup(ctrlDirName, cacheDirPerm, cacheName, piconfig, createFlag, verboseFlags, runtimeFlags, openMode, versionData) != 0) {
		Trc_SHR_OSC_Vmem_startup_commonStartupFailure();
		goto _errorPreFileOpen;
	}
	Trc_SHR_OSC_Vmem_startup_commonStartupSuccess();

	versionData->cacheType = J9PORT_SHR_CACHE_TYPE_VMEM;

	/* Detect remote filesystem */
	if (openMode & J9OSCACHE_OPEN_MODE_CHECK_NETWORK_CACHE) {
		if (0 == j9file_stat(_cacheDirName, 0, &statBuf)) {
			if (statBuf.isRemote) {
				Trc_SHR_OSC_Vmem_startup_detectedNetworkCache();
				errorHandler(J9NLS_SHRC_OSCACHE_VMEM_NETWORK_CACHE, NULL);
				goto _errorPreFileOpen;
			}
		}
	}

	/* if numLocks is greater than zero, this is the initial open, so try to read an existing cache */
	if (numLocks > 0 && j9file_attr(_cachePathName) == EsIsFile) {
		/* Open the file */
		if (!openCacheFile(false, &lastErrorInfo)) {
			Trc_SHR_OSC_Vmem_startup_badfileopen(_cachePathName);
			errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_FILEOPEN_ERROR, &lastErrorInfo);  /* TODO: ADD FILE NAME */
			goto _errorPostFileOpen;
		}
		Trc_SHR_OSC_Vmem_startup_goodfileopen(_cachePathName, _fileHandle);

		for (UDATA i = 0; i < J9SH_OSCACHE_VMEM_LOCK_COUNT; i++) {
			if (omrthread_monitor_init_with_name(&_lockMutex[i], 0, "Vmem shared classes lock mutex")) {
				Trc_SHR_OSC_Vmem_startup_failed_mutex_init(i);
				goto _errorPostFileOpen;
			}
		}
		initMutex = false;
		Trc_SHR_OSC_Vmem_startup_initialized_mutexes();
	
		/* Get cache header write lock */
		if (-1 == acquireHeaderWriteLock(_activeGeneration, &lastErrorInfo)) {
			Trc_SHR_OSC_Vmem_startup_badAcquireHeaderWriteLock();
			errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ACQUIREHEADERWRITELOCK_ERROR, &lastErrorInfo);
			errorCode = J9SH_OSCACHE_CORRUPT;
			goto _errorPostHeaderLock;
		}
		Trc_SHR_OSC_Vmem_startup_goodAcquireHeaderWriteLock();
		
		/* Try to acquire the attach write lock. This will only succeed if noone else
		 * has the attach read lock i.e. there is noone connected to the cache */
		if (0 == tryAcquireAttachWriteLock(_activeGeneration)) {
			Trc_SHR_OSC_Vmem_startup_cacheNotInUse();
		} else {
			Trc_SHR_OSC_Vmem_startup_cacheInUse();
			errorHandler(J9NLS_SHRC_OSCACHE_NESTED_IN_USE, NULL);
			errorCode = J9SH_OSCACHE_INUSE;
  			goto _errorPostAttachLock;
		}
	
		/* Check the length of the file */
#if defined(WIN32) || defined(WIN64)
		_cacheSize = (U_32)j9file_blockingasync_flength(_fileHandle);
#else
		_cacheSize = (U_32)j9file_flength(_fileHandle);
#endif
		if (_cacheSize <= 0) {
			closeCacheFile();
		}
	}

	if (_cacheSize > 0) {
		/* We are opening an existing cache */
		J9SRP* dataStartField;
		U_32* dataLengthField;
		void* localHeader;
	
		Trc_SHR_OSC_Vmem_startup_fileOpened();
		
		if (_cacheSize <= sizeof(OSCachemmap_header_version_current)) {
			Trc_SHR_OSC_Vmem_startup_cacheTooSmall();
			errorCode = J9SH_OSCACHE_CORRUPT;
			goto _errorPostAttachLock;
		}

		if (!(localHeader = j9mem_allocate_memory(_cacheSize, J9MEM_CATEGORY_CLASSES))) {
			Trc_SHR_OSC_Vmem_startup_allocation_failed(_cacheSize);
			errorCode = J9SH_OSCACHE_OUTOFMEMORY;
			goto _errorPostAttachLock;
		}
#if defined(WIN32) || defined(WIN64)
		if (j9file_blockingasync_read(_fileHandle, localHeader, _cacheSize) != (IDATA)_cacheSize) {
#else
		if (j9file_read(_fileHandle, localHeader, _cacheSize) != (IDATA)_cacheSize) {
#endif
			goto _errorPostAttachLock;
		}

		/* Release attach write lock */
		if (0 != releaseAttachWriteLock(_activeGeneration)) {
			Trc_SHR_OSC_Vmem_startup_badReleaseAttachReadLock();
			errorHandler(J9NLS_SHRC_OSCACHE_VMEM_STARTUP_RELEASEATTACHWRITELOCK_ERROR, NULL);
			goto _errorPostHeaderLock;
		}
		Trc_SHR_OSC_Vmem_startup_goodReleaseAttachReadLock();
		
		/* Release cache header write lock */
		if (0 != releaseHeaderWriteLock(_activeGeneration, &lastErrorInfo)) {
			Trc_SHR_OSC_Vmem_startup_badReleaseHeaderWriteLock();
			errorHandler(J9NLS_SHRC_OSCACHE_VMEM_STARTUP_RELEASEHEADERWRITELOCK_ERROR, &lastErrorInfo);
			goto _errorPostFileOpen;
		}
		Trc_SHR_OSC_Vmem_startup_goodReleaseHeaderWriteLock();

		if (!closeCacheFile()) {
			Trc_SHR_OSC_Vmem_startup_closefilefailed();
			goto _errorPreFileOpen;
		}

		/* Use a local variable until closeCacheFile() is called, as it will assert _headerStart is NULL */
		_headerStart = localHeader;

		/* Get the values from the header */
		dataLengthField = (U_32*)getMmapHeaderFieldAddressForGen(_headerStart, _activeGeneration, OSCACHE_HEADER_FIELD_DATA_LENGTH);
		if (NULL != dataLengthField) {
			_dataLength = *dataLengthField;
		}
		dataStartField = (J9SRP*)getMmapHeaderFieldAddressForGen(_headerStart, _activeGeneration, OSCACHE_HEADER_FIELD_DATA_START);
		if (NULL != dataStartField) {
			_dataStart = MMAP_DATASTARTFROMHEADER(dataStartField);
		}
		
		if (_runningReadOnly) {
			U_32* initCompleteAddr = (U_32*)getMmapHeaderFieldAddressForGen(_headerStart, _activeGeneration, OSCACHE_HEADER_FIELD_CACHE_INIT_COMPLETE);

			/* In readonly, we can't get a header lock, so if the cache is mid-init, fail */
			if (!*initCompleteAddr) {
				errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_READONLY_CACHE_NOTINITIALIZED, NULL);
				Trc_SHR_OSC_Vmem_startup_cacheNotInitialized();
	 			goto _errorPreFileOpen;
			}
		}
				
		if (_verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE) {
			if (_runningReadOnly) {
				OSC_TRACE1(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_OPENED_READONLY, _cacheName);
			} else {
				OSC_TRACE1(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_OPENED, _cacheName);
			}
		}

	} else {
		/* Create a new cache */

		if (initMutex) {
			for (UDATA i = 0; i < J9SH_OSCACHE_VMEM_LOCK_COUNT; i++) {
				if (omrthread_monitor_init_with_name(&_lockMutex[i], 0, "Vmem shared classes lock mutex")) {
					Trc_SHR_OSC_Vmem_startup_failed_mutex_init(i);
					return false;
				}
			}
			Trc_SHR_OSC_Vmem_startup_initialized_mutexes();
		}

		_dataLength = (U_32)piconfig->sharedClassCacheSize;
		if (!(_dataStart = j9mem_allocate_memory(_dataLength, J9MEM_CATEGORY_CLASSES))) {
			Trc_SHR_OSC_Vmem_startup_allocation_failed(_dataLength);
			errorCode = J9SH_OSCACHE_OUTOFMEMORY;
			goto _errorPreFileOpen;
		}
		
		Trc_SHR_OSC_Vmem_startup_callinginit2(_dataStart,
												_dataLength, 
												piconfig->sharedClassMinAOTSize, 
												piconfig->sharedClassMaxAOTSize,
												piconfig->sharedClassMinJITSize,
												piconfig->sharedClassMaxJITSize,
												readWriteBytes);
		initializer->init((char*)_dataStart, 
									_dataLength, 
									(I_32)piconfig->sharedClassMinAOTSize, 
									(I_32)piconfig->sharedClassMaxAOTSize,
									(I_32)piconfig->sharedClassMinJITSize,
									(I_32)piconfig->sharedClassMaxJITSize,
									readWriteBytes);
		Trc_SHR_OSC_Vmem_startup_initialized();
	}

	_startupCompleted = true;
	Trc_SHR_OSC_Vmem_startup_Exit();
	return true;
	
_errorPostAttachLock :
	releaseAttachWriteLock(_activeGeneration);
_errorPostHeaderLock :
	releaseHeaderWriteLock(_activeGeneration, NULL);
_errorPostFileOpen :
	closeCacheFile();
_errorPreFileOpen :
	setError(errorCode);
	return false;
}

IDATA
SH_OSCachevmem::destroy(bool suppressVerbose, bool isReset)
{
	cleanup();
	return 0;
}

IDATA
SH_OSCachevmem::setRegionPermissions(J9PortLibrary* portLibrary, void *address, UDATA length, UDATA flags)
{
	return 0;
}

UDATA
SH_OSCachevmem::getPermissionsRegionGranularity(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	/* This call to capabilities is arguably unnecessary, but it is a good check to do */
	if (j9mmap_capabilities() & J9PORT_MMAP_CAPABILITY_PROTECT) {
		return j9mmap_get_region_granularity((void*)_headerStart);
	}
	return 0;
}


void
SH_OSCachevmem::cleanup()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	releaseAttachWriteLock(_activeGeneration);
	releaseHeaderWriteLock(_activeGeneration, NULL);

	/* The memory is allocated into _headerStart when reading an existing cache,
	 * or _dataStart when creating a new cache. See startup().
	 */
	if (_headerStart != NULL) {
		j9mem_free_memory(_headerStart);
	} else if (_dataStart != NULL) {
		j9mem_free_memory(_dataStart);
	}
	_headerStart = NULL;
	_dataStart = NULL;

	/* Call closeCacheFile() after freeing memory, as it assert that
	 * _headerStart and _dataStart are NULL.
	 */
	closeCacheFile();

	for (UDATA i = 0; i < J9SH_OSCACHE_VMEM_LOCK_COUNT; i++) {
		if (_lockMutex[i]) {
			omrthread_monitor_destroy(_lockMutex[i]);
		}
	}

	commonCleanup();
}

IDATA
SH_OSCachevmem::getNewWriteLockID()
{
	if (_writeLockCounter < J9SH_OSCACHE_VMEM_LOCK_COUNT) {
		return _writeLockCounter++;
	} else {
		return -1;
	}
}

IDATA
SH_OSCachevmem::getWriteLockID() {
	return this->getNewWriteLockID();
}

IDATA
SH_OSCachevmem::getReadWriteLockID() {
	return this->getNewWriteLockID();
}

IDATA
SH_OSCachevmem::acquireWriteLock(UDATA lockID)
{
	if (lockID >= J9SH_OSCACHE_VMEM_LOCK_COUNT) {
		return -1;
	}
	if (omrthread_monitor_enter(_lockMutex[lockID]) != 0) {
		return -1;
	}

	return 0;
}

IDATA
SH_OSCachevmem::releaseWriteLock(UDATA lockID)
{
	if (lockID >= J9SH_OSCACHE_VMEM_LOCK_COUNT) {
		return -1;
	}
	if (omrthread_monitor_exit(_lockMutex[lockID]) != 0) {
		return -1;
 	}

	return 0;
}

void *
SH_OSCachevmem::attach(J9VMThread *currentThread, J9PortShcVersion* expectedVersionData)
{
	J9JavaVM *vm = currentThread->javaVM;

	Trc_SHR_OSC_Vmem_attach_Entry1(UnitTest::unitTest);
	
	if (_headerStart != NULL) {
		/* Verify the header */
		OSCachemmap_header_version_current *cacheHeader = (OSCachemmap_header_version_current *)_headerStart;
		IDATA headerRc;
		U_32 orgType = expectedVersionData->cacheType;

		/* look like a persistent cache for the header check */
		expectedVersionData->cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
		if ((headerRc = isCacheHeaderValid(cacheHeader, expectedVersionData)) != J9SH_OSCACHE_HEADER_OK) {
			if (headerRc == J9SH_OSCACHE_HEADER_CORRUPT) {
				Trc_SHR_OSC_Vmem_attach_corruptCacheHeader();
				/* cache is corrupt, trigger hook to generate a system dump */
				if (0 == (_runtimeFlags & J9SHR_RUNTIMEFLAG_DISABLE_CORRUPT_CACHE_DUMPS)) {
					TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
				}
				setError(J9SH_OSCACHE_CORRUPT);
			} else if (headerRc == J9SH_OSCACHE_HEADER_DIFF_BUILDID) {
				Trc_SHR_OSC_Vmem_attach_differentBuildID();
				setError(J9SH_OSCACHE_DIFF_BUILDID);
			} else {
				errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_INVALID_CACHE_HEADER, NULL);
				Trc_SHR_OSC_Vmem_attach_invalidCacheHeader();
				setError(J9SH_OSCACHE_FAILURE);
			}
			expectedVersionData->cacheType = orgType;
			destroy(true);
			return NULL;
		}
		Trc_SHR_OSC_Vmem_attach_validCacheHeader();
	}

	return _dataStart;
} 

void
SH_OSCachevmem::runExitCode()
{
}

U_32
SH_OSCachevmem::getTotalSize()
{
	return _dataLength;
}

UDATA 
SH_OSCachevmem::getJavacoreData(J9JavaVM *vm, J9SharedClassJavacoreDataDescriptor* descriptor)
{
	descriptor->cacheGen = _activeGeneration;
	descriptor->shmid = descriptor->semid = -2;
	descriptor->cacheDir = _cachePathName;

	return 1;
}

void
SH_OSCachevmem::errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	if (moduleName && id && _verboseFlags) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, moduleName, id);
		if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->lastErrorCode)) {
			I_32 errorno = lastErrorInfo->lastErrorCode;
			const char* errormsg = lastErrorInfo->lastErrorMsg;
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, errormsg);
		}
	}
}

IDATA
SH_OSCachevmem::getLockCapabilities(void)
{
	return J9OSCACHE_DATA_WRITE_LOCK;
}

void *
SH_OSCachevmem::getAttachedMemory()
{
	/* This method should only be called between calls to attach and detach
	 */
	return _dataStart;
}

#endif /* J9SHR_CACHELET_SUPPORT */
