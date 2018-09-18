
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(CONCURRENTSWEEPGC_HPP_)
#define CONCURRENTSWEEPGC_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_GC_CONCURRENT_SWEEP)

#include "EnvironmentBase.hpp"
#include "ParallelGlobalGC.hpp"

class MM_EnvironmentBase;
class MM_MemorySubSpace;

/**
 * Temporary GC class to finish full blown concurrent sweep functionality.
 * This class allows exploration of details concurrent sweep is missing to operate in a real world environment.
 */
class MM_ConcurrentSweepGC : public MM_ParallelGlobalGC
{
private:
	J9JavaVM *_javaVM;
protected:
public:

private:
protected:
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, U_32 gcCode);

public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_COLLECTOR_CONCURRENTSWEEPGC; };
	
	static MM_ConcurrentSweepGC *newInstance(MM_EnvironmentBase *env);

	virtual void payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription);
		
	virtual bool replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, UDATA size);

	MM_ConcurrentSweepGC(MM_EnvironmentBase *env)
		: MM_ParallelGlobalGC(env)
		, _javaVM((J9JavaVM*)env->getOmrVM()->_language_vm)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* J9VM_GC_CONCURRENT_SWEEP */

#endif /* CONCURRENTSWEEPGC_HPP_ */
