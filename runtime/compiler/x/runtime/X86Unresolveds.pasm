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

include x/runtime/X86PicBuilder.inc

#include "j9cfg.h"

ifndef TR_HOST_64BIT

; --------------------------------------------------------------------------------
;
;                                    32-BIT
;
; --------------------------------------------------------------------------------

      .686p
      .xmm

      option NoOldMacros
      eq_offsetof_J9Object_clazz equ   8     ; offset of class pointer in a J9Object

      include jilconsts.inc

      assume cs:flat,ds:flat,ss:flat
      _TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
      _TEXT ends

      _TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

      public         interpreterUnresolvedStaticGlue
      public         interpreterUnresolvedSpecialGlue
      public         updateInterpreterDispatchGlueSite
      public         interpreterVoidStaticGlue
      public         interpreterSyncVoidStaticGlue
      public         interpreterEAXStaticGlue
      public         interpreterSyncEAXStaticGlue
      public         interpreterEDXEAXStaticGlue
      public         interpreterSyncEDXEAXStaticGlue
      public         interpreterST0FStaticGlue
      public         interpreterSyncST0FStaticGlue
      public         interpreterST0DStaticGlue
      public         interpreterSyncST0DStaticGlue

      public         interpreterUnresolvedStringGlue
      public         interpreterUnresolvedMethodTypeGlue
      public         interpreterUnresolvedMethodHandleGlue
      public         interpreterUnresolvedCallSiteTableEntryGlue
      public         interpreterUnresolvedMethodTypeTableEntryGlue
      public         interpreterUnresolvedClassGlue
      public         interpreterUnresolvedClassFromStaticFieldGlue
      public         interpreterUnresolvedStaticFieldGlue
      public         interpreterUnresolvedStaticFieldSetterGlue
      public         interpreterUnresolvedFieldGlue
      public         interpreterUnresolvedFieldSetterGlue
      public         interpreterUnresolvedConstantDynamicGlue

      public         MTUnresolvedInt32Load
      public         MTUnresolvedInt64Load
      public         MTUnresolvedFloatLoad
      public         MTUnresolvedDoubleLoad
      public         MTUnresolvedAddressLoad

      public         MTUnresolvedInt32Store
      public         MTUnresolvedInt64Store
      public         MTUnresolvedFloatStore
      public         MTUnresolvedDoubleStore
      public         MTUnresolvedAddressStore

      ExternHelper   jitResolveStaticMethod
      ExternHelper   jitResolveSpecialMethod
      ExternHelper   jitCallCFunction
      ExternHelper   jitResolveString
      ExternHelper   jitResolveConstantDynamic
      ExternHelper   jitResolveMethodType
      ExternHelper   jitResolveMethodHandle
      ExternHelper   jitResolveInvokeDynamic
      ExternHelper   jitResolveHandleMethod
      ExternHelper   jitResolveClass
      ExternHelper   jitResolveClassFromStaticField
      ExternHelper   jitResolveStaticField
      ExternHelper   jitResolveStaticFieldSetter
      ExternHelper   jitResolveField
      ExternHelper   jitResolveFieldSetter
      ;1179
      ExternHelper   jitResolvedFieldIsVolatile

      ExternHelper   icallVMprJavaSendStatic0
      ExternHelper   icallVMprJavaSendStatic1
      ExternHelper   icallVMprJavaSendStaticJ
      ExternHelper   icallVMprJavaSendStaticF
      ExternHelper   icallVMprJavaSendStaticD
      ExternHelper   icallVMprJavaSendStaticSync0
      ExternHelper   icallVMprJavaSendStaticSync1
      ExternHelper   icallVMprJavaSendStaticSyncJ
      ExternHelper   icallVMprJavaSendStaticSyncF
      ExternHelper   icallVMprJavaSendStaticSyncD
      ExternHelper   interpretedDispatchGlueDisp32_unwrapper

ifdef WINDOWS
      ExternHelper   j9thread_self
      ExternHelper   j9thread_tls_get
      extern         vmThreadTLSKey:dword
endif

      ExternHelper   memoryFence


CheckIfMethodCompiledAndPatch macro helperName
      test        byte ptr[edi+J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit
      jnz         &helperName
      jmp         mergedStaticGlueCallFixer2
endm

; mergedStaticGlueCallFixer
;
; This function is a back-patching routine for interpreter calls to static
; methods that have recently been compiled. It patches the call site (in the
; code cache) of an interpreter call snippet, so that future executions will
; invoke the compiled method instead.
;
; NOTES:
;
; [1] The RAM method is in EDI on entry.  It was loaded by the preceding instruction.
;
; [2] This function must appear here so as to require a non-short jump on NTO
;
		align 16
mergedStaticGlueCallFixer2 proc near
      mov         edi, dword ptr [edi+J9TR_MethodPCStartOffset]
      mov         edx, dword ptr [esp]
      sub         edi, edx
      mov         dword ptr [edx-4], edi
      add         edi, edx
      jmp         edi
mergedStaticGlueCallFixer2 endp



; interpreterUnresolved{Static|Special}Glue
;
; Resolve a static or special method.  The call instruction routing control here is
; updated to load the RAM method into EDI.
;
; The unresolved {Static|Special} interpreted dispatch snippet look like:
;       align 8
; (5)   call  interpreterUnresolved{Static|Special}Glue  ; replaced with "mov edi, 0xaabbccdd"
; (3)   NOP
; ---
; (5)   call  updateInterpreterDispatchGlueSite          ; replaced with "JMP disp32"
; (2)   dw    2-byte glue method helper index
; (4)   dd    cpAddr
; (4)   dd    cpIndex
;
; NOTES:
;
; [1] A POP is not strictly necessary to shapen the stack.  The RA could be left on the
;     stack and the new stack shape updated in getJitStaticMethodResolvePushes().  It
;     was left this way because we have to dork with the RA anyways to get back from
;     this function which will already cause a return mispredict.  Leaving it as is
;     changes less code.
;
; [2] STACK SHAPE: must maintain stack shape expected by call to getJitStaticMethodResolvePushes()
;     across the call to the lookup helper.
;
      align 16
interpreterUnresolvedStaticGlue proc near
      pop         edi                                          ; RA in snippet (see [1] above)

      push        dword ptr [edi+14]                           ; p3) cpIndex
                                                               ;    14 = 3 (NOP) + 5 (CALL) + 2 (DW) + 4 (cpAddr)
      push        dword ptr [edi+10]                           ; p2) cpAddr
                                                               ;    10 = 3 (NOP) + 5 (CALL) + 2 (DW)
      push        dword ptr [esp+8]                            ; p1) RA in mainline code
      call        jitResolveStaticMethod
      lea         edi, [edi+3]                                 ; Adjust the return address to "call updateInterpreterDispatchGlueSite"
      push        edi                                          ; The RET will mispredict anyway so we can get away with pushing
                                                               ; the adjusted RA back on the stack.

      ; The interpreter may low-tag the result to avoid populating the constant pool -- whack it and record in CF.
      ;
      btr         eax, 0

      ; Skip code patching if the interpreter low-tag the RAM mathod
ifdef WINDOWS
      jc          @@fin
else
      jc          0f
endif

      ; Patch the call that brought us here into a load of the resolved RAM method into EDI.
      ;
      sub         esp, 16
      movdqu      [esp], xmm0
      push        001f0f00h                                    ; NOP
      push        000000bfh                                    ; 1st byte of MOV edi, 0xaabbccdd
      mov         [esp+1], eax
      movsd       xmm0, qword ptr [esp]
      movsd       qword ptr [edi-8], xmm0
      movdqu      xmm0, [esp+8]
      add         esp, 24                                      ; 24 = 16 + 4*2 (for two pushes)

ifdef WINDOWS
   @@fin:
else
   0:
endif
      ; Load the resolved RAM method into EDI so that the caller doesn't have to re-run patched code
      mov         edi, eax
      ret
interpreterUnresolvedStaticGlue endp


      align 16
