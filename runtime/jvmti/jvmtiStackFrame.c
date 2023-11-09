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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"

static UDATA popFrameCheckIterator (J9VMThread * currentThread, J9StackWalkState * walkState);
static UDATA jvmtiInternalGetStackTraceIterator (J9VMThread * currentThread, J9StackWalkState * walkState);
static jvmtiError jvmtiInternalGetStackTrace(
	jvmtiEnv *env, J9VMThread *currentThread, J9VMThread *targetThread, j9object_t threadObject,
	jint start_depth, UDATA max_frame_count, jvmtiFrameInfo *frame_buffer, jint *count_ptr);


jvmtiError JNICALL
jvmtiGetStackTrace(jvmtiEnv* env,
	jthread thread,
	jint start_depth,
	jint max_frame_count,
	jvmtiFrameInfo* frame_buffer,
	jint* count_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jint rv_count = 0;

	Trc_JVMTI_jvmtiGetStackTrace_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread *targetThread = NULL;
		J9InternalVMFunctions* vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(max_frame_count);
		ENSURE_NON_NULL(frame_buffer);
		ENSURE_NON_NULL(count_ptr);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (rc == JVMTI_ERROR_NONE) {
			j9object_t threadObject = NULL;
#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vmFuncs->haltThreadForInspection(currentThread, targetThread);
			}
			threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			rc = jvmtiInternalGetStackTrace(env, currentThread, targetThread, threadObject, start_depth, (UDATA) max_frame_count, frame_buffer, &rv_count);
#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vmFuncs->resumeThreadForInspection(currentThread, targetThread);
			}
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	if (NULL != count_ptr) {
		*count_ptr = rv_count;
	}
	TRACE_JVMTI_RETURN(jvmtiGetStackTrace);
}


jvmtiError JNICALL
jvmtiGetAllStackTraces(jvmtiEnv* env,
	jint max_frame_count,
	jvmtiStackInfo** stack_info_ptr,
	jint* thread_count_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	PORT_ACCESS_FROM_JAVAVM(vm);

	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jvmtiStackInfo *rv_stack_info = NULL;
	jint rv_thread_count = 0;

	Trc_JVMTI_jvmtiGetAllStackTraces_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		UDATA threadCount;
		jvmtiStackInfo * stackInfo;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(max_frame_count);
		ENSURE_NON_NULL(stack_info_ptr);
		ENSURE_NON_NULL(thread_count_ptr);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		threadCount = vm->totalThreadCount;
		stackInfo = j9mem_allocate_memory(((sizeof(jvmtiStackInfo) + (max_frame_count * sizeof(jvmtiFrameInfo))) * threadCount) + sizeof(jlocation), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (NULL == stackInfo) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jvmtiFrameInfo *currentFrameInfo = (jvmtiFrameInfo *)((((UDATA)(stackInfo + threadCount)) + sizeof(jlocation)) & ~sizeof(jlocation));
			jvmtiStackInfo *currentStackInfo = stackInfo;
			J9VMThread *targetThread = vm->mainThread;

			do {
				/* If threadObject is NULL, ignore this thread. */
#if JAVA_SPEC_VERSION >= 19
				j9object_t threadObject = targetThread->carrierThreadObject;
#else /* JAVA_SPEC_VERSION >= 19 */
				j9object_t threadObject = targetThread->threadObject;
#endif /* JAVA_SPEC_VERSION >= 19 */
				if (NULL == threadObject) {
					--threadCount;
				} else {
					rc = jvmtiInternalGetStackTrace(
						env,
						currentThread,
						targetThread,
						threadObject,
						0,
						(UDATA)max_frame_count,
						(void *)currentFrameInfo,
						&(currentStackInfo->frame_count));

					if (JVMTI_ERROR_NONE != rc) {
						j9mem_free_memory(stackInfo);
						goto fail;
					}

					currentStackInfo->thread = (jthread)vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)currentThread, threadObject);
					currentStackInfo->state = getThreadState(currentThread, threadObject);
					currentStackInfo->frame_buffer = currentFrameInfo;

					++currentStackInfo;
					currentFrameInfo += max_frame_count;
				}
			} while ((targetThread = targetThread->linkNext) != vm->mainThread);

			rv_stack_info = stackInfo;
			rv_thread_count = (jint)threadCount;
		}
