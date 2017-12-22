/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#if !defined(OPENCACHEHELPER_H_INCLUDED)
#define OPENCACHEHELPER_H_INCLUDED

extern "C"
{
#include "shrinit.h"
}
#include "SCTestCommon.h"
#include "UnitTest.hpp"

#define DEFAULT_CACHE_NAME "shrtestcache"

typedef char* BlockPtr;

class OpenCacheHelper {

protected:
	BlockPtr cacheMemory, cacheAllocated;
	SH_OSCache *oscache;
	UDATA osPageSize;
	bool doSegmentProtect;
	J9JavaVM* vm;
	const char *cacheName;
	char *cacheDir;
	bool inMemoryCache;
	I_32 cacheType;

public:
	J9SharedClassPreinitConfig *piConfig, *origPiConfig;
	J9SharedClassConfig *sharedClassConfig, *origSharedClassConfig;
	SH_CacheMap *cacheMap;

	IDATA openTestCache(I_32 cacheType, I_32 cacheSize, const char *cacheName,
			bool inMemoryCache, BlockPtr cacheBuffer,
			char *cacheDir = NULL, const char *cacheDirPermStr = NULL,
			U_64 extraRunTimeFlag = 0,
			U_64 unsetTheseRunTimeFlag = (J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW),
			IDATA unitTest = UnitTest::NO_TEST,
			UDATA extraVerboseFlag = J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT,
			const char * testName="openTestCache",
			bool startupWillFail=false,
			bool doCleanupOnFail=true,
			J9SharedClassPreinitConfig* cacheDetails=0,
			J9SharedClassConfig* sharedclassconfig=0);
	IDATA closeTestCache(bool freeCache);
	IDATA addDummyROMClass(const char *romClassName, U_32 dummyROMClassSize = 4096);
	void setRomClassName(J9ROMClass * rc, const char * name);
	OpenCacheHelper(J9JavaVM* vm)
	{
		doSegmentProtect = false;
		osPageSize=0;
		oscache=NULL;
		piConfig=NULL;
		origPiConfig=NULL; 
		sharedClassConfig=NULL; 
		origSharedClassConfig=NULL; 
		cacheMap=NULL;
		cacheAllocated = NULL;
		cacheMemory=NULL;
		this->vm = vm;
		origSharedClassConfig = NULL;
	}	
	~OpenCacheHelper()
	{
	}
	void setOscache(SH_OSCache * osc) {
		oscache = osc;
	}
	void setOsPageSize(UDATA pagesize) {
		osPageSize = pagesize;
	}
	void setDoSegmentProtect(bool value) {
		doSegmentProtect = value;
	}
};

#endif /* !defined(OPENCACHEHELPER_H_INCLUDED) */
