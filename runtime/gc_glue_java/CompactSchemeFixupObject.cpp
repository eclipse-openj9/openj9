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
#include "j9.h"

#include "CompactSchemeFixupObject.hpp"

#include "objectdescription.h"

#include "CollectorLanguageInterfaceImpl.hpp"
#include "EnvironmentStandard.hpp"
#include "Debug.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "MixedObjectIterator.hpp"
#include "ObjectAccessBarrier.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "VMHelpers.hpp"
#include "ParallelDispatcher.hpp"
#include "PointerContiguousArrayIterator.hpp"
#include "FlattenedContiguousArrayIterator.hpp"
#include "Task.hpp"

void
MM_CompactSchemeFixupObject::fixupMixedObject(omrobjectptr_t objectPtr)
{
	GC_MixedObjectIterator it(_omrVM, objectPtr);
	GC_SlotObject *slotObject;

	while (NULL != (slotObject = it.nextSlot())) {
		_compactScheme->fixupObjectSlot(slotObject);
	}
}

void
MM_CompactSchemeFixupObject::doStackSlot(MM_EnvironmentBase *env, omrobjectptr_t fromObject, omrobjectptr_t *slot)
{
	*slot = _compactScheme->getForwardingPtr(*slot);
}

/**
 * @todo Provide function documentation
 */
void
stackSlotIteratorForCompactScheme(J9JavaVM *javaVM, J9Object **slotPtr, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	StackIteratorData4CompactSchemeFixupObject *data = (StackIteratorData4CompactSchemeFixupObject *)localData;
	data->compactSchemeFixupObject->doStackSlot(data->env, data->fromObject, slotPtr);
}


void
MM_CompactSchemeFixupObject::fixupContinuationNativeSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* fixup Java Stacks in J9VMContinuation */
	J9VMThread *currentThread = (J9VMThread *)env->getLanguageVMThread();
	/* In sliding compaction we must fix slots exactly once. Since we will fixup stack slots of
	 * mounted Virtual threads later during root fixup, we will skip it during this heap fixup pass
	 * (hence passing true for scanOnlyUnmounted parameter).
	 */
	const bool isConcurrentGC = false;
	const bool isGlobalGC = true;
	const bool beingMounted = false;
	if (MM_GCExtensions::needScanStacksForContinuationObject(currentThread, objectPtr, isConcurrentGC, isGlobalGC, beingMounted)) {
		StackIteratorData4CompactSchemeFixupObject localData;
		localData.compactSchemeFixupObject = this;
		localData.env = env;
		localData.fromObject = objectPtr;

		GC_VMThreadStackSlotIterator::scanContinuationSlots(currentThread, objectPtr, (void *)&localData, stackSlotIteratorForCompactScheme, false, false);
	}
}

void
MM_CompactSchemeFixupObject::fixupContinuationObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* fixup continuation java stack first and then fixup the Object itself, fixup order shouldn't matter. */
	fixupContinuationNativeSlots(env, objectPtr);
	fixupMixedObject(objectPtr);
}

void
MM_CompactSchemeFixupObject::fixupArrayObject(omrobjectptr_t objectPtr)
{
	GC_PointerContiguousArrayIterator it(_omrVM, objectPtr);
	GC_SlotObject *slotObject;

	while (NULL != (slotObject = it.nextSlot())) {
		_compactScheme->fixupObjectSlot(slotObject);
	}
}

void
MM_CompactSchemeFixupObject::fixupFlattenedArrayObject(omrobjectptr_t objectPtr)
{
	GC_FlattenedContiguousArrayIterator it(_omrVM, objectPtr);
	GC_SlotObject *slotObject;

	while (NULL != (slotObject = it.nextSlot())) {
		_compactScheme->fixupObjectSlot(slotObject);
	}
}

MMINLINE void
MM_CompactSchemeFixupObject::addOwnableSynchronizerObjectInList(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	/* if isObjectInOwnableSynchronizerList() return NULL, it means the object isn't in OwnableSynchronizerList,
	 * it could be the constructing object which would be added in the list after the construction finish later. ignore the object to avoid duplicated reference in the list. */
	if (NULL != _extensions->accessBarrier->isObjectInOwnableSynchronizerList(objectPtr)) {
		env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->add(env, objectPtr);
	}
}

void
MM_CompactSchemeFixupObject::fixupObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	switch(env->getExtensions()->objectModel.getScanType(objectPtr)) {
	case GC_ObjectModel::SCAN_MIXED_OBJECT_LINKED:
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		fixupMixedObject(objectPtr);
		break;

	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		fixupArrayObject(objectPtr);
		break;

	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		/* nothing to do */
		break;

	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		addOwnableSynchronizerObjectInList(env, objectPtr);
		fixupMixedObject(objectPtr);
		break;
	case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
		fixupContinuationObject(env, objectPtr);
		break;
	case GC_ObjectModel::SCAN_FLATTENED_ARRAY_OBJECT:
		fixupFlattenedArrayObject(objectPtr);
		break;

	default:
		Assert_MM_unreachable();
	}
}


void
MM_CompactSchemeFixupObject::verifyForwardingPtr(omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr)
{
	assume0(forwardingPtr <= objectPtr);
	assume0(J9GC_J9OBJECT_CLAZZ(forwardingPtr, _extensions) && ((uintptr_t)J9GC_J9OBJECT_CLAZZ(forwardingPtr, _extensions) & 0x3) == 0);
}
