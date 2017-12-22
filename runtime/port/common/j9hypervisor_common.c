/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief  Functions common to Hypervisor
 */

#include <string.h>
#include "j9comp.h"
#include "j9port.h"
#include "j9hypervisor.h"
#if defined(LINUXPPC) || defined(LINUXPPC64) || defined(AIXPPC)
#include "j9hypervisor_systemp.h"
#elif defined(J9X86) || defined(J9HAMMER) || defined(WIN32)
#include "j9hypervisor_vmware.h"
#elif defined(S390) || defined(J9ZOS390)
#include "j9hypervisor_systemz.h"
#endif
#include "portnls.h"
#include "portpriv.h"
#include "ut_j9prt.h"
#include "omrutil.h"
#include "vmargs_core_api.h"

/**
 * Save the error message provided into J9HypervisorData.vendorErrMsg ptr
 *
 * @param [in]  portLibrary - The Port Library Handle
 * @param [in]  errMsg String containing the error message
 *
 * @return                   nothing
 */
void
save_error_message(struct J9PortLibrary *portLibrary, char *errMsg)
{
	uintptr_t stringLen = 0;
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

	/* Allocate space for null terminator and braces */
	stringLen = strlen(errMsg) + 3;
	PHD_vendorErrMsg = omrmem_allocate_memory(stringLen, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL != PHD_vendorErrMsg) {
		/* Store error message in the J9HypervisorData pointer */
		omrstr_printf((char *)PHD_vendorErrMsg, stringLen, "(%s)", errMsg);
	}
}

/**
 * Check if the hypervisor guest startup has failed. If so find the reason
 * and cache an error message
 *
 * @param [in] portLibrary - The Port Library Handle
 * @param [in] errorCode   - error code to be mapped
 *
 * @return                   nothing
 */
void
check_and_save_hypervisor_startup_error(struct J9PortLibrary *portLibrary, intptr_t errorCode)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	const char *errMsg = NULL;

	switch(errorCode) {
		case J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR:
			errMsg = omrnls_lookup_message(J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_PORT_NO_HYPERVISOR__MODULE,
						J9NLS_PORT_NO_HYPERVISOR__ID,
						NULL);
			break;

		case J9PORT_ERROR_HYPERVISOR_UNSUPPORTED:
			errMsg = omrnls_lookup_message(J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_PORT_UNSUPPORTED_HYPERVISOR__MODULE,
						J9NLS_PORT_UNSUPPORTED_HYPERVISOR__ID,
						NULL);
			break;

		default:
			/* Vendor specific errors will already have been saved */
			break;
	}

	if (NULL != errMsg) {
		save_error_message(portLibrary, (char *) errMsg);
	}
}

/**
 * Startup stub.
 *
 * @param [in] portLibrary - The Port Library Handle
 *
 * @return	Return success (0) except when the IBM_JAVA_HYPERVISOR_SETTINGS
 * 			environment variable is set wrongly or monitor initialization fails.
 * 			Return negative error code on failure.
 */
int32_t
j9hypervisor_startup(struct J9PortLibrary *portLibrary)
{
	intptr_t ret = 0;
	intptr_t hypervisorStatus = 0;

	PHD_hypFunc.get_guest_processor_usage = NULL;
	PHD_hypFunc.get_guest_memory_usage = NULL;
	PHD_hypFunc.hypervisor_impl_shutdown = NULL;
	PHD_vendorPrivateData = NULL;
	PHD_vendorStatus = J9HYPERVISOR_NOT_INITIALIZED;
	PHD_vendorErrMsg = NULL;

	/* Initialize the hypervisor monitor. Needed for synchronization
	 * between multiple threads trying to call into the vendor specific
	 * hypervisor initialization
	 */
	ret = omrthread_monitor_init(&(PHD_vendorMonitor), 0);
	if (0 != ret) {
		/* If monitor initialization fails, fail JVM startup */
		return (int32_t)ret;
	}

	detect_hypervisor(portLibrary);

	hypervisorStatus = j9hypervisor_hypervisor_present(portLibrary);

	/*
	 * Fall-back mechanism for Hypervisor detection.
	 * i.e If the original detection detect_hypervisor() reported that there is
	 * no Hypervisor even when a Hypervisor is present. More specifically in
	 * cases where the CPUID instruction is unable to detect a Hypervisor.We
	 * check for the environment variable IBM_JAVA_HYPERVISOR_SETTINGS which is
	 * set by the user when it is known that the System is running on a Hypervisor
	 */
	if (J9HYPERVISOR_NOT_PRESENT == hypervisorStatus) {
		ret = detect_hypervisor_from_env(portLibrary);
		/* Malformed env variable, fail JVM startup */
		if (J9PORT_ERROR_HYPERVISOR_ENV_VAR_MALFORMED == ret) {
			omrthread_monitor_destroy(PHD_vendorMonitor);
			return (int32_t)ret;
		}
	}

	/* Always return zero */
	return 0;
}

