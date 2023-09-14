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
#include "j9cp.h"

extern jvmtiNativeInterface jvmtiFunctionTable;

/* Jazz 99339: Map JVMTI event number to the reason code for zAAP switching on zOS.
 * Note: refer to jvmtiEventCallbacks (/runtime/include/jvmti.h) for reserved JVMTI events.
 */
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
static const UDATA reasonCodeFromJVMTIEvent[] = {
	J9_JNI_OFFLOAD_SWITCH_JVMTI_VM_INIT,								/* JVMTI_EVENT_VM_INIT */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_VM_DEATH,								/* JVMTI_EVENT_VM_DEATH */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_THREAD_START,							/* JVMTI_EVENT_THREAD_START */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_THREAD_END,								/* JVMTI_EVENT_THREAD_END */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_CLASS_FILE_LOAD_HOOK,					/* JVMTI_EVENT_CLASS_FILE_LOAD_HOOK */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_CLASS_LOAD,								/* JVMTI_EVENT_CLASS_LOAD */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_CLASS_PREPARE,							/* JVMTI_EVENT_CLASS_PREPARE */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_VM_START,								/* JVMTI_EVENT_VM_START */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_EXCEPTION,								/* JVMTI_EVENT_EXCEPTION */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_EXCEPTION_CATCH,						/* JVMTI_EVENT_EXCEPTION_CATCH */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_SINGLE_STEP,							/* JVMTI_EVENT_SINGLE_STEP */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_FRAME_POP,								/* JVMTI_EVENT_FRAME_POP */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_BREAKPOINT,								/* JVMTI_EVENT_BREAKPOINT */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_FIELD_ACCESS,							/* JVMTI_EVENT_FIELD_ACCESS */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_FIELD_MODIFICATION,						/* JVMTI_EVENT_FIELD_MODIFICATION */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_METHOD_ENTRY,							/* JVMTI_EVENT_METHOD_ENTRY */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_METHOD_EXIT,							/* JVMTI_EVENT_METHOD_EXIT */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_NATIVE_METHOD_BIND,						/* JVMTI_EVENT_NATIVE_METHOD_BIND */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_COMPILED_METHOD_LOAD,					/* JVMTI_EVENT_COMPILED_METHOD_LOAD */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_COMPILED_METHOD_UNLOAD,					/* JVMTI_EVENT_COMPILED_METHOD_UNLOAD */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_DYNAMIC_CODE_GENERATED,					/* JVMTI_EVENT_DYNAMIC_CODE_GENERATED */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_DATA_DUMP_REQUEST,						/* JVMTI_EVENT_DATA_DUMP_REQUEST */
	0,																	/* Reserved for JVMTI event */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_MONITOR_WAIT,							/* JVMTI_EVENT_MONITOR_WAIT */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_MONITOR_WAITED,							/* JVMTI_EVENT_MONITOR_WAITED */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_MONITOR_CONTENDED_ENTER,				/* JVMTI_EVENT_MONITOR_CONTENDED_ENTER */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_MONITOR_CONTENDED_ENTERED,				/* JVMTI_EVENT_MONITOR_CONTENDED_ENTERED */
	0,																	/* Reserved for JVMTI event */
	0,																	/* Reserved for JVMTI event */
	0,																	/* Reserved for JVMTI event */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_RESOURCE_EXHAUSTED,						/* JVMTI_EVENT_RESOURCE_EXHAUSTED */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_GC_START,								/* JVMTI_EVENT_GARBAGE_COLLECTION_START */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_GC_FINISH,								/* JVMTI_EVENT_GARBAGE_COLLECTION_FINISH */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_OBJECT_FREE,							/* JVMTI_EVENT_OBJECT_FREE */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_VM_OBJECT_ALLOC,						/* JVMTI_EVENT_VM_OBJECT_ALLOC */
	0,																	/* Reserved for JVMTI event */
	J9_JNI_OFFLOAD_SWITCH_JVMTI_SAMPLED_OBJECT_ALLOC,					/* JVMTI_EVENT_SAMPLED_OBJECT_ALLOC */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_COMPILING_START,						/* J9JVMTI_EVENT_COM_IBM_COMPILING_START */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_COMPILING_END,						/* J9JVMTI_EVENT_COM_IBM_COMPILING_END */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_INSTRUMENTABLE_OBJECT_ALLOC,			/* J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_VM_DUMP_START,						/* J9JVMTI_EVENT_COM_IBM_VM_DUMP_START */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_VM_DUMP_END,							/* J9JVMTI_EVENT_COM_IBM_VM_DUMP_END */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_GC_CYCLE_START,						/* J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_GC_CYCLE_FINISH,						/* J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_VIRTUAL_THREAD_MOUNT,					/* J9JVMTI_EVENT_COM_SUN_HOTSPOT_EVENTS_VIRTUAL_THREAD_MOUNT */
	J9_JNI_OFFLOAD_SWITCH_J9JVMTI_VIRTUAL_THREAD_UNMOUNT,					/* J9JVMTI_EVENT_COM_SUN_HOTSPOT_EVENTS_VIRTUAL_THREAD_UNMOUNT */
};
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */

static void clearGlobalBreakpoint (J9VMThread * currentThread, J9JVMTIGlobalBreakpoint * globalBreakpoint);
static jvmtiError setGlobalBreakpoint (J9VMThread * currentThread, J9Method * ramMethod, IDATA location, J9JVMTIGlobalBreakpoint ** globalBreakpointPtr);
static jvmtiError createSingleBreakpoint (J9VMThread * currentThread, J9Method * ramMethod, IDATA location, J9JVMTIGlobalBreakpoint ** globalBreakpointPtr);
static UDATA hashObjectTag (void *entry, void *userData);
static void deleteBreakpointedMethodReference (J9VMThread * currentThread, J9JVMTIBreakpointedMethod * breakpointedMethod);
static UDATA fixBytecodesFrameIterator (J9VMThread * currentThread, J9StackWalkState * walkState);
static void fixBytecodesInAllStacks (J9JavaVM * vm, J9Method * method, UDATA delta);
static void clearSingleBreakpoint (J9VMThread * currentThread, J9JVMTIGlobalBreakpoint * globalBreakpoint);
static J9JVMTIGlobalBreakpoint * findGlobalBreakpoint (J9JVMTIData * jvmtiData, J9Method * ramMethod, IDATA location);
static J9JVMTIBreakpointedMethod * createBreakpointedMethod (J9VMThread * currentThread, J9Method * ramMethod);
static UDATA hashEqualObjectTag (void *lhsEntry, void *rhsEntry, void *userData);
static UDATA findDecompileInfoFrameIterator(J9VMThread *currentThread, J9StackWalkState *walkState);
static UDATA watchedClassHash (void *entry, void *userData);
static UDATA watchedClassEqual (void *lhsEntry, void *rhsEntry, void *userData);
static jint getThreadStateHelper(J9VMThread *currentThread, j9object_t threadObject, J9VMThread *vmThread);

jvmtiError
getVMThread(J9VMThread *currentThread, jthread thread, J9VMThread **vmThreadPtr, jvmtiError vThreadError, UDATA flags)
{
	J9JavaVM *vm = currentThread->javaVM;
	j9object_t threadObject = NULL;
	J9VMThread *targetThread = NULL;
	BOOLEAN isThreadAlive = FALSE;
#if JAVA_SPEC_VERSION >= 19
	BOOLEAN isVirtualThread = FALSE;
#endif /* JAVA_SPEC_VERSION >= 19 */

	if (NULL == thread) {
		if (OMR_ARE_ANY_BITS_SET(flags, J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD)) {
			return JVMTI_ERROR_INVALID_THREAD;
		}
#if JAVA_SPEC_VERSION >= 19
		if (OMR_ARE_ANY_BITS_SET(flags, J9JVMTI_GETVMTHREAD_ERROR_ON_VIRTUALTHREAD)
		&& IS_JAVA_LANG_VIRTUALTHREAD(currentThread, currentThread->threadObject)
		) {
			return vThreadError;
		}
#endif /* JAVA_SPEC_VERSION >= 19 */
		*vmThreadPtr = currentThread;
		return JVMTI_ERROR_NONE;
	} else {
		threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		if (!IS_JAVA_LANG_THREAD(currentThread, threadObject)) {
			return JVMTI_ERROR_INVALID_THREAD;
		}
#if JAVA_SPEC_VERSION >= 19
		if (OMR_ARE_ANY_BITS_SET(flags, J9JVMTI_GETVMTHREAD_ERROR_ON_VIRTUALTHREAD)
		&& IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)
		) {
			return vThreadError;
		}
#endif /* JAVA_SPEC_VERSION >= 19 */
		if (currentThread->threadObject == threadObject) {
			*vmThreadPtr = currentThread;
			return JVMTI_ERROR_NONE;
		}
	}

	/* Make sure the vmThread stays alive while it is being used. */
	omrthread_monitor_enter(vm->vmThreadListMutex);
#if JAVA_SPEC_VERSION >= 19
	isVirtualThread = IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject);
	if (isVirtualThread) {
		vm->internalVMFunctions->acquireVThreadInspector(currentThread, thread, TRUE);

		jint vthreadState = J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObject);
		j9object_t carrierThread = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObject);
		if (NULL != carrierThread) {
			targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, carrierThread);
		}
		isThreadAlive = (JVMTI_VTHREAD_STATE_NEW != vthreadState) && (JVMTI_VTHREAD_STATE_TERMINATED != vthreadState);
	} else
#endif /* JAVA_SPEC_VERSION >= 19 */
	{
		targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject);
		isThreadAlive = J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject) && (NULL != targetThread);
	}

	if (!isThreadAlive) {
		if (OMR_ARE_ANY_BITS_SET(flags, J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD)) {
#if JAVA_SPEC_VERSION >= 19
			if (isVirtualThread) {
				vm->internalVMFunctions->releaseVThreadInspector(currentThread, thread);
			}
#endif /* JAVA_SPEC_VERSION >= 19 */
			omrthread_monitor_exit(vm->vmThreadListMutex);
			return JVMTI_ERROR_THREAD_NOT_ALIVE;
		}
	}

	*vmThreadPtr = targetThread;
	if (NULL != targetThread) {
		targetThread->inspectorCount += 1;
	}

	omrthread_monitor_exit(vm->vmThreadListMutex);

