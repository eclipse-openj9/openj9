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
/**
 * @file
 * @ingroup PortTest
 * @brief Hypervisor tests
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "testHelpers.h"
#include "j9hypervisor.h"

/* The capability as got by hv:xxxx */
#define VMWARE	"vmware"
#define KVM	"kvm"
#define POWERVM	"powervm"
#define HYPERV	"hyperv"
#define ZVM	"zvm"
#define PRSM	"prsm"
#define POWERKVM "powerkvm"

#define HYPFS		"hypfs"
#define NOHYPFS		"nohypfs"
#define NOUSERHYPFS	"nouserhypfs"

#define  GET_GUEST_USAGE_ITERATIONS 5

/* CPU speeds are expressed in platform-dependent units. */
#if defined(J9ZOS390)
/* On Z/OS, CPU speed is expressed in Millions of Service Units (MSU). */
#define CPUSPEEDUNIT "MSU"
#else
/* On other platforms, CPU speed is expressed in MegaHertz (MHz). */
#define CPUSPEEDUNIT "MHz"
#endif

#ifdef LINUXPPC
#define LINUX_PPC_CONFIG	"/proc/ppc64/lparcfg"
#define LINUXPPC_VERSION_STRING	"lparcfg"
#define MAXSTRINGLENGTH		128
/*
 * Helper function to check the lparcfg version on Linux PPC
 *
 * @param[in] portLibrary The port library
 *
 * @return	0 - If the lparcfg version is < 1.8
 *		1 - If the lparcfg version is >= 1.8
 *		-1 - If there is error reading the file /proc/ppc64/lparcfg
 */
int is_lparcfg_version_supported()
{
	int rc = -1;
	FILE *lparcfgFP = fopen(LINUX_PPC_CONFIG, "r");

	if (NULL != lparcfgFP) {
		while (0 == feof(lparcfgFP)) {
			char partitionString[MAXSTRINGLENGTH];
			char *tmpPtr = NULL;
			if (NULL == fgets(partitionString, sizeof(partitionString), lparcfgFP)) {
				break;
			}
			tmpPtr = strstr(partitionString, LINUXPPC_VERSION_STRING);
			if (NULL != tmpPtr) {
				tmpPtr += sizeof(LINUXPPC_VERSION_STRING);
				/* Check if the version is greater/equal to 1.8, if yes return 1, else return 0. */
				if (atof(tmpPtr) >= 1.8) {
					rc = 1;
				} else {
					rc = 0;
				}
				break;
			}
		}
		fclose(lparcfgFP);
	}

	return rc;
}
#endif /* LINUXPPC */

/*
 * Helper function to create a busy load for verifying the increase in the
 * CPU times
 *
 * @param[in]	portLibrary pointer to port library
 * @param[in]	secs The no of secs for which the busy load is executed
 *
 * @return	none
 */
void create_busy_load(J9PortLibrary* portLibrary, int secs) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	U_64 startTimeNs = 0;
	U_64 endTimeNs = 0;
	startTimeNs = j9time_hires_clock();
	do {
		endTimeNs = j9time_hires_clock();
	} while ((j9time_hires_delta(startTimeNs, endTimeNs, 1) <= secs));
}

/*
 * Helper function to validate the processor statistics
 *
 * @param[in]	portLibrary pointer to port library
 * @param[in]	currguestprocessorUsage the current Processor Usage
 * @param[in]	prevguestprocessorUsage previous Processor Usage
 *
 * @return 		0 on Successfully Validating the statistics, -1 on failure
 */
int validate_processor_stats(J9PortLibrary* portLibrary, J9GuestProcessorUsage currguestprocessorUsage, J9GuestProcessorUsage prevguestprocessorUsage) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = 0;

	/* Print the statistics for both the success and the failure case */
	outputComment(portLibrary, "\t Guest Processor Usage Statistics  at TimeStamp: %lld microseconds\n", currguestprocessorUsage.timestamp);
	outputComment(portLibrary, "\t Host CPU Clock Speed: %lld %s\n", currguestprocessorUsage.hostCpuClockSpeed, CPUSPEEDUNIT);
	/* On z/linux and z/OS CPU entitlement is not available. Enable test when available. */
