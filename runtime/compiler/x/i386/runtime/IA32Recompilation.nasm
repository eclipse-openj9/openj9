; Copyright (c) 2000, 2019 IBM Corp. and others
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

%ifndef TR_HOST_64BIT

        %include "jilconsts.inc"

segment .text

        DECLARE_EXTERN jitRetranslateMethod
        DECLARE_EXTERN induceRecompilation_unwrapper
        DECLARE_EXTERN initialInvokeExactThunk_unwrapper
        DECLARE_EXTERN jitCallCFunction

        DECLARE_GLOBAL countingRecompileMethod
        DECLARE_GLOBAL samplingRecompileMethod
        DECLARE_GLOBAL countingPatchCallSite
        DECLARE_GLOBAL samplingPatchCallSite
        DECLARE_GLOBAL induceRecompilation
        DECLARE_GLOBAL initialInvokeExactThunkGlue


; Offsets for induceRecompilationSnippet
eq_induceSnippet_startPCOffset  equ 5

; Offsets from esp for sampling
;
eq_stack_samplingBodyInfo	equ 0
eq_stack_samplingCodeStart	equ 8

; Offsets from esp for counting
eq_stack_countingStartPCToBodyInfo    equ -8
eq_stack_countingStartPCToMethodStart equ 13

eq_BodyInfo_MethodInfo          equ 4
eq_MethodInfo_J9Method          equ 0
eq_MethodInfo_Flags             equ 4
eq_MethodInfo_HasBeenReplaced   equ 0100000h

;
        align 16
;
; [esp] contains address of recompilation info:
;   0:  offset to start of method (dec/cmp)
;
countingRecompileMethod:
;
        pop     edi

        push    edx

        ; TODO: This slot on the stack is no longer required and should be removed.
                ;
        push    0
        
        ; repush senderPC
        push    dword  [esp+8]

        mov     eax, edi
        add     eax, dword [edi]
        push    eax                     ; Old start address
        mov     eax, dword [eax+eq_stack_countingStartPCToBodyInfo] ; Body info
        mov     edx, dword [eax+eq_BodyInfo_MethodInfo] ; Method info
        mov     eax, dword [edx+eq_MethodInfo_J9Method]
        push    eax                     ; J9 method
        CallHelperUseReg jitRetranslateMethod,eax

        test    eax, eax
        jnz     countingGotStartAddress

        ; If the compilation hasn't been done yet, skip the counting
        ; code at the start of the method and continue with the rest of
        ; the method body
        mov     eax, edi
        add     eax, dword [edi]     ; start of method
        test    dword  [edx+eq_MethodInfo_Flags], eq_MethodInfo_HasBeenReplaced
        jnz     countingGotStartAddress ; HCR: old body is invalid; must execute revert-to-interpreter code at start of method
        add     eax, eq_stack_countingStartPCToMethodStart	; skip the sub/jl (or cmp/jl)
countingGotStartAddress:

        ; Now the new start address is in eax.
        mov     edi, eax
        add     esp, 4
        pop     edx
        
        ; Note: If the JIT linkage ever expects EAX to be loaded with the receiver on
        ; method entry then the above code will have to rematerialize EAX before passing
        ; control to the method below.
        
        jmp     edi
;
;



;
        align 16
;
; [esp] contains address of recompilation info:
;   0:  address of persistent method info
;
samplingRecompileMethod:
;
        pop     edi
        
        ; TODO: These two slots on the stack are no longer required and should be removed.
                ;
        push    0
        push    0

        ; repush senderPC
        push    dword  [esp+8]

        ; Get the old start address and push it
        lea     eax, [edi+eq_stack_samplingCodeStart]
        push    eax

                ; Get the J9Method and push it
        mov     eax, dword [edi+eq_stack_samplingBodyInfo] ; Body Info
        mov     eax, dword [eax+eq_BodyInfo_MethodInfo] ; Method Info
        mov     eax, dword [eax+eq_MethodInfo_J9Method]
        push    eax
        CallHelperUseReg jitRetranslateMethod,eax

        ; If the compilation has not been done yet, restart this method.
                ; It should now execute normally
        test    eax, eax
        jnz     samplingGotStartAddress
        lea     edi, [edi+eq_stack_samplingCodeStart]
                add     esp, 8

                ; Note: If the JIT linkage ever expects EAX to be loaded with the receiver on
                ; method entry then the above code will have to rematerialize EAX before passing
                ; control to the method below.

        jmp 	edi

samplingGotStartAddress:
        mov 	edi, eax
                add     esp, 8

                ; Note: If the JIT linkage ever expects EAX to be loaded with the receiver on
                ; method entry then the above code will have to rematerialize EAX before passing
                ; control to the method below.

        jmp     edi
;
;


                align 16
;
induceRecompilation:
;
                xchg    edi, [esp] ; Return address in snippet
                push    eax        ; Preserve

                ; old startPC
                mov     eax, dword [edi+eq_induceSnippet_startPCOffset]
                add     eax, edi

                ; set up args to induceRecompilation_unwrapper
                push    ebp        ; parm: vmThread
                push    eax        ; parm: startPC

                ; set up args to jitCallCFunction (right-to-left)
        mov     eax, esp
                push    eax                                    ; parm: result pointer; don't care
        push    eax                                    ; parm: args array
                MoveHelper eax, induceRecompilation_unwrapper  ; parm: C function to call
        push    eax

                CallHelper jitCallCFunction
                add     esp, 8

                ; restore regs and return to snippet
                pop     eax
                xchg    edi, [esp]
                ret

