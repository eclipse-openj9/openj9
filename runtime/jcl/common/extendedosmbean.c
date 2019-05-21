/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

#include <assert.h>

#include "j9.h"
#include "j9port.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jni.h"
#include "portnls.h"

#define MESSAGE_STRING_LENGTH 256

/**
 * Function returns the Class ID of ProcessorUsage and method ID of the named methods. This is
 * a common code that routines dealing with ProcessorUsage class need. Internally, the function
 * resolves the IDs only the first time and caches them up. Every next time, the cached values
 * are returned, provided that the resolution (up on the first call) was successful.
 *
 * @param[in] env The JNI env.
 * @param[out] ProcessorUsage A pointer to the class ProcessorUsage's ID.
 * @param[out] ProcessorUsageCtor A pointer to ProcessorUsage's default constructor.
 * @param[out] UpdateValues A pointer to ProcessorUsage's updateValues() constructor.
 *
 * @return 0, on success; -1, on failure.
 */
static I_32
resolveProcessorUsageIDs(JNIEnv *env,
	jclass *ProcessorUsage,
	jmethodID *ProcessorUsageCtor,
	jmethodID *UpdateValues)
{
	/* Check whether the class ID and the method IDs have already been cached or not. */
	if (NULL == JCL_CACHE_GET(env, MID_com_ibm_lang_management_ProcessorUsage_init)) {
		jclass localProcUsageRef = NULL;

		/* Get the class ID for the Java class ProcessorUsage. */
		localProcUsageRef = (*env)->FindClass(env, "com/ibm/lang/management/ProcessorUsage");
		if (NULL == localProcUsageRef) {
			return -1;
		}

		/* Convert this into a Global reference. */
		*ProcessorUsage = (*env)->NewGlobalRef(env, localProcUsageRef);

		/* Delete the local reference. */
		(*env)->DeleteLocalRef(env, localProcUsageRef);
		if (NULL == ProcessorUsage) {
			return -1;
		}

		/* Cache the thus resolved global class ID for ProcessorUsage class. */
		JCL_CACHE_SET(env, CLS_com_ibm_lang_management_ProcessorUsage, *ProcessorUsage);

		/* Obtain the method ID for ProcessorUsage's updateValues() method. */
		*UpdateValues = (*env)->GetMethodID(env, *ProcessorUsage, "updateValues", "(JJJJJIIJ)V" );
		if (NULL == *UpdateValues) {
			return -1;
		}

		/* Save the method ID for updateValues() into the cache. */
		JCL_CACHE_SET(env, MID_com_ibm_lang_management_ProcessorUsage_updateValues, *UpdateValues);

		/* Find the default constructor's method ID. */
		*ProcessorUsageCtor = (*env)->GetMethodID(env, *ProcessorUsage, "<init>", "()V");
		if (NULL == *ProcessorUsageCtor) {
			return -1;
		}

		/* Save this method ID for constructor into the cache. */
		JCL_CACHE_SET(env, MID_com_ibm_lang_management_ProcessorUsage_init, *ProcessorUsageCtor);
	} else {
		/* Turns out that these have already been resolved and cached in a prior call. Return these. */
		*ProcessorUsage = JCL_CACHE_GET(env, CLS_com_ibm_lang_management_ProcessorUsage);
		*ProcessorUsageCtor = JCL_CACHE_GET(env, MID_com_ibm_lang_management_ProcessorUsage_init);
		*UpdateValues = JCL_CACHE_GET(env, MID_com_ibm_lang_management_ProcessorUsage_updateValues);
	}

	return 0;
}

static const struct { char *name; } objType[] = { 
	{ "Processor Usage" },
	{ "Memory Usage" },
};

typedef enum {
	PROCESSOR_USAGE_ERROR = 0,
	MEMORY_USAGE_ERROR
} objTypes;

/**
 * @internal	Throw appropriate exception based on the error code passed
 *
 * @param error	Error code from port library
 * @param type  Error when retrieving either GUEST_PROCESSOR or GUEST_MEMORY statistics
 *
 * @return Always returns 0
 */