interpreterUnresolvedSpecialGlue proc near
      pop         edi                                          ; RA in snippet (see [1] above)

      push        dword ptr [edi+14]                           ; p3) cpIndex
                                                               ;    14 = 3 (NOP) + 5 (CALL) + 2 (DW) + 4 (cpAddr)
      push        dword ptr [edi+10]                           ; p2) cpAddr
                                                               ;    10 = 3 (NOP) + 5 (CALL) + 2 (DW)
      push        dword ptr [esp+8]                            ; p1) RA in mainline code
      call        jitResolveSpecialMethod
      lea         edi, [edi+3]                                 ; Adjust the return address to "call updateInterpreterDispatchGlueSite"
      push        edi                                          ; The RET will mispredict anyway so we can get away with pushing
                                                               ; the adjusted RA back on the stack.

      ; Patch the call that brought us here into a load of the resolved RAM method into EDI.
      ;
      sub         esp, 16
      movdqu      [esp], xmm0
      push        001f0f00h                                    ; NOP
      push        000000bfh                                    ; 1st byte of MOV edi, 0xaabbccdd
      mov         [esp+1], eax
      movsd       xmm0, qword ptr [esp]
      movsd       qword ptr [edi-8], xmm0
      movdqu      xmm0, [esp+8]
      add         esp, 24                                      ; 24 = 16 + 4*2 (for two pushes)

      ; Load the resolved RAM method into EDI so that the caller doesn't have to re-run patched code
      mov         edi, eax
      ret
interpreterUnresolvedSpecialGlue endp


; updateInterpreterDispatchGlueSite
;
; Now that a resolved RAM method is available, determine the appropriate interpreter
; dispatch glue code to call and then patch the snippet to jump to it directly.
;
; NOTES:
;
; [1] The RAM method is in EDI on entry.  It was loaded by the preceding instruction.
;
      align 16
updateInterpreterDispatchGlueSite proc near

      mov         edx, dword ptr[esp]                          ; edx = snippet RA

      sub         esp, 4                                       ; descriptor+8 : return value
      push        edx                                          ; descriptor+4 : snippet RA
      push        edi                                          ; descriptor+0 : RAM method
      lea         edx, [esp+8]
      push        edx                                          ; p3) EA of return value in TLS area
      lea         edx, [esp+4]
      push        edx                                          ; p2) EA parms in TLS area
      MoveHelper  edx, interpretedDispatchGlueDisp32_unwrapper
      push        edx                                          ; p1) helper address
      call        jitCallCFunction
      add         esp, 8                                       ; tear down descriptor
      pop         eax                                          ; eax = return value (disp32 to dispatch glue)

      ; Patch the call that brought us here into a JMP to the dispatch glue code.
      ;
      push        ebx                                          ; preserve
      push        ecx                                          ; preserve
      push        esi                                          ; preserve

      ; Stack shape:
      ;
      ; esp+16 : main line code RA
      ; esp+12 : snippet RA
      ; esp+8  : ebx
      ; esp+4  : ecx
      ; esp+0  : esi
      ;
      mov         ebx, eax
      MoveHelper  eax, updateInterpreterDispatchGlueSite

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      mov         esi, dword ptr [esp+12]                      ; snippet RA

      mov         edx, dword ptr [esi-1]                       ; edx = 3 bytes after the call to helper + high byte of disp32
      mov         ecx, edx
      sub         eax, esi                                     ; Expected disp32 for call to helper
      rol         eax, 8
      mov         dl, al                                       ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                     ; add CALL opcode

      ; Construct the byte sequence in ecx:ebx to jump to the dispatch glue.
      ;
      rol         ebx, 8
      mov         cl, bl
      mov         bl, 0e9h

      lea         esi, [esi-5]

      ; Attempt to patch the code.
      ;
ifdef WINDOWS
      lock cmpxchg8b   qword ptr [esi]
else
      lock cmpxchg8b   [esi]
endif

      mov         dword ptr [esp+12], esi                      ; update return address to re-run

      pop         esi                                          ; restore
      pop         ecx                                          ; restore
      pop         ebx                                          ; restore
      ret

updateInterpreterDispatchGlueSite endp

         align 16
;1179
checkReferenceVolatility proc near

      ; preserve register states
      ;
      push        ebp
      push        ebx                                           ; Low dword of patch instruction in snippet
      push        ecx                                           ; High dword of patch instruction in snippet
      push        eax                                           ; The call instruction (edx:eax) that should have brought
      push        edx                                           ; us to this snippet + the following 3 bytes.
      push        esi                                           ; The RA in mainline code

      ; jitResolvedFieldIsVolatile requires us to restore ebp before calling it
      ;
      mov         ebp, dword ptr [esp + 28]                       ; [esp + 4 + 24]

      ; determine if the field is volatile.
      ;
      mov         ecx, dword ptr [esp + 152]                    ; load the cpIndex [esp + 124 + 24 + 4]
      mov         ebx, ecx
      mov         edx, ecx
      and         ebx, eq_ResolveStatic                         ; get the static flag bit
      and         ecx, eq_CPIndexMask

      ; push the parameters for the volatile check
      ;
      push        ebx                                           ; push isStatic
      push        ecx                                           ; push cpIndex
      push        dword ptr [esp + eq_MemFenceRAOffset32]       ; push cpAddr

      ; call the volatile check
      ;
      xor eax, eax
      CallHelperUseReg jitResolvedFieldIsVolatile, eax

      ;clear ebp which will be used as to tell if we need to patch over a barrier in the mainline code.
      ;
      xor ebp, ebp

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
      mov         esi, dword ptr [esp + 144]                     ; find the instruction in the snippet cache.
      mov         dx, word ptr [esi + 1]
      cmp         dx, eq_MemFenceLCXHG                           ; check if the opcode is for a lock cmpxchng
      je          patchCmpxchgForLongStore                       ; lock cmpxchng should appear ONLY for a long store


      ; we need to patch an mfence in the mainline code, set the flag
      ;
      or         ebp, 001h

      jmp        patchMainlineInstructionFromCache

patchBarrierWithNop:

      mov         esi, dword ptr [esp + eq_MemFenceRAOffset32]  ; esp + 128 = RA of mainline code (mfence instruction or nop)

      ; patch the mfence following a non-long store.
      ; get the instruction descriptor and find the length of the store instruction
      ;
      mov         edx, dword ptr [esp + 144]                    ; get instruction descriptor address  [esp + 116 + 24 + 4]
      mov         bl, byte ptr [edx]                            ; get instruction length from instruciton descriptor
      and         ebx, 0f0h                                     ; mask size of instruction
      shr         ebx, 4                                        ; shift size to last nibble of byte
      lea         ebx, dword ptr [ebx - eq_MemFenceCallLength32] ; get the delta between the RA in the mainline code and
                                                                ; the end of the store instruction
      lea         esi, dword ptr [esi + ebx]                    ; find the end of the store instruction

      ; determine what kind of fence we are dealing with: mfence or LOCK OR [ESP] (on legacy systems)
      ;
      mov         edx, dword ptr [esp + 28]
      mov         edx, J9TR_VMThread_javaVM[edx]
      mov         edx, J9TR_JavaVMJitConfig[edx]
      mov         edx, J9TR_JitConfig_runtimeFlags[edx]
      test        edx, J9TR_runtimeFlags_PatchingFenceType
      jz short    doLOCKORESP

      ; 3 byte memory fence
      ;
      lea         esi, dword ptr [esi + 3]                      ; find the 4 byte aligned address
      and         esi, 0fffffffch                               ; should now have a 4 byte aligned instruction of the memfence

      ;make sure we are patching over an mfence (avoids potential race condition with lock cmpxchg patching)
      ;
      cmp         word ptr [esi], 0ae0fh
      jne         restoreRegs;

      mov         ecx, 001f0f00h                                ; load the 3 byte nop instruction plus padding
      mov         cl, byte ptr [esi + 3]                        ; get the byte following the memory fence
      ror         ecx, 8                                        ; realign and copied byte
      mov         dword ptr [esi], ecx                          ; write the nop
      jmp         restoreRegs

      ; 5 byte lock or esp
      ;
