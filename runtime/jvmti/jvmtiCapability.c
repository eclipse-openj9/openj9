/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"
#include "mmomrhook.h"

typedef IDATA (* J9EventHookFunction)(J9JVMTIEnv * j9env, jint event);

#ifdef DEBUG
static void dumpCapabilities (J9JavaVM * vm, const jvmtiCapabilities *capabilities, const char * caption);
#endif
static IDATA mapCapabilitiesToEvents (J9JVMTIEnv * j9env, jvmtiCapabilities * capabilities, J9EventHookFunction eventHookFunction);



#ifdef DEBUG
static void
dumpCapabilities(J9JavaVM * vm, const jvmtiCapabilities *capabilities, const char * caption)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_printf(PORTLIB, "%s\n", caption);

#define PRINT_CAPABILITY(capability) if (capabilities->capability) j9tty_printf(PORTLIB, "\t%s\n", J9_STR(capability));

	PRINT_CAPABILITY(can_tag_objects);
	PRINT_CAPABILITY(can_generate_field_modification_events);
	PRINT_CAPABILITY(can_generate_field_access_events);
	PRINT_CAPABILITY(can_get_bytecodes);
	PRINT_CAPABILITY(can_get_synthetic_attribute);
	PRINT_CAPABILITY(can_get_owned_monitor_info);
	PRINT_CAPABILITY(can_get_current_contended_monitor);
	PRINT_CAPABILITY(can_get_monitor_info);
	PRINT_CAPABILITY(can_pop_frame);
	PRINT_CAPABILITY(can_redefine_classes);
	PRINT_CAPABILITY(can_signal_thread);
	PRINT_CAPABILITY(can_get_source_file_name);
	PRINT_CAPABILITY(can_get_line_numbers);
	PRINT_CAPABILITY(can_get_source_debug_extension);
	PRINT_CAPABILITY(can_access_local_variables);
	PRINT_CAPABILITY(can_maintain_original_method_order);
	PRINT_CAPABILITY(can_generate_single_step_events);
	PRINT_CAPABILITY(can_generate_exception_events);
	PRINT_CAPABILITY(can_generate_frame_pop_events);
	PRINT_CAPABILITY(can_generate_breakpoint_events);
	PRINT_CAPABILITY(can_suspend);
	PRINT_CAPABILITY(can_redefine_any_class);
	PRINT_CAPABILITY(can_get_current_thread_cpu_time);
	PRINT_CAPABILITY(can_get_thread_cpu_time);
	PRINT_CAPABILITY(can_generate_method_entry_events);
	PRINT_CAPABILITY(can_generate_method_exit_events);
	PRINT_CAPABILITY(can_generate_all_class_hook_events);
	PRINT_CAPABILITY(can_generate_compiled_method_load_events);
	PRINT_CAPABILITY(can_generate_monitor_events);
	PRINT_CAPABILITY(can_generate_vm_object_alloc_events);
	PRINT_CAPABILITY(can_generate_native_method_bind_events);
	PRINT_CAPABILITY(can_generate_garbage_collection_events);
	PRINT_CAPABILITY(can_generate_object_free_events);

	/* JVMTI 1.1 */

	PRINT_CAPABILITY(can_force_early_return);
	PRINT_CAPABILITY(can_get_owned_monitor_stack_depth_info);
	PRINT_CAPABILITY(can_get_constant_pool);
	PRINT_CAPABILITY(can_set_native_method_prefix);
	PRINT_CAPABILITY(can_retransform_classes);
	PRINT_CAPABILITY(can_retransform_any_class);
	PRINT_CAPABILITY(can_generate_resource_exhaustion_heap_events);
	PRINT_CAPABILITY(can_generate_resource_exhaustion_threads_events);

	/* JVMTI 9.0 */
#if JAVA_SPEC_VERSION >= 9
	PRINT_CAPABILITY(can_generate_early_vmstart);
	PRINT_CAPABILITY(can_generate_early_class_hook_events);
#endif /* JAVA_SPEC_VERSION >= 9 */

	/* JVMTI 11 */
#if JAVA_SPEC_VERSION >= 11
	PRINT_CAPABILITY(can_generate_sampled_object_alloc_events);
#endif /* JAVA_SPEC_VERSION >= 11 */
#undef PRINT_CAPABILITY
}

#endif

