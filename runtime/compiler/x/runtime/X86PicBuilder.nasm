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
;                                    32-BIT
;
; --------------------------------------------------------------------------------

      %include "jilconsts.inc"
      %include "X86PicBuilder.inc"

      segment .text

      DECLARE_EXTERN jitLookupInterfaceMethod
      DECLARE_EXTERN jitResolveInterfaceMethod
      DECLARE_EXTERN jitResolveVirtualMethod
      DECLARE_EXTERN jitCallJitAddPicToPatchOnClassUnload
      DECLARE_EXTERN jitInstanceOf
      DECLARE_EXTERN j2iVirtual

      DECLARE_GLOBAL resolveIPicClass
      DECLARE_GLOBAL populateIPicSlotClass
      DECLARE_GLOBAL populateIPicSlotCall
      DECLARE_GLOBAL dispatchInterpretedFromIPicSlot
      DECLARE_GLOBAL IPicLookupDispatch

      DECLARE_GLOBAL resolveVPicClass
      DECLARE_GLOBAL populateVPicSlotClass
      DECLARE_GLOBAL populateVPicSlotCall
      DECLARE_GLOBAL dispatchInterpretedFromVPicSlot
      DECLARE_GLOBAL populateVPicVTableDispatch
      DECLARE_GLOBAL memoryFence


; Perform a memory fence. The capabilities of the target architecture must be inspected at runtime
; to determine the appropriate fence instruction to use.
;
      align 16
memoryFence:

      push        ecx                                       ; preserve
      mov         ecx, [ebp + J9TR_VMThread_javaVM]
      mov         ecx, [ecx + J9TR_JavaVMJitConfig]
      mov         ecx, [ecx + J9TR_JitConfig_runtimeFlags]
      test        ecx, J9TR_runtimeFlags_PatchingFenceType
      jz short    doLockOrEsp

      mfence
      jmp         doneMemoryFence

doLockOrEsp:
      lock or     dword [esp], 0

doneMemoryFence:
      pop         ecx                                ; restore
      retn

; Do direct dispatch given the J9Method (for both VPIC & IPIC)
;
; Registers on entry:
;    eax: direct J9Method pointer
;    edi: adjusted return address
;
; Stack shape on entry:
;    +16 last parameter
;    +12 RA in code cache (call to helper)
;    +8  saved edx
;    +4  saved edi
;    +0  saved eax (receiver)
;
      align 16
dispatchDirectMethod:
      mov         dword [esp+12], edi                                             ; Replace RA on stack

      test        dword [eax+J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit ; method compiled?
      jnz         dispatchDirectMethodInterpreted

      ; Method is compiled
      mov         edi, dword [eax+J9TR_MethodPCStartOffset]                       ; interpreter/JIT entry point for compiled method
      jmp         mergeDispatchDirectMethod

dispatchDirectMethodInterpreted:
      lea         edx, [eax+J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG]              ; low-tagged J9Method
      MoveHelper  edi, j2iVirtual

mergeDispatchDirectMethod:
      pop         eax                                          ; restore
      add         esp, 8                                       ; skip edi, edx
      jmp         edi

ret 



; Resolve the interface method and update the IPIC data area with the interface J9Class
; and the itable index.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the resolution helper.
;
      align 16
resolveIPicClass: ; proc near
      push        edx                                                   ; preserve
      push        edi                                                   ; preserve
      mov         edi, dword [esp+8]                                    ; RA in code cache

      cmp         byte [edi+1], 075h                                    ; is it a short branch?
      jnz         resolveIPicClassLongBranch                            ; no

      lea         edi, [edi+9]                                          ; EA of disp8 in JMP
                                                                        ;   9 = 1 + 2 (JNE) + 5 (CALL) + 1 (JMP)
      movzx       edx, byte [edi]                                       ; disp8 to end of IPic
      lea         edx, [edi+edx-11]                                     ; edx = EA of disp32 of JNE
                                                                        ; -11 = 1 (instr. after JMP) -12 (EA of disp32 of JNE)
      add         edx, dword [edx]                                      ; EA of lookup dispatch snippet
      lea         edx, [edx+14]                                         ; EA of IPic data block

mergeFindDataBlockForResolveIPicClass:
      push        eax                                                   ; push receiver

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;
      
      test        dword [edx+eq_IPicData_itableOffset], J9TR_J9_ITABLE_OFFSET_DIRECT
      jz          callResolveInterfaceMethod

      cmp         dword [edx+eq_IPicData_interfaceClass], 0
      jne         typeCheckAndDirectDispatchIPic

callResolveInterfaceMethod:
      push        edi                                                   ; p) jit valid EIP
      push        edx                                                   ; p) push the address of the constant pool and cpIndex
      CallHelperUseReg jitResolveInterfaceMethod,eax                   ; returns interpreter vtable index

      call        memoryFence                                          ; Ensure IPIC data area drains to memory before proceeding.

      test        dword [edx+eq_IPicData_itableOffset], J9TR_J9_ITABLE_OFFSET_DIRECT
      jnz         typeCheckAndDirectDispatchIPic

      push        ebx                                                   ; preserve
      push        ecx                                                   ; preserve

      ; Stack shape:
      ;
      ; +24 last parameter
      ; +20 RA in code cache (call to helper)
      ; +16 saved edx
      ; +12 saved edi
      ; +8 saved eax (receiver)
      ; +4 saved ebx
      ; +0 saved ecx
      ;

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  eax, resolveIPicClass
      MoveHelper  ebx, populateIPicSlotClass
      mov         edi, dword [esp+20]                                   ; edi = RA in mainline (call to helper)
      mov         edx, dword [edi-1]                                    ; edx = 3 bytes after the call to helper + high byte of disp32
      sub         eax, edi                                              ; Expected disp32 for call to helper
      rol         eax, 8
      mov         dl, al                                                ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                              ; add CALL opcode

      ; Construct the byte sequence in ecx:ebx to re-route the call
      ; to populateIPicSlotClass.
      ;
      sub         ebx, edi                                              ; disp32 to populate snippet
      mov         ecx, edx
      rol         ebx, 8
      mov         cl, bl                                                ; Form high byte of calculated disp32
      mov         bl, 0e8h                                              ; add CALL opcode

      ; Attempt to patch the code.
      ;
      lock cmpxchg8b    [edi-5]                                         ; EA of CMP instruction in mainline

      pop         ecx                                                   ; restore
      pop         ebx                                                   ; restore
      pop         eax
      pop         edi
      pop         edx

      sub         dword [esp], 5                                        ; fix RA to re-run the call
      ret                                                               ; branch will mispredict so single-byte RET is ok

resolveIPicClassLongBranch:
      mov         edx, dword [edi+3]                                    ; disp32 for branch to snippet
      lea         edx, [edi+edx+17]                                     ; EA of IPic data block
                                                                        ; 17 = 7 (offset to instr after branch) + 5 (CALL) + 5 (JMP)
      jmp         mergeFindDataBlockForResolveIPicClass

typeCheckAndDirectDispatchIPic:
      push        dword [esp]                                           ; p) receiver (saved eax)
      push        dword [edx+eq_IPicData_interfaceClass]                ; p) interface class
      CallHelperUseReg jitInstanceOf, eax
      test        eax, eax
      jz          throwOnFailedTypeCheckIPic

      ; Type check passed
      mov         eax, dword [edx+eq_IPicData_itableOffset]
      xor         eax, J9TR_J9_ITABLE_OFFSET_DIRECT                     ; Direct J9Method pointer

      lea         edi, [edx-5]                                          ; Adjusted return address
                                                                        ;    5 (JMP)
      jmp         dispatchDirectMethod

throwOnFailedTypeCheckIPic:
      mov         eax, [esp]                                   ; receiver (saved eax)
      lea         edx, [edx+eq_IPicData_interfaceClass]        ; EA of IClass in data block
      push        edi                                          ; p) jit EIP
      push        edx                                          ; p) EA of resolved interface class
      LoadClassPointerFromObjectHeader eax, eax, eax
      push        eax                                          ; p) receiver class
      CallHelperUseReg jitLookupInterfaceMethod, eax           ; guaranteed to throw
      int         3

ret

; Look up a resolved interface method and attempt to occupy the PIC slot
; that routed control to this helper.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateIPicSlotClass: ; proc near
      push        edx                                       ; preserve
      push        edi                                       ; preserve
      mov         edi, dword [esp+8]                        ; RA in code cache

      cmp         byte [edi+1], 075h                        ; is it a short branch?
      jnz         populateIPicClassLongBranch               ; no

      lea         edi, [edi+9]                              ; EA of disp8 in JMP
                                                            ; 9 = 1 + 2 (JNE) + 5 (CALL) + 1 (JMP)
      movzx       edx, byte  [edi]                          ; disp8 to end of IPic
      lea         edx, [edi+edx-11]                         ; edx = EA of disp32 of JNE
                                                            ; -11 = 1 (instr. after JMP) -12 (EA of disp32 of JNE)
      add         edx, dword  [edx]                         ; EA of lookup dispatch snippet

      lea         edx, [edx+14]                             ; EA of constant pool and cpIndex
                                                            ; 14 = 4 + 10 (CALL+JMP)
mergePopulateIPicClass:
      push eax                                              ; push receiver

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;



      lea         edx, [edx+eq_IPicData_interfaceClass]     ; EA of IClass in data block
      push        edi                                       ; p) jit EIP
      push        edx                                       ; p) EA of resolved interface class
      LoadClassPointerFromObjectHeader eax, eax, eax
      push        eax                                       ; p) receiver class
      CallHelperUseReg jitLookupInterfaceMethod,eax        ; returns interpreter vtable offset

      ; Lookup succeeded--attempt to grab the PIC slot for this receivers class.
      ;
      push        ebx                                       ; preserve
      push        ecx                                       ; preserve
      push        esi                                       ; preserve

      ; Stack shape:
      ;
      ; +28 last parameter
      ; +24 RA in code cache (call to helper)
      ; +20 saved edx
      ; +16 saved edi
      ; +12 saved eax (receiver)
      ; +8 saved ebx
      ; +4 saved ecx
      ; +0 saved esi
      ;

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      lea         esi, [edx-eq_IPicData_interfaceClass]     ; esi = EA of IClass in data block

      MoveHelper  eax, populateIPicSlotClass
      mov         edi, dword  [esp+24]                      ; edi = RA in mainline (call to helper)
      mov         edx, dword  [edi-1]                       ; edx = 3 bytes after the call to helper + high byte of disp32
      sub         eax, edi                                  ; Expected disp32 for call to snippet
      rol         eax, 8
      mov         dl, al                                    ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                  ; add CALL opcode

      ; Construct the CMP instruction in ecx:ebx to devirtualize the interface
      ; method for this receivers class.
      ;
      mov         cx, word  [edi+1]                               ; 2 bytes of jump to next slot
      rol         ecx, 16
      mov         ebx, dword  [esp+12]                            ; receiver
      LoadClassPointerFromObjectHeader ebx, ebx, ebx              ; receiver class
      rol         ebx, 16
      mov         cx, bx
      mov         bl, 081h                                        ; CMP opcode
      mov         bh, byte  [esi+eq_IPicData_cmpRegImm4ModRM]     ; ModRM byte in data area (offset from IClass)

      ; Attempt to patch in the CMP instruction.
      ;
      lock cmpxchg8b    [edi-5]                                   ; EA of CMP instruction in mainline


