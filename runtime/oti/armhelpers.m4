dnl Copyright (c) 1991, 2017 IBM Corp. and others
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
ifelse(eval(CINTERP_STACK_SIZE % 8),0,,{ERROR stack size CINTERP_STACK_SIZE is not 8-aligned})

ifdef({ASM_J9VM_ENV_DATA64},{
define({ALen},{8})        
},{
define({ALen},{4})       
})

define({J9VMTHREAD},{r8})
define({J9SP},{r7})
define({J9PC},{r5})
define({J9LITERALS},{r4})
define({J9A0},{r6})

define({CONCAT},{$1$2})

define({FUNC_LABEL},{$1})

define({DECLARE_PUBLIC},{
	.globl FUNC_LABEL($1)
	.type FUNC_LABEL($1),function
})

define({DECLARE_EXTERN},{.globl FUNC_LABEL($1)})

define({START_PROC},{
	.text
	DECLARE_PUBLIC($1)
	.ALIGN 2
	FUNC_LABEL($1):
})

define({END_PROC})

define({BRANCH_SYMBOL},{FUNC_LABEL($1)(PLT)})

define({CALL_DIRECT},{bl BRANCH_SYMBOL($1)})

define({GPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedGPRs+(($1)*ALen))})
define({GPR_SAVE_SLOT},{[R13,{#}GPR_SAVE_OFFSET($1)]})
define({FPR_SAVE_OFFSET},{eval(J9TR_cframe_preservedFPRs+(($1)*8))})
define({FPR_SAVE_SLOT},{[R13,{#}FPR_SAVE_OFFSET($1)]})
define({JIT_GPR_SAVE_OFFSET},{eval(J9TR_cframe_jitGPRs+(($1)*ALen))})
define({JIT_GPR_SAVE_SLOT},{[R13,{#}JIT_GPR_SAVE_OFFSET($1)]})
define({JIT_FPR_SAVE_OFFSET},{eval(J9TR_cframe_jitFPRs+(($1)*8))})
define({JIT_FPR_SAVE_SLOT},{[R13,{#}JIT_FPR_SAVE_OFFSET($1)]})

define({SAVE_LR},{str r14,JIT_GPR_SAVE_SLOT(14)})

define({RESTORE_LR},{ldr r14,JIT_GPR_SAVE_SLOT(14)})

define({BEGIN_HELPER},{
	START_PROC($1)
	SAVE_LR
})

define({END_HELPER},{
	mov r15,r14
	END_PROC($1)
})

define({CALL_C_WITH_VMTHREAD},{
	mov r0,J9VMTHREAD
	CALL_DIRECT($1)
})

define({SAVE_C_VOLATILE_REGS},{
	str r0,JIT_GPR_SAVE_SLOT(0)
	str r1,JIT_GPR_SAVE_SLOT(1)
	str r2,JIT_GPR_SAVE_SLOT(2)
	str r3,JIT_GPR_SAVE_SLOT(3)
	add r3,r13,{#}JIT_FPR_SAVE_OFFSET(0)
	vstmia.64 r3,{{ D0-D7 }}
})

dnl No need to save/restore D8-D15 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).

define({RESTORE_C_VOLATILE_REGS},{
	add r3,r13,{#}JIT_FPR_SAVE_OFFSET(0)
	vldmia.64 r3,{{ D0-D7 }}
	ldr r0,JIT_GPR_SAVE_SLOT(0)
	ldr r1,JIT_GPR_SAVE_SLOT(1)
	ldr r2,JIT_GPR_SAVE_SLOT(2)
	ldr r3,JIT_GPR_SAVE_SLOT(3)
})

define({SAVE_C_NONVOLATILE_REGS},{
	str r4,JIT_GPR_SAVE_SLOT(4)
	str r5,JIT_GPR_SAVE_SLOT(5)
	str r6,JIT_GPR_SAVE_SLOT(6)
	str r9,JIT_GPR_SAVE_SLOT(9)
	str r10,JIT_GPR_SAVE_SLOT(10)
	str r11,JIT_GPR_SAVE_SLOT(11)
})

define({RESTORE_C_NONVOLATILE_REGS},{
	ldr r4,JIT_GPR_SAVE_SLOT(4)
	ldr r5,JIT_GPR_SAVE_SLOT(5)
	ldr r6,JIT_GPR_SAVE_SLOT(6)
	ldr r9,JIT_GPR_SAVE_SLOT(9)
	ldr r10,JIT_GPR_SAVE_SLOT(10)
	ldr r11,JIT_GPR_SAVE_SLOT(11)
})

define({SAVE_ALL_REGS},{
	SAVE_C_VOLATILE_REGS
	SAVE_C_NONVOLATILE_REGS
})

define({SAVE_PRESERVED_REGS},{
	str r6,JIT_GPR_SAVE_SLOT(6)
	str r9,JIT_GPR_SAVE_SLOT(9)
	str r10,JIT_GPR_SAVE_SLOT(10)
})

define({RESTORE_ALL_REGS},{
	RESTORE_C_VOLATILE_REGS
	RESTORE_C_NONVOLATILE_REGS
})

define({RESTORE_PRESERVED_REGS},{
	ldr r6,JIT_GPR_SAVE_SLOT(6)
	ldr r9,JIT_GPR_SAVE_SLOT(9)
	ldr r10,JIT_GPR_SAVE_SLOT(10)
})

define({SWITCH_TO_JAVA_STACK},{ldr J9SP,[J9VMTHREAD,{#}J9TR_VMThread_sp]})
define({SWITCH_TO_C_STACK},{str J9SP,[J9VMTHREAD,{#}J9TR_VMThread_sp]})