fail:
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != stack_info_ptr) {
		*stack_info_ptr = rv_stack_info;
	}
	if (NULL != thread_count_ptr) {
		*thread_count_ptr = rv_thread_count;
	}
	TRACE_JVMTI_RETURN(jvmtiGetAllStackTraces);
}


jvmtiError JNICALL
jvmtiGetThreadListStackTraces(jvmtiEnv* env,
	jint thread_count,
	const jthread* thread_list,
	jint max_frame_count,
	jvmtiStackInfo** stack_info_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jvmtiStackInfo *rv_stack_info = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JVMTI_jvmtiGetThreadListStackTraces_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jvmtiStackInfo *stackInfo = NULL;
#if JAVA_SPEC_VERSION >= 19
		const jint originalThreadCount = thread_count;
		const jthread *originalThreadList = thread_list;
#endif /* JAVA_SPEC_VERSION >= 19 */

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(thread_count);
		ENSURE_NON_NULL(thread_list);
		ENSURE_NON_NEGATIVE(max_frame_count);
		ENSURE_NON_NULL(stack_info_ptr);

#if JAVA_SPEC_VERSION >= 19
		while (0 != thread_count) {
			jthread thread = *thread_list;

			if ((NULL != thread) && IS_JAVA_LANG_VIRTUALTHREAD(currentThread, J9_JNI_UNWRAP_REFERENCE(thread))) {
				J9VMThread *targetThread = NULL;
				getVMThread(currentThread, thread, &targetThread, JVMTI_ERROR_NONE, 0);
			}
			++thread_list;
			--thread_count;
		}
#endif /* JAVA_SPEC_VERSION >= 19 */

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

#if JAVA_SPEC_VERSION >= 19
		thread_count = originalThreadCount;
		thread_list = originalThreadList;
#endif /* JAVA_SPEC_VERSION >= 19 */

		stackInfo = j9mem_allocate_memory(((sizeof(jvmtiStackInfo) + (max_frame_count * sizeof(jvmtiFrameInfo))) * thread_count) + sizeof(jlocation), J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (stackInfo == NULL) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			jvmtiFrameInfo *currentFrameInfo = (jvmtiFrameInfo *)((((UDATA)(stackInfo + thread_count)) + sizeof(jlocation)) & ~sizeof(jlocation));
			jvmtiStackInfo *currentStackInfo = stackInfo;

			while (0 != thread_count) {
				jthread thread = *thread_list;
				J9VMThread *targetThread = NULL;
				j9object_t threadObject = NULL;
				BOOLEAN isThreadAlive = FALSE;
#if JAVA_SPEC_VERSION >= 19
				BOOLEAN isVirtual = FALSE;
#endif /* JAVA_SPEC_VERSION >= 19 */

				if (NULL == thread) {
					rc = JVMTI_ERROR_NULL_POINTER;
					goto deallocate;
				}

				threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
				if (!IS_JAVA_LANG_THREAD(currentThread, threadObject)) {
					rc = JVMTI_ERROR_INVALID_THREAD;
					goto deallocate;
				}

#if JAVA_SPEC_VERSION >= 19
				if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
					isVirtual = TRUE;
					j9object_t carrierThread = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObject);
					jint vthreadState = J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObject);
					if (NULL != carrierThread) {
						targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, carrierThread);
					}
					isThreadAlive = (JVMTI_VTHREAD_STATE_NEW != vthreadState) && (JVMTI_VTHREAD_STATE_TERMINATED != vthreadState);
				} else
#endif /* JAVA_SPEC_VERSION >= 19 */
				{
					targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject);
					isThreadAlive = (NULL != targetThread);
				}

				if (!isThreadAlive) {
					currentStackInfo->frame_count = 0;
				} else {
					rc = jvmtiInternalGetStackTrace(
						env,
						currentThread,
						targetThread,
						threadObject,
						0,
						(UDATA) max_frame_count,
						(void *) currentFrameInfo,
						&(currentStackInfo->frame_count));
					if (rc != JVMTI_ERROR_NONE) {
deallocate:
						j9mem_free_memory(stackInfo);
						goto fail;
					}
				}
				currentStackInfo->thread = thread;