%ifdef ASM_J9VM_GC_DYNAMIC_CLASS_UNLOADING
      jnz         short IPicClassSlotUpdateFailed

      ; Register the address of the immediate field in the CMP instruction
      ; for dynamic class unloading iff the interface class and the
      ; receiver class have been loaded by different class loaders.
      ;
      mov         eax, dword  [esp+12]                            ; grab the receiver before its restored
      LoadClassPointerFromObjectHeader eax, edx, edx              ; receiver class
      mov         eax, dword  [edx+J9TR_J9Class_classLoader]      ; receivers class loader

      mov         ebx, dword  [esi+eq_IPicData_interfaceClass]    
      cmp         eax, dword  [ebx+J9TR_J9Class_classLoader]      ; compare class loaders
      jz short IPicClassSlotUpdateFailed                          ; class loaders are the same--do nothing

      lea         edi, [edi-3]                                    ; EA of immediate
      push        edi                                             ; p) address to patch
      push        edx                                             ; p) receivers class
      CallHelper jitCallJitAddPicToPatchOnClassUnload
IPicClassSlotUpdateFailed:
%endif

      pop         esi                                             ; restore
      pop         ecx                                             ; restore
      pop         ebx                                             ; restore
      pop         eax                                             ; restore receiver
      pop         edi
      pop         edx

      sub         dword [esp], 5                                  ; fix RA to re-run the CMP
      ret                                                         ; branch will mispredict so single-byte RET is ok

populateIPicClassLongBranch:
      mov         edx, dword  [edi+3]                             ; disp32 for branch to snippet
      lea         edx, [edi+edx+17]                               ; EA of constant pool and cpIndex
                                                                  ; 17 = 7 (offset to instr after branch) + 5 (CALL) +
                                                                  ; 5 (JMP)
      jmp         mergePopulateIPicClass
ret


; Look up a resolved interface method and update the call to the method
; that routed control to this helper.
;
; STACK SHAPE: must maintain stack shape expected by call to
; getJitVirtualMethodResolvePushes() across the call to the lookup helper.
;
      align 16
populateIPicSlotCall:
      push        edx                                       ; preserve
      push        edi                                       ; preserve
      mov         edi, dword  [esp+8]                       ; RA in code cache

      cmp         byte  [edi-13], 085h                      ; is this the last slot?
                                                            ; The branch in the last slot will be a long branch.
                                                            ; This byte will either be a long JNE opcode (last
                                                            ; slot) or a CMP opcode.
                                                            ; -13 = -5 (CALL) -3 (NOP) -5 (CMP or JNE)
      jz          populateLastIPicSlot
      movzx       edx, byte  [edi+1]                        ; JMP displacement to end of IPic
      lea         edx, [edi+edx-6]                          ; edx = EA of NOP instruction after JNE in last slot
                                                            ; -6 = +2 (skip JMP disp8) - 5 (call in last slot) - 3 (NOP)

mergeIPicSlotCall:
      add         edx, dword  [edx-4]                       ; EA of lookup dispatch snippet
      lea         edx, [edx+10]                             ; EA of constant pool and cpIndex
                                                            ; 10 = 5 (CALL) + 5 (JMP)

      push        eax                                       ; push receiver
      LoadClassPointerFromObjectHeader eax, eax, eax        ; receiver class

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;



      lea         edx, [edx+eq_IPicData_interfaceClass]           ; EA of IClass in data block

      push        edi                                             ; p) jit EIP
      push        edx                                             ; p) EA of resolved interface class
      push        eax                                             ; p) receiver class
      CallHelperUseReg jitLookupInterfaceMethod,eax              ; returns interpreter vtable offset
      mov         edx, eax
      pop         eax                                             ; restore receiver
      push        ecx                                             ; preserve

      LoadClassPointerFromObjectHeader eax, ecx, ecx              ; receiver class
      mov         ecx, dword  [ecx+edx]                           ; J9Method from interpreter vtable
      test        byte  [ecx + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit       ; method compiled?
      jnz         short IPicSlotMethodInterpreted

mergeUpdateIPicSlotCallWithCompiledMethod:
      mov         ecx, dword  [ecx+J9TR_MethodPCStartOffset]

mergeUpdateIPicSlotCall:
      mov         edi, dword  [esp+12]                            ; RA in code cache
      sub         ecx, edi                                        ; compute new disp32
      mov         dword  [edi-4], ecx                             ; patch disp32 in call instruction

      pop         ecx                                             ; restore
      add         esp, 8                                          ; edx and edi do not need to be restored because they
                                                                  ; are killed by the linkage
      sub         edi, 5                                          ; set up RA to re-run patched call instruction
      mov         dword  [esp], edi
      ret                                                         ; re-run the call instruction

populateLastIPicSlot:
      lea         edx, [edi-8]                                    ; 8 = 5 (CALL) + 3 (NOP)
      jmp         mergeIPicSlotCall

IPicSlotMethodInterpreted:
      MoveHelper  ecx, dispatchInterpretedFromIPicSlot
      jmp         short mergeUpdateIPicSlotCall

ret


; Call an interpreted method from an IPic slot.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
dispatchInterpretedFromIPicSlot:
      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache

      cmp         byte  [edi-13], 085h                            ; is this the last slot?
                                                                  ; The branch in the last slot will be a long branch.
                                                                  ; This byte will either be a long JNE opcode (last
                                                                  ; slot) or a CMP opcode.
                                                                  ; -13 = -5 (CALL) -3 (NOP) -5 (CMP or JNE)
      jz          interpretedLastIPicSlot
      movzx       edx, byte  [edi+1]                              ; JMP displacement to end of IPic
      lea         edx, [edi+edx-6]                                ; edx = EA of NOP instruction after JNE in last slot
                                                                  ; -6 = +2 (skip JMP disp8) - 5 (call in last slot) - 3 (NOP)
mergeIPicInterpretedDispatch:
      add         edx, dword  [edx-4]                             ; EA of lookup dispatch snippet
      lea         edx, [edx+10]                                   ; EA of constant pool and cpIndex
                                                                  ; 10 = 5 (CALL) + 5 (JMP)

      push        eax                                             ; push receiver
      LoadClassPointerFromObjectHeader eax, eax, eax              ; receiver class

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;

      lea         edx, [edx+eq_IPicData_interfaceClass]           ; EA of IClass in data block
                                                                  ; 18 = 5 (CALL) + 5 (JMP) + 8 (cpAddr,cpIndex)

      push        edi                                             ; p) jit EIP
      push        edx                                             ; p) EA of resolved interface class
      push        eax                                             ; p) receiver class
      CallHelperUseReg jitLookupInterfaceMethod,eax              ; returns interpreter vtable offset
      mov         edx, eax
      pop         eax                                             ; restore receiver
      push        ecx                                             ; preserve

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved ecx
      ;
      LoadClassPointerFromObjectHeader eax, ecx, ecx              ; receiver class
      mov         ecx, dword  [ecx+edx]                           ; J9Method from interpreter vtable
      test        byte  [ecx + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit       ; method compiled?
      jz          mergeUpdateIPicSlotCallWithCompiledMethod

      ; Target method is still interpreted--dispatch it.
      ;
      sub         edx, J9TR_InterpVTableOffset
      neg         edx                                             ; JIT vtable offset
      LoadClassPointerFromObjectHeader eax, ecx, ecx              ; receiver class
      mov         edi, [ecx+edx]                                  ; target in JIT side vtable
      pop         ecx                                             ; restore
      add         esp, 8                                          ; edx and edi do not need to be restored because they
                                                                  ; are killed by the linkage.
      jmp         edi

interpretedLastIPicSlot:
      lea         edx, [edi-8]                                    ; 8 = 5 (CALL) + 3 (NOP)
      jmp         mergeIPicInterpretedDispatch

ret


; Look up an implemented interface method in this receivers class and
; dispatch it.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
IPicLookupDispatch:
      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache

      lea         edi, [edi+5]                                    ; EA of constant pool and cpIndex
                                                                  ; 5 = 5 (JMP)

      push        eax                                             ; push receiver
      LoadClassPointerFromObjectHeader eax, edx, edx              ; receiver class

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;

      lea         edi, [edi+eq_IPicData_interfaceClass]           ; EA of IClass in data block
                                                                  ; 13 = 5 (JMP) + 8 (cpAddr,cpIndex)

      push        edi                                             ; p) jit EIP
      push        edi                                             ; p) EA of resolved interface class in IPic
      push        edx                                             ; p) receiver class
      CallHelperUseReg jitLookupInterfaceMethod,eax              ; returns interpreter vtable offset
      sub         eax, J9TR_InterpVTableOffset
      neg         eax                                             ; JIT vtable offset
      mov         edi, [edx+eax]                                  ; target in JIT side vtable

      ; j2iVirtual expects edx to contain the JIT vtable offset
      ;
      mov         edx, eax
      pop         eax                                             ; restore receiver
      add         esp, 8                                          ; edx and edi do not need to be restored because they
                                                                  ; are killed by the linkage
      jmp         edi
ret


; Resolve the virtual method and throw an exception if the resolution was not successful.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the resolution helper.
;
      align 16
resolveVPicClass: 

      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache

      cmp         byte  [edi+1], 075h                             ; is it a short branch?
      jnz         resolveVPicClassLongBranch                     ; no

      lea         edi, [edi+9]                                    ; EA of disp8 in JMP
                                                                  ; 9 = 1 + 2 (JNE) + 5 (CALL) + 1 (JMP)
      movzx       edx, byte  [edi]                                ; disp8 to end of VPic
      lea         edx, [edi+edx-11]                               ; edx = EA of disp32 of JNE
                                                                  ; -11 = 1 (instr. after JMP) -12 (EA of disp32 of JNE)
      add         edx, dword  [edx]                               ; EA of vtable dispatch snippet
      lea         edx, [edx-eq_VPicData_size]                     ; EA of VPic data block