jvmtiError JNICALL
jvmtiGetPotentialCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	jvmtiCapabilities rv_capabilities;
	J9HookInterface ** vmHook = vm->internalVMFunctions->getVMHookInterface(vm);
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetPotentialCapabilities_Entry(env);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(capabilities_ptr);

	memset(&rv_capabilities, 0, sizeof(jvmtiCapabilities));

	/* Get the JVMTI mutex to ensure to prevent multiple agents acquiring capabilities that can only be held by one agent at a time */

	omrthread_monitor_enter(jvmtiData->mutex);

	if (J9_ARE_ALL_BITS_SET(vm->requiredDebugAttributes, J9VM_DEBUG_ATTRIBUTE_MAINTAIN_ORIGINAL_METHOD_ORDER)
	|| (JVMTI_PHASE_ONLOAD == jvmtiData->phase)
	) {
		rv_capabilities.can_maintain_original_method_order = 1;
	}

	rv_capabilities.can_generate_all_class_hook_events = 1;

	rv_capabilities.can_get_bytecodes = 1;

	rv_capabilities.can_get_constant_pool = 1;

	rv_capabilities.can_get_synthetic_attribute = 1;

	rv_capabilities.can_signal_thread = 1;

	rv_capabilities.can_suspend = 1;

	if (isEventHookable(j9env, JVMTI_EVENT_METHOD_ENTRY)) {
		rv_capabilities.can_generate_method_entry_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_GARBAGE_COLLECTION_START) &&
		isEventHookable(j9env, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH)
	) {
		rv_capabilities.can_generate_garbage_collection_events = 1;		
	}

	if (isEventHookable(j9env, JVMTI_EVENT_OBJECT_FREE)) {
		rv_capabilities.can_generate_object_free_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_SINGLE_STEP)) {
		rv_capabilities.can_generate_single_step_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_FIELD_MODIFICATION)) {
		rv_capabilities.can_generate_field_modification_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_FIELD_ACCESS)) {
		rv_capabilities.can_generate_field_access_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_VM_OBJECT_ALLOC)) {
		rv_capabilities.can_generate_vm_object_alloc_events = 1;
	}

#if JAVA_SPEC_VERSION >= 11
	if (isEventHookable(j9env, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC)) {
		rv_capabilities.can_generate_sampled_object_alloc_events = 1;
	}
#endif /* JAVA_SPEC_VERSION >= 11 */

	if (isEventHookable(j9env, JVMTI_EVENT_NATIVE_METHOD_BIND)) {
		rv_capabilities.can_generate_native_method_bind_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_COMPILED_METHOD_LOAD) &&
		isEventHookable(j9env, JVMTI_EVENT_COMPILED_METHOD_UNLOAD)
	) {
		rv_capabilities.can_generate_compiled_method_load_events = 1;
	}

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_POP_FRAMES_INTERRUPT)) {
		rv_capabilities.can_pop_frame = 1;
		rv_capabilities.can_force_early_return = 1;
	}

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES) ||
		(vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_CAN_REDEFINE_CLASSES)
	) {
		rv_capabilities.can_redefine_classes = 1;
	}

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES) ||
		(vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_LINE_NUMBER_TABLE)
	) {
		rv_capabilities.can_get_line_numbers = 1;
	}

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES) ||
		(vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_SOURCE_FILE)
	) {
		rv_capabilities.can_get_source_file_name = 1;
	}

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES) ||
		(vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_CAN_ACCESS_LOCALS)
	) {
		rv_capabilities.can_access_local_variables = 1;
	}

	rv_capabilities.can_tag_objects = 1;

#ifdef J9VM_OPT_DEBUG_JSR45_SUPPORT
	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES) ||
		(vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_SOURCE_DEBUG_EXTENSION)
	) {
		rv_capabilities.can_get_source_debug_extension = 1;
	}
