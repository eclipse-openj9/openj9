dnl Copyright (c) 2019, 2019 IBM Corp. and others
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

include(arm64helpers.m4)

	.file "arm64cinterp.s"

DECLARE_PUBLIC(cInterpreter)

START_PROC(c_cInterpreter)
	sub sp, sp, {#}CINTERP_STACK_SIZE
	stp x19, x20, [sp, GPR_SAVE_OFFSET(19)]
	stp x21, x22, [sp, GPR_SAVE_OFFSET(21)]
	stp x23, x24, [sp, GPR_SAVE_OFFSET(23)]
	stp x25, x26, [sp, GPR_SAVE_OFFSET(25)]
	stp x27, x28, [sp, GPR_SAVE_OFFSET(27)]
	stp x29, x30, [sp, GPR_SAVE_OFFSET(29)]
	stp d8, d9, [sp, FPR_SAVE_OFFSET(8)]
	stp d10, d11, [sp, FPR_SAVE_OFFSET(10)]
	stp d12, d13, [sp, FPR_SAVE_OFFSET(12)]
	stp d14, d15, [sp, FPR_SAVE_OFFSET(14)]
	mov J9VMTHREAD, x0
	ldr x27, [J9VMTHREAD,{#}J9TR_VMThread_entryLocalStorage]
	add x28, sp, {#}JIT_GPR_SAVE_OFFSET(0)
	str x28, [x27,{#}J9TR_ELS_jitGlobalStorageBase]
	add x28, sp, {#}JIT_FPR_SAVE_OFFSET(0)
	str x28, [x27,{#}J9TR_ELS_jitFPRegisterStorageBase]
cInterpreter:
	mov x0, J9VMTHREAD
	ldr x27, [J9VMTHREAD,{#}J9TR_VMThread_javaVM]
	ldr x28, [x27,{#}J9TR_JavaVM_bytecodeLoop]
	blr x28
	cmp x0, {#}J9TR_bcloop_exit_interpreter
	beq .L_cInterpExit
	RESTORE_PRESERVED_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
.L_cInterpExit:
	ldp x19, x20, [sp, GPR_SAVE_OFFSET(19)]
	ldp x21, x22, [sp, GPR_SAVE_OFFSET(21)]
	ldp x23, x24, [sp, GPR_SAVE_OFFSET(23)]
	ldp x25, x26, [sp, GPR_SAVE_OFFSET(25)]
	ldp x27, x28, [sp, GPR_SAVE_OFFSET(27)]
	ldp x29, x30, [sp, GPR_SAVE_OFFSET(29)]
	ldp d8, d9, [sp, FPR_SAVE_OFFSET(8)]
	ldp d10, d11, [sp, FPR_SAVE_OFFSET(10)]
	ldp d12, d13, [sp, FPR_SAVE_OFFSET(12)]
	ldp d14, d15, [sp, FPR_SAVE_OFFSET(14)]
	add sp, sp, {#}CINTERP_STACK_SIZE
	ret
END_PROC(c_cInterpreter)
