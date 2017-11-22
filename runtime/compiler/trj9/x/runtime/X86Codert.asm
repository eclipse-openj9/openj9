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

        include jilconsts.inc
        include x/runtime/X86RegisterMap.inc

ifdef TR_HOST_64BIT
        ; 64-bit
eq_floats_Offset              equ 128
eq_gprs_Offset                equ 0

eq_gpr_size                   equ 8
ifdef ASM_J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   eq_vft_pointer_size        equ 4
else
   eq_vft_pointer_size        equ 8
endif

   OPTION NOSCOPED

   ; Hack marker:
ifdef WINDOWS
   NO_X87_UNDER_WIN64 textequ <true>
endif
else
        ; 32-bit
        .686p
        assume cs:flat,ds:flat,ss:flat
        option NoOldMacros
        .xmm

eq_gpr_size                   equ 4
eq_vft_pointer_size           equ 4

endif


eq_MaxInteger               equ 07fffffffh
eq_MinInteger               equ 080000000h

eq_singleSignMask           equ 080000000h
eq_singleExponentMask       equ 07f800000h
eq_singleMantissaMask       equ 0007fffffh
eq_singleNanMask            equ 07fc00000h

eq_IndefiniteInteger        equ 080000000h
eq_singleDelta              equ 00a800000h
eq_doubleDelta              equ 003200000h
eq_mxcsr_rounding_mask      equ 0ffff9fffh
eq_mxcsr_round_truncate     equ 000006000h

eq_ObjectClassMask          equ -J9TR_RequiredClassAlignment

ifdef TR_HOST_64BIT

eq_MaxLong                    equ 07fffffffffffffffh
eq_MinLong                    equ 08000000000000000h

; doubles are inspected after a left rotation to get the interesting bits into
; the lower half of the register, where we can use CMP, TEST, etc. on them.
eq_doubleLeftRotation         equ 12
eq_doubleSignMaskRotated      equ 0800h
eq_doubleExponentMaskRotated  equ 07ffh
;eq_doubleMantissaMaskRotated  equ 0fffffffffffff000h
eq_doubleMantissaMaskRotated  equ -01000h

else

eq_MaxLongLow               equ 0ffffffffh
eq_MaxLongHigh              equ eq_MaxInteger
eq_MinLongHigh              equ eq_MinInteger

eq_doubleSignMaskHigh       equ 080000000h
eq_doubleExponentMaskHigh   equ 07ff00000h
eq_doubleMantissaMaskHigh   equ 0000fffffh
eq_doubleNanMaskHigh        equ 07ff80000h

endif


ifdef TR_HOST_64BIT
_DATA segment para 'DATA'
else
_DATA segment para use32 public 'DATA'
endif
        align 16
ABSMASK:
   dq 7fffffffffffffffh
   dq 7fffffffffffffffh
QNaN:
   dq 7ff8000000000000h
   dq 7ff8000000000000h
POS_INFINITY:
   dq 7ff0000000000000h
   dq 7ff0000000000000h
INF_NULL_MASK:
   dq 7ff0000000000000h
   dq 0000000000000000h
NULL_INF_MASK:
   dq 0000000000000000h
   dq 7ff0000000000000h
MAGIC_NUM1:
        dq 7fde42d130773b76h
        dq 7fde42d130773b76h
H32_NOSIGN_MASK:
        dq 7fffffff00000000h
        dq 7fffffff00000000h
SCALEUP_NUM:
        dq 4350000000000000h
        dq 4350000000000000h
SCALEDOWN_NUM:
        dq 3c90000000000000h
        dq 3c90000000000000h
ONEHALF:
        dq 3fe0000000000000h
        dq 3fe0000000000000h
_DATA ends

ifdef TR_HOST_64BIT
_TEXT segment para 'CODE'
else
_TEXT segment para use32 public 'CODE'
endif

ifdef TR_HOST_32BIT
        public _doubleToLong
        public _doubleToInt
        public _floatToLong
        public _floatToInt
endif
        public _X87floatRemainder
        public _X87doubleRemainder

        public _SSEfloatRemainder
        public _SSEdoubleRemainder
        public _SSEfloatRemainderIA32Thunk
        public _SSEdoubleRemainderIA32Thunk
ifdef TR_HOST_32BIT
        public _SSEdouble2LongIA32
endif
        public jitFPHelpersBegin
        public jitFPHelpersEnd

        public outlinedPrologue_0preserved
        public outlinedPrologue_1preserved
        public outlinedPrologue_2preserved
        public outlinedPrologue_3preserved
        public outlinedPrologue_4preserved
        public outlinedPrologue_5preserved
        public outlinedPrologue_6preserved
        public outlinedPrologue_7preserved
        public outlinedPrologue_8preserved
        public outlinedEpilogue

    ExternHelper jitStackOverflow

        align 16
jitFPHelpersBegin:


ifdef TR_HOST_64BIT

      align 16
outlinedPrologue_0preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      push        [r8]                                         ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_1preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_2preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_3preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r10
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_4preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r11
      push        r10
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_5preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r12
      push        r11
      push        r10
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_6preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r13
      push        r12
      push        r11
      push        r10
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_7preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r14
      push        r13
      push        r12
      push        r11
      push        r10
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

      align 16
outlinedPrologue_8preserved:
      cmp         r8, [rbp + J9TR_VMThread_stackOverflowMark]
      jbe         outlinedStackOverflow                        ; Overflow check
      xchg        r8, rsp                                      ; Buy the autos
      mov         r8, [r8]                                     ; Save prologue return address
      push        r15
      push        r14
      push        r13
      push        r12
      push        r11
      push        r10
      push        r9
      push        rbx
      push        r8                                           ; Re-push return address
      ret                                                      ; Use ret to get proper return address branch prediction

outlinedStackOverflow:
      lea         rdi, [rsp+8]                                 ; r8's location includes space for the return address; if we bump rdi too, then RA space will be negated when we subtract them
      sub         rdi, r8                                      ; Negative of displacement in LEA instruction
      je          outlinedStackOverflow3                       ; No displacement field at all
      cmp         rdi, 128                                     ; rdi contains the negative of the displacement field, which can accomodate -128
      jle         outlinedStackOverflow2                       ; Check whether 8-bit displacement field suffices
longLEA:
      sub         qword ptr [rsp], 3                           ; Displacement field in the LEA will actually be 4 bytes instead of 1
outlinedStackOverflow2:
      sub         qword ptr [rsp], 2                           ; It's not a mov r8, rsp; it's a lea r8, [rsp+disp8] which is 2 bytes longer
outlinedStackOverflow3:
      sub         qword ptr [rsp], 8                           ; Back up return address to re-execute the mov r8,rsp (3 bytes)
                                                               ;   and call (5 bytes) to outlined prologue after stack overflow check returns
      add         rdi, 64                                      ; Conservatively assume we need 64 bytes for preserved regs
      ; Note r8 doesn't matter at this point, because when we re-execute the prologue, it will get the proper value
      jmp         jitStackOverflow                             ; Will return to "call outlinedPrologue" instruction in prologue