#endif

	if (isEventHookable(j9env, JVMTI_EVENT_BREAKPOINT)) {
		rv_capabilities.can_generate_breakpoint_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_EXCEPTION) &&
		isEventHookable(j9env, JVMTI_EVENT_EXCEPTION_CATCH)
	) {
		rv_capabilities.can_generate_exception_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_FRAME_POP)) {
		rv_capabilities.can_generate_frame_pop_events = 1;
	}

	if (isEventHookable(j9env, JVMTI_EVENT_METHOD_EXIT)) {
		rv_capabilities.can_generate_method_exit_events = 1;
	}

	if (omrthread_get_self_cpu_time(omrthread_self()) != -1) {
		rv_capabilities.can_get_current_thread_cpu_time = 1;
	}

	if (omrthread_get_cpu_time(omrthread_self()) != -1 ) {
		rv_capabilities.can_get_thread_cpu_time = 1;
	}

	rv_capabilities.can_get_owned_monitor_info = 1;

	rv_capabilities.can_get_current_contended_monitor = 1;

	rv_capabilities.can_get_monitor_info = 1;


	if (isEventHookable(j9env, JVMTI_EVENT_MONITOR_CONTENDED_ENTER) &&
		isEventHookable(j9env, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED) &&
		isEventHookable(j9env, JVMTI_EVENT_MONITOR_WAIT) &&
		isEventHookable(j9env, JVMTI_EVENT_MONITOR_WAITED)
	) {
		rv_capabilities.can_generate_monitor_events = 1;
	}

	rv_capabilities.can_get_owned_monitor_stack_depth_info = 1;

	if (((j9env->flags & (J9JVMTIENV_FLAG_CLASS_LOAD_HOOK_EVER_ENABLED | J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE)) != J9JVMTIENV_FLAG_CLASS_LOAD_HOOK_EVER_ENABLED) &&
		((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES) || (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_ALLOW_RETRANSFORM))
	) {
		rv_capabilities.can_retransform_classes = 1;	
	}

	rv_capabilities.can_set_native_method_prefix = 1;

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_RESOURCE_EXHAUSTED)) {
		rv_capabilities.can_generate_resource_exhaustion_threads_events = 1;
	}

	if ((*vmHook)->J9HookIsEnabled(vmHook, J9HOOK_VM_RESOURCE_EXHAUSTED)) {
		rv_capabilities.can_generate_resource_exhaustion_heap_events = 1;
	}

#if JAVA_SPEC_VERSION >= 9
	if (JVMTI_PHASE_ONLOAD == jvmtiData->phase) {
		rv_capabilities.can_generate_early_vmstart = 1;
		rv_capabilities.can_generate_early_class_hook_events = 1;
	}
#endif /* JAVA_SPEC_VERSION >= 9 */

	rc = JVMTI_ERROR_NONE;
	omrthread_monitor_exit(jvmtiData->mutex);
done:

	if (NULL != capabilities_ptr) {
		*capabilities_ptr = rv_capabilities;
	}
	TRACE_JVMTI_RETURN(jvmtiGetPotentialCapabilities);
}


