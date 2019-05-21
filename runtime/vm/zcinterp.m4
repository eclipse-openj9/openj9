dnl Copyright (c) 2017, 2019 IBM Corp. and others
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

include(zhelpers.m4)

    FILE_START(zcinterp)

BEGIN_C_FUNC(c_cInterpreter,1,CINTERP_STACK_SIZE)
ifdef({J9ZOS390},{
    ST_GPR CARG1,ARG_SAVE_OFFSET+CINTERP_STACK_SIZE(CSP)
    std fpr8,FPR_SAVE_OFFSET(0)(CSP)
    std fpr9,FPR_SAVE_OFFSET(1)(CSP)
    std fpr10,FPR_SAVE_OFFSET(2)(CSP)
    std fpr11,FPR_SAVE_OFFSET(3)(CSP)
    std fpr12,FPR_SAVE_OFFSET(4)(CSP)
    std fpr13,FPR_SAVE_OFFSET(5)(CSP)
    std fpr14,FPR_SAVE_OFFSET(6)(CSP)
    std fpr15,FPR_SAVE_OFFSET(7)(CSP)
    LR_GPR J9VMTHREAD,CARG1
    L_GPR r8,J9TR_VMThread_javaVM(J9VMTHREAD)
    l r8,J9TR_JavaVM_extendedRuntimeFlags(r8)
    lhi r9,J9TR_J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS
    nr r9,r8
    jz LABEL_NAME(L_NOVECTORSAVE)
    VSTM(16,23,VR_SAVE_OFFSET(0),CSP)
PLACE_LABEL(L_NOVECTORSAVE)
},{
ifdef({ASM_J9VM_ENV_DATA64},{
    std fpr8,FPR_SAVE_OFFSET(0)(CSP)
    std fpr9,FPR_SAVE_OFFSET(1)(CSP)
    std fpr10,FPR_SAVE_OFFSET(2)(CSP)
    std fpr11,FPR_SAVE_OFFSET(3)(CSP)
    std fpr12,FPR_SAVE_OFFSET(4)(CSP)
    std fpr13,FPR_SAVE_OFFSET(5)(CSP)
    std fpr14,FPR_SAVE_OFFSET(6)(CSP)
    std fpr15,FPR_SAVE_OFFSET(7)(CSP)
},{
    std fpr4,FPR_SAVE_OFFSET(0)(CSP)
    std fpr6,FPR_SAVE_OFFSET(1)(CSP)
})
    LR_GPR J9VMTHREAD,CARG1
})
    L_GPR CARG2,J9TR_VMThread_entryLocalStorage(J9VMTHREAD)
    la r0,JIT_GPR_SAVE_SLOT(0)
    ST_GPR r0,J9TR_ELS_jitGlobalStorageBase(CARG2)
    la r0,JIT_FPR_SAVE_SLOT(0)
    ST_GPR r0,J9TR_ELS_jitFPRegisterStorageBase(CARG2)
