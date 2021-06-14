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
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] http://openjdk.java.net/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

include(jilvalues.m4)

J9CONST({CINTERP_STACK_SIZE},J9TR_cframe_sizeof)
ifelse(eval(CINTERP_STACK_SIZE % 16),0,,{ERROR stack size CINTERP_STACK_SIZE is not 16-aligned})

define({M},{$2{(}$1{)}})


define({ALen},{8})

define({J9VMTHREAD},{s10})
define({J9SP},{s11})

define({FUNC_LABEL},{$1})

define({DECLARE_PUBLIC},{
    .globl FUNC_LABEL($1)
    .type FUNC_LABEL($1),function
})

define({DECLARE_EXTERN},{.extern $1})

define({START_PROC},{
    .text
    DECLARE_PUBLIC($1)
    .align 2
FUNC_LABEL($1):
})

define({END_PROC})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)})

define({CALL_DIRECT},{jal BRANCH_SYMBOL($1)})

define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+(($1)*ALen))})
define({GPR_SAVE_SLOT},{M(sp, GPR_SAVE_OFFSET($1))})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+(($1-8)*8))})
define({FPR_SAVE_SLOT},{M(sp, FPR_SAVE_OFFSET($1))})

define({JIT_GPR_SAVE_OFFSET},{eval(J9TR_cframe_jitGPRs+(($1)*ALen))})
define({JIT_GPR_SAVE_SLOT},{M(sp, JIT_GPR_SAVE_OFFSET($1))})
define({JIT_FPR_SAVE_OFFSET},{eval(J9TR_cframe_jitFPRs+(($1)*8))})
define({JIT_FPR_SAVE_SLOT},{M(sp, JIT_FPR_SAVE_OFFSET($1))})

define({SAVE_FPLR},{
    sd fp, JIT_GPR_SAVE_SLOT(8)
    sd ra, JIT_GPR_SAVE_SLOT(1)
})

define({RESTORE_FPLR},{
    ld fp, JIT_GPR_SAVE_SLOT(8)
    ld ra, JIT_GPR_SAVE_SLOT(1)    
})

define({BEGIN_HELPER},{
    START_PROC($1)
    SAVE_FPLR
})

define({END_HELPER},{
    jr ra
    END_PROC($1)
})

define({CALL_C_WITH_VMTHREAD},{
    mv a0, J9VMTHREAD
    CALL_DIRECT($1)
})

define({SAVE_C_VOLATILE_REGS},{
    sd  ra, JIT_GPR_SAVE_SLOT(1)

    sd  t0,  JIT_GPR_SAVE_SLOT(5)
    sd  t1,  JIT_GPR_SAVE_SLOT(6)
    sd  t2,  JIT_GPR_SAVE_SLOT(7)

    sd  a0,  JIT_GPR_SAVE_SLOT(10)
    sd  a1,  JIT_GPR_SAVE_SLOT(11)
    sd  a2,  JIT_GPR_SAVE_SLOT(12)
    sd  a3,  JIT_GPR_SAVE_SLOT(13)
    sd  a4,  JIT_GPR_SAVE_SLOT(14)
    sd  a5,  JIT_GPR_SAVE_SLOT(15)
    sd  a6,  JIT_GPR_SAVE_SLOT(16)
    sd  a7,  JIT_GPR_SAVE_SLOT(17)
    
    sd  t3,  JIT_GPR_SAVE_SLOT(28)
    sd  t4,  JIT_GPR_SAVE_SLOT(29)
    sd  t6,  JIT_GPR_SAVE_SLOT(30)
    sd  t6,  JIT_GPR_SAVE_SLOT(31)

    fsd ft0, JIT_FPR_SAVE_SLOT(0)
    fsd ft1, JIT_FPR_SAVE_SLOT(1)
    fsd ft2, JIT_FPR_SAVE_SLOT(2)
    fsd ft3, JIT_FPR_SAVE_SLOT(3)
    fsd ft4, JIT_FPR_SAVE_SLOT(4)
    fsd ft5, JIT_FPR_SAVE_SLOT(5)
    fsd ft6, JIT_FPR_SAVE_SLOT(6)
    fsd ft7, JIT_FPR_SAVE_SLOT(7)

    fsd fa0, JIT_FPR_SAVE_SLOT(10)
    fsd fa1, JIT_FPR_SAVE_SLOT(11)
    fsd fa2, JIT_FPR_SAVE_SLOT(12)
    fsd fa3, JIT_FPR_SAVE_SLOT(13)
    fsd fa4, JIT_FPR_SAVE_SLOT(14)
    fsd fa5, JIT_FPR_SAVE_SLOT(15)
    fsd fa6, JIT_FPR_SAVE_SLOT(16)
    fsd fa7, JIT_FPR_SAVE_SLOT(17)

    fsd ft8, JIT_FPR_SAVE_SLOT(28)
    fsd ft9, JIT_FPR_SAVE_SLOT(29)
    fsd ft10,JIT_FPR_SAVE_SLOT(30)
    fsd ft11,JIT_FPR_SAVE_SLOT(31)
})

