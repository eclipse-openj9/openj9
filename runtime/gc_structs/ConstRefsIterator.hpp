/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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

#if !defined(CONSTREFSITERATOR_HPP_)
#define CONSTREFSITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)

/**
 * Iterate over all singly-owned constant reference slots in a class.
 * @ingroup GC_Structs
 */
class GC_ConstRefsIterator
{
	J9ConstRefArray * const _sentinel;
	J9ConstRefArray *_node;
	UDATA _index;
	const bool _empty;

public:
	GC_ConstRefsIterator(J9Class *clazz)
		: _sentinel(clazz->constRefArrays)
		, _node(NULL == _sentinel ? NULL : _sentinel->nextInClass)
		, _index(0)
		, _empty(_node == _sentinel)
	{
		if (!_empty) {
			omrthread_jit_write_protect_disable();
		}
	}

	~GC_ConstRefsIterator()
	{
		if (!_empty) {
			omrthread_jit_write_protect_enable();
		}
	}

	/**
	 * @return the next constant reference slot in the class.
	 * @return NULL if there are no more such such slots.
	 */
	volatile j9object_t *
	nextSlot()
	{
		if (_sentinel == _node) {
			return NULL;
		}

		j9object_t *slotPtr = &_node->references[_index++];
		if (_index >= _node->count) {
			_index = 0;
			_node = _node->nextInClass;
		}

		return slotPtr;
	}
};

#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

#endif /* !defined(CONSTREFSITERATOR_HPP_) */
