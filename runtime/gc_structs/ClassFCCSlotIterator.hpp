
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

#if !defined(CLASSFCCSLOTITERATOR_HPP_)
#define CLASSFCCSLOTITERATOR_HPP_

#include "j9.h"
#include "j9nonbuilder.h"

/**
 * Iterate through slots in the flattened class cache which may contain a
 *  class reference.
 *
 * @see GC_ClassFCCSLotIterator
 * @ingroup GC_Structs
 */
class GC_ClassFCCSlotIterator
{
private:
    J9Class* _clazz;
    UDATA _numberOfFlattenedFields;
    UDATA _index;
public:
	GC_ClassFCCSlotIterator(J9Class *clazz) :
        _clazz(clazz),
        _numberOfFlattenedFields(0),
        _index(0)
	{
        if (J9_ARE_NO_BITS_SET(J9CLASS_FLAGS(clazz), J9AccClassArray) && (NULL != clazz->flattenedClassCache))  {
            _numberOfFlattenedFields = clazz->flattenedClassCache->numberOfEntries;
        }
    };

	J9Class *nextSlot();

	/**
	 * Gets the current slot's index in the FCC.
	 * @return index of current slot index in the flattened class cache
	 */
	MMINLINE IDATA getIndex() {
        return _index;
    }
};
#endif /* CLASSFCCSLOTITERATOR_HPP_ */
