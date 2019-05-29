/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include "j2sever.h"
#include "j9protos.h"
#include "j9shrnls.h"
#include "jvminit.h"
#include "shrinit.h"
#include "ut_j9shr.h"

#include "AtomicSupport.hpp"
#include "UnitTest.hpp"
#include "CompositeCacheImpl.hpp"

#define SEMSUFFIX "SHSEM"

/**
 * CACHE AREAS
 *
 * Cache areas consists of a header followed by a series of segment entries
 * followed by free space followed by a series of cache entries. 
 * The layout of a cache entry is described later.
 *
 * *-----------------------------*---------*-------*-------*-------------------*-------*-------*-------------------*
 * |                             |         |       |       |                   |       |       |   Class Debug     |
 * | J9SharedCacheHeader         |readWrite|segment|segment|    free space     |cache  |cache  |   Attribute       |
 * |  {totalBytes, updateSRP,    |area     |entry1 |entry2 |                   |entry2 |entry1 |   Data            |
 * |   updateCount, segmentSRP}  |         |       |       |                   |       |       |                   |
 * |                             |         |       |       |                   |       |       |                   |
 * *-----------------------------*---------*-------*-------*-------------------*-------*-------*-------------------*
 *
 *  <-------------------- totalBytes ------------------------------------------------------------------------------>
 *  <-------------------- readWriteBytes -->
 *                                                          <--- free --------->
 * ^                             ^                         ^                   ^               ^                   ^
 * _theca				   readWriteSRP                segmentSRP          updateSRP      CADEBUGSTART         CAEND
 */
 
#define CASTART(ca)  (((BlockPtr)(ca)) + (ca)->readWriteBytes)
#define CADEBUGSTART(ca) (((BlockPtr)(ca)) + (ca)->totalBytes - (ca)->debugRegionSize)
#define CAEND(ca) (((BlockPtr)(ca)) + (ca)->totalBytes)
#define CCFIRSTENTRY(ca) (((BlockPtr)(ca)) + (ca)->totalBytes - (ca)->debugRegionSize - sizeof(ShcItemHdr))

#define UPDATEPTR(ca) (((BlockPtr)(ca)) + (ca)->updateSRP)
#define RWUPDATEPTR(ca) (((BlockPtr)(ca)) + (ca)->readWriteSRP)
#define SEGUPDATEPTR(ca) (((BlockPtr)(ca)) + (ca)->segmentSRP)

#define READWRITEAREASIZE(ca) ((ca)->readWriteBytes - sizeof(J9SharedCacheHeader))
#define READWRITEAREASTART(ca) (((BlockPtr)(ca)) + sizeof(J9SharedCacheHeader))

#define FREEBYTES(ca) ((ca)->updateSRP - (ca)->segmentSRP)
#define FREEREADWRITEBYTES(ca) ((ca)->readWriteBytes - (U_32)(ca)->readWriteSRP)

#define CC_TRACE(verboseLevel, nlsFlags, var1) if (_verboseFlags & verboseLevel) j9nls_printf(PORTLIB, nlsFlags, var1)
#define CC_TRACE1(verboseLevel, nlsFlags, var1, p1) if (_verboseFlags & verboseLevel) j9nls_printf(PORTLIB, nlsFlags, var1, p1)
#define CC_TRACE2(verboseLevel, nlsFlags, var1, p1, p2) if (_verboseFlags & verboseLevel) j9nls_printf(PORTLIB, nlsFlags, var1, p1, p2)
#define CC_ERR_TRACE(var) if (_verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var)
#define CC_ERR_TRACE1(var, p1) if (_verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var, p1)
#define CC_ERR_TRACE2(var, p1, p2) if (_verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var, p1, p2)
#define CC_WARNING_TRACE(var, isJCLCall) if (_verboseFlags && !isJCLCall) j9nls_printf(PORTLIB, J9NLS_WARNING, var)
#define CC_WARNING_TRACE1(var, p1, isJCLCall) if (_verboseFlags && !isJCLCall) j9nls_printf(PORTLIB, J9NLS_WARNING, var, p1)
#define CC_INFO_TRACE(var, isJCLCall) if (_verboseFlags && !isJCLCall) j9nls_printf(PORTLIB, J9NLS_INFO, var)
#define CC_INFO_TRACE1(var, p1, isJCLCall) if (_verboseFlags && !isJCLCall) j9nls_printf(PORTLIB, J9NLS_INFO, var, p1)
#define CC_INFO_TRACE3(var, p1, p2, p3) if (_verboseFlags) j9nls_printf(PORTLIB, J9NLS_INFO, var, p1, p2, p3)

#define CACHE_CORRUPT_FLAG 0x40000000
#define WRITEHASH_MASK 0x000FFFFF
#define WRITEHASH_SHIFT 20
#define FAILED_WRITEHASH_MAX_COUNT 20

#define DEFAULT_READWRITE_BYTES_DIVISOR 150		/* 1/150 of cache is set aside for readWrite by default */

#define ALLOCATE_TYPE_BLOCK 1
#define ALLOCATE_TYPE_AOT 2
#define ALLOCATE_TYPE_JIT 3

#define DIRECTION_FORWARD 1
#define DIRECTION_BACKWARD 2

#define CC_NUM_WRITE_LOCKS 2

/* If the means of calculating CRC changes, increment the CC_CRC_VALID_VALUE */
#define CC_CRC_VALID_VALUE 3

#define CC_INIT_COMPLETE 2
#define CC_STARTUP_COMPLETE 1

#define CC_READONLY_LOCK_VALUE (U_32)-1
#define CC_MAX_READONLY_WAIT_FOR_CACHE_LOCK_MILLIS 100

#define CC_COULD_NOT_ENTER_STRINGTABLE_ON_STARTUP 0xdeadbeef

#define CACHE_LOCK_PATIENCE_COUNTER 400

SH_CompositeCacheImpl::SH_SharedCacheHeaderInit*
SH_CompositeCacheImpl::SH_SharedCacheHeaderInit::newInstance(SH_SharedCacheHeaderInit* memForConstructor)
{
	new(memForConstructor) SH_SharedCacheHeaderInit();

	return memForConstructor;
}

void 
SH_CompositeCacheImpl::SH_SharedCacheHeaderInit::init(BlockPtr data, U_32 len, I_32 minAOT, I_32 maxAOT, I_32 minJIT, I_32 maxJIT, U_32 readWriteLen, U_32 softMaxBytes)
{
	J9SharedCacheHeader* ca = (J9SharedCacheHeader*)data;

	memset(ca, 0, sizeof(J9SharedCacheHeader));

	ca->totalBytes = len;
	ca->readWriteBytes = readWriteLen + sizeof(J9SharedCacheHeader);
	ca->updateSRP = (UDATA)len;
	ca->readWriteSRP = sizeof(J9SharedCacheHeader);
	ca->segmentSRP = (UDATA)ca->readWriteBytes;
	ca->minAOT = minAOT;
	ca->maxAOT = maxAOT;
	ca->crcValue = 0;
	ca->crcValid = 0;
	ca->debugRegionSize = 0;
	ca->lineNumberTableNextSRP = 0;
	ca->localVariableTableNextSRP = 0;
	ca->ccInitComplete = CC_INIT_COMPLETE;
	ca->minJIT = minJIT;
	ca->maxJIT = maxJIT;
	ca->sharedInternTableBytes = -1;
	ca->corruptionCode = NO_CORRUPTION;
	ca->corruptValue = 0;
	ca->lastMetadataType = 0;
	ca->writerCount = 0;
	ca->softMaxBytes = softMaxBytes;
	ca->cacheFullFlags = 0;
	ca->unused8 = 0;
	ca->unused9 = 0;
	ca->unused10 = 0;
	/* Note that the updateCountLockWord is only ever used single threaded, so no need to dereference this */
	WSRP_SET(ca->updateCountPtr, &(ca->updateCount));
	WSRP_SET(ca->corruptFlagPtr, &(ca->corruptFlag));
	WSRP_SET(ca->lockedPtr, &(ca->locked));
}

/**
 * Constructs a new instance of SH_CompositeCache.
 * 
 * @param [in] vm A Java VM
 * @param [in] memForConstructor Memory for building this object as
 *       returned by @ref getRequiredConstrBytes
 * @param [in] cacheDirName Name of the control directory which will contain the control files
 * @param [in] cacheName Name of the cache
 * @param [in] cacheTypeRequired The cache type required
 *
 * @return A pointer to the new instance
 */
SH_CompositeCacheImpl*
SH_CompositeCacheImpl::newInstance(J9JavaVM* vm, J9SharedClassConfig* sharedClassConfig, SH_CompositeCacheImpl* memForConstructor, const char* cacheName, I_32 cacheTypeRequired, bool startupForStats)
{
	SH_CompositeCacheImpl* newCC = (SH_CompositeCacheImpl*)memForConstructor;

	new(newCC) SH_CompositeCacheImpl();
	newCC->initializeWithCommonInfo(vm, sharedClassConfig, ((BlockPtr)memForConstructor + sizeof(SH_CompositeCacheImpl)), cacheName, cacheTypeRequired, startupForStats);

	return newCC;
}

#if defined(J9SHR_CACHELET_SUPPORT)

/**
 * Constructs a new nested instance of SH_CompositeCache.
 * 
 * @param [in] vm A Java VM
 * @param [in] parent The cache in which this cache is nested
 * @param [in] memForConstructor Memory for building this object as
 *       returned by @ref getRequiredConstrBytes
 *
 * @return A pointer to the new instance
 */
SH_CompositeCacheImpl*
SH_CompositeCacheImpl::newInstanceNested(J9JavaVM* vm, SH_CompositeCacheImpl* parent, SH_CompositeCacheImpl* memForConstructor, UDATA nestedSize, BlockPtr nestedMemory, bool creatingCachelet)
{
	SH_CompositeCacheImpl* newCC = (SH_CompositeCacheImpl*)memForConstructor;

	new(newCC) SH_CompositeCacheImpl();
	newCC->initializeNested(vm, parent, ((BlockPtr)memForConstructor + sizeof(SH_CompositeCacheImpl)), nestedSize, nestedMemory, creatingCachelet);

	if (0 != omrthread_monitor_init_with_name(&newCC->_startupMonitor, 0, "Cachelet startup")) {
		Trc_SHR_CC_newInstanceNested_allocStartupMonitorFailed(newCC);
		return NULL;
	}
	return newCC;
}

SH_CompositeCacheImpl*
SH_CompositeCacheImpl::newInstanceChained(J9JavaVM* vm, SH_CompositeCacheImpl* memForConstructor, J9SharedClassConfig* sharedClassConfig, I_32 cacheTypeRequired)
{
	SH_CompositeCacheImpl* newCC = (SH_CompositeCacheImpl*)memForConstructor;

	new(newCC) SH_CompositeCacheImpl();
	newCC->initialize(vm, ((BlockPtr)memForConstructor + sizeof(SH_CompositeCacheImpl)), sharedClassConfig, "CHAINED_CACHE_ROOT", cacheTypeRequired, false);

	return newCC;
}

#endif /* J9SHR_CACHELET_SUPPORT */

void
SH_CompositeCacheImpl::commonInit(J9JavaVM* vm)
{
	_started = false;
	_cacheName = NULL;
	_portlib = vm->portLibrary;
	_theca = NULL;
	_scan = NULL;
	_prevScan = NULL;
	_storedScan = NULL;
	_storedPrevScan = NULL;
	_oldUpdateCount = 0;
	_totalStoredBytes = _storedMetaUsedBytes = _storedSegmentUsedBytes = _storedAOTUsedBytes = _storedJITUsedBytes = _storedReadWriteUsedBytes = 0;
	_softmxUnstoredBytes = 0;
	_maxAOTUnstoredBytes = 0;
	_maxJITUnstoredBytes = 0;
	_maxAOT = 0;
	_maxJIT = 0;
	_lastFailedWHCount = 0;
	_lastFailedWriteHash = 0;
	_headerProtectMutex = NULL;
	_runtimeFlagsProtectMutex = NULL;
	_readOnlyOSCache = false;
	_readOnlyReaderCount = 0;
	_readWriteProtectCntr = 0;
	_headerProtectCntr = 1;		/* Initialize to 1 indicating "unprotected" */
	_readWriteAreaHeaderIsReadOnly = false;
	_metadataSegmentPtr = NULL;
	_currentROMSegment = NULL;
	_next = NULL;
	_ccHead = NULL;
	_cacheFullFlags = 0;
#if defined(J9SHR_CACHELET_SUPPORT)
	_deployedOffset = 0;
#endif
	_doMetaProtect = _doSegmentProtect = _doHeaderProtect = _doHeaderReadWriteProtect = _doReadWriteSync = _doPartialPagesProtect = false;
	_useWriteHash = false;
	_reduceStoreContentionDisabled = false;
	_initializingNewCache = false;
	_minimumAccessedShrCacheMetadata = 0;
	_maximumAccessedShrCacheMetadata = 0;
}

#if defined(J9SHR_CACHELET_SUPPORT)

void
SH_CompositeCacheImpl::initializeNested(J9JavaVM* vm, SH_CompositeCacheImpl* parent, BlockPtr memForConstructor, UDATA nestedSize, BlockPtr nestedMemory, bool creatingCachelet)
{
	J9VMThread* currentThread = vm->internalVMFunctions->currentVMThread(vm);

	commonInit(vm);

	if (creatingCachelet) {
		parent->setContainsCachelets(currentThread);
	}
	_parent = parent;
	_newHdrPtr = (SH_SharedCacheHeaderInit*)((BlockPtr)memForConstructor);
	_oscache = NULL;
	_osPageSize = parent->_osPageSize;
	_readOnlyOSCache = parent->_readOnlyOSCache;
	_nestedSize = nestedSize;
	_nestedMemory = nestedMemory;
	_runtimeFlags = parent->_runtimeFlags;
	_verboseFlags = parent->_verboseFlags;
	/*All nested caches use their parents debug area*/
	_debugData = _parent->_debugData;
	/* TODO: Are there any more inherited fields? */
}

#endif /* J9SHR_CACHELET_SUPPORT */

/*
 *  Calculating free block bytes as below
 */
I_32
SH_CompositeCacheImpl::getFreeBlockBytes(void)
{
	I_32 retVal;
	I_32 minAOT = _theca->minAOT;
	I_32 minJIT = _theca->minJIT;
	I_32 aotBytes = (I_32)_theca->aotBytes;
	I_32 jitBytes = (I_32)_theca->jitBytes;
	I_32 freeBytes = (I_32) FREEBYTES(_theca);


	if (((-1 == minAOT) && (-1 == minJIT)) ||
		((-1 == minAOT) && (minJIT <= jitBytes)) ||
		((-1 == minJIT) && (minAOT <= aotBytes)) ||
		((minAOT <= aotBytes) && (minJIT <= jitBytes))
	) {
		/* if no reserved space for AOT and JIT or
		*   no reserved space for AOT and jitBytes is equal to or crossed reserved space for JIT
		*	no reserved space for JIT and aotBytes is equal to or crossed reserved space for AOT
		*	then free block bytes = freeBytes as calculated above
		*/
		retVal = freeBytes;
	} else if (minJIT > jitBytes && ((-1 == minAOT) || (minAOT <= aotBytes))) {
		/*if jitBytes within the reserved space for JIT but no reserved space for AOT
		 * or aotBytes is  equal to or crossed reserved space for AOT
		 * then free block bytes =  freebytes - bytes not yet used in reserved space of JIT
		*/
		retVal = freeBytes - (minJIT - jitBytes) ;
	} else if ((minAOT > aotBytes) && ((-1 == minJIT) || minJIT <= jitBytes)) {
		/*if aotBytes within the reserved space for AOT but no reserved space for JIT
		 * or jitBytes is  equal to or crossed reserved space for JIT
		 * then free block bytes =  freebytes - bytes not yet used in reserved space of AOT
		*/
		retVal = freeBytes - (minAOT - aotBytes) ;
	} else	{
		 /* We are here if both jitBytes and aotBytes are within the their respective reserved space
		 *  free block bytes = freebytes - bytes not yet used in reserved space of AOT -
		 *       						   bytes not yet used in reserved space of JIT
		 */
		retVal = freeBytes - (minJIT - jitBytes) - (minAOT - aotBytes);
	}
	/* When creating the cache with -Xscminaot/-Xscminjit > real cache size, minAOT/minJIT will be set to the same size to the real cache size by ensureCorrectCacheSizes(),
	 * retVal calculated here can be < 0 in this case.
	 */
	return (retVal > 0) ? retVal : 0;
}

void
SH_CompositeCacheImpl::initializeWithCommonInfo(J9JavaVM* vm, J9SharedClassConfig* sharedClassConfig, BlockPtr memForConstructor, const char* cacheName, I_32 cacheTypeRequired, bool startupForStats)
{
	const char * ctrlDirName = NULL;
	BlockPtr memToUse = (BlockPtr)memForConstructor;

	if (sharedClassConfig != NULL) {
		/*sharedClassConfig is NULL when this function called via SH_OSCache::getCacheStatsCommon()*/
		ctrlDirName = sharedClassConfig->ctrlDirName;
	}

	Trc_SHR_CC_initializeWithCommonInfo_Entry1(memForConstructor, ctrlDirName, cacheName, cacheTypeRequired);

	_commonCCInfo = (J9ShrCompositeCacheCommonInfo *)memToUse;
	initCommonCCInfoHelper();

	memToUse += sizeof(J9ShrCompositeCacheCommonInfo);
	initialize(vm, memToUse, sharedClassConfig, cacheName, cacheTypeRequired, startupForStats);

	Trc_SHR_CC_initializeWithCommonInfo_Exit();
}

void
SH_CompositeCacheImpl::initialize(J9JavaVM* vm, BlockPtr memForConstructor, J9SharedClassConfig* sharedClassConfig, const char* cacheName, I_32 cacheTypeRequired, bool startupForStats)
{
	J9PortShcVersion versionData;

	Trc_SHR_CC_initialize_Entry1(memForConstructor, sharedClassConfig, cacheName, cacheTypeRequired, UnitTest::unitTest);

	commonInit(vm);
	setCurrentCacheVersion(vm, J2SE_VERSION(vm), &versionData);
	versionData.cacheType = cacheTypeRequired;

	if ((UnitTest::NO_TEST != UnitTest::unitTest) && (UnitTest::CORRUPT_CACHE_TEST != UnitTest::unitTest)) {
		PORT_ACCESS_FROM_PORT(_portlib);
		_oscache = NULL;
		/* Construct to initialize _osPageSize is copied from SH_OSCache::getPermissionsRegionGranularity().
		 * We cannot call SH_OSCache::getPermissionsRegionGranularity() because _oscache is NULL as
		 * it is not required to initialize _oscache for unit tests.
		 */
		_osPageSize = (j9mmap_capabilities() & J9PORT_MMAP_CAPABILITY_PROTECT) ? j9mmap_get_region_granularity(UnitTest::cacheMemory) : 0;
		_newHdrPtr = (SH_SharedCacheHeaderInit*)((BlockPtr)memForConstructor);
		_debugData = (ClassDebugDataProvider *)((BlockPtr)memForConstructor + sizeof(SH_CompositeCacheImpl::SH_SharedCacheHeaderInit));

		_debugData->initialize();
	} else if (startupForStats == true) {
		_oscache = NULL;
		_newHdrPtr = (SH_SharedCacheHeaderInit*)((BlockPtr)memForConstructor);
		_debugData = (ClassDebugDataProvider *)((BlockPtr)memForConstructor + sizeof(SH_CompositeCacheImpl::SH_SharedCacheHeaderInit));

		_debugData->initialize();
		_osPageSize = 0;
	} else {
		if ((sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_CREATE_OLD_GEN) != 0) {
			_oscache = SH_OSCache::newInstance(_portlib, (SH_OSCache*)memForConstructor, cacheName, SH_OSCache::getCurrentCacheGen()-1, &versionData);
		} else {
			_oscache = SH_OSCache::newInstance(_portlib, (SH_OSCache*)memForConstructor, cacheName, SH_OSCache::getCurrentCacheGen(), &versionData);
		}
		_newHdrPtr = (SH_SharedCacheHeaderInit*)((BlockPtr)memForConstructor + SH_OSCache::getRequiredConstrBytes());
		_debugData = (ClassDebugDataProvider *)((BlockPtr)memForConstructor + SH_OSCache::getRequiredConstrBytes() + sizeof(SH_CompositeCacheImpl::SH_SharedCacheHeaderInit));

		_debugData->initialize();
		_osPageSize = _oscache->getPermissionsRegionGranularity(_portlib);
	}
	_parent = NULL;
	_sharedClassConfig = sharedClassConfig;
	
	Trc_SHR_CC_initialize_Exit();
}

/**
 * Returns memory bytes the constructor requires to build an object instance
 *
 * @param [in] isNested - true if the CompositeCache is nested
 *
 * @return Number of bytes required for a SH_CompositeCache object
 */
UDATA
SH_CompositeCacheImpl::getRequiredConstrBytes(bool isNested, bool startupForStats)
{
	UDATA reqBytes = 0;

	Trc_SHR_CC_getRequiredConstrBytes_Entry(isNested, startupForStats, UnitTest::unitTest);

	if ((isNested == false) &&
		(startupForStats == false) &&
		((UnitTest::NO_TEST == UnitTest::unitTest) || (UnitTest::CORRUPT_CACHE_TEST == UnitTest::unitTest))
	){
		reqBytes += SH_OSCache::getRequiredConstrBytes();
	}
	reqBytes += sizeof(SH_CompositeCacheImpl);
	reqBytes += sizeof(SH_CompositeCacheImpl::SH_SharedCacheHeaderInit);
	reqBytes += sizeof(ClassDebugDataProvider);
	Trc_SHR_CC_getRequiredConstrBytes_Exit();
	return reqBytes;
}


/**
 * Get required bytes for a constructor including the common info structure.
 *
 * @return Number of bytes required for getRequiredConstrBytesWithCommonInfo
 */
UDATA 
SH_CompositeCacheImpl::getRequiredConstrBytesWithCommonInfo(bool isNested, bool startupForStats)
{
	UDATA reqBytes = SH_CompositeCacheImpl::getRequiredConstrBytes(isNested, startupForStats);
	reqBytes += sizeof(J9ShrCompositeCacheCommonInfo);
	return reqBytes;
}

/** 
 * Clean up resources used by the cache
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 */
void
SH_CompositeCacheImpl::cleanup(J9VMThread* currentThread)
{
	Trc_SHR_CC_cleanup_Entry(currentThread);
	
	if (_oscache) {
		_oscache->cleanup();
		if (_headerProtectMutex) {
			omrthread_monitor_destroy(_headerProtectMutex);
		}
		if (_runtimeFlagsProtectMutex) {
			omrthread_monitor_destroy(_runtimeFlagsProtectMutex);
		}
	} else if (_utMutex) {
		omrthread_monitor_destroy(_utMutex);
	}
	_started = false;
	_commonCCInfo->cacheIsCorrupt = 0;

	if (_commonCCInfo->writeMutexEntryCount != 0) {
		omrthread_tls_free(_commonCCInfo->writeMutexEntryCount);
		_commonCCInfo->writeMutexEntryCount = 0;
	}
	
	Trc_SHR_CC_cleanup_Exit(currentThread);
}

/** 
 * Check whether a crash has been detected while updating the shared classes cache
 *
 * @param [in,out] localCrashCntr  Pointer to field containing expected crash counter value.  On return, this
 *        field will contain the current crash counter value.
 *
 * @return true if a JVM has crashed updating the cache header since localCrashCntr was updated, false otherwise
 */
bool
SH_CompositeCacheImpl::crashDetected(UDATA* localCrashCntr)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	if (*localCrashCntr != _theca->crashCntr) {
		*localCrashCntr = _theca->crashCntr;
		return true;
	}
	return false;
}

/**
 * Reset the composite cache
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 */
void
SH_CompositeCacheImpl::reset(J9VMThread* currentThread)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_reset_Entry(currentThread);

	findStart(currentThread);
	_oldUpdateCount = 0;
	_storedSegmentUsedBytes = 0;
	_storedMetaUsedBytes = 0;
	_storedAOTUsedBytes = 0;
	_storedJITUsedBytes = 0;
	_storedReadWriteUsedBytes = 0;
	_softmxUnstoredBytes = 0;
	_maxAOTUnstoredBytes = 0;
	_maxJITUnstoredBytes = 0;

	/* If cache is locked, unlock it */
	doUnlockCache(currentThread);

	Trc_SHR_CC_reset_Exit(currentThread);
}

/**
 * Test whether shared classes cache is corrupt
 *
 * @return true if cache is corrupt, false otherwise
 */
bool
SH_CompositeCacheImpl::isCacheCorrupt(void)
{
	SH_CompositeCacheImpl *ccToUse;
	
	/* If read-only cache discovers corruption, the local flag will be set */
	if (_commonCCInfo->cacheIsCorrupt == 1) {
		return true;
	}
	ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent) : _ccHead);
	return (0 != ccToUse->_theca->corruptFlag);
}

/**
 * Set shared classes cache as corrupted. It does not store corruption context.
 * This method is used when the cache is discovered to be corrupt,
 * and the corruption context has already been stored by earlier methods.
 * This method does not generate system dump, which should have been done when the corruption context was set in earlier methods.
 *
 * @see setCorruptCache(J9VMThread *, IDATA, UDATA)
 */
void
SH_CompositeCacheImpl::setCorruptCache(J9VMThread* currentThread)
{
	SH_CompositeCacheImpl *ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent) : _ccHead);

	Trc_SHR_CC_setCorruptCache_Entry();

	/* Always set the local flag because a read-only JVM will not be able to change the cache header */
	_commonCCInfo->cacheIsCorrupt = 1;

	if (ccToUse->_theca == NULL || _readOnlyOSCache) {
		Trc_SHR_CC_setCorruptCache_Exit();
		return;
	}

	if (_started) {
		ccToUse->unprotectHeaderReadWriteArea(currentThread, false);
	}

	getCorruptionContext(&ccToUse->_theca->corruptionCode, &ccToUse->_theca->corruptValue);

	if ( !((UnitTest::CORRUPT_CACHE_TEST == UnitTest::unitTest) && (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_STATS))) ) {
		/* This function may be called during shrtest with J9SHR_RUNTIMEFLAG_ENABLE_STATS. In this case
		 * if the cache is corrupt we skip marking the cache header, which allows the next test use the 
		 * same cache and find the same problems on its own.
		 */
		ccToUse->_theca->corruptFlag = 1;
	}
	if (_started) {
		ccToUse->protectHeaderReadWriteArea(currentThread, false);
	}
	Trc_SHR_CC_setCorruptCache_Exit();
}

/**
 * Store corruption context and call setCorruptCache(void) to set shared classes cache as corrupted.
 *
 * This method is used when the cache is discovered to be corrupt,
 * and we want to set the corruption context as well.
 * This method also triggers a system dump after setting corruption context.
 *
 * @see setCorruptCache(J9VMThread* currentThread)
 *
 * @param currentThread		the current thread
 * @param corruptionCode	code to indicate type of corruption
 * @param corruptValue		corrupt value in cache
 */
void
SH_CompositeCacheImpl::setCorruptCache(J9VMThread *currentThread, IDATA corruptionCode, UDATA corruptValue)
{
	J9JavaVM* vm = currentThread->javaVM;
	BOOLEAN runCorruptCacheHook = TRUE;
	SH_CompositeCacheImpl *ccToUse;

	Trc_SHR_CC_setCorruptCacheWithContext_Entry(corruptionCode, corruptValue, UnitTest::unitTest);

	ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent) : _ccHead);

	if (1 == ccToUse->_theca->corruptFlag) {
		Trc_SHR_CC_setCorruptCacheWithContext_AlreadyCorrupt();
		if (0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_FORCE_DUMP_IF_CORRUPT)) {
			runCorruptCacheHook = FALSE;
		}
	}

	if ((UnitTest::NO_TEST == UnitTest::unitTest) || (UnitTest::CORRUPT_CACHE_TEST == UnitTest::unitTest)) {
		/* skip this during unit testing of CompositeCache as _oscache is always NULL. */
		SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? _oscache : _parent->_oscache) : _ccHead->_oscache);
		oscacheToUse->setCorruptionContext(corruptionCode, corruptValue);
	}

	/* If the cache is about to be marked as corrupt, trigger hook to generate system dump
	 *
	 * Does not trigger with -Xshareclasses:disablecorruptcachedumps
	 */
	if ((TRUE == runCorruptCacheHook) && (0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_DISABLE_CORRUPT_CACHE_DUMPS))) {
		TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
	}

	setCorruptCache(currentThread);
	Trc_SHR_CC_setCorruptCacheWithContext_Exit();
}

/* Set the various boundaries on the cache areas to ensure that they meet with the requirements
 * Should only be called for a new cache 
 * Prereq: Cache header should be unprotected - this is only done during initialization 
 * Don't derive the piConfig from currentThread as we need to be able to pass it through in the unit tests */
void
SH_CompositeCacheImpl::setCacheAreaBoundaries(J9VMThread* currentThread, J9SharedClassPreinitConfig* piConfig)
{
	J9JavaVM* vm = currentThread->javaVM;
	U_32 finalReadWriteSize = 0;
	BlockPtr finalSegmentStart;
	U_32 numOfSharedNodes;
	U_32 maxSharedStringTableSize;
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	if (_readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_setCacheAreaBoundaries_Entry(currentThread);

	finalReadWriteSize = READWRITEAREASIZE(_theca);		/* Note that this does not include the size of the J9SharedCacheHeader */
	/**
	 * finalReadWriteSize can be 0 in two cases
	 * 1. If user do not pass an argument to define the size of shared string intern table. (i.e -Xitsn option)
	 * 		In this case, sharedClassReadWriteBytes is expected to be -1 and finalReadWriteSize is set to the proportional value of shared cache.
	 * 2. If user pass an argument to define the size of shared intern table is 0 then finalReadWriteSize is 0 and it is user intentional.
	 * 		In this case it is user's preference that finalReadWriteSize is 0.
	 */
	if ((finalReadWriteSize == 0) && (piConfig->sharedClassReadWriteBytes == -1)) {
		/* If no explicit value for sharedClassReadWriteBytes was set, set it to a proportion of the cache size */
		finalReadWriteSize = SHC_PAD((_theca->totalBytes / DEFAULT_READWRITE_BYTES_DIVISOR), SHC_WORDALIGN);
		maxSharedStringTableSize = srpHashTable_requiredMemorySize(SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT, sizeof(J9SharedInternSRPHashTableEntry), TRUE);
		if (maxSharedStringTableSize == PRIMENUMBERHELPER_OUTOFRANGE) {
			/*
			 * We should never be here.
			 * Because SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT value should be set with a number
			 * in the supported range of primeNumberHelper by developer.
			 */
			Trc_SHR_Assert_ShouldNeverHappen();
		}
		if (finalReadWriteSize > maxSharedStringTableSize) {
			finalReadWriteSize = maxSharedStringTableSize;
		}
		/*
		 * Calculate the exact amount of memory that is needed
		 * by SRP hashtable with a chosen table size from PrimeBitsArray of it depending on the given memory.
		 * This will prevent wasting any extra memory that is wasted by SRP hashtable.
		 */
		if (!(*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_ROUND_TO_PAGE_SIZE)) {
			numOfSharedNodes = srpHashTable_calculateTableSize(finalReadWriteSize, sizeof(J9SharedInternSRPHashTableEntry), FALSE);
			if (numOfSharedNodes == PRIMENUMBERHELPER_OUTOFRANGE) {
				/**
				 * This should never happen since finalReadWriteSize is limited up to maxSharedStringTableSize above.
				 * maxSharedStringTableSize is calculated above with the function srpHashTable_requiredMemorySize
				 * which returns PRIMENUMBERHELPER_OUTOFRANGE if SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT
				 * is not in the supported range of PrimeNumberHelper.
				 */
				Trc_SHR_Assert_ShouldNeverHappen();
			}
			finalReadWriteSize = srpHashTable_requiredMemorySize(numOfSharedNodes, sizeof(J9SharedInternSRPHashTableEntry), FALSE);
		}
	}


	/* Work out where the segment area will now start from */
	finalSegmentStart = (BlockPtr)SHC_PAD(((UDATA)((BlockPtr)_theca) + finalReadWriteSize + sizeof(J9SharedCacheHeader)), SHC_WORDALIGN);
	
	/* If we're rounding to page sizes, the segment area and cache end will need to be reset onto page boundaries */
	if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_ROUND_TO_PAGE_SIZE) {
		BlockPtr origCacheEnd = CAEND(_theca);
		BlockPtr newCacheEnd;

		Trc_SHR_CC_setCacheAreaBoundaries_Event_prePageRounding(currentThread, finalSegmentStart, origCacheEnd, _theca->totalBytes);

		/* Reset the segment start area onto a page boundary */
		finalSegmentStart = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)finalSegmentStart);
		
		/* Now, adjust the end of the cache so that it is on a page boundary */
		newCacheEnd = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)origCacheEnd);
		
		_theca->totalBytes -= (U_32)(origCacheEnd - newCacheEnd);
		_theca->roundedPagesFlag = 1;

		Trc_SHR_CC_setCacheAreaBoundaries_Event_postPageRounding(currentThread, finalSegmentStart, newCacheEnd, _theca->totalBytes);
	} else if (isVerbosePages() == true) {
		j9tty_printf(PORTLIB, "Page size rounding not supported\n");
	}

	_theca->osPageSize = _osPageSize;
	_theca->readWriteBytes = (U_32)(finalSegmentStart - (BlockPtr)_theca);		/* includes the cache header */
	_theca->updateSRP = (UDATA)_theca->totalBytes;
	_theca->segmentSRP = _theca->readWriteBytes;

	/**
	 * Currently the read write area of composite cache is only used by shared string intern table.
	 * This area starts from offset cacheMap->getStringTableBase() and size of this area is cacheMap->getReadWriteBytes()
	 * Size of string intern table can be defined by user,
	 * currently -Xitsn is the only option that allows user to set shared string table size.
	 * In such cases, required memory size for shared string intern table is set to the sharedClassPreinitConfig->sharedClassReadWriteBytes,
	 * default value is -1 which indicates that shared intern table will be generated in the default proportion of shared cache.
	 * If sharedClassPreinitConfig->sharedClassReadWriteBytes is not -1, we should only use this value for shared string intern table.
	 * Otherwise use all readWrite area to generate shared string intern table.
	 * ReadWrite area is optimized by page rounding up in most of the cases and
	 * if user requested specific size of shared string intern table,
	 * using all readWrite area for shared intern table might return bigger shared string intern table
	 * because of extra space gained by page rounding up.
	 * 
	 * Currently -Xitsn option is not restricted by SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT
	 *
	 */
	if (piConfig->sharedClassReadWriteBytes > -1) {
		/*
		 * ReadWrite area calculated depending on piConfig->sharedClassReadWriteBytes and
		 * it can never be smaller than piConfig->sharedClassReadWriteBytes codewise.
		 * Still we do assertion check here just to be sure.
		 */
		Trc_SHR_Assert_True(((IDATA)READWRITEAREASIZE(_theca)) >= piConfig->sharedClassReadWriteBytes);
		_theca->sharedInternTableBytes = piConfig->sharedClassReadWriteBytes;
	} else {
		_theca->sharedInternTableBytes = (IDATA)READWRITEAREASIZE(_theca);
	}

	/*
	 * Set up the 'debug' region of the cache header
	 */	
	if (NULL == this->_parent) {
#if !defined (J9SHR_CACHELET_SUPPORT)
		/* The size of the cache will be rounded down to a multiple of 'align' which normally the osPageSize.
		 * On some AIX boxes _osPageSize is zero, in this case we set align to be 4KB.
		 */
		U_32 align = 0x1000;
		U_32 debugsize = 0;
		U_32 freeBlockBytes = getFreeBlockBytes();

		if (0 != _osPageSize) {
			align = (U_32)_osPageSize;
		}
		
		/* If the cache was created with -Xnolinenumbers then the debug area is not created by default.
		 * If -Xscdmx is used then we will still reserve the debug area.
		 */
		if ((true == this->getIsNoLineNumberEnabled()) && (piConfig->sharedClassDebugAreaBytes == -1)) {
			Trc_SHR_CC_setCacheAreaBoundaries_Event_XnolinenumbersMeansNoDebugArea(currentThread);
			piConfig->sharedClassDebugAreaBytes = 0;
		}

		/* If -Xscdmx was used sharedClassDebugAreaBytes will be set to a positive value.*/
		if (piConfig->sharedClassDebugAreaBytes != -1) {
			debugsize = (U_32)piConfig->sharedClassDebugAreaBytes;
			if (debugsize < align) {
				debugsize = 0;
			} else {
				debugsize = ROUND_DOWN_TO(align, debugsize);
			}
		}
		
		/* If -Xscdmx is too large, or -Xscdmx was not used then use default size.*/
		if ( (piConfig->sharedClassDebugAreaBytes == -1) || (debugsize > freeBlockBytes) ) {
			U_32 newdebugsize = ClassDebugDataProvider::recommendedSize(freeBlockBytes, align);
			if (debugsize > (U_32)(FREEBYTES(_theca))) {
				CC_INFO_TRACE3(J9NLS_SHRC_SHRINIT_SCDMX_GRTHAN_SCMX, debugsize, FREEBYTES(_theca), newdebugsize);
			}
			debugsize = newdebugsize;
		}		
		
		Trc_SHR_CC_setCacheAreaBoundaries_Event_CreateDebugAreaSize(currentThread, debugsize);
		ClassDebugDataProvider::HeaderInit(_theca, debugsize);
		_theca->updateSRP -= (UDATA)debugsize;
#else
		ClassDebugDataProvider::HeaderInit(_theca, 0);
#endif /*J9SHR_CACHELET_SUPPORT*/
	}

	if ((U_32)-1 != _theca->softMaxBytes) {
		U_32 usedBytes = getUsedBytes();
		U_32 softMaxValue = _theca->softMaxBytes;

		if (softMaxValue < usedBytes) {
			/* softmax is infeasible which is smaller than the bytes already used, adjust it to the min feasible value */
			CC_WARNING_TRACE1(J9NLS_SHRC_CC_SOFTMAX_INFEASIBLE, usedBytes, false);
			Trc_SHR_CC_setCacheAreaBoundaries_infeasibleSoftMaxBytes(currentThread, softMaxValue, usedBytes);
			setSoftMaxBytes(currentThread, usedBytes);
		}
	}

	if ((*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_ROUND_TO_PAGE_SIZE) && (isVerbosePages() == true)) {
		j9tty_printf(PORTLIB, "New cache rounded to page size of %d bytes\n", _osPageSize);
		j9tty_printf(PORTLIB, "   CompositeCache header starts at %p\n", _theca);
		j9tty_printf(PORTLIB, "   ReadWrite area starts at %p and is %d bytes\n", READWRITEAREASTART(_theca), READWRITEAREASIZE(_theca));
		j9tty_printf(PORTLIB, "   ROMClass segment starts at %p\n", CASTART(_theca));
		j9tty_printf(PORTLIB, "   Debug Region starts at %p and is %d bytes\n", CADEBUGSTART(_theca), _theca->debugRegionSize);
		j9tty_printf(PORTLIB, "   Cache ends at %p\n", CAEND(_theca));
		j9tty_printf(PORTLIB, "   Cache soft max bytes is %d\n", (IDATA)_theca->softMaxBytes);
	}
	
	Trc_SHR_CC_setCacheAreaBoundaries_Exit(currentThread, finalReadWriteSize, _theca->readWriteBytes);
}

