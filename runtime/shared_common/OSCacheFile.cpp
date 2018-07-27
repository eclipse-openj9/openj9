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

#include "OSCacheFile.hpp"


/**
 * Method to acquire the write lock on the cache header region
 * 
 * Needs to be able to work with all generations
 * 
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 * 
 * @return 0 on success, -1 on failure
 */
IDATA
SH_OSCacheFile::acquireHeaderWriteLock(UDATA generation, LastErrorInfo *lastErrorInfo)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	I_32 lockFlags = J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_WAIT_FOR_LOCK;
	U_64 lockOffset, lockLength;
	I_32 rc = 0;

	Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_Entry();

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

	if (_runningReadOnly) {
		Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_ExitReadOnly();
		return 0;
	}
	
	lockOffset = (U_64)getMmapHeaderFieldOffsetForGen(generation, OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK);
	lockLength = sizeof(((OSCachemmap_header_version_current *)NULL)->headerLock);

	Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_gettingLock(_fileHandle, lockFlags, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
	rc = j9file_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#else
	rc = j9file_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#endif
	
	if (-1 == rc) {
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = j9error_last_error_number();
			lastErrorInfo->lastErrorMsg = j9error_last_error_message();
		}
		Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_badLock();
	} else {
		Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_goodLock();
	}
	
	Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_Exit(rc);
	return (IDATA)rc;
}

/**
 * Method to release the write lock on the cache header region
 * 
 * Needs to be able to work with all generations
 * 
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 * 
 * @return 0 on success, -1 on failure
 */
IDATA
SH_OSCacheFile::releaseHeaderWriteLock(UDATA generation, LastErrorInfo *lastErrorInfo)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	U_64 lockOffset, lockLength;
	I_32 rc = 0;

	Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_Entry();

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

	if (_runningReadOnly) {
		Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_ExitReadOnly();
		return 0;
	}

	lockOffset = (U_64)getMmapHeaderFieldOffsetForGen(generation, OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK);
	lockLength = sizeof(((OSCachemmap_header_version_current *)NULL)->headerLock);

	Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_gettingLock(_fileHandle, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
	rc = j9file_blockingasync_unlock_bytes(_fileHandle, lockOffset, lockLength);
#else
	rc = j9file_unlock_bytes(_fileHandle, lockOffset, lockLength);
#endif	
	if (-1 == rc) {
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = j9error_last_error_number();
			lastErrorInfo->lastErrorMsg = j9error_last_error_message();
		}
		Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_badLock();
	} else {
		Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_goodLock();
	}
	
	Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_Exit(rc);
	return (IDATA)rc;
}

/**
 * Method to try to acquire the write lock on the cache attach region
 * 
 * Needs to be able to work with all generations
 * 
 * This method does not wait for the lock to become available, but
 * immediately returns whether the lock has been obtained to the caller
 * 
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 * 
 * @return 0 on success, -1 on failure
 */
IDATA
SH_OSCacheFile::tryAcquireAttachWriteLock(UDATA generation)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	I_32 lockFlags = J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_NOWAIT_FOR_LOCK;
	U_64 lockOffset, lockLength;
	I_32 rc = 0;

	Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_Entry();
	
	lockOffset = (U_64)getMmapHeaderFieldOffsetForGen(generation, OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK);
	lockLength = sizeof(((OSCachemmap_header_version_current *)NULL)->attachLock);
	
	Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_gettingLock(_fileHandle, lockFlags, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
	rc = j9file_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#else
	rc = j9file_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#endif	
	if (-1 == rc) {
		Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_badLock();
	} else {
		Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_goodLock();
	}
	
	Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_Exit(rc);
	return rc;
}

/**
 * Method to release the write lock on the cache attach region
 * 
 * Needs to be able to work with all generations
 * 
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 * 
 * @return 0 on success, -1 on failure
 */
