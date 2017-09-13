; Copyright (c) 2000, 2017 IBM Corp. and others
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
; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

ifndef TR_HOST_64BIT

; --------------------------------------------------------------------------------
;
;                                    32-BIT
;
; --------------------------------------------------------------------------------

      .686p

      option NoOldMacros

      assume cs:flat,ds:flat,ss:flat
      _TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
      _TEXT ends

      _TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

      public         jitGetTarget_CPUID_BufferSize
      public         jitGetTarget_CPUID_RawData

      eq_Fn0000_0000 equ 00000000h
      eq_Fn0000_0001 equ 00000001h
      eq_Fn0000_0002 equ 00000002h
      eq_Fn0000_0004 equ 00000004h

      eq_Fn8000_0000 equ 80000000h
      eq_Fn8000_0001 equ 80000001h
      eq_Fn8000_0002 equ 80000002h
      eq_Fn8000_0003 equ 80000003h
      eq_Fn8000_0004 equ 80000004h
      eq_Fn8000_0005 equ 80000005h
      eq_Fn8000_0006 equ 80000006h
      eq_Fn8000_0008 equ 80000008h
      eq_Fn8000_0019 equ 80000019h
      eq_Fn8000_001A equ 8000001Ah


; Determine the maximum buffer size the CPUID instruction could
; possibly return on the target hardware.
;
; This function uses cdecl linkage on Linux and stdcall
; linkage on Windows.
;
      align 16
jitGetTarget_CPUID_BufferSize proc
      push        ebp
      mov         ebp, esp
      push        esi                                          ; preserve
      push        edi                                          ; preserve
      push        ebx                                          ; preserve

      xor         esi, esi                                     ; esi = tally of buffer size
      mov         eax, eq_Fn0000_0000
      cpuid
      lea         esi, [esi +4]                                ; Fn0000_0000 : eax,ebx,ecx,edx
      mov         edi, eax

      cmp         ebx, 756e6547h                               ; "Genu"
      jne checkForAMDcpuBufferSize
      cmp         edx, 49656e69h                               ; "ineI"
      jne checkForAMDcpuBufferSize
      cmp         ecx, 6c65746eh                               ; "ntel"
      jne checkForAMDcpuBufferSize

      ; Processor is a genuine Intel chip
      ;

      ; Intel function 1 - Feature Information
      ;
      mov         eax, eq_Fn0000_0001
      cmp         edi, eax
      jb          getIntelExtendedFunctionsBufferSize
      lea         esi, [esi +3]                                ; Fn0000_0001 : eax,ecx,edx

      ; Intel function 2 - Cache Descriptors
      ;
      mov         eax, eq_Fn0000_0002
      cmp         edi, eax
      jb          getIntelExtendedFunctionsBufferSize
      cpuid
      shl         eax, 2
      and         eax, 0cffh                                   ; eax = number of dwords collected by Fn0000_0002
      add         esi, eax

      ; Intel function 4 - Deterministic Cache Parameters
      ;
      mov         eax, eq_Fn0000_0004
      cmp         edi, eax
      jb          getIntelExtendedFunctionsBufferSize

      push        edi                                          ; preserve
      xor         edi, edi

Fn4BufferSizeLoop:
      mov         ecx, edi
      cpuid
      test        eax, 01fh                                    ; EAX[4:0] != 0?
      je exitFn4BufferSizeLoop
      add         esi, 4                                       ; Fn0000_0004 : eax,ebx,ecx,edx
      inc         edi
      mov         eax, eq_Fn0000_0004
      jmp         Fn4BufferSizeLoop

exitFn4BufferSizeLoop:
      pop         edi                                          ; restore