else ; !TR_HOST_64BIT

outlinedPrologue_0preserved:
outlinedPrologue_1preserved:
outlinedPrologue_2preserved:
outlinedPrologue_3preserved:
outlinedPrologue_4preserved:
outlinedPrologue_5preserved:
outlinedPrologue_6preserved:
outlinedPrologue_7preserved:
outlinedPrologue_8preserved:
      int 3 ; Not yet used

endif

outlinedEpilogue proc
      int 3 ; Not yet used
outlinedEpilogue endp



ifdef TR_HOST_32BIT

; _doubleToInt
;
; Convert doubles to ints when it has already been determined that the
; double cannot be represented in an int OR if it appears that the
; double is MIN_INT.
;
; Parameters:
;
;   [esp+4] - double to convert
;
; Returns:
;
;   eax - equivalent integer: either 0, MAX_INT, or MIN_INT
;
        align       16
_doubleToInt proc near
        push        edx
        mov         eax, dword ptr[esp + 12]      ; binary representation of high dword of double
        mov         edx, eax
        and         edx, eq_doubleExponentMaskHigh
        cmp         edx, eq_doubleExponentMaskHigh
        jz short    d2i_NaNorInfinity

d2i_MinOrMaxInt:
        test        eax, eq_doubleSignMaskHigh    ; MAX_INT if +ve, MIN_INT if -ve
        mov         eax, eq_MinInteger
        jnz short   d2i_done
        mov         eax, eq_MaxInteger
        jmp short   d2i_done

d2i_NaNorInfinity:
        test        eax, eq_doubleMantissaMaskHigh
        jnz short   d2i_isNaN
        mov         edx, dword ptr[esp + 8]
        test        edx, edx
        jz short    d2i_MinOrMaxInt               ; double is +INF or -INF

d2i_isNaN:
        xor         eax, eax                      ; double is a NaN

d2i_done:
        pop         edx
        ret
_doubleToInt endp


; _doubleToLong
;
; Convert doubles to longs when it has already been determined that the
; double cannot be represented in a long OR if it appears that the
; double is MIN_LONG.
;
; Parameters:
;
;   ST0 - double to convert
;
; Returns:
;
;   eax,edx - equivalent low/high dword of long: either 0, MAX_LONG, or MIN_LONG
;   ST0     - unchanged
;
        align       16
_doubleToLong proc near
        push        ecx
        sub         esp, 8
        fst         qword ptr[esp]
        mov         eax, dword ptr[esp +4]        ; binary representation of high dword of double
        mov         ecx, eax
        and         ecx, eq_doubleExponentMaskHigh
        cmp         ecx, eq_doubleExponentMaskHigh
        jz short    d2l_NaNorInfinity

d2l_MinOrMaxInt:
        test        eax, eq_doubleSignMaskHigh    ; MAX_LONG if +ve, MIN_LONG if -ve
        mov         eax, eq_MaxLongLow
        mov         edx, eq_MaxLongHigh
        jz short    d2l_done
        xor         eax, eax
        mov         edx, eq_MinLongHigh
        jmp short   d2l_done

d2l_NaNorInfinity:
        test        eax, eq_doubleMantissaMaskHigh
        jnz short   d2l_isNaN
        mov         ecx, dword ptr[esp]
        test        ecx, ecx
        jz short    d2l_MinOrMaxInt               ; double is +INF or -INF

d2l_isNaN:
        xor         eax, eax                      ; double is a NaN
        xor         edx, edx

d2l_done:
        add         esp, 8
        pop         ecx
        ret
_doubleToLong endp


; _floatToInt
;
; Convert floats to ints when it has already been determined that the
; float cannot be represented in an int OR if it appears that the float
; is MIN_INT.
;
; Parameters:
;
;   [esp+4] - float to convert
;
; Returns:
;
;   eax - equivalent integer: either 0, MAX_INT, or MIN_INT
;
        align       16
_floatToInt proc near
        push        edx
        mov         eax, dword ptr[esp+8]         ; binary representation of float
        mov         edx, eax
        and         edx, eq_singleExponentMask
        cmp         edx, eq_singleExponentMask
        jnz short   f2i_MinOrMaxInt
        test        eax, eq_singleMantissaMask
        jz short    f2i_MinOrMaxInt               ; float is +INF or -INF
        xor         eax, eax                      ; float is a NaN
        jmp short   f2i_done

f2i_MinOrMaxInt:
        test        eax, eq_singleSignMask        ; MAX_INT if +ve, MIN_INT if -ve
        mov         eax, eq_MaxInteger
        jz short    f2i_done
        mov         eax, eq_MinInteger

f2i_done:
        pop         edx
        ret
_floatToInt endp


; _floatToLong
;
; Convert floats to longs when it has already been determined that the
; float cannot be represented in a long OR if it appears that the
; float is MIN_LONG.
;
; Parameters:
;
;   ST0 - float to convert
;
; Returns:
;
;   eax,edx - equivalent low/high dword of long: either 0, MAX_LONG, or MIN_LONG
;   ST0     - unchanged
;
        align       16
_floatToLong proc near
        push        ecx
        sub         esp, 4
        fst         dword ptr[esp]
        mov         eax, dword ptr[esp]           ; binary representation of float
        mov         ecx, eax
        and         ecx, eq_singleExponentMask
        cmp         ecx, eq_singleExponentMask
        jz short    f2l_NaNorInfinity

f2l_MinOrMaxInt:
        test        eax, eq_singleSignMask        ; MAX_LONG if +ve, MIN_LONG if -ve
        mov         eax, eq_MaxLongLow
        mov         edx, eq_MaxLongHigh
        jz short    f2l_done
        xor         eax, eax
        mov         edx, eq_MinLongHigh
        jmp short   f2l_done

f2l_NaNorInfinity:
        test        eax, eq_singleMantissaMask
        jz short    f2l_MinOrMaxInt               ; float is +INF or -INF
        xor         eax, eax                      ; float is a NaN
        xor         edx, edx

f2l_done:
        add         esp, 4
        pop         ecx
        ret
_floatToLong endp

endif


; _X87floatRemainder, _X87doubleRemainder
;
; Compute the remainder from the division of two floats, or two doubles,
; respectively, using the Intel floating-point partial remainder instruction
; (FPREM) in an iterative algorithm.
;
; Parameters:
;
;   (float)                      (double)
;   [esp+8] - divisor            [esp+12] - divisor
;   [esp+4] - dividend           [esp+4]  - dividend
;
; Returns:
;
;   ST0 - dividend % divisor
;
        align       16
ifdef TR_HOST_64BIT
_X87floatRemainder proc
else
_X87floatRemainder proc near
endif
ifdef NO_X87_UNDER_WIN64
   int 3
else
        push        _rax
        fld         dword ptr[_rsp+12]  ; load divisor (ST1)
        fld         dword ptr[_rsp+8]   ; load dividend (ST0)
fremloop:
        fprem
        fstsw       ax
        test        ax, 0400h
        jnz short   fremloop
        fstp        st(1)
        pop         _rax
        ret         8
