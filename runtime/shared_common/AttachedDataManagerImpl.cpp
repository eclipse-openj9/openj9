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

/**
 * @file
 * @ingroup Shared_Common
 */

#include "AttachedDataManagerImpl.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_AttachedDataManagerImpl::SH_AttachedDataManagerImpl()
{
}

SH_AttachedDataManagerImpl::~SH_AttachedDataManagerImpl()
{
}

/**
 * Returns the number of bytes required to construct this SH_AttachedDataManager
 *
 * @return size in bytes
 */
UDATA
SH_AttachedDataManagerImpl::getRequiredConstrBytes(void)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(SH_AttachedDataManagerImpl);
	return reqBytes;
}

/**
 * Create a new instance of SH_AttachedDataManagerImpl
 *
 * @param [in] vm A Java VM
 * @param [in] cache_ The SH_SharedCache that will use this AttachedDataManager
 * @param [in] memForConstructor Memory in which to build the instance
 *
 * @return new SH_AttachedDataManager
 */
SH_AttachedDataManagerImpl*
SH_AttachedDataManagerImpl::newInstance(J9JavaVM* vm, SH_SharedCache* cache_, SH_AttachedDataManagerImpl* memForConstructor)
{
	SH_AttachedDataManagerImpl* newADM = (SH_AttachedDataManagerImpl*)memForConstructor;

	Trc_SHR_ADMI_newInstance_Entry(vm, cache_);

	new(newADM) SH_AttachedDataManagerImpl();
	newADM->initialize(vm, cache_, ((BlockPtr)memForConstructor + sizeof(SH_AttachedDataManagerImpl)));

	Trc_SHR_ADMI_newInstance_Exit(newADM);

	return newADM;
}

/* Initialize the SH_AttachedDataManager - should be called before startup */
void
SH_AttachedDataManagerImpl::initialize(J9JavaVM* vm, SH_SharedCache* cache_, BlockPtr memForConstructor)
{
	Trc_SHR_ADMI_initialize_Entry();

	_cache = cache_;
	_portlib = vm->portLibrary;
	_htMutex = NULL;
	_htMutexName = "adTableMutex";
	memset(_bytesByType, 0, sizeof(_bytesByType));
	memset(_numBytesByType, 0, sizeof(_numBytesByType));
	_dataTypesRepresented[0] = TYPE_ATTACHED_DATA;
	_dataTypesRepresented[1] = _dataTypesRepresented[2] = 0;

	_rrmHashTableName = J9_GET_CALLSITE();
	_rrmLookupFnName = "adTableLookup";
	_rrmAddFnName = "adTableAdd";
	_rrmRemoveFnName = "adTableRemove";

	_accessPermitted = true;	/* No mechanism to prevent access */

	notifyManagerInitialized(_cache->managers(), "TYPE_ATTACHED_DATA");

	Trc_SHR_ADMI_initialize_Exit();
}

UDATA
SH_AttachedDataManagerImpl::getNumOfType(UDATA type) {
	if (type > J9SHR_ATTACHED_DATA_TYPE_MAX) {
		Trc_SHR_ADMI_getNumOfType_Error(type);
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _numBytesByType[type];
}

UDATA
SH_AttachedDataManagerImpl::getDataBytesForType(UDATA type) {
	if (type > J9SHR_ATTACHED_DATA_TYPE_MAX) {
		Trc_SHR_ADMI_getDataBytesForType_Error(type);
		Trc_SHR_Assert_ShouldNeverHappen();
		return 0;
	}
	return _bytesByType[type];
}

/**
 * Registers a new resource with the SH_AttachedDataManager
 * Calls super class method (SH_ROMClassResourceManager) to update the hash table.
 *
 * Called when a new resource has been identified in the cache
 *
 * @see Manager.hpp
 * @param[in] currentThread The current thread
 * @param[in] itemInCache The address of the item found in the cache
 *
 * @return true if successful, false otherwise
 */
bool
SH_AttachedDataManagerImpl::storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet)
{
	bool rc;
	UDATA type;
	AttachedDataWrapper *adw;

	Trc_SHR_ADMI_storeNew_Entry(currentThread, itemInCache);

	/* Need to allow storeNew to work even if accessPermitted == false.
	 * This is because we need to keep the hashtables up to date incase accessPermitted becomes true */
	if (getState() != MANAGER_STATE_STARTED) {
		return false;
	}

	adw = (AttachedDataWrapper *)ITEMDATA(itemInCache);
	type = ADWTYPE(adw);
	if (type <= J9SHR_ATTACHED_DATA_TYPE_MAX) {
		_bytesByType[type] += ITEMDATALEN(itemInCache);
		_numBytesByType[type] += 1;
	} else {
		/* Unknown type */
		_bytesByType[J9SHR_ATTACHED_DATA_TYPE_UNKNOWN] += ITEMDATALEN(itemInCache);
		_numBytesByType[J9SHR_ATTACHED_DATA_TYPE_UNKNOWN] += 1;
	}

	rc = SH_ROMClassResourceManager::storeNew(currentThread, itemInCache, cachelet);

	if (false == rc) {
		Trc_SHR_ADMI_storeNew_ExitFalse(currentThread);
	} else {
		Trc_SHR_ADMI_storeNew_ExitTrue(currentThread);
	}

	return rc;
}

U_32
SH_AttachedDataManagerImpl::getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes)
{
	return (U_32)((cacheSizeBytes / 5000) + 100);
}

UDATA
SH_AttachedDataManagerImpl::getKeyForItem(const ShcItem* cacheItem)
{
	return (UDATA)ADWCACHEOFFSET((AttachedDataWrapper*)ITEMDATA(cacheItem)) + (UDATA)ADWTYPE((AttachedDataWrapper*)ITEMDATA(cacheItem));
}
