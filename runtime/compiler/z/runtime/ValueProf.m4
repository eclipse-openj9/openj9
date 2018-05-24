ifdef(`J9ZOS390',`dnl
define(`ZZ',`**')
',`dnl
define(`ZZ',`##')
')dnl

ZZ Copyright (c) 2000, 2017 IBM Corp. and others
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

changequote([,])dnl

ifdef([J9ZOS390],[dnl

        TITLE 'ValueProf.s'

VALPROF#START      CSECT
VALPROF#START      RMODE ANY
ifdef([TR_HOST_64BIT],[dnl
VALPROF#START      AMODE 64
],[dnl
VALPROF#START      AMODE 31
])dnl
])dnl

define([VALPROF_M4],[1])

ZZ ===================================================================
ZZ codert/jilconsts.inc is a j9 assembler include that defines
ZZ offsets of various fields in structs used by jit
ZZ ===================================================================

include([jilconsts.inc])dnl


include([z/runtime/s390_macros.inc])dnl
include([z/runtime/s390_asdef.inc])dnl

ZZ ===================================================================
ZZ codert.dev/codertTOC.inc is a JIT assembler include that defines
ZZ index of various runtime helper functions addressable from
ZZ codertTOC.
ZZ ===================================================================

include([runtime/Helpers.inc])dnl

SETVAL(DblLen,8)
ifdef([J9_SOFT_FLOAT],[dnl
SETVAL(PVFPSize,0)
],[dnl
SETVAL(PVFPSize,8*DblLen)
])dnl

ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
SETVAL(PVGPRSize,27*PTR_SIZE)
],[dnl
SETVAL(PVGPRSize,11*PTR_SIZE)
])dnl

SETVAL(FPSlot0,0*DblLen)
SETVAL(FPSlot1,1*DblLen)
SETVAL(FPSlot2,2*DblLen)
SETVAL(FPSlot3,3*DblLen)
SETVAL(FPSlot4,4*DblLen)
SETVAL(FPSlot5,5*DblLen)
SETVAL(FPSlot6,6*DblLen)
SETVAL(FPSlot7,7*DblLen)
SETVAL(ProfileVFPSlot0,PVGPRSize+FPSlot0)
SETVAL(ProfileVFPSlot1,PVGPRSize+FPSlot1)
SETVAL(ProfileVFPSlot2,PVGPRSize+FPSlot2)
SETVAL(ProfileVFPSlot3,PVGPRSize+FPSlot3)
SETVAL(ProfileVFPSlot4,PVGPRSize+FPSlot4)
SETVAL(ProfileVFPSlot5,PVGPRSize+FPSlot5)
SETVAL(ProfileVFPSlot6,PVGPRSize+FPSlot6)
SETVAL(ProfileVFPSlot7,PVGPRSize+FPSlot7)
SETVAL(ProfileValueStackSize,PVFPSize+PVGPRSize)


ZZ * * * * * * * * * * * * * * *
ZZ macros for jitProfile* code
ZZ

ifdef([J9ZOS390],[dnl
define(XFORM_JITARGS_TO_CARGS_INT,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  L_GPR  r9,2200(rSSP) ZZ preserve value in r9
  ST_GPR r1,2200(rSSP) ZZ save as arg ZZ 4 on C stack
],[dnl
ZZ restore CAA to r12
  L_GPR r12,J9TR_CAA_save_offset(rSSP)
  L_GPR  r9,2124(rSSP) ZZ preserve value in r9
  ST_GPR r1,2124(rSSP) ZZ save as arg ZZ 4
])dnl
  LR_GPR r1,r3 ZZ save
  LR_GPR r3,r2 ZZ c arg 3
  LR_GPR r2,r1 ZZ c arg 2
  L  r1,ProfileValueStackSize+0*PTR_SIZE(r5)

ZZ load up the C Environment addr into r5
LOAD_ADDR_FROM_TOC(r5,TR_S390CEnvironmentAddress)

])dnl

],[dnl

define(XFORM_JITARGS_TO_CARGS_INT,
[dnl

  LR_GPR r4,r2
  L  r2,ProfileValueStackSize+0*PTR_SIZE(r5)
  LR_GPR r5,r1
ZZ  LR_GPR r3,r3 no need
])dnl

])dnl

