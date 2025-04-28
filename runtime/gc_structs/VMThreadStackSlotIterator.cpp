
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

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9protos.h"

#include "VMThreadStackSlotIterator.hpp"
#include "VMHelpers.hpp"

#if JAVA_SPEC_VERSION >= 19
#include "ContinuationHelpers.hpp"
#endif /* JAVA_SPEC_VERSION >= 19 */

extern "C" {
	
/**
 * The object callback function that is passed to the stack walker.
 * Simply massages the arguments and calls the function that was passed by the user into
 * GC_VMThreadStackSlotIterator::scanSlots()
 */
void
gc_vmThreadStackDoOSlotIterator(J9VMThread *vmThread, J9StackWalkState *walkState, j9object_t *oSlotPointer, const void * stackLocation)
{
	J9MODRON_OSLOTITERATOR *oSlotIterator = (J9MODRON_OSLOTITERATOR*)walkState->userData1;

	(*oSlotIterator)(
		(J9JavaVM *)walkState->userData2,
		(J9Object **)oSlotPointer,
		(void *)walkState->userData3,
		walkState,
		stackLocation);
}

static UDATA
vmThreadStackFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	currentThread->javaVM->collectJitPrivateThreadData(currentThread, walkState);
	return J9_STACKWALK_KEEP_ITERATING;
}

} /* extern "C" */

void
GC_VMThreadStackSlotIterator::initializeStackWalkState(
		J9StackWalkState *stackWalkState,
		J9VMThread *vmThread,
		void *userData,
		J9MODRON_OSLOTITERATOR *oSlotIterator,
		bool includeStackFrameClassReferences,
		bool trackVisibleFrameDepth
		)
{
	J9JavaVM *vm = vmThread->javaVM;

	stackWalkState->objectSlotWalkFunction = gc_vmThreadStackDoOSlotIterator;
	stackWalkState->userData1 = (void *)oSlotIterator;
	stackWalkState->userData2 = (void *)vm;
	stackWalkState->userData3 = userData;

	stackWalkState->flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK;
	stackWalkState->walkThread = NULL;

	if (trackVisibleFrameDepth) {
		stackWalkState->skipCount = 0;
		stackWalkState->flags |= J9_STACKWALK_VISIBLE_ONLY;
	} else {
		if (NULL != vm->collectJitPrivateThreadData) {
			stackWalkState->frameWalkFunction = vmThreadStackFrameIterator;
			stackWalkState->flags |= J9_STACKWALK_ITERATE_FRAMES;
		}
		stackWalkState->flags |= J9_STACKWALK_SKIP_INLINES;
	}

	if (includeStackFrameClassReferences) {
		stackWalkState->flags |= J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS;
	}

}
/**
 * Walk all slots of the walk thread which contain object references.
 * For every object slot found in <code>walkThread</code> call the <code>oSlotIterator</code> function.
 * If <code>includeStackFrameClassReferences</code> is true, calls the <code>oSlotIterator</code> function
 * for every running method's class. 
 * The contents of the slot can be changed by the iterator function.
 * 
 * @param vmThread a thread (when out of process, must be in local memory)
 * @param walkThread the thread whose stack is to be walked (when out of process, can be in remote memory)
 * @param userData will be passed as an argument to the callback function
 * @param oSlotIterator the callback function to be called with each slots
 * @param includeStackFrameClassReferences specifies whether the running methods classes should be included
 */
void
GC_VMThreadStackSlotIterator::scanSlots(
			J9VMThread *vmThread,
			J9VMThread *walkThread,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth
		)
{
	J9StackWalkState stackWalkState;
	initializeStackWalkState(&stackWalkState, vmThread, userData, oSlotIterator, includeStackFrameClassReferences, trackVisibleFrameDepth);
	stackWalkState.walkThread = walkThread;

	vmThread->javaVM->walkStackFrames(vmThread, &stackWalkState);
}

void
GC_VMThreadStackSlotIterator::scanContinuationSlots(
			J9VMThread *vmThread,
			j9object_t continuationObjectPtr,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth
		)
{
	J9StackWalkState stackWalkState;
	initializeStackWalkState(&stackWalkState, vmThread, userData, oSlotIterator, includeStackFrameClassReferences, trackVisibleFrameDepth);

#if JAVA_SPEC_VERSION >= 19
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(vmThread, continuationObjectPtr);
	/* pass NULL as threadObject to avoid to retrieve threadObject via const pool api, since we don't need it for this case */
	vmThread->javaVM->internalVMFunctions->walkContinuationStackFrames(vmThread, continuation, NULL, &stackWalkState);
#endif /* JAVA_SPEC_VERSION >= 19 */
}

#if JAVA_SPEC_VERSION >= 19
void
GC_VMThreadStackSlotIterator::scanSlots(
			J9VMThread *vmThread,
			J9VMThread *walkThread,
			J9VMContinuation *continuation,
			void *userData,
			J9MODRON_OSLOTITERATOR *oSlotIterator,
			bool includeStackFrameClassReferences,
			bool trackVisibleFrameDepth
		)
{
	J9StackWalkState stackWalkState;
	initializeStackWalkState(&stackWalkState, vmThread, userData, oSlotIterator, includeStackFrameClassReferences, trackVisibleFrameDepth);

	j9object_t threadObject = NULL;
	if (NULL != walkThread) {
		threadObject = walkThread->carrierThreadObject;
	}
	vmThread->javaVM->internalVMFunctions->walkContinuationStackFrames(vmThread, continuation, threadObject, &stackWalkState);
}
#endif /* JAVA_SPEC_VERSION >= 19 */