endif
_X87floatRemainder endp

        align 16
ifdef TR_HOST_64BIT
_X87doubleRemainder proc
else
_X87doubleRemainder proc near
endif
ifdef NO_X87_UNDER_WIN64
   int 3
else
        push        _rax
        fld         qword ptr[_rsp+16]  ; load divisor (ST1)
        fld         qword ptr[_rsp+8]   ; load dividend (ST0)
dremloop:
        fprem
        fstsw       ax
        test        ax, 0400h
        jnz short   dremloop
        fstp        st(1)
        pop         _rax
        ret         16
endif
_X87doubleRemainder endp


; The assumption is that this thunk method would be called from a 32-bit runtime only.
; Otherwise, the stack offsets will have to change to accomodate the correct size of return
; address.
;
        align 16
ifdef TR_HOST_64BIT
_SSEfloatRemainderIA32Thunk proc
else
_SSEfloatRemainderIA32Thunk proc near
endif
        sub         _rsp, 8
        movq        qword ptr[_rsp], xmm1
        cvtss2sd    xmm1, dword ptr[_rsp +16]    ; dividend
        cvtss2sd    xmm0, dword ptr[_rsp +12]     ; divisor
        call        doSSEdoubleRemainder
        cvtsd2ss    xmm0, xmm0
        movq        xmm1, qword ptr[_rsp]
        add         _rsp, 8
        ret         8
_SSEfloatRemainderIA32Thunk endp


; The assumption is that this thunk method would be called from a 32-bit runtime only.
; Otherwise, the stack offsets will have to change to accomodate the correct size of return
; address.
;
        align 16
ifdef TR_HOST_64BIT
_SSEdoubleRemainderIA32Thunk proc
else
_SSEdoubleRemainderIA32Thunk proc near
endif
        sub         _rsp, 8
        movq        qword ptr[_rsp], xmm1
        movq        xmm1, qword ptr[_rsp +20]   ; dividend
        movq        xmm0, qword ptr[_rsp +12]   ; divisor
        call        doSSEdoubleRemainder
        movq        xmm1, qword ptr[_rsp]
        add         _rsp, 8
        ret         16
_SSEdoubleRemainderIA32Thunk endp


        align 16
ifdef TR_HOST_64BIT
_SSEfloatRemainder proc
else
_SSEfloatRemainder proc near
endif
        cvtss2sd    xmm0, xmm0
        cvtss2sd    xmm1, xmm1
        call        doSSEdoubleRemainder
        cvtsd2ss    xmm0, xmm0
        ret
_SSEfloatRemainder endp

        align 16
ifdef TR_HOST_64BIT
_SSEdoubleRemainder proc
else
_SSEdoubleRemainder proc near
endif
doSSEdoubleRemainder:
ifdef ASM_J9VM_USE_GOT
        push        _rdx
        LoadGOTInto _rdx
endif
        ucomisd xmm0, xmm1      ; unordered compare, is either operand NaN
        jp short RETURN_NAN     ; either dividend or divisor was a NaN

        sub     _rsp, 8
        movq    qword ptr[_rsp], xmm2

        ; TODO: current helper linkage expects the dividend and divisor to be
        ; the opposite order of how this code is written
        ; swap xmm0 and xmm1 until this is fixed

        movq xmm2, xmm0
        movq xmm0, xmm1
        movq xmm1, xmm2

        punpcklqdq xmm1, xmm0   ; xmm1 = {a, b}
        ; note, the sign of the divisor has no affect on the final result
ifndef ASM_J9VM_USE_GOT
        andpd xmm1, QWORD PTR [_rip ABSMASK]           ; xmm1 = {|a|, |b|}
        movapd xmm2, QWORD PTR [_rip NULL_INF_MASK]    ; xmm2 = {+inf, 0.0}
else
        andpd xmm1, QWORD PTR ABSMASK@GOTOFF[_rdx]           ; xmm1 = {|a|, |b|}
        movapd xmm2, QWORD PTR NULL_INF_MASK@GOTOFF[_rdx]    ; xmm2 = {+inf, 0.0}
