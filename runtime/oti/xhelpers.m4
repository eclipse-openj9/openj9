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
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

include(jilvalues.m4)

define({CINTERP_STACK_SIZE},{J9CONST(J9TR_cframe_sizeof,$1,$2)})

ifdef({WIN32},{

define({SHORT_JMP},{short})

ifdef({ASM_J9VM_ENV_DATA64},{

define({FILE_START},{
	OPTION NOSCOPED
	_TEXT SEGMENT 'CODE'
})

},{	dnl ASM_J9VM_ENV_DATA64

define({FILE_START},{
	.686p
	assume cs:flat,ds:flat,ss:flat
	option NoOldMacros
	.xmm
	_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
})

})	dnl ASM_J9VM_ENV_DATA64

define({FILE_END},{
	_TEXT ends
	end
})

define({START_PROC},{
	align 16
	DECLARE_PUBLIC($1)
	$1 proc
})

define({END_PROC},{$1 endp})

define({DECLARE_PUBLIC},{public $1})

define({DECLARE_EXTERN},{extrn $1:near})

define({LABEL},$1)

},{	dnl WIN32

ifdef({OSX},{ 

define({SHORT_JMP},{})

define({FILE_START},{
	.intel_syntax noprefix
	.text
})

},{	dnl OSX

define({SHORT_JMP},{short})

define({FILE_START},{
	.intel_syntax noprefix
	.arch pentium4
	.text
})

})	dnl OSX

define({FILE_END})

define({START_PROC},{
	.align 16
	DECLARE_PUBLIC($1)
	$1:
})

define({END_PROC},{
END_$1:
ifdef({OSX},{

},{	dnl OSX
	.size $1,END_$1 - $1
})	dnl OSX
})

define({DECLARE_PUBLIC},{.global $1})

define({DECLARE_EXTERN},{.extern $1})

ifdef({OSX},{
define({LABEL},$1)
},{	dnl OSX
define({LABEL},.$1)
})	dnl OSX

})	dnl WIN32

