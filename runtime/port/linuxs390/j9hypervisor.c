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
#include <errno.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
/* The structures defined in j9port.h are required earlier */
#include "j9port.h"
#include "j9hypervisor.h"
#include "portnls.h"
#include "portpriv.h"
#include "omrutil.h"
#include "ut_j9prt.h"

/* system information path on linux-s390 (zLinux) */
#define S390_CONFIG_PATH		"/proc/sysinfo"
#define S390_VM_CP_SCAN_STRING	"VM([0-9][0-9]) Control Program: (z/VM|KVM/Linux) "
#define S390_LPAR_SCAN_STRING	"LPAR Number:[ \t]+([0-9]+)"

/* hypervisor detection strings as captured by system information file */
#define S390_ZVM_DETECT_STRING	"z/VM"
#define S390_KVM_DETECT_STRING	"KVM/Linux"

/* Length of string buffers. */
#define MAXSTRINGLENGTH		128

intptr_t
detect_hypervisor(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	intptr_t fd = 0;
	intptr_t nbytes = 0;
	intptr_t rc = 0;
	char fileBuf[BUFSIZ] = {0};
	char tmpBuf[MAXSTRINGLENGTH] = {0};
	regex_t reg = {0};
	regmatch_t match[3] = {0};
	intptr_t nmatch = 3;
	const char *errMsg = NULL;

	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Entry();
	/* Hypervisor related information is exposed on zLinux via sysinfo file under procfs. */
	fd = omrfile_open(S390_CONFIG_PATH, EsOpenRead, 0);
	if (-1 == fd) {
		rc = omrerror_last_error_number();
		portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_OPFAILED;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_file_openfailed(rc);
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
		return rc;
	}

	nbytes = omrfile_read(fd, fileBuf, sizeof(fileBuf));
	/* File close operation doesn't alter the last error message. */
	omrfile_close(fd);
	if (-1 == nbytes) {
		rc = omrerror_last_error_number();
		portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_OPFAILED;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_file_readfailed(rc);
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
		return rc;
	}

	/* First try to detect level-3 or above hypervisor. */
	rc = regcomp(&reg, S390_VM_CP_SCAN_STRING, (REG_EXTENDED|REG_NEWLINE));
	if (0 != rc) {
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_NOT_PRESENT;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
		return rc;
	}

	rc = regexec(&reg, fileBuf, nmatch, match, 0);
	regfree(&reg);
	if (0 != rc) {
		/* We failed to detect level-3 hypervisor; let's fall back on level-2 virtualization. */
		memset(&reg, 0, sizeof(reg));
		rc = regcomp(&reg, S390_LPAR_SCAN_STRING, (REG_EXTENDED|REG_NEWLINE));
		if (0 != rc) {
			portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_NOT_PRESENT;
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
			Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
			return rc;
		}

		rc = regexec(&reg, fileBuf, 0, NULL, 0);
		regfree(&reg);
		if (0 != rc) {
			portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_NOT_PRESENT;
		} else {
			/* We don't further decipher the scan string; mere existence is enough to decide. */
			portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_PRESENT;
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_PRSM;
		}
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
		return rc;
	} else {
		/* Level-3 Hypervisor */
		portLibrary->portGlobals->hypervisorData.isVirtual = J9HYPERVISOR_PRESENT;
	}

	memcpy(tmpBuf, &fileBuf[match[2].rm_so], (match[2].rm_eo - match[2].rm_so));

	/* Detect level-3 or above z/VM. */
	rc = memcmp(tmpBuf, S390_ZVM_DETECT_STRING, strlen(S390_ZVM_DETECT_STRING));
	if (0 == rc) {
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_ZVM;
	} else {
		/* Failed to detect z/VM; let's check whether this VM is managed by KVM. */
		rc = memcmp(tmpBuf, S390_KVM_DETECT_STRING, strlen(S390_KVM_DETECT_STRING));
		if (0 == rc) {
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_KVM;
		} else {
			/* Unknown condition - paradoxically, we shouldn't be here. */
			portLibrary->portGlobals->hypervisorData.isVirtual = FALSE;
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
			errMsg = omrnls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
													J9NLS_PORT_HYPERVISOR_OPFAILED__MODULE,
													J9NLS_PORT_HYPERVISOR_OPFAILED__ID,
													NULL);
			omrerror_set_last_error_with_message(J9PORT_ERROR_HYPERVISOR_OPFAILED, errMsg);
			rc = J9PORT_ERROR_HYPERVISOR_OPFAILED;
		}
	}
	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
	return rc;
}
