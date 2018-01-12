/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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



j9object_t
getStackTraceForThread(J9VMThread *currentThread, J9VMThread *targetThread, UDATA skipCount)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions * vmfns = vm->internalVMFunctions;
	j9object_t throwable = NULL;
	J9StackWalkState walkState;
	UDATA rc;

	/* Halt the target thread */
	vmfns->haltThreadForInspection(currentThread, targetThread);

	/* walk stack and cache PCs */
	walkState.walkThread = targetThread;
	walkState.flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY;
	walkState.skipCount = skipCount;
	rc = vm->walkStackFrames(currentThread, &walkState);

	/* Now that the stack trace has been copied, resume the thread */
	vmfns->resumeThreadForInspection(currentThread, targetThread);

	/* Check for stack walk failure */
	if (rc != J9_STACKWALK_RC_NONE) {
		vmfns->setNativeOutOfMemoryError(currentThread, 0, 0);
		goto fail;
	}

	throwable = createStackTraceThrowable(currentThread, walkState.cache, walkState.framesWalked);

fail:
	vmfns->freeStackWalkCaches(currentThread, &walkState);

	/* Return the result - any pending exception will be checked by the caller and the result discarded */
	return throwable;
}

j9object_t
createStackTraceThrowable(J9VMThread *currentThread,  const UDATA *frames, UDATA maxFrames)
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

