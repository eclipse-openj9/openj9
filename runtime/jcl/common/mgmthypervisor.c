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

#include <assert.h>

#include "j9.h"
#include "j9jclnls.h"
#include "portnls.h"
#include "j9port.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jni.h"

/* Macro for the error code for Hypervisor detection not supported
 * NOTE: This error code should be in sync with the HypervisorMXBeanImpl.java
 */
#define HYPERVISOR_UNSUPPORTED -100001

#define MESSAGE_STRING_LENGTH 512

/**
* Checks if running on a Virtualized environment or not. i.e On a Hypervisor or not
*
* Class:     com_ibm_virtualization_management_internal_HypervisorMXBeanImpl
* Method:    isEnvironmentVirtual
*
* @param[in] env The JNI env.
* @param[in] obj The this pointer.
*
* @return - 0 - If not running on a Hypervisor, 1- If Running on a Hypervisor
* 			Negative value on Error.
*/
jint JNICALL
Java_com_ibm_virtualization_management_internal_HypervisorMXBeanImpl_isEnvironmentVirtualImpl(JNIEnv *env, jobject obj)
{
	PORT_ACCESS_FROM_ENV(env);
	IDATA rc = 0;
	rc = j9hypervisor_hypervisor_present();

	/* Return Hypervisor detection not supported */
	if (rc == J9PORT_ERROR_HYPERVISOR_UNSUPPORTED) {
		return (jint)(HYPERVISOR_UNSUPPORTED);
	}
	/* Return the value stored returned by j9hypervisor_hypervisor_present for all other cases */
	return (jint)rc;
}

/**
* Retrieves the Hypervisor Vendor Name
*
* Class:     com_ibm_virtualization_management_internal_HypervisorMXBeanImpl
* Method:    getVendor
*
* @param[in] env The JNI env.
* @param[in] obj The this pointer.
*
* @return -  String containing the Hypervisor Vendor Name, NULL in case of No Hypervisor
*/
jstring JNICALL
Java_com_ibm_virtualization_management_internal_HypervisorMXBeanImpl_getVendorImpl(JNIEnv *env, jobject obj)
{
	PORT_ACCESS_FROM_ENV(env);
	J9HypervisorVendorDetails vendor;
	if(0 == j9hypervisor_get_hypervisor_info(&vendor)) {
		return ((*env)->NewStringUTF(env, (char *)vendor.hypervisorName));
	}
	return NULL; /* Error returned by j9hypervisor_get_hypervisor_info, Hypervisor name is NULL */
}

static const struct { char *name; } objType[] = { 
	{ "GuestOS Processor Usage" },
	{ "GuestOS Memory Usage" },
};

typedef enum {
	GUEST_PROCESSOR = 0,
	GUEST_MEMORY
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

	assert((type == GUEST_PROCESSOR || type == GUEST_MEMORY));

	/* If out of memory setup a pending OutOfMemoryError */
	if (J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED == error) {
		((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, J9NLS_PORT_HYPERVISOR_OUT_OF_MEMORY_ERROR_MSG);
		return 0;
	}

	/* For all other errors setup a GuestOSInfoRetrievalException exception */
	/* Read in the generic usage retrieval error string */
	formatString = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_JCL_HYPERVISOR_USAGE_RETRIEVAL_ERROR_MSG,
					NULL);
	/* Add in the specific error and the type. Append the port library specific error. */
	j9str_printf(PORTLIB,
		exceptionMessage,
		sizeof(exceptionMessage),
		(char *)formatString,
		error,
		objType[type].name,
		j9error_last_error_message());

	exceptionClass = (*env)->FindClass(env, "com/ibm/virtualization/management/GuestOSInfoRetrievalException");
	if (NULL != exceptionClass) {
		(*env)->ThrowNew(env, exceptionClass, exceptionMessage);
	}

	return 0;
}

/**
 * Returns guest processor usage statistics as reported by the hypervisor host
 *
 * @param[in] env	The JNI env.
 * @param[in] beanInstance	The this pointer.
 * @param[in/out] procUsageObject		User supplied J9GuestProcessorUsage object
 *
 * @return				J9GuestProcessorUsage object on success or NULL on error
 */