#if !defined(J9ZOS390) && !(defined(LINUX) && defined(S390))
	outputComment(portLibrary, "\t The CPU Entitlement of the current VM: %f\n", currguestprocessorUsage.cpuEntitlement);
#else
	outputComment(portLibrary, "\t The CPU Entitlement of the current VM: <undefined>\n");
#endif
	outputComment(portLibrary, "\t The time during which the virtual machine has used the CPU: %lld microseconds\n", currguestprocessorUsage.cpuTime);
	outputComment(portLibrary, "\t--------------------------------------------\n");

	/* Validate the statistics
	 * - hostCpuClockSpeed of Guest OS is not 0
	 * - whether the timestamp has increased
	 * - whether the cpuTime has increased
	 * - cpuEntitlement is not 0
	 */

	/* Excluding the checks for hostCpuClockSpeed & cpuEntitlement
	 * for zLinux since they can be -1.
	 */
	if (
#if !(defined(LINUX) && defined(S390))
		/* On z/linux, hostCpuClockSpeed may not be available. So exclude this test. */
		(currguestprocessorUsage.hostCpuClockSpeed > 0) &&
#endif
		(currguestprocessorUsage.cpuTime > 0) &&
#if !defined(J9ZOS390)
		/* On z/OS, usage details are only stored for the previous 4 hours.
		 * The usage is not guaranteed to increase monotonically
		 */
		(currguestprocessorUsage.cpuTime >= prevguestprocessorUsage.cpuTime) &&
#endif
#if !(defined(LINUX) && defined(S390)) && !defined(J9ZOS390) && \
	!(defined(PPC64) && defined(J9VM_ENV_LITTLE_ENDIAN))
		/* CPU entitlement is not available on z/linux, z/OS, and PowerKVM. Disable this
		 * test, until this information is available. 
		 */
		(currguestprocessorUsage.cpuEntitlement > 0) &&
#endif
		(currguestprocessorUsage.timestamp > 0) &&
		(currguestprocessorUsage.timestamp >= prevguestprocessorUsage.timestamp)) {
		rc = 0; /* Return Success */
	} else {
		rc = -1; /* Return failure */
	}

	return rc;
}

/*
 * Test various aspect of j9hypervisor_hypervisor_present
 * 1. In a non-virtual environment
 * 2. In a virtual environment
 *
 * @param[in] portLibrary The port library
 * @param[in] hypervisor The name of the Hypervisor on which the test is run
 * @param[in] hvOptions   Hypervisor specific options if any
 *
 * @return the return value from @reportTestExit
 *
 */