mergeFindDataBlockForResolveVPicClass:
      push eax                                                    ; push receiver

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;

      mov         eax, dword [edx+eq_VPicData_directMethod]
      test        eax, eax
      jnz         callDirectMethodVPic

      push        edi                                             ; p) jit valid EIP
      add         edx, eq_VPicData_cpAddr
      push        edx                                             ; p) push the address of the constant pool and cpIndex
      CallHelperUseReg jitResolveVirtualMethod,eax               ; returns compiler vtable index

      test        eax, J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG
      jnz         resolvedToDirectMethodVPic

      push        ebx                                             ; preserve
      push        ecx                                             ; preserve

      ; Stack shape:
      ;
      ; +24 last parameter
      ; +20 RA in code cache (call to helper)
      ; +16 saved edx
      ; +12 saved edi
      ; +8 saved eax (receiver)
      ; +4 saved ebx
      ; +0 saved ecx
      ;

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  eax, resolveVPicClass
      MoveHelper  ebx, populateVPicSlotClass
      mov         edi, dword  [esp+20]                            ; edi = RA in mainline (call to helper)
      mov         edx, dword  [edi-1]                             ; edx = 3 bytes after the call to helper + high byte of disp32
      sub         eax, edi                                        ; Expected disp32 for call to helper
      rol         eax, 8
      mov         dl, al                                          ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                        ; add CALL opcode

      ; Construct the byte sequence in ecx:ebx to re-route the call
      ; to populateVPicSlotClass.
      ;
      sub         ebx, edi                                        ; disp32 to populate snippet
      mov         ecx, edx
      rol         ebx, 8
      mov         cl, bl                                          ; Form high byte of calculated disp32
      mov         bl, 0e8h                                        ; add CALL opcode

      ; Attempt to patch the code.
      ;
      lock cmpxchg8b    [edi-5]                                   ; EA of CMP instruction in mainline

      pop         ecx                                             ; restore
      pop         ebx                                             ; restore
      pop         eax                                             ; restore
      pop         edi                                             ; restore
      pop         edx                                             ; restore

      sub dword [esp], 5                                          ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

resolveVPicClassLongBranch:
      mov edx, dword  [edi+3]                                     ; disp32 for branch to snippet
      lea edx, [edi+edx+7-eq_VPicData_size]                       ; EA of VPic data block
                                                                  ; 7 (offset to instr after branch) -9 (DB+cpIndex+cpAddr)
      jmp mergeFindDataBlockForResolveVPicClass

resolvedToDirectMethodVPic:
      sub         edx, eq_VPicData_cpAddr                         ; restore edx = EA of VPIC data

      ; We'll return to the jump just after the position where the call through
      ; the VFT would be. In the j2i case, the VM assumes control was transferred
      ; via a call though the VFT *unless* RA-5 has 0e8h (direct call relative).
      ; Make sure the VM sees an 0e8h so that it uses the VFT offset register.
      ; Start with a debug trap because the call itself is dead.
      mov         word [edx+eq_VPicData_size], 0e8cch             ; int 3, call

      xor         eax, J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG    ; eax is the J9Method to be directly invoked
      mov         dword [edx+eq_VPicData_directMethod], eax

callDirectMethodVPic:
      lea         edi, [edx+eq_VPicData_size+6]                   ; Adjusted return address
                                                                  ;    6 (offset to jump after call through VFT)
      jmp         dispatchDirectMethod

ret


; Populate a PIC slot with the class of the receiver.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateVPicSlotClass:

      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache

      cmp         byte  [edi+1], 075h                             ; is it a short branch?
      jnz         populateVPicClassLongBranch                     ; no

      lea         edi, [edi+9]                                    ; EA of disp8 in JMP
                                                                  ; 9 = 1 + 2 (JNE) + 5 (CALL) + 1 (JMP)
      movzx       edx, byte  [edi]                                ; disp8 to end of VPic
      lea         edx, [edi+edx-11]                               ; edx = EA of disp32 of JNE
                                                                  ; -11 = 1 (instr. after JMP) -12 (EA of disp32 of JNE)
      add         edx, dword  [edx]                               ; EA of vtable dispatch snippet
      lea         edx, [edx+4-eq_VPicData_size]                   ; EA of VPic data block
                                                                  ; 4 (offset to instruction after JNE)
mergePopulateVPicClass:
      push        eax                                             ; push receiver
      push        ebx                                             ; preserve
      push        ecx                                             ; preserve
      push        esi                                             ; preserve

      ; Stack shape:
      ;
      ; +28 last parameter
      ; +24 RA in code cache (call to helper)
      ; +20 saved edx
      ; +16 saved edi
      ; +12 saved eax (receiver)
      ; +8 saved ebx
      ; +4 saved ecx
      ; +0 saved esi
      ;

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      mov         esi, edx                                        ; esi = EA of VPic data block

      MoveHelper eax, populateVPicSlotClass
      mov         edi, dword [esp+24]                             ; edi = RA in mainline (call to helper)
      mov         edx, dword [edi-1]                              ; edx = 3 bytes after the call to helper + high byte of disp32
      sub         eax, edi                                        ; Expected disp32 for call to snippet
      rol         eax, 8
      mov         dl, al                                          ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                        ; add CALL opcode

      ; Construct the CMP instruction in ecx:ebx to devirtualize the virtual
      ; method for this receivers class.
      ;
      mov         cx, word  [edi+1]                               ; 2 bytes of jump to next slot
      rol         ecx, 16
      mov         ebx, dword  [esp+12]                            ; receiver
      LoadClassPointerFromObjectHeader ebx, ebx, ebx              ; receiver class
      rol         ebx, 16
      mov         cx, bx
      mov         bl, 081h                                        ; CMP opcode
      mov         bh, byte [esi+eq_VPicData_cmpRegImm4ModRM]      ; ModRM byte in VPic data area

      ; Attempt to patch in the CMP instruction.
      ;
      lock        cmpxchg8b [edi-5]                               ; EA of CMP instruction in mainline


%ifdef ASM_J9VM_GC_DYNAMIC_CLASS_UNLOADING
      jnz         short VPicClassSlotUpdateFailed

      ; Register the address of the immediate field in the CMP instruction.
      ;
      mov         eax, dword  [esp+12]                            ; grab the receiver before its restored
      LoadClassPointerFromObjectHeader eax, edx, edx              ; receiver class

      lea         edi, [edi-3]                                    ; EA of immediate
      push        edi                                             ; p) address to patch
      push        edx                                             ; p) receivers class
      CallHelper  jitCallJitAddPicToPatchOnClassUnload
VPicClassSlotUpdateFailed:
%endif

      pop         esi                                             ; restore
      pop         ecx                                             ; restore
      pop         ebx                                             ; restore
      pop         eax                                             ; restore receiver
      pop         edi                                             ; restore
      pop         edx                                             ; restore
      sub         dword [esp], 5                                  ; fix RA to re-run the CMP
      ret                                                         ; branch will mispredict so single-byte RET is ok

populateVPicClassLongBranch:
      mov         edx, dword  [edi+3]                             ; disp32 for branch to snippet
      lea         edx, [edi+edx+7-eq_VPicData_size]               ; EA of VPic data block
                                                                  ;    7 (offset to instr after branch)
      jmp mergePopulateVPicClass
ret


; Populate a PIC slot with the devirtualized method from this receivers class.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateVPicSlotCall:

      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache

      cmp         byte [edi-13], 085h                             ; is this the last slot?
                                                                  ; The branch in the last slot will be a long branch.
                                                                  ; This byte will either be a long JNE opcode (last
                                                                  ; slot) or a CMP opcode.
                                                                  ; -13 = -5 (CALL) -3 (NOP) -5 (CMP or JNE)
      jz          populateLastVPicSlot
      movzx       edx, byte [edi+1]                               ; JMP displacement to end of VPic
      lea         edx, [edi+edx-6]                                ; edx = EA of NOP instruction after JNE in last slot
                                                                  ; -6 = +2 (skip JMP disp8) - 5 (call in last slot) - 3 (NOP)
