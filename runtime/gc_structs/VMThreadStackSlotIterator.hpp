
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

#if !defined(VMTHREADSTACKSLOTITERATOR_HPP_)
#define VMTHREADSTACKSLOTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"

/**
 * Signature for the callback function passed to the stack walker
 */
typedef void J9MODRON_OSLOTITERATOR(J9JavaVM *javaVM, J9Object **objectIndirect, void *localData, J9StackWalkState *walkState, const void *stackLocation);

/**
 * Iterate over all slots on the stack of a given thread which contain object references.
 * @ingroup GC_Structs
 */
class GC_VMThreadStackSlotIterator
{
public:
	static void scanSlots(
			J9VMThread *vmThread,
			J9VMThread *walkThread,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth);
};

#endif /* VMTHREADSTACKSLOTITERATOR_HPP_ */

