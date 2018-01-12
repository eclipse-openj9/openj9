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
 * @brief Functions common to both the z/VM and PR/SM hypervisors
 */

#include "j9hypervisor_common.h"
#include "j9csrsi.h"
#include "j9port.h"
#include "portpriv.h"
#include "j9sysinfo_helpers.h"

/* Forward declarations. */
static void
zos_systemz_shutdown(struct J9PortLibrary *portLibrary);

/**
 * This is called at java startup time if we are running on z/VM or PR/SM
 *
 * @param [in]  portLibrary The port Library
 *
 * @return      0 on success, negative value on failure
 */
static intptr_t
zos_systemz_startup(struct J9PortLibrary *portLibrary)
{
	PHD_hypFunc.hypervisor_impl_shutdown = zos_systemz_shutdown;
	PHD_hypFunc.get_guest_processor_usage = retrieveZGuestProcessorStats;
	PHD_hypFunc.get_guest_memory_usage = retrieveZGuestMemoryStats;
	PHD_vendorStatus = HYPERVISOR_VENDOR_INIT_SUCCESS;
	PHD_vendorPrivateData = NULL;

	return 0;
}

/**
 * Cleanup function called at java shutdown time.
 * Close all open handles and free any memory allocated at startup
 *
 * @param [in]  portLibrary The port Library
 */
static void
zos_systemz_shutdown(struct J9PortLibrary *portLibrary)
{
	PHD_hypFunc.get_guest_processor_usage = NULL;
	PHD_hypFunc.get_guest_memory_usage = NULL;
	PHD_hypFunc.hypervisor_impl_shutdown = NULL;

	if (NULL != PPG_j9csrsi_session) {
		j9csrsi_shutdown(PPG_j9csrsi_session);
	}
}

/**
 * Common function for systemz startup.
 * Calls the appropriate architecture specific startup function
 *
 * @param [in] portLibrary The port Library
 *
 * @return 0 on success and negative on failure
 */
intptr_t
systemz_startup(struct J9PortLibrary *portLibrary)
{
	/* Have the setup function set the function pointer appropriately. */
	return zos_systemz_startup(portLibrary);
}
