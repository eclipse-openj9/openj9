
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

#include "j9.h"
#include "j9cfg.h"

#include "IncrementalCardTable.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"

MM_IncrementalCardTable *
MM_IncrementalCardTable::newInstance(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_IncrementalCardTable *cardTable = (MM_IncrementalCardTable *)env->getForge()->allocate(sizeof(MM_IncrementalCardTable), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != cardTable) {
		new(cardTable) MM_IncrementalCardTable();
		if (!cardTable->initialize(env, heap)) {
			cardTable->kill(env);
			cardTable = NULL;
		}
	}
	return cardTable;
}

bool
MM_IncrementalCardTable::initialize(MM_EnvironmentBase *env, MM_Heap *heap)
{
	bool initialized = MM_CardTable::initialize(env, heap);
	if (initialized) {
		_heapAlloc = (void *)heap->getHeapTop();
		_cardTableSize = calculateCardTableSize(env, heap->getMaximumPhysicalRange());
	}
	return initialized;
}

void
MM_IncrementalCardTable::tearDown(MM_EnvironmentBase *env)
{
	/*
	 * Card Table Memory Decommit should be here however we can not do it properly because of:
	 * - wrong startup sequence (allocation failure caused tear down of Global Collector with heap already set to NULL
	 * - decommit card table memory logic is too "smart" and required alive heap to tear down
	 *
	 * Currently it is not critical because we just line away to release memory in MM_CardTable::tearDown.
	 * Temporary comment out decommit request until commit/decommit logic will be improved
	 */
	/*
	if (isCardTableMemoryInitialized()) {
		Card *lowCard = getCardTableStart();
		Card *highCard = (Card *)((UDATA)lowCard + _cardTableSize);
		decommitCardTableMemory(env, lowCard, highCard, lowCard, highCard);
	}
	*/
	MM_CardTable::tearDown(env);
}
