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

#if defined(S390) || defined(J9ZOS390)
#include <math.h>
#include "j9gs.h"

#if defined(J9ZOS390)
#pragma linkage (GSLDCB,OS)
#pragma map(j9gs_load_gscb,"GSLDCB")
void j9gs_load_gscb(void*);
#else
static void
j9gs_load_gscb(void * gscb_controls)
{
	J9GSControlBlock* gsControlBlock = (J9GSControlBlock*) gscb_controls;
	asm volatile(".insn rxy,0xe3000000004d,0,%0"
			     : : "Q" (*gsControlBlock));
}
#endif

#define GSC_OFFSET 6
#define GSC_OFFSET_MASK 0xFFFFFFFFFFFFFFFFULL

void
j9gs_params_init(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void *gsControlBlock)
{
	Trc_PRT_gs_params_init_Entry(gsParams, gsControlBlock);
	gsParams->flags = 0;
	gsParams->controlBlock = gsControlBlock;
	Trc_PRT_gs_params_init_Exit();
}

void
j9gs_enable(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void* baseAddress, uint64_t perBitSectionSize, uint64_t bitMask)
{
	Trc_PRT_gs_enable_Entry(gsParams, baseAddress, perBitSectionSize, bitMask);
	if ((gsParams->flags & J9PORT_GS_INITIALIZED) != 0) {

		J9GSControlBlock* gsControlBlock = (J9GSControlBlock*)(gsParams->controlBlock);

		/* Zero out GSD Register except for GSL bits 53-56 */
		gsControlBlock->designationRegister &= 0x0000000000000700ULL;

		/* gsCharacteristic is calculated based on perBitSectionSize (always power of 2),
		 * bits 56-63 in GSD register
		 */
		uint64_t gsCharacteristic = GSC_OFFSET + log2(perBitSectionSize);
		gsControlBlock->designationRegister |= gsCharacteristic;
		uintptr_t baseAddressMasked = ((uintptr_t)baseAddress & (GSC_OFFSET_MASK << gsCharacteristic));
		if (baseAddressMasked != (uintptr_t) baseAddress) {
			Trc_PRT_gs_enable_alignment_Exception(-1, gsParams->flags);
		} else {
			/* GSO address bits 0-J, J = 63-GSC */
			gsControlBlock->designationRegister |= baseAddressMasked;

			gsControlBlock->sectionMask = bitMask;
			j9gs_load_gscb((void*)gsControlBlock);
			gsParams->flags |= J9PORT_GS_ENABLED;
		}

	} else {
		Trc_PRT_gs_enable_Exception(-1, gsParams->flags);
	}
	Trc_PRT_gs_enable_Exit();

}

void
j9gs_disable(struct J9PortLibrary *portLibrary,  struct J9GSParameters *gsParams)
{
	Trc_PRT_gs_disable_Entry(gsParams);
	if ((gsParams->flags & J9PORT_GS_INITIALIZED) != 0) {
		if ((gsParams->flags & J9PORT_GS_ENABLED) != 0) {
			((J9GSControlBlock*)gsParams->controlBlock)->sectionMask = 0x0;
			j9gs_load_gscb((void*)gsParams->controlBlock);
			gsParams->flags &= ~(J9PORT_GS_ENABLED);
		}
	} else {
		Trc_PRT_gs_disable_Exception(-1, gsParams->flags);
	}
	Trc_PRT_gs_disable_Exit();
}

int32_t
j9gs_isEnabled(struct J9PortLibrary *portLibrary, struct J9GSParameters *gsParams, void** baseAddress, uint64_t* perBitSectionSize, uint64_t* bitMask)
{
	Trc_PRT_gs_isEnabled_Entry(gsParams);
	uint32_t ret_code = 0;
	if ((gsParams->flags & J9PORT_GS_INITIALIZED) != 0 && (gsParams->flags & J9PORT_GS_ENABLED) != 0) {
		J9GSControlBlock* gsControlBlock = (J9GSControlBlock*)(gsParams->controlBlock);
		uint64_t gsCharacteristic = (gsControlBlock->designationRegister & 0x00000000000000FFULL);
		*perBitSectionSize = pow(2.0, (gsCharacteristic - GSC_OFFSET));
		*baseAddress = (void*)(intptr_t)(gsControlBlock->designationRegister & ~(0x0000000000000FFFULL) );
		*bitMask = gsControlBlock->sectionMask;
		ret_code = 1;
	}
	Trc_PRT_gs_isEnabled_Exit(ret_code,*baseAddress, *perBitSectionSize, *bitMask);
	return ret_code;
}

#endif

