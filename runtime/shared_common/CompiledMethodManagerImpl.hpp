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

#if !defined(COMPILED_METHOD_MANAGER_IMPL_HPP_INCLUDED)
#define COMPILED_METHOD_MANAGER_IMPL_HPP_INCLUDED

#include "CompiledMethodManager.hpp"
#include "SharedCache.hpp"
#include "j9protos.h"
#include "j9.h"

class SH_CompiledMethodManagerImpl : public SH_CompiledMethodManager
{
public:
	typedef char* BlockPtr;
	
	SH_CompiledMethodManagerImpl();

	~SH_CompiledMethodManagerImpl();

	static SH_CompiledMethodManagerImpl* newInstance(J9JavaVM* vm, SH_SharedCache* cache, SH_CompiledMethodManagerImpl* memForConstructor);

	static UDATA getRequiredConstrBytes(void);

protected:
	virtual U_32 getHashTableEntriesFromCacheSize(UDATA cacheSizeBytes);

	virtual UDATA getKeyForItem(const ShcItem* cacheItem);
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool canCreateHints() { return true; }
#endif
private:
	/* Copy prevention */
	SH_CompiledMethodManagerImpl(const SH_CompiledMethodManagerImpl&);
	SH_CompiledMethodManagerImpl& operator=(const SH_CompiledMethodManagerImpl&);

	void initialize(J9JavaVM* vm, SH_SharedCache* cache, BlockPtr memForConstructor);
};

#endif

