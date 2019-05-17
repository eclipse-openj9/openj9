
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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(VALUETYPESITERATOR_HPP_)
#define VALUETYPESITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

/**
 * Iterate over all ValueType slots in a class which contain an object reference.
 * @ingroup GC_Structs
 */
class GC_ValueTypesIterator
{
private:
	/**
	 * State constants representing the current stage of the iteration process
	 * @anchor ValueTypeIteratorState
	 */
	typedef enum {
		valuetypesiterator_state_default_value = 0,
		valuetypesiterator_state_end
	} ValueTypesState;
	
	ValueTypesState _state;             /** current Iterator state  */	
	J9Class *_clazzPtr;                 /** Class being scanned for default value  */
	
public:
	GC_ValueTypesIterator(J9Class *clazz)
		: _state(valuetypesiterator_state_default_value)
		, _clazzPtr(clazz)
	{};

	/**
	 * Fetch the next slot in the class.
	 * Note that the pointer is volatile. In concurrent applications the mutator may 
	 * change the value in the slot while iteration is in progress.
	 * @return the address of the default value if the class is valuetype and is not array
	 * @return NULL if not valuetype or is an array
	 * or if the class has already been scanned
	 */
	j9object_t * nextSlot()
	{	
		switch(_state) {
		case valuetypesiterator_state_default_value:
			_state = valuetypesiterator_state_end;
			if (J9_IS_J9CLASS_VALUETYPE(_clazzPtr) && (!J9CLASS_IS_ARRAY(_clazzPtr))) {
				return &(_clazzPtr->flattenedClassCache->defaultValue);
			}
			break;

		default:
			break;
		}

		return NULL;
	}
};

#endif /* VALUETYPESITERATOR_HPP_ */

