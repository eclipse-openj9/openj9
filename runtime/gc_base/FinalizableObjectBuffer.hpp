
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

#if !defined(FINALIZABLEOBJECTBUFFER_HPP_)
#define FINALIZABLEOBJECTBUFFER_HPP_

#include "j9.h"

#include "FinalizeListManager.hpp"
#include "ObjectAccessBarrier.hpp"

class GC_FinalizableObjectBuffer
{
private:
	j9object_t _systemHead; /**< the head of the linked list of finalizable objects loaded by the system class loader */
	j9object_t _systemTail; /**< the tail of the linked list of unfinalized objects loaded by the system class loader */
	UDATA _systemObjectCount; /**< the number of buffered objects loaded by the system class loader */
	j9object_t _defaultHead; /**< the head of the linked list of unfinalized objects not loaded by the system class loader */
	j9object_t _defaultTail; /**< the tail of the linked list of unfinalized objects not loaded by the system class loader */
	UDATA _defaultObjectCount; /**< the number of buffered objects not loaded by the system class loader */
	MM_GCExtensions * const _extensions; /**< a cached pointer to the extensions structure */
	J9ClassLoader* const _systemClassLoader;
protected:
public:

private:
	void addSystemObject(MM_EnvironmentBase* env, j9object_t object) {
		if (NULL == _systemHead) {
			Assert_MM_true(NULL == _systemTail);
			Assert_MM_true(0 == _systemObjectCount);
			_extensions->accessBarrier->setFinalizeLink(object, NULL);
			_systemHead = object;
			_systemTail = object;
			_systemObjectCount = 1;
		} else {
			Assert_MM_true(NULL != _systemTail);
			Assert_MM_true(0 != _systemObjectCount);
			_extensions->accessBarrier->setFinalizeLink(object, _systemHead);
			_systemHead = object;
			_systemObjectCount += 1;
		}
	}

	void addDefaultObject(MM_EnvironmentBase* env, j9object_t object) {
		if (NULL == _defaultHead) {
			_extensions->accessBarrier->setFinalizeLink(object, NULL);
			_defaultHead = object;
			_defaultTail = object;
			_defaultObjectCount = 1;
		} else {
			_extensions->accessBarrier->setFinalizeLink(object, _defaultHead);
			_defaultHead = object;
			_defaultObjectCount += 1;
		}
	}
protected:
public:
	/**
	 * Add the specified unfinalized object to the buffer.
	 * @param env[in] the current thread
	 * @param object[in] the object to add
	 */
	virtual void add(MM_EnvironmentBase* env, j9object_t object)
	{
		if (_systemClassLoader == (J9OBJECT_CLAZZ((J9VMThread *)env->getOmrVMThread()->_language_vmthread, object)->classLoader)) {
			addSystemObject(env, object);
		} else {
			addDefaultObject(env, object);
		}
	}

	void flush(MM_EnvironmentBase* env)
	{
		GC_FinalizeListManager *finalizeListManager = _extensions->finalizeListManager;
		if (NULL != _systemHead) {
			finalizeListManager->addSystemFinalizableObjects(_systemHead, _systemTail, _systemObjectCount);
			_systemHead = NULL;
			_systemTail = NULL;
			_systemObjectCount = 0;
		}
		if (NULL != _defaultHead) {
			finalizeListManager->addDefaultFinalizableObjects(_defaultHead, _defaultTail, _defaultObjectCount);
			_defaultHead = NULL;
			_defaultTail = NULL;
			_defaultObjectCount = 0;
		}
	}

	/**
	 * Construct a new buffer.
	 * @param extensions[in] the GC extensions
	 */
	GC_FinalizableObjectBuffer(MM_GCExtensions *extensions) :
		_systemHead(NULL)
		,_systemTail(NULL)
		,_systemObjectCount(0)
		,_defaultHead(NULL)
		,_defaultTail(NULL)
		,_defaultObjectCount(0)
		,_extensions(extensions)
		,_systemClassLoader(((J9JavaVM *)extensions->getOmrVM()->_language_vm)->systemClassLoader)
	{}
};
#endif /* FINALIZABLEOBJECTBUFFER_HPP_ */