jvmtiError JNICALL
jvmtiAddCapabilities(jvmtiEnv* env,
	const jvmtiCapabilities* capabilities_ptr)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	jvmtiCapabilities potentialCapabilities;
	jvmtiCapabilities newCapabilities;
	UDATA i = 0;
	jvmtiError rc = JVMTI_ERROR_NOT_AVAILABLE;
	J9VMThread *currentThread = NULL;

	Trc_JVMTI_jvmtiAddCapabilities_Entry(env);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(capabilities_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		rc = JVMTI_ERROR_NOT_AVAILABLE;
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		/* Get the JVMTI mutex to ensure to prevent multiple agents acquiring capabilities that can only be held by one agent at a time */

		omrthread_monitor_enter(jvmtiData->mutex);

		/* Get the current set of potential capabilities */

		jvmtiGetPotentialCapabilities(env, &potentialCapabilities);

		/* Verify the requested capabilities */

		for (i = 0; i < sizeof(jvmtiCapabilities); ++i) {
			U_8 byte = ((U_8 *) capabilities_ptr)[i];

			/* Mask out any currently-owned capabilities */

			byte &= ~(((U_8 *) &(j9env->capabilities))[i]);

			/* Make sure all of the requested capabilities are available */

			if ((byte & ~(((U_8 *) &potentialCapabilities)[i])) != 0) {
				goto fail;
			}

			((U_8 *) &newCapabilities)[i] = byte;
		}

		/* can't request original method order after onLoad phase */

		if (newCapabilities.can_maintain_original_method_order) {
			if (J9_ARE_NO_BITS_SET(vm->requiredDebugAttributes, J9VM_DEBUG_ATTRIBUTE_MAINTAIN_ORIGINAL_METHOD_ORDER)) {
				if (JVMTI_PHASE_ONLOAD == jvmtiData->phase) {
					/* turn off ROMClassMethodSorting */
					Trc_JVMTI_jvmtiAddCapabilities_turnOfRomMethodSorting();
					vm->romMethodSortThreshold = UDATA_MAX;
					vm->requiredDebugAttributes |= J9VM_DEBUG_ATTRIBUTE_MAINTAIN_ORIGINAL_METHOD_ORDER;
				} else {
					rc = JVMTI_ERROR_NOT_AVAILABLE;
					goto fail;
				}
			}
		}

#if JAVA_SPEC_VERSION >= 11
		if (newCapabilities.can_generate_sampled_object_alloc_events) {
			if (J9_ARE_ANY_BITS_SET(jvmtiData->flags, J9JVMTI_FLAG_SAMPLED_OBJECT_ALLOC_ENABLED)) {
				rc = JVMTI_ERROR_NOT_AVAILABLE;
				goto fail;
			}

			/* Initial sampling interval is MM_GCExtensions::oolObjectSamplingBytesGranularity which is 16M by default
			 * or set by command line option -Xgc:allocationSamplingGranularity.
			 * Set it to 512KB which is default sampling interval as per JEP 331 specification.
			 */
			vm->memoryManagerFunctions->j9gc_set_allocation_sampling_interval(currentThread, 512 * 1024);
			jvmtiData->flags |= J9JVMTI_FLAG_SAMPLED_OBJECT_ALLOC_ENABLED;
		}
#endif /* JAVA_SPEC_VERSION >= 11 */

		/* Always consider can_generate_compiled_method_load_events to be a newly-requested capability
		 * so that hookNonEventCapabilities will fork the event thread if need be.
		 */
		if (capabilities_ptr->can_generate_compiled_method_load_events) {
			newCapabilities.can_generate_compiled_method_load_events = 1;
		}

		/* Reserve hooks for any events now allowed by the new capabilities */

		if (mapCapabilitiesToEvents(j9env, &newCapabilities, reserveEvent) != 0) {
			goto fail;
		}

		/* Handle non-event hooks */

		if (hookNonEventCapabilities(j9env, &newCapabilities) != 0) {
			goto fail;
		}

		/* All capabilities acquired successfully */

		for (i = 0; i < sizeof(jvmtiCapabilities); ++i) {
			((U_8 *) &(j9env->capabilities))[i] |= ((U_8 *) &newCapabilities)[i];
		}
		rc = JVMTI_ERROR_NONE;

fail:
#ifdef DEBUG
		if (rc != JVMTI_ERROR_NONE) {
			dumpCapabilities(vm, capabilities_ptr, "Requested capabilities");
			dumpCapabilities(vm, &potentialCapabilities, "Potential capabilities");
		}
#endif
		omrthread_monitor_exit(jvmtiData->mutex);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}
done:
	/* If can_generate_compiled_method_load_events is being requested, the event reporting thread
	 * has been started but may not be successfully attached to the VM yet.  Wait for the thread to
	 * indicate that it's completely ready before returning. If the VM attach fails, return an error.
	 *
	 * Waiting for the thread must take place while not holding jvmtiData->mutex to avoid deadlock.
	 */
	if (capabilities_ptr->can_generate_compiled_method_load_events) {
		if (JVMTI_ERROR_NONE == rc) {
			if (J9JVMTI_COMPILE_EVENT_THREAD_STATE_NEW == jvmtiData->compileEventThreadState) {
				omrthread_monitor_enter(jvmtiData->compileEventMutex);
				while (J9JVMTI_COMPILE_EVENT_THREAD_STATE_NEW == jvmtiData->compileEventThreadState) {
					omrthread_monitor_wait(jvmtiData->compileEventMutex);
				}
				omrthread_monitor_exit(jvmtiData->compileEventMutex);
			}
			if (J9JVMTI_COMPILE_EVENT_THREAD_STATE_ALIVE != jvmtiData->compileEventThreadState) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				omrthread_monitor_enter(jvmtiData->mutex);
				j9env->capabilities.can_generate_compiled_method_load_events = 0;
				omrthread_monitor_exit(jvmtiData->mutex);
			}
		}
	}
	TRACE_JVMTI_RETURN(jvmtiAddCapabilities);	
}


jvmtiError JNICALL
jvmtiRelinquishCapabilities(jvmtiEnv* env,
	const jvmtiCapabilities* capabilities_ptr)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	jvmtiCapabilities removedCapabilities;
	UDATA i = 0;
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiRelinquishCapabilities_Entry(env);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(capabilities_ptr);

	/* Get the JVMTI mutex to ensure to prevent multiple agents releasing capabilities that can only be held by one agent at a time */
	/* Also prevents multiple threads from releasing capabilities in the same agent */

	omrthread_monitor_enter(jvmtiData->mutex);

