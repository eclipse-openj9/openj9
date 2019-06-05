dnl Copyright (c) 1991, 2019 IBM Corp. and others
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

include(jilvalues.m4)

define({CINTERP_STACK_SIZE},{J9CONST(J9TR_cframe_sizeof,$1,$2)})

define({r0},0)
define({r1},1)
define({r2},2)
define({r3},3)
define({r4},4)
define({r5},5)
define({r6},6)
define({r7},7)
define({r8},8)
define({r9},9)
define({r10},10)
define({r11},11)
define({r12},12)
define({r13},13)
define({r14},14)
define({r15},15)

dnl We cannot use 'f0', 'f1', etc. for these macros as such definitions would
dnl conflict with the hexadecimal result of eval operations and incorrect
dnl macro substitutions will be performed.
define({fpr0},0)
define({fpr1},1)
define({fpr2},2)
define({fpr3},3)
define({fpr4},4)
define({fpr5},5)
define({fpr6},6)
define({fpr7},7)
define({fpr8},8)
define({fpr9},9)
define({fpr10},10)
define({fpr11},11)
define({fpr12},12)
define({fpr13},13)
define({fpr14},14)
define({fpr15},15)

ifdef({ASM_J9VM_ENV_DATA64},{
define({A_GPR},ag)
define({AHI_GPR},aghi)
define({AL_GPR},alg)
define({AL32_GPR},algf)
define({ALR_GPR},algr)
define({AR_GPR},agr)
define({AGFR_GPR},agfr)
define({AGF_GPR},agf)
define({BCTR_GPR},bctgr)
define({BRCT_GPR},brctg)
define({C_GPR},cg)
define({CHI_GPR},cghi)
define({CDS_GPR},cdsg)
define({CL_GPR},clg)
define({CLFI_GPR},clgfi)
define({CLM_GPR},clmh)
define({CLR_GPR},clgr)
define({CR_GPR},cgr)
define({CS_GPR},csg)
define({ICMH_GPR},icmh)
define({LCR_GPR},lcgr)
define({L_GPR},lg)
define({LDT_GPR},ltgr)
define({LGF_GPR},lgf)
define({LGFR_GPR},lgfr)
define({LR_GPR},lgr)
define({LH_GPR},lgh)
define({LHI_GPR},lghi)
define({LM_GPR},lmg)
define({LNR_GPR},lngr)
define({LTR_GPR},ltgr)
define({MSGR_GPR},msgr)
define({N_GPR},ng)
define({NIHH_GPR},nihh)
define({O_GPR},og)
define({S_GPR},sg)
define({SL32_GPR},slgf)
define({SLA_GPR},{slag $1,$1,$2})
define({SLL_GPR},{sllg $1,$1,$2})
define({SLR_GPR},slgr)
define({SR_GPR},sgr)
define({SRA_GPR},{srag $1,$1,$2})
define({SRL_GPR},{srlg $1,$1,$2})
define({ST_GPR},stg)
define({STDG_GPR},stdg)
define({STM_GPR},stmg)
define({STCMH_GPR},stcmh)
define({XR_GPR},xgr)
define({L32_GPR},llgf)
define({NR_GPR},ngr)
},{
define({A_GPR},a)
define({AHI_GPR},ahi)
define({AL_GPR},al)
define({AL32_GPR},al)
define({ALR_GPR},alr)
define({AR_GPR},ar)
define({AGFR_GPR},ar)
define({AGF_GPR},a)
define({BCTR_GPR},bctr)
define({BRCT_GPR},brct)
define({C_GPR},c)
define({CHI_GPR},chi)
define({CDS_GPR},cds)
define({CL_GPR},cl)
define({CLFI_GPR},clfi)
define({CLM_GPR},clm)
define({CLR_GPR},clr)
define({CR_GPR},cr)
define({CS_GPR},cs)
define({ICM_GPR},icm)
define({IC_GPR},ic)
define({L_GPR},l)
define({LCR_GPR},lcr)
define({LDT_GPR},ltr)
define({LGF_GPR},l)
define({LR_GPR},lr)
define({LGFR_GPR},lr)
define({LH_GPR},lh)
define({LHI_GPR},lhi)
define({LM_GPR},lm)
define({LMH_GPR},lmh)
define({LNR_GPR},lnr)
define({LTR_GPR},ltr)
define({N_GPR},n)
define({O_GPR},o)
define({S_GPR},s)
define({SL32_GPR},sl)
define({SLA_GPR},{sla $1,$2})
define({SLL_GPR},{sll $1,$2})
define({SLR_GPR},slr)
define({SR_GPR},sr)
define({SRA_GPR},{sra $1,$2})
define({SRL_GPR},{srl $1,$2})
define({ST_GPR},st)
define({STM_GPR},stm)
define({STMH_GPR},stmh)
define({XR_GPR},xr)
define({L32_GPR},l)
define({NR_GPR},nr)
})

