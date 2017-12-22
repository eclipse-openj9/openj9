
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

#include "CopyScanCacheVLHGC.hpp"
#include "CopyScanCacheChunkVLHGCInHeap.hpp"


UDATA
MM_CopyScanCacheChunkVLHGCInHeap::numberOfCachesInChunk(MM_EnvironmentVLHGC *env)
{
	UDATA tlhMinimumSize = MM_GCExtensions::getExtensions(env)->tlhMinimumSize;
	UDATA sizeToAllocate = sizeof(MM_CopyScanCacheChunkVLHGCInHeap);
	UDATA numberOfCaches = 1;

	if (sizeToAllocate < tlhMinimumSize) {
		/* calculate number of caches to just barely exceed tlhMinimumSize */
		numberOfCaches = ((tlhMinimumSize - sizeToAllocate) / sizeof(MM_CopyScanCacheVLHGC)) + 1;
	}
	return numberOfCaches;
}

UDATA
MM_CopyScanCacheChunkVLHGCInHeap::bytesRequiredToAllocateChunkInHeap(MM_EnvironmentVLHGC *env)
{
	UDATA sizeToAllocate = sizeof(MM_CopyScanCacheChunkVLHGCInHeap);
	UDATA numberOfCaches = numberOfCachesInChunk(env);

	/* total size required to allocate */
	sizeToAllocate += numberOfCaches * sizeof(MM_CopyScanCacheVLHGC);
	/* this is going to be allocated on the heap so ensure that the chunk we are allocating is adjusted for heap alignment (since object consumed sizes already have this requirement) */
	sizeToAllocate = MM_Math::roundToCeiling(env->getObjectAlignmentInBytes(), sizeToAllocate);
	return sizeToAllocate;
}

MM_CopyScanCacheChunkVLHGCInHeap *
MM_CopyScanCacheChunkVLHGCInHeap::newInstance(MM_EnvironmentVLHGC *env, void *buffer, UDATA bufferLengthInBytes, MM_CopyScanCacheVLHGC **nextCacheAddr, MM_CopyScanCacheChunkVLHGC *nextChunk)
{
	/* make sure that the memory extent is exactly the right size to allocate an instance of the receiver */
	Assert_MM_true(bytesRequiredToAllocateChunkInHeap(env) == bufferLengthInBytes);
	MM_CopyScanCacheChunkVLHGCInHeap *chunk = (MM_CopyScanCacheChunkVLHGCInHeap *)buffer;
	new(chunk) MM_CopyScanCacheChunkVLHGCInHeap();
	if (!chunk->initialize(env, numberOfCachesInChunk(env), nextCacheAddr, nextChunk)) {
		chunk->tearDown(env);
		chunk = NULL;
	}
	return chunk;	
}

bool
MM_CopyScanCacheChunkVLHGCInHeap::initialize(MM_EnvironmentVLHGC *env, UDATA cacheEntryCount, MM_CopyScanCacheVLHGC **nextCacheAddr, MM_CopyScanCacheChunkVLHGC *nextChunk)
{
	bool success = MM_CopyScanCacheChunkVLHGC::initialize(env, cacheEntryCount, nextCacheAddr, nextChunk);
	if (success) {
		MM_CopyScanCacheVLHGC *structureArrayBase = getBase();
		for (UDATA i = 0; i < cacheEntryCount; i++) {
			structureArrayBase[i].flags |= J9VM_MODRON_SCAVENGER_CACHE_TYPE_HEAP;
		}
	}
	return success;
}

void
MM_CopyScanCacheChunkVLHGCInHeap::kill(MM_EnvironmentVLHGC *env)
{
	tearDown(env);
	/* the receiver's memory was allocated for it in memory which does not require cleanup */
}