mergeVPicSlotCall:
      add         edx, dword  [edx-4]                             ; EA of vtable dispatch snippet
      lea         edx, [edx-eq_VPicData_size]                     ; EA of VPic data block
                                                                  ; -9 = (DB,cpIndex,cpAddr)
      push        eax                                             ; push receiver

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;
      push        edi                                             ; p) jit valid EIP
      add         edx, eq_VPicData_cpAddr
      push        edx                                             ; p) push the address of the constant pool and cpIndex
      CallHelperUseReg jitResolveVirtualMethod,eax               ; returns compiler vtable index

      mov         edx, dword [esp]                                ; edx = receiver

      ; Stack shape:
      ;
      ; +12 last parameter
      ; +8 RA in code cache (call to helper)
      ; +4 saved edx
      ; +0 saved edi
      ;
      LoadClassPointerFromObjectHeader edx, edx, edx              ; receiver class

      ; Check the RAM method to see if the target is compiled.
      ;
      neg         eax                                                               ; negate to turn it into an interpreter offset
      mov         eax, dword [edx + eax + J9TR_InterpVTableOffset]                  ; eax = RAM method from interpreter vtable
      test        byte  [eax + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit ; method compiled?
      jnz         short VPicSlotMethodInterpreted

mergeUpdateVPicSlotCallWithCompiledMethod:
      mov         edx, dword  [eax+J9TR_MethodPCStartOffset]      ; edx = compiled method entry point

mergeUpdateVPicSlotCall:
      pop         eax                                             ; restore receiver
      sub         edx, edi                                        ; compute new disp32
      mov         dword [edi-4], edx                              ; patch disp32 in call instruction
      lea         edi, [edi-5]                                    ; set up RA to re-run patched call instruction
      mov         dword [esp+8], edi                              ; re-run the call instruction
      pop         edi
      pop         edx
      ret

populateLastVPicSlot:
      lea         edx, [edi-8]                                    ; 8 = 5 (CALL) + 3 (NOP)
      jmp         mergeVPicSlotCall

VPicSlotMethodInterpreted:
      MoveHelper  edx, dispatchInterpretedFromVPicSlot
      jmp         short mergeUpdateVPicSlotCall

ret


; Dispatch an interpreted devirtualized target. If the target is compiled then patch the PIC slot
; with the compiled target and dispatch through that. Otherwise, route control through the vtable
; dispatch path.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
dispatchInterpretedFromVPicSlot:

      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache

      cmp         byte  [edi-13], 085h                            ; is this the last slot?
                                                                  ; The branch in the last slot will be a long branch.
                                                                  ; This byte will either be a long JNE opcode (last
                                                                  ; slot) or a CMP opcode.
                                                                  ; -13 = -5 (CALL) -3 (NOP) -5 (CMP or JNE)
      jz          interpretedLastVPicSlot
      movzx       edx, byte  [edi+1]                              ; JMP displacement to end of VPic
      lea         edx, [edi+edx-6]                                ; edx = EA of NOP instruction after JNE in last slot
                                                                  ; -6 = +2 (skip JMP disp8) - 5 (call in last slot) - 3 (NOP)
mergeVPicInterpretedDispatch:
      add         edx, dword  [edx-4]                             ; edx = EA of vtable dispatch snippet
      push        eax                                             ; push receiver
      LoadClassPointerFromObjectHeader eax, edi, edi              ; receiver class

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;

      ; If the call through vtable has already been patched then grab the
      ; compiler vtable offset from the disp32 of that instruction.
      ;
      cmp         byte  [edx], 0e8h                               ; still a direct CALL instruction?
      je          vtableCallNotPatched                            ; yes
      mov         eax, dword [edx+2]                              ; eax = compiler vtable offset

mergeCheckIfMethodCompiled:

      ; Check the RAM method to see if the target is compiled.
      ;
      neg         eax                                                               ; negate to turn it into an interpreter offset
      mov         eax, dword [edi + eax + J9TR_InterpVTableOffset]                  ; eax = RAM method from interpreter vtable
      test        byte  [eax + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit ; method compiled?

      mov         edi, dword [esp+12]                             ; restore edi for merge branch
      jz          mergeUpdateVPicSlotCallWithCompiledMethod

      ; Re-route the caller to dispatch through the vtable code in the snippet.
      ;
      mov         dword  [esp+12], edx                            ; redirect return to vtable dispatch snippet
      pop         eax                                             ; restore receiver
      pop         edi                                             ; restore
      pop         edx                                             ; restore
      ret

interpretedLastVPicSlot:
      lea         edx, [edi-8]                                    ; 8 = 5 (CALL) + 3 (NOP)
      jmp         mergeVPicInterpretedDispatch

vtableCallNotPatched:
      lea         edx, [edx-eq_VPicData_size+eq_VPicData_cpAddr]  ; edx = EA of VPic data cpAddr and cpIndex
      push        dword  [esp+12]                                 ; p) jit valid EIP
      push        edx                                             ; p) push the address of the constant pool and cpIndex
      CallHelperUseReg jitResolveVirtualMethod,eax               ; eax = compiler vtable index
      lea         edx, [edx+eq_VPicData_size-eq_VPicData_cpAddr] ; restore edx = EA of vtable dispatch
      jmp         short mergeCheckIfMethodCompiled

ret


; Patch the code to dispatch through the compiled vtable slot now that the vtable
; slot is known.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the resolution helper.
;
      align 16
populateVPicVTableDispatch:

      push        edx                                             ; preserve
      push        edi                                             ; preserve
      mov         edi, dword  [esp+8]                             ; RA in code cache
      lea         edx, [edi-5-eq_VPicData_size+eq_VPicData_cpAddr]; EA of VPic data block cpAddr and cpIndex
                                                                  ; -5 (CALL)
      push        eax                                             ; push receiver

      ; Stack shape:
      ;
      ; +16 last parameter
      ; +12 RA in code cache (call to helper)
      ; +8 saved edx
      ; +4 saved edi
      ; +0 saved eax (receiver)
      ;
      push        edi                                             ; p) jit valid EIP
      push        edx                                             ; p) push the address of the constant pool and cpIndex
      CallHelperUseReg  jitResolveVirtualMethod,eax              ; returns compiler vtable index

      push        ebx                                             ; preserve
      push        ecx                                             ; preserve
      push        esi                                             ; preserve

      ; Stack shape:
      ;
      ; +28 last parameter
      ; +24 RA in code cache (call to helper)
      ; +20 saved edx
      ; +16 saved edi
      ; +12 saved eax (receiver)
      ; +8 saved ebx
      ; +4 saved ecx
      ; +0 saved esi
      ;

      mov         ecx, eax                                        ; ecx = compiler vtable index

      ; An atomic CMPXCHG8B is not strictly necessary to patch this instruction.
      ; However, it simplifies the code to have only a single path through here rather
      ; than several based on the atomic 64-bit store capabilities on various
      ; supported processors.
      ;

      ; Construct the call instruction in edx:eax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  eax, populateVPicVTableDispatch
      mov         edx, dword  [edi-1]                             ; edx = 3 bytes after the call to helper + high byte of disp32
      mov         esi, edx                                        ; copy to preserve 3 bytes
      sub         eax, edi                                        ; Expected disp32 for call to helper
      rol         eax, 8
      mov         dl, al                                          ; Copy high byte of calculated disp32 to expected word
      mov         al, 0e8h                                        ; add CALL opcode

      ; Construct the byte sequence in ecx:ebx to dispatch through vtable.
      ;
      rol         ecx, 16
      mov         ebx, ecx
      and         esi, 0ffff0000h                                 ; mask off 2 bytes following CMP instruction
      and         ecx, 0ffffh                                     ; mask off high 2 bytes of vtable disp32
      or          ecx, esi

      and         ebx, 0ffff0000h                                 ; mask off low 2 bytes of vtable disp32
      mov         bh, byte [edi-5-eq_VPicData_size+eq_VPicData_cmpRegImm4ModRM]
      sub         bh, 068h                                        ; convert to ModRM for CALL instruction
      mov         bl, 0ffh                                        ; add CALL opcode

      lea         edi, [edi-5]

      ; Attempt to patch the code.
      ;

      lock        cmpxchg8b [edi]                                 ; EA of vtable dispatch

      mov         dword  [esp+24], edi                            ; fix RA to run the vtable call
      pop         esi                                             ; restore
      pop         ecx                                             ; restore
      pop         ebx                                             ; restore
      pop         eax                                             ; restore receiver
      pop         edi                                             ; restore
      pop         edx                                             ; restore
ret                                                               ; branch will mispredict so single-byte RET is ok

%else

; --------------------------------------------------------------------------------
;
;                                    64-BIT
;
; --------------------------------------------------------------------------------

      %include "jilconsts.inc"
      %include "X86PicBuilder.inc"

segment .text

      DECLARE_GLOBAL resolveIPicClass
      DECLARE_GLOBAL populateIPicSlotClass
      DECLARE_GLOBAL populateIPicSlotCall
      DECLARE_GLOBAL dispatchInterpretedFromIPicSlot
      DECLARE_GLOBAL IPicLookupDispatch

      DECLARE_GLOBAL resolveVPicClass
      DECLARE_GLOBAL populateVPicSlotClass
      DECLARE_GLOBAL populateVPicSlotCall
      DECLARE_GLOBAL dispatchInterpretedFromVPicSlot
      DECLARE_GLOBAL populateVPicVTableDispatch

      DECLARE_EXTERN jitResolveInterfaceMethod
      DECLARE_EXTERN jitLookupInterfaceMethod
      DECLARE_EXTERN jitResolveVirtualMethod
      DECLARE_EXTERN jitCallCFunction
      DECLARE_EXTERN jitCallJitAddPicToPatchOnClassUnload
      DECLARE_EXTERN jitMethodIsNative
      DECLARE_EXTERN jitInstanceOf

      DECLARE_EXTERN resolveIPicClassHelperIndex
      DECLARE_EXTERN populateIPicSlotClassHelperIndex
      DECLARE_EXTERN dispatchInterpretedFromIPicSlotHelperIndex
      DECLARE_EXTERN resolveVPicClassHelperIndex
      DECLARE_EXTERN populateVPicSlotClassHelperIndex
      DECLARE_EXTERN dispatchInterpretedFromVPicSlotHelperIndex
      DECLARE_EXTERN populateVPicVTableDispatchHelperIndex

      DECLARE_EXTERN mcc_lookupHelperTrampoline_unwrapper
      DECLARE_EXTERN mcc_reservationInterfaceCache_unwrapper
      DECLARE_EXTERN mcc_callPointPatching_unwrapper



; Given a code cache address following a branch to a helper, return the disp32
; to either the helper itself or its reachable trampoline.
;
; p1 [rax] : helper address
; p2 [rsi] : helper index
; p3 [rdx] : RA in code cache after branch to helper
;
; All registers preserved except rax.
;
      align 16
selectHelperOrTrampolineDisp32:
      push        rcx                                 ; preserve
      push        rdx                                 ; preserve
      push        rsi                                 ; preserve

      ; Determine if the call to the helper is within range.
      ;
      mov         rcx, rax
      sub         rcx, rdx
      sar         rcx, 31
      jz          helperIsReachable
      cmp         rcx, -1
      jz          helperIsReachable

      ; Helper is not in range--find the trampoline for this helper.
      ;
      ; Create and populate TLS area for trampoline lookup call.
      ;
      ; rsp+32 return value
      ; rsp+24 padding
      ; rsp+16 padding
      ; rsp+8 helper index
      ; rsp call site

      mov         rcx, rdx                                        ; preserve RA in code cache
      sub         rsp, 24                                         ; 24 = 8 (RV) + 16 (padding)
      push        rsi                                             ; helper index
      push        rdx                                             ; RA after call site

      ; Call MCC service
      ;

      MoveHelper  rax, mcc_lookupHelperTrampoline_unwrapper      ; p1) helper address
      lea         rsi, [rsp]                                      ; p2) EA parms in TLS area
      lea         rdx, [rsp+32]                                   ; p3) EA of return value in TLS area
      call        jitCallCFunction
      add         rsp, 32
      pop         rax                                             ; trampoline address from return area
      mov         rdx, rcx                                        ; restore RA in code cache

helperIsReachable:
      sub         rax, rdx                                        ; calculate disp32
      mov         eax, eax                                        ; ensure upper 32-bits are zero
      pop         rsi                                             ; restore
      pop         rdx                                             ; restore
      pop         rcx                                             ; restore
ret

