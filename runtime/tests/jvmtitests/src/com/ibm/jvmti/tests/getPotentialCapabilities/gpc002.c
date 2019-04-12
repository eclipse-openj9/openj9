/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include <stdlib.h>
#include <string.h>

#include "jvmti_test.h"

static agentEnv * env;


static jvmtiCapabilities initialCapabilities;

static void JNICALL callback_ThreadStart(jvmtiEnv * jvmti_env, JNIEnv * jniEnv, jthread thread);
static void getCapabilities(jvmtiCapabilities * caps, int * availableCount, int * unavailableCount);



jint JNICALL
gpc002(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities caps;
	jvmtiPhase phase;
	jvmtiError err;

	env = agent_env;
	
	err = (*jvmti_env)->GetPhase(jvmti_env, &phase);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetPhase");
		return JNI_ERR;
	}
	
	if (phase != JVMTI_PHASE_ONLOAD) {
		error(env, JVMTI_ERROR_WRONG_PHASE, "Wrong phase [%d], expected JVMTI_PHASE_ONLOAD");
		return JNI_ERR;
	}
	
	
	err = (*jvmti_env)->GetPotentialCapabilities(jvmti_env, &initialCapabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetPotentialCapabilities");
		return JNI_ERR;
	}

	memset(&caps, 0x00, sizeof(jvmtiCapabilities));

	if (initialCapabilities.can_suspend == 1) {
		caps.can_suspend = 1;
	}

	if (initialCapabilities.can_get_owned_monitor_stack_depth_info == 1) {
		caps.can_get_owned_monitor_stack_depth_info = 1;
	}

	if (initialCapabilities.can_get_constant_pool == 1) {
		caps.can_get_constant_pool = 1;
	}

	if (initialCapabilities.can_set_native_method_prefix == 1) {
		caps.can_set_native_method_prefix = 1;
	}

	if (initialCapabilities.can_retransform_classes == 1) {
		caps.can_retransform_classes = 1;
	}

	if (initialCapabilities.can_generate_resource_exhaustion_heap_events == 1) {
		caps.can_generate_resource_exhaustion_heap_events = 1;
	}
/*
	if (initialCapabilities.can_retransform_any_class == 1) {
		caps.can_retransform_any_class = 1;
	}
*/
	{
	int availableCount = 0;
	int unavailableCount = 0;
    getCapabilities(&initialCapabilities, &availableCount, &unavailableCount);
	}

	err = (*jvmti_env)->AddCapabilities(jvmti_env, &caps);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}


	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, (jthread) NULL);
	if (err != JVMTI_ERROR_NONE)
	{
		error(env, err, "Failed to SetEventNotificationMode for thread start");
		return JNI_ERR;
	}
	 
	memset(&callbacks, 0, sizeof(callbacks));
	 
	callbacks.ThreadStart = &callback_ThreadStart;
	 
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, (jint) sizeof(callbacks));
	if (err != JVMTI_ERROR_NONE)
	{
		error(env, err, "Failed to SetEventCallbacks");
		return JNI_ERR;
	}
	
	return JNI_OK;
}


static void
getCapabilities(jvmtiCapabilities * caps, int * availableCount, int * unavailableCount)
{
	#define PRINT_CAPABILITY(capability) \
		printf("\t%d %s\n", caps->capability, #capability); \
		if (caps->capability) \
			(*availableCount)++; else (*unavailableCount)++;
	
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

#if JAVA_SPEC_VERSION >= 9
	/* JVMTI 9.0 */
	PRINT_CAPABILITY(can_generate_early_vmstart);
	PRINT_CAPABILITY(can_generate_early_class_hook_events);
#endif /* JAVA_SPEC_VERSION >= 9 */
	/* JVMTI 11 */

#if JAVA_SPEC_VERSION >= 11
	PRINT_CAPABILITY(can_generate_sampled_object_alloc_events);
#endif /* JAVA_SPEC_VERSION >= 11 */

}


jboolean JNICALL
Java_com_ibm_jvmti_tests_getPotentialCapabilities_gpc002_verifyCapabilityRetention(JNIEnv *jni_env, jclass cls)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	int availableCount = 0;
	int unavailableCount = 0;
	jvmtiCapabilities capabilities;
	jvmtiPhase phase;
	jvmtiError err;

	err = (*jvmti_env)->GetPhase(jvmti_env, &phase);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetPhase");
		return JNI_ERR;
	}
	
	if (phase != JVMTI_PHASE_LIVE) {
		error(env, JVMTI_ERROR_WRONG_PHASE, "Wrong phase [%d], expected JVMTI_PHASE_LIVE");
		return JNI_ERR;
	}


	err = (*jvmti_env)->GetPotentialCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetPotentialCapabilities");
		return JNI_ERR;
	}


	if (capabilities.can_suspend == 0) {
		error(env, err, "Missing can_suspend");
	}

	if (capabilities.can_get_owned_monitor_stack_depth_info == 0) {
		error(env, err, "Missing can_get_owned_monitor_stack_depth_info");
	}

	if (capabilities.can_get_constant_pool == 0) {
		error(env, err, "Missing can_get_constant_pool");
	}

	if (capabilities.can_set_native_method_prefix == 0) {
		error(env, err, "Missing can_set_native_method_prefix");
	}

	if (capabilities.can_retransform_classes == 0) {
		error(env, err, "Missing can_retransform_classes");
	}

	if (capabilities.can_generate_resource_exhaustion_heap_events == 0) {
		error(env, err, "Missing can_generate_resource_exhaustion_heap_events");
	}

	if (capabilities.can_retransform_any_class == 1) {
		error(env, err, "Unexpected can_retransform_any_class");
	}

	return JNI_TRUE;
}

static void JNICALL
callback_ThreadStart(jvmtiEnv * jvmti_env, JNIEnv * jniEnv, jthread thread)
{
	jvmtiError err;
	jvmtiPhase phase;
	static jvmtiCapabilities capabilities;



	/* get the phase */
	err = (*jvmti_env)->GetPhase(jvmti_env, &phase);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetPhase");
		return;
	}

    printf("Thread start. phase = %d\n", phase);
	
	
	if (phase == JVMTI_PHASE_LIVE)
	{
		err = (*jvmti_env)->GetCapabilities(jvmti_env, &capabilities);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to GetPotentialCapabilities");
			return;
		}

		if (capabilities.can_suspend == 0) {
			error(env, err, "Missing can_suspend");
		}

		if (capabilities.can_get_owned_monitor_stack_depth_info == 0) {
			error(env, err, "Missing can_get_owned_monitor_stack_depth_info");
		}

		if (capabilities.can_get_constant_pool == 0) {
			error(env, err, "Missing can_get_constant_pool");
		}

		if (capabilities.can_set_native_method_prefix == 0) {
			error(env, err, "Missing can_set_native_method_prefix");
		}

		if (capabilities.can_retransform_classes == 0) {
			error(env, err, "Missing can_retransform_classes");
		}

		if (capabilities.can_generate_resource_exhaustion_heap_events == 0) {
			error(env, err, "Missing can_generate_resource_exhaustion_heap_events");
		}

		if (capabilities.can_retransform_any_class == 1) {
			error(env, err, "Unexpected can_retransform_any_class");
		}


	}

}
