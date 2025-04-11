
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
#if !defined(MARKINGDELEGATEBASE_HPP_)

#define MARKINGDELEGATEBASE_HPP_

#include "j9.h"
#include "j9nonbuilder.h"
#include "objectdescription.h"

#include "GCExtensions.hpp"
#include "Heap.hpp"

#if JAVA_SPEC_VERSION >= 24
class GC_ContinuationSlotIterator;
#endif /* JAVA_SPEC_VERSION >= 24 */
class MM_EnvironmentBase;

//class MM_MarkingScheme;

class MM_MarkingDelegateBase
{
protected:
	MM_Heap *_heap;

private:
	/**
	 * Determine whether the object pointer is found within the heap proper.
	 * @return Boolean indicating if the object pointer is within the heap boundaries.
	 */
	MMINLINE bool isHeapObject(J9Object *objectPtr)
	{
		return (_heap->getHeapBase() <= (uint8_t *)objectPtr) && (_heap->getHeapTop() > (uint8_t *)objectPtr);
	}

public:
	virtual void doSlot(MM_EnvironmentBase *env, omrobjectptr_t *slotPtr) = 0;
#if JAVA_SPEC_VERSION >= 24
    virtual void doContinuationSlot(MM_EnvironmentBase *env, omrobjectptr_t *slotPtr, GC_ContinuationSlotIterator *continuationSlotIterator);
#endif /* JAVA_SPEC_VERSION >= 24 */
    virtual void doStackSlot(MM_EnvironmentBase *env, omrobjectptr_t *slotPtr, J9StackWalkState *walkState, const void *stackLocation);

    /**
	 * Initialize base class.
	 *
	 * @param env environment for calling thread
	 * @return true if base class initialized successfully
	 */
	bool initialize(MM_EnvironmentBase *env);

	/**
	 * Constructor.
	 */
	MMINLINE MM_MarkingDelegateBase()
		: _heap(NULL)
	{}
};

#endif /* MARKINGDELEGATEBASE_HPP_ */


