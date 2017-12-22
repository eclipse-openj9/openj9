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
 * @brief Guarded Storage
 */
#include "j9port.h"
#include "ut_j9prt.h"
#include "j9gs.h"

#pragma linkage (GETGSSUP,OS)
#pragma map(j9gs_is_supported,"GETGSSUP")
uintptr_t j9gs_is_supported();

#pragma linkage (GSSTCB,OS)
#pragma map(j9gs_store_gscb,"GSSTCB")
void j9gs_store_gscb(void*);

#pragma linkage (GSLDCB,OS)
#pragma map(j9gs_load_gscb,"GSLDCB")
void j9gs_load_gscb(void*);

#pragma linkage(enable_guarded_storage_facility,OS_NOSTACK)
uintptr_t enable_guarded_storage_facility(void*);
#pragma linkage(disable_guarded_storage_facility,OS_NOSTACK)
uintptr_t disable_guarded_storage_facility();

int32_t
j9gs_initialize(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, int32_t shiftAmount)
{
	Trc_PRT_gs_initialize_Entry(gsParams,shiftAmount);

	uint32_t ret_code = -1;
	if (j9gs_is_supported()) {
		/**
		 * ret_code values from authorization routine is as follows:
		 *  0x0  - authorization routine successful.
		 *  0x8  - authorization failed with user error.
		 *  0xC  - authorization failed due to environment.
		 */

		J9GSControlBlock* gsControlBlock = (J9GSControlBlock*)(gsParams->controlBlock);
		/* Guarded load shift value, bits 53-56 in Guarded-Storage-Designation Register */
		gsControlBlock->designationRegister = (uint64_t) shiftAmount << 8;
		ret_code = enable_guarded_storage_facility((void*)gsControlBlock);
		if (0 == ret_code) {
			gsParams->flags |= J9PORT_GS_INITIALIZED;
		} else {
			Trc_PRT_gs_initialize_Exception(ret_code, gsParams->flags);
		}
	} else {
		/**
		 * Use -1 for return code when GS is not supported.
		 */
		Trc_PRT_gs_initialize_Exception(-1, gsParams->flags);
	}
	Trc_PRT_gs_initialize_Exit(ret_code, gsParams->flags);

	return IS_GS_INITIALIZED(gsParams) ? 1 : 0;
}

int32_t
j9gs_deinitialize(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams)
{
	Trc_PRT_gs_deinitialize_Entry(gsParams);

	if ((gsParams->flags & J9PORT_GS_INITIALIZED) != 0) {
		uint32_t ret_code = disable_guarded_storage_facility();
		if (0 == ret_code) {
			gsParams->flags &= ~(J9PORT_GS_INITIALIZED);
		} else {
			Trc_PRT_gs_deinitialize_Exception(ret_code, gsParams->flags);
		}
	} else {
		Trc_PRT_gs_deinitialize_Exception(-1, gsParams->flags);
	}

	Trc_PRT_gs_deinitialize_Exit();

	return IS_GS_INITIALIZED(gsParams) ? 0 : 1;
}