;
;


        align 16
;
; [esp] contains (old start PC + 5)
;
countingPatchCallSite:
;
        xchg    dword [esp], edi

                ; These two pushes *should not* be required any longer as they are done to preserve the
                ; registers for an IPICDispatch.
        push    edx
        push    eax

        ; Get the old start address
        sub     edi, 5

        ; Get the new start address and push it
        mov     eax, dword [edi+eq_stack_countingStartPCToBodyInfo] ; Body info
        mov	eax, dword [eax+eq_BodyInfo_MethodInfo]
        mov     eax, dword [eax+eq_MethodInfo_J9Method]
        mov     eax, dword [eax+J9TR_MethodPCStartOffset]
        test    eax, 1 ; HCR: Has method been replaced by one that's not jitted yet?
        jne     countingPatchToRecompile
        push    eax

        ; Get the call site
        mov     edx, dword [esp+16]

patchCallSite:
        ; Check for a call immediate
        cmp     byte [edx-5], 0e8h
        jne     patchDone

        ; Check for a direct call to the method
        mov     eax, dword [edx-4]
        add     eax, edx
        cmp     eax, edi
        jne     patchDone

        ; Call site is a call immediate. Patch it with the new address
        mov     eax, dword [esp]
        sub     eax, edx
        mov     dword [edx-4], eax

patchDone:
        ; Must preserve eax and edx as they were on entry since this may go to
        ; another patch point which will expect eax and edx to be set up
        ; properly for the IPicDispatch case
                ;
                ; TODO: This *should not* be required because patchCallSite does not
                ; patch vtable slots any longer.
                ;
        pop     edi
        pop     eax
        pop     edx
        add     esp, 4                  ; was pushed as caller's edi
        jmp     edi

countingPatchToRecompile:
        ; HCR: we've got our hands on a new j9method that hasn't been compiled yet,
        ; so there's no way to patch the call site without first compiling the new method
        ;
        ; edi points to this:
        ;   CALL  patchCallSite        (5 bytes)
        ;   DB    ??                   (2 byte offset to oldStartPC)
        ;   JL    recompilationSnippet   (always 6 bytes)
        ;
        ; Restore registers and stack the way they looked originally, then jump to the recompilationSnippet
        pop     eax
        pop     edx
        add     edi, 5+2+6             ; end of JL instruction
        add     edi, dword  [edi-4] ; start of recompilationSnippet
        xchg    dword [esp], edi    ; restore edi
        ret                            ; jump to recompilationSnippet while minimizing damage to return address branch prediction
        
;

        align 16
;
; [esp] contains address of patch info:
;   0:  persistent method info
;   4:  3-byte padding
;   7:  slot for return info
;  11:  old code start
;
;
samplingPatchCallSite:
;
        xchg    dword [esp], edi
        push    edx
        push    eax

        ; Get the new start address from the method info and push it
        mov     eax, dword [edi+eq_stack_samplingBodyInfo]
        mov     eax, dword [eax+eq_BodyInfo_MethodInfo]
        mov     eax, dword [eax+eq_MethodInfo_J9Method]
        mov     eax, dword [eax+J9TR_MethodPCStartOffset]
        test    eax, 1 ; HCR: Has method been replaced by one that's not jitted yet?
        jne     samplingPatchToRecompile
        push    eax

        ; Get the old start address
        add     edi, eq_stack_samplingCodeStart

        ; Get the call site
        mov     edx, dword [esp+16]

                jmp     patchCallSite

samplingPatchToRecompile:
        ; HCR: we've got our hands on a new j9method that hasn't been compiled yet,
        ; so there's no way to patch the call site without first compiling the new method
        ;
        ; Make the world look the way it should for a call to samplingRecompileMethod
        pop     eax
        pop     edx
        xchg    dword [esp], edi
        jmp     samplingRecompileMethod
;

                align 16
;
initialInvokeExactThunkGlue:
;
        ; preserve all non-scratch regs
        push    eax
        push    edi ; use this as a temporary here.  It's volatile, and we can't change eax because it has the receiver.  In fact we may not need to save it!

        ; set up args to initialInvokeExactThunk_unwrapper
        push    ebp        ; parm: vmThread
        push    eax        ; parm: receiver MethodHandle; also, result goes here

        ; set up args to jitCallCFunction (right-to-left)
        mov     edi, esp
        push    edi                                             ; parm: args array
        push    edi                                             ; parm: result pointer
        MoveHelper edi, initialInvokeExactThunk_unwrapper       ; parm: C function to call
        push    edi
        CallHelper jitCallCFunction
        
        pop     eax ; returned startPC
        pop     edi ; discard pushed ebp

        pop     edi ; Restore

        ; Restore eax and jump to address returned by initialInvokeExactThunk
        ; Sadly, this probably kills return address stack branch prediction
        xchg    eax, [esp]
        ret
;
;


%else
segment .data
IA32Recompilation:
                db      00h
%endif
