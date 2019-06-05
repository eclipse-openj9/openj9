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

; --------------------------------------------------------------------------------
;
; 32-BIT
;
; --------------------------------------------------------------------------------

      eq_offsetof_J9Object_clazz equ 8                            ; offset of class pointer in a J9Object

      %include "jilconsts.inc"
      %include "X86PicBuilder.inc"

      segment .text

      DECLARE_GLOBAL interpreterUnresolvedStaticGlue
      DECLARE_GLOBAL interpreterUnresolvedSpecialGlue
      DECLARE_GLOBAL interpreterStaticAndSpecialGlue

      DECLARE_GLOBAL interpreterUnresolvedStringGlue
      DECLARE_GLOBAL interpreterUnresolvedMethodTypeGlue
      DECLARE_GLOBAL interpreterUnresolvedMethodHandleGlue
      DECLARE_GLOBAL interpreterUnresolvedCallSiteTableEntryGlue
      DECLARE_GLOBAL interpreterUnresolvedMethodTypeTableEntryGlue
      DECLARE_GLOBAL interpreterUnresolvedClassGlue
      DECLARE_GLOBAL interpreterUnresolvedClassFromStaticFieldGlue
      DECLARE_GLOBAL interpreterUnresolvedStaticFieldGlue
      DECLARE_GLOBAL interpreterUnresolvedStaticFieldSetterGlue
      DECLARE_GLOBAL interpreterUnresolvedFieldGlue
      DECLARE_GLOBAL interpreterUnresolvedFieldSetterGlue
      DECLARE_GLOBAL interpreterUnresolvedConstantDynamicGlue

      DECLARE_GLOBAL MTUnresolvedInt32Load
      DECLARE_GLOBAL MTUnresolvedInt64Load
      DECLARE_GLOBAL MTUnresolvedFloatLoad
      DECLARE_GLOBAL MTUnresolvedDoubleLoad
      DECLARE_GLOBAL MTUnresolvedAddressLoad

      DECLARE_GLOBAL MTUnresolvedInt32Store
      DECLARE_GLOBAL MTUnresolvedInt64Store
      DECLARE_GLOBAL MTUnresolvedFloatStore
      DECLARE_GLOBAL MTUnresolvedDoubleStore
      DECLARE_GLOBAL MTUnresolvedAddressStore

      DECLARE_EXTERN j2iTransition
      DECLARE_EXTERN jitResolveStaticMethod
      DECLARE_EXTERN jitResolveSpecialMethod
      DECLARE_EXTERN jitCallCFunction
      DECLARE_EXTERN jitResolveString
      DECLARE_EXTERN jitResolveConstantDynamic
      DECLARE_EXTERN jitResolveMethodType
      DECLARE_EXTERN jitResolveMethodHandle
      DECLARE_EXTERN jitResolveInvokeDynamic
      DECLARE_EXTERN jitResolveHandleMethod
      DECLARE_EXTERN jitResolveClass
      DECLARE_EXTERN jitResolveClassFromStaticField
      DECLARE_EXTERN jitResolveStaticField
      DECLARE_EXTERN jitResolveStaticFieldSetter
      DECLARE_EXTERN jitResolveField
      DECLARE_EXTERN jitResolveFieldSetter
      ;1179
      DECLARE_EXTERN jitResolvedFieldIsVolatile

%ifdef WINDOWS
      DECLARE_EXTERN j9thread_self
      DECLARE_EXTERN j9thread_tls_get
      DECLARE_EXTERN vmThreadTLSKey
%endif

      DECLARE_EXTERN memoryFence


; interpreterUnresolved{Static|Special}Glue
;
; Resolve a static or special method. The call instruction routing control here is
; updated to load the RAM method into EDI.
;
; The unresolved {Static|Special} interpreted dispatch snippet look like:
; align 8
; (5) call interpreterUnresolved{Static|Special}Glue ; replaced with "mov edi, 0xaabbccdd"
; (3) NOP
; ---
; (5) jmp  interpreterStaticAndSpecialGlue
; (2) dw   2-byte glue method helper index
; (4) dd   cpAddr
; (4) dd   cpIndex
;
; NOTES:
;
; [1] A POP is not strictly necessary to shape the stack. The RA could be left on the
; stack and the new stack shape updated in getJitStaticMethodResolvePushes(). It
; was left this way because we have to dork with the RA anyways to get back from
; this function which will already cause a return mispredict. Leaving it as is
; changes less code.
;
; [2] STACK SHAPE: must maintain stack shape expected by call to getJitStaticMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
interpreterUnresolvedStaticGlue:
      pop         edi                                       ; RA in snippet (see [1] above)

      push        dword  [edi+14]                           ; p3) cpIndex
                                                            ; 14 = 3 (NOP) + 5 (CALL) + 2 (DW) + 4 (cpAddr)
      push        dword  [edi+10]                           ; p2) cpAddr
                                                            ; 10 = 3 (NOP) + 5 (CALL) + 2 (DW)
      push        dword  [esp+8]                            ; p1) RA in mainline code
      call        jitResolveStaticMethod

      ; The interpreter may low-tag the result to avoid populating the constant pool -- whack it and record in CF.
      ;
      btr         eax, 0
      ; Load the resolved RAM method into EDI so that the caller doesn't have to re-run patched code
      xchg        edi, eax
      ; Skip code patching if the interpreter low-tag the RAM method
      jc          .skippatching

      ; Patch the call that brought us here into a load of the resolved RAM method into EDI.
      ;
      sub         esp, 16
      movdqu      [esp], xmm0
      push        001f0f00h                                 ; NOP
      push        000000bfh                                 ; 1st byte of MOV edi, 0xaabbccdd
      mov         dword [esp+1], edi
      movsd       xmm0, qword [esp]
      movsd       qword [eax-5], xmm0
      movdqu      xmm0, [esp+8]
      add         esp, 24                                   ; 24 = 16 + 4*2 (for two pushes)
      jmp         interpreterStaticAndSpecialGlue           ; The next instruction is always "jmp interpreterStaticAndSpecialGlue",
                                                            ; jump to its target directly.
   .skippatching:
      test        byte [edi+J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit
      jnz         j2iTransition
      jmp         dword [edi+J9TR_MethodPCStartOffset]


      align 16
interpreterUnresolvedSpecialGlue:
      pop         edi                                       ; RA in snippet (see [1] above)

      push        dword  [edi+14]                           ; p3) cpIndex
                                                            ; 14 = 3 (NOP) + 5 (CALL) + 2 (DW) + 4 (cpAddr)
      push        dword  [edi+10]                           ; p2) cpAddr
                                                            ; 10 = 3 (NOP) + 5 (CALL) + 2 (DW)
      push        dword  [esp+8]                            ; p1) RA in mainline code
      call        jitResolveSpecialMethod

      ; Load the resolved RAM method into EDI so that the caller doesn't have to re-run patched code
      xchg        edi, eax

      ; Patch the call that brought us here into a load of the resolved RAM method into EDI.
      ;
      sub         esp, 16
      movdqu      [esp], xmm0
      push        001f0f00h                                 ; NOP
      push        000000bfh                                 ; 1st byte of MOV edi, 0xaabbccdd
      mov         dword [esp+1], edi
      movsd       xmm0, qword [esp]
      movsd       qword [eax-5], xmm0
      movdqu      xmm0, [esp+8]
      add         esp, 24                                   ; 24 = 16 + 4*2 (for two pushes)
      jmp         interpreterStaticAndSpecialGlue           ; The next instruction is always "jmp interpreterStaticAndSpecialGlue",
                                                            ; jump to its target directly.