/**
 * Shut down stub.
 * @param [in] portLibrary - The Port Library Handle
 *
 * @return                   nothing
 */
void
j9hypervisor_shutdown(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	/* Call the hypervisor_impl_shutdown function to cleanup hypervisor private data */
	if (NULL != PHD_hypFunc.hypervisor_impl_shutdown) {
		PHD_hypFunc.hypervisor_impl_shutdown(portLibrary);
	}

	/* dealloc the hypervisor monitor */
	if (NULL != PHD_vendorMonitor) {
		omrthread_monitor_destroy(PHD_vendorMonitor);
	}

	/* If a error message was cached during startup, free message string */
	if (NULL != PHD_vendorErrMsg) {
		omrmem_free_memory(PHD_vendorErrMsg);
	}
}

/**
 * Returns whether the application is running on a Hypervisor or not
 *
 * @param [in] J9PortLibrary portLibrary the Port Library.
 *
 * @return J9HYPERVISOR_NOT_PRESENT if hypervisor is not present,
 * 		   J9HYPERVISOR_PRESENT if hypervisor is present,
 * 		   negative error code on failure
 */
intptr_t
j9hypervisor_hypervisor_present(struct J9PortLibrary *portLibrary)
{
	return PHD_isVirtual;
}

/**
 * Fills in the provided J9HypervisorVendorDetails structure.
 * Caller is responsible for storage of the structure,although the string hypervisorName in the
 * J9HypervisorVendorDetails structure should NOT be freed.
 *
 * @param [in] J9PortLibrary portLibrary the Port Library.
 * @param [out] J9HypervisorVendorDetails vendor the structure to be filled in
 *
 * @return  0 on Success, negative error code on failure
 */
intptr_t
j9hypervisor_get_hypervisor_info(struct J9PortLibrary *portLibrary, J9HypervisorVendorDetails *vendorDetails)
{
	intptr_t ret = 0;

	if (NULL == vendorDetails) {
		return J9PORT_ERROR_INVALID_ARGUMENTS;
	}

	ret = j9hypervisor_hypervisor_present(portLibrary);
	switch (ret) {
		case J9HYPERVISOR_PRESENT:
			vendorDetails->hypervisorName = PHD_vendorDetails.hypervisorName;
			ret = 0;
			break;

		case J9HYPERVISOR_NOT_PRESENT:
			ret = J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR;
			/* Fall through */
		default:
			/* Error during Hypervisor detection */
			vendorDetails->hypervisorName = NULL;
			break;
	}
	return ret;
}

/**
 * Call the vendor specific hypervisor initialization if we are running on
 * a hypervisor. This will be called the first time either the
 * j9hypervisor_get_guest_processor_usage() or
 * j9hypervisor_get_guest_memory_usage API are called.
 * 
 * @param [in]  portLibrary The Port Library
 *
 * @return 0 on success and a negative value on failure
 */
