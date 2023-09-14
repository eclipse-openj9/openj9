
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
private:
	static void initializeStackWalkState(
			J9StackWalkState *stackWalkState,
			J9VMThread *vmThread,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth);
public:
	static void scanSlots(
			J9VMThread *vmThread,
			J9VMThread *walkThread,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth);

	static void scanContinuationSlots(
			J9VMThread *vmThread,
			j9object_t continuationObjectPtr,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth);

#if JAVA_SPEC_VERSION >= 19
	static void scanSlots(
			J9VMThread *vmThread,
			J9VMThread *walkThread,
			J9VMContinuation *continuation,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth);
#endif /* JAVA_SPEC_VERSION >= 19 */
};

#endif /* VMTHREADSTACKSLOTITERATOR_HPP_ */