static jint
handle_error(JNIEnv *env, IDATA error, jint type)
{
	PORT_ACCESS_FROM_ENV(env);
	jclass exceptionClass = NULL;
	const char *formatString = NULL;
	char exceptionMessage[MESSAGE_STRING_LENGTH] = {0};

	assert((type == PROCESSOR_USAGE_ERROR || type == MEMORY_USAGE_ERROR));

	/* If out of memory setup a pending OutOfMemoryError */
	if (J9PORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED == error) {
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, J9NLS_PORT_SYSINFO_OUT_OF_MEMORY_ERROR_MSG);
		return 0;
	}

	/* Read in the generic usage retrieval error string */
	formatString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_PORT_SYSINFO_USAGE_RETRIEVAL_ERROR_MSG,
					NULL);
	/* Add in the specific error and the type. */
	j9str_printf(PORTLIB,
		exceptionMessage,
		sizeof(exceptionMessage),
		(char *)formatString,
		error,
		objType[type].name);

	if (PROCESSOR_USAGE_ERROR == type) {
		exceptionClass = (*env)->FindClass(env, "com/ibm/lang/management/ProcessorUsageRetrievalException");
	} else if (MEMORY_USAGE_ERROR == type) {
		exceptionClass = (*env)->FindClass(env, "com/ibm/lang/management/MemoryUsageRetrievalException");
	}
	if (NULL != exceptionClass) {
		(*env)->ThrowNew(env, exceptionClass, exceptionMessage);
	}

	return 0;
}

/**
 * Function obtains Processor usage statistics by invoking an appropriate port library routine.
 * It also sets up a pending exception should processor usage retrieval fail.
 *
 * @param[in] env The JNI env.
 * @param[out] procInfo Pointer to a J9ProcessorInfos instance to be populated with usage stats.
 *
 * @return 0, on success; -1, on failure.
 */
static IDATA
getProcessorUsage(JNIEnv *env, J9ProcessorInfos *procInfo)
{
	IDATA rc = 0;
	/* Need port library access for calling j9sysinfo_get_processor_info(). */
	PORT_ACCESS_FROM_ENV(env);

	/* Retrieve processor usage stats - overall as well as individual for each online processor. */
	rc = j9sysinfo_get_processor_info(procInfo);
	if (0 != rc) {
		handle_error(env, rc, PROCESSOR_USAGE_ERROR);
		return -1;
	}

	return 0;
}

/**
 * Retrieves the system-wide (total) usage statistics for online processors on the underlying machine.
 *
 * Class:     com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl
 * Method:    getTotalProcessorUsageImpl
 *
 * @param[in] env The JNI env.
 * @param[in] instance The this pointer.
 * @param[in] procTotalObject An object of kind ProcessorUsage that will be populated with aggregates
 *    of all online processors usage times.
 *
 * @return A ProcessorUsage object, if processor usage data retrieval successful. NULL for failure
 *    conditions.
 */
jobject JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getTotalProcessorUsageImpl(JNIEnv *env,
    jobject instance,
    jobject procTotalObject)
{
	IDATA rc = 0;
	J9ProcessorInfos procInfo = {0};
	jclass CID_ProcessorUsage = NULL;
	jmethodID MID_updateValues = NULL;
	jmethodID MID_ProcessorUsageCtor = NULL;

	PORT_ACCESS_FROM_ENV(env);

	/* Resolve the class ID for the Java class ProcessorUsage and the method IDs for its default
	 * constructor and the updateValues() method.
	 */
	rc = resolveProcessorUsageIDs(env, &CID_ProcessorUsage, &MID_ProcessorUsageCtor, &MID_updateValues);
	if (0 != rc) {
		/* If a resolution failed, there is a pending exception here. */
		return NULL;
	}

	/* Now obtain the processor usage statistics on the underlying platform. */
	rc = getProcessorUsage(env, &procInfo);
	if (0 != rc) {
		/* If processor usage statistics retrieval failed for some reason, there is already a
		 * pending exception. Returning a NULL anyway.
		 */
		return NULL;
	}

	/* Invoke updateValues() method on the ProcessorUsage object initializing the instance fields. */
	(*env)->CallVoidMethod(env,
				   procTotalObject,
				   MID_updateValues,
				   (jlong)procInfo.procInfoArray[0].userTime,
				   (jlong)procInfo.procInfoArray[0].systemTime,
				   (jlong)procInfo.procInfoArray[0].idleTime,
				   (jlong)procInfo.procInfoArray[0].waitTime,
				   (jlong)procInfo.procInfoArray[0].busyTime,
				   ((jint) -1),
				   (jint)procInfo.procInfoArray[0].online,
				   (jlong)procInfo.timestamp);

	/* Clean up the local arrays. */
	j9sysinfo_destroy_processor_info(&procInfo);

	return procTotalObject;
}