bool
SH_CompositeCacheImpl::isLocked(void)
{
	SH_CompositeCacheImpl *ccToUse;

	ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent) : _ccHead);

	return  (ccToUse->_theca->locked != 0);
}

void
SH_CompositeCacheImpl::setIsLocked(bool value)
{
	SH_CompositeCacheImpl *ccToUse;

	ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent) : _ccHead);
	
	ccToUse->_theca->locked = (U_32)value;
}

void
SH_CompositeCacheImpl::notifyPagesRead(BlockPtr start, BlockPtr end, UDATA expectedDirection, bool protect)
{
	/* If the cache is locked, the whole metadata area is unprotected for the duration of the lock and then reprotected when the lock is released
	 * therefore don't go through this function if we're in a locked state */
	if ((*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT) && !isLocked()) {
		UDATA actualDirection = (start < end) ? DIRECTION_FORWARD : DIRECTION_BACKWARD;
		bool doProtect = (actualDirection == expectedDirection) && protect;
		BlockPtr protectStart, protectEnd;

		if ((_osPageSize == 0) || _readOnlyOSCache) {
			 /*This will only execute when called from shrtest. During shrtest assertions are off.*/
			Trc_SHR_Assert_ShouldNeverHappen();
			return;
		}
		
		Trc_SHR_CC_notifyPagesRead_Entry(start, end, expectedDirection, actualDirection);

		if (actualDirection == DIRECTION_FORWARD) {
			if (expectedDirection == DIRECTION_FORWARD) {
				protectStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)start);
				protectEnd = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)end);
			} else {
				protectStart = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)start);
				protectEnd = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)end);
			}
		} else {
			if (expectedDirection == DIRECTION_BACKWARD) {
				protectStart = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)end);
				protectEnd = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)start);
			} else {
				protectStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)end);
				protectEnd = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)start);
			}
		}
		if (protectStart != protectEnd) {
			IDATA rc = setRegionPermissions(_portlib, (void*)protectStart, (protectEnd - protectStart), (doProtect ? J9PORT_PAGE_PROTECT_READ : (J9PORT_PAGE_PROTECT_WRITE | J9PORT_PAGE_PROTECT_READ)));
			PORT_ACCESS_FROM_PORT(_portlib);

			if (rc != 0) {
				I_32 myerror = j9error_last_error_number();
				Trc_SHR_CC_notifyPagesRead_setRegionPermissions_Failed(myerror);
				Trc_SHR_Assert_ShouldNeverHappen();
			}
			if (isVerbosePages() == true) {
				j9tty_printf(PORTLIB, "Set memory region permissions in notifyPagesRead for %p to %p - doProtect=%d - rc=%d\n", protectStart, protectEnd, doProtect, rc);
			}
		}

		Trc_SHR_CC_notifyPagesRead_Exit(protectStart, protectEnd, doProtect);
	}
}
	
void
SH_CompositeCacheImpl::notifyPagesCommitted(BlockPtr start, BlockPtr end, UDATA expectedDirection)
{
	Trc_SHR_CC_notifyPagesCommitted_Entry(start, end, expectedDirection);

#if defined (J9SHR_MSYNC_SUPPORT)
	if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MSYNC) {
		UDATA actualDirection = (start < end) ? DIRECTION_FORWARD : DIRECTION_BACKWARD;
		BlockPtr syncStart, syncEnd;
	
		if ((_osPageSize == 0) || _readOnlyOSCache) {
			Trc_SHR_Assert_ShouldNeverHappen();
			return;
		}

		if (actualDirection == DIRECTION_FORWARD) {
			syncStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)start);
			syncEnd = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)end);
		} else {
			syncStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)end);
			syncEnd = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)start);
		}
		if (syncStart != syncEnd) {
			_oscache->syncUpdates((void*)syncStart, (syncEnd - syncStart), (J9PORT_MMAP_SYNC_WAIT | J9PORT_MMAP_SYNC_INVALIDATE));
		}
	}
#endif
	notifyPagesRead(start, end, expectedDirection, true);

	Trc_SHR_CC_notifyPagesCommitted_Exit();
}

#if defined(J9SHR_CACHELET_SUPPORT)

IDATA 
SH_CompositeCacheImpl::startupNested(J9VMThread* currentThread)
{
	J9SharedClassPreinitConfig tempConfig;
	J9JavaVM* vm = currentThread->javaVM;
	U_32 actualSize;
	UDATA ignore;
	bool ignore2;

	memcpy(&tempConfig, vm->sharedClassPreinitConfig, sizeof(J9SharedClassPreinitConfig));
	tempConfig.sharedClassCacheSize = _nestedSize;
	tempConfig.sharedClassReadWriteBytes = 0;
	
	Trc_SHR_Assert_True(_parent->_commonCCInfo != NULL);
	this->_commonCCInfo = _parent->_commonCCInfo;
	return startup(currentThread, &tempConfig, _nestedMemory, _runtimeFlags, _verboseFlags, "A_CACHELET", "CACHELET_ROOT", J9SH_DIRPERM_ABSENT, &actualSize, &ignore, false, &ignore2);
}

IDATA 
SH_CompositeCacheImpl::startupChained(J9VMThread* currentThread, SH_CompositeCacheImpl* ccHead,
		J9SharedClassPreinitConfig* piconfig, U_32* actualSize, UDATA* localCrashCntr)
{
	bool ignore;
	J9JavaVM* vm = currentThread->javaVM;

	this->_ccHead = ccHead;
	Trc_SHR_Assert_True(_ccHead->_commonCCInfo != NULL);
	this->_commonCCInfo = _ccHead->_commonCCInfo;
	return startup(currentThread, piconfig, NULL, ccHead->_runtimeFlags, ccHead->_verboseFlags, "A_CHAINED_CACHE", _sharedClassConfig->ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, actualSize, localCrashCntr, false, &ignore);
}

#endif /* J9SHR_CACHELET_SUPPORT */

/**
 * Starts the SH_CompositeCache. 
 *
 * The method tries to connect to an existing shared cache of rootName and if one 
 * cannot be found, a new one is created
 * 
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * @param [in] sharedClassConfig
 * @param [in] config A pre-init config used for specifying values such as -Xscmx
 * @param [in] cacheMemory  Used in unit testing to avoid use of _oscache (when non-zero).  In normal system use, always specify zero.
 * @param [in] runtimeFlags_  Runtime flags
 * @param [in] verboseFlags_  Flags controlling the level of verbose messages to be issued
 * @param [in] rootName  Name of shared cache to connect to
 * @param [in] cacheDirName  Name of control file directory to be used (optional - typically NULL)
 * @param [in] cacheDirPerm	 Access permissions for ctrlDirName
 * @param [out] actualSize  Ptr to size in MB of new cache (if one is being created).
 *          If one exists, value is set to size of existing cache.
 * @param [out] localCrashCntr  Initializes local CacheMap value
 * @param [out] cacheHasIntegrity Set to true if the cache is new or has been crc integrity checked, false otherwise
 *
 * @return 0 for success, -1 for failure and -2 for corrupt.
 * @retval CC_STARTUP_OK (0) success
 * @retval CC_STARTUP_FAILED (-1)
 * @retval CC_STARTUP_CORRUPT (-2)
 * @retval CC_STARTUP_RESET (-3)
 * @retval CC_STARTUP_SOFT_RESET (-4)
 * @retval CC_STARTUP_NO_CACHELETS (-5)
 */
IDATA 
SH_CompositeCacheImpl::startup(J9VMThread* currentThread, J9SharedClassPreinitConfig* piconfig, BlockPtr cacheMemory,
		U_64* runtimeFlags, UDATA verboseFlags, const char* rootName, const char* ctrlDirName, UDATA cacheDirPerm, U_32* actualSize,
		UDATA* localCrashCntr, bool isFirstStart, bool *cacheHasIntegrity)
{
	IDATA rc = 0;
	const char* fnName = "CC startup";
	/* _commonCCInfo must be set before startup is called. All composite caches within 
	 * a JVM should point to the same _commonCCInfo structure.
	 */
	Trc_SHR_Assert_True(_commonCCInfo != NULL);
	SH_SharedCacheHeaderInit* headerInit = SH_SharedCacheHeaderInit::newInstance(_newHdrPtr);
	J9PortShcVersion versionData;
	I_32 openMode = 0;
	UDATA createFlags = J9SH_OSCACHE_CREATE;
	bool hasWriteMutex = SH_CompositeCacheImpl::hasWriteMutex(currentThread);
	bool hasReadWriteMutex = SH_CompositeCacheImpl::hasReadWriteMutex(currentThread);
	bool doReleaseWriteMutex = false;
	bool doReleaseReadWriteMutex = false;
	J9JavaVM* vm = currentThread->javaVM;
	PORT_ACCESS_FROM_PORT(_portlib);

	/* This function/method may be called indirectly from j9shr_init, return CC_STARTUP_OK if it is already started. */
	if (_started == true) {
		return CC_STARTUP_OK;
	}

	Trc_SHR_CC_startup_Entry1(currentThread, cacheMemory, rootName, ctrlDirName, UnitTest::unitTest);

	*cacheHasIntegrity = false;

	_cacheName = rootName;
	_runtimeFlags = runtimeFlags;
	_verboseFlags = verboseFlags;

	if (_parent == NULL) {
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS) {
			openMode |= J9OSCACHE_OPEN_MODE_GROUPACCESS;
		}
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_READONLY) {
			openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
		} else if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NONFATAL) {
			openMode |= J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL;
		}
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_AUTOKILL_DIFF_BUILDID) {
			/* If noAutoPunt or read-only is selected, don't check the buildID when opening the cache file */
			openMode |= J9OSCACHE_OPEN_MODE_CHECKBUILDID;
		}
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_DETECT_NETWORK_CACHE) {
			openMode |= J9OSCACHE_OPEN_MODE_CHECK_NETWORK_CACHE;
		}
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_STATS) {
			createFlags = J9SH_OSCACHE_OPEXIST_STATS;
		} else if (J9_ARE_ANY_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_SNAPSHOT | J9SHR_RUNTIMEFLAG_DO_NOT_CREATE_CACHE)) {
			createFlags = J9SH_OSCACHE_OPEXIST_DO_NOT_CREATE;
		}
	}
		
	if (0 != omrthread_monitor_init(&_headerProtectMutex, 0)) {
		Trc_SHR_CC_startup_Exit10(currentThread);
		return CC_STARTUP_FAILED;
	}
	
	if (0 != omrthread_monitor_init(&_runtimeFlagsProtectMutex, 0)) {
		Trc_SHR_CC_startup_Exit11(currentThread);
		return CC_STARTUP_FAILED;
	}
	
	if (isFirstStart) {
		_commonCCInfo->cacheIsCorrupt = 0;
		if (_osPageSize == 0) {
			/* If page size of the region is unknown, cannot mprotect or round to page sizes */
			*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_ENABLE_ROUND_TO_PAGE_SIZE;
		}
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_ROUND_TO_PAGE_SIZE) {
			/* If we're rounding to page sizes, we need at least 2 OS pages - one for the cache header and one for the data */
			if (piconfig->sharedClassCacheSize <= (_osPageSize * 2)) {
				piconfig->sharedClassCacheSize = (_osPageSize * 2);
			} else {
				piconfig->sharedClassCacheSize = ROUND_UP_TO(_osPageSize, piconfig->sharedClassCacheSize);
			}
		}
		if (piconfig->sharedClassCacheSize > MAX_CC_SIZE) {
			piconfig->sharedClassCacheSize = MAX_CC_SIZE;
		} else if (piconfig->sharedClassCacheSize < MIN_CC_SIZE) {
			piconfig->sharedClassCacheSize = MIN_CC_SIZE;
		}
	}

	*actualSize = 0;

	if (cacheMemory != NULL) {
		if (!(((J9SharedCacheHeader*)cacheMemory)->ccInitComplete & CC_STARTUP_COMPLETE)) {
			U_32 readWriteBytes = (U_32)((piconfig->sharedClassReadWriteBytes > 0) ? piconfig->sharedClassReadWriteBytes : 0);
			headerInit->init(cacheMemory, (U_32)piconfig->sharedClassCacheSize, (I_32)piconfig->sharedClassMinAOTSize, (I_32)piconfig->sharedClassMaxAOTSize,
					(I_32)piconfig->sharedClassMinJITSize, (I_32)piconfig->sharedClassMaxJITSize, readWriteBytes, (U_32)piconfig->sharedClassSoftMaxBytes);
		}
		if (_parent == NULL) {
			/* If parent is NULL and cache memory is provided, then we're unit testing */

			/* Set the readOnly state for testing that sets the J9SHR_RUNTIMEFLAG_ENABLE_READONLY flag */
			_readOnlyOSCache = (openMode & J9OSCACHE_OPEN_MODE_DO_READONLY) ? true : false;

			if (_readOnlyOSCache) {
				_commonCCInfo->writeMutexID = CC_READONLY_LOCK_VALUE;
				_commonCCInfo->readWriteAreaMutexID = CC_READONLY_LOCK_VALUE;
				if (isFirstStart) {
					if (_commonCCInfo->writeMutexEntryCount == 0) {
						IDATA rc = omrthread_tls_alloc(&(_commonCCInfo->writeMutexEntryCount));
						if (rc != 0) {
							Trc_SHR_CC_startup_ExitFailedAllocWriteMutexTLS(currentThread, rc);
							return CC_STARTUP_FAILED;
						}
						/* the initial value of the tls is 0 */
					}
				}
			}
			if (omrthread_monitor_init(&_utMutex, 0)) {
				return CC_STARTUP_FAILED;
			}
		}
	} else {
		IDATA lockID;
		bool OSCStarted = false;

		setCurrentCacheVersion(vm, J2SE_VERSION(currentThread->javaVM), &versionData);

		/* Note that OSCache startup does not leave any kind of lock on the cache, so the cache file could in theory
		 * be recreated by another process whenever we're not holding a lock on it. This can happen until attach() is called */

		OSCStarted = _oscache->startup(vm, ctrlDirName, cacheDirPerm, rootName, piconfig, ((_ccHead == NULL) ? SH_CompositeCacheImpl::getNumRequiredOSLocks() : 0), createFlags, _verboseFlags, *_runtimeFlags, openMode, vm->sharedCacheAPI->storageKeyTesting, &versionData, headerInit, SHR_STARTUP_REASON_NORMAL);

		if ((J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_TEST_BAD_BUILDID))
			&& (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_SNAPSHOT))
		) {
			/*J9SHR_RUNTIMEFLAG_ENABLE_TEST_BAD_BUILDID is for testing only. It is only
			 * set the first time OSCache is created. It is unset here instead of in
			 * OSCache b/c OSCache works with a copy of _runtimeFlags. However, when creating
			 * a cache snapshot with a bad build ID we do not unset it here since
			 * J9SHR_RUNTIMEFLAG_ENABLE_TEST_BAD_BUILDID will be used in j9shr_createCacheSnapshot()
			 */
			(*_runtimeFlags) &= ~(J9SHR_RUNTIMEFLAG_ENABLE_TEST_BAD_BUILDID);
		}

		_readOnlyOSCache = _oscache->isRunningReadOnly();

		if (OSCStarted == false) {
			if (_oscache->getError() == J9SH_OSCACHE_CORRUPT) {
				/* cache is corrupt, trigger hook to generate a system dump */
				if (0 ==(*_runtimeFlags & J9SHR_RUNTIMEFLAG_DISABLE_CORRUPT_CACHE_DUMPS)) {
					TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
				}
				/* no need to store corruption context as it has already been done during _oscache->startup(). */
				setCorruptCache(currentThread);
				Trc_SHR_CC_startup_Exit7(currentThread);
				return CC_STARTUP_CORRUPT;
			}
			/* _oscache returns failure - no point marking it corrupted as we are not able to attach to the cache */
			Trc_SHR_CC_startup_Exit2(currentThread);
			if (_oscache->getError() == J9SH_OSCACHE_NO_CACHE) {
				return CC_STARTUP_NO_CACHE;
			}
			return CC_STARTUP_FAILED;
		}

		if (_readOnlyOSCache) {
			_commonCCInfo->writeMutexID = CC_READONLY_LOCK_VALUE;
			_commonCCInfo->readWriteAreaMutexID = CC_READONLY_LOCK_VALUE;

			if (isFirstStart) {
				if (_commonCCInfo->writeMutexEntryCount == 0) {
					IDATA rc = omrthread_tls_alloc(&(_commonCCInfo->writeMutexEntryCount));
					if (rc != 0) {
						Trc_SHR_CC_startup_ExitFailedAllocWriteMutexTLS(currentThread, rc);
						return CC_STARTUP_FAILED;
					}
					/* the initial value of the tls is 0 */
				}
			}
		} else if (isFirstStart) {
			if ((lockID = _oscache->getWriteLockID()) >= 0) {
				_commonCCInfo->writeMutexID = (U_32)lockID;
			} else {
				Trc_SHR_CC_startup_Exit3(currentThread);
				return CC_STARTUP_FAILED;
			}
			if ((lockID = _oscache->getReadWriteLockID()) >= 0) {
				_commonCCInfo->readWriteAreaMutexID = (U_32)lockID;
			} else {
				Trc_SHR_CC_startup_Exit6(currentThread);
				return CC_STARTUP_FAILED;
			}
		}
	}
	if (!hasWriteMutex) {
		IDATA result;
		
		if (_parent == NULL) {
			result = enterWriteMutex(currentThread, false, fnName);
		} else {
			result = _parent->enterWriteMutex(currentThread, false, fnName);
		}
		
		if (result == 0) {
			hasWriteMutex = doReleaseWriteMutex = true;
		}
	}
	if (hasWriteMutex) {
		_oldUpdateCount = 0;
		if (cacheMemory != NULL) {
			_theca = (J9SharedCacheHeader*)cacheMemory;
		} else {
			_theca = (J9SharedCacheHeader*)_oscache->attach(currentThread, &versionData);
#ifndef J9SHR_CACHELET_SUPPORT
			/* Verify that a non-realtime VM is not attaching to a Realtime cache when printing stats */
			if ((_theca != 0) && getContainsCachelets() &&
					((J9SHR_RUNTIMEFLAG_ENABLE_STATS == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_STATS)) || isRunningReadOnly())) {
				Trc_SHR_CC_realtime_cache_not_compatible(currentThread);
				CC_ERR_TRACE(J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ERROR_REALTIME_CACHE_NOT_COMPATIBLE);
				rc = CC_STARTUP_FAILED;
				goto releaseLockCheck;
			}
#endif
		}
		if (_theca != 0) {
			UDATA crcValue;
			if (!_readOnlyOSCache && !(_theca->ccInitComplete & CC_INIT_COMPLETE)) {
				CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_ERROR, J9NLS_CACHE_BAD_CC_INIT, _theca->ccInitComplete);
				Trc_SHR_Bad_CC_Init_Value(currentThread, _theca, _theca->ccInitComplete);
				rc = CC_STARTUP_CORRUPT;
				setCorruptCache(currentThread, CACHE_BAD_CC_INIT, _theca->ccInitComplete);
				goto releaseLockCheck;
			}
			
			if (isCacheInitComplete() == true) {
				if (_theca->osPageSize != _osPageSize) {
					/* For some reason, the osPageSize supported by this platform is different to the one on which
				 	 * the cache was created. We cannot use the cache, so attempt to recreate it */
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_OSPAGE_SIZE_MISMATCH);
					Trc_SHR_CC_OSPAGE_SIZE_MISMATCH(currentThread, _theca, _theca->osPageSize, _osPageSize);
					rc = CC_STARTUP_RESET;
					goto releaseLockCheck;
				}
			}

#if defined(WIN32)
			if ((true == this->isMemProtectEnabled()) && (false == _readOnlyOSCache)) {
				/* CMVC 171136
				 * It appears on some windows systems there may be a bug in VirtualProtect.
				 * When an existing shared memory is being mapped into the JVM we mark all
				 * data already stored in the cache as 'read-only'.
				 *
				 * It seems pages adjacent to the protected regions are somehow effected
				 * if they have not been read (e.g. they are not part of the processes working
				 * set yet). Despite not being identifiably mprotected by functions
				 * like VirtualQuery accessing these adjacent pages results in a segv.
				 *
				 * Doing this call to setRegionPermissions() on the entire cache during 'startup'
				 * has the fortunate effect of avoiding this issue without paging in extra memory
				 * into the process.
				 * 
				 * Note: The first calls to mprotect the cache readonly are:
				 * 	- COLD CACHE: The call to 'SH_CompositeCacheImpl::notifyPagesRead()' below in this function
				 * 	- WARM CACHE: From j9shr_init when it calls 'SH_CacheMap::enterStringTableMutex()'
				 * The set to RW is done here to ensure we are well before both.
				 */
				UDATA pageSize = _osPageSize;
				UDATA cacheStart, mprotectSize;

				if (UnitTest::ATTACHED_DATA_TEST == UnitTest::unitTest) {
					cacheStart = ROUND_UP_TO(pageSize, (UDATA) UnitTest::cacheMemory);
					mprotectSize = ROUND_DOWN_TO(pageSize, UnitTest::cacheSize);
				} else {
					cacheStart = ROUND_UP_TO(pageSize, (UDATA)_oscache->getOSCacheStart());
					mprotectSize = ROUND_DOWN_TO(pageSize, (UDATA)_oscache->getTotalSize());
				}
				bool doVerbosePages = this->isVerbosePages();
				PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);

				if ( (mprotectSize != 0) && (_osPageSize > 0)) {
					if (this->setRegionPermissions(PORTLIB, (void *) cacheStart, mprotectSize, (J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE)) != 0) {
						I_32 myerror = j9error_last_error_number();
						Trc_SHR_CC_startup_setRegionPermissions_Failed(myerror);
						Trc_SHR_Assert_ShouldNeverHappen();
					} else {
						Trc_SHR_CC_startup_Init_UnprotectForWin32_Event(currentThread, (void *)cacheStart, (void *)(cacheStart + mprotectSize), mprotectSize);
						if (doVerbosePages) {
							j9tty_printf(PORTLIB, "Set initial memory region permissions in SH_CompositeCacheImpl::startup() for entire cache area from %p to %p - for %d bytes\n", (void *) cacheStart, (void *) (cacheStart + mprotectSize), mprotectSize);
						}
					}
				}
			}
#endif

			if (!isCacheCorrupt()) {
				/*If the cache is already marked corrupt there is no reason to check the crc*/
				if (!checkCacheCRC(cacheHasIntegrity, &crcValue)) {
					CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT, J9NLS_ERROR, J9NLS_SHRC_CC_STARTUP_CACHE_CRC_INVALID, crcValue);
					setCorruptCache(currentThread, CACHE_CRC_INVALID, crcValue);
				}
			} else {
				/* If the vm is run with -Xshareclasses:forceDumpIfCorrupt, then fire the 
				 * corrupt cache hook to generate a dump.
				 */
				if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_FORCE_DUMP_IF_CORRUPT) {
					TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
				}
			}
			if (!isCacheCorrupt()) {
				IDATA retryCntr = 0;
				
				/* If we're running read-only, we have no write mutex. Resolve this race by waiting for ccInitComplete flag */
				if (!_readOnlyOSCache) {
					/* The cache is about to undergo change, so mark the CRC as invalid */
					_theca->crcValid = 0;
				} else {
					while ((isCacheInitComplete() == false) && (retryCntr < J9SH_OSCACHE_READONLY_RETRY_COUNT)) {
						omrthread_sleep(J9SH_OSCACHE_READONLY_RETRY_SLEEP_MILLIS);
						++retryCntr;
					}
					if (isCacheInitComplete() == false) {
						Trc_SHR_CC_Startup_Cache_Initialization_Failure(currentThread);
						rc = CC_STARTUP_FAILED;
						goto releaseLockCheck;
					}
				}
				if ((_ccHead != NULL) && (isCacheInitComplete() == false)) {
					/* These fields in the chained cache header point to the head of the chain */
					WSRP_SET(_theca->updateCountPtr, &(_ccHead->_theca->updateCount));
					WSRP_SET(_theca->corruptFlagPtr, &(_ccHead->_theca->corruptFlag));
					WSRP_SET(_theca->lockedPtr, &(_ccHead->_theca->locked));
				}
				*actualSize = _theca->totalBytes;
				/* If the cache is new, then
				 * 	- set up the boundaries
				 *  - store information regarding -Xnolinenumbers
				 */
				if (isCacheInitComplete() == false) {
					_initializingNewCache = true;
					UDATA extraFlags = 0;
					
					Trc_SHR_CC_startup_Event_InitializingNewCache(currentThread);

					if (J9_ARE_NO_BITS_SET(currentThread->javaVM->requiredDebugAttributes, (J9VM_DEBUG_ATTRIBUTE_LINE_NUMBER_TABLE | J9VM_DEBUG_ATTRIBUTE_SOURCE_FILE))) {
						extraFlags |= J9SHR_EXTRA_FLAGS_NO_LINE_NUMBERS;
					}

					if (J9_ARE_ANY_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHERETRANSFORMED)) {
						if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_DISABLE_BCI)) {
							CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_OSCACHE_STARTUP_CACHERETRANSFORMED_IMPLIES_DISABLE_BCI);
						}
						*_runtimeFlags |= J9SHR_RUNTIMEFLAG_DISABLE_BCI;
					}

					if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_DISABLE_BCI)) {
						extraFlags |= J9SHR_EXTRA_FLAGS_BCI_ENABLED;
						*_runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_BCI;
					}

					if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES)) {
						/* Creating a new cache with "mprotect=partialpages" option */
						extraFlags |= J9SHR_EXTRA_FLAGS_MPROTECT_PARTIAL_PAGES;
						if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP)) {
							extraFlags |= J9SHR_EXTRA_FLAGS_MPROTECT_PARTIAL_PAGES_ON_STARTUP;
						}
					}

					if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_RESTRICT_CLASSPATHS)) {
						CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_OSCACHE_STARTUP_RESTRICT_CLASSPATHS_ENABLED);
						extraFlags |= J9SHR_EXTRA_FLAGS_RESTRICT_CLASSPATHS;
					}

					setCacheHeaderExtraFlags(currentThread, extraFlags);
					
					setCacheAreaBoundaries(currentThread, piconfig);

					_canStoreClasspaths = true;

				} else {
#if defined(J9SHR_CACHELET_SUPPORT)
					if (isFirstStart && (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED) && !getContainsCachelets()) {
						/* Trying to grow a nested cache which doesn't contain cachelets
						 */
						if (_oscache != NULL) {
							_oscache->destroy(true);
						}
						Trc_SHR_CC_Startup_No_Cachelets(currentThread, *_runtimeFlags, (I_32)getContainsCachelets());
						rc = CC_STARTUP_NO_CACHELETS;
						goto releaseLockCheck;
					}
#endif
					bool compatible = checkCacheCompatibility(currentThread);
					if (false == compatible) {
						rc = CC_STARTUP_FAILED;
						goto releaseLockCheck;
					}

					_canStoreClasspaths = (false == this->isRestrictClasspathsSet(currentThread)) || J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ALLOW_CLASSPATHS);
					if (_canStoreClasspaths) {
						if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ALLOW_CLASSPATHS)) {
							CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_OSCACHE_STARTUP_ALLOW_CLASSPATHS_ENABLED);
						} else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_RESTRICT_CLASSPATHS)) {
							CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_WARNING, J9NLS_SHRC_OSCACHE_STARTUP_RESTRICT_CLASSPATHS_IGNORED);
							/* Unset the runtime flag */
							*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_RESTRICT_CLASSPATHS;
						}
					} else {
						CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_OSCACHE_STARTUP_CANNOT_STORE_CLASSPATHS);
					}

					/* If cache is created with partialpage protection enabled then use the cache in that mode,
					 * and ignore any incompatible option like mprotect=nopartialpages or mprotect=none.
					 * Similarly, if the cache is created with mprotect=nopartialpages, then use the cache in that mode,
					 * and unset runtime flags for protecting partially filled pages.
					 */
					if (isMprotectPartialPagesSet(currentThread)) {
						if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES)) {
							CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_WARNING, J9NLS_SHRC_OSCACHE_STARTUP_MPROTECT_PARTIAL_PAGES_ENABLED);
						}
#if defined(J9ZOS390) || defined(AIXPPC)
						*runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES;
#else
						*_runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES;
#endif /* defined(J9ZOS390) || defined(AIXPPC) */
						if (isMprotectPartialPagesOnStartupSet(currentThread)) {
							if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP)) {
								*_runtimeFlags |= J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP;
								CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_WARNING, J9NLS_SHRC_OSCACHE_STARTUP_MPROTECT_PARTIAL_PAGES_ON_STARTUP_ENABLED);
							}
						} else {
							if (J9_ARE_ANY_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP)) {
								*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP;
								CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_OSCACHE_STARTUP_MPROTECT_PARTIAL_PAGES_NOT_ENABLED_ON_STARTUP);
							}
						}
					} else {
						if (J9_ARE_ANY_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ONFIND)) {
							/* Unset the runtime flags */
							*_runtimeFlags &= ~(J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ONFIND);
							*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP;
							CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_OSCACHE_STARTUP_MPROTECT_PARTIAL_PAGES_DISABLED);
						}
					}
				}

				_prevScan = _scan = (ShcItemHdr*)CCFIRSTENTRY(_theca);
				/* For unit testing, there may not be a sharedClassConfig */
				if (_sharedClassConfig && isFirstStart) {
					_sharedClassConfig->cacheDescriptorList->cacheStartAddress = _theca;
					/* TODO: The idea of having a single metadata segment is broken */
					_metadataSegmentPtr = &(_sharedClassConfig->metadataMemorySegment);
				}
				/* TODO: Maybe move _commonCCInfo->vmID into CacheMap */
				if (isFirstStart) {
					if (_readOnlyOSCache) {
						_commonCCInfo->vmID = _theca->vmCntr + 1;
						if (_commonCCInfo->vmID == 0) {
							/* Don't allow the _vmID to be zero if the vmCntr wraps */
							_commonCCInfo->vmID = 1;
						}
					} else {
						_commonCCInfo->vmID = ++(_theca->vmCntr);
						if (_commonCCInfo->vmID == 0) {
							/* Don't allow the _vmID to be zero if the vmCntr wraps */
							_commonCCInfo->vmID = ++(_theca->vmCntr);
						}
					}
				}

				*localCrashCntr = _theca->crashCntr;

				/* Unlock the cache if some JVM crashed while it has locked the cache in SH_CompositeCacheImpl::startCriticalUpdate(). */
				if (!_readOnlyOSCache && isLocked()) {
					Trc_SHR_CC_Startup_CacheIsLocked(currentThread, _theca->crashCntr);
					/* do not call doUnlockCache() as it tries to call unprotectHeaderReadWriteArea() which will assert on _started == false */
					setIsLocked(false);
				}

				if (READWRITEAREASIZE(_theca)) {
					_readWriteAreaStart = READWRITEAREASTART(_theca);
					_readWriteAreaBytes = READWRITEAREASIZE(_theca);
				} else {
					_readWriteAreaStart = NULL;
					_readWriteAreaBytes = 0;
				}
				if (isFirstStart && (!_theca->roundedPagesFlag || _readOnlyOSCache)) {
					/* If we don't have rounded pages or if we're running readonly, we can't do mprotect or msync */
#if defined (J9SHR_MSYNC_SUPPORT)
					*_runtimeFlags &= ~(J9SHR_RUNTIMEFLAG_ENABLE_MSYNC
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW
										| J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ONFIND);
#else
					*_runtimeFlags &= ~(J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW
										| J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES
										| J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ONFIND);
#endif 
					if (!_readOnlyOSCache) {
						CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_UNSUPPORTED);
					}
				}