define({RESTORE_C_VOLATILE_REGS},{
    ld  ra, JIT_GPR_SAVE_SLOT(1)

    ld  t0,  JIT_GPR_SAVE_SLOT(5)
    ld  t1,  JIT_GPR_SAVE_SLOT(6)
    ld  t2,  JIT_GPR_SAVE_SLOT(7)

    ld  a0,  JIT_GPR_SAVE_SLOT(10)
    ld  a1,  JIT_GPR_SAVE_SLOT(11)
    ld  a2,  JIT_GPR_SAVE_SLOT(12)
    ld  a3,  JIT_GPR_SAVE_SLOT(13)
    ld  a4,  JIT_GPR_SAVE_SLOT(14)
    ld  a5,  JIT_GPR_SAVE_SLOT(15)
    ld  a6,  JIT_GPR_SAVE_SLOT(16)
    ld  a7,  JIT_GPR_SAVE_SLOT(17)
    
    ld  t3,  JIT_GPR_SAVE_SLOT(28)
    ld  t4,  JIT_GPR_SAVE_SLOT(29)
    ld  t6,  JIT_GPR_SAVE_SLOT(30)
    ld  t6,  JIT_GPR_SAVE_SLOT(31)

    fld ft0, JIT_FPR_SAVE_SLOT(0)
    fld ft1, JIT_FPR_SAVE_SLOT(1)
    fld ft2, JIT_FPR_SAVE_SLOT(2)
    fld ft3, JIT_FPR_SAVE_SLOT(3)
    fld ft4, JIT_FPR_SAVE_SLOT(4)
    fld ft5, JIT_FPR_SAVE_SLOT(5)
    fld ft6, JIT_FPR_SAVE_SLOT(6)
    fld ft7, JIT_FPR_SAVE_SLOT(7)

    fld fa0, JIT_FPR_SAVE_SLOT(10)
    fld fa1, JIT_FPR_SAVE_SLOT(11)
    fld fa2, JIT_FPR_SAVE_SLOT(12)
    fld fa3, JIT_FPR_SAVE_SLOT(13)
    fld fa4, JIT_FPR_SAVE_SLOT(14)
    fld fa5, JIT_FPR_SAVE_SLOT(15)
    fld fa6, JIT_FPR_SAVE_SLOT(16)
    fld fa7, JIT_FPR_SAVE_SLOT(17)

    fld ft8, JIT_FPR_SAVE_SLOT(28)
    fld ft9, JIT_FPR_SAVE_SLOT(29)
    fld ft10,JIT_FPR_SAVE_SLOT(30)
    fld ft11,JIT_FPR_SAVE_SLOT(31)

})

dnl No need to save/restore fs0-fs11 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).
dnl

