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


; --------------------------------------------------------------------------------
;                                    COMMON
; --------------------------------------------------------------------------------

eq_WideCPIndexPush                equ 000800000h
eq_ResolveWithoutPatchingMainline equ 000400000h
eq_ResolveWithoutPatchingSnippet  equ 000200000h
eq_ResolveStatic                  equ 020000000h
eq_Patch8ByteResolution           equ 040000000h
eq_CPIndexMask                    equ 00001ffffh
eq_VolatilityCheck                equ 000080000h
eq_UpperLongCheck                 equ 000020000h
eq_LowerLongCheck                 equ 000040000h
eq_MemFenceRAOffset32             equ 09ch
eq_MemFenceCallLength32           equ 005h
eq_MemFenceLCXHG                  equ 0ff0h
eq_HighWordOfLongPair             equ 040000000h
eq_ExtremeStaticMemBarPos         equ 080000000h
eq_LORBinaryWord                  equ 083f0h
eq_IsFloatOp                      equ 000040000h
eq_CompressedPointer              equ 040000000h
eq_isOwningObjectNeeded           equ 000040000h
eq_ObjectClassMask                equ -J9TR_RequiredClassAlignment

; --------------------------------------------------------------------------------
;                                    32-BIT
; --------------------------------------------------------------------------------

%ifndef TR_HOST_64BIT

eq_VPicData_cmpRegImm4ModRM    equ 00h
eq_VPicData_cpAddr             equ 01h
eq_VPicData_cpIndex            equ 05h
eq_VPicData_directMethod       equ 09h
eq_VPicData_size               equ 0dh

eq_IPicData_cpAddr             equ 00h
eq_IPicData_cpIndex            equ 04h
eq_IPicData_interfaceClass     equ 08h
eq_IPicData_itableOffset       equ 0ch
eq_IPicData_cmpRegImm4ModRM    equ 10h
; IPIC data size is omitted here since it would be unused anyway.

J9PreservedFPRStackSize    equ   80

%macro LoadClassPointerFromObjectHeader 3 ;args: ObjectReg, ClassPtrReg64, ClassPtrReg32
        mov     %3, dword [%1+J9TR_J9Object_class]
        and     %3, eq_ObjectClassMask
%endmacro

%else ; ifndef TR_HOST_64BIT

; --------------------------------------------------------------------------------
;                                    64-BIT
; --------------------------------------------------------------------------------

eq_VPicData_movabsRexAndOpcode equ 00h
eq_VPicData_callMemRex         equ 02h
eq_VPicData_callMemModRM       equ 03h
eq_VPicData_cpAddr             equ 04h
eq_VPicData_cpIndex            equ 0ch
eq_VPicData_directMethod       equ 14h
eq_VPicData_j2iThunk           equ 1ch
eq_VPicData_size               equ 24h

eq_IPicData_cpAddr             equ 00h
eq_IPicData_cpIndex            equ 08h
eq_IPicData_interfaceClass     equ 10h
eq_IPicData_itableOffset       equ 18h
eq_IPicData_movabsRexAndOpcode equ 20h
eq_IPicData_j2iThunk           equ 22h    ; only present if direct invoke is possible
; IPIC data size is either 22h or 2ah, due to the optional j2iThunk.
; It's omitted here since it would be unused anyway.

%macro LoadHelperIndex 2 ; args: targetReg, helperIndexSym
        mov     %1, dword [rel + %2]
%endmacro


; Reads the class pointer from an object (the first 4 or 8 bytes)
; ObjectReg and ClassPtrReg may be the same register
; ClassPtrReg32 must be the lower part of ClassPtrReg64
;
%macro LoadClassPointerFromObjectHeader 3 ;ObjectReg, ClassPtrReg64, ClassPtrReg32
%ifdef ASM_OMR_GC_COMPRESSED_POINTERS
        mov     %3, dword [%1 + J9TR_J9Object_class] ; read only 32 bits and zero extend
        and     %3, eq_ObjectClassMask
%else
        mov     %2, qword [%1 + J9TR_J9Object_class]
        and     %2, eq_ObjectClassMask
%endif
%endmacro

%endif ; ifndef TR_HOST_64BIT