int
j9hypervisor_test_hypervisor_present(J9PortLibrary* portLibrary, char* hypervisor, char *hvOptions, BOOLEAN negative)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9hypervisor_test_hypervisor_present";
	IDATA rc = 0;
	BOOLEAN isVirtual = FALSE;

	reportTestEntry(portLibrary, testName);
	if (NULL != hypervisor) {
		/* -hypervisor *was* specified */
		isVirtual = TRUE;
	}

	rc = j9hypervisor_hypervisor_present();
	if (!isVirtual) {
		/* Test for Non virtualized Environment */
		switch(rc) {
		case J9HYPERVISOR_NOT_PRESENT: {
			/* Pass case, since -hypervisor was NOT specified; neither was it detected. */
			outputComment(portLibrary,
				"\tTest Passed: No Hypervisor present and j9hypervisor_hypervisor_present()"
				"returned false\n");
			break;
		}
		case J9PORT_ERROR_HYPERVISOR_UNSUPPORTED: {
			/* Pass case; give benefit of doubt since hypervisor detection is NOT supported. */
			outputComment(portLibrary, "\tThis platform does not support Hypervisor detection\n");
			break;
		}
		case J9HYPERVISOR_PRESENT: {
			J9HypervisorVendorDetails vendDetails;

			/* Failure scenario, since we ARE running on hypervisor, and yet missing the -hypervisor
			   specification on the command line to test. */
			rc = j9hypervisor_get_hypervisor_info(&vendDetails);
			if (0 == rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS,
					"Expecting bare metal machine but is running on hypervisor: %s\n"
					"[Info]: Please verify the machine capabilities.\n",
					vendDetails.hypervisorName);
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS,
					"Expecting bare metal machine but is running on hypervisor: <unknown>\n",
					"[Info]: Failed to obtain hypervisor information: %i", rc);
			}
			break;
		}
		default: { /* Capture any possible error codes reported (other than hypdetect unsupported). */
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_hypervisor_present() returned: %i "
				"when 0 (J9HYPERVISOR_NOT_PRESENT) was expected\n", rc);
		}
		};
	} else {
		/* Test for Virtualized Environment */
		switch(rc) {
		case J9HYPERVISOR_PRESENT: {
			/* Pass case, since we detected a hypervisor as also found -hypervisor specified. */
			outputComment(portLibrary, "\tTest Passed: Expected a hypervisor and"
				" j9hypervisor_hypervisor_present() returned true\n");
			break;
		}
		case J9PORT_ERROR_HYPERVISOR_UNSUPPORTED: {
			if (negative) {
				/* If this is a negative test case then we expect hypervisor detection to return
				 * error code: J9PORT_ERROR_HYPERVISOR_UNSUPPORTED, when -hypervisor is specified.
				 */
				outputComment(portLibrary, "\tTest (negative case) passed: Expected an "
					"unsupported hypervisor.\n\tj9hypervisor_hypervisor_present() returned "
					"-856 (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED).\n");
			} else {
				/* Failed test if this is /not/ a negative test, since it looks like it indeed
				 * detected an unsupported hypervisor (other the ones passed for negative tests).
				 */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_hypervisor_present() "
					"returned: -856 (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED).\n");
			}
			break;
		}
		case J9HYPERVISOR_NOT_PRESENT: {
			outputErrorMessage(PORTTEST_ERROR_ARGS,
				"Expecting virtualized environment whereas no hypervisor detected\n");
			break;
		}
		default: {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_hypervisor_present() returned: "
				"%i when 1 (true) was expected\n", rc);
		}
		};
	}

	return reportTestExit(portLibrary, testName);
}

/*
 * Test various aspect of j9hypervisor_get_hypervisor_info
 *
 * @param[in] portLibrary The port library
 * @param[in] hypervisor The name of the Hypervisor on which the test is run
 * @param[in] hvOptions   Hypervisor specific options if any
 *
 * @return the return value from @reportTestExit
 */
