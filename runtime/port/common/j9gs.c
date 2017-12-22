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

/**
 * Initialize the supplied J9GSParameters with default values: controlBlock=gsControlBlock, all flags cleared.
 *
 * @param[in] portLibrary The port library
 * @param[in] gsParams pointer to generic guarded storage parameters structure.
 * @param[in] gsControlBlock pointer to machine architecture dependent control block
 */
void
j9gs_params_init(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void *gsControlBlock)
{
	Trc_PRT_gs_params_init_Entry(gsParams, gsControlBlock);
	gsParams->flags = 0;
	gsParams->controlBlock = gsControlBlock;
	Trc_PRT_gs_params_init_Exit();
}


/**
 * Enable guarded storage on the calling hardware thread.
 * 
 * The memory starting at 'baseAddress' is divided into sections of 'perBitSectionSize' and the 'bitMask' indicates which sections are being guarded
 * This sets the J9PORT_GS_ENABLED bit in gsParams->flags
 *
 * @param[in] portLibrary The port library
 * @param[in] gsParams pointer to generic guarded storage parameters structure
 * @param[in] baseAddress base address for guarded storage
 * @param[in] perBitSectionSize Section size in bytes
 * @param[in] bitMask Bit mask for guarded memory sections
 */
void
j9gs_enable(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void* baseAddress, uint64_t perBitSectionSize, uint64_t bitMask)
{
	Trc_PRT_gs_enable_Entry(gsParams, baseAddress, perBitSectionSize, bitMask);

	Trc_PRT_gs_enable_Exit();
}

/**
 * Disable guarded storage on the calling hardware thread.
 *
 * The thread must already be authorized.
 * This clears the J9PORT_GS_ENABLED bit in gsParams->flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] gsParams pointer to generic guarded storage parameters structure.
 */
void
j9gs_disable(struct J9PortLibrary *portLibrary,  struct J9GSParameters *gsParams)
{
	Trc_PRT_gs_disable_Entry(gsParams);

	Trc_PRT_gs_disable_Exit();
}

/**
 * Set up hardware and/or operating structures for guarded storage.
 * This must be called on the current thread before calling j9gs_enabled.
 *
 * This sets the J9PORT_GS_INITIALIZED in gsParams->flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] gsParams pointer to generic guarded storage parameters structure.
 * @param[in] shiftAmount compressedrefs shift amount, used by hardware when calculating guarded storage region
 * 
 * @return 1 if successful, 0 otherwise
 */
int32_t
j9gs_initialize(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, int32_t shiftAmount)
{
	Trc_PRT_gs_initialize_Entry(gsParams, shiftAmount);
	Trc_PRT_gs_initialize_Exit(0, gsParams->flags);
	return 0;
}

/**
 * Destroys hardware and/or operating system guarded storage structures created by j9gs_initialize.
 * It must be called to release resources allocated by j9gs_initialize.
 *
 * This clears the J9PORT_GS_INITIALIZED bit in gsParams->flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] gsParams pointer to generic guarded storage parameters structure.
 * 
 * @return 1 if successful, 0 otherwise
 */
int32_t
j9gs_deinitialize(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams)
{
	Trc_PRT_gs_deinitialize_Entry(gsParams);
	Trc_PRT_gs_deinitialize_Exit();
	return 0;
}

/**
 * Returns 1 when guarded storage is enabled,
 * 0 when it's disabled
 * If guarded storage is enabled,
 * baseAddress, perBitSectionSize and bitMask are set
 *
 *
 * @param[in] portLibrary The port library
 * @param[in] gsParams pointer to generic guarded storage parameters structure
 * @param[in] baseAddress base address for guarded storage
 * @param[in] perBitSectionSize Section size in bits
 * @param[in] bitMask Bit mask for guarded memory sections
 */
int32_t
j9gs_isEnabled(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void** baseAddress, uint64_t* perBitSectionSize, uint64_t* bitMask)
{
	Trc_PRT_gs_isEnabled_Entry(gsParams);
	Trc_PRT_gs_isEnabled_Exit(0, *baseAddress, *perBitSectionSize, *bitMask);
	return 0;
}