ifdef([J9ZOS390],[dnl
define(XFORM_JITARGS_TO_CARGS_ADDR,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  L_GPR  r9,2200(rSSP) ZZ preserve value in r9
  ST_GPR r1,2200(rSSP) ZZ save as arg ZZ 4 on C stack
],[dnl
ZZ restore CAA to r12
  L_GPR r12,J9TR_CAA_save_offset(rSSP)
  L_GPR  r9,2124(rSSP) ZZ preserve value in r9
  ST_GPR r1,2124(rSSP) ZZ save as arg ZZ 4
])dnl
  LR_GPR r1,r3 ZZ save
  LR_GPR r3,r2 ZZ c arg 3
  LR_GPR r2,r1 ZZ c arg 2
  L_GPR  r1,ProfileValueStackSize+0*PTR_SIZE(r5)

ZZ load up the C Environment addr into r5
LOAD_ADDR_FROM_TOC(r5,TR_S390CEnvironmentAddress)

])dnl

],[dnl

define(XFORM_JITARGS_TO_CARGS_ADDR,
[dnl

  LR_GPR r4,r2
  L_GPR  r2,ProfileValueStackSize+0*PTR_SIZE(r5)
  LR_GPR r5,r1
ZZ  LR_GPR r3,r3 no need
])dnl

])dnl

ifdef([J9ZOS390],[dnl
define(XFORM_JITARGS_TO_CARGS_LONG,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  L_GPR  r9,2200(rSSP) ZZ preserve value in r9
  ST_GPR r1,2200(rSSP) ZZ save as arg ZZ 4 on C stack
  LR_GPR r1,r3 ZZ save
  LR_GPR r3,r2 ZZ c arg 3
  LR_GPR r2,r1 ZZ c arg 2
  L_GPR  r1,ProfileValueStackSize+0*PTR_SIZE(r5)
],[dnl
ZZ restore CAA to r12
  L_GPR r12,J9TR_CAA_save_offset(rSSP)
  L_GPR  r9,2124(rSSP) ZZ preserve value in r9
  L_GPR  r10,2128(rSSP) ZZ preserve value in r10
  ST_GPR r2,2124(rSSP) ZZ save as arg ZZ 5
  ST_GPR r1,2128(rSSP) ZZ save as arg ZZ 4
  LM  r1,r2,ProfileValueStackSize+0*PTR_SIZE(r5)
])dnl

ZZ load up the C Environment addr into r5
LOAD_ADDR_FROM_TOC(r5,TR_S390CEnvironmentAddress)
])dnl

],[dnl

define(XFORM_JITARGS_TO_CARGS_LONG,
[dnl
ifdef([TR_HOST_64BIT],[dnl
  LR_GPR r4,r2
  L_GPR  r2,ProfileValueStackSize+0*PTR_SIZE(r5)
  LR_GPR r5,r1
ZZ  LR_GPR r3,r3 no need
],[dnl
  LR_GPR r4,r3
  LR_GPR r6,r2 ZZ Temporarily save r2 value into r6.
  LM_GPR r2,r3,ProfileValueStackSize+0*PTR_SIZE(r5)
  LR_GPR r5,r6 ZZ Load r2's original value back out of r6
  LR_GPR r6,r1
])dnl
])dnl
])dnl

ZZ * end macro *

