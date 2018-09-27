dnl Copyright (c) 1991, 2018 IBM Corp. and others
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
define({r16},16)
define({r17},17)
define({r18},18)
define({r19},19)
define({r20},20)
define({r21},21)
define({r22},22)
define({r23},23)
define({r24},24)
define({r25},25)
define({r26},26)
define({r27},27)
define({r28},28)
define({r29},29)
define({r30},30)
define({r31},31)

define({fp0},0)
define({fp1},1)
define({fp2},2)
define({fp3},3)
define({fp4},4)
define({fp5},5)
define({fp6},6)
define({fp7},7)
define({fp8},8)
define({fp9},9)
define({fp10},10)
define({fp11},11)
define({fp12},12)
define({fp13},13)
define({fp14},14)
define({fp15},15)
define({fp16},16)
define({fp17},17)
define({fp18},18)
define({fp19},19)
define({fp20},20)
define({fp21},21)
define({fp22},22)
define({fp23},23)
define({fp24},24)
define({fp25},25)
define({fp26},26)
define({fp27},27)
define({fp28},28)
define({fp29},29)
define({fp30},30)
define({fp31},31)

define({CINTERP_STACK_SIZE},{J9CONST(J9TR_cframe_sizeof,$1,$2)})

ifdef({ASM_J9VM_ENV_DATA64},{

ifelse(eval(CINTERP_STACK_SIZE % 64),0, ,{ERROR stack size CINTERP_STACK_SIZE is not 64-aligned})

define({laddr},ld)
define({laddrx},ldx)
define({laddru},ldu)
define({staddr},std)
define({staddrx},stdx)
define({staddru},stdu)
define({cmpliaddr},{cmpli 0,1,})
define({ADDR},.llong)
define({ALen},{J9CONST(8,$1,$2)})
define({J9VMTHREAD},{r15})

},{ dnl ASM_J9VM_ENV_DATA64

ifelse(eval(CINTERP_STACK_SIZE % 16),0, ,{ERROR stack size CINTERP_STACK_SIZE is not 16-aligned})

define({laddr},lwz)
define({laddrx},lwzx)
define({laddru},lwzu)
define({staddr},stw)
define({staddrx},stwx)
define({staddru},stwu)
define({cmpliaddr},{cmpli 0,0,})
define({ADDR},.long)
define({ALen},{J9CONST(4,$1,$2)})     

define({J9VMTHREAD},{r13})

}) dnl ASM_J9VM_ENV_DATA64

define({J9SP},r14)
define({J9PC},r16)
define({J9LITERALS},r17)
define({J9A0},r21)

ifdef({AIXPPC},{

dnl AIX PPC 32 and 64

define({FUNC_PTR},{r11})

define({CALL_INDIRECT},{
	ifelse($1,,,{mr FUNC_PTR,$1})
dnl inline version of _ptrgl follows
	laddr r2,0(FUNC_PTR)
	mtctr r2
	laddr r2,ALen(FUNC_PTR)
	bctrl
})

define({FUNC_LABEL},{.$1})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)[pr]})

define({DECLARE_PUBLIC},{
	.globl .$1[pr]
	.globl $1[ds]
	.globl .$1
	.toc
	TOC.$1: .tc .$1[tc],$1[ds]
	.csect $1[ds]
	ADDR .$1[pr]
	ADDR  TOC[tc0]
	ADDR  0
})

define({DECLARE_EXTERN},{
	.extern .$1[pr]
	.extern $1[ds]
	.toc
	TOC.$1: .tc .$1[tc],$1[ds]
})

define({START_TEXT},{
	.csect .$1[pr]
})

define({START_PROC},{
	DECLARE_PUBLIC($1)
	START_TEXT(CSECT_NAME)
	.$1:
	.function .$1,startproc.$1,16,0,(endproc.$1-startproc.$1)
	startproc.$1:
})

define({END_PROC},{endproc.$1:})

define({CALL_DIRECT},{
	bl BRANCH_SYMBOL($1)
	cror 31,31,31
})