#if JAVA_SPEC_VERSION >= 19
				if (isVirtual) {
					currentStackInfo->state = getVirtualThreadState(currentThread, thread);
				} else
#endif /* JAVA_SPEC_VERSION >= 19 */
				{
					currentStackInfo->state = getThreadState(currentThread, threadObject);
				}
				currentStackInfo->frame_buffer = currentFrameInfo;

				++thread_list;
				--thread_count;
				++currentStackInfo;
				currentFrameInfo += max_frame_count;
			}

			rv_stack_info = stackInfo;
		}
fail:
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

#if JAVA_SPEC_VERSION >= 19
		thread_count = originalThreadCount;
		thread_list = originalThreadList;
		while (0 != thread_count) {
			jthread thread = *thread_list;

			if (NULL != thread) {
				j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
				if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
					J9VMThread *targetThread = NULL;
					j9object_t carrierThread = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObject);
					if (NULL != carrierThread) {
						targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, carrierThread);
					}
					releaseVMThread(currentThread, targetThread, thread);
				}
			}
			++thread_list;
			--thread_count;
		}
#endif /* JAVA_SPEC_VERSION >= 19 */

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != stack_info_ptr) {
		*stack_info_ptr = rv_stack_info;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadListStackTraces);
}


jvmtiError JNICALL
jvmtiGetFrameCount(jvmtiEnv* env,
	jthread thread,
	jint* count_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jint rv_count = 0;

	Trc_JVMTI_jvmtiGetFrameCount_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread *targetThread = NULL;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NULL(count_ptr);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (rc == JVMTI_ERROR_NONE) {
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			J9StackWalkState walkState;
			walkState.flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY;
			walkState.skipCount = 0;
			/* The number of frames skipped is stored in userData1. */
			walkState.userData1 = (void *)0;
#if JAVA_SPEC_VERSION >= 20
			walkState.flags |= J9_STACKWALK_ITERATE_FRAMES;
			walkState.frameWalkFunction = genericFrameIterator;
#endif /* JAVA_SPEC_VERSION >= 20 */

#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vmFuncs->haltThreadForInspection(currentThread, targetThread);
			}
			genericWalkStackFramesHelper(currentThread, targetThread, threadObject, &walkState);
#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vmFuncs->resumeThreadForInspection(currentThread, targetThread);
			}

			rv_count = (jint)(walkState.framesWalked - (UDATA)walkState.userData1);

			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	if (NULL != count_ptr) {
		*count_ptr = rv_count;
	}
	TRACE_JVMTI_RETURN(jvmtiGetFrameCount);
}

/**
 * Pops the top frame off the stack, leaving execution state immediately before
 * the invoke.  Resuming the thread will result in the method being reinvoked.
 * At this point we need only ensure that the frame being popped is debuggable.
 * 
 * At this stage we simply set a halt flag in the thread, which will be processed
 * when the thread resumes running.  The remaining logic is handled in 
 * jvmtiHookPopFramesInterrupt() where the actual stack manipulation occurs.
 */
