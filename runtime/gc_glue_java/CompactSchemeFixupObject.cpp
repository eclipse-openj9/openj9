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

#include "CompactSchemeFixupObject.hpp"

#include "objectdescription.h"

#include "CollectorLanguageInterfaceImpl.hpp"
#include "EnvironmentStandard.hpp"
#include "Debug.hpp"
#include "Dispatcher.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "MixedObjectIterator.hpp"
#include "ObjectAccessBarrier.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "PointerContiguousArrayIterator.hpp"
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
MM_CompactSchemeFixupObject::fixupArrayObject(omrobjectptr_t objectPtr)
{
	GC_PointerContiguousArrayIterator it(_omrVM, objectPtr);
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

	default:
		Assert_MM_unreachable();
	}
}


void
MM_CompactSchemeFixupObject::verifyForwardingPtr(omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr)
{
	assume0(forwardingPtr <= objectPtr);
	assume0(J9GC_J9OBJECT_CLAZZ(forwardingPtr) && ((UDATA)J9GC_J9OBJECT_CLAZZ(forwardingPtr) & 0x3) == 0);
}
