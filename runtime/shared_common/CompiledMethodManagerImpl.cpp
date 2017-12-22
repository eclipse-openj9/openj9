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

#include "CompiledMethodManagerImpl.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include "j9consts.h"
#include <string.h>

SH_CompiledMethodManagerImpl::SH_CompiledMethodManagerImpl()
{
}

SH_CompiledMethodManagerImpl::~SH_CompiledMethodManagerImpl()
{
}

/**
 * Returns the number of bytes required to construct this SH_CompiledMethodManager
 *
 * @return size in bytes
 */
UDATA
SH_CompiledMethodManagerImpl::getRequiredConstrBytes(void)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(SH_CompiledMethodManagerImpl);
	return reqBytes;
}

/**
 * Create a new instance of SH_CompiledMethodManagerImpl
 *
 * @param [in] vm A Java VM
 * @param [in] cache_ The SH_SharedCache that will use this CompiledMethodManager
 * @param [in] memForConstructor Memory in which to build the instance
 *
 * @return new SH_CompiledMethodManager
 */	
SH_CompiledMethodManagerImpl*
SH_CompiledMethodManagerImpl::newInstance(J9JavaVM* vm, SH_SharedCache* cache_, SH_CompiledMethodManagerImpl* memForConstructor)
{
	SH_CompiledMethodManagerImpl* newCMM = (SH_CompiledMethodManagerImpl*)memForConstructor;

	Trc_SHR_CMMI_newInstance_Entry(vm, cache_);

	new(newCMM) SH_CompiledMethodManagerImpl();
	newCMM->initialize(vm, cache_, ((BlockPtr)memForConstructor + sizeof(SH_CompiledMethodManagerImpl)));

	Trc_SHR_CMMI_newInstance_Exit(newCMM);

	return newCMM;
}

/* Initialize the SH_CompiledMethodManager - should be called before startup */
void
SH_CompiledMethodManagerImpl::initialize(J9JavaVM* vm, SH_SharedCache* cache_, BlockPtr memForConstructor)
{
	Trc_SHR_CMMI_initialize_Entry();

	_cache = cache_;
	_portlib = vm->portLibrary;
	_htMutex = NULL;
	_htMutexName = "cmTableMutex";
	_dataTypesRepresented[0] = TYPE_COMPILED_METHOD;
	_dataTypesRepresented[1] = TYPE_INVALIDATED_COMPILED_METHOD;
	_dataTypesRepresented[2] = 0;

	_rrmHashTableName = J9_GET_CALLSITE();
	_rrmLookupFnName = "cmTableLookup";
	_rrmAddFnName = "cmTableAdd";
	_rrmRemoveFnName = "cmTableRemove";
	
	_accessPermitted = true;	/* No mechanism to prevent access */

	notifyManagerInitialized(_cache->managers(), "TYPE_COMPILED_METHOD");

	Trc_SHR_CMMI_initialize_Exit();
}

UDATA
SH_CompiledMethodManagerImpl::getKeyForItem(const ShcItem* cacheItem)
{
	return (UDATA)CMWROMMETHOD((CompiledMethodWrapper*)ITEMDATA(cacheItem));
}

U_32
SH_CompiledMethodManagerImpl::getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes)
{
	return (U_32)((cacheSizeBytes / 5000) + 100);
}
