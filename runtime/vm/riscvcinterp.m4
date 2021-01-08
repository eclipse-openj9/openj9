dnl Copyright (c) 2021, 2021 IBM Corp. and others
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
dnl M(2) http://openjdk.java.net/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

include(riscvhelpers.m4)

    .file "riscvcinterp.s"

DECLARE_PUBLIC(cInterpreter)

START_PROC(c_cInterpreter)
    addi sp, sp, - CINTERP_STACK_SIZE

    sd  ra,  GPR_SAVE_SLOT(12)

    sd  s0,  GPR_SAVE_SLOT(0)
    sd  s1,  GPR_SAVE_SLOT(1)
    sd  s2,  GPR_SAVE_SLOT(2)
    sd  s3,  GPR_SAVE_SLOT(3)
    sd  s4,  GPR_SAVE_SLOT(4)
    sd  s5,  GPR_SAVE_SLOT(5)
    sd  s6,  GPR_SAVE_SLOT(6)
    sd  s7,  GPR_SAVE_SLOT(7)
    sd  s8,  GPR_SAVE_SLOT(8)
    sd  s9,  GPR_SAVE_SLOT(9)
    sd  s10, GPR_SAVE_SLOT(10)
    sd  s11, GPR_SAVE_SLOT(11)

    fsd fs0, FPR_SAVE_SLOT(0)
    fsd fs1, FPR_SAVE_SLOT(1)
    fsd fs2, FPR_SAVE_SLOT(2)
    fsd fs3, FPR_SAVE_SLOT(3)
    fsd fs4, FPR_SAVE_SLOT(4)
    fsd fs5, FPR_SAVE_SLOT(5)
    fsd fs6, FPR_SAVE_SLOT(6)
    fsd fs7, FPR_SAVE_SLOT(7)
    fsd fs8, FPR_SAVE_SLOT(8)
    fsd fs9, FPR_SAVE_SLOT(9)
    fsd fs10,FPR_SAVE_SLOT(10)
    fsd fs11,FPR_SAVE_SLOT(11)

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
    RESTORE_PRESERVED_REGS
    RESTORE_FPLR
    SWITCH_TO_JAVA_STACK
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
.L_cInterpExit:
    ld  ra,  GPR_SAVE_SLOT(12)

    ld  s0,  GPR_SAVE_SLOT(0)
    ld  s1,  GPR_SAVE_SLOT(1)
    ld  s2,  GPR_SAVE_SLOT(2)
    ld  s3,  GPR_SAVE_SLOT(3)
    ld  s4,  GPR_SAVE_SLOT(4)
    ld  s5,  GPR_SAVE_SLOT(5)
    ld  s6,  GPR_SAVE_SLOT(6)
    ld  s7,  GPR_SAVE_SLOT(7)
    ld  s8,  GPR_SAVE_SLOT(8)
    ld  s9,  GPR_SAVE_SLOT(9)
    ld  s10, GPR_SAVE_SLOT(10)
    ld  s11, GPR_SAVE_SLOT(11)

    fld fs0, FPR_SAVE_SLOT(0)
    fld fs1, FPR_SAVE_SLOT(1)
    fld fs2, FPR_SAVE_SLOT(2)
    fld fs3, FPR_SAVE_SLOT(3)
    fld fs4, FPR_SAVE_SLOT(4)
    fld fs5, FPR_SAVE_SLOT(5)
    fld fs6, FPR_SAVE_SLOT(6)
    fld fs7, FPR_SAVE_SLOT(7)
    fld fs8, FPR_SAVE_SLOT(8)
    fld fs9, FPR_SAVE_SLOT(9)
    fld fs10,FPR_SAVE_SLOT(10)
    fld fs11,FPR_SAVE_SLOT(11)


    addi sp, sp, CINTERP_STACK_SIZE
    ret
END_PROC(c_cInterpreter)