IDATA
SH_OSCacheFile::releaseAttachWriteLock(UDATA generation)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	U_64 lockOffset, lockLength;
	I_32 rc = 0;
	
	Trc_SHR_OSC_Mmap_releaseAttachWriteLock_Entry();
	
	lockOffset = (U_64)getMmapHeaderFieldOffsetForGen(generation, OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK);
	lockLength = sizeof(((OSCachemmap_header_version_current *)NULL)->attachLock);
	
	Trc_SHR_OSC_Mmap_releaseAttachWriteLock_gettingLock(_fileHandle, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
	rc = j9file_blockingasync_unlock_bytes(_fileHandle, lockOffset, lockLength);
#else
	rc = j9file_unlock_bytes(_fileHandle, lockOffset, lockLength);
#endif	
	if (-1 == rc) {
		Trc_SHR_OSC_Mmap_releaseAttachWriteLock_badLock();
	} else {
		Trc_SHR_OSC_Mmap_releaseAttachWriteLock_goodLock();
	}
	
	Trc_SHR_OSC_Mmap_releaseAttachWriteLock_Exit(rc);
	return rc;
}

/**
 * Method to determine whether a persistent cache's header is valid
 * 
 * @param [in] headerArg  A pointer to the cache header
 * @param [in] versionData  The expected cache version
 * 
 * @return true if header is valid, false if the header is invalid or doesn't match the versionData
 * THREADING: Pre-req caller holds the cache header write lock
 */
IDATA
SH_OSCacheFile::isCacheHeaderValid(OSCachemmap_header_version_current *header, J9PortShcVersion* versionData)
{
	Trc_SHR_OSC_Mmap_isCacheHeaderValid_Entry(header);
	IDATA headerRc;
	U_32 headerLen = MMAP_CACHEHEADERSIZE;
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	if (strncmp(header->eyecatcher, J9SH_OSCACHE_MMAP_EYECATCHER, J9SH_OSCACHE_MMAP_EYECATCHER_LENGTH)) {
		Trc_SHR_OSC_Mmap_isCacheHeaderValid_eyecatcherFailed(header->eyecatcher, J9SH_OSCACHE_MMAP_EYECATCHER);
		errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ISCACHEHEADERVALID_EYECATCHER, NULL); /* TODO: Add values */
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_HEADER_BAD_EYECATCHER, header->eyecatcher);
		setCorruptionContext(CACHE_HEADER_BAD_EYECATCHER, (UDATA)(header->eyecatcher));
		return J9SH_OSCACHE_HEADER_CORRUPT;
	}

	if (header->oscHdr.size != _cacheSize) {
		Trc_SHR_OSC_Mmap_isCacheHeaderValid_wrongSize(header->oscHdr.size, _cacheSize);
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_HEADER_INCORRECT_CACHE_SIZE, header->oscHdr.size);
		setCorruptionContext(CACHE_HEADER_INCORRECT_CACHE_SIZE, (UDATA)header->oscHdr.size);
		return J9SH_OSCACHE_HEADER_CORRUPT;
	}
	
	if ((headerRc = checkOSCacheHeader(&(header->oscHdr), versionData, headerLen)) != J9SH_OSCACHE_HEADER_OK) {
		Trc_SHR_OSC_Mmap_isCacheHeaderValid_OSCacheHeaderBad(headerRc);
		return headerRc; 
	}
	
	Trc_SHR_OSC_Mmap_isCacheHeaderValid_Exit();
	return J9SH_OSCACHE_HEADER_OK;
}

/**
 * Open the cache file
 * 
 * @param [in] doCreateFile  If true, attempts to create the cache if it does not exist
 * 
 * @return true on success, false on failure
 */
bool
SH_OSCacheFile::openCacheFile(bool doCreateFile, LastErrorInfo *lastErrorInfo)
{
	bool result = true;
	PORT_ACCESS_FROM_PORT(_portLibrary);
	I_32 openFlags = (_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY) ? EsOpenRead : (EsOpenRead | EsOpenWrite);
	I_32 fileMode = getFileMode();
	IDATA i;
	
	Trc_SHR_OSC_Mmap_openCacheFile_entry();

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

	if (doCreateFile && (openFlags & EsOpenWrite)) {
		/* Only add EsOpenCreate if we have write access */
		openFlags |= EsOpenCreate;
	}
	for (i=0; i<2; i++) {
#if defined(WIN32) || defined(WIN64)
		_fileHandle = j9file_blockingasync_open(_cachePathName, openFlags, fileMode);
#else
		_fileHandle = j9file_open(_cachePathName, openFlags, fileMode);
#endif
		if ((_fileHandle == -1) && (openFlags != EsOpenRead) && (_openMode & J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL)) {
			openFlags &= ~EsOpenWrite;
			continue;
		}
		break;
	}
	
	if (-1 == _fileHandle) {
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = j9error_last_error_number();
			lastErrorInfo->lastErrorMsg = j9error_last_error_message();
		}
		Trc_SHR_OSC_Mmap_openCacheFile_failed();
		result = false;
	} else if ((openFlags & (EsOpenRead | EsOpenWrite)) == EsOpenRead) {
		Trc_SHR_OSC_Event_OpenReadOnly();
		_runningReadOnly = true;
	}

	Trc_SHR_OSC_Mmap_openCacheFile_exit();
	return result;
}