; interpreterStaticAndSpecialGlue
;
; This function is a back-patching routine for interpreter calls to static
; methods that have recently been compiled. It patches the call site (in the
; code cache) of an interpreter call snippet, so that future executions will
; invoke the compiled method instead.
;
; NOTES:
;
; [1] The RAM method is in RDI on entry. It was loaded by the preceding instruction.
;
; [2] This runtime code uses the JIT linkage registers as volatile registers and hence
; does not preserve them. This is because the eventual dispatch path will be through
; the interpreter which reads the parameters from the stack. The backspilling has
; already occured at this point.
;
      align 16
interpreterStaticAndSpecialGlue:
      test        byte [edi+J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit
      jnz         j2iTransition
      mov         edi, dword [edi+J9TR_MethodPCStartOffset]
      mov         edx, dword [esp]
      sub         edi, edx
      mov         dword [edx-4], edi
      add         edi, edx
      jmp         edi


;1179
    align 16
checkReferenceVolatility:

      ; preserve register states
      ;
      push        ebp
      push        ebx                                        ; Low dword of patch instruction in snippet
      push        ecx                                        ; High dword of patch instruction in snippet
      push        eax                                        ; The call instruction (edx:eax) that should have brought
      push        edx                                        ; us to this snippet + the following 3 bytes.
      push        esi                                        ; The RA in mainline code

      ; jitResolvedFieldIsVolatile requires us to restore ebp before calling it
      ;
      mov         ebp, dword  [esp + 28]                     ; [esp + 4 + 24]

      ; determine if the field is volatile.
      ;
      mov         ecx, dword  [esp + 152]                    ; load the cpIndex [esp + 124 + 24 + 4]
      mov         ebx, ecx
      mov         edx, ecx
      and         ebx, eq_ResolveStatic                     ; get the static flag bit
      and         ecx, eq_CPIndexMask

      ; push the parameters for the volatile check
      ;
      push        ebx                                       ; push isStatic
      push        ecx                                       ; push cpIndex
      push        dword  [esp + eq_MemFenceRAOffset32]      ; push cpAddr

      ; call the volatile check
      ;
      xor         eax, eax
      CallHelperUseReg jitResolvedFieldIsVolatile, eax

      ;clear ebp which will be used as to tell if we need to patch over a barrier in the mainline code.
      ;
      xor         ebp, ebp

      ; if this field is not volatile, patch the mfence with a nop
      ;
      test        eax, eax
      jnz         patchMainlineInstructionFromCache

      ; check to see if we are patching the lower or upper half of a long store.
      ;
      test        edx, eq_UpperLongCheck
      jnz         patchUpperLong

      test        edx, eq_LowerLongCheck
      jnz         patchLowerLong

      ; check to see if we are patching the lock cmpxchng8b of a long store.
      ;
      mov         esi, dword  [esp + 144]                   ; find the instruction in the snippet cache.
      mov         dx, word  [esi + 1]
      cmp         dx, eq_MemFenceLCXHG                      ; check if the opcode is for a lock cmpxchng
      je          patchCmpxchgForLongStore                  ; lock cmpxchng should appear ONLY for a long store


      ; we need to patch an mfence in the mainline code, set the flag
      ;
      or          ebp, 001h

      jmp         patchMainlineInstructionFromCache

patchBarrierWithNop:

      mov         esi, dword  [esp + eq_MemFenceRAOffset32] ; esp + 128 = RA of mainline code (mfence instruction or nop)

      ; patch the mfence following a non-long store.
      ; get the instruction descriptor and find the length of the store instruction
      ;
      mov         edx, dword  [esp + 144]                   ; get instruction descriptor address [esp + 116 + 24 + 4]
      mov         bl, byte  [edx]                           ; get instruction length from instruction descriptor
      and         ebx, 0f0h                                 ; mask size of instruction
      shr         ebx, 4                                    ; shift size to last nibble of byte
      lea         ebx, [ebx - eq_MemFenceCallLength32] ; get the delta between the RA in the mainline code and
                                                            ; the end of the store instruction
      lea         esi, [esi + ebx]                   ; find the end of the store instruction

      ; determine what kind of fence we are dealing with: mfence or LOCK OR [ESP] (on legacy systems)
      ;
      mov         edx, dword  [esp + 28]
      mov         edx, [edx+J9TR_VMThread_javaVM]
      mov         edx, [edx+J9TR_JavaVMJitConfig]
      mov         edx, [edx+J9TR_JitConfig_runtimeFlags]
      test        edx, J9TR_runtimeFlags_PatchingFenceType
      jz          short doLOCKORESP

      ; 3 byte memory fence
      ;
      lea         esi, [esi + 3]                     ; find the 4 byte aligned address
      and         esi, 0fffffffch                           ; should now have a 4 byte aligned instruction of the memfence

      ;make sure we are patching over an mfence (avoids potential race condition with lock cmpxchg patching)
      ;
      cmp         word  [esi], 0ae0fh
      jne         restoreRegs

      mov         ecx, 001f0f00h                            ; load the 3 byte nop instruction plus padding
      mov         cl, byte  [esi + 3]                       ; get the byte following the memory fence
      ror         ecx, 8                                    ; realign and copied byte
      mov         dword  [esi], ecx                         ; write the nop
      jmp         restoreRegs

      ; 5 byte lock or esp
      ;
doLOCKORESP:

      ; ecx:ebx nop instruction
      ; edx:eax original instruction
      ;
      mov         esi, dword  [esp + eq_MemFenceRAOffset32]         ; esp + 128 = RA of mainline code (mfence instruction or nop)
      lea         esi, [esi + ebx]                           ; find the end of the store instruction
      lea         esi, [esi + 7]                             ; find the 8 byte aligned address of the lock or [esp]
      and         esi, 0fffffff8h
      mov         eax, dword  [esi]                                 ; construct the edx:eax pair (original instruction)
      mov         edx, dword  [esi + 4]
      mov         ebx, 00441f0fh                                    ; construct the ecx:ebx pair (nop and trailing bytes)
      mov         ecx, edx
      mov         cl, 00h

      lock cmpxchg8b [esi]

      jmp restoreRegs



      ; Loading a long in 32 bit is done with a lock cmpxchng by default.
      ; We need to switch the op bits of the instructions that performs loads for the lock cmpxchng setup
      ; to stores. We also need to patch over the lock cmpxchg with nops.
      ;
patchLowerLong:

      mov         esi, dword  [esp + 144]
      mov         ebx, dword  [esi + 1]
      and         ebx, 0fffffffdh                                   ; change the mov reg mem opcode (8b) into a mov mem reg opcode (89)
      or          ebx, 000001800h                                   ; change EAX to EBX
      mov         ecx, dword  [esp + 12]
      jmp         patchMainlineInstruction


patchUpperLong:

      mov         esi, dword  [esp + 144]
      mov         ebx, dword  [esi + 1]
      and         ebx, 0ffffeffdh                                    ; change the mov reg mem opcode (8b) into a mov mem reg opcode (89)
      or          ebx, 000000800h                                    ; change EDX to ECX
      mov         ecx, dword  [esp + 12]
      jmp         patchMainlineInstruction


patchCmpxchgForLongStore:

      mov         ebx, 090666666h                                    ; setup the ecx:ebx pair (nop instruction)
      mov         ecx, 090666666h
      jmp         patchMainlineInstruction


patchMainlineInstructionFromCache:

      mov         ecx, dword  [esp + 12]                            ; Load low dword of patch instruction in snippet BEFORE volatile resolution
      mov         ebx, dword  [esp + 16]                            ; Load high dword of patch instruction in snippet BEFORE volatile resolution


patchMainlineInstruction:


      mov         esi, dword  [esp]                                 ; Restore RA in mainline code.
      mov         edx, dword  [esp + 4]                             ; Restore the call instruction (edx:eax) that should have brought
      mov         eax, dword  [esp + 8]                             ; us to this snippet + the following 3 bytes.

      lock cmpxchg8b [esi - 5]

      test        ebp, ebp
      jnz         patchBarrierWithNop

restoreRegs:

      pop         esi
      pop         edx
      pop         eax
      pop         ecx
      pop         ebx
      pop         ebp

ret


; --------------------------------------------------------------------------------
;
; 32-BIT DATA RESOLUTION
;
; --------------------------------------------------------------------------------


%macro DataResolvePrologue 0
      ; local: doneFPRpreservation, preserveX87loop

      pushfd                        ; save flags , addr=esp+28
      push        ebp               ; save register, addr=esp+24
      push        esi               ; save register, addr=esp+20
      push        edi               ; save register, addr=esp+16
      push        edx               ; save register, addr=esp+12
      push        ecx               ; save register, addr=esp+8
      push        ebx               ; save register, addr=esp+4
      push        eax               ; save register, addr=esp+0

%ifdef WINDOWS
      ; Restore the VMThread into ebp
      ;
      call        j9thread_self
      push        dword [vmThreadTLSKey]
      push        eax
      call        j9thread_tls_get
      add         esp, 8
      mov         ebp, dword  [eax+8]       ; get J9JavaVM from OMR_VMThread in tls
%endif

      mov         esi,          dword  [esp+32]      ; esi = return address in snippet
      mov         eax,          dword  [esp+36]      ; eax = cpAddr
      mov         ecx,          dword  [esp+40]      ; ecx = cpIndex
      mov         edx,          ecx                  ; edx = cpIndex

      sub         esp,          J9PreservedFPRStackSize     ; reserve stack space for all possible FPRs
      shr         edx,          24
      and         edx,          01fh                        ; isolate number of live FPRs across this resolution
      jz          short .doneFPRpreservation                ; none, so were done

      test        edx, 010h                                 ; test SSE bit
      jz          short .preserveX87loop
      movq        qword  [esp], xmm0
      movq        qword  [esp+8], xmm1
      movq        qword  [esp+16], xmm2
      movq        qword  [esp+24], xmm3
      movq        qword  [esp+32], xmm4
      movq        qword  [esp+40], xmm5
      movq        qword  [esp+48], xmm6
      movq        qword  [esp+56], xmm7
      jmp         short .doneFPRpreservation

.preserveX87loop:
      dec         edx
      lea         edi, [edx+edx*4]
      fstp        tword  [esp+edi*2]                        ; store and pop from the FP stack
      jnz         short .preserveX87loop
.doneFPRpreservation:

%endmacro


%macro DispatchUnresolvedDataHelper 1 ; arg: helper
      and         ecx, 1ffffh                               ; isolate the cpIndex
      push        dword  [esp+124]                          ; p) RA in mainline code
      push        ecx                                       ; p) cpIndex
      push        eax                                       ; p) cpAddr
      CallHelperUseReg %1,eax                               ; helper