endif
        cmppd xmm2, xmm1, 4     ; compare xmm2 != xmm1, leave mask in xmm1

        ; xmm1 = {(|a| != +inf ? |a| : 0, |b| != 0.0 ? |b| : 0}
        andpd xmm1, xmm2

        ; xmm2 = {|a| != +inf ? 0 : QNaN, |b| != 0.0 ? 0 : QNaN}
ifndef ASM_J9VM_USE_GOT
        andnpd xmm2, QWORD PTR [_rip  QNaN]
else
        andnpd xmm2, QWORD PTR QNaN@GOTOFF[_rdx]
endif
        ; xmm1 = {|a| != +inf ? |a| : QNaN, |b| != 0.0 ? |b| : QNaN}
        orpd xmm1, xmm2
        movapd xmm2, xmm1       ; xmm2 = xmm1
        shufpd xmm2, xmm2, 1    ; swap elems in xmm2 so |a| (or QNaN) in xmm2

        add     _rsp, 8
        ucomisd xmm2, xmm1
        movq    xmm2, qword ptr[_rsp -8]  ; for scheduling reasons updating of the SP was done 2 intrs above
ifndef ASM_J9VM_USE_GOT
        jae short _dblRemain    ; Do normal algorithm
else
        jae short JMP_DBLREMAIN ; Do normal algorithm
endif
        jnp short RETURN_DIVIDEND

RETURN_NAN:
ifndef ASM_J9VM_USE_GOT
        movapd xmm0, QWORD PTR [_rip QNaN]     ; xmm0 = QNaN
else
        movapd xmm0, QWORD PTR QNaN@GOTOFF[_rdx]     ; xmm0 = QNaN
endif
RETURN_DIVIDEND:                               ; dividend already in xmm0
ifdef ASM_J9VM_USE_GOT
        pop     _rdx
endif
        nop
        ret

ifdef ASM_J9VM_USE_GOT
JMP_DBLREMAIN:
        pop     _rdx
        jmp short _dblRemain    ; Do normal algorithm
endif


_SSEdoubleRemainder endp

                align 16
ifdef TR_HOST_64BIT
_dblRemain proc
else
_dblRemain proc near
endif
        ; Prolog Start
        push _rax
        push _rbx
        push _rcx
ifdef ASM_J9VM_USE_GOT
        push _rdx
        LoadGOTInto _rdx
endif
        sub _rsp, 48                            ; _rsp = _rsp - 48(bytes)
        movq QWORD PTR [_rsp+16], xmm2      ; preserve xmm2
        ; Prolog End
ifndef ASM_J9VM_USE_GOT
        andpd xmm1, QWORD PTR [_rip ABSMASK]   ; xmm1 = |divisor|
else
        andpd xmm1, QWORD PTR ABSMASK@GOTOFF[_rdx]   ; xmm1 = |divisor|
endif
        movq QWORD PTR [_rsp], xmm0 ; store dividend on stack
        movq QWORD PTR [_rsp+8], xmm1       ; store |divisor| on stack
        mov eax, DWORD PTR [_rsp+12]    ; eax = high 32 bits of |divisor|
        and eax, 7ff00000h              ; eax = exponent of |divisor|
        shr eax, 20
        cmp eax, 2                      ; ppc uses 0x0001 and does bgt L144
        jb SMALL_NUMS             ; we do < 2 and jb SMALL_NUMS
L144:                                   ; and fall through to the normal case

        ; Anything greater than MAGIC_NUM1 is considered "large"
        ; compare |dividend| to MAGIC_NUM1
ifndef ASM_J9VM_USE_GOT
        ucomisd xmm1, QWORD PTR [_rip MAGIC_NUM1]
else
        ucomisd xmm1, QWORD PTR MAGIC_NUM1@GOTOFF[_rdx]
endif
        jae LARGE_NUMS            ; if xmm1 >= 0x7fde42d13077b76

L184:
        movq  xmm2, xmm1             ; xmm2 = xmm1
        addsd xmm2, xmm1                ; xmm2 = xmm1 + xmm1 (2 * |divisor|)
        movq  QWORD PTR [_rsp+32], xmm2  ; store xmm2 on the stack
        mov eax, DWORD PTR [_rsp+4]     ; eax = high 32 bits of dividend

        movq  xmm4, xmm0             ; xmm4 = xmm0 (dividend)
ifndef ASM_J9VM_USE_GOT
        andpd xmm4, QWORD PTR [_rip ABSMASK] ; xmm4 = |dividend|
else
        andpd xmm4, QWORD PTR ABSMASK@GOTOFF[_rdx] ; xmm4 = |dividend|
endif
        movq QWORD PTR [_rsp], xmm4 ; store xmm4 on the stack
        mov ebx, eax                    ; ebx = eba
        and ebx, 80000000h              ; ebx = sign bit of dividend

        ucomisd xmm4, xmm2              ; compare xmm4 with xmm2
        jna L234                  ; if xmm4 <= xmm2 goto L234

        movq QWORD PTR [_rsp+40], xmm2  ; store xmm2 on the stack
        mov ecx, DWORD PTR [_rsp+36]    ; ms 32bits of |dividend| (was xmm4)
        sub eax, ecx                    ; eax = eax - ecx
        and eax, 7ff00000h               ; get exponent (rlwinm r0,r0,0,1,11)
        mov ecx, DWORD PTR [_rsp+44]
        movq xmm2, QWORD PTR [_rsp]         ; save xmm2 to stack
        add ecx, eax                            ; ecx += eax
        mov eax, ecx                            ; eax = ecx (2 results)
        add eax, 0fff00000h                     ; addis r0,r4,-16
        mov DWORD PTR [_rsp+44], ecx            ; save ecx to stack
        movq xmm0, QWORD PTR [_rsp+40]      ; store xmm0 to stack
L1d8:
        ucomisd xmm2, xmm0              ; moved this instruction down from ppc
        jae short L1e8                  ; if xmm2 >= xmm0 goto L1e8
        mov DWORD PTR [_rsp+44], eax
        movq  xmm2, QWORD PTR [_rsp]
        movq  xmm0, QWORD PTR [_rsp+40]
L1e8:
        movq  xmm4, xmm2                     ; xmm4 = xmm2
        subsd xmm4, xmm0                        ; xmm4 = xmm4 - xmm0
        movq  QWORD PTR [_rsp], xmm4         ; save xmm4
        movq  xmm2, QWORD PTR [_rsp+32]      ; load xmm2 from stack
        ucomisd xmm4, xmm2                      ; compare xmm4 with xmm2
        jna short L230                          ; if xmm4 <= xmm2 goto L230
        mov eax, DWORD PTR [_rsp+4]     ; eax = ms 32bits of dividend stack area
        mov ecx, DWORD PTR [_rsp+36]
        movq QWORD PTR [_rsp+40], xmm2      ; store xmm2 on stack
   sub eax, ecx               ;  (subf r0,r4,r0)
        mov ecx, DWORD PTR [_rsp+44]
        and eax, 7ff00000h              ; eax = exponent of |divisor|
        add ecx, eax                    ; ecx += eax
        mov eax, ecx
        add eax, 0fff00000h             ; addis r0,r4,-16
        mov DWORD PTR [_rsp+44], ecx    ; save ecx to stack
        movq xmm2, QWORD PTR [_rsp]
        movq xmm0, QWORD PTR [_rsp+40]
        jmp short L1d8

L230:
        movq    xmm1, QWORD PTR [_rsp+8]
L234:
        ucomisd xmm4, xmm1
        xorpd   xmm2, xmm2      ; xmm2 = {0.0, 0.0} (only need scalar part)
        jb short L258
        subsd xmm4, xmm1
        ucomisd xmm4, xmm1
        movq    QWORD PTR [_rsp], xmm4
        jb L258
        subsd xmm4, xmm1
        movq  QWORD PTR [_rsp], xmm4

L258:
        ucomisd xmm2, xmm4
        mov eax, DWORD PTR [_rsp+4]     ; ms 32bits of dividend
        jne short L280
        or eax, ebx                     ; restore sign of result
        mov DWORD PTR [_rsp+4], eax     ; put result back on stack
        movq xmm0, QWORD PTR [_rsp] ; load result into xmm0

        ; Epilog Start
        movq xmm2, QWORD PTR [_rsp+16]      ; restore xmm2
        add _rsp, 48
ifdef ASM_J9VM_USE_GOT
        pop _rdx
endif
        pop _rcx
        pop _rbx
        pop _rax
        ret                                     ; Epilog End
L280:
        xor eax, ebx
        mov DWORD PTR [_rsp+4], eax
        movq xmm0, QWORD PTR [_rsp]         ; load result into xmm0

        ; Epilog Start
        movq xmm2, QWORD PTR [_rsp+16]      ; restore xmm2
        add _rsp, 48
ifdef ASM_J9VM_USE_GOT
        pop _rdx
endif
        pop _rcx
        pop _rbx
        pop _rax
        ret                                     ; Epilog End

SMALL_NUMS:
ifndef ASM_J9VM_USE_GOT
        mulsd xmm1, QWORD PTR [_rip SCALEUP_NUM] ; xmm1 = |divisor| * 2^54
else
        mulsd xmm1, QWORD PTR SCALEUP_NUM@GOTOFF[_rdx] ; xmm1 = |divisor| * 2^54
endif
        movq  QWORD PTR [_rsp+8], xmm1       ; store xmm1 on the stack
        call _dblRemain

ifndef ASM_J9VM_USE_GOT
        mulsd xmm0, QWORD PTR [_rip SCALEUP_NUM]       ; xmm0 = dividend * 2^54
else
        mulsd xmm0, QWORD PTR SCALEUP_NUM@GOTOFF[_rdx]       ; xmm0 = dividend * 2^54
endif
        movq  xmm1, QWORD PTR [_rsp+8]       ; load divisor from the stack
        movq  QWORD ptr [_rsp], xmm0         ; store dividend on the stack
        call _dblRemain

ifndef ASM_J9VM_USE_GOT
        mulsd xmm0, QWORD PTR [_rip SCALEDOWN_NUM]     ; dividend * 1/2^54
else
        mulsd xmm0, QWORD PTR SCALEDOWN_NUM@GOTOFF[_rdx]     ; dividend * 1/2^54
endif

        ; Epilog Start
        movq xmm2, QWORD PTR [_rsp+16]              ; restore xmm2
        add _rsp, 48
ifdef ASM_J9VM_USE_GOT
        pop _rdx
endif
        pop _rcx
        pop _rbx
        pop _rax
        ret                                             ; Epilog End

LARGE_NUMS:
ifndef ASM_J9VM_USE_GOT
        mulsd xmm1, QWORD PTR [_rip ONEHALF]   ; xmm1 *= 0.5 (scaledown)
else
        mulsd xmm1, QWORD PTR ONEHALF@GOTOFF[_rdx]   ; xmm1 *= 0.5 (scaledown)
endif
        ; store xmm1 in divisor slot on stack
        movq  QWORD PTR [_rsp+8], xmm1
ifndef ASM_J9VM_USE_GOT
        mulsd xmm0, QWORD PTR [_rip ONEHALF]   ; xmm2 *= 0.5 (scaledown)
else
        mulsd xmm0, QWORD PTR ONEHALF@GOTOFF[_rdx]   ; xmm2 *= 0.5 (scaledown)
endif
        movq     QWORD PTR [_rsp], xmm0 ; store xmm0 in dividend slot on stack
        call _dblRemain

        addsd xmm0, xmm0                        ; xmm0 *= 2 (scaleup)

        ; Epilog Start
        movq     xmm2, QWORD PTR [_rsp+16]  ; restore xmm2
        add _rsp, 48
ifdef ASM_J9VM_USE_GOT
        pop _rdx
endif
        pop _rcx
        pop _rbx
        pop _rax
        ret                             ; Epilog End

_dblRemain endp

ifndef TR_HOST_64BIT
; in:   32-bit slot on stack - double to convert
; out:  edx, eax - resulting long
; uses: eax, ebx, ecx, edx, esi
;
; Note that the double is assumed to have an exponent >= 32.

        align 16
ifdef TR_HOST_64BIT
_SSEdouble2LongIA32 proc
else
_SSEdouble2LongIA32 proc near
endif
        mov edx, dword ptr[_rsp+8]      ; load the double into edx:eax
        mov eax, dword ptr[_rsp+4]

   push esi       ; save esi as private linkage
   push ecx       ; save ecx as private linkage
   push ebx       ; save ebx as private linkage

        mov ecx, edx                    ; extract the biased exponent E of the double into cx
        shr ecx, 20
        and cx, 07ffh

        mov esi, edx                    ; set esi if the double is negative
        shr esi, 31

        and edx, 000fffffh              ; clear out the sign bit and the 11-bit exponent field of the double number
        or edx, 00100000h               ; add the implicit leading 1 to the mantissa of the double

        cmp cx, 1075
        jae SSEd2l_convertLarge         ; if exponent is >= 52, we need to scale up the mantissa

; if E < 1075 (i.e. the unbiased exponent is >= 32 and < 52),
; then 1 or more low bits of the 52-bit mantissa must be truncated,
; since those bits represent the fraction

        neg cx                          ; cl = cx = 52 - exponent
        add cx, 1075

        mov ebx, edx                    ; shift the mantissa (edx:eax) right by cl; this removes the fraction from the double
        shr edx, cl
        shr eax, cl

        neg cl                          ; cl = 32 - cl
        add cl, 32
        shl ebx, cl                     ; shift the lower bits of edx into the upper end of eax
        or eax, ebx

        jmp SSEd2l_doneConversion       ; done

SSEd2l_convertLarge:
        cmp cx, 1086
        jae SSEd2l_convertSpecial       ; if exponent is >= 63, the value is too large for a long

; if E < 1086 (i.e. the unbiased exponent is >= 52 and < 63),
; then the long can be computed by shifting the mantissa to the left
; by (exponent - 52).

        sub cx, 1075                    ; cl = cx = exponent - 52
        je  SSEd2l_doneConversion       ; if exponent == 52, no shifting is needed

        mov ebx, eax                    ; shift the mantissa (edx:eax) left by cl; the result is the desired long
        shl edx, cl
        shl eax, cl

        neg cl                          ; cl = 32 - cl
        add cl, 32
        shr ebx, cl                     ; shift the upper bits eax into the lower end of edx
        or edx, ebx

SSEd2l_doneConversion:
        test esi, esi
        jne SSEd2l_negateLong
   pop ebx           ; restore ebx as private linkage
   pop ecx           ; restore ecx as private linkage
   pop esi           ; restore esi as private linkage
        ret 8

SSEd2l_negateLong:
        not edx
        neg eax
        sbb edx, -1
   pop ebx           ; restore ebx as private linkage
   pop ecx           ; restore ecx as private linkage
   pop esi           ; restore esi as private linkage
        ret 8

SSEd2l_convertSpecial:                  ; perform conversions for special numbers (very large values, infinities, NaNs)
        cmp cx, 7ffh                    ; if the exponent is all 1's then the double is either an infinity or a NaN
        je SSEd2l_NaNOrInfinity

SSEd2l_maxLong:                         ; if double is simply a very large number (exponent >= 63 and < 1023)
        test esi, esi                   ; if the number is positive then return (2^63 - 1), else return -(2^63)
        jne SSEd2l_minLong
        or eax, -1
        mov edx, 7fffffffh
   pop ebx           ; restore ebx as private linkage
   pop ecx           ; restore ecx as private linkage
   pop esi           ; restore esi as private linkage
        ret 8

SSEd2l_minLong:
        xor eax, eax
        mov edx, 80000000h
   pop ebx           ; restore ebx as private linkage
   pop ecx           ; restore ecx as private linkage
   pop esi           ; restore esi as private linkage
        ret 8

SSEd2l_NaNOrInfinity:                   ; if the double is either an infinity or Nan
        test eax, eax                   ; if the mantissa is 0, the double is an infinity; return a max or min long
        jne SSEd2l_NaN
        cmp edx, 00100000h              ; mantissa is 0, but don't forget we added a leading 1, hence 00100000h
        jne SSEd2l_NaN
        jmp SSEd2l_maxLong

SSEd2l_NaN:                             ; if the number is a NaN, return 0 (note: no exception flags are set if it is a QNaN)
        xor edx, edx
        xor eax, eax
   pop ebx           ; restore ebx as private linkage
   pop ecx           ; restore ecx as private linkage
   pop esi           ; restore esi as private linkage
        ret 8
_SSEdouble2LongIA32 endp
endif

jitFPHelpersEnd:


eq_J9VMThread_heapAlloc      equ J9TR_VMThread_heapAlloc
eq_J9VMThread_heapTop        equ J9TR_VMThread_heapTop
eq_J9VMThread_PrefetchCursor equ J9TR_VMThread_tlhPrefetchFTA

      public prefetchTLH
      public newPrefetchTLH
      public outlinedNewObject
      public outlinedNewArray
      public outlinedNewObjectNoZeroInit
      public outlinedNewArrayNoZeroInit

; outlinedNewArray
;
; Allocates new object out of line. Callable by the JIT generated code.
; Assumes EBP = vmThread, EAX is the return value.
; If we are out of TLH space, it returns 0.
; IT DOES NOT INITIALIZE THE HEADER
;
; Parameters:
;
;   class
;   instance size
;
; Returns:
;
;   eax - allocated object address or 0 if we are out of TLH space
;
        align       16
ifdef TR_HOST_64BIT
outlinedNewArray proc
else
outlinedNewArray proc near
endif

ifndef J9TR_VMThread_heapAlloc
	int 3 ; outlined allocation not supported without heapAlloc field
else
				cmp _rdi, 8h
                                jle cannotAllocateNZI
				mov		_rax, 10h
				cmp   _rdi, 10h
				cmovb _rdi, _rax
				;jmp startNew

				mov		_rax, [_rbp + eq_J9VMThread_heapAlloc]		; load heap alloc
 				mov		_rcx, [_rbp + eq_J9VMThread_heapTop]		; check if it's larger than heapTop
				sub             _rcx, _rax                                      ; compute how much space is available
				cmp             _rcx, _rdi                                      ; see if it's enough for our object
				jb              cannotAllocateNewArray
 				lea		_rcx, [_rax + _rdi]				; compute new heapAlloc
        prefetchnta [_rcx + 0c0h]
				mov		[_rbp + eq_J9VMThread_heapAlloc], _rcx		; store new heapAlloc
        prefetchnta [_rcx + 0100h]
        prefetchnta [_rcx + 0140h]
        prefetchnta [_rcx + 0180h]
		                xor             _rcx, _rcx
ifdef TR_HOST_64BIT
ifdef ASM_J9VM_INTERP_COMPRESSED_OBJECT_HEADER
                                mov             dword ptr       [_rax + 8], ecx
else
                                mov             dword ptr       [_rax + 12], ecx
endif
else
                                mov             dword ptr       [_rax + 8], ecx
endif

donePrefetch:

 				cmp _rdi, 40
 				jb doArrayLoopCopy

ifdef TR_HOST_64BIT
 				lea _rcx, [_rdi + 7]
            shr _rcx, 3
else
            lea _rcx, [_rdi + 3]
            shr _rcx, 2
endif
 				mov _rdi, _rax
 				mov _rsi, _rax
 				xor _rax, _rax

ifdef TR_HOST_64BIT
 				rep stosq
else
 				rep stosd
endif
 				mov _rax, _rsi
 				ret

doArrayLoopCopy:
 				; clear the memory
 				xor _rcx, _rcx
 				sub _rdi, eq_gpr_size     ; added right subtract in init loop
zeroInitLoopNewArray:
 				mov [_rax + _rdi], _rcx ; store zero
 				sub _rdi, eq_gpr_size
 				jge zeroInitLoopNewArray
 				ret

cannotAllocateNewArray:
				xor _rax, _rax
				ret

endif ; ifndef J9TR_VMThread_heapAlloc

outlinedNewArray endp

        align       16
ifdef TR_HOST_64BIT
outlinedNewArrayNoZeroInit proc
else
outlinedNewArrayNoZeroInit proc near
endif

ifndef J9TR_VMThread_heapAlloc
	int 3 ; outlined allocation not supported without heapAlloc field
else
                                cmp _rdi, 8h
                                jle cannotAllocateNZI
				mov		_rax, 10h
				cmp   _rdi, 10h
				cmovb _rdi, _rax
				;jmp startNewNZI

				mov		_rax, [_rbp + eq_J9VMThread_heapAlloc]		; load heap alloc
 				mov		_rcx, [_rbp + eq_J9VMThread_heapTop]		; check if it's larger than heapTop
				sub             _rcx, _rax                                      ; compute how much space is available
				cmp             _rcx, _rdi                                      ; see if it's enough for our object
				jb              cannotAllocateNZI
 				lea		_rcx, [_rax + _rdi]				; compute new heapAlloc
        prefetchnta [_rcx + 0c0h]
				mov		[_rbp + eq_J9VMThread_heapAlloc], _rcx		; store new heapAlloc
        prefetchnta [_rcx + 0100h]
        prefetchnta [_rcx + 0140h]
        prefetchnta [_rcx + 0180h]
        xor             _rcx, _rcx
ifdef TR_HOST_64BIT
ifdef ASM_J9VM_INTERP_COMPRESSED_OBJECT_HEADER
                                mov             dword ptr       [_rax + 8], ecx
else
                                mov             dword ptr       [_rax + 12], ecx
endif
else
                                mov             dword ptr       [_rax + 8], ecx
endif

doneNZI:
        ret

cannotAllocateNZI:
                               xor _rax, _rax
                               jmp doneNZI

endif ; ifndef J9TR_VMThread_heapAlloc

outlinedNewArrayNoZeroInit endp

; outlinedNewObject
;
; Allocates new object out of line. Callable by the JIT generated code.
; Assumes EBP = vmThread, EAX is the return value.
; If we are out of TLH space, it returns 0.
; IT DOES NOT INITIALIZE THE HEADER
;
; Parameters:
;
;   class
;   instance size
;
; Returns:
;
;   eax - allocated object address or 0 if we are out of TLH space
;
        align       16
ifdef TR_HOST_64BIT
outlinedNewObject proc
else
outlinedNewObject proc near
endif
startNew:

ifndef J9TR_VMThread_heapAlloc
	int 3 ; outlined allocation not supported without heapAlloc field
else

				mov		_rax, [_rbp + eq_J9VMThread_heapAlloc]		; load heap alloc
 				mov		_rcx, [_rbp + eq_J9VMThread_heapTop]		; check if it's larger than heapTop
				sub             _rcx, _rax                                      ; compute how much space is available
				cmp             _rcx, _rdi                                      ; see if it's enough for our object
				jb              cannotAllocateNew
 				lea		_rcx, [_rax + _rdi]				; compute new heapAlloc
        prefetchnta [_rcx + 0c0h]
				mov		[_rbp + eq_J9VMThread_heapAlloc], _rcx		; store new heapAlloc
        prefetchnta [_rcx + 0100h]
        prefetchnta [_rcx + 0140h]
        prefetchnta [_rcx + 0180h]

ifdef ASM_J9VM_GC_TLH_PREFETCH_FTA_DISABLE
				sub	dword ptr [_rbp + J9TR_VMThread_tlhPrefetchFTA], edi
 				jle	prefetchHeap																					; do prefetch of the next TLH chunk
donePrefetch:
endif

 				cmp _rdi, 40
 				jb doObjectLoopCopy

ifdef TR_HOST_64BIT
 				lea _rcx, [_rdi + 7]
            shr _rcx, 3
else
            lea _rcx, [_rdi + 3]
            shr _rcx, 2
endif
 				mov _rdi, _rax
 				mov _rsi, _rax
 				xor _rax, _rax

ifdef TR_HOST_64BIT
 				rep stosq
else
 				rep stosd
endif
 				mov _rax, _rsi
 				ret

doObjectLoopCopy:
				; clear the memory
 				xor    _rcx, _rcx
 				sub    _rdi, eq_gpr_size     ; added right subtract in init loop
zeroInitLoop:
 				mov    [_rax + _rdi], _rcx ; store zero
 				sub    _rdi, eq_gpr_size
 				jge    zeroInitLoop
        ret

cannotAllocateNew:
				xor _rax, _rax
				ret

ifdef ASM_J9VM_GC_TLH_PREFETCH_FTA_DISABLE
prefetchHeap:
				call prefetchTLH;
				jmp donePrefetch
endif

endif ; ifndef J9TR_VMThread_heapAlloc

outlinedNewObject endp

        align       16
ifdef TR_HOST_64BIT
outlinedNewObjectNoZeroInit proc
else
outlinedNewObjectNoZeroInit proc near
endif
startNewNZI:

ifndef J9TR_VMThread_heapAlloc
	int 3 ; outlined allocation not supported without heapAlloc field
else

				mov		_rax, [_rbp + eq_J9VMThread_heapAlloc]		; load heap alloc
 				mov		_rcx, [_rbp + eq_J9VMThread_heapTop]		; check if it's larger than heapTop
				sub             _rcx, _rax                                      ; compute how much space is available
				cmp             _rcx, _rdi                                      ; see if it's enough for our object
				jb              cannotAllocateNI
 				lea		_rcx, [_rax + _rdi]				; compute new heapAlloc
        prefetchnta [_rcx + 0c0h]
				mov		[_rbp + eq_J9VMThread_heapAlloc], _rcx		; store new heapAlloc
        prefetchnta [_rcx + 0100h]
        prefetchnta [_rcx + 0140h]
        prefetchnta [_rcx + 0180h]

ifdef ASM_J9VM_GC_TLH_PREFETCH_FTA_DISABLE
				sub	dword ptr [_rbp + J9TR_VMThread_tlhPrefetchFTA], edi
 				jle	prefetchHeapNZI																					; do prefetch of the next TLH chunk
donePrefetchNZI:
endif
				; clear the memory
ifdef DO_CLASS_POINTER_INIT
 				xor     _rcx, _rcx
endif

ifdef ENABLE_THIS_CODE_FOR_LOCK_WORD_INIT
ifdef TR_HOST_64BIT
                                int3 ; reminder so that someone can add this code
else
                                mov     _rdi, dword ptr [_rax + J9TR_J9Object_class] ; receiver class
                                and     _rdi, eq_ObjectClassMask
                                mov     _rdi, dword ptr [_rdi + J9TR_J9Class_lockOffset] ; offset of lock word in receiver class
                                cmp     _rdi, 0
                                jle     skipLockWordInit
                                mov     dword ptr [_rax + _rdi], _rcx
endif
skipLockWordInit:
endif

ifdef DO_CLASS_POINTER_INIT
 				mov     [_rax ], _rcx ; 8-byte store is harmless even with compressed object headers
endif
doneNI:
        ret

cannotAllocateNI:
				xor _rax, _rax
				jmp doneNI

ifdef ASM_J9VM_GC_TLH_PREFETCH_FTA_DISABLE
prefetchHeapNZI:
				call prefetchTLH;
				jmp donePrefetchNZI
endif

endif ; ifndef J9TR_VMThread_heapAlloc

outlinedNewObjectNoZeroInit endp


ifdef ASM_J9VM_GC_TLH_PREFETCH_FTA

      align 16
ifdef TR_HOST_64BIT
prefetchTLH proc
else
prefetchTLH proc near
endif
      push  _rcx

ifdef TR_HOST_64BIT
      mov   _rcx, qword ptr[_rbp + eq_J9VMThread_heapAlloc]
else
      mov   _rcx, dword ptr[_rbp + eq_J9VMThread_heapAlloc]
endif
      prefetchnta byte ptr[_rcx+256]
      prefetchnta byte ptr[_rcx+320]
      prefetchnta byte ptr[_rcx+384]
      prefetchnta byte ptr[_rcx+448]
      prefetchnta byte ptr[_rcx+512]
      prefetchnta byte ptr[_rcx+576]
      prefetchnta byte ptr[_rcx+640]
      prefetchnta byte ptr[_rcx+704]
      prefetchnta byte ptr[_rcx+768]

ifdef TR_HOST_64BIT
      mov   qword ptr[_rbp + eq_J9VMThread_PrefetchCursor], 384
else
      mov   dword ptr[_rbp + eq_J9VMThread_PrefetchCursor], 384
endif
prefetch_done:
      pop   _rcx
      ret
prefetchTLH endp



; Linkage:
;
; _rax = start of object just allocated
;
; Variable definitions:
;
; O = TLH allocation pointer before the current object was allocated
; S = size of current object + padding
; A = allocation pointer (A = O+S)
; B = current prefetch trigger boundary

eq_initialPrefetchWindow    equ 64*10
eq_prefetchWindow           equ 64*4
eq_prefetchTriggerDistance  equ 64*8

      align 16
ifdef TR_HOST_64BIT
newPrefetchTLH proc
else
newPrefetchTLH proc near
endif

;      int 3

      push        _rcx                                         ; preserve
      push        _rdx                                         ; preserve
      push        _rsi                                         ; preserve

ifdef TR_HOST_64BIT
      mov         _rdx, qword ptr[_rbp + eq_J9VMThread_heapAlloc]      ; _rdx = O+S
      mov         ecx, dword ptr[_rbp + eq_J9VMThread_PrefetchCursor]  ; _rcx = current trigger boundary (B)
else
      mov         _rdx, dword ptr[_rbp + eq_J9VMThread_heapAlloc]      ; _rdx = O+S
      mov         _rcx, dword ptr[_rbp + eq_J9VMThread_PrefetchCursor] ; _rcx = current trigger boundary (B)
endif

      ; A zero value for the trigger boundary indicates that this is a new TLH
      ; that we haven't prefetched from yet.
      ;
      test        _rcx, _rcx
      jz prefetchFromNewTLH

      lea         _rcx, [_rcx + eq_prefetchTriggerDistance]    ; _rcx = derive prefetch frontier (F)
                                                               ;    where F = B + prefetchTriggerDistance
      lea         _rsi, [_rcx +64]                             ; _rsi = prefetch frontier (F) + 1 cache line
                                                               ;    +1 = start at the next cache line after the frontier

      ; Push the new frontier out even further if we're allocating a large object that
      ; exceeds the current frontier.
      ;
      cmp         _rcx, _rdx
      cmovl       _rcx, _rdx                                   ; _rcx = max(F, O+S)
      add         _rcx, eq_prefetchWindow                      ; _rcx = F' = F+eq_prefetchWindow = new prefetch frontier (F')