define({CONCAT},$1$2)
define({SYM_COUNT},0)
define({BEGIN_FUNC},{
define({SYM_COUNT},eval(1+SYM_COUNT))
define({CURRENT_LONG},$1)
define({CURRENT_SHORT},CONCAT(J9SYM,SYM_COUNT))
START_FUNC(CURRENT_LONG,CURRENT_SHORT)
})
define({END_CURRENT},{END_FUNC(CURRENT_LONG,CURRENT_SHORT,eval(1+len(CURRENT_LONG)))})

ifdef({J9ZOS390},{

ifelse(eval(CINTERP_STACK_SIZE % 32),0, ,{ERROR stack size CINTERP_STACK_SIZE is not 32-aligned})

define({CSP},4)
define({CARG1},1)
define({CARG2},2)
define({CARG3},3)
define({CRA},7)
define({CRINT},3)
define({STACK_BIAS},{J9CONST(2048,$1,$2)})
define({LABEL_NAME},{translit({$*},{a-z},{A-Z})})
define({PLACE_LABEL},{
LABEL_NAME($1) DS 0H
})
define({BYTE},{DC X'translit(eval($1,16,2),{a-z},{A-Z})'})

ifdef({ASM_J9VM_ENV_DATA64},{

define({START_FUNC},{
     DS    0D
Z#$2  CSECT
Z#$2  RMODE ANY
Z#$2  AMODE ANY
     DC    0D'0',XL8'00C300C500C500F1'
     DC    AL4(PPAZ$2-Z#$2),AL4(0)
Z$2  DS 0D
     ENTRY Z$2
Z$2  ALIAS C'$1'
Z$2  XATTR SCOPE(X),LINKAGE(XPLINK)
Z$2  AMODE 64
})
define({END_FUNC},{
CODELZ$2      EQU *-Z$2
PPAZ$2      DS    0F
        DC    B'00000010'    
        DC    X'CE'         
        DC    XL2'0FFF'        
        DC    AL4(PPA2-PPAZ$2)       
        DC    B'10000000'       
        DC    B'00000000'  
        DC    B'00000000'     
        DC    B'00000001'     
        DC    AL2(0)                
        DC    AL1(0)               
        DC    XL.4'0',AL.4(0)    
        DC    AL4(CODELZ$2)     
        DC    H'$3'       
        DC    CL$3'Z$1'  
        DC    AL4(Z$2-PPAZ$2) 
})
define({J9_AMODE},64)

define({CALL_INDIRECT},{
    lmg r5,r6,0($1)
    basr CRA,r6
    nopr 0
})

define({FILE_START},{
        TITLE '$1.s'
LABEL_NAME($1)#START      CSECT
LABEL_NAME($1)#START      RMODE ANY
LABEL_NAME($1)#START      AMODE 64
      EXTRN CELQSTRT
PPA2   DS    0D
        DC    XL4'03002204'
        DC   AL4(CELQSTRT-PPA2)
        DC   AL4(0)
        DC   AL4(0)
        DC   AL4(0)
        DC   B'10000001'
        DC   XL3'000000'
})

define({FILE_END},{
    END
})

},{

define({START_FUNC},{
Z$2  DS 0D
     ENTRY Z$2
Z$2  ALIAS C'$1'
Z$2  XATTR SCOPE(X),LINKAGE(XPLINK)
Z$2  AMODE 31
})
define({END_FUNC},{
     DC AL4(Z$2)
     DC XL8'005F004A00490054'
     DC CL$3'Z$2'
})
define({J9_AMODE},31)

define({CALL_INDIRECT},{
ifdef({FIRST_SYM},,define({FIRST_SYM},CURRENT_SHORT))
    lm r5,r6,16($1)
    basr CRA,r6
    DC X'4700',Y((LCALLDESC-(*-8))/8)
})

define({FILE_START},{
        TITLE '$1.s'
LABEL_NAME($1)#START      CSECT
LABEL_NAME($1)#START      RMODE ANY
LABEL_NAME($1)#START      AMODE 31
    EXTRN CEESTART
PPA2 DS 0D
    DC XL4'03002204'
    DC AL4(CEESTART-PPA2)
    DC AL4(0)
    DC AL4(0)
    DC AL4(0)
    DC B'10000001'
    DC XL3'000000'
})

define({FILE_END},{
ifdef({FIRST_SYM},{
LCALLDESC DS 0D
    DC A(CONCAT(Z,FIRST_SYM)-*)
    DC BL.3'000',BL.5'00000'
    DC BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
})
    END
})

})

define({BEGIN_C_FUNC},{
define({CURRENT_ARG_COUNT},$2)
define({CURRENT_STACK_SIZE},$3)
define({SYM_COUNT},eval(1+SYM_COUNT))
define({CURRENT_LONG},$1)
define({CURRENT_SHORT},CONCAT(J9SYM,SYM_COUNT))
define({TAGGED_SHORT},CONCAT(Z,CURRENT_SHORT))
    DS 0D
CONCAT(EPM,TAGGED_SHORT) CSECT
CONCAT(EPM,TAGGED_SHORT) RMODE ANY
CONCAT(EPM,TAGGED_SHORT) AMODE ANY
    DC 0D'0',XL8'00C300C500C500F1'
    DC AL4(CONCAT(PPA,TAGGED_SHORT)-CONCAT(EPM,TAGGED_SHORT))
    DC AL4(CURRENT_STACK_SIZE)
TAGGED_SHORT DS 0D
    ENTRY TAGGED_SHORT
TAGGED_SHORT ALIAS C'CURRENT_LONG'
TAGGED_SHORT XATTR SCOPE(X),LINKAGE(XPLINK)
TAGGED_SHORT AMODE J9_AMODE
    STM_GPR r4,r15,(STACK_BIAS-CURRENT_STACK_SIZE)(CSP)
CONCAT(STKPPA,TAGGED_SHORT) EQU (*-TAGGED_SHORT)/2
    AHI_GPR CSP,-CURRENT_STACK_SIZE
CONCAT(PRLGPPA,TAGGED_SHORT) EQU (*-TAGGED_SHORT)/2
})
define({END_C_FUNC},{
    LM_GPR r4,r15,STACK_BIAS(CSP)
    br CRA
CONCAT(CODEL,TAGGED_SHORT) EQU *-TAGGED_SHORT
CONCAT(PPA,TAGGED_SHORT) DS 0F
    DC B'00000010'
    DC X'CE'
    DC XL2'0FFF'
    DC AL4(PPA2-CONCAT(PPA,TAGGED_SHORT))
    DC B'00000000'
    DC B'00000000'
    DC B'00000000'
    DC B'00000001'
    DC AL2(CURRENT_ARG_COUNT)
    DC AL1(CONCAT(PRLGPPA,TAGGED_SHORT))
    DC XL.4'0',AL.4(CONCAT(STKPPA,TAGGED_SHORT))
    DC AL4(CONCAT(CODEL,TAGGED_SHORT))
    DC H'len(TAGGED_SHORT)'
    DC CONCAT(CL,len(TAGGED_SHORT))'TAGGED_SHORT'
})

define({ARG_SAVE_OFFSET},{J9CONST(eval(STACK_BIAS+J9TR_cframe_argRegisterSave),$1,$2)})
define({OUTGOING_ARG_OFFSET},{J9CONST(eval(ARG_SAVE_OFFSET+(4*J9TR_pointerSize)),$1,$2)})
define({FPR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_preservedFPRs+(($1)*8))})
define({VR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_preservedVRs+(($1)*16))})
ifdef({ASM_J9VM_PORT_ZOS_CEEHDLRSUPPORT},{
define({CEEHDLR_FPC_SAVE_OFFSET},{J9CONST(eval(STACK_BIAS+J9TR_cframe_fpcCEEHDLR),$1,$2)})
define({CEEHDLR_GPR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_gprCEEHDLR+(($1)*J9TR_pointerSize))})
})

},{

ifelse(eval(CINTERP_STACK_SIZE % 8),0, ,{ERROR stack size CINTERP_STACK_SIZE is not 8-aligned})

define({CSP},15)
define({CARG1},2)
define({CARG2},3)
define({CARG3},4)
define({CRA},14)
define({CRINT},2)
define({STACK_BIAS},{J9CONST(0,$1,$2)})
define({LABEL_NAME},{.$1})
define({PLACE_LABEL},{
LABEL_NAME($1):
})
define({BYTE},{.byte $1})

define({CALL_INDIRECT},{basr CRA,$1})

define({FILE_START},{.file "%1.s"})
define({FILE_END}, )
define({START_FUNC},{
    .align 8
    .global $1
    .text
    .type $1,@function
$1:
})
define({END_FUNC},{.size $1,.-$1})

define({BEGIN_C_FUNC},{
define({CURRENT_ARG_COUNT},$2)
define({CURRENT_STACK_SIZE},$3)
BEGIN_FUNC($1)
    STM_GPR r6,r15,GPR_SAVE_OFFSET_IN_CALLER_FRAME(0)(CSP)
    LR_GPR r0,CSP
    AHI_GPR CSP,-CURRENT_STACK_SIZE
    ST_GPR r0,0(CSP)
})
define({END_C_FUNC},{
    LM_GPR r6,r15,(GPR_SAVE_OFFSET_IN_CALLER_FRAME(0)+CURRENT_STACK_SIZE)(CSP)
    br CRA
END_CURRENT
})

define({GPR_SAVE_OFFSET_IN_CALLER_FRAME},{eval(J9TR_cframe_preservedGPRs+(($1)*J9TR_pointerSize))})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+(($1)*8))})

})

