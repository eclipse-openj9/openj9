dnl Copyright IBM Corp. and others 2023
dnl
dnl This program and the accompanying materials are made available under
dnl the terms of the Eclipse Public License 2.0 which accompanies this
dnl distribution and is available at https://www.eclipse.org/legal/epl-2.0/
dnl or the Apache License, Version 2.0 which accompanies this distribution and
dnl is available at https://www.apache.org/licenses/LICENSE-2.0.
dnl
dnl This Source Code may also be made available under the following
dnl Secondary Licenses when the conditions for such availability set
dnl forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
dnl General Public License, version 2 with the GNU Classpath
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] https://openjdk.org/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0

include(xhelpers.m4)

	FILE_START

dnl For all of these functions, on entry:
dnl
dnl 1) return address on the stack
dnl 2) r8 is a scratch register on 64-bit
dnl 3) eax is a scratch register on 32-bit

START_PROC(jitSaveVectorRegistersAVX512)
	lfence

	dnl save ZMM registers

ifdef({ASM_J9VM_ENV_DATA64},{
	pop r8
	forloop({REG_CTR}, 0, 31, {SAVE_ZMM_REG(REG_CTR, J9TR_cframe_jitFPRs+(REG_CTR*64))})
}, { dnl ASM_J9VM_ENV_DATA64
	pop eax
	forloop({REG_CTR}, 0, 7, {SAVE_ZMM_REG(REG_CTR, J9TR_cframe_jitFPRs+(REG_CTR*64))})
})

	vzeroupper

	dnl save Opmask registers
	forloop({REG_CTR}, 0, 7, {SAVE_MASK_64(REG_CTR, J9TR_cframe_maskRegisters+(REG_CTR*8))})

ifdef({ASM_J9VM_ENV_DATA64},{
	push r8
}, { dnl ASM_J9VM_ENV_DATA64
	push eax
})
	ret
END_PROC(jitSaveVectorRegistersAVX512)

START_PROC(jitRestoreVectorRegistersAVX512)
	lfence

	dnl restore ZMM registers
ifdef({ASM_J9VM_ENV_DATA64},{
	pop r8
	forloop({REG_CTR}, 0, 31, {RESTORE_ZMM_REG(REG_CTR, J9TR_cframe_jitFPRs+(REG_CTR*64))})
}, { dnl ASM_J9VM_ENV_DATA64
	pop eax
	forloop({REG_CTR}, 0, 7, {RESTORE_ZMM_REG(REG_CTR, J9TR_cframe_jitFPRs+(REG_CTR*64))})
})

	dnl restore Opmask registers
	forloop({REG_CTR}, 0, 7, {RESTORE_MASK_64(REG_CTR, J9TR_cframe_maskRegisters+(REG_CTR*8))})

ifdef({ASM_J9VM_ENV_DATA64},{
	push r8
}, { dnl ASM_J9VM_ENV_DATA64
	push eax
})
	ret
END_PROC(jitRestoreVectorRegistersAVX512)

START_PROC(jitSaveVectorRegistersAVX)
	lfence

	dnl save YMM registers

ifdef({ASM_J9VM_ENV_DATA64},{
	pop r8
	forloop({REG_CTR}, 0, 15, {vmovdqu ymmword ptr J9TR_cframe_jitFPRs+(REG_CTR*32)[_rsp],ymm{}REG_CTR})
}, { dnl ASM_J9VM_ENV_DATA64
	pop eax
	forloop({REG_CTR}, 0, 7, {vmovdqu ymmword ptr J9TR_cframe_jitFPRs+(REG_CTR*32)[_rsp],ymm{}REG_CTR})
})

	vzeroupper

ifdef({ASM_J9VM_ENV_DATA64},{
	push r8
}, { dnl ASM_J9VM_ENV_DATA64
	push eax
})
	ret
END_PROC(jitSaveVectorRegistersAVX)

START_PROC(jitRestoreVectorRegistersAVX)
	lfence

	dnl restore YMM registers
ifdef({ASM_J9VM_ENV_DATA64},{
	pop r8
	forloop({REG_CTR}, 0, 15, {vmovdqu ymm{}REG_CTR,ymmword ptr J9TR_cframe_jitFPRs+(REG_CTR*32)[_rsp]})
}, { dnl ASM_J9VM_ENV_DATA64
	pop eax
	forloop({REG_CTR}, 0, 7, {vmovdqu ymm{}REG_CTR,ymmword ptr J9TR_cframe_jitFPRs+(REG_CTR*32)[_rsp]})
})

ifdef({ASM_J9VM_ENV_DATA64},{
	push r8
}, { dnl ASM_J9VM_ENV_DATA64
	push eax
})
	ret
END_PROC(jitRestoreVectorRegistersAVX)

	FILE_END
