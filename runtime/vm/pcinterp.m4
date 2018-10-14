dnl Copyright (c) 2017, 2018 IBM Corp. and others
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
dnl [2] http://openjdk.java.net/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

include(phelpers.m4)

	.file "pcinterp.s"

define({CSECT_NAME},{c_cInterpreter})

START_PROC(c_cInterpreter)
ifdef({ASM_J9VM_ENV_DATA64},{
ifdef({ASM_J9VM_ENV_LITTLE_ENDIAN},{
	addis r2,r12,(.TOC.-c_cInterpreter)@ha
	addi r2,r2,(.TOC.-c_cInterpreter)@l
	.localentry c_cInterpreter,.-c_cInterpreter
}) dnl ASM_J9VM_ENV_LITTLE_ENDIAN
}) dnl ASM_J9VM_ENV_DATA64
	staddru r1,-CINTERP_STACK_SIZE(r1)
	mflr r0
	staddr r0,LR_SAVE_OFFSET(r1)
	mfcr r0
	staddr r0,CR_SAVE_OFFSET(r1)
ifdef({SAVE_R13},{
	SAVE_GPR(13)
})
	SAVE_GPR(14)
	SAVE_GPR(15)
	SAVE_GPR(16)
	SAVE_GPR(17)
	SAVE_GPR(18)
	SAVE_GPR(19)
	SAVE_GPR(20)
	SAVE_GPR(21)
	SAVE_GPR(22)
	SAVE_GPR(23)
	SAVE_GPR(24)
	SAVE_GPR(25)
	SAVE_GPR(26)
	SAVE_GPR(27)
	SAVE_GPR(28)
	SAVE_GPR(29)
	SAVE_GPR(30)
	SAVE_GPR(31)
	SAVE_FPR(14)
	SAVE_FPR(15)
	SAVE_FPR(16)
	SAVE_FPR(17)
	SAVE_FPR(18)
	SAVE_FPR(19)
	SAVE_FPR(20)
	SAVE_FPR(21)
	SAVE_FPR(22)
	SAVE_FPR(23)
	SAVE_FPR(24)
	SAVE_FPR(25)
	SAVE_FPR(26)
	SAVE_FPR(27)
	SAVE_FPR(28)
	SAVE_FPR(29)
	SAVE_FPR(30)
	SAVE_FPR(31)
 	mr J9VMTHREAD,r3
	laddr r4,J9TR_VMThread_entryLocalStorage(r3)
	addi r0,r1,JIT_GPR_SAVE_OFFSET(0)
	staddr r0,J9TR_ELS_jitGlobalStorageBase(r4)
	addi r0,r1,JIT_FPR_SAVE_OFFSET(0)
	staddr r0,J9TR_ELS_jitFPRegisterStorageBase(r4)
	li r3,-1
ifdef({ASM_J9VM_ENV_DATA64},{
	staddr r3,JIT_GPR_SAVE_SLOT(17)
	laddr r3,J9TR_VMThread_javaVM(J9VMTHREAD)
	laddr r3,J9TR_JavaVMJitConfig(r3)
	cmpliaddr r3,0
	beq .L_noJIT
	laddr r3,J9TR_JitConfig_pseudoTOC(r3)
	staddr r3,JIT_GPR_SAVE_SLOT(16)
.L_noJIT:
},{ dnl ASM_J9VM_ENV_DATA64
	staddr r3,JIT_GPR_SAVE_SLOT(15)
}) dnl ASM_J9VM_ENV_DATA64
.L_cInterpOnCStack:
	mr r3,J9VMTHREAD
	laddr FUNC_PTR,J9TR_VMThread_javaVM(J9VMTHREAD)
	laddr FUNC_PTR,J9TR_JavaVM_bytecodeLoop(FUNC_PTR)
	CALL_INDIRECT
	cmpliaddr r3,J9TR_bcloop_exit_interpreter
	beq .L_cInterpExit
	RESTORE_PRESERVED_REGS
	RESTORE_LR
	SWITCH_TO_JAVA_STACK
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
.L_cInterpExit:
	laddr r0,LR_SAVE_OFFSET(r1)
	mtlr r0
	laddr r0,CR_SAVE_OFFSET(r1)
	mtcr r0
ifdef({SAVE_R13},{
	RESTORE_GPR(13)
})
	RESTORE_GPR(14)
	RESTORE_GPR(15)
	RESTORE_GPR(16)
	RESTORE_GPR(17)
	RESTORE_GPR(18)
	RESTORE_GPR(19)
	RESTORE_GPR(20)
	RESTORE_GPR(21)
	RESTORE_GPR(22)
	RESTORE_GPR(23)
	RESTORE_GPR(24)
	RESTORE_GPR(25)
	RESTORE_GPR(26)
	RESTORE_GPR(27)
	RESTORE_GPR(28)
	RESTORE_GPR(29)
	RESTORE_GPR(30)
	RESTORE_GPR(31)
	RESTORE_FPR(14)
	RESTORE_FPR(15)
	RESTORE_FPR(16)
	RESTORE_FPR(17)
	RESTORE_FPR(18)
	RESTORE_FPR(19)
	RESTORE_FPR(20)
	RESTORE_FPR(21)
	RESTORE_FPR(22)
	RESTORE_FPR(23)
	RESTORE_FPR(24)
	RESTORE_FPR(25)
	RESTORE_FPR(26)
	RESTORE_FPR(27)
	RESTORE_FPR(28)
	RESTORE_FPR(29)
	RESTORE_FPR(30)
	RESTORE_FPR(31)
	addi r1,r1,CINTERP_STACK_SIZE
	blr
END_PROC(c_cInterpreter)

START_PROC(cInterpreter)
	b .L_cInterpOnCStack
END_PROC(cInterpreter)