doLOCKORESP:

      ; ecx:ebx nop instruction
      ; edx:eax original instruction
      ;
      mov         esi, dword ptr [esp + eq_MemFenceRAOffset32]  ; esp + 128 = RA of mainline code (mfence instruction or nop)
      lea         esi, dword ptr [esi + ebx]                    ; find the end of the store instruction
      lea         esi, dword ptr [esi + 7]                      ; find the 8 byte aligned address of the lock or [esp]
      and         esi, 0fffffff8h
      mov         eax, dword ptr [esi]                          ; construct the edx:eax pair (original instruction)
      mov         edx, dword ptr [esi + 4]
      mov         ebx, 00441f0fh                                ; construct the ecx:ebx pair (nop and trailing bytes)
      mov         ecx, edx
      mov         cl, 00h

ifdef WINDOWS                                                     ; write the nop
      lock cmpxchg8b   qword ptr [esi]
else
      lock cmpxchg8b   [esi]
endif
      jmp restoreRegs



      ; Loading a long in 32 bit is done with a lock cmpxchng by default.
      ; We need to swtitch the op bits of the instructions that performs loads for the lock cmpxchng setup
      ; to stores. We also need to patch over the lock cmpxchg with nops.
      ;
patchLowerLong:

      mov         esi, dword ptr [esp + 144]
      mov         ebx, dword ptr [esi + 1]
      and         ebx, 0fffffffdh                                   ; change the mov reg mem opcode (8b) into a mov mem reg opcode (89)
      or          ebx, 000001800h                                   ; change EAX to EBX
      mov         ecx, dword ptr [esp + 12]
      jmp         patchMainlineInstruction


patchUpperLong:

      mov         esi, dword ptr [esp + 144]
      mov         ebx, dword ptr [esi + 1]
      and         ebx, 0ffffeffdh                                    ; change the mov reg mem opcode (8b) into a mov mem reg opcode (89)
      or          ebx, 000000800h                                    ; change EDX to ECX
      mov         ecx, dword ptr [esp + 12]
      jmp         patchMainlineInstruction


patchCmpxchgForLongStore:

      mov         ebx, 090666666h	                              ; setup the ecx:ebx pair (nop instruction)
      mov         ecx, 090666666h
      jmp         patchMainlineInstruction


patchMainlineInstructionFromCache:

      mov         ecx, dword ptr [esp + 12]                     ; Load low dword of patch instruction in snippet BEFORE volatile resolution
      mov         ebx, dword ptr [esp + 16]                     ; Load high dword of patch instruction in snippet BEFORE volatile resoluion


patchMainlineInstruction:


      mov         esi, dword ptr [esp]                          ; Restor RA in mainline code.
      mov         edx, dword ptr [esp + 4]                      ; Restore the call instruction (edx:eax) that should have brought
      mov         eax, dword ptr [esp + 8]                      ; us to this snippet + the following 3 bytes.

ifdef WINDOWS
      lock cmpxchg8b   qword ptr [esi - 5]
else
      lock cmpxchg8b   [esi - 5]
endif

      test ebp, ebp
      jnz patchBarrierWithNop

restoreRegs:

      pop         esi
      pop         edx
      pop         eax
      pop         ecx
      pop         ebx
      pop         ebp
      ret

checkReferenceVolatility endp
;1179

         align 16
interpreterVoidStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStatic0
interpreterVoidStaticGlue endp

         align 16
interpreterSyncVoidStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSync0
interpreterSyncVoidStaticGlue endp

         align 16
interpreterEAXStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStatic1
interpreterEAXStaticGlue endp

         align 16
interpreterSyncEAXStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSync1
interpreterSyncEAXStaticGlue endp

         align 16
interpreterEDXEAXStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticJ
interpreterEDXEAXStaticGlue endp

         align 16
interpreterSyncEDXEAXStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSyncJ
interpreterSyncEDXEAXStaticGlue endp

         align 16
interpreterST0FStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticF
interpreterST0FStaticGlue endp

         align 16
interpreterSyncST0FStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSyncF
interpreterSyncST0FStaticGlue endp

         align 16
interpreterST0DStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticD
interpreterST0DStaticGlue endp

         align 16
interpreterSyncST0DStaticGlue proc near
         CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSyncD
interpreterSyncST0DStaticGlue endp


; --------------------------------------------------------------------------------
;
;                            32-BIT DATA RESOLUTION
;
; --------------------------------------------------------------------------------


DataResolvePrologue MACRO
      local       doneFPRpreservation, preserveX87loop

      pushfd                                                   ; save flags   , addr=esp+28
      push        ebp                                          ; save register, addr=esp+24
      push        esi                                          ; save register, addr=esp+20
      push        edi                                          ; save register, addr=esp+16
      push        edx                                          ; save register, addr=esp+12
      push        ecx                                          ; save register, addr=esp+8
      push        ebx                                          ; save register, addr=esp+4
      push        eax                                          ; save register, addr=esp+0

ifdef WINDOWS
      ; Restore the VMThread into ebp
      ;
      call        j9thread_self
      push        [vmThreadTLSKey]
      push        eax
      call        j9thread_tls_get
      add         esp, 8
      mov         ebp, dword ptr [eax+8]                       ; get J9JavaVM from OMR_VMThread in tls
endif

      mov         esi, dword ptr [esp+32]                      ; esi = return address in snippet
      mov         eax, dword ptr [esp+36]                      ; eax = cpAddr
      mov         ecx, dword ptr [esp+40]                      ; ecx = cpIndex
      mov         edx, ecx                                     ; edx = cpIndex

      sub         esp, J9PreservedFPRStackSize                 ; reserve stack space for all possible FPRs
      shr         edx, 24
      and         edx, 01fh                                    ; isolate number of live FPRs across this resolution
      jz short    doneFPRpreservation                          ; none, so were done

      test        edx, 010h                                    ; test SSE bit
      jz short    preserveX87loop
      movq        qword ptr [esp], xmm0
      movq        qword ptr [esp+8], xmm1
      movq        qword ptr [esp+16], xmm2
      movq        qword ptr [esp+24], xmm3
      movq        qword ptr [esp+32], xmm4
      movq        qword ptr [esp+40], xmm5
      movq        qword ptr [esp+48], xmm6
      movq        qword ptr [esp+56], xmm7
      jmp short   doneFPRpreservation

preserveX87loop:
      dec         edx
      lea         edi, [edx+edx*4]
      fstp        tbyte ptr [esp+edi*2]                        ; store and pop from the FP stack
      jnz short  preserveX87loop
doneFPRpreservation:

ENDM


DispatchUnresolvedDataHelper MACRO helper
      and         ecx, 1ffffh                                  ; isolate the cpIndex
      push        dword ptr [esp+124]                          ; p) RA in mainline code
      push        ecx                                          ; p) cpIndex
      push        eax                                          ; p) cpAddr
      CallHelperUseReg &helper,eax
ENDM


FPRDataResolveEpilogue MACRO cpScratchReg, scratchReg, scratchReg2
      local       restoreX87stack, restoreX87loop, doneFPRrestoration

      movzx       &cpScratchReg, byte ptr[esp+123]             ; load number of FPRs live across this resolution
      and         &cpScratchReg, 01fh                          ; isolate number of live FPRs across this resolution
      test        &cpScratchReg, &cpScratchReg
      jz short    doneFPRrestoration                           ; none, so leave

      test        &cpScratchReg, 010h                          ; test SSE bit
      jz short    restoreX87stack
      movq        xmm0, qword ptr [esp]
      movq        xmm1, qword ptr [esp +8]
      movq        xmm2, qword ptr [esp +16]
      movq        xmm3, qword ptr [esp +24]
      movq        xmm4, qword ptr [esp +32]
      movq        xmm5, qword ptr [esp +40]
      movq        xmm6, qword ptr [esp +48]
      movq        xmm7, qword ptr [esp +56]
      jmp short   doneFPRrestoration