/**
 * Retrieves the system-wide usage statistics for online processors on the underlying machine.
 *
 * Class:     com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl
 * Method:    getProcessorUsageImpl
 *
 * @param[in] env The JNI env.
 * @param[in] instance The this pointer.
 * @param[in] procUsageArray An array of objects of kind ProcessorUsage.
 *
 * @return An array of ProcessorUsage objects, if processor usage data retrieval successful. NULL for
 *    failure conditions.
 */
jobjectArray JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessorUsageImpl(JNIEnv *env,
	jobject instance,
	jobjectArray procUsageArray)
{
	I_32 i = 0;
	IDATA rc = 0;
	J9ProcessorInfos procInfo = {0};
	jobject procUsageObject = NULL;
	jclass CID_ProcessorUsage = NULL;
	jmethodID MID_ProcessorUsageCtor = NULL;
	jmethodID MID_updateValues = NULL;

	PORT_ACCESS_FROM_ENV(env);

	/* Resolve the class ID for the Java class ProcessorUsage and the method IDs for its default
	 * constructor and the updateValues() method.
	 */
	rc = resolveProcessorUsageIDs(env, &CID_ProcessorUsage, &MID_ProcessorUsageCtor, &MID_updateValues);
	if (0 != rc) {
		/* If a resolution failed, there is a pending exception here. */
		return NULL;
	}

	/* Now obtain the processor usage statistics on the underlying platform. */
	rc = getProcessorUsage(env, &procInfo);
	if (0 != rc) {
		/* If processor usage statistics retrieval failed for some reason, there is already a
		 * pending exception. Returning a NULL anyway.
		 */
		return NULL;
	}

	/* Did we receive a NULL array? If yes, allocate space to hold an array of suitable length. */
	if (NULL == procUsageArray) {
		jobject procObject;

		procUsageArray = (*env)->NewObjectArray(env,
							(jsize)procInfo.totalProcessorCount,
							CID_ProcessorUsage,
							NULL);
		if (NULL == procUsageArray) {
			/* Array creation failed. There is a pending OutOfMemoryError that will be
			 * thrown when we return to the Java layer. Here, we simply clean up and return.
			 */
			j9sysinfo_destroy_processor_info(&procInfo);
			return NULL;
		}

		/* Allocate memory for all the processors and fill in the array using these objects. */
		for (i = 0; i < procInfo.totalProcessorCount; i++) {
			procObject = (*env)->NewObject(env, CID_ProcessorUsage, MID_ProcessorUsageCtor);
			if (NULL == procObject) {
				/* If allocation failed there is a pending exception here. */
				j9sysinfo_destroy_processor_info(&procInfo);
				return NULL;
			}
			(*env)->SetObjectArrayElement(env, procUsageArray, (jsize)i, procObject);
		}
	} else if ((*env)->GetArrayLength(env, procUsageArray) < procInfo.totalProcessorCount) {

		/* Array provided has insufficient entries as there are more CPUs to report on. */
		throwNewIllegalArgumentException(env, "Insufficient sized processor array received");

		/* Cleanup before returning - we have already set up an exception to be thrown. */
		j9sysinfo_destroy_processor_info(&procInfo);
		return NULL;
	}

	/* Initialize the jobjectArray procUsageArray with the processor usage data as retrieved
	 * through by j9sysinfo_get_processor_info().
	 */
	for (i = 1; i <= procInfo.totalProcessorCount; i++) {
		procUsageObject = (*env)->GetObjectArrayElement(env, procUsageArray, i - 1);

		/* Invoke updateValues() method on each ProcessorUsage object in the array to initialize
		 * the fields in that instance.
		 */
		(*env)->CallVoidMethod(env,
					   procUsageObject,
					   MID_updateValues,
					   (jlong)procInfo.procInfoArray[i].userTime,
					   (jlong)procInfo.procInfoArray[i].systemTime,
					   (jlong)procInfo.procInfoArray[i].idleTime,
					   (jlong)procInfo.procInfoArray[i].waitTime,
					   (jlong)procInfo.procInfoArray[i].busyTime,
					   (jint)procInfo.procInfoArray[i].proc_id,
					   (jint)procInfo.procInfoArray[i].online,
					   (jlong)procInfo.timestamp);
	} /* end for(;;) */

	/* Clean up the local array. */
	j9sysinfo_destroy_processor_info(&procInfo);

	return procUsageArray;
}

