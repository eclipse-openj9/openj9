/*******************************************************************************
 * Copyright IBM Corp. and others 2022
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(HEAPWALKERDELEGATE_HPP_)
#define HEAPWALKERDELEGATE_HPP_

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

class MM_HeapWalker;
typedef void (*MM_HeapWalkerSlotFunc)(OMR_VM *, omrobjectptr_t *, void *, uint32_t);

/**
 * Provides language-specific support for heap walking.
 */
class MM_HeapWalkerDelegate
{
	/*
	 * Data members
	 */
private:

protected:
	GC_ObjectModel *_objectModel;
	MM_HeapWalker *_heapWalker;

public:

	/*
	 * Function members
	 */
private:
	void doContinuationNativeSlots(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_HeapWalkerSlotFunc function, void *userData);
protected:

public:
	/**
	 * Initialize the delegate.
	 *
	 * @param env environment for calling thread
	 * @param heapWalker the MM_HeapWalker that the delegate is bound to
	 * @return true if delegate initialized successfully
	 */
	MMINLINE bool
	initialize(MM_EnvironmentBase *env, MM_HeapWalker *heapWalker)
	{
		_objectModel = &(env->getExtensions()->objectModel);
		_heapWalker = heapWalker;
		return true;
	}

	/**
	 * This method is used to do language specific treatment for the Object found during heap walking
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param omrVMThread the calling thread
	 * @param objectPtr heap object ptr
	 */
	void objectSlotsDo(OMR_VMThread *omrVMThread, omrobjectptr_t objectPtr, MM_HeapWalkerSlotFunc function, void *userData);

	/**
	 * Constructor.
	 */
	MMINLINE MM_HeapWalkerDelegate()
		: _objectModel(NULL)
		, _heapWalker(NULL)
	{ }
};

typedef struct StackIteratorData4HeapWalker {
	MM_HeapWalker *heapWalker;
	MM_EnvironmentBase *env;
	omrobjectptr_t fromObject;
	MM_HeapWalkerSlotFunc function;
	void *userData;
} StackIteratorData4HeapWalker;

#endif /* HEAPWALKERDELEGATE_HPP_ */