restoreX87stack:
      lea         &scratchReg, [&cpScratchReg+&cpScratchReg*4]
      lea         &scratchReg2, [esp+&scratchReg*2-10]         ; first FPR preserved
      neg         &cpScratchReg
restoreX87loop:
      inc         &cpScratchReg
      lea         &scratchReg, [&cpScratchReg+&cpScratchReg*4]
      fld         tbyte ptr[&scratchReg2+&scratchReg*2]        ; restore FP stack register
      jnz short   restoreX87loop
doneFPRrestoration:
      add         esp, J9PreservedFPRStackSize
ENDM



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
interpreterUnresolvedStringGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveString
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedStringGlue endp

      align 16
interpreterUnresolvedConstantDynamicGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveConstantDynamic
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedConstantDynamicGlue endp

      align 16
interpreterUnresolvedMethodTypeGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodType
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedMethodTypeGlue endp


      align 16
interpreterUnresolvedMethodHandleGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodHandle
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedMethodHandleGlue endp


      align 16
interpreterUnresolvedCallSiteTableEntryGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveInvokeDynamic
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedCallSiteTableEntryGlue endp


      align 16
interpreterUnresolvedMethodTypeTableEntryGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveHandleMethod
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedMethodTypeTableEntryGlue endp


      align 16
interpreterUnresolvedClassGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClass
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedClassGlue endp


      align 16
interpreterUnresolvedClassFromStaticFieldGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClassFromStaticField
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedClassFromStaticFieldGlue endp


      align 16
interpreterUnresolvedStaticFieldGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticField
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedStaticFieldGlue endp


      align 16
interpreterUnresolvedStaticFieldSetterGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticFieldSetter
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedStaticFieldSetterGlue endp

      align 16
interpreterUnresolvedFieldGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveField
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedFieldGlue endp


      align 16
interpreterUnresolvedFieldSetterGlue proc near
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveFieldSetter
;      int 3

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

      push        ebp                                          ; keep the address of the vmThread at the top of the stack
      mov         ecx, dword ptr [esp+124]                     ; ecx = full cpIndex dword

      ; Check for the special case of a data resolution without code patching.
      ; This happens when you have an explicit NULLCHK guarding a data resolution and
      ; we need to ensure the exceptions are thrown in the correct order if they occur.
      ; In this case any resolution exceptions are required to be thrown before any NPEs.
      ;
      test        ecx, eq_ResolveWithoutPatchingMainline
      jnz         doneDataResolution

      ; Check if the snippet instruction must be patched.  This is not necessary, for
      ; example, for unresolved strings because the snippet instruction is already
      ; correct since it contains the address of the constant pool entry from which
      ; the java/lang/Strings address can now be loaded.
      ;
      test        ecx, eq_ResolveWithoutPatchingSnippet
      jnz         disp32AlreadyPatched

      mov         edx, dword ptr [esp+116]                     ; edx = EA of header byte in snippet

      ; Patch the cached instruction bytes in the snippet.  The disp32 field must lie within
      ; the first 8 bytes of the instruction.  Valid default values for the disp32 field
      ; are 0, 4, or an address.
      ;
      ; Multiple threads can be executing this code, but all threads must be patching the
      ; same value.
      ;
      movzx       edx, byte ptr [edx]                          ; header byte
      and         edx, 0fh                                     ; isolate disp32 offset in first nibble
      mov         edi, eax                                     ; edi = resolve result
      and         edi, 0fffffffeh                              ; whack any low tagging of resolve result
      test        ecx, eq_HighWordOfLongPair                   ; is this the high word of a long pair?
      jz short    patchDisp32
      add         edi, 4                                       ; yes, offset address by 4 bytes

patchDisp32:
      mov         dword ptr [esi+edx+1], edi                   ; patch the disp32 field in the snippet
      call        memoryFence                                  ; make sure all stores crossing a line are issued

disp32AlreadyPatched:

      ; For static resolves if the address from the resolution helper is low tagged then
      ; this means that class initialization has not completed yet and we should not
      ; be patching the mainline code (in case the remaining initialization were to fail).
      ;
      ; We could probably get away with just testing whether the resolve result was low-tagged
      ; in order to do this.
      ;
      mov         ebp, dword ptr [esp+116]                     ; ebp = EA of header byte in snippet
      test        ecx, eq_ResolveStatic                        ; static resolve?
      jz short    patchMainlineCode
      test        eax, 1                                       ; low-tagged resolution result (clinit not finished)?
      jnz         executeSnippetCode

patchMainlineCode:

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this snippet + the following 3 bytes.
      ;
      lea         eax, [ebp-12]                                ; assume short cpIndex push
      test        ecx, eq_WideCPIndexPush
      jz short    shortCPIndexPush
      lea         eax, [eax-3]                                 ; account for wide cpIndex push

shortCPIndexPush:
      mov         esi, dword ptr [esp+128]                     ; edx = RA in mainline
      mov         edx, dword ptr [esi-1]                       ; edx = 3 bytes after the call to snippet + high byte of disp32
      sub         eax, esi                                     ; calculate disp32
      rol         eax, 8
      mov         dl, al                                       ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                     ; add CALL opcode

      test        ecx, eq_ResolveStatic                        ; static resolve?
      jnz         patchUnresolvedStatic

loadTargetInstruction:

      ; Load the patched data reference instruction in ecx:ebx.
      ;
      mov         ebx, dword ptr [ebp+1]                       ; low dword of patch instruction in snippet
      mov         ecx, dword ptr [ebp+5]                       ; high dword of patch instruction in snippet

mergePatchUnresolvedDataReference:

; determine if nop patching might be necessary
; if a volatile check is necessary, code patching will occur within the check.
;
      test        dword ptr [esp+124], eq_VolatilityCheck      ; get the cpIndex
      jz short    noVolatileCheck

      call checkReferenceVolatility
      jmp  doneDataResolution

noVolatileCheck:

      ; Attempt to patch the code.
      ;
ifdef WINDOWS
      lock cmpxchg8b   qword ptr [esi-5]
else
      lock cmpxchg8b   [esi-5]
endif

doneDataResolution:

      pop         eax                                          ; remove the vmThread address from the top of the stack
      FPRDataResolveEpilogue ecx, edi, edx

      pop         eax                                          ; restore
      pop         ebx                                          ; restore
      pop         ecx                                          ; restore
      pop         edx                                          ; restore
      pop         edi                                          ; restore
      pop         esi                                          ; restore
      pop         ebp                                          ; restore
      add         dword ptr [esp+16], -5                       ; re-run patched instruction
      popfd                                                    ; restore
      lea         esp, [esp+12]                                ; skip over cpAddr, cpIndex, RA in snippet
      ret

doneDataResolutionAndNoRerun:
      pop		  eax										   ; remove the vmThread address from the top of the stack
      FPRDataResolveEpilogue ecx, edi, edx

      pop		  eax										   ; restore
      pop		  ebx										   ; restore
      pop		  ecx										   ; restore
      pop		  edx										   ; restore
      pop		  edi										   ; restore
      pop		  esi										   ; restore
      pop		  ebp										   ; restore
      popfd 												   ; restore
      lea         esp, [esp+12]                                ; skip over cpAddr, cpIndex, RA in snippet
      ret

patchUnresolvedStatic:

      ; For unresolved statics the RET instruction may need to be overwritten
      ; if the static instruction length < 8.
      ;
      movzx       ecx, byte ptr [ebp]                          ; header byte
      cmp         ecx, 080h                                    ; length < 8?
      jge         loadTargetInstruction

      ; The RET instruction must be inserted at least 4 bytes past the start
      ; of the unresolved static instruction.  This is guaranteed because the
      ; unresolved static will have a disp32 field.  Since this is true, then
      ; only the instruction data in ECX needs to be patched.
      ;
      and         ecx, 030h                                    ; isolate dword offset
      shr         ecx, 1                                       ; number of bits to rotate by

      mov         ebx, dword ptr [ebp+5]                       ; high dword of patch instruction in snippet
      ror         ebx, cl
      mov         bl, byte ptr [ebp+9]                         ; replace RET instruction by cached original value
      rol         ebx, cl

      mov         ecx, dword ptr [ebp+1]                       ; low dword of patch instruction in snippet
      xchg        ebx, ecx
      jmp         mergePatchUnresolvedDataReference

