/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"
#include <string.h>

jvmtiError JNICALL
jvmtiSetEventCallbacks(jvmtiEnv* env,
	const jvmtiEventCallbacks* callbacks,
	jint size_of_callbacks)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	jint i;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetEventCallbacks_Entry(env);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	/* Verify that the size is legal for the current JVMTI version, if callbacks is not NULL (NULL means remove all callbacks) */

	if (callbacks == NULL) {
		size_of_callbacks = 0;
	} else {
		/* Value must be non-negative and a multiple of the platform pointer size */

		ENSURE_NON_NEGATIVE(size_of_callbacks);
		if ((size_of_callbacks % sizeof(void *)) != 0) {
			JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT);
		}

		/* Assume jvmtiEventCallbacks always reflects the largest possible callback table (most current version) */

		if (size_of_callbacks > sizeof(jvmtiEventCallbacks)) {
			JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT);
		}

		size_of_callbacks /= sizeof(void *);
	}

	/* Do the callback change atomically */

	omrthread_monitor_enter(j9env->mutex);

	/*	Copy/zero must take place a pointer at a time to avoid corrupting the function pointers (memcpy may copy byte-wise) */

	for (i = 0; i < size_of_callbacks; ++i) {
		((void **) &(j9env->callbacks))[i] = J9_COMPATIBLE_FUNCTION_POINTER( ((void **) callbacks)[i] );
	}
	for (i = size_of_callbacks; i < (sizeof(jvmtiEventCallbacks) / sizeof(void *)); ++i) {
		((void **) &(j9env->callbacks))[i] = NULL;
	}

	omrthread_monitor_exit(j9env->mutex);
	rc = JVMTI_ERROR_NONE;

done:
	TRACE_JVMTI_RETURN(jvmtiSetEventCallbacks);
}


jvmtiError JNICALL
jvmtiSetEventNotificationMode(jvmtiEnv* env,
	jvmtiEventMode mode,
	jvmtiEvent event_type,
	jthread event_thread,
	...)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetEventNotificationMode_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		/* Ensure the required capabilities are enabled for the event if the event is being enabled */

		if (mode == JVMTI_ENABLE) {
			switch(event_type) {
				case JVMTI_EVENT_FIELD_MODIFICATION:
					ENSURE_CAPABILITY(env, can_generate_field_modification_events);
					break;
 
				case JVMTI_EVENT_FIELD_ACCESS:
					ENSURE_CAPABILITY(env, can_generate_field_access_events);
					break;
 
				case JVMTI_EVENT_SINGLE_STEP:
					ENSURE_CAPABILITY(env, can_generate_single_step_events);
					break;
 
				case JVMTI_EVENT_EXCEPTION:
				case JVMTI_EVENT_EXCEPTION_CATCH:
					ENSURE_CAPABILITY(env, can_generate_exception_events);
					break;
 
				case JVMTI_EVENT_FRAME_POP:
					ENSURE_CAPABILITY(env, can_generate_frame_pop_events);
					break;
 
				case JVMTI_EVENT_BREAKPOINT:
					ENSURE_CAPABILITY(env, can_generate_breakpoint_events);
					break;
 
				case JVMTI_EVENT_METHOD_ENTRY:
					ENSURE_CAPABILITY(env, can_generate_method_entry_events);
					break;
 
				case JVMTI_EVENT_METHOD_EXIT:
					ENSURE_CAPABILITY(env, can_generate_method_exit_events);
					break;
 
				case JVMTI_EVENT_COMPILED_METHOD_LOAD:
				case JVMTI_EVENT_COMPILED_METHOD_UNLOAD:
					ENSURE_CAPABILITY(env, can_generate_compiled_method_load_events);
					break;
 
	 			case JVMTI_EVENT_MONITOR_CONTENDED_ENTER:
				case JVMTI_EVENT_MONITOR_CONTENDED_ENTERED:
				case JVMTI_EVENT_MONITOR_WAIT:
				case JVMTI_EVENT_MONITOR_WAITED:
					ENSURE_CAPABILITY(env, can_generate_monitor_events);
					break;

				case  JVMTI_EVENT_VM_OBJECT_ALLOC:
					ENSURE_CAPABILITY(env, can_generate_vm_object_alloc_events);
					break;
 
				case JVMTI_EVENT_NATIVE_METHOD_BIND:
					ENSURE_CAPABILITY(env, can_generate_native_method_bind_events);
					break;

				case JVMTI_EVENT_GARBAGE_COLLECTION_START:
				case JVMTI_EVENT_GARBAGE_COLLECTION_FINISH:
					ENSURE_CAPABILITY(env, can_generate_garbage_collection_events);
					break;
 
				case JVMTI_EVENT_OBJECT_FREE:
 					ENSURE_CAPABILITY(env, can_generate_object_free_events);
					break;
					
				default:
					break;
 			}
		}

		/* Disallow certain events at the thread level */

		switch(event_type) {
			case JVMTI_EVENT_VM_INIT:
			case JVMTI_EVENT_VM_START:
			case JVMTI_EVENT_VM_DEATH:
			case JVMTI_EVENT_THREAD_START:
			case JVMTI_EVENT_COMPILED_METHOD_LOAD:
			case JVMTI_EVENT_COMPILED_METHOD_UNLOAD:
			case JVMTI_EVENT_DYNAMIC_CODE_GENERATED:
			case JVMTI_EVENT_DATA_DUMP_REQUEST:
				if (event_thread != NULL) {
					JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT);
				}
				break;
			default:
				break;
		}

		rc = setEventNotificationMode(j9env, currentThread, mode, event_type, event_thread, J9JVMTI_LOWEST_EVENT, J9JVMTI_HIGHEST_EVENT);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSetEventNotificationMode);
}


