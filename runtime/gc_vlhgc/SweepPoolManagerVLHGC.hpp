
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

#if !defined(SWEEPPOOLMANAGERVLHGC_HPP_)
#define SWEEPPOOLMANAGERVLHGC_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "SweepPoolManager.hpp"

class MM_EnvironmentBase;

class MM_SweepPoolManagerVLHGC : public MM_SweepPoolManager
{
private:
protected:
public:

private:
	/**
	 * A helper to find the size of the final object in a chunk in order to update trailing object and hole data.
	 * @param sweepChunk[in] The chunk for which this data must be calculated
	 * @param trailingCandidate[in] A pointer to the slot after the beginning of the last object in the chunk
	 * @param trailingCandidateSlotCount[in] The number of slots between the trailing candidate and the end of the chunk
	 */
	void calculateTrailingDetails(MM_ParallelSweepChunk *sweepChunk, UDATA *trailingCandidate, UDATA trailingCandidateSlotCount);

protected:
public:
	static MM_SweepPoolManagerVLHGC *newInstance(MM_EnvironmentBase *env);
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void flushFinalChunk(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);
	virtual void connectFinalChunk(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);
	virtual void poolPostProcess(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);
	virtual void connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk);
	virtual bool addFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, UDATA *heapSlotFreeHead, UDATA heapSlotFreeCount);
	virtual void updateTrailingFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, UDATA *heapSlotFreeHead, UDATA heapSlotFreeCount);
	virtual MM_SweepPoolState *getPoolState(MM_MemoryPool *memoryPool);

	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManagerVLHGC(MM_EnvironmentBase *env)
		: MM_SweepPoolManager(env)
	{
		_typeId = __FUNCTION__;
	}

};
#endif /* SWEEPPOOLMANAGERVLHGC_HPP_ */