%endmacro


%macro FPRDataResolveEpilogue 3 ; args: cpScratchReg, scratchReg, scratchReg2
      ; local restoreX87stack, restoreX87loop, doneFPRrestoration

      movzx       %1, byte [esp+123]                        ; &cpScratchReg ; load number of FPRs live across this resolution
      and         %1, 01fh                                  ; isolate number of live FPRs across this resolution
      test        %1, %1
      jz          short .doneFPRrestoration                 ; none, so leave

      test        %1, 010h                                  ; test SSE bit
      jz          short .restoreX87stack
      movq        xmm0, qword  [esp]
      movq        xmm1, qword  [esp +8]
      movq        xmm2, qword  [esp +16]
      movq        xmm3, qword  [esp +24]
      movq        xmm4, qword  [esp +32]
      movq        xmm5, qword  [esp +40]
      movq        xmm6, qword  [esp +48]
      movq        xmm7, qword  [esp +56]
      jmp         short .doneFPRrestoration

.restoreX87stack:
      lea         %2, [%1 + %1 * 4]                         ;&scratchReg, [&cpScratchReg+&cpScratchReg*4]
      lea         %3, [esp + %2*2 - 10]                     ;&scratchReg2, [esp+&scratchReg*2-10] ; first FPR preserved
      neg         %1                                        ;&cpScratchReg
.restoreX87loop:
      inc         %1                                        ;&cpScratchReg
      lea         %2, [%1 + %1 * 4]                         ;&scratchReg, [&cpScratchReg+&cpScratchReg*4]
      fld         tword [%3 + %2 * 2]                       ;[&scratchReg2+&scratchReg*2] ; restore FP stack register
      jnz         short .restoreX87loop
.doneFPRrestoration:
      add         esp, J9PreservedFPRStackSize
%endmacro



; interpreterUnresolved{*}Glue
;
; Generic code to perform runtime resolution of a data reference.  These functions
; are called from a snippet that has the following general shape:
;
;     push  cpIndex
;     push  cpAddr
;     call  interpreterUnresolved{*}Glue
;     db    high nibble (L) : length of instruction in snippet (must be 1-15)
;            low nibble (O) : offset to disp32 in patched instruction (must be 1-4)
;     dq    8 bytes of instruction to patch (includes bytes from following instruction if necessary)
;     db    N     remaining N bytes of instruction for static resolves
;     db    0xc3  RET instruction for unpatched static resolves
;     db          if static and L<8 then this is the byte over which the RET instruction
;                    is written.
;
; Spare bits in the cpIndex passed in are used to specify behaviour based on the
; kind of resolution.  The anatomy of a cpIndex:
;
;       byte 3   byte 2   byte 1   byte 0
;
;      3 222  2 222    1 1      0 0      0
;      10987654 32109876 54321098 76543210
;      |||||__| ||| ||||_________________|
;      |||| |   ||| |||         |
;      |||| |   ||| |||         +---------------- cpIndex (0-16)
;      |||| |   ||| ||+-------------------------- upper long dword resolution (17)
;      |||| |   ||| |+--------------------------- lower long dword resolution (18)
;      |||| |   ||| +---------------------------- check volatility (19)
;      |||| |   ||+------------------------------ resolve, but do not patch snippet template (21)
;      |||| |   |+------------------------------- resolve, but do not patch mainline code (22)
;      |||| |   +-------------------------------- long push instruction (23)
;      |||| +------------------------------------ number of live X87 registers across this resolution (24-27)
;      |||+-------------------------------------- has live XMM registers (28)
;      ||+--------------------------------------- static resolution (29)
;      |+---------------------------------------- 64-bit: resolved value is 8 bytes (30)
;      |                                          32-bit: resolved value is high word of long pair (30)
;      +----------------------------------------- 64 bit: extreme static memory barrier position (31)
;
; NOTES:
;
; [1] STACK SHAPE: must maintain stack shape expected by call to getJitDataResolvePushes()
;     across the resolution helper.
;
; [2] On 32-bit we will never patch more than 8 bytes in the mainline code.  Hence, the
;     resolved data must always reside within the first 8 bytes of the instruction and we
;     only need to write the first 8 bytes cached in the snippet back over the mainline
;     code.
;
      align 16
interpreterUnresolvedStringGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveString
; int 3
      jmp commonUnresolvedCode
retn

      align 16
interpreterUnresolvedConstantDynamicGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveConstantDynamic
;      int 3
      jmp commonUnresolvedCode
retn

      align 16
interpreterUnresolvedMethodTypeGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodType
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedMethodHandleGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodHandle
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedCallSiteTableEntryGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveInvokeDynamic
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedMethodTypeTableEntryGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveHandleMethod
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedClassGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClass
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedClassFromStaticFieldGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClassFromStaticField
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedStaticFieldGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticField
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedStaticFieldSetterGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticFieldSetter
; int 3
      jmp commonUnresolvedCode
retn

      align 16
interpreterUnresolvedFieldGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveField
; int 3
      jmp commonUnresolvedCode
retn


      align 16
interpreterUnresolvedFieldSetterGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveFieldSetter
; int 3

commonUnresolvedCode:

      ; STACK SHAPE:
      ;
      ;          esp+128 : RA in mainline code
      ;          esp+124 : cp index
      ;          esp+120 : cp address
      ;          esp+116 : RA in snippet
      ;          -------
      ;          esp+112 : eflags
      ;          esp+108 : ebp
      ;          esp+104 : esi
      ;          esp+100 : edi
      ;          esp+96  : edx
      ;          esp+92  : ecx
      ;          esp+88  : ebx
      ;          esp+84  : eax
      ;
      ;                |
      ;        +-------+--------+
      ;        |                |
      ;
      ; ----- XMM ----    ---- X87 ----
      ; esp+60  : xmm7     esp+74 : ST0
      ; esp+52  : xmm6     esp+64 : ST1
      ; esp+44  : xmm5     esp+54 : ST2
      ; esp+36  : xmm4     esp+44 : ST3
      ; esp+28  : xmm3     esp+34 : ST4
      ; esp+20  : xmm2     esp+24 : ST5
      ; esp+12  : xmm1     esp+14 : ST6
      ; esp+4   : xmm0     esp+4  : ST7
      ;
      ;        |                |
      ;        +-------+--------+
      ;                |
      ;
      ;            esp+0 : vmThread address


      push        ebp                           ; keep the address of the vmThread at the top of the stack
      mov         ecx, dword  [esp+124]         ; ecx = full cpIndex dword

      ; Check for the special case of a data resolution without code patching.
      ; This happens when you have an explicit NULLCHK guarding a data resolution and
      ; we need to ensure the exceptions are thrown in the correct order if they occur.
      ; In this case any resolution exceptions are required to be thrown before any NPEs.
      ;
      test        ecx, eq_ResolveWithoutPatchingMainline
      jnz         doneDataResolution

      ; Check if the snippet instruction must be patched. This is not necessary, for
      ; example, for unresolved strings because the snippet instruction is already
      ; correct since it contains the address of the constant pool entry from which
      ; the java/lang/Strings address can now be loaded.
      ;
      test        ecx, eq_ResolveWithoutPatchingSnippet
      jnz         disp32AlreadyPatched

      mov         edx, dword  [esp+116]         ; edx = EA of header byte in snippet

      ; Patch the cached instruction bytes in the snippet. The disp32 field must lie within
      ; the first 8 bytes of the instruction. Valid default values for the disp32 field
      ; are 0, 4, or an address.
      ;
      ; Multiple threads can be executing this code, but all threads must be patching the
      ; same value.
      ;
      movzx       edx, byte  [edx]              ; header byte
      and         edx, 0fh                      ; isolate disp32 offset in first nibble
      mov         edi, eax                      ; edi = resolve result
      and         edi, 0fffffffeh               ; whack any low tagging of resolve result
      test        ecx, eq_HighWordOfLongPair    ; is this the high word of a long pair?
      jz          short patchDisp32
      add         edi, 4                        ; yes, offset address by 4 bytes

patchDisp32:
      mov         dword  [esi+edx+1], edi       ; patch the disp32 field in the snippet
      call        memoryFence                  ; make sure all stores crossing a line are issued

disp32AlreadyPatched:

      ; For static resolves if the address from the resolution helper is low tagged then
      ; this means that class initialization has not completed yet and we should not
      ; be patching the mainline code (in case the remaining initialization were to fail).
      ;
      ; We could probably get away with just testing whether the resolve result was low-tagged
      ; in order to do this.
      ;
      mov         ebp, dword  [esp+116]         ; ebp = EA of header byte in snippet
      test        ecx, eq_ResolveStatic         ; static resolve?
      jz          short patchMainlineCode
      test        eax, 1                        ; low-tagged resolution result (clinit not finished)?
      jnz         executeSnippetCode

patchMainlineCode:

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this snippet + the following 3 bytes.
      ;
      lea         eax, [ebp-12]                 ; assume short cpIndex push
      test        ecx, eq_WideCPIndexPush
      jz          short shortCPIndexPush
      lea         eax, [eax-3]                  ; account for wide cpIndex push

shortCPIndexPush:
      mov         esi, dword  [esp+128]         ; edx = RA in mainline
      mov         edx, dword  [esi-1]           ; edx = 3 bytes after the call to snippet + high byte of disp32
      sub         eax, esi                      ; calculate disp32
      rol         eax, 8
      mov         dl, al                        ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                      ; add CALL opcode

      test        ecx, eq_ResolveStatic         ; static resolve?
      jnz         patchUnresolvedStatic

loadTargetInstruction:

      ; Load the patched data reference instruction in ecx:ebx.
      ;
      mov         ebx, dword  [ebp+1]           ; low dword of patch instruction in snippet
      mov         ecx, dword  [ebp+5]           ; high dword of patch instruction in snippet

mergePatchUnresolvedDataReference:

; determine if nop patching might be necessary
; if a volatile check is necessary, code patching will occur within the check.
;
      test        dword  [esp+124], eq_VolatilityCheck ; get the cpIndex
      jz          short noVolatileCheck

      call        checkReferenceVolatility
      jmp         doneDataResolution

noVolatileCheck:

      ; Attempt to patch the code.
      ;

      lock cmpxchg8b [esi-5]

doneDataResolution:

      pop         eax                           ; remove the vmThread address from the top of the stack
      FPRDataResolveEpilogue ecx, edi, edx

      pop         eax                           ; restore
      pop         ebx                           ; restore
      pop         ecx                           ; restore
      pop         edx                           ; restore
      pop         edi                           ; restore
      pop         esi                           ; restore
      pop         ebp                           ; restore
      add         dword  [esp+16], -5           ; re-run patched instruction
      popfd                                     ; restore
      lea         esp, [esp+12]                 ; skip over cpAddr, cpIndex, RA in snippet
      ret

doneDataResolutionAndNoRerun:
      pop         eax                                   ; remove the vmThread address from the top of the stack
      FPRDataResolveEpilogue ecx, edi, edx

      pop         eax                           ; restore
      pop         ebx                           ; restore
      pop         ecx                           ; restore
      pop         edx                           ; restore
      pop         edi                           ; restore
      pop         esi                           ; restore
      pop         ebp                           ; restore
      popfd                                     ; restore
      lea         esp, [esp+12]                 ; skip over cpAddr, cpIndex, RA in snippet
      ret

patchUnresolvedStatic:

      ; For unresolved statics the RET instruction may need to be overwritten
      ; if the static instruction length < 8.
      ;
      movzx       ecx, byte  [ebp]              ; header byte
      cmp         ecx, 080h                     ; length < 8?
      jge         loadTargetInstruction

      ; The RET instruction must be inserted at least 4 bytes past the start
      ; of the unresolved static instruction. This is guaranteed because the
      ; unresolved static will have a disp32 field. Since this is true, then
      ; only the instruction data in ECX needs to be patched.
      ;
      and         ecx, 030h                     ; isolate dword offset
      shr         ecx, 1                        ; number of bits to rotate by

      mov         ebx, dword  [ebp+5]           ; high dword of patch instruction in snippet
      ror         ebx, cl
      mov         bl, byte  [ebp+9]             ; replace RET instruction by cached original value
      rol         ebx, cl

      mov         ecx, dword  [ebp+1]           ; low dword of patch instruction in snippet
      xchg        ebx, ecx
      jmp         mergePatchUnresolvedDataReference

