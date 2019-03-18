dnl Copyright (c) 2019, 2019 IBM Corp. and others
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
ifelse(eval(CINTERP_STACK_SIZE % 16),0,,{ERROR stack size CINTERP_STACK_SIZE is not 16-aligned})

define({ALen},{8})

define({J9VMTHREAD},{x19})
define({J9SP},{x20})
define({J9PC},{x21})
define({J9LITERALS},{x22})
define({J9A0},{x23})

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

define({CALL_DIRECT},{bl BRANCH_SYMBOL($1)})

define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+(($1-19)*ALen))})
define({GPR_SAVE_SLOT},{[sp,{#}GPR_SAVE_OFFSET($1)]})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+(($1-8)*8))})
define({FPR_SAVE_SLOT},{[sp,{#}FPR_SAVE_OFFSET($1)]})

define({JIT_GPR_SAVE_OFFSET},{eval(J9TR_cframe_jitGPRs+(($1)*ALen))})
define({JIT_GPR_SAVE_SLOT},{[sp,{#}JIT_GPR_SAVE_OFFSET($1)]})
define({JIT_FPR_SAVE_OFFSET},{eval(J9TR_cframe_jitFPRs+(($1)*8))})
define({JIT_FPR_SAVE_SLOT},{[sp,{#}JIT_FPR_SAVE_OFFSET($1)]})

define({SAVE_FPLR},{stp x29,x30,JIT_GPR_SAVE_SLOT(29)})

define({RESTORE_FPLR},{ldp x29,x30,JIT_GPR_SAVE_SLOT(29)})

define({BEGIN_HELPER},{
	START_PROC($1)
	SAVE_FPLR
})

define({END_HELPER},{
	ret
	END_PROC($1)
})

define({CALL_C_WITH_VMTHREAD},{
	mov x0,J9VMTHREAD
	CALL_DIRECT($1)
})

define({SAVE_C_VOLATILE_REGS},{
	stp x0,x1,JIT_GPR_SAVE_SLOT(0)
	stp x2,x3,JIT_GPR_SAVE_SLOT(2)
	stp x4,x5,JIT_GPR_SAVE_SLOT(4)
	stp x6,x7,JIT_GPR_SAVE_SLOT(6)
	stp x8,x9,JIT_GPR_SAVE_SLOT(8)
	stp x10,x11,JIT_GPR_SAVE_SLOT(10)
	stp x12,x13,JIT_GPR_SAVE_SLOT(12)
	stp x14,x15,JIT_GPR_SAVE_SLOT(14)
	stp x16,x17,JIT_GPR_SAVE_SLOT(16)
	str x18,JIT_GPR_SAVE_SLOT(18)
	stp d0,d1,JIT_FPR_SAVE_SLOT(0)
	stp d2,d3,JIT_FPR_SAVE_SLOT(2)
	stp d4,d5,JIT_FPR_SAVE_SLOT(4)
	stp d6,d7,JIT_FPR_SAVE_SLOT(6)
	add x15, sp, JIT_FPR_SAVE_OFFSET(16)
	stp d16,d17,[x15,#0]
	stp d18,d19,[x15,#16]
	stp d20,d21,[x15,#32]
	stp d22,d23,[x15,#48]
	stp d24,d25,[x15,#64]
	stp d26,d27,[x15,#80]
	stp d28,d29,[x15,#96]
	stp d30,d31,[x15,#112]
})

define({RESTORE_C_VOLATILE_REGS},{
	ldp d0,d1,JIT_FPR_SAVE_SLOT(0)
	ldp d2,d3,JIT_FPR_SAVE_SLOT(2)
	ldp d4,d5,JIT_FPR_SAVE_SLOT(4)
	ldp d6,d7,JIT_FPR_SAVE_SLOT(6)
	add x15, sp, JIT_FPR_SAVE_OFFSET(16)
	ldp d16,d17,[x15,#0]
	ldp d18,d19,[x15,#16]
	ldp d20,d21,[x15,#32]
	ldp d22,d23,[x15,#48]
	ldp d24,d25,[x15,#64]
	ldp d26,d27,[x15,#80]
	ldp d28,d29,[x15,#96]
	ldp d30,d31,[x15,#112]
	ldp x0,x1,JIT_GPR_SAVE_SLOT(0)
	ldp x2,x3,JIT_GPR_SAVE_SLOT(2)
	ldp x4,x5,JIT_GPR_SAVE_SLOT(4)
	ldp x6,x7,JIT_GPR_SAVE_SLOT(6)
	ldp x8,x9,JIT_GPR_SAVE_SLOT(8)
	ldp x10,x11,JIT_GPR_SAVE_SLOT(10)
	ldp x12,x13,JIT_GPR_SAVE_SLOT(12)
	ldp x14,x15,JIT_GPR_SAVE_SLOT(14)
	ldp x16,x17,JIT_GPR_SAVE_SLOT(16)
	ldr x18,JIT_GPR_SAVE_SLOT(18)
})

dnl No need to save/restore d8-15 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).

define({SAVE_C_NONVOLATILE_REGS},{
	str x19,JIT_GPR_SAVE_SLOT(19)
	stp x20,x21,JIT_GPR_SAVE_SLOT(20)
	stp x22,x23,JIT_GPR_SAVE_SLOT(22)
	stp x24,x25,JIT_GPR_SAVE_SLOT(24)
	stp x26,x27,JIT_GPR_SAVE_SLOT(26)
	stp x28,x29,JIT_GPR_SAVE_SLOT(28)
})

define({RESTORE_C_NONVOLATILE_REGS},{
	ldr x19,JIT_GPR_SAVE_SLOT(19)
	ldp x20,x21,JIT_GPR_SAVE_SLOT(20)
	ldp x22,x23,JIT_GPR_SAVE_SLOT(22)
	ldp x24,x25,JIT_GPR_SAVE_SLOT(24)
	ldp x26,x27,JIT_GPR_SAVE_SLOT(26)
	ldp x28,x29,JIT_GPR_SAVE_SLOT(28)
})

define({SAVE_ALL_REGS},{
	SAVE_C_VOLATILE_REGS
	SAVE_C_NONVOLATILE_REGS
})

define({RESTORE_ALL_REGS},{
	RESTORE_C_VOLATILE_REGS
	RESTORE_C_NONVOLATILE_REGS
})

define({SAVE_PRESERVED_REGS},{
	str x19,JIT_GPR_SAVE_SLOT(19)
	stp x20,x21,JIT_GPR_SAVE_SLOT(20)
	stp x22,x23,JIT_GPR_SAVE_SLOT(22)
	stp x24,x25,JIT_GPR_SAVE_SLOT(24)
	stp x26,x27,JIT_GPR_SAVE_SLOT(26)
	stp x28,x29,JIT_GPR_SAVE_SLOT(28)
})

define({RESTORE_PRESERVED_REGS},{
	ldr x19,JIT_GPR_SAVE_SLOT(19)
	ldp x20,x21,JIT_GPR_SAVE_SLOT(20)
	ldp x22,x23,JIT_GPR_SAVE_SLOT(22)
	ldp x24,x25,JIT_GPR_SAVE_SLOT(24)
	ldp x26,x27,JIT_GPR_SAVE_SLOT(26)
	ldp x28,x29,JIT_GPR_SAVE_SLOT(28)
})

define({BRANCH_VIA_VMTHREAD},{
	ldr x0,[J9VMTHREAD,{#}$1]
	br x0
})

define({SWITCH_TO_JAVA_STACK},{ldr J9SP,[J9VMTHREAD,{#}J9TR_VMThread_sp]})
define({SWITCH_TO_C_STACK},{str J9SP,[J9VMTHREAD,{#}J9TR_VMThread_sp]})
