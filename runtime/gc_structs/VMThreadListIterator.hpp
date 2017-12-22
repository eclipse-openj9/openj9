
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

#if !defined(VMTHREADLISTITERATOR_HPP_)
#define VMTHREADLISTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

/**
 * Iterate over a list of VM threads.
 * @ingroup GC_Structs
 */
class GC_VMThreadListIterator
{
	J9VMThread *_initialVMThread;
	J9VMThread *_vmThread;

public:
	/**
	 * Create an iterator which will start with the main thread in the given javaVM
	 */
	GC_VMThreadListIterator(J9JavaVM *javaVM) :
		_initialVMThread(javaVM->mainThread),
		_vmThread(javaVM->mainThread)
	{}

	/**
	 * Create an iterator which will start with the given thread
	 */
	GC_VMThreadListIterator(J9VMThread *vmThread) :
		_initialVMThread(vmThread),
		_vmThread(vmThread)
	{}

	/**
	 * Restart the iterator back to the initial thread.
	 */
	MMINLINE void reset() {
		_vmThread = _initialVMThread;
	}

	/**
	 * Restart the iterator back to a specific initial thread.
	 */
	MMINLINE void reset(J9VMThread *resetThread) {
		_vmThread = _initialVMThread = resetThread;
	}

	J9VMThread *nextVMThread();
};

#endif /* VMTHREADLISTITERATOR_HPP_ */