executeSnippetCode:
      ; Execute the patched instruction in the snippet.  The instruction is followed
      ; by a 1-byte RET to route control to after the data reference instruction in the
      ; mainline code.
      ;
      pop         eax                                          ; remove the vmThread address from the top of the stack
      FPRDataResolveEpilogue ecx, edi, edx

      movzx       ecx, byte ptr [ebp]                          ; header byte in snippet
      and         ecx, 0f0h                                    ; mask off static instruction length
      shr         ecx, 4
      sub         ecx, 5                                       ; account for length of CALL instruction
      add         dword ptr [esp+44], ecx                      ; adjust RA in mainline code to point to the
                                                               ;   instruction following the patched instruction

      pop         eax                                          ; restore
      pop         ebx                                          ; restore
      pop         ecx                                          ; restore
      pop         edx                                          ; restore
      pop         edi                                          ; restore
      pop         esi                                          ; restore
      pop         ebp                                          ; restore
      add         dword ptr [esp+4], 1                         ; adjust RA in snippet to patched instruction in snippet
      popfd                                                    ; restore
      ret         8                                            ; execute patched instruction in snippet

interpreterUnresolvedFieldSetterGlue endp



MTUnresolvedInt32Load proc near
MTUnresolvedInt32Load endp

MTUnresolvedInt64Load proc near
MTUnresolvedInt64Load endp

MTUnresolvedFloatLoad proc near
MTUnresolvedFloatLoad endp

MTUnresolvedDoubleLoad proc near
MTUnresolvedDoubleLoad endp

MTUnresolvedAddressLoad proc near
MTUnresolvedAddressLoad endp

MTUnresolvedInt32Store proc near
MTUnresolvedInt32Store endp

MTUnresolvedInt64Store proc near
MTUnresolvedInt64Store endp

MTUnresolvedFloatStore proc near
MTUnresolvedFloatStore endp

MTUnresolvedDoubleStore proc near
MTUnresolvedDoubleStore endp

MTUnresolvedAddressStore proc near
MTUnresolvedAddressStore endp

_TEXT ends

else

; --------------------------------------------------------------------------------
;
;                                    64-BIT
;
; --------------------------------------------------------------------------------

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
eq_offsetof_J9Object_clazz equ   8     ; offset of class pointer in a J9Object
#else
eq_offsetof_J9Object_clazz equ   16     ; offset of class pointer in a J9Object
#endif

        include jilconsts.inc

OPTION NOSCOPED

_TEXT   segment para 'CODE'

      public         interpreterUnresolvedStaticGlue
      public         interpreterUnresolvedSpecialGlue
      public         updateInterpreterDispatchGlueSite
      public         interpreterVoidStaticGlue
      public         interpreterEAXStaticGlue
      public         interpreterRAXStaticGlue
      public         interpreterXMM0FStaticGlue
      public         interpreterXMM0DStaticGlue
      public         interpreterSyncVoidStaticGlue
      public         interpreterSyncEAXStaticGlue
      public         interpreterSyncRAXStaticGlue
      public         interpreterSyncXMM0FStaticGlue
      public         interpreterSyncXMM0DStaticGlue

      public         interpreterUnresolvedStringGlue
      public         interpreterUnresolvedMethodTypeGlue
      public         interpreterUnresolvedMethodHandleGlue
      public         interpreterUnresolvedCallSiteTableEntryGlue
      public         interpreterUnresolvedMethodTypeTableEntryGlue
      public         interpreterUnresolvedClassGlue
      public         interpreterUnresolvedClassFromStaticFieldGlue
      public         interpreterUnresolvedStaticFieldGlue
      public         interpreterUnresolvedStaticFieldSetterGlue
      public         interpreterUnresolvedFieldGlue
      public         interpreterUnresolvedFieldSetterGlue
      public         interpreterUnresolvedConstantDynamicGlue

      public         MTUnresolvedInt32Load
      public         MTUnresolvedInt64Load
      public         MTUnresolvedFloatLoad
      public         MTUnresolvedDoubleLoad
      public         MTUnresolvedAddressLoad

      public         MTUnresolvedInt32Store
      public         MTUnresolvedInt64Store
      public         MTUnresolvedFloatStore
      public         MTUnresolvedDoubleStore
      public         MTUnresolvedAddressStore

      ExternHelper   jitResolveStaticMethod
      ExternHelper   jitResolveSpecialMethod
      ExternHelper   jitCallCFunction
      ExternHelper   jitResolveString
      ExternHelper   jitResolveConstantDynamic
      ExternHelper   jitResolveMethodType
      ExternHelper   jitResolveMethodHandle
      ExternHelper   jitResolveInvokeDynamic
      ExternHelper   jitResolveHandleMethod
      ExternHelper   jitResolveClass
      ExternHelper   jitResolveClassFromStaticField
      ExternHelper   jitResolveStaticField
      ExternHelper   jitResolveStaticFieldSetter
      ExternHelper   jitResolveField
      ExternHelper   jitResolveFieldSetter
      ;1179
      ExternHelper   jitResolvedFieldIsVolatile

      ExternHelper   icallVMprJavaSendStatic0
      ExternHelper   icallVMprJavaSendStatic1
      ExternHelper   icallVMprJavaSendStaticJ
      ExternHelper   icallVMprJavaSendStaticF
      ExternHelper   icallVMprJavaSendStaticD
      ExternHelper   icallVMprJavaSendStaticSync0
      ExternHelper   icallVMprJavaSendStaticSync1
      ExternHelper   icallVMprJavaSendStaticSyncJ
      ExternHelper   icallVMprJavaSendStaticSyncF
      ExternHelper   icallVMprJavaSendStaticSyncD

      ExternHelper   adjustTrampolineInterpretedDispatchGlueDisp32_unwrapper
      ExternHelper   mcc_callPointPatching_unwrapper


CheckIfMethodCompiledAndPatch macro helperName
      test        byte ptr[rdi+J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit
      jnz         &helperName
      jmp         mergedStaticGlueCallFixer2
endm


; interpreterUnresolved{Static|Special}Glue
;
; Resolve a static or special method.  The call instruction routing control here is
; updated to load the RAM method into RDI.
;
; The unresolved {Static|Special} interpreted dispatch snippet look like:
;       align 8
; (10)  call  interpreterUnresolved{Static|Special}Glue  ; replaced with "mov rdi, 0x0000aabbccddeeff"
; (5)   call  updateInterpreterDispatchGlueSite          ; replaced with "JMP disp32"
; (2)   dw    2-byte glue method helper index
; (8)   dq    cpAddr
; (4)   dd    cpIndex
;
; NOTES:
;
; [1] This runtime code uses the JIT linkage registers as volatile registers and hence
;     does not preserve them.  This is because the eventual dispatch path will be through
;     the interpreter which reads the parameters from the stack.  The backspilling has
;     already occured at this point.
;
; [2] A POP is not strictly necessary to shapen the stack.  The RA could be left on the
;     stack and the new stack shape updated in getJitStaticMethodResolvePushes().  It
;     was left this way because we have to dork with the RA anyways to get back from
;     this function which will already cause a return mispredict.  Leaving it as is
;     changes less code.
;
; [3] STACK SHAPE: must maintain stack shape expected by call to getJitStaticMethodResolvePushes()
;     across the call to the lookup helper.
;
      align 16
interpreterUnresolvedStaticGlue proc
      pop         rdi                                          ; RA in snippet (see [2] above)

      ; Attempt to resolve static method.
      ;
      mov         rax, qword ptr[rsp]                          ; p1) rax = RA in mainline code
      mov         rsi, qword ptr[rdi+12]                       ; p2) rsi = cpAddr
                                                               ;        12 = 5 + 5 (call update) + 2 (DW)
      mov         edx, dword ptr[rdi+20]                       ; p3) rdx = cpIndex
                                                               ;        20 = 5 + 5 (call update) + 2 (DW) + 8 (cpAddr)
      CallHelperUseReg jitResolveStaticMethod,rax

      ; The interpreter may low-tag the result to avoid populating the constant pool--whack it.
      ;
      and         rax, -2

