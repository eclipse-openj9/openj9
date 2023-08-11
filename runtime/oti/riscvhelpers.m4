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
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] https://openjdk.org/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0

.cfi_sections .eh_frame, .debug_frame

include(jilvalues.m4)

J9CONST({CINTERP_STACK_SIZE},J9TR_cframe_sizeof)
ifelse(eval(CINTERP_STACK_SIZE % 16),0,,{ERROR stack size CINTERP_STACK_SIZE is not 16-aligned})

define({M},{$2{(}$1{)}})

define({ALen},{8})

dnl
dnl See J9::RV::PrivateLinkageProperties::PrivateLinkageProperties() in RVPrivateLinkage.cpp
dnl Following definitions must be kept in sync with private linkage.
dnl
define({J9VMTHREAD},{s10})
define({J9SP},{s11})
define({J9VTABLEINDEX},{t3})

define({FUNC_LABEL},{$1})

define({DECLARE_PUBLIC},{
    .globl FUNC_LABEL($1)
    .type FUNC_LABEL($1),function})

define({DECLARE_EXTERN},{.extern FUNC_LABEL($1)})

define({START_PROC},{
    .text
    .globl FUNC_LABEL($1)
    .type FUNC_LABEL($1),function
    .align 2
FUNC_LABEL($1):
    .cfi_startproc
    pushdef({CURRENT_PROC},{$1})})

define({END_PROC},{
    .cfi_endproc
    .size   FUNC_LABEL(CURRENT_PROC), .-FUNC_LABEL(CURRENT_PROC)
    popdef({CURRENT_PROC})})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)})

define({CALL_DIRECT},{jal BRANCH_SYMBOL($1)})

define({GPR_NUMBER_zero}, 0)
define({GPR_NUMBER_ra},   1)
define({GPR_NUMBER_sp},   2)
define({GPR_NUMBER_gp},   3)
define({GPR_NUMBER_tp},   4)
define({GPR_NUMBER_t0},   5)

define({GPR_NUMBER_t1},   6)
define({GPR_NUMBER_t2},   7)

define({GPR_NUMBER_s0},   8)
define({GPR_NUMBER_s1},   9)

define({GPR_NUMBER_a0},  10)
define({GPR_NUMBER_a1},  11)
define({GPR_NUMBER_a2},  12)
define({GPR_NUMBER_a3},  13)
define({GPR_NUMBER_a4},  14)
define({GPR_NUMBER_a5},  15)
define({GPR_NUMBER_a6},  16)
define({GPR_NUMBER_a7},  17)

define({GPR_NUMBER_s2},  18)
define({GPR_NUMBER_s3},  19)
define({GPR_NUMBER_s4},  20)
define({GPR_NUMBER_s5},  21)
define({GPR_NUMBER_s6},  22)
define({GPR_NUMBER_s7},  23)
define({GPR_NUMBER_s8},  24)
define({GPR_NUMBER_s9},  25)
define({GPR_NUMBER_s10}, 26)
define({GPR_NUMBER_s11}, 27)

define({GPR_NUMBER_t3},  28)
define({GPR_NUMBER_t4},  29)
define({GPR_NUMBER_t5},  30)
define({GPR_NUMBER_t6},  31)

define({FPR_NUMBER_ft0},  0)
define({FPR_NUMBER_ft1},  1)
define({FPR_NUMBER_ft2},  2)
define({FPR_NUMBER_ft3},  3)
define({FPR_NUMBER_ft4},  4)
define({FPR_NUMBER_ft5},  5)
define({FPR_NUMBER_ft6},  6)
define({FPR_NUMBER_ft7},  7)

define({FPR_NUMBER_fs0},  8)
define({FPR_NUMBER_fs1},  9)

define({FPR_NUMBER_fa0}, 10)
define({FPR_NUMBER_fa1}, 11)
define({FPR_NUMBER_fa2}, 12)
define({FPR_NUMBER_fa3}, 13)
define({FPR_NUMBER_fa4}, 14)
define({FPR_NUMBER_fa5}, 15)
define({FPR_NUMBER_fa6}, 16)
define({FPR_NUMBER_fa7}, 17)

define({FPR_NUMBER_fs2}, 18)
define({FPR_NUMBER_fs3}, 19)
define({FPR_NUMBER_fs4}, 20)
define({FPR_NUMBER_fs5}, 21)
define({FPR_NUMBER_fs6}, 22)
define({FPR_NUMBER_fs7}, 23)
define({FPR_NUMBER_fs8}, 24)
define({FPR_NUMBER_fs9}, 25)
define({FPR_NUMBER_fs10},26)
define({FPR_NUMBER_fs11},27)

