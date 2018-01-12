
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"

#include "CopyScanCacheChunkVLHGC.hpp"
#include "CopyScanCacheVLHGC.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"


MM_CopyScanCacheChunkVLHGC *
MM_CopyScanCacheChunkVLHGC::newInstance(MM_EnvironmentVLHGC*env, UDATA cacheEntryCount, MM_CopyScanCacheVLHGC **nextCacheAddr, MM_CopyScanCacheChunkVLHGC *nextChunk)
{
	MM_CopyScanCacheChunkVLHGC *chunk;
	
	chunk = (MM_CopyScanCacheChunkVLHGC *)env->getForge()->allocate(sizeof(MM_CopyScanCacheChunkVLHGC) + cacheEntryCount * sizeof(MM_CopyScanCacheVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (chunk) {
		new(chunk) MM_CopyScanCacheChunkVLHGC();
		if(!chunk->initialize(env, cacheEntryCount, nextCacheAddr, nextChunk)) {
			chunk->kill(env);
			return NULL;
		}
	}
	return chunk;	
}

void
MM_CopyScanCacheChunkVLHGC::kill(MM_EnvironmentVLHGC *env)
{
	tearDown(env);
	env->getForge()->free(this);
}


bool
MM_CopyScanCacheChunkVLHGC::initialize(MM_EnvironmentVLHGC *env, UDATA cacheEntryCount, MM_CopyScanCacheVLHGC **nextCacheAddr, MM_CopyScanCacheChunkVLHGC *nextChunk)
{
	_baseCache = (MM_CopyScanCacheVLHGC *)(this + 1);
	_nextChunk = nextChunk;
	
	MM_CopyScanCacheVLHGC *currentCache = _baseCache + cacheEntryCount;

	while(--currentCache >= _baseCache) {
		new(currentCache) MM_CopyScanCacheVLHGC();
		currentCache->next = *nextCacheAddr;
		*nextCacheAddr = currentCache;
	}
	
	return true;
}

void
MM_CopyScanCacheChunkVLHGC::tearDown(MM_EnvironmentVLHGC *env)
{
	_baseCache = NULL;
	_nextChunk = NULL;
}