mergeInterpreterUnresolvedDispatch:

      ; Patch the call that brought us here into a load of the resolved RAM method into RDI.
      ;
      lea         rdi, [rdi-5]                                 ; Adjust the return address to re-run the patched
      push        rdi                                          ; instruction.  The RET will mispredict anyway so we
                                                               ; can get away with pushing the adjusted RA back on
                                                               ; the stack.
      rol         rax, 16
      mov         ax, 0bf48h                                   ; REX+MOV bytes

      mov         qword ptr[rdi], rax
      ret

interpreterUnresolvedStaticGlue endp


      align 16
interpreterUnresolvedSpecialGlue proc
      pop         rdi                                          ; RA in snippet

      ; Attempt to resolve special method.
      ;
      mov         rax, qword ptr[rsp]                          ; p1) rax = RA in mainline code
      mov         rsi, qword ptr[rdi+12]                       ; p2) rsi = cpAddr
                                                               ;        12 = 5 + 5 (call update) + 2 (DW)
      mov         edx, dword ptr[rdi+20]                       ; p3) rdx = cpIndex
                                                               ;        20 = 5 + 5 (call update) + 2 (DW) + 8 (cpAddr)
      CallHelperUseReg jitResolveSpecialMethod,rax
      jmp         mergeInterpreterUnresolvedDispatch

interpreterUnresolvedSpecialGlue endp


; updateInterpreterDispatchGlueSite
;
; Now that a resolved RAM method is available, determine the appropriate interpreter
; dispatch glue code to call and then patch the snippet to jump to it directly (or
; to its trampoline).
;
; NOTES:
;
; [1] The RAM method is in RDI on entry.  It was loaded by the preceding instruction.
;
; [2] This runtime code uses the JIT linkage registers as volatile registers and hence
;     does not preserve them.  This is because the eventual dispatch path will be through
;     the interpreter which reads the parameters from the stack.  The backspilling has
;     already occured at this point.
;
      align 16
updateInterpreterDispatchGlueSite proc
      mov         rcx, qword ptr[rsp]                          ; rcx = snippet RA

      sub         rsp, 8                                       ; descriptor+32: return value
      push        rcx                                          ; descriptor+24: snippet RA
      mov         edx, dword ptr [rcx+10]                      ; 10 = 2 (DW index) + 8 (cpAddr)
      push        rdx                                          ; descriptor+16: cpIndex
      mov         rdx, qword ptr [rcx+2]                       ; 2 = (DW index)
      push        rdx                                          ; descriptor+8:  cpAddr
      push        rdi                                          ; descriptor+0:  RAM method
      MoveHelper  rax, adjustTrampolineInterpretedDispatchGlueDisp32_unwrapper ; p1) helper address
      lea         rsi, [rsp]                                   ; p2) EA parms in TLS area
      lea         rdx, [rsp+32]                                ; p3) EA of return value in TLS area
      call        jitCallCFunction
      add         rsp, 32                                      ; tear down descriptor
      pop         rax                                          ; rax = return value (disp32)

      ; Patch the call instruction with a direct jump to the glue code.
      ; An atomic CMPXCHG is not required to patch this in because all threads
      ; should be patching the same value and should either see a call to this
      ; PicBuilder function or a jump to the interpreter dispatch glue.
      ;
      lea         rcx, [rcx-5]

      mov         eax, eax                                     ; whack any upper bit sign extension
      shl         rax, 8
      mov         al, 0e9h                                     ; add JMP opcode

      mov         rdx, qword ptr[rcx]                          ; load up existing 8-bytes
      shr         rdx, 40
      shl         rdx, 40                                      ; isolate upper 24-bits
      or          rdx, rax                                     ; merge instructions

      mov         qword ptr[rcx], rdx                          ; patch the call instruction

      MEMORY_FENCE

      sub         qword ptr[rsp], 5                            ; re-run the calling instruction
      ret

updateInterpreterDispatchGlueSite endp


; mergedStaticGlueCallFixer
;
; This function is a back-patching routine for interpreter calls to static
; methods that have recently been compiled. It patches the call site (in the
; code cache) of an interpreter call snippet, so that future executions will
; invoke the compiled method instead.
;
; NOTES:
;
; [1] The RAM method is in RDI on entry.  It was loaded by the preceding instruction.
;
; [2] This runtime code uses the JIT linkage registers as volatile registers and hence
;     does not preserve them.  This is because the eventual dispatch path will be through
;     the interpreter which reads the parameters from the stack.  The backspilling has
;     already occured at this point.
;
      align 16
mergedStaticGlueCallFixer2 proc
      mov         rcx, qword ptr[rdi + J9TR_MethodPCStartOffset] ; rcx = interpreter entry point of the compiled method
      mov         rsi, qword ptr[rsp]                          ; rsi = RA in mainline code
      sub         rsi, 5
      push        0                                            ; descriptor+24: 0
      push        rcx                                          ; descriptor+16: compiled method PC
      push        rsi                                          ; descriptor+8:  call site
      push        rdi                                          ; descriptor+0:  RAM method
      MoveHelper  rax, mcc_callPointPatching_unwrapper         ; p1) rax = helper
      lea         rsi, [rsp]                                   ; p2) rsi = EA of descriptor
      lea         rdx, [rsp]                                   ; p3) rdx = EA of return value
      call        jitCallCFunction
      add         rsp, 32                                      ; tear down descriptor
      jmp         rcx                                          ; Dispatch compiled method through the interpreted entry
                                                               ; point.  This is because the linkage registers may not be
                                                               ; set up any longer.
mergedStaticGlueCallFixer2 endp


; interpreter[Sync]{Void,EAX,RAX,XMM0F,XMM0D}StaticGlue
;
; This function checks if the given RAM method in rdi has already been
; compiled, and if it has not, jumps to the VM helper that interprets
; methods of the matching return type.  If the given method has
; already been compiled, this function jumps to mergedStaticGlueCallFixer,
; which patches the call site so that future invocations will call the
; compiled method directly.
;
      align 16
interpreterVoidStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStatic0
interpreterVoidStaticGlue endp

      align 16
interpreterSyncVoidStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSync0
interpreterSyncVoidStaticGlue endp

      align 16
interpreterEAXStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStatic1
interpreterEAXStaticGlue endp

      align 16
interpreterSyncEAXStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSync1
interpreterSyncEAXStaticGlue endp

      align 16
interpreterRAXStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticJ
interpreterRAXStaticGlue endp

      align 16
interpreterSyncRAXStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSyncJ
interpreterSyncRAXStaticGlue endp

      align 16
interpreterXMM0FStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticF
interpreterXMM0FStaticGlue endp

      align 16
interpreterSyncXMM0FStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSyncF
interpreterSyncXMM0FStaticGlue endp

      align 16
interpreterXMM0DStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticD
interpreterXMM0DStaticGlue endp

      align 16
interpreterSyncXMM0DStaticGlue proc
      CheckIfMethodCompiledAndPatch icallVMprJavaSendStaticSyncD
interpreterSyncXMM0DStaticGlue endp


; --------------------------------------------------------------------------------
;
;                            64-BIT DATA RESOLUTION
;
; --------------------------------------------------------------------------------

