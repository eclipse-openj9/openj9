
/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#include "SweepPoolManagerAddressOrderedList.hpp"

class MM_EnvironmentBase;
class MM_MemoryPool;

class MM_SweepPoolManagerVLHGC : public MM_SweepPoolManagerAddressOrderedList
{
private:
protected:
public:

private:

protected:
	/**
	 * addFreeMemoryPostProcess is called after new free memory is added in free list, it is called only once for the same free memory,
	 * if the size of free memory is changed, oldAddrTop will be set.
	 */
	MMINLINE virtual void addFreeMemoryPostProcess(MM_EnvironmentBase *env, MM_MemoryPoolAddressOrderedListBase *memoryPool, void *addrBase, void *addrTop, bool needSync, void *oldAddrTop = NULL);

public:
	static MM_SweepPoolManagerVLHGC *newInstance(MM_EnvironmentBase *env);
	virtual bool initialize(MM_EnvironmentBase *env);
	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManagerVLHGC(MM_EnvironmentBase *env)
		: MM_SweepPoolManagerAddressOrderedList(env)
	{
		_typeId = __FUNCTION__;
	}
};
#endif /* SWEEPPOOLMANAGERVLHGC_HPP_ */
