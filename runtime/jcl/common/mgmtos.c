/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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
#include "mgmtinit.h"
#include "j9port.h"
#if defined(AIXPPC)
#include <sys/dr.h>
#endif /* defined(AIXPPC) */
#include "jclglob.h"

static UDATA reconfigHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);
static void enqueueNotification( struct J9PortLibrary *portLibrary, struct J9JavaLangManagementData *mgmt, U_32 type, U_64 data );



jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getTotalPhysicalMemoryImpl(JNIEnv *env, jobject beanInstance)
{
	PORT_ACCESS_FROM_ENV(env);

	return (jlong)j9sysinfo_get_physical_memory();
}

jint JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessingCapacityImpl(JNIEnv *env, jobject beanInstance)
{
	PORT_ACCESS_FROM_ENV(env);

	return (jint)j9sysinfo_get_processing_capacity();
}

jboolean JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_isDLPAREnabled(JNIEnv *env, jobject beanInstance)
{
	PORT_ACCESS_FROM_ENV(env);

	return (jboolean)j9sysinfo_DLPAR_enabled();
}

jdouble JNICALL
Java_com_ibm_java_lang_management_internal_OperatingSystemMXBeanImpl_getSystemLoadAverageImpl(JNIEnv *env, jobject beanInstance)
{

	PORT_ACCESS_FROM_ENV(env);
	J9PortSysInfoLoadData loadData;
	jdouble oneMinuteLoadAverage = -1.0;
	IDATA sysinfoResult;

	sysinfoResult = j9sysinfo_get_load_average(&loadData);
	if (sysinfoResult == 0) {
		oneMinuteLoadAverage = (jdouble) loadData.oneMinuteAverage;
	}

	return oneMinuteLoadAverage;
}

void JNICALL
Java_com_ibm_lang_management_internal_OperatingSystemNotificationThread_processNotificationLoop(JNIEnv *env, jobject instance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	jclass threadClass;
	jmethodID helperID;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	PORT_ACCESS_FROM_ENV( env );

	if (NULL == mgmt->dlparNotificationMonitor) {
		/* initialization error */
		return;
	}
	
#if 0
	/* these are initialized in managementInit() */
	mgmt->dlparNotificationQueue = NULL;
	mgmt->dlparNotificationsPending = 0;
#endif
	
	mgmt->dlparNotificationCount = 0;
	mgmt->currentNumberOfCPUs = (U_32)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	mgmt->currentProcessingCapacity = (U_32)j9sysinfo_get_processing_capacity();
	mgmt->currentTotalPhysicalMemory = j9sysinfo_get_physical_memory();

	/* locate the helper that constructs and dispatches the Java notification objects */
	threadClass = (*env)->FindClass( env, "com/ibm/lang/management/internal/OperatingSystemNotificationThread" );
	if( threadClass == NULL ) {
		return;
	}

	helperID = (*env)->GetMethodID( env, threadClass, "dispatchNotificationHelper", "(IJJ)V" );
	if( helperID == NULL ) {
		return;
	}

	j9sig_set_async_signal_handler(reconfigHandler, mgmt, J9PORT_SIG_FLAG_SIGRECONFIG);

	while(1) {
		J9DLPARNotification *notification;

		/* wait on the notification queue monitor until a notification is posted */
		omrthread_monitor_enter( mgmt->dlparNotificationMonitor );
		while( mgmt->dlparNotificationsPending == 0 ) {
			omrthread_monitor_wait( mgmt->dlparNotificationMonitor );
		}
		mgmt->dlparNotificationsPending--;
		omrthread_monitor_exit( mgmt->dlparNotificationMonitor );

		/* lock the management struct and take a notification off the queue head */
		omrthread_rwmutex_enter_write( mgmt->managementDataLock );
		notification = mgmt->dlparNotificationQueue;
		mgmt->dlparNotificationQueue = notification->next;
		omrthread_rwmutex_exit_write( mgmt->managementDataLock );

		if( notification->type != DLPAR_NOTIFIER_SHUTDOWN_REQUEST ) {
			(*env)->CallVoidMethod( env, instance, helperID, (jint)notification->type, (jlong)notification->data, (jlong)notification->sequenceNumber );
		} else {
			/* shutdown request */
			j9mem_free_memory( notification );
			break;
		}

		/* clean up the notification */
		j9mem_free_memory( notification );

		if( (*env)->ExceptionCheck(env) ) {
			/* if the dispatcher throws, just kill the thread for now */
			break;
		}
	}

	/* Handle the race condition where the shutdown thread disabled reconfigHandler
	 * before we enabled it.
	 */
	j9sig_set_async_signal_handler(reconfigHandler, mgmt, 0);
}