define({J9VMTHREAD},13)
define({J9PC},8)
define({J9LITERALS},9)
define({J9A0},10)
define({J9SP},5)

define({VECTOR_LOADSTORE},{
    BYTE(231)
    BYTE(eval((($1 % 16) * 16) +($2 % 16)))
    BYTE(eval(($4 * 16) +($3 / 256)))
    BYTE(eval($3 % 256))
    BYTE(eval((($1 / 16) * 8) +(($2 / 16) * 4)))
    BYTE($5)
})

define({VLDM},{VECTOR_LOADSTORE($1,$2,$3,$4,54)})
define({VSTM},{VECTOR_LOADSTORE($1,$2,$3,$4,62)})

dnl Encodes the Branch Indirect On Condition (BIC) instruction with the
dnl following format:
dnl 
dnl BIC M1,D2(X2,B2)
dnl
dnl Where:
dnl
dnl M1 = $1
dnl D2 = $2
dnl X2 = $3
dnl B2 = $4
define({ENCODE_BIC},{
    BYTE(227)
    BYTE(eval((($1 % 16) * 16) + ($3 % 16)))
    BYTE(eval((($4 % 16) * 16) + (($2 / 256) % 16)))
    BYTE(eval($2 % 256))
    BYTE(eval(($2 / 4096) % 256))
    BYTE(71)
})