DataResolvePrologue MACRO
      pushfq                                                   ; rsp+256 = flags
      push        r15                                          ; rsp+248
      push        r14                                          ; rsp+240
      push        r13                                          ; rsp+232
      push        r12                                          ; rsp+224
      push        r11                                          ; rsp+216
      push        r10                                          ; rsp+208
      push        r9                                           ; rsp+200
      push        r8                                           ; rsp+192
      push        rsp                                          ; rsp+184 -- Ugh
      push        rbp                                          ; rsp+176
      push        rsi                                          ; rsp+168
      push        rdi                                          ; rsp+160
      push        rdx                                          ; rsp+152
      push        rcx                                          ; rsp+144
      push        rbx                                          ; rsp+136
      push        rax                                          ; rsp+128
      sub         rsp, 128
      movsd       qword ptr [rsp+0],   xmm0
      movsd       qword ptr [rsp+8],   xmm1
      movsd       qword ptr [rsp+16],  xmm2
      movsd       qword ptr [rsp+24],  xmm3
      movsd       qword ptr [rsp+32],  xmm4
      movsd       qword ptr [rsp+40],  xmm5
      movsd       qword ptr [rsp+48],  xmm6
      movsd       qword ptr [rsp+56],  xmm7
      movsd       qword ptr [rsp+64],  xmm8
      movsd       qword ptr [rsp+72],  xmm9
      movsd       qword ptr [rsp+80],  xmm10
      movsd       qword ptr [rsp+88],  xmm11
      movsd       qword ptr [rsp+96],  xmm12
      movsd       qword ptr [rsp+104], xmm13
      movsd       qword ptr [rsp+112], xmm14
      movsd       qword ptr [rsp+120], xmm15
ENDM


DispatchUnresolvedDataHelper MACRO helper
      mov         rdi, qword ptr[rsp+264]                      ; RA in snippet (see stack shape below)
      mov         rax, qword ptr[rdi]                          ; p1) rax = cpAddr
      mov         esi, dword ptr[rdi+8]                        ; p2) rsi = cpIndex
      and         esi, 1ffffh                                  ; isolate the cpIndex
      mov         rdx, qword ptr[rsp+272]                      ; p3) rdx = RA in mainline code
                                                               ;           (see stack shape below)
      CallHelperUseReg &helper,rax
ENDM


DataResolveEpilogue MACRO
      movsd       xmm0,  qword ptr [rsp+0]
      movsd       xmm1,  qword ptr [rsp+8]
      movsd       xmm2,  qword ptr [rsp+16]
      movsd       xmm3,  qword ptr [rsp+24]
      movsd       xmm4,  qword ptr [rsp+32]
      movsd       xmm5,  qword ptr [rsp+40]
      movsd       xmm6,  qword ptr [rsp+48]
      movsd       xmm7,  qword ptr [rsp+56]
      movsd       xmm8,  qword ptr [rsp+64]
      movsd       xmm9,  qword ptr [rsp+72]
      movsd       xmm10, qword ptr [rsp+80]
      movsd       xmm11, qword ptr [rsp+88]
      movsd       xmm12, qword ptr [rsp+96]
      movsd       xmm13, qword ptr [rsp+104]
      movsd       xmm14, qword ptr [rsp+112]
      movsd       xmm15, qword ptr [rsp+120]
      add         rsp, 128
      pop         rax
      pop         rbx
      pop         rcx
      pop         rdx
      pop         rdi
      pop         rsi

      ; Do not pop the old RSP value as the stack may have moved during the
      ; resolution.  RBP is already restored by the VM.
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
ENDM


         align 16
;1179
checkReferenceVolatility proc

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
      mov         rbp, qword ptr [rsp + 240]                    ; [rsp + 176 + 8 + 56]
                                                                ; [rsp + rbp stack slot + proc RA + local reg preservation]

      ; determine if the field is volatile.
      ;
      mov         rbx, qword ptr [rsp + 328]                    ; load the snippet RA [rsp + 264 + 8 + 56]
      mov         edx, dword ptr [rbx + 8]                      ; load the cpIndex [snippet RA + size of cpAddr]
      mov         esi, edx
      and         edx, eq_ResolveStatic                         ; get the static flag bit
      and         esi, eq_CPIndexMask                           ; mask with the CPIndexMask
      mov         rax, qword ptr [rbx]                          ; load the cpAddr [snippet RA]

      ; call the volatile check
      ;
      CallHelperUseReg jitResolvedFieldIsVolatile,rax

      ; if this field is not volatile, patch the barrier with a nop
      ;
      test        eax, eax
      jnz         restoreRegs


      ; determine what kind of fence we are dealing with: LOCK OR [ESP] (or mfence using appropriate command line option)
      ;
      mov         rsi, J9TR_VMThread_javaVM[rbp]
      mov         rsi, J9TR_JavaVMJitConfig[rsi]
      mov         rsi, J9TR_JitConfig_runtimeFlags[rsi]

      ; get the RA in the mainline code
      ;
      mov         r9, qword ptr [rsp + 336]

      ; determine if we are looking at a store to a static field
      ;
      cmp        edx, eq_ResolveStatic
      je         patchStatic

      ; get the instruction descriptor and find the length of the patched instruction
      ;

      mov         bl, byte ptr [rbx + 12]                       ; get the first byte of the instruction descriptor (+ 8 (length of cpAddr) + 4 (length of cpIndex))
      and         rbx, 0f0h                                     ; find the length of the instruction stored in the upper nibble
      shr         rbx, 4
      lea         rbx, qword ptr [rbx - 5]
      lea         rax, qword ptr [r9 + rbx]

      ; select the patching path based on the type of barrier being used
      ;

      test        esi, J9TR_runtimeFlags_PatchingFenceType
      jnz short   patchOverMfence
      jmp         patchOverLockOrESP

patchStatic:


      ; select the patching path based on the type of barrier being used
      ;
      sub         r9, 5
      test        esi, J9TR_runtimeFlags_PatchingFenceType
      jz short    patchOverLockOrESPStatic


patchOverMfenceStatic:
     ; find the address of the LOR instruction. It is either 16 or 24 bytes from the start of the resolved instruction.
     ;

     test        edx, eq_ExtremeStaticMemBarPos
     jnz         barrierAt20Offset

     lea         rax, [r9 + 16]
     jmp         patchWith3ByteNOP

barrierAt20Offset:

     lea         rax, [r9 + 20]
     jmp         patchWith3ByteNOP

patchOverMfence:

      ; patch over 3 byte mfence
      ;
      lea         rax, [rax + 3]
      mov         rcx, 0fffffffffffffffch
      and         rax, rcx                                      ; should now have the 4 byte aligned instruction of the memfence

patchWith3ByteNOP:

      mov         ecx, dword ptr [rax]                          ; load existing double word containg mfence
      and         ecx, 0ff000000h                               ; insert trailing byte of 3 word nop
      or          ecx, 000001f0fh                               ; load the the first 2 bytes of the 3 word nop
      mov         dword ptr [rax], ecx                          ; write the nop

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
     mov         edx, dword ptr [rbx + 8]                      ; get unchanged CPindex

     test        edx, eq_ExtremeStaticMemBarPos
     jnz         barrierAt24Offset

     lea         rax, [r9 + 16]
     jmp         patchWith5ByteNOP

barrierAt24Offset:

     lea         rax, [r9 + 24]
     jmp         patchWith5ByteNOP


patchOverLockOrESP:

      mov         rbx, qword ptr [rsp + 328]                    ; load the cpIndex [rsp + 272 + 8 + 56 + 8]
      mov         edx, dword ptr [rbx + 8]                      ; get unchanged CPindex

      ; patch over 5 byte lock or esp
      ;
      lea         rax, [rax + 7]

      ; if this is a float, we need to add an extra 8 bytes to account for the LEA.
      ;
      test        edx, eq_IsFloatOp
      jz          findOffset

      lea         rax, [rax + 8]

findOffset:

      mov         rcx, 0fffffffffffffff8h                       ; find the 8 byte aligned LOR instruction
      and         rax, rcx

patchWith5ByteNOP:
      ;cmp         word ptr[rax], eq_LORBinaryWord               ; make sure we are pathcing over a lock or [esp]   ;TEMP
      ;je          restoreRegs

      mov         ecx, dword ptr [rax + 4]                      ; load the existing dword with the last byte of the lock and the following 3 bytes
      and         ecx, 0ffffff00h                               ; insert the last byte if the 5 byte nop
      rol         rcx, 32
      or          rcx, 00441f0fh                                ; insert the first 4 bytes of the 5 byte nop
      mov         qword ptr [rax], rcx                          ; write the nop