#if JAVA_SPEC_VERSION >= 19
	if (isThreadAlive && !isVirtualThread) {
		/* targetThread should not be NULL for alive non-virtual threads. */
		Assert_JVMTI_true(NULL != targetThread);
	}
#endif /* JAVA_SPEC_VERSION >= 19 */

	return JVMTI_ERROR_NONE;
}



void
releaseVMThread(J9VMThread *currentThread, J9VMThread *targetThread, jthread thread)
{
#if JAVA_SPEC_VERSION >= 19
	if (NULL != thread) {
		j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		if ((currentThread->threadObject != threadObject) && IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
			currentThread->javaVM->internalVMFunctions->releaseVThreadInspector(currentThread, thread);
		}
	}
#endif /* JAVA_SPEC_VERSION >= 19 */

	if ((NULL != targetThread) && (currentThread != targetThread)) {
		J9JavaVM *vm = targetThread->javaVM;
		/* Release the J9VMThread (allow it to die) now that we are no longer inspecting it. */
		omrthread_monitor_enter(vm->vmThreadListMutex);
		if (0 == --(targetThread->inspectorCount)) {
			omrthread_monitor_notify_all(vm->vmThreadListMutex);
		}
		omrthread_monitor_exit(vm->vmThreadListMutex);
	}
}



/* Assumes that exclusive VM access and jvmtiData->mutex is held, or that execution is single-threaded (at shutdown) */

void
disposeEnvironment(J9JVMTIEnv * j9env, UDATA freeData)
{
	J9JavaVM * vm = j9env->vm;

	/* Mark this env as disposed - this prevents any further events being reported */

	if (J9_ARE_NO_BITS_SET(j9env->flags, J9JVMTIENV_FLAG_DISPOSED)) {
		J9HookInterface ** vmHook = j9env->vmHook.hookInterface;
		J9HookInterface ** gcHook = j9env->gcHook.hookInterface;
		J9HookInterface ** gcOmrHook = j9env->gcOmrHook.hookInterface;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
		J9HookInterface** jitHook = j9env->jitHook.hookInterface;
#endif

		j9env->flags |= J9JVMTIENV_FLAG_DISPOSED;

#if JAVA_SPEC_VERSION >= 11
		if (j9env->capabilities.can_generate_sampled_object_alloc_events) {
			J9JVMTI_DATA_FROM_VM(vm)->flags &= ~J9JVMTI_FLAG_SAMPLED_OBJECT_ALLOC_ENABLED;
			/* Set sampling interval to UDATA_MAX to inform GC that sampling is not required */
			vm->memoryManagerFunctions->j9gc_set_allocation_sampling_interval(vm, UDATA_MAX);
		}
#endif /* JAVA_SPEC_VERSION >= 11 */

		/* Remove all breakpoints */

		if (j9env->breakpoints != NULL) {
			J9VMThread * currentThread = vm->internalVMFunctions->currentVMThread(vm);
			pool_state poolState;
			J9JVMTIAgentBreakpoint * agentBreakpoint = NULL;

			agentBreakpoint = pool_startDo(j9env->breakpoints, &poolState);
			while (agentBreakpoint != NULL) {
				deleteAgentBreakpoint(currentThread, j9env, agentBreakpoint);
				agentBreakpoint = pool_nextDo(&poolState);
			}
		}

		/* Unhook all the events - note this does not guarantee that no further events will be received - some may be in progress already */

		unhookAllEvents(j9env);

		/* Discard the agent IDs */

		(*vmHook)->J9HookDeallocateAgentID(vmHook, j9env->vmHook.agentID);
		(*gcHook)->J9HookDeallocateAgentID(gcHook, j9env->gcHook.agentID);
		(*gcOmrHook)->J9HookDeallocateAgentID(gcOmrHook, j9env->gcOmrHook.agentID);
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
		if (jitHook != NULL) {
			(*jitHook)->J9HookDeallocateAgentID(vmHook, j9env->jitHook.agentID);
		}
#endif
	}

	/* Free the data if requested */

	if (freeData) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		j9mem_free_memory(j9env->prefixes);

		if (NULL != j9env->mutex) {
			omrthread_monitor_destroy(j9env->mutex);
			j9env->mutex = NULL;
		}

		if (NULL !=  j9env->threadDataPoolMutex) {
			omrthread_monitor_destroy(j9env->threadDataPoolMutex);
			j9env->threadDataPoolMutex = NULL;
		}

		if (NULL != j9env->threadDataPool) {
			pool_kill(j9env->threadDataPool);
			j9env->threadDataPool = NULL;
		}

		if (NULL != j9env->objectTagTable) {
			hashTableFree(j9env->objectTagTable);
			j9env->objectTagTable = NULL;
		}

		if (NULL != j9env->watchedClasses) {
			J9HashTableState walkState;
			J9JVMTIWatchedClass *watchedClass = hashTableStartDo(j9env->watchedClasses, &walkState);
			while (NULL != watchedClass) {
				if (J9JVMTI_CLASS_REQUIRES_ALLOCATED_J9JVMTI_WATCHED_FIELD_ACCESS_BITS(watchedClass->clazz)) {
					j9mem_free_memory(watchedClass->watchBits);
				}
				watchedClass = hashTableNextDo(&walkState);
			}
			hashTableFree(j9env->watchedClasses);
			j9env->watchedClasses = NULL;
		}

		if (NULL != j9env->breakpoints) {
			pool_kill(j9env->breakpoints);
			j9env->breakpoints = NULL;
		}

		if (0 != j9env->tlsKey) {
			jvmtiTLSFree(vm, j9env->tlsKey);
			j9env->tlsKey = 0;
		}
	}
}



jint
allocateEnvironment(J9InvocationJavaVM * invocationJavaVM, jint version, void ** penv)
{
	J9JavaVM * vm = NULL;
	J9JVMTIData * jvmtiData = NULL;
	J9JVMTIEnv * j9env;
	jint rc = JNI_EDETACHED;
	J9VMThread * currentThread;

	Assert_JVMTI_true(NULL != invocationJavaVM);
	vm = invocationJavaVM->j9vm;
	jvmtiData = J9JVMTI_DATA_FROM_VM(vm);

	if (getCurrentVMThread(vm, &currentThread) == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		omrthread_monitor_enter(jvmtiData->mutex);

		rc = JNI_ENOMEM;

		j9env = pool_newElement(jvmtiData->environments);
		if (j9env != NULL) {
			J9HookInterface ** vmHook = vm->internalVMFunctions->getVMHookInterface(vm);
			J9HookInterface ** gcHook = vm->memoryManagerFunctions->j9gc_get_hook_interface(vm);
			J9HookInterface ** gcOmrHook = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
			J9HookInterface** jitHook = vm->internalVMFunctions->getJITHookInterface(vm);
#endif

			memset(j9env, 0, sizeof(J9JVMTIEnv));
			j9env->functions = &jvmtiFunctionTable;
			j9env->vm = vm;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			/* Jazz 99339: store J9NativeLibrary in J9JVMTIEnv so as to check the zAAP switching flag in the JVMTI event callbacks */
			j9env->library = invocationJavaVM->reserved2_library;
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
			j9env->vmHook.hookInterface = vmHook;
			j9env->gcHook.hookInterface = gcHook;
			j9env->gcOmrHook.hookInterface = gcOmrHook;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
			j9env->jitHook.hookInterface = jitHook;
#endif
			if ((j9env->vmHook.agentID = (*vmHook)->J9HookAllocateAgentID(vmHook)) == 0) {
				goto fail;
			}
			if ((j9env->gcHook.agentID = (*gcHook)->J9HookAllocateAgentID(gcHook)) == 0) {
				goto fail;
			}
			if ((j9env->gcOmrHook.agentID = (*gcOmrHook)->J9HookAllocateAgentID(gcOmrHook)) == 0) {
				goto fail;
			}
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
			if (jitHook != NULL) {
				if ((j9env->jitHook.agentID = (*jitHook)->J9HookAllocateAgentID(jitHook)) == 0) {
					goto fail;
				}
			}
#endif
			if (omrthread_monitor_init(&(j9env->mutex), 0) != 0) {
				goto fail;
			}
			if (omrthread_monitor_init(&(j9env->threadDataPoolMutex), 0) != 0) {
				goto fail;
			}
			j9env->threadDataPool = pool_new(sizeof(J9JVMTIThreadData), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
			if (j9env->threadDataPool == NULL) {
				goto fail;
			}
			j9env->objectTagTable = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), J9_GET_CALLSITE(), 0, sizeof(J9JVMTIObjectTag), sizeof(jlong), 0,  J9MEM_CATEGORY_JVMTI, hashObjectTag, hashEqualObjectTag, NULL, NULL);
			if (j9env->objectTagTable == NULL) {
				goto fail;
			}
			j9env->watchedClasses = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), J9_GET_CALLSITE(), 0, sizeof(J9JVMTIWatchedClass), sizeof(UDATA), 0,  J9MEM_CATEGORY_JVMTI, watchedClassHash, watchedClassEqual, NULL, NULL);
			if (j9env->watchedClasses == NULL) {
				goto fail;
			}
			j9env->breakpoints = pool_new(sizeof(J9JVMTIAgentBreakpoint), 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
			if (j9env->breakpoints == NULL) {
				goto fail;
			}
			j9env->tlsKey = 0;

			/* Hook the thread and virtual thread destroy events to clean up any allocated TLS structs. */

			if (hookRequiredEvents(j9env) != 0) {
				goto fail;
			}

			if (jvmtiData->environmentsHead == NULL) {
				issueWriteBarrier();
				jvmtiData->environmentsHead = jvmtiData->environmentsTail = j9env;
			} else {
				j9env->linkPrevious = jvmtiData->environmentsTail;
				issueWriteBarrier();
				jvmtiData->environmentsTail->linkNext = j9env;
				jvmtiData->environmentsTail = j9env;
			}

			rc = JNI_OK;
			*penv = j9env;

fail:
			if (rc != JNI_OK) {
				disposeEnvironment(j9env, TRUE);
			}
		}

		omrthread_monitor_exit(jvmtiData->mutex);
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	return rc;
}