jvmtiError JNICALL
jvmtiPopFrame(jvmtiEnv* env,
	jthread thread)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiPopFrame_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_pop_frame);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD | J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (rc == JVMTI_ERROR_NONE) {
#if JAVA_SPEC_VERSION >= 21
			j9object_t threadObject = NULL;
			/* Error if a virtual thread is unmounted since it won't be able to
			 * pop the current frame.
			 */
			if (NULL == targetThread) {
				rc = JVMTI_ERROR_OPAQUE_FRAME;
				goto release;
			}
			threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
#endif /* JAVA_SPEC_VERSION >= 21 */

			/* Error if the thread is not suspended and not the current thread. */
			if ((currentThread != targetThread)
#if JAVA_SPEC_VERSION >= 21
			&& (0 == J9OBJECT_U32_LOAD(currentThread, threadObject, vm->isSuspendedInternalOffset))
#else /* JAVA_SPEC_VERSION >= 21 */
			&& OMR_ARE_NO_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)
#endif /* JAVA_SPEC_VERSION >= 21 */
			)  {
				rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
			} else {
				J9StackWalkState walkState = {0};
				J9VMThread *threadToWalk = targetThread;
#if JAVA_SPEC_VERSION >= 21
				J9VMThread stackThread = {0};
				J9VMEntryLocalStorage els = {0};
				J9VMContinuation *continuation = getJ9VMContinuationToWalk(currentThread, targetThread, threadObject);
				if (NULL != continuation) {
					vm->internalVMFunctions->copyFieldsFromContinuation(currentThread, &stackThread, &els, continuation);
					threadToWalk = &stackThread;
				}
#endif /* JAVA_SPEC_VERSION >= 21 */
				walkState.walkThread = threadToWalk;
				walkState.userData1 = (void *) JVMTI_ERROR_NO_MORE_FRAMES;
				walkState.userData2 = (void *) 0;
				walkState.frameWalkFunction = popFrameCheckIterator;
				walkState.skipCount = 0;
				walkState.flags = J9_STACKWALK_INCLUDE_CALL_IN_FRAMES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
				vm->walkStackFrames(currentThread, &walkState);
				rc = (UDATA)walkState.userData1;
				if (JVMTI_ERROR_NONE == rc) {
					vm->internalVMFunctions->setHaltFlag(targetThread, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT);
				}
			}
#if JAVA_SPEC_VERSION >= 21
release:
#endif /* JAVA_SPEC_VERSION >= 21 */
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiPopFrame);
}


jvmtiError JNICALL
jvmtiGetFrameLocation(jvmtiEnv *env,
	jthread thread,
	jint depth,
	jmethodID *method_ptr,
	jlocation *location_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jmethodID rv_method = NULL;
	jlocation rv_location = 0;

	Trc_JVMTI_jvmtiGetFrameLocation_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread *targetThread = NULL;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		ENSURE_NON_NEGATIVE(depth);
		ENSURE_NON_NULL(method_ptr);
		ENSURE_NON_NULL(location_ptr);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (rc == JVMTI_ERROR_NONE) {
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			J9StackWalkState walkState = {0};
			walkState.flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
			walkState.skipCount = (UDATA) depth;
			walkState.maxFrames = 1;

#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vmFuncs->haltThreadForInspection(currentThread, targetThread);
			}
			genericWalkStackFramesHelper(currentThread, targetThread, threadObject, &walkState);
#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vmFuncs->resumeThreadForInspection(currentThread, targetThread);
			}

			if (1 == walkState.framesWalked) {
				jmethodID methodID = getCurrentMethodID(currentThread, walkState.method);

				if (NULL == methodID) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
				} else {
					rv_method = methodID;
					/* The location = -1 for native method case is handled in the stack walker */
					rv_location = (jlocation) walkState.bytecodePCOffset;
				}
			} else {
				rc = JVMTI_ERROR_NO_MORE_FRAMES;
			}

			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	if (NULL != method_ptr) {
		*method_ptr = rv_method;
	}
	if (NULL != location_ptr) {
		*location_ptr = rv_location;
	}
	TRACE_JVMTI_RETURN(jvmtiGetFrameLocation);
}


