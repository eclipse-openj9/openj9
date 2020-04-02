/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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
 * @ingroup GC_Structs
 */

#if !defined(MIXEDOBJECTSCANNER_HPP_)
#define MIXEDOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "objectdescription.h"
#include "GCExtensions.hpp"
#include "HeadlessMixedObjectScanner.hpp"

/**
 * This class is used to iterate over the slots of a Java object.
 */
class GC_MixedObjectScanner : public GC_HeadlessMixedObjectScanner
{

	/* Data Members */
private:

protected:

public:

	/* Member Functions */
private:
protected:
	/**
	 * @param env The scanning thread environment
	 * @param objectPtr the object to be processed
	 * @param flags Scanning context flags
	 */
	MMINLINE GC_MixedObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, uintptr_t flags)
		: GC_HeadlessMixedObjectScanner(env, env->getExtensions()->mixedObjectModel.getHeadlessObject(objectPtr), env->getExtensions()->mixedObjectModel.getSizeInBytesWithoutHeader(J9GC_J9OBJECT_CLAZZ(objectPtr, env)), flags)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Subclasses must call this method to set up the instance description bits and description pointer.
	 * @param[in] env The scanning thread environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env, J9Class *clazzPtr)
	{
#if defined(J9VM_GC_LEAF_BITS)
		GC_HeadlessMixedObjectScanner::initialize(env, clazzPtr->instanceDescription, clazzPtr->instanceLeafDescription);
#else /* J9VM_GC_LEAF_BITS */
		GC_HeadlessMixedObjectScanner::initialize(env, clazzPtr->instanceDescription);
#endif /* J9VM_GC_LEAF_BITS */
	}

public:
	/**
	 * In-place instantiation and initialization for mixed object scanner.
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr The object to scan
	 * @param[in] allocSpace Pointer to space for in-place instantiation (at least sizeof(GC_MixedObjectScanner) bytes)
	 * @param[in] flags Scanning context flags
	 * @return Pointer to GC_MixedObjectScanner instance in allocSpace
	 */
	MMINLINE static GC_MixedObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
	{
		GC_MixedObjectScanner *objectScanner = (GC_MixedObjectScanner *)allocSpace;
		J9Class *classPtr = J9GC_J9OBJECT_CLAZZ(objectPtr, env);

		new(objectScanner) GC_MixedObjectScanner(env, objectPtr, flags);
		objectScanner->initialize(env, classPtr);
		return objectScanner;
	}
};
#endif /* MIXEDOBJECTSCANNER_HPP_ */
