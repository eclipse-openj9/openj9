
/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(MIXEDOBJECTALLOCATIONMODEL_HPP_)
#define MIXEDOBJECTALLOCATIONMODEL_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "JavaObjectAllocationModel.hpp"

/**
 * Class definition for the mixed object allocation model.
 */
class MM_MixedObjectAllocationModel : public MM_JavaObjectAllocationModel
{
	/*
	 * Member data and types
	 */
private:

protected:

public:

	/*
	 * Member functions
	 */
private:
	/**
	 * Calculate the total allocation size in bytes required to represent a Java object (or array) given
	 * the number of bytes required to hold the instance data (without header or hash code). This adds
	 * on the size in bytes of the
	 */
	static uintptr_t
	calculateRequiredSize(MM_EnvironmentBase *env, J9Class *clazz, uintptr_t allocateFlags)
	{
		/* Calculate the size in bytes required for the object being allocated */
		uintptr_t sizeInBytesRequired = clazz->totalInstanceSize + sizeof(J9Object);

#if defined (J9VM_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL)
		if (OMR_GC_ALLOCATE_OBJECT_HASHED == (OMR_GC_ALLOCATE_OBJECT_HASHED & allocateFlags)) {
			if (sizeInBytesRequired == env->getExtensions()->mixedObjectModel.getHashcodeOffset(clazz)) {
				/* Add extra uintptr_t for hash */
				sizeInBytesRequired += sizeof(uintptr_t);
			}
		}
#endif /* defined (J9VM_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL) */

		return env->getExtensions()->objectModel.adjustSizeInBytes(sizeInBytesRequired);
	}

protected:

public:
	/**
	 * Constructor.
	 */
	MM_MixedObjectAllocationModel(MM_EnvironmentBase *env, J9Class *clazz, uintptr_t allocateObjectFlags = 0)
		: MM_JavaObjectAllocationModel(env, clazz, allocation_category_mixed,
				calculateRequiredSize(env, clazz, allocateObjectFlags),
				allocateObjectFlags)
	{}

	/**
	 * Vet the allocation description and set the _isAllocatable flag to false if not viable.
	 *
	 * @param[in] env the environment for the calling thread
	 * @return false if the allocation cannot proceed
	 */
	MMINLINE bool
	initializeAllocateDescription(MM_EnvironmentBase *env)
	{
#if defined(OMR_GC_VLHGC)
		if (isAllocatable() && env->getExtensions()->isVLHGC()) {
			/* CMVC 170688:  Ensure that we don't try to allocate an object which will overflow the region
			 * if it ever grows (easier to handle this case in the allocator than to special-case the
			 * collectors to know how to avoid this case) Currently, we only grow by a hashcode slot
			 * which is 4-bytes but will increase our size by the granule of alignment.
			 */
			uintptr_t objectSizeAfterGrowing = getAllocateDescription()->getBytesRequested() + env->getObjectAlignmentInBytes();
			if (objectSizeAfterGrowing > env->getExtensions()->regionSize) {
				setAllocatable(false);
			}
		}
#endif /* defined(OMR_GC_VLHGC) */

		return isAllocatable();
	}

	/**
	 * Initializer.
	 */
	MMINLINE omrobjectptr_t
	initializeMixedObject(MM_EnvironmentBase *env, void *allocatedBytes)
	{
		/* Initialize object header */
		omrobjectptr_t objectPtr = initializeJavaObject(env, allocatedBytes);

		/* Initialize hashcode slot */
		if (getAllocateDescription()->getPreHashFlag()) {
			env->getExtensions()->objectModel.initializeHashSlot((J9JavaVM*)env->getLanguageVM(), objectPtr);
		}

		return objectPtr;
	}
};
#endif /* MIXEDOBJECTALLOCATIONMODEL_HPP_ */