ifdef([J9ZOS390],[dnl
define(XFORM_JITARGS_TO_CARGS_BIGDECIMAL,
[dnl
ifdef([TR_HOST_64BIT],[dnl
  L_GPR  r9,2200(rSSP)      ZZ preserve value in r9
  L_GPR  r10,2208(rSSP)     ZZ preserve value in r10
  L_GPR  r11,2216(rSSP)     ZZ preserve value in r11
  L_GPR  r12,2224(rSSP)     ZZ preserve value in r12
  ST_GPR r3,2208(rSSP)      ZZ save as arg ZZ 5
  ST     r2,2220(rSSP)      ZZ save as arg ZZ 6
  ST_GPR r1,2224(rSSP)      ZZ save as arg ZZ 7
  L      r1,ProfileValueStackSize+3*PTR_SIZE(r5)
  ST     r1,2204(rSSP)      ZZ save as arg ZZ 4
  L_GPR  r1,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L_GPR  r2,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L      r3,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
],[dnl
  L_GPR  r12,J9TR_CAA_save_offset(rSSP)
  L_GPR  r9,2124(rSSP)      ZZ preserve value in r9
  L_GPR  r10,2128(rSSP)     ZZ preserve value in r10
  L_GPR  r11,2132(rSSP)     ZZ preserve value in r11
  L_GPR  r14,2136(rSSP)     ZZ preserve value in r14
  ST_GPR r3,2128(rSSP)      ZZ save as arg ZZ 5
  ST     r2,2132(rSSP)      ZZ save as arg ZZ 6
  ST_GPR r1,2136(rSSP)      ZZ save as arg ZZ 7
  L      r1,ProfileValueStackSize+3*PTR_SIZE(r5)
  ST     r1,2124(rSSP)      ZZ save as arg ZZ 4
  L_GPR  r1,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L_GPR  r2,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L      r3,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
])dnl

ZZ load up the C Environment addr into r5
LOAD_ADDR_FROM_TOC(r5,TR_S390CEnvironmentAddress)
])dnl

],[dnl

define(XFORM_JITARGS_TO_CARGS_BIGDECIMAL,
[dnl
ifdef([TR_HOST_64BIT],[dnl
ifdef([J9ZTPF],[dnl
  L_GPR r9,456(rSSP)      ZZ preserve value in r9
  L_GPR r10,448(rSSP)     ZZ preserve value in r10
  ST_GPR r1,456(rSSP)     ZZ save as arg ZZ 7
  ST_GPR r2,448(rSSP)     ZZ save as arg ZZ 6
  LR_GPR r6,r3            ZZ c arg ZZ 5
  L_GPR r2,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L_GPR r3,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L     r4,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
  L     r5,ProfileValueStackSize+3*PTR_SIZE(r5) ZZ c arg ZZ 4
],[dnl
  L_GPR r9,168(rSSP)      ZZ preserve value in r9
  L_GPR r10,164(rSSP)     ZZ preserve value in r10
  ST_GPR r1,168(rSSP)     ZZ save as arg ZZ 7
  ST r2,164(rSSP)         ZZ save as arg ZZ 6
  LR_GPR r6,r3            ZZ c arg ZZ 5
  L_GPR r2,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L_GPR r3,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L     r4,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
  L     r5,ProfileValueStackSize+3*PTR_SIZE(r5) ZZ c arg ZZ 4
])dnl
],[dnl
  L_GPR r9,100(rSSP)      ZZ preserve value in r9
  L_GPR r10,96(rSSP)      ZZ preserve value in r10
  ST_GPR r1,100(rSSP)     ZZ save as arg ZZ 7
  ST r2,96(rSSP)          ZZ save as arg ZZ 6
  LR_GPR r6,r3            ZZ c arg ZZ 5
  L_GPR r2,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L_GPR r3,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L     r4,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
  L     r5,ProfileValueStackSize+3*PTR_SIZE(r5) ZZ c arg ZZ 4
])dnl
])dnl
])dnl

ZZ STRING

