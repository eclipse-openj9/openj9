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
 * @brief Guarded Storage
 */
#include <errno.h>
#include <string.h>

#include "j9port.h"
#include "j9gs.h"
#include "ut_j9prt.h"

/* The system call number for guarded-storage interface */
#define __NR_s390_guarded_storage 378

#define GS_ENABLE 0
#define GS_DISABLE 1
#define GS_SET_BC_CB 2
#define GS_CLEAR_BC_CB  3
#define GS_BROADCAST 4

#if defined(J9VM_PORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) */

static void
j9gs_store_gscb(void * gscb_controls)
{
	J9GSControlBlock* gsControlBlock = (J9GSControlBlock*) gscb_controls;
	asm volatile(".insn rxy,0xe30000000049,0,%0"
			     : : "Q" (*gsControlBlock));
}

static void
j9gs_load_gscb(void * gscb_controls)
{
	J9GSControlBlock* gsControlBlock = (J9GSControlBlock*) gscb_controls;
	asm volatile(".insn rxy,0xe3000000004d,0,%0"
			     : : "Q" (*gsControlBlock));
}

int32_t
j9gs_initialize(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, int32_t shiftAmount)
{
	int32_t ret_code = -1;
	Trc_PRT_gs_initialize_Entry(gsParams,shiftAmount);

#if !defined(J9ZTPF)
	/*
	 * syscall is _s390_guarded_storage
	 * errno values from syscall routine is as follows:
	 * EINVAL     The value specified for command is not a valid command.
	 * EFAULT     The cp pointer is outside of the accessible address space.
	 * ENOMEM     Allocating memory for the guarded-storage control block failed.
	 * EOPNOTSUPP The guarded-storage facility is not available
	 */

	ret_code = syscall(__NR_s390_guarded_storage, GS_ENABLE);
	if (0 != ret_code) {
		Trc_PRT_gs_initialize_Exception(errno, gsParams->flags);
	} else {
		J9GSControlBlock* gsControlBlock = (J9GSControlBlock*)(gsParams->controlBlock);
		/* Guarded load shift value, bits 53-56 in Guarded-Storage-Designation Register */
		gsControlBlock->designationRegister = (uint64_t) shiftAmount << 8;
		j9gs_load_gscb((void*)gsControlBlock);
		gsParams->flags |= J9PORT_GS_INITIALIZED;
	}
#endif /* !defined(J9ZTPF) */

	Trc_PRT_gs_initialize_Exit(ret_code, gsParams->flags);

	return IS_GS_INITIALIZED(gsParams) ? 1 : 0;
}

int32_t
j9gs_deinitialize(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams)
{
	int32_t ret_code = -1;
	Trc_PRT_gs_deinitialize_Entry(gsParams);

#if !defined(J9ZTPF)
	/*
	 * syscall is _s390_guarded_storage
	 * errno values from syscall routine is as follows:
	 * EINVAL     The value specified for command is not a valid command.
	 * EFAULT     The cp pointer is outside of the accessible address space.
	 * ENOMEM     Allocating memory for the guarded-storage control block failed.
	 * EOPNOTSUPP The guarded-storage facility is not available
	 */

	ret_code = syscall(__NR_s390_guarded_storage, GS_DISABLE);

	if (0 == ret_code) {
		gsParams->flags &= ~(J9PORT_GS_INITIALIZED);  /* Clear J9PORT_GS_INITIALIZE bit */
	} else {
		Trc_PRT_gs_deinitialize_Exception(errno, gsParams->flags);
	}
#endif /* !defined(J9ZTPF) */

	Trc_PRT_gs_deinitialize_Exit();

	return IS_GS_INITIALIZED(gsParams) ? 0 : 1;
}