ifdef({ASM_J9VM_PORT_ZOS_CEEHDLRSUPPORT},{
    ST_GPR r0,J9TR_ELS_ceehdlrFPRBase(CARG2)
    la r0,CEEHDLR_GPR_SAVE_OFFSET(0)(CSP)
    ST_GPR r0,J9TR_ELS_ceehdlrGPRBase(CARG2)
    la r0,CEEHDLR_FPC_SAVE_OFFSET(CSP)
    ST_GPR r0,J9TR_ELS_ceehdlrFPCLocation(CARG2)
    ST_GPR CSP,J9TR_ELS_machineSPSaveSlot(CARG2)
})
PLACE_LABEL(L_CINTERP)
    L_GPR CARG2,J9TR_VMThread_javaVM(J9VMTHREAD)
    L_GPR CARG2,J9TR_JavaVM_bytecodeLoop(CARG2)
    LR_GPR CARG1,J9VMTHREAD
    CALL_INDIRECT(CARG2)
    LHI_GPR r0,J9TR_bcloop_exit_interpreter
    CLR_GPR CRINT,r0
    je LABEL_NAME(L_EXIT)
    RESTORE_LR
    RESTORE_PRESERVED_REGS_AND_SWITCH_TO_JAVA_STACK
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
PLACE_LABEL(L_EXIT)
ifdef({J9ZOS390},{
    ld fpr8,FPR_SAVE_OFFSET(0)(CSP)
    ld fpr9,FPR_SAVE_OFFSET(1)(CSP)
    ld fpr10,FPR_SAVE_OFFSET(2)(CSP)
    ld fpr11,FPR_SAVE_OFFSET(3)(CSP)
    ld fpr12,FPR_SAVE_OFFSET(4)(CSP)
    ld fpr13,FPR_SAVE_OFFSET(5)(CSP)
    ld fpr14,FPR_SAVE_OFFSET(6)(CSP)
    ld fpr15,FPR_SAVE_OFFSET(7)(CSP)
    L_GPR CARG2,J9TR_VMThread_javaVM(J9VMTHREAD)
    l CARG1,J9TR_JavaVM_extendedRuntimeFlags(CARG2)
    lhi r0,J9TR_J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS
    nr r0,CARG1
    jz LABEL_NAME(L_NOVECTORRESTORE)
    VLDM(16,23,VR_SAVE_OFFSET(0),CSP)
PLACE_LABEL(L_NOVECTORRESTORE)
},{
ifdef({ASM_J9VM_ENV_DATA64},{
    ld fpr8,FPR_SAVE_OFFSET(0)(CSP)
    ld fpr9,FPR_SAVE_OFFSET(1)(CSP)
    ld fpr10,FPR_SAVE_OFFSET(2)(CSP)
    ld fpr11,FPR_SAVE_OFFSET(3)(CSP)
    ld fpr12,FPR_SAVE_OFFSET(4)(CSP)
    ld fpr13,FPR_SAVE_OFFSET(5)(CSP)
    ld fpr14,FPR_SAVE_OFFSET(6)(CSP)
    ld fpr15,FPR_SAVE_OFFSET(7)(CSP)
},{
    ld fpr4,FPR_SAVE_OFFSET(0)(CSP)
    ld fpr6,FPR_SAVE_OFFSET(1)(CSP)
})
})
END_C_FUNC

BEGIN_FUNC(cInterpreter)
    J LABEL_NAME(L_CINTERP)
END_CURRENT

ifdef({ASM_J9VM_PORT_ZOS_CEEHDLRSUPPORT},{

BEGIN_FUNC(CEEJNIWrapper)
    L_GPR r6,J9TR_VMThread_entryLocalStorage(CARG1)
    L_GPR r6,J9TR_ELS_machineSPSaveSlot(r6)
    STM_GPR r0,r15,CEEHDLR_GPR_SAVE_OFFSET(0)(r6)
ifdef({ASM_J9VM_JIT_32BIT_USES64BIT_REGISTERS},{
    STMH_GPR r0,r15,CEEHDLR_GPR_SAVE_OFFSET(16)(r6)
})
    std fpr0,JIT_FPR_SAVE_OFFSET(0)(r6)
    std fpr1,JIT_FPR_SAVE_OFFSET(1)(r6)
    std fpr2,JIT_FPR_SAVE_OFFSET(2)(r6)
    std fpr3,JIT_FPR_SAVE_OFFSET(3)(r6)
    std fpr4,JIT_FPR_SAVE_OFFSET(4)(r6)
    std fpr5,JIT_FPR_SAVE_OFFSET(5)(r6)
    std fpr6,JIT_FPR_SAVE_OFFSET(6)(r6)
    std fpr7,JIT_FPR_SAVE_OFFSET(7)(r6)
    std fpr8,JIT_FPR_SAVE_OFFSET(8)(r6)
    std fpr9,JIT_FPR_SAVE_OFFSET(9)(r6)
    std fpr10,JIT_FPR_SAVE_OFFSET(10)(r6)
    std fpr11,JIT_FPR_SAVE_OFFSET(11)(r6)
    std fpr12,JIT_FPR_SAVE_OFFSET(12)(r6)
    std fpr13,JIT_FPR_SAVE_OFFSET(13)(r6)
    std fpr14,JIT_FPR_SAVE_OFFSET(14)(r6)
    std fpr15,JIT_FPR_SAVE_OFFSET(15)(r6)
    stfpc CEEHDLR_FPC_SAVE_OFFSET(r6)
    L_GPR r6,J9TR_VMThread_tempSlot(CARG1)
    lm r5,r6,16(r6)
    br r6
END_CURRENT

})

