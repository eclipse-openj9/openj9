/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"
#include "j9jclnls.h"
#include "objhelp.h"
#include "VMHelpers.hpp"
#include "ObjectAllocationAPI.hpp"
#include "ObjectAccessBarrierAPI.hpp"

extern "C" {

/* java.lang.Throwable: public native Throwable fillInStackTrace(); */
j9object_t JNICALL
Fast_java_lang_Throwable_fillInStackTrace(J9VMThread *currentThread, j9object_t receiver)
{
	J9JavaVM *vm = currentThread->javaVM;
	/* Don't fill in stack traces if -XX:-StackTraceInThrowable is in effect */
	if (0 == (vm->runtimeFlags & J9_RUNTIME_OMIT_STACK_TRACES)) {
		MM_ObjectAllocationAPI objectAllocate(currentThread);
		MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
		/* If the enableWritableStackTrace field is unresolved (i.e. doesn't exist) or is set to true,
		 * continue filling in the stack trace.
		 */
		if (VM_VMHelpers::vmConstantPoolFieldIsResolved(vm, J9VMCONSTANTPOOL_JAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE)
			&& J9VMJAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE(currentThread, receiver)
		) {
			j9object_t walkback = (j9object_t)J9VMJAVALANGTHROWABLE_WALKBACK(currentThread, receiver);
			J9StackWalkState *walkState = currentThread->stackWalkState;
			UDATA walkFlags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC |
				J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_SKIP_INLINES;
			/* Do not hide exception frames if fillInStackTrace is called on an exception which already
			 * has a stack trace.  In the out of memory case, there is a bit indicating that we should
			 * explicitly override this behaviour, since we've precached the stack trace array.
			 */
			if ((NULL == walkback) || (currentThread->privateFlags & J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE)) {
				walkFlags |= J9_STACKWALK_HIDE_EXCEPTION_FRAMES;
				walkState->restartException = receiver;
			}
			walkState->flags = walkFlags;
			walkState->skipCount = 1;	// skip the INL frame
			walkState->walkThread = currentThread;
			UDATA walkRC = vm->walkStackFrames(currentThread, walkState);
			UDATA framesWalked = walkState->framesWalked;
			UDATA *cachePointer = walkState->cache;
			if (J9_STACKWALK_RC_NONE != walkRC) {
				/* Avoid infinite recursion if already throwing OOM */
				if (currentThread->privateFlags & J9_PRIVATE_FLAGS_OUT_OF_MEMORY) {
					goto recursiveOOM;
				}
				setNativeOutOfMemoryError(currentThread, J9NLS_JCL_FAILED_TO_CREATE_STACK_TRACE);
				goto done;
			}
			/* If there is no stack trace in the exception, or we are not in the out of memory case,
			 * allocate a new stack trace.  The cached receiver object is invalid after this point.
			 */
			if ((NULL == walkback) || (0 == (currentThread->privateFlags & J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE))) {
#if defined(J9VM_ENV_DATA64)
				J9Class *arrayClass = vm->longArrayClass;
#else
				J9Class *arrayClass = vm->intArrayClass;
#endif
				walkback = objectAllocate.inlineAllocateIndexableObject(currentThread, arrayClass, (U_32)framesWalked, false);
				if (NULL == walkback) {
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, receiver);
					walkback = vm->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, (U_32)framesWalked, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
					receiver = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					if (J9_UNEXPECTED(NULL == walkback)) {
						setHeapOutOfMemoryError(currentThread);
						goto done;
					}
				}
			} else {
				/* Using existing array - be sure not to overrun it */
				UDATA maxSize = J9INDEXABLEOBJECT_SIZE(currentThread, walkback);
				if (framesWalked > maxSize) {
					framesWalked = maxSize;
				}
			}
			for (UDATA i = 0; i < framesWalked; ++i) {
#if defined(J9VM_ENV_DATA64)
				objectAccessBarrier.inlineIndexableObjectStoreI64(currentThread, walkback, i, cachePointer[i]);
#else
				objectAccessBarrier.inlineIndexableObjectStoreI32(currentThread, walkback, i, cachePointer[i]);
#endif
			}
			freeStackWalkCaches(currentThread, walkState);
recursiveOOM:
			J9VMJAVALANGTHROWABLE_SET_WALKBACK(currentThread, receiver, walkback);
			J9VMJAVALANGTHROWABLE_SET_STACKTRACE(currentThread, receiver, NULL);
		}
	}
done:
	return receiver;
}

J9_FAST_JNI_METHOD_TABLE(java_lang_Throwable)
#if !defined(J9ZOS390) // TODO - retest to see if ZOS compiler bug is gone now
	J9_FAST_JNI_METHOD("fillInStackTrace", "()Ljava/lang/Throwable;", Fast_java_lang_Throwable_fillInStackTrace,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
#endif
J9_FAST_JNI_METHOD_TABLE_END

}