/**
 * Close the cache file
 * 
 * @return true on success, false on failure
 * @Pre-req is that internalDetach() has been called beforehand
 */
bool
SH_OSCacheFile::closeCacheFile()
{
	bool result = true;
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	Trc_SHR_Assert_Equals(_headerStart, NULL);
	Trc_SHR_Assert_Equals(_dataStart, NULL);
	
	if (-1 == _fileHandle) {
		return true;
	}

	Trc_SHR_OSC_Mmap_closeCacheFile_entry();
#if defined(WIN32) || defined(WIN64)	
	if(-1 == j9file_blockingasync_close(_fileHandle)) {
#else
	if(-1 == j9file_close(_fileHandle)) {
#endif	
		Trc_SHR_OSC_Mmap_closeCacheFile_failed();
		result = false;
	}
	_fileHandle = -1;
	_startupCompleted = false;

	Trc_SHR_OSC_Mmap_closeCacheFile_exit();
	return result;
}

/**
 * Method to return the file mode required for a new cache.
 * 
 * @return  The file mode required for the cache
 */
I_32
SH_OSCacheFile::getFileMode()
{
	I_32 perm = 0;
	
	Trc_SHR_OSC_Mmap_getFileMode_Entry();
	
	if (_isUserSpecifiedCacheDir) {
		if (_openMode & J9OSCACHE_OPEN_MODE_GROUPACCESS) {
			perm = J9SH_CACHE_FILE_MODE_USERDIR_WITH_GROUPACCESS;
		} else {
			perm = J9SH_CACHE_FILE_MODE_USERDIR_WITHOUT_GROUPACCESS;
		}
	} else {
		if (_openMode & J9OSCACHE_OPEN_MODE_GROUPACCESS) {
			perm = J9SH_CACHE_FILE_MODE_DEFAULTDIR_WITH_GROUPACCESS;
		} else {
			perm = J9SH_CACHE_FILE_MODE_DEFAULTDIR_WITHOUT_GROUPACCESS;
		}
	}
	
	Trc_SHR_OSC_Mmap_getFileMode_Exit(_openMode, perm);
	return perm;
}

/**
 * Method to issue an NLS error message hen an error situation has occurred.  It
 * can optional issue messages detailing the last error recorded in the Port Library.
 * 
 * @param [in]  moduleName The message module name
 * @param [in]  id The message id
 * @param [in]  lastErrorInfo Stores portable error number and message
 */
void
SH_OSCacheFile::errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->lastErrorCode)) {
		Trc_SHR_OSC_Mmap_errorHandler_Entry_V2(moduleName, id, lastErrorInfo->lastErrorCode, lastErrorInfo->lastErrorMsg);
	} else {
		Trc_SHR_OSC_Mmap_errorHandler_Entry_V2(moduleName, id, 0, "");
	}

	if(moduleName && id && _verboseFlags) {
		Trc_SHR_OSC_Mmap_errorHandler_printingMessage(_verboseFlags);
		j9nls_printf(PORTLIB, J9NLS_ERROR, moduleName, id);
		if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->lastErrorCode)) {
			I_32 errorno = lastErrorInfo->lastErrorCode;
			const char* errormsg = lastErrorInfo->lastErrorMsg;
			Trc_SHR_OSC_Mmap_errorHandler_printingPortMessages();
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, errormsg);
		}
	} else {
		Trc_SHR_OSC_Mmap_errorHandler_notPrintingMessage(_verboseFlags);
	}
	Trc_SHR_OSC_Mmap_errorHandler_Exit();
}

