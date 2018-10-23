
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(JAVAOBJECTALLOCATIONMODEL_HPP_)
#define JAVAOBJECTALLOCATIONMODEL_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "objectdescription.h"
#include "modron.h"

#include "AllocateInitialization.hpp"
#include "ObjectAccessBarrier.hpp"

/**
 * Class definition for the Java object allocation model.
 */
class MM_JavaObjectAllocationModel : public MM_AllocateInitialization
{
	/*
	 * Member data and types
	 */
public:
	/**
	 * Define object allocation categories. These are represented in MM_AllocateInitialization
	 * objects and are used in GC_ObjectModel::initializeAllocation() to determine how to
	 * initialize the header of a newly allocated object.
	 */
	enum {
		allocation_category_mixed
		, allocation_category_indexable
	};

protected:
	J9Class *_class;

private:

	/*
	 * Member functions
	 */
private:

	MMINLINE J9Class *getClass() { return J9_CURRENT_CLASS(_class); }

protected:

public:

	/**
	 * Initializer.
	 */
	MMINLINE omrobjectptr_t
	initializeJavaObject(MM_EnvironmentBase *env, void *allocatedBytes)
	{
		omrobjectptr_t objectPtr = (omrobjectptr_t)allocatedBytes;

		if (NULL != objectPtr) {
			/* Initialize class pointer in object header -- preserve flags set by base class */
			MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
			extensions->objectModel.setObjectClass(objectPtr, getClass());

			/* This might set the remembered bit in the header flags ... */
			J9VMThread *vmThread = (J9VMThread *)env->getOmrVMThread()->_language_vmthread;
			extensions->accessBarrier->recentlyAllocatedObject(vmThread, objectPtr);
		}

		return objectPtr;
	}

	/**
	 * Constructor.
	 */
	MM_JavaObjectAllocationModel(MM_EnvironmentBase *env,
			J9Class *clazz,
			uintptr_t allocationCategory,
			uintptr_t requiredSizeInBytes,
			uintptr_t allocateObjectFlags = 0
	)
		: MM_AllocateInitialization(env, allocationCategory, requiredSizeInBytes, allocateObjectFlags)
		, _class(clazz)
	{}
};
#endif /* JAVAOBJECTALLOCATIONMODEL_HPP_ */