UDATA
prepareForEvent(J9JVMTIEnv * j9env, J9VMThread * currentThread, J9VMThread * eventThread, UDATA eventNumber, jthread * threadRefPtr, UDATA * hadVMAccessPtr, UDATA wantVMAccess, UDATA jniRefSlots, UDATA * javaOffloadOldState)
{
	J9JVMTIThreadData *threadData = NULL;
#if JAVA_SPEC_VERSION >= 19
	j9object_t threadObj = currentThread->threadObject;
	/* threadObj can be null early in J9VMThread initialization. */
	if (NULL != threadObj) {
		void *tlsArray = J9OBJECT_ADDRESS_LOAD(currentThread, threadObj, currentThread->javaVM->tlsOffset);
		if (NULL != tlsArray) {
			threadData = jvmtiTLSGet(currentThread, threadObj, j9env->tlsKey);
		}
	}
#else
	threadData = jvmtiTLSGet(currentThread, currentThread->threadObject, j9env->tlsKey);
#endif /* JAVA_SPEC_VERSION >= 19 */

	/* If this environment has been disposed, do not report any events */

	if (j9env->flags & J9JVMTIENV_FLAG_DISPOSED) {
		return FALSE;
	}

	/* Allow the VM_DEATH and THREAD_END event to be sent on a dead thread, but no others */

	if (currentThread->publicFlags & J9_PUBLIC_FLAGS_STOPPED) {
		if ((eventNumber != JVMTI_EVENT_VM_DEATH)
		&& (eventNumber != JVMTI_EVENT_THREAD_END)
		) {
			return FALSE;
		}
	}

	/* Do not issue events on threads that do not have a threadObject. We ignore this check
	 * when in Primordial phase since the threadObject is not initialized until sometime after
	 * j.l.Thread is loaded. This is done to correctly fire the Class File Load events on classes
	 * loaded prior to j.l.Thread. See CMVC 127730 */
	
	if ((NULL == eventThread->threadObject) && (J9JVMTI_PHASE(j9env) != JVMTI_PHASE_PRIMORDIAL)) {
        return FALSE;
    }

	/* See if the event is enabled either globally or for the current thread */

	if (EVENT_IS_ENABLED(eventNumber, &(j9env->globalEventEnable))
		|| ((NULL != threadData) && EVENT_IS_ENABLED(eventNumber, &(threadData->threadEventEnable)))
	) {
		j9object_t * refs;

		++jniRefSlots;		/* for saving the current exception */
		if (threadRefPtr != NULL) {
			++jniRefSlots;
		}
		*hadVMAccessPtr = pushEventFrame(currentThread, TRUE, jniRefSlots);

		/* Preserve and clear any pending exception in the current thread */

		refs = (j9object_t*) (((J9SFJNINativeMethodFrame *) currentThread->sp) + 1);
		refs[0] = currentThread->currentException;
		currentThread->currentException = NULL;

		if (threadRefPtr != NULL) {
			jthread threadRef = (jthread) (refs + 1);

			*((j9object_t*) threadRef) = eventThread->threadObject;
			*threadRefPtr = threadRef;
		}
		if (!wantVMAccess) {
			currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
		}

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		/* Jazz 99339: Switch away from the zAAP processor if running there */
		if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
			javaOffloadSwitchOff(j9env, currentThread, eventNumber, javaOffloadOldState);
		}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */

		return TRUE;
	}

	return FALSE;
}


#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
void
javaOffloadSwitchOff(J9JVMTIEnv * j9env, J9VMThread * currentThread, UDATA eventNumber, UDATA * javaOffloadOldState)
{
	J9JavaVM * vm = NULL;
	BOOLEAN zAAPSwitchingFlag = FALSE;
	J9NativeLibrary * nativeLibrary = j9env->library;
	
	Assert_JVMTI_true(NULL != currentThread);
	vm = currentThread->javaVM;

	/* There are two cases for zAAP switching:
	 * 1) The agent library (most likely created by customer) calls GetEnv() at any time when required 
	 *    but doesn't need to call GetEnv() in Agent_OnLoad. Thus, it may be possible for j9env->library 
	 *    to be NULL if JVMTI is used directly by the VM.
	 * 2) The flag in J9NativeLibrary->doSwitching requires it to do zAAP switching.
	 */
	if (NULL == nativeLibrary) {
		zAAPSwitchingFlag = TRUE;
	} else if (J9_ARE_ALL_BITS_SET(nativeLibrary->doSwitching, J9_NATIVE_LIBRARY_SWITCH_REQUIRED)) {
		zAAPSwitchingFlag = TRUE;
	}

	if ((NULL != vm->javaOffloadSwitchOffWithReasonFunc) && (TRUE == zAAPSwitchingFlag)) {
		/* Save the old state */
		*javaOffloadOldState = currentThread->javaOffloadState;
		/* Zero the state and switch java offload mode OFF */
		currentThread->javaOffloadState = 0;
		vm->javaOffloadSwitchOffWithReasonFunc(currentThread, reasonCodeFromJVMTIEvent[eventNumber - J9JVMTI_LOWEST_EVENT]);
	}
}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */


jvmtiError
getCurrentVMThread(J9JavaVM * vm, J9VMThread ** currentThread)
{
	*currentThread = vm->internalVMFunctions->currentVMThread(vm);
	if (*currentThread == NULL) {
		return JVMTI_ERROR_UNATTACHED_THREAD;
	}
	return JVMTI_ERROR_NONE;
}



jvmtiError
cStringFromUTF(jvmtiEnv * env, J9UTF8 * utf, char ** cString)
{
	return cStringFromUTFChars(env, J9UTF8_DATA(utf), J9UTF8_LENGTH(utf), cString);
}


jvmtiError
cStringFromUTFChars(jvmtiEnv * env, U_8 * data, UDATA length, char ** cString)
{
	jvmtiError rc = JVMTI_ERROR_NONE;
	PORT_ACCESS_FROM_JVMTI(env);

	*cString = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (*cString == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		memcpy(*cString, data, length);
		(*cString)[length] = '\0';
	}

	return rc;
}


U_16
nextUTFChar(U_8 ** pUtfData)
{
	U_8 * utfData = *pUtfData;
	U_16 utfChar;

	utfChar = *(utfData++);
	if (utfChar & 0x80) {
		if (utfChar & 0x20) {
			utfChar = (utfChar & 0x0F) << 12;
			utfChar |= ((((U_16) *(utfData++)) & 0x3F) << 6);
			utfChar |= (((U_16) *(utfData++)) & 0x3F);
		} else {
			utfChar = (utfChar & 0x1F) << 6;
			utfChar |= (((U_16) *(utfData++)) & 0x3F);
		}
	}

	*pUtfData = utfData;
	return utfChar;
}


void
skipSignature(U_8 ** pUtfData)
{
	U_16 utfChar;

	/* Skip array prefixes */

	do {
		utfChar = nextUTFChar(pUtfData);
	} while (utfChar == '[');

	/* Skip to the end of Object type signatures */

	if (IS_REF_OR_VAL_SIGNATURE(utfChar)) {
		do {
			utfChar = nextUTFChar(pUtfData);
		} while (utfChar != ';');
	}
}


void
finishedEvent(J9VMThread * currentThread, UDATA eventNumber, UDATA hadVMAccess, UDATA javaOffloadOldState)
{
	J9SFJNINativeMethodFrame * frame = ((J9SFJNINativeMethodFrame *) (((U_8 *) currentThread->sp) + (UDATA) currentThread->literals));

	/* Acquire VM access if the current thread does not already have it */

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	if (currentThread->inNative) {
		currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
	if (!(currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS)) {
		currentThread->javaVM->internalVMFunctions->internalAcquireVMAccess(currentThread);
	}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

	/* Restore the pending exception in the current thread */

	currentThread->currentException = getObjectFromRef((j9object_t*) (frame + 1));

	/* Pop the frame */

	popEventFrame(currentThread, hadVMAccess);

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	/* Jazz 99339: Switch onto the zAAP processor if not running there after native finishes running on GP */
	if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
		javaOffloadSwitchOn(currentThread, eventNumber, javaOffloadOldState);
	}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}


#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
void
javaOffloadSwitchOn(J9VMThread * currentThread, UDATA eventNumber, UDATA javaOffloadOldState)
{
	J9JavaVM *vm = NULL;

	Assert_JVMTI_true(NULL != currentThread);
	vm = currentThread->javaVM;

	/* Check if java offload mode is enabled */
	if (NULL != vm->javaOffloadSwitchOnWithReasonFunc) {
		if (0 == currentThread->javaOffloadState) {
			/* Call the offload switch ON */
			vm->javaOffloadSwitchOnWithReasonFunc(currentThread, reasonCodeFromJVMTIEvent[eventNumber - J9JVMTI_LOWEST_EVENT]);
			/* Restore the old state */
			currentThread->javaOffloadState = javaOffloadOldState;
		}
	}
}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */


static jint
getThreadStateHelper(J9VMThread *currentThread, j9object_t threadObject, J9VMThread *vmThread)
{
	jint state = 0;
	UDATA vmstate = getVMThreadObjectStatesAll(vmThread, NULL, NULL, NULL);
	
	/* Assumes that the current thread has VM access, and the thread is locked (will not die) */

	if (vmThread == NULL || (vmstate & J9VMTHREAD_STATE_UNKNOWN)) {
		if (J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject)) {
			state |= JVMTI_THREAD_STATE_TERMINATED;
		} else {
			/* new thread */
		}
	} else if (vmstate & J9VMTHREAD_STATE_DEAD)  {
		state |= JVMTI_THREAD_STATE_TERMINATED;
	} else {		
		state |= JVMTI_THREAD_STATE_ALIVE;

		if (vmstate & J9VMTHREAD_STATE_SUSPENDED) {
			state |= JVMTI_THREAD_STATE_SUSPENDED;
		}
#if JAVA_SPEC_VERSION >= 19
		/* Based on the isSuspendedInternal field, set the JVMTI
		 * thread state to suspended for the corresponding thread.
		 */
		if (0 != J9OBJECT_U32_LOAD(currentThread, threadObject, currentThread->javaVM->isSuspendedInternalOffset)) {
			state |= JVMTI_THREAD_STATE_SUSPENDED;
		} else {
			state &= ~JVMTI_THREAD_STATE_SUSPENDED;
		}
#endif /* JAVA_SPEC_VERSION >= 19 */
		if (vmstate & J9VMTHREAD_STATE_INTERRUPTED) {
			state |= JVMTI_THREAD_STATE_INTERRUPTED;
		}
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		if (vmThread->inNative) {
			state |= JVMTI_THREAD_STATE_IN_NATIVE;
		}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
		if ( (vmThread->omrVMThread->vmState & J9VMSTATE_MAJOR) == J9VMSTATE_JNI ) {
			if (!(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS)) {
				state |= JVMTI_THREAD_STATE_IN_NATIVE;
			}
		}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		if (vmstate & J9VMTHREAD_STATE_BLOCKED) {
			state |= JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;
		/* Object.wait() */
		} else if (vmstate & J9VMTHREAD_STATE_WAITING) {
			state |= JVMTI_THREAD_STATE_IN_OBJECT_WAIT;
			state |= JVMTI_THREAD_STATE_WAITING_INDEFINITELY;
			state |= JVMTI_THREAD_STATE_WAITING;
		/* Object.wait(...) */
		} else if (vmstate & J9VMTHREAD_STATE_WAITING_TIMED) {
			state |= JVMTI_THREAD_STATE_IN_OBJECT_WAIT;
			state |= JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;
			state |= JVMTI_THREAD_STATE_WAITING;
		/* Lock.park() */
		} else if (vmstate & J9VMTHREAD_STATE_PARKED) {
			state |= JVMTI_THREAD_STATE_PARKED;
			state |= JVMTI_THREAD_STATE_WAITING_INDEFINITELY;
			state |= JVMTI_THREAD_STATE_WAITING;
		/* Lock.park(...) */
		} else if (vmstate & J9VMTHREAD_STATE_PARKED_TIMED) {
			state |= JVMTI_THREAD_STATE_PARKED;
			state |= JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;
			state |= JVMTI_THREAD_STATE_WAITING;	
		/* Thread.sleep(...) */
		} else if (vmstate & J9VMTHREAD_STATE_SLEEPING) {
			state |= JVMTI_THREAD_STATE_SLEEPING;
			state |= JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;
			state |= JVMTI_THREAD_STATE_WAITING;
		} else {
			state |= JVMTI_THREAD_STATE_RUNNABLE;
		}
	}

	return state;
}

jint
getThreadState(J9VMThread *currentThread, j9object_t threadObject)
{
	J9VMThread *vmThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject);
	jint state = getThreadStateHelper(currentThread, threadObject, vmThread);

#if JAVA_SPEC_VERSION >= 19
	if (J9_ARE_ANY_BITS_SET(state, JVMTI_THREAD_STATE_RUNNABLE)) {
		if ((NULL != vmThread->currentContinuation)
		&& (vmThread->threadObject != vmThread->carrierThreadObject)
		) {
			/* If a virtual thread is mounted on a carrier thread, then the carrier thread
			 * is considered implicitly parked.
			 */
			state &= ~JVMTI_THREAD_STATE_RUNNABLE;
			state |= JVMTI_THREAD_STATE_WAITING_INDEFINITELY;
			state |= JVMTI_THREAD_STATE_WAITING;
		}
	}
#endif /* JAVA_SPEC_VERSION >= 19 */

	return state;
}

#if JAVA_SPEC_VERSION >= 19
jint
getVirtualThreadState(J9VMThread *currentThread, jthread thread)
{
	J9JavaVM* vm = currentThread->javaVM;
	jint rc = 0;
	J9VMThread *targetThread = NULL;
	Assert_JVMTI_notNull(thread);
	Assert_JVMTI_mustHaveVMAccess(currentThread);
	rc = getVMThread(
			currentThread, thread, &targetThread, JVMTI_ERROR_NONE,
			J9JVMTI_GETVMTHREAD_ERROR_ON_NULL_JTHREAD);
	if (JVMTI_ERROR_NONE == rc) {
		j9object_t vThreadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		if (NULL != targetThread) {
			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
			rc = getThreadStateHelper(currentThread, targetThread->carrierThreadObject, targetThread);
			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
		} else {
			jint vThreadState = (jint) J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, vThreadObject);
			/* The mapping from JVMTI_VTHREAD_STATE_XXX to JVMTI_JAVA_LANG_THREAD_STATE_XXX is based
			 * on j.l.VirtualThread.threadState().
			 */
			switch (vThreadState) {
			case JVMTI_VTHREAD_STATE_NEW:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_NEW;
				break;
			case JVMTI_VTHREAD_STATE_STARTED:
			{
				JNIEnv *env = (JNIEnv *)currentThread;
				jfieldID fid = NULL;
				jclass jlThread = NULL;

				vm->internalVMFunctions->internalExitVMToJNI(currentThread);
				jlThread = (*env)->FindClass(env, "java/lang/Thread");
				if (NULL != jlThread) {
					fid = (*env)->GetFieldID(env, jlThread, "container", "Ljdk/internal/vm/ThreadContainer;");
				}
				if ((NULL != fid)
					&& (NULL == (*env)->GetObjectField(env, thread, fid))
				) {
					rc = JVMTI_JAVA_LANG_THREAD_STATE_NEW;
				} else {
					rc = JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE;
				}
				vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
				/* Re-fetch object to correctly set the isSuspendedInternal field. */
				vThreadObject = J9_JNI_UNWRAP_REFERENCE(thread);
				break;
			}
			case JVMTI_VTHREAD_STATE_RUNNABLE:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE;
				break;
			case JVMTI_VTHREAD_STATE_RUNNABLE_SUSPENDED:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE | JVMTI_THREAD_STATE_SUSPENDED;
				break;
			case JVMTI_VTHREAD_STATE_RUNNING:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE;
				break;
			case JVMTI_VTHREAD_STATE_PARKING:
				/* Fall Through */
			case JVMTI_VTHREAD_STATE_YIELDING:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE;
				break;
			case JVMTI_VTHREAD_STATE_PARKED:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_PARKED;
				break;
			case JVMTI_VTHREAD_STATE_PARKED_SUSPENDED:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_PARKED | JVMTI_THREAD_STATE_SUSPENDED;
				break;
			case JVMTI_VTHREAD_STATE_PINNED:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_WAITING;
				break;
			case JVMTI_VTHREAD_STATE_TERMINATED:
				rc = JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED;
				break;
			default:
				Assert_JVMTI_unreachable();
				rc = JVMTI_ERROR_INTERNAL;
			}
		}
		/* Re-fetch object to correctly set the isSuspendedInternal field. */
		vThreadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		if (0 != J9OBJECT_U32_LOAD(currentThread, vThreadObject, vm->isSuspendedInternalOffset)) {
			rc |= JVMTI_THREAD_STATE_SUSPENDED;
		} else {
			rc &= ~JVMTI_THREAD_STATE_SUSPENDED;
		}
		releaseVMThread(currentThread, targetThread, thread);
	} else {
		/* This is unreachable. */
		Assert_JVMTI_unreachable();
		rc = JVMTI_ERROR_INTERNAL;
	}
	return rc;
}
#endif /* JAVA_SPEC_VERSION >= 19 */


void
fillInJValue(char signatureType, jvalue * jvaluePtr, void * valueAddress, j9object_t * objectStorage)
{
	j9object_t object;

	switch (signatureType) {
		case 'Z':
			jvaluePtr->z = (jboolean) *((I_32 *) valueAddress);
			break;
		case 'B':
			jvaluePtr->b = (jbyte) *((I_32 *) valueAddress);
			break;
		case 'C':
			jvaluePtr->c = (jchar) *((I_32 *) valueAddress);
			break;
		case 'S':
			jvaluePtr->s = (jshort) *((I_32 *) valueAddress);
			break;
		case 'I':
			jvaluePtr->i = (jint) *((I_32 *) valueAddress);
			break;
		case 'J':
			memcpy(&(jvaluePtr->j), valueAddress, 8);
			break;
		case 'F':
			jvaluePtr->f = *((jfloat *) valueAddress);
			break;
		case 'D':
			memcpy(&(jvaluePtr->d), valueAddress, 8);
			break;
		case 'L':
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		case 'Q':
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
			object = *((j9object_t*) valueAddress);
			if (object == NULL) {
				jvaluePtr->l = NULL;
			} else {
				*objectStorage = object;
				jvaluePtr->l = (jobject) objectStorage;
			}
			break;
	}
}



j9object_t
getObjectFromRef(j9object_t * ref)
{
	j9object_t result = *ref;

#ifdef J9VM_INTERP_GROWABLE_STACKS
	/* See if the ref was redirected */

	if (((UDATA) result) & J9_REDIRECTED_REFERENCE) {
		result = *((j9object_t*) (((UDATA) result) - J9_REDIRECTED_REFERENCE));
	}
#endif

	return result;
}


J9Class *
getCurrentClass(J9Class * clazz)
{
	return J9_CURRENT_CLASS(clazz);
}



static UDATA
hashObjectTag(void *entry, void *userData) 
{
	return ((UDATA) ((J9JVMTIObjectTag *) entry)->ref) / sizeof(UDATA);
}


static UDATA
hashEqualObjectTag(void *lhsEntry, void *rhsEntry, void *userData) 
{
	return (((J9JVMTIObjectTag *) lhsEntry)->ref == ((J9JVMTIObjectTag *) rhsEntry)->ref);
}


