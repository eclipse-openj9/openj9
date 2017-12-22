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
#include <stdlib.h>
#include <string.h>

/* The structures defined in j9port.h are required earlier */
#include "j9csrsi.h"
#include "j9hypervisor.h"
#include "portnls.h"
#include "j9port.h"
#include "portpriv.h"
#include "ut_j9prt.h"

/* Hypervisor detection string for z/OS on top of z/VM. */
#define J9ZOS390_ZVM_DETECT_STRING "z/VM"

/* VM hypervisor program name buffer length */
#define VMHP_NAME_LENGTH           32U

intptr_t
detect_hypervisor(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	const j9csrsi_t * j9csrsi_sess = NULL;
	char *tmpPtr = NULL;							/* temporary pointer */
	char cpIdentifier[VMHP_NAME_LENGTH+1] = {0};	/* vm control program identifier */
	const char *errMsg = NULL;
	intptr_t rc = 0;

	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Entry();

	j9csrsi_sess = j9csrsi_init(&PPG_j9csrsi_ret_code);
	PPG_j9csrsi_session = j9csrsi_sess;

	if (NULL == j9csrsi_sess) {
		errMsg = omrnls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
												J9NLS_PORT_CSRSIC_FAILURE__MODULE,
												J9NLS_PORT_CSRSIC_FAILURE__ID,
												NULL);
		omrerror_set_last_error_with_message(J9PORT_ERROR_HYPERVISOR_UNSUPPORTED, errMsg);
		portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_HYPERVISOR_OPFAILED;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_CSRSICfailed();
		Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(J9PORT_ERROR_HYPERVISOR_OPFAILED);
		return J9PORT_ERROR_HYPERVISOR_OPFAILED;
	}

	/* Let's first check for level-3 and above hypervisor. */
	if (j9csrsi_is_vmh(j9csrsi_sess)) {
		/* Detected level-3 and above configuration. */
		portLibrary->portGlobals->hypervisorData.isVirtual = TRUE;
		/* In a multilevel virtualization stack, the first description block
		 * always points to the highest-numbered level-3 configuration;
		 * and we are only concerned about the hypervisor managing the guest
		 * that is running this program.
		 */
		rc = j9csrsi_get_vmhpidentifier(j9csrsi_sess, 0U, cpIdentifier, sizeof(cpIdentifier));
		/* During j9 compilation on z/OS, string literals are by default converted to an ASCII code page
		 * using convlit(ISO8859-1) compiler option - to search for substring the identifier should get
		 * converted into ASCII as well.
		 */
		if (0 >= rc) {
			rc = J9PORT_ERROR_OPFAILED;
			portLibrary->portGlobals->hypervisorData.isVirtual = J9PORT_ERROR_OPFAILED;
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
			Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
			return rc;
		}

		tmpPtr = strstr(cpIdentifier, J9ZOS390_ZVM_DETECT_STRING);
		if (NULL != tmpPtr) {
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_ZVM;
			rc = 0; /* return success */
		} else {
			/* Unknown condition - paradoxically, we shouldn't be here. */
			portLibrary->portGlobals->hypervisorData.isVirtual = FALSE;
			portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
			rc = J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR;
		}
	} else if (j9csrsi_is_lpar(j9csrsi_sess)) {
		/* Failed to detect any level-3 hypervisor - fallback on level-2 configuration. */
		portLibrary->portGlobals->hypervisorData.isVirtual = TRUE;
		/* There is no need to probe further; we are definitely managed by PR/SM. */
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = HYPE_NAME_PRSM;
		rc = 0; /* return success */
	} else {
		/* The machine is neither VM nor LPAR => are we running directly on bare metal? */
		portLibrary->portGlobals->hypervisorData.isVirtual = FALSE;
		portLibrary->portGlobals->hypervisorData.vendorDetails.hypervisorName = NULL;
		rc = J9PORT_ERROR_HYPERVISOR_NO_HYPERVISOR;
	}
	Trc_PRT_virt_j9hypervisor_detect_hypervisor_Exit(rc);
	return rc;
}