#if defined (J9SHR_MSYNC_SUPPORT)
				_doHeaderSync = _doReadWriteSync = _doSegmentSync = _doMetaSync = (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MSYNC);
#endif
				if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT)) {
					_doSegmentProtect = !getContainsCachelets();
					_doMetaProtect = true;
				}
				_doHeaderProtect = J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL);
				_doHeaderReadWriteProtect = J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW);
				_doPartialPagesProtect = J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES);
				
				if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL)) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_ALLENABLED_INFO);
				}
#if !defined(J9ZOS390) && !defined(AIXPPC)
				else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES)) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_INFO1);
				} else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES)) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_INFO2);
				} else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_RW)) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_INFO3);
				}
#else
				else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT | J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_PARTIAL_PAGES)) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_INFO2);
				}
#endif /* !defined(J9ZOS390) && !defined(AIXPPC) */
				else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT)) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_INFO4);
				} else if (_theca->roundedPagesFlag) {
					CC_TRACE(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_CC_PAGE_PROTECTION_NONE_INFO);
				}
				
				/* Calculate the page boundaries for the CC header and readWrite areas, used for page protection.
				 * Note that on platforms with 1MB page boundaries, the header and readWrite areas will all be in one page */
				if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_ROUND_TO_PAGE_SIZE) {
					U_32 roundedHeaderAndRWSize = (U_32)ROUND_UP_TO(_osPageSize, ((UDATA)CASTART(_theca) - (UDATA)_theca));
					
					_cacheHeaderPageStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)_theca);
					_cacheHeaderPageBytes = (U_32)ROUND_UP_TO(_osPageSize, (UDATA)sizeof(J9SharedCacheHeader));
					if (_readWriteAreaStart != NULL) {
						_readWriteAreaPageStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)_readWriteAreaStart);
						_readWriteAreaPageBytes = (U_32)ROUND_UP_TO(_osPageSize, (UDATA)_readWriteAreaBytes);
						if (_readWriteAreaPageStart == _cacheHeaderPageStart) {
							if (roundedHeaderAndRWSize == _osPageSize) {
								/* Entire header and readWrite area is within the same page - treat as just header */ 
								_readWriteAreaPageStart = NULL;
								_readWriteAreaPageBytes = 0;
							} else {
								/* Header is smaller than a page, round up the start of the readWrite area to the next page
								 * to ensure that it can be protected/unprotected independently */
								_readWriteAreaPageStart += _osPageSize;
								_readWriteAreaPageBytes = (U_32)(CASTART(_theca) - _readWriteAreaPageStart);
							}
						}
					} else {
						_readWriteAreaPageStart = NULL;
						_readWriteAreaPageBytes = 0;
					}
					/* Double-check that the numbers are correct */
					if (((_cacheHeaderPageBytes + _readWriteAreaPageBytes) % _osPageSize) ||
							((_cacheHeaderPageBytes + _readWriteAreaPageBytes) != roundedHeaderAndRWSize)) {
						Trc_SHR_Assert_ShouldNeverHappen();
					}
				}

				/* CMVC 162844:
				 * For a non-readonly cache '_localReadWriteCrashCntr' must be set holding the string table lock.
				 * If the lock is not held it is possible that '_theca->readWriteCrashCntr' may be incremented by
				 * another JVM holding the lock.
				 * 
				 * Note: when running ./shrtest _oscache may be null, during composite cache testing.
				 */
				 if ((_parent == NULL) && (_oscache != NULL)) {
					if ((!hasReadWriteMutex) && (_commonCCInfo->readWriteAreaMutexID != CC_READONLY_LOCK_VALUE)) {
						if (_oscache->acquireWriteLock(_commonCCInfo->readWriteAreaMutexID) == 0) {
							hasReadWriteMutex = doReleaseReadWriteMutex = true;
						}
					}
					if ((hasReadWriteMutex) || (_commonCCInfo->readWriteAreaMutexID == CC_READONLY_LOCK_VALUE)) {
						_localReadWriteCrashCntr = _theca->readWriteCrashCntr;
	
						if (doReleaseReadWriteMutex) {
							if (_oscache->releaseWriteLock(_commonCCInfo->readWriteAreaMutexID) != 0) {
								rc = CC_STARTUP_FAILED;
							}
						}
					} else {
						/* Should only enter here when obtaining the readWriteArea lock failed: 
						 * 	((!hasReadWriteMutex) && (_commonCCInfo->readWriteAreaMutexID != CC_READONLY_LOCK_VALUE))
						 * 
						 * If entering 'readWriteAreaMutex' failed then it should be safe to 
						 * continue to use this cache without the string table. This will likely only
						 * occur with persistent caches using fcntl.
						 * 
						 * Because this JVM will never enter the string table lock, or reset the string table,
						 *  we can set _localReadWriteCrashCntr to any old value.
						 */
						_commonCCInfo->readWriteAreaMutexID = CC_READONLY_LOCK_VALUE;
						_localReadWriteCrashCntr = CC_COULD_NOT_ENTER_STRINGTABLE_ON_STARTUP;
					} 		
				}

				if (_doSegmentProtect && (SEGUPDATEPTR(_theca) != CASTART(_theca))) {
					/* If the cache has ROMClass data, notify that the area exists and will be read */
					notifyPagesRead(CASTART(_theca), SEGUPDATEPTR(_theca), DIRECTION_FORWARD, true);
				}
				/* Update romClassProtectEnd regardless of protect being enabled. This value is 
				 * used by shrtest which doesn't enable protection until after startup
				 */
				setRomClassProtectEnd(SEGUPDATEPTR(_theca));
				if (_initializingNewCache) {
					*cacheHasIntegrity = true;
					_theca->ccInitComplete |= CC_STARTUP_COMPLETE;
				}
				_maxAOT = _theca->maxAOT;
				_maxJIT = _theca->maxJIT;
			} else {
				rc = CC_STARTUP_CORRUPT;
			}
		} else {
			rc = CC_STARTUP_FAILED;
			if (_parent == NULL) {
				if (_oscache->getError() == J9SH_OSCACHE_CORRUPT) {
					Trc_SHR_CC_startup_Event6(currentThread);
					/* no need to store corruption context as it has already been done during _oscache->attach(). */
					setCorruptCache(currentThread);
					rc = CC_STARTUP_CORRUPT;
				} else if (_oscache->getError() == J9SH_OSCACHE_DIFF_BUILDID) {
					Trc_SHR_CC_startup_Event7(currentThread);
					if (J9_ARE_ANY_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_DO_NOT_CREATE_CACHE)) {
						/* do not reset the cache, but print out a error message about the buildID mismatch */
						CC_ERR_TRACE(J9NLS_SHRC_CC_INCORRECT_BUILDID);
					} else {
						rc = CC_STARTUP_SOFT_RESET;
					}
				}
			}
		}

		/* Each copy of composite cache needs an initialized '_debugData'.
		 * Nested caches use the same debugData area as there parent cache.
		 */
		if ((CC_STARTUP_OK == rc) && (hasWriteMutex)) {
			Trc_SHR_CC_startup_Event_DebugDataInit(currentThread);
			/* Note: if '_parent != NULL' then this is a nested cache and _debugData was 
			 * already initialized during _parent startup. Else _debugData needs to be initialized
			 */
			if (this->_parent == NULL) {
				if (_debugData->Init(currentThread, _theca, (AbstractMemoryPermission *)this, verboseFlags, runtimeFlags, false) == false) {
					setCorruptCache(currentThread, _debugData->getFailureReason(), _debugData->getFailureValue());
					rc = CC_STARTUP_CORRUPT;
				}
			}
		}

releaseLockCheck:
		if (doReleaseWriteMutex) {
			if (_parent == NULL) {
				if (exitWriteMutex(currentThread, fnName) != 0) {
					rc = (CC_STARTUP_OK == rc)? CC_STARTUP_FAILED: rc ;
				}
			} else {
				if (_parent->exitWriteMutex(currentThread, fnName) != 0) {
					rc = (CC_STARTUP_OK == rc)? CC_STARTUP_FAILED: rc ;
				}
			}
		}
	} else {
		rc = CC_STARTUP_FAILED;
	}
	if (rc == CC_STARTUP_OK) {
		_started = true;
		protectHeaderReadWriteArea(currentThread, false);
	}
	Trc_SHR_CC_startup_Exit5(currentThread, rc);
	return rc;
}

/**
 * Checks whether existing cache is compatible with options specified on command-line.
 * This is called by startup().
 *
 * @param [in] currentThread  current VM thread
 *
 * @return true if cache is compatible, else false.
 */
bool
SH_CompositeCacheImpl::checkCacheCompatibility(J9VMThread* currentThread)
{
	bool rc = true;
	PORT_ACCESS_FROM_PORT(_portlib);

	if (false == this->getIsBCIEnabled()) {

		/* Set the runtime flag to indicates support for BCI agent is disabled */
		*_runtimeFlags |= J9SHR_RUNTIMEFLAG_DISABLE_BCI;

		if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_BCI)) {
			/* Cache does not support BCI agent but -Xshareclasses:enableBCI is specified.
			 * If cache is opened for stats, then just clear the runtime flag for enableBCI,
			 * otherwise treat it as error.
			 */
			if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_STATS)) {
				*_runtimeFlags &= (~J9SHR_RUNTIMEFLAG_ENABLE_BCI);
			} else {
				Trc_SHR_CC_Startup_Cache_BCI_NotEnabled(currentThread);
				CC_ERR_TRACE(J9NLS_SHRC_OSCACHE_STARTUP_ERROR_BCI_AGENT_NOT_SUPPORTED);
				rc = false;
				goto _end;
			}
		}
	}

	if (true == this->getIsBCIEnabled()) {

		/* Set the runtime flag to indicates support for BCI agent is enabled */
		*_runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_BCI;

		if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_DISABLE_BCI)) {
			/* Cache supports BCI agent but -Xshareclasses:disableBCI is specified,
			 * If cache is opened for stats, then just clear runtime flag for disableBCI,
			 * otherwise treat it as error.
			 */
			if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_STATS)) {
				*_runtimeFlags &= (~J9SHR_RUNTIMEFLAG_DISABLE_BCI);
			} else {
				Trc_SHR_CC_Startup_Cache_DisableBCI_NotSupported(currentThread);
				CC_ERR_TRACE(J9NLS_SHRC_OSCACHE_STARTUP_ERROR_DISABLE_BCI_NOT_SUPPORTED);
				rc = false;
				goto _end;
			}
		}

		if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_CACHERETRANSFORMED)) {
			/* Cache supports BCI agent but -Xshareclasses:cacheRetransformed is specified,
			 * If cache is opened for stats, then just clear runtime flag for cacheRetransformed,
			 * otherwise treat it as error.
			 */
			if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_STATS)) {
				*_runtimeFlags &= (~J9SHR_RUNTIMEFLAG_ENABLE_CACHERETRANSFORMED);
			} else {
				Trc_SHR_CC_Startup_Cache_CacheRetransformOption_With_BCIEnabledCache(currentThread);
				CC_ERR_TRACE(J9NLS_SHRC_OSCACHE_STARTUP_ERROR_CACHERETRANSFORM_OPTION_WITH_BCI_ENABLED_CACHE_V1);
				rc = false;
				goto _end;
			}
		}
	}

_end:
	return rc;
}

/* THREADING: Should be protected by local mutex or cache mutex
 * Only called by nextEntry() which itself should be protected
*/
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::next(J9VMThread* currentThread)
{
	ShcItemHdr* result = NULL;
	ShcItemHdr** ih = &_scan;

	Trc_SHR_CC_next_Entry(currentThread, _scan);
	Trc_SHR_Assert_True((currentThread == _commonCCInfo->hasRefreshMutexThread) || hasWriteMutex(currentThread));
	
	/* THREADING: It is possible (but unlikely) that commitBytes() will be being called by another thread
	 * as this is being calculated. This is not a problem. nextEntry() in most cases
	 * is only called when it is known that there is data to read. It only matters that free is calculated correctly
	 * in the case where nextEntry() is being called when it is not known how much data there is to read.
	 * The latter case should only happen with a locked cache
	 */
	BlockPtr free = UPDATEPTR(_theca);
	UDATA maxCCItemLen = (((UDATA)(*ih)) - ((UDATA)free)) + sizeof(struct ShcItemHdr);

	/*
	 * If we have found an item, return it
	 */
	if ((BlockPtr)(*ih) > free) {
		if ((CCITEMLEN(*ih) <= 0) || (CCITEMLEN(*ih) > maxCCItemLen)) {
			/*This if will catch 2 types of problems, that may indicate a corrupted cache:
			 * 1.) CCITEMLEN(*ih) should never be <= 0. Without this clause, we can
			 *     end up looping by walking 0 bytes each time.
			 *
			 * 2.) CCITEMLEN(*ih) is larger that amount of meta remaining in this cache.
			 *     This indicates that the metadata has been corrupted.
			 */
			PORT_ACCESS_FROM_PORT(_portlib);
			CC_TRACE2(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT, J9NLS_ERROR, J9NLS_SHRC_CM_READ_CORRUPT_CACHE_ENTRY_HEADER_BAD_LEN, ih, CCITEMLEN(*ih));
			setCorruptCache(currentThread, ITEM_LENGTH_CORRUPT, (UDATA)ih);
		} else {
			_prevScan = _scan;
			result = *ih;
			*ih = CCITEMNEXT(*ih);
		}
		if (_doMetaProtect) {
			/* Note that _scan always points to the next available ShcItemHdr, 
			 * so the memory between _scan and (_scan + sizeof(ShcItemHdr)) can be modified */
			notifyPagesRead((BlockPtr)_prevScan, (BlockPtr)_scan + sizeof(ShcItemHdr), DIRECTION_BACKWARD, true);
		}
	}

	Trc_SHR_CC_next_Exit(currentThread, result, _scan);

	return (BlockPtr)result;
}

/**
 * Initialize ShcItem Block data.
 *
 * Simply initializes an "item" to the values set. Hides the _commonCCInfo->vmID in the dataType.
 *
 * @param [out] itemBuf  Pointer to ShcItem to be updated
 * @param [in] dataLen  Data length to be stored in item
 * @param [in] dataType  Data type to be stored in item
 */
void 
SH_CompositeCacheImpl::initBlockData(ShcItem** itemBuf, U_32 dataLen, U_16 dataType) 
{
	if (_readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_initBlockData_Entry(dataLen, dataType);

	(*itemBuf)->dataLen = dataLen;
	(*itemBuf)->dataType = dataType;
	(*itemBuf)->jvmID = _commonCCInfo->vmID;

	Trc_SHR_CC_initBlockData_Exit();
}

/**
 * Returns number of cache updates since last nextEntry() call.
 *
 * This is a snapshot of the cache. checkUpdates should be used in conjunction with doneReadUpdates.
 * The number of updates returned by checkUpdates should be passed to doneReadUpdates when the updates are read
 *
 * @return The number of cache updates since last nextEntry() call.
 */
UDATA 
SH_CompositeCacheImpl::checkUpdates(J9VMThread* currentThread)
{
	IDATA result;
	UDATA returnVal;
	UDATA* updateCountAddress = WSRP_GET(_theca->updateCountPtr, UDATA*);

	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	result = (*updateCountAddress - _oldUpdateCount);
	returnVal = (UDATA)((result < 0) ? 0 : result);		/* Ensure that checkUpdates cannot go negative */

	Trc_SHR_CC_checkUpdates_Event_result(result, returnVal);

	return returnVal; 
}

/**
 * Tells CompositeCache that updates returned by checkUpdates have been read.
 *
 * @param [in] updates  The number of updates actually read from the cache
 *
 * @pre The caller must hold the shared classes cache write mutex
 */
void 
SH_CompositeCacheImpl::doneReadUpdates(J9VMThread* currentThread, IDATA updates)
{
	UDATA* updateCountAddress = WSRP_GET(_theca->updateCountPtr, UDATA*);
	
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	if ((updates > 0) && (_oldUpdateCount < *updateCountAddress)) {
		BlockPtr newRomClassEndAddr = SEGUPDATEPTR(_theca);
		_oldUpdateCount += (I_32)updates;
		/*There could be updates to debug & raw class data areas if the updateCount has changed*/
		_debugData->processUpdates(currentThread, this);

		if (_doSegmentProtect) {
			this->notifyPagesRead(this->getRomClassProtectEnd(), newRomClassEndAddr, DIRECTION_FORWARD, true);
		}
		this->setRomClassProtectEnd(newRomClassEndAddr);

	}
	Trc_SHR_CC_doneReadUpdates_Event_result(updates, _oldUpdateCount);
}

/**
 * Reads the next entry in the cache (since an update).
 * 
 * @param [in] currentThread  The current thread
 * @param [in] staleItems  If staleItems is NULL, nextEntry returns all items, including stale. 
 *								Otherwise, it skips stale items but counts the number skipped.
 *
 * @return block found if one exists, NULL otherwise.
 *
 * @pre Local or cache mutex must be obtained to use this function
 */
SH_CompositeCacheImpl::BlockPtr 
SH_CompositeCacheImpl::nextEntry(J9VMThread* currentThread, UDATA* staleItems)
{
	BlockPtr result = 0;
	ShcItemHdr* ih;

	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	Trc_SHR_CC_nextEntry_Entry(currentThread);
	Trc_SHR_Assert_True((currentThread == _commonCCInfo->hasRefreshMutexThread) || hasWriteMutex(currentThread));

	ih = (ShcItemHdr*)next(currentThread);
	if (staleItems) {
		*staleItems = 0;
	}
	/* If staleItems is not NULL, skip over stale entries. Otherwise, return first entry found. */
	while (ih != 0 && (staleItems && CCITEMSTALE(ih))) {
		ih = (ShcItemHdr*)next(currentThread);
		if (staleItems) {
			++(*staleItems);
		}
	}
	if (ih != 0) {
		result = (BlockPtr)CCITEM(ih);
	}
	if (staleItems) {
		Trc_SHR_CC_nextEntry_Exit1(currentThread, result, *staleItems);
	} else {
		Trc_SHR_CC_nextEntry_Exit2(currentThread, result);
	}
	return result;
}

/**
 * Enter shared semaphore mutex to access the shared classes cache.
 *
 * Allows only single-threaded writing to the cache.
 * Write mutex allows multiple concurrent readers, unless lockCache
 * is set to true, in which case it blocks until all readers have finished.
 * 
 * @param [in] currentThread  Point to the J9VMThread struct for the current thread
 * @param [in] lockCache  Set to true if whole cache should be locked for this write
 * @param [in] caller String to identify the calling thread
 *
 * @return 0 if call succeeded and -1 for failure
 */
IDATA 
SH_CompositeCacheImpl::enterWriteMutex(J9VMThread* currentThread, bool lockCache, const char* caller)
{
	IDATA rc;
	SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache); 
	const char *fname = "enterWriteMutex";

	Trc_SHR_CC_enterWriteMutex_Enter(currentThread, lockCache, caller);
	
	if (_commonCCInfo->writeMutexID == CC_READONLY_LOCK_VALUE) {
		omrthread_t self = omrthread_self();
		IDATA entryCount;

		entryCount = (IDATA)omrthread_tls_get(self, _commonCCInfo->writeMutexEntryCount);
		omrthread_tls_set(self, _commonCCInfo->writeMutexEntryCount, (void*)(entryCount + 1));
		Trc_SHR_CC_enterWriteMutex_ExitReadOnly(currentThread);
		return 0;
	}

	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasWriteMutexThread);
	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasReadWriteMutexThread);
	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasRefreshMutexThread);

	if (oscacheToUse) {
		rc = oscacheToUse->acquireWriteLock(_commonCCInfo->writeMutexID);
	} else {
		rc = omrthread_monitor_enter(_utMutex);
	}
	if (rc == 0) {
		_commonCCInfo->hasWriteMutexThread = currentThread;
		if (*_runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES) {
			/*Pass doDecWriteCounter=false b/c exitWriteMutex is being called without updating writerCount*/
			exitWriteMutex(currentThread, fname, false);
			rc = -1;
		} else {
			if (lockCache) {
				doLockCache(currentThread);
			}
		}
	}
	if ((UnitTest::unitTest != UnitTest::COMPOSITE_CACHE_TEST_SKIP_WRITE_COUNTER_UPDATE)
			&& (true == _started) && (0 == rc)) {
		/*This function may be called before _theca is set (e.g SH_CompositeCacheImpl::startup)*/
		unprotectHeaderReadWriteArea(currentThread, false);

		this->_commonCCInfo->oldWriterCount = _theca->writerCount;
		_theca->writerCount += 1;
		protectHeaderReadWriteArea(currentThread, false);
	}
	if (rc == -1) {
		Trc_SHR_CC_enterWriteMutex_ExitError(currentThread, lockCache, caller, rc);
	} else {
		Trc_SHR_CC_enterWriteMutex_Exit(currentThread, lockCache, caller, rc);
	}
	return rc;
}

/**
 * Exit shared semaphore mutex
 *
 * @param [in] currentThread  Point to the J9VMThread struct for the current thread
 * @param [in] caller String to identify the calling thread
 *
 * @return 0 if call succeeded and -1 for failure
 */
IDATA
SH_CompositeCacheImpl::exitWriteMutex(J9VMThread* currentThread, const char* caller, bool doDecWriteCounter)
{
	IDATA rc = 0;
	SH_OSCache* oscacheToUse = ((NULL == _ccHead ) ? _oscache : _ccHead->_oscache);

	Trc_SHR_CC_exitWriteMutex_Enter(currentThread, caller);

	if (CC_READONLY_LOCK_VALUE == _commonCCInfo->writeMutexID ) {
		omrthread_t self = omrthread_self();
		IDATA entryCount;

		entryCount = (IDATA)omrthread_tls_get(self, _commonCCInfo->writeMutexEntryCount);
		Trc_SHR_Assert_True(entryCount > 0);

		omrthread_tls_set(self, _commonCCInfo->writeMutexEntryCount, (void*)(entryCount - 1));
		Trc_SHR_CC_exitWriteMutex_ExitReadOnly(currentThread);
		return 0;
	}

	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);
	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasReadWriteMutexThread);
	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasRefreshMutexThread);

	if ((UnitTest::unitTest != UnitTest::COMPOSITE_CACHE_TEST_SKIP_WRITE_COUNTER_UPDATE)
			&& (true == doDecWriteCounter) && (true == _started)) {
		/*This function may be called before _theca is set (e.g SH_CompositeCacheImpl::startup)*/
		unprotectHeaderReadWriteArea(currentThread, false);

		_theca->writerCount -= 1;
		protectHeaderReadWriteArea(currentThread, false);
		Trc_SHR_Assert_True(this->_commonCCInfo->oldWriterCount == _theca->writerCount);
	}

	doUnlockCache(currentThread);
	_commonCCInfo->hasWriteMutexThread = NULL;
	if (oscacheToUse) {
		rc = oscacheToUse->releaseWriteLock(_commonCCInfo->writeMutexID);
	} else {
		rc = omrthread_monitor_exit(_utMutex);
	}
	if (0 != rc ) {
		PORT_ACCESS_FROM_PORT(_portlib);
		CC_ERR_TRACE1(J9NLS_SHRC_CC_FAILED_EXIT_MUTEX, rc);
	}
	Trc_SHR_CC_exitWriteMutex_Exit(currentThread, caller, rc);
	return rc;
}

/**
 * Lock shared classes cache
 *
 * In the case where caller already has the write mutex, but wishes
 * to lock the cache, call this function.
 *
 * @pre The calling thread MUST hold the write mutex
 */
void
SH_CompositeCacheImpl::doLockCache(J9VMThread* currentThread)
{
	UDATA patienceCntr = 0;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_doLockCache_Entry(currentThread);
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);

	unprotectHeaderReadWriteArea(currentThread, false);
	setIsLocked(true);
	/* The metadata is changing and so mark the CRC as invalid. */
	/* TODO: This will not work for cachelets - need to reorganised how the CRCing is done */
	_theca->crcValid = 0;
	protectHeaderReadWriteArea(currentThread, false);
	while ((patienceCntr < CACHE_LOCK_PATIENCE_COUNTER) && (_theca->readerCount>0)) {
		omrthread_sleep(5);
		++patienceCntr;
	}
	if ((CACHE_LOCK_PATIENCE_COUNTER == patienceCntr) 
		&& (_theca->readerCount > 0)
	) {
		/* Reader has almost certainly died. Cannot wait forever. Whack to zero and proceed. */
		Trc_SHR_CC_doLockCache_EventWhackedToZero(currentThread);
		unprotectHeaderReadWriteArea(currentThread, false);
		_theca->readerCount = 0;
		protectHeaderReadWriteArea(currentThread, false);
	}

	unprotectMetadataArea();			/* When the cache is locked, the whole metadata area is unprotected for changes to occur */
	Trc_SHR_CC_doLockCache_Exit(currentThread);
}

/**
 * Unlock shared classes cache
 *
 * This method is used to unlock cache.
 *
 * @pre The calling thread MUST hold the write mutex
 */
void
SH_CompositeCacheImpl::doUnlockCache(J9VMThread* currentThread)
{
	/* This can be called before the cache has _started */
	if (_readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_doUnlockCache_Entry(currentThread);
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);

	/* test for _theca because if cache fails to initialize, this will be called from exitWriteMutex */
	if (_theca && isLocked()) {
		protectMetadataArea(currentThread);			/* When the cache is unlocked, the whole metadata area is re-protected */
		unprotectHeaderReadWriteArea(currentThread, false);
		setIsLocked(false);
		protectHeaderReadWriteArea(currentThread, false);
	}
	Trc_SHR_CC_doUnlockCache_Exit(currentThread);
}

/**
 * Check whether to use writeHash when loading classes
 *
 * This method helps answer the question: Is it worth us using writeHash when loading classes? 
 * The answer is: Yes if another VM has connected since us or if a VM previous to us has detected us 
 * and started writeHashing
 * 
 * Current thread should hold refresh mutex when entering this method.
 *
 * @param [in] currentThread  Points to the J9VMThread struct for the current thread
 * 
 * @return true if this VM should use writeHash while loading classes, false otherwise
 */
bool
SH_CompositeCacheImpl::peekForWriteHash(J9VMThread *currentThread)
{
	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}

	Trc_SHR_Assert_True(currentThread == _commonCCInfo->hasRefreshMutexThread);

	_useWriteHash = ((_commonCCInfo->vmID < _theca->vmCntr) || _theca->writeHash);
	return _useWriteHash;
}

/**
 * Test whether a class is being loaded by another JVM.  If not, update to indicate this JVM will load it.
 *
 * Should be called if an attempt to find a class in the cache has failed.
 * If the hash field in the cache field is free (0), it is set, indicating that the JVM is going to load the class.
 * If the hash field has already been set, then this means that another JVM is loading the class.
 * If it is the same class, 1 is returned which indicates that it would be wise to wait. Otherwise, 0 is returned.
 * 
 * @param [in] hashValue  Hash value of the name of the class which could not be found
 *
 * @return 1 if the JVM should wait, 0 otherwise.
 */
/* THREADING: This function can be called multi-threaded */
UDATA
SH_CompositeCacheImpl::testAndSetWriteHash(J9VMThread *currentThread, UDATA hashValue)
{
	UDATA cacheValue, maskedHash;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	cacheValue = _theca->writeHash;
	Trc_SHR_CC_testAndSetWriteHash_Enter(_commonCCInfo->vmID, hashValue, cacheValue, _theca->writeHash);

	maskedHash = hashValue & WRITEHASH_MASK;
	if (cacheValue==0) {
		/* There is a potential timing hole here... at this point, cacheValue could have changed.
		 * If it has, well boo-hoo. We over-write it with our value instead.
		 * Using a single UDATA cannot possibly be foolproof or always correct, rather it is a hint.
		 */
		setWriteHash(currentThread, hashValue);
	} else
	if (maskedHash == (cacheValue & WRITEHASH_MASK)) {
		UDATA vmInCache = (cacheValue >> WRITEHASH_SHIFT);

		if (vmInCache != _commonCCInfo->vmID) {
			Trc_SHR_CC_testAndSetWriteHash_Exit1(_commonCCInfo->vmID, vmInCache, _theca->writeHash);
			return 1;
		}
	}

	Trc_SHR_CC_testAndSetWriteHash_Exit2(_commonCCInfo->vmID, _theca->writeHash);
	return 0;
}

/**
 * Reset hash value, if class is as expected
 *
 * Once a class has been loaded from disk, this function is called. If the class being loaded is the
 * one that was originally registered in the field, the field is reset (regardless of the JVM ID).
 * The field is also reset if it has remained unchanged for too long.
 * 
 * @param [in]  hashValue  Hash value of the name of the class which has just been loaded
 *
 * @return 1 if the field was reset, 0 otherwise.
 */
/* THREADING: Function can be called multi-threaded */
UDATA
SH_CompositeCacheImpl::tryResetWriteHash(J9VMThread *currentThread, UDATA hashValue)
{
	UDATA cacheValue, maskedHash;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	cacheValue = _theca->writeHash;
	Trc_SHR_CC_tryResetWriteHash_Entry(_commonCCInfo->vmID, hashValue, cacheValue, _theca->writeHash);

	maskedHash = hashValue & WRITEHASH_MASK;
	/* If we have completed the load OR if the writeHash has remained unchanged for too long, reset it */
	if ((maskedHash == (cacheValue & WRITEHASH_MASK)) || (_lastFailedWHCount > FAILED_WRITEHASH_MAX_COUNT)) {
		setWriteHash(currentThread, 0);
		_lastFailedWHCount = 0;
		_lastFailedWriteHash = 0;
		Trc_SHR_CC_tryResetWriteHash_Exit1(_commonCCInfo->vmID, maskedHash, _theca->writeHash);
		return 1;
	}

	/* If a FIND is called for a class which could not be found on disk, no STORE is called and the writeHash field therefore
	 * does not get reset. To mitigate against this, count the number of times the writeHash value has remained unchanged. 
	 * Thread safety of the _lastFailedWHCount is not important as it does not have to be exact 
	 */
	if (cacheValue != 0) {
		if (_lastFailedWriteHash == cacheValue) {
			++_lastFailedWHCount;
		} else {
			_lastFailedWriteHash = cacheValue;
			_lastFailedWHCount = 0;
		}
	}

	Trc_SHR_CC_tryResetWriteHash_Exit2(_commonCCInfo->vmID, _theca->writeHash);
	return 0;
}

/**
 * Explicitly set the writeHash field to a value
 * 
 * @param [in] hashValue  Hash value of the name of the class which has just been loaded
 */
void
SH_CompositeCacheImpl::setWriteHash(J9VMThread *currentThread, UDATA hashValue)
{
	UDATA oldNum, value, compareSwapResult;

	if (!_started) {
		return;
	}
	if (_readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	oldNum = _theca->writeHash;
	Trc_SHR_CC_setWriteHash_Entry(_commonCCInfo->vmID, hashValue, oldNum, _theca->writeHash);

	value = 0;
	if (hashValue!=0) {
		value = ((hashValue & WRITEHASH_MASK) | (_commonCCInfo->vmID << WRITEHASH_SHIFT));
	}
	unprotectHeaderReadWriteArea(currentThread, false);
	compareSwapResult = VM_AtomicSupport::lockCompareExchange(&(_theca->writeHash), oldNum, value);
	protectHeaderReadWriteArea(currentThread, false);

	Trc_SHR_CC_setWriteHash_Exit(_commonCCInfo->vmID, oldNum, value, compareSwapResult, _theca->writeHash);
}

void
SH_CompositeCacheImpl::incReaderCount(J9VMThread* currentThread)
{
	UDATA oldNum, value;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}

	oldNum = _theca->readerCount;
	Trc_SHR_CC_incReaderCount_Entry(oldNum);

	value = 0;
	unprotectHeaderReadWriteArea(currentThread, false);

	do {
		value = oldNum + 1;
		oldNum = VM_AtomicSupport::lockCompareExchange((UDATA*)&(_theca->readerCount), oldNum, value);
	} while ((UDATA)value != (oldNum + 1));

	protectHeaderReadWriteArea(currentThread, false);

	Trc_SHR_CC_incReaderCount_Exit(_theca->readerCount);
}

void
SH_CompositeCacheImpl::decReaderCount(J9VMThread* currentThread)
{
	UDATA oldNum, value;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}

	oldNum = _theca->readerCount;
	Trc_SHR_CC_decReaderCount_Entry(oldNum);

	value = 0;
	unprotectHeaderReadWriteArea(currentThread, false);

	do {
		if (0 == oldNum) {
			/* This can happen if _theca->readerCount is whacked to 0 by doLockCache() */
			PORT_ACCESS_FROM_PORT(_portlib);
			CC_ERR_TRACE(J9NLS_SHRC_CC_NEGATIVE_READER_COUNT);
			break;
		}
		value = oldNum - 1;
		oldNum = VM_AtomicSupport::lockCompareExchange((UDATA*)&(_theca->readerCount), oldNum, value);
	} while ((UDATA)value != (oldNum - 1));

	protectHeaderReadWriteArea(currentThread, false);

	Trc_SHR_CC_decReaderCount_Exit(_theca->readerCount);
}

