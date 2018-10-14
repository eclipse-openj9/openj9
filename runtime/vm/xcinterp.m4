dnl Copyright (c) 2017, 2018 IBM Corp. and others
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

include(xhelpers.m4)

	FILE_START

	DECLARE_PUBLIC(cInterpreter)

ifdef({ASM_J9VM_ENV_DATA64},{

ifdef({WIN32},{

dnl Windows 64

define({C_PROLOGUE},{
	push rbp
	mov rbp,rsp
	push rbx
	push rsi
	push rdi
	push r12
	push r13
	push r14
	push r15
	sub rsp,CINTERP_STACK_SIZE
	movdqa (J9TR_cframe_preservedFPRs+(16*0))[RSP],xmm6
	movdqa (J9TR_cframe_preservedFPRs+(16*1))[RSP],xmm7
	movdqa (J9TR_cframe_preservedFPRs+(16*2))[RSP],xmm8
	movdqa (J9TR_cframe_preservedFPRs+(16*3))[RSP],xmm9
	movdqa (J9TR_cframe_preservedFPRs+(16*4))[RSP],xmm10
	movdqa (J9TR_cframe_preservedFPRs+(16*5))[RSP],xmm11
	movdqa (J9TR_cframe_preservedFPRs+(16*6))[RSP],xmm12
	movdqa (J9TR_cframe_preservedFPRs+(16*7))[RSP],xmm13
	movdqa (J9TR_cframe_preservedFPRs+(16*8))[RSP],xmm14
	movdqa (J9TR_cframe_preservedFPRs+(16*9))[RSP],xmm15
	mov qword ptr J9TR_machineSP_machineBP[rsp],rbp
	mov rbp,rcx
})

define({C_EPILOGUE},{
	movdqa xmm6,(J9TR_cframe_preservedFPRs+(16*0))[RSP]
	movdqa xmm7,(J9TR_cframe_preservedFPRs+(16*1))[RSP]
	movdqa xmm8,(J9TR_cframe_preservedFPRs+(16*2))[RSP]
	movdqa xmm9,(J9TR_cframe_preservedFPRs+(16*3))[RSP]
	movdqa xmm10,(J9TR_cframe_preservedFPRs+(16*4))[RSP]
	movdqa xmm11,(J9TR_cframe_preservedFPRs+(16*5))[RSP]
	movdqa xmm12,(J9TR_cframe_preservedFPRs+(16*6))[RSP]
	movdqa xmm13,(J9TR_cframe_preservedFPRs+(16*7))[RSP]
	movdqa xmm14,(J9TR_cframe_preservedFPRs+(16*8))[RSP]
	movdqa xmm15,(J9TR_cframe_preservedFPRs+(16*9))[RSP]
	add rsp,CINTERP_STACK_SIZE
	pop r15
	pop r14
	pop r13
	pop r12
	pop rdi
	pop rsi
	pop rbx
	pop rbp
	ret
})

},{	dnl WIN32

dnl Linux 64

define({C_PROLOGUE},{
	push rbp
	mov rbp,rsp
	push rbx
	push r12
	push r13
	push r14
	push r15
	sub rsp,CINTERP_STACK_SIZE
	mov qword ptr J9TR_machineSP_machineBP[rsp],rbp
	mov rbp,rdi
})

define({C_EPILOGUE},{
	add rsp,CINTERP_STACK_SIZE
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret
})

})	dnl WIN32

},{	dnl ASM_J9VM_ENV_DATA64

dnl Windows and Linux 32

define({C_PROLOGUE},{
	push ebp
	mov ebp,esp
dnl force 16-byte stack alignment
	and esp,-16
	push esi
	push edi
	push ebx
	sub esp,CINTERP_STACK_SIZE
	mov dword ptr J9TR_machineSP_machineBP[esp],ebp
	mov ebp,(2*J9TR_pointerSize)[ebp]
})

define({C_EPILOGUE},{
	mov ebp,dword ptr J9TR_machineSP_machineBP[esp]
	add esp,CINTERP_STACK_SIZE
	pop ebx
	pop edi
	pop esi
	mov esp,ebp
	pop ebp
	ret
})

ifdef({WIN32},{

dnl Windows 32

START_PROC(getFS0)
	mov eax,dword ptr fs:[0]
	ret
END_PROC(getFS0)

START_PROC(setFS0)
	mov eax,dword ptr 4[esp]
	mov dword ptr fs:[0],eax
	ret
END_PROC(setFS0)

})	dnl WIN32

})	dnl ASM_J9VM_ENV_DATA64

START_PROC(c_cInterpreter)
	C_PROLOGUE
	mov _rax,uword ptr J9TR_VMThread_entryLocalStorage[_rbp]
	mov uword ptr J9TR_ELS_machineSPSaveSlot[_rax],_rsp
	lea _rbx,J9TR_cframe_jitGPRs[_rsp]
	mov uword ptr J9TR_ELS_jitGlobalStorageBase[_rax],_rbx
	lea _rbx,J9TR_cframe_jitFPRs[_rsp]
	mov uword ptr J9TR_ELS_jitFPRegisterStorageBase[_rax],_rbx
C_FUNCTION_SYMBOL(cInterpreter):
	mov _rax,uword ptr J9TR_VMThread_javaVM[_rbp]
	CALL_C_ADDR_WITH_VMTHREAD(uword ptr J9TR_JavaVM_bytecodeLoop[_rax],0)
	cmp _rax,J9TR_bcloop_exit_interpreter
	je SHORT_JMP cInterpExit
	RESTORE_PRESERVED_REGS
	SWITCH_TO_JAVA_STACK
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
cInterpExit:
	C_EPILOGUE
END_PROC(c_cInterpreter)

	FILE_END
