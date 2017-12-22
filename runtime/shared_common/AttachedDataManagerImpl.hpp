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

#if !defined(ATTACHED_DATA_MANAGER_IMPL_HPP_INCLUDED)
#define ATTACHED_DATA_MANAGER_IMPL_HPP_INCLUDED

/* @ddr_namespace: default */
#include "AttachedDataManager.hpp"
#include "SharedCache.hpp"
#include "j9protos.h"
#include "j9.h"

class SH_AttachedDataManagerImpl : public SH_AttachedDataManager
{
public:
	typedef char* BlockPtr;

	SH_AttachedDataManagerImpl();

	~SH_AttachedDataManagerImpl();

	static SH_AttachedDataManagerImpl* newInstance(J9JavaVM* vm, SH_SharedCache* cache, SH_AttachedDataManagerImpl* memForConstructor);

	static UDATA getRequiredConstrBytes(void);

	virtual UDATA getNumOfType(UDATA type);

	virtual UDATA getDataBytesForType(UDATA type);

	virtual bool storeNew(J9VMThread* currentThread, const ShcItem* itemInCache, SH_CompositeCache* cachelet);

protected:
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes);

	virtual UDATA getKeyForItem(const ShcItem* cacheItem);

private:
	UDATA _numBytesByType[J9SHR_ATTACHED_DATA_TYPE_MAX+1];
	UDATA _bytesByType[J9SHR_ATTACHED_DATA_TYPE_MAX+1];

	/* Copy prevention */
	SH_AttachedDataManagerImpl(const SH_AttachedDataManagerImpl&);
	SH_AttachedDataManagerImpl& operator=(const SH_AttachedDataManagerImpl&);

	void initialize(J9JavaVM* vm, SH_SharedCache* cache, BlockPtr memForConstructor);
};

#endif	/* ATTACHED_DATA_MANAGER_IMPL_HPP_INCLUDED */