int
j9hypervisor_test_get_hypervisor_info(J9PortLibrary* portLibrary, char* hypervisor, char *hvOptions, BOOLEAN negative)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9hypervisor_test_get_hypervisor_info";
	IDATA rc = 0;
	IDATA count = 0;
	J9HypervisorVendorDetails details;

	reportTestEntry(portLibrary, testName);

	rc = j9hypervisor_get_hypervisor_info(&details);

	if ((NULL == hypervisor) || (J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR == rc)) {
		/* Nothing to test , not running on a hypervisor */
	} else if (negative && (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED == rc)) {
		/* Handle negative test cases where we expect all kinds of unsupported hypervisor names. In
		 * the face of an error, j9hypervisor_get_hypervisor_info() does NOT point 'details.hypervisorName'
		 * to the string received as it treats it as an erroneous result. Use 'hypervisor' instead.
		 */
		outputComment(PORTLIB, "\tTest (negative case) passed: Expected an unsupported hypervisor name.\n");
		outputComment(PORTLIB, "\tj9hypervisor_get_hypervisor_info() returned -856 "
			"(J9PORT_ERROR_HYPERVISOR_UNSUPPORTED).\n");
		outputComment(PORTLIB, "\tHypervisor Name: %s (unsupported).\n", hypervisor);
	} else if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tj9hypervisor_get_hypervisor_info returned Error: %d.\n", rc);
	} else {
		outputComment(PORTLIB, "\t--- Returned data ---\n");
		outputComment(PORTLIB, "\tHypervisor Name :%s \n", details.hypervisorName);
		outputComment(PORTLIB, "\tHypervisor :\"%s\" \n", hypervisor);

		/* Verify the return value that is passed via the command line for the Vendor Name */
		if ((strcmp(hypervisor, VMWARE) == 0) && (strcmp(details.hypervisorName, "VMWare") == 0)) {
		} else if ((strcmp(hypervisor, KVM) == 0) && (strcmp(details.hypervisorName, "KVM") == 0)) {
		} else if ((strcmp(hypervisor, POWERVM) == 0) && (strcmp(details.hypervisorName, "PowerVM") == 0)) {
		} else if ((strcmp(hypervisor, POWERKVM) == 0) && (strcmp(details.hypervisorName, "PowerKVM") == 0)) {
		} else if ((strcmp(hypervisor, HYPERV) == 0) && (strcmp(details.hypervisorName, "Hyper-V") == 0)) {
		} else if ((strcmp(hypervisor, ZVM) == 0) && (strcmp(details.hypervisorName, "z/VM") == 0)) {
		} else if ((strcmp(hypervisor, PRSM) == 0) && (strcmp(details.hypervisorName, "PR/SM") == 0)) {
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\tThe hypervisor name returned by j9hypervisor_get_hypervisor_info is not correct \n"
					"\t\tHypervisor : %s\n\t\tdetails.hypervisorName : %s\n",
					hypervisor, details.hypervisorName);
		}
	}
	return reportTestExit(portLibrary, testName);
}

/*
 * Test various aspect of j9hypervisor_get_guest_memory_usage()
 *
 * @param[in] portLibrary The port library
 * @param[in] hypervisor The name of the Hypervisor on which the test is run
 * @param[in] hvOptions   Hypervisor specific options if any
 *
 * @return the return value from @reportTestExit
 */
int
j9hypervisor_test_get_guest_memory_usage(J9PortLibrary* portLibrary, char* hypervisor, char *hvOptions)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9hypervisor_test_get_guest_memory_usage";
	IDATA rc = 0;
	IDATA count = 0;
	J9GuestMemoryUsage prevguestMemUsage;
	J9GuestMemoryUsage currguestMemUsage;

	reportTestEntry(portLibrary, testName);

	/* None of the memory usage parameters are available on PowerKVM.  Return success. */
#if defined(PPC64) && defined(J9VM_ENV_LITTLE_ENDIAN)
	outputComment(portLibrary, "\tGuest OS Memory Usage statistics not supported on PowerKVM.\n");
	goto exit;
