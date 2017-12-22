
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include "j9consts.h"

#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "MarkingDelegate.hpp"
#include "MarkingScheme.hpp"
#include "MarkingSchemeRootMarker.hpp"
#include "StackSlotValidator.hpp"
#include "VMThreadIterator.hpp"

void
MM_MarkingSchemeRootMarker::doSlot(omrobjectptr_t *slotPtr)
{
	_markingScheme->inlineMarkObject(_env, *slotPtr);
}

void
MM_MarkingSchemeRootMarker::doStackSlot(omrobjectptr_t *slotPtr, void *walkState, const void* stackLocation)
{
	omrobjectptr_t object = *slotPtr;
	if (_markingScheme->isHeapObject(object) && !_extensions->heap->objectIsInGap(object)) {
		/* heap object - validate and mark */
		Assert_MM_validStackSlot(MM_StackSlotValidator(0, object, stackLocation, walkState).validate(_env));
		_markingScheme->inlineMarkObject(_env, object);

	} else if (NULL != object) {
		/* stack object - just validate */
		Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, object, stackLocation, walkState).validate(_env));
	}
}

void
MM_MarkingSchemeRootMarker::doVMThreadSlot(omrobjectptr_t *slotPtr, GC_VMThreadIterator *vmThreadIterator)
{
	omrobjectptr_t object = *slotPtr;
	if (_markingScheme->isHeapObject(object) && !_extensions->heap->objectIsInGap(object)) {
		_markingScheme->inlineMarkObject(_env, object);
	} else if (NULL != object) {
		Assert_MM_true(vmthreaditerator_state_monitor_records == vmThreadIterator->getState());
	}
}

void
MM_MarkingSchemeRootMarker::doClass(J9Class *clazz)
{
	_markingDelegate->scanClass(_env, clazz);
}

void
MM_MarkingSchemeRootMarker::doClassLoader(J9ClassLoader *classLoader)
{
	/* Scan any classloader that hasn't previously marked as dead.  Don't mark them as scanned, since
	 * we won't be running MM_ParallelGlobalGC::unloadDeadClassLoaders() which clears the bit
	 */
	if(J9_GC_CLASS_LOADER_DEAD != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
		_markingScheme->inlineMarkObject(_env, classLoader->classLoaderObject);
	}
}

void
MM_MarkingSchemeRootMarker::doFinalizableObject(omrobjectptr_t object)
{
	_markingScheme->inlineMarkObject(_env, object);
}