ifdef({ASM_J9VM_ENV_DATA64},{
	dnl 64-bit

dnl JIT linkage:
dnl	register save order in memory: RAX RBX RCX RDX RDI RSI RBP RSP R8-R15 XMM0-15
dnl	argument GPRs: RAX RSI RDX RCX
dnl	preserved: RBX R9-R15

define({_rax},{rax})
define({_rbx},{rbx})
define({_rcx},{rcx})
define({_rdx},{rdx})
define({_rsi},{rsi})
define({_rdi},{rdi})
define({_rsp},{rsp})
define({_rbp},{rbp})
define({uword},{qword})

ifdef({WIN32},{

dnl C linkage for windows:
dnl	argument GPRs: RCX RDX R8 R9
dnl	preserved: RBX RDI RSI R12-R15 XMM6-15

define({PARM_REG},{ifelse($1,1,_rcx,$1,2,_rdx,$1,3,_r8,$1,4,_r9,{ERROR})})

define({FASTCALL_C},{
	sub _rsp,4*J9TR_pointerSize
	CALL_C_FUNC($1,4,0)
	add _rsp,4*J9TR_pointerSize
})

define({FASTCALL_C_WITH_VMTHREAD},{
	mov _rcx,_rbp
	FASTCALL_C($1,$2)
})

define({CALL_C_WITH_VMTHREAD},{FASTCALL_C_WITH_VMTHREAD($1,$2)})

},{	dnl WIN32

dnl C linkage for linux:
dnl	argument GPRs: RDI RSI RDX RCX R8 R9
dnl	preserved: RBX R12-R15, no XMM

define({PARM_REG},{ifelse($1,1,_rdi,$1,2,_rsi,$1,3,_rdx,$1,4,_rcx,$1,5,_r8,$1,6,_r9,{ERROR})})

define({FASTCALL_C},{
	CALL_C_FUNC($1,0,0)
})

define({FASTCALL_C_WITH_VMTHREAD},{
	mov _rdi,_rbp
	FASTCALL_C($1,$2)
})

define({CALL_C_WITH_VMTHREAD},{FASTCALL_C_WITH_VMTHREAD($1,$2)})

})	dnl WIN32

},{	dnl ASM_J9VM_ENV_DATA64
	dnl 32-bit

dnl JIT linkage:
dnl	register save order in memory: EAX EBX ECX EDX EDI ESI EBP ESP XMM0-7
dnl	argument GPRs: none
dnl	preserved: EBX ECX ESI, no XMM
dnl C linkage (windows and linux)
dnl	argument GPRs: none (stdcall) / ECX EDX (fastcall)
dnl	preserved: EBX EDI ESI, no XMM

define({_rax},{eax})
define({_rbx},{ebx})
define({_rcx},{ecx})
define({_rdx},{edx})
define({_rsi},{esi})
define({_rdi},{edi})
define({_rsp},{esp})
define({_rbp},{ebp})
define({uword},{dword})

define({FASTCALL_STACK_PARM_SLOTS},{ifelse(eval($1),0,0,eval($1),1,0,eval(($1)-2))})

ifdef({WIN32},{

define({FASTCALL_SYMBOL},{@$1@eval(($2)*4)})

define({FASTCALL_EXTERN},{extern PASCAL FASTCALL_SYMBOL($1,$2):near})

define({FASTCALL_CLEAN_STACK},{ifelse(FASTCALL_STACK_PARM_SLOTS($1),0,{},{add _rsp,4*FASTCALL_STACK_PARM_SLOTS($1)})})

},{	dnl WIN32

define({FASTCALL_CLEAN_STACK},{})

})	dnl WIN32

define({FASTCALL_C},{
	call FASTCALL_SYMBOL($1,$2)
	FASTCALL_CLEAN_STACK($2)
})

define({FASTCALL_C_WITH_VMTHREAD},{
	mov _rcx,_rbp
	CALL_C_FUNC(FASTCALL_SYMBOL($1,1),0,0)
	FASTCALL_CLEAN_STACK(1)
})

define({FASTCALL_INDIRECT_WITH_VMTHREAD},{
	mov _rcx,_rbp
	CALL_C_FUNC($1,0,0)
	FASTCALL_CLEAN_STACK(1)
})

define({CALL_C_WITH_VMTHREAD},{
dnl maintain 16-byte stack alignment
	sub esp,12
	push ebp
	CALL_C_FUNC($1,4,0)
	add esp,16
})

})	dnl ASM_J9VM_ENV_DATA64

define({SWITCH_TO_C_STACK},{
	mov uword ptr J9TR_VMThread_sp[_rbp],_rsp
	mov _rsp,uword ptr J9TR_VMThread_machineSP[_rbp]
})

define({SWITCH_TO_JAVA_STACK},{
	mov uword ptr J9TR_VMThread_machineSP[_rbp],_rsp
	mov _rsp,uword ptr J9TR_VMThread_sp[_rbp]
})

define({CALL_C_FUNC},{
	mov uword ptr (J9TR_machineSP_vmStruct+$3+(J9TR_pointerSize*$2))[_rsp],_rbp
	mov _rbp,uword ptr (J9TR_machineSP_machineBP+$3+(J9TR_pointerSize*$2))[_rsp]
	call $1
	mov _rbp,uword ptr (J9TR_machineSP_vmStruct+$3+(J9TR_pointerSize*$2))[_rsp]
})

define({J9VMTHREAD},{_rbp})
define({J9SP},{_rsp})
define({J9PC},{_rsi})
define({J9LITERALS},{_rbx})
define({J9A0},{_rcx})

dnl When entering any a JIT helper, _rbp is always the vmThread
dnl and _rsp is always the java SP, so those are ignored by the
dnl SAVE/RESTORE macros as they will never be register mapped.
dnl The exceptions to the above rule are jitAcquireVMAccess / jitReleaseVMAccess
dnl which are called with the _rbp as above, but the _rsp points to the C stack.

