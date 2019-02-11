
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

#if !defined(CLASSARRAYCLASSSLOTITERATOR_HPP_)
#define CLASSARRAYCLASSSLOTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modron.h"

/** 
 * Iterate over all references in a J9ArrayClass.
 * @ingroup GC_Structs
 */
class GC_ClassArrayClassSlotIterator
{
	J9Class *_iterateClazz;
	bool _isArrayClass;
	int _state;
	
	enum {
		classArrayClassSlotIterator_state_arrayClass = 0,
		classArrayClassSlotIterator_state_componentType,
		classArrayClassSlotIterator_state_leafComponentType,
		classArrayClassSlotIterator_state_done
	};

public:
	GC_ClassArrayClassSlotIterator(J9Class *clazz) :
		_iterateClazz(clazz),
		_isArrayClass((clazz->romClass->modifiers & J9AccClassArray) != 0),
		_state(classArrayClassSlotIterator_state_arrayClass)
	{};

	J9Class **nextSlot();
	
	/**
	 * Gets the current slot's "index".
	 * @return 0 when the last call of nextSlot returned the array class.
	 * @return 1 when the last call of nextSlot returned the component type.
	 * @return 2 when the last call of nextSlot returned the leaf component type.
	 * @return -1 if nextSlot has yet to be called.
	 */
	MMINLINE IDATA getIndex() {
		/* just use the state as none of the states return multiple objects */
		return _state - 1;
	}
};

#endif /* CLASSARRAYCLASSSLOTITERATOR_HPP_ */

