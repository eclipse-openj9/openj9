; Copyright (c) 2000, 2018 IBM Corp. and others
;
; This program and the accompanying materials are made available under
; the terms of the Eclipse Public License 2.0 which accompanies this
; distribution and is available at https://www.eclipse.org/legal/epl-2.0/
; or the Apache License, Version 2.0 which accompanies this distribution and
; is available at https://www.apache.org/licenses/LICENSE-2.0.
;
; This Source Code may also be made available under the following
; Secondary Licenses when the conditions for such availability set
; forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
; General Public License, version 2 with the GNU Classpath
; Exception [1] and GNU General Public License, version 2 with the
; OpenJDK Assembly Exception [2].
;
; [1] https://www.gnu.org/software/classpath/license.html
; [2] http://openjdk.java.net/legal/assembly-exception.html
;
; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
        
%ifdef TR_HOST_64BIT

%include "jilconsts.inc"

            DECLARE_EXTERN jitRetranslateMethod
            DECLARE_EXTERN jitCallCFunction
            DECLARE_EXTERN induceRecompilation_unwrapper
            DECLARE_EXTERN mcc_AMD64callPointPatching_unwrapper
            DECLARE_EXTERN initialInvokeExactThunk_unwrapper
            DECLARE_EXTERN methodHandleJ2I_unwrapper

            DECLARE_GLOBAL countingRecompileMethod
            DECLARE_GLOBAL samplingRecompileMethod
            DECLARE_GLOBAL countingPatchCallSite
            DECLARE_GLOBAL samplingPatchCallSite
            DECLARE_GLOBAL induceRecompilation
            DECLARE_GLOBAL initialInvokeExactThunkGlue
            DECLARE_GLOBAL methodHandleJ2IGlue

segment .text

; Offsets for sampling
eq_stack_samplingBodyInfo            equ  0
eq_retAddr_startPC                   equ  12

; Offsets for counting
eq_stack_countingJitEntryToPrologue  equ  19
eq_countingSnippet_startPCOffset     equ  0

; Offsets for induceRecompilationSnippet
eq_induceSnippet_startPCOffset       equ  5

; Common offsets
eq_startPC_BodyInfo                  equ -12
eq_startPC_JitEntryOffset            equ -2
eq_startPC_LinkageInfo               equ -4
eq_startPC_OriginalFirstTwoBytes     equ -19
eq_BodyInfo_MethodInfo               equ  8
eq_MethodInfo_J9Method               equ  0
eq_MethodInfo_Flags                  equ 8
eq_MethodInfo_HasBeenReplaced        equ 0100000h
eq_stack_senderPC                    equ  96
eq_HasFailedRecompilation            equ  80h ; Flag bit, defined in codert.dev/Runtime.hpp

%macro saveRegs 0
                ; XMMs
                sub     rsp, 64 ; Reserve space for XMMs
                ; Do the writes in-order so we don't defeat the cache
                movsd   qword  [rsp+0],  xmm8
                movsd   qword  [rsp+8],  xmm9
                movsd   qword  [rsp+16], xmm10
                movsd   qword  [rsp+24], xmm11
                movsd   qword  [rsp+32], xmm12
                movsd   qword  [rsp+40], xmm13
                movsd   qword  [rsp+48], xmm14
                movsd   qword  [rsp+56], xmm15
                ; GPRs
                push    rax     ; arg0
                push    rsi     ; arg1
                push    rdx     ; arg2
                push    rcx     ; arg3
%endmacro

exitRecompileMethod: ; PROC
                ; GPRs
                pop     rcx     ; arg3
                pop     rdx     ; arg2
                pop     rsi     ; arg1
                pop     rax     ; arg0
                ; XMMs
                ; Do the reads in-order so we don't defeat the cache
                movsd   xmm8,  qword  [rsp+0]
                movsd   xmm9,  qword  [rsp+8]
                movsd   xmm10, qword  [rsp+16]
                movsd   xmm11, qword  [rsp+24]
                movsd   xmm12, qword  [rsp+32]
                movsd   xmm13, qword  [rsp+40]
                movsd   xmm14, qword  [rsp+48]
                movsd   xmm15, qword  [rsp+56]
                add     rsp,   64
                ; Branch to desired target
                jmp     rdi
ret ;exitRecompileMethod endp

;
                align 16
;
countingRecompileMethod:
;
                pop     rdi ; Return address in snippet
                saveRegs

                ; senderPC (= call site)
                mov     rdx, qword [rsp+eq_stack_senderPC]

                ; old startPC
                movsxd  rsi, dword [rdi+eq_countingSnippet_startPCOffset]
                add     rsi, rdi

                ; J9Method
                mov     rax, qword [rsi+eq_startPC_BodyInfo]
                mov	rax, qword [rax+eq_BodyInfo_MethodInfo]
                mov     rax, qword [rax+eq_MethodInfo_J9Method]

                CallHelperUseReg jitRetranslateMethod,rax
                test    rax, rax
                jnz     countingGotStartAddress

                ; If the compilation hasn't been done yet, skip the counting
                ; code at the start of the method and continue with the rest of
                ; the method body
                movsxd  rax, dword [rdi+eq_countingSnippet_startPCOffset]
                add     rdi, rax
                movsx   rax, word  [rdi+eq_startPC_JitEntryOffset]
                add     rdi, rax
                add     rdi, eq_stack_countingJitEntryToPrologue        ; skip the sub/jl (or cmp/jl)
                jmp     exitRecompileMethod