/**
 * Method to return the object's error status code
 * 
 * Note;  This method of error reporting is for consistency with SH_OSCachesysv
 * 
 * @return The object's error code
 */
IDATA
SH_OSCacheFile::getError()
{
	Trc_SHR_OSC_Mmap_getError_Event(_errorCode);
	return _errorCode;
}

/**
 * Method to set the object's error status code
 * 
 * @param [in]  errorCode The error code to be set
 * 
 */
void
SH_OSCacheFile::setError(IDATA errorCode)
{
	Trc_SHR_OSC_Mmap_setError_Entry(errorCode);
	_errorCode = errorCode;
	Trc_SHR_OSC_Mmap_setError_Exit(_errorCode);
}

/* Used for connecting to legacy cache headers */
void* 
SH_OSCacheFile::getMmapHeaderFieldAddressForGen(void* header, UDATA headerGen, UDATA fieldID)
{
	return (U_8*)header + getMmapHeaderFieldOffsetForGen(headerGen, fieldID);
}

/* Used for connecting to legacy cache headers */
IDATA 
SH_OSCacheFile::getMmapHeaderFieldOffsetForGen(UDATA headerGen, UDATA fieldID)
{
	if ((4 < headerGen) && (headerGen <= OSCACHE_CURRENT_CACHE_GEN)) {
		switch (fieldID) {
		case OSCACHEMMAP_HEADER_FIELD_CREATE_TIME :
			return offsetof(OSCachemmap_header_version_current, createTime);
		case OSCACHEMMAP_HEADER_FIELD_LAST_ATTACHED_TIME :
			return offsetof(OSCachemmap_header_version_current, lastAttachedTime);
		case OSCACHEMMAP_HEADER_FIELD_LAST_DETACHED_TIME :
			return offsetof(OSCachemmap_header_version_current, lastDetachedTime);
		case OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK :
			return offsetof(OSCachemmap_header_version_current, headerLock);
		case OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK :
			return offsetof(OSCachemmap_header_version_current, attachLock);
		case OSCACHEMMAP_HEADER_FIELD_DATA_LOCKS :
			return offsetof(OSCachemmap_header_version_current, dataLocks);
		default :
		{
			IDATA osc_offset = getHeaderFieldOffsetForGen(headerGen, fieldID);
			
			if (osc_offset) {
				return offsetof(OSCachemmap_header_version_current, oscHdr) + osc_offset;
			}
		}
		}
	} else if (4 == headerGen) {
		switch (fieldID) {
		case OSCACHEMMAP_HEADER_FIELD_CREATE_TIME :
			return offsetof(OSCachemmap_header_version_G04, createTime);
		case OSCACHEMMAP_HEADER_FIELD_LAST_ATTACHED_TIME :
			return offsetof(OSCachemmap_header_version_G04, lastAttachedTime);
		case OSCACHEMMAP_HEADER_FIELD_LAST_DETACHED_TIME :
			return offsetof(OSCachemmap_header_version_G04, lastDetachedTime);
		case OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK :
			return offsetof(OSCachemmap_header_version_G04, headerLock);
		case OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK :
			return offsetof(OSCachemmap_header_version_G04, attachLock);
		case OSCACHEMMAP_HEADER_FIELD_DATA_LOCKS :
			return offsetof(OSCachemmap_header_version_G04, dataLocks);
		default :
		{
			IDATA osc_offset = getHeaderFieldOffsetForGen(headerGen, fieldID);
			
			if (osc_offset) {
				return offsetof(OSCachemmap_header_version_G04, oscHdr) + osc_offset;
			}
		}
		}
	} else if (3 == headerGen) {
		switch (fieldID) {
		case OSCACHEMMAP_HEADER_FIELD_CREATE_TIME :
			return offsetof(OSCachemmap_header_version_G03, createTime);
		case OSCACHEMMAP_HEADER_FIELD_LAST_ATTACHED_TIME :
			return offsetof(OSCachemmap_header_version_G03, lastAttachedTime);
		case OSCACHEMMAP_HEADER_FIELD_LAST_DETACHED_TIME :
			return offsetof(OSCachemmap_header_version_G03, lastDetachedTime);
		case OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK :
			return offsetof(OSCachemmap_header_version_G03, headerLock);
		case OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK :
			return offsetof(OSCachemmap_header_version_G03, attachLock);
		case OSCACHEMMAP_HEADER_FIELD_DATA_LOCKS :
			return offsetof(OSCachemmap_header_version_G03, dataLocks);
		case OSCACHE_HEADER_FIELD_CACHE_INIT_COMPLETE :
			return offsetof(OSCachemmap_header_version_G03, cacheInitComplete);
		default :
		{
			IDATA osc_offset = getHeaderFieldOffsetForGen(headerGen, fieldID);
			
			if (osc_offset) {
				return offsetof(OSCachemmap_header_version_G03, oscHdr) + osc_offset;
			}
		}
		}
	}
	Trc_SHR_Assert_ShouldNeverHappen();
	return 0;
}