define({TOC_SAVE_OFFSET},{J9CONST(J9TR_cframe_currentTOC,$1,$2)})
define({SAVE_R13})
define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+((($1)-13)*ALen))})
define({GPR_SAVE_SLOT},{GPR_SAVE_OFFSET($1)(r1)})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+((($1)-14)*8))})
define({FPR_SAVE_SLOT},{FPR_SAVE_OFFSET($1)(r1)})
define({CR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedCR),$1,$2)})
define({LR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedLR),$1,$2)})

},{ dnl AIXPPC

ifdef({ASM_J9VM_ENV_DATA64},{

ifdef({ASM_J9VM_ENV_LITTLE_ENDIAN},{

dnl Linux PPC 64 LE

define({FUNC_PTR},{r12})

define({CALL_INDIRECT},{
	ifelse($1,,,{mr FUNC_PTR,$1})
	mtctr FUNC_PTR
	bctrl
})

define({FUNC_LABEL},{$1})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)})

define({DECLARE_PUBLIC},{
	.globl $1
	.type $1,@function
})

define({DECLARE_EXTERN})

define({START_TEXT},{
	.section ".text"
	.align 3
})

define({CALL_DIRECT},{
	bl BRANCH_SYMBOL($1)
	ori r0,r0,0
})

define({START_PROC},{
	DECLARE_PUBLIC($1)
	START_TEXT(CSECT_NAME)
	FUNC_LABEL($1):
	.localentry $1,.-$1
})

define({END_PROC})

define({TOC_SAVE_OFFSET},{J9CONST(J9TR_cframe_currentTOC,$1,$2)})
define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+((($1)-14)*ALen))})
define({GPR_SAVE_SLOT},{GPR_SAVE_OFFSET($1)(r1)})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+((($1)-14)*8))})
define({FPR_SAVE_SLOT},{FPR_SAVE_OFFSET($1)(r1)})
define({CR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedCR),$1,$2)})
define({LR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedLR),$1,$2)})

},{ dnl ASM_J9VM_ENV_LITTLE_ENDIAN

dnl Linux PPC 64 BE

define({FUNC_PTR},{r11})

define({CALL_INDIRECT},{
	ifelse($1,,,{mr FUNC_PTR,$1})
dnl inline version of _ptrgl follows
	ld r2,0(FUNC_PTR)
	mtctr r2
	ld r2,8(FUNC_PTR)
	bctrl
})

define({FUNC_LABEL},{.$1})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)})

define({DECLARE_PUBLIC},{
	.globl .$1
	.section ".toc","wa"
	.align 3
	.tocL_$1:
	.tc $1[TC],$1
	.section ".opd","wa"
	.type .$1,@function
	.globl $1
	.size $1,24
	$1:
	.quad .$1
	.quad .TOC.@tocbase
	.long 0x00000000
	.long 0x00000000
})

define({START_TEXT},{
	.section ".text"
	.align 3
})

define({DECLARE_EXTERN},{
	.section".toc","wa"
	.align 3
	.tocL_$1:
	.tc $1[TC],$1
})

define({START_PROC},{
	DECLARE_PUBLIC($1)
	START_TEXT(CSECT_NAME)
	FUNC_LABEL($1):
})

define({END_PROC})

define({TOC_SAVE_OFFSET},{J9CONST(J9TR_cframe_currentTOC,$1,$2)})
define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+((($1)-14)*ALen))})
define({GPR_SAVE_SLOT},{GPR_SAVE_OFFSET($1)(r1)})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+((($1)-14)*8))})
define({FPR_SAVE_SLOT},{FPR_SAVE_OFFSET($1)(r1)})
define({CR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedCR),$1,$2)})
define({LR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedLR),$1,$2)})

define({CALL_DIRECT},{
	ld FUNC_PTR, .tocL_$1@toc(r2)
	CALL_INDIRECT
})

}) dnl ASM_J9VM_ENV_LITTLE_ENDIAN

},{ dnl ASM_J9VM_ENV_DATA64

dnl Linux PPC 32

define({FUNC_PTR},{r11})

define({CALL_INDIRECT},{
	ifelse($1,,{mtctr FUNC_PTR},{mtctr $1})
	bctrl
})

define({FUNC_LABEL},{$1})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)@plt})

define({DECLARE_PUBLIC},{.globl $1})

define({DECLARE_EXTERN},{.extern $1})

define({START_TEXT})

define({CALL_DIRECT},{bl BRANCH_SYMBOL($1)})

define({START_PROC},{
	DECLARE_PUBLIC($1)
	START_TEXT(CSECT_NAME)
	FUNC_LABEL($1):
})

define({END_PROC})

define({INIT_JIT_TOC},{
	bl _GLOBAL_OFFSET_TABLE_@local-4
	mflr r29
})

define({SAVE_JIT_GOT_REGISTERS},{staddr r29,JIT_GPR_SAVE_SLOT(29)})
define({RESTORE_JIT_GOT_REGISTERS},{laddr r29,JIT_GPR_SAVE_SLOT(29)})
define({SAVE_R2_FOR_ALL},{staddr r2,JIT_GPR_SAVE_SLOT(2)})
define({RESTORE_R2_FOR_ALL},{laddr r2,JIT_GPR_SAVE_SLOT(2)})
define({SAVE_R29_FOR_ALL})
define({RESTORE_R29_FOR_ALL})