getIntelExtendedFunctionsBufferSize:

      mov         eax, eq_Fn8000_0000
      cpuid
      mov         edi, eax
      lea         esi, [esi +1]

      ; Intel extended function Fn8000_0001 - Extended feature bits
      ;
      mov         eax, eq_Fn8000_0001
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +2]                                ; Fn8000_0001 : ecx,edx

      ; Intel extended function Fn8000_000[2-4] - Processor Name / Brand String
      ;
      mov         eax, eq_Fn8000_0004
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +12]                               ; Fn8000_0002 : eax,ebx,ecx,edx
                                                               ; Fn8000_0003 : eax,ebx,ecx,edx
                                                               ; Fn8000_0004 : eax,ebx,ecx,edx

      ; Intel extended function Fn8000_0006 - Extended L2 Cache Features
      ;
      mov         eax, eq_Fn8000_0006
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +1]                                ; Fn8000_0006 : ecx

      ; Intel extended function Fn8000_0008 - Virtual and Physical Address Sizes
      ;
      mov         eax, eq_Fn8000_0008
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +1]                                ; Fn8000_0008 : eax

      jmp         doneBufferSize

checkForAMDcpuBufferSize:
      cmp         ebx, 68747541h                               ; "Auth"
      jne doneBufferSize
      cmp         edx, 69746e65h                               ; "enti"
      jne doneBufferSize
      cmp         ecx, 444d4163h                               ; "cAMD"
      jne doneBufferSize

      ; Processor is an authentic AMD chip
      ;

      ; AMD function 1 - Feature Identifiers
      ;
      mov         eax, eq_Fn0000_0001
      cmp         edi, eax
      jb          getAMDExtendedFunctionsBufferSize
      lea         esi, [esi +4]                                ; Fn0000_0001 : eax,ebx,ecx,edx

getAMDExtendedFunctionsBufferSize:

      mov         eax, eq_Fn8000_0000
      cpuid
      mov         edi, eax

      ; AMD extended function Fn8000_0001 - Extended Features
      ;
      mov         eax, eq_Fn8000_0001
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +3]                                ; Fn8000_0001 : ebx,ecx,edx

      ; AMD extended function Fn8000_000[2-4] - Processor Name / Brand String
      ;
      mov         eax, eq_Fn8000_0004
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +12]                               ; Fn8000_0002 : eax,ebx,ecx,edx
                                                               ; Fn8000_0003 : eax,ebx,ecx,edx
                                                               ; Fn8000_0004 : eax,ebx,ecx,edx

      ; AMD extended function Fn8000_0005 - L1 cache and TLB identifiers
      ;
      mov         eax, eq_Fn8000_0005
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +4]                                ; Fn8000_0005 : eax,ebx,ecx,edx

      ; AMD extended function Fn8000_0006 - L2/L3 Cache and L2 TLB Identifiers
      ;
      mov         eax, eq_Fn8000_0006
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +4]                                ; Fn8000_0006 : eax,ebx,ecx,edx

      ; AMD extended function Fn8000_0008 - Address Size And Physical Core Count Information
      ;
      mov         eax, eq_Fn8000_0008
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +2]                                ; Fn8000_0008 : eax,ecx

      ; AMD extended function Fn8000_0019 - TLB 1GB Identifiers
      ;
      mov         eax, eq_Fn8000_0019
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +2]                                ; Fn8000_0019 : eax,ebx

      ; AMD extended function Fn8000_001A - Performance Optimization Identifiers
      ;
      mov         eax, eq_Fn8000_001A
      cmp         edi, eax
      jb          doneBufferSize
      lea         esi, [esi +1]                                ; Fn8000_001A : eax

doneBufferSize:
      mov         eax, esi
      shl         eax, 2                                       ; number of dwords * 4
      pop         ebx                                          ; restore
      pop         edi                                          ; restore
      pop         esi                                          ; restore
      pop         ebp                                          ; restore
      ret

jitGetTarget_CPUID_BufferSize endp