dnl Encodes the Add Immediate (AGSI) instruction with the following
dnl format:
dnl 
dnl AGSI D1(B2),I2
dnl
dnl Where:
dnl
dnl D1 = $1
dnl B1 = $2
dnl I2 = $3
define({ENCODE_AGSI},{
    BYTE(235)
    BYTE($3)
    BYTE(eval((($2 % 16) * 16) + (($1 / 256) % 16)))
    BYTE(eval($1 % 256))
    BYTE(eval(($1 / 4096) % 256))
    BYTE(122)
})

dnl Encodes the Rotate Then Insert Selected Bits (RISBGN) instruction
dnl with the following format:
dnl 
dnl RISBGN R1,R2,I3,I4,I5
dnl
dnl Where:
dnl
dnl R1 = $1
dnl R2 = $2
dnl I3 = $3
dnl I4 = $4
dnl I5 = $5
define({ENCODE_RISBGN},{
    BYTE(236)
    BYTE(eval((($1 % 16) * 16) + ($2 % 16)))
    BYTE($3)
    BYTE($4)
    BYTE($5)
    BYTE(89)
})

define({JIT_GPR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_jitGPRs+(($1)*J9TR_pointerSize))})
define({JIT_GPR_SAVE_SLOT},{JIT_GPR_SAVE_OFFSET($1)(CSP)})
define({JIT_FPR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_jitFPRs+(($1)*8))})
define({JIT_FPR_SAVE_SLOT},{JIT_FPR_SAVE_OFFSET($1)(CSP)})
define({JIT_VR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_jitVRs+(($1)*16))})
define({JIT_VR_SAVE_SLOT},{JIT_VR_SAVE_OFFSET($1)(CSP)})