ifdef([J9ZOS390],[dnl
define(XFORM_JITARGS_TO_CARGS_STRING,
[dnl
ifdef([TR_HOST_64BIT],[dnl
  L_GPR  r9,2200(rSSP)      ZZ preserve value in r9
  L_GPR  r10,2208(rSSP)     ZZ preserve value in r10
  L_GPR  r11,2216(rSSP)     ZZ preserve value in r11
  ST     r2,2212(rSSP)      ZZ save as arg ZZ 5
  ST_GPR r1,2216(rSSP)      ZZ save as arg ZZ 6
  ST_GPR r3,2200(rSSP)      ZZ save as arg ZZ 4
  L_GPR  r1,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L      r2,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L      r3,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
],[dnl
  L_GPR  r12,J9TR_CAA_save_offset(rSSP)
  L_GPR  r9,2124(rSSP)      ZZ preserve value in r9
  L_GPR  r10,2128(rSSP)     ZZ preserve value in r10
  L_GPR  r11,2132(rSSP)     ZZ preserve value in r11
  ST     r2,2128(rSSP)      ZZ save as arg ZZ 5
  ST_GPR r1,2132(rSSP)      ZZ save as arg ZZ 6
  ST_GPR r3,2124(rSSP)      ZZ save as arg ZZ 4
  L_GPR  r1,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L      r2,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L      r3,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
])dnl

ZZ load up the C Environment addr into r5
LOAD_ADDR_FROM_TOC(r5,TR_S390CEnvironmentAddress)
])dnl

],[dnl

define(XFORM_JITARGS_TO_CARGS_STRING,
[dnl
ifdef([TR_HOST_64BIT],[dnl
  L_GPR r10,164(rSSP)     ZZ preserve value in r10
  ST_GPR r2,164(rSSP)         ZZ save as arg ZZ 6
  LR    r6,r3            ZZ c arg ZZ 5
  L_GPR r2,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L     r3,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L     r4,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
  L_GPR r5,ProfileValueStackSize+3*PTR_SIZE(r5) ZZ c arg ZZ 4
],[dnl
  L_GPR r10,96(rSSP)      ZZ preserve value in r10
  ST_GPR r2,96(rSSP)          ZZ save as arg ZZ 6
  LR    r6,r3            ZZ c arg ZZ 5
  L_GPR r2,ProfileValueStackSize+0*PTR_SIZE(r5) ZZ c arg ZZ 1
  L     r3,ProfileValueStackSize+1*PTR_SIZE(r5) ZZ c arg ZZ 2
  L     r4,ProfileValueStackSize+2*PTR_SIZE(r5) ZZ c arg ZZ 3
  L_GPR r5,ProfileValueStackSize+3*PTR_SIZE(r5) ZZ c arg ZZ 4
])dnl
])dnl
])dnl



ZZ macro for restoring C caller stack as
ZZ before--xplink only
ZZ Restore arg4 to C stack
ZZ probably not necessary, but just in case
ifdef([J9ZOS390],[dnl
define(RESTORE_CALLER_OUTARGS,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  ST_GPR r9,2200(rSSP) ZZ restore previous value
],[dnl
ZZ restore CAA to r12
  ST_GPR r9,2124(rSSP) ZZ restore
])dnl
])dnl

],[dnl

define(RESTORE_CALLER_OUTARGS,
[dnl

ZZ no restore required
])dnl
])dnl

ZZ macro for restoring C caller stack as
ZZ before--xplink only
ZZ Restore arg4 to C stack
ZZ probably not necessary, but just in case
ifdef([J9ZOS390],[dnl
define(RESTORE_CALLER_LONG_OUTARGS,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  ST_GPR r9,2200(rSSP) ZZ restore previous value
],[dnl
ZZ restore CAA to r12
  ST_GPR r9,2124(rSSP) ZZ restore
  ST_GPR r10,2128(rSSP) ZZ restore
])dnl
])dnl

],[dnl

define(RESTORE_CALLER_LONG_OUTARGS,
[dnl

ZZ no restore required
])dnl
])dnl


ifdef([J9ZOS390],[dnl
define(RESTORE_CALLER_BIGDECIMAL_OUTARGS,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  ST_GPR  r9,2200(rSSP)  ZZ restore
  ST_GPR  r10,2208(rSSP) ZZ restore
  ST_GPR  r11,2216(rSSP) ZZ restore
  ST_GPR  r12,2224(rSSP) ZZ restore

],[dnl
  ST_GPR r9,2124(rSSP)  ZZ restore
  ST_GPR r10,2128(rSSP) ZZ restore
  ST_GPR r11,2132(rSSP) ZZ restore
  ST_GPR r14,2136(rSSP) ZZ restore
])dnl
])dnl

],[dnl

define(RESTORE_CALLER_BIGDECIMAL_OUTARGS,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  ST_GPR r9,168(rSSP)   ZZ restore
  ST_GPR r10,164(rSSP)  ZZ restore
],[dnl
ZZ restore CAA to r12
  ST_GPR r9,100(rSSP) ZZ restore
  ST_GPR r10,96(rSSP) ZZ restore
])dnl