executeSnippetCode:
      ; Execute the patched instruction in the snippet. The instruction is followed
      ; by a 1-byte RET to route control to after the data reference instruction in the
      ; mainline code.
      ;
      pop         eax                           ; remove the vmThread address from the top of the stack
      FPRDataResolveEpilogue ecx, edi, edx

      movzx       ecx, byte  [ebp]              ; header byte in snippet
      and         ecx, 0f0h                     ; mask off static instruction length
      shr         ecx, 4
      sub         ecx, 5                        ; account for length of CALL instruction
      add         dword  [esp+44], ecx          ; adjust RA in mainline code to point to the
                                                ; instruction following the patched instruction

      pop         eax                           ; restore
      pop         ebx                           ; restore
      pop         ecx                           ; restore
      pop         edx                           ; restore
      pop         edi                           ; restore
      pop         esi                           ; restore
      pop         ebp                           ; restore
      add         dword  [esp+4], 1             ; adjust RA in snippet to patched instruction in snippet
      popfd                                     ; restore
      ret 8                                     ; execute patched instruction in snippet

retn



MTUnresolvedInt32Load:
retn

MTUnresolvedInt64Load:
retn

MTUnresolvedFloatLoad:
retn

MTUnresolvedDoubleLoad:
retn

MTUnresolvedAddressLoad:
retn

MTUnresolvedInt32Store:
retn

MTUnresolvedInt64Store:
retn

MTUnresolvedFloatStore:
retn

MTUnresolvedDoubleStore:
retn

MTUnresolvedAddressStore:
retn

%else

; --------------------------------------------------------------------------------
;
; 64-BIT
;
; --------------------------------------------------------------------------------


      %include "jilconsts.inc"
      %include "X86PicBuilder.inc"

%ifdef ASM_OMR_GC_COMPRESSED_POINTERS
eq_offsetof_J9Object_clazz equ   8        ; offset of class pointer in a J9Object
%else
eq_offsetof_J9Object_clazz equ   16       ; offset of class pointer in a J9Object
%endif



segment .text

      DECLARE_GLOBAL interpreterUnresolvedStaticGlue
      DECLARE_GLOBAL interpreterUnresolvedSpecialGlue
      DECLARE_GLOBAL interpreterStaticAndSpecialGlue

      DECLARE_GLOBAL interpreterUnresolvedStringGlue
      DECLARE_GLOBAL interpreterUnresolvedMethodTypeGlue
      DECLARE_GLOBAL interpreterUnresolvedMethodHandleGlue
      DECLARE_GLOBAL interpreterUnresolvedCallSiteTableEntryGlue
      DECLARE_GLOBAL interpreterUnresolvedMethodTypeTableEntryGlue
      DECLARE_GLOBAL interpreterUnresolvedClassGlue
      DECLARE_GLOBAL interpreterUnresolvedClassFromStaticFieldGlue
      DECLARE_GLOBAL interpreterUnresolvedStaticFieldGlue
      DECLARE_GLOBAL interpreterUnresolvedStaticFieldSetterGlue
      DECLARE_GLOBAL interpreterUnresolvedFieldGlue
      DECLARE_GLOBAL interpreterUnresolvedFieldSetterGlue
      DECLARE_GLOBAL interpreterUnresolvedConstantDynamicGlue

      DECLARE_GLOBAL MTUnresolvedInt32Load
      DECLARE_GLOBAL MTUnresolvedInt64Load
      DECLARE_GLOBAL MTUnresolvedFloatLoad
      DECLARE_GLOBAL MTUnresolvedDoubleLoad
      DECLARE_GLOBAL MTUnresolvedAddressLoad

      DECLARE_GLOBAL MTUnresolvedInt32Store
      DECLARE_GLOBAL MTUnresolvedInt64Store
      DECLARE_GLOBAL MTUnresolvedFloatStore
      DECLARE_GLOBAL MTUnresolvedDoubleStore
      DECLARE_GLOBAL MTUnresolvedAddressStore

      DECLARE_EXTERN j2iTransition
      DECLARE_EXTERN jitResolveStaticMethod
      DECLARE_EXTERN jitResolveSpecialMethod
      DECLARE_EXTERN jitCallCFunction
      DECLARE_EXTERN jitResolveString
      DECLARE_EXTERN jitResolveConstantDynamic
      DECLARE_EXTERN jitResolveMethodType
      DECLARE_EXTERN jitResolveMethodHandle
      DECLARE_EXTERN jitResolveInvokeDynamic
      DECLARE_EXTERN jitResolveHandleMethod
      DECLARE_EXTERN jitResolveClass
      DECLARE_EXTERN jitResolveClassFromStaticField
      DECLARE_EXTERN jitResolveStaticField
      DECLARE_EXTERN jitResolveStaticFieldSetter
      DECLARE_EXTERN jitResolveField
      DECLARE_EXTERN jitResolveFieldSetter
      ;1179
      DECLARE_EXTERN jitResolvedFieldIsVolatile

      DECLARE_EXTERN mcc_callPointPatching_unwrapper


; interpreterUnresolved{Static|Special}Glue
;
; Resolve a static or special method. The call instruction routing control here is
; updated to load the RAM method into RDI.
;
; The unresolved {Static|Special} interpreted dispatch snippet look like:
; align 8
; (10) call interpreterUnresolved{Static|Special}Glue ; replaced with "mov rdi, 0x0000aabbccddeeff"
; (5)  jmp  interpreterStaticAndSpecialGlue
; (2)  dw   2-byte glue method helper index
; (8)  dq   cpAddr
; (4)  dd   cpIndex
;
; NOTES:
;
; [1] This runtime code uses the JIT linkage registers as volatile registers and hence
; does not preserve them. This is because the eventual dispatch path will be through
; the interpreter which reads the parameters from the stack. The backspilling has
; already occured at this point.
;
; [2] A POP is not strictly necessary to shape the stack. The RA could be left on the
; stack and the new stack shape updated in getJitStaticMethodResolvePushes(). It
; was left this way because we have to dork with the RA anyways to get back from
; this function which will already cause a return mispredict. Leaving it as is
; changes less code.
;
; [3] STACK SHAPE: must maintain stack shape expected by call to getJitStaticMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
interpreterUnresolvedStaticGlue:
      pop         rdi                                       ; RA in snippet (see [2] above)

      ; Attempt to resolve static method.
      ;
      mov         rax, qword [rsp]                          ; p1) rax = RA in mainline code
      mov         rsi, qword [rdi+12]                       ; p2) rsi = cpAddr
                                                            ;        12 = 5 + 5 (call update) + 2 (DW)
      mov         edx, dword [rdi+20]                       ; p3) rdx = cpIndex
                                                            ;        20 = 5 + 5 (call update) + 2 (DW) + 8 (cpAddr)
      call        jitResolveStaticMethod
      mov         rsi, rdi                                  ; Save the return address

      ; The interpreter may low-tag the result to avoid populating the constant pool -- whack it and record in CF. 
      ;
      btr         rax, 0
      ; Load the resolved RAM method into RDI so that the caller doesn't have to re-run patched code
      mov         rdi, rax
      ; Skip code patching if the interpreter low-tag the RAM method 
      jc          .skippatching

      ; Patch the call that brought us here into a load of the resolved RAM method into RDI.
      ;
      shl         rax, 16
      xor         rax, 0bf48h                               ; REX+MOV bytes
      mov         qword [rsi-5], rax                        ; Replaced current call with "mov rdi, 0x0000aabbccddeeff"
      jmp         interpreterStaticAndSpecialGlue           ; The next instruction is always "jmp interpreterStaticAndSpecialGlue",
                                                            ; jump to its target directly.
   .skippatching:
      mov         rcx, qword [rdi+J9TR_MethodPCStartOffset] ; rcx = interpreter entry point of the compiled method
      test        cl, J9TR_MethodNotCompiledBit
      jnz         j2iTransition
      jmp         rcx


      align 16
