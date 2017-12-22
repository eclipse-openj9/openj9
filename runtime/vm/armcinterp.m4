dnl Copyright (c) 2017, 2017 IBM Corp. and others
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

include(armhelpers.m4)

	.file "armcinterp.s"

	DECLARE_PUBLIC(cInterpreter)

START_PROC(c_cInterpreter)
	sub r13,r13,{#}CINTERP_STACK_SIZE
	stmia r13,{{ r4,r5,r6,r7,r8,r9,r10,r11,r14 }}
	add r3,r13,{#}FPR_SAVE_OFFSET(0)
	vstmia.64 r3,{{ D8-D15 }}
	mov J9VMTHREAD,r0
	ldr r4,[J9VMTHREAD,{#}J9TR_VMThread_entryLocalStorage]	
	add r3,r13,{#}JIT_GPR_SAVE_OFFSET(0)
	str r3,[r4,{#}J9TR_ELS_jitGlobalStorageBase]
	add r3,r13,{#}JIT_FPR_SAVE_OFFSET(0)
	str r3,[r4,{#}J9TR_ELS_jitFPRegisterStorageBase]
cInterpreter:
	mov r0,J9VMTHREAD
	ldr r4,[J9VMTHREAD,{#}J9TR_VMThread_javaVM]
	mov r14,r15
	ldr r15,[r4,{#}J9TR_JavaVM_bytecodeLoop]
	cmp r0,{#}J9TR_bcloop_exit_interpreter
	beq .L_cInterpExit
	RESTORE_PRESERVED_REGS
	RESTORE_LR
	SWITCH_TO_JAVA_STACK
	ldr r15,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
.L_cInterpExit:
	ldmia r13,{{ r4,r5,r6,r7,r8,r9,r10,r11,r14 }}
	add r3,r13,{#}FPR_SAVE_OFFSET(0)
	vldmia.64 r3,{{ D8-D15 }}
	add r13,r13,{#}CINTERP_STACK_SIZE
	bx r14
END_PROC(c_cInterpreter)
