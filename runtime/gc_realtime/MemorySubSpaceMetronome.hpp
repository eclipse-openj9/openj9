/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(MEMORYSUBSPACEMETRONOME_HPP_)
#define MEMORYSUBSPACEMETRONOME_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "MemorySubSpaceSegregated.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_GCCode;
class MM_MemoryPool;

/**
 * @todo Provide class documentation
 * @ingroup GC_Metronome
 */
class MM_MemorySubSpaceMetronome : public MM_MemorySubSpaceSegregated
{
private:
	/* TODO: this is temporary as a way to avoid dup code in MemorySubSpaceMetronome::allocate. 
	 * We will specifically fix this allocate method in a separate design.
	 */
	void *allocateMixedObjectOrArraylet(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, AllocateType allocType);
	void collectOnOOM(MM_EnvironmentBase *env, MM_GCCode gcCode, MM_AllocateDescription *allocDescription);

protected:
	bool initialize(MM_EnvironmentBase *env);

public:
	static MM_MemorySubSpaceMetronome *newInstance(
		MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemoryPool *memoryPool,
		bool usesGlobalCollector, UDATA minimumSize, UDATA initialSize, UDATA maximumSize);

	virtual const char *getName() { return MEMORY_SUBSPACE_NAME_METRONOME; }
	virtual const char *getDescription() { return MEMORY_SUBSPACE_DESCRIPTION_METRONOME; }

	virtual void systemGarbageCollect(MM_EnvironmentBase *env, U_32 gcCode);
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
#if defined(OMR_GC_ARRAYLETS)
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
#endif /* defined(OMR_GC_ARRAYLETS) */

 	void collect(MM_EnvironmentBase *env, MM_GCCode gcCode);

	
	MM_MemorySubSpaceMetronome(
		MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemoryPool *memoryPool,
		bool usesGlobalCollector, UDATA minimumSize, UDATA initialSize, UDATA maximumSize
	)
 		: MM_MemorySubSpaceSegregated(env, physicalSubArena, memoryPool, usesGlobalCollector, minimumSize, initialSize, maximumSize)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* MEMORYSUBSPACEMETRONOME_HPP_ */

