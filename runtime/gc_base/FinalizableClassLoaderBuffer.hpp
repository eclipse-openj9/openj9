
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

#if !defined(FINALIZABLECLASSLOADERBUFFER_HPP_)
#define FINALIZABLECLASSLOADERBUFFER_HPP_

#include "j9.h"

#include "FinalizeListManager.hpp"

class GC_FinalizableClassLoaderBuffer
{
private:
	J9ClassLoader *_head; /**< the head of the linked list of J9ClassLoader */
	J9ClassLoader *_tail; /**< the tail of the linked list of J9ClassLoader */
	UDATA _count; /**< the number of buffered J9ClassLoader */
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
	void add(MM_EnvironmentBase* env, J9ClassLoader *loader)
	{
		if (NULL == _head) {
			Assert_MM_true(NULL == _tail);
			Assert_MM_true(0 == _count);
			loader->unloadLink = NULL;
			_head = loader;
			_tail = loader;
			_count = 1;
		} else {
			Assert_MM_true(NULL != _tail);
			Assert_MM_true(0 != _count);
			loader->unloadLink = _head;
			_head = loader;
			_count += 1;
		}
	}

	void flush(MM_EnvironmentBase* env)
	{
		if (NULL != _head) {
			Assert_MM_true(NULL != _tail);
			Assert_MM_true(0 != _count);
			GC_FinalizeListManager *finalizeListManager = _extensions->finalizeListManager;
			finalizeListManager->addClassLoaders(_head, _tail, _count);
			_head = NULL;
			_tail = NULL;
			_count = 0;
		}
	}

	/**
	 * Construct a new buffer.
	 * @param extensions[in] the GC extensions
	 */
	GC_FinalizableClassLoaderBuffer(MM_GCExtensions *extensions) :
		_head(NULL)
		,_tail(NULL)
		,_count(0)
		,_extensions(extensions)
	{}
};
#endif /* FINALIZABLECLASSLOADERBUFFER_HPP_ */
