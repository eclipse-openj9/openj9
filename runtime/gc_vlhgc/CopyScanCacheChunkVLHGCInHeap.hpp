
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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(COPYSCANCACHECHUNKVLHGCINHEAP_HPP_)
#define COPYSCANCACHECHUNKVLHGCINHEAP_HPP_

#include "CopyScanCacheChunkVLHGC.hpp" 

#include "GCExtensions.hpp"


/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CopyScanCacheChunkVLHGCInHeap : public MM_CopyScanCacheChunkVLHGC
{
public:
	/**
	 * Returns the number of scan caches which are allocated as part of an instance of the receiver (since they are all currently the same size).
	 * @param env[in] A GC thread
	 * @return The number of caches which will be allocated as part of an instance of the receiver
	 */
	static UDATA numberOfCachesInChunk(MM_EnvironmentVLHGC *env);
	/**
	 * The number of bytes required to allocate an instance of the receiver (since they are all currently the same size).
	 * @param env[in] A GC thread
	 * @return The size, in bytes, of the memory extent required to hold one instance of the receiver
	 */
	static UDATA bytesRequiredToAllocateChunkInHeap(MM_EnvironmentVLHGC *env);
	static MM_CopyScanCacheChunkVLHGCInHeap *newInstance(MM_EnvironmentVLHGC *env, void *buffer, UDATA bufferLengthInBytes, MM_CopyScanCacheVLHGC **nextCacheAddr, MM_CopyScanCacheChunkVLHGC *nextChunk);
	bool initialize(MM_EnvironmentVLHGC *env, UDATA cacheEntryCount, MM_CopyScanCacheVLHGC **nextCacheAddr, MM_CopyScanCacheChunkVLHGC *nextChunk);
	virtual void kill(MM_EnvironmentVLHGC *env);

	/**
	 * Create a CopyScanCacheChunkVLHGCInHeap object.
	 */
	MM_CopyScanCacheChunkVLHGCInHeap()
		: MM_CopyScanCacheChunkVLHGC()
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* COPYSCANCACHECHUNKVLHGCINHEAP_HPP_ */