define({SAVE_LR},{ST_GPR r14,JIT_GPR_SAVE_SLOT(14)})
define({RESTORE_LR},{L_GPR r14,JIT_GPR_SAVE_SLOT(14)})

ifdef({ASM_J9VM_JIT_32BIT_USES64BIT_REGISTERS},{
define({SAVE_HIWORD_REGS},{stmh r0,r15,JIT_GPR_SAVE_SLOT(16)})
define({RESTORE_HIWORD_REGS},{lmh r0,r15,JIT_GPR_SAVE_SLOT(16)})
})

ifdef({J9ZOS390},{
define({JUMP_REG},r15)
define({FIRST_REG},r8)
define({LAST_REG},r15)
define({RELOAD_SSP},{
    L_GPR CSP,J9TR_VMThread_systemStackPointer(J9VMTHREAD)
    xc J9TR_VMThread_systemStackPointer(J9TR_pointerSize,J9VMTHREAD),J9TR_VMThread_systemStackPointer(J9VMTHREAD)
})
define({RELOAD_SSP_SAVE_VALUE},{
    ST_GPR CSP,J9TR_VMThread_returnValue(J9VMTHREAD)
    RELOAD_SSP
})
define({SWAP_SSP_VALUE},{
    mvc JIT_GPR_SAVE_OFFSET(CSP)(J9TR_pointerSize,CSP),J9TR_VMThread_returnValue(J9VMTHREAD)
})
define({STORE_SSP},{
    ST_GPR CSP,J9TR_VMThread_systemStackPointer(J9VMTHREAD)
})
ifdef({J9TR_CAA_save_offset},{define({LOAD_CAA},{L_GPR r12,J9TR_CAA_save_offset(CSP)})})
},{
define({JUMP_REG},r4)
define({FIRST_REG},r6)
define({LAST_REG},r14)
})