ZZ no restore required
])dnl
])dnl


ifdef([J9ZOS390],[dnl
define(RESTORE_CALLER_STRING_OUTARGS,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  ST_GPR  r9,2200(rSSP)  ZZ restore
  ST_GPR  r10,2208(rSSP) ZZ restore
  ST_GPR  r11,2216(rSSP) ZZ restore

],[dnl
  ST_GPR r9,2124(rSSP)  ZZ restore
  ST_GPR r10,2128(rSSP) ZZ restore
  ST_GPR r11,2132(rSSP) ZZ restore
])dnl
])dnl

],[dnl

define(RESTORE_CALLER_STRING_OUTARGS,
[dnl

ifdef([TR_HOST_64BIT],[dnl
  ST_GPR r10,164(rSSP)  ZZ restore
],[dnl
ZZ restore CAA to r12
  ST_GPR r10,96(rSSP) ZZ restore
])dnl


ZZ no restore required
])dnl
])dnl


ZZ * end macro *

SETPPA2()

ZZ Wrapper function for jitProfileValue
ZZ save non-volatile regs on JIT stack
ZZ save JIT sp in non-volatile register
ZZ move regs to appropriate C linkage
ZZ make call
ZZ restore JIT sp
ZZ restore registers
ZZ we know there's some 'slush' in stack so
ZZ there's space to save the registers
ZZ Note: A helper has it's arguments evaluated
ZZ in the *reverse* order to a C call, so
ZZ we must swap them around ie
ZZ 1->4
ZZ 2->3
ZZ 3->2
ZZ 4->1
ZZ
ZZ  Prototype:
ZZ    void _jitProfileValue(uint32_t value,
ZZ           TR_ValueInfo *info,
ZZ           int32_t maxNumValuesProfiled,
ZZ           int32_t *recompilationCounter)
ZZ
START_FUNC(_jitProfileValueWrap,_jitPVW)

    AHI_GPR J9SP,-ProfileValueStackSize
    STM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,44(J9SP)
])dnl
ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    STD f0,ProfileVFPSlot0(J9SP)
    STD f1,ProfileVFPSlot1(J9SP)
    STD f2,ProfileVFPSlot2(J9SP)
    STD f3,ProfileVFPSlot3(J9SP)
    STD f4,ProfileVFPSlot4(J9SP)
    STD f5,ProfileVFPSlot5(J9SP)
    STD f6,ProfileVFPSlot6(J9SP)
    STD f7,ProfileVFPSlot7(J9SP)
])dnl

  LR_GPR r8,J9SP  ZZ happen to know r8 is saved by callee

RestoreSSP

  XFORM_JITARGS_TO_CARGS_INT

LOAD_ADDR_FROM_TOC(CEP,TR_S390jitProfileValueC)

  BASR CRA,CEP
  RETURN_NOP

SaveSSP

  RESTORE_CALLER_OUTARGS

  LR_GPR J9SP,r8

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    LD f0,ProfileVFPSlot0(J9SP)
    LD f1,ProfileVFPSlot1(J9SP)
    LD f2,ProfileVFPSlot2(J9SP)
    LD f3,ProfileVFPSlot3(J9SP)
    LD f4,ProfileVFPSlot4(J9SP)
    LD f5,ProfileVFPSlot5(J9SP)
    LD f6,ProfileVFPSlot6(J9SP)
    LD f7,ProfileVFPSlot7(J9SP)
])dnl
    LM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,44(J9SP)
])dnl
    AHI_GPR J9SP,ProfileValueStackSize
    BR r14