/**
 * Find the first cache file in a given cacheDir
 * Follows the format of j9file_findfirst
 *
 * @param [in] portLibrary  A port library
 * @param [in] findHandle  The handle of the last file found
 * @param [in] cacheType  The type of cache file
 * @param [out] resultbuf  The name of the file found
 *
 * @return A handle to the cache file found or -1 if the cache file doesn't exist
 */
UDATA
SH_OSCacheFile::findfirst(J9PortLibrary *portLibrary, char *cacheDir, char *resultbuf, UDATA cacheType)
{
	UDATA findHandle = (UDATA)-1;
	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_SHR_OSC_File_findfirst_Entry(cacheDir);

	findHandle = j9file_findfirst(cacheDir, resultbuf);

	if ((UDATA)-1 == findHandle) {
		Trc_SHR_OSC_File_findfirst_NoFileFound(cacheDir);
		return (UDATA)-1;
	}
	while (!isCacheFileName(PORTLIB, resultbuf, cacheType, NULL)) {
		if (-1 == j9file_findnext(findHandle, resultbuf)) {
			j9file_findclose(findHandle);
			Trc_SHR_OSC_File_findfirst_NoSharedCacheFileFound(cacheDir);
			return (UDATA)-1;
		}
	}

	Trc_SHR_OSC_File_findfirst_Exit(findHandle);
	return findHandle;
}

/**
 * Find the next cache file in a given cacheDir
 * Follows the format of j9file_findnext
 *
 * @param [in] portLibrary Pointer to the port library
 * @param [in] findHandle The handle of the last file found
 * @param [in] cacheType  The type of cache file
 * @param [out] resultbuf The name of the next snapshot file found
 *
 * @return A handle to the next cache file found or -1 if the cache file doesn't exist
 */
I_32
SH_OSCacheFile::findnext(J9PortLibrary *portLibrary, UDATA findHandle, char *resultbuf, UDATA cacheType)
{
	I_32 rc = -1;
	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_SHR_OSC_File_findnext_Entry();

	do {
		rc = j9file_findnext(findHandle, resultbuf);
	} while ((-1 != rc)
			&& (!isCacheFileName(PORTLIB, resultbuf, cacheType, NULL))
			);

	Trc_SHR_OSC_File_findnext_Exit();
	return rc;
}

/**
 * Finish finding cache files
 * Follows the format of j9file_findclose
 *
 * @param [in] portLibrary  A port library
 * @param [in] findHandle  The handle of the last file found
 */
void
SH_OSCacheFile::findclose(J9PortLibrary *portLibrary, UDATA findhandle)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_SHR_OSC_File_findclose_Entry();
	j9file_findclose(findhandle);
	Trc_SHR_OSC_File_findclose_Exit();

}

/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to shared cache file
 * 
 * @param [in] portLibrary  The port library
 * @param [in] findHandle  The handle of the shared cache file
 * @param[in] openMode The file access mode
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 *
 * @return enum SH_CacheFileAccess indicating if the process can access the shared cache file set or not
 */