interpreterUnresolvedSpecialGlue:
      pop         rdi                                       ; RA in snippet

      ; Attempt to resolve special method.
      ;
      mov         rax, qword [rsp]                          ; p1) rax = RA in mainline code
      mov         rsi, qword [rdi+12]                       ; p2) rsi = cpAddr
                                                            ;        12 = 5 + 5 (call update) + 2 (DW)
      mov         edx, dword [rdi+20]                       ; p3) rdx = cpIndex
                                                            ;        20 = 5 + 5 (call update) + 2 (DW) + 8 (cpAddr)
      call        jitResolveSpecialMethod
      mov         rsi, rdi                                  ; Save the return address

      ; Load the resolved RAM method into RDI so that the caller doesn't have to re-run patched code
      mov         rdi, rax

      ; Patch the call that brought us here into a load of the resolved RAM method into RDI.
      ;
      shl         rax, 16
      xor         rax, 0bf48h                               ; REX+MOV bytes
      mov         qword [rsi-5], rax                        ; Replaced current call with "mov rdi, 0x0000aabbccddeeff"
      jmp         interpreterStaticAndSpecialGlue           ; The next instruction is always "jmp interpreterStaticAndSpecialGlue",
                                                            ; jump to its target directly.


; interpreterStaticAndSpecialGlue
;
; This function is a back-patching routine for interpreter calls to static
; methods that have recently been compiled. It patches the call site (in the
; code cache) of an interpreter call snippet, so that future executions will
; invoke the compiled method instead.
;
; NOTES:
;
; [1] The RAM method is in RDI on entry. It was loaded by the preceding instruction.
;
; [2] This runtime code uses the JIT linkage registers as volatile registers and hence
; does not preserve them. This is because the eventual dispatch path will be through
; the interpreter which reads the parameters from the stack. The backspilling has
; already occured at this point.
;
      align 16
interpreterStaticAndSpecialGlue:
      mov         rcx, qword [rdi + J9TR_MethodPCStartOffset]     ; rcx = interpreter entry point of the compiled method
      test        cl, J9TR_MethodNotCompiledBit
      jnz         j2iTransition

      mov         rsi, qword [rsp]                                ; rsi = RA in mainline code
      sub         rsi, 5
      push        0                                               ; descriptor+24: 0
      push        rcx                                             ; descriptor+16: compiled method PC
      push        rsi                                             ; descriptor+8: call site
      push        rdi                                             ; descriptor+0: RAM method
      MoveHelper  rax, mcc_callPointPatching_unwrapper            ; p1) rax = helper
      lea         rsi, [rsp]                                      ; p2) rsi = EA of descriptor
      lea         rdx, [rsp]                                      ; p3) rdx = EA of return value
      call        jitCallCFunction
      add         rsp, 32                                         ; tear down descriptor
      jmp         rcx                                             ; Dispatch compiled method through the interpreted entry
                                                                  ; point. This is because the linkage registers may not be
                                                                  ; set up any longer.


; --------------------------------------------------------------------------------
;
; 64-BIT DATA RESOLUTION
;
; --------------------------------------------------------------------------------

%macro DataResolvePrologue 0
      pushfq                              ; rsp+256 = flags
      push        r15                     ; rsp+248
      push        r14                     ; rsp+240
      push        r13                     ; rsp+232
      push        r12                     ; rsp+224
      push        r11                     ; rsp+216
      push        r10                     ; rsp+208
      push        r9                      ; rsp+200
      push        r8                      ; rsp+192
      push        rsp                     ; rsp+184 -- Ugh
      push        rbp                     ; rsp+176
      push        rsi                     ; rsp+168
      push        rdi                     ; rsp+160
      push        rdx                     ; rsp+152
      push        rcx                     ; rsp+144
      push        rbx                     ; rsp+136
      push        rax                     ; rsp+128
      sub         rsp, 128
      movsd       qword  [rsp+0], xmm0
      movsd       qword  [rsp+8], xmm1
      movsd       qword  [rsp+16], xmm2
      movsd       qword  [rsp+24], xmm3
      movsd       qword  [rsp+32], xmm4
      movsd       qword  [rsp+40], xmm5
      movsd       qword  [rsp+48], xmm6
      movsd       qword  [rsp+56], xmm7
      movsd       qword  [rsp+64], xmm8
      movsd       qword  [rsp+72], xmm9
      movsd       qword  [rsp+80], xmm10
      movsd       qword  [rsp+88], xmm11
      movsd       qword  [rsp+96], xmm12
      movsd       qword  [rsp+104], xmm13
      movsd       qword  [rsp+112], xmm14
      movsd       qword  [rsp+120], xmm15
%endmacro


%macro DispatchUnresolvedDataHelper 1 ; helper
      mov         rdi, qword [rsp+264]                ; RA in snippet (see stack shape below)
      mov         rax, qword [rdi]                    ; p1) rax = cpAddr
      mov         esi, dword [rdi+8]                  ; p2) rsi = cpIndex
      and         esi, 1ffffh                         ; isolate the cpIndex
      mov         rdx, qword [rsp+272]                ; p3) rdx = RA in mainline code
                                                      ; (see stack shape below)
      CallHelperUseReg %1, rax
%endmacro


%macro DataResolveEpilogue 0
      movsd       xmm0, qword  [rsp+0]
      movsd       xmm1, qword  [rsp+8]
      movsd       xmm2, qword  [rsp+16]
      movsd       xmm3, qword  [rsp+24]
      movsd       xmm4, qword  [rsp+32]
      movsd       xmm5, qword  [rsp+40]
      movsd       xmm6, qword  [rsp+48]
      movsd       xmm7, qword  [rsp+56]
      movsd       xmm8, qword  [rsp+64]
      movsd       xmm9, qword  [rsp+72]
      movsd       xmm10, qword  [rsp+80]
      movsd       xmm11, qword  [rsp+88]
      movsd       xmm12, qword  [rsp+96]
      movsd       xmm13, qword  [rsp+104]
      movsd       xmm14, qword  [rsp+112]
      movsd       xmm15, qword  [rsp+120]
      add         rsp, 128
      pop         rax
      pop         rbx
      pop         rcx
      pop         rdx
      pop         rdi
      pop         rsi

      ; Do not pop the old RSP value as the stack may have moved during the
      ; resolution. RBP is already restored by the VM.
      ;
      add         rsp, 16

      pop         r8
      pop         r9
      pop         r10
      pop         r11
      pop         r12
      pop         r13
      pop         r14
      pop         r15
%endmacro


         align 16
