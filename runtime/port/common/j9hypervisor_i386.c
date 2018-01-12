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
 * @brief  Hypervisor Detection helper functions common to i386 platforms
 */
#include <string.h>

#include "j9hypervisor.h"
#include "j9port.h"
#include "j9porterror.h"
#include "portpriv.h"
#include "portnls.h"
#include "j9sysinfo_helpers.h"
#include "ut_j9prt.h"

/* Length of string buffers. */
#define MAXSTRINGLENGTH         128

/* Hypervisor detection Strings as emitted from the CPUID instruction */
#define KVM_DETECT_STRING		"KVMKVMKVM"
#define VMWARE_DETECT_STRING	"VMwareVMware"
#define HYPERV_DETECT_STRING	"Microsoft Hv"

/* Hypervisor name Length from CPUID */
#define MAX_HYPERVISOR_NAME_LENGTH_FROM_CPUID 15

/* MACROS for the CPUID instruction */
#define CPUID_HYPERVISOR_PRESENT		0x80000000
#define CPUID_HYPERVISOR_DETECT			0x1
#define CPUID_HYPERVISOR_VENDOR_INFO	0x40000000

/*
 * Detect Hypervisor function common to i386 platforms
 *
 * param[in] portLibrary
 * @return	0 on Success
 * 		Negative error code on failure
 */
intptr_t
detect_hypervisor_i386(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	char const *errMsg;
	uint32_t CPUInfo[4] = {0};
	char hypervisorName[MAX_HYPERVISOR_NAME_LENGTH_FROM_CPUID];

	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Entry();

	/* Leaf =0x1 for CPUID instruction to check for Hypervisor present bit */
	getX86CPUID(CPUID_HYPERVISOR_DETECT, CPUInfo);

	if (CPUID_HYPERVISOR_PRESENT == (CPUID_HYPERVISOR_PRESENT & CPUInfo[2])) {
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_PRESENT;
		/* Check for the Hypervisor Vendor */
		getX86CPUID(CPUID_HYPERVISOR_VENDOR_INFO,CPUInfo);
		memcpy (hypervisorName+0, &CPUInfo[1], 4);
		memcpy (hypervisorName+4, &CPUInfo[2], 4);
		memcpy (hypervisorName+8, &CPUInfo[3], 4);
		hypervisorName[12] = '\0';
	} else {
		/* Not running on a Hypervisor */
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_NOT_PRESENT;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(0);
		return 0;
	}
	if (0 == strcmp(hypervisorName,KVM_DETECT_STRING)) {
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_KVM;
	} else if (0 == strcmp(hypervisorName,VMWARE_DETECT_STRING)) {
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_VMWARE;
	} else if (0 == strcmp(hypervisorName,HYPERV_DETECT_STRING)) {
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_HYPERV;
	} else {
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
		errMsg = omrnls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
												J9NLS_PORT_UNSUPPORTED_HYPERVISOR__MODULE,
												J9NLS_PORT_UNSUPPORTED_HYPERVISOR__ID,
												NULL);
		omrerror_set_last_error_with_message(J9PORT_ERROR_HYPERVISOR_UNSUPPORTED, errMsg);
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(J9PORT_ERROR_HYPERVISOR_UNSUPPORTED);
		return J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
	}
	/* Return success. */
	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(0);
	return 0;
}