define({SAVE_C_NONVOLATILE_GPRS},{
    STM_GPR FIRST_REG,LAST_REG,JIT_GPR_SAVE_SLOT(FIRST_REG)
    SAVE_HIWORD_REGS
})

define({RESTORE_C_NONVOLATILE_GPRS},{
    LM_GPR FIRST_REG,LAST_REG,JIT_GPR_SAVE_SLOT(FIRST_REG)
    RESTORE_HIWORD_REGS
})

ifdef({RELOAD_SSP},,{define({RELOAD_SSP})})
ifdef({RELOAD_SSP_SAVE_VALUE},,{define({RELOAD_SSP_SAVE_VALUE})})
ifdef({SWAP_SSP_VALUE},,{define({SWAP_SSP_VALUE})})
ifdef({STORE_SSP},,{define({STORE_SSP})})
ifdef({LOAD_CAA},,{define({LOAD_CAA})})
ifdef({SAVE_HIWORD_REGS},,{define({SAVE_HIWORD_REGS})})
ifdef({RESTORE_HIWORD_REGS},,{define({RESTORE_HIWORD_REGS})})

define({BEGIN_HELPER},{
BEGIN_FUNC($1)
    RELOAD_SSP_SAVE_VALUE
    STM_GPR r0,LAST_REG,JIT_GPR_SAVE_SLOT(0)
    SWAP_SSP_VALUE
    SAVE_HIWORD_REGS
    LOAD_CAA
})

define({END_HELPER},{
    RESTORE_HIWORD_REGS
    ST_GPR J9SP,JIT_GPR_SAVE_SLOT(J9SP)
    STORE_SSP
    LM_GPR r0,LAST_REG,JIT_GPR_SAVE_SLOT(0)
    br r14
END_CURRENT
})

define({LOAD_LABEL_CONSTANT},{
    L_GPR $3,J9TR_VMThread_javaVM(J9VMTHREAD)
    L_GPR $3,J9TR_JavaVMJitConfig{($3)}
    L_GPR $3,J9TR_JitConfig_$2{($3)}
})

define({CALL_C},{
    LOAD_LABEL_CONSTANT($1,$2,CRA)
    CALL_INDIRECT(CRA)
})

define({CALL_C_WITH_VMTHREAD},{
    LR_GPR CARG1,J9VMTHREAD
    CALL_C($1,$2)
})

ifdef({J9ZOS390},{
define({SAVE_C_VOLATILE_VRS},{
    VSTM(0, 15, JIT_VR_SAVE_OFFSET(0), CSP)
    VSTM(24, 31, JIT_VR_SAVE_OFFSET(24), CSP)
})
define({RESTORE_C_VOLATILE_VRS},{
    VLDM(0, 15, JIT_VR_SAVE_OFFSET(0), CSP)
    VLDM(24, 31, JIT_VR_SAVE_OFFSET(24), CSP)
})
},{
define({SAVE_C_VOLATILE_VRS},{
    VSTM(0, 15, JIT_VR_SAVE_OFFSET(0), CSP)
    VSTM(16, 31, JIT_VR_SAVE_OFFSET(16), CSP)
})
define({RESTORE_C_VOLATILE_VRS},{
    VLDM(0, 15, JIT_VR_SAVE_OFFSET(0), CSP)
    VLDM(16, 31, JIT_VR_SAVE_OFFSET(16), CSP)
})
})

ifdef({ASM_J9VM_ENV_DATA64},{
define({FPRS_0_TO_7_VOLATILE})
})
ifdef({J9ZOS390},{
define({FPRS_0_TO_7_VOLATILE})
})

