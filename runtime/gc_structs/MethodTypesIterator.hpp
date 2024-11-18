
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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(METHODTYPESITERATOR_HPP_)
#define METHODTYPESITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

/**
 * Iterate over all MethodType slots in a class which contain an object reference.
 * @ingroup GC_Structs
 */
class GC_MethodTypesIterator
{
	uint32_t _methodTypeTotal;
	uint32_t _methodTypeCount;
	j9object_t *_methodTypePtr;
	
public:
	GC_MethodTypesIterator(uint32_t methodTypeCount, j9object_t *methodTypes)
		: _methodTypeCount(methodTypeCount)
		, _methodTypePtr(methodTypes)
	{
		/* check if the class's MethodTypes have been abandoned due to hot
		 * code replace. If so, this class has no MethodTypes of its own
		 */
		if (NULL == _methodTypePtr) {
			_methodTypeCount = 0;
		}
		_methodTypeTotal = _methodTypeCount;
	}

	/**
	 * @return the next MethodType slot in the class containing an object reference
	 * @return NULL if there are no more such slots
	 */
	j9object_t *
	nextSlot()
	{
		j9object_t *slotPtr = NULL;
		
		if (0 != _methodTypeCount) {
			_methodTypeCount -= 1;
			slotPtr = _methodTypePtr++;
		}

		return slotPtr;
	}

	/**
	 * Gets the current MethodType index.
	 * @return zero based MethodType index of the entry returned by the last call of nextSlot.
	 * @return -1 if nextSlot has yet to be called.
	 */
	MMINLINE intptr_t getIndex() {
		return _methodTypeTotal - _methodTypeCount - 1;
	}
};

#endif /* METHODTYPESITERATOR_HPP_ */