jvmtiError JNICALL
jvmtiNotifyFramePop(jvmtiEnv *env,
	jthread thread,
	jint depth)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;

	Trc_JVMTI_jvmtiNotifyFramePop_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9VMThread *targetThread = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_generate_frame_pop_events);

		ENSURE_NON_NEGATIVE(depth);

		rc = getVMThread(
				currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (JVMTI_ERROR_NONE == rc) {
#if JAVA_SPEC_VERSION >= 19
			j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
			}

			/* Error if the thread is not suspended and not the current thread. */
			if ((currentThread != targetThread)
#if JAVA_SPEC_VERSION >= 19
			&& (0 == J9OBJECT_U32_LOAD(currentThread, threadObject, vm->isSuspendedInternalOffset))
#else /* JAVA_SPEC_VERSION >= 19 */
			&& OMR_ARE_NO_BITS_SET(targetThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)
#endif /* JAVA_SPEC_VERSION >= 19 */
			)  {
				rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
			} else {
				J9StackWalkState walkState = {0};
				J9VMThread *threadToWalk = targetThread;
#if JAVA_SPEC_VERSION >= 19
				J9VMThread stackThread = {0};
				J9VMEntryLocalStorage els = {0};
				j9object_t threadObject = (NULL == thread) ? currentThread->threadObject : J9_JNI_UNWRAP_REFERENCE(thread);
				J9VMContinuation *continuation = getJ9VMContinuationToWalk(currentThread, targetThread, threadObject);
				if (NULL != continuation) {
					vm->internalVMFunctions->copyFieldsFromContinuation(currentThread, &stackThread, &els, continuation);
					threadToWalk = &stackThread;
				}
#endif /* JAVA_SPEC_VERSION >= 19 */

				rc = findDecompileInfo(currentThread, threadToWalk, (UDATA)depth, &walkState);
				if (JVMTI_ERROR_NONE == rc) {
					J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState.method);

					if (OMR_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccNative)) {
						rc = JVMTI_ERROR_OPAQUE_FRAME;
					} else {
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
						if (NULL != walkState.jitInfo) {
							UDATA inlineDepth = (UDATA)walkState.userData2;
							vm->jitConfig->jitFramePopNotificationAdded(currentThread, &walkState, inlineDepth);
						} else
#endif /* J9VM_JIT_FULL_SPEED_DEBUG */
						{
							*walkState.bp |= J9SF_A0_REPORT_FRAME_POP_TAG;
						}
					}
				}
			}

#if JAVA_SPEC_VERSION >= 19
			if (NULL != targetThread)
#endif /* JAVA_SPEC_VERSION >= 19 */
			{
				vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			}
			releaseVMThread(currentThread, targetThread, thread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiNotifyFramePop);
}


static UDATA
jvmtiInternalGetStackTraceIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	jmethodID methodID = NULL;
	UDATA rc = J9_STACKWALK_KEEP_ITERATING;
	J9Method *method = walkState->method;

#if JAVA_SPEC_VERSION >= 20
	J9ROMMethod *romMethod = NULL;
	U_32 extendedModifiers = 0;

	/* walkState->method can never be NULL since the J9_STACKWALK_VISIBLE_ONLY flag is set. */
	Assert_JVMTI_true(NULL != method);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	extendedModifiers = getExtendedModifiersDataFromROMMethod(romMethod);

	if (J9_ARE_ANY_BITS_SET(extendedModifiers, CFR_METHOD_EXT_JVMTIMOUNTTRANSITION_ANNOTATION)) {
		/* The number of frames skipped is stored in userData2. */
		UDATA framesSkipped = (UDATA)walkState->userData2;
		walkState->userData2 = (void *)(framesSkipped + 1);
		goto skip;
	}
#endif /* JAVA_SPEC_VERSION >= 20 */

	methodID = getCurrentMethodID(currentThread, method);
	if (NULL == methodID) {
		walkState->userData1 = NULL;
		rc = J9_STACKWALK_STOP_ITERATING;
	} else {
		jvmtiFrameInfo *frame_buffer = walkState->userData1;

		frame_buffer->method = methodID;
		/* The location = -1 for native method case is handled in the stack walker. */
		frame_buffer->location = (jlocation)walkState->bytecodePCOffset;

		/* If the location specifies a JBinvokeinterface, back it up to the JBinvokeinterface2. */

		if (!IS_SPECIAL_FRAME_PC(walkState->pc)) {
			if (JBinvokeinterface == *(walkState->pc)) {
				frame_buffer->location -= 2;
			}
		}

		walkState->userData1 = frame_buffer + 1;
	}

#if JAVA_SPEC_VERSION >= 20
skip:
#endif /* JAVA_SPEC_VERSION >= 20 */
	return rc;
}