;1179
checkReferenceVolatility:

      ; preserve register states
      ;
      push        rbp
      push        rax
      push        rcx
      push        rdx
      push        rsi
      push        rbx
      push        r9

      ; jitResolvedFieldIsVolatile requires us to restore ebp before calling it
      ;
      mov         rbp, qword  [rsp + 240]             ; [rsp + 176 + 8 + 56]
                                                      ; [rsp + rbp stack slot + proc RA + local reg preservation]

      ; determine if the field is volatile.
      ;
      mov         rbx, qword  [rsp + 328]             ; load the snippet RA [rsp + 264 + 8 + 56]
      mov         edx, dword  [rbx + 8]               ; load the cpIndex [snippet RA + size of cpAddr]
      mov         esi, edx
      and         edx, eq_ResolveStatic               ; get the static flag bit
      and         esi, eq_CPIndexMask                 ; mask with the CPIndexMask
      mov         rax, qword  [rbx]                   ; load the cpAddr [snippet RA]

      ; call the volatile check
      ;
      CallHelperUseReg jitResolvedFieldIsVolatile,rax

      ; if this field is not volatile, patch the barrier with a nop
      ;
      test        eax, eax
      jnz         restoreRegs


      ; determine what kind of fence we are dealing with: LOCK OR [ESP] (or mfence using appropriate command line option)
      ;
      mov         rsi, [J9TR_VMThread_javaVM + rbp]
      mov         rsi, [J9TR_JavaVMJitConfig + rsi]
      mov         rsi, [J9TR_JitConfig_runtimeFlags + rsi]

      ; get the RA in the mainline code
      ;
      mov         r9, qword  [rsp + 336]

      ; determine if we are looking at a store to a static field
      ;
      cmp         edx, eq_ResolveStatic
      je          patchStatic

      ; get the instruction descriptor and find the length of the patched instruction
      ;

      mov         bl, byte  [rbx + 12]                ; get the first byte of the instruction descriptor (+ 8 (length of cpAddr) + 4 (length of cpIndex))
      and         rbx, 0f0h                           ; find the length of the instruction stored in the upper nibble
      shr         rbx, 4
      lea         rbx,   [rbx - 5]
      lea         rax,   [r9 + rbx]

      ; select the patching path based on the type of barrier being used
      ;

      test        esi, J9TR_runtimeFlags_PatchingFenceType
      jnz         short patchOverMfence
      jmp         patchOverLockOrESP

patchStatic:


      ; select the patching path based on the type of barrier being used
      ;
      sub         r9, 5
      test        esi, J9TR_runtimeFlags_PatchingFenceType
      jz          short patchOverLockOrESPStatic


patchOverMfenceStatic:
     ; find the address of the LOR instruction. It is either 16 or 24 bytes from the start of the resolved instruction.
     ;

     test         edx, eq_ExtremeStaticMemBarPos
     jnz          barrierAt20Offset

     lea          rax, [r9 + 16]
     jmp          patchWith3ByteNOP

barrierAt20Offset:

     lea          rax, [r9 + 20]
     jmp          patchWith3ByteNOP

patchOverMfence:

      ; patch over 3 byte mfence
      ;
      lea         rax, [rax + 3]
      mov         rcx, 0fffffffffffffffch
      and         rax, rcx                            ; should now have the 4 byte aligned instruction of the memfence

patchWith3ByteNOP:

      mov         ecx, dword  [rax]                   ; load existing double word containing mfence
      and         ecx, 0ff000000h                     ; insert trailing byte of 3 word nop
      or          ecx, 000001f0fh                     ; load the the first 2 bytes of the 3 word nop
      mov         dword  [rax], ecx                   ; write the nop

      pop         r9
      pop         rbx
      pop         rsi
      pop         rdx
      pop         rcx
      pop         rax
      pop         rbp

      ret

patchOverLockOrESPStatic:

     ; find the address of the LOR instruction. It is either 16 or 24 bytes from the start of the resolved instruction.
     ;
     mov          edx, dword  [rbx + 8]               ; get unchanged CPindex

     test         edx, eq_ExtremeStaticMemBarPos
     jnz          barrierAt24Offset

     lea          rax, [r9 + 16]
     jmp          patchWith5ByteNOP

barrierAt24Offset:

     lea rax, [r9 + 24]
     jmp patchWith5ByteNOP


patchOverLockOrESP:

      mov         rbx, qword  [rsp + 328]             ; load the cpIndex [rsp + 272 + 8 + 56 + 8]
      mov         edx, dword  [rbx + 8]               ; get unchanged CPindex

      ; patch over 5 byte lock or esp
      ;
      lea         rax, [rax + 7]

      ; if this is a float, we need to add an extra 8 bytes to account for the LEA.
      ;
      test        edx, eq_IsFloatOp
      jz          findOffset

      lea         rax, [rax + 8]

findOffset:

      mov         rcx, 0fffffffffffffff8h             ; find the 8 byte aligned LOR instruction
      and         rax, rcx

patchWith5ByteNOP:
      ;cmp word ptr[rax], eq_LORBinaryWord ; make sure we are patching over a lock or [esp] ;TEMP
      ;je restoreRegs

      mov         ecx,    dword  [rax + 4]            ; load the existing dword with the last byte of the lock and the following 3 bytes
      and         ecx, 0ffffff00h                     ; insert the last byte if the 5 byte nop
      rol         rcx, 32
      or          rcx, 00441f0fh                      ; insert the first 4 bytes of the 5 byte nop
      mov         qword  [rax], rcx                   ; write the nop

restoreRegs:

      pop         r9
      pop         rbx
      pop         rsi
      pop         rdx
      pop         rcx
      pop         rax
      pop         rbp

ret

;1179

; interpreterUnresolved{*}Glue
;
; Generic code to perform runtime resolution of a data reference.  These functions
; are called from a snippet that has the following general shape:
;
;     call  interpreterUnresolved{*}Glue
;     dq    cpAddr
;     dd    cpIndex
;     [db]  header byte -- ONLY PRESENT FOR disp32 PATCHING
;            high nibble : length of instruction in snippet (must be 8-15)
;            low nibble : offset to disp32 in patched instruction
;     dq    8 bytes of instruction to patch (includes bytes from following instruction if necessary)
;     db    remaining bytes of instruction for static resolves
;     ret   return for unpatched static resolves
;
; Spare bits in the cpIndex passed in are used to specify behaviour based on the
; kind of resolution.  The anatomy of a cpIndex:
;
; Spare bits in the cpIndex passed in are used to specify behaviour based on the
; kind of resolution.  The anatomy of a cpIndex:
;
;       byte 3   byte 2   byte 1   byte 0
;
;      3 222  2 222    1 1      0 0      0
;      10987654 32109876 54321098 76543210
;      |||||__| ||| ||||_________________|
;      |||| |   ||| |||         |
;      |||| |   ||| |||         +---------------- cpIndex (0-16)
;      |||| |   ||| ||+-------------------------- upper long dword resolution (17)
;      |||| |   ||| |+--------------------------- lower long dword resolution (18)
;      |||| |   ||| +---------------------------- check volatility (19)
;      |||| |   ||+------------------------------ resolve, but do not patch snippet template (21)
;      |||| |   |+------------------------------- resolve, but do not patch mainline code (22)
;      |||| |   +-------------------------------- long push instruction (23)
;      |||| +------------------------------------ number of live X87 registers across this resolution (24-27)
;      |||+-------------------------------------- has live XMM registers (28)
;      ||+--------------------------------------- static resolution (29)
;      |+---------------------------------------- 64-bit: resolved value is 8 bytes (30)
;      |                                          32-bit: resolved value is high word of long pair (30)
;      +----------------------------------------- 64 bit: extreme static memory barrier position (31)
;
;
; NOTES:
;
; [1] STACK SHAPE: must maintain stack shape expected by call to getJitDataResolvePushes()
;     across the resolution helper.

      align 16
interpreterUnresolvedStringGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveString
; int 3
      jmp         commonUnresolvedCode