static UDATA
watchedClassHash(void *entry, void *userData) 
{
	return ((UDATA) ((J9JVMTIWatchedClass *) entry)->clazz) / J9_REQUIRED_CLASS_ALIGNMENT;
}


static UDATA
watchedClassEqual(void *lhsEntry, void *rhsEntry, void *userData) 
{
	return (((J9JVMTIWatchedClass *) lhsEntry)->clazz == ((J9JVMTIWatchedClass *) rhsEntry)->clazz);
}


jlong
getObjectSize(J9JavaVM *vm, j9object_t obj)
{
	/* NOTE: we treat classes just like any other object and ignore
	 * the space used by the backing J9Class
	 */

	return (jlong) vm->memoryManagerFunctions->j9gc_get_object_size_in_bytes(vm, obj);
}


static void
clearGlobalBreakpoint(J9VMThread * currentThread, J9JVMTIGlobalBreakpoint * globalBreakpoint)
{
	/* Decrement the reference count of the breakpoint - if it reaches 0, remove it */

	if (--(globalBreakpoint->referenceCount) == 0) {
		do {
			J9JVMTIGlobalBreakpoint * next = globalBreakpoint->equivalentBreakpoint;
			clearSingleBreakpoint(currentThread, globalBreakpoint);
			globalBreakpoint = next;
		} while (globalBreakpoint != NULL);
	}
}



J9JVMTIAgentBreakpoint *
findAgentBreakpoint(J9VMThread * currentThread, J9JVMTIEnv * j9env, J9Method * ramMethod, IDATA location)
{
	pool_state poolState;
	J9JVMTIAgentBreakpoint * agentBreakpoint;
	jmethodID currentMethod;

	currentMethod = getCurrentMethodID(currentThread, ramMethod);
	agentBreakpoint = pool_startDo(j9env->breakpoints, &poolState);
	while (agentBreakpoint != NULL) {
		if ((agentBreakpoint->method == currentMethod) && (agentBreakpoint->location == location)) {
			break;
		}
		agentBreakpoint = pool_nextDo(&poolState);
	}
	return agentBreakpoint;
}



J9JVMTIBreakpointedMethod *
findBreakpointedMethod(J9JVMTIData * jvmtiData, J9Method * ramMethod)
{
	pool_state poolState;
	J9JVMTIBreakpointedMethod * breakpointedMethod;

	breakpointedMethod = pool_startDo(jvmtiData->breakpointedMethods, &poolState);
	while (breakpointedMethod != NULL) {
		if (breakpointedMethod->method == ramMethod) {
			break;
		}
		breakpointedMethod = pool_nextDo(&poolState);
	}
	return breakpointedMethod;
}



static jvmtiError
setGlobalBreakpoint(J9VMThread * currentThread, J9Method * ramMethod, IDATA location, J9JVMTIGlobalBreakpoint ** globalBreakpointPtr)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9JVMTIGlobalBreakpoint * globalBreakpoint;
	jvmtiError rc = JVMTI_ERROR_NONE;

	/* See if the breakpoint already exists - if it does, just increment the reference count */

	globalBreakpoint = findGlobalBreakpoint(jvmtiData, ramMethod, location);
	if (globalBreakpoint != NULL) {
		++(globalBreakpoint->referenceCount);
		*globalBreakpointPtr = globalBreakpoint;
		rc = JVMTI_ERROR_NONE;
		goto done;
	}

	rc = createSingleBreakpoint(currentThread, ramMethod, location, &globalBreakpoint);
	if (rc == JVMTI_ERROR_NONE) {
		/* there may be other equivalent versions of this method, must search the replaced class
		 * chain starting from the most current class */
		J9JVMTIGlobalBreakpoint ** bpChain = globalBreakpointPtr;
		jmethodID methodID = getCurrentMethodID(currentThread, ramMethod);
		J9Method *currentMethod = ((J9JNIMethodID *) methodID)->method;
		J9Class *currentClass = J9_CLASS_FROM_METHOD(currentMethod);

		/* we should always be passed in the most current equivalent version of the method */
		Assert_JVMTI_true(ramMethod == currentMethod);

		/* search replacedClass chain for the rest */
		while (NULL != currentClass->replacedClass) {
			void **methodIDs = NULL;
			UDATA methodIndex = 0;
			UDATA methodCount = 0;
			J9Method *equivalentMethod = NULL;
			BOOLEAN found = FALSE;

			currentClass = currentClass->replacedClass;
			methodIDs = currentClass->jniIDs;
			methodCount = currentClass->romClass->romMethodCount;

			for (methodIndex = 0; methodIndex < methodCount; methodIndex++) {
				if (methodIDs[methodIndex] == methodID) {
					equivalentMethod = currentClass->ramMethods + methodIndex;
					found = TRUE;
					
					*bpChain = globalBreakpoint;
					bpChain = &globalBreakpoint->equivalentBreakpoint;

					rc = createSingleBreakpoint(currentThread, equivalentMethod, location, &globalBreakpoint);
					if (rc != JVMTI_ERROR_NONE) {
						clearGlobalBreakpoint(currentThread, *globalBreakpointPtr);
						*globalBreakpointPtr = NULL;
						goto done;
					}
					break;
				}
			}
			if (!found) {
				/* method was not found in the currentClass, stop searching now */
				break;
			}
		}
		
		*bpChain = globalBreakpoint;
	}

done:
	return rc;
}



static void
clearSingleBreakpoint(J9VMThread * currentThread, J9JVMTIGlobalBreakpoint * globalBreakpoint)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9JVMTIBreakpointedMethod * breakpointedMethod = globalBreakpoint->breakpointedMethod;
	IDATA location = globalBreakpoint->location;

	/* Fix the breakpoint bytecode back to it's original bytecode */

	J9_BYTECODE_START_FROM_ROM_METHOD(breakpointedMethod->copiedROMMethod)[location] =
		 J9_BYTECODE_START_FROM_ROM_METHOD(breakpointedMethod->originalROMMethod)[location];

	/* Remove this breakpoint's reference to the breakpointed method */

	deleteBreakpointedMethodReference(currentThread, breakpointedMethod);

	/* Delete the breakpoint */

	pool_removeElement(jvmtiData->breakpoints, globalBreakpoint);
}



static J9JVMTIBreakpointedMethod *
createBreakpointedMethod(J9VMThread * currentThread, J9Method * ramMethod)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JVMTIBreakpointedMethod * breakpointedMethod;
	J9ROMMethod * originalROMMethod;
	J9ROMMethod * copiedROMMethod;
	UDATA allocSize;
	UDATA methodSize;
	UDATA delta;
	J9ExceptionInfo * originalExceptionInfo = NULL;
#ifdef J9VM_ENV_DATA64
	J9SRP * originalThrowNames = NULL;
	J9UTF8 * methodName;
	J9UTF8 * methodSignature;
	J9UTF8 * genericSignature;
	U_8 * copyPtr;
#endif

	/* Create breakpointed method */

	/*
	 * NOTE:
	 * Ignoring the SRPs in the MethodDebugInfo area, since they are only accessed
	 * through the getMethodDebugInfoForROMClass() function in optinfo.c.
	 * That function uses getOriginalROMMethod() to look for the debug information
	 * in the original ROMMethod and not the copy that is about to be made.
	 */

	breakpointedMethod = pool_newElement(jvmtiData->breakpointedMethods);
	if (breakpointedMethod == NULL) {
		return NULL;
	}
	originalROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	breakpointedMethod->referenceCount = 0;
	breakpointedMethod->method = ramMethod;
	breakpointedMethod->originalROMMethod = originalROMMethod;

	/* Get throw exception names, if any */

	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(originalROMMethod)) {
		originalExceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(originalROMMethod);
#ifdef J9VM_ENV_DATA64
		originalThrowNames = J9EXCEPTIONINFO_THROWNAMES(originalExceptionInfo);
#endif
	}

	/* Copy ROM method */

	methodSize = ((UDATA) nextROMMethod(originalROMMethod)) - (UDATA) originalROMMethod;
	allocSize = methodSize;
#ifdef J9VM_ENV_DATA64
	/* Account for the space taken by the name and signature UTF8s */

	methodName = J9ROMMETHOD_NAME(originalROMMethod);
	/* allocSize guaranteed to be 4-aligned at this point, so no need to align for UTF */
	allocSize += ((sizeof(U_16) + J9UTF8_LENGTH(methodName) + 1) & ~1);
	methodSignature = J9ROMMETHOD_SIGNATURE(originalROMMethod);
	allocSize += ((sizeof(U_16) + J9UTF8_LENGTH(methodSignature) + 1) & ~1);
	genericSignature = J9_GENERIC_SIGNATURE_FROM_ROM_METHOD(originalROMMethod);
	if (genericSignature != NULL) {
		allocSize += ((sizeof(U_16) + J9UTF8_LENGTH(genericSignature) + 1) & ~1);
	}

	/* Account for space for thrown exception names */

	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(originalROMMethod)) {
		U_16 i;

		for (i = 0; i < originalExceptionInfo->throwCount; ++i) {
			J9UTF8 * originalName = NNSRP_GET(originalThrowNames[i], J9UTF8 *);

			allocSize += ((sizeof(U_16) + J9UTF8_LENGTH(originalName) + 1) & ~1);
		}
	}
#endif
	copiedROMMethod = j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_JVMTI);
	if (copiedROMMethod == NULL) {
		pool_removeElement(jvmtiData->breakpointedMethods, breakpointedMethod);
		return NULL;
	}
	breakpointedMethod->copiedROMMethod = copiedROMMethod;
	memcpy(copiedROMMethod, originalROMMethod, methodSize);
	delta = ((UDATA) copiedROMMethod) - (UDATA) originalROMMethod;