define({SAVE_R13})
define({CR_SAVE_OFFSET},{J9CONST(J9TR_cframe_preservedCR,$1,$2)})
define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+((($1)-13)*ALen))})
define({GPR_SAVE_SLOT},{GPR_SAVE_OFFSET($1)(r1)})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+((($1)-14)*8))})
define({FPR_SAVE_SLOT},{FPR_SAVE_OFFSET($1)(r1)})
define({LR_SAVE_OFFSET},{J9CONST(eval(CINTERP_STACK_SIZE+J9TR_cframe_preservedLR),$1,$2)})

}) dnl ASM_J9VM_ENV_DATA64
}) dnl AIXPPC

ifdef({INIT_JIT_TOC},,{
define({INIT_JIT_TOC},{
	laddr r2,J9TR_VMThread_jitTOC(J9VMTHREAD)
})
}) dnl !INIT_JIT_TOC

define({SAVE_GPR},{staddr r$1,GPR_SAVE_SLOT($1)})
define({RESTORE_GPR},{laddr r$1,GPR_SAVE_SLOT($1)})
define({SAVE_FPR},{stfd fp$1,FPR_SAVE_SLOT($1)})
define({RESTORE_FPR},{lfd fp$1,FPR_SAVE_SLOT($1)})

define({JIT_GPR_SAVE_OFFSET},{eval(J9TR_cframe_jitGPRs+(($1)*ALen))})
define({JIT_GPR_SAVE_SLOT},{JIT_GPR_SAVE_OFFSET($1)(r1)})
define({JIT_FPR_SAVE_OFFSET},{eval(J9TR_cframe_jitFPRs+(($1)*8))})
define({JIT_FPR_SAVE_SLOT},{JIT_FPR_SAVE_OFFSET($1)(r1)})
define({JIT_CR_SAVE_OFFSET},{J9CONST(J9TR_cframe_jitCR,$1,$2)})
define({JIT_CR_SAVE_SLOT},{JIT_CR_SAVE_OFFSET(r1)})
define({JIT_LR_SAVE_OFFSET},{J9CONST(J9TR_cframe_jitLR,$1,$2)})
define({JIT_LR_SAVE_SLOT},{JIT_LR_SAVE_OFFSET(r1)})

define({SAVE_LR},{
	mflr r0
	staddr r0,JIT_LR_SAVE_SLOT
})

define({SAVE_CR},{
	mfcr r0
	staddr r0,JIT_CR_SAVE_SLOT
})

define({RESTORE_LR},{
	laddr r0,JIT_LR_SAVE_SLOT
	mtlr r0
})

define({RESTORE_CR},{
	laddr r0,JIT_CR_SAVE_SLOT
	mtcr r0
})

ifdef({ASM_J9VM_ENV_DATA64},{

define({SAVE_R13},{staddr r13,JIT_GPR_SAVE_SLOT(13)})
define({RESTORE_R13},{laddr r13,JIT_GPR_SAVE_SLOT(13)})
define({SAVE_R15})
define({RESTORE_R15})

},{

define({SAVE_R13})
define({RESTORE_R13})
define({SAVE_R15},{staddr r15,JIT_GPR_SAVE_SLOT(15)})
define({RESTORE_R15},{laddr r15,JIT_GPR_SAVE_SLOT(15)})

})

ifdef({SAVE_JIT_GOT_REGISTERS},,{

dnl All AIX and Linux 64-bit use TOC in r2

define({SAVE_JIT_GOT_REGISTERS},{staddr r2,JIT_GPR_SAVE_SLOT(2)})
define({RESTORE_JIT_GOT_REGISTERS},{laddr r2,JIT_GPR_SAVE_SLOT(2)})
define({SAVE_R2_FOR_ALL})
define({RESTORE_R2_FOR_ALL})
define({SAVE_R29_FOR_ALL},{staddr r29,JIT_GPR_SAVE_SLOT(29)})
define({RESTORE_R29_FOR_ALL},{laddr r29,JIT_GPR_SAVE_SLOT(29)})

})

define({CALL_C_WITH_VMTHREAD},{
	mr r3,J9VMTHREAD
	CALL_DIRECT($1)
})

