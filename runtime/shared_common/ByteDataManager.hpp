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

/**
 * @file
 * @ingroup Shared_Common
 */

#if !defined(BYTEDATAMANAGER_HPP_INCLUDED)
#define BYTEDATAMANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "Manager.hpp"

/**
 * Sub-interface of SH_Manager used for managing byte data in the cache
 *
 * @see SH_ByteDataManagerImpl.hpp
 * @ingroup Shared_Common
 */
class SH_ByteDataManager : public SH_Manager
{
public:
	typedef char* BlockPtr;

	/* Data entry will be returned which uniquely matches key, dataType and jvmID (matches on privateOwnerID field) */
	virtual ByteDataWrapper* findSingleEntry(J9VMThread* currentThread, const char* key, UDATA keylen, UDATA dataType, U_16 jvmID, UDATA* dataLen) = 0;

	/* Goes through all private and public entries for a given key and marks them all stale */
	virtual void markAllStaleForKey(J9VMThread* currentThread, const char* key, UDATA keylen) = 0;

	/* Fill descriptorPool with entries found and return number of entries or -1 */	
	virtual IDATA find(J9VMThread* currentThread, const char* key, UDATA keylen, UDATA limitDataType, UDATA includePrivateData, J9SharedDataDescriptor* firstItem, const J9Pool* descriptorPool) = 0;

	/* Attempt to make the entry represented by "data" private to this JVM. 1 if success, 0 for failure.
	 * Input to this function should be a descriptor obtained from calling find */
	virtual UDATA acquirePrivateEntry(J9VMThread* currentThread, const J9SharedDataDescriptor* data) = 0;

	/* Attempt to "release" a piece of private data so that it can be used by other JVMs.
	 * Input to this function should be a descriptor obtained from calling find */
	virtual UDATA releasePrivateEntry(J9VMThread* currentThread, const J9SharedDataDescriptor* data) = 0;

	virtual UDATA getNumOfType(UDATA type) = 0;

	virtual UDATA getDataBytesForType(UDATA type) = 0;

	virtual UDATA getUnindexedDataBytes() = 0;
};

#endif /* !defined(BYTEDATAMANAGER_HPP_INCLUDED) */