define({FPR_NUMBER_ft8}, 28)
define({FPR_NUMBER_ft9}, 29)
define({FPR_NUMBER_ft10},30)
define({FPR_NUMBER_ft11},31)

define({GPR_NUMBER},{GPR_NUMBER_$1})
define({FPR_NUMBER},{FPR_NUMBER_$1})

define({GPR_SAVE_INDEX},{
    ifelse(eval(GPR_NUMBER_$1 == GPR_NUMBER_ra),1,
        12,
        ifelse(eval(GPR_NUMBER_$1 >= GPR_NUMBER_s0 && GPR_NUMBER_$1 <= GPR_NUMBER_s1),1,
            eval(GPR_NUMBER_$1-8),
            ifelse(eval(GPR_NUMBER_$1 >= GPR_NUMBER_s2 && GPR_NUMBER_$1 <= GPR_NUMBER_s11),1,
                eval(GPR_NUMBER_$1-16),
                errprint(Invalid register $1)
            )
        )
    )
})
define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+(($1)*ALen))})
define({GPR_SAVE_SLOT},{M(sp, GPR_SAVE_OFFSET(GPR_SAVE_INDEX($1)))})
define({GPR_SAVE},{sd $1, GPR_SAVE_SLOT($1)
    .cfi_rel_offset $1, GPR_SAVE_OFFSET(GPR_SAVE_INDEX($1))})
define({GPR_RESTORE},{ld $1, GPR_SAVE_SLOT($1)
    .cfi_same_value $1})

define({FPR_SAVE_INDEX},{
    ifelse(eval(FPR_NUMBER_$1 >= FPR_NUMBER_fs0 && FPR_NUMBER_$1 <= FPR_NUMBER_fs1),1,
        eval(FPR_NUMBER_$1-8),
        ifelse(eval(FPR_NUMBER_$1 >= FPR_NUMBER_fs2 && FPR_NUMBER_$1 <= FPR_NUMBER_fs11),1,
            eval(FPR_NUMBER_$1-16),
            errprint(Invalid register $1)
        )
    )
})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+(($1)*8))})
define({FPR_SAVE_SLOT},{M(sp, FPR_SAVE_OFFSET(FPR_SAVE_INDEX($1)))})
define({FPR_SAVE},{fsd $1, FPR_SAVE_SLOT($1)
    .cfi_rel_offset $1, FPR_SAVE_OFFSET(FPR_SAVE_INDEX($1))})
define({FPR_RESTORE},{fld $1, FPR_SAVE_SLOT($1)
    .cfi_same_value $1})

define({JIT_GPR_SAVE_OFFSET},{eval(J9TR_cframe_jitGPRs+(($1)*ALen))})
define({JIT_GPR_SAVE_SLOT},{M(sp, JIT_GPR_SAVE_OFFSET(GPR_NUMBER($1)))})
define({JIT_FPR_SAVE_OFFSET},{eval(J9TR_cframe_jitFPRs+(($1)*8))})
define({JIT_FPR_SAVE_SLOT},{M(sp, JIT_FPR_SAVE_OFFSET(FPR_NUMBER($1)))})