intptr_t
j9hypervisor_vendor_init(struct J9PortLibrary *portLibrary)
{
	intptr_t ret = 0;
	intptr_t hypervisorStatus = 0;
	char *vendorName = NULL;

	omrthread_monitor_enter(PHD_vendorMonitor);

	/* This code can only be run once. vendorStatus is J9HYPERVISOR_NOT_INITIALIZED
	 * only for the first time. If it is not J9HYPERVISOR_NOT_INITIALIZED, the current
	 * thread got here too late and the init has already happened. Just return the
	 * stored status value in that case.
	 */
	if (J9HYPERVISOR_NOT_INITIALIZED != PHD_vendorStatus) {
		omrthread_monitor_exit(PHD_vendorMonitor);
		return PHD_vendorStatus;
	}

	PHD_vendorStatus = J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;

	/* Check if we are running on a hypervisor */
	hypervisorStatus = j9hypervisor_hypervisor_present(portLibrary);

	/* If we are not running on a hypervisor, cache status for later use */
	if (J9HYPERVISOR_PRESENT != hypervisorStatus) {
		PHD_vendorStatus = J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR;
		goto done;
	}

	/* Check the hypervisor we are on currently using info that gets setup early in JVM startup */
	vendorName = (char *) PHD_vendorDetails.hypervisorName;

	/*
	 * Call the appropriate hypervisor startup code.  The error will be cached for later
	 * use by the called function.
	 */
#if defined(LINUXPPC) || defined(LINUXPPC64) || defined(AIXPPC)
	if ((0 == strcmp(vendorName, HYPE_NAME_POWERVM)) || 
		(0 == strcmp(vendorName, HYPE_NAME_POWERKVM))) {
		ret = systemp_startup(portLibrary);
	}
#elif defined(J9X86) || defined(J9HAMMER) || defined(WIN32)
#if defined(J9PORT_HYPERVISOR_VMWARE_SUPPORT)
	if (0 == strcmp(vendorName, HYPE_NAME_VMWARE)) {
		ret = vmware_startup(portLibrary);
	}
#endif /* J9PORT_HYPERVISOR_VMWARE_SUPPORT */
#elif (defined(S390) && !defined(J9ZTPF)) || defined(J9ZOS390)
	if (0 == strcmp(vendorName, HYPE_NAME_ZVM) || 0 == strcmp(vendorName, HYPE_NAME_PRSM)) {
		ret = systemz_startup(portLibrary);
	}
#endif

done:
	check_and_save_hypervisor_startup_error(portLibrary, PHD_vendorStatus);
	omrthread_monitor_exit(PHD_vendorMonitor);
	Trc_PRT_j9hypervisor_vendor_init_exit(ret);
	return ret;
}

/* Initialize vendor-specific data area in a protected way.
 * @param [in] portLibrary The port library
 *
 * @return 0 on success; non-0 on failure
 */  
static intptr_t
initializeHypervisorState(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t ret = 0;

	Trc_PRT_initializeHypervisorState_Entered();

	if (J9_UNEXPECTED(J9HYPERVISOR_NOT_INITIALIZED == PHD_vendorStatus)) {
	
		/* Vendor-specific initialization hasn't been done yet, do it as a first timer. */
		ret = j9hypervisor_vendor_init(portLibrary);
		if (ret < 0) {
			Trc_PRT_initializeHypervisorState_initFailed(ret);
			if (NULL != PHD_vendorErrMsg) {
				omrerror_set_last_error_with_message((int32_t) ret, PHD_vendorErrMsg);
			}
		}
	} else if (HYPERVISOR_VENDOR_INIT_SUCCESS != PHD_vendorStatus) {
	
		/* Check if the initialization has failed (while holding the monitor). */
		omrthread_monitor_enter(PHD_vendorMonitor);

		if (J9_UNEXPECTED(HYPERVISOR_VENDOR_INIT_SUCCESS != PHD_vendorStatus)) {
			/* Initialization failed! */
			Trc_PRT_initializeHypervisorState_initFailed(PHD_vendorStatus);
			if (NULL != PHD_vendorErrMsg) {
				omrerror_set_last_error_with_message( PHD_vendorStatus, PHD_vendorErrMsg);
			}
		}

		ret = PHD_vendorStatus;
		omrthread_monitor_exit(PHD_vendorMonitor);
	}

	Trc_PRT_initializeHypervisorState_Exit(ret);
	return ret;
}

/**
 * Calls the vendor specific hypervisor functions to retrieve guest processor
 * statistics and returns an updated J9GuestProcessorUsage structure that is
 * provided
 *
 * @param [in]  portLibrary The Port Library
 * @param [out] gpUsage The structure to be filled in
 *
 * @return 0 on success and a negative value on failure
 */
