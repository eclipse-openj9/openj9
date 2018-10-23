/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#define J9JVMTI_NANO_TICKS  1000000000L

jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr)
{
	jvmtiError rc;
	jvmtiTimerInfo rv_info = {0};

	Trc_JVMTI_jvmtiGetCurrentThreadCpuTimerInfo_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_current_thread_cpu_time);

	ENSURE_NON_NULL(info_ptr);

	rv_info.max_value = (jlong)-1;							/* according to JVMTI spec an unsigned value held as a jlong */
	rv_info.may_skip_forward = JNI_FALSE;
	rv_info.may_skip_backward = JNI_FALSE;
	rv_info.kind = JVMTI_TIMER_TOTAL_CPU;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != info_ptr) {
		*info_ptr = rv_info;
	}
	TRACE_JVMTI_RETURN(jvmtiGetCurrentThreadCpuTimerInfo);
}


jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTime(jvmtiEnv* env,
	jlong* nanos_ptr)
{
	jvmtiError rc;
	jlong rv_nanos = 0;

	Trc_JVMTI_jvmtiGetCurrentThreadCpuTime_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_current_thread_cpu_time);

	ENSURE_NON_NULL(nanos_ptr);

	rv_nanos = (jlong)omrthread_get_self_cpu_time(omrthread_self());
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != nanos_ptr) {
		*nanos_ptr = rv_nanos;
	}
	TRACE_JVMTI_RETURN(jvmtiGetCurrentThreadCpuTime);
}


jvmtiError JNICALL
jvmtiGetThreadCpuTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr)
{
	jvmtiError rc;
	jvmtiTimerInfo rv_info = {0};

	Trc_JVMTI_jvmtiGetThreadCpuTimerInfo_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_thread_cpu_time);

	ENSURE_NON_NULL(info_ptr);

	rv_info.max_value = (jlong)-1;							/* according to JVMTI spec an unsigned value held as a jlong */
	rv_info.may_skip_forward = JNI_FALSE;
	rv_info.may_skip_backward = JNI_FALSE;
	rv_info.kind = JVMTI_TIMER_TOTAL_CPU;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != info_ptr) {
		*info_ptr = rv_info;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadCpuTimerInfo);
}


jvmtiError JNICALL
jvmtiGetThreadCpuTime(jvmtiEnv* env,
	jthread thread,
	jlong* nanos_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;
	jlong rv_nanos = 0;

	Trc_JVMTI_jvmtiGetThreadCpuTime_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_thread_cpu_time);

		if (thread == NULL) {
			ENSURE_NON_NULL(nanos_ptr);
			rv_nanos = (jlong)omrthread_get_cpu_time(omrthread_self());
		} else {
			rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
			if (rc == JVMTI_ERROR_NONE) {
				if (nanos_ptr == NULL) {
					rc = JVMTI_ERROR_NULL_POINTER;
				} else {
					rv_nanos = (jlong)omrthread_get_cpu_time(targetThread->osThread);
				}
				releaseVMThread(currentThread, targetThread);
			}
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != nanos_ptr) {
		*nanos_ptr = rv_nanos;
	}
	TRACE_JVMTI_RETURN(jvmtiGetThreadCpuTime);
}


jvmtiError JNICALL
jvmtiGetTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr)
{
	jvmtiError rc;
	jvmtiTimerInfo rv_info = {0};

	Trc_JVMTI_jvmtiGetTimerInfo_Entry(env);

	ENSURE_NON_NULL(info_ptr);

	rv_info.max_value = (jlong)-1;							/* according to JVMTI spec an unsigned value held as a jlong */
	rv_info.may_skip_forward = JNI_TRUE;
	rv_info.may_skip_backward = JNI_TRUE;
	rv_info.kind = JVMTI_TIMER_ELAPSED;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != info_ptr) {
		*info_ptr = rv_info;
	}
	TRACE_JVMTI_RETURN(jvmtiGetTimerInfo);
}


jvmtiError JNICALL
jvmtiGetTime(jvmtiEnv* env,
	jlong* nanos_ptr)
{
	jlong ticks;
	jlong freq;
	jvmtiError rc;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	PORT_ACCESS_FROM_JAVAVM(vm);
	jlong rv_nanos = 0;

	Trc_JVMTI_jvmtiGetTime_Entry(env);

	ENSURE_NON_NULL(nanos_ptr);

	ticks = j9time_hires_clock();
	freq = j9time_hires_frequency();

	/* freq is "ticks per s" */
	if ( freq == J9JVMTI_NANO_TICKS ) {
		rv_nanos = ticks;
	} else if ( freq < J9JVMTI_NANO_TICKS ) {
		rv_nanos = ticks * (J9JVMTI_NANO_TICKS / freq);
	} else {
		rv_nanos = ticks / (freq / J9JVMTI_NANO_TICKS);
	}
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != nanos_ptr) {
		*nanos_ptr = rv_nanos;
	}
	TRACE_JVMTI_RETURN(jvmtiGetTime);
}


jvmtiError JNICALL
jvmtiGetAvailableProcessors(jvmtiEnv* env,
	jint* processor_count_ptr)
{
	jvmtiError rc;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA cpuCount;
	jint rv_processor_count = 0;

	Trc_JVMTI_jvmtiGetAvailableProcessors_Entry(env);

	ENSURE_NON_NULL(processor_count_ptr);

	/* This implementation should be kept consistent with JVM_ActiveProcessorCount */
	cpuCount = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
	rv_processor_count = ((cpuCount < 1) ? 1 : (jint) cpuCount);
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != processor_count_ptr) {
		*processor_count_ptr = rv_processor_count;
	}
	TRACE_JVMTI_RETURN(jvmtiGetAvailableProcessors);
}



