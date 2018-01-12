
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

#if !defined(CONSTANTPOOLOBJECTSLOTITERATOR_HPP_)
#define CONSTANTPOOLOBJECTSLOTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9cp.h"
#include "modron.h"

/**
 * Iterate over object references (but not class references) in the constant pool of a class.
 * 
 * @see GC_ConstantPoolClassSlotIterator
 * @ingroup GC_Structs
 */
class GC_ConstantPoolObjectSlotIterator
{
	j9object_t *_cpEntry;
	U_32 _cpEntryCount;
	U_32 _cpEntryTotal;
	U_32 *_cpDescriptionSlots;
	U_32 _cpDescription;
	UDATA _cpDescriptionIndex;
	
public:
	GC_ConstantPoolObjectSlotIterator(J9Class *clazz) :
		_cpEntry((j9object_t *)J9_CP_FROM_CLASS(clazz)),
		_cpEntryCount(clazz->romClass->ramConstantPoolCount)
	{
		_cpEntryTotal = _cpEntryCount;
		if(_cpEntryCount) {
			_cpDescriptionSlots = SRP_PTR_GET(&clazz->romClass->cpShapeDescription, U_32 *);
			_cpDescriptionIndex = 0;
		}

	};

	/**
	 * Gets the current constant pool index.
	 * @return zero based constant pool index of the entry returned by the last call of nextSlot.
	 * @return -1 if nextSlot has yet to be called.
	 */
	MMINLINE IDATA getIndex() {
		return _cpEntryTotal - _cpEntryCount - 1;
	}

	j9object_t *nextSlot();

	/**
	 * Required in order to resume constant pool scanning state.  The caller doesn't know which class
	 * of a replacedClass list the constant pool slot refers to so this will do a simple range check
	 * to determine if it refers to this class's constant pool.
	 *
	 * @param slot[in] A pointer to a slot in a class's constant pool
	 * @return True if the slot points into the constant pool known to this iterator
	 */
	bool isSlotInConstantPool(j9object_t *slot);

	/**
	 * Sets the next slot to be returned to the given slot.  This is only useful for
	 * resuming a suspended constant pool iteration.
	 *
	 * @param slot[in] The slot pointer which is to be returned by the next call to nextSlot
	 */
	void setNextSlot(j9object_t *slot);
};

#endif /* CONSTANTPOOLOBJECTSLOTITERATOR_HPP_ */