jvmtiError JNICALL
jvmtiGenerateEvents(jvmtiEnv* env,
	jvmtiEvent event_type)
{
	jvmtiError rc = JVMTI_ERROR_NONE;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_ENV(env);
#endif

	Trc_JVMTI_jvmtiGenerateEvents_Entry(env);

	ENSURE_PHASE_LIVE(env);

	if ((event_type < J9JVMTI_LOWEST_EVENT) || (event_type > J9JVMTI_HIGHEST_EVENT)) {
		JVMTI_ERROR(JVMTI_ERROR_INVALID_EVENT_TYPE);
	} else if (event_type == JVMTI_EVENT_COMPILED_METHOD_LOAD) {
		ENSURE_CAPABILITY(env, can_generate_compiled_method_load_events);
	} else if (event_type != JVMTI_EVENT_DYNAMIC_CODE_GENERATED) {
		JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT);
	}

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	if (jvmtiData->compileEventThread != NULL) {
		J9JavaVM * vm = JAVAVM_FROM_ENV(env);
		J9VMThread * currentThread;

		rc = getCurrentVMThread(vm, &currentThread);
		if (rc == JVMTI_ERROR_NONE) {
			vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
			vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

			if (event_type == JVMTI_EVENT_COMPILED_METHOD_LOAD) {
#ifdef J9VM_INTERP_NATIVE_SUPPORT
				if ((vm->jitConfig != NULL) && (vm->jitConfig->jitReportDynamicCodeLoadEvents != NULL)) {
					vm->jitConfig->jitReportDynamicCodeLoadEvents(currentThread);
				}
#endif
			} else {
#ifdef J9VM_NEEDS_JNI_REDIRECTION
				J9ClassLoader * classLoader;
				J9JNIRedirectionBlock * block;
				J9ClassLoaderWalkState walkState;

				classLoader = vm->internalVMFunctions->allClassLoadersStartDo(&walkState, vm, J9CLASSLOADERWALK_INCLUDE_DEAD);
				while (NULL != classLoader) {
					block = classLoader->jniRedirectionBlocks;
					while (NULL != block) {
						TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(vm->hookInterface, currentThread, NULL, block, J9JNIREDIRECT_BLOCK_SIZE, "JNI trampoline area", NULL);
						block = block->next;
					}
					classLoader = vm->internalVMFunctions->allClassLoadersNextDo(&walkState);
				}
				vm->internalVMFunctions->allClassLoadersEndDo(&walkState);
#endif
			}

			vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
			vm->internalVMFunctions->internalReleaseVMAccess(currentThread);

			/* Wait for the events to be reported */

			omrthread_monitor_enter(jvmtiData->compileEventMutex);
			while (jvmtiData->compileEventQueueHead != NULL) {
				omrthread_monitor_wait(jvmtiData->compileEventMutex);
			}
			omrthread_monitor_exit(jvmtiData->compileEventMutex);
		}
	}
#endif

done:
	TRACE_JVMTI_RETURN(jvmtiGenerateEvents);
}