ifdef({FPRS_0_TO_7_VOLATILE},{

define({SAVE_C_VOLATILE_FPRS},{
    std fpr0,JIT_FPR_SAVE_OFFSET(0)(CSP)
    std fpr1,JIT_FPR_SAVE_OFFSET(1)(CSP)
    std fpr2,JIT_FPR_SAVE_OFFSET(2)(CSP)
    std fpr3,JIT_FPR_SAVE_OFFSET(3)(CSP)
    std fpr4,JIT_FPR_SAVE_OFFSET(4)(CSP)
    std fpr5,JIT_FPR_SAVE_OFFSET(5)(CSP)
    std fpr6,JIT_FPR_SAVE_OFFSET(6)(CSP)
    std fpr7,JIT_FPR_SAVE_OFFSET(7)(CSP)
})

define({RESTORE_C_VOLATILE_FPRS},{
    ld fpr0,JIT_FPR_SAVE_OFFSET(0)(CSP)
    ld fpr1,JIT_FPR_SAVE_OFFSET(1)(CSP)
    ld fpr2,JIT_FPR_SAVE_OFFSET(2)(CSP)
    ld fpr3,JIT_FPR_SAVE_OFFSET(3)(CSP)
    ld fpr4,JIT_FPR_SAVE_OFFSET(4)(CSP)
    ld fpr5,JIT_FPR_SAVE_OFFSET(5)(CSP)
    ld fpr6,JIT_FPR_SAVE_OFFSET(6)(CSP)
    ld fpr7,JIT_FPR_SAVE_OFFSET(7)(CSP)
})

dnl No need to save/restore non-volatile FPRs.
dnl The stack walker will never need to read or
dnl modify them (no preserved FPRs in the JIT
dnl private linkage).

define({SAVE_C_NONVOLATILE_REGS}, )

define({RESTORE_C_NONVOLATILE_REGS}, )

},{

define({SAVE_C_VOLATILE_FPRS},{
    std fpr0,JIT_FPR_SAVE_OFFSET(0)(CSP)
    std fpr1,JIT_FPR_SAVE_OFFSET(1)(CSP)
    std fpr2,JIT_FPR_SAVE_OFFSET(2)(CSP)
    std fpr3,JIT_FPR_SAVE_OFFSET(3)(CSP)
    std fpr5,JIT_FPR_SAVE_OFFSET(5)(CSP)
    std fpr7,JIT_FPR_SAVE_OFFSET(7)(CSP)
    std fpr8,JIT_FPR_SAVE_OFFSET(8)(CSP)
    std fpr9,JIT_FPR_SAVE_OFFSET(9)(CSP)
    std fpr10,JIT_FPR_SAVE_OFFSET(10)(CSP)
    std fpr11,JIT_FPR_SAVE_OFFSET(11)(CSP)
    std fpr12,JIT_FPR_SAVE_OFFSET(12)(CSP)
    std fpr13,JIT_FPR_SAVE_OFFSET(13)(CSP)
    std fpr14,JIT_FPR_SAVE_OFFSET(14)(CSP)
    std fpr15,JIT_FPR_SAVE_OFFSET(15)(CSP)
})

define({RESTORE_C_VOLATILE_FPRS},{
    ld fpr0,JIT_FPR_SAVE_OFFSET(0)(CSP)
    ld fpr1,JIT_FPR_SAVE_OFFSET(1)(CSP)
    ld fpr2,JIT_FPR_SAVE_OFFSET(2)(CSP)
    ld fpr3,JIT_FPR_SAVE_OFFSET(3)(CSP)
    ld fpr5,JIT_FPR_SAVE_OFFSET(5)(CSP)
    ld fpr7,JIT_FPR_SAVE_OFFSET(7)(CSP)
    ld fpr8,JIT_FPR_SAVE_OFFSET(8)(CSP)
    ld fpr9,JIT_FPR_SAVE_OFFSET(9)(CSP)
    ld fpr10,JIT_FPR_SAVE_OFFSET(10)(CSP)
    ld fpr11,JIT_FPR_SAVE_OFFSET(11)(CSP)
    ld fpr12,JIT_FPR_SAVE_OFFSET(12)(CSP)
    ld fpr13,JIT_FPR_SAVE_OFFSET(13)(CSP)
    ld fpr14,JIT_FPR_SAVE_OFFSET(14)(CSP)
    ld fpr15,JIT_FPR_SAVE_OFFSET(15)(CSP)
})

dnl No need to save/restore non-volatile FPRs.
dnl The stack walker will never need to read or
dnl modify them (no preserved FPRs in the JIT
dnl private linkage).
dnl The exception to this is fpr4 and fpr6, which are JIT
dnl argument registers which may need to be seen by the
dnl decompiler.  When vector registers are in use, they
dnl will already have been preserved.
dnl If vector registers are not in use, it's harmless to
dnl preserve them in the FPR save area(which is distinct
dnl from the VR save area).

define({SAVE_C_NONVOLATILE_REGS},{
    std fpr4,JIT_FPR_SAVE_OFFSET(4)(CSP)
    std fpr6,JIT_FPR_SAVE_OFFSET(6)(CSP)
})

define({RESTORE_C_NONVOLATILE_REGS}, )

})

