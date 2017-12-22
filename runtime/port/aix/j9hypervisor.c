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
 * @brief  Hypervisor Detection helper functions
 */
#include <errno.h>
#include <string.h>
#include <sys/sysconfig.h>
#include <sys/types.h>

#include "j9hypervisor.h"
#include "j9porterror.h"
#include "j9port.h"
#include "portnls.h"
#include "portpriv.h"
#include "ut_j9prt.h"

/* Length of string buffers. */
#define MAXSTRINGLENGTH         128

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
	int32_t rc = 0;
	const char* errMsg;
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	struct getlpar_info lpar_info;
	char partitionName[MAXSTRINGLENGTH] = {0};
	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Entry();

	memset((void*)&lpar_info, 0, sizeof(struct getlpar_info));
	lpar_info.lpar_namesz = MAXSTRINGLENGTH;
	lpar_info.lpar_name = (char*)partitionName;

	/* Retrieve LPAR information using the AIX sysconfig() API. */
	rc = sysconfig(SYS_GETLPAR_INFO, &lpar_info, sizeof(struct getlpar_info));
	if (0 != rc) {
		omrerror_set_last_error(errno, J9PORT_ERROR_OPFAILED);
		portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_OPFAILED;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_failedSysconfig(errno);
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(J9PORT_ERROR_OPFAILED);
		return J9PORT_ERROR_OPFAILED;
	} /* end if */

	/**
	 * Check whether we are running on a lpar at all or not. Note that the flags may be ORed
	 * rather than being absolute values on their own. Check for the combination of flags.
	 */
	if ((LPAR_CAPABLE == (LPAR_CAPABLE & lpar_info.lpar_flags)) &&
		(LPAR_ENABLED == (LPAR_ENABLED & lpar_info.lpar_flags))) {
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_PRESENT;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_POWERVM;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(0);
		return 0; /* Success */
	}

	/**
	 * AIX always runs as an LPAR,if for some reason the flag is not set,
	 * we will return it as an error so that the caller can get more insight
	 * into the nature of error
	 */
	errMsg = omrnls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
											J9NLS_PORT_UNSUPPORTED_HYPERVISOR__MODULE,
											J9NLS_PORT_UNSUPPORTED_HYPERVISOR__ID,
											NULL);
	omrerror_set_last_error_with_message(J9PORT_ERROR_HYPERVISOR_UNSUPPORTED,errMsg);

	portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;
	portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(J9PORT_ERROR_HYPERVISOR_UNSUPPORTED);
	return J9PORT_ERROR_HYPERVISOR_UNSUPPORTED;

}