/**
 * Enter mutex to read data from the cache.
 *
 * This will only block if the cache is locked.
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 * @param [in] caller  A string representing the caller of this method
 *
 * @return 0 if call succeeded and -1 for failure
 */
IDATA
SH_CompositeCacheImpl::enterReadMutex(J9VMThread* currentThread, const char* caller)
{
	IDATA rc = 0;

	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return -1;
	}

	Trc_SHR_CC_enterReadMutex_Enter(currentThread, caller);

	if (_commonCCInfo->writeMutexID == CC_READONLY_LOCK_VALUE) {
		IDATA cntr = 0;

		/* If we don't have a write mutex, sleep until the cache is unlocked */
		++_readOnlyReaderCount;			/* Maintained so that our assertions still work */
		while (isLocked() && (cntr < (CC_MAX_READONLY_WAIT_FOR_CACHE_LOCK_MILLIS/10))) {
			omrthread_sleep(10);
			++cntr;
		}
		Trc_SHR_CC_enterReadMutex_ExitReadOnly(currentThread);
		return 0;
	}

	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasWriteMutexThread);

	/* THREADING: Important to increment readerCount before checking isLocked(), as the incremented
	 * reader count prevents a lock from occurring.
	 */
	incReaderCount(currentThread);

	if (isLocked()) {
		/* If cache is locked, wait on mutex which will be owned by thread which called for the lock */
		SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache);

		/* Decrement the count before waiting for the write lock. */
		decReaderCount(currentThread);

		Trc_SHR_CC_enterReadMutex_WaitOnGlobalMutex(currentThread, caller);
		if (oscacheToUse) {
			rc = oscacheToUse->acquireWriteLock(_commonCCInfo->writeMutexID);
		} else {
			rc = omrthread_monitor_enter(_utMutex);
		}
		if (rc == 0) {
			/* THREADING: Important to increment readerCount before exiting mutex, otherwise writer could
				get a cache lock and think that all readers have finished */
			incReaderCount(currentThread);

			/* Once lock is released, fall through and exit mutex */
			Trc_SHR_CC_enterReadMutex_ReleasingGlobalMutex(currentThread, caller);
			if (oscacheToUse) {
				rc = oscacheToUse->releaseWriteLock(_commonCCInfo->writeMutexID);
			} else {
				rc = omrthread_monitor_exit(_utMutex);
			}
			if (rc != 0) {
				PORT_ACCESS_FROM_PORT(_portlib);
				CC_ERR_TRACE1(J9NLS_SHRC_CC_FAILED_EXIT_MUTEX, rc);
			}
		}
	}
	Trc_SHR_CC_enterReadMutex_Exit(currentThread, caller, rc);
	return rc;
}

/**
 * Exit read mutex
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 * @param [in] caller  A string representing the caller of this method
 */
void
SH_CompositeCacheImpl::exitReadMutex(J9VMThread* currentThread, const char* caller)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_exitReadMutex_Enter(currentThread, caller);

	if (_commonCCInfo->writeMutexID == CC_READONLY_LOCK_VALUE) {
		--_readOnlyReaderCount;			/* Maintained so that our assertions still work */
		Trc_SHR_CC_exitReadMutex_ExitReadOnly(currentThread);
		return;
	}

	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasWriteMutexThread);

	decReaderCount(currentThread);
	Trc_SHR_CC_exitReadMutex_Exit(currentThread, caller);
}

/**
 * Delete the shared classes cache
 *
 * @todo FIXME: this is not as spec'd. It should not disrupt other VMs.
 * @return 0 for success, -1 for failure
 * @pre The cache mutex must be held by the calling thread held.
 * @post The cache mutex is  released and deleted.
 */
IDATA
SH_CompositeCacheImpl::deleteCache(J9VMThread *currentThread, bool suppressVerbose)
{
	Trc_SHR_CC_deleteCache_Entry();
	IDATA returnVal = -1;

	if (_oscache) {
		if (_started) {
			/* unprotect the header as call to destroy() tries to update the header when detaching the cache */
			unprotectHeaderReadWriteArea(currentThread, false);
		}
		returnVal = _oscache->destroy(suppressVerbose);
		if ((-1 == returnVal) && (_started)) {
			protectHeaderReadWriteArea(currentThread, false);
		}
	}

	Trc_SHR_CC_deleteCache_Exit2(returnVal);
	return returnVal;
}

/**
 * Called when performing critical section which, if does not complete, will
 * cause the cache to be corrupted.
 * Calling this function causes the cache header to become unprotected until endCriticalUpdate is called
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 */
void
SH_CompositeCacheImpl::startCriticalUpdate(J9VMThread *currentThread)
{
	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	unprotectHeaderReadWriteArea(currentThread, false);
	_theca->crashCntr++;
	Trc_SHR_CC_startCriticalUpdate_Event(_theca->crashCntr);
}

/**
 * Called when ending critical section update
 * Calling this function causes the cache header to be re-protected
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 */
void
SH_CompositeCacheImpl::endCriticalUpdate(J9VMThread *currentThread)
{
	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	_theca->crashCntr--;
	protectHeaderReadWriteArea(currentThread, false);
	Trc_SHR_CC_endCriticalUpdate_Event(_theca->crashCntr);
}

/**
 * Allocate a block of memory in the shared classes cache.
 *
 * Allocates a block of memory in the cache. Memory is of len size
 * and memory is allocated backwards from end of the cache.
 * 
 * @param [in] currentThread  The current thread
 * @param [in] itemToWrite  ShcItem struct describing the data, including the length which will be written as a header to the data
 * @param [in] align Bytes alignment for the start of the data
 * @param [in] alignOffset Offset from the start of the data of the address to align
 *
 * @return Address of item header allocated, NULL if cache is full
 */
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateBlock(J9VMThread* currentThread, ShcItem* itemToWrite, U_32 align, U_32 alignOffset)
{
	return allocate(currentThread, ALLOCATE_TYPE_BLOCK, itemToWrite, 0, 0, NULL, NULL, align, alignOffset);
}

/**
 * Allocate a block of AOT memory in the shared classes cache
 *
 * Allocates a block of memory in the cache. Memory is of len total size
 * and memory is allocated backwards from end of the cache. 
 * The dataLength should be the length of the code data, for reporting purposes.
 * 
 * @param [in] currentThread  The current thread
 * @param [in] itemToWrite  ShcItem struct describing the data, including the length which will be written as a header to the data
 * @param [in] dataBytes Size of code data
 *
 * @return Address of item header allocated, NULL if cache is full
 */
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateAOT(J9VMThread* currentThread, ShcItem* itemToWrite, U_32 dataBytes)
{
	return allocate(currentThread, ALLOCATE_TYPE_AOT, itemToWrite, dataBytes, 0, NULL, NULL, SHC_WORDALIGN, 0);
}

/**
 * Allocate a block of JIT memory in the shared classes cache
 *
 * Allocates a block of memory in the cache. Memory is of len total size
 * and memory is allocated backwards from end of the cache.
 * The dataLength should be the length of the profile data, for reporting purposes.
 *
 * @param [in] currentThread  The current thread
 * @param [in] itemToWrite  ShcItem struct describing the data, including the length which will be written as a header to the data
 * @param [in] dataBytes Size of profile data
 *
 * @return Address of item header allocated, NULL if cache is full
 */
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateJIT(J9VMThread* currentThread, ShcItem* itemToWrite, U_32 dataBytes)
{
	return allocate(currentThread, ALLOCATE_TYPE_JIT, itemToWrite, dataBytes, 0, NULL, NULL, SHC_DOUBLEALIGN, 0);
}

/**
 * Allocate a block of memory in the shared classes cache with a segment.
 *
 * Allocates a block of memory in the cache. Memory is of len size
 * and memory is allocated backwards from end of the cache.
 * Providing a segmentBufferSize will cause a segment block to
 * also be allocated, which is returned in placeholder segmentBuffer.
 * Segment blocks are allocated sequentially from the start of the cache.
 * 
 * @param [in] currentThread  The current thread
 * @param [in] itemToWrite  ShcItem struct describing the data, including the length which will be written as a header to the data
 * @param [in] segBufSize  Size of segment buffer to allocate
 * @param [out] segBuf  	Address of segment buffer allocated
 *
 * @return Address of item header allocated, NULL if cache is full
 */
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateWithSegment(J9VMThread* currentThread, ShcItem* itemToWrite, U_32 segmentBufferSize, BlockPtr* segmentBuffer)
{
	return allocate(currentThread, ALLOCATE_TYPE_BLOCK, itemToWrite, 0, segmentBufferSize, segmentBuffer, NULL, SHC_WORDALIGN, 0);
}

/**
 * Allocate a block of memory in the shared classes cache with a a readWrite area.
 *
 * Allocates a block of memory in the cache. Memory is of len size
 * and memory is allocated backwards from end of the cache.
 * Providing a readWriteBufferSize will cause a block in the readWrite area to
 * also be allocated, which is returned in placeholder readWriteBuffer.
 * ReadWrite blocks are allocated sequentially in the readWrite area.
 * 
 * @param [in] currentThread  The current thread
 * @param [in] itemToWrite  ShcItem struct describing the data, including the length which will be written as a header to the data
 * @param [in] segBufSize  Size of readWrite buffer to allocate
 * @param [out] readWriteBuffer  Address of readWrite buffer allocated
 *
 * @return Address of item header allocated, NULL if cache is full
 */
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateWithReadWriteBlock(J9VMThread* currentThread, ShcItem* itemToWrite, U_32 readWriteBufferSize, BlockPtr* readWriteBuffer)
{
	return allocate(currentThread, ALLOCATE_TYPE_BLOCK, itemToWrite, 0, readWriteBufferSize, NULL, readWriteBuffer, SHC_WORDALIGN, 0);
}

U_32
SH_CompositeCacheImpl::getBytesRequiredForItemWithAlign(ShcItem* itemToWrite, U_32 align, U_32 alignOffset)
{
	U_32 itemLen, padAmount;
	BlockPtr allocPtr;

	allocPtr = UPDATEPTR(_theca);
	/* Round length to align (must be even) because the lower bit is used to indicate staleness */
	itemLen = itemToWrite->dataLen + sizeof(ShcItemHdr) + sizeof(ShcItem);
	if ((padAmount = (U_32)(((UDATA)(allocPtr - itemLen) + alignOffset) % align))) {
		itemLen += padAmount;
	}
	return itemLen;
}

U_32
SH_CompositeCacheImpl::getBytesRequiredForItem(ShcItem* itemToWrite)
{
	return getBytesRequiredForItemWithAlign(itemToWrite, SHC_WORDALIGN, 0);
}

SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocate(J9VMThread* currentThread, U_8 type, ShcItem* itemToWrite, U_32 codeDataLen, U_32 separateBufferSize,
		BlockPtr* segmentBuffer, BlockPtr* readWriteBuffer, U_32 align, U_32 alignOffset)
{
	BlockPtr result = 0;
	U_32 itemLen;
	I_32 freeBytes = 0;
	bool enoughSpace = false;
	bool enoughAvailableSpace = false;
	U_32 usedBytesInc = 0;
	U_32 usedBytes = 0;
	U_32 softMaxValue = 0;
	U_32 addBufferSize = (NULL == readWriteBuffer) ? separateBufferSize : 0;
	UDATA regionFullFlag = 0;
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	/* Allocate readWrite data from the parent */
	SH_CompositeCacheImpl *parent = _parent == NULL ? this : _parent;
#endif

	if (!_started || _readOnlyOSCache || (itemToWrite == NULL) || ((I_32)itemToWrite->dataLen < 0)) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	Trc_SHR_CC_allocate_Entry(currentThread, type, itemToWrite->dataLen, codeDataLen, separateBufferSize);
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);

	if (isCacheCorrupt()) {
		Trc_SHR_CC_allocate_Exit_CacheCorrupt(currentThread);
		return NULL;
	}

	/* Round length to align (must be even) because the lower bit is used to indicate staleness */
	if (itemToWrite->dataLen > 0) {
		itemLen = getBytesRequiredForItemWithAlign(itemToWrite, align, alignOffset);
	} else {
		/* It is possible for the metadata length to be zero if we're allocating unindexed byte data into the segment area */
		itemLen = 0;
	}

	Trc_SHR_Assert_False(_storedSegmentUsedBytes | _storedReadWriteUsedBytes | _storedMetaUsedBytes | _storedAOTUsedBytes | _storedJITUsedBytes);

	if (segmentBuffer) {
		*segmentBuffer = NULL;
	}
	if (readWriteBuffer) {
		*readWriteBuffer = NULL;
	}
	if (type == ALLOCATE_TYPE_BLOCK) {
		freeBytes = getFreeBlockBytes();
		/* _debugData->getStoredDebugDataBytes() is the debug bytes that have been allocated but not yet committed.
		 * It should also be counted as bytes to be used.
		 */
		usedBytesInc = itemLen + addBufferSize + _debugData->getStoredDebugDataBytes();
	} else if (type == ALLOCATE_TYPE_AOT) {
		U_32 reservedAOT = getAvailableReservedAOTBytes(currentThread);

		freeBytes = (I_32)getFreeAOTBytes(currentThread);

		if (reservedAOT >= (_theca->aotBytes + codeDataLen)) {
			/* Reserved AOT has already been counted as used bytes. If it is not reached, codeDataLen should not be counted as used bytes */
			usedBytesInc = itemLen + addBufferSize - codeDataLen;
		} else if ((reservedAOT > _theca->aotBytes) && (reservedAOT < _theca->aotBytes + codeDataLen)) {
			usedBytesInc = (U_32)_theca->aotBytes + itemLen + addBufferSize - reservedAOT;
		} else {
			/* reservedAOT <= _theca->aotBytes */
			usedBytesInc = itemLen + addBufferSize;
		}
		regionFullFlag |= J9SHR_AOT_SPACE_FULL;
	} else if (type == ALLOCATE_TYPE_JIT) {
		U_32 reservedJIT = getAvailableReservedJITBytes(currentThread);

		freeBytes = (I_32)getFreeJITBytes(currentThread);
		if (reservedJIT >= (_theca->jitBytes + codeDataLen)) {
			/* Reserved JIT has already been counted as used bytes. If it is not reached, codeDataLen should not be counted as used bytes */
			usedBytesInc = itemLen + addBufferSize - codeDataLen;
		} else if ((reservedJIT > _theca->jitBytes) && (reservedJIT < _theca->jitBytes + codeDataLen)) {
			usedBytesInc = (U_32)_theca->jitBytes + itemLen + addBufferSize - reservedJIT;
		} else {
			/* reservedAOT <= _theca->jitBytes */
			usedBytesInc = itemLen + addBufferSize;
		}
		regionFullFlag |= J9SHR_JIT_SPACE_FULL;
	}
	if (!readWriteBuffer) {
		/* Ensure there is always a 1K gap between the classes and metadata.
		 * If modifying this code, see also CC_MIN_SPACE_BEFORE_CACHE_FULL. 
		 */
		if (freeBytes > J9SHR_MIN_GAP_BEFORE_METADATA) {
			freeBytes -= J9SHR_MIN_GAP_BEFORE_METADATA;
		} else {
			freeBytes = 0;
		}
		enoughSpace = (freeBytes >= (I_32)(itemLen + separateBufferSize));
	} else {
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
		enoughSpace = ((freeBytes >= (I_32)itemLen) && (parent->getFreeReadWriteBytes() >= (I_32)separateBufferSize));
#else
		enoughSpace = ((freeBytes >= (I_32)itemLen) && ((I_32)FREEREADWRITEBYTES(_theca) >= (I_32)separateBufferSize));
#endif
	}
	usedBytes = getUsedBytes();
	softMaxValue = _theca->softMaxBytes;
	enoughAvailableSpace = enoughSpace && ((usedBytes + usedBytesInc) <= softMaxValue);

	if (enoughAvailableSpace) {
		if (itemLen > 0) {
			if (type == ALLOCATE_TYPE_AOT) {
				_storedAOTUsedBytes = codeDataLen;
				_storedMetaUsedBytes = itemLen - codeDataLen;	/* Don't count AOT data as metadata */
			} else if (type == ALLOCATE_TYPE_JIT) {
				_storedJITUsedBytes = codeDataLen;
				_storedMetaUsedBytes = itemLen - codeDataLen;	/* Don't count JIT data as metadata */
			} else {
				_storedMetaUsedBytes = itemLen;
			}
			result = allocateMetadataEntry(currentThread, UPDATEPTR(_theca), itemToWrite, itemLen);
		} else {
			_storedMetaUsedBytes = 0;
		}
		if (separateBufferSize > 0) {
			if (segmentBuffer) {
				Trc_SHR_Assert_True((_storedMetaUsedBytes > 0) || (itemToWrite->dataType == TYPE_CACHELET));
				_storedSegmentUsedBytes = separateBufferSize;
				*segmentBuffer = SEGUPDATEPTR(_theca);

				/* Mark the last page as read-write where data is to be written */
				this->changePartialPageProtection(currentThread, *segmentBuffer, false);

				Trc_SHR_CC_allocate_EventSegmentBufSet(currentThread, *segmentBuffer);
			}
			if (readWriteBuffer) {
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
				*readWriteBuffer = parent->allocateReadWrite(separateBufferSize);
#if defined(J9SHR_CACHELET_SUPPORT)
				if (parent != this) {
					_commitParent = true;
				}
#endif
#else
				_storedReadWriteUsedBytes = separateBufferSize;
				*readWriteBuffer = RWUPDATEPTR(_theca);
#endif
				Trc_SHR_CC_allocate_EventReadWriteBufSet(currentThread, *readWriteBuffer);
			}
		}
	} else if (enoughSpace) {
		/* Cache space is enough, available space is not enough */
		SH_CompositeCacheImpl *ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent->_ccHead) : _ccHead);

		Trc_SHR_CC_allocate_softMaxBytesReached(currentThread, softMaxValue);
		if (ALLOCATE_TYPE_BLOCK == type) {
			/* Similar to the behaviour when setting J9SHR_BLOCK_SPACE_FULL,
			 * if available bytes < CC_MIN_SPACE_BEFORE_CACHE_FULL, J9SHR_AVAILABLE_SPACE_FULL is set in the previous commit. J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL is checked in 
			 * higher level APIs in SH_CacheMap. If it is set, higher level APIs will return directly and we would not reach here. 
			 * if available bytes >= CC_MIN_SPACE_BEFORE_CACHE_FULL, do not set J9SHR_AVAILABLE_SPACE_FULL here as it is above the threshold.
			 */
			Trc_SHR_Assert_True((softMaxValue - usedBytes) >= CC_MIN_SPACE_BEFORE_CACHE_FULL);
			/* If allocating block data, we would reach here only when allocating something that requires more than CC_MIN_SPACE_BEFORE_CACHE_FULL bytes. As free available space
		 	 * is still > CC_MIN_SPACE_BEFORE_CACHE_FULL so J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL is not set.
		 	 */
			increaseUnstoredBytes(usedBytesInc, 0, 0);
		} else {
			ccToUse->setCacheHeaderFullFlags(currentThread, regionFullFlag, true);
		}
	} else {
		/* enoughSpace is false, cache space is not enough or maxAOT/maxJIT is reached */
		UDATA flags = 0;
		SH_CompositeCacheImpl *ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent->_ccHead) : _ccHead);

		if (ALLOCATE_TYPE_AOT == type) {
			Trc_SHR_Assert_True((0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED)));
			flags |= J9SHR_AOT_SPACE_FULL;
		} else if (ALLOCATE_TYPE_JIT == type) {
			Trc_SHR_Assert_True((0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED)));
			flags |= J9SHR_JIT_SPACE_FULL;
		} else {
			if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED)) {
				/* For realtime cache, allocation failure can happen only when trying to allocate new cachelet. */
				Trc_SHR_Assert_Equals(itemToWrite->dataType, TYPE_CACHELET);
				flags |= J9SHR_BLOCK_SPACE_FULL;
			} else {
				I_32 freeBlockBytes = getFreeBlockBytes();
				/* Allocation request for BLOCK data can fail if
				 * 		requested amount > freeBlockBytes >= CC_MIN_SPACE_BEFORE_CACHE_FULL, or
				 * 		requested amount > freeBlockBytes < (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE).
				 * In first case we don't want to set J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL as free block bytes is above threshold.
				 * In second case we don't need to set J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL as it would have been done in previous commit.
				 */
				if (freeBlockBytes < (I_32) (J9SHR_MIN_GAP_BEFORE_METADATA + J9SHR_MIN_DUMMY_DATA_SIZE)) {
					Trc_SHR_Assert_True(J9SHR_BLOCK_SPACE_FULL == (J9SHR_BLOCK_SPACE_FULL & _theca->cacheFullFlags));
				} else {
					Trc_SHR_Assert_True(freeBlockBytes >= (I_32) CC_MIN_SPACE_BEFORE_CACHE_FULL);
				}
			}
		}
		ccToUse->setCacheHeaderFullFlags(currentThread, flags, true);
	}

	Trc_SHR_CC_allocate_Exit2(currentThread, result, _scan, _storedMetaUsedBytes, _storedSegmentUsedBytes, _storedReadWriteUsedBytes, _storedAOTUsedBytes,  _storedJITUsedBytes);
	return result;
}

/*
 * Allocate an entry in metadata area of the cache.
 *
 * @param [in] currentThread  The current thread
 * @param [in] allocPtr  Pointer to next entry in metadata area
 * @param [in] itemToWrite	Metadata item to be written
 * @param [in] itemLen	Length of metadata entry. It includes size of ShcItem, ShcItemHdr, actual data length, and padding.
 *
 * @return pointer to allocated metadata item.
 */
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateMetadataEntry(J9VMThread* currentThread, BlockPtr allocPtr, ShcItem *itemToWrite, U_32 itemLen)
{
	BlockPtr result = NULL;
	ShcItemHdr* ih = (ShcItemHdr*)(allocPtr - sizeof(ShcItemHdr));
	Trc_SHR_CC_allocateMetadataEntry_EventNewItem(currentThread, ih);

	/* Mark the last page as read-write where data is to be written */
	this->changePartialPageProtection(currentThread, allocPtr, false);

	if (0 != _osPageSize) {
		/* If we are close to filling up the cache, then metadata to be added may spill to the page pointed by segmentSRP,
		 * which may not have been marked as read-write in allocate() (for instance when adding AOT/JIT data).
		 * So mark the page pointed by segmentSRP as read-write here.
		 */
		BlockPtr itemStart = allocPtr - itemLen;
		BlockPtr itemStartPage = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)itemStart);
		BlockPtr segmentPtrPage = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)SEGUPDATEPTR(_theca));
		if (itemStartPage == segmentPtrPage) {
			this->changePartialPageProtection(currentThread, SEGUPDATEPTR(_theca), false);
		}
	}
	
	CCSETITEMLEN(ih, itemLen);
	result = (BlockPtr)CCITEM(ih);
	itemToWrite->dataLen = (itemLen - sizeof(ShcItemHdr));
	memcpy(result, itemToWrite, sizeof(ShcItem));
	_storedScan = _scan;
	_storedPrevScan = _prevScan;
	_prevScan = _scan;
	_scan = CCITEMNEXT(ih);

	return result;
}

/*
 * This method fills up cache with dummy data.
 * Dummy data added is a sequence of 0xD9 byte.
 * Cache is filled only if (available bytes - (AOT + JIT reserved bytes)) < CC_MIN_SPACE_BEFORE_CACHE_FULL.
 * Dummy data is added such that a gap of J9SHR_MIN_GAP_BEFORE_METADATA bytes is maintained between classes and metadata.
 * After adding the dummy data, it sets the runtime flags to mark the cache full.
 *
 * Note: This method is not called for realtime cache. If ever it needs to be called for realtime cache,
 * it would have to be modified to call setRuntimeCacheFullFlags() on first super cache.
 *
 * @param [in] currentThread  The current thread
 *
 * @return void
 */
void
SH_CompositeCacheImpl::fillCacheIfNearlyFull(J9VMThread* currentThread)
{
	I_32 freeBlockBytes = getFreeBlockBytes();
	U_32 usedBytes = getUsedBytes();
	U_32 softMaxValue = _theca->softMaxBytes;
	UDATA cacheFullFlags = 0;

	Trc_SHR_CC_fillCacheIfNearlyFull_Entry1(currentThread, freeBlockBytes, usedBytes, softMaxValue, CC_MIN_SPACE_BEFORE_CACHE_FULL);

	Trc_SHR_Assert_True(usedBytes <= softMaxValue);

	if (freeBlockBytes < (I_32) CC_MIN_SPACE_BEFORE_CACHE_FULL) {
		cacheFullFlags = J9SHR_BLOCK_SPACE_FULL | J9SHR_AVAILABLE_SPACE_FULL;
	
		if (freeBlockBytes > (I_32) J9SHR_MIN_GAP_BEFORE_METADATA) {
			freeBlockBytes -= J9SHR_MIN_GAP_BEFORE_METADATA;
		} else {
			freeBlockBytes = 0;
		}

		if (freeBlockBytes >= (I_32) J9SHR_MIN_DUMMY_DATA_SIZE) {
			ShcItem item;
			ShcItem *itemPtr = &item;
			U_32 itemLen = freeBlockBytes;
			BlockPtr allocPtr = UPDATEPTR(_theca);
			ShcItem *result = NULL;
			U_32 padAmount;

			padAmount = (U_32)((UDATA)(allocPtr - itemLen) % SHC_WORDALIGN);
			if (padAmount > 0) {
				itemLen -= (SHC_WORDALIGN - padAmount);
			}

			initBlockData(&itemPtr, itemLen, TYPE_UNINDEXED_BYTE_DATA);

			result = (ShcItem *)allocateMetadataEntry(currentThread, allocPtr, itemPtr, itemLen);

			memset(ITEMDATA(result), J9SHR_DUMMY_DATA_BYTE, ITEMDATALEN(result));

			_storedMetaUsedBytes += itemLen;

			Trc_SHR_CC_fillCacheIfNearlyFull_CacheFullDummyDataAdded(currentThread, result, _scan, itemLen);

			/* Calling commitUpdate() here will lead to unnecessary recursive call to fillCacheIfNearlyFull().
			 * To avoid that, directly call commitUpdateHelper().
			 */
			commitUpdateHelper(currentThread, false);

		} else {
			/* We don't have enough space to add dummy data */
			Trc_SHR_CC_fillCacheIfNearlyFull_CacheFullDummyDataNotAdded(currentThread);
		}
	} else if ((softMaxValue - usedBytes) < CC_MIN_SPACE_BEFORE_CACHE_FULL) {
		cacheFullFlags = J9SHR_AVAILABLE_SPACE_FULL;
	}

	if (0 != cacheFullFlags) {
		if ((J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL))
			&& (0 == getAvailableReservedAOTBytes(currentThread))
		) {
			cacheFullFlags |= J9SHR_AOT_SPACE_FULL;
		}

		if ((J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL))
			&& (0 == getAvailableReservedJITBytes(currentThread))
		) {
			cacheFullFlags |= J9SHR_JIT_SPACE_FULL;
		}

		setCacheHeaderFullFlags(currentThread, cacheFullFlags, true);
	}

	Trc_SHR_CC_fillCacheIfNearlyFull_Exit(currentThread);
	return;
}

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::allocateReadWrite(U_32 separateBufferSize)
{
	_storedReadWriteUsedBytes = (UDATA)separateBufferSize;
	return RWUPDATEPTR(_theca);
}
#endif

/**
 * Rollback shared classes cache update.
 * 
 * Called after an allocate and before a commit if the update is to be discarded.
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 */
void
SH_CompositeCacheImpl::rollbackUpdate(J9VMThread* currentThread)
{
	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);
	Trc_SHR_CC_rollbackUpdate_Event2(currentThread, _scan, _storedMetaUsedBytes, _storedSegmentUsedBytes, _storedReadWriteUsedBytes, _storedAOTUsedBytes,  _storedJITUsedBytes);

	_storedMetaUsedBytes = _storedSegmentUsedBytes = _storedAOTUsedBytes = _storedJITUsedBytes = _storedReadWriteUsedBytes = 0;
	_prevScan = _storedPrevScan;
	_scan = _storedScan;

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
#if defined(J9SHR_CACHELET_SUPPORT)
	if (_commitParent) {
		_parent->rollbackUpdate(currentThread);
		_commitParent = false;
	}
#endif
#endif
}

/**
 * Update value of _storedSegmentUsedBytes
 *
 * Called before ::commitUpdate() if ROM Classes builder allocated more than it needed.
 *
 * @param [in] usedBytes  new value
 *
 * @pre hold cache write mutex
 */

void
SH_CompositeCacheImpl::updateStoredSegmentUsedBytes(U_32 usedBytes) {
	Trc_SHR_Assert_True(_storedMetaUsedBytes > 0);
	_storedSegmentUsedBytes = usedBytes;
}

/**
 * Helper method to commit changes to shared class cache.
 * Called by commitUpdate() to update cache header counters
 * to inform other JVMs of the update.
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 * @param [in] isCachelet true when committing a cachelet
 *
 * @pre The caller MUST hold the shared classes cache write mutex
 */
void
SH_CompositeCacheImpl::commitUpdateHelper(J9VMThread* currentThread, bool isCachelet)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA oldNum = 0;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_commitUpdate_Entry2(currentThread, _scan, _storedMetaUsedBytes, _storedSegmentUsedBytes, _storedReadWriteUsedBytes, _storedAOTUsedBytes, _storedJITUsedBytes);
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);

	startCriticalUpdate(currentThread);		/* Note that this un-protects the header */

	Trc_SHR_CC_CRASH2_commitUpdate_EnteredCritical(currentThread);

	/* The cache is changing and so mark the CRC as invalid. */
	/* TODO: This will not work for cachelets - need to reorganised how the CRCing is done */
	_theca->crcValid = 0;

	if (_storedSegmentUsedBytes) {
		BlockPtr startAddress = SEGUPDATEPTR(_theca);

		Trc_SHR_Assert_True((_storedMetaUsedBytes > 0) || isCachelet);

		oldNum = _theca->segmentSRP;
		_theca->segmentSRP += _storedSegmentUsedBytes;

		Trc_SHR_CC_CRASH3_commitUpdate_Event1(currentThread, oldNum, _theca->segmentSRP);

		/* mprotect must be done after committing the update, otherwise the memory could be
		 * protected although the update doesn't complete because the vm exits before the commit
		 * can occur. On at least zOS, the mprotected memory persists across vm invocations.
		 */
		if (_doSegmentProtect
#if defined (J9SHR_MSYNC_SUPPORT)
				|| _doSegmentSync
#endif
		) {
			if ((J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP) || (J9VM_PHASE_NOT_STARTUP == vm->phase)) 
				&& (true == _doPartialPagesProtect)
			) {
				/* Add 1 page size so that last partially filled page also gets protected,
				 * but if we are already on page boundary then no need to add extra page size.
				 */
				if ((UDATA)(startAddress + _storedSegmentUsedBytes) % _osPageSize) {
					notifyPagesCommitted(startAddress, startAddress + _storedSegmentUsedBytes + _osPageSize, DIRECTION_FORWARD);
				} else {
					notifyPagesCommitted(startAddress, startAddress + _storedSegmentUsedBytes, DIRECTION_FORWARD);
				}
			} else {
				notifyPagesCommitted(startAddress, startAddress + _storedSegmentUsedBytes, DIRECTION_FORWARD);
			}
		}
		setRomClassProtectEnd(startAddress + _storedSegmentUsedBytes);
	}
	if (_storedReadWriteUsedBytes) {
		/* Don't currently notify read or commit of readWrite bytes */
		_theca->readWriteSRP += _storedReadWriteUsedBytes;
	}

	/* If we crash here, romclass/readwrite will exist without any reference to it. 
	Result: Duplicate ROMClass in ROMClass segment or node data which can't be found by other VMs */
	
	oldNum = _theca->updateSRP;
	/* Store the ShcItem->dataType and jvmID */
	_theca->lastMetadataType = *(U_32 *)&((ShcItem *)((UDATA)_theca + oldNum - (_storedMetaUsedBytes + _storedAOTUsedBytes + _storedJITUsedBytes)))->dataType;

	_theca->updateSRP -= _storedMetaUsedBytes + _storedAOTUsedBytes + _storedJITUsedBytes;

	Trc_SHR_Assert_True((IDATA)(_theca->updateSRP - _theca->segmentSRP) >= (IDATA)J9SHR_MIN_GAP_BEFORE_METADATA);

	Trc_SHR_CC_CRASH4_commitUpdate_Event2(currentThread, oldNum, _theca->updateSRP);

	/* If we crash here, metadata and romclass will exist, but no update count change, so other VMs not aware of update 
	Result: JVMs should re-populate their hashtables */

	UDATA* updateCountAddress = WSRP_GET(_theca->updateCountPtr, UDATA*);
	*updateCountAddress = *updateCountAddress + 1;
	Trc_SHR_CC_incCacheUpdateCount_Event(*updateCountAddress);
	_oldUpdateCount = *updateCountAddress;

	/* If we crash here, we simply allow slightly more AOT data in the cache than the set limit. This is not a concern.
	 * The aotBytes field is not used for calculating free space or pointers - it is used purely for setting the AOT limit. */

	if (_storedAOTUsedBytes) {
		_theca->aotBytes += _storedAOTUsedBytes;
	}
	if (_storedJITUsedBytes) {
		_theca->jitBytes += _storedJITUsedBytes;
	}
	if (_doMetaProtect
#if defined (J9SHR_MSYNC_SUPPORT)
			|| _doMetaSync
#endif
	) {
		/* Note that _scan always points to the next available ShcItemHdr, 
		 * so the memory between _scan and (_scan + sizeof(ShcItemHdr)) can be modified */
		if ((J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP) || (J9VM_PHASE_NOT_STARTUP == vm->phase)) 
			&& (true == _doPartialPagesProtect)
		) {
			/* Subtract 1 page size so that last partially filled page also gets protected,
			 * but if we are already on page boundary then no need to subtract the page size.
			 */
			if (((UDATA)_scan + sizeof(ShcItemHdr)) % _osPageSize) {
				notifyPagesCommitted((BlockPtr)_prevScan + sizeof(ShcItemHdr), (BlockPtr)_scan + sizeof(ShcItemHdr) - _osPageSize, DIRECTION_BACKWARD);
			} else {
				notifyPagesCommitted((BlockPtr)_prevScan + sizeof(ShcItemHdr), (BlockPtr)_scan + sizeof(ShcItemHdr), DIRECTION_BACKWARD);
			}
		} else {
			notifyPagesCommitted((BlockPtr)_prevScan + sizeof(ShcItemHdr), (BlockPtr)_scan + sizeof(ShcItemHdr), DIRECTION_BACKWARD);
		}
	}

	Trc_SHR_CC_CRASH5_commitUpdate_EndingCritical(currentThread);

	endCriticalUpdate(currentThread);		/* Note that this re-protects the header */

	_totalStoredBytes += (_storedMetaUsedBytes + _storedSegmentUsedBytes + _storedAOTUsedBytes + _storedJITUsedBytes + _storedReadWriteUsedBytes);
	_storedMetaUsedBytes = _storedSegmentUsedBytes = _storedAOTUsedBytes = _storedJITUsedBytes = _storedReadWriteUsedBytes = 0;

	updateMetadataSegment(currentThread);

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
#if defined(J9SHR_CACHELET_SUPPORT)
	if (_commitParent) {
		_parent->commitUpdate(currentThread);
		_commitParent = false;
	}
