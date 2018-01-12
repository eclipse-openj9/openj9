
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(CLASSITERATORCLASSSLOTS_HPP_)
#define CLASSITERATORCLASSSLOTS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "ConstantPoolClassSlotIterator.hpp"
#include "ClassSuperclassesIterator.hpp"
#include "ClassLocalInterfaceIterator.hpp"
#include "ClassArrayClassSlotIterator.hpp"

/**
 * State constants representing the current stage of the iteration process
 * @anchor ClassIteratorClassSlotsState
 */
enum {
	classiteratorclassslots_state_start = 0,
	classiteratorclassslots_state_constant_pool,
	classiteratorclassslots_state_superclasses,
	classiteratorclassslots_state_interfaces,
	classiteratorclassslots_state_array_class_slots,
	classiteratorclassslots_state_end
};


/** 
 * Iterate through slots in the class which contain a class reference (as compared to
 * GC_ClassIterator, which iterates over object references).
 * 
 * @see GC_ClassIterator
 * @ingroup GC_Structs
 */
class GC_ClassIteratorClassSlots
{
protected:
	J9Class *_clazzPtr;
	int _state;

	GC_ConstantPoolClassSlotIterator _constantPoolClassSlotIterator;
	GC_ClassSuperclassesIterator _classSuperclassesIterator;
	GC_ClassLocalInterfaceIterator _classLocalInterfaceIterator;
	GC_ClassArrayClassSlotIterator _classArrayClassSlotIterator;

public:

	GC_ClassIteratorClassSlots(J9Class *clazz) :
		_clazzPtr(clazz),
		_state(classiteratorclassslots_state_start),
		_constantPoolClassSlotIterator(clazz),
		_classSuperclassesIterator(clazz),
		_classLocalInterfaceIterator(clazz),
		_classArrayClassSlotIterator(clazz)
	{}
	
	/**
	 * @return @ref ClassIteratorClassSlotsState representing the current state (stage
	 * of the iteration process)
	 */
	MMINLINE int getState() 
	{ 
		return _state; 
	};
	
	/**
	 * Gets the current index corresponding to the current state.
	 * @return current index of the current state where appropriate.
	 * @return -1 if the current state is not indexed.
	 */
	MMINLINE IDATA getIndex()
	{
		switch (getState()) {
			case classiteratorclassslots_state_constant_pool:
				return _constantPoolClassSlotIterator.getIndex();

			case classiteratorclassslots_state_superclasses:
				return _classSuperclassesIterator.getIndex();

			case classiteratorclassslots_state_array_class_slots:
				return _classArrayClassSlotIterator.getIndex();
				
			case classiteratorclassslots_state_interfaces:
			default:
				return -1;
		}			
	}
	
	J9Class **nextSlot();

};

#endif /* CLASSITERATORCLASSSLOTS_HPP_ */