define({UNUSED},{
BEGIN_FUNC($1)
    LHI_GPR CARG1,-1
    ST_GPR CARG1,0(CARG1)
END_CURRENT
})

define({SAVE_C_VOLATILE_REGS},{
    L_GPR r8,J9TR_VMThread_javaVM(J9VMTHREAD)
    l r8,J9TR_JavaVM_extendedRuntimeFlags(r8)
    lhi r9,J9TR_J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS
    nr r9,r8
    jz LABEL_NAME(CONCAT(L_SF,SYM_COUNT))
    SAVE_C_VOLATILE_VRS
    J LABEL_NAME(CONCAT(L_SV,SYM_COUNT))
PLACE_LABEL(CONCAT(L_SF,SYM_COUNT))
    SAVE_C_VOLATILE_FPRS
PLACE_LABEL(CONCAT(L_SV,SYM_COUNT))
})

define({RESTORE_C_VOLATILE_REGS},{
    nr r9,r8
    jz LABEL_NAME(CONCAT(L_RF,SYM_COUNT))
    RESTORE_C_VOLATILE_VRS
    J LABEL_NAME(CONCAT(L_RV,SYM_COUNT))
PLACE_LABEL(CONCAT(L_RF,SYM_COUNT))
    RESTORE_C_VOLATILE_FPRS
PLACE_LABEL(CONCAT(L_RV,SYM_COUNT))
})

define({SAVE_ALL_REGS},{
    SAVE_C_VOLATILE_REGS($1)
    SAVE_C_NONVOLATILE_REGS
})

define({SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS},{
    RELOAD_SSP
    STM_GPR r6,r12,JIT_GPR_SAVE_SLOT(6)
ifdef({ASM_J9VM_JIT_32BIT_USES64BIT_REGISTERS},{
    stmh r6,r12,JIT_GPR_SAVE_SLOT(22)
})
    LOAD_CAA
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
})

dnl Assumes J9SP value has been stored into the JIT GPR slot

define({RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK},{
    STORE_SSP
    RESTORE_C_VOLATILE_REGS($1)
    RESTORE_C_NONVOLATILE_REGS
    RESTORE_HIWORD_REGS
    LM_GPR r0,LAST_REG,JIT_GPR_SAVE_SLOT(0)
})

define({RESTORE_PRESERVED_REGS_AND_SWITCH_TO_JAVA_STACK},{
ifdef({ASM_J9VM_JIT_32BIT_USES64BIT_REGISTERS},{
    lmh r6,r12,JIT_GPR_SAVE_SLOT(22)
})
    LM_GPR r6,r12,JIT_GPR_SAVE_SLOT(6)
    SWITCH_TO_JAVA_STACK
})

define({BRANCH_VIA_VMTHREAD},{
    L_GPR JUMP_REG,$1(J9VMTHREAD)
    br JUMP_REG
})

define({SWITCH_TO_C_STACK},{
    RELOAD_SSP
    LOAD_CAA
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
})

define({SWITCH_TO_JAVA_STACK},{
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    STORE_SSP
})