#endif
#endif

	Trc_SHR_CC_CRASH6_commitUpdate_Exit(currentThread, _oldUpdateCount);
	Trc_SHR_CC_commitUpdate_Exit(currentThread);
}

/**
 * Commit changes to shared classes cache
 *
 * Called when new item has been added to the cache (after an allocate call)
 * once the write is completed. This will inform other JVMs of the update.
 * In the case of a crash during a write, the cache will simply write over
 * the corrupt data.
 *
 * After the update is done, for non-nested cache, it calls fillCacheIfNearlyFull()
 * to fill up the cache with dummy data if the free block bytes are below a threshold value.
 * If the caller does not want this functionality, use commitUpdateHelper() instead.
 *
 * @param [in] currentThread  Pointer to J9VMThread structure for the current thread
 * @param [in] isCachelet true when committing a cachelet
 *
 * @pre The caller MUST hold the shared classes cache write mutex
 */
void
SH_CompositeCacheImpl::commitUpdate(J9VMThread* currentThread, bool isCachelet)
{
	commitUpdateHelper(currentThread, isCachelet);

	if (0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED)) {
		/* Since this method is not called for realtime cache,
		 * we can avoid determining first super cache to call fillCacheIfNearlyFull().
		 */
		fillCacheIfNearlyFull(currentThread);
	}
}

/**
 * Mark a block stale.
 * 
 * @param [in] blockEnd  Address of block to mark stale.  Need to pass pointer to first byte after end of block.
 * @param [in] isCacheLocked  Should be true if cache is locked
 *
 * @pre The calling function needs to hold the writeMutex before calling this function
 */
void
SH_CompositeCacheImpl::markStale(J9VMThread* currentThread, BlockPtr blockEnd, bool isCacheLocked)
{
	ShcItemHdr* ih = (ShcItemHdr*)(blockEnd);
	BlockPtr areaStart = NULL;
	UDATA areaLength = 0;

	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasWriteMutexThread);
	Trc_SHR_CC_markStale_Event(currentThread, ih);

	if (0 != _theca->crcValid) {
		/* _theca->crcValid is set to 0 when locking the cache. isCacheLocked cannot be true here */
		Trc_SHR_Assert_False(isCacheLocked);
		unprotectHeaderReadWriteArea(currentThread, false);
		_theca->crcValid = 0;
		protectHeaderReadWriteArea(currentThread, false);
	}

	/* If the cache is locked, don't bother to unprotect the page as the whole metadata area will be unprotected */
	if (_doMetaProtect && !isCacheLocked) {
		if (_osPageSize == 0) {
			Trc_SHR_Assert_ShouldNeverHappen();
			return;
		}
		areaStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)&(ih->itemLen));
		areaLength = _osPageSize;
		if (setRegionPermissions(_portlib, (void*)areaStart, areaLength, (J9PORT_PAGE_PROTECT_WRITE | J9PORT_PAGE_PROTECT_READ)) != 0) {
			PORT_ACCESS_FROM_PORT(_portlib);
			I_32 myerror = j9error_last_error_number();
			Trc_SHR_CC_markStale_setRegionPermissions_Failed(myerror);
			Trc_SHR_Assert_ShouldNeverHappen();
		}
	}

	CCSETITEMSTALE(ih);

	/* If the cache is locked, don't re-protect the page as the whole area remains unprotected. 
	 * Also, don't re-protect the page if we're not done modifying it. */
	if (_doMetaProtect && !isCacheLocked && ((UDATA)areaStart > (UDATA)_prevScan)) {
		if (setRegionPermissions(_portlib, (void*)areaStart, areaLength, J9PORT_PAGE_PROTECT_READ) != 0) {
			PORT_ACCESS_FROM_PORT(_portlib);
			I_32 myerror = j9error_last_error_number();
			Trc_SHR_CC_markStale_setRegionPermissions_Failed(myerror);
			Trc_SHR_Assert_ShouldNeverHappen();
		}
	}
}

/**
 * Test to see if a block is stale.
 * 
 * @param [in] block  Pointer to block to test
 *
 * @return 1 if a block is stale, 0 otherwise.
 */
UDATA
SH_CompositeCacheImpl::stale(BlockPtr blockEnd)
{
	ShcItemHdr* ih;

	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	ih = (ShcItemHdr*)(blockEnd);
	return (CCITEMSTALE(ih)) ? 1 : 0;
}

/**
 * Sets nextEntry pointer back to the start of the cache.
 *
 * This is used for walking the cache, for example if many entries need 
 * to be marked stale: Call @ref findStart and then loop on @ref nextEntry.
 *
 * @note Start of cache is actually end of cache!
 *
 * @pre This must be called with the cache mutex held and this should not be
 * released until the walk is completed.
 */
void
SH_CompositeCacheImpl::findStart(J9VMThread* currentThread)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	_prevScan = _scan;
	_scan = (ShcItemHdr*)CCFIRSTENTRY(_theca);
	Trc_SHR_CC_findStart_Event(currentThread, _scan);
}

/**
 * Utility function for finding the address of the start of the cache data.
 *
 * @note This function is needed for representing the cache as a J9MemorySegment.
 *
 * @return Address of start of shared classes cache
 */
void*
SH_CompositeCacheImpl::getBaseAddress(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return CASTART(_theca);
}

/**
 * Utility function for finding the address of the cache header.
 *
 * @return Address of start of shared classes cache header
 */
J9SharedCacheHeader *
SH_CompositeCacheImpl::getCacheHeaderAddress(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return _theca;
}

/**
 * Utility function for finding the address of the start of the String Table
 * data which is currently the same as the start of the readWrite data.
 *
 * @return Address of start of shared string table area
 */
void*
SH_CompositeCacheImpl::getStringTableBase(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return READWRITEAREASTART(_theca);
}

/**
 * Utility function for finding the total address of the cache memory, including the header
 *
 * @return Total size of the cache memory
 */
UDATA
SH_CompositeCacheImpl::getCacheMemorySize(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _theca->totalBytes;
}

/**
 * Utility function for finding the address of the end of the cache.
 *
 * @note This function is needed for representing the cache as a J9MemorySegment.
 *
 * @return Address of end of shared classes cache
 */
void*
SH_CompositeCacheImpl::getCacheEndAddress(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return CAEND(_theca);
}

/**
 * Utility function for finding the last writeable/readable UDATA address in the cache.
 * This function was created b.c when the cache is mapped to the end of memory CAEND(_theca)
 * will return NULL. Which causes problems in various asserts, and could possible create problems
 * with bounds checking in a ROM class segment.
 *
 * @note This function is needed for representing the cache as a J9MemorySegment.
 *
 * @return Address of the last writeable/readable UDATA in the cache
 */
void*
SH_CompositeCacheImpl::getCacheLastEffectiveAddress(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	UDATA cachEndAddr = ((UDATA)CAEND(_theca)) - sizeof(UDATA);
	return (void*)cachEndAddr;
}

/**
 * Get next free byte in the segment allocation area.
 *
 * @note This function is needed for representing the cache as a J9MemorySegment.
 *
 * @return pointer to the next free byte in the segment allocation area.
 */
void*
SH_CompositeCacheImpl::getSegmentAllocPtr(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return SEGUPDATEPTR(_theca);
}

/**
 * Return the start of the metadata allocation area.
 *
 * @note This function is needed for representing the cache as a J9MemorySegment.
 *
 * @return pointer to the next free byte in the segment allocation area.
 */
void*
SH_CompositeCacheImpl::getMetaAllocPtr(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return UPDATEPTR(_theca);
}

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
/**
 * Return the start of the readWrite allocation area.
 *
 * @return pointer to the next free byte in the readWrite allocation area.
 */
void*
SH_CompositeCacheImpl::getReadWriteAllocPtr(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return RWUPDATEPTR(_theca);
}
#endif

/**
 * Get total usable shared classes cache size
 *
 * @return total usable size of cache in bytes
 */
UDATA
SH_CompositeCacheImpl::getTotalUsableCacheSize(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return (CAEND(_theca) - READWRITEAREASTART(_theca));
}

/**
 * Get the total number of bytes stored in the shared classes cache
 *
 * @return total bytes stored (for stats)
 */
UDATA
SH_CompositeCacheImpl::getTotalStoredBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _totalStoredBytes;
}

/**
 * Get the unstored bytes
 *
 * @param [out] softmxUnstoredBytes bytes that are not stored due to the setting of softmx
 * @param [out] maxAOTUnstoredBytes AOT bytes that are not stored due to the setting of maxAOT
 * @param [out] maxJITUnstoredBytes JIT bytes that are not stored due to the setting of maxJIT
 */
void
SH_CompositeCacheImpl::getUnstoredBytes(U_32 *softmxUnstoredBytes, U_32 *maxAOTUnstoredBytes, U_32 *maxJITUnstoredBytes) const
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	if (NULL != softmxUnstoredBytes) {
		if ((0 == _osPageSize) || (UnitTest::CACHE_FULL_TEST == UnitTest::unitTest)) {
			/* Do not do round up in the case of UnitTest::CACHE_FULL_TEST, as we are testing the actual value there. */
			*softmxUnstoredBytes = _softmxUnstoredBytes;
		} else {
			*softmxUnstoredBytes = (U_32)ROUND_UP_TO(_osPageSize, _softmxUnstoredBytes);
		}
	}
	if (NULL != maxAOTUnstoredBytes) {
		if ((0 == _osPageSize) || (UnitTest::CACHE_FULL_TEST == UnitTest::unitTest)) {
			*maxAOTUnstoredBytes = _maxAOTUnstoredBytes;
		} else {
			*maxAOTUnstoredBytes = (U_32)ROUND_UP_TO(_osPageSize, _maxAOTUnstoredBytes);
		}
	}
	if (NULL != maxJITUnstoredBytes) {
		if ((0 == _osPageSize) || (UnitTest::CACHE_FULL_TEST == UnitTest::unitTest)) {
			*maxJITUnstoredBytes = _maxJITUnstoredBytes;
		} else {
			*maxJITUnstoredBytes = (U_32)ROUND_UP_TO(_osPageSize, _maxJITUnstoredBytes);
		}
	}
}

/**
 * Get the total number of AOT bytes stored in the shared classes cache
 *
 * @return total bytes stored (for stats)
 */
UDATA
SH_CompositeCacheImpl::getAOTBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return (_theca)->aotBytes;
}

/**
 * Get the total number of JIT bytes stored in the shared classes cache
 *
 * @return total bytes stored (for stats)
 */
UDATA
SH_CompositeCacheImpl::getJITBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return (_theca)->jitBytes;
}

/**
 * Get the total number of bytes in the readWrite area of the cache
 *
 * @return total bytes stored (for stats)
 */
UDATA
SH_CompositeCacheImpl::getReadWriteBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return (UDATA)READWRITEAREASIZE(_theca);
}

/**
 * Get the number of bytes to be used by shared string intern table
 *
 * @return shared intern table bytes
 */
UDATA
SH_CompositeCacheImpl::getStringTableBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _theca->sharedInternTableBytes;
}

/**
 * Get the number of free bytes in the shared classes cache between segmentSRP and updateSRP
 *
 * @return free bytes in cache.
 */
UDATA
SH_CompositeCacheImpl::getFreeBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return FREEBYTES(_theca);
}

/**
 * Get the number of free available bytes within softmx (softmx - usedBytes).
 *
 * @return free available bytes in cache (for stats)
 */
UDATA
SH_CompositeCacheImpl::getFreeAvailableBytes(void)
{
	/* This method might be called when _started is false */
	UDATA ret = 0;
	UDATA freeBlockBytes = (UDATA)getFreeBlockBytes();

	if ((U_32)-1 == _theca->softMaxBytes) {
		ret = (UDATA)(getTotalSize() - getUsedBytes());
	} else {
		if (J9_ARE_NO_BITS_SET(_cacheFullFlags, J9SHR_AVAILABLE_SPACE_FULL)) {
			ret = (UDATA)(_theca->softMaxBytes - getUsedBytes());
		}
		/* J9SHR_AVAILABLE_SPACE_FULL is set once (softmx - usedBytes) < CC_MIN_SPACE_BEFORE_CACHE_FULL. (softmx - usedBytes) might be > 0, but no data can be added,
		 * keep ret to 0 in this case.
		 */
	}
	
	/* Compared against free block bytes, as reserved AOT/JIT bytes are considered as used bytes */
	return (freeBlockBytes < ret) ? freeBlockBytes : ret;
}

/**
 * Is address is in the caches debug area?
 *
 * @ Returns true if address is in the caches debug area
 */
BOOLEAN
SH_CompositeCacheImpl::isAddressInCacheDebugArea(void *address, UDATA length)
{
	void * debugAreaStart = NULL;
	void * debugAreaEnd = NULL;
	void * addressEnd = ((U_8 *)address) + length;

	debugAreaStart = _debugData->getDebugAreaStartAddress();
	debugAreaEnd = (void *)(((UDATA)_debugData->getDebugAreaEndAddress()) - 1);

	if ((debugAreaStart <= address)  && (addressEnd <= debugAreaEnd)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * Get the number of debug bytes in the shared classes cache
 *
 * @return debug bytes in cache (for stats)
 */
U_32
SH_CompositeCacheImpl::getDebugBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _debugData->getDebugDataSize();
}

/**
 * Get the number of free debug area bytes in the shared classes cache
 *
 * @return number of bytes
 */
U_32
SH_CompositeCacheImpl::getFreeDebugSpaceBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _debugData->getFreeDebugSpaceBytes();
}

/**
 * Get the number of LineNumberTable debug bytes in the shared classes cache
 *
 * @return number of bytes
 */
U_32
SH_CompositeCacheImpl::getLineNumberTableBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _debugData->getLineNumberTableBytes();
}

/**
 * Get the number of LocalVariableTable debug bytes in the shared classes cache
 *
 * @return number of bytes
 */
U_32
SH_CompositeCacheImpl::getLocalVariableTableBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _debugData->getLocalVariableTableBytes();
}

/**
 * Get the number of free readWrite bytes remaining
 *
 * @return free readWrite bytes in cache (for stats)
 */
UDATA
SH_CompositeCacheImpl::getFreeReadWriteBytes(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return FREEREADWRITEBYTES(_theca);
}

/**
 * Check whether given address is in the Shared Classes cache ROMClass segment
 *
 * @param [in] address  Address of ROMClass
 *
 * @return true if address is within the shared ROMClass segment, false otherwise
 */
bool
SH_CompositeCacheImpl::isAddressInROMClassSegment(const void* address)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	return ((address >= CASTART(_theca)) && (address < SEGUPDATEPTR(_theca)));
}

/**
 * Checks whether given address is in the metadata area of this shared cache
 *
 * @param [in] address  Address to be checked
 *
 * @return true if address is within the shared metadata area, false otherwise
 */
bool
SH_CompositeCacheImpl::isAddressInMetaDataArea(const void* address) const
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	return ((address >= UPDATEPTR(_theca)) && (address < CADEBUGSTART(_theca)));
}

/**
 * Check whether given address is in the Shared Classes cache
 * Note - does not include the cache header
 *
 * @param [in] address  Address of cache item
 *
 * @return true if address is within the shared class cache boundaries, false otherwise
 */
bool
SH_CompositeCacheImpl::isAddressInCache(const void* address)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	return ((address >= CASTART(_theca)) && (address <= getCacheLastEffectiveAddress()));
}

/**
 * Run guaranteed code to be run at JVM exit
 */
void
SH_CompositeCacheImpl::runExitCode(J9VMThread *currentThread)
{
	SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache);

	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	/* Unprotect the cache header on exit to allow for updates. */
#if defined(J9ZOS390)
	/* On z/OS, readwrite area needs to be unprotected as well if mprotect=all is set.
	 * This is because memory protection on z/OS persists across processes.
	 * If not unprotected here, subsequent JVMs will not be able to write to readwrite area.
	 */
	unprotectHeaderReadWriteArea(currentThread, true);
	if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL)) {
		/* If mprotect=all is set, above call to unprotectHeaderReadWriteArea() will set _readWriteProtectCntr to 1. */
		Trc_SHR_Assert_Equals(_readWriteProtectCntr, 1);
	} else {
		Trc_SHR_Assert_Equals(_readWriteProtectCntr, 0);
	}
#else
	unprotectHeaderReadWriteArea(currentThread, false);
	Trc_SHR_Assert_Equals(_readWriteProtectCntr, 0);
#endif
	/* If mprotect=all is not set, then final value of _headerProtectCntr should be same is its initial value (= 1).
	 * If mprotect=all is set, then above call to unprotectHeaderReadWriteArea() will set it to 1.
	 */
	Trc_SHR_Assert_Equals(_headerProtectCntr, 1);

	if (_commonCCInfo->hasRWMutexThreadMprotectAll == currentThread) {
		Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasReadWriteMutexThread);
		_commonCCInfo->hasReadWriteMutexThread = NULL;
		_commonCCInfo->hasRWMutexThreadMprotectAll = NULL;
		if (0 != oscacheToUse->releaseWriteLock(_commonCCInfo->readWriteAreaMutexID)) {
			Trc_SHR_CC_runExitCode_ReleaseMutexFailed(currentThread);
		}
	}

	if (UnitTest::MINMAX_TEST == UnitTest::unitTest) {
		return;
	}
	/* If we're exiting abnormally, a thread may have the write mutex.
	 * If that's the case, do not attempt to update the cache CRC
	 * because otherwise we can hang writing the dumps */
	if ((_commonCCInfo->hasWriteMutexThread == NULL) && (_commonCCInfo->writeMutexID != CC_READONLY_LOCK_VALUE)) {
		IDATA lockrc = 0;
		PORT_ACCESS_FROM_PORT(_portlib);
		if ((lockrc = oscacheToUse->acquireWriteLock(_commonCCInfo->writeMutexID)) == 0) {
			updateCacheCRC();
			/* Deny updates so the CRC is not invalidated */
			*_runtimeFlags |= J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES;
			if ((lockrc = oscacheToUse->releaseWriteLock(_commonCCInfo->writeMutexID)) != 0) {
				CC_ERR_TRACE1(J9NLS_SHRC_CC_FAILED_EXIT_MUTEX, lockrc);
			}
		} else {
			CC_ERR_TRACE1(J9NLS_SHRC_CC_FAILED_RUN_EXIT_CODE_ACQUIRE_MUTEX, lockrc);
		}
	}
	oscacheToUse->runExitCode();
	return;
}

/**
 * Return ID of this JVM
 */
U_16
SH_CompositeCacheImpl::getJVMID(void)
{
	return _commonCCInfo->vmID;
}

/**
 * Set the fields to the values in the cache header
 */
void
SH_CompositeCacheImpl::setInternCacheHeaderFields(J9SRP** sharedTail, J9SRP** sharedHead, U_32** totalSharedNodes, U_32** totalSharedWeight)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_setInternCacheHeaderFields_Entry();

	*sharedTail = &(_theca->sharedStringTail);
	*sharedHead = &(_theca->sharedStringHead);
	*totalSharedNodes = &(_theca->totalSharedStringNodes);
	*totalSharedWeight = &(_theca->totalSharedStringWeight);
	Trc_SHR_CC_setInternCacheHeaderFields_ExitV2(*sharedTail, *sharedHead, *totalSharedNodes, *totalSharedWeight);
}

/**
 * Enter mutex to modify the readWrite area of the cache.
 * Allows only single-threaded modification of the readWriteArea.
 * Note that this will unprotect the area if mprotect is set.
 * Note If the cache is readonly this function will return -1.
 *	    doRebuildLocalData is always 0 when a cache is readonly.
 *	    doRebuildCacheData is always 0 when a cache is readonly.
 * 
 * @param [in] currentThread  Point to the J9VMThread struct for the current thread
 * @param [out] doRebuildLocalData  If not null, the readWrite cache area has potentially been 
 *              corrupted and any local data pointing to data in the readWrite area therefore be rebuilt by the caller.
 * @param [out] doRebuildCacheData  If not null, the readWrite cache area has potentially been 
 *              corrupted and this is the first JVM to be notified. The caller is responsible for rebuilding the cached data.
 *
 * @return 0 if call succeeded and -1 for failure
 */
IDATA
SH_CompositeCacheImpl::enterReadWriteAreaMutex(J9VMThread* currentThread, BOOLEAN readOnly, UDATA* doRebuildLocalData, UDATA* doRebuildCacheData)
{
	IDATA rc = -1;
	SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache);
	UDATA readWriteCrashCntr = UDATA_MAX;

	if (!_started) {
		return -1;
	}
	Trc_SHR_CC_enterReadWriteAreaMutex_Entry(currentThread);

	/* Initialize 'doRebuildCacheData', and 'doRebuildLocalData', to indicate success unless they are set below.
	 */
	*doRebuildCacheData = 0;
	*doRebuildLocalData = 0;

	if (oscacheToUse && _readWriteAreaBytes) {
		if (_commonCCInfo->readWriteAreaMutexID != CC_READONLY_LOCK_VALUE) {

			Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasReadWriteMutexThread);
			Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasRefreshMutexThread);

			rc = oscacheToUse->acquireWriteLock(_commonCCInfo->readWriteAreaMutexID);
			if (rc == 0) {
				UDATA oldReadWriteCrashCntr = _theca->readWriteCrashCntr;

				_commonCCInfo->hasReadWriteMutexThread = currentThread;

				/* CMVC 176498 : Set testing conditions to force string table reset */
				if ((0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READONLY))) {
					readOnly = TRUE;
				} else if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READWRITE)) {
					readOnly = FALSE;
				}

				if (!readOnly) {
					/* When the cache is full 'SH_CacheMap::enterStringTableMutex' will 
					 * set 'J9AVLTREE_DISABLE_SHARED_TREE_UPDATES' which will prevent any 
					 * 'unsafe' operations from occurring on the string table.
					 * 
					 * Due to this there is no need to increment '_theca->readWriteCrashCnt'. 
					 * Not incrementing this counter means if ctrl+c, or kill -9, is executed 
					 * on the JVM then there will be no need to reset the string table.  
					 */
					 _incrementedRWCrashCntr = true;
					unprotectHeaderReadWriteArea(currentThread, true);
					unprotectHeaderReadWriteArea(currentThread, false);
					_theca->readWriteCrashCntr = oldReadWriteCrashCntr + 1;
					protectHeaderReadWriteArea(currentThread, false);
					if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READWRITE)) {
						/* CMVC 176498 : Set conditions to force string table reset with readOnly = TRUE.
						 * This should be executed after J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READONLY case.
						 * Variable conditions required to test RESET_READWRITE rely on RESET_READONLY setting them first.
						 *
						 * Here variable state is:
	 					 * 		_theca->readWriteCrashCntr = 1
						 * 		oldReadWriteCrashCntr = 0
						 * 		_theca->readWriteRebuildCntr = -1 or UDATA_MAX (This gets set to oldReadWriteCrashCntr (0) down the line as the two are unequal).
						 */
						*_runtimeFlags &= (~J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READWRITE);
					}
				} else {
					_incrementedRWCrashCntr = false;
					if ((0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READONLY))) {
						/* CMVC 176498 : Set conditions to force string table reset with readOnly = FALSE.
						 * This block is executed during first call.
						 * 
						 * Note: b/c _incrementedRWCrashCntr is false _theca->readWriteCrashCntr will not 
						 * be decremented on exit.
						 * 
						 * Here variable state is:
						 * 		_theca->readWriteCrashCntr = 1
						 * 		oldReadWriteCrashCntr = 0
						 * 		_theca->readWriteRebuildCntr = 0
						 */
						oldReadWriteCrashCntr -= 1;
						*_runtimeFlags &= (~J9SHR_RUNTIMEFLAG_CHECK_STRINGTABLE_RESET_READONLY);
						/* Here variable state is:
						 * 		_theca->readWriteCrashCntr = 1
						 * 		oldReadWriteCrashCntr =  -1 or UDATA_MAX 
						 * 		_theca->readWriteRebuildCntr = 0 (This gets set to oldReadWriteCrashCntr (-1) down the line as the two are unequal).
						 */
					}

				}

				if (oldReadWriteCrashCntr != _theca->readWriteRebuildCntr) {
					Trc_SHR_CC_enterReadWriteAreaMutex_EventRebuildCacheData2(currentThread,oldReadWriteCrashCntr,_theca->readWriteRebuildCntr);
					*doRebuildCacheData = 1;
					if (readOnly) {
						unprotectHeaderReadWriteArea(currentThread, true);
					}
					_theca->readWriteRebuildCntr = oldReadWriteCrashCntr;
					if (readOnly) {
						protectHeaderReadWriteArea(currentThread, false);
					}
				}
				_commonCCInfo->stringTableStarted = TRUE;

				if (_localReadWriteCrashCntr != oldReadWriteCrashCntr) {
					Trc_SHR_CC_enterReadWriteAreaMutex_EventRebuildLocalData2(currentThread, _localReadWriteCrashCntr, oldReadWriteCrashCntr);
					*doRebuildLocalData = 1;
					_localReadWriteCrashCntr = oldReadWriteCrashCntr;
				}

				readWriteCrashCntr = _theca->readWriteCrashCntr;

			}
		}
	}
	if (rc == -1) {
		Trc_SHR_CC_enterReadWriteAreaMutex_ExitError(currentThread, rc);
	} else {
		Trc_SHR_CC_enterReadWriteAreaMutex_ExitCntr(currentThread, rc, readWriteCrashCntr);
	}
	return rc;
}

/**
 * Exit readWrite area mutex
 * Note that when the function returns, the readWrite area will be protected if mprotect is enabled.
 * It will also have been msync'd if msync is enabled.
 *
 * @param [in] currentThread  Point to the J9VMThread struct for the current thread
 * @param [in] resetReason State of the read write area.
 *
 * @return 0 if call succeeded and -1 for failure
 */
IDATA
SH_CompositeCacheImpl::exitReadWriteAreaMutex(J9VMThread* currentThread, UDATA resetReason)
{
	IDATA rc = -1;
	SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache);
	UDATA readWriteCrashCntr = UDATA_MAX;

	if (!_started) {
		return -1;
	}
	Trc_SHR_CC_exitReadWriteAreaMutex_Entry(currentThread);

	if (_commonCCInfo->readWriteAreaMutexID == CC_READONLY_LOCK_VALUE) {
		_commonCCInfo->hasReadWriteMutexThread = NULL;
		Trc_SHR_CC_exitReadWriteAreaMutex_ExitReadOnly(currentThread);
		return 0;
	}

	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasReadWriteMutexThread);
	Trc_SHR_Assert_NotEquals(currentThread, _commonCCInfo->hasRefreshMutexThread);

	if (oscacheToUse && _readWriteAreaBytes) {
#if defined (J9SHR_MSYNC_SUPPORT)
		if (_doReadWriteSync) {
			oscacheToUse->syncUpdates((void*)_readWriteAreaStart, _readWriteAreaBytes, (J9PORT_MMAP_SYNC_WAIT | J9PORT_MMAP_SYNC_INVALIDATE));
		}
#endif

		if (resetReason != J9SHR_STRING_POOL_OK) {
			UDATA oldNum = _theca->readWriteVerifyCntr;
			/* The bottom 4 bits are reserved for flags, the count is shifted by 4 */
			UDATA value = (oldNum + 0x10) & ~0xf;
			value |= (oldNum & 0xf) | resetReason;
			_theca->readWriteVerifyCntr = value;
		}

		if (true == _incrementedRWCrashCntr) {
			unprotectHeaderReadWriteArea(currentThread, false);
			_theca->readWriteCrashCntr--;
			protectHeaderReadWriteArea(currentThread, false);
		}
		readWriteCrashCntr = _theca->readWriteCrashCntr;

		/* Detect if unprotect was called from enterReadWriteAreaMutex(). */
		if (_readWriteProtectCntr > 0) {
			protectHeaderReadWriteArea(currentThread, true);
		}

		/**
		 * Do assertion checks before releasing the lock to prevent others threads changing the values of
		 * _headerProtectCntr and _readWriteProtectCntr
		 */
		if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL)) {
			Trc_SHR_Assert_Equals(_headerProtectCntr, 0);
		} else {
			/* If mprotect=all is not set, _headerProtectCntr should be same as initial value (= 1). */
			Trc_SHR_Assert_Equals(_headerProtectCntr, 1);
		}
		Trc_SHR_Assert_Equals(_readWriteProtectCntr, 0);

		/* Clear hasReadWriteMutexThread just before calling releaseWriteLock() */
		_commonCCInfo->hasReadWriteMutexThread = NULL;
		if ((rc = oscacheToUse->releaseWriteLock(_commonCCInfo->readWriteAreaMutexID)) != 0) {
			PORT_ACCESS_FROM_PORT(_portlib);
			CC_ERR_TRACE1(J9NLS_SHRC_CC_FAILED_EXIT_MUTEX, rc);
			return -1;
		}
	}

	Trc_SHR_CC_exitReadWriteAreaMutex_ExitCntr(currentThread, rc, readWriteCrashCntr);
	return rc;
}

UDATA
SH_CompositeCacheImpl::getReaderCount(J9VMThread* currentThread)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	if (!_readOnlyOSCache) {
		return _theca->readerCount;
	} else {
		return _readOnlyReaderCount;			/* Maintained so that our assertions still work */
	}
}

bool
SH_CompositeCacheImpl::hasWriteMutex(J9VMThread* currentThread)
{
	/* _commonCCInfo->writeMutexID may be 0 (uninitialized). However, in this case, _commonCCInfo->hasWriteMutexThread
	 * should still be NULL, so we should still get a reasonable result.
	 */
	if (_commonCCInfo->writeMutexID == CC_READONLY_LOCK_VALUE) {
		if (_commonCCInfo->writeMutexEntryCount > 0) {
			IDATA entryCount = (IDATA)omrthread_tls_get(omrthread_self(), _commonCCInfo->writeMutexEntryCount);
			return (entryCount > 0);
		}
		return false;
	} else {
		return (_commonCCInfo->hasWriteMutexThread == currentThread);
	}
}

bool
SH_CompositeCacheImpl::hasReadWriteMutex(J9VMThread* currentThread)
{
	return (_commonCCInfo->hasReadWriteMutexThread == currentThread);
}

void
SH_CompositeCacheImpl::notifyRefreshMutexEntered(J9VMThread* currentThread)
{
	Trc_SHR_Assert_Equals(NULL, _commonCCInfo->hasRefreshMutexThread);
	_commonCCInfo->hasRefreshMutexThread = currentThread;
}

void
SH_CompositeCacheImpl::notifyRefreshMutexExited(J9VMThread* currentThread)
{
	Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasRefreshMutexThread);
	_commonCCInfo->hasRefreshMutexThread = NULL;
}

/* Since the cache header and the readWrite area may in the same page, protecting and unprotecting
 * these areas must be coordinated. On entry to this function, it should be possible for 3 states:
 * 1) Neither area is protected (another thread is changing the areas)
 * 2) Only the readWrite area is protected (another thread is changing the header)
 * 3) Both areas are protected
 * Entering this function, it is implicit that the header needs to be unprotected, but the readWrite area is optional
 *
 * @param [in] currentThread Point to the J9VMThread struct for the current thread
 * @param [in] changeReadWrite Whether to unprotect the readWrite area
 */
void
SH_CompositeCacheImpl::unprotectHeaderReadWriteArea(J9VMThread* currentThread, bool changeReadWrite)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	if (_doHeaderProtect || (_doHeaderReadWriteProtect && changeReadWrite)) {
		void* areaStart;
		UDATA areaLength = 0;
		bool doUnprotectReadWrite;
		PORT_ACCESS_FROM_PORT(_portlib);

		if (_readOnlyOSCache) {
			Trc_SHR_Assert_ShouldNeverHappen();
			return;
		}
		Trc_SHR_CC_unprotectHeaderReadWriteArea_Entry(changeReadWrite);

		if ((J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT_ALL))
			&& (!hasReadWriteMutex(currentThread))
		) {
			/* readWriteMutex need to be held when we are unprotecting the header with option mprotect=all */
			SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache);
			IDATA readWriteAreaMutexAcquired = oscacheToUse->acquireWriteLock(_commonCCInfo->readWriteAreaMutexID);

			if (0 == readWriteAreaMutexAcquired) {
				/* If we acquired readWriteMutex here, it will be released in the next call of protectHeaderReadWriteArea()*/
				Trc_SHR_Assert_Equals(NULL, _commonCCInfo->hasRWMutexThreadMprotectAll);
				_commonCCInfo->hasReadWriteMutexThread = currentThread;
				_commonCCInfo->hasRWMutexThreadMprotectAll = currentThread;
			} else {
				Trc_SHR_CC_unprotectHeaderReadWriteArea_AcquireMutexFailed(currentThread);
			}
		}
		Trc_SHR_CC_unprotectHeaderReadWriteArea_Debug_Pre1(changeReadWrite, _headerProtectCntr, _readWriteProtectCntr);
		omrthread_monitor_enter(_headerProtectMutex);
		doUnprotectReadWrite = (changeReadWrite && (_readWriteProtectCntr == 0));
		Trc_SHR_CC_unprotectHeaderReadWriteArea_Debug_Post1(doUnprotectReadWrite, _headerProtectCntr, _readWriteProtectCntr);

		if (_doHeaderProtect && (_headerProtectCntr == 0)) {
			areaStart = (void*)_cacheHeaderPageStart;
			areaLength = _cacheHeaderPageBytes;
			_readWriteAreaHeaderIsReadOnly = false;
			if (doUnprotectReadWrite) {
				areaLength += _readWriteAreaPageBytes;
			}
		} else if (doUnprotectReadWrite && (_readWriteAreaPageStart != 0)) {
			areaStart = (void*)_readWriteAreaPageStart;
			areaLength = _readWriteAreaPageBytes;
		} else {
			areaStart = NULL;
		}

		if (areaStart != NULL) {
			IDATA rc;

			if ((rc = setRegionPermissions(_portlib, areaStart, areaLength, (J9PORT_PAGE_PROTECT_WRITE | J9PORT_PAGE_PROTECT_READ))) != 0) {
				I_32 myerror = j9error_last_error_number();
				Trc_SHR_CC_unprotectHeaderReadWriteArea_setRegionPermissions_Failed(myerror);
				Trc_SHR_Assert_ShouldNeverHappen();
			}
			if (isVerbosePages() == true) {
				if (doUnprotectReadWrite) {
					j9tty_printf(PORTLIB, "Unprotecting cache header and readWrite area - from %x for %d bytes - rc=%d\n", areaStart, areaLength, rc);
				} else {
					j9tty_printf(PORTLIB, "Unprotecting cache header - from %x for %d bytes - rc=%d\n", areaStart, areaLength, rc);
				}
			}
		}
		if (_doHeaderProtect) {
			++_headerProtectCntr;
		}
		if (changeReadWrite) {
			++_readWriteProtectCntr;
		}

		Trc_SHR_CC_unprotectHeaderReadWriteArea_Debug_Pre2(areaStart, areaLength, _headerProtectCntr, _readWriteProtectCntr);
		omrthread_monitor_exit(_headerProtectMutex);
		Trc_SHR_CC_unprotectHeaderReadWriteArea_Debug_Post2(_headerProtectCntr, _readWriteProtectCntr);

		Trc_SHR_CC_unprotectHeaderReadWriteArea_Exit();
	}
}