END_FUNC(_jitProfileValueWrap,_jitPVW,9)

ZZ ------------------------------------------
START_FUNC(_jitProfileAddressWrap,_jitPAW)
    AHI_GPR J9SP,-ProfileValueStackSize
    STM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,44(J9SP)
])dnl
ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    STD f0,ProfileVFPSlot0(J9SP)
    STD f1,ProfileVFPSlot1(J9SP)
    STD f2,ProfileVFPSlot2(J9SP)
    STD f3,ProfileVFPSlot3(J9SP)
    STD f4,ProfileVFPSlot4(J9SP)
    STD f5,ProfileVFPSlot5(J9SP)
    STD f6,ProfileVFPSlot6(J9SP)
    STD f7,ProfileVFPSlot7(J9SP)
])dnl

  LR_GPR r8,J9SP  ZZ happen to know r8 is saved by callee

RestoreSSP

  XFORM_JITARGS_TO_CARGS_ADDR


LOAD_ADDR_FROM_TOC(CEP,TR_S390jitProfileAddressC)

  BASR CRA,CEP
  RETURN_NOP

SaveSSP

  RESTORE_CALLER_OUTARGS

  LR_GPR J9SP,r8

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    LD f0,ProfileVFPSlot0(J9SP)
    LD f1,ProfileVFPSlot1(J9SP)
    LD f2,ProfileVFPSlot2(J9SP)
    LD f3,ProfileVFPSlot3(J9SP)
    LD f4,ProfileVFPSlot4(J9SP)
    LD f5,ProfileVFPSlot5(J9SP)
    LD f6,ProfileVFPSlot6(J9SP)
    LD f7,ProfileVFPSlot7(J9SP)
])dnl
    LM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,44(J9SP)
])dnl
    AHI_GPR J9SP,ProfileValueStackSize
    BR r14

END_FUNC(_jitProfileAddressWrap,_jitPAW,9)

ZZ ------------------------------------------
START_FUNC(_jitProfileLongValueWrap,_jitPLW)
    AHI_GPR J9SP,-ProfileValueStackSize
    STM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,44(J9SP)
])dnl
ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    STD f0,ProfileVFPSlot0(J9SP)
    STD f1,ProfileVFPSlot1(J9SP)
    STD f2,ProfileVFPSlot2(J9SP)
    STD f3,ProfileVFPSlot3(J9SP)
    STD f4,ProfileVFPSlot4(J9SP)
    STD f5,ProfileVFPSlot5(J9SP)
    STD f6,ProfileVFPSlot6(J9SP)
    STD f7,ProfileVFPSlot7(J9SP)
])dnl

  LR_GPR r8,J9SP  ZZ happen to know r8 is saved by callee

RestoreSSP

  XFORM_JITARGS_TO_CARGS_LONG

LOAD_ADDR_FROM_TOC(CEP,TR_S390jitProfileLongValueC)

  BASR CRA,CEP
  RETURN_NOP

SaveSSP

  RESTORE_CALLER_LONG_OUTARGS

  LR_GPR J9SP,r8

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    LD f0,ProfileVFPSlot0(J9SP)
    LD f1,ProfileVFPSlot1(J9SP)
    LD f2,ProfileVFPSlot2(J9SP)
    LD f3,ProfileVFPSlot3(J9SP)
    LD f4,ProfileVFPSlot4(J9SP)
    LD f5,ProfileVFPSlot5(J9SP)
    LD f6,ProfileVFPSlot6(J9SP)
    LD f7,ProfileVFPSlot7(J9SP)
])dnl
    LM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,44(J9SP)
])dnl
    AHI_GPR J9SP,ProfileValueStackSize
    BR r14

END_FUNC(_jitProfileLongValueWrap,_jitPLW,9)

