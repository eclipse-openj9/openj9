
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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(CLASSSTATICSITERATOR_HPP_)
#define CLASSSTATICSITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9cp.h"
#include "util_api.h"
#include "modron.h"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"

/**
 * Iterate over all static slots in a class which contain an object reference.
 * @ingroup GC_Structs
 */
class GC_ClassStaticsIterator
{
	U_32 _objectStaticCount;
	j9object_t *_staticPtr;
	
public:
	GC_ClassStaticsIterator(MM_EnvironmentBase *env, J9Class *clazz)
		: _objectStaticCount(clazz->romClass->objectStaticCount)
		, _staticPtr((j9object_t *)clazz->ramStatics)
	{
		/* check if the class's statics have been abandoned due to hot
		 * code replace. If so, this class has no statics of its own
		 */
		/*
		 * Note: we have no special recognition for J9ArrayClass here
		 * J9ArrayClass does not have a ramStatics field but something else at this place
		 * so direct check (NULL != clazz->ramStatics) would not be correct,
		 * however romClazz->objectStaticCount must be 0 for this case
		 * as well as J9ArrayClass can not be hot swapped.
		 *
		 * Classes which have been hotswapped by fast HCR still have the ramStatics field
		 * set.  Statics must be walked only once, so only walk them for the most current
		 * version of the class.
		 */
		if ((NULL == _staticPtr) || (J9ClassReusedStatics == (J9CLASS_EXTENDED_FLAGS(clazz) & J9ClassReusedStatics))) {
			_objectStaticCount = 0;
		}
	};

	GC_ClassStaticsIterator(MM_GCExtensionsBase *extensions, J9Class *clazz)
		: _objectStaticCount(clazz->romClass->objectStaticCount)
		, _staticPtr((j9object_t *)clazz->ramStatics)
	{
		/* check if the class's statics have been abandoned due to hot 
		 * code replace. If so, this class has no statics of its own 
		 */
		/*
		 * Note: we have no special recognition for J9ArrayClass here
		 * J9ArrayClass does not have a ramStatics field but something else at this place
		 * so direct check (NULL != clazz->ramStatics) would not be correct,
		 * however romClazz->objectStaticCount must be 0 for this case
		 * as well as J9ArrayClass can not be hot swapped.
		 *
		 * Classes which have been hotswapped by fast HCR still have the ramStatics field
		 * set.  Statics must be walked only once, so only walk them for the most current
		 * version of the class.
		 */
		if ((NULL == _staticPtr) || (J9ClassReusedStatics == (J9CLASS_EXTENDED_FLAGS(clazz) & J9ClassReusedStatics))) {
			_objectStaticCount = 0;
		}
	};

	/**
	 * Fetch the next static field in the class.
	 * Note that the pointer is volatile. In concurrent applications the mutator may 
	 * change the value in the slot while iteration is in progress.
	 * @return the next static slot in the class containing an object reference
	 * @return NULL if there are no more such slots
	 */
	volatile j9object_t *
	nextSlot()
	{
		j9object_t *slotPtr;
		
		if (0 == _objectStaticCount) {
			return NULL;
		}
		_objectStaticCount -= 1;
		slotPtr = _staticPtr++;

		return slotPtr;
	}
};
#endif /* CLASSSTATICSITERATOR_HPP_ */