/* This function protect the header and readWrite area.
 *
 * @param [in] currentThread Point to the J9VMThread struct for the current thread
 * @param [in] changeReadWrite Whether to protect the readWrite area
 */
void
SH_CompositeCacheImpl::protectHeaderReadWriteArea(J9VMThread* currentThread, bool changeReadWrite)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	if (_doHeaderProtect || (_doHeaderReadWriteProtect && changeReadWrite)) {
		void* areaStart;
		U_32 areaLength = 0;
		bool doProtectReadWrite;
		PORT_ACCESS_FROM_PORT(_portlib);

		Trc_SHR_CC_protectHeaderReadWriteArea_Entry(changeReadWrite);

		Trc_SHR_CC_protectHeaderReadWriteArea_Debug_Pre1(changeReadWrite, _headerProtectCntr, _readWriteProtectCntr);
		omrthread_monitor_enter(_headerProtectMutex);
		doProtectReadWrite = (changeReadWrite && (_readWriteProtectCntr == 1));
		Trc_SHR_CC_protectHeaderReadWriteArea_Debug_Post1(doProtectReadWrite, _headerProtectCntr, _readWriteProtectCntr);

		if (_doHeaderProtect && (_headerProtectCntr == 1)) {
			areaStart = (void*)_cacheHeaderPageStart;
			areaLength = _cacheHeaderPageBytes;
			_readWriteAreaHeaderIsReadOnly = true;
			if (doProtectReadWrite) {
				areaLength += _readWriteAreaPageBytes;
			}
		} else if (doProtectReadWrite && (_readWriteAreaPageStart != 0)) {
			areaStart = (void*)_readWriteAreaPageStart;
			areaLength = _readWriteAreaPageBytes;
		} else {
			areaStart = NULL;
		}
		if (areaStart != NULL) {
			IDATA rc;

			if ((rc = setRegionPermissions(_portlib, areaStart, areaLength, J9PORT_PAGE_PROTECT_READ)) != 0) {
				I_32 myerror = j9error_last_error_number();
				Trc_SHR_CC_protectHeaderReadWriteArea_setRegionPermissions_Failed(myerror);
				Trc_SHR_Assert_ShouldNeverHappen();
			}
			if (isVerbosePages() == true) {
				if (doProtectReadWrite) {
					j9tty_printf(PORTLIB, "Protecting cache header and readWrite area - from %x for %d bytes - rc=%d\n", areaStart, areaLength, rc);
				} else {
					j9tty_printf(PORTLIB, "Protecting cache header - from %x for %d bytes - rc=%d\n", areaStart, areaLength, rc);
				}
			}
		}
		if (_doHeaderProtect) {
			if (--_headerProtectCntr < 0) {
				Trc_SHR_Assert_ShouldNeverHappen();
			}
		}
		if (changeReadWrite) {
			if (--_readWriteProtectCntr < 0) {
				Trc_SHR_Assert_ShouldNeverHappen();
			}
		}

		Trc_SHR_CC_protectHeaderReadWriteArea_Debug_Pre2(areaStart, areaLength, _headerProtectCntr, _readWriteProtectCntr);
		omrthread_monitor_exit(_headerProtectMutex);
		Trc_SHR_CC_protectHeaderReadWriteArea_Debug_Post2(_headerProtectCntr, _readWriteProtectCntr);
		if (_commonCCInfo->hasRWMutexThreadMprotectAll == currentThread) {
			SH_OSCache* oscacheToUse = ((_ccHead == NULL) ? _oscache : _ccHead->_oscache);
			IDATA readWriteMutexReleased = 0;

			Trc_SHR_Assert_Equals(currentThread, _commonCCInfo->hasReadWriteMutexThread);
			_commonCCInfo->hasReadWriteMutexThread = NULL;
			_commonCCInfo->hasRWMutexThreadMprotectAll = NULL;
			readWriteMutexReleased = oscacheToUse->releaseWriteLock(_commonCCInfo->readWriteAreaMutexID);
			if (0 != readWriteMutexReleased) {
				Trc_SHR_CC_protectHeaderReadWriteArea_ReleaseMutexFailed(currentThread);
			}
		}
		Trc_SHR_CC_protectHeaderReadWriteArea_Exit();
	}
}

/* Unprotects the whole metadata area. This is only done if the cache is locked */
void
SH_CompositeCacheImpl::unprotectMetadataArea()
{
	if (!_started || _readOnlyOSCache) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}

	if (_doMetaProtect) {
		BlockPtr areaStart;
		U_32 areaLength;
		IDATA rc;
		PORT_ACCESS_FROM_PORT(_portlib);

		if ((_osPageSize == 0) || (_readOnlyOSCache)) {
			Trc_SHR_Assert_ShouldNeverHappen();
			return;
		}

		Trc_SHR_CC_unprotectMetadataArea_Entry();

		areaStart = (BlockPtr)_scan;
		areaStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)areaStart);
		areaLength = (U_32)((BlockPtr)CADEBUGSTART(_theca) - (BlockPtr)areaStart);
		if ((rc = setRegionPermissions(_portlib, (void*)areaStart, areaLength, (J9PORT_PAGE_PROTECT_WRITE | J9PORT_PAGE_PROTECT_READ))) != 0) {
			I_32 myerror = j9error_last_error_number();
			Trc_SHR_CC_unprotectMetadataArea_setRegionPermissions_Failed(myerror);
			Trc_SHR_Assert_ShouldNeverHappen();
		}
		if (isVerbosePages() == true) {
			j9tty_printf(PORTLIB, "Unprotecting entire metadata area - from %x for %d bytes - rc=%d\n", areaStart, areaLength, rc);
		}

		Trc_SHR_CC_unprotectMetadataArea_Exit(rc);
	}
}

void
SH_CompositeCacheImpl::protectMetadataArea(J9VMThread *currentThread)
{
	if (!_started) {
		return;
	}
	if (_doMetaProtect) {
		BlockPtr areaStart;
		U_32 areaLength;
		IDATA rc;
		PORT_ACCESS_FROM_PORT(_portlib);

		if ((_osPageSize == 0) || (_readOnlyOSCache)) {
			Trc_SHR_Assert_ShouldNeverHappen();
			return;
		}

		Trc_SHR_CC_protectMetadataArea_Entry();

		areaStart = (BlockPtr)_scan + sizeof(ShcItemHdr);
		if ((true == isCacheMarkedFull(currentThread))
			|| ((J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP) || (J9VM_PHASE_NOT_STARTUP == currentThread->javaVM->phase))
			 && (true == _doPartialPagesProtect))
		) {
			areaStart = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)areaStart);
		} else {
			areaStart = (BlockPtr)ROUND_UP_TO(_osPageSize, (UDATA)areaStart);
		}
		areaLength = (U_32)((BlockPtr)CADEBUGSTART(_theca) - (BlockPtr)areaStart);
		if ((rc = setRegionPermissions(_portlib, (void*)areaStart, areaLength, J9PORT_PAGE_PROTECT_READ)) != 0) {
			I_32 myerror = j9error_last_error_number();
			Trc_SHR_CC_protectMetadataArea_setRegionPermissions_Failed(myerror);
			Trc_SHR_Assert_ShouldNeverHappen();
		}
		if (isVerbosePages() == true) {
			j9tty_printf(PORTLIB, "Protecting entire metadata area - from %x for %d bytes - rc=%d\n", areaStart, areaLength, rc);
		}

		Trc_SHR_CC_protectMetadataArea_Exit(rc);
	}
}

bool
SH_CompositeCacheImpl::isRunningReadOnly(void)
{
	return _readOnlyOSCache;
}

bool
SH_CompositeCacheImpl::isVerbosePages(void)
{
	if (_verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_PAGES) {
		return true;
	}
	return false;
}

bool
SH_CompositeCacheImpl::isMemProtectEnabled(void)
{
	if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT)) {
		return true;
	}
	return false;
}

bool
SH_CompositeCacheImpl::isMemProtectPartialPagesEnabled(void)
{
	return _doPartialPagesProtect;
}

/* THREADING: Pre-req holds the cache write mutex */
U_32
SH_CompositeCacheImpl::getCacheCRC(void)
{
	U_32 value = 0;
	U_32 areaForCrcSize;
	U_8* areaForCrc;

	if (_theca == NULL) {
		return 0;
	}

	Trc_SHR_CC_getCacheCRC_Entry();

	/* CRC from the start of the ROMClass area to the end of the ROMClass area */
	areaForCrc = (U_8*)CASTART(_theca);
	areaForCrcSize = (U_32)((U_8*)SEGUPDATEPTR(_theca) - areaForCrc);
	value += getCacheAreaCRC(areaForCrc, areaForCrcSize);

	/* CRC from the start of the metadata area to the end of the cache */
	areaForCrc = (U_8*)UPDATEPTR(_theca);
	areaForCrcSize = (U_32)((U_8*)CADEBUGSTART(_theca) - areaForCrc);
	value += getCacheAreaCRC(areaForCrc, areaForCrcSize);

	Trc_SHR_CC_getCacheCRC_Exit(value, _theca->crcValue);
	return value;
}

/* THREADING: Pre-req holds the cache write mutex */
U_32
SH_CompositeCacheImpl::getCacheAreaCRC(U_8* areaStart, U_32 areaSize)
{
	U_32 seed, value, stepsize;

	Trc_SHR_CC_getCacheAreaCRC_Entry(areaStart, areaSize);

	/* 
	 * 1535=1.5k - 1.  Chosen so that we aren't stepping on exact power of two boundaries through
	 * the cache and yet we use a decent number of samples through the cache.
	 * For a 16Meg cache this will cause us to take 10000 samples.
	 * For a 100Meg cache this will cause us to take 68000 samples.
	 */
	stepsize = 1535;
	if ((areaSize/stepsize) > J9SHR_CRC_MAX_SAMPLES) {
		/* Reduce the number of samples */
		stepsize = areaSize/J9SHR_CRC_MAX_SAMPLES;
	}

	seed = j9crc32(0, NULL, 0);
	value = j9crcSparse32(seed, areaStart, areaSize, stepsize);

	Trc_SHR_CC_getCacheAreaCRC_Exit(value, stepsize);

	return value;
}

/* THREADING: Pre-req holds the cache write mutex 
 * Pre-req: The header is unprotected */
void
SH_CompositeCacheImpl::updateCacheCRC(void)
{
	U_32 value;

	if (_readOnlyOSCache) {
		return;
	}
	value = getCacheCRC();
	if (value) {
		_theca->crcValue = value;
		/* If the means of calculating CRC changes, increment the CC_CRC_VALID_VALUE */
		_theca->crcValid = CC_CRC_VALID_VALUE;
	}
}

/* THREADING: Pre-req holds the cache write mutex */
bool
SH_CompositeCacheImpl::checkCacheCRC(bool* cacheHasIntegrity, UDATA *crcValue)
{
	if (NULL != crcValue) {
		*crcValue = 0;
	}
	if (isCacheInitComplete() == true) {
		U_32 value = getCacheCRC();

		if (value && (_theca->crcValid == CC_CRC_VALID_VALUE)) {
			*cacheHasIntegrity = _theca->crcValue == value;
			if (!*cacheHasIntegrity) {
				if (NULL != crcValue) {
					*crcValue = value;
				}
				PORT_ACCESS_FROM_PORT(_portlib);
				CC_ERR_TRACE2(J9NLS_SHRC_CC_CRC_CHECK_FAILED, _theca->crcValue, value);
			}
			/* If the shared classes cache is started with -Xshareclasses:testFakeCorruption,
			 * then the crc is failed, and the test flag is disabled.
			 */
			if ((true == *cacheHasIntegrity) && (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_FAKE_CORRUPTION))) {
				*_runtimeFlags &= ~(J9SHR_RUNTIMEFLAG_FAKE_CORRUPTION);
				*cacheHasIntegrity = false;
				return false;	
			}
			return *cacheHasIntegrity;
		}
	}
	*cacheHasIntegrity = false;
	return true;
}

/**
 * Returns the size of the cache memory in bytes.
 * 
 * Return value of this function is not derived from the cache header itself, so
 * it should be reliable even if the cache is corrupted
 *
 * Returns size of the cache in bytes 
 */
U_32
SH_CompositeCacheImpl::getTotalSize(void)
{
	if (UnitTest::CACHE_FULL_TEST == UnitTest::unitTest) {
		/* in CACHE_FULL_TEST, _oscache->getTotalSize() returns 0 which is not true, return _theca->totalBytes instead */
		return _theca->totalBytes;
	}
	if (_oscache) {
		return _oscache->getTotalSize();
	} else {
		return 0;
	}
}

UDATA
SH_CompositeCacheImpl::getJavacoreData(J9JavaVM *vm, J9SharedClassJavacoreDataDescriptor* descriptor)
{
	getCorruptionContext(&descriptor->corruptionCode, &descriptor->corruptValue);

	if((UnitTest::MINMAX_TEST != UnitTest::unitTest)) {
		if (NULL != _oscache) {
			if (!_oscache->getJavacoreData(vm, descriptor)) {
				return 0;
			}
			descriptor->totalSize = this->_oscache->getTotalSize();
		}
	}

	if (isCacheInitComplete()) {
		/* ROMClass start/end obviously doesn't make sense for cachelets */
		descriptor->romClassStart = CASTART(_theca); /* getBaseAddress() */
		descriptor->romClassEnd = SEGUPDATEPTR(_theca); /* getSegmentAllocPtr() */
		descriptor->metadataStart = UPDATEPTR(_theca); /* getMetaAllocPtr() */
		descriptor->cacheEndAddress = CAEND(_theca); /* getCacheEndAddress() */
		descriptor->cacheSize = CAEND(_theca) - READWRITEAREASTART(_theca); /* getTotalUsableCacheSize() */
		descriptor->readWriteBytes = (UDATA)READWRITEAREASIZE(_theca); /* getReadWriteBytes() */

		descriptor->extraFlags = _theca->extraFlags;
		descriptor->minAOT = _theca->minAOT;
		descriptor->maxAOT = _theca->maxAOT;
		descriptor->minJIT = _theca->minJIT;
		descriptor->maxJIT = _theca->maxJIT;
		descriptor->softMaxBytes = (UDATA)((U_32)-1 == _theca->softMaxBytes ? descriptor->cacheSize : _theca->softMaxBytes);

		if ((NULL != _debugData) && !_debugData->getJavacoreData(vm, descriptor, _theca)) {
			return 0;
		}
	}

	descriptor->writeLockTID = _commonCCInfo->hasWriteMutexThread;
	descriptor->readWriteLockTID = _commonCCInfo->hasReadWriteMutexThread;

	return 1;
}

void
SH_CompositeCacheImpl::updateMetadataSegment(J9VMThread* currentThread)
{
	J9JavaVM* vm = currentThread->javaVM;

	if (_metadataSegmentPtr == NULL) {
		return;
	}

#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_t memorySegmentMutex = vm->memorySegments->segmentMutex;
	bool hasMemorySegmentMutex = false;

	if (memorySegmentMutex) {
		hasMemorySegmentMutex = (omrthread_monitor_owned_by_self(memorySegmentMutex) != 0);
		if (!hasMemorySegmentMutex) {
			omrthread_monitor_enter(memorySegmentMutex);
		}
	}
#endif

	(*_metadataSegmentPtr)->heapBase = (U_8*)getMetaAllocPtr();

#if defined(J9VM_THR_PREEMPTIVE)
	if (memorySegmentMutex && !hasMemorySegmentMutex) {
		omrthread_monitor_exit(memorySegmentMutex);
	}
#endif
}

/*
 * _readWriteAreaHeaderIsReadOnly is protected by the readWriteAreaMutex.
 * This function can only be called when this mutex is acquired.
 */
BOOLEAN
SH_CompositeCacheImpl::isReadWriteAreaHeaderReadOnly()
{
	return _readWriteAreaHeaderIsReadOnly;
}

SH_CompositeCacheImpl*
SH_CompositeCacheImpl::getNext(void)
{
	return _next;
}

void
SH_CompositeCacheImpl::setNext(SH_CompositeCacheImpl* next)
{
	_next = next;
}

J9MemorySegment*
SH_CompositeCacheImpl::getCurrentROMSegment(void)
{
	return _currentROMSegment;
}

void
SH_CompositeCacheImpl::setCurrentROMSegment(J9MemorySegment* segment)
{
	_currentROMSegment = segment;
}

#if defined(J9SHR_CACHELET_SUPPORT)

void
SH_CompositeCacheImpl::setContainsCachelets(J9VMThread* currentThread)
{
	if (!_started){
		return;
	}
	/* If the cache contains cachelets, disable segment page protection as this will be dealt with by the cachelets themselves */
	if (_doSegmentProtect) {
		notifyPagesRead(CASTART(_theca), SEGUPDATEPTR(_theca), DIRECTION_BACKWARD, true);
		_doSegmentProtect = false;
	}
	if (!isRunningReadOnly()) {
		unprotectHeaderReadWriteArea(currentThread, false);
		_theca->containsCachelets = 1;
		protectHeaderReadWriteArea(currentThread, false);
	}
}

#endif /* J9SHR_CACHELET_SUPPORT */

bool
SH_CompositeCacheImpl::getContainsCachelets(void)
{
	return (_theca->containsCachelets == 1);
}

SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::getFirstROMClassAddress(bool isNested)
{
	BlockPtr returnVal = (BlockPtr)getBaseAddress();

	if (isNested || getContainsCachelets()) {
		return returnVal + sizeof(J9SharedCacheHeader);
	} else {
		return returnVal;
	}
}

/* Note: In case of failure callers are expected to be able to get the error code using portlibrary API j9error_last_error_number(). */
IDATA
SH_CompositeCacheImpl::setRegionPermissions(J9PortLibrary* portLibrary, void *address, UDATA length, UDATA flags)
{
#if defined(J9ZTPF)
	return 0;
#else
	SH_OSCache* oscache = ((_parent) ? _parent->_oscache : _oscache);
	PORT_ACCESS_FROM_PORT(_portlib);

	if (0 != length) {
		/* AttachedDataTest.cpp does not have valid oscache */
		if ((NULL != oscache) && (UnitTest::ATTACHED_DATA_TEST != UnitTest::unitTest)) {
			return oscache->setRegionPermissions(portLibrary, address, length, flags);
		} else {
			return j9mmap_protect(address, length, flags);
		}
	} else {
		/* Win implementation of j9mmap_protect() fails if length is 0. We don't want to return failure in such case. */
		return 0;
	}
#endif /* defined(J9ZTPF) */
}

bool
SH_CompositeCacheImpl::isStarted(void)
{
	return _started;
}

bool
SH_CompositeCacheImpl::getIsNoLineNumberEnabled(void)
{
	/*_start is not checked because this method is called during startup 
	 * from SH_CompositeCacheImpl::setCacheAreaBoundaries
	 */
	if(NULL == this->_theca) {
		return false;
	}
	return ((this->_theca->extraFlags & J9SHR_EXTRA_FLAGS_NO_LINE_NUMBERS) != 0);
}

/**
 * This function is used to get whether shared cache has content with line numbers or not
 * @param	void
 * @return 	true if shared cache has at least one ROM Method without line numbers,
 * 			false otherwise.
 */
bool
SH_CompositeCacheImpl::getIsNoLineNumberContentEnabled(void)
{
	if(NULL == this->_theca) {
		return false;
	}
	return (0 != (this->_theca->extraFlags & J9SHR_EXTRA_FLAGS_NO_LINE_NUMBER_CONTENT));
}

/**
 * This function sets a bit in extraFlags indicating that there is content without line numbers in the shared cache.
 * This function should be called once a ROM Class is added to shared cache without line numbers.
 */
void
SH_CompositeCacheImpl::setNoLineNumberContentEnabled(J9VMThread *currentThread)
{
	if(NULL == this->_theca) {
		return;
	}
	setCacheHeaderExtraFlags(currentThread, J9SHR_EXTRA_FLAGS_NO_LINE_NUMBER_CONTENT);
}

/**
 * This function is used to get whether shared cache has content with line numbers or not
 * @param	void
 * @return 	true if shared cache has at least one ROM Method with line numbers,
 * 			false otherwise.
 */
bool
SH_CompositeCacheImpl::getIsLineNumberContentEnabled(void)
{
	if(NULL == this->_theca) {
		return false;
	}
	return (0 != (this->_theca->extraFlags & J9SHR_EXTRA_FLAGS_LINE_NUMBER_CONTENT));
}

/**
 * This function sets a bit in extraFlags indicating that there is content with line numbers in the shared cache.
 * This function should be called once a ROM Class is added to shared cache with line numbers.
 */

void
SH_CompositeCacheImpl::setLineNumberContentEnabled(J9VMThread* currentThread)
{
	if(NULL == this->_theca) {
		return;
	}
	setCacheHeaderExtraFlags(currentThread, J9SHR_EXTRA_FLAGS_LINE_NUMBER_CONTENT);
}

bool
SH_CompositeCacheImpl::getIsBCIEnabled(void)
{
	Trc_SHR_Assert_True(NULL != this->_theca);
	return (0 != (this->_theca->extraFlags & J9SHR_EXTRA_FLAGS_BCI_ENABLED));
}

/**
 * This function sets a bit in extraFlags in cache header indicating AOT Header has been added to the cache.
 * This function should only be called once when the AOT Header is first added to the cache.
 * 
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * 
 * @pre The caller must hold the shared classes cache write mutex
 */
void
SH_CompositeCacheImpl::setAOTHeaderPresent(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True(hasWriteMutex(currentThread));
	setCacheHeaderExtraFlags(currentThread, J9SHR_EXTRA_FLAGS_AOT_HEADER_PRESENT);
}

/**
 * This function returns whether AOT Header has been added to the cache or not.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * 
 * @return 	true if AOT Header has been added to the cache,
 * 			false otherwise.
 *
 * @pre The caller must hold the shared classes cache write mutex
 */
bool
SH_CompositeCacheImpl::isAOTHeaderPresent(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True((NULL != this->_theca) && hasWriteMutex(currentThread));
	return (0 != (this->_theca->extraFlags & J9SHR_EXTRA_FLAGS_AOT_HEADER_PRESENT));
}

/**
 * This function returns true if cache was created with "-Xshareclasses:restrictClasspaths",
 * false otherwise
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return 	true if J9SHR_EXTRA_FLAGS_RESTRICT_CLASSPATHS is set in shared cache header, false otherwise.
 *
 */
bool
SH_CompositeCacheImpl::isRestrictClasspathsSet(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True(NULL != this->_theca);
	return (0 != (this->_theca->extraFlags & J9SHR_EXTRA_FLAGS_RESTRICT_CLASSPATHS));
}

void
SH_CompositeCacheImpl::setCacheHeaderExtraFlags(J9VMThread* currentThread, UDATA extraFlags)
{
	Trc_SHR_Assert_True(NULL != this->_theca);
	if (true == _started) {
		unprotectHeaderReadWriteArea(currentThread, false);
	}
	this->_theca->extraFlags |= extraFlags;
	if (true == _started) {
		protectHeaderReadWriteArea(currentThread, false);
	}
}

UDATA
SH_CompositeCacheImpl::getOSPageSize(void)
{
	if (!_started) {
		return 0;
	}
	return _osPageSize;
}

#if defined(J9SHR_CACHELET_SUPPORT)

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
UDATA
SH_CompositeCacheImpl::computeDeployedReadWriteOffsets(J9VMThread* currentThread, SH_CompositeCacheImpl* ccHead)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	BlockPtr destSegmentStart;
	UDATA totalReadWriteLen;
	SH_CompositeCacheImpl* walk = ccHead; /* supercache iterator */

	if (!_started || !ccHead->isStarted() || (_scan != (ShcItemHdr*)CCFIRSTENTRY(_theca))) {
		return 0;
	}

	destSegmentStart = CASTART(_theca);
	totalReadWriteLen = 0;

	/* copy into the serialized cache */
	while (walk) {
		SH_CompositeCacheImpl* next = walk->getNext();
		BlockPtr srcSegmentStart, srcSegmentEnd;
		BlockPtr srcReadWriteStart, srcReadWriteEnd;
		UDATA srcSegmentLen, srcReadWriteLen;

		srcReadWriteStart = (BlockPtr)walk->getReadWriteBase();
		srcReadWriteEnd = (BlockPtr)walk->getReadWriteAllocPtr();
		srcReadWriteLen = srcReadWriteEnd - srcReadWriteStart;

		srcSegmentStart = (BlockPtr)walk->getBaseAddress();
		srcSegmentEnd = (BlockPtr)walk->getSegmentAllocPtr();
		srcSegmentLen = srcSegmentEnd - srcSegmentStart;

		totalReadWriteLen += srcReadWriteLen;
		if (totalReadWriteLen > READWRITEAREASIZE(_theca)) {
			return 0;
		}

		j9tty_printf(PORTLIB, "%x %x %x %x %x\n", (UDATA)destSegmentStart, (UDATA)CASTART(_theca), walk->getFreeReadWriteBytes(),
				READWRITEAREASIZE(_theca), totalReadWriteLen);
		walk->setDeployedReadWriteOffset((UDATA)destSegmentStart - (UDATA)CASTART(_theca)
				- walk->getFreeReadWriteBytes()
				+ (READWRITEAREASIZE(_theca) - totalReadWriteLen));
		destSegmentStart += srcSegmentLen;
		walk = next;
	}

	return 1;
}
#endif

/**
 * Called by the serialized (deployed) cache to pull all data from the chained deployment cache.
 * @param[in] self serialized cache
 * @param[in] ccHead chain of supercaches
 * @param[out] metadataOffset
 *
 * @retval true ok
 * @retval false error
 */
bool
SH_CompositeCacheImpl::copyFromCacheChain(J9VMThread* currentThread, SH_CompositeCacheImpl* ccHead, IDATA* metadataOffset)
{
	BlockPtr destSegmentStart = NULL, destMetaStart = NULL, srcMetaStart = NULL;
	UDATA srcMetaLen, totalSegmentLen;
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	BlockPtr destReadWriteStart;
	UDATA totalReadWriteLen;
#endif
	SH_CompositeCacheImpl* walk = ccHead; /* supercache iterator */

	if (!_started || !ccHead->isStarted() || (_scan != (ShcItemHdr*)CCFIRSTENTRY(_theca))) {
		return false;
	}

	destSegmentStart = CASTART(_theca);
	totalSegmentLen = 0;
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	destReadWriteStart = READWRITEAREASTART(_theca);
	totalReadWriteLen = 0;
#endif

	/* copy into the serialized cache */
	while (walk) {
		SH_CompositeCacheImpl* next = walk->getNext();
		BlockPtr srcSegmentStart, srcSegmentEnd, srcMetaEnd;
		UDATA srcSegmentLen;
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
		BlockPtr srcReadWriteStart, srcReadWriteEnd;
		UDATA srcReadWriteLen;

		srcReadWriteStart = (BlockPtr)walk->getReadWriteBase();
		srcReadWriteEnd = (BlockPtr)walk->getReadWriteAllocPtr();
		srcReadWriteLen = srcReadWriteEnd - srcReadWriteStart;
		memcpy(destReadWriteStart, srcReadWriteStart, srcReadWriteLen);
		destReadWriteStart += srcReadWriteLen;
#endif

		srcSegmentStart = (BlockPtr)walk->getBaseAddress();
		srcSegmentEnd = (BlockPtr)walk->getSegmentAllocPtr();
		srcSegmentLen = srcSegmentEnd - srcSegmentStart;
		memcpy(destSegmentStart, srcSegmentStart, srcSegmentLen);
		if (next == NULL) {
			srcMetaStart = (BlockPtr)walk->getMetaAllocPtr();
			srcMetaEnd = (BlockPtr)walk->getCacheEndAddress();
			srcMetaLen = srcMetaEnd - srcMetaStart;
			destMetaStart = CADEBUGSTART(_theca) - srcMetaLen;
			memcpy(destMetaStart, srcMetaStart, srcMetaLen);
		} else {
			srcMetaStart = srcMetaEnd = NULL;
			srcMetaLen = 0;
		}

		totalSegmentLen += srcSegmentLen;
		if ((totalSegmentLen + srcMetaLen) >= FREEBYTES(_theca)) {
			return false;
		}

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
		totalReadWriteLen += srcReadWriteLen;
		if (totalReadWriteLen > READWRITEAREASIZE(_theca)) {
			return false;
		}
#endif

		walk->setDeployedOffset((UDATA)destSegmentStart - (UDATA)srcSegmentStart);
		destSegmentStart += srcSegmentLen;
		walk = next;
	}

	_theca->updateSRP -= srcMetaLen;
	_theca->segmentSRP += totalSegmentLen;
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	_theca->readWriteSRP += totalReadWriteLen;
#endif
	_theca->updateCount = ccHead->_theca->updateCount;
	_theca->containsCachelets = ccHead->_theca->containsCachelets;

	*metadataOffset = (UDATA)destMetaStart - (UDATA)srcMetaStart;
	/* TODO: Note that currently this function doesn't leave the cache in a state that it can be written to */
	return true;
}

SH_CompositeCacheImpl*
SH_CompositeCacheImpl::getParent(void)
{
	return _parent;
}

IDATA
SH_CompositeCacheImpl::getDeployedOffset(void)
{
	return _deployedOffset;
}

void
SH_CompositeCacheImpl::setDeployedOffset(IDATA offset)
{
	_deployedOffset = offset;
}

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
IDATA
SH_CompositeCacheImpl::getDeployedReadWriteOffset(void)
{
	return _deployedReadWriteOffset;
}

void
SH_CompositeCacheImpl::setDeployedReadWriteOffset(IDATA offset)
{
	_deployedReadWriteOffset = offset;
}
#endif

/**
 * Fixup the AOT code in this cachelet. 
 * Does not descend into nested cachelets.
 * This renders the cache unusable to the current process.
 * @param[in] self the cachelet
 * @param[in] currentThread the current VMThread
 * @param[in] serializedROMClassStartAddress address of the first ROMClass segment in the serialized (deployed) cache
 * @retval 0 success
 * @retval -1 failure
 */
IDATA
SH_CompositeCacheImpl::fixupSerializedCompiledMethods(J9VMThread* currentThread, void* serializedROMClassStartAddress)
{
	IDATA rc = 0;

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	J9JITConfig *jitConfig = currentThread->javaVM->jitConfig;
	CompiledMethodWrapper *wrapper;
	ShcItem* it;

	if (!jitConfig) {
		return rc;
	}
	if (!jitConfig->updateROMClassOffsetsInAOTMethod) {
		return -1;
	}

	/* walk all items in the cache */
	/* will not skip over stale items */
	this->findStart(currentThread);
	while (NULL != (it = (ShcItem*)this->nextEntry(currentThread, NULL))) {
		if (TYPE_COMPILED_METHOD == ITEMTYPE(it)) {
			wrapper = (CompiledMethodWrapper*)ITEMDATA(it);
			if (jitConfig->updateROMClassOffsetsInAOTMethod(currentThread->javaVM, CMWDATA(wrapper), (J9ROMMethod*)CMWROMMETHOD(wrapper), serializedROMClassStartAddress)) {
				rc = -1;
				break;
			}
		}
	}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

	return rc;
}

#if 0
/**
 * Grow the read write area, and discard all metadata.
 */
void
SH_CompositeCacheImpl::growCacheInPlace(UDATA rwGrowth, UDATA freeGrowth)
{
	_theca->totalBytes += rwGrowth;
	_theca->readWriteBytes += rwGrowth;
	_theca->updateSRP += rwGrowth;
	_theca->segmentSRP += rwGrowth;
	_theca->crcValue = 0;
	_theca->crcValid = 0;

	_prevScan = _scan = (ShcItemHdr*)CCFIRSTENTRY(_theca);
}
#endif

SH_CompositeCacheImpl::BlockPtr
SH_CompositeCacheImpl::getNestedMemory(void)
{
	return _nestedMemory;
}

/**
 * Count the segments in this cache(let).
 * ROMClass segments are contiguously adjacent to each other in the write area of the cache.
 * @param[in] currentThread
 * @returns number of segments
 * 
 * @see writeROMSegmentMetadata
 */
UDATA
SH_CompositeCacheImpl::countROMSegments(J9VMThread* currentThread)
{
	UDATA count = 0;
	J9AVLTree* tree = &currentThread->javaVM->classMemorySegments->avlTreeData;
	const J9MemorySegment* segment = NULL;
	UDATA baseAddress;

	if (!getCurrentROMSegment()) {
		return 0;
	}

	baseAddress = (UDATA)getBaseAddress();
	do {
		segment = (const J9MemorySegment*)avl_search(tree, baseAddress);
		if (!segment) {
			break;
		}

		/* validate that the segment is inside the cache */
		Trc_SHR_Assert_False((segment->baseAddress < getBaseAddress()) || (segment->heapTop > getCacheLastEffectiveAddress()));

		if (segment->type == (MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_ROM | MEMORY_TYPE_FIXEDSIZE))	{
			++count;
		}
		baseAddress = (UDATA)segment->heapTop;
	} while (segment != getCurrentROMSegment());

	return count;
}

/**
 * Write this cache(let)'s segments into the cachelet metadata when serializing the cache.
 * ROMClass segments are contiguously adjacent to each other in the write area of the cache.
 * Each cache(let)'s segment metadata consists of an array of UDATA-sized lengths.
 * 
 * @param[in] currentThread
 * @param[in] numSegments Number of segment metadata elements allocated for this function to populate.
 * @param[in] dest Address of segment metadata elements in serialized cache.
 * @param[out] lastSegmentAlloc Allocated size of last segment.
 * @returns number of segments written out
 * 
 * @see countROMSegments
 */