ZZ ------------------------------------------
START_FUNC(_jitProfileBigDecimalValueWrap,_jitPBDW)
    AHI_GPR J9SP,-ProfileValueStackSize
    STM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,44(J9SP)
])dnl
ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    STD f0,ProfileVFPSlot0(J9SP)
    STD f1,ProfileVFPSlot1(J9SP)
    STD f2,ProfileVFPSlot2(J9SP)
    STD f3,ProfileVFPSlot3(J9SP)
    STD f4,ProfileVFPSlot4(J9SP)
    STD f5,ProfileVFPSlot5(J9SP)
    STD f6,ProfileVFPSlot6(J9SP)
    STD f7,ProfileVFPSlot7(J9SP)
])dnl

  LR_GPR r8,J9SP  ZZ happen to know r8 is saved by callee

RestoreSSP

  XFORM_JITARGS_TO_CARGS_BIGDECIMAL

LOAD_ADDR_FROM_TOC(CEP,TR_S390jitProfileBigDecimalValueC)

  BASR CRA,CEP
  RETURN_NOP

SaveSSP

  RESTORE_CALLER_BIGDECIMAL_OUTARGS

  LR_GPR J9SP,r8

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    LD f0,ProfileVFPSlot0(J9SP)
    LD f1,ProfileVFPSlot1(J9SP)
    LD f2,ProfileVFPSlot2(J9SP)
    LD f3,ProfileVFPSlot3(J9SP)
    LD f4,ProfileVFPSlot4(J9SP)
    LD f5,ProfileVFPSlot5(J9SP)
    LD f6,ProfileVFPSlot6(J9SP)
    LD f7,ProfileVFPSlot7(J9SP)
])dnl
    LM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,44(J9SP)
])dnl
    AHI_GPR J9SP,ProfileValueStackSize
    BR r14

END_FUNC(_jitProfileBigDecimalValueWrap,_jitPBDW,9)

ZZ ------------------------------------------
START_FUNC(_jitProfileStringValueWrap,_jitPSW)
    AHI_GPR J9SP,-ProfileValueStackSize
    STM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,44(J9SP)
])dnl
ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    STD f0,ProfileVFPSlot0(J9SP)
    STD f1,ProfileVFPSlot1(J9SP)
    STD f2,ProfileVFPSlot2(J9SP)
    STD f3,ProfileVFPSlot3(J9SP)
    STD f4,ProfileVFPSlot4(J9SP)
    STD f5,ProfileVFPSlot5(J9SP)
    STD f6,ProfileVFPSlot6(J9SP)
    STD f7,ProfileVFPSlot7(J9SP)
])dnl

  LR_GPR r8,J9SP  ZZ happen to know r8 is saved by callee

RestoreSSP

  XFORM_JITARGS_TO_CARGS_STRING

LOAD_ADDR_FROM_TOC(CEP,TR_S390jitProfileStringValueC)

  BASR CRA,CEP
  RETURN_NOP

SaveSSP

  RESTORE_CALLER_STRING_OUTARGS

  LR_GPR J9SP,r8

ifdef([J9_SOFT_FLOAT],[dnl
ZZ do nothing
],[dnl
    LD f0,ProfileVFPSlot0(J9SP)
    LD f1,ProfileVFPSlot1(J9SP)
    LD f2,ProfileVFPSlot2(J9SP)
    LD f3,ProfileVFPSlot3(J9SP)
    LD f4,ProfileVFPSlot4(J9SP)
    LD f5,ProfileVFPSlot5(J9SP)
    LD f6,ProfileVFPSlot6(J9SP)
    LD f7,ProfileVFPSlot7(J9SP)
])dnl
    LM_GPR r5,r15,0(J9SP)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,44(J9SP)
])dnl
    AHI_GPR J9SP,ProfileValueStackSize
    BR r14

END_FUNC(_jitProfileStringValueWrap,_jitPSW,9)

ZZ ===================================================================
ZZ Wrapper to modify runtime instrumentation controls.
ZZ @param CARG1 pointer to RI Control Block
ZZ ==================================================================
START_FUNC(_jitRIMRIC,_jitRIMRIC)
ifdef([J9ZOS390],[dnl
   DC HEX(EB001000)
   DC HEX(0062)
   B  RETURNOFFSET(CRA)
],[dnl
   .long HEX(EB002000)
   .long HEX(0062)
   BR CRA
])dnl
END_FUNC(_jitRIMRIC,_jitRIMRIC,12)

