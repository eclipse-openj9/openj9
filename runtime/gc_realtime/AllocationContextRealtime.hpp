
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

/**
 * @file
 * @ingroup GC_Metronome
 */

#if !defined(ALLOCATIONCONTEXTREALTIME_HPP_)
#define ALLOCATIONCONTEXTREALTIME_HPP_

#include "AllocationContext.hpp"
#include "AllocationContextSegregated.hpp"

class MM_EnvironmentBase;
class MM_GlobalAllocationManagerSegregated;
class MM_RegionPoolSegregated;

class MM_AllocationContextRealtime : public MM_AllocationContextSegregated
{
/* Data members / Types */
public:

protected:

private:

/* Methods */
public:
	static MM_AllocationContextRealtime *newInstance(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool);

	virtual uintptr_t *allocateLarge(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired);

protected:
	MM_AllocationContextRealtime(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool)
		: MM_AllocationContextSegregated(env, gam, regionPool)
	{
		_typeId = __FUNCTION__;
	}

	virtual bool shouldPreMarkSmallCells(MM_EnvironmentBase *env);
	virtual bool trySweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass, uintptr_t *sweepCount, U_64 *sweepStartTime);
	virtual void signalSmallRegionDepleted(MM_EnvironmentBase *env, uintptr_t sizeClass);

private:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
};

#endif /* ALLOCATIONCONTEXTREALTIME_HPP_ */