dnl SAFE_FPLR / RESTORE_FPLR only saves LR since on RISC-V there's
dnl no designated FP register (when used, it's s0).
dnl
dnl However, we still call the macro SAVE_FPLR / RESTORE_FPLR for
dnl consistency with AArch64.
define({SAVE_FPLR},{
    sd ra, JIT_GPR_SAVE_SLOT(ra)
})

define({RESTORE_FPLR},{
    ld ra, JIT_GPR_SAVE_SLOT(ra)
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
    sd  ra, JIT_GPR_SAVE_SLOT(ra)

    sd  t0,  JIT_GPR_SAVE_SLOT(t0)
    sd  t1,  JIT_GPR_SAVE_SLOT(t1)
    sd  t2,  JIT_GPR_SAVE_SLOT(t2)

    sd  a0,  JIT_GPR_SAVE_SLOT(a0)
    sd  a1,  JIT_GPR_SAVE_SLOT(a1)
    sd  a2,  JIT_GPR_SAVE_SLOT(a2)
    sd  a3,  JIT_GPR_SAVE_SLOT(a3)
    sd  a4,  JIT_GPR_SAVE_SLOT(a4)
    sd  a5,  JIT_GPR_SAVE_SLOT(a5)
    sd  a6,  JIT_GPR_SAVE_SLOT(a6)
    sd  a7,  JIT_GPR_SAVE_SLOT(a7)

    sd  t3,  JIT_GPR_SAVE_SLOT(t3)
    sd  t4,  JIT_GPR_SAVE_SLOT(t4)
    sd  t5,  JIT_GPR_SAVE_SLOT(t5)
    sd  t6,  JIT_GPR_SAVE_SLOT(t6)

    fsd ft0, JIT_FPR_SAVE_SLOT(ft0)
    fsd ft1, JIT_FPR_SAVE_SLOT(ft1)
    fsd ft2, JIT_FPR_SAVE_SLOT(ft2)
    fsd ft3, JIT_FPR_SAVE_SLOT(ft3)
    fsd ft4, JIT_FPR_SAVE_SLOT(ft4)
    fsd ft5, JIT_FPR_SAVE_SLOT(ft5)
    fsd ft6, JIT_FPR_SAVE_SLOT(ft6)
    fsd ft7, JIT_FPR_SAVE_SLOT(ft7)

    fsd fa0, JIT_FPR_SAVE_SLOT(fa0)
    fsd fa1, JIT_FPR_SAVE_SLOT(fa1)
    fsd fa2, JIT_FPR_SAVE_SLOT(fa2)
    fsd fa3, JIT_FPR_SAVE_SLOT(fa3)
    fsd fa4, JIT_FPR_SAVE_SLOT(fa4)
    fsd fa5, JIT_FPR_SAVE_SLOT(fa5)
    fsd fa6, JIT_FPR_SAVE_SLOT(fa6)
    fsd fa7, JIT_FPR_SAVE_SLOT(fa7)

    fsd ft8, JIT_FPR_SAVE_SLOT(ft8)
    fsd ft9, JIT_FPR_SAVE_SLOT(ft9)
    fsd ft10,JIT_FPR_SAVE_SLOT(ft10)
    fsd ft11,JIT_FPR_SAVE_SLOT(ft11)
})

define({RESTORE_C_VOLATILE_REGS},{
    ld  ra, JIT_GPR_SAVE_SLOT(ra)

    ld  t0,  JIT_GPR_SAVE_SLOT(t0)
    ld  t1,  JIT_GPR_SAVE_SLOT(t1)
    ld  t2,  JIT_GPR_SAVE_SLOT(t2)

    ld  a0,  JIT_GPR_SAVE_SLOT(a0)
    ld  a1,  JIT_GPR_SAVE_SLOT(a1)
    ld  a2,  JIT_GPR_SAVE_SLOT(a2)
    ld  a3,  JIT_GPR_SAVE_SLOT(a3)
    ld  a4,  JIT_GPR_SAVE_SLOT(a4)
    ld  a5,  JIT_GPR_SAVE_SLOT(a5)
    ld  a6,  JIT_GPR_SAVE_SLOT(a6)
    ld  a7,  JIT_GPR_SAVE_SLOT(a7)

    ld  t3,  JIT_GPR_SAVE_SLOT(t3)
    ld  t4,  JIT_GPR_SAVE_SLOT(t4)
    ld  t5,  JIT_GPR_SAVE_SLOT(t5)
    ld  t6,  JIT_GPR_SAVE_SLOT(t6)

    fld ft0, JIT_FPR_SAVE_SLOT(ft0)
    fld ft1, JIT_FPR_SAVE_SLOT(ft1)
    fld ft2, JIT_FPR_SAVE_SLOT(ft2)
    fld ft3, JIT_FPR_SAVE_SLOT(ft3)
    fld ft4, JIT_FPR_SAVE_SLOT(ft4)
    fld ft5, JIT_FPR_SAVE_SLOT(ft5)
    fld ft6, JIT_FPR_SAVE_SLOT(ft6)
    fld ft7, JIT_FPR_SAVE_SLOT(ft7)

    fld fa0, JIT_FPR_SAVE_SLOT(fa0)
    fld fa1, JIT_FPR_SAVE_SLOT(fa1)
    fld fa2, JIT_FPR_SAVE_SLOT(fa2)
    fld fa3, JIT_FPR_SAVE_SLOT(fa3)
    fld fa4, JIT_FPR_SAVE_SLOT(fa4)
    fld fa5, JIT_FPR_SAVE_SLOT(fa5)
    fld fa6, JIT_FPR_SAVE_SLOT(fa6)
    fld fa7, JIT_FPR_SAVE_SLOT(fa7)

    fld ft8, JIT_FPR_SAVE_SLOT(ft8)
    fld ft9, JIT_FPR_SAVE_SLOT(ft9)
    fld ft10,JIT_FPR_SAVE_SLOT(ft10)
    fld ft11,JIT_FPR_SAVE_SLOT(ft11)

})

dnl No need to save/restore fs0-fs11 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).
dnl