#ifdef J9VM_ENV_DATA64
	copyPtr = ((U_8 *) copiedROMMethod) + methodSize;
	/* copyPtr guaranteed to be 4-aligned at this point, so no need to align for UTF */

	/* Copy name and signature UTF8s */

	NNSRP_SET(copiedROMMethod->nameAndSignature.name, copyPtr);
	memcpy(copyPtr, methodName, sizeof(U_16) + J9UTF8_LENGTH(methodName));
	copyPtr += ((sizeof(U_16) + J9UTF8_LENGTH(methodName) + 1) & ~1);
	NNSRP_SET(copiedROMMethod->nameAndSignature.signature, copyPtr);
	memcpy(copyPtr, methodSignature, sizeof(U_16) + J9UTF8_LENGTH(methodSignature));
	copyPtr += ((sizeof(U_16) + J9UTF8_LENGTH(methodSignature) + 1) & ~1);
	if (genericSignature != NULL) {
		NNSRP_PTR_SET(J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(copiedROMMethod), copyPtr);
		memcpy(copyPtr, genericSignature, sizeof(U_16) + J9UTF8_LENGTH(genericSignature));
		copyPtr += ((sizeof(U_16) + J9UTF8_LENGTH(genericSignature) + 1) & ~1);
	}

	/* Copy the thrown exception names, if any */

	if (originalExceptionInfo != NULL) {
		U_16 i;
		J9ExceptionInfo * copiedExceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(copiedROMMethod);
		J9SRP * newThrowNames = J9EXCEPTIONINFO_THROWNAMES(copiedExceptionInfo);

		for (i = 0; i < originalExceptionInfo->throwCount; ++i) {
			J9UTF8 * originalName = NNSRP_GET(originalThrowNames[i], J9UTF8 *);

			NNSRP_SET(newThrowNames[i], copyPtr);
			memcpy(copyPtr, originalName, sizeof(U_16) + J9UTF8_LENGTH(originalName));
			copyPtr += ((sizeof(U_16) + J9UTF8_LENGTH(originalName) + 1) & ~1);
		}
	}
#else
	/* Relocate the name and signature UTF8s */

	copiedROMMethod->nameAndSignature.name -= delta;
	copiedROMMethod->nameAndSignature.signature -= delta;
	if (J9ROMMETHOD_HAS_GENERIC_SIGNATURE(copiedROMMethod)) {
		(*J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(copiedROMMethod)) -= delta;
	}

	/* Relocate the exception names, if any */

	if (originalExceptionInfo != NULL) {
		U_16 i;
		J9ExceptionInfo * copiedExceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(copiedROMMethod);
		J9SRP * newThrowNames = J9EXCEPTIONINFO_THROWNAMES(copiedExceptionInfo);

		for (i = 0; i < copiedExceptionInfo->throwCount; ++i) {
			newThrowNames[i] -= delta;
		}
	}
#endif

	/* Fix the method and stacks to point to the new bytecodes */

	fixBytecodesInAllStacks(vm, ramMethod, delta);
	
	/* Add the delta to point at the copied bytecodes. This effectively changes this RAM Method's
	   ROM Method to the copy when accessed using J9_ROM_METHOD_FROM_RAM_METHOD */

	ramMethod->bytecodes += delta;

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	if (J9_FSD_ENABLED(vm)) {
		vm->jitConfig->jitCodeBreakpointAdded(currentThread, ramMethod);
	}
#endif

	return breakpointedMethod;
}



static jvmtiError
createSingleBreakpoint(J9VMThread * currentThread, J9Method * ramMethod, IDATA location, J9JVMTIGlobalBreakpoint ** globalBreakpointPtr)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	J9JVMTIGlobalBreakpoint * globalBreakpoint;
	J9JVMTIBreakpointedMethod * breakpointedMethod;

	/* See if this method has already been breakpointed - if not, create a record for it */

	breakpointedMethod = findBreakpointedMethod(jvmtiData, ramMethod);
	if (breakpointedMethod == NULL) {
		breakpointedMethod = createBreakpointedMethod(currentThread, ramMethod);
		if (breakpointedMethod == NULL) {
			return JVMTI_ERROR_OUT_OF_MEMORY;
		}
	}
	++(breakpointedMethod->referenceCount);
	
	/* Create a new breakpoint */

	globalBreakpoint = pool_newElement(jvmtiData->breakpoints);
	if (globalBreakpoint == NULL) {
		deleteBreakpointedMethodReference(currentThread, breakpointedMethod);
		return JVMTI_ERROR_OUT_OF_MEMORY;
	}
	globalBreakpoint->referenceCount = 1;
	globalBreakpoint->flags = 0;
	globalBreakpoint->breakpointedMethod = breakpointedMethod;
	globalBreakpoint->location = location;

	/* Install the breakpoint in the copied bytecodes */

	J9_BYTECODE_START_FROM_ROM_METHOD(breakpointedMethod->copiedROMMethod)[location] = JBbreakpoint;

	*globalBreakpointPtr = globalBreakpoint;
	return JVMTI_ERROR_NONE;
}



static void
deleteBreakpointedMethodReference(J9VMThread * currentThread, J9JVMTIBreakpointedMethod * breakpointedMethod)
{
	if (--(breakpointedMethod->referenceCount) == 0) {
		J9JavaVM * vm = currentThread->javaVM;
		J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
		PORT_ACCESS_FROM_JAVAVM(vm);
		UDATA delta;
		J9Method * ramMethod;
		J9ROMMethod * copiedROMMethod;

		/* Fix the method and stacks to point to the original bytecodes */

		ramMethod = breakpointedMethod->method;
		copiedROMMethod = breakpointedMethod->copiedROMMethod;
		delta = ((UDATA) breakpointedMethod->originalROMMethod) - (UDATA) copiedROMMethod;
		fixBytecodesInAllStacks(vm, ramMethod, delta);
		ramMethod->bytecodes += delta;

		/* Free the copied method */

		j9mem_free_memory(copiedROMMethod);

		/* Free the breakpointed method */

		pool_removeElement(jvmtiData->breakpointedMethods, breakpointedMethod);

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
		if (J9_FSD_ENABLED(vm)) {
			vm->jitConfig->jitCodeBreakpointRemoved(currentThread, ramMethod);
		}
#endif
	}
}



static J9JVMTIGlobalBreakpoint *
findGlobalBreakpoint(J9JVMTIData * jvmtiData, J9Method * ramMethod, IDATA location)
{
	pool_state poolState;
	J9JVMTIGlobalBreakpoint * globalBreakpoint;

	globalBreakpoint = pool_startDo(jvmtiData->breakpoints, &poolState);
	while (globalBreakpoint != NULL) {
		if ((globalBreakpoint->breakpointedMethod->method == ramMethod) && (globalBreakpoint->location == location)) {
			break;
		}
		globalBreakpoint = pool_nextDo(&poolState);
	}
	return globalBreakpoint;
}



static UDATA
fixBytecodesFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->jitInfo == NULL)
#endif
	{
		/* If the interesting method is running in this frame, apply the delta to it's PC */

		if (walkState->method == walkState->userData1) {
			if (!IS_SPECIAL_FRAME_PC(walkState->pc)) {
				*(walkState->pcAddress) += (UDATA) walkState->userData2;
			}
		}
	}

	return J9_STACKWALK_KEEP_ITERATING;
}



static void
fixBytecodesInAllStacks(J9JavaVM * vm, J9Method * method, UDATA delta)
{
	J9VMThread * currentThread;

	currentThread = vm->mainThread;
	do {
		J9StackWalkState walkState;

		walkState.walkThread = currentThread;
		walkState.skipCount = 0;
		walkState.flags = J9_STACKWALK_ITERATE_FRAMES;
		walkState.frameWalkFunction = fixBytecodesFrameIterator;
		walkState.userData1 = method;
		walkState.userData2 = (void *) delta;
		vm->walkStackFrames(currentThread, &walkState);
	} while ((currentThread = currentThread->linkNext) != vm->mainThread);
}

jmethodID
getCurrentMethodID(J9VMThread * currentThread, J9Method * method)
{
	return (jmethodID) currentThread->javaVM->internalVMFunctions->getJNIMethodID(currentThread, method);
}



void
suspendAgentBreakpoint(J9VMThread * currentThread, J9JVMTIAgentBreakpoint * agentBreakpoint)
{
	clearGlobalBreakpoint(currentThread, agentBreakpoint->globalBreakpoint);
	agentBreakpoint->globalBreakpoint = NULL;
}



jvmtiError
installAgentBreakpoint(J9VMThread * currentThread, J9JVMTIAgentBreakpoint * agentBreakpoint)
{
	J9Method * ramMethod = ((J9JNIMethodID *) agentBreakpoint->method)->method;

	return setGlobalBreakpoint(currentThread, ramMethod, agentBreakpoint->location, &(agentBreakpoint->globalBreakpoint));
}



void
deleteAgentBreakpoint(J9VMThread * currentThread, J9JVMTIEnv * j9env, J9JVMTIAgentBreakpoint * agentBreakpoint)
{
	suspendAgentBreakpoint(currentThread, agentBreakpoint);
	pool_removeElement(j9env->breakpoints, agentBreakpoint);
}



J9JVMTIAgentBreakpoint *
allAgentBreakpointsNextDo(J9JVMTIAgentBreakpointDoState * state)
{
	J9JVMTIAgentBreakpoint * agentBreakpoint;

	agentBreakpoint = pool_nextDo(&(state->breakpointState));
	if (agentBreakpoint != NULL) {
		return agentBreakpoint;
	}

	while ((state->j9env = pool_nextDo(&(state->environmentState))) != NULL) {
		agentBreakpoint = pool_startDo(state->j9env->breakpoints, &(state->breakpointState));
		if (agentBreakpoint != NULL) {
			return agentBreakpoint;
		}
	}

	return NULL;
}