/**
 * Retrieves system-wide memory statistics for the underlying machine.
 *
 * Class:     com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl
 * Method:    getMemoryUsageImpl
 *
 * @param[in] env The JNI env.
 * @param[in] instance The this pointer.
 * @param[in] memUsageObject An object of type MemoryUsage
 *
 * @return A MemoryUsage object, if memory stats usage retrieval successful. NULL, in case of a failure.
 */
jobject JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getMemoryUsageImpl(JNIEnv *env,
	jobject instance,
	jobject memUsageObject)
{
	IDATA rc = 0;
	J9MemoryInfo memInfo = {0};
	jclass CID_MemoryUsage = NULL;
	jmethodID MID_updateValues = NULL;

	PORT_ACCESS_FROM_ENV(env);

	/* Check whether MemoryUsage class has already been resolved and cached. If not, do so now.*/
	if (NULL == JCL_CACHE_GET(env, MID_com_ibm_lang_management_MemoryUsage_updateValues)) {
		jclass localMemoryUsageRef;

		/* Find the class ID for the Java class MemoryUsage and cache.*/
		localMemoryUsageRef = (*env)->GetObjectClass(env, memUsageObject);

		/* Convert this into a Global reference. */
		CID_MemoryUsage = (*env)->NewGlobalRef(env, localMemoryUsageRef);

		/* Delete the local reference. */
		(*env)->DeleteLocalRef(env, localMemoryUsageRef);
		if (NULL == CID_MemoryUsage) {
			return NULL;
		}

		/* Cache the resolved global class ID for MemoryUsage class. */
		JCL_CACHE_SET(env, CLS_com_ibm_lang_management_MemoryUsage, CID_MemoryUsage);

		/* Also, cache the method ID for updateValues() method. */
		MID_updateValues = (*env)->GetMethodID(env, CID_MemoryUsage, "updateValues", "(JJJJJJJ)V");
		if (NULL == MID_updateValues) {
			return NULL;
		}
		JCL_CACHE_SET(env, MID_com_ibm_lang_management_MemoryUsage_updateValues, MID_updateValues);
	} else {
		MID_updateValues = JCL_CACHE_GET(env, MID_com_ibm_lang_management_MemoryUsage_updateValues);
	}

	/* Call port library routine to collect memory usage statistics on current system. */
	rc = j9sysinfo_get_memory_info(&memInfo);
	if (0 != rc) {
		handle_error(env, rc, MEMORY_USAGE_ERROR);
		return NULL;
	}

	/* Invoke the updateValues() method on the MemoryUsage object so as to initialize the fields
	 * in that instance with the values obtained from j9sysinfo_get_memory_info().
	 */
	(*env)->CallVoidMethod(env,
				   memUsageObject,
				   MID_updateValues,
				   (jlong)memInfo.totalPhysical,
				   (jlong)memInfo.availPhysical,
				   (jlong)memInfo.totalSwap,
				   (jlong)memInfo.availSwap,
				   (jlong)memInfo.cached,
				   (jlong)memInfo.buffered,
				   (jlong)memInfo.timestamp);

	return memUsageObject;
}

/**
 * Retrieves the configured number of processors on the current machine.
 * [UNUSED] Currently there is no native function in the Java class
 * 	com.ibm.lang.management.internal.ExtendedOperatingSystem
 * by the name of getProcessorCountImpl() as this is not required or used
 * (most of the time we use online CPU count, than physical CPU count).
 * However, we let this be around and add a Java native only when needed.
 *
 * Class:     com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl
 * Method:    getProcessorCountImpl
 *
 * @param[in] env The JNI env.
 * @param[in] instance The this pointer.
 *
 * @return processor count; if not available on current platform, -1 is returned.
 */