UDATA
SH_CompositeCacheImpl::writeROMSegmentMetadata(J9VMThread* currentThread, UDATA numSegments, BlockPtr dest,
		UDATA* lastSegmentAlloc)
{
	J9AVLTree* tree = &currentThread->javaVM->classMemorySegments->avlTreeData;
	const J9MemorySegment* segment = NULL;
	UDATA baseAddress;
	UDATA* lengthArray = (UDATA*)dest;
	UDATA segmentsWritten = 0;

	*lastSegmentAlloc = 0;

	if (!getCurrentROMSegment()) {
		return 0;
	}

	baseAddress = (UDATA)getBaseAddress();
	do {
		segment = (const J9MemorySegment*)avl_search(tree, baseAddress);
		if (!segment) {
			break;
		}

		/* validate that the segment is inside the cache */
		Trc_SHR_Assert_False((segment->baseAddress < getBaseAddress()) || (segment->heapTop > getCacheLastEffectiveAddress()));

		if (segment->type == (MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_ROM | MEMORY_TYPE_FIXEDSIZE)) {
			*lengthArray = segment->heapTop - segment->baseAddress;

			if (segment == getCurrentROMSegment()) {
				/* this is the condition for exiting the loop, so this is the last segment */
				*lastSegmentAlloc = segment->heapAlloc - segment->heapBase;
			}

			++lengthArray;
			++segmentsWritten;

			Trc_SHR_Assert_True(segmentsWritten <= numSegments);
		}
		baseAddress = (UDATA)segment->heapTop;
	} while (segment != getCurrentROMSegment());

	return segmentsWritten;
}

/**
 * Acquire cachelet startup monitor.
 * @param[in] currentThread
 * @retval 0 success
 * @retval -1 lock acquire failed
 * @retval -2 lock allocation failed
 * @see unlockStartupMonitor
 * @bug HACK. THIS IS TOTALLY UNSAFE FOR WRITEABLE CACHES.
 */
IDATA
SH_CompositeCacheImpl::lockStartupMonitor(J9VMThread* currentThread)
{
	if (!_startupMonitor) {
		return -2;
	}
	if (0 != omrthread_monitor_enter(_startupMonitor)) {
		return -1;
	}
	return 0;
}

/**
 * Release cachelet startup monitor.
 * @param[in] currentThread
 * @see lockStartupMonitor
 * @bug HACK. THIS IS TOTALLY UNSAFE FOR WRITEABLE CACHES.
 */
void
SH_CompositeCacheImpl::unlockStartupMonitor(J9VMThread* currentThread)
{
	if (_startupMonitor) {
		omrthread_monitor_exit(_startupMonitor);
	}
}

#endif /* J9SHR_CACHELET_SUPPORT */


/**
 *	Set the string table initialized state
 * @param[in] isInitialized
 *
 * @return void
 */
void
SH_CompositeCacheImpl::setStringTableInitialized(bool isInitialized)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
	} else {
		if (isInitialized) {
			_theca->readWriteFlags |= J9SHR_HEADER_STRING_TABLE_INITIALIZED;
		} else {
			_theca->readWriteFlags &= ~J9SHR_HEADER_STRING_TABLE_INITIALIZED;
		}
	}
}

/**
 * 	Get the string table area initialized state
 *
 * @return true if the string table initialized flag is set in the readWriteFlags, false otherwise
 */
bool
SH_CompositeCacheImpl::isStringTableInitialized(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	return (0 != (_theca->readWriteFlags & J9SHR_HEADER_STRING_TABLE_INITIALIZED));
}

/**
 * Allocates debug attribute data for a ROMClass
 *
 * @param [in] currentThread the thread calling this function
 * @param [in] classnameLength ROMClass class name length
 * @param [in] classnameData ROMClass class name
 * @param [in] sizes structure containing sizes of ROMClass parts
 * @param [out] pieces results of calling this function
 *
 * @return 0 if this function passes, otherwise this function has failed
 *
 * THREADING: We assume the Segment Mutex, String Table Lock, and Write Area Lock is held by the transaction.
 */
IDATA
SH_CompositeCacheImpl::allocateClassDebugData(J9VMThread* currentThread, U_16 classnameLength, const char* classnameData, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces)
{
	U_32 sizeToBeAllocated = sizes->lineNumberTableSize + sizes->localVariableTableSize;
	IDATA retval = -1;
	U_32 softMaxValue = _theca->softMaxBytes;
	U_32 usedBytes = getUsedBytes();
	bool enoughAvailableSpace = ((sizeToBeAllocated + usedBytes) <= softMaxValue);

	if (!enoughAvailableSpace) {
		/* Do not need to set J9SHR_AVAILABLE_SPACE_FULL here */
		Trc_SHR_Assert_True((softMaxValue - usedBytes) >= CC_MIN_SPACE_BEFORE_CACHE_FULL);
		Trc_SHR_CC_allocateClassDebugData_EventSoftMaxBytesReached(currentThread, softMaxValue);
		return retval;
	}

	retval = _debugData->allocateClassDebugData(currentThread, classnameLength, classnameData, sizes, pieces, (AbstractMemoryPermission*)this);
	if ((-1 == retval) && (_debugData->getFailureReason() != NO_CORRUPTION)) {
		setCorruptCache(currentThread, _debugData->getFailureReason(), _debugData->getFailureValue());
	}
	return retval;
}

/**
 * Roll back uncommitted changes made by the last call too 'allocateClassDebugData()'
 *
 * @param [in] currentThread the thread calling this function
 * @param [in] classnameLength ROMClass class name length
 * @param [in] classnameData ROMClass class name
 *
 * @return void
 *
 * THREADING: We assume the Segment Mutex, String Table Lock, and Write Area Lock is held by the transaction.
 */
void
SH_CompositeCacheImpl::rollbackClassDebugData(J9VMThread* currentThread, U_16 classnameLength, const char* classnameData)
{
	_debugData->rollbackClassDebugData(currentThread, classnameLength, classnameData, (AbstractMemoryPermission*)this);
}

/**
 * Commited changes made by the last call too 'allocateClassDebugData()'
 *
 * @param [in] currentThread the thread calling this function
 * @param [in] classnameLength ROMClass class name length
 * @param [in] classnameData ROMClass class name
 *
 * @return void
 *
 * THREADING: We assume the Segment Mutex, String Table Lock, and Write Area Lock is held by the transaction.
 */
void
SH_CompositeCacheImpl::commitClassDebugData(J9VMThread* currentThread, U_16 classnameLength, const char* classnameData)
{
	bool ok = false;
	U_32 storedDebugDataBytes = _debugData->getStoredDebugDataBytes();
	/* set _storedDebugDataBytes before calling commitClassDebugData(), as getStoredDebugDataBytes() returns 0 after debugData is committed */

	ok = _debugData->commitClassDebugData(currentThread, classnameLength, classnameData, (AbstractMemoryPermission*)this);
	if ((false == ok) && (_debugData->getFailureReason() != NO_CORRUPTION)) {
		setCorruptCache(currentThread, _debugData->getFailureReason(), _debugData->getFailureValue());
	} else {
		_totalStoredBytes += storedDebugDataBytes;
	}
	return;
}

/**
 * Get start of class debug data area
 *
 * @return void * ptr to start of class debug data area
 */
void *
SH_CompositeCacheImpl::getClassDebugDataStartAddress(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	return _debugData->getDebugAreaStartAddress();
}

/**
 * Private helper used to initializes private class member _commonCCInfo memory.
 * This structure is shared by all composite caches within a JVM.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::initCommonCCInfoHelper()
{
	/*All fields in this structure are initialized to 0*/
	memset(_commonCCInfo, 0, sizeof(J9ShrCompositeCacheCommonInfo));
}

/**
 * Starts the SH_CompositeCache.
 *
 * The method creates the CompositeCache for an existing attached shared cache.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * @param [in] config The shared class config
 * @param [in] attachedMemory  The attached shared memory region.
 * @param [in] runtimeFlags_  Runtime flags
 * @param [in] verboseFlags_  Flags controlling the level of verbose messages to be issued
  *
 * @return 0 for success, -1 for failure and -2 for corrupt.
 * @retval CC_STARTUP_OK (0) success
 * @retval CC_STARTUP_FAILED (-1)
 * @retval CC_STARTUP_CORRUPT (-2)
 */
IDATA
SH_CompositeCacheImpl::startupForStats(J9VMThread* currentThread, SH_OSCache * oscache, U_64 * runtimeFlags, UDATA verboseFlags)
{
	IDATA retval = CC_STARTUP_OK;
	const char* fnName = "CC startupForStats";
	bool cacheHasIntegrity;
	BlockPtr attachedMemory = NULL;

	if (_started == true) {
		retval = CC_STARTUP_OK;
		goto done;
	}

	_oscache = oscache;
	_osPageSize = _oscache->getPermissionsRegionGranularity(_portlib);
	attachedMemory = (BlockPtr)oscache->getAttachedMemory();

	_runtimeFlags = runtimeFlags;

	_readOnlyOSCache = _oscache->isRunningReadOnly();
	if (_readOnlyOSCache) {
		_commonCCInfo->writeMutexID = CC_READONLY_LOCK_VALUE;
		_commonCCInfo->readWriteAreaMutexID = CC_READONLY_LOCK_VALUE;
	} else {
		IDATA lockID;
		if ((lockID = _oscache->getWriteLockID()) >= 0) {
			_commonCCInfo->writeMutexID = (U_32)lockID;
		} else {
			retval = CC_STARTUP_FAILED;
			goto done;
		}
		if ((lockID = _oscache->getReadWriteLockID()) >= 0) {
			_commonCCInfo->readWriteAreaMutexID = (U_32)lockID;
		} else {
			retval = CC_STARTUP_FAILED;
			goto done;
		}
	}

	if (omrthread_tls_alloc(&(_commonCCInfo->writeMutexEntryCount)) != 0) {
		retval = CC_STARTUP_FAILED;
		goto done;
	}

	_theca = (J9SharedCacheHeader*)attachedMemory;

	if (isCacheInitComplete() == false) {
		retval = CC_STARTUP_CORRUPT;
		goto done;
	}

	if (enterWriteMutex(currentThread, false, fnName) != 0) {
		retval = CC_STARTUP_FAILED;
		goto done;
	}

	/* _verboseFlags = J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_PAGES; */
			
	if (!oscache->isRunningReadOnly() && _theca->roundedPagesFlag && ((currentThread->javaVM->sharedCacheAPI->runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT) != 0)) {
		/* If mprotection is globally enabled, protect the entire cache body. Although
		 * we have the write lock, the header may still be written by other JVMs to
		 * take the read lock. The read-write area can also be written since we don't
		 * have the read-write lock.
		 */
		*_runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT;
		notifyPagesRead(CASTART(_theca), CAEND(_theca), DIRECTION_FORWARD, true);
	}

	/* _started is set to true if the write area mutex was entered. Because _started 
	 * is used in ::shutdownForStats() to decide if '::exitWriteMutex()' needs to be called,
	 * _started will still be set if any of the below code finds the cache to be corrupt.
	 * 
	 * All other failures after this point are a result of a corrupt cache.
	 */
	_started = true;

	if (!checkCacheCRC(&cacheHasIntegrity, NULL)) {
		retval = CC_STARTUP_CORRUPT;
		goto done;
	}

	/* Needs to be set or 'SH_CompositeCacheImpl::next' will always return nothing.
	 * This is a problem during 'SH_CacheMap::readCache' when collecting stats for
	 * the cache.
	 */
	_prevScan = _scan = (ShcItemHdr*)CCFIRSTENTRY(_theca);

	if (_debugData->Init(currentThread, _theca, (AbstractMemoryPermission *)this, verboseFlags, _runtimeFlags, true) == false) {
		retval = CC_STARTUP_CORRUPT;
		goto done;
	}

done:
	/*Calling ::shutdownForStats() calls 'exitWriteMutex()'*/
	return retval;
}

/**
 * Shut down a CompositeCache started with startupForStats().
 *
 * THREADING: Only ever single threaded
 *
 * @param [in] currentThread  The current thread
 *
 * @return 0 on success or -1 for failure
 */
IDATA
SH_CompositeCacheImpl::shutdownForStats(J9VMThread* currentThread)
{
	const char* fnName = "CC shutdownForStats";
	IDATA retval = 0;
	/* _started is only set to true if enterWriteMutex() was called successfully. 
	 * It may be set even if the cache is corrupt, see comments in ::startupForStats for more info.
	 */
	if (_started == true) {

		if ((*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_MPROTECT) != 0) {
			/* If the cache was protected, unprotect it */
			notifyPagesRead(CASTART(_theca), CAEND(_theca), DIRECTION_FORWARD, false);
		}

		if (exitWriteMutex(currentThread, fnName, false) != 0) {
			_started = false;
			retval = -1;
			goto done;
		}
		_started = false;
	}

	if (_commonCCInfo->writeMutexEntryCount != 0) {
		if (omrthread_tls_free(_commonCCInfo->writeMutexEntryCount) != 0) {
			retval = -1;
			goto done;
		}
		_commonCCInfo->writeMutexEntryCount = 0;
	}
	done:
	return retval;
}

/**
 * Has the cache memory been initialized?
 *
 * THREADING: Only ever single threaded
 *
 * @return true if the cache memory has been initialized
 */
bool
SH_CompositeCacheImpl::isCacheInitComplete(void)
{
	return ((_theca != NULL) && (0 != (_theca->ccInitComplete & CC_STARTUP_COMPLETE)));
}

/**
 * The number of OS write locks required for a rw cache.
 *
 * THREADING: Only ever single threaded
 *
 * @return IDATA
 */
IDATA
SH_CompositeCacheImpl::getNumRequiredOSLocks() {
	return CC_NUM_WRITE_LOCKS;
}

#if defined(J9SHR_CACHELET_SUPPORT)
/**
 * Starts the SH_CompositeCache for a cachelet.
 *
 * The method starts the CompositeCache for an existing cachelet.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * 
 * THREADING: Current thread owns write lock
 *
 * @return 0 for success, and -2 for corrupt.
 * @retval CC_STARTUP_OK (0) success
 * @retval CC_STARTUP_CORRUPT (-2)
 */
IDATA 
SH_CompositeCacheImpl::startupNestedForStats(J9VMThread* currentThread)
{
	IDATA retval = CC_STARTUP_OK;

	if (_started == true) {
		goto done;
	}
	
	Trc_SHR_Assert_True(_parent->_commonCCInfo != NULL);
	Trc_SHR_Assert_Equals(currentThread, _parent->_commonCCInfo->hasWriteMutexThread);

	_commonCCInfo = _parent->_commonCCInfo;
	_oscache = _parent->_oscache;
	_osPageSize = _oscache->getPermissionsRegionGranularity(_portlib);

	_theca = (J9SharedCacheHeader*)_nestedMemory;
	if (this->isCacheInitComplete() == false) {
		retval = CC_STARTUP_CORRUPT;
		goto done;
	}
	_prevScan = _scan = (ShcItemHdr*)CCFIRSTENTRY(_theca);
	
	done:
	if (retval == CC_STARTUP_OK) {
		_started = true;
	}
	return retval;

}
#endif /*J9SHR_CACHELET_SUPPORT*/

/*
 * Returns corruption context that includes a corruption code and the corrupt value.
 *
 * @param [out] code	if non-null, it is populated with a code indicating corruption type.
 * @param [out] value	if non-null, it is populated with a value to be interpreted depending on code.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::getCorruptionContext(IDATA *code, UDATA *value) {
	IDATA corruptionCode = NO_CORRUPTION;
	UDATA corruptValue;
	SH_CompositeCacheImpl *ccToUse;

	ccToUse = ((_ccHead == NULL) ? ((_parent == NULL) ? this : _parent) : _ccHead);

	/* If the cache was marked as corrupt after startup (i.e. corruptFlag is set in cache header),
	 * then read corruption context from the cache header
	 * else get it using SH_OSCache APIs.
	 */
	if (ccToUse->_theca != NULL) {
		if (0 != ccToUse->_theca->corruptFlag) {
			corruptionCode = ccToUse->_theca->corruptionCode;
			corruptValue = ccToUse->_theca->corruptValue;
		}
	}
	if (NO_CORRUPTION == corruptionCode) {
		/* skip this during unit testing of CompositeCache as _oscache is always NULL. */
		if ((UnitTest::NO_TEST == UnitTest::unitTest) || (UnitTest::CORRUPT_CACHE_TEST == UnitTest::unitTest)) {
			ccToUse->_oscache->getCorruptionContext(&corruptionCode, &corruptValue);
		}
	}

	if (NULL != code) {
		*code = corruptionCode;
	}
	if (NULL != value) {
		*value = corruptValue;
	}

	return;
}

/*
 * Sets corruption context that specifies a corruption code and the corrupt value.
 *
 * @param [in] code	used to set SH_OSCache::_corruptionCode
 * @param [in] value used to set SH_OSCache::_corruptValue
 *
 * @return void
 */
void
SH_CompositeCacheImpl::setCorruptionContext(IDATA corruptionCode, UDATA corruptValue) {

	if ((UnitTest::NO_TEST == UnitTest::unitTest) || (UnitTest::CORRUPT_CACHE_TEST == UnitTest::unitTest)) {
		_oscache->setCorruptionContext(corruptionCode, corruptValue);
	}
}

/**
 * Sets runtime cache full flag corresponding to cache full flags set in cache header.
 * It should hold either of write mutex or refresh mutex.
 *
 * Note that this method takes actions only for those cache full flags that are not already set.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return void
 */
void
SH_CompositeCacheImpl::setRuntimeCacheFullFlags(J9VMThread* currentThread)
{
	PORT_ACCESS_FROM_PORT(_portlib);
	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	if (0 != (_theca->cacheFullFlags & J9SHR_ALL_CACHE_FULL_BITS)) {
		bool allRuntimeCacheFullFlagsSet = false;
		U_64 cacheFullFlags = 0;

		omrthread_monitor_enter(_runtimeFlagsProtectMutex);

		/* don't set DENY_CACHE_UPDATES - we still need updates for timestamp optimizations */
		if ((0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) &&
			(0 != (_theca->cacheFullFlags & J9SHR_BLOCK_SPACE_FULL))
		) {
			cacheFullFlags |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
			Trc_SHR_CC_setRuntimeCacheFullFlags_BlockSpaceFull(currentThread);
		}
		if ((0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) &&
			(0 != (_theca->cacheFullFlags & J9SHR_AOT_SPACE_FULL))
		) {
			cacheFullFlags |= J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL;
			Trc_SHR_CC_setRuntimeCacheFullFlags_AOTSpaceFull(currentThread);
		}
		if ((0 == (*_runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) &&
			(0 != (_theca->cacheFullFlags & J9SHR_JIT_SPACE_FULL))
		) {
			cacheFullFlags |= J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL;
			Trc_SHR_CC_setRuntimeCacheFullFlags_JITSpaceFull(currentThread);
		}
		if ((J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) &&
			(J9_ARE_ANY_BITS_SET(_theca->cacheFullFlags, J9SHR_AVAILABLE_SPACE_FULL))
		) {
			cacheFullFlags |= J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL;
			Trc_SHR_CC_setRuntimeCacheFullFlags_availableSpaceFull(currentThread);
		}

		*_runtimeFlags |= cacheFullFlags;

		/* Reset the writeHash field in the cache as no more updates can occur */
		if (J9_ARE_ANY_BITS_SET(cacheFullFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL | J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
			if ((true == _useWriteHash) && (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION))) {
				this->setWriteHash(currentThread, 0);
				_reduceStoreContentionDisabled = true;
			}
			*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION;
			_useWriteHash = false;
		}

		if (0 != cacheFullFlags) {
			if (true == isAllRuntimeCacheFullFlagsSet()) {
				_debugData->protectUnusedPages(currentThread, (AbstractMemoryPermission*)this);
				protectLastUnusedPages(currentThread);
				allRuntimeCacheFullFlagsSet = true;
			} else if (J9_ARE_ANY_BITS_SET(cacheFullFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
				/* At this point no more ROMClass can be added.
				 * This implies no new data can be added in class debug area.
				 * Partially filled page and unused pages in class debug area can now be protected.
				 */
				_debugData->protectUnusedPages(currentThread, (AbstractMemoryPermission*)this);
			} else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL | J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL| J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
				protectPartiallyFilledPages(currentThread, true, true, true, false);
			} else if (J9_ARE_ANY_BITS_SET(cacheFullFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
				/* Only protect partially filled pages in debug area */
				protectPartiallyFilledPages(currentThread, false, false, true, false);
			}
		}

		omrthread_monitor_exit(_runtimeFlagsProtectMutex);

		if (0 != cacheFullFlags) {
			if (true == allRuntimeCacheFullFlagsSet) {
				/* Only report full cache to std-err with verbose enabled */
				CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_CM_WARN_FULL_CACHE, _cacheName);
			} else {
				if (0 != (cacheFullFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
					if (0 != (cacheFullFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED)) {
						/* If we requested a growable cache, this is an error. */
						CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT, J9NLS_INFO, J9NLS_SHRC_CM_WARN_FULL_CACHE, _cacheName);
					} else {
						/* Only report Block data space full to std-err with verbose enabled */
						CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_CM_WARN_BLOCK_SPACE_FULL, _cacheName);
					}
				}
				if (J9_ARE_ALL_BITS_SET(cacheFullFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
					/* Only report cache soft full to std-err with verbose enabled */
					CC_TRACE2(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_CC_WARN_AVAILABLE_SPACE_FULL, _cacheName, OPTION_ADJUST_SOFTMX_EQUALS);
				}
				if (0 != (cacheFullFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
					/* Only report AOT data space full to std-err with verbose enabled */
					CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_CC_WARN_AOT_SPACE_FULL, _cacheName);
				}
				if (0 != (cacheFullFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
					/* Only report JIT data space full to std-err with verbose enabled */
					CC_TRACE1(J9SHR_VERBOSEFLAG_ENABLE_VERBOSE, J9NLS_INFO, J9NLS_SHRC_CC_WARN_JIT_SPACE_FULL, _cacheName);
				}
			}
		}
	}
	return;
}

/**
 * This method is called when the cache is full, and no data of any type can be allocated.
 * It protects the partially filled pages belonging to ROMClass area and Metadata area,
 * along with any unused pages page between the two regions.
 *
 * It should hold either of write mutex or refresh mutex.
 * 
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return void
 */
void
SH_CompositeCacheImpl::protectLastUnusedPages(J9VMThread *currentThread)
{
	UDATA updatePtr, segmentPtr;

	/* No need to mprotect these pages for realtime cache */
	if (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED)) {
		return;
	}

	Trc_SHR_CC_protectLastUnusedPages_Entry();

	Trc_SHR_Assert_True((currentThread == _commonCCInfo->hasRefreshMutexThread) || hasWriteMutex(currentThread));

	/* Skip it if we are running MINMAX_TEST in shrtest, as this test requires _osPageSize to be zero. */
	if (0 == _osPageSize) {
		return;
	}

	updatePtr = (UDATA)UPDATEPTR(_theca);
	segmentPtr = (UDATA)SEGUPDATEPTR(_theca);

	Trc_SHR_CC_protectLastUnusedPages_Protect(updatePtr, segmentPtr);

	notifyPagesCommitted((BlockPtr)segmentPtr, (BlockPtr)(updatePtr + _osPageSize), DIRECTION_FORWARD);

	Trc_SHR_CC_protectLastUnusedPages_Exit();
}

/**
 * Checks if all cache full flags are set in the cache header.
 * It should be called when holding the write lock as it checks for fields in cache header.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return true if all cache full flags are set in the cache header, false otherwise
 */
bool
SH_CompositeCacheImpl::isCacheMarkedFull(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	return (J9SHR_ALL_CACHE_FULL_BITS == ((_theca->cacheFullFlags & J9SHR_ALL_CACHE_FULL_BITS)));
}

/**
 * Sets cache full flags in cache header.
 * It should be called when holding the write lock as it sets a field in cache header.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * @param [in] flags Bitwise-OR of cache full flags to be set in the cache header
 * @param [in] setRuntimeFlags true if corresponding runtime time flags should be set, false otherwise
 *
 * @return void
 */
void
SH_CompositeCacheImpl::setCacheHeaderFullFlags(J9VMThread *currentThread, UDATA flags, bool setRuntimeFlags)
{
	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	if (0 != flags) {
		unprotectHeaderReadWriteArea(currentThread, false);

		_theca->cacheFullFlags |= flags;
		
		_cacheFullFlags = _theca->cacheFullFlags;
		
		protectHeaderReadWriteArea(currentThread, false);

		if (true == setRuntimeFlags) {
			setRuntimeCacheFullFlags(currentThread);
		}
	}
	return;
}

/**
 * Clears cache full flags in cache header.
 * It should be called when holding the write lock as it sets a field in cache header.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return void
 */
void
SH_CompositeCacheImpl::clearCacheHeaderFullFlags(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	unprotectHeaderReadWriteArea(currentThread, false);
	_theca->cacheFullFlags = 0;
	protectHeaderReadWriteArea(currentThread, false);

	return;
}

/**
 * Check if all runtime cache full flags are set.
 * These flag include J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL, J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL and J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL.
 *
 * @return true if all runtime cache full flags are set, false otherwise
 */
bool
SH_CompositeCacheImpl::isAllRuntimeCacheFullFlagsSet(void) const
{
	if ((0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) &&
		(0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) &&
		(0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) &&
		(0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL))
	) {
		return true;
	} else {
		return false;
	}
}

/**
 * This method only sets the runtime cache full flags and disables storage contention.
 * This is used to mark the cache full locally, as desired when running in read-only mode.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::markReadOnlyCacheFull(void)
{
	/* don't set DENY_CACHE_UPDATES - we still need updates for timestamp optimizations */
	*_runtimeFlags |= (J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL | J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL | J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL | J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL);

	*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION;
}

/**
 * Get amount of reserved bytes for AOT data available for use.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return amount of reserved bytes for AOT data available for use.
 */
I_32
SH_CompositeCacheImpl::getAvailableReservedAOTBytes(J9VMThread *currentThread)
{
	I_32 minAOT = _theca->minAOT;
	I_32 aotBytes = (I_32)_theca->aotBytes;

	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	if ((minAOT != -1) && (minAOT > aotBytes)) {
		return (minAOT - aotBytes);
	} else {
		return 0;
	}
}

/**
 * Get amount of reserved bytes for JIT data available for use.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return amount of reserved bytes for JIT data available for use.
 */
I_32
SH_CompositeCacheImpl::getAvailableReservedJITBytes(J9VMThread *currentThread)
{
	I_32 minJIT = _theca->minJIT;
	I_32 jitBytes = (I_32)_theca->jitBytes;

	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	if ((minJIT != -1) && (minJIT > jitBytes)) {
		return (minJIT - jitBytes);
	} else {
		return 0;
	}
}

/**
 * Get amount of free bytes for AOT data.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return amount of free bytes for AOT data available for use.
 */
I_32
SH_CompositeCacheImpl::getFreeAOTBytes(J9VMThread *currentThread)
{
	/* Subtract out unused reserved JIT bytes from free bytes to get the space available for AOT data */
	I_32 usableFreeBytes = (I_32)getFreeBytes() - getAvailableReservedJITBytes(currentThread);
	if ((-1 == _theca->maxAOT) ||
		((_theca->maxAOT - (I_32)_theca->aotBytes) >= usableFreeBytes)
	) {
		return usableFreeBytes;
	} else {
		return (_theca->maxAOT - (I_32)_theca->aotBytes);
	}
}

/**
 * Get amount of free bytes for JIT data.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return amount of free bytes for JIT data available for use.
 */
I_32
SH_CompositeCacheImpl::getFreeJITBytes(J9VMThread *currentThread)
{
	/* Subtract out unused reserved AOT bytes from free bytes to get the space available for JIT data */
	I_32 usableFreeBytes = (I_32)getFreeBytes() - getAvailableReservedAOTBytes(currentThread);
	if ((-1 == _theca->maxJIT) ||
		((_theca->maxJIT - (I_32)_theca->jitBytes) >= usableFreeBytes)
	) {
		return usableFreeBytes;
	} else {
		return (_theca->maxJIT - (I_32)_theca->jitBytes);
	}
}

/**
 * Checks if the flag for partialpages protection flag is set in the cache header.
 *
 * @return true if the flag for protecting partialpages is set in the cache header, false otherwise.
 */
bool
SH_CompositeCacheImpl::isMprotectPartialPagesSet(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True((NULL != this->_theca) && hasWriteMutex(currentThread));
	return J9_ARE_ALL_BITS_SET(this->_theca->extraFlags, J9SHR_EXTRA_FLAGS_MPROTECT_PARTIAL_PAGES);
}

/**
 * Checks if the flag for enabling partialpages protection on startup is set in the cache header.
 *
 * @return true if the flag to enable protecting partialpages is set in the cache header, false otherwise.
 */
bool
SH_CompositeCacheImpl::isMprotectPartialPagesOnStartupSet(J9VMThread *currentThread)
{
	Trc_SHR_Assert_True((NULL != this->_theca) && hasWriteMutex(currentThread));
	return J9_ARE_ALL_BITS_SET(this->_theca->extraFlags, J9SHR_EXTRA_FLAGS_MPROTECT_PARTIAL_PAGES_ON_STARTUP);
}

/**
 * Returns if the cache is accessible by current user or not
 *
 * @return enum SH_CacheAccess
 */
SH_CacheAccess
SH_CompositeCacheImpl::isCacheAccessible(void) const
{
	return _oscache->isCacheAccessible();
}

/**
 * Returns true if the JVM is allowed to store classpaths to cache, false otherwise.
 *
 * @return bool
 */
bool
SH_CompositeCacheImpl::canStoreClasspaths(void) const
{
	return _canStoreClasspaths;
}

/**
 * Helper function to restore a non-persistent cache from a snapshot file
 *
 * @param[in] vm The current J9JavaVM
 * @param[in] cacheName The name of the cache
 * @param[in, out] cacheExist True if the cache to be restored already exits, false otherwise
 * 
 * @return IDATA -1 on failure, 0 on success
 */
IDATA
SH_CompositeCacheImpl::restoreFromSnapshot(J9JavaVM* vm, const char* cacheName, bool* cacheExist)
{
	UDATA numLocks = getNumRequiredOSLocks();
	SH_SharedCacheHeaderInit* headerInit = SH_SharedCacheHeaderInit::newInstance(_newHdrPtr);

	return ((SH_OSCachesysv *)_oscache)->restoreFromSnapshot(vm, cacheName, numLocks, headerInit, cacheExist);
}

/**
 * Advise the OS to release resources used by a section of the shared classes cache
 */
void
SH_CompositeCacheImpl::dontNeedMetadata(J9VMThread *currentThread)
{
	UDATA  min = _minimumAccessedShrCacheMetadata;
	UDATA  max = _maximumAccessedShrCacheMetadata;
	size_t length = (size_t) (max - min);
	if ((0 != min)
		&& (0 != length)
	) {
		_oscache->dontNeedMetadata(currentThread, (const void *)min, length);
	}
}
/**
 * This function changes the permission of the page containing given address by marking the page as read-only or read-write.
 * The address may belong to either segment region, metadata region or class debug data region.
 * If the address is page aligned, then no action is taken.
 *
 * Must be called when holding write mutex.
 *
 * @param [in] currentThread pointer to current J9VMThread
 * @param [in] addr Address in the page that needs to be protected
 * @param [in] readOnly true if the page is to be marked read-only, false if it is to be marked read-write.
 * @param [in] phaseCheck Whether to check JVM phase when changing the permission of the page.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::changePartialPageProtection(J9VMThread *currentThread, void *addr, bool readOnly, bool phaseCheck)
{
	IDATA rc = 0;
	BlockPtr pageAddr = NULL;
	PORT_ACCESS_FROM_PORT(_portlib);

	Trc_SHR_CC_changePartialPageProtection_Entry(currentThread, addr, readOnly, phaseCheck, currentThread->javaVM->phase);

	Trc_SHR_Assert_True(hasWriteMutex(currentThread));

	if (!_started) {
		Trc_SHR_CC_changePartialPageProtection_StartupNotComplete(currentThread);
		goto done;
	}

	if ((J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_MPROTECT_PARTIAL_PAGES_ON_STARTUP)
		|| ((false == phaseCheck) || (J9VM_PHASE_NOT_STARTUP == currentThread->javaVM->phase)))
		&& (true == _doPartialPagesProtect)
	) {
		/* If the given address is page aligned, then it is no-op. */
		if (0 == ((UDATA)addr % _osPageSize)) {
			Trc_SHR_CC_changePartialPageProtection_AddrPageAligned(currentThread);
			goto done;
		}

		pageAddr = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)addr);

		Trc_SHR_CC_changePartialPageProtection_Event(currentThread, pageAddr, pageAddr + _osPageSize, readOnly ? "read-only" : "read-write");

		rc = setRegionPermissions(_portlib, (void *)pageAddr, _osPageSize, readOnly ? J9PORT_PAGE_PROTECT_READ : J9PORT_PAGE_PROTECT_READ | J9PORT_PAGE_PROTECT_WRITE);
		if (0 != rc) {
			I_32 myerror = j9error_last_error_number();
			Trc_SHR_CC_changePartialPageProtection_setRegionPermissions_Failed(currentThread, myerror);
			Trc_SHR_Assert_ShouldNeverHappen();
		}
	} else {
		Trc_SHR_CC_changePartialPageProtection_NotDone(currentThread);
	}

done:
	Trc_SHR_CC_changePartialPageProtection_Exit(currentThread);
}

/**
 * Protect the pages containing segmentSRP and updateSRP, as well as last used pages in class debug data.
 *
 * @param [in] currentThread pointer to current J9VMThread
 * @param [in] protectSegmentPage if protect the segment page
 * @param [in] protectMetadataPage if protect the metadata page
 * @param [in] protectDebugDataPages if protect the debug data page
 * @param [in] phaseCheck Whether to check JVM phase when protecting the pages.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::protectPartiallyFilledPages(J9VMThread *currentThread,  bool protectSegmentPage, bool protectMetadataPage, bool protectDebugDataPages, bool phaseCheck)
{
	if (0 != _osPageSize) {
		/* PR 105482: If segmentSRP and updateSRP are on same page and but only one of them is requested to be protected,
		 * then do not protect the page, as the JVM expects the page to be unprotected for writing or updates.
		 * For example if protectSegmentPage is true but protectMetadataPage is not,
		 * then later the JVM may attempt to write/update data in metadata region, so the page should not be protected.
		 */
		BlockPtr segmentPtrPage = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)SEGUPDATEPTR(_theca));
		BlockPtr updatePtrPage = (BlockPtr)ROUND_DOWN_TO(_osPageSize, (UDATA)UPDATEPTR(_theca));

		if ((segmentPtrPage != updatePtrPage) || (protectSegmentPage == protectMetadataPage)) {
			if (protectSegmentPage) {
				this->changePartialPageProtection(currentThread, SEGUPDATEPTR(_theca), true, phaseCheck);
			}

			if (protectMetadataPage) {
				this->changePartialPageProtection(currentThread, UPDATEPTR(_theca), true, phaseCheck);
			}
		}
	}

	if (protectDebugDataPages) {
		_debugData->protectPartiallyFilledPages(currentThread, (AbstractMemoryPermission*)this, phaseCheck);
	}
}