void JNICALL
Java_com_ibm_lang_management_internal_OperatingSystemNotificationThreadShutdown_sendShutdownNotification(JNIEnv *env, jobject instance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	J9DLPARNotification *notification, *next;
	PORT_ACCESS_FROM_ENV( env );

	j9sig_set_async_signal_handler(reconfigHandler, mgmt, 0);

	if( mgmt->dlparNotificationMonitor == NULL ) {
		/* initialization error - abort */
		return;
	}

	/* allocate a notification */
	notification = j9mem_allocate_memory( sizeof( *notification ) , J9MEM_CATEGORY_VM_JCL);
	if( notification != NULL ) {
		/* set up a shutdown notification */
		notification->type = DLPAR_NOTIFIER_SHUTDOWN_REQUEST;
		notification->next = NULL;

		/* replace the queue with just this notification - the notifier thread does not care that the pending count might be >1, it will not process anything after the shutdown request */
		omrthread_rwmutex_enter_write( mgmt->managementDataLock );
		next = mgmt->dlparNotificationQueue;
		mgmt->dlparNotificationQueue = notification;
		omrthread_rwmutex_exit_write( mgmt->managementDataLock );

		/* free the old queue entries if any */
		while( next != NULL ) {
			J9DLPARNotification *temp = next;
			next = next->next;
			j9mem_free_memory( temp );
		}

		/* notify the notification thread */
		omrthread_monitor_enter( mgmt->dlparNotificationMonitor );
		mgmt->dlparNotificationsPending++;
		omrthread_monitor_notify( mgmt->dlparNotificationMonitor );
		omrthread_monitor_exit( mgmt->dlparNotificationMonitor );
	}

}

/**
 * Call j9sysinfo_get_CPU_utilization to get CPU utilization statistics.
 * Create and populate a SysinfoCpuTime with the results.
 * The status field is a summary
 * of the status code returned by the port library.  For more error details, use the
 * port library tracepoints.
 * @return SysinfoCpuTime or NULL if the object could  not be created.
 */
jobject JNICALL
Java_com_ibm_lang_management_internal_SysinfoCpuTime_getCpuUtilizationImpl(JNIEnv *env, jclass clazz) {
	jmethodID tempMethod;
	J9SysinfoCPUTime cpuTime;
	IDATA portLibraryStatus;
	IDATA myStatus = 0;
	jobject result;
	PORT_ACCESS_FROM_ENV(env);

	tempMethod = JCL_CACHE_GET(env, MID_com_ibm_lang_management_SysinfoCpuTime_getCpuUtilization_init);

	if (NULL == tempMethod) {

		tempMethod = (*env)->GetMethodID(env, clazz, "<init>", "(JJII)V");
		if (NULL == tempMethod) {
			return NULL;
		}

		JCL_CACHE_SET(env, MID_com_ibm_lang_management_SysinfoCpuTime_getCpuUtilization_init, tempMethod);
	}

	portLibraryStatus = j9sysinfo_get_CPU_utilization(&cpuTime);
	if (portLibraryStatus < 0) {
		const IDATA ERROR_VALUE = -1;
		const IDATA INSUFFICIENT_PRIVILEGE = -2;
		const IDATA UNSUPPORTED_VALUE = -3;
/*		const IDATA INTERNAL_ERROR = -4; placeholder.  Not used in this context */

		switch (portLibraryStatus) {
		case J9PORT_ERROR_SYSINFO_INSUFFICIENT_PRIVILEGE:
			myStatus = INSUFFICIENT_PRIVILEGE;
			break;
		case J9PORT_ERROR_SYSINFO_NOT_SUPPORTED:
			myStatus = UNSUPPORTED_VALUE;
			break;
		default:
			myStatus = ERROR_VALUE;
			break;
		}
	}
	result = (*env)->NewObject(env, clazz, tempMethod,
			cpuTime.timestamp, cpuTime.cpuTime, cpuTime.numberOfCpus, myStatus);
	return result;
}