jint JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getProcessorCountImpl(JNIEnv *env, jobject instance)
{
	PORT_ACCESS_FROM_ENV(env);
	jint result = (jint)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL);
	if (0 == result) {
		result = -1;
	}
	return result;
}

/**
 * @brief Call j9sysinfo_get_number_CPUs_by_type to get the instantaneous online CPU count.
 * Class:     com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl
 * Method:    getOnlineProcessorsImpl
 *
 * @param[in] env The JNI env.
 * @param[in] instance The this pointer.
 * @return Online CPU count.
 */
jint JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getOnlineProcessorsImpl(JNIEnv *env, jobject instance)
{
	jint onlineCPUs = 0;
	PORT_ACCESS_FROM_ENV(env);

	onlineCPUs = (jint) j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);

	/* If the port-library failed determining the online CPU count, return 1 as the minimum CPU count. */
	return (0 != onlineCPUs) ? onlineCPUs : 1;
}

/**
 * Retrieve hardware model
 *
 * @param env The JNI env.
 * @param obj bean instance.
 *
 * @return String containing the hardware model. NULL in case of an error.
 * @throws UnsupportedOperationException if the operation is not implemented on this platform. 
 * UnsupportedOperationException will also be thrown if the operation is implemented but it 
 * cannot be performed because the system does not satisfy all the requirements, for example,
 * an OS service is not installed.
 */
jstring JNICALL
Java_com_ibm_lang_management_internal_ExtendedOperatingSystemMXBeanImpl_getHardwareModelImpl(JNIEnv *env, jobject obj)
{
	const char * str = NULL;
	I_32 rc = 0;
	char buf[J9PORT_SYSINFO_HW_INFO_MODEL_BUF_LENGTH];
	PORT_ACCESS_FROM_ENV(env);

	rc = j9sysinfo_get_hw_info(J9PORT_SYSINFO_GET_HW_INFO_MODEL, buf, sizeof(buf));

	switch (rc) {
	case J9PORT_SYSINFO_GET_HW_INFO_SUCCESS:
		str = buf;
		break;
	case J9PORT_SYSINFO_GET_HW_INFO_NOT_AVAILABLE:
		throwNewUnsupportedOperationException(env);
		break;
	default:
		break;
	}

	return (NULL == str) ? NULL : (*env)->NewStringUTF(env, str);
}

/**
 * Returns the maximum number of file descriptors that can be opened in a process.
 *
 * Class: com_ibm_lang_management_internal_UnixExtendedOperatingSystem
 * Method: getMaxFileDescriptorCountImpl
 *
 * @param[in] env The JNI env.
 * @param[in] theClass The bean class.
 *
 * @return The maximum number of file descriptors that can be opened in a process;
 * -1 on failure.  If this is set to unlimited on the OS, simply return the signed 
 * integer (long long) limit.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_UnixExtendedOperatingSystem_getMaxFileDescriptorCountImpl(JNIEnv *env, jclass theClass)
{
	U_32 rc = 0;
	U_64 limit = 0;
	PORT_ACCESS_FROM_ENV(env);
	rc = j9sysinfo_get_limit(OMRPORT_RESOURCE_FILE_DESCRIPTORS, &limit);
	if (OMRPORT_LIMIT_UNKNOWN == rc) { /* The port library failed! */
		limit = ((U_64) -1);
	} else if (OMRPORT_LIMIT_UNLIMITED == rc) { /* No limit set (i.e., "unlimited"). */
		limit = ((U_64) LLONG_MAX);
	}
	return ((jlong) limit);
}

/**
 * Returns the current number of file descriptors that are in opened state.
 *
 * Class: com_ibm_lang_management_internal_UnixExtendedOperatingSystem
 * Method: getOpenFileDescriptorCountImpl
 *
 * @param[in] env The JNI env.
 * @param[in] theClass The bean class.
 *
 * @return The current number of file descriptors that are in opened state;
 * -1 on failure.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_UnixExtendedOperatingSystem_getOpenFileDescriptorCountImpl(JNIEnv *env, jclass theClass)
{
	I_32 ret = 0;
	U_64 count = 0;
	PORT_ACCESS_FROM_ENV(env);
	ret = j9sysinfo_get_open_file_count(&count);
	/* Check if an error occurred while obtaining the open file count. */
	if (ret < 0) {
		count = ((U_64) -1);
	}
	return ((jlong) count);
}