%if 0
(10) mov rdi, 0x0000ffffffffffff call resolveIPicClass -> populateIPicSlotClass
(3) cmp rdx, rdi
(2) jne checkNextSlot
(5) call devirtualizedTarget call populateIPicSlotCall
(2) jmp done

(2) .align 8

         .align 8
checkLastSlot:
(10) mov rdi, 0x0000ffffffffffff call populateIPicSlotClass
(3) cmp rdx, rdi
(6) jne lookupDispatchSnippet
(5) call devirtualizedTarget call populateIPicSlotCall

      0: ret addr to picbuilder
      1: arg3
      2: arg2
      3: arg1
      4: arg0
      5: saved RDI <==== unwindSP points here
      6: code cache return address
      7: last arg <==== unwindSP should point here
%endif

; Do direct dispatch given the J9Method (for both VPIC & IPIC)
;
; Registers on entry:
;    rax: direct J9Method pointer
;    rdi: adjusted return address
;    rdx: pointer to j2i virtual thunk pointer within PIC data
;
; Stack shape on entry:
;    +40 RA in code cache (call to helper)
;    +32 saved rdi
;    +24 saved rax (receiver)
;    +16 saved rsi
;    +8  saved rdx
;    +0  saved rcx
;
      align 16
dispatchDirectMethod:
      mov         qword [rsp+40], rdi                             ; Replace RA on stack

      test        qword [rax+J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit   ; method compiled?
      jnz         dispatchDirectMethodInterpreted

      ; Method is compiled
      mov         rax, qword [rax+J9TR_MethodPCStartOffset]       ; interpreter entry point for compiled method
      mov         edi, dword [rax-4]                              ; preprologue info word
      shr         edi, 16                                         ; offset to JIT entry point
      add         rdi, rax                                        ; JIT entry point
      jmp         mergeDispatchDirectMethod

dispatchDirectMethodInterpreted:
      lea         r8, [rax+J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG]     ; low-tagged J9Method
      mov         rdi, qword [rdx]                                      ; j2i virtual thunk

mergeDispatchDirectMethod:
      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      add         rsp, 8                                          ; skip rdi
      jmp         rdi

ret


; Resolve the interface method and update the IPIC data area with the interface J9Class
; and the itable index.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the resolution helper.
;
         align 16
resolveIPicClass:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3
      mov         rdi, qword  [rsp+40]                            ; RA in code cache

      cmp         byte  [rdi+8], 075h                             ; is it a short branch?
      jnz         resolveIPicClassLongBranch                      ; no
      movzx       rsi, byte  [rdi+16]                             ; disp8 to end of IPic
                                                                  ; 16 = 5 + 3 (CMP) + 2 (JNE) + 5 (CALL) + 1 (JMP)
      lea         rdx, [rdi+rsi+8]                                ; rdx = EA of disp32 of JNE
                                                                  ; 8 = 1 (instr. after JMP) -9 (EA of disp32 of JNE) + 16 (offset to disp8 EA)
      movsxd      rsi, dword  [rdx]
      lea         rdx, [rsi+rdx+14]                               ; EA of IPic data block
                                                                  ; 14 = +4 (EA after JNE) + 5 (CALL) + 5 (JMP)
mergeResolveIPicClass:

      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;
      test        qword [rdx+eq_IPicData_itableOffset], J9TR_J9_ITABLE_OFFSET_DIRECT
      jz          callResolveInterfaceMethod

      cmp         qword [rdx+eq_IPicData_interfaceClass], 0
      jne         typeCheckAndDirectDispatchIPic

callResolveInterfaceMethod:
      mov         rsi, rdi                                        ; p2) jit valid EIP
      mov         rax, rdx                                        ; p1) address of the constant pool and cpIndex
      CallHelperUseReg  jitResolveInterfaceMethod,rax            ; returns interpreter vtable index

      mfence                                                      ; Ensure IPIC data area drains to memory before proceeding.

      test        qword [rdx+eq_IPicData_itableOffset], J9TR_J9_ITABLE_OFFSET_DIRECT
      jnz         typeCheckAndDirectDispatchIPic


      ; Construct the call instruction in rax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  rax, populateIPicSlotClass                     ; p1) rax = helper address
      LoadHelperIndex   esi, populateIPicSlotClassHelperIndex    ; p2) rsi = helper index
      mov         rdx, rdi                                        ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      mov         rcx, rax
      shl         rcx, 8
      mov         cl, 0e8h

      MoveHelper  rax, resolveIPicClass                          ; p1) rax = helper address
      LoadHelperIndex   esi, resolveIPicClassHelperIndex         ; p2) rsi = helper index
                                                                  ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      shl         rax, 8
      mov         al, 0e8h

      lock cmpxchg qword  [rdx-5], rcx                            ; patch attempt

      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      pop         rdi                                             ; restore

      sub         qword [rsp], 5                                  ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

resolveIPicClassLongBranch:
      movsxd      rdx, dword  [rdi+10]                            ; disp32 for branch to snippet
                                                                  ; 10 = 5 + 3 (CMP) + 2 (JNE)
      lea         rdx, [rdi+rdx+24]                               ; EA of IPic data block
                                                                  ; 24 = 14 (offset to instr after branch) + 5 (CALL) + 5 (JMP)
      jmp         mergeResolveIPicClass

typeCheckAndDirectDispatchIPic:
      mov         rax, qword [rdx+eq_IPicData_interfaceClass]     ; p1) interface class
      mov         rsi, qword [rsp+24]                             ; p2) receiver (saved rax)
      CallHelperUseReg jitInstanceOf, rax
      test        rax, rax
      jz          throwOnFailedTypeCheckIPic

      ; Type check passed
      mov         rax, qword [rdx+eq_IPicData_itableOffset]
      xor         rax, J9TR_J9_ITABLE_OFFSET_DIRECT               ; Direct J9Method pointer

      lea         rdi, [rdx-5]                                    ; Adjusted return address
                                                                  ;    5 (JMP)
      add         rdx, eq_IPicData_j2iThunk
      jmp         dispatchDirectMethod

throwOnFailedTypeCheckIPic:
      LoadClassPointerFromObjectHeader rsi, rax, eax              ; p1) rax = receiver class
      lea         rsi, [rdx+eq_IPicData_interfaceClass]           ; p2) rsi = resolved interface class EA
      mov         rcx, rdx                                        ; p4) rcx = address of constant pool and cpIndex
      mov         rdx, qword [rsp+40]                             ; p3) rdx = jit RIP (saved RA in code cache)
      CallHelperUseReg jitLookupInterfaceMethod, rax             ; guaranteed to throw
      int         3

ret

; Look up a resolved interface method and attempt to occupy the PIC slot
; that routed control to this helper.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateIPicSlotClass:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3

      mov         rdx, qword [rsp+40]                             ; RA in code cache

      cmp         byte  [rdx+8], 075h                             ; is it a short branch?
      jnz         populateIPicClassLongBranch                     ; no
      movzx       rcx, byte  [rdx+16]                             ; disp8 to end of IPic
                                                                  ; 16 = 5 + 3 (CMP) + 2 (JNE) + 5 (CALL) + 1 (JMP)
      lea         rsi, [rdx+rcx+8]                                ; rsi = EA of disp32 of JNE
                                                                  ; 8 = 1 (instr. after JMP) -9 (EA of disp32 of JNE) + 16 (offset to disp8 EA)
      movsxd      rdi, dword  [rsi]
      lea         rcx, [rsi+rdi+14]                               ; EA of constant pool and cpIndex
                                                                  ;   14 = 4 + 10 (CALL+JMP)
      lea         rsi, [rsi+rdi+14+eq_IPicData_interfaceClass]    ; EA of interface class in IPic data block
                                                                  ;    14 = 4 + 10 (CALL+JMP)
mergePopulateIPicClass:

      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;
      LoadClassPointerFromObjectHeader rax, rax, eax              ; p1) receiver class
                                                                  ; p2) rsi = resolved interface class EA
                                                                  ; p3) rdx = jit RIP
                                                                  ; p4) rcx = address of constant pool and cpIndex
      CallHelperUseReg jitLookupInterfaceMethod,rax              ; returns interpreter vtable offset

      mov         rdi, rcx                                        ; start of IPIC data

      ; Lookup succeeded--attempt to grab the PIC slot for this receivers class.
      ;
      ; Construct the call instruction in rax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper rax, populateIPicSlotClass                      ; p1) rax = helper address
      LoadHelperIndex esi, populateIPicSlotClassHelperIndex      ; p2) rsi = helper index
                                                                  ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      shl         rax, 8
      mov         al, 0e8h

      ; Construct the MOV instruction in rcx to load the cached class address.
      ;
      mov         rcx, qword [rsp+24]                             ; receiver
      LoadClassPointerFromObjectHeader rcx, rcx, ecx              ; receiver class
      mov         rsi, rcx                                        ; receiver class
      rol         rcx, 16
      mov         cx, word [rdi+eq_IPicData_movabsRexAndOpcode]   ; REX+MOV

      lock cmpxchg qword [rdx-5], rcx                             ; patch attempt

%ifdef ASM_J9VM_GC_DYNAMIC_CLASS_UNLOADING
      jnz         short IPicClassSlotUpdateFailed

      ; Register the address of the immediate field in the MOV instruction
      ; for dynamic class unloading iff the interface class and the
      ; receiver class have been loaded by different class loaders.
      ;
      mov         rax, rsi                                        ; receivers class
      mov         rsi, qword  [rsi+J9TR_J9Class_classLoader]      ; receivers class loader

      mov         rcx, qword  [rdi+eq_IPicData_interfaceClass]    ; rcx == IClass from IPic data area
      cmp         rsi, qword  [rcx+J9TR_J9Class_classLoader]      ; compare class loaders
      jz          short IPicClassSlotUpdateFailed                 ; class loaders are the same--do nothing

                                                                  ; p1) rax = receivers class
      lea         rsi, [rdx-3]                                    ; p2) rsi = EA of Imm64 to patch
      CallHelper  jitCallJitAddPicToPatchOnClassUnload
IPicClassSlotUpdateFailed:
%endif

      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      pop         rdi                                             ; restore

      sub         qword [rsp], 5                                  ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

populateIPicClassLongBranch:
      movsxd      rsi, dword  [rdx+10]                            ; disp32 for branch to snippet
                                                                  ; 10 = 5 + 3 (CMP) + 2 (JNE)
      lea         rcx, [rdx+rsi+24]                               ; EA of constant pool and cpIndex
                                                                  ; 24 = 14 (offset to instr after branch) + 5 (CALL) + 5 (JMP)
      lea         rsi, [rdx+rsi+24+eq_IPicData_interfaceClass]    ; EA of interface class in IPic data block
                                                                  ;    24 = 14 (offset to instr after branch) + 5 (CALL) + 5 (JMP)
      jmp         mergePopulateIPicClass