countingGotStartAddress:

                ; Now the new startPC is in rax.
                movsx   rdi, word  [rax+eq_startPC_JitEntryOffset]
                add     rdi, rax
                jmp     exitRecompileMethod
;
ret
;
;



;
                align 16
;
samplingRecompileMethod:
;
                pop     rdi ; Return address in preprologue
                saveRegs

                ; senderPC (= call site)
                mov     rdx, qword [rsp+eq_stack_senderPC]

                ; old startPC
                lea     rsi, [rdi+eq_retAddr_startPC]

                ; J9Method
                mov     rax, qword [rdi+eq_stack_samplingBodyInfo]
                mov	rax, qword [rax+eq_BodyInfo_MethodInfo]
                mov     rax, qword [rax+eq_MethodInfo_J9Method]

                CallHelperUseReg jitRetranslateMethod,rax

                ; If the compilation has not been done yet, restart this method.
                ; It should now execute normally
                test    rax, rax
                jnz     samplingGotStartAddress
                movsx   rdi, word  [rsi+eq_startPC_JitEntryOffset]
                add     rdi, rsi
                jmp     exitRecompileMethod

samplingGotStartAddress:

                ; Now the new startPC is in rax.
                movsx   rdi, word  [rax+eq_startPC_JitEntryOffset]
                add     rdi, rax
                jmp     exitRecompileMethod
;
;
ret
;


                align 16
;
induceRecompilation:
;
                xchg    rdi, [rsp] ; Return address in snippet
                push    rsi        ; Preserve
                push    rax        ; Preserve

                ; old startPC
                movsxd  rsi, dword [rdi+eq_induceSnippet_startPCOffset]
                add     rsi, rdi

                ; set up args to induceRecompilation_unwrapper
                push    rbp        ; parm: vmThread
                push    rsi        ; parm: startPC

                ; set up args to jitCallCFunction
                MoveHelper rax, induceRecompilation_unwrapper  ; parm: C function to call
                mov     rsi, rsp                               ; parm: args array
                                                               ; parm: result pointer; don't care
                CallHelper jitCallCFunction
                add     rsp, 16

                ; restore regs and return to snippet
                pop     rax
                pop     rsi
                xchg    rdi, [rsp]
                ret

;
;
ret
;

                align 16
;
countingPatchCallSite:
;
                xchg    qword [rsp], rdi
                push    rdx
                push    rax

                ; Compute the old startPC
                mov     rax, rdi
                movsx   rdx, word [rdi]
                sub     rax, rdx

                ; Assume:
                ;    rax == old startPC
                ;    rdi == return address in preprologue
                ;    old rdi, rdx, rax on stack
                ;    no return address on stack

                push    rdi ; HCR: We may need this
                ; rdi = new jit entry point
                mov     rdi, qword [rax+eq_startPC_BodyInfo]
                mov	rdi, qword [rdi+eq_BodyInfo_MethodInfo]
                mov     rdi, qword [rdi+eq_MethodInfo_J9Method]
                mov     rdi, qword [rdi+J9TR_MethodPCStartOffset]   ; rdi = new startPC

                test    rdi, 1h ; HCR: Has method been replaced by one that's not jitted yet?
                jne     countingPatchToRecompile ;
                add     rsp, 8 ; Turns out we don't need the preprologue return address after all



patchCallSite:
                movsx   rdx, word  [rdi+eq_startPC_JitEntryOffset]  ; rdx = jitEntryOffset
                add     rdi, rdx                                       ; rdi = new jit entry point

                ; rdx = call site return address
                mov     rdx, qword [rsp+24]

                ; Check if it's a call immediate
                cmp     byte [rdx-5], 0e8h
                jne     finishedPatchCallSite


                ; Call MCC service to do the call site patching. Argument layout:
                ; rsp+24    extraArg
                ; rsp+16    newPC
                ; rsp+8     callSite
                ; rsp       j9method
                ;
                push    rsi                                          ; preserve rsi
                xor     rsi, rsi
                push    rsi                                          ; extraArg
                push    rax                                          ; pass old startPC to mcc_AMD64callPointPatching_unwrapper
                mov     rsi, qword [rax+eq_startPC_BodyInfo]
                mov     rsi, qword [rsi+eq_BodyInfo_MethodInfo]
                mov     rax, qword [rsi+eq_MethodInfo_J9Method]   ; rax = j9method
                mov     rsi, qword [rax+J9TR_MethodPCStartOffset] ; rsi = new startPC
                sub     rdx, 5                                       ; call instruction
                push    rbp                                          ; vmThread
                push    rsi                                          ; new startPC
                push    rdx                                          ; call instruction
                push    rax                                          ; j9method
                MoveHelper rax, mcc_AMD64callPointPatching_unwrapper
                lea     rsi, [rsp]
                lea     rdx, [rsp]
                call    jitCallCFunction
                add     rsp, 48
                pop     rsi
                