; Populates a supplied buffer with the raw data returned from
; all supported CPUID functions for the target processor.
;
; jitGetTarget_CPUID_BufferSize should be called before this
; function to determine the minimum buffer size required.
;
; This function uses cdecl linkage on Linux and stdcall
; linkage on Windows.
;
      align 16
jitGetTarget_CPUID_RawData proc
      push        ebp
      mov         ebp, esp
      push        esi                                          ; preserve
      push        edi                                          ; preserve
      push        ebx                                          ; preserve

      mov         esi, dword [esp+20]                          ; buffer cursor

      mov         eax, eq_Fn0000_0000
      cpuid

      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx
      lea         esi, [esi +16]

      mov         edi, eax                                     ; edi = max standard function number

      cmp         ebx, 756e6547h                               ; "Genu"
      jne checkForAMDcpuRawData
      cmp         edx, 49656e69h                               ; "ineI"
      jne checkForAMDcpuRawData
      cmp         ecx, 6c65746eh                               ; "ntel"
      jne checkForAMDcpuRawData

      ; Processor is a genuine Intel chip
      ;

      ; Intel function 1 - Feature Information
      ;
      mov         eax, eq_Fn0000_0001
      cmp         edi, eax
      jb          getIntelExtendedFunctionsRawData

      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ecx
      mov         dword ptr [esi+8], edx
      lea         esi, [esi +12]

      ; Intel function 2 - Cache Descriptors
      ;
getNextIntelCacheDescriptor:
      mov         eax, eq_Fn0000_0002
      cmp         edi, eax
      jb          getIntelExtendedFunctionsRawData
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx
      lea         esi, [esi +16]
      cmp         al, 1
      jne getNextIntelCacheDescriptor

      ; Intel function 4 - Deterministic Cache Parameters
      ;
      mov         eax, eq_Fn0000_0004
      cmp         edi, eax
      jl          getIntelExtendedFunctionsRawData

      push        edi                                          ; preserve
      xor         edi, edi

Fn4BufferLoop:
      mov         ecx, edi
      cpuid
      test        eax, 01fh                                    ; EAX[4:0] != 0?
      je exitFn4BufferLoop
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx
      lea         esi, [esi +16]
      inc         edi
      mov         eax, eq_Fn0000_0004
      jmp         Fn4BufferLoop

exitFn4BufferLoop:
      pop         edi                                          ; restore

getIntelExtendedFunctionsRawData:

      mov         eax, eq_Fn8000_0000
      cpuid
      mov         edi, eax
      mov         dword ptr [esi], eax
      lea         esi, [esi +4]

      ; Intel extended function Fn8000_0001 - Extended feature bits
      ;
      mov         eax, eq_Fn8000_0001
      cmp         edi, eax
      jb          done_CPUID_RawData
      mov         dword ptr [esi], ecx
      mov         dword ptr [esi+4], edx
      lea         esi, [esi +8]

      ; Intel extended function Fn8000_000[2-4] - Processor Name / Brand String
      ;
      mov         eax, eq_Fn8000_0004
      cmp         edi, eax
      jb          done_CPUID_RawData

      cpuid
      mov         dword ptr [esi+32], eax
      mov         dword ptr [esi+36], ebx
      mov         dword ptr [esi+40], ecx
      mov         dword ptr [esi+44], edx

      mov         eax, eq_Fn8000_0002
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx

      mov         eax, eq_Fn8000_0003
      cpuid
      mov         dword ptr [esi+16], eax
      mov         dword ptr [esi+20], ebx
      mov         dword ptr [esi+24], ecx
      mov         dword ptr [esi+28], edx

      lea         esi, [esi +48]

      ; Intel extended function Fn8000_0006 - Extended L2 Cache Features
      ;
      mov         eax, eq_Fn8000_0006
      cmp         edi, eax
      jb          done_CPUID_RawData
      mov         dword ptr [esi], ecx
      lea         esi, [esi +4]

      ; Intel extended function Fn8000_0008 - Virtual and Physical Address Sizes
      ;
      mov         eax, eq_Fn8000_0008
      cmp         edi, eax
      jb          done_CPUID_RawData
      mov         dword ptr [esi], ecx

      jmp         done_CPUID_RawData