ret


; Look up a resolved interface method and update the call to the method
; that routed control to this helper.
;
; STACK SHAPE: must maintain stack shape expected by call to
; getJitVirtualMethodResolvePushes() across the call to the lookup helper.
;
         align 16
populateIPicSlotCall:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3

      mov         rdx, qword [rsp+40]                             ; RA in code cache

      cmp         byte  [rdx-10], 085h                            ; is this the last slot?
                                                                  ; The branch in the last slot will be a long branch.
                                                                  ; This byte will either be a long JNE opcode (last
                                                                  ; slot) or the REX prefix of a CMP opcode.
                                                                  ; -10 = -5 (CALL) -5 (CMP or JNE)
      jz          populateLastIPicSlot
      movzx       rdi, byte  [rdx+1]                              ; JMP displacement to end of IPic
      lea         rdi, [rdi+rdx-3]                                ; rdi = EA after JNE in last slot
                                                                  ;  -3 = +2 (skip JMP disp8) - 5 (call in last slot)
mergeIPicSlotCall:
      movsxd      rsi, dword [rdi-4]

      lea         rcx, [rsi+rdi+10]                               ; EA of constant pool and cpIndex
                                                                  ; 10 = 10 (CALL+JMP)

      lea         rsi, [rsi+rdi+10+eq_IPicData_interfaceClass]    ; EA of interface class in IPic data block
                                                                  ; 10 = 10 (CALL+JMP) + 16 (cpAddr,cpIndex)
      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;
      LoadClassPointerFromObjectHeader rax, rax, eax              ; p1) receiver class
      mov         rdi, rax
                                                                  ; p2) rsi = resolved interface class EA
                                                                  ; p3) rdx = jit RIP
                                                                  ; p4) rcx = address of constant pool and cpIndex
      CallHelperUseReg jitLookupInterfaceMethod,rax              ; returns interpreter vtable offset

      mov         rax, qword  [rdi+rax]                           ; rax = J9Method from interpreter vtable
      test        byte  [rax + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit ; method compiled?
      jnz         short IPicSlotMethodInterpreted

      ; We have a compiled method. Claim the trampoline reservation for this
      ; target method if one doesn't exist for it yet reclaim the trampoline
      ; space if it already does.
      ;
mergeUpdateIPicSlotCallWithCompiledMethod:
      push        rax                                             ; build descriptor : J9Method
      push        rdx                                             ; build descriptor : call site
      MoveHelper  rax, mcc_reservationInterfaceCache_unwrapper   ; p1) helper
      lea         rsi, [rsp]                                      ; p2) pointer to args
      lea         rdx, [rsp]                                      ; p3) pointer to return
      call        jitCallCFunction
      pop         rdx                                             ; tear down descriptor
      pop         rax                                             ; tear down descriptor

      mov         rcx, qword  [rax+J9TR_MethodPCStartOffset]      ; compiled method interpreter entry point from J9Method
      mov         edi, dword  [rcx-4]                             ; fetch preprologue info word
      shr         edi, 16                                         ; isolate interpreter argument flush area size
      add         rcx, rdi                                        ; rcx = JIT entry point

      ; Check if a branch through a trampoline is currently required at this callsite.
      ; If not, defer creating one until its actually required at runtime.
      ;
      sub         rcx, rdx
      mov         rdi, rcx
      sar         rdi, 31
      jz          mergeUpdateIPicSlotCall
      cmp         rdi, -1
      jnz         populateIPicSlotCallWithTrampoline

mergeUpdateIPicSlotCall:
      mov         dword  [rdx-4], ecx                             ; patch disp32 in call instruction

populateIPicSlotCallExit:
      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      pop         rdi                                             ; restore

      sub         qword [rsp], 5                                  ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

IPicSlotMethodInterpreted:
      MoveHelper rax, dispatchInterpretedFromIPicSlot            ; p1) rax = helper address
      LoadHelperIndex esi, dispatchInterpretedFromIPicSlotHelperIndex  ; p2) rsi = helper index
                                                                  ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      mov         ecx, eax                                        ; 32-bit copy because disp32
      jmp         short mergeUpdateIPicSlotCall

populateLastIPicSlot:
      lea         rdi, [rdx-5]                                    ; -5 = -5 (CALL)
      jmp         mergeIPicSlotCall

populateIPicSlotCallWithTrampoline:
      push        0                                               ; argsPtr[3] : extra arg
      mov         rsi, qword [rax+J9TR_MethodPCStartOffset]
      push        rsi                                             ; argsPtr[2] : compiled method interpreter entry point
      lea         rsi, [rdx-5]
      push        rsi                                             ; argsPtr[1] : call site (EA of CALL instruction)
      push        rax                                             ; argsPtr[0] : J9Method

      MoveHelper  rax, mcc_callPointPatching_unwrapper           ; p1) rax = helper address
      lea         rsi, [rsp]                                      ; p2) rsi = descriptor EA (args)
      lea         rdx, [rsp]                                      ; p3) rdx = return value (meaningless because call is void)
      call        jitCallCFunction
      add         rsp, 32
      jmp         populateIPicSlotCallExit

ret


; Call an interpreted method from an IPic slot.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
dispatchInterpretedFromIPicSlot:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3

      mov         rdx, qword [rsp+40]                             ; RA in code cache

      cmp         byte  [rdx-10], 085h                            ; is this the last slot?
                                                                  ; The branch in the last slot will be a long branch.
                                                                  ; This byte will either be a long JNE opcode (last
                                                                  ; slot) or the REX prefix of a CMP opcode.
                                                                  ; -10 = -5 (CALL) -5 (CMP or JNE)
      jz          interpretedLastIPicSlot

      movzx       rdi, byte  [rdx+1]                              ; JMP displacement to end of IPic
      lea         rdi, [rdi+rdx-3]                                ; rdi = EA after JNE in last slot
                                                                  ; -3 = +2 (skip JMP disp8) - 5 (call in last slot)

mergeIPicInterpretedDispatch:
      movsxd      rsi, dword  [rdi-4]

      lea         rcx, [rsi+rdi+10]                               ; EA of constant pool and cpIndex
                                                                  ; 10 = 10 (CALL+JMP)

      lea         rsi, [rsi+rdi+26] ; EA of IPic data block
                                                                  ; 26 = 10 (CALL+JMP) + 16 (cpAddr,cpIndex)
      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;
      LoadClassPointerFromObjectHeader rax, rax, eax              ; p1) receiver class
      mov         rdi, rax
                                                                  ; p2) rsi = resolved interface class EA
                                                                  ; p3) rdx = jit RIP
                                                                  ; p4) rcx = address of constant pool and cpIndex
      CallHelperUseReg jitLookupInterfaceMethod,rax              ; returns interpreter vtable offset

      mov         r8, rax
      mov         rax, qword  [rdi+rax]                           ; rax = J9Method from interpreter vtable
      test        byte  [rax + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit ; method compiled?
      jz          mergeUpdateIPicSlotCallWithCompiledMethod

      ; Target method is still interpreted--dispatch it through JIT vtable.
      ;
      jmp         mergeIPicLookupDispatch

interpretedLastIPicSlot:
      lea         rdi, [rdx-5]                                    ; -5 = -5 (CALL)
      jmp         mergeIPicInterpretedDispatch

ret


; Look up an implemented interface method in this receivers class and
; dispatch it.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
IPicLookupDispatch:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3

      mov         rdx, qword [rsp+40]                             ; RA in code cache

      lea         rcx, [rdx+5]                                    ; EA of constant pool and cpIndex
                                                                  ; 5 = 5 (JMP)

      lea         rsi, [rdx+21] ; EA of IPic data block
                                                                  ; 21 = 5 (JMP) + 16 (cpAddr,cpIndex)
      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;
      LoadClassPointerFromObjectHeader rax, rax, eax              ; p1) receiver class
      mov         rdi, rax
                                                                  ; p2) rsi = resolved interface class EA
                                                                  ; p3) rdx = jit RIP
                                                                  ; p4) rcx = address of constant pool and cpIndex
      CallHelperUseReg jitLookupInterfaceMethod,rax              ; returns interpreter vtable offset
      mov         r8, rax

mergeIPicLookupDispatch:
      sub         r8, J9TR_InterpVTableOffset
      neg         r8                                              ; r8 = JIT vtable offset
      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      add         rsp, 8                                          ; skip rdi

      ; j2iVirtual expects r8 to contain the JIT vtable offset
      ;
      mov rdi, qword [rdi + r8]
      jmp rdi

ret


; Resolve the virtual method and throw an exception if the resolution was not successful.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the resolution helper.
;
      align 16
resolveVPicClass:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3
      mov         rdi, qword  [rsp+40]                            ; RA in code cache

      cmp         byte  [rdi+8], 075h                             ; is it a short branch?
      jnz         resolveVPicClassLongBranch                      ; no
      movzx       rsi, byte  [rdi+16]                             ; disp8 to end of VPic
                                                                  ; 16 = 5 (zeros after call) + 3 (CMP) + 2 (JNE) + 5 (CALL) + 1 (JMP)
      lea         rdx, [rdi+rsi+8]                                ; rdx = EA of disp32 of JNE
                                                                  ; 8 = 1 (instr. after JMP) -9 (EA of disp32 of JNE) + 16 (offset to disp8 EA)
      movsxd      rsi, dword  [rdx]
      lea         rdx, [rsi+rdx+4-eq_VPicData_size]               ; EA of VPic data block

mergeResolveVPicClass:

      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;

      mov         rax, qword [rdx+eq_VPicData_directMethod]
      test        rax, rax
      jnz         callDirectMethodVPic

      lea         rax, [rdx+eq_VPicData_cpAddr]                   ; p1) address of the constant pool and cpIndex
      mov         rsi, rdi                                        ; p2) jit valid EIP
      CallHelperUseReg jitResolveVirtualMethod,rax               ; returns compiler vtable index, , or (low-tagged) direct J9Method pointer

      test        rax, J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG
      jnz         resolvedToDirectMethodVPic

      ; Construct the call instruction in rax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  rax, populateVPicSlotClass                     ; p1) rax = helper address
      LoadHelperIndex esi, populateVPicSlotClassHelperIndex      ; p2) rsi = helper index
      mov         rdx, rdi                                        ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      mov         rcx, rax
      shl         rcx, 8
      mov         cl, 0e8h

      MoveHelper rax, resolveVPicClass                           ; p1) rax = helper address
      LoadHelperIndex esi, resolveVPicClassHelperIndex           ; p2) rsi = helper index
                                                                  ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      shl         rax, 8
      mov         al, 0e8h

      lock cmpxchg qword  [rdx-5], rcx                            ; patch attempt

      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      pop         rdi                                             ; restore

      sub         qword [rsp], 5                                  ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