define({SAVE_C_VOLATILE_REGS},{
	SAVE_CR
	SAVE_R2_FOR_ALL
	staddr r3,JIT_GPR_SAVE_SLOT(3)
	staddr r4,JIT_GPR_SAVE_SLOT(4)
	staddr r5,JIT_GPR_SAVE_SLOT(5)
	staddr r6,JIT_GPR_SAVE_SLOT(6)
	staddr r7,JIT_GPR_SAVE_SLOT(7)
	staddr r8,JIT_GPR_SAVE_SLOT(8)
	staddr r9,JIT_GPR_SAVE_SLOT(9)
	staddr r10,JIT_GPR_SAVE_SLOT(10)
	staddr r11,JIT_GPR_SAVE_SLOT(11)
	staddr r12,JIT_GPR_SAVE_SLOT(12)
	stfd fp0,JIT_FPR_SAVE_SLOT(0)
	stfd fp1,JIT_FPR_SAVE_SLOT(1)
	stfd fp2,JIT_FPR_SAVE_SLOT(2)
	stfd fp3,JIT_FPR_SAVE_SLOT(3)
	stfd fp4,JIT_FPR_SAVE_SLOT(4)
	stfd fp5,JIT_FPR_SAVE_SLOT(5)
	stfd fp6,JIT_FPR_SAVE_SLOT(6)
	stfd fp7,JIT_FPR_SAVE_SLOT(7)
	stfd fp8,JIT_FPR_SAVE_SLOT(8)
	stfd fp9,JIT_FPR_SAVE_SLOT(9)
	stfd fp10,JIT_FPR_SAVE_SLOT(10)
	stfd fp11,JIT_FPR_SAVE_SLOT(11)
	stfd fp12,JIT_FPR_SAVE_SLOT(12)
	stfd fp13,JIT_FPR_SAVE_SLOT(13)
})

define({RESTORE_C_VOLATILE_REGS},{
	RESTORE_CR
	RESTORE_R2_FOR_ALL
	laddr r3,JIT_GPR_SAVE_SLOT(3)
	laddr r4,JIT_GPR_SAVE_SLOT(4)
	laddr r5,JIT_GPR_SAVE_SLOT(5)
	laddr r6,JIT_GPR_SAVE_SLOT(6)
	laddr r7,JIT_GPR_SAVE_SLOT(7)
	laddr r8,JIT_GPR_SAVE_SLOT(8)
	laddr r9,JIT_GPR_SAVE_SLOT(9)
	laddr r10,JIT_GPR_SAVE_SLOT(10)
	laddr r11,JIT_GPR_SAVE_SLOT(11)
	laddr r12,JIT_GPR_SAVE_SLOT(12)
	lfd fp0,JIT_FPR_SAVE_SLOT(0)
	lfd fp1,JIT_FPR_SAVE_SLOT(1)
	lfd fp2,JIT_FPR_SAVE_SLOT(2)
	lfd fp3,JIT_FPR_SAVE_SLOT(3)
	lfd fp4,JIT_FPR_SAVE_SLOT(4)
	lfd fp5,JIT_FPR_SAVE_SLOT(5)
	lfd fp6,JIT_FPR_SAVE_SLOT(6)
	lfd fp7,JIT_FPR_SAVE_SLOT(7)
	lfd fp8,JIT_FPR_SAVE_SLOT(8)
	lfd fp9,JIT_FPR_SAVE_SLOT(9)
	lfd fp10,JIT_FPR_SAVE_SLOT(10)
	lfd fp11,JIT_FPR_SAVE_SLOT(11)
	lfd fp12,JIT_FPR_SAVE_SLOT(12)
	lfd fp13,JIT_FPR_SAVE_SLOT(13)
})

dnl No need to save/restore fp14-31 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).

define({SAVE_C_NONVOLATILE_REGS},{
	SAVE_R13
	SAVE_R15
	staddr r16,JIT_GPR_SAVE_SLOT(16)
	staddr r17,JIT_GPR_SAVE_SLOT(17)
	staddr r18,JIT_GPR_SAVE_SLOT(18)
	staddr r19,JIT_GPR_SAVE_SLOT(19)
	staddr r20,JIT_GPR_SAVE_SLOT(20)
	staddr r21,JIT_GPR_SAVE_SLOT(21)
	staddr r22,JIT_GPR_SAVE_SLOT(22)
	staddr r23,JIT_GPR_SAVE_SLOT(23)
	staddr r24,JIT_GPR_SAVE_SLOT(24)
	staddr r25,JIT_GPR_SAVE_SLOT(25)
	staddr r26,JIT_GPR_SAVE_SLOT(26)
	staddr r27,JIT_GPR_SAVE_SLOT(27)
	staddr r28,JIT_GPR_SAVE_SLOT(28)
	SAVE_R29_FOR_ALL
	staddr r30,JIT_GPR_SAVE_SLOT(30)
	staddr r31,JIT_GPR_SAVE_SLOT(31)
})

