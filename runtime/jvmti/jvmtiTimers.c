/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

	Trc_JVMTI_jvmtiGetCurrentThreadCpuTimerInfo_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_current_thread_cpu_time);

	ENSURE_NON_NULL(info_ptr);

	memset(info_ptr, 0, sizeof(jvmtiTimerInfo));

	info_ptr->max_value = (jlong)-1;							/* according to JVMTI spec an unsigned value held as a jlong */
	info_ptr->may_skip_forward = JNI_FALSE;
	info_ptr->may_skip_backward = JNI_FALSE;
	info_ptr->kind = JVMTI_TIMER_TOTAL_CPU;
	rc = JVMTI_ERROR_NONE;

done:
	TRACE_JVMTI_RETURN(jvmtiGetCurrentThreadCpuTimerInfo);
}


jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTime(jvmtiEnv* env,
	jlong* nanos_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetCurrentThreadCpuTime_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_current_thread_cpu_time);

	ENSURE_NON_NULL(nanos_ptr);

	*nanos_ptr = (jlong)omrthread_get_self_cpu_time(omrthread_self());
	rc = JVMTI_ERROR_NONE;

done:
	TRACE_JVMTI_RETURN(jvmtiGetCurrentThreadCpuTime);
}


jvmtiError JNICALL
jvmtiGetThreadCpuTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetThreadCpuTimerInfo_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_get_thread_cpu_time);

	ENSURE_NON_NULL(info_ptr);

	memset(info_ptr, 0, sizeof(jvmtiTimerInfo));

	info_ptr->max_value = (jlong)-1;							/* according to JVMTI spec an unsigned value held as a jlong */
	info_ptr->may_skip_forward = JNI_FALSE;
	info_ptr->may_skip_backward = JNI_FALSE;
	info_ptr->kind = JVMTI_TIMER_TOTAL_CPU;
	rc = JVMTI_ERROR_NONE;

done:
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

	Trc_JVMTI_jvmtiGetThreadCpuTime_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_thread_cpu_time);

		if (thread == NULL) {
			ENSURE_NON_NULL(nanos_ptr);
			*nanos_ptr = (jlong)omrthread_get_cpu_time(omrthread_self());
		} else {
			rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
			if (rc == JVMTI_ERROR_NONE) {
				if (nanos_ptr == NULL) {
					rc = JVMTI_ERROR_NULL_POINTER;
				} else {
					*nanos_ptr = (jlong)omrthread_get_cpu_time(targetThread->osThread);
				}
				releaseVMThread(currentThread, targetThread);
			}
		}
done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiGetThreadCpuTime);
}


jvmtiError JNICALL
jvmtiGetTimerInfo(jvmtiEnv* env,
	jvmtiTimerInfo* info_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetTimerInfo_Entry(env);

	ENSURE_NON_NULL(info_ptr);

	memset(info_ptr, 0, sizeof(jvmtiTimerInfo));

	info_ptr->max_value = (jlong)-1;							/* according to JVMTI spec an unsigned value held as a jlong */
	info_ptr->may_skip_forward = JNI_TRUE;
	info_ptr->may_skip_backward = JNI_TRUE;
	info_ptr->kind = JVMTI_TIMER_ELAPSED;
	rc = JVMTI_ERROR_NONE;

done:
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

	Trc_JVMTI_jvmtiGetTime_Entry(env);

	ENSURE_NON_NULL(nanos_ptr);

	ticks = j9time_hires_clock();
	freq = j9time_hires_frequency();

	/* freq is "ticks per s" */
	if ( freq == J9JVMTI_NANO_TICKS ) {
		*nanos_ptr = ticks;
	} else if ( freq < J9JVMTI_NANO_TICKS ) {
		*nanos_ptr = ticks * (J9JVMTI_NANO_TICKS / freq);
	} else {
		*nanos_ptr = ticks / (freq / J9JVMTI_NANO_TICKS);
	}
	rc = JVMTI_ERROR_NONE;

done:
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

	Trc_JVMTI_jvmtiGetAvailableProcessors_Entry(env);

	ENSURE_NON_NULL(processor_count_ptr);

	cpuCount = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	*processor_count_ptr = ((cpuCount == 0) ? 1 : (jint) cpuCount);
	rc = JVMTI_ERROR_NONE;

done:
	TRACE_JVMTI_RETURN(jvmtiGetAvailableProcessors);
}