ZZ ==================================================================
ZZ Wrapper to acquire runtime instrumentation controls.
ZZ @param   CARG1 pointer to RI Control Block
ZZ @returns 0  if Controls are valid and unchanged
ZZ          2  if Problem-state prohibited
ZZ          3  if Controls are not valid
ZZ ==================================================================
START_FUNC(_jitRISTRIC,_jitRISTRIC)
   LHI CRINT,0
ifdef([J9ZOS390],[dnl
   DC HEX(EB001000)
   DC HEX(0061)
   BC 8,RETURNOFFSET(CRA)
   LHI CRINT,2
   BC 2,RETURNOFFSET(CRA)
   LHI CRINT,3
   B  RETURNOFFSET(CRA)
],[dnl
   .long HEX(EB002000)
   .long HEX(0061)
   BCR 8,CRA
   LHI CRINT,2
   BCR 2,CRA
   LHI CRINT,3
   BR CRA
])dnl
END_FUNC(_jitRISTRIC,_jitRISTRIC,13)

ZZ ==================================================================
ZZ Wrapper to test runtime instrumentation controls.
ZZ @returns 0  if Controls are valid and unchanged
ZZ          1  if Controls are valid and changed
ZZ          2  if Problem-state prohibited
ZZ          3  if Controls are not valid
ZZ ==================================================================
START_FUNC(_jitRITRIC,_jitRITRIC)
   LHI CRINT,0
ifdef([J9ZOS390],[dnl
   DC HEX(AA020000)
   BC 8,RETURNOFFSET(CRA)
   LHI CRINT,1
   BC 4,RETURNOFFSET(CRA)
   LHI CRINT,2
   BC 2,RETURNOFFSET(CRA)
   LHI CRINT,3
   B  RETURNOFFSET(CRA)
],[dnl
   .long HEX(AA020000)
   BCR 8,CRA
   LHI CRINT,1
   BCR 4,CRA
   LHI CRINT,2
   BCR 2,CRA
   LHI CRINT,3
   BR CRA
])dnl
END_FUNC(_jitRITRIC,_jitRITRIC,12)

ZZ ==================================================================
ZZ Wrapper to enable runtime instrumentation.
ZZ @returns 0  if Controls are valid and unchanged
ZZ          2  if Problem-state prohibited
ZZ          3  if Controls are not valid
ZZ ==================================================================
START_FUNC(_jitRION,_jitRION)
   LHI CRINT,0
ifdef([J9ZOS390],[dnl
   DC HEX(AA010000)
   BC 8,RETURNOFFSET(CRA)
   LHI CRINT,2
   BC 2,RETURNOFFSET(CRA)
   LHI CRINT,3
   B  RETURNOFFSET(CRA)
],[dnl
   .long HEX(AA010000)
   BCR 8,CRA
   LHI CRINT,2
   BCR 2,CRA
   LHI CRINT,3
   BR CRA
])dnl
END_FUNC(_jitRION,_jitRION,10)

ZZ ==================================================================
ZZ Wrapper to disable runtime instrumentation.
ZZ @returns 0  if Controls are valid and unchanged
ZZ          2  if Problem-state prohibited
ZZ          3  if Controls are not valid
ZZ ==================================================================
START_FUNC(_jitRIOFF,_jitRIOFF)
   LHI CRINT,0
ifdef([J9ZOS390],[dnl
   DC HEX(AA030000)
   BC 8,RETURNOFFSET(CRA)
   LHI CRINT,2
   BC 2,RETURNOFFSET(CRA)
   LHI CRINT,3
   B  RETURNOFFSET(CRA)
],[dnl
   .long HEX(AA030000)
   BCR 8,CRA
   LHI CRINT,2
   BCR 2,CRA
   LHI CRINT,3
   BR CRA
   ])dnl
END_FUNC(_jitRIOFF,_jitRIOFF,11)

ifdef([J9ZOS390],[dnl
    END
])dnl

