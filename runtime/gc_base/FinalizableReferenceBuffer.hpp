
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
 * @ingroup GC_Base
 */

#if !defined(FINALIZABLEREFERENCEBUFFER_HPP_)
#define FINALIZABLEREFERENCEBUFFER_HPP_

#include "j9.h"

#include "ObjectAccessBarrier.hpp"
#include "FinalizeListManager.hpp"

class GC_FinalizableReferenceBuffer
{
private:
	j9object_t _head; /**< the head of the linked list of reference objects */
	j9object_t _tail; /**< the tail of the linked list of reference objects */
	UDATA _count; /**< the number of buffered objects */
	MM_GCExtensions * const _extensions; /**< a cached pointer to the extensions structure */
protected:
public:

private:
protected:
public:
	/**
	 * Add the specified reference object to the buffer.
	 * @param env[in] the current thread
	 * @param object[in] the object to add
	 */
	void add(MM_EnvironmentBase* env, j9object_t object)
	{
		if (NULL == _head) {
			Assert_MM_true(NULL == _tail);
			Assert_MM_true(0 == _count);
			_extensions->accessBarrier->setReferenceLink(object, NULL);
			_head = object;
			_tail = object;
			_count = 1;
		} else {
			Assert_MM_true(NULL != _tail);
			Assert_MM_true(0 != _count);
			_extensions->accessBarrier->setReferenceLink(object, _head);
			_head = object;
			_count += 1;
		}
	}

	void flush(MM_EnvironmentBase* env)
	{
		if (NULL != _head) {
			Assert_MM_true(NULL != _tail);
			Assert_MM_true(0 != _count);
			GC_FinalizeListManager *finalizeListManager = _extensions->finalizeListManager;
			finalizeListManager->addReferenceObjects(_head, _tail, _count);
			_head = NULL;
			_tail = NULL;
			_count = 0;
		}
	}

	/**
	 * Construct a new buffer.
	 * @param extensions[in] the GC extensions
	 */
	GC_FinalizableReferenceBuffer(MM_GCExtensions *extensions) :
		_head(NULL)
		,_tail(NULL)
		,_count(0)
		,_extensions(extensions)
	{}
};
#endif /* FINALIZABLEREFERENCEBUFFER_HPP_ */
