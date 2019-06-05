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

jint JNICALL
gpc001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
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

#if JAVA_SPEC_VERSION >= 11
	/* JVMTI 11 */
	PRINT_CAPABILITY(can_generate_sampled_object_alloc_events);
#endif /* JAVA_SPEC_VERSION >= 11 */
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getPotentialCapabilities_gpc001_verifyOnLoadCapabilities(JNIEnv *jni_env, jclass cls)
{
	int availableCount = 0;
	int unavailableCount = 0;
	jboolean result = JNI_TRUE;
	
	getCapabilities(&initialCapabilities, &availableCount, &unavailableCount);

	if (!initialCapabilities.can_retransform_any_class) {
		unavailableCount--;
	}
	
	if (!initialCapabilities.can_redefine_any_class) {
		unavailableCount--;
	}
	
	/* s390 thread/port lib does not support this functionality */
	if (!initialCapabilities.can_get_current_thread_cpu_time) {
		unavailableCount--;
	}
	
	/* s390 thread/port lib does not support this functionality */
	if (!initialCapabilities.can_get_thread_cpu_time) {
		unavailableCount--;
	}

	if (unavailableCount != 0) {
		error(env, JVMTI_ERROR_INTERNAL, "Unexpected number [%d] of unavailable capabilities. Expected 0", unavailableCount);
		return JNI_FALSE;
	}

#if JAVA_SPEC_VERSION >= 9
	if (!initialCapabilities.can_generate_early_vmstart) {
		error(env, JVMTI_ERROR_INTERNAL, "can_generate_early_vmstart should be available in onload phase.");
		result = JNI_FALSE;
	} else if (!initialCapabilities.can_generate_early_class_hook_events) {
		error(env, JVMTI_ERROR_INTERNAL, "can_generate_early_class_hook_events should be available in onload phase.");
		result = JNI_FALSE;
	}
#if JAVA_SPEC_VERSION >= 11
	else if (!initialCapabilities.can_generate_sampled_object_alloc_events) {
		error(env, JVMTI_ERROR_INTERNAL, "can_generate_sampled_object_alloc_events should be available in onload phase.");
		result = JNI_FALSE;
	}
#endif /* JAVA_SPEC_VERSION >= 11 */
#endif /* JAVA_SPEC_VERSION >= 9 */

	return result;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getPotentialCapabilities_gpc001_verifyLiveCapabilities(JNIEnv *jni_env, jclass cls)
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
 
	getCapabilities(&capabilities, &availableCount, &unavailableCount);

	if (availableCount <= 0) {
		error(env, JVMTI_ERROR_INTERNAL, "Unexpected number [%d] of available capabilities.", availableCount);
		return JNI_FALSE;
	}

#if JAVA_SPEC_VERSION >= 9
	if (1 == capabilities.can_generate_early_vmstart) {
		error(env, JVMTI_ERROR_INTERNAL, "can_generate_early_vmstart should not be available in live phase.");
		return JNI_FALSE;
	}
		
	if (1 == capabilities.can_generate_early_class_hook_events) {
		error(env, JVMTI_ERROR_INTERNAL, "can_generate_early_class_hook_events should not be available in the live phase.");
		return JNI_FALSE;
	}
#endif /* JAVA_SPEC_VERSION >= 9 */

	return JNI_TRUE;
}