static UDATA 
reconfigHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	J9JavaLangManagementData *mgmt = userData;
	U_32 newNumberOfCPUs, newProcessingCapacity;
	U_64 newTotalPhysicalMemory;
#if defined(AIXPPC)
	/*
	 * dr_info_t struct to store reconfig identification information that is acquired through port control.
	 */
	dr_info_t dr;
#endif

	PORT_ACCESS_FROM_PORT(portLibrary);

#if defined(AIXPPC)
	/* Obtain reconfig identification information from the OS.  */
	dr_reconfig(DR_QUERY, &dr);
#endif

	/* we just caught SIGRECONFIG - rather than try to figure out the DLPAR event directly, we just check whether the 3 numbers we're interested in have actually changed */
	newNumberOfCPUs = (U_32)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	newProcessingCapacity = (U_32)j9sysinfo_get_processing_capacity();
	newTotalPhysicalMemory = j9sysinfo_get_physical_memory();

#if defined(AIXPPC) && !defined(J9OS_I5)
	/* Tell the OS that we're done with the reconfig request passing in the identification
	 * information obtained during the RECONFIG_QUERY */
	dr_reconfig(DR_RECONFIG_DONE, &dr);
#endif

	/* lock the management struct */
	omrthread_rwmutex_enter_write( mgmt->managementDataLock );

	if( mgmt->currentNumberOfCPUs != newNumberOfCPUs ) {
		mgmt->currentNumberOfCPUs = newNumberOfCPUs;
		enqueueNotification( PORTLIB, mgmt, DLPAR_NUMBER_OF_CPUS_CHANGE, newNumberOfCPUs );
	}

	if( mgmt->currentProcessingCapacity != newProcessingCapacity ) {
		mgmt->currentProcessingCapacity = newProcessingCapacity;
		enqueueNotification( PORTLIB, mgmt, DLPAR_PROCESSING_CAPACITY_CHANGE, newProcessingCapacity );
	}

	if( mgmt->currentTotalPhysicalMemory != newTotalPhysicalMemory ) {
		mgmt->currentTotalPhysicalMemory = newTotalPhysicalMemory;
		enqueueNotification( PORTLIB, mgmt, DLPAR_TOTAL_PHYSICAL_MEMORY_CHANGE, newTotalPhysicalMemory );
	}
	
	/* If the Live Application Mobility feature of WPAR is enabled then this SIGRECONFIG may have been 
	 * the result of moving from one hardware system to another and will requires the reset 
	 * of some internal cache for fast path current time millis code on AIX. 
	 * Even if this was not the cause of the SIGRECONFIG this cache reset is safe. */
	j9port_control(J9PORT_CTLDATA_TIME_CLEAR_TICK_TOCK, 0);

	omrthread_rwmutex_exit_write( mgmt->managementDataLock );

	return 0;
}

static void
enqueueNotification( struct J9PortLibrary *portLibrary, struct J9JavaLangManagementData *mgmt, U_32 type, U_64 data )
{
	J9DLPARNotification *notification;
	U_32 count = 0;
	J9DLPARNotification *last = mgmt->dlparNotificationQueue;

	PORT_ACCESS_FROM_PORT(portLibrary);

	/* find the end of the queue and count the entries */
	while( last != NULL && last->next != NULL ) {
		last = last->next;
		count++;
	}

	/* if the queue is full, silently discard this notification */
	if( count < NOTIFICATION_QUEUE_MAX ) {
		notification = j9mem_allocate_memory(sizeof( *notification ) , J9MEM_CATEGORY_VM_JCL);
		/* in case of allocation failure, silently discard the notification */
		if( notification != NULL ) {
			/* populate the notification data */
			notification->type = type;
			notification->next = NULL;
			notification->data = data;
			notification->sequenceNumber = mgmt->dlparNotificationCount++;

			/* add to queue */
			if( last == NULL ) {
				mgmt->dlparNotificationQueue = notification;
			} else {
				last->next = notification;
			}

			/* notify the thread that dispatches notifications to Java handlers */
			omrthread_monitor_enter( mgmt->dlparNotificationMonitor );
			mgmt->dlparNotificationsPending++;
			omrthread_monitor_notify( mgmt->dlparNotificationMonitor );
			omrthread_monitor_exit( mgmt->dlparNotificationMonitor );
		}
	}
}