J9JVMTIAgentBreakpoint *
allAgentBreakpointsStartDo(J9JVMTIData * jvmtiData, J9JVMTIAgentBreakpointDoState * state)
{
	state->j9env = pool_startDo(jvmtiData->environments, &(state->environmentState));
	while (state->j9env != NULL) {
		/* CMVC 196966: only inspect live (not disposed) environments */
		if (0 == (state->j9env->flags & J9JVMTIENV_FLAG_DISPOSED)) {
			J9JVMTIAgentBreakpoint * agentBreakpoint;

			agentBreakpoint = pool_startDo(state->j9env->breakpoints, &(state->breakpointState));
			if (agentBreakpoint != NULL) {
				return agentBreakpoint;
			}
		}
		state->j9env = pool_nextDo(&(state->environmentState));
	}

	return NULL;
}



#if defined(J9VM_INTERP_NATIVE_SUPPORT)
J9JVMTICompileEvent *
queueCompileEvent(J9JVMTIData * jvmtiData, jmethodID methodID, void * startPC, UDATA length, void * metaData, UDATA isLoad)
{
	J9JVMTICompileEvent * compEvent = pool_newElement(jvmtiData->compileEvents);

	if (compEvent != NULL) {
		compEvent->methodID = methodID;
		compEvent->code_size = length;
		compEvent->code_addr = (const void *) startPC;
		compEvent->compile_info = (const void *) metaData;
		compEvent->isLoad = isLoad;

		J9_LINKED_LIST_ADD_LAST(jvmtiData->compileEventQueueHead, compEvent);
		omrthread_monitor_notify(jvmtiData->compileEventMutex);
	}

	return compEvent;
}

#endif /* INTERP_NATIVE_SUPPORT */

#if JAVA_SPEC_VERSION >= 19
jvmtiError
allocateTLS(J9JavaVM *vm, j9object_t thread)
{
	void *tlsArray = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
	Assert_JVMTI_notNull(thread);

	tlsArray = J9OBJECT_ADDRESS_LOAD_VM(vm, thread, vm->tlsOffset);
	if (NULL == tlsArray) {
		omrthread_monitor_enter(vm->tlsPoolMutex);
		tlsArray = J9OBJECT_ADDRESS_LOAD_VM(vm, thread, vm->tlsOffset);
		if (NULL == tlsArray) {
			/* We are the first to access the TLS array for the given thread. */
			tlsArray = pool_newElement(vm->tlsPool);
			if (NULL == tlsArray) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				J9OBJECT_ADDRESS_STORE_VM(vm, thread, vm->tlsOffset, tlsArray);
			}
		}
		omrthread_monitor_exit(vm->tlsPoolMutex);
	}

	return rc;
}
#endif /* JAVA_SPEC_VERSION >= 19 */

jvmtiError
createThreadData(J9JVMTIEnv *j9env, J9VMThread *vmThread, j9object_t thread)
{
	J9JVMTIThreadData *threadData = NULL;
	jvmtiError rc = JVMTI_ERROR_NONE;
	Assert_JVMTI_notNull(thread);

	if (0 == j9env->tlsKey) {
		omrthread_monitor_enter(j9env->threadDataPoolMutex);
		if (0 == j9env->tlsKey) {
			rc = jvmtiTLSAlloc(vmThread->javaVM, &j9env->tlsKey);
			if (JVMTI_ERROR_NONE != rc) {
				omrthread_monitor_exit(j9env->threadDataPoolMutex);
				goto exit;
			}
			goto allocate_thread_data_locked;
		}
		goto check_thread_data_locked;
	}

	threadData = jvmtiTLSGet(vmThread, thread, j9env->tlsKey);
	if (NULL == threadData) {
		omrthread_monitor_enter(j9env->threadDataPoolMutex);

check_thread_data_locked:
		threadData = jvmtiTLSGet(vmThread, thread, j9env->tlsKey);
		if (NULL == threadData) {
			/* We are the first to access the thread data for the given thread. */
allocate_thread_data_locked:
			threadData = pool_newElement(j9env->threadDataPool);
			if (NULL == threadData) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				jvmtiTLSSet(vmThread, thread, j9env->tlsKey, threadData);
			}
		}
		omrthread_monitor_exit(j9env->threadDataPoolMutex);
	}

exit:
	return rc;
}

jvmtiError
setEventNotificationMode(J9JVMTIEnv * j9env, J9VMThread * currentThread, jint mode, jint event_type, jthread event_thread, jint low, jint high)
{
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9JVMTIEventEnableMap * eventMap;
	J9VMThread * targetThread = NULL;
	J9JavaVM * vm = currentThread->javaVM;
	BOOLEAN shouldDecompileAllThreads = J9_FSD_ENABLED(vm)
			&& ((event_type == JVMTI_EVENT_METHOD_ENTRY) || (event_type == JVMTI_EVENT_METHOD_EXIT) || (event_type == JVMTI_EVENT_SINGLE_STEP));
	BOOLEAN safePointFV = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT_FV);

	ENSURE_PHASE_ONLOAD_OR_LIVE(j9env);

	if ((mode != JVMTI_ENABLE) && (mode != JVMTI_DISABLE)) {
		JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT);
	}
	if ((event_type < low) || (event_type > high)) {
		JVMTI_ERROR(JVMTI_ERROR_INVALID_EVENT_TYPE);
	}

	if (event_thread == NULL) {
		eventMap = &(j9env->globalEventEnable);
	} else {
		j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(event_thread);
		J9VMThread *vmThreadForTLS = NULL;
		rc = getVMThread(
				currentThread, event_thread, &targetThread, JVMTI_ERROR_NONE,
				J9JVMTI_GETVMTHREAD_ERROR_ON_DEAD_THREAD);
		if (rc != JVMTI_ERROR_NONE) {
			goto done;
		}
		vmThreadForTLS = targetThread;
#if JAVA_SPEC_VERSION >= 19
		rc = allocateTLS(vm, threadObject);
		if (JVMTI_ERROR_NONE != rc) {
			releaseVMThread(currentThread, targetThread, event_thread);
			goto done;
		}
		if (NULL == targetThread) {
			/* targetThread is NULL only for virtual threads, as per the assertion in getVMThread.
			 * vmThreadForTLS is only used to acquire J9JavaVM in createThreadData and jvmtiTLSGet.
			 * If targetThread is NULL, currentThread is passed to createThreadData and jvmtiTLSGet
			 * for retrieving J9JavaVM in JDK19+.
			 */
			vmThreadForTLS = currentThread;
		}
#endif /* JAVA_SPEC_VERSION >= 19 */
		rc = createThreadData(j9env, vmThreadForTLS, threadObject);
		if (JVMTI_ERROR_NONE != rc) {
			releaseVMThread(currentThread, targetThread, event_thread);
			goto done;
		}
		eventMap = &(jvmtiTLSGet(vmThreadForTLS, threadObject, j9env->tlsKey)->threadEventEnable);
	}

	/* Single step can alloc/free the bytecode table, so we need exclusive to prevent any threads from using the table */

	if (shouldDecompileAllThreads || (event_type == JVMTI_EVENT_SINGLE_STEP)) {
		if (safePointFV) {
			vm->internalVMFunctions->acquireSafePointVMAccess(currentThread);
		} else {
			vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		}
	}

	omrthread_monitor_enter(j9env->mutex);
	if (mode == JVMTI_ENABLE) {
		if (!EVENT_IS_ENABLED(event_type, eventMap)) {
			hookEvent(j9env, event_type);
			ENABLE_EVENT(event_type, eventMap);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
			if (shouldDecompileAllThreads) {
				vm->jitConfig->jitSingleStepAdded(currentThread);
			}
#endif
		}
	} else {
		if (EVENT_IS_ENABLED(event_type, eventMap)) {
			DISABLE_EVENT(event_type, eventMap);
			unhookEvent(j9env, event_type);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
			if (shouldDecompileAllThreads) {
				vm->jitConfig->jitSingleStepRemoved(currentThread);
			}
#endif
		}
	}
	omrthread_monitor_exit(j9env->mutex);

	if (shouldDecompileAllThreads || (event_type == JVMTI_EVENT_SINGLE_STEP)) {
		if (safePointFV) {
			vm->internalVMFunctions->releaseSafePointVMAccess(currentThread);
		} else {
			vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
		}
	}

	if (NULL != event_thread) {
		releaseVMThread(currentThread, targetThread, event_thread);
	}
done:
	return rc;
}

/** 
 * \brief Get a primitive type for the supplied signature. 
 * \ingroup jvmti.helpers
 * 
 * @param[in]	signature 	signature of the data
 * @param[out]	primitiveType	primitive type 
 * @return JVMTI_ERROR_NONE on success or JVMTI_ERROR_ILLEGAL_ARGUMENT if signature does not describe a primitive type
 *	
 */
jvmtiError 
getPrimitiveType(J9UTF8 *signature, jvmtiPrimitiveType *primitiveType)
{
	switch (J9UTF8_DATA(signature)[0]) {

		case 'Z':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_BOOLEAN;
			break;
		case 'B':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_BYTE;
			break;
	        case 'C':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_CHAR;
			break;
		case 'S':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_SHORT;
			break;
		case 'I':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_INT;
			break;
		case 'J':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_LONG;
			break;
		case 'F':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_FLOAT;
			break;
		case 'D':
			*primitiveType = JVMTI_PRIMITIVE_TYPE_DOUBLE;
			break;
		default:
			*primitiveType = jvmtiPrimitiveTypeEnsureWideEnum;
			return JVMTI_ERROR_ILLEGAL_ARGUMENT;
	}

	return JVMTI_ERROR_NONE;
}

/**
 * Frame iterator function for locating a possibly-inlined frame.
 *
 * Prior to the walk, walkState->userData1 must be set to JVMTI_ERROR_NO_MORE_FRAMES and
 * the skipCount must be set to the required frame depth.
 *
 * During the walk, walkState->userData1 is the state:
 *
 * 	JVMTI_ERROR_NO_MORE_FRAMES means that the requested frame has just been reached
 * 	JVMTI_ERROR_NONE means that the walk is continuing to the outer method
 *
 * After the walk is complete, walkState->userData1 contains the JVMTI error code.  If it's
 * JVMTI_ERROR_NONE, the frame was located and the following information from the located frame
 * is filled in:
 *
 *	walkState->userData2 is the inline depth
 *	walkState->userData3 is the method
 *	walkState->userData4 is the bytecodePCOffset
 *
 * @param[in] *currentThread current thread
 * @param[in] *walkState stack walk state
 *
 * @return J9_STACKWALK_KEEP_ITERATING or J9_STACKWALK_STOP_ITERATING
 */