intptr_t
j9hypervisor_get_guest_processor_usage(struct J9PortLibrary *portLibrary, J9GuestProcessorUsage *gpUsage)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t ret = 0;

	Trc_PRT_j9hypervisor_get_guest_processor_usage_enter();

	if (NULL == gpUsage) {
		ret = J9PORT_ERROR_INVALID_ARGUMENTS;
		Trc_PRT_j9hypervisor_get_guest_processor_usage_arg_null(ret);
		Trc_PRT_j9hypervisor_get_guest_processor_usage_exit(ret);
		return ret;
	}
	memset(gpUsage, 0, sizeof(J9GuestProcessorUsage));

	/* If the hypervisor vendor initialization has not happened yet, do it now. If this fails, no
	 * point in proceeding as we haven't setup function pointers to valid functions for the particular
	 * hypervisor-specific API's.
	 */
	ret = initializeHypervisorState(portLibrary);
	if (0 != ret) {
		Trc_PRT_j9hypervisor_get_guest_processor_usage_hypervisor_error(ret);

		/* If a vendor-specific error message is available, it is already set as the last error 
		 * message, at this stage. Simply return the error code indicating failure.
		 */
		Trc_PRT_j9hypervisor_get_guest_processor_usage_exit(ret);
		return ret;
	}

	/*
	 * hypFunc should be non NULL if we are running on a hypervisor and if the guest OS is
	 * setup right to call into the hypervisor. If NULL, return cached error
	 */
	if (NULL == PHD_hypFunc.get_guest_processor_usage) {
		ret = PHD_vendorStatus;
		Trc_PRT_j9hypervisor_get_guest_processor_usage_hypervisor_error(ret);
		if (NULL != PHD_vendorErrMsg) {
			omrerror_set_last_error_with_message((int32_t) ret, PHD_vendorErrMsg);
		}
		Trc_PRT_j9hypervisor_get_guest_processor_usage_exit(ret);
		return ret;
	}

	/* Call the stored function pointer to get processor utilization data */
	ret = PHD_hypFunc.get_guest_processor_usage(portLibrary, gpUsage);
	Trc_PRT_j9hypervisor_get_guest_processor_usage_exit(ret);
	return ret;
}

/**
 * Calls the vendor specific hypervisor functions to retrieve guest memory
 * statistics and returns an updated J9GuestMemoryUsage structure that is
 * provided
 *
 * @param [in]  portLibrary The Port Library
 * @param [out] gmUsage The structure to be filled in
 *
 * @return 0 on success and a negative value on failure
 */
intptr_t
j9hypervisor_get_guest_memory_usage(struct J9PortLibrary *portLibrary, J9GuestMemoryUsage *gmUsage)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t ret = 0;

	Trc_PRT_j9hypervisor_get_guest_memory_usage_enter();

	if (NULL == gmUsage) {
		ret = J9PORT_ERROR_INVALID_ARGUMENTS;
		Trc_PRT_j9hypervisor_get_guest_memory_usage_arg_null(ret);
		Trc_PRT_j9hypervisor_get_guest_memory_usage_exit(ret);
		return ret;
	}
	memset(gmUsage, 0, sizeof(J9GuestMemoryUsage));

	/* If the hypervisor vendor initialization has not happened yet, do it now. If this fails, no
	 * point in proceeding as we haven't setup function pointers to valid functions for the particular
	 * hypervisor-specific API's.
	 */
	ret = initializeHypervisorState(portLibrary);
	if (0 != ret) {
		Trc_PRT_j9hypervisor_get_guest_memory_usage_hypervisor_error(ret);

		/* If a vendor-specific error message is available, it is already set as the last error 
		 * message, at this stage. Simply return the error code indicating failure.
		 */
		Trc_PRT_j9hypervisor_get_guest_memory_usage_exit(ret);
		return ret;
	}

	/*
	 * hypFuncPtr should be non NULL if we are running on a hypervisor and if the guest OS is
	 * setup right to call into the hypervisor. If NULL, return cached error
	 */
	if (NULL == PHD_hypFunc.get_guest_memory_usage) {
		ret = PHD_vendorStatus;
		Trc_PRT_j9hypervisor_get_guest_memory_usage_hypervisor_error(ret);
		if (NULL != PHD_vendorErrMsg) {
			omrerror_set_last_error_with_message((int32_t) ret, PHD_vendorErrMsg);
		}
		Trc_PRT_j9hypervisor_get_guest_memory_usage_exit(ret);
		return ret;
	}

	ret = PHD_hypFunc.get_guest_memory_usage(portLibrary, gmUsage);
	Trc_PRT_j9hypervisor_get_guest_memory_usage_exit(ret);
	return ret;
}

#define HYPERVISOR_NAME_MAX 20
#define HYPERVISOR_DEFAULTNAME_STR "DefaultName="
/**
 * Fall back for detecting the Hypervisor when the automatic detection fails
 * Parses the environment variable "IBM_JAVA_HYPERVISOR_SETTINGS" and set the
 * Hypervisor details.
 *
 * @param [in] J9PortLibrary portLibrary the Port Library.
 *
 * @return 0 on Success, negative error code on failure
 */