ifdef({OMR_GC_CONCURRENT_SCAVENGER},{
define({HANDLE_HARDWARE_READ_BARRIER},{
BEGIN_HELPER($1)
    SAVE_ALL_REGS($1)

dnl Test the TX bit of GSECI (bit 16) to see if the CPU was in transactional-
dnl execution mode when the guarded storage event was recognized. Subsequently
dnl test the CX bit of GSECI (bit 17) to see which type of transaction we were
dnl executing. At that point we have two cases:
dnl
dnl 1. TX was a non-constrained transaction:
dnl    If this was the case do not execute the read barrier and simply return to
dnl    the fallback transaction abort JIT code to handle it by forcing condition
dnl    code to be 2 with the below CL(G)FI instruction indicating a transient 
dnl    transaction abort.
dnl
dnl 2. TX was a constrained transaction:
dnl    This should not happen because we cannot handle constrained transactions
dnl    in the hardware read barrier case. Because executing any of the following
dnl    code will be functionally incorrect we force a crash here so we fail fast
dnl    and can manually debug.
dnl
dnl See the following PRs for more details:
dnl eclipse/openj9#2545
dnl eclipse/openj9#2790
dnl eclipse/openj9#3763
    TM J9TR_VMThread_gsParameters_GSECI(J9VMTHREAD),128
    JZ LABEL_NAME(L_GS_CALL_HELPER)
    TM J9TR_VMThread_gsParameters_GSECI(J9VMTHREAD),64
    JNZ LABEL_NAME(L_GS_FORCE_CRASH)
    CLFI_GPR J9SP,0
    J LABEL_NAME(L_GS_SKIP_HELPER)

PLACE_LABEL(L_GS_CALL_HELPER)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    LR_GPR CARG1,J9VMTHREAD
    L_GPR CRA,J9TR_VMThread_javaVM(J9VMTHREAD)
    L_GPR CRA,J9TR_JavaVM_invokeJ9ReadBarrier(CRA)
    CALL_INDIRECT(CRA)

dnl Load the updated object pointer into CARG2
    LG CARG1,J9TR_VMThread_gsParameters_operandAddr(J9VMTHREAD)
ifdef({ASM_OMR_GC_COMPRESSED_POINTERS},{
dnl Some objects may be stored in a decompressed format on the heap.
dnl A typical example of this may be static objects. For such objects
dnl we must not decompress via the compressedPointersShift value. To
dnl check for this we disassemble the guarded load instruction in the
dnl mainline and check whether it is an LGG or LLGFSG instruction.
    LG CARG2,J9TR_VMThread_gsParameters_instructionAddr(J9VMTHREAD)
    TM 5(CARG2),4
    JNZ LABEL_NAME(L_GS_SKIP_SHIFT)
    LLGF CARG1,0(CARG1)
    LG CARG2,J9TR_VMThread_javaVM(J9VMTHREAD)
    LG CARG2,J9TR_JavaVM_compressedPointersShift(CARG2)
    SLLG CARG2,CARG1,0(CARG2)
    J LABEL_NAME(L_GS_DONE_SHIFT)
})

PLACE_LABEL(L_GS_SKIP_SHIFT)
    LG CARG2,0(CARG1)
PLACE_LABEL(L_GS_DONE_SHIFT)

dnl Extract the target address register
    LG CARG1,J9TR_VMThread_gsParameters_instructionAddr(J9VMTHREAD)
    LLGH CARG1,0(CARG1)

dnl We combine a shift right by 4 and a multiply by 8 to a shift
dnl right by 1 to get the byte offset into the save area where to
dnl store the updated object pointer
    ENCODE_RISBGN(CARG1,CARG1,57,188,63)

dnl Finally store the updated object pointer into the save slot
    STG CARG2,JIT_GPR_SAVE_OFFSET(0)(CARG1,CSP)

dnl Skip past the guarded load instruction by updating the returnAddr
dnl to 6 bytes (length of the guarded load instruction) past the
dnl original value
    ENCODE_AGSI(J9TR_VMThread_gsParameters_returnAddr,J9VMTHREAD,6)

    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    ST_GPR J9SP,JIT_GPR_SAVE_SLOT(J9SP)
PLACE_LABEL(L_GS_SKIP_HELPER)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK($1)
    ENCODE_BIC(15,J9TR_VMThread_gsParameters_returnAddr,0,J9VMTHREAD)
PLACE_LABEL(L_GS_FORCE_CRASH)
    BYTE(0)
    BYTE(0)
END_CURRENT
})

HANDLE_HARDWARE_READ_BARRIER(handleHardwareReadBarrier)
})

    FILE_END
