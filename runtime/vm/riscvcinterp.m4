dnl Copyright IBM Corp. and others 2021
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
dnl Exception M(1) and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception M(2).
dnl
dnl M(1) https://www.gnu.org/software/classpath/license.html
dnl M(2) https://openjdk.org/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0

include(riscvhelpers.m4)

    .file "riscvcinterp.s"

DECLARE_PUBLIC(cInterpreter)

START_PROC(c_cInterpreter)
    .cfi_def_cfa sp, 0
    addi sp, sp, - CINTERP_STACK_SIZE
    .cfi_def_cfa_offset CINTERP_STACK_SIZE

    GPR_SAVE(ra)

    GPR_SAVE(s0)
    GPR_SAVE(s1)
    GPR_SAVE(s2)
    GPR_SAVE(s3)
    GPR_SAVE(s4)
    GPR_SAVE(s5)
    GPR_SAVE(s6)
    GPR_SAVE(s7)
    GPR_SAVE(s8)
    GPR_SAVE(s9)
    GPR_SAVE(s10)
    GPR_SAVE(s11)

    FPR_SAVE(fs0)
    FPR_SAVE(fs1)
    FPR_SAVE(fs2)
    FPR_SAVE(fs3)
    FPR_SAVE(fs4)
    FPR_SAVE(fs5)
    FPR_SAVE(fs6)
    FPR_SAVE(fs7)
    FPR_SAVE(fs8)
    FPR_SAVE(fs9)
    FPR_SAVE(fs10)
    FPR_SAVE(fs11)

    mv J9VMTHREAD, a0
    ld  s8, M(J9VMTHREAD,J9TR_VMThread_entryLocalStorage)
    add s9, sp, JIT_GPR_SAVE_OFFSET(0)
    sd  s9, M(s8,J9TR_ELS_jitGlobalStorageBase)
    add s9, sp, JIT_FPR_SAVE_OFFSET(0)
    sd  s9, M(s8,J9TR_ELS_jitFPRegisterStorageBase)
cInterpreter:
    mv  a0, J9VMTHREAD
    ld  s8, M(J9VMTHREAD,J9TR_VMThread_javaVM)
    ld  s8, M(s8,J9TR_JavaVM_bytecodeLoop)
    jalr ra, s8, 0
    li s7, J9TR_bcloop_exit_interpreter
    beq a0, s7, .L_cInterpExit
    li s7, J9TR_bcloop_reenter_interpreter
    beq a0, s7, cInterpreter
    RESTORE_PRESERVED_REGS
    RESTORE_FPLR
    SWITCH_TO_JAVA_STACK
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
.L_cInterpExit:
    GPR_RESTORE(ra)

    GPR_RESTORE(s0)
    GPR_RESTORE(s1)
    GPR_RESTORE(s2)
    GPR_RESTORE(s3)
    GPR_RESTORE(s4)
    GPR_RESTORE(s5)
    GPR_RESTORE(s6)
    GPR_RESTORE(s7)
    GPR_RESTORE(s8)
    GPR_RESTORE(s9)
    GPR_RESTORE(s10)
    GPR_RESTORE(s11)

    FPR_RESTORE(fs0)
    FPR_RESTORE(fs1)
    FPR_RESTORE(fs2)
    FPR_RESTORE(fs3)
    FPR_RESTORE(fs4)
    FPR_RESTORE(fs5)
    FPR_RESTORE(fs6)
    FPR_RESTORE(fs7)
    FPR_RESTORE(fs8)
    FPR_RESTORE(fs9)
    FPR_RESTORE(fs10)
    FPR_RESTORE(fs11)

    addi sp, sp, CINTERP_STACK_SIZE
    .cfi_def_cfa_offset 0
    ret
END_PROC(c_cInterpreter)
