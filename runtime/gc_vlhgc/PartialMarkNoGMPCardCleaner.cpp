
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
#include "modronopt.h"

#include "PartialMarkNoGMPCardCleaner.hpp"

#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "HeapMapIterator.hpp"
#include "PartialMarkingScheme.hpp"

void
MM_PartialMarkNoGMPCardCleaner::clean(MM_EnvironmentBase *envModron, void *lowAddress, void *highAddress, Card *cardToClean)
{
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envModron);
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType);
	Assert_MM_true(NULL != _markingScheme);
	
	Card fromState = *cardToClean;
	Card toState = CARD_INVALID;
	bool shouldScan = false;
	bool rememberedOnly = false;
	switch(fromState) {
	case CARD_DIRTY:
	case CARD_PGC_MUST_SCAN:
		shouldScan = true;
		toState = CARD_CLEAN;
		break;
	case CARD_REMEMBERED_AND_GMP_SCAN:
		shouldScan = true;
		rememberedOnly = true;
		toState = CARD_GMP_MUST_SCAN;
		break;
	case CARD_REMEMBERED:
		shouldScan = true;
		rememberedOnly = true;
		toState = CARD_CLEAN;
		break;
	case CARD_GMP_MUST_SCAN:
	    /* GMP is not active, so this state should not be found */
		Assert_MM_unreachable();
		break;
	case CARD_CLEAN:
		Assert_MM_unreachable();
		break;
	default:
		Assert_MM_unreachable();
	}
	/* if we determined a new card state, write it into the card */
	if (CARD_INVALID != toState) {
		*cardToClean = toState;
		
		/* if this state transition is one which requires that we scan the objects in the card (in this cleaner, 
		 * that would be to build inter-region remember data) then do so.
		 */
		if (shouldScan) {
			_markingScheme->scanObjectsInRange(env, lowAddress, highAddress, rememberedOnly);
		}
	}
}
