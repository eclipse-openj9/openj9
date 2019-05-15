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

/**
 * @file
 * @ingroup Port
 * @brief  Hypervisor Detection helper functions
 */

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatibility */
#if defined(LINUX)
#define _GNU_SOURCE
#endif /* defined(__GNUC__) */

#include <errno.h>
#include <string.h>
#include "j9hypervisor.h"
#include "j9porterror.h"
#include "portnls.h"
#include "portpriv.h"
#include "ut_j9prt.h"

/* Length of string buffers. */
#define MAXSTRINGLENGTH         128


/* lpar configuration path on linux-ppc */
#define LINUXPPC_CONFIG_PATH		"/proc/ppc64/lparcfg"
#define LINUXPPC_PARTITION_STRING	"partition_id"
#define LINUXPPC_PURR_STRING		"purr"
#define LINUXPPC_SYSTEM_TYPE		"system_type"
#define QEMU_STRING					"qemu"
#define COMPUTE_BASE				10

/**
 * Platform specific implementation for Hypervisor detection
 * Fills in the J9Hypervisor structure instance accordingly.
 *
 * @param - portLibrary
 *
 * @return - Zero, indicating success. Non-zero, indicating failure.
 */
intptr_t
detect_hypervisor(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t ret = 0;
	FILE* fp = NULL;
	int32_t purr_found = 0;
	int32_t partition_id = 0;
	char *kvm_found = NULL;
	char lineStr[MAXSTRINGLENGTH] = {0};

	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Entry();

	/* Initialize to defaults */
	portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
	portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;

	/**
	 * On Linux targeting PowerPC there is a special configuration file that contains
	 * information regarding the LPAR that the current OS instance is running on.
	 */
	fp = fopen(LINUXPPC_CONFIG_PATH, "r");
	if (NULL == fp) {
		switch(errno) {
		case EACCES:
				omrerror_set_last_error(errno, J9PORT_ERROR_FILE_NOPERMISSION);
				portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_FILE_NOPERMISSION;
				break;
		case ENOENT:
				omrerror_set_last_error(errno, J9PORT_ERROR_FILE_NOENT);
				portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_FILE_NOENT;
				break;
		default:
				omrerror_set_last_error(errno, J9PORT_ERROR_FILE_OPFAILED);
				portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_FILE_OPFAILED;
				break;
		}
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_failedRead(errno);
		return portLibrary->portGlobals->hypervisorData.isVirtual;
	}

	/* check for the key 'partition_id=' in the configuration file. */
	while (NULL != fgets(lineStr, sizeof(lineStr), fp)) {

		char *endPtr = NULL;
		char tokenName[MAXSTRINGLENGTH] = {0};
		char tokenValue[MAXSTRINGLENGTH] = {0};

		int32_t num_tokens = sscanf(lineStr, "%[^=]=%[^\n]", tokenName, tokenValue);
		if (2 != num_tokens) {
			continue;
		}
		/* Add a check for KVM Hypervisor. Look for "qemu" string in the system_type
		 * system_type=IBM pSeries (emulated by qemu)
		 * This is a temporary fix as it is not clear if the qemu string is
		 * fixed at this point. - 20131205
		 */
		if (0 == strcmp(tokenName, LINUXPPC_SYSTEM_TYPE)) {
			kvm_found = strcasestr(tokenValue, QEMU_STRING);
			if (NULL != kvm_found) {
				break;
			}
		}
		else if (0 == strcmp(tokenName, LINUXPPC_PARTITION_STRING)) {
			errno = 0;
			partition_id = strtol(tokenValue, &endPtr, COMPUTE_BASE);
			if (errno > 0) {
				ret = J9PORT_ERROR_FILE_OPFAILED;
				break;
			}
		}
		else if (0 == strcmp(tokenName, LINUXPPC_PURR_STRING)) {
			errno = 0;
			uint64_t purr = strtoll(tokenValue, &endPtr, COMPUTE_BASE);
			if (errno > 0) {
				ret = J9PORT_ERROR_FILE_OPFAILED;
				break;
			}
			purr_found = 1;
		}
	}
	fclose(fp);

	/* Currently assuming the presence of qemu string to indicate powerkvm */
	if (NULL != kvm_found) {
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_PRESENT;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_POWERKVM;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(0);
		return 0;       /* Indicate success. */
	}

	/* if the partition_id is non zero and the purr value is present, it is running on PowerVM */
	if ((0 == ret) && (0 != partition_id) && (0 != purr_found)) {
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_PRESENT;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_POWERVM;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(0);
		return 0;	/* Indicate success. */
	}

	/* A Partition ID greater than 0 and the presence of the "purr" field in
	 * /proc/ppc64/lparcfg indicate a system with the hypervisor.
	 * Else we are running on bare metal.
	 */
	const char* errMsg = omrnls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
								 J9NLS_PORT_UNSUPPORTED_HYPERVISOR__MODULE,
							     J9NLS_PORT_UNSUPPORTED_HYPERVISOR__ID,
							     NULL);
	omrerror_set_last_error_with_message(ret, errMsg);
	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(-1);
	return J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
}