resolveVPicClassLongBranch:
      movsxd      rdx, dword  [rdi+10]                            ; disp32 for branch to snippet
                                                                  ; 10 = 5 + 3 (CMP) + 2 (JNE)
      lea         rdx, [rdi+rdx+14-eq_VPicData_size]              ; EA of VPic data block (+14: offset to instr after branch)

      jmp mergeResolveVPicClass

resolvedToDirectMethodVPic:
      ; We'll return to the jump just after the position where the call through
      ; the VFT would be. In the j2i case, the VM assumes control was transferred
      ; via a call though the VFT *unless* RA-5 has 0e8h (direct call relative).
      ; Make sure the VM sees an 0e8h so that it uses the VFT offset register.
      ; Start with a debug trap because the call itself is dead.
      ;
      ; By writing 0e8h twice in a row, it doesn't matter here whether the vtable
      ; call instruction would have been 7 or 8 bytes.
      mov         dword [rdx+eq_VPicData_size], 0e8e8cccch        ; int 3, int 3, call, call

      xor         rax, J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG    ; rax is the J9Method to be directly invoked
      mov         qword [rdx+eq_VPicData_directMethod], rax

callDirectMethodVPic:
      ; The size of the vtable call instruction is 7 bytes + possibly a SIB byte,
      ; which is needed when ModR/M for the call is 94h.
      cmp         byte [rdx+eq_VPicData_callMemModRM], 94h
      sete        dil                                             ; 1 for SIB byte, or else 0
      movzx       edi, dil
      lea         rdi, [rdx+eq_VPicData_size+7+rdi]               ; Adjusted return address
                                                                  ;    7 (size of vtable call without SIB byte)
      add         rdx, eq_VPicData_j2iThunk
      jmp         dispatchDirectMethod

ret


; Populate a PIC slot with the class of the receiver.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateVPicSlotClass:  ;proc

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3

      mov         rdx, qword [rsp+40]                             ; RA in code cache

      cmp         byte  [rdx+8], 075h                             ; is it a short branch?
      jnz         populateVPicClassLongBranch                     ; no
      movzx       rcx, byte  [rdx+16]                             ; disp8 to end of VPic
                                                                  ; 16 = 5 + 3 (CMP) + 2 (JNE) + 5 (CALL) + 1 (JMP)
      lea         rsi, [rdx+rcx+8]                                ; rsi = EA of disp32 of JNE
                                                                  ; 8 = 1 (instr. after JMP) -9 (EA of disp32 of JNE) + 16 (offset to disp8 EA)
      movsxd      rdi, dword  [rsi]
      lea         rsi, [rsi+rdi+4-eq_VPicData_size]               ; EA of VPic data block (+4 EA after JNE)
mergePopulateVPicClass:

      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;
      mov         rdi, rsi                                        ; EA of VPic data block

      ; Construct the call instruction in rax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  rax, populateVPicSlotClass                     ; p1) rax = helper address
      LoadHelperIndex esi, populateVPicSlotClassHelperIndex      ; p2) rsi = helper index
                                                                  ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      shl         rax, 8
      mov         al, 0e8h

      ; Construct the MOV instruction in rcx to load the cached class address.
      ;
      mov         rcx, qword [rsp+24]                             ; receiver
      LoadClassPointerFromObjectHeader rcx, rcx, ecx              ; receiver class
      mov         rsi, rcx                                        ; receiver class
      rol         rcx, 16
      mov         cx, word [rdi+eq_VPicData_movabsRexAndOpcode]

      lock        cmpxchg qword  [rdx-5], rcx                     ; patch attempt

%ifdef ASM_J9VM_GC_DYNAMIC_CLASS_UNLOADING
      jnz         short VPicClassSlotUpdateFailed

      ; Register the address of the immediate field in the MOV instruction
      ; for dynamic class unloading.
      ;
      mov         rax, rsi                                        ; p1) rax = receivers class
      lea         rsi, [rdx-3]                                    ; p2) rsi = EA of Imm64 to patch
      CallHelper  jitCallJitAddPicToPatchOnClassUnload
VPicClassSlotUpdateFailed:
%endif

      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      pop         rdi                                             ; restore

      sub         qword [rsp], 5                                  ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

populateVPicClassLongBranch:
      movsxd      rsi, dword  [rdx+10]                            ; disp32 for branch to snippet
                                                                  ; 10 = 5 + 3 (CMP) + 2 (JNE)
      lea         rsi, [rdx+rsi+14-eq_VPicData_size]              ; EA of VPic data block (+14: offset to instr after branch)
                                                                  ; -6 = 14 (offset to instr after branch) - 4 (instr data) - 16 (cpA+cpI)
      jmp         mergePopulateVPicClass

ret


; Populate a PIC slot with the devirtualized method from this receivers class.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateVPicSlotCall:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                             ; preserve
      push        rax                                             ; preserve GP arg0 (the receiver)
      push        rsi                                             ; preserve GP arg1
      push        rdx                                             ; preserve GP arg2
      push        rcx                                             ; preserve GP arg3

      mov         rdx, qword [rsp+40]                             ; RA in code cache

      cmp         byte  [rdx-10], 085h                            ; is this the last slot?
                                                                  ; The branch in the last slot will be a long branch.
                                                                  ; This byte will either be a long JNE opcode (last
                                                                  ; slot) or the REX prefix of a CMP opcode.
                                                                  ; -10 = -5 (CALL) -5 (CMP or JNE)
      jz          populateLastVPicSlot
      movzx       rdi, byte  [rdx+1]                              ; JMP displacement to end of VPic
      lea         rdi, [rdi+rdx-3]                                ; rdi = EA after JNE in last slot
                                                                  ; -3 = +2 (skip JMP disp8) - 5 (call in last slot)
mergeVPicSlotCall:
      movsxd rsi, dword  [rdi-4]
      LoadClassPointerFromObjectHeader rax, rcx, ecx
      lea rax, [rsi+rdi-eq_VPicData_size+eq_VPicData_cpAddr]      ; EA of VPic data block cpAddr and cpIndex
                                                                  ; -20 = -4 (instr data) - 16 (cpAddr+cpIndex)
      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;

      ; Get vtable offset again by resolving.
      ;
                                                                  ; p1) rax = address of constant pool and cpIndex
      mov         rsi, rdx                                        ; p2) rsi = jit valid EIP
      CallHelperUseReg jitResolveVirtualMethod,rax               ; returns compiler vtable index

      ; Check the RAM method to see if the target is compiled.
      ;
      neg         rax                                                                     ; negate to turn it into an interpreter offset
      mov         rax, qword [rcx + rax + J9TR_InterpVTableOffset]                        ; load RAM method from interpreter vtable
      test        byte  [rax + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit       ; method compiled?
      jnz         short VPicSlotMethodInterpreted

      ; Now check if the target is a native because they need special handling.
      ;
      mov         rcx, rax                                        ; rcx = RAM method
      CallHelperUseReg jitMethodIsNative,rax
      test        rax, rax
      mov         rax, rcx                                        ; restore rax
      jnz         short VPicSlotMethodIsNative

      ; We have a compiled method. Claim the trampoline reservation for this
      ; target method if one doesn't exist for it yet reclaim the trampoline
      ; space if it already does.
      ;
mergeUpdateVPicSlotCallWithCompiledMethod:
      push        rax                                             ; build descriptor : J9Method
      push        rdx                                             ; build descriptor : call site
      MoveHelper rax, mcc_reservationInterfaceCache_unwrapper    ; p1) helper
      lea         rsi, [rsp]                                      ; p2) pointer to args
      lea         rdx, [rsp]                                      ; p3) pointer to return
      call        jitCallCFunction
      pop         rdx                                             ; tear down descriptor
      pop         rax                                             ; tear down descriptor

      mov         rcx, qword  [rax+J9TR_MethodPCStartOffset]      ; compiled method interpreter entry point from J9Method
      mov         edi, dword  [rcx-4]                             ; fetch preprologue info word
      shr         edi, 16                                         ; isolate interpreter argument flush area size
      add         rcx, rdi                                        ; rcx = JIT entry point

      ; Check if a branch through a trampoline is currently required at this callsite.
      ; If not, defer creating one until its actually required at runtime.
      ;
      sub         rcx, rdx
      mov         rdi, rcx
      sar         rdi, 31
      jz          mergeUpdateVPicSlotCall
      cmp         rdi, -1
      jnz         populateVPicSlotCallWithTrampoline

mergeUpdateVPicSlotCall:
      mov         dword  [rdx-4], ecx                             ; patch disp32 in call instruction

populateVPicSlotCallExit:
      pop         rcx                                             ; restore
      pop         rdx                                             ; restore
      pop         rsi                                             ; restore
      pop         rax                                             ; restore
      pop         rdi                                             ; restore

      sub         qword [rsp], 5                                  ; fix RA to re-run the call
      ret                                                         ; branch will mispredict so single-byte RET is ok

VPicSlotMethodInterpreted:
      MoveHelper        rax, dispatchInterpretedFromVPicSlot                 ; p1) rax = helper address
      LoadHelperIndex   esi, dispatchInterpretedFromVPicSlotHelperIndex      ; p2) rsi = helper index
                                                                              ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      mov         ecx, eax                                                    ; 32-bit copy because disp32
      jmp         short mergeUpdateVPicSlotCall

