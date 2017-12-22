/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "AllocateInitialization.hpp"
#include "EnvironmentBase.hpp"
#include "IndexableObjectAllocationModel.hpp"
#include "MixedObjectAllocationModel.hpp"
#include "ObjectModel.hpp"

omrobjectptr_t
GC_ObjectModelDelegate::initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization)
{
	omrobjectptr_t objectPtr = NULL;

	switch (allocateInitialization->getAllocationCategory()) {
	case MM_JavaObjectAllocationModel::allocation_category_mixed: {
		MM_MixedObjectAllocationModel *mixedObjectAllocationModel = (MM_MixedObjectAllocationModel *)allocateInitialization;
		objectPtr = mixedObjectAllocationModel->initializeMixedObject(env, allocatedBytes);
		break;
	}
	case MM_JavaObjectAllocationModel::allocation_category_indexable: {
		MM_IndexableObjectAllocationModel *indexableObjectAllocationModel = (MM_IndexableObjectAllocationModel *)allocateInitialization;
		objectPtr = (omrobjectptr_t)indexableObjectAllocationModel->initializeIndexableObject(env, allocatedBytes);
		break;
	}
	default:
		Assert_MM_unreachable();
		break;
	}

	return objectPtr;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
GC_ObjectModelDelegate::calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *reservedObjectSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor)
{
	GC_ObjectModel *objectModel = &(env->getExtensions()->objectModel);
	J9Class* clazz = objectModel->getPreservedClass(forwardedHeader);
	uintptr_t actualObjectCopySizeInBytes = 0;
	uintptr_t hashcodeOffset = 0;

	if (objectModel->isIndexable(clazz)) {
		uint32_t size = objectModel->getPreservedIndexableSize(forwardedHeader);
		*objectCopySizeInBytes = getArrayObjectModel()->getSizeInBytesWithHeader(clazz, size);
		hashcodeOffset = getArrayObjectModel()->getHashcodeOffset(clazz, size);
	} else {
		*objectCopySizeInBytes = clazz->totalInstanceSize + sizeof(J9Object);
		hashcodeOffset = getMixedObjectModel()->getHashcodeOffset(clazz);
	}

	if (hashcodeOffset == *objectCopySizeInBytes) {
		if (objectModel->hasBeenMoved(objectModel->getPreservedFlags(forwardedHeader))) {
			*objectCopySizeInBytes += sizeof(uintptr_t);
		} else if (objectModel->hasBeenHashed(objectModel->getPreservedFlags(forwardedHeader))) {
			actualObjectCopySizeInBytes += sizeof(uintptr_t);
		}
	}
	actualObjectCopySizeInBytes += *objectCopySizeInBytes;
	*reservedObjectSizeInBytes = objectModel->adjustSizeInBytes(actualObjectCopySizeInBytes);
	*hotFieldAlignmentDescriptor = clazz->instanceHotFieldDescription;
}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