ifdef({ASM_J9VM_ENV_DATA64},{

define({END_HELPER},{
	ret
	END_PROC($1)
})

ifdef({WIN32},{

define({SAVE_C_VOLATILE_REGS},{
	mov qword ptr J9TR_cframe_rax[_rsp],rax
	mov qword ptr J9TR_cframe_rcx[_rsp],rcx
	mov qword ptr J9TR_cframe_rdx[_rsp],rdx
 dnl RSI not volatile in C but is a JIT helper argument register
	mov qword ptr J9TR_cframe_rsi[_rsp],rsi
	mov qword ptr J9TR_cframe_r8[_rsp],r8
	mov qword ptr J9TR_cframe_r9[_rsp],r9
	mov qword ptr J9TR_cframe_r10[_rsp],r10
	mov qword ptr J9TR_cframe_r11[_rsp],r11
	movq qword ptr J9TR_cframe_jitFPRs+(0*8)[_rsp],xmm0
	movq qword ptr J9TR_cframe_jitFPRs+(1*8)[_rsp],xmm1
	movq qword ptr J9TR_cframe_jitFPRs+(2*8)[_rsp],xmm2
	movq qword ptr J9TR_cframe_jitFPRs+(3*8)[_rsp],xmm3
	movq qword ptr J9TR_cframe_jitFPRs+(4*8)[_rsp],xmm4
	movq qword ptr J9TR_cframe_jitFPRs+(5*8)[_rsp],xmm5
})

define({RESTORE_C_VOLATILE_REGS},{
	mov rax,qword ptr J9TR_cframe_rax[_rsp]
	mov rcx,qword ptr J9TR_cframe_rcx[_rsp]
	mov rdx,qword ptr J9TR_cframe_rdx[_rsp]
	mov r8,qword ptr J9TR_cframe_r8[_rsp]
	mov r9,qword ptr J9TR_cframe_r9[_rsp]
	mov r10,qword ptr J9TR_cframe_r10[_rsp]
	mov r11,qword ptr J9TR_cframe_r11[_rsp]
	movq xmm0,qword ptr J9TR_cframe_jitFPRs+(0*8)[_rsp]
	movq xmm1,qword ptr J9TR_cframe_jitFPRs+(1*8)[_rsp]
	movq xmm2,qword ptr J9TR_cframe_jitFPRs+(2*8)[_rsp]
	movq xmm3,qword ptr J9TR_cframe_jitFPRs+(3*8)[_rsp]
	movq xmm4,qword ptr J9TR_cframe_jitFPRs+(4*8)[_rsp]
	movq xmm5,qword ptr J9TR_cframe_jitFPRs+(5*8)[_rsp]
})

dnl No need to save/restore xmm9-15 - the stack walker will never need to read
dnl or modify them (no preserved FPRs in the JIT private linkage).  xmm6-7
dnl are preserved as they are JIT FP arguments which may need to be read
dnl in order to decompile.  They do not need to be restored.
dnl For the old-style dual mode helper case, RSI has already been saved,
dnl but this macro may be used without having preserved the volatile
dnl registers, so save it again here just in case.  As this is the slow path
dnl anyway, there will be no performance issue.

define({SAVE_C_NONVOLATILE_REGS},{
	mov qword ptr J9TR_cframe_rbx[_rsp],rbx
	mov qword ptr J9TR_cframe_rdi[_rsp],rdi
	mov qword ptr J9TR_cframe_rsi[_rsp],rsi
	mov qword ptr J9TR_cframe_r12[_rsp],r12
	mov qword ptr J9TR_cframe_r13[_rsp],r13
	mov qword ptr J9TR_cframe_r14[_rsp],r14
	mov qword ptr J9TR_cframe_r15[_rsp],r15
	movq qword ptr J9TR_cframe_jitFPRs+(6*8)[_rsp],xmm6
	movq qword ptr J9TR_cframe_jitFPRs+(7*8)[_rsp],xmm7
})

dnl xmm6-7 are preserved as they are JIT FP arguments which may need
dnl to be read in order to decompile.  They do not need to be restored.

define({SAVE_C_NONVOLATILE_JIT_FP_ARG_REGS},{
	movq qword ptr J9TR_cframe_jitFPRs+(6*8)[_rsp],xmm6
	movq qword ptr J9TR_cframe_jitFPRs+(7*8)[_rsp],xmm7
})

define({RESTORE_C_NONVOLATILE_REGS},{
	mov rbx,qword ptr J9TR_cframe_rbx[_rsp]
	mov rdi,qword ptr J9TR_cframe_rdi[_rsp]
	mov rsi,qword ptr J9TR_cframe_rsi[_rsp]
	mov r12,qword ptr J9TR_cframe_r12[_rsp]
	mov r13,qword ptr J9TR_cframe_r13[_rsp]
	mov r14,qword ptr J9TR_cframe_r14[_rsp]
	mov r15,qword ptr J9TR_cframe_r15[_rsp]
})

},{	dnl WIN32

define({SAVE_C_VOLATILE_REGS},{
	mov qword ptr J9TR_cframe_rax[_rsp],rax
	mov qword ptr J9TR_cframe_rcx[_rsp],rcx
	mov qword ptr J9TR_cframe_rdx[_rsp],rdx
	mov qword ptr J9TR_cframe_rdi[_rsp],rdi
	mov qword ptr J9TR_cframe_rsi[_rsp],rsi
	mov qword ptr J9TR_cframe_r8[_rsp],r8
	mov qword ptr J9TR_cframe_r9[_rsp],r9
	mov qword ptr J9TR_cframe_r10[_rsp],r10
	mov qword ptr J9TR_cframe_r11[_rsp],r11
	movq qword ptr J9TR_cframe_jitFPRs+(0*8)[_rsp],xmm0
	movq qword ptr J9TR_cframe_jitFPRs+(1*8)[_rsp],xmm1
	movq qword ptr J9TR_cframe_jitFPRs+(2*8)[_rsp],xmm2
	movq qword ptr J9TR_cframe_jitFPRs+(3*8)[_rsp],xmm3
	movq qword ptr J9TR_cframe_jitFPRs+(4*8)[_rsp],xmm4
	movq qword ptr J9TR_cframe_jitFPRs+(5*8)[_rsp],xmm5
	movq qword ptr J9TR_cframe_jitFPRs+(6*8)[_rsp],xmm6
	movq qword ptr J9TR_cframe_jitFPRs+(7*8)[_rsp],xmm7
	movq qword ptr J9TR_cframe_jitFPRs+(8*8)[_rsp],xmm8
	movq qword ptr J9TR_cframe_jitFPRs+(9*8)[_rsp],xmm9
	movq qword ptr J9TR_cframe_jitFPRs+(10*8)[_rsp],xmm10
	movq qword ptr J9TR_cframe_jitFPRs+(11*8)[_rsp],xmm11
	movq qword ptr J9TR_cframe_jitFPRs+(12*8)[_rsp],xmm12
	movq qword ptr J9TR_cframe_jitFPRs+(13*8)[_rsp],xmm13
	movq qword ptr J9TR_cframe_jitFPRs+(14*8)[_rsp],xmm14
	movq qword ptr J9TR_cframe_jitFPRs+(15*8)[_rsp],xmm15
})

define({RESTORE_C_VOLATILE_REGS},{
	mov rax,qword ptr J9TR_cframe_rax[_rsp]
	mov rcx,qword ptr J9TR_cframe_rcx[_rsp]
	mov rdx,qword ptr J9TR_cframe_rdx[_rsp]
	mov rdi,qword ptr J9TR_cframe_rdi[_rsp]
	mov rsi,qword ptr J9TR_cframe_rsi[_rsp]
	mov r8,qword ptr J9TR_cframe_r8[_rsp]
	mov r9,qword ptr J9TR_cframe_r9[_rsp]
	mov r10,qword ptr J9TR_cframe_r10[_rsp]
	mov r11,qword ptr J9TR_cframe_r11[_rsp]
	movq xmm0,qword ptr J9TR_cframe_jitFPRs+(0*8)[_rsp]
	movq xmm1,qword ptr J9TR_cframe_jitFPRs+(1*8)[_rsp]
	movq xmm2,qword ptr J9TR_cframe_jitFPRs+(2*8)[_rsp]
	movq xmm3,qword ptr J9TR_cframe_jitFPRs+(3*8)[_rsp]
	movq xmm4,qword ptr J9TR_cframe_jitFPRs+(4*8)[_rsp]
	movq xmm5,qword ptr J9TR_cframe_jitFPRs+(5*8)[_rsp]
	movq xmm6,qword ptr J9TR_cframe_jitFPRs+(6*8)[_rsp]
	movq xmm7,qword ptr J9TR_cframe_jitFPRs+(7*8)[_rsp]
	movq xmm8,qword ptr J9TR_cframe_jitFPRs+(8*8)[_rsp]
	movq xmm9,qword ptr J9TR_cframe_jitFPRs+(9*8)[_rsp]
	movq xmm10,qword ptr J9TR_cframe_jitFPRs+(10*8)[_rsp]
	movq xmm11,qword ptr J9TR_cframe_jitFPRs+(11*8)[_rsp]
	movq xmm12,qword ptr J9TR_cframe_jitFPRs+(12*8)[_rsp]
	movq xmm13,qword ptr J9TR_cframe_jitFPRs+(13*8)[_rsp]
	movq xmm14,qword ptr J9TR_cframe_jitFPRs+(14*8)[_rsp]
	movq xmm15,qword ptr J9TR_cframe_jitFPRs+(15*8)[_rsp]
})

define({SAVE_C_NONVOLATILE_REGS},{
	mov qword ptr J9TR_cframe_rbx[_rsp],rbx
	mov qword ptr J9TR_cframe_r12[_rsp],r12
	mov qword ptr J9TR_cframe_r13[_rsp],r13
	mov qword ptr J9TR_cframe_r14[_rsp],r14
	mov qword ptr J9TR_cframe_r15[_rsp],r15
})

define({RESTORE_C_NONVOLATILE_REGS},{
	mov rbx,qword ptr J9TR_cframe_rbx[_rsp]
	mov r12,qword ptr J9TR_cframe_r12[_rsp]
	mov r13,qword ptr J9TR_cframe_r13[_rsp]
	mov r14,qword ptr J9TR_cframe_r14[_rsp]
	mov r15,qword ptr J9TR_cframe_r15[_rsp]
})

})	dnl WIN32

define({SAVE_PRESERVED_REGS},{
	mov qword ptr J9TR_cframe_rbx[_rsp],rbx
	mov qword ptr J9TR_cframe_r9[_rsp],r9
	mov qword ptr J9TR_cframe_r10[_rsp],r10
	mov qword ptr J9TR_cframe_r11[_rsp],r11
	mov qword ptr J9TR_cframe_r12[_rsp],r12
	mov qword ptr J9TR_cframe_r13[_rsp],r13
	mov qword ptr J9TR_cframe_r14[_rsp],r14
	mov qword ptr J9TR_cframe_r15[_rsp],r15
})

define({RESTORE_PRESERVED_REGS},{
	mov rbx,qword ptr J9TR_cframe_rbx[_rsp]
	mov r9,qword ptr J9TR_cframe_r9[_rsp]
	mov r10,qword ptr J9TR_cframe_r10[_rsp]
	mov r11,qword ptr J9TR_cframe_r11[_rsp]
	mov r12,qword ptr J9TR_cframe_r12[_rsp]
	mov r13,qword ptr J9TR_cframe_r13[_rsp]
	mov r14,qword ptr J9TR_cframe_r14[_rsp]
	mov r15,qword ptr J9TR_cframe_r15[_rsp]
})

define({STORE_VIRTUAL_REGISTERS},{
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],_rax
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],r8
})

},{	dnl ASM_J9VM_ENV_DATA64

define({END_HELPER},{
	ret J9TR_pointerSize*$2
	END_PROC($1)
})

dnl	preserved: EBX EDI ESI, no XMM

define({SAVE_C_VOLATILE_REGS},{
	mov dword ptr J9TR_cframe_rax[_rsp],eax
	mov dword ptr J9TR_cframe_rcx[_rsp],ecx
	mov dword ptr J9TR_cframe_rdx[_rsp],edx
	movq qword ptr J9TR_cframe_jitFPRs+(0*8)[_rsp],xmm0
	movq qword ptr J9TR_cframe_jitFPRs+(1*8)[_rsp],xmm1
	movq qword ptr J9TR_cframe_jitFPRs+(2*8)[_rsp],xmm2
	movq qword ptr J9TR_cframe_jitFPRs+(3*8)[_rsp],xmm3
	movq qword ptr J9TR_cframe_jitFPRs+(4*8)[_rsp],xmm4
	movq qword ptr J9TR_cframe_jitFPRs+(5*8)[_rsp],xmm5
	movq qword ptr J9TR_cframe_jitFPRs+(6*8)[_rsp],xmm6
	movq qword ptr J9TR_cframe_jitFPRs+(7*8)[_rsp],xmm7
})

define({RESTORE_C_VOLATILE_REGS},{
	mov eax,dword ptr J9TR_cframe_rax[_rsp]
	mov ecx,dword ptr J9TR_cframe_rcx[_rsp]
	mov edx,dword ptr J9TR_cframe_rdx[_rsp]
	movq xmm0,qword ptr J9TR_cframe_jitFPRs+(0*8)[_rsp]
	movq xmm1,qword ptr J9TR_cframe_jitFPRs+(1*8)[_rsp]
	movq xmm2,qword ptr J9TR_cframe_jitFPRs+(2*8)[_rsp]
	movq xmm3,qword ptr J9TR_cframe_jitFPRs+(3*8)[_rsp]
	movq xmm4,qword ptr J9TR_cframe_jitFPRs+(4*8)[_rsp]
	movq xmm5,qword ptr J9TR_cframe_jitFPRs+(5*8)[_rsp]
	movq xmm6,qword ptr J9TR_cframe_jitFPRs+(6*8)[_rsp]
	movq xmm7,qword ptr J9TR_cframe_jitFPRs+(7*8)[_rsp]
})

define({SAVE_C_NONVOLATILE_REGS},{
	mov dword ptr J9TR_cframe_rbx[_rsp],ebx
	mov dword ptr J9TR_cframe_rdi[_rsp],edi
	mov dword ptr J9TR_cframe_rsi[_rsp],esi
})

define({RESTORE_C_NONVOLATILE_REGS},{
	mov ebx,dword ptr J9TR_cframe_rbx[_rsp]
	mov edi,dword ptr J9TR_cframe_rdi[_rsp]
	mov esi,dword ptr J9TR_cframe_rsi[_rsp]
})

define({SAVE_PRESERVED_REGS},{
	mov dword ptr J9TR_cframe_rbx[_rsp],ebx
	mov dword ptr J9TR_cframe_rcx[_rsp],ecx
	mov dword ptr J9TR_cframe_rsi[_rsp],esi
})

define({RESTORE_PRESERVED_REGS},{
	mov ebx,dword ptr J9TR_cframe_rbx[_rsp]
	mov ecx,dword ptr J9TR_cframe_rcx[_rsp]
	mov esi,dword ptr J9TR_cframe_rsi[_rsp]
})

define({STORE_VIRTUAL_REGISTERS},{
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],_rax
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],_rdx
})

})	dnl ASM_J9VM_ENV_DATA64

ifdef({FASTCALL_SYMBOL},,{define({FASTCALL_SYMBOL},{$1})})

ifdef({FASTCALL_INDIRECT_WITH_VMTHREAD},,{define({FASTCALL_INDIRECT_WITH_VMTHREAD},{CALL_C_WITH_VMTHREAD($1,$2)})})

ifdef({FASTCALL_EXTERN},,{define({FASTCALL_EXTERN},{DECLARE_EXTERN(FASTCALL_SYMBOL($1,$2))})})

ifdef({SAVE_C_NONVOLATILE_JIT_FP_ARG_REGS},,{define({SAVE_C_NONVOLATILE_JIT_FP_ARG_REGS},{})})

define({SAVE_ALL_REGS},{
	SAVE_C_VOLATILE_REGS($1)
	SAVE_C_NONVOLATILE_REGS($1)
	SAVE_C_NONVOLATILE_JIT_FP_ARG_REGS
})

define({RESTORE_ALL_REGS},{
	RESTORE_C_VOLATILE_REGS($1)
	RESTORE_C_NONVOLATILE_REGS($1)
})

define({BEGIN_HELPER},{START_PROC($1)})