static UDATA
findDecompileInfoFrameIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
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
		goto skip;
	}
#endif /* JAVA_SPEC_VERSION >= 20 */

	if (JVMTI_ERROR_NONE != (UDATA)walkState->userData1) {
		/* The current frame is the requested frame.  Record the inline depth for the decompiler
		 * and the method and bytecodePCOffset for the slot validator.
		 */
		walkState->userData1 = (void*)JVMTI_ERROR_NONE;
		walkState->userData2 = (void*)walkState->inlineDepth;
		walkState->userData3 = method;
		walkState->userData4 = (void*)(UDATA)walkState->bytecodePCOffset;
	}

	/* Keep walking until the outer method is reached (the inline decompiler requires that
	 * the walkState be at this point).
	 */
	if (0 == walkState->inlineDepth) {
		rc = J9_STACKWALK_STOP_ITERATING;
	}

#if JAVA_SPEC_VERSION >= 20
skip:
#endif /* JAVA_SPEC_VERSION >= 20 */
	return rc;
}

UDATA
findDecompileInfo(J9VMThread *currentThread, J9VMThread *targetThread, UDATA depth, J9StackWalkState *walkState)
{
	walkState->walkThread = targetThread;
	walkState->flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
	walkState->skipCount = depth;
	walkState->userData1 = (void *)JVMTI_ERROR_NO_MORE_FRAMES;
	walkState->frameWalkFunction = findDecompileInfoFrameIterator;
	currentThread->javaVM->walkStackFrames(currentThread, walkState);
	return (UDATA)walkState->userData1;
}

void
ensureHeapWalkable(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	/* Must be called while holding exclusive */
	Assert_JVMTI_true(currentThread->omrVMThread->exclusiveCount > 0);
	/* If heap walk is already enabled, nothing need be done */
	if (J9_ARE_NO_BITS_SET(vm->requiredDebugAttributes, J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK)) {
		J9MemoryManagerFunctions const * const mmFuncs = vm->memoryManagerFunctions;
		vm->requiredDebugAttributes |= J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		/* J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT allows the GC to run while the current thread is holding
		 * exclusive VM access.
		 */
		mmFuncs->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT);
		if (J9_GC_POLICY_METRONOME == vm->gcPolicy) {
			/* In metronome, the previous GC call may have only finished the current cycle.
			 * Call again to ensure a full GC takes place.					 */
			mmFuncs->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT);
		}
	}
}

#if JAVA_SPEC_VERSION >= 19
static void
tlsNullFinalizer(void *entry)
{
	/* do nothing */
}
#endif /* JAVA_SPEC_VERSION >= 19 */

IDATA
jvmtiTLSAlloc(J9JavaVM *vm, UDATA *handle)
{
#if JAVA_SPEC_VERSION >= 19
	return jvmtiTLSAllocWithFinalizer(vm, handle, tlsNullFinalizer);
#else /* JAVA_SPEC_VERSION >= 19 */
	return omrthread_tls_alloc(handle);
#endif /* JAVA_SPEC_VERSION >= 19 */
}

#if JAVA_SPEC_VERSION >= 19
IDATA
jvmtiTLSAllocWithFinalizer(J9JavaVM *vm, UDATA *handle, j9_tls_finalizer_t finalizer)
{
	IDATA i = 0;

	Assert_JVMTI_notNull(finalizer);
	*handle = 0;

	omrthread_monitor_enter(vm->tlsFinalizersMutex);
	for (i = 0; i < J9JVMTI_MAX_TLS_KEYS; i++) {
		if (NULL == vm->tlsFinalizers[i]) {
			*handle = i + 1;
			vm->tlsFinalizers[i] = finalizer;
			break;
		}
	}
	omrthread_monitor_exit(vm->tlsFinalizersMutex);

	return i < J9JVMTI_MAX_TLS_KEYS ? 0 : -1;
}
#endif /* JAVA_SPEC_VERSION >= 19 */

IDATA
jvmtiTLSFree(J9JavaVM *vm, UDATA key)
{
#if JAVA_SPEC_VERSION >= 19
	pool_state state;
	J9JVMTIThreadData **each = NULL;

	omrthread_monitor_enter(vm->tlsPoolMutex);
	each = pool_startDo(vm->tlsPool, &state);
	while (NULL != each) {
		each[key - 1] = NULL;
		each = pool_nextDo(&state);
	}
	omrthread_monitor_exit(vm->tlsPoolMutex);

	omrthread_monitor_enter(vm->tlsFinalizersMutex);
	vm->tlsFinalizers[key - 1] = NULL;
	omrthread_monitor_exit(vm->tlsFinalizersMutex);

	return 0;
#else /* JAVA_SPEC_VERSION >= 19 */
	return omrthread_tls_free(key);
#endif /* JAVA_SPEC_VERSION >= 19 */
}

IDATA
jvmtiTLSSet(J9VMThread *vmThread, j9object_t thread, UDATA key, J9JVMTIThreadData *value)
{
#if JAVA_SPEC_VERSION >= 19
	J9JavaVM *vm = vmThread->javaVM;
	J9JVMTIThreadData **data = NULL;
	Assert_JVMTI_notNull(thread);

	data = (J9JVMTIThreadData **)J9OBJECT_ADDRESS_LOAD_VM(vm, thread, vm->tlsOffset);
	Assert_JVMTI_notNull(data);
	data[key - 1] = value;

	return 0;
#else /* JAVA_SPEC_VERSION >= 19 */
	return omrthread_tls_set(vmThread->osThread, key, (void *)(value));
#endif /* JAVA_SPEC_VERSION >= 19 */
}

J9JVMTIThreadData *
jvmtiTLSGet(J9VMThread *vmThread, j9object_t thread, UDATA key)
{
#if JAVA_SPEC_VERSION >= 19
	J9JavaVM *vm = vmThread->javaVM;
	J9JVMTIThreadData **data = NULL;
	Assert_JVMTI_notNull(thread);

	if (0 == key) {
		return NULL;
	}

	data = (J9JVMTIThreadData **)J9OBJECT_ADDRESS_LOAD_VM(vm, thread, vm->tlsOffset);
	Assert_JVMTI_notNull(data);

	return data[key - 1];
#else /* JAVA_SPEC_VERSION >= 19 */
	J9JVMTIThreadData *data = NULL;
	if (0 != key) {
		data = (J9JVMTIThreadData *)omrthread_tls_get(vmThread->osThread, key);
	}
	return data;
#endif /* JAVA_SPEC_VERSION >= 19 */
}

#if JAVA_SPEC_VERSION >= 20
UDATA
genericFrameIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	J9Method *method = walkState->method;
	J9ROMMethod *romMethod = NULL;
	U_32 extendedModifiers = 0;

	/* walkState->method can never be NULL since the J9_STACKWALK_VISIBLE_ONLY flag is set. */
	Assert_JVMTI_true(NULL != method);

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	extendedModifiers = getExtendedModifiersDataFromROMMethod(romMethod);

	if (J9_ARE_ANY_BITS_SET(extendedModifiers, CFR_METHOD_EXT_JVMTIMOUNTTRANSITION_ANNOTATION)) {
		/* The number of frames skipped is stored in userData1. */
		UDATA framesSkipped = (UDATA)walkState->userData1;
		walkState->userData1 = (void *)(framesSkipped + 1);
	}

	return J9_STACKWALK_KEEP_ITERATING;
}
#endif /* JAVA_SPEC_VERSION >= 20 */

UDATA
genericWalkStackFramesHelper(J9VMThread *currentThread, J9VMThread *targetThread, j9object_t threadObject, J9StackWalkState *walkState)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA rc = J9_STACKWALK_RC_NONE;

#if JAVA_SPEC_VERSION >= 19
	if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
		if (NULL != targetThread) {
			walkState->walkThread = targetThread;
			rc = vm->walkStackFrames(currentThread, walkState);
		} else {
			j9object_t contObject = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObject);
			J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, contObject);
			vm->internalVMFunctions->walkContinuationStackFrames(currentThread, continuation, threadObject, walkState);
		}
	} else
#endif /* JAVA_SPEC_VERSION >= 19 */
	{
#if JAVA_SPEC_VERSION >= 19
		J9VMContinuation *currentContinuation = targetThread->currentContinuation;
		if (NULL != currentContinuation) {
			rc = vm->internalVMFunctions->walkContinuationStackFrames(currentThread, currentContinuation, threadObject, walkState);
		} else
#endif /* JAVA_SPEC_VERSION >= 19 */
		{
			walkState->walkThread = targetThread;
			rc = vm->walkStackFrames(currentThread, walkState);
		}
	}

	return rc;
}

#if JAVA_SPEC_VERSION >= 19
J9VMContinuation *
getJ9VMContinuationToWalk(J9VMThread *currentThread, J9VMThread *targetThread, j9object_t threadObject)
{
	J9VMContinuation *continuation = NULL;
	if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObject)) {
		if (NULL == targetThread) {
			/* An unmounted virtual thread will have a valid J9VMContinuation. */
			j9object_t contObject = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObject);
			continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, contObject);
		}
	} else {
		/* If a virtual thread is not mounted and a continuation is being run directly on a
		 * platform thread, then the continuation is considered part of the platform thread,
		 * and the fields should not be swapped with J9VMContinuation->currentContinuation
		 * to get the platform thread details. This behaviour matches the RI.
		 */
		if (threadObject != targetThread->threadObject) {
			/* Get platform thread details from J9VMThread->currentContinuation IFF it
			 * is not mounted.
			 */
			continuation = targetThread->currentContinuation;
		}
	}
	return continuation;
}
#endif /* JAVA_SPEC_VERSION >= 19 */
