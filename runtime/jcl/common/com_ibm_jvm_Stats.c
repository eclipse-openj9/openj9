/*******************************************************************************
 * Copyright (c) 2012, 2016 IBM Corp. and others
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
#include "jclprots.h"
#include "jclglob.h"

/**
 * Provides the following statistics:
 *   used heap memory
 *   committed heap memory
 *   max heap memory
 *   softmx heap memory
 *   free physical memory,
 * 	 total physical memory,
 *   system load average
 *   cpuTime
 *
 * This function avoids any allocation of a new object. Instead it sets the field
 * variables in the class Stats.java
 */

void JNICALL
Java_com_ibm_jvm_Stats_getStats(JNIEnv *env, jobject obj)
{
	/* System and Heap statistics */
	jlong used = 0;
	jlong committed = 0;
	jlong max = 0;
	jlong softmx = 0;
	jlong free = 0;
	jlong tot = 0;
    jdouble sysLoadAvg = -1.0;
    jlong cpuTime = 0;

	/* Access to the current jvm */
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	/* Port lib access */
	PORT_ACCESS_FROM_ENV(env);
	J9PortSysInfoLoadData loadData;
	IDATA sysInfoResult;

	/* Class and method ID used to set our variables */
	jclass setFieldsClass = NULL;
	jmethodID methodID = NULL;

	/* Calculation of java heap properties */
	committed = javaVM->memoryManagerFunctions->j9gc_heap_total_memory(javaVM);
	used = committed - javaVM->memoryManagerFunctions->j9gc_heap_free_memory(javaVM);
	max = (jlong) javaVM->managementData->maximumHeapSize;

	/* Calculation of softmx */
	softmx = javaVM->memoryManagerFunctions->j9gc_get_softmx( javaVM );

	/* if no softmx has been set, report -Xmx instead as it is the current max heap size */
	if (0 == softmx) {
		softmx = max;
	}

	/* Calculation of free physical memory from existing function in mgmtosext.c*/
	free = Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getFreePhysicalMemorySizeImpl(env, obj);

	/* Calculation of total physical memory from port lib api */
	tot = (jlong) j9sysinfo_get_physical_memory();

	/* Calculation of system load average */
	sysInfoResult = j9sysinfo_get_load_average(&loadData);
	if (0 == sysInfoResult) {
		sysLoadAvg = (jdouble) loadData.oneMinuteAverage;
	}

	/* Calculation of cpu time from existing function in mgmtosext.c*/
	cpuTime = Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessCpuTimeImpl(env, obj);

	/* Set the statistics by calling the java setFields function */
	setFieldsClass = (*env)->GetObjectClass(env, obj);
	methodID = JCL_CACHE_GET(env, MID_com_ibm_jvm_Stats_setFields);
	if (NULL == methodID) {
		methodID = (*env)->GetMethodID(env, setFieldsClass, "setFields", "(JJJJJDJJ)V");
		JCL_CACHE_SET(env, MID_com_ibm_jvm_Stats_setFields, methodID);
	}

	/* Call method only if there's no exception pending (prevent crash on C side) */
	if (!((*env)->ExceptionCheck(env))) {
		(*env)->CallVoidMethod(env, obj, methodID, committed, used, max, free, tot, sysLoadAvg, cpuTime, softmx);
	}
}
