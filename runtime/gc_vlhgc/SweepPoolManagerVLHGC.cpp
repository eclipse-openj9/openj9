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

#include "omrcomp.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"

#include "Math.hpp"
#include "CardTable.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "GCExtensions.hpp"
#include "SweepPoolManagerVLHGC.hpp"



MM_SweepPoolManagerVLHGC *
MM_SweepPoolManagerVLHGC::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepPoolManagerVLHGC *sweepPoolManager = (MM_SweepPoolManagerVLHGC *)env->getForge()->allocate(sizeof(MM_SweepPoolManagerVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != sweepPoolManager) {
		new(sweepPoolManager) MM_SweepPoolManagerVLHGC(env);
		if (!sweepPoolManager->initialize(env)) { 
			sweepPoolManager->kill(env);        
			sweepPoolManager = NULL;            
		}                                       
	}

	return sweepPoolManager;
}

bool
MM_SweepPoolManagerVLHGC::initialize(MM_EnvironmentBase *env)
{
	_minimumFreeSize = OMR_MAX(((MM_GCExtensions*)_extensions)->minimumFreeSizeForSurvivor, ((MM_GCExtensions*)_extensions)->getMinimumFreeEntrySize());
	return MM_SweepPoolManagerAddressOrderedList::initialize(env);
}

/**
 * clear Card for the free memory and calculate free size adjustment for free memory card alignment
 */
void
MM_SweepPoolManagerVLHGC::addFreeMemoryPostProcess(MM_EnvironmentBase *env, MM_MemoryPoolAddressOrderedListBase *memoryPool, void *addrBase, void *addrTop, bool needSync, void *oldAddrTop)
{
	/* the post process targets only on the sweep for GMP, which no compaction right after sweep */
	if ((!env->_cycleState->_noCompactionAfterSweep) || (NULL == addrBase))
	{
		return;
	}
	uintptr_t adjustedbytes = 0;
	MM_CardTable *cardTable = _extensions->cardTable;

	void* addrBaseAligned = (void*) MM_Math::roundToCeilingCard((uintptr_t) addrBase);
	void* addrTopAligned  = (void*) MM_Math::roundToFloorCard((uintptr_t) addrTop);
	adjustedbytes += ((uintptr_t)addrTop - (uintptr_t)addrBase);
	uintptr_t minFreeEntrySize = memoryPool->getMinimumFreeEntrySize();
	if (((uintptr_t)addrTopAligned - (uintptr_t)addrBaseAligned) >= minFreeEntrySize) {
		/* clear Cards */
		cardTable->clearCardsInRange(env, addrBaseAligned, addrTopAligned);
		adjustedbytes -= ((uintptr_t)addrTopAligned - (uintptr_t)addrBaseAligned);
	}
	/* for free entry just size update case, remove adjustedbytes for old size */
	if (NULL != oldAddrTop) {
		void* oldAddrTopAligned =(void*) MM_Math::roundToFloorCard((uintptr_t) oldAddrTop);
		adjustedbytes -= ((uintptr_t)oldAddrTop - (uintptr_t)addrBase);
		if (((uintptr_t)oldAddrTopAligned - (uintptr_t)addrBaseAligned) >= minFreeEntrySize) {
			adjustedbytes += ((uintptr_t)oldAddrTopAligned - (uintptr_t)addrBaseAligned);
		}
	}
	if (0 != adjustedbytes) {
		((MM_MemoryPoolAddressOrderedList *)memoryPool)->setAdjustedBytesForCardAlignment(adjustedbytes, needSync);
	}
}

MMINLINE bool
MM_SweepPoolManagerVLHGC::isEligibleForFreeMemory(MM_EnvironmentBase *env, MM_MemoryPoolAddressOrderedListBase *memoryPool, void* address, uintptr_t size)
{
	bool ret = false;
	if (!env->_cycleState->_noCompactionAfterSweep)
	{
		ret = memoryPool->canMemoryBeConnectedToPool(env, address, size);
	} else {
		if (size >= _minimumFreeSize) {
			ret = true;
		}
	}

	if (!ret) {
		memoryPool->fillWithHoles(address, (void*)((UDATA)address+size));
	}
	return ret;
}
