/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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

#include "jni.h"
#include "j9.h"
#include "j9port.h"

/**
 * Returns total amount of time the process has been scheduled or executed so far
 * in both kernel and user modes.
 *
 * @return Time in 100 ns units, or -1 in the case of an error or if this metric
 *         is not supported for this operating system.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessCpuTimeImpl(JNIEnv *env, jobject instance)
{
	intptr_t rc = 0;
	omrthread_process_time_t processTime = {0};

	rc = omrthread_get_process_times(&processTime);
	if (0 == rc) {
		jlong times = (jlong)(processTime._userTime + processTime._systemTime) / (jlong) 100;
		return times;
	} else {
		return -1;
	}
}

/**
 * Returns the amount of physical memory that is available on the system in bytes.
 *
 * @return Amount of physical memory available in bytes or -1
 *         in the case of an error or if this metric is not
 *         supported for this operating system.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getFreePhysicalMemorySizeImpl(JNIEnv *env, jobject instance)
{
	PORT_ACCESS_FROM_ENV(env);
	uint64_t size = 0;
	int32_t rc = j9vmem_get_available_physical_memory(&size);
	return (0 == rc)? (jlong) size: (jlong) -1;
}

/**
 * Returns the amount of virtual memory used by the process
 * in bytes, including physical memory and swap space.
 *
 * @return Amount of virtual memory used by the process in bytes,
 *         or -1 in the case of an error or if this metric is not
 *         supported for this operating system.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessVirtualMemorySizeImpl(JNIEnv *env, jobject instance)
{
	PORT_ACCESS_FROM_ENV(env);
	uint64_t size = 0;
	int32_t rc = j9vmem_get_process_memory_size(J9PORT_VMEM_PROCESS_VIRTUAL, &size);
	return (0 == rc)? (jlong) size: (jlong) -1;
}

/**
 * Returns the amount of private memory used by the process
 * in bytes. It is platform specific whether this value includes
 * "share-able" memory that is not used by any other process.
 *
 * @return Amount of private memory used by the process in bytes,
 *         or -1 in the case of an error or if this metric is not
 *         supported for this operating system.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessPrivateMemorySizeImpl(JNIEnv *env, jobject instance)
{
	PORT_ACCESS_FROM_ENV(env);
	uint64_t size = 0;
	int32_t rc = j9vmem_get_process_memory_size(J9PORT_VMEM_PROCESS_PRIVATE, &size);
	return (0 == rc)? (jlong) size: (jlong) -1;
}

/**
 * Returns the amount of physical memory being used by the process
 * in bytes. This corresponds to the "resident set size" (rss) on
 * Unix platforms and "working set size" on Windows.
 *
 * @return Amount of physical memory being used by the process in bytes,
 *         or -1 in the case of an error or if this metric is not
 *         supported for this operating system.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessPhysicalMemorySizeImpl(JNIEnv *env, jobject instance)
{
	PORT_ACCESS_FROM_ENV(env);
	uint64_t size = 0;
	int32_t rc = j9vmem_get_process_memory_size(J9PORT_VMEM_PROCESS_PHYSICAL, &size);
	return (0 == rc)? (jlong) size: (jlong) -1;
}
