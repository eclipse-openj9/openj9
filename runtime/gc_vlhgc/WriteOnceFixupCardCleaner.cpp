
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include "WriteOnceFixupCardCleaner.hpp"

#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "HeapMapWordIterator.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "WriteOnceCompactor.hpp"


MM_WriteOnceFixupCardCleaner::MM_WriteOnceFixupCardCleaner(MM_WriteOnceCompactor *compactScheme, MM_CycleState *cycleState, MM_HeapRegionManager* regionManager)
	: MM_CardCleaner()
	, _compactScheme(compactScheme)
	, _isGlobalMarkPhaseRunning(NULL != cycleState->_externalCycleState)
	, _regionManager(regionManager)
{
	_typeId = __FUNCTION__;
	Assert_MM_true(MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == cycleState->_collectionType);
	Assert_MM_true(NULL != _compactScheme);
	Assert_MM_true(NULL != regionManager);
}

void
MM_WriteOnceFixupCardCleaner::clean(MM_EnvironmentBase *envModron, void *lowAddress, void *highAddress, Card *cardToClean)
{
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envModron);
	Card fromState = *cardToClean;
	Card toState = CARD_INVALID;
	bool rememberedOnly = false;	
	switch(fromState) {
	case CARD_DIRTY:
		if (_isGlobalMarkPhaseRunning) {
			toState = CARD_GMP_MUST_SCAN;
		} else {
			toState = CARD_CLEAN;
		}
		break;
	case CARD_MARK_COMPACT_TRANSITION:
		/* presently, this state needs to be treated much the same was as PGC_MUST_SCAN but we know that any objects under the card which point to other regions have been remembered */
		rememberedOnly = true;
	case CARD_PGC_MUST_SCAN:
		if (_isGlobalMarkPhaseRunning) {
			/* This transition seems incorrect but is absolutely essential so some explanation will be provided:
			 * -(note that the GMP doesn't build RSCL for non-overflowed regions)
			 * -consider a card containing a object which doesn't point into the collection set
			 * -the mutator writes a pointer into the collection set to the object and sets the card to DIRTY
			 * -a GMP runs, marking the referenced object but not updating the RSCL and then sets the card to PGC_MUST_SCAN
			 * -a PGC runs, marks the referenced object but realizes that it points into the collection set so it doesn't clear the state
			 * and doesn't update the RSCL
			 * -compact then moves the object, adds the reference to the RSCL, and fixup sets its state to CLEAN
			 * -the GMP then runs, ignores the now-clean card and finishes marking
			 * -observe that the new location of the referenced object was never marked in the next mark map so the map is invalid
			 */
			toState = CARD_GMP_MUST_SCAN;
		} else {
			toState = CARD_CLEAN;
		}
		break;
	case CARD_REMEMBERED_AND_GMP_SCAN:
		Assert_MM_true(_isGlobalMarkPhaseRunning);	
		rememberedOnly = true;
		toState = CARD_GMP_MUST_SCAN;
		break;
	case CARD_REMEMBERED:
		rememberedOnly = true;
		toState = CARD_CLEAN;
		break;		
	case CARD_GMP_MUST_SCAN:
		Assert_MM_true(_isGlobalMarkPhaseRunning);
		break;
	default:
		Assert_MM_unreachable();
	}
	/* only update the card state if we identified a transition we are interested in since some are to be ignored:
	 * Consider the case of seeing CARD_GMP_MUST_SCAN during a PGC (which is allowed to happen if we have GMP followed
	 * by GMP).  We want to ignore this transition.
	 */
	if (CARD_INVALID != toState) {
		*cardToClean = toState;

		/* we should not be cleaning cards in compact regions -- those objects get fixed up differently */
		Assert_MM_false(((MM_HeapRegionDescriptorVLHGC *)_regionManager->tableDescriptorForAddress(lowAddress))->_compactData._shouldCompact);

		_compactScheme->fixupObjectsInRange(env, lowAddress, highAddress, rememberedOnly);

	}
}
