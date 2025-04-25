
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include "ScanContinuationSlotsBase.hpp"

#if JAVA_SPEC_VERSION >= 24
#include "ContinuationSlotIterator.hpp"
#endif /* JAVA_SPEC_VERSION >= 24 */
#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"
#include "StackSlotValidator.hpp"

bool
MM_ScanContinuationSlotsBase::initialize(MM_EnvironmentBase *env)
{
	_heap = MM_GCExtensions::getExtensions(env)->heap;
	return true;
}

#if JAVA_SPEC_VERSION >= 24
void
MM_ScanContinuationSlotsBase::doContinuationSlot(MM_EnvironmentBase *env, omrobjectptr_t *slotPtr, GC_ContinuationSlotIterator *continuationSlotIterator)
{
	if (isHeapObject(*slotPtr) && !_heap->objectIsInGap(*slotPtr)) {
		doSlot(env, slotPtr);
	} else if (NULL != *slotPtr) {
		Assert_MM_true(GC_ContinuationSlotIterator::state_monitor_records == continuationSlotIterator->getState());
	}
}
#endif /* JAVA_SPEC_VERSION >= 24 */

void
MM_ScanContinuationSlotsBase::doStackSlot(MM_EnvironmentBase *env, omrobjectptr_t *slotPtr, J9StackWalkState *walkState, const void *stackLocation)
{
	omrobjectptr_t object = *slotPtr;
	if (isHeapObject(object) && !_heap->objectIsInGap(object)) {
		/* heap object - validate and mark */
		Assert_MM_validStackSlot(MM_StackSlotValidator(0, object, stackLocation, walkState).validate(env));
		doSlot(env, slotPtr);
	} else if (NULL != object) {
		/* stack object - just validate */
		Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, object, stackLocation, walkState).validate(env));
	}
}