define({SAVE_C_NONVOLATILE_REGS},{
    sd s0,  JIT_GPR_SAVE_SLOT(8)
    sd s1,  JIT_GPR_SAVE_SLOT(9)

    sd s2,  JIT_GPR_SAVE_SLOT(18)
    sd s3,  JIT_GPR_SAVE_SLOT(19)
    sd s4,  JIT_GPR_SAVE_SLOT(20)
    sd s5,  JIT_GPR_SAVE_SLOT(21)
    sd s6,  JIT_GPR_SAVE_SLOT(22)
    sd s7,  JIT_GPR_SAVE_SLOT(23)
    sd s8,  JIT_GPR_SAVE_SLOT(24)
    sd s9,  JIT_GPR_SAVE_SLOT(25)
    sd s10, JIT_GPR_SAVE_SLOT(26)
    sd s11, JIT_GPR_SAVE_SLOT(27)
})

define({RESTORE_C_NONVOLATILE_REGS},{
    ld s0,  JIT_GPR_SAVE_SLOT(8)
    ld s1,  JIT_GPR_SAVE_SLOT(9)

    ld s2,  JIT_GPR_SAVE_SLOT(18)
    ld s3,  JIT_GPR_SAVE_SLOT(19)
    ld s4,  JIT_GPR_SAVE_SLOT(20)
    ld s5,  JIT_GPR_SAVE_SLOT(21)
    ld s6,  JIT_GPR_SAVE_SLOT(22)
    ld s7,  JIT_GPR_SAVE_SLOT(23)
    ld s8,  JIT_GPR_SAVE_SLOT(24)
    ld s9,  JIT_GPR_SAVE_SLOT(25)
    ld s10, JIT_GPR_SAVE_SLOT(26)
    ld s11, JIT_GPR_SAVE_SLOT(27)
})

define({SAVE_ALL_REGS},{
    SAVE_C_VOLATILE_REGS
    SAVE_C_NONVOLATILE_REGS
})

define({RESTORE_ALL_REGS},{
    RESTORE_C_VOLATILE_REGS
    RESTORE_C_NONVOLATILE_REGS
})

dnl Note, that s10 (vmthread) & s11 (java sp) are not 
dnl saved / restored

define({SAVE_PRESERVED_REGS},{
    sd s0,  JIT_GPR_SAVE_SLOT(1)
    sd s1,  JIT_GPR_SAVE_SLOT(2)

    sd s2,  JIT_GPR_SAVE_SLOT(3)
    sd s3,  JIT_GPR_SAVE_SLOT(4)
    sd s4,  JIT_GPR_SAVE_SLOT(5)
    sd s5,  JIT_GPR_SAVE_SLOT(6)
    sd s6,  JIT_GPR_SAVE_SLOT(7)
    sd s7,  JIT_GPR_SAVE_SLOT(8)
    sd s8,  JIT_GPR_SAVE_SLOT(9)
    sd s9,  JIT_GPR_SAVE_SLOT(10)
})

define({RESTORE_PRESERVED_REGS},{
    ld s0,  JIT_GPR_SAVE_SLOT(1)
    ld s1,  JIT_GPR_SAVE_SLOT(2)

    ld s2,  JIT_GPR_SAVE_SLOT(3)
    ld s3,  JIT_GPR_SAVE_SLOT(4)
    ld s4,  JIT_GPR_SAVE_SLOT(5)
    ld s5,  JIT_GPR_SAVE_SLOT(6)
    ld s6,  JIT_GPR_SAVE_SLOT(7)
    ld s7,  JIT_GPR_SAVE_SLOT(8)
    ld s8,  JIT_GPR_SAVE_SLOT(9)
    ld s9,  JIT_GPR_SAVE_SLOT(10)
})

define({BRANCH_VIA_VMTHREAD},{
    ld t2, M(J9VMTHREAD, $1)
    jalr zero, t2, 0
})

define({SWITCH_TO_JAVA_STACK},{
    ld J9SP,M(J9VMTHREAD, J9TR_VMThread_sp)
})

define({SWITCH_TO_C_STACK},{
    sd J9SP,M(J9VMTHREAD, J9TR_VMThread_sp)
})
