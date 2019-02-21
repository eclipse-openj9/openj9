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

define({START_PROC},{
	.text
	DECLARE_PUBLIC($1)
	.align 2
FUNC_LABEL($1):
})

define({END_PROC})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)(PLT)})

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

dnl No need to save/restore D8-D31 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).

dnl To be filled
define({SAVE_C_VOLATILE_REGS},{
})

dnl To be filled
define({RESTORE_C_VOLATILE_REGS},{
})

dnl To be filled
define({SAVE_C_NONVOLATILE_REGS},{
})

dnl To be filled
define({RESTORE_C_NONVOLATILE_REGS},{
})

define({SAVE_ALL_REGS},{
	SAVE_C_VOLATILE_REGS
	SAVE_C_NONVOLATILE_REGS
})

dnl To be filled
define({SAVE_PRESERVED_REGS},{
})

define({RESTORE_ALL_REGS},{
	RESTORE_C_VOLATILE_REGS
	RESTORE_C_NONVOLATILE_REGS
})

dnl To be filled
define({RESTORE_PRESERVED_REGS},{
})

define({SWITCH_TO_JAVA_STACK},{ldr J9SP,[J9VMTHREAD,{#}J9TR_VMThread_sp]})
define({SWITCH_TO_C_STACK},{str J9SP,[J9VMTHREAD,{#}J9TR_VMThread_sp]})