ifdef TR_HOST_64BIT
      mov         _rdx, qword ptr[_rbp + eq_J9VMThread_heapTop]
else
      mov         _rdx, dword ptr[_rbp + eq_J9VMThread_heapTop]
endif

      cmp         _rcx, _rdx                                   ; F' >= heapTop?
      jae prefetchFinalFrontier

mergePrefetchTLHRoundDown:
      and         _rcx, -64                                    ; round down to cache line

mergePrefetchTLH:
      ; Prefetch all lines between the last prefetch frontier (F) and the new one (F').
      ; Prefetch forward to give the data a greater chance of being there when we need it.
      ;
      ; _rsi = last frontier (F)
      ; _rcx = next frontier (F')

      ; Since interpreter allocations do not advance the prefetch frontier the last frontier
      ; might be behind the allocation pointer, in which case we want to move it forward so
      ; that we don't prefetch data that has already been allocated.
      ;
      cmp         _rsi, _rax
      cmovl       _rsi, _rax                                   ; _rsi = max(F+64, O)

      sub         _rsi, _rcx
doPrefetch:
      prefetchnta [_rcx+_rsi]
      add         _rsi, 64
      jle short doPrefetch

      sub         _rcx, eq_prefetchTriggerDistance             ; _rcx = new trigger boundary (B)

ifdef TR_HOST_64BIT
      mov         dword ptr[_rbp + eq_J9VMThread_PrefetchCursor], ecx
else
      mov         dword ptr[_rbp + eq_J9VMThread_PrefetchCursor], _rcx
endif
      pop         _rsi
      pop         _rdx
      pop         _rcx
      ret

prefetchFinalFrontier:
      mov         _rcx, _rdx
      jmp mergePrefetchTLH


prefetchFromNewTLH:
      ; We have a new TLH that has not been prefetched.  Assume that the object
      ; just allocated will have already been pulled into the cache.  Begin
      ;
      ; Prefetch from O through O+initialPrefetchWindow
      ; if (O+i <= O+s) prefetch to
      ;
      mov         _rsi, _rdx                                   ; _rsi = A
      lea         _rcx, [_rdx + eq_initialPrefetchWindow]      ; _rcx = A + Wi

ifdef TR_HOST_64BIT
      mov         _rdx, qword ptr[_rbp + eq_J9VMThread_heapTop]
else
      mov         _rdx, dword ptr[_rbp + eq_J9VMThread_heapTop]
endif
      cmp         _rcx, _rdx
      jae mergePrefetchTLH

;;      cmovg    _rcx, _rdx                                    ; _rcx = F' = min(F', heapTop)

      jmp mergePrefetchTLHRoundDown

newPrefetchTLH endp


else  ; ASM_J9VM_GC_TLH_PREFETCH_FTA

      align 16
ifdef TR_HOST_64BIT
prefetchTLH proc
else
prefetchTLH proc near
endif
         ret
prefetchTLH endp

      align 16
ifdef TR_HOST_64BIT
newPrefetchTLH proc
else
newPrefetchTLH proc near
endif
         ret
newPrefetchTLH endp

endif  ; REALTIME

      public   jitReleaseVMAccess

ifndef TR_HOST_64BIT

; --------------------------------------------------------------------------------
;
;                                    32-BIT
;
; --------------------------------------------------------------------------------

      public   clearFPStack

jitReleaseVMAccess PROC NEAR
                pusha
                push       ebp
                mov        eax, [ebp+J9TR_VMThread_javaVM]
                mov        eax, [eax+J9TR_JavaVMInternalFunctionTable]
                call       [eax+J9TR_InternalFunctionTableReleaseVMAccess]
                add        esp,4
                popa
                ret
jitReleaseVMAccess ENDP

	; Wipe the entire FP stack without raising any exceptions.
	;
		align 16
clearFPStack proc near
		ffree     st(0)
		ffree     st(1)
		ffree     st(2)
		ffree     st(3)
		ffree     st(4)
		ffree     st(5)
		ffree     st(6)
		ffree     st(7)
		ret
clearFPStack endp

else

; --------------------------------------------------------------------------------
;
;                                    64-BIT
;
; --------------------------------------------------------------------------------

jitReleaseVMAccess proc
        ; Save system-linkage volatile regs
        ; GPRs
        push    r11     ; Lin Win
        push    r10     ; Lin Win
        push    r9      ; Lin Win
        push    r8      ; Lin Win
        push    rsi     ; Lin
        push    rdi     ; Lin
        push    rdx     ; Lin Win
        push    rcx     ; Lin Win
        push    rax     ; Lin Win
        ; XMMs
        ; TODO: We don't need to save them all on Windows
        sub     rsp, 128 ; Reserve space for XMMs
        ; Do the writes in-order so we don't defeat the cache
        movq    qword ptr [rsp+0],   xmm0
        movq    qword ptr [rsp+8],   xmm1
        movq    qword ptr [rsp+16],  xmm2
        movq    qword ptr [rsp+24],  xmm3
        movq    qword ptr [rsp+32],  xmm4
        movq    qword ptr [rsp+40],  xmm5
        movq    qword ptr [rsp+48],  xmm6
        movq    qword ptr [rsp+56],  xmm7
        movq    qword ptr [rsp+64],  xmm8
        movq    qword ptr [rsp+72],  xmm9
        movq    qword ptr [rsp+80],  xmm10
        movq    qword ptr [rsp+88],  xmm11
        movq    qword ptr [rsp+96],  xmm12
        movq    qword ptr [rsp+104], xmm13
        movq    qword ptr [rsp+112], xmm14
        movq    qword ptr [rsp+120], xmm15

        ; Call vmThread->internalFunctionTable->releaseVMAccess(rbp)
ifdef WINDOWS
        mov     rcx, rbp
else
        mov     rdi, rbp
endif
        mov     rax, qword ptr[rbp+J9TR_VMThread_javaVM]
        mov     rax, qword ptr[rax+J9TR_JavaVMInternalFunctionTable]
        call    qword ptr [rax+J9TR_InternalFunctionTableReleaseVMAccess]

        ; Restore registers
        ; XMMs
        movq    xmm0,  qword ptr [rsp+0]
        movq    xmm1,  qword ptr [rsp+8]
        movq    xmm2,  qword ptr [rsp+16]
        movq    xmm3,  qword ptr [rsp+24]
        movq    xmm4,  qword ptr [rsp+32]
        movq    xmm5,  qword ptr [rsp+40]
        movq    xmm6,  qword ptr [rsp+48]
        movq    xmm7,  qword ptr [rsp+56]
        movq    xmm8,  qword ptr [rsp+64]
        movq    xmm9,  qword ptr [rsp+72]
        movq    xmm10, qword ptr [rsp+80]
        movq    xmm11, qword ptr [rsp+88]
        movq    xmm12, qword ptr [rsp+96]
        movq    xmm13, qword ptr [rsp+104]
        movq    xmm14, qword ptr [rsp+112]
        movq    xmm15, qword ptr [rsp+120]
        add     rsp, 128
        ; GPRs
        pop     rax
        pop     rcx
        pop     rdx
        pop     rdi
        pop     rsi
        pop     r8
        pop     r9
        pop     r10
        pop     r11
        ret
jitReleaseVMAccess endp


endif ; 64-bit


_TEXT ends

end