jobject JNICALL
Java_com_ibm_virtualization_management_internal_GuestOS_retrieveProcessorUsageImpl(JNIEnv *env, jobject beanInstance, jobject procUsageObject)
{
	PORT_ACCESS_FROM_ENV(env);
	IDATA rc = 0;
	jmethodID MID_updateValues = NULL;
	jclass CLS_GuestOSProcessorUsage = NULL;
	J9GuestProcessorUsage procUsage;

	/* Check if the GuestOSProcessorUsage class has been cached */
	CLS_GuestOSProcessorUsage = JCL_CACHE_GET(env, CLS_java_com_ibm_virtualization_management_GuestOSProcessorUsage);
	if (NULL == CLS_GuestOSProcessorUsage) {
		jclass GuestOSProcessorUsageID = NULL;

		/* Get the class ID */
		GuestOSProcessorUsageID = (*env)->GetObjectClass(env, procUsageObject);
		if (NULL == GuestOSProcessorUsageID) {
			return NULL;
		}

		/* Convert to a global reference and delete the local one */
		CLS_GuestOSProcessorUsage = (*env)->NewGlobalRef(env, GuestOSProcessorUsageID);
		(*env)->DeleteLocalRef(env, GuestOSProcessorUsageID);
		if (NULL == CLS_GuestOSProcessorUsage) {
			return NULL;
		}

		JCL_CACHE_SET(env, CLS_java_com_ibm_virtualization_management_GuestOSProcessorUsage, CLS_GuestOSProcessorUsage);

		/* Get the method ID for updateValues() method */
		MID_updateValues = (*env)->GetMethodID(env, CLS_GuestOSProcessorUsage, "updateValues", "(JJFJ)V");
		if (NULL == MID_updateValues) {
			return NULL;
		}
		JCL_CACHE_SET(env, MID_java_com_ibm_virtualization_management_GuestOSProcessorUsage_updateValues, MID_updateValues);
	} else {
		MID_updateValues = JCL_CACHE_GET(env, MID_java_com_ibm_virtualization_management_GuestOSProcessorUsage_updateValues);
	}

	/* Call port library routine to get guest processor usage statistics */
	rc = j9hypervisor_get_guest_processor_usage(&procUsage);
	if (0 != rc) {
		handle_error(env, rc, GUEST_PROCESSOR);
		return NULL;
	}

	/* Call the update values method to update the object with the values obtained */
	(*env)->CallVoidMethod(env,
				procUsageObject,
				MID_updateValues,
				(jlong)procUsage.cpuTime,
				(jlong)procUsage.timestamp,
				(jfloat)procUsage.cpuEntitlement,
				(jlong)procUsage.hostCpuClockSpeed);

	return procUsageObject;
}

/**
 * Returns guest memory usage statistics as reported by the hypervisor host
 *
 * @param[in] env	The JNI env.
 * @param[in] beanInstance	The this pointer.
 * @param[in/out] memUsageObject	User supplied J9GuestMemoryUsage object
 *
 * @return				J9GuestMemoryUsage object on success or NULL on error
 */
jobject JNICALL
Java_com_ibm_virtualization_management_internal_GuestOS_retrieveMemoryUsageImpl(JNIEnv *env, jobject beanInstance, jobject memUsageObject)
{
	PORT_ACCESS_FROM_ENV(env);
	IDATA rc = 0;
	jmethodID MID_updateValues = NULL;
	J9GuestMemoryUsage memUsage;
	jclass CLS_GuestOSMemoryUsage = NULL;

	/* Check if the GuestOSMemoryUsage class has been cached */
	CLS_GuestOSMemoryUsage = JCL_CACHE_GET(env, CLS_java_com_ibm_virtualization_management_GuestOSMemoryUsage);
	if (NULL == CLS_GuestOSMemoryUsage)
	{
		jclass GuestOSMemoryUsageID = NULL;

		/* Get the class ID */
		GuestOSMemoryUsageID = (*env)->GetObjectClass(env, memUsageObject);
		if (NULL == GuestOSMemoryUsageID) {
			return NULL;
		}

		/* Convert to a global reference and delete the local one */
		CLS_GuestOSMemoryUsage = (*env)->NewGlobalRef(env, GuestOSMemoryUsageID);
		(*env)->DeleteLocalRef(env, GuestOSMemoryUsageID);
		if (NULL == CLS_GuestOSMemoryUsage) {
			return NULL;
		}

		JCL_CACHE_SET(env, CLS_java_com_ibm_virtualization_management_GuestOSMemoryUsage, CLS_GuestOSMemoryUsage);

		/* Get the method ID for updateValues() method */
		MID_updateValues = (*env)->GetMethodID(env, CLS_GuestOSMemoryUsage, "updateValues", "(JJJ)V");
		if (NULL == MID_updateValues) {
			return NULL;
		}
		JCL_CACHE_SET(env, MID_java_com_ibm_virtualization_management_GuestOSMemoryUsage_updateValues, MID_updateValues);
	} else {
		MID_updateValues = JCL_CACHE_GET(env, MID_java_com_ibm_virtualization_management_GuestOSMemoryUsage_updateValues);
	}

	/* Call port library routine to get guest memory usage statistics */
	rc = j9hypervisor_get_guest_memory_usage(&memUsage);
	if (0 != rc) {
		handle_error(env, rc, GUEST_MEMORY);
		return NULL;
	}

	/* Call the update values method to update the object with the values obtained */
	(*env)->CallVoidMethod(env,
				memUsageObject,
				MID_updateValues,
				(jlong)memUsage.memUsed,
				(jlong)memUsage.timestamp,
				(jlong)memUsage.maxMemLimit);

	return memUsageObject;
}