ret

      align 16
interpreterUnresolvedConstantDynamicGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveConstantDynamic
;      int 3
      jmp         commonUnresolvedCode
ret

      align 16
interpreterUnresolvedMethodTypeGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodType
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedMethodHandleGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodHandle
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedCallSiteTableEntryGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveInvokeDynamic
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedMethodTypeTableEntryGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveHandleMethod
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedClassGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClass
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedClassFromStaticFieldGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClassFromStaticField
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedStaticFieldGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticField
; int 3
      jmp         commonUnresolvedCode
ret


      align 16
interpreterUnresolvedStaticFieldSetterGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticFieldSetter
; int 3
      jmp         commonUnresolvedCode
ret

      align 16
interpreterUnresolvedFieldGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveField
; int 3
      jmp         commonUnresolvedCode
ret

      align 16
interpreterUnresolvedFieldSetterGlue:
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveFieldSetter
; int 3

commonUnresolvedCode:

      ; STACK SHAPE:
      ;
      ; rsp+272 : RA in mainline
      ; rsp+264 : RA in snippet
      ; rsp+256 : RFlags
      ; rsp+248 : r15
      ; rsp+240 : r14
      ; rsp+232 : r13
      ; rsp+224 : r12
      ; rsp+216 : r11
      ; rsp+208 : r10
      ; rsp+200 : r9
      ; rsp+192 : r8
      ; rsp+184 : rsp
      ; rsp+176 : rbp
      ; rsp+168 : rsi
      ; rsp+160 : rdi
      ; rsp+152 : rdx
      ; rsp+144 : rcx
      ; rsp+136 : rbx
      ; rsp+128 : rax
      ; rsp+120 : xmm15
      ; rsp+112 : xmm14
      ; rsp+104 : xmm13
      ; rsp+96 : xmm12
      ; rsp+88 : xmm11
      ; rsp+80 : xmm10
      ; rsp+72 : xmm9
      ; rsp+64 : xmm8
      ; rsp+56 : xmm7
      ; rsp+48 : xmm6
      ; rsp+40 : xmm5
      ; rsp+32 : xmm4
      ; rsp+24 : xmm3
      ; rsp+16 : xmm2
      ; rsp+8 : xmm1
      ; rsp+0 : xmm0
      ;
      ; rax = result from resolve helper
      ; rdi = return address in snippet

      mov         esi, dword [rdi+8]                        ; full cpIndex word

      ; Check for the special case of a data resolution without code patching.
      ; This happens when you have an explicit NULLCHK guarding a data resolution and
      ; we need to ensure the exceptions are thrown in the correct order if they occur.
      ; In this case any resolution exceptions are required to be thrown before any NPEs.
      ;
      test        esi, eq_ResolveWithoutPatchingMainline
      jnz         doneDataResolution

      test        esi, eq_Patch8ByteResolution
      jz          patch4ByteResolution

      ; Resolved value is 8-bytes. The target instruction must be a 'MOV R,Imm64' instruction.
      ; Synthesize the first 8 bytes of this instruction with the REX+op from the snippet and
      ; the 48-bit virtual address returned from the resolution and patch the snippet.
      ;
      ; Multiple threads can be executing this code, but all threads must be patching the
      ; same value.
      ;
      test        esi, eq_ResolveStatic
      jnz         patchStaticResolution

mergePatch8ByteResolution:
      movzx       edx, word  [rdi+12]                       ; rdx = REX + OpCode
      mov         rbx, rax                                  ; rbx = resolve result
      and         rbx, -2                                   ; whack any low tagging of resolve result
      shl         rbx, 16
      or          rdx, rbx                                  ; synthesize patched instruction bytes

      mov         rbx, qword  [rsp+272]                     ; rbx = RA in mainline
      mov         qword  [rbx-5], rdx                       ; patch mainline

; determine if nop patching might be necessary
;

     test         dword  [rdi + 8], eq_VolatilityCheck      ; test for the volatility check flag
     jz           short noVolatileCheck8Byte

     call         checkReferenceVolatility


noVolatileCheck8Byte:

      DataResolveEpilogue

      add         qword  [rsp+16], -5                       ; re-run patched instruction
      popfq                                                 ; restore
      lea         rsp, [rsp+8]                              ; skip over RA in snippet--mispredict
      ret

patchStaticResolution:
      test        rax, 1                                    ; low-tagged resolution result (clinit not finished)?
      jz          mergePatch8ByteResolution

      ; Static resolves have the entire unresolved address load instruction
      ; embedded in the snippet plus a RET instruction. The snippet must
      ; be patched and executed.
      ;
      movzx       edx, word  [rdi+12]                       ; rdx = REX + OpCode
      mov         rbx, rax                                  ; rbx = resolve result
      and         rbx, -2                                   ; whack any low tagging of resolve result
      shl         rbx, 16
      or          rdx, rbx                                  ; synthesize patched instruction bytes

      mov         qword  [rdi+12], rdx                      ; patch snippet
      mfence                                                ; make sure memory stores are seen

doneDataResolution:
      DataResolveEpilogue

      add         qword  [rsp+16], 5                           ; patch the RA in the mainline code to skip
                                                               ; over the remaining 5 bytes of what would
                                                               ; have been the 10-byte MOV R,Imm64 instruction
                                                               ; that did not get patched.

      add         qword  [rsp+8], 12                           ; patch RA in snippet such that the instruction
                                                               ; patched in the snippet is re-run.
                                                               ; +12 = +8 (cpAddr) + 4 (cpIndex)
      popfq
      ret

doneDataResolutionAndNoRerun:
      DataResolveEpilogue

      popfq
      lea         rsp, [rsp+8]
      ret

patch4ByteResolution:

      ; Patch the cached instruction bytes in the snippet. The disp32 field must lie within
      ; the first 8 bytes of the instruction.
      ;
      ; Multiple threads can be executing this code, but all threads must be patching the
      ; same value.
      ;
      movzx       ecx, byte  [rdi+12]                       ; rcx = header byte
      and         ecx, 0fh                                  ; isolate disp32 offset in first nibble

      mov         rdx, qword  [rdi+13]                      ; rdx = cached instruction bytes
                                                            ; +13 = +8 (cpAddr) + 4 (cpIndex) + 1 (header)
      shl         ecx, 3
      ror         rdx, cl                                   ; rotate disp32 into lower dword

      ; The dword field from the snippet is assumed to be 0.
      ;
      or          rdx, rax                                  ; patch the disp32 field
      rol         rdx, cl

      mov         rbx, qword  [rsp+272]                     ; rbx = RA in mainline
      mov         qword  [rbx-5], rdx                       ; patch mainline

; determine if nop patching might be necessary
;
      test        dword  [rdi + 8], eq_VolatilityCheck      ; test for the check volatility flag
      jz          short noVolatileCheck4Byte

      call        checkReferenceVolatility

noVolatileCheck4Byte:

      DataResolveEpilogue

      add qword  [rsp+16], -5                               ; re-run patched instruction
      popfq                                                 ; restore
      lea rsp, [rsp+8]                                      ; skip over RA in snippet--mispredict
      ret

ret


MTUnresolvedInt32Load:
ret

MTUnresolvedInt64Load:
ret

MTUnresolvedFloatLoad:
ret

MTUnresolvedDoubleLoad:
ret

MTUnresolvedAddressLoad:
ret

MTUnresolvedInt32Store:
ret

MTUnresolvedInt64Store:
ret

MTUnresolvedFloatStore:
ret

MTUnresolvedDoubleStore:
ret

MTUnresolvedAddressStore:
ret

%endif