#endif /* defined(PPC64) && defined(J9VM_ENV_LITTLE_ENDIAN) */

	rc = j9hypervisor_get_guest_memory_usage(&prevguestMemUsage);

	if (J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR == rc || NULL == hypervisor) {
		outputComment(portLibrary, "\tNot running on a Hypervisor. Guest OS Statistics not available\n");
		goto exit;
	} else if (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED == rc) {
		outputComment(portLibrary, "\tGuest OS statistics not supported on this Hypervisor\n");
		goto exit;
	} else if (J9PORT_ERROR_HYPERVISOR_VMWARE_GUEST_SDK_OPEN_FAILED == rc) {
		/*
		 * TBD - Once the we get the machine setup ready with hypervisor:noGuestSDK,
		 * for machines without GuestSDK installed, we will have to validate this
		 * error condition.Until then, assuming that the API is returning the correct
		 * error code and will not proceed with the test.
		 */
		outputComment(portLibrary, "\tCannot proceed with the test. Guest SDK open failed\n");
		goto exit;
#ifdef LINUXPPC
	} else if (J9PORT_ERROR_HYPERVISOR_LPARCFG_MEM_UNSUPPORTED == rc) {
		/* Check if the lparcfg version is supported or not for retrieving the
		 * statistics on PowerVM
		 */
		int isSupported = is_lparcfg_version_supported();
		/* If not supported we cannot test the API */
		if (0 == isSupported) {
			outputComment(portLibrary, "\tCannot proceed with the test. Guest Memory "
					"statistics cannot be retrieved. Check the lparcfg "
					"version, should be >= lparcfg 1.8\n");
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed\n");
		}
		goto exit;
#elif (defined(LINUX) && defined(S390))
	} else if (J9PORT_ERROR_HYPERVISOR_HYPFS_NOT_MOUNTED == rc) {
		if (strcmp(hvOptions, NOHYPFS) == 0) {
			outputComment(portLibrary, "\tCannot proceed with the test. Cannot retrieve Guest Memory "
						"Usage statistics as HYPFS is not mounted\n");
		}
		else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed as hypfs is not mounted\n");
		}
		goto exit;
	} else if (J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_OPEN_FAILED == rc) {
		if (strcmp(hvOptions, NOUSERHYPFS) == 0) {
			outputComment(portLibrary, "\tCannot proceed with the test. Cannot retrieve Guest Memory "
						"Usage statistics as HYPFS is mounted readonly\n");
		}
		else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed as hypfs is mounted readonly\n");
		}
		goto exit;
	} else if ((0 == rc) && (strcmp(hvOptions, HYPFS) != 0)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Testing for failure case, but test passing!!\n"
				"\t\tPlease correct the hypervisorOptions flag for this VM\n"
				"\t\thvOptions for this test: %s, but hypfs seems to be mounted right.\n", hvOptions);
		goto exit;