intptr_t
detect_hypervisor_from_env(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t rc = 0;
	char *tmpPtr = NULL;
	char *envHypervisorVal = NULL;
	J9JavaVMArgInfo *current = NULL;
	J9JavaVMArgInfoList vmArgumentsList;
	char hypervisorName[HYPERVISOR_NAME_MAX] = {0};
	const char *envHypervisor = ENVVAR_IBM_JAVA_HYPERVISOR_SETTINGS;
	intptr_t envHypervisorSize = omrsysinfo_get_env(envHypervisor, NULL, 0);

	if (envHypervisorSize <= 0) {
		/* IBM_JAVA_HYPERVISOR_SETTINGS variable not set */
		return J9PORT_ERROR_HYPERVISOR_ENV_NOT_SET;
	}

	envHypervisorVal = omrmem_allocate_memory(envHypervisorSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == envHypervisorVal) {
		return J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED;
	}
	omrsysinfo_get_env(envHypervisor, envHypervisorVal, envHypervisorSize);

	vmArgumentsList.pool = pool_new(sizeof(J9JavaVMArgInfo),
				4, /* max = 1, over allocate for now */
				0, 0,
				J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(portLibrary));
	if (NULL == vmArgumentsList.pool) {
		/* Explicitly call free memory here instead of at clean_up
		 * This ensures that there is no double free in some cases
		 */
		omrmem_free_memory(envHypervisorVal);
		rc = J9PORT_ERROR_HYPERVISOR_MEMORY_ALLOC_FAILED;
		goto clean_up;
	}
	vmArgumentsList.head = NULL;
	vmArgumentsList.tail = NULL;

	rc = parseHypervisorEnvVar(portLibrary, envHypervisorVal, &vmArgumentsList);
	if (rc < 0) {
		omrnls_printf(J9NLS_ERROR, J9NLS_PORT_MALFORMED_HYPERVISOR_SETTINGS);
		rc = J9PORT_ERROR_HYPERVISOR_ENV_VAR_MALFORMED;
		goto free_mem;
	}

	/* Re-setting the isVirtual value to J9PORT_ERROR_HYPERVISOR_UNSUPPORTED
	 * since this value would have been filled up with the error from detect_hypervisor()
	 * Also, setting this after the above two error conditions because if the
	 * Env variable itself is not present or parsing is unsuccessful we do not
	 * want to overwrite the actual error code that was set previously.
	 * The Hypervisor Name will be already filled up with the default value(NULL)
	 */
	PHD_isVirtual = J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
	
	current = vmArgumentsList.head;
	/* Iterate through the list and fill the HypervisorData Structure */
	while (NULL != current) {
		/* Look for the "DefaultName=" string */
		tmpPtr = strstr(current->vmOpt.optionString, HYPERVISOR_DEFAULTNAME_STR);
		if (NULL != tmpPtr) {
			tmpPtr += sizeof(HYPERVISOR_DEFAULTNAME_STR) - 1;
			strncpy(hypervisorName, tmpPtr, HYPERVISOR_NAME_MAX);
			hypervisorName[HYPERVISOR_NAME_MAX -1] = '\0';

			/* Check for the validity of the Hypervisor Name before setting the
			 * structure
			 */
			if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_VMWARE)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_VMWARE;
			} else if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_KVM)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_KVM;
			} else if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_POWERVM)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_POWERVM;
			} else if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_ZVM)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_ZVM;
			} else if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_HYPERV)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_HYPERV;
			} else if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_PRSM)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_PRSM;
			} else if (0 == j9_cmdla_stricmp(hypervisorName, HYPE_NAME_POWERKVM)) {
				PHD_vendorDetails.hypervisorName = HYPE_NAME_POWERKVM;
			} else {
				/* Not a supported Hypervisor */ 
				rc = J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
				goto free_mem;
			}
			PHD_isVirtual = J9HYPERVISOR_PRESENT;
		}
		current = current->next;
	}

free_mem:
	current = vmArgumentsList.head;
	while (current) {
		J9CmdLineOption* j9Option = &current->cmdLineOpt;

		if (ARG_MEMORY_ALLOCATION == (j9Option->flags & ARG_MEMORY_ALLOCATION)) {
			omrmem_free_memory(current->vmOpt.optionString);
		}
		current = current->next;
	}

clean_up:
	pool_kill(vmArgumentsList.pool);
	return rc;
}
