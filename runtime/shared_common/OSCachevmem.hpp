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
#if !defined(OSCACHEVMEM_HPP_INCLUDED)
#define OSCACHEVMEM_HPP_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "j9port.h"
#include "pool_api.h"
#include "OSCacheFile.hpp"

typedef struct OSCache_vmem_header1 {
	OSCache_header2 oscHdr;
	U_32 unused32[5];
	U_64 unused64[5];
} OSCache_vmem_header2;

typedef OSCache_vmem_header1 OSCachevmem_header_version_current;

#define J9SH_OSCACHE_VMEM_LOCK_COUNT 5

/**
 * A class to manage Shared Classes on Operating System level
 * 
 * This class provides an abstraction of a shared memory mapped file
 */
class SH_OSCachevmem : public SH_OSCacheFile
{
public:
	SH_OSCachevmem(J9PortLibrary* portlib, const char* cachedirname, const char* cacheName, J9SharedClassPreinitConfig* piconfig, IDATA numLocks,
			UDATA createFlag, UDATA verboseFlags, U_64 runtimeFlags, I_32 openMode, J9PortShcVersion* versionData, SH_OSCacheInitializer* initializer);
	/*This constructor should only be used by this class or its parent*/
	SH_OSCachevmem() {};

	/**
	 * Override new operator
	 * @param [in] sizeArg  The size of the object
	 * @param [in] memoryPtrArg  Pointer to the address where the object must be located
	 *
	 * @return The value of memoryPtrArg
	 */
	void *operator new(size_t sizeArg, void *memoryPtrArg) { return memoryPtrArg; };

	virtual bool startup(const char* ctrlDirName, UDATA cacheDirPerm, const char* cacheName, J9SharedClassPreinitConfig* piconfig, IDATA numLocks,
			UDATA createFlag, UDATA verboseFlags, U_64 runtimeFlags, I_32 openMode, UDATA storageKeyTesting, J9PortShcVersion* versionData, SH_OSCacheInitializer* initializer, UDATA reason);

	/* Work around compile error on mac osx due to hiding the other startup api */
	using SH_OSCache::startup;

	virtual IDATA destroy(bool suppressVerbose, bool isReset);

	virtual void cleanup();
	
	virtual void* attach(J9VMThread *currentThread, J9PortShcVersion* expectedVersionData);
	
	IDATA getWriteLockID(void);
	IDATA getReadWriteLockID(void);
	virtual IDATA acquireWriteLock(UDATA lockID);
	virtual IDATA releaseWriteLock(UDATA lockID);
  	
	virtual IDATA setRegionPermissions(J9PortLibrary* portLibrary, void *address, UDATA length, UDATA flags);
	
	virtual UDATA getPermissionsRegionGranularity(J9PortLibrary* portLibrary);

	virtual void runExitCode();
	
	virtual IDATA getLockCapabilities();
	
	virtual void initialize(J9PortLibrary* portLibrary, char* memForConstructor, UDATA generation);
	
	virtual U_32 getTotalSize();

	virtual UDATA getJavacoreData(J9JavaVM *vm, J9SharedClassJavacoreDataDescriptor* descriptor);

protected:

	virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo);
	virtual void * getAttachedMemory();
	
private:
	I_64 _actualFileLength;
	UDATA _writeLockCounter;
	
	omrthread_monitor_t _lockMutex[J9SH_OSCACHE_VMEM_LOCK_COUNT];

	virtual IDATA getNewWriteLockID();
};

#endif /* OSCACHEVMEM_HPP_INCLUDED */