checkForAMDcpuRawData:

      cmp         ebx, 68747541h                               ; "Auth"
      jne done_CPUID_RawData
      cmp         edx, 69746e65h                               ; "enti"
      jne done_CPUID_RawData
      cmp         ecx, 444d4163h                               ; "cAMD"
      jne done_CPUID_RawData

      ; Processor is an authentic AMD chip
      ;

      ; AMD function 1 - Feature Identifiers
      ;
      mov         eax, eq_Fn0000_0001
      cmp         edi, eax
      jb          getAMDExtendedFunctionsRawData

      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx
      lea         esi, [esi +16]

getAMDExtendedFunctionsRawData:

      mov         eax, eq_Fn8000_0000
      cpuid
      mov         edi, eax
      mov         dword ptr [esi], eax
      lea         esi, [esi +4]

      ; AMD extended function Fn8000_0001 - Extended features
      ;
      mov         eax, eq_Fn8000_0001
      cmp         edi, eax
      jb          done_CPUID_RawData
      lea         esi, [esi +3]

      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ecx
      mov         dword ptr [esi+8], edx
      lea         esi, [esi +12]

      ; AMD extended function Fn8000_000[2-4] - Processor Name / Brand String
      ;
      mov         eax, eq_Fn8000_0004
      cmp         edi, eax
      jb          done_CPUID_RawData

      cpuid
      mov         dword ptr [esi+32], eax
      mov         dword ptr [esi+36], ebx
      mov         dword ptr [esi+40], ecx
      mov         dword ptr [esi+44], edx

      mov         eax, eq_Fn8000_0002
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx

      mov         eax, eq_Fn8000_0003
      cpuid
      mov         dword ptr [esi+16], eax
      mov         dword ptr [esi+20], ebx
      mov         dword ptr [esi+24], ecx
      mov         dword ptr [esi+28], edx

      lea         esi, [esi +48]

      ; AMD extended function Fn8000_0005 - L1 cache and TLB identifiers
      ;
      mov         eax, eq_Fn8000_0005
      cmp         edi, eax
      jb          done_CPUID_RawData
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx
      lea         esi, [esi +16]

      ; AMD extended function Fn8000_0006 - L2/L3 Cache and L2 TLB Identifiers
      ;
      mov         eax, eq_Fn8000_0006
      cmp         edi, eax
      jb          done_CPUID_RawData
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      mov         dword ptr [esi+8], ecx
      mov         dword ptr [esi+12], edx
      lea         esi, [esi +16]

      ; AMD extended function Fn8000_0008 - Address Size And Physical Core Count Information
      ;
      mov         eax, eq_Fn8000_0008
      cmp         edi, eax
      jb          done_CPUID_RawData
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ecx
      lea         esi, [esi +8]

      ; AMD extended function Fn8000_0019 - TLB 1GB Identifiers
      ;
      mov         eax, eq_Fn8000_0019
      cmp         edi, eax
      jb          done_CPUID_RawData
      cpuid
      mov         dword ptr [esi], eax
      mov         dword ptr [esi+4], ebx
      lea         esi, [esi +8]

      ; AMD extended function Fn8000_001A - Performance Optimization Identifiers
      ;
      mov         eax, eq_Fn8000_001A
      cmp         edi, eax
      jb          done_CPUID_RawData
      mov         dword ptr [esi], eax
      lea         esi, [esi +4]

done_CPUID_RawData:
      pop         ebx                                          ; restore
      pop         edi                                          ; restore
      pop         esi                                          ; restore
      pop         ebp                                          ; restore

ifdef LINUX
      ret
else
      ret         4
endif

jitGetTarget_CPUID_RawData endp

	_TEXT	ENDS
endif

      end