/**
 * Unprotect the pages containing segmentSRP and updateSRP, as well as last used pages in class debug data.
 *
 * @param [in] currentThread Pointer to current J9VMThread
 * @param [in] unprotectSegmentPage If unprotect the segment page
 * @param [in] unprotectMetadataPage If unprotect the metadata page
 * @param [in] unprotectDebugDataPages If unprotect the debug data page
 * @param [in] phaseCheck Whether to check JVM phase when protecting the pages.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::unprotectPartiallyFilledPages(J9VMThread *currentThread,  bool unprotectSegmentPage, bool unprotectMetadataPage, bool unprotectDebugDataPages, bool phaseCheck)
{
	if (unprotectSegmentPage) {
		this->changePartialPageProtection(currentThread, SEGUPDATEPTR(_theca), false, phaseCheck);
	}

	if (unprotectMetadataPage) {
		this->changePartialPageProtection(currentThread, UPDATEPTR(_theca), false, phaseCheck);
	}

	if (unprotectDebugDataPages) {
		_debugData->unprotectPartiallyFilledPages(currentThread, (AbstractMemoryPermission*)this, phaseCheck);
	}
}

/**
 * Get the number of used bytes in the shared classes cache. The headers, RW area, reserved AOT space and reserve JIT data space are counted as used space.
 *
 * @return bytes used in the cache
 */
U_32
SH_CompositeCacheImpl::getUsedBytes(void)
{
	I_64 ret = 0;

	if (_started) {
		ret = getTotalSize() - getFreeBlockBytes()- getFreeDebugSpaceBytes();
	} else {
		/* Directly use localVariableTableNextSRP and lineNumberTableNextSRP only when _started is false where _debugData may not be initialized */
		ret = getTotalSize() - getFreeBlockBytes() - (_theca->localVariableTableNextSRP - _theca->lineNumberTableNextSRP);
	}
	/* getTotalSize() returns an invalid value in some UnitTest, making ret negative. Always return a non-negative value here */
	return ((ret > 0) ? (U_32)ret : 0);
}

/**
 * Set the number of soft max bytes in the shared classes cache
 * It should be called when holding the write mutex and the header is unprotected.
 *
 * @param[in] currentThread The current vm thread
 * @param[in] softMaxBytes The value of soft max bytes in the cache
 * @param [in] isJCLCall True if JCL is invoking this function
 *
 */
void
SH_CompositeCacheImpl::setSoftMaxBytes(J9VMThread *currentThread, U_32 softMaxBytes, bool isJCLCall)
{
	PORT_ACCESS_FROM_PORT(_portlib);
	/* add trace points */
	Trc_SHR_Assert_True(
						(NULL != _theca)
						&& hasWriteMutex(currentThread)
						&& (getTotalSize() >= softMaxBytes)
						&& (softMaxBytes >= getUsedBytes())
						);

	_theca->softMaxBytes = softMaxBytes;
	
	Trc_SHR_CC_setSoftMaxBytes(currentThread, softMaxBytes);
	CC_INFO_TRACE1(J9NLS_SHRC_CC_SOFTMX_SET, softMaxBytes, isJCLCall);
	return;
}

/**
 * Try adjusting the value of softMaxBytes, minAOT, maxAOT, minJIT and maxJIT in the cache header. Print out warning message if such passed in value(s) are infeasible.
 *
 * @param[in] currentThread The current vm thread
 * @param [in] isJCLCall True if JCL is invoking this function
 * 
 * @return I_32	J9SHR_SOFTMX_ADJUSTED is set if softmx has been adjusted
 *				J9SHR_MIN_AOT_ADJUSTED is set if minAOT has been adjusted
 *				J9SHR_MAX_AOT_ADJUSTED is set if maxAOT has been adjusted
 *				J9SHR_MIN_JIT_ADJUSTED is set if minJIT has been adjusted
 *				J9SHR_MAX_JIT_ADJUSTED is set if maxJIT has been adjusted
 */
I_32
SH_CompositeCacheImpl::tryAdjustMinMaxSizes(J9VMThread *currentThread, bool isJCLCall)
{
	U_32 totalBytes = getTotalSize();
	bool adjustMinAOT = false;
	bool adjustMaxAOT = false;
	bool adjustMinJIT = false;
	bool adjustMaxJIT = false;
	bool adjustSoftMax = false;
	I_32 minAOT = 0;
	I_32 maxAOT = 0;
	I_32 minJIT = 0;
	I_32 maxJIT = 0;
	U_32 softMax = 0;
	U_32 usedBytesBefore = 0;
	U_32 usedBytesAfter = 0;
	U_32 maxLimit = 0;
	UDATA flagsToUnset = 0;
	bool releaseWriteMutex = false;
	const char* fnName= "CC tryAdjustMinMaxSizes";
	I_32 ret = 0;
	PORT_ACCESS_FROM_PORT(_portlib);

	Trc_SHR_CC_tryAdjustMinMaxSizes_Entry(currentThread);
	Trc_SHR_Assert_True((NULL != _theca) && (_started));

	if (_readOnlyOSCache) {
		Trc_SHR_CC_tryAdjustMinMaxSizes_cacheOpenReadOnly(currentThread);
		goto done;
	}

	if (!hasWriteMutex(currentThread)) {
		if (0 == enterWriteMutex(currentThread, false, fnName)) {
			releaseWriteMutex = true;
		} else {
			Trc_SHR_CC_tryAdjustMinMaxSizes_enterWriteMutexFailed(currentThread);
			goto done;
		}
	}

	if (true == isCacheMarkedFull(currentThread)) {
		/* All cache full bits are set, no more data can be added to the cache anymore. Ignore the attempt to adjust softmx/minAOT/maxAOT/minJIT/maxJIT */
		CC_INFO_TRACE(J9NLS_SHRC_CC_ADJUST_SIZE_ON_FULL_CACHE, isJCLCall);
		Trc_SHR_CC_tryAdjustMinMaxSizes_cacheFull(currentThread);
		goto done;
	}

	/* set the variables inside the write mutex */
	adjustMinAOT = (_sharedClassConfig->minAOT >= 0);
	adjustMaxAOT = (_sharedClassConfig->maxAOT >= 0);
	adjustMinJIT = (_sharedClassConfig->minJIT >= 0);
	adjustMaxJIT = (_sharedClassConfig->maxJIT >= 0);
	adjustSoftMax = ((U_32)-1 != _sharedClassConfig->softMaxBytes);
	minAOT = (adjustMinAOT ? _sharedClassConfig->minAOT : _theca->minAOT);
	maxAOT = (adjustMaxAOT ? _sharedClassConfig->maxAOT : _theca->maxAOT);
	minJIT = (adjustMinJIT ? _sharedClassConfig->minJIT : _theca->minJIT);
	maxJIT = (adjustMaxJIT ? _sharedClassConfig->maxJIT : _theca->maxJIT);
	softMax = (adjustSoftMax ? _sharedClassConfig->softMaxBytes : _theca->softMaxBytes);
	usedBytesBefore = getUsedBytes();

	if (adjustSoftMax) {
		if (softMax > totalBytes) {
			softMax = totalBytes;
			CC_WARNING_TRACE1(J9NLS_SHRC_SOFTMAX_TOO_BIG, totalBytes, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_softMaxSizeTooBig(currentThread, softMax, totalBytes);
		} else if (softMax < usedBytesBefore) {
			/* softMax is infeasible which is smaller than the already used bytes,
			 * adjust it to the min feasible value
			 */
			softMax = usedBytesBefore;
			CC_WARNING_TRACE1(J9NLS_SHRC_CC_SOFTMAX_TOO_SMALL, usedBytesBefore, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_softMaxSizeTooSmall(currentThread, softMax, usedBytesBefore);
		}
	}

	maxLimit = ((U_32)-1 == softMax) ? totalBytes : softMax;

	if (adjustMinAOT || adjustMaxAOT || adjustSoftMax) {
		U_32 usedAotBytes = (U_32)_theca->aotBytes;

		if ((0 < maxAOT) && (maxAOT < minAOT)) {
			CC_WARNING_TRACE(J9NLS_SHRC_CC_MINAOT_GRTHAN_MAXAOT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_minAOTGreaterThanMaxAOT(currentThread, minAOT, maxAOT);
			goto done;
		} else if ((0 < maxAOT) && ((U_32)maxAOT < usedAotBytes)) {
			maxAOT = (I_32)usedAotBytes;
			CC_WARNING_TRACE1(J9NLS_SHRC_CC_MAXAOT_SMALLERTHAN_USEDAOT, usedAotBytes, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_maxAOTSmallerThanUsedAOT(currentThread, minAOT, usedAotBytes);
		} else if ((maxAOT > 0) && ((U_32)maxAOT > maxLimit)) {
			/* when adjustSoftMax is true, the value of maxLimit will be changed, we should perform this check in this case  */
			maxAOT = maxLimit;
			CC_WARNING_TRACE1(J9NLS_SHRC_CC_MAXAOT_GRTHAN_MAXLIMIT, maxLimit, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_maxAOTGreaterThanMaxLimit(currentThread, maxAOT, maxLimit);
		}
	}

	if (adjustMinJIT || adjustMaxJIT || adjustSoftMax) {
		U_32 usedJitBytes = (U_32)_theca->jitBytes;

		if ((0 < maxJIT) && (maxJIT < minJIT)) {
			CC_WARNING_TRACE(J9NLS_SHRC_CC_MINJIT_GRTHAN_MAXJIT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_minJITGreaterThanMaxJIT(currentThread, minJIT, maxJIT);
			goto done;
		} else if ((0 < maxJIT) && ((U_32)maxJIT < usedJitBytes)) {
			maxJIT = (I_32)usedJitBytes;
			CC_WARNING_TRACE1(J9NLS_SHRC_CC_MAXJIT_SMALLERTHAN_USEDJIT, usedJitBytes, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_maxJITSmallerThanUsedJIT(currentThread, minJIT, usedJitBytes);
		} else if ((maxJIT > 0) && ((U_32)maxJIT > maxLimit)) {
			maxJIT = maxLimit;
			CC_WARNING_TRACE1(J9NLS_SHRC_CC_MAXJIT_GRTHAN_MAXLIMIT, maxLimit, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_maxJITGreaterThanMaxLimit(currentThread, maxJIT, maxLimit);
		}
	}

	if (adjustMinAOT || adjustMinJIT) {
		/* The reserved AOT and JIT data space are counted as used space. The increase of used AOTBytes + increased of used JITBytes + already usedBytes
		 * should always be <= softMaxBytes/total cache bytes.
		 */
		I_32 usedAOTBytesBefore = ((I_32)_theca->aotBytes > _theca->minAOT) ? (I_32)_theca->aotBytes : _theca->minAOT;
		I_32 usedJIBytesBefore = ((I_32)_theca->jitBytes > _theca->minJIT) ? (I_32)_theca->jitBytes : _theca->minJIT;
		I_32 usedAOTBytesAfter = ((I_32)_theca->aotBytes > minAOT) ? (I_32)_theca->aotBytes : minAOT;
		I_32 usedJITBytesAfter = ((I_32)_theca->jitBytes > minJIT) ? (I_32)_theca->jitBytes : minJIT;
		I_32 freeDebugBytes = (I_32)(_theca->localVariableTableNextSRP - _theca->lineNumberTableNextSRP);
		U_32 maxUsedBytes = (maxLimit < (totalBytes - freeDebugBytes - J9SHR_MIN_GAP_BEFORE_METADATA)) ? maxLimit : (totalBytes - freeDebugBytes - J9SHR_MIN_GAP_BEFORE_METADATA);
		/* The reserved AOT/JIT data space can only come from space between updateSRP and segmentSRP, so free space in debug area should be subtracted.
		 * Leave at least J9SHR_MIN_GAP_BEFORE_METADATA, so J9SHR_MIN_GAP_BEFORE_METADATA should also be subtracted.
		 */
		if ((usedAOTBytesAfter - usedAOTBytesBefore + usedJITBytesAfter - usedJIBytesBefore + usedBytesBefore) > maxUsedBytes) {
			CC_WARNING_TRACE(J9NLS_SHRC_CC_TOTAL_USED_BYTES_GRTHAN_MAXLIMIT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_totalUsedBytesGreaterThanMaxLimit(currentThread, usedAOTBytesBefore, usedAOTBytesAfter, usedJIBytesBefore, usedJITBytesAfter, usedBytesBefore, maxUsedBytes);
			goto done;
		}
	}

	unprotectHeaderReadWriteArea(currentThread, false);

	if (adjustMinAOT) {
		if (minAOT != _theca->minAOT) {
			_theca->minAOT = minAOT;
			ret |= J9SHR_MIN_AOT_ADJUSTED;
			CC_INFO_TRACE1(J9NLS_SHRC_CC_MINAOT_SET, minAOT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_minAOTValueSet(currentThread, minAOT);
		}
	}
	if (adjustMaxAOT) {
		if (maxAOT != _theca->maxAOT) {
			if ((_theca->maxAOT >= 0)
				&& (maxAOT > _theca->maxAOT)
			) {
				Trc_SHR_CC_tryAdjustMinMaxSizes_maxAOTIncreased(currentThread);
				flagsToUnset |= J9SHR_AOT_SPACE_FULL;
			}
			_theca->maxAOT = maxAOT;
			ret |= J9SHR_MAX_AOT_ADJUSTED;
			CC_INFO_TRACE1(J9NLS_SHRC_CC_MAXAOT_SET, maxAOT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_maxAOTValueSet(currentThread, maxAOT);
		}
	}
	if (adjustMinJIT) {
		if (minJIT != _theca->minJIT) {
			_theca->minJIT = minJIT;
			ret |= J9SHR_MIN_JIT_ADJUSTED;
			CC_INFO_TRACE1(J9NLS_SHRC_CC_MINJIT_SET, minJIT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_minJITValueSet(currentThread, minJIT);
		}
	}
	if (adjustMaxJIT) {
		if (maxJIT != _theca->maxJIT) {
			if ((_theca->maxJIT >= 0)
				&& (maxJIT > _theca->maxJIT)
			) {
				Trc_SHR_CC_tryAdjustMinMaxSizes_maxJITIncreased(currentThread);
				flagsToUnset |= J9SHR_JIT_SPACE_FULL;
			}
			_theca->maxJIT = maxJIT;
			ret |= J9SHR_MAX_JIT_ADJUSTED;
			CC_INFO_TRACE1(J9NLS_SHRC_CC_MAXJIT_SET, maxJIT, isJCLCall);
			Trc_SHR_CC_tryAdjustMinMaxSizes_maxJITValueSet(currentThread, maxJIT);
		}
	}

	if (adjustSoftMax) {
		if (softMax != _theca->softMaxBytes) {
			if (softMax > _theca->softMaxBytes) {
				Trc_SHR_CC_tryAdjustMinMaxSizes_softmxIncreased(currentThread);
				flagsToUnset |= J9SHR_AVAILABLE_SPACE_FULL;
				flagsToUnset |= J9SHR_BLOCK_SPACE_FULL;
				flagsToUnset |= J9SHR_JIT_SPACE_FULL;
				flagsToUnset |= J9SHR_AOT_SPACE_FULL;
			}
			setSoftMaxBytes(currentThread, softMax, isJCLCall);
			ret |= J9SHR_SOFTMX_ADJUSTED;
		}
	}

	usedBytesAfter = getUsedBytes();

	if (usedBytesBefore > usedBytesAfter) {
		/* minAOT/minJIT has not been reached and the value(s) decreased */
		Trc_SHR_CC_tryAdjustMinMaxSizes_minAOTminJITDecreased(currentThread);
		flagsToUnset |= J9SHR_AVAILABLE_SPACE_FULL;
		flagsToUnset |= J9SHR_BLOCK_SPACE_FULL;
	} else if (usedBytesBefore < usedBytesAfter) {
		Trc_SHR_CC_tryAdjustMinMaxSizes_minAOTminJITIncreased(currentThread);
	}

	if (0 != flagsToUnset) {
		unsetCacheHeaderFullFlags(currentThread, flagsToUnset);
	}

	protectHeaderReadWriteArea(currentThread, false);

	if (adjustMinJIT || adjustMinAOT || adjustSoftMax) {
		/* The free block bytes and available bytes (softmx - usedBytes) can be changed if minAOT, minJIT or softmx has been adjusted. fillCacheIfNearlyFull() needs to be called.
		 */
		fillCacheIfNearlyFull(currentThread);
	}

done:
	_sharedClassConfig->softMaxBytes = (U_32)-1;
	_sharedClassConfig->minAOT = -1;
	_sharedClassConfig->maxAOT = -1;
	_sharedClassConfig->minJIT = -1;
	_sharedClassConfig->maxJIT = -1;

	if (releaseWriteMutex) {
		exitWriteMutex(currentThread, fnName);
	}

	Trc_SHR_CC_tryAdjustMinMaxSizes_Exit(currentThread);
	return ret;
}

/* Another JVM may have adjusted the softMaxBytes/minAOT/maxAOT/minJIT/maxJIT in the cache header.
 * Update the runtime cache full flags according to cache full flags in the cache header
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 *
 * @return void
 */
void
SH_CompositeCacheImpl::updateRuntimeFullFlags(J9VMThread *currentThread)
{
	UDATA cacheFullFlags = _theca->cacheFullFlags;
	bool holdWriteMutex = hasWriteMutex(currentThread);
	const char* fnName = "CC updateRuntimeFullFlags";
	U_64 flagSet = 0;
	U_64 flagUnset = 0;
	bool resetMaxAOTUnstoredBytes = false;
	bool resetMaxJITUnstoredBytes = false;
	bool resetSoftmxUnstoredBytes = false;

	Trc_SHR_CC_updateRuntimeFullFlags_Entry(currentThread);
	
	if (_readOnlyOSCache || J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_READONLY)) {
		goto done;
	}

	if (_cacheFullFlags == cacheFullFlags) {
		goto done;
	} else {
		_cacheFullFlags = cacheFullFlags;
	}

	omrthread_monitor_enter(_runtimeFlagsProtectMutex);

	if (J9_ARE_NO_BITS_SET(cacheFullFlags, J9SHR_BLOCK_SPACE_FULL)) {
		if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagUnset(currentThread, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL);
			flagUnset |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
		}
	} else {
		if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagSet(currentThread, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL);
			flagSet |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
		}
	}

	if (J9_ARE_NO_BITS_SET(cacheFullFlags, J9SHR_AVAILABLE_SPACE_FULL)) {
		if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
			if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
				/* J9SHR_BLOCK_SPACE_FULL is always unset together with J9SHR_AVAILABLE_SPACE_FULL, do not need to remove J9AVLTREE_DISABLE_SHARED_TREE_UPDATES
				 * when unsetting J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL, do it here is enough.
				 */
				if (NULL != currentThread->javaVM->sharedInvariantInternTable) {
					Trc_SHR_CC_updateRuntimeFullFlags_flagUnset(currentThread, J9AVLTREE_DISABLE_SHARED_TREE_UPDATES);
					currentThread->javaVM->sharedInvariantInternTable->flags &= ~J9AVLTREE_DISABLE_SHARED_TREE_UPDATES;
				}
				if (_reduceStoreContentionDisabled) {
					Trc_SHR_CC_updateRuntimeFullFlags_flagSet(currentThread, J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION);
					*_runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION;
					_useWriteHash = true;
					_reduceStoreContentionDisabled = false;
				}
			}
			Trc_SHR_CC_updateRuntimeFullFlags_flagUnset(currentThread, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL);
			flagUnset |= J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL;
			resetSoftmxUnstoredBytes = true;
		}
	} else {
		if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagSet(currentThread, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL);
			flagSet |= J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL;
			
			if ((true == _useWriteHash) && (0 != (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION))) {
				this->setWriteHash(currentThread, 0);
				_reduceStoreContentionDisabled = true;
			}
			*_runtimeFlags &= ~J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION;
			_useWriteHash = false;
		}
	}

	if (J9_ARE_NO_BITS_SET(cacheFullFlags, J9SHR_AOT_SPACE_FULL)) {
		if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagUnset(currentThread, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL);
			flagUnset |= J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL;
			/* J9SHR_AOT_SPACE_FULL can be removed when either softmx or maxAOT gets increased by this VM or another VM. Keep a local copy of _maxAOT,
			 * so that we know whether it is the increased maxAOT that removes J9SHR_AOT_SPACE_FULL. Set _maxAOTUnstoredBytes to 0 only when maxAOT is increased.
			 */
			if (_maxAOT < _theca->maxAOT) {
				resetMaxAOTUnstoredBytes = true;
				_maxAOT = _theca->maxAOT;
				Trc_SHR_CC_updateRuntimeFullFlags_maxAOTIncreased(currentThread, _maxAOT);
			}
		}
	} else {
		if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagSet(currentThread, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL);
			flagSet |= J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL;
		}
	}

	if (J9_ARE_NO_BITS_SET(cacheFullFlags, J9SHR_JIT_SPACE_FULL)) {
		if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagUnset(currentThread, J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL);
			flagUnset |= J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL;
			/* J9SHR_JIT_SPACE_FULL can be removed when either softmx or maxJIT gets increased by this VM or another VM. Keep a local copy of _maxJIT,
			 * so that we know whether it is the increased maxJIT that removes J9SHR_JIT_SPACE_FULL. Set _maxJITUnstoredBytes to 0 only when maxJIT is increased.
			 */
			if (_maxJIT < _theca->maxJIT) {
				resetMaxJITUnstoredBytes = true;
				_maxJIT = _theca->maxJIT;
				Trc_SHR_CC_updateRuntimeFullFlags_maxJITIncreased(currentThread, _maxJIT);
			}
		}	
	} else {
		if (J9_ARE_NO_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
			Trc_SHR_CC_updateRuntimeFullFlags_flagSet(currentThread, J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL);
			flagSet |= J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL;
		}
	}

	*_runtimeFlags &= ~flagUnset;
	*_runtimeFlags |= flagSet;
	/* JAZZ103 108287 Add write barrier to ensure the setting/unsetting of runtime flags happens before resetting 
	 * _maxAOTUnstoredBytes/_maxJITUnstoredBytes/_softmxUnstoredBytes to 0.
	 */
	VM_AtomicSupport::writeBarrier();
	if (resetMaxAOTUnstoredBytes) {
		_maxAOTUnstoredBytes = 0;
	}
	if (resetMaxJITUnstoredBytes) {
		_maxJITUnstoredBytes = 0;
	}
	if (resetSoftmxUnstoredBytes) {
		_softmxUnstoredBytes = 0;
	}

	omrthread_monitor_exit(_runtimeFlagsProtectMutex);

	if ((holdWriteMutex)
		|| (0 == enterWriteMutex(currentThread, false, fnName))
	) {
		if (flagSet > 0) {
			if (true == isAllRuntimeCacheFullFlagsSet()) {
				_debugData->protectUnusedPages(currentThread, (AbstractMemoryPermission*)this);
				protectLastUnusedPages(currentThread);
			} else if (J9_ARE_ANY_BITS_SET(flagSet, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
				/* At this point no more ROMClass can be added.
				 * This implies no new data can be added in class debug area.
				 * Partially filled page and unused pages in class debug area can now be protected.
				 */
				_debugData->protectUnusedPages(currentThread, (AbstractMemoryPermission*)this);
			} else if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL | J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL| J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
				protectPartiallyFilledPages(currentThread, true, true, true, false);
			} else if (J9_ARE_ANY_BITS_SET(flagSet, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
				/* Only protect partially filled pages in debug area */
				protectPartiallyFilledPages(currentThread, false, false, true, false);
			}
		}

		if (flagUnset > 0) {
			if (J9_ARE_ANY_BITS_SET(flagUnset, J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
				_debugData->unprotectUnusedPages(currentThread, (AbstractMemoryPermission*)this);
				unprotectPartiallyFilledPages(currentThread, true, true, false, false);
			} else if (J9_ARE_ANY_BITS_SET(flagUnset, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
				unprotectPartiallyFilledPages(currentThread, true, true, true, false);
			} else if (J9_ARE_ANY_BITS_SET(flagUnset, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL | J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
				/* Allocating AOT/JIT data might touch the last segment page, unprotect segment page as well */
				unprotectPartiallyFilledPages(currentThread, true, true, false, false);
			}
		}
		if (!holdWriteMutex) {
			exitWriteMutex(currentThread, fnName);
		}
	}

done:
	Trc_SHR_CC_updateRuntimeFullFlags_Exit(currentThread);
	return;
}

/**
 * Unset cache full flags in cache header.
 * It should be called when holding the write mutex. If started, cache header should be unprotected.
 *
 * @param [in] currentThread Pointer to J9VMThread structure for the current thread
 * @param [in] flagsToUnset Flags to be unset.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::unsetCacheHeaderFullFlags(J9VMThread *currentThread, UDATA flagsToUnset)
{
	Trc_SHR_Assert_True((NULL != _theca) && hasWriteMutex(currentThread));
	Trc_SHR_CC_unsetCacheHeaderFullFlags_Entry(currentThread, flagsToUnset);

	_theca->cacheFullFlags &= ~flagsToUnset;
	updateRuntimeFullFlags(currentThread);

	Trc_SHR_CC_unsetCacheHeaderFullFlags_Exit(currentThread);
	return;
}

/**
 * Get the softmx, minAOT, maxAOT, minJIT, maxJIT value in bytes
 *
 * @param [out] vm softmx value in bytes
 * @param [out] The minAOT value in bytes
 * @param [out] The maxAOT value in bytes
 * @param [out] The minJIT value in bytes
 * @param [out] The maxJIT value in bytes
 *
 */
void
SH_CompositeCacheImpl::getMinMaxBytes(U_32 *softmx, I_32 *minAOT, I_32 *maxAOT, I_32 *minJIT, I_32 *maxJIT)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}

	if (NULL != softmx) {
		if ((U_32)-1 == _theca->softMaxBytes) {
			*softmx = (U_32)getTotalUsableCacheSize();
		} else {
			*softmx = _theca->softMaxBytes;
		}
	}
	if (NULL != minAOT) {
		*minAOT = _theca->minAOT;
	}
	if (NULL != maxAOT) {
		*maxAOT = _theca->maxAOT;
	}
	if (NULL != minJIT) {
		*minJIT = _theca->minJIT;
	}
	if (NULL != maxJIT) {
		*maxJIT = _theca->maxJIT;
	}
}

/* Increase the unstored bytes.
 * 
 * @param [in] blockBytes Unstored block bytes.
 * @param [in] aotBytes Unstored aot bytes.
 * @param [in] jitBytes Unstored jit bytes.
 *
 * @return void
 */
void
SH_CompositeCacheImpl::increaseUnstoredBytes(U_32 blockBytes, U_32 aotBytes, U_32 jitBytes)
{
	U_32 softmxUnstoredLimit = MAX_CC_SIZE - _theca->softMaxBytes;
	I_32 maxAOT = _theca->maxAOT;
	I_32 maxJIT = _theca->maxJIT;
	U_32 maxAOTUnstoredLimit = MAX_CC_SIZE - maxAOT;
	U_32 maxJITUnstoredLimit = MAX_CC_SIZE - maxJIT;

	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return;
	}
	Trc_SHR_CC_increaseUnstoredBytes_Entry(blockBytes, aotBytes, jitBytes);

	if (blockBytes > 0) {
		if ((_softmxUnstoredBytes + blockBytes) < softmxUnstoredLimit) {
			VM_AtomicSupport::addU32((volatile U_32 *)&_softmxUnstoredBytes, blockBytes);
			Trc_SHR_CC_increaseUnstoredBytes_softmxUnstoredBytesIncreased(blockBytes, _softmxUnstoredBytes);
		} else {
			_softmxUnstoredBytes = softmxUnstoredLimit;
		}
	}
	if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL)) {
		if ((aotBytes + jitBytes) > 0) {
			if ((_softmxUnstoredBytes + aotBytes + jitBytes) < softmxUnstoredLimit) {
				VM_AtomicSupport::addU32((volatile U_32 *)&_softmxUnstoredBytes, aotBytes + jitBytes);
				Trc_SHR_CC_increaseUnstoredBytes_softmxUnstoredBytesIncreased(aotBytes + jitBytes, _softmxUnstoredBytes);
			} else {
				_softmxUnstoredBytes = softmxUnstoredLimit;
			}
		}
	} else {
		if (aotBytes > 0) {
			if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL)) {
				/* Add check for J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL as maxAOT or softmx might have been increased and J9SHR_RUNTIMEFLAG_AOT_SPACE_FULL is removed.
				 * No need to increase _maxAOTUnstoredBytes or _softmxUnstoredBytes if that's the case.
				 */
				if (-1 == maxAOT) {
					/* maxAOT is not set. It is the softmx that prevents the AOT data being stored into the cache. J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL
					 * is not set as free available space can still be > CC_MIN_SPACE_BEFORE_CACHE_FULL.
					 */
					if ((_softmxUnstoredBytes + aotBytes) < softmxUnstoredLimit) {
						VM_AtomicSupport::addU32((volatile U_32 *)&_softmxUnstoredBytes, aotBytes);
						Trc_SHR_CC_increaseUnstoredBytes_softmxUnstoredBytesIncreased(aotBytes, _softmxUnstoredBytes);
					} else {
						_softmxUnstoredBytes = softmxUnstoredLimit;
					}
				} else {
					if ((_maxAOTUnstoredBytes + aotBytes) < maxAOTUnstoredLimit) {
						VM_AtomicSupport::addU32((volatile U_32 *)&_maxAOTUnstoredBytes, aotBytes);
						Trc_SHR_CC_increaseUnstoredBytes_maxAOTUnstoredBytesIncreased(aotBytes, _maxAOTUnstoredBytes);
					} else {
						_maxAOTUnstoredBytes = maxAOTUnstoredLimit;
					}
				}
			}
		}
		if (jitBytes > 0) {
			if (J9_ARE_ALL_BITS_SET(*_runtimeFlags, J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL)) {
				/* Add check for J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL as maxJIT or softmx might have been increased and J9SHR_RUNTIMEFLAG_JIT_SPACE_FULL is removed.
				 * No need to increase _maxJITUnstoredBytes or _softmxUnstoredBytes if that's the case.
				 */
				if (-1 == maxJIT) {
					/* maxJIT is not set. It is the softmx that prevents the JIT data being stored into the cache. J9SHR_RUNTIMEFLAG_AVAILABLE_SPACE_FULL
					 * is not set as free available space can still be > CC_MIN_SPACE_BEFORE_CACHE_FULL.
					 */
					if ((_softmxUnstoredBytes + jitBytes) < softmxUnstoredLimit) {
						VM_AtomicSupport::addU32((volatile U_32 *)&_softmxUnstoredBytes, jitBytes);
						Trc_SHR_CC_increaseUnstoredBytes_softmxUnstoredBytesIncreased(jitBytes, _softmxUnstoredBytes);
					} else {
						_softmxUnstoredBytes = softmxUnstoredLimit;
					}
				} else {
					if ((_maxJITUnstoredBytes + jitBytes) < maxJITUnstoredLimit) {
						VM_AtomicSupport::addU32((volatile U_32 *)&_maxJITUnstoredBytes, jitBytes);
						Trc_SHR_CC_increaseUnstoredBytes_maxJITUnstoredBytesIncreased(jitBytes, _maxJITUnstoredBytes);
					} else {
						_maxJITUnstoredBytes = maxJITUnstoredLimit;
					}
				}
			}
		}
	}

	Trc_SHR_CC_increaseUnstoredBytes_Exit();
}

/* Query whether the cache is being created by the current VM,
 * which we assume to be in cold run.
 *
 * @return true if cache is being created, false otherwise
 */
bool
SH_CompositeCacheImpl::isNewCache(void)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	return _initializingNewCache;
}

/**
 * Record the minimum and maximum boundaries of accessed address in this shared classes cache.
 * @param [in] metadataAddress address accessed in metadata
 * @return true if the boundaries are updated, false otherwise
 */
bool
SH_CompositeCacheImpl::updateAccessedShrCacheMetadataBounds(J9VMThread* currentThread, uintptr_t const * metadataAddress)
{
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}

	if (!isAddressInMetaDataArea((const void*)metadataAddress)) {
		return false;
	}

	uintptr_t* updateAddress = (uintptr_t *) &_minimumAccessedShrCacheMetadata;
	uintptr_t currentValue = (uintptr_t) _minimumAccessedShrCacheMetadata;
	uintptr_t const newValue = (uintptr_t const) metadataAddress;

	if (0 == currentValue) { /* set initial value.  Don't care if someone beats us to this  */
		Trc_SHR_CM_updateAccessedShrCacheMetadataMinimum(currentThread, metadataAddress);
		compareAndSwapUDATA(
				updateAddress,
				currentValue,
				newValue
		);
	}

	currentValue = (uintptr_t) _minimumAccessedShrCacheMetadata;
	while (newValue < (uintptr_t) currentValue) {
		Trc_SHR_CM_updateAccessedShrCacheMetadataMinimum(currentThread, metadataAddress);

		compareAndSwapUDATA(
				updateAddress,
				currentValue,
				newValue
		);
		currentValue = (uintptr_t) _minimumAccessedShrCacheMetadata;
	}

	updateAddress = (uintptr_t *) &_maximumAccessedShrCacheMetadata;
	currentValue = (uintptr_t) _maximumAccessedShrCacheMetadata;
	while (newValue > (uintptr_t) _maximumAccessedShrCacheMetadata) {
		Trc_SHR_CM_updateAccessedShrCacheMetadataMaximum(currentThread, metadataAddress);

		compareAndSwapUDATA(
				updateAddress,
				currentValue,
				newValue
		);
		currentValue = (uintptr_t) _maximumAccessedShrCacheMetadata;
	}

	return true;
}

/**
 * Checks whether the address is inside the released metadata boundaries.
 *
 * @param [in] currentThread The current JVM thread
 * @param [in] metadataAddress The address to be checked
 *
 * @return true if the _minimumAccessedShrCacheMetadata <= metadataAddress <= _maximumAccessedShrCacheMetadata, false otherwise.
 */
bool
SH_CompositeCacheImpl::isAddressInReleasedMetaDataBounds(J9VMThread* currentThread, UDATA metadataAddress) const
{
	bool rc = false;
	if (!_started) {
		Trc_SHR_Assert_ShouldNeverHappen();
		return rc;
	}
	if ((0 != _minimumAccessedShrCacheMetadata)
		&& (0 != _maximumAccessedShrCacheMetadata)
	) {
		rc = ((_minimumAccessedShrCacheMetadata <= metadataAddress) && (metadataAddress <= _maximumAccessedShrCacheMetadata));
	}

	return rc;
}
