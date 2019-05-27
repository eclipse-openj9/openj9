ifdef(`J9ZOS390',`dnl
define(`ZZ',`**')
',`dnl
define(`ZZ',`##')
')dnl

ZZ Copyright (c) 2000, 2019 IBM Corp. and others
ZZ
ZZ This program and the accompanying materials are made 
ZZ available under the terms of the Eclipse Public License 2.0 
ZZ which accompanies this distribution and is available at 
ZZ https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
ZZ Version 2.0 which accompanies this distribution and is available 
ZZ at https://www.apache.org/licenses/LICENSE-2.0.
ZZ
ZZ This Source Code may also be made available under the following
ZZ Secondary Licenses when the conditions for such availability set
ZZ forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
ZZ General Public License, version 2 with the GNU Classpath 
ZZ Exception [1] and GNU General Public License, version 2 with the
ZZ OpenJDK Assembly Exception [2].
ZZ
ZZ [1] https://www.gnu.org/software/classpath/license.html
ZZ [2] http://openjdk.java.net/legal/assembly-exception.html
ZZ
ZZ SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
ZZ GPL-2.0 WITH Classpath-exception-2.0 OR
ZZ LicenseRef-GPL-2.0 WITH Assembly-exception

ZZ ===================================================================
ZZ The above macro replaces ZZ with appropriate line comment
ZZ For zLinux GAS assembler and zOS HLASM assembler
ZZ ===================================================================

ZZ ===================================================================
ZZ changequote is used to replace ` with [ and ' with ]
ZZ For better readability
ZZ ===================================================================
changequote([,])dnl

ZZ ===================================================================
ZZ For zOS, We declare just one csect with multiple entry points.
ZZ Each PICBuilder function will be declared as an entrypoint in this
ZZ csect.
ZZ ===================================================================

ifdef([J9ZOS390],[dnl
        TITLE 'Recompilation.s'
RECOMP#START      CSECT
RECOMP#START      RMODE ANY
ifdef([TR_HOST_64BIT],[dnl
RECOMP#START      AMODE 64
],[dnl
RECOMP#START      AMODE 31
])dnl
])dnl

ZZ ===================================================================
ZZ Author  : Chris Donawa
ZZ Version : 0.0
ZZ Date    : Aug 2003
ZZ ===================================================================

ZZ ===================================================================
ZZ codert/jilconsts.inc is a j9 assembler include that defines
ZZ offsets of various fields in structs used by jit
ZZ ===================================================================

include([jilconsts.inc])dnl

ZZ ===================================================================
ZZ codert.dev/s390_asdef.inc is a JIT assembler include that defines
ZZ macros for instructions for 64 bit and 31 bit mode. Appropriate set
ZZ of instruction macros are selected for each mode
ZZ ===================================================================

include([z/runtime/s390_asdef.inc])dnl


ZZ ===================================================================
ZZ codert.dev/s390_macros.inc is a JIT assembler include that defines
ZZ various m4 macros used in this file. It includes zOS_macros.inc on
ZZ zOS and zLinux_macros.inc on zLinux
ZZ ===================================================================

include([z/runtime/s390_macros.inc])dnl

ZZ ===================================================================
ZZ codert.dev/codertTOC.inc is a JIT assembler include that defines
ZZ index of various runtime helper functions addressable from
ZZ codertTOC.
ZZ ===================================================================

include([runtime/Helpers.inc])dnl

ZZ ===================================================================
ZZ Offsets of items in various code snippets
ZZ ===================================================================
SETVAL(eq_BodyInfo_prePrologue,0)
SETVAL(ALen,PTR_SIZE)
ZZ ===================================================================
ZZ Definitions for various offsets used by PicBuilder code
ZZ ===================================================================
ZZ Code base register for PIC method
SETVAL(breg,r10)
SETVAL(FPLen,4)
SETVAL(DblLen,8)
ZZ Snippet Base Register
SETVAL(sbase,r11)
SETVAL(eq_JitCompilerAddr,0)
ZZ previous field only 4 bytes big, but compiler aligns
ZZ to double word boundary on 64 bit
SETVAL(eq_BodyInfo_MethodInfo,PTR_SIZE)
SETVAL(eq_VMMethodInfo_j9Method,0)

SETVAL(FPSlot0,0*DblLen)
SETVAL(FPSlot1,1*DblLen)
SETVAL(FPSlot2,2*DblLen)
SETVAL(FPSlot3,3*DblLen)
SETVAL(FPSlot4,4*DblLen)
SETVAL(FPSlot5,5*DblLen)
SETVAL(FPSlot6,6*DblLen)
SETVAL(FPSlot7,7*DblLen)

ZZ Counting has 3 items on stack that count towards it's
ZZ total stack size.  Async compilation needs to know the
ZZ stack size for stack traversal.  We buffer the *sampling*
ZZ version so the two routines have identical stack size
SETVAL(countTempSize,2*PTR_SIZE)


SETVAL(FloatFrameSize,8*DblLen)
SETVAL(SamplingFrameSize,countTempSize+FloatFrameSize+SampleGPRSaveAr)
SETVAL(SamplingFPSlot0,SampleGPRSaveAr+FPSlot0)
SETVAL(SamplingFPSlot1,SampleGPRSaveAr+FPSlot1)
SETVAL(SamplingFPSlot2,SampleGPRSaveAr+FPSlot2)
SETVAL(SamplingFPSlot3,SampleGPRSaveAr+FPSlot3)
SETVAL(SamplingFPSlot4,SampleGPRSaveAr+FPSlot4)
SETVAL(SamplingFPSlot5,SampleGPRSaveAr+FPSlot5)
SETVAL(SamplingFPSlot6,SampleGPRSaveAr+FPSlot6)
SETVAL(SamplingFPSlot7,SampleGPRSaveAr+FPSlot7)


SETVAL(CountingGPRSaveArea,6*PTR_SIZE)
SETVAL(CountingStackSize,FloatFrameSize+CountingGPRSaveArea)
SETVAL(CountingFPSlot0,CountingGPRSaveArea+FPSlot0)
SETVAL(CountingFPSlot1,CountingGPRSaveArea+FPSlot1)
SETVAL(CountingFPSlot2,CountingGPRSaveArea+FPSlot2)
SETVAL(CountingFPSlot3,CountingGPRSaveArea+FPSlot3)
SETVAL(CountingFPSlot4,CountingGPRSaveArea+FPSlot4)
SETVAL(CountingFPSlot5,CountingGPRSaveArea+FPSlot5)
SETVAL(CountingFPSlot6,CountingGPRSaveArea+FPSlot6)
SETVAL(CountingFPSlot7,CountingGPRSaveArea+FPSlot7)

ZZ This is the stack frame for the patching of brasl
ifdef([MCC_SUPPORTED],[dnl
SETVAL(allocStackSize,9*PTR_SIZE)
SETVAL(InterfaceSnippetEP_onStack,9*PTR_SIZE)
SETVAL(ExtraArgPatchCallParam_onStack,8*PTR_SIZE)
SETVAL(NewPCPatchCallParam_onStack,7*PTR_SIZE)
SETVAL(CallSitePatchCallParam_onStack,6*PTR_SIZE)
SETVAL(MethPtrPatchCallParam_onStack,5*PTR_SIZE)
SETVAL(RetAddrPatchCallParam_onStack,4*PTR_SIZE)
SETVAL(SaveR1PatchCallParam_onStack,3*PTR_SIZE)
SETVAL(SaveREPPatchCallParam_onStack,2*PTR_SIZE)
],[dnl
ZZ allocStackSize is the number of bytes to be
ZZ allocated on the stack to spill registers 
ZZ which are used inside the function
ZZ for non-MCC we always need 4 64-bit registers
SETVAL(allocStackSize,32)
SETVAL(InterfaceSnippetEP_onStack,32)
])dnl

ZZ Force Recomp Snippet Layout
SETVAL(RetAddrInForceRecompSnippet,0)
SETVAL(StartPCInForceRecompSnippet,PTR_SIZE)

ZZ Induce Recomp Stack Layout for jitCallCFunction
ZZ Preserve: r1,r2,r3
ZZ Params: retAddr, StartPC, VMThread.

SETVAL(VMThreadIndRecompCallParam_onStack,5*PTR_SIZE)
SETVAL(StartPCIndRecompCallParam_onStack,4*PTR_SIZE)
SETVAL(RetAddrIndRecompCallParam_onStack,3*PTR_SIZE)
SETVAL(SaveR3IndRecompCallParam_onStack,2*PTR_SIZE)
SETVAL(SaveR2IndRecompCallParam_onStack,PTR_SIZE)
SETVAL(SaveR1IndRecompCallParam_onStack,0)

ZZ initialInvokeExactThunk_unwrapper Stack Layout for jitCallCFunction
ZZ Preserve: r1,r2,r3
ZZ Params: receiver MethodHandle, VMThread pointer

SETVAL(SaveR14_IIEThunkParm_onStack,5*PTR_SIZE)
SETVAL(SaveR3_IIEThunkParm_onStack,4*PTR_SIZE)
SETVAL(SaveR2_IIEThunkParm_onStack,3*PTR_SIZE)
SETVAL(SaveR1_IIEThunkParm_onStack,2*PTR_SIZE)
SETVAL(VMThread_IIEThunkParm_onStack,1*PTR_SIZE)
SETVAL(MethodHandle_IIEThunkParm_onStack,0*PTR_SIZE)
SETVAL(ParmsArray_IIEThunkParm_onStack,0*PTR_SIZE)
SETVAL(ReturnedStartPC_IIEThunkParm_onStack,0*PTR_SIZE)

ZZ methodHandleJ2I_unwrapper Stack Layout for jitCallCFunction

SETVAL(SaveR14_MHJ2IParm_onStack,6*PTR_SIZE)
SETVAL(SaveR3_MHJ2IParm_onStack,5*PTR_SIZE)
SETVAL(SaveR2_MHJ2IParm_onStack,4*PTR_SIZE)
SETVAL(SaveR1_MHJ2IParm_onStack,3*PTR_SIZE)
SETVAL(VMThread_MHJ2IParm_onStack,2*PTR_SIZE)
SETVAL(StackPointer_MHJ2IParm_onStack,1*PTR_SIZE)
SETVAL(MethodHandle_MHJ2IParm_onStack,0*PTR_SIZE)
SETVAL(ParmsArray_MHJ2IParm_onStack,0*PTR_SIZE)
SETVAL(ReturnedValue_MHJ2IParm_onStack,0*PTR_SIZE)

ZZ ========================================================  ZZ
ZZ
ZZ _samplingRecompileMethod
ZZ Called from sampled method when it needs to be recompiled.
ZZ
ZZ r14 is pointing at the bodyInfo address
ZZ It's assumed the layout is
ZZ   DC bodyInfo  4 or 8 bytes
ZZ   DC magic <hdr> 4 bytes
ZZ     < possible load of args>
ZZ   ST r14,x(r5)  <- jit entry point, will have been rewritten
ZZ to be J <sampling code>
ZZ Note that it's assumed the preprologue has saved r14 at 4(r5)
ZZ ( that's 8(r5) on 64 bit systems)
ZZ before entering this routine.
ZZ ========================================================  ZZ
ZZ
ZZ Frame Layout
ZZ +----------+
ZZ | .......  |  <- prev entrySP
ZZ +----------+
ZZ | reserved |  <- holds return address to caller
ZZ +----------+
ZZ |  rEP     |  <- new entrySP (used for patching interface calls)
ZZ +----------+
ZZ |  empty   |    --+-> these 2 slots are to match stack size with
ZZ +----------+      |   counting version so async comp can treat
ZZ |  empty   |    --+   routines as the same
ZZ +----------+
ZZ |          |
ZZ |    64    |
ZZ |   Bytes  |
ZZ | FP Saves |
ZZ |          |
ZZ +----------+
ZZ | r14      | slot 5
ZZ +----------+
ZZ | r0       | slot 4       << has return address with new frame shape
ZZ +----------+
ZZ | rSSP     | slot 3
ZZ +----------+
ZZ | r1       | slot 2
ZZ +----------+
ZZ | r2       | slot 1
ZZ +----------+
ZZ | r3       | slot 0       <- SP at call out
ZZ +----------+
ZZ |  .....   |
ZZ
SETVAL(SampleGPRSaveAr,6*PTR_SIZE)

SETPPA2()

    START_FUNC(_samplingRecompileMethod,SMPRCPM)

LABEL(LsamplingRecompileMethod)
    AHI_GPR J9SP,-SamplingFrameSize

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl

    STD f0,SamplingFPSlot0(J9SP)
    STD f1,SamplingFPSlot1(J9SP)
    STD f2,SamplingFPSlot2(J9SP)
    STD f3,SamplingFPSlot3(J9SP)
    STD f4,SamplingFPSlot4(J9SP)
    STD f5,SamplingFPSlot5(J9SP)
    STD f6,SamplingFPSlot6(J9SP)
    STD f7,SamplingFPSlot7(J9SP)
])dnl
ZZ Save gprs
    ST_GPR r14,5*PTR_SIZE(J9SP)     ZZ store r14 in slot
    ST_GPR r0,4*PTR_SIZE(J9SP)      ZZ store r0 in slot 4
    ST_GPR rSSP,3*PTR_SIZE(J9SP)    ZZ store rSSP in slot 3
    ST_GPR r1,2*PTR_SIZE(J9SP)      ZZ save args in resolution order
    ST_GPR r2,1*PTR_SIZE(J9SP)
    ST_GPR r3,0*PTR_SIZE(J9SP)

ZZ call jitRetranslateMethod(J9Method* method,oldStartPC,senderPC)
    L_GPR        r1,0(r14)  ZZ fetch bodyinfo
    L_GPR        r1,eq_BodyInfo_MethodInfo(r1)  ZZ fetch methodinfo
    AHI_GPR      r14,4+ALen ZZ point past magic 32-bit word to startPC
    LR_GPR       r2,r14
    L_GPR        r1,eq_VMMethodInfo_j9Method(r1)  ZZ load J9Method *
ZZ                            fetch senderPC as third arg
    LR_GPR        r3,r0

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitRetranslateMethod)
    BASR    r14,rEP
    LR_GPR rEP,r2
ZZ if compile returned null, then compilation has not yet finished
ZZ restart this method
    LTR     rEP,rEP
    JNZ     LsamplingGotStartAddress
ZZ load old address
    L_GPR        rEP,5*PTR_SIZE(J9SP)  ZZ load saved link reg
    AHI_GPR      rEP,4+PTR_SIZE        ZZ Get OldStartPC
LABEL(LsamplingGotStartAddress)

ZZ rEP now points to the startPC.  This is the entry point for the
ZZ interpreter, and if there are any arguments, will be pointing to
ZZ the load eg on 32-bit with 1 arg:
ZZ  DC 0x00040000 <- magic word
ZZ  L_GPR  R1,8(R5)    <-- startPC--rEP is pointer here
ZZ  ST_GPRr14,4(r5)  <- need to point here
ZZ The magic word's upper half word contains the # bytes
ZZ from startPC to the jit method--add it to R4
   LR_GPR r1,rEP
   AHI_GPR r1,-4  ZZ point at magic word
ifdef([TR_HOST_64BIT],[dnl
   LH_GPR r1,0(r1)
   AR_GPR rEP,r1
],[dnl
   AH     rEP,0(r1)
])dnl
ZZ Restore Regs
   L_GPR   r3,0*PTR_SIZE(J9SP)
   L_GPR   r2,1*PTR_SIZE(J9SP)
   L_GPR   r1,2*PTR_SIZE(J9SP)
   L_GPR   r0,4*PTR_SIZE(J9SP)
   L_GPR   rSSP,(3*PTR_SIZE)(J9SP)

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
ZZ restore FP regs if required
    LD f0,SamplingFPSlot0(J9SP)
    LD f1,SamplingFPSlot1(J9SP)
    LD f2,SamplingFPSlot2(J9SP)
    LD f3,SamplingFPSlot3(J9SP)
    LD f4,SamplingFPSlot4(J9SP)
    LD f5,SamplingFPSlot5(J9SP)
    LD f6,SamplingFPSlot6(J9SP)
    LD f7,SamplingFPSlot7(J9SP)
])dnl

ZZ                  make sure to pop off 2 slots added in preprologue
    AHI_GPR J9SP,(SamplingFrameSize+(2*PTR_SIZE))

ZZ restore the old return register (r14),presumed to be stored
ZZ by preprologue
ZZ load old address
    LR_GPR        r14,r0
    BCR     HEX(f),rEP  ZZ branch directly back, never to return
    END_FUNC(_samplingRecompileMethod,SMPRCPM,9)




ZZ ========================================================  ZZ
ZZ On entry, r1 will contain the address of bodyInfo
ZZ r2 the address of this routine and 0(sp) in memory
ZZ the resume point if recompilation fails.
ZZ rEP contains the startPC
ZZ Note that we
ZZ must restore r1 and r2 if recompilation succeed--
ZZ the original values are stored as:
ZZ r1: 1*regSize(sp)
ZZ r2: 2*regSize(sp)
ZZ However, to support asynchronous compilation, the
ZZ frame must have the values stored on the stack, highest
ZZ arg in highest memory
ZZ Note that with new frame shape, r0 wil contain return
ZZ address
ZZ ========================================================  ZZ
ZZ
ZZ Frame Layout
ZZ
ZZ |  ...     |                         <- prev SP
ZZ +----------+
ZZ | reserved |
ZZ +----------+
ZZ | r2       |
ZZ +----------+
ZZ | r1       |
ZZ +----------+
ZZ |  resume  |                  <- entrySP
ZZ +----------+
ZZ |          |
ZZ |    64    |
ZZ |   Bytes  |
ZZ | FP Saves |
ZZ |          |
ZZ +----------+
ZZ | r14      | slot 5  <<< has return address(same as r0 slot)
ZZ +----------+
ZZ | r0       | slot 4  <<< has return address with new frame shape
ZZ +----------+
ZZ | rSSP     | slot 3
ZZ +----------+
ZZ | r1       | slot 2
ZZ +----------+
ZZ | r2       | slot 1
ZZ +----------+
ZZ | r3       | slot 0         <- SP at call out
ZZ +----------+
ZZ |  .....   |
ZZ
ZZ .align 8
    START_FUNC(_countingRecompileMethod,CNTRCMPM)
LABEL(LcountingRecompileMethod)
    AHI_GPR J9SP,-CountingStackSize
ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl

    STD f0,CountingFPSlot0(J9SP)
    STD f1,CountingFPSlot1(J9SP)
    STD f2,CountingFPSlot2(J9SP)
    STD f3,CountingFPSlot3(J9SP)
    STD f4,CountingFPSlot4(J9SP)
    STD f5,CountingFPSlot5(J9SP)
    STD f6,CountingFPSlot6(J9SP)
    STD f7,CountingFPSlot7(J9SP)
])dnl
    ST_GPR r0,5*PTR_SIZE(J9SP)  ZZ store return address in slot 5
    ST_GPR r0,4*PTR_SIZE(J9SP)  ZZ store r0 in slot 4
    ST_GPR rSSP,3*PTR_SIZE(J9SP)  ZZ store rSSP


    LR_GPR  r14,r1               ZZ save bodyInfo ptr in r14

ZZ restore r1 and r2 from stack
    LM_GPR r1,r2,(CountingStackSize+PTR_SIZE)(J9SP)
    ST_GPR r1,2*PTR_SIZE(J9SP)
    ST_GPR r2,1*PTR_SIZE(J9SP)
    ST_GPR r3,0*PTR_SIZE(J9SP)

ZZ call jitRetranslateMethod(J9Method* method,oldStartPC,senderPC)
    L_GPR        r1,eq_BodyInfo_MethodInfo(r14)
    L_GPR        r1,eq_VMMethodInfo_j9Method(r1)  ZZ load J9Method *
    LR_GPR       r2,rEP ZZ oldStartPC
    LR_GPR       r3,r0  ZZ caller
LOAD_ADDR_FROM_TOC(rEP,TR_S390jitRetranslateMethod)
    BASR    r14,rEP
    LR_GPR rEP,r2
ZZ if compile returned null, then compilation has not yet finished
ZZ restart this method
    LTR     rEP,rEP
    JNZ     LcountingGotStartAddress
ZZ load resume point
    L_GPR  rEP,CountingStackSize(J9SP)
    J LcountingRestoreRegs
LABEL(LcountingGotStartAddress)

ZZ rEP now points to the startPC.  This is the entry point for the
ZZ interpreter, and if there are any arguments, will be pointing to
ZZ the load eg on 32-bit with 1 arg:
ZZ  DC 0x00040000 <- magic word
ZZ  L R1,8(R5)    <-- startPC--rEP is pointer here
ZZ  ST_GPR r14,4(r5)  <- need to point here
ZZ The magic word's upper half word contains the # bytes
ZZ from startPC to the jit method--add it to R4
   LR_GPR r1,rEP
   AHI_GPR r1,-4  ZZ point at magic 32-bit word
ifdef([TR_HOST_64BIT],[dnl
   LH_GPR r1,0(r1)
   AR_GPR rEP,r1
],[dnl
   AH     rEP,0(r1)
])dnl
LABEL(LcountingRestoreRegs)
ZZ Restore Regs
   L_GPR   r3,0*PTR_SIZE(J9SP)
   L_GPR   r2,1*PTR_SIZE(J9SP)
   L_GPR   r1,2*PTR_SIZE(J9SP)
   L_GPR   r0,4*PTR_SIZE(J9SP)
   L_GPR   r14,5*PTR_SIZE(J9SP)
   L_GPR   rSSP,3*PTR_SIZE(J9SP)

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
ZZ restore FP regs if required
    LD f0,CountingFPSlot0(J9SP)
    LD f1,CountingFPSlot1(J9SP)
    LD f2,CountingFPSlot2(J9SP)
    LD f3,CountingFPSlot3(J9SP)
    LD f4,CountingFPSlot4(J9SP)
    LD f5,CountingFPSlot5(J9SP)
    LD f6,CountingFPSlot6(J9SP)
    LD f7,CountingFPSlot7(J9SP)
])dnl

ZZ restore stack & pop off 4 elements.
    AHI_GPR J9SP,(CountingStackSize+(4*PTR_SIZE))

    BCR     HEX(f),rEP  ZZ branch directly back, never to return
    END_FUNC(_countingRecompileMethod,CNTRCMPM,10)


ZZ ========================================================  ZZ
ZZ right now just redirect call to new method.  Later, patch
ZZ caller as well
ZZ Assumptions: rEP used to branch here
ZZ and that previous rEP stored on the stack (needs to be popped off)
ZZ r14 contains bodyInfo address.  Original r14 found
ZZ in r0
ZZ Frame Layout
ZZ
ZZ |  ...     |                         <- prev SP
ZZ +----------+
ZZ | reserved |
ZZ +----------+
ZZ | rEP      |             <- SP  (used for interface cache)
ZZ +----------+
ZZ |   ...    |
ZZ ========================================================  ZZ
ZZ .align 8
    START_FUNC(_countingPatchCallSite,CNTPCS)

ZZ load up new address from method info via VM method
    L_GPR        rEP,eq_BodyInfo_MethodInfo(r14)
    L_GPR        rEP,eq_VMMethodInfo_j9Method(rEP)
    L_GPR        rEP,J9TR_MethodPCStartOffset(rEP)

ZZ Check whether the lower bit of the method address
ZZ is odd.  In such a case, the method has not been compiled
ZZ (i.e. class may been replaced by HCR).  In such a scenario
ZZ we need to force a recompile.
    TML     rEP,HEX(0001)             # Check low order bit
    JNZ     LrevertToRecompile        # Recompile Method


ZZ remove to skip patching
    J LCheckAndPatchBrasl

ZZ pop off rEP
    AHI_GPR J9SP,2*PTR_SIZE

ZZ now restore return address
ZZ See _samplingRecompileMethod for why we have to do the following:
ZZ Use r14 as temp reg
    LR_GPR       r14,rEP
    AHI_GPR r14,-4 ZZ point to magic word
ifdef([TR_HOST_64BIT],[dnl
    LH_GPR r14,0(r14)
    AR_GPR rEP,r14
],[dnl
    AH     rEP,0(r14) ZZ point to beginning of JITted method
])dnl
ZZ restore saved r14--have been saved before jumping here
    LR_GPR      r14,r0
    BCR     HEX(f),rEP

ZZ Reverting to _countingRecompileMethod
LABEL(LrevertToRecompile)
ZZ Fix up registers and stack to make call to _countingRecompMeth
ZZ   - 2 more slots of stack
ZZ   - GPR1/2 saved in slot 2/3 of stack
ZZ   - GPREP StartPC
ZZ   - GPR1 BodyInfo
ZZ   - GPR0 Return Address

ZZ Buy two more stack slots
    AHI_GPR J9SP,-2*PTR_SIZE
ZZ Save GPR1/GPR2 onto Stack
    STM_GPR r1,r2,PTR_SIZE(J9SP)

ZZ Save JIT Entry Point to slot 0 - We don't want to return to
ZZ body, as the body might be invalidated by HCR.  We need to
ZZ calculate JIT EP from StartPC. The magic word's upper half
ZZ word contains the # bytes from startPC to the jit method.
    L_GPR   rEP,3*PTR_SIZE(J9SP)  ZZ Load StartPC
    LR_GPR  r1,rEP
    AHI_GPR r1,-4        ZZ point at magic 32-bit word
ifdef([TR_HOST_64BIT],[dnl
    LH_GPR  r1,0(r1)
    AR_GPR  rEP,r1
],[dnl
    AH      rEP,0(r1)
])dnl
ZZ Save the JIT EP onto Stack.
    ST_GPR  rEP,0(J9SP)

ZZ Load StartPC from Stack into rEP
    L_GPR   rEP,3*PTR_SIZE(J9SP)

ZZ Copy BodyInfo into R1
    LR_GPR  r1,r14

ZZ Jump to Recompile
    J LcountingRecompileMethod

    END_FUNC(_countingPatchCallSite,CNTPCS,8)


ZZ ========================================================  ZZ
ZZ Patch caller with correct address, and jump to the new
ZZ method.  For now, no patching is done--just the redirection.
ZZ note that previous rEP stored on the stack (needs to be popped off)
ZZ r14 points to the bodyInfo address
ZZ Frame Layout
ZZ
ZZ |  ...     |                         <- prev SP
ZZ +----------+
ZZ | reserved |
ZZ +----------+
ZZ | rEP      |             <- SP  (used for interface cache)
ZZ +----------+
ZZ |   ...    |
ZZ ========================================================  ZZ
ZZ .align 8
    START_FUNC(_samplingPatchCallSite,SMPPCS)
ZZ load up new address.  Get method info
    L_GPR       rEP,0(r14)
ZZ now get the VMMethod info from method info
    L_GPR       rEP,eq_BodyInfo_MethodInfo(rEP)
    L_GPR       rEP,eq_VMMethodInfo_j9Method(rEP)
ZZ and finally the new address
    L_GPR       rEP,J9TR_MethodPCStartOffset(rEP)

ZZ Check whether the lower bit of the method address
ZZ is odd.  In such a case, the method has not been compiled
ZZ (i.e. class may been replaced by HCR).  In such a scenario
ZZ we need to force a recompile.
    TML     rEP,HEX(0001)             # Check low order bit
    JNZ     LsamplingRecompileMethod  # Recompile Method

ZZ load up new address.  Get method info
    L_GPR       r14,0(r14)

ZZ remove to skip patching
    J LCheckAndPatchBrasl

ZZ pop off rEP and reserved slot
    AHI_GPR J9SP,2*PTR_SIZE

ZZ See _samplingRecompileMethod for why we have to do the following:
ZZ Use r14 as temp reg
    LR_GPR      r14,rEP
    AHI_GPR     r14,-4 ZZ point to magic word
ifdef([TR_HOST_64BIT],[dnl
    LH_GPR r14,0(r14)
    AR_GPR rEP,r14
],[dnl
    AH     rEP,0(r14) ZZ point to beginning of JITted method
])dnl

ZZ restore saved r14--have been saved before jumping here
    LR_GPR      r14,r0

    BCR     HEX(f),rEP
    END_FUNC(_samplingPatchCallSite,SMPPCS,8)

ZZ clear high order bit in 31 bit version
ifdef([TR_HOST_64BIT],[dnl
define(CLEANSE,
[dnl

ZZ
])dnl
],[dnl
define(CLEANSE,
[dnl

ZZ clear high order bit
   sll $1,1
   srl $1,1
])dnl
])dnl


ZZ ========================================================  ZZ
ZZ Check whether caller is a BRASL. If so, then patch the callers
ZZ address with the new address. It is assumed that the address of
ZZ relative immediate field in the BRASL does not cross an 8 byte 
ZZ boundary. This fact should be enforced by the JIT.
ZZ 
ZZ This routine will never be called directly. Rather we jump in
ZZ here from the two patch routines.  It is assumed that all
ZZ registers have their appropriate values, except rEP points to
ZZ the entry point of the function, r14 is available as a temp,
ZZ and PTR_SIZE(stackPointer) contains the return address if
ZZ previous instruction is not a BRASL (eg L/BASR pair) then
ZZ simply transfer control. The JVM will have remapped vtable
ZZ slots we spill r0 to r3 to help in the checking. Note that
ZZ rEP is currently on the stack and needs to be popped.
ZZ
ZZ |  ...     |                         <- prev SP
ZZ +----------+
ZZ | reserved |
ZZ +----------+
ZZ | rEP      |                         <- cur SP
ZZ +----------+
ZZ | r0       |     <---+
ZZ +----------+         +
ZZ | r1       |     <---+
ZZ +----------+         +--- will save these
ZZ | r2       |     <---+
ZZ +----------+         +
ZZ | r3       |     <---+
ZZ +----------+
ZZ |  ...     |
ZZ
ZZ --------------------------------------------------------  ZZ
ZZ With Multi-Code Cache Support, slots in the stack are
ZZ reserved for parameter passing in calling the CallPointPatching
ZZ C-Function.  The Stack Frame will look like:
ZZ |  ...     |                         <- prev SP
ZZ +----------+
ZZ | reserved |
ZZ +----------+
ZZ | rEP      |                         <- cur SP
ZZ +----------+     <-------- MCC: Parameters
ZZ | extraArg |       <-- Param4
ZZ +----------+
ZZ | newPC    |       <-- Param3
ZZ +----------+
ZZ | callSite |       <-- Param2
ZZ +----------+
ZZ | MethPtr  |       <-- Param1
ZZ +----------+     <-------- MCC: Return Addy
ZZ | retAddr  |       <-- Return Address of function
ZZ +----------+
ZZ | save r1  |       <-- Save R1 (Clobbered by MCC Call)
ZZ +----------+
ZZ | save rEP |       <-- Save REP (Clobbered by MCC Call)
ZZ +----------+
ZZ | r2       |     <---+
ZZ +----------+         +--- will save these
ZZ | r3       |     <---+
ZZ +----------+
ZZ ========================================================  ZZ
ZZ .align 8
    START_FUNC(_checkAndPatchBrasl,PCHBRSL)
LABEL(LCheckAndPatchBrasl)

ifdef([MCC_SUPPORTED],[dnl
ZZ R14 is currently pointing to BodyInfo.  Get J9Method.
    L_GPR       r14,eq_BodyInfo_MethodInfo(r14)
    L_GPR       r14,eq_VMMethodInfo_j9Method(r14)
ZZ Save the Method Ptr and Entry Point for MCC Call Point Patching
ZZ before they get clobbered.
ZZ Param 2 is stored later on when it is available.
ZZ 7 slots for MCC Call Parameters.
    AHI_GPR   J9SP,-allocStackSize
ZZ Param1: Method Ptr
    ST_GPR    r14,MethPtrPatchCallParam_onStack(J9SP)
ZZ Param3: new PC
    ST_GPR    rEP,NewPCPatchCallParam_onStack(J9SP)
ZZ Param4: extraArgs
    ST_GPR    r0,ExtraArgPatchCallParam_onStack(J9SP)

ZZ See _samplingRecompileMethod for why we have to do the following:
ZZ r14 is temp reg
    LR_GPR    r14,rEP
    AHI_GPR   r14,-4 ZZ point to magic word
],[dnl
ZZ See _samplingRecompileMethod for why we have to do the following:
ZZ r14 is temp reg
    LR_GPR    r14,rEP
    AHI_GPR   r14,-4 ZZ point to magic word

    AHI_GPR   J9SP,-allocStackSize
])dnl

ifdef([TR_HOST_64BIT],[dnl
    LH_GPR r14,0(r14)
    AR_GPR rEP,r14
],[dnl
    AH      rEP,0(r14) ZZ point to beginning of JITted method
])dnl
ifdef([MCC_SUPPORTED],[dnl
    STM_GPR   r2,r3,0(J9SP)
],[dnl
    STMG      r0,r3,0(J9SP)
])dnl
    LR_GPR      r14,r0

    LR_GPR    r2,r14
    SR_GPR    r3,r3    ZZ zero out

ZZ  check if interface call  BCR 0xF,rEP ; DC PTR_TO_DATA; LR r0,r0
    AHI_GPR   r2,-(2+PTR_SIZE+2)    ZZ point to beginning of BCR
    IC        r3,0(r2)
    CHI_GPR   r3,HEX(07) ZZ compare with BCR
    JNZ       LcheckVirtual
    ICM       r3,3,2+PTR_SIZE(r2)
    CHI_GPR   r3,HEX(1800) ZZ compare if LR r0,r0
    JNZ       LcheckVirtual
ZZ check in snippet to see if the 'firstSlot' stores the address
ZZ of the actual first slot, which is at a fixed offset from
ZZ itself. Currently this offset is 2 * PTR_SIZE.
ZZ The address of the Snippet is stored @ 2*PTR_SIZE off
ZZ the stack.
    L_GPR     r2,2(r2)
    C_GPR     r2,InterfaceSnippetEP_onStack(J9SP)
    JNZ       LJumpToNewRoutine
ZZ Skip patching if not coming from mainline code
ZZ where the rEP contains dataSnippetBegin constant

    LA       r3,eq_firstSlot_inInterfaceSnippet(r2)
    C_GPR    r3,eq_firstSlotField_inInterfaceSnippet(r2)
    JZ       LUpdateInterfaceCallCache


ZZ check if virtual call BASR r14,r14
LABEL(LcheckVirtual)
    LR_GPR    r2,r14
    AHI_GPR   r2,-2    ZZ point to beginning of BASR
    SR_GPR    r3,r3    ZZ zero out
    ICM       r3,3,0(r2)
    CHI_GPR   r3,HEX(0DEE) ZZ compare if BASR r14,r14
    JZ        LJumpToNewRoutine

ZZ check for direct call BRASL
    LR_GPR    r2,r14
    AHI_GPR   r2,-6    ZZ point to beginning of BASR
    SR_GPR    r3,r3    ZZ zero out

    IC        r3,0(r2) ZZ load leading 8 bits
    CHI_GPR   r3,HEX(c0) ZZ compare with brasl's 0xC0
    JNZ       LJumpToNewRoutine

    IC        r3,1(r2) ZZ left most 3 bytes already zeroed
    LHI_GPR   r14,15
    nr        r3,r14

    LR_GPR    r14,r0

    CHI_GPR   r3,5 ZZ compare with brasl's 0xC0?5
    JNZ       LJumpToNewRoutine

ZZ Save the call site for MCC Call.
ifdef([MCC_SUPPORTED],[dnl
ZZ Param2: CallSite.
    ST_GPR    r2,CallSitePatchCallParam_onStack(J9SP)
])dnl

ZZ Check if BRASL relative immediate field crosses an 8 byte boundary.
ZZ This only happens if the least significant 3 bits of the BRASL
ZZ address are 100 (in binary).
ifdef([DEBUG],[dnl
    TML       r2,HEX(0004)     ZZ check if bit 61 is set
    JZ        LSkipAlignCheck  ZZ if not we are OK
    TML       r2,HEX(0003)     ZZ check if bits 62 and 63 are set
    JNZ       LSkipAlignCheck  ZZ crosses an 8 byte boundary
    LHI_GPR   r2,0
    ST_GPR    r2,0(r2)         ZZ force a crash so we can debug
LABEL(LSkipAlignCheck)
])dnl

ZZ patch the BRASL's four byte field
ZZ rEP contains the new address, r2 address of brasl
ZZ 2 bytes in is a 4 byte field containing displacement
ZZ in half words.  Assume that it's in range!
ZZ calculate r3 = (rEP-r3)/2
    LR_GPR    r3,r2
    CLEANSE(r3)
    CLEANSE(rEP)
ifdef([MCC_SUPPORTED],[dnl
ZZ Call MCC service for code patching of the resolved method.
ZZ Parameters:
ZZ       j9method, callSite, newPC, extraArg
ZZ  We have to load the arguments onto the stack to pass to
ZZ  jitCallCFunction:
ZZ         SP +16:   Save Entry Point (not used by JitCallC)
ZZ         SP +24:   Return Address.
ZZ         SP +32:   J9Method
ZZ         SP +40:   callSite
ZZ         SP +48:   newPC
ZZ         SP +56:   extraArg (not used in zLinux MCC Patching)
ZZ JitCallCFunction calling convention is:
ZZ r1: C Function Ptr
ZZ r2: Ptr to parameters
ZZ r3: Ptr to Return Address

ZZ Set pointer to arguments
    LA  r2,MethPtrPatchCallParam_onStack(,J9SP)
ZZ Set pointer to return address
    LA  r3,RetAddrPatchCallParam_onStack(,J9SP)
ZZ Save the Entry Point
    ST_GPR rEP,SaveREPPatchCallParam_onStack(J9SP)
ZZ Save r1, since we clobber it.
    ST_GPR r1,SaveR1PatchCallParam_onStack(J9SP)
ZZ Save Return Address
    ST_GPR r14,RetAddrPatchCallParam_onStack(J9SP)
LOAD_ADDR_FROM_TOC(r1,TR_S390mcc_callPointPatching_unwrapper)
LOAD_ADDR_FROM_TOC(r14,TR_S390jitCallCFunction)
    BASR r14,r14                  ZZ Call to jitCallCFunction
ZZ Restore Entry Point
    L_GPR rEP,SaveREPPatchCallParam_onStack(,J9SP)
ZZ Restore r1
    L_GPR r1,SaveR1PatchCallParam_onStack(,J9SP)
ZZ Restore Return Address
    L_GPR r14,RetAddrPatchCallParam_onStack(,J9SP)
],[dnl
    SR_GPR    r3,rEP  ZZ because it's destructive,can't say r3=rEP-r3
    LCR_GPR   r3,r3 ZZ negate it
ifdef([TR_HOST_64BIT],[dnl
    srag     r3,r3,1
],[dnl
    sra     r3,1
])dnl
    TML       r2,HEX(0007)              ZZ check if 8 byte aligned
    JZ        LCompareAndSwapLoopHeader  ZZ patch 8 byte atomically
    ST        r3,2(r2)   ZZ store 4 bytes 2 bytes off of BRASL
    J         LCompareAndSwapLoopEnd
LABEL(LCompareAndSwapLoopHeader)
    LG        r0,0(r2)      ZZ load 8 bytes including the BRASL
    SLAG      r3,r3,16      ZZ shift new address so it aligns up
    NIHH      r3,HEX(0000)
LABEL(LCompareAndSwapLoop)
    LGR       r1,r0         ZZ save original memory
    NIHL      r1,HEX(0000)  ZZ clear bits 16-48
    NILH      r1,HEX(0000)
    OGR       r1,r3         ZZ or in the new address
    CSG       r0,r1,0(r2)
    BRC       HEX(4),LCompareAndSwapLoop
LABEL(LCompareAndSwapLoopEnd)
])dnl


LABEL(LJumpToNewRoutine)
ZZ Non-MCC has to restore 4 slots on the stack, while
ZZ MCC has to restore 9 slots.
ifdef([MCC_SUPPORTED],[dnl
    LM_GPR    r2,r3,0(J9SP)
],[dnl
    LMG       r0,r3,0(J9SP)
])dnl
ZZ pop MCC params, 2 regs, rEP & reserved slot
    AHI_GPR   J9SP,allocStackSize+2*PTR_SIZE

    BCR       HEX(f),rEP


LABEL(LUpdateInterfaceCallCache)
ZZ We want to recurse through the valid cache slots and look for
ZZ a slot that has the same class pointer.  If we find one such
ZZ slot, we can update the old methodEP to the new methodEP.
ZZ Since we never have two threads updating the same slot, if we
ZZ find a match, then we know we can update this slot safely.
ZZ Note:  The class pointer we use to compare is derived from
ZZ the 'this' parameter passed in from r1 (first param register).
ZZ r1 is never clobbered from callSite->oldMethodEP->Recomp-
ZZ Prologue->_samplingPatchCallSite.
ZZ At this point:
ZZ            r1  - 'this' param
ZZ            r2  - Snippet data
ZZ            rEP - new method Entry Point

ZZ Get the lastCacheSlot pointer into r3.
    L_GPR     r3,eq_lastCachedSlotField_inInterfaceSnippet(r2)
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
ZZ Load the class offset (32 bits)
    L         r0,J9TR_J9Object_class(,r1)
    LLGFR     r0,r0
],[dnl
ZZ Load the class ptr from 'this' param
    L_GPR     r0,J9TR_J9Object_class(r1)
])dnl

ZZ Mask out low byte flags
    NILL      r0,HEX(10000)-J9TR_RequiredClassAlignment

LABEL(LSearchInterfaceSlotToPatch)

ZZ if lastCacheSlot < firstSlot, we either have walked all the
ZZ slots or slots are uninitialized.
    CL_GPR    r3,eq_firstSlotField_inInterfaceSnippet(r2)
    JL        LJumpToNewRoutine

ZZ Compare our class pointer with the class pointer in current slot
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    CL        r0,0(,r3)
],[dnl
    CL_GPR    r0,0(,r3)
])dnl
    JZ        LPatchInterfaceSlot  ZZ If class ptrs match, we patch!
    AHI_GPR   r3,-2*PTR_SIZE    ZZ Otherwise, we move to previous slot
    J LSearchInterfaceSlotToPatch

ZZ If we find a slot that matches the same class pointer,
ZZ we would patch the method entry point
LABEL(LPatchInterfaceSlot)
    ST_GPR    rEP,PTR_SIZE(r3)
    J         LJumpToNewRoutine

    END_FUNC(_checkAndPatchBrasl,PCHBRSL,9)

ZZ Induce Recompilation asm routine is just a wrapper to call the C
ZZ function induceRecompilation(VMThread,StartPC), which will patch
ZZ the startPC in a thread-safe manner.
ZZ This asm routine sets up the appropriate parameters and uses the
ZZ jitCallCFunction to invoke the induceRecompilation C routine.
ZZ
ZZ _induceRecompilation is called by a ForceRecompilationSnippet
ZZ which will provide:
ZZ      r14 - point to memory location storing:
ZZ      r14 + 0       - Return Address of Mainline Code.
ZZ      r14 + PTRSize - Start PC.  (Param 2)
ZZ
ZZ Private Linkage conventions dictate:
ZZ      r13 to be VMThread Pointer.(Param 1)
ZZ
ZZ jitCallCFunction reserves 7 slots to make call.
ZZ +----------+ <---------  Old SP
ZZ | VMTread  | slot 5  - Param 2
ZZ +----------+
ZZ | StartPC  | slot 4  - Param 1
ZZ +----------+
ZZ | retAddr  | slot 3
ZZ +----------+
ZZ | r3       | slot 2
ZZ +----------+
ZZ | r2       | slot 1
ZZ +----------+
ZZ | r1       | slot 0
ZZ +----------+ <---------  SP at call out.
ZZ
    START_FUNC(_induceRecompilation,INDRECOMP)

    AHI_GPR  J9SP,-6*PTR_SIZE           ZZ Reserve Space on Java Stack.

ZZ Save the preserved registers.
    ST_GPR    r1,SaveR1IndRecompCallParam_onStack(J9SP)
    ST_GPR    r2,SaveR2IndRecompCallParam_onStack(J9SP)
    ST_GPR    r3,SaveR3IndRecompCallParam_onStack(J9SP)

    L_GPR    r1,StartPCInForceRecompSnippet(r14)
    L_GPR    r14,RetAddrInForceRecompSnippet(r14)
ZZ Save Parameter 1 onto Stack - VM Thread Pointer.
    ST_GPR   r13,VMThreadIndRecompCallParam_onStack(J9SP)
ZZ Save Parameter 2 onto Stack - StartPC
    ST_GPR   r1,StartPCIndRecompCallParam_onStack(J9SP)
ZZ Save Return Address onto Stack
    ST_GPR   r14,RetAddrIndRecompCallParam_onStack(J9SP)

ZZ JitCallCFunction calling convention is:
ZZ r1: C Function Ptr
ZZ r2: Ptr to parameters
ZZ r3: Ptr to Return Address
    LA       r2,StartPCIndRecompCallParam_onStack(J9SP)
    LA       r3,RetAddrIndRecompCallParam_onStack(J9SP)
LOAD_ADDR_FROM_TOC(r1,TR_S390induceRecompilation_unwrapper)
LOAD_ADDR_FROM_TOC(r14,TR_S390jitCallCFunction)
    BASR     r14,r14                ZZ Call to jitCallCFunction

ZZ Restore our preserved registers.
    L_GPR    r1,SaveR1IndRecompCallParam_onStack(J9SP)
    L_GPR    r2,SaveR2IndRecompCallParam_onStack(J9SP)
    L_GPR    r3,SaveR3IndRecompCallParam_onStack(J9SP)
    L_GPR    r14,RetAddrIndRecompCallParam_onStack(J9SP)

    AHI_GPR  J9SP,6*PTR_SIZE        ZZ Store Java Stack.
    BCR      HEX(f),r14             ZZ Return to main code.

    END_FUNC(_induceRecompilation,INDRECOMP,11)

ZZ JSR292: initialInvokeExactThunk is the default thunk used
ZZ for MethodHandles that don't yet have a thunk.  This glue
ZZ routine has JIT private linkage, and uses jitCallCFunction
ZZ to call the initialInvokeExactThunk's unwrapper routine
ZZ which then calls initialInvokeExactThunk.
ZZ
    START_FUNC(_initialInvokeExactThunkGlue,IIETG)

    AHI_GPR  J9SP,-6*PTR_SIZE  ZZ Reserve Space on Java Stack.

ZZ Save the preserved registers.
    ST_GPR    r14,SaveR14_IIEThunkParm_onStack(J9SP)
    ST_GPR    r3,SaveR3_IIEThunkParm_onStack(J9SP)
    ST_GPR    r2,SaveR2_IIEThunkParm_onStack(J9SP)
    ST_GPR    r1,SaveR1_IIEThunkParm_onStack(J9SP)

ZZ Parameter 1 - receiver MethodHandle
    ST_GPR   r1,MethodHandle_IIEThunkParm_onStack(J9SP)
ZZ Parameter 2 - VMThread pointer
    ST_GPR   r13,VMThread_IIEThunkParm_onStack(J9SP)

ZZ JitCallCFunction calling convention is:
ZZ r1: C Function Ptr
ZZ r2: Ptr to parameters
ZZ r3: Ptr to Return Address
    LA       r2,ParmsArray_IIEThunkParm_onStack(J9SP)
    LA       r3,ReturnedStartPC_IIEThunkParm_onStack(J9SP)
LOAD_ADDR_FROM_TOC(r1,TR_initialInvokeExactThunk_unwrapper)
LOAD_ADDR_FROM_TOC(r14,TR_S390jitCallCFunction)
    BASR     r14,r14                ZZ Call to jitCallCFunction
ZZ Restore our preserved registers.
    L_GPR    r1,SaveR1_IIEThunkParm_onStack(J9SP)
    L_GPR    r2,SaveR2_IIEThunkParm_onStack(J9SP)
    L_GPR    r3,SaveR3_IIEThunkParm_onStack(J9SP)
    L_GPR    r14,SaveR14_IIEThunkParm_onStack(J9SP)

    L_GPR    rEP,ReturnedStartPC_IIEThunkParm_onStack(J9SP)
    AHI_GPR  J9SP,6*PTR_SIZE        ZZ Restore Java Stack.
    BCR      HEX(f),rEP             ZZ Branch to returned startPC

    END_FUNC(_initialInvokeExactThunkGlue,IIETG,7)

ZZ JSR292: methodHandleJ2IGlue can be called before the VM's J2I
ZZ helper function to do some additional printing.  It implements
ZZ the -Xjit:verbose={j2iThunks} functionality.
ZZ
    START_FUNC(methodHandleJ2IGlue,MHJ2IG)

    AHI_GPR  J9SP,-7*PTR_SIZE  ZZ Reserve Space on Java Stack.

ZZ Save the preserved registers.
    ST_GPR    r14,SaveR14_MHJ2IParm_onStack(J9SP)
    ST_GPR    r3,SaveR3_MHJ2IParm_onStack(J9SP)
    ST_GPR    r2,SaveR2_MHJ2IParm_onStack(J9SP)
    ST_GPR    r1,SaveR1_MHJ2IParm_onStack(J9SP)

ZZ Parameter 1 - receiver MethodHandle
    ST_GPR   r1,MethodHandle_MHJ2IParm_onStack(J9SP)
ZZ Parameter 2 - stack pointer before the call to the J2I thunk
    LA       r1,7*PTR_SIZE(J9SP)
    ST_GPR   r1,StackPointer_MHJ2IParm_onStack(J9SP)
ZZ Parameter 3 - VMThread pointer
    ST_GPR   r13,VMThread_MHJ2IParm_onStack(J9SP)

ZZ JitCallCFunction calling convention is:
ZZ r1: C Function Ptr
ZZ r2: Ptr to parameters
ZZ r3: Ptr to Return Address
    LA       r2,ParmsArray_MHJ2IParm_onStack(J9SP)
    LA       r3,ReturnedValue_MHJ2IParm_onStack(J9SP)
LOAD_ADDR_FROM_TOC(r1,TR_methodHandleJ2I_unwrapper)
LOAD_ADDR_FROM_TOC(r14,TR_S390jitCallCFunction)
    BASR     r14,r14                ZZ Call to jitCallCFunction
ZZ Restore our preserved registers.
    L_GPR    r1,SaveR1_MHJ2IParm_onStack(J9SP)
    L_GPR    r2,SaveR2_MHJ2IParm_onStack(J9SP)
    L_GPR    r3,SaveR3_MHJ2IParm_onStack(J9SP)
    L_GPR    r14,SaveR14_MHJ2IParm_onStack(J9SP)

    AHI_GPR  J9SP,7*PTR_SIZE        ZZ Restore Java Stack.
    BCR      HEX(f),rEP             ZZ Branch to J2I helper

    END_FUNC(methodHandleJ2IGlue,MHJ2IG,8)

ifdef([J9ZOS390],[dnl
    END
])dnl