restoreRegs:

      pop         r9
      pop         rbx
      pop         rsi
      pop         rdx
      pop         rcx
      pop         rax
      pop         rbp

      ret

checkReferenceVolatility endp
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
interpreterUnresolvedStringGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveString
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedStringGlue endp

      align 16
interpreterUnresolvedConstantDynamicGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveConstantDynamic
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedConstantDynamicGlue endp

      align 16
interpreterUnresolvedMethodTypeGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodType
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedMethodTypeGlue endp


      align 16
interpreterUnresolvedMethodHandleGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveMethodHandle
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedMethodHandleGlue endp


      align 16
interpreterUnresolvedCallSiteTableEntryGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveInvokeDynamic
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedCallSiteTableEntryGlue endp


      align 16
interpreterUnresolvedMethodTypeTableEntryGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveHandleMethod
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedMethodTypeTableEntryGlue endp


      align 16
interpreterUnresolvedClassGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClass
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedClassGlue endp


      align 16
interpreterUnresolvedClassFromStaticFieldGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveClassFromStaticField
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedClassFromStaticFieldGlue endp


      align 16
interpreterUnresolvedStaticFieldGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticField
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedStaticFieldGlue endp


      align 16
interpreterUnresolvedStaticFieldSetterGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveStaticFieldSetter
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedStaticFieldSetterGlue endp

      align 16
interpreterUnresolvedFieldGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveField
;      int 3
      jmp commonUnresolvedCode
interpreterUnresolvedFieldGlue endp

      align 16
interpreterUnresolvedFieldSetterGlue proc
      DataResolvePrologue
      DispatchUnresolvedDataHelper jitResolveFieldSetter
;      int 3

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
      ; rsp+96  : xmm12
      ; rsp+88  : xmm11
      ; rsp+80  : xmm10
      ; rsp+72  : xmm9
      ; rsp+64  : xmm8
      ; rsp+56  : xmm7
      ; rsp+48  : xmm6
      ; rsp+40  : xmm5
      ; rsp+32  : xmm4
      ; rsp+24  : xmm3
      ; rsp+16  : xmm2
      ; rsp+8   : xmm1
      ; rsp+0   : xmm0
      ;
      ; rax = result from resolve helper
      ; rdi = return address in snippet

      mov         esi, dword ptr[rdi+8]                        ; full cpIndex word

      ; Check for the special case of a data resolution without code patching.
      ; This happens when you have an explicit NULLCHK guarding a data resolution and
      ; we need to ensure the exceptions are thrown in the correct order if they occur.
      ; In this case any resolution exceptions are required to be thrown before any NPEs.
      ;
      test        esi, eq_ResolveWithoutPatchingMainline
      jnz         doneDataResolution

      test        esi, eq_Patch8ByteResolution
      jz          patch4ByteResolution

      ; Resolved value is 8-bytes.  The target instruction must be a 'MOV R,Imm64' instruction.
      ; Synthesize the first 8 bytes of this instruction with the REX+op from the snippet and
      ; the 48-bit virtual address returned from the resolution and patch the snippet.
      ;
      ; Multiple threads can be executing this code, but all threads must be patching the
      ; same value.
      ;
      test        esi, eq_ResolveStatic
      jnz         patchStaticResolution

mergePatch8ByteResolution:
      movzx       edx, word ptr [rdi+12]                       ; rdx = REX + OpCode
      mov         rbx, rax                                     ; rbx = resolve result
      and         rbx, -2                                      ; whack any low tagging of resolve result
      shl         rbx, 16
      or          rdx, rbx                                     ; synthesize patched instruction bytes

      mov         rbx, qword ptr [rsp+272]                     ; rbx = RA in mainline
      mov         qword ptr [rbx-5], rdx                       ; patch mainline

; determine if nop patching might be necessary
;

     test        dword ptr [rdi + 8], eq_VolatilityCheck       ; test for the volatility check flag
     jz short    noVolatileCheck8Byte

     call checkReferenceVolatility


noVolatileCheck8Byte:

      DataResolveEpilogue

      add         qword ptr [rsp+16], -5                       ; re-run patched instruction
      popfq                                                    ; restore
      lea         rsp, [rsp+8]                                 ; skip over RA in snippet--mispredict
      ret

patchStaticResolution:
      test        rax, 1                                       ; low-tagged resolution result (clinit not finished)?
      jz          mergePatch8ByteResolution

      ; Static resolves have the entire unresolved address load instruction
      ; embedded in the snippet plus a RET instruction.  The snippet must
      ; be patched and executed.
      ;
      movzx       edx, word ptr [rdi+12]                       ; rdx = REX + OpCode
      mov         rbx, rax                                     ; rbx = resolve result
      and         rbx, -2                                      ; whack any low tagging of resolve result
      shl         rbx, 16
      or          rdx, rbx                                     ; synthesize patched instruction bytes

      mov         qword ptr [rdi+12], rdx                      ; patch snippet
      mfence                                                   ; make sure memory stores are seen

doneDataResolution:
      DataResolveEpilogue

      add         qword ptr [rsp+16], 5                        ; patch the RA in the mainline code to skip
                                                               ;    over the remaining 5 bytes of what would
                                                               ;    have been the 10-byte MOV R,Imm64 instruction
                                                               ;    that did not get patched.

      add         qword ptr [rsp+8], 12                        ; patch RA in snippet such that the instruction
                                                               ;    patched in the snippet is re-run.
                                                               ;    +12 = +8 (cpAddr) + 4 (cpIndex)
      popfq
      ret

doneDataResolutionAndNoRerun:
      DataResolveEpilogue

      popfq
      lea         rsp, [rsp+8]
      ret

patch4ByteResolution:

      ; Patch the cached instruction bytes in the snippet.  The disp32 field must lie within
      ; the first 8 bytes of the instruction.
      ;
      ; Multiple threads can be executing this code, but all threads must be patching the
      ; same value.
      ;
      movzx       ecx, byte ptr [rdi+12]                       ; rcx = header byte
      and         ecx, 0fh                                     ; isolate disp32 offset in first nibble

      mov         rdx, qword ptr [rdi+13]                      ; rdx = cached instruction bytes
                                                               ;    +13 = +8 (cpAddr) + 4 (cpIndex) + 1 (header)
      shl         ecx, 3
      ror         rdx, cl                                      ; rotate disp32 into lower dword

      ; The dword field from the snippet is assumed to be 0.
      ;
      or          rdx, rax                                     ; patch the disp32 field
      rol         rdx, cl

      mov         rbx, qword ptr [rsp+272]                     ; rbx = RA in mainline
      mov         qword ptr [rbx-5], rdx                       ; patch mainline

; determine if nop patching might be necessary
;
      test        dword ptr [rdi + 8], eq_VolatilityCheck      ; test for the check volatility flag
      jz short    noVolatileCheck4Byte

      call checkReferenceVolatility

noVolatileCheck4Byte:

      DataResolveEpilogue

      add         qword ptr [rsp+16], -5                       ; re-run patched instruction
      popfq                                                    ; restore
      lea         rsp, [rsp+8]                                 ; skip over RA in snippet--mispredict
      ret

interpreterUnresolvedFieldSetterGlue endp


MTUnresolvedInt32Load proc
MTUnresolvedInt32Load endp

MTUnresolvedInt64Load proc
MTUnresolvedInt64Load endp

MTUnresolvedFloatLoad proc
MTUnresolvedFloatLoad endp

MTUnresolvedDoubleLoad proc
MTUnresolvedDoubleLoad endp

MTUnresolvedAddressLoad proc
MTUnresolvedAddressLoad endp

MTUnresolvedInt32Store proc
MTUnresolvedInt32Store endp

MTUnresolvedInt64Store proc
MTUnresolvedInt64Store endp

MTUnresolvedFloatStore proc
MTUnresolvedFloatStore endp

MTUnresolvedDoubleStore proc
MTUnresolvedDoubleStore endp

MTUnresolvedAddressStore proc
MTUnresolvedAddressStore endp
_TEXT ends

;; !TR_HOST_64BIT
endif

      end