SH_CacheFileAccess
SH_OSCacheFile::checkCacheFileAccess(J9PortLibrary *portLibrary, UDATA fileHandle, I_32 openMode, LastErrorInfo *lastErrorInfo)
{
	SH_CacheFileAccess cacheFileAccess = J9SH_CACHE_FILE_ACCESS_ALLOWED;

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

#if !defined(WIN32)
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9FileStat statBuf;
	IDATA rc = j9file_fstat(fileHandle, &statBuf);

	if (-1 != rc) {
		UDATA uid = j9sysinfo_get_euid();

		if (statBuf.ownerUid != uid) {
			UDATA gid = j9sysinfo_get_egid();
			bool sameGroup = false;

			if (statBuf.ownerGid == gid) {
				sameGroup = true;
				Trc_SHR_OSC_File_checkCacheFileAccess_GroupIDMatch(gid, statBuf.ownerGid);
			} else {
				/* check supplementary groups */
				U_32 *list = NULL;
				IDATA size = 0;
				IDATA i = 0;

				size = j9sysinfo_get_groups(&list, J9MEM_CATEGORY_CLASSES_SHC_CACHE);
				if (size > 0) {
					for (i = 0; i < size; i++) {
						if (statBuf.ownerGid == list[i]) {
							sameGroup = true;
							Trc_SHR_OSC_File_checkCacheFileAccess_SupplementaryGroupMatch(list[i], statBuf.ownerGid);
							break;
						}
					}
				} else {
					cacheFileAccess = J9SH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED;
					if (NULL != lastErrorInfo) {
						lastErrorInfo->lastErrorCode = j9error_last_error_number();
						lastErrorInfo->lastErrorMsg = j9error_last_error_message();
					}
					Trc_SHR_OSC_File_checkCacheFileAccess_GetGroupsFailed();
					goto _end;
				}
				if (NULL != list) {
					j9mem_free_memory(list);
				}
			}
			if (sameGroup) {
				/* This process belongs to same group as owner of the shared cache file. */
				if (!J9_ARE_ANY_BITS_SET(openMode, J9OSCACHE_OPEN_MODE_GROUPACCESS)) {
					/* If 'groupAccess' option is not set, it implies this process wants to access a shared cache file that it created.
					 * But this process is not the owner of the cache file.
					 * This implies we should not allow this process to use the cache.
					 */
					cacheFileAccess = J9SH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED;
					Trc_SHR_OSC_File_checkCacheFileAccess_GroupAccessRequired();
				}
			} else {
				/* This process does not belong to same group as owner of the shared cache file.
				 * Do not allow access to the cache.
				 */
				cacheFileAccess = J9SH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED;
				Trc_SHR_OSC_File_checkCacheFileAccess_OthersNotAllowed();
			}
		}
	} else {
			cacheFileAccess = J9SH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED;
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = j9error_last_error_number();
			lastErrorInfo->lastErrorMsg = j9error_last_error_message();
		}
		Trc_SHR_OSC_File_checkCacheFileAccess_FileStatFailed();
	}

_end:
#endif /* !defined(WIN32) */

	return cacheFileAccess;
}

/**
 * This method checks whether the group access of the shared cache file is successfully set when a new cache/snapshot is created with "groupAccess" suboption
 *
 * @param [in] portLibrary The port library
 * @param[in] fileHandle The handle of the shared cache file
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 * 
 * @return -1 Failed to get the stats of the file.
 * 			0 Group access is not set.
 * 			1 Group access is set.
 */
I_32
SH_OSCacheFile::verifyCacheFileGroupAccess(J9PortLibrary *portLibrary, IDATA fileHandle, LastErrorInfo *lastErrorInfo)
{
	I_32 rc = 1;
#if !defined(WIN32)
	struct J9FileStat statBuf;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	memset(&statBuf, 0, sizeof(statBuf));
	if (0 == j9file_fstat(fileHandle, &statBuf)) {
		if ((1 != statBuf.perm.isGroupWriteable)
			|| (1 != statBuf.perm.isGroupReadable)
		) {
			rc = 0;
		}
	} else {
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = j9error_last_error_number();
			lastErrorInfo->lastErrorMsg = j9error_last_error_message();
		}
		rc = -1;
	}

#endif /* !defined(WIN32) */
	return rc;
}