define({SAVE_C_NONVOLATILE_REGS},{
    sd  s0,  JIT_GPR_SAVE_SLOT(s0)
    sd  s1,  JIT_GPR_SAVE_SLOT(s1)

    sd  s2,  JIT_GPR_SAVE_SLOT(s2)
    sd  s3,  JIT_GPR_SAVE_SLOT(s3)
    sd  s4,  JIT_GPR_SAVE_SLOT(s4)
    sd  s5,  JIT_GPR_SAVE_SLOT(s5)
    sd  s6,  JIT_GPR_SAVE_SLOT(s6)
    sd  s7,  JIT_GPR_SAVE_SLOT(s7)
    sd  s8,  JIT_GPR_SAVE_SLOT(s8)
    sd  s9,  JIT_GPR_SAVE_SLOT(s9)
    sd s10,  JIT_GPR_SAVE_SLOT(s10)
    sd s11,  JIT_GPR_SAVE_SLOT(s11)
})

define({RESTORE_C_NONVOLATILE_REGS},{
    ld  s0,  JIT_GPR_SAVE_SLOT(s0)
    ld  s1,  JIT_GPR_SAVE_SLOT(s1)

    ld  s2,  JIT_GPR_SAVE_SLOT(s2)
    ld  s3,  JIT_GPR_SAVE_SLOT(s3)
    ld  s4,  JIT_GPR_SAVE_SLOT(s4)
    ld  s5,  JIT_GPR_SAVE_SLOT(s5)
    ld  s6,  JIT_GPR_SAVE_SLOT(s6)
    ld  s7,  JIT_GPR_SAVE_SLOT(s7)
    ld  s8,  JIT_GPR_SAVE_SLOT(s8)
    ld  s9,  JIT_GPR_SAVE_SLOT(s9)
    ld s10,  JIT_GPR_SAVE_SLOT(s10)
    ld s11,  JIT_GPR_SAVE_SLOT(s11)
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
    sd  s0,  JIT_GPR_SAVE_SLOT(s0)
    sd  s1,  JIT_GPR_SAVE_SLOT(s1)

    sd  s2,  JIT_GPR_SAVE_SLOT(s2)
    sd  s3,  JIT_GPR_SAVE_SLOT(s3)
    sd  s4,  JIT_GPR_SAVE_SLOT(s4)
    sd  s5,  JIT_GPR_SAVE_SLOT(s5)
    sd  s6,  JIT_GPR_SAVE_SLOT(s6)
    sd  s7,  JIT_GPR_SAVE_SLOT(s7)
    sd  s8,  JIT_GPR_SAVE_SLOT(s8)
    sd  s9,  JIT_GPR_SAVE_SLOT(s9)                  # save preserved regs (end)
})

define({RESTORE_PRESERVED_REGS},{
    ld  s0,  JIT_GPR_SAVE_SLOT(s0)                  # restore preserved regs
    ld  s1,  JIT_GPR_SAVE_SLOT(s1)

    ld  s2,  JIT_GPR_SAVE_SLOT(s2)
    ld  s3,  JIT_GPR_SAVE_SLOT(s3)
    ld  s4,  JIT_GPR_SAVE_SLOT(s4)
    ld  s5,  JIT_GPR_SAVE_SLOT(s5)
    ld  s6,  JIT_GPR_SAVE_SLOT(s6)
    ld  s7,  JIT_GPR_SAVE_SLOT(s7)
    ld  s8,  JIT_GPR_SAVE_SLOT(s8)
    ld  s9,  JIT_GPR_SAVE_SLOT(s9)                  # restore preserved regs (end)
})

define({BRANCH_VIA_VMTHREAD},{
    ld t2, M(J9VMTHREAD, $1)
    jalr zero, t2, 0
})

define({SWITCH_TO_JAVA_STACK},{
    ld J9SP,M(J9VMTHREAD, J9TR_VMThread_sp)         # restore Java SP from VMThread
})

define({SWITCH_TO_C_STACK},{
    sd J9SP,M(J9VMTHREAD, J9TR_VMThread_sp)         # save Java SP to VMThread
})