#if JAVA_SPEC_VERSION >= 11
		if (capabilities_ptr->can_generate_sampled_object_alloc_events) {
			/* The default sampling interval is not changed. */
			jvmtiData->flags &= ~J9JVMTI_FLAG_SAMPLED_OBJECT_ALLOC_ENABLED;
		}
#endif /* JAVA_SPEC_VERSION >= 11 */

	/* Verify the requested capabilities */

	for (i = 0; i < sizeof(jvmtiCapabilities); ++i) {
		U_8 byte = ((U_8 *) capabilities_ptr)[i];

		/* Mask out any unowned capabilities (this also takes care of masking for the environment version) */

		byte &= ((U_8 *) &(j9env->capabilities))[i];
		((U_8 *) &removedCapabilities)[i] = byte;

		/* Mark the capabilities as unowned in this environment */

		((U_8 *) &(j9env->capabilities))[i] &= ~byte;
	}

	omrthread_monitor_exit(jvmtiData->mutex);

done:
	TRACE_JVMTI_RETURN(jvmtiRelinquishCapabilities);
}


jvmtiError JNICALL
jvmtiGetCapabilities(jvmtiEnv* env,
	jvmtiCapabilities* capabilities_ptr)
{
	jvmtiError rc;
	Trc_JVMTI_jvmtiGetCapabilities_Entry(env);

	ENSURE_NON_NULL(capabilities_ptr);

	*capabilities_ptr = ((J9JVMTIEnv *) env)->capabilities;
	rc = JVMTI_ERROR_NONE;

done:
	TRACE_JVMTI_RETURN(jvmtiGetCapabilities);
}


static IDATA
mapCapabilitiesToEvents(J9JVMTIEnv * j9env, jvmtiCapabilities * capabilities, J9EventHookFunction eventHookFunction)
{
	IDATA rc = 0;

	if (capabilities->can_generate_single_step_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_SINGLE_STEP);
	}

	if (capabilities->can_generate_breakpoint_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_BREAKPOINT);
	}

	if (capabilities->can_generate_field_access_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_FIELD_ACCESS);
	}

	if (capabilities->can_generate_field_modification_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_FIELD_MODIFICATION);
	}

	if (capabilities->can_generate_frame_pop_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_FRAME_POP);
	}

	if (capabilities->can_generate_method_entry_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_METHOD_ENTRY);
	}

	if (capabilities->can_generate_method_exit_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_METHOD_EXIT);
	}

	if (capabilities->can_generate_native_method_bind_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_NATIVE_METHOD_BIND);
	}

	if (capabilities->can_generate_exception_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_EXCEPTION);
		rc |= eventHookFunction(j9env, JVMTI_EVENT_EXCEPTION_CATCH);
	}

	if (capabilities->can_generate_compiled_method_load_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_COMPILED_METHOD_LOAD);
		rc |= eventHookFunction(j9env, JVMTI_EVENT_COMPILED_METHOD_UNLOAD);
	}

	if (capabilities->can_generate_monitor_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_MONITOR_CONTENDED_ENTER);
		rc |= eventHookFunction(j9env, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED);
		rc |= eventHookFunction(j9env, JVMTI_EVENT_MONITOR_WAIT);
		rc |= eventHookFunction(j9env, JVMTI_EVENT_MONITOR_WAITED);
	}

	if (capabilities->can_generate_vm_object_alloc_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_VM_OBJECT_ALLOC);
	}

#if JAVA_SPEC_VERSION >= 11
	if (capabilities->can_generate_sampled_object_alloc_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC);
	}
#endif /* JAVA_SPEC_VERSION >= 11 */

	if (capabilities->can_generate_object_free_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_OBJECT_FREE);
	}

	if (capabilities->can_generate_garbage_collection_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_GARBAGE_COLLECTION_START);
		rc |= eventHookFunction(j9env, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH);
	}

	if (capabilities->can_generate_resource_exhaustion_heap_events ||
		capabilities->can_generate_resource_exhaustion_threads_events) {
		rc |= eventHookFunction(j9env, JVMTI_EVENT_RESOURCE_EXHAUSTED);	
	}

	return rc;
}