#endif
	} else if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed\n");
		goto exit;
	}

	outputComment(portLibrary, "\t Guest Memory Usage Statistics  at TimeStamp: %lld microseconds\n", prevguestMemUsage.timestamp);
	outputComment(portLibrary, "\t Maximum memory that can be used by the GuestOS: %lld MB\n", prevguestMemUsage.maxMemLimit);
	outputComment(portLibrary, "\t The Current Memory used by the GuestOS in MB: %lld MB\n", prevguestMemUsage.memUsed);
	outputComment(portLibrary, "\t--------------------------------------------\n");

	/* Wait for 10 seconds before re-sampling */
	omrthread_sleep(10000);

	rc = j9hypervisor_get_guest_memory_usage(&currguestMemUsage);

	/* If there are errors retrieving */
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed\n");
		goto exit;
	}

	/* Print the statistics in both the success and the failure case */
	outputComment(portLibrary, "\t--------------------------------------------\n");
	outputComment(portLibrary, "\t Guest Memory Usage Statistics  at TimeStamp: %lld microseconds\n", currguestMemUsage.timestamp);
	outputComment(portLibrary, "\t Maximum memory that can be used by the GuestOS: %lld MB\n", currguestMemUsage.maxMemLimit);
	outputComment(portLibrary, "\t The Current Memory used by the GuestOS in MB: %lld MB\n", currguestMemUsage.memUsed);
	outputComment(portLibrary, "\t--------------------------------------------\n");

	/* Validate the memory statistics
	 * - whether the timeStamp has increased
	 * - memUsed is not 0
	 * - memUsed is less than(or equal to) the maxMemLimit when the maxMemLimit is set.
	 */
	if ((prevguestMemUsage.timestamp < currguestMemUsage.timestamp) &&
		(currguestMemUsage.memUsed > 0 )) {
		/* Check if the maxMemLimit is set, if yes, check if the memUsed >= maxMemLimit */
		if ((currguestMemUsage.maxMemLimit != -1)) {
			if (currguestMemUsage.memUsed > currguestMemUsage.maxMemLimit) {
				outputComment(portLibrary, "\t Invalid Guest OS Memory Usage statistics retrieved.\n");
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Guest OS MemUsed is greater than the MaxMemLimit\n");
				goto exit;
			}
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid Guest OS Memory Usage statistics retrieved.\n");
		goto exit;
	}

	/* Call the API multiple times without any time interval between the calls.
	 * Check to validate whether the multiple calls to the API passes successfully
	 */
	outputComment(portLibrary, "\tCalling the API multiple times\n");
	for (count = 0; count < GET_GUEST_USAGE_ITERATIONS; count++) {
		rc = j9hypervisor_get_guest_memory_usage(&currguestMemUsage);
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed\n");
			outputComment(portLibrary, "\t--------------------------------------------\n");
			goto exit;
		}
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/*
 * Test various aspect of j9hypervisor_get_guest_memory_usage()
 *
 * @param[in] portLibrary The port library
 * @param[in] hypervisor The name of the Hypervisor on which the test is run
 * @param[in] hvOptions   Hypervisor specific options if any
 *
 * @return the return value from @reportTestExit
 */
int
j9hypervisor_test_get_guest_processor_usage(J9PortLibrary* portLibrary, char* hypervisor, char *hvOptions)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9hypervisor_test_get_guest_processor_usage";
	IDATA rc = 0;
	IDATA count = 0;

	J9GuestProcessorUsage prevguestprocessorUsage;
	J9GuestProcessorUsage currguestprocessorUsage;

	J9GuestProcessorUsage guestProcUsage[5] = {0};

	reportTestEntry(portLibrary, testName);
	rc = j9hypervisor_get_guest_processor_usage(&prevguestprocessorUsage);

	if (J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR == rc || NULL == hypervisor) {
		outputComment(portLibrary, "\tNot running on a Hypervisor.GuestOS Statistics not available\n");
		goto exit;
	} else if (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED == rc) {
		outputComment(portLibrary, "\tGuest OS statistics not supported on this Hypervisor\n");
		goto exit;
	} else if (J9PORT_ERROR_HYPERVISOR_VMWARE_GUEST_SDK_OPEN_FAILED == rc) {
		outputComment(portLibrary, "\tCannot proceed with the test. VMWare Guest SDK open failed\n");
		goto exit;
#if (defined(LINUX) && defined(S390))
	} else if (J9PORT_ERROR_HYPERVISOR_HYPFS_NOT_MOUNTED == rc) {
		if (strcmp(hvOptions, NOHYPFS) == 0) {
			outputComment(portLibrary, "\tCannot proceed with the test. Cannot retrieve Guest Memory"
						" Usage statistics as HYPFS is not mounted\n");
		}
		else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed as hypfs is not mounted\n");
		}
		goto exit;
	} else if (J9PORT_ERROR_HYPERVISOR_HYPFS_UPDATE_OPEN_FAILED == rc) {
		if (strcmp(hvOptions, NOUSERHYPFS) == 0) {
			outputComment(portLibrary, "\tCannot proceed with the test. Cannot retrieve Guest Memory"
						" Usage statistics as HYPFS is mounted readonly\n");
		}
		else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_memory_usage() failed as hypfs is mounted readonly\n");
		}
		goto exit;
	} else if ((0 == rc) && (strcmp(hvOptions, HYPFS) != 0)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Testing for failure case, but test passing!!\n"
				"\t\tPlease correct the hypervisorOptions flag for this VM\n"
				"\t\thvOptions for this test: %s, but hypfs seems to be mounted right.\n", hvOptions);
		goto exit;
#endif
	} else if (0 < rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_processor_usage() failed\n");
		goto exit;
	}

	outputComment(portLibrary, "\t Guest Processor Usage Statistics  at TimeStamp: %lld microseconds\n", prevguestprocessorUsage.timestamp);
	outputComment(portLibrary, "\t Host CPU Clock Speed: %lld %s\n", prevguestprocessorUsage.hostCpuClockSpeed, CPUSPEEDUNIT);
	/* On z/linux and z/OS CPU entitlement is not available. Enable test when available. */
