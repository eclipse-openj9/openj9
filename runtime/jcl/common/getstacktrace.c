/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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

#include <string.h>

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "j9protos.h"
#include "jcl_internal.h"
#include "objhelp.h"
#include "ut_j9jcl.h"
#include "vmaccess.h"

/**
 * Creates a throwable object containing the stacktrace of threadObject.
 * @param[in] currentThread
 * @param[in] targetThread the J9VMThread that threadObject is running on.
 * @param[in] skipCount number of frames to skip during stackwalk (ignored if walking continuation stack).
 * @param[in] threadObject the target thread object to retrieve stacktrace on.
 * @return throwable object
 */
j9object_t
getStackTraceForThread(J9VMThread *currentThread, J9VMThread *targetThread, UDATA skipCount, j9object_t threadObject)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions * vmfns = vm->internalVMFunctions;
	j9object_t throwable = NULL;
	J9StackWalkState walkState = {0};
	UDATA rc = J9_STACKWALK_RC_NONE;

#if JAVA_SPEC_VERSION >= 19
	BOOLEAN isVirtual = IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject);
	if (isVirtual) {
		/* Return NULL if a valid CarrierThread object cannot be found through VirtualThread object,
		 * the caller of getStackTraceImpl will handle whether to retry or get the stack using the unmounted path.
		 */
		j9object_t carrierThread = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObject);
		/* Ensure the VirtualThread is mounted and not during transition. */
		if (NULL == carrierThread) {
			goto done;
		}
		/* Gets targetThread from the carrierThread object. */
		targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, carrierThread);
		Assert_JCL_notNull(targetThread);
	}
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, threadObject);
#endif /* JAVA_SPEC_VERSION >= 19 */
	/* Halt the target thread. */
	vmfns->haltThreadForInspection(currentThread, targetThread);

	/* walk stack and cache PCs. */
	walkState.flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY;
#if JAVA_SPEC_VERSION >= 19
	threadObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	/* Re-check thread state. */
	if ((NULL != targetThread->currentContinuation) && (threadObject == targetThread->carrierThreadObject)) {
		/* If targetThread has a continuation mounted and its threadObject matches its carrierThreadObject,
		 * then the carrier thread's stacktrace is retrieved through the cached state in the continuation.
		 */
		walkState.skipCount = 0;
		rc = vmfns->walkContinuationStackFrames(currentThread, targetThread->currentContinuation, threadObject, &walkState);
	} else if (isVirtual && (threadObject != targetThread->threadObject)) {
		/* If the virtual thread object doesn't match the current thread object, it must have unmounted
		 * from this carrier thread, return NULL and the JCL code will handle the retry.
		 */
		vmfns->resumeThreadForInspection(currentThread, targetThread);
		goto done;
	} else
#endif /* JAVA_SPEC_VERSION >= 19 */
	{
		walkState.walkThread = targetThread;
		walkState.skipCount = skipCount;
		rc = vm->walkStackFrames(currentThread, &walkState);
	}
	/* Now that the stack trace has been copied, resume the thread. */
	vmfns->resumeThreadForInspection(currentThread, targetThread);

	/* Check for stack walk failure. */
	if (rc != J9_STACKWALK_RC_NONE) {
		vmfns->setNativeOutOfMemoryError(currentThread, 0, 0);
		goto fail;
	}

	throwable = createStackTraceThrowable(currentThread, walkState.cache, walkState.framesWalked);

fail:
	vmfns->freeStackWalkCaches(currentThread, &walkState);
#if JAVA_SPEC_VERSION >= 19
done:
#endif /* JAVA_SPEC_VERSION >= 19 */
	/* Return the result - any pending exception will be checked by the caller and the result discarded */
	return throwable;
}

j9object_t
createStackTraceThrowable(J9VMThread *currentThread, const UDATA *frames, UDATA maxFrames)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmfns = vm->internalVMFunctions;
	J9MemoryManagerFunctions *mmfns = vm->memoryManagerFunctions;
	J9Class *throwableClass = NULL;
	J9Class *arrayClass = NULL;
	j9object_t throwable = NULL;
	j9array_t walkback = NULL;
	UDATA i;

	Assert_JCL_notNull(currentThread);
	Assert_JCL_mustHaveVMAccess(currentThread);
	if (maxFrames > 0) {
		Assert_JCL_notNull(frames);
	}

	/* Create the result array */
	/* We may allocate an array of zero elements */

#ifdef J9VM_ENV_DATA64
	arrayClass = vm->longArrayClass;
#else
	arrayClass = vm->intArrayClass;
#endif
	walkback = (j9array_t)mmfns->J9AllocateIndexableObject(
		currentThread, arrayClass, (U_32)maxFrames, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (walkback == NULL) {
		goto fail_outOfMemory;
	}
		
	for (i = 0; i < maxFrames; ++i) {
		J9JAVAARRAYOFUDATA_STORE(currentThread, walkback, i, frames[i]);
	}

	/* Create the Throwable and store the walkback in it */

	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t)walkback);
	throwableClass = vmfns->internalFindKnownClass(currentThread, J9VMCONSTANTPOOL_JAVALANGTHROWABLE, J9_FINDKNOWNCLASS_FLAG_INITIALIZE);
	if (throwableClass == NULL) {
		/* Exception already set */
		DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
		return NULL;
	}
	throwable = mmfns->J9AllocateObject(
		currentThread, throwableClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (throwable == NULL) {
		DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
		goto fail_outOfMemory;
	}
	walkback = (j9array_t)POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	J9VMJAVALANGTHROWABLE_SET_WALKBACK(currentThread, throwable, walkback);

	/* Return the result - any pending exception will be checked by the caller and the result discarded */
	return throwable;

fail_outOfMemory:
	vmfns->setHeapOutOfMemoryError(currentThread);
	return NULL;
}