finishedPatchCallSite:
                ; Assume:
                ;    rdi == new jit entry
                ;    old rdi, rdx, rax on stack
                ;    no return address on stack

                ; Restore regs, "return" to the new jit entry
                pop     rax
                pop     rdx
                xchg    qword [rsp], rdi
                ret

countingPatchToRecompile:
                ; HCR: we've got our hands on a new j9method that hasn't been compiled yet,
                ; so there's no way to patch the call site without first compiling the new method
                ;
                ; edi points to the DB here:
                ;   CALL  patchCallSite          (5 bytes)
                ;   DB    ??                     (8 bytes of junk)
                ;   JL    recompilationSnippet   (always 6 bytes)
                ;
                ; Restore registers and stack the way they looked originally, then jump to the recompilationSnippet
                pop     rdi
                pop     rax
                add     rdi, 8+6               ; end of JL instruction
                movsxd  rdx, dword  [rdi-4] ; offset to recompilationSnippet
                add     rdi, rdx               ; start of recompilationSnippet
                pop     rdx
                xchg    qword [rsp], rdi    ; restore edi
                ret                            ; jump to recompilationSnippet while minimizing damage to return address branch prediction
                
;
;
ret
;


                align 16
;
samplingPatchCallSite:
;
                xchg    qword [rsp], rdi
                push    rdx
                push    rax

                ; Compute the old startPC
                lea     rax, [rdi+eq_retAddr_startPC]


                ; Assume:
                ;    rax == old startPC
                ;    rdi == return address in preprologue
                ;    old rdi, rdx, rax on stack
                ;    no return address on stack

                push    rdi ; HCR: We may need this
                ; rdi = new jit entry point
                mov     rdi, qword [rax+eq_startPC_BodyInfo]
                mov	rdi, qword [rdi+eq_BodyInfo_MethodInfo]
                mov     rdi, qword [rdi+eq_MethodInfo_J9Method]
                mov     rdi, qword [rdi+J9TR_MethodPCStartOffset]   ; rdi = new startPC

                test    rdi, 1h ; HCR: Has method been replaced by one that's not jitted yet?
                jne     samplingPatchToRecompile ;
                add     rsp, 8 ; Turns out we don't need the preprologue return address after all


                jmp     patchCallSite

samplingPatchToRecompile:
                ; HCR: we've got our hands on a new j9method that hasn't been compiled yet,
                ; so there's no way to patch the call site without first compiling the new method
                ;
                ; Make the world look the way it should for a call to samplingRecompileMethod
                pop     rdi
                pop     rax
                pop     rdx
                xchg    qword [rsp], rdi
                jmp     samplingRecompileMethod
;
;
ret
;

                align 16
;
initialInvokeExactThunkGlue:
;
                ; preserve all non-scratch regs
                push    rax
                push    rsi
                push    rdx

                ; set up args to initialInvokeExactThunk_unwrapper
                push    rbp     ; parm: vmThread
                push    rax     ; parm: receiver MethodHandle; also, result goes here

                ; set up args to jitCallCFunction
                MoveHelper rax, initialInvokeExactThunk_unwrapper       ; parm: C function to call
                mov     rsi, rsp                                        ; parm: args array
                mov     rdx, rsp                                        ; parm: result pointer
                CallHelper jitCallCFunction
                pop     rax      ; result jitted entry point
                pop     rbp

                ; restore regs
                pop     rdx
                pop     rsi

                ; Restore rax and jump to address returned by initialInvokeExactThunk
                ; Sadly, this probably kills return address stack branch prediction
                ; TODO:JSR292: check for null and call vm helper to interpret instead
                xchg    rax, [rsp]
                ret
;
;
ret
;

                align 16
;
methodHandleJ2IGlue:
;
                ; Note: this glue is not called, it is jumped to.  There is no
                ; return address on the stack.

                ; preserve all non-scratch regs
                push    rax
                push    rsi
                push    rdx

                ; set up args to initialInvokeExactThunk_unwrapper
                push    rbp     ; parm: vmThread
                lea     rsi, [rsp+40]
                push    rsi     ; parm: stack pointer before the call to the j2i thunk
                push    rax     ; parm: receiver MethodHandle; also, result goes here

                ; set up args to jitCallCFunction
                MoveHelper rax, methodHandleJ2I_unwrapper               ; parm: C function to call
                mov     rsi, rsp                                        ; parm: args array
                mov     rdx, rsp                                        ; parm: result pointer
                CallHelper jitCallCFunction
                pop     rax      ; result (currently unused)
                pop     rsi
                pop     rbp

                ; restore regs
                pop     rdx
                pop     rsi
                pop     rax

                ; continue J2I
                jmp     rdi
;
;
ret
;

%else ; TR_HOST_64BIT

                CPU P2

segment .data
AMD64PicBuilder:
                db      00h

%endif ;TR_HOST_64BIT