#if !defined(J9ZOS390) && !(defined(LINUX) && defined(S390))
	outputComment(portLibrary, "\t The CPU Entitlement of the current VM: %f\n", prevguestprocessorUsage.cpuEntitlement);
#else
	outputComment(portLibrary, "\t The CPU Entitlement of the current VM: <undefined>\n");
#endif /* !defined(J9ZOS390) && !(defined(LINUX) && defined(S390)) */
	outputComment(portLibrary, "\t The time during which the virtual machine has used the CPU: %lld microseconds\n", prevguestprocessorUsage.cpuTime);
	outputComment(portLibrary, "\t--------------------------------------------\n");

	/* Create a busy load which runs for 2s */
	create_busy_load(portLibrary, 2);

	rc = j9hypervisor_get_guest_processor_usage(&currguestprocessorUsage);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_processor_usage() failed\n");
		goto exit;
	}

	rc = validate_processor_stats(portLibrary, currguestprocessorUsage, prevguestprocessorUsage);

	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid Guest OS Processor Usage statistics retrieved.\n");
		goto exit;
	}


	/* Call the API multiple times without any time interval between the calls.
	 * Check to validate whether the multiple calls to the API passes successfully
	 */
	outputComment(portLibrary, "\tCalling the API multiple times\n");
	for (count = 0; count < GET_GUEST_USAGE_ITERATIONS; count++) {
		rc = j9hypervisor_get_guest_processor_usage(&guestProcUsage[count]);

		/* Check if there is an error while retrieving the statistics */
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9hypervisor_get_guest_processor_usage() failed\n");
			outputComment(portLibrary, "\t--------------------------------------------\n");
			goto exit;
		}
		/* Validate the statistics for the next iteration onwards */
		if (count > 0) {
			rc = validate_processor_stats(portLibrary, guestProcUsage[count], guestProcUsage[count-1]);
			if (rc < 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid Guest OS Processor Usage statistics retrieved.\n");
				goto exit;
			}
		}
	}

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify j9hypervisor features.
 *
 * Exercise all API related to Hypervisor detection and Guest Usage APIs
 * present in @ref j9hypervisor.c
 *
 * @param[in] portLibrary The port library under test
 * @param[in] hypervisor  The Hypervisor Name on which this test is run.
 * @param[in] hvOptions   Hypervisor specific options if any
 *
 * @return 0 on success, -1 on failure
 */
I_32
j9hypervisor_runTests(struct J9PortLibrary *portLibrary, char *hypervisor, char *hvOptions, BOOLEAN negative)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = TEST_PASS;
	HEADING(portLibrary, "J9Hypervisor Test");

	outputComment(portLibrary, "Add -hypervisor:<Hypervisor Name> to reproduce these tests Manually.\n");
	outputComment(portLibrary, "Supported Hypervisor Names:\n\t1.kvm\n\t2.vmware\n\t3.hyperv\n\t4.powervm\n\t5.zvm\n\t6.prsm\n\t7.powerkvm\n");

	if (NULL != hypervisor) {
		outputComment(portLibrary, "Hypervisor Name Specified for the test: %s\n", hypervisor);
	}
	if (NULL != hvOptions) {
		outputComment(portLibrary, "Hypervisor Options: %s\n", hvOptions);
	}
	rc |= j9hypervisor_test_hypervisor_present(portLibrary, hypervisor, hvOptions, negative);
	rc |= j9hypervisor_test_get_hypervisor_info(portLibrary, hypervisor, hvOptions, negative);
	rc |= j9hypervisor_test_get_guest_processor_usage(portLibrary, hypervisor, hvOptions);
	rc |= j9hypervisor_test_get_guest_memory_usage(portLibrary, hypervisor, hvOptions);

	/* Output results */
	j9tty_printf(PORTLIB, "\nJ9Hypervisor test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