VPicSlotMethodIsNative:
      ; Check if the target is a compiled JNI native.
      ;
      test        qword  [rax+8], 1                                           ; check low-bit of constant pool for JNI native
      jnz         mergeUpdateVPicSlotCallWithCompiledMethod

      ; This is a JIT callable native for which trampolines are not created.
      ; Check if the target is reachable without and if so then branch to
      ; it directly. Otherwise, the longer dispatch sequence through glue
      ; code needs to be taken.
      ;
      ; TODO: figure out why trampolines are not created. I do not see why they
      ; shouldn't be...
      ;
      mov         rcx, qword  [rax+J9TR_MethodPCStartOffset]                  ; compiled native entry point
      mov         edi, dword  [rcx-4]                                         ; fetch preprologue info word
      shr         edi, 16                                                     ; isolate interpreter argument flush area size
      add         rcx, rdi                                                    ; rcx = JIT entry point

      ; Check if a branch through a trampoline is currently required at this callsite.
      ; If not, defer creating one until its actually required at runtime.
      ;
      sub         rcx, rdx
      mov         rdi, rcx
      sar         rdi, 31
      jz          mergeUpdateVPicSlotCall
      cmp         rdi, -1
      jz          mergeUpdateVPicSlotCall
      jmp         VPicSlotMethodInterpreted

populateLastVPicSlot:
      lea         rdi, [rdx-5]                                                ; -5 = -5 (CALL)
      jmp         mergeVPicSlotCall

populateVPicSlotCallWithTrampoline:
      push        0                                                           ; argsPtr[3] : extra arg
      mov         rsi, qword [rax+J9TR_MethodPCStartOffset]
      push        rsi                                                         ; argsPtr[2] : compiled method interpreter entry point
      lea         rsi, [rdx-5]
      push        rsi                                                         ; argsPtr[1] : call site (EA of CALL instruction)
      push        rax                                                         ; argsPtr[0] : J9Method

      MoveHelper rax, mcc_callPointPatching_unwrapper                        ; p1) rax = helper address
      lea         rsi, [rsp]                                                  ; p2) rsi = descriptor EA (args)
      lea         rdx, [rsp]                                                  ; p3) rdx = return value (meaningless because call is void)
      call        jitCallCFunction
      add         rsp, 32
      jmp         populateVPicSlotCallExit

ret


; Dispatch an interpreted devirtualized target. If the target is compiled then patch the PIC slot
; with the compiled target and dispatch through that. Otherwise, route control through the vtable
; dispatch path.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
dispatchInterpretedFromVPicSlot:  ;proc

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                                         ; preserve
      push        rax                                                         ; preserve GP arg0 (the receiver)
      push        rsi                                                         ; preserve GP arg1
      push        rdx                                                         ; preserve GP arg2
      push        rcx                                                         ; preserve GP arg3

      mov         rdx, qword [rsp+40]                                         ; RA in code cache

      cmp         byte  [rdx-10], 085h                                        ; is this the last slot?
                                                                              ; The branch in the last slot will be a long branch.
                                                                              ; This byte will either be a long JNE opcode (last
                                                                              ; slot) or the REX prefix of a CMP opcode.
                                                                              ; -10 = -5 (CALL) -5 (CMP or JNE)
      jz          interpretedLastVPicSlot
      movzx       rdi, byte  [rdx+1]                                          ; JMP displacement to end of VPic
      lea         rdi, [rdi+rdx-3]                                            ; rdi = EA after JNE in last slot
                                                                              ; -3 = +2 (skip JMP disp8) - 5 (call in last slot)
mergeVPicInterpretedDispatch:
      movsxd      rsi, dword  [rdi-4]
      LoadClassPointerFromObjectHeader rax, rcx, ecx
      lea         rax, [rsi+rdi]                                              ; EA of CALLMem instruction in snippet

      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;

      ; If the call through vtable has already been patched then grab the
      ; compiler vtable offset from the disp32 of that instruction.
      ;
      cmp         byte  [rax], 0e8h                                           ; still a direct CALL instruction?
      je          vtableCallNotPatched                                        ; yes

      ; If theres a SIB byte then the displacement has been shifted by one byte.
      ;
      cmp         byte  [rax+2], 094h                                         ; if ModRM==0x94 then a SIB is necessary for r12+disp32 form
      jne         short noSIB
      lea         rax, [rax+1]
noSIB:
      movsxd      rax, dword  [rax+3]

mergeCheckIfMethodCompiled:

      ; Check the RAM method to see if the target is compiled.
      ;
      neg         rax                                                               ; negate to turn it into an interpreter offset
      mov         rax, qword [rcx + rax + J9TR_InterpVTableOffset]                  ; load RAM method from interpreter vtable
      test        byte  [rax + J9TR_MethodPCStartOffset], J9TR_MethodNotCompiledBit ; method compiled?
      jnz         dispatchThroughVTable                                             ; branch if interpreted

      ; The target is JIT callable, but check if the target is a native because
      ; they may need special handling.
      ;
      mov         rcx, rax                                                          ; rcx = RAM method
      CallHelperUseReg jitMethodIsNative,rax
      test        rax, rax
      mov         rax, rcx                                                          ; restore rax
      jz          mergeUpdateVPicSlotCallWithCompiledMethod                         ; branch if not native

      ; Check if the target is a compiled JNI native.
      ;
      test        qword  [rax+8], 1                                                 ; check low-bit of constant pool for JNI native
      jnz         mergeUpdateVPicSlotCallWithCompiledMethod

      ; This is a JIT callable native for which trampolines are not created.
      ; Check if the target is reachable without and if so then branch to
      ; it directly. Otherwise, the longer dispatch sequence through glue
      ; code needs to be taken.
      ;
      mov         rcx, qword  [rax+J9TR_MethodPCStartOffset]                        ; compiled native entry point
      mov         esi, dword  [rcx-4]                                               ; fetch preprologue info word
      shr         esi, 16                                                           ; isolate interpreter argument flush area size
      add         rcx, rsi                                                          ; rcx = JIT entry point

      ; Check if a branch through a trampoline is currently required at this callsite.
      ; If not, defer creating one until its actually required at runtime.
      ;
      sub         rcx, rdx
      mov         rsi, rcx
      sar         rsi, 31
      jz          mergeUpdateVPicSlotCall
      cmp         rsi, -1
      jz          mergeUpdateVPicSlotCall

dispatchThroughVTable:

      ; Re-route the caller to dispatch through the vtable code in the snippet.
      ;
      movsxd      rsi, dword  [rdi-4]
      lea         rdi, [rsi+rdi]                                                    ; EA of vtable dispatch instruction

      pop         rcx                                                               ; restore
      pop         rdx                                                               ; restore
      pop         rsi                                                               ; restore
      pop         rax                                                               ; restore

      mov         qword [rsp+8], rdi
      pop         rdi                                                               ; restore

      ret                                                                           ; branch will mispredict so single-byte RET is ok

interpretedLastVPicSlot:
      lea         rdi, [rdx-5]                                                      ; -5 = -5 (CALL)
      jmp         mergeVPicInterpretedDispatch

vtableCallNotPatched:
      ; Get vtable offset again by resolving.
      ;
      lea         rax, [rax-eq_VPicData_size+eq_VPicData_cpAddr]                    ; p1) rax = address of constant pool and cpIndex
                                                                                    ; -20 = -4 (instr data) - 16 (cpAddr,cpIndex)
      mov         rsi, rdx                                                          ; p2) rsi = jit valid EIP
      CallHelperUseReg  jitResolveVirtualMethod,rax                                ; returns compiler vtable index
      jmp         mergeCheckIfMethodCompiled

ret


; Patch the code to dispatch through the compiled vtable slot now that the vtable
; slot is known.
;
; STACK SHAPE: must maintain stack shape expected by call to getJitVirtualMethodResolvePushes()
; across the call to the lookup helper.
;
      align 16
populateVPicVTableDispatch:

%ifdef ENABLE_PIC_INT3
      int 3
%endif
      push        rdi                                                               ; preserve
      push        rax                                                               ; preserve GP arg0 (the receiver)
      push        rsi                                                               ; preserve GP arg1
      push        rdx                                                               ; preserve GP arg2
      push        rcx                                                               ; preserve GP arg3

      mov         rdx, qword [rsp+40]                                               ; RA in code cache

      LoadClassPointerFromObjectHeader rax, rcx, ecx
      lea         rax, [rdx-5-eq_VPicData_size]                                     ; EA of VPic data block -5 (CALL)

      ; Stack shape:
      ;
      ; +40 RA in code cache (call to helper)
      ; +32 saved rdi
      ; +24 saved rax (receiver)
      ; +16 saved rsi
      ; +8 saved rdx
      ; +0 saved rcx
      ;

      ; Get vtable offset again by resolving.
      ;
      mov         rdi, rax
      add         rax, eq_VPicData_cpAddr                                           ; p1) rax = address of constant pool and cpIndex
      mov         rsi, rdx                                                          ; p2) rsi = jit valid EIP
      CallHelperUseReg  jitResolveVirtualMethod,rax                                ; returns compiler vtable index

      ; Construct the indirect call instruction in rcx plus the first
      ; byte of the direct long jump.
      ;
      mov         ecx, eax                                                          ; copy and whack the high 32-bits
      rol         rcx, 32

      cmp         byte [rdi+eq_VPicData_callMemModRM], 094h                         ; SIB byte needed?
      je          short callNeedsSIB                                                ; yes

      or          rcx, 0ff00e9h                                                     ; JMP4 + CALLMem opcodes
      mov         ch, byte [rdi+eq_VPicData_callMemRex]                             ; REX from snippet
      ror         rcx, 24
      mov         cl, byte [rdi+eq_VPicData_callMemModRM]                           ; ModRM from snippet
      rol         rcx, 16
      mov         rdi, 0e9000000000000e8h

mergePopulateVPicVTableDispatch:

      ; Construct the call instruction in rax that should have brought
      ; us to this helper + the following 3 bytes.
      ;
      MoveHelper  rax, populateVPicVTableDispatch                                  ; p1) rax = helper address
      LoadHelperIndex esi, populateVPicVTableDispatchHelperIndex                   ; p2) rsi = helper index
                                                                                    ; p3) rdx = JIT RA
      call        selectHelperOrTrampolineDisp32
      shl         rax, 8
      or          rax, rdi                                                          ; add JMP and CALL bytes

      lock cmpxchg qword  [rdx-5], rcx                                              ; patch attempt

      pop         rcx                                                               ; restore
      pop         rdx                                                               ; restore
      pop         rsi                                                               ; restore
      pop         rax                                                               ; restore
      pop         rdi                                                               ; restore

      sub         qword  [rsp], 5                                                   ; re-run the patched call instruction
      ret                                                                           ; branch will mispredict so single-byte RET is ok

callNeedsSIB:
      or rcx, 02494ff49h                                                            ; REX + CALL op + ModRM + SIB for CALL [r12+disp32]
      mov edi, 0e8h
      jmp short mergePopulateVPicVTableDispatch

ret

%endif
