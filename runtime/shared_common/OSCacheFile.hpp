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
#if !defined(OSCACHEFILE_HPP_INCLUDED)
#define OSCACHEFILE_HPP_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "j9port.h"
#include "pool_api.h"
#include "OSCache.hpp"

#define J9SH_OSCACHE_MMAP_EYECATCHER "J9SCMAP"
#define J9SH_OSCACHE_MMAP_EYECATCHER_LENGTH 7

#define MMAP_CACHEHEADERSIZE SHC_PAD(sizeof(OSCachemmap_header_version_current), SHC_WORDALIGN)
#define MMAP_DATASTARTFROMHEADER(dataStartFieldAddr) SRP_GET(*dataStartFieldAddr, void *);


/* DO NOT use UDATA/IDATA in the cache headers so that 32-bit/64-bit JVMs can read each others headers
 * This is why OSCache_mmap_header2 was added
 * 
 * Versioning is achieved by using the typedef aliases below
 */

/* CMVC 145095: There are 5 locks, but only 2 are used. If we ever use more 
 * we need to re-fix SH_OSCachemmap::acquireWriteLock b/c it assumes 2 
 * locks when attempting to resolve EDEADLK.
 */
#define J9SH_OSCACHE_MMAP_LOCK_COUNT 5
#define J9SH_OSCACHE_MMAP_LOCKID_WRITELOCK 0
#define J9SH_OSCACHE_MMAP_LOCKID_READWRITELOCK 1

typedef struct OSCache_mmap_header1 {
	char eyecatcher[J9SH_OSCACHE_MMAP_EYECATCHER_LENGTH+1];
	OSCache_header1 oscHdr;
	UDATA cacheInitComplete;
	UDATA unused[9];
	I_64 createTime;
	I_64 lastAttachedTime;
	I_64 lastDetachedTime;
	I_32 headerLock;				/* Do not change the size of the header lock */
	I_32 attachLock;
	I_32 dataLocks[J9SH_OSCACHE_MMAP_LOCK_COUNT];
} OSCache_mmap_header1;

typedef struct OSCache_mmap_header2 {
	char eyecatcher[J9SH_OSCACHE_MMAP_EYECATCHER_LENGTH+1];
	OSCache_header2 oscHdr;
	I_64 createTime;
	I_64 lastAttachedTime;
	I_64 lastDetachedTime;
	I_32 headerLock;
	I_32 attachLock;
	I_32 dataLocks[J9SH_OSCACHE_MMAP_LOCK_COUNT];
	U_32 unused32[5];
	U_64 unused64[5];
} OSCache_mmap_header2;

typedef OSCache_mmap_header2 OSCachemmap_header_version_current;
typedef OSCache_mmap_header2 OSCachemmap_header_version_G04;
typedef OSCache_mmap_header1 OSCachemmap_header_version_G03;
/* STRUCT_G02 and STRUCT_G01 have not shipped, so are not included here */

#define OSCACHEMMAP_HEADER_FIELD_CREATE_TIME 1001
#define OSCACHEMMAP_HEADER_FIELD_LAST_ATTACHED_TIME 1002
#define OSCACHEMMAP_HEADER_FIELD_LAST_DETACHED_TIME 1003
#define OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK 1004
#define OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK 1005
#define OSCACHEMMAP_HEADER_FIELD_DATA_LOCKS 1006

/**
 * This enum contains constants that are used to indicate the reason for not allowing access to the cache file.
 * It is returned by @ref SH_OSCacheFile::checkCacheFileAccess().
 */
typedef enum SH_CacheFileAccess {
	J9SH_CACHE_FILE_ACCESS_ALLOWED 				= 0,
	J9SH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED,
	J9SH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED,
	J9SH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED,
} SH_CacheFileAccess;

/**
 * A class to manage Shared Classes on Operating System level
 * 
 * This class provides an abstraction of a file
 */
class SH_OSCacheFile : public SH_OSCache
{
public:
	/**
	 * Override new operator
	 * @param [in] sizeArg  The size of the object
	 * @param [in] memoryPtrArg  Pointer to the address where the object must be located
	 *
	 * @return The value of memoryPtrArg
	 */
	void *operator new(size_t sizeArg, void *memoryPtrArg) { return memoryPtrArg; };

	virtual IDATA getError();

	static UDATA findfirst(J9PortLibrary *portLibrary, char *cacheDir, char *resultbuf, UDATA cacheType);

	static I_32 findnext(J9PortLibrary *portLibrary, UDATA findHandle, char *resultbuf, UDATA cacheType);

	static void findclose(J9PortLibrary *portLibrary, UDATA findhandle);

	static SH_CacheFileAccess checkCacheFileAccess(J9PortLibrary *portLibrary, UDATA fileHandle, I_32 openMode, LastErrorInfo *lastErrorInfo);

	static I_32 verifyCacheFileGroupAccess(J9PortLibrary *portLibrary, IDATA fileHandle, LastErrorInfo *lastErrorInfo);

protected:
	virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo);
	
	IDATA _fileHandle;
	
	IDATA acquireHeaderWriteLock(UDATA generation, LastErrorInfo *lastErrorInfo);
	IDATA releaseHeaderWriteLock(UDATA generation, LastErrorInfo *lastErrorInfo);

	IDATA tryAcquireAttachWriteLock(UDATA generation);
	IDATA releaseAttachWriteLock(UDATA generation); 
	
	IDATA isCacheHeaderValid(OSCachemmap_header_version_current *header, J9PortShcVersion* expectedVersionData);

	bool openCacheFile(bool doCreateFile, LastErrorInfo *lastErrorInfo);
	bool closeCacheFile();

	void setError(IDATA errorCode);

	static IDATA getMmapHeaderFieldOffsetForGen(UDATA headerGen, UDATA fieldID);

	static void* getMmapHeaderFieldAddressForGen(void* header, UDATA headerGen, UDATA fieldID);

private:
	I_32 getFileMode();
};

#endif /* OSCACHEFILE_HPP_INCLUDED */