define({RESTORE_C_NONVOLATILE_REGS},{
	RESTORE_R13
	RESTORE_R15
	laddr r16,JIT_GPR_SAVE_SLOT(16)
	laddr r17,JIT_GPR_SAVE_SLOT(17)
	laddr r18,JIT_GPR_SAVE_SLOT(18)
	laddr r19,JIT_GPR_SAVE_SLOT(19)
	laddr r20,JIT_GPR_SAVE_SLOT(20)
	laddr r21,JIT_GPR_SAVE_SLOT(21)
	laddr r22,JIT_GPR_SAVE_SLOT(22)
	laddr r23,JIT_GPR_SAVE_SLOT(23)
	laddr r24,JIT_GPR_SAVE_SLOT(24)
	laddr r25,JIT_GPR_SAVE_SLOT(25)
	laddr r26,JIT_GPR_SAVE_SLOT(26)
	laddr r27,JIT_GPR_SAVE_SLOT(27)
	laddr r28,JIT_GPR_SAVE_SLOT(28)
	RESTORE_R29_FOR_ALL
	laddr r30,JIT_GPR_SAVE_SLOT(30)
	laddr r31,JIT_GPR_SAVE_SLOT(31)
})

define({SAVE_ALL_REGS},{
	SAVE_C_VOLATILE_REGS
	SAVE_C_NONVOLATILE_REGS
})

define({SAVE_PRESERVED_REGS},{
	SAVE_R15
	staddr r16,JIT_GPR_SAVE_SLOT(16)
	staddr r17,JIT_GPR_SAVE_SLOT(17)
	staddr r18,JIT_GPR_SAVE_SLOT(18)
	staddr r19,JIT_GPR_SAVE_SLOT(19)
	staddr r20,JIT_GPR_SAVE_SLOT(20)
	staddr r21,JIT_GPR_SAVE_SLOT(21)
	staddr r22,JIT_GPR_SAVE_SLOT(22)
	staddr r23,JIT_GPR_SAVE_SLOT(23)
	staddr r24,JIT_GPR_SAVE_SLOT(24)
	staddr r25,JIT_GPR_SAVE_SLOT(25)
	staddr r26,JIT_GPR_SAVE_SLOT(26)
	staddr r27,JIT_GPR_SAVE_SLOT(27)
	staddr r28,JIT_GPR_SAVE_SLOT(28)
	staddr r29,JIT_GPR_SAVE_SLOT(29)
	staddr r30,JIT_GPR_SAVE_SLOT(30)
	staddr r31,JIT_GPR_SAVE_SLOT(31)
})

define({RESTORE_ALL_REGS},{
	RESTORE_C_VOLATILE_REGS
	RESTORE_C_NONVOLATILE_REGS
})

define({RESTORE_PRESERVED_REGS},{
	RESTORE_R15
	laddr r16,JIT_GPR_SAVE_SLOT(16)
	laddr r17,JIT_GPR_SAVE_SLOT(17)
	laddr r18,JIT_GPR_SAVE_SLOT(18)
	laddr r19,JIT_GPR_SAVE_SLOT(19)
	laddr r20,JIT_GPR_SAVE_SLOT(20)
	laddr r21,JIT_GPR_SAVE_SLOT(21)
	laddr r22,JIT_GPR_SAVE_SLOT(22)
	laddr r23,JIT_GPR_SAVE_SLOT(23)
	laddr r24,JIT_GPR_SAVE_SLOT(24)
	laddr r25,JIT_GPR_SAVE_SLOT(25)
	laddr r26,JIT_GPR_SAVE_SLOT(26)
	laddr r27,JIT_GPR_SAVE_SLOT(27)
	laddr r28,JIT_GPR_SAVE_SLOT(28)
	laddr r29,JIT_GPR_SAVE_SLOT(29)
	laddr r30,JIT_GPR_SAVE_SLOT(30)
	laddr r31,JIT_GPR_SAVE_SLOT(31)
})

define({BRANCH_VIA_VMTHREAD},{
	laddr r0,$1(J9VMTHREAD)
	mtctr r0
	bctr
})

define({SWITCH_TO_JAVA_STACK},{laddr J9SP,J9TR_VMThread_sp(J9VMTHREAD)})
define({SWITCH_TO_C_STACK},{staddr J9SP,J9TR_VMThread_sp(J9VMTHREAD)})