static UDATA
popFrameCheckIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM* vm = currentThread->javaVM;
	UDATA framesVisited = (UDATA)walkState->userData2;
	J9Method *method = walkState->method;
	J9ROMMethod* romMethod = NULL;

	/* If there's no method, this must be a call-in frame - fail immediately */
	if (NULL == method) {
		walkState->userData1 = (void *) JVMTI_ERROR_OPAQUE_FRAME;
		return J9_STACKWALK_STOP_ITERATING;
	}

	/* CMVC 113680 - The top most method could be a Thread.yield() implemented
	 * as a native, make sure we return JVMTI_ERROR_OPAQUE_FRAME instead of falling
	 * off the edge with JVMTI_ERROR_NO_MORE_FRAMES.
	 */
	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	if (romMethod->modifiers & J9AccNative) {
		walkState->userData1 = (void *) JVMTI_ERROR_OPAQUE_FRAME;
		return J9_STACKWALK_STOP_ITERATING;
	}

	/* If the frame being dropped is <clinit>, disallow it */
	if (1 == walkState->framesWalked) {
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
		if ((romMethod->modifiers & J9AccStatic) && (J9UTF8_LENGTH(methodName) > 0) && ('<' == J9UTF8_DATA(methodName)[0])) {
			walkState->userData1 = (void *) JVMTI_ERROR_OPAQUE_FRAME;
			return J9_STACKWALK_STOP_ITERATING;
		}
	}

	if (NULL == walkState->jitInfo) {
		/* Interpreted frame, no need for decompilation */
		framesVisited += 1;
	} else if (0 == walkState->inlineDepth) {
		/* Outer JIT frame */
		framesVisited += 1;
		if (NULL == vm->jitConfig->jitAddDecompilationForFramePop(currentThread, walkState)) {
			walkState->userData1 = (void *) JVMTI_ERROR_OUT_OF_MEMORY;
			return J9_STACKWALK_STOP_ITERATING;
		}
	}

	/* Once both frames have been located, stop with success */
	walkState->userData2 = (void*)framesVisited;
	if (2 == framesVisited) {
		walkState->userData1 = (void *) JVMTI_ERROR_NONE;
		return J9_STACKWALK_STOP_ITERATING;
	}

	return J9_STACKWALK_KEEP_ITERATING;
}


static jvmtiError
jvmtiInternalGetStackTrace(
	jvmtiEnv *env,
	J9VMThread *currentThread,
	J9VMThread *targetThread,
	j9object_t threadObject,
	jint start_depth,
	UDATA max_frame_count,
	jvmtiFrameInfo *frame_buffer,
	jint *count_ptr)
{
	J9StackWalkState walkState = {0};
	UDATA framesWalked = 0;
	walkState.flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY;
	walkState.skipCount = 0;
	/* The number of frames skipped is stored in userData1. */
	walkState.userData1 = (void *)0;
#if JAVA_SPEC_VERSION >= 20
	walkState.flags |= J9_STACKWALK_ITERATE_FRAMES;
	walkState.frameWalkFunction = genericFrameIterator;
#endif /* JAVA_SPEC_VERSION >= 20 */
	genericWalkStackFramesHelper(currentThread, targetThread, threadObject, &walkState);
	framesWalked = walkState.framesWalked - (UDATA)walkState.userData1;
	if (start_depth == 0) {
		/* This violates the spec, but matches JDK behaviour - allows querying an empty stack with start_depth == 0 */
		walkState.skipCount = 0;
	} else if (start_depth > 0) {
		if (((UDATA)start_depth) >= framesWalked) {
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
		}
		walkState.skipCount = (UDATA) start_depth;
	} else {
		if (((UDATA)-start_depth) > framesWalked) {
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
		}
		walkState.skipCount = framesWalked + start_depth;
	}
	walkState.maxFrames = max_frame_count;
	walkState.flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY
		| J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET | J9_STACKWALK_COUNT_SPECIFIED
		| J9_STACKWALK_ITERATE_FRAMES;
	walkState.userData1 = frame_buffer;
	/* The number of frames skipped is stored in userData2. */
	walkState.userData2 = (void *)0;
	walkState.frameWalkFunction = jvmtiInternalGetStackTraceIterator;

	genericWalkStackFramesHelper(currentThread, targetThread, threadObject, &walkState);
	framesWalked = walkState.framesWalked - (UDATA)walkState.userData2;

	if (NULL == walkState.userData1) {
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	*count_ptr = (jint)framesWalked;
	return JVMTI_ERROR_NONE;
}
