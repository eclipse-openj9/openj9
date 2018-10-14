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

%include "jilconsts.inc"

%ifdef TR_HOST_64BIT
        ; 64-bit
eq_floats_Offset              equ 128
eq_gprs_Offset                equ 0

eq_gpr_size                   equ 8
%ifdef ASM_J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   eq_vft_pointer_size        equ 4
%else
   eq_vft_pointer_size        equ 8
%endif


   ; Hack marker:
%ifdef WINDOWS
   %define NO_X87_UNDER_WIN64 1
%endif
%else
        ; 32-bit
        CPU P2

eq_gpr_size                   equ 4
eq_vft_pointer_size           equ 4

%endif


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

%ifdef TR_HOST_64BIT

eq_MaxLong                    equ 07fffffffffffffffh
eq_MinLong                    equ 08000000000000000h

; doubles are inspected after a left rotation to get the interesting bits into
; the lower half of the register, where we can use CMP, TEST, etc. on them.
eq_doubleLeftRotation         equ 12
eq_doubleSignMaskRotated      equ 0800h
eq_doubleExponentMaskRotated  equ 07ffh
;eq_doubleMantissaMaskRotated  equ 0fffffffffffff000h
eq_doubleMantissaMaskRotated  equ -01000h

%else

eq_MaxLongLow               equ 0ffffffffh
eq_MaxLongHigh              equ eq_MaxInteger
eq_MinLongHigh              equ eq_MinInteger

eq_doubleSignMaskHigh       equ 080000000h
eq_doubleExponentMaskHigh   equ 07ff00000h
eq_doubleMantissaMaskHigh   equ 0000fffffh
eq_doubleNanMaskHigh        equ 07ff80000h

%endif


segment .data

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

segment .text

%ifdef TR_HOST_32BIT
        global _doubleToLong
        global _doubleToInt
        global _floatToLong
        global _floatToInt
%endif
        global _X87floatRemainder
        global _X87doubleRemainder

        global _SSEfloatRemainder
        global _SSEdoubleRemainder
        global _SSEfloatRemainderIA32Thunk
        global _SSEdoubleRemainderIA32Thunk
%ifdef TR_HOST_32BIT
        global _SSEdouble2LongIA32
%endif
        global _jitFPHelpersBegin
        global _jitFPHelpersEnd

        align 16
_jitFPHelpersBegin:

%ifdef TR_HOST_32BIT

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
_doubleToInt:
        push        edx
        mov         eax, dword  [esp + 12]         ; binary representation of high dword of double
        mov         edx, eax
        and         edx, eq_doubleExponentMaskHigh
        cmp         edx, eq_doubleExponentMaskHigh
        jz short    d2i_NaNorInfinity

d2i_MinOrMaxInt:
        test        eax, eq_doubleSignMaskHigh     ; MAX_INT if +ve, MIN_INT if -ve
        mov         eax, eq_MinInteger
        jnz short   d2i_done
        mov         eax, eq_MaxInteger
        jmp short   d2i_done

d2i_NaNorInfinity:
        test        eax, eq_doubleMantissaMaskHigh
        jnz short   d2i_isNaN
        mov         edx, dword [esp + 8]
        test        edx, edx
        jz short    d2i_MinOrMaxInt                ; double is +INF or -INF

d2i_isNaN:
        xor         eax, eax                       ; double is a NaN

d2i_done:
        pop         edx
        retn


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
_doubleToLong:
        push        ecx
        sub         esp, 8
        fst         qword  [esp]
        mov         eax, dword  [esp +4]           ; binary representation of high dword of double
        mov         ecx, eax
        and         ecx, eq_doubleExponentMaskHigh
        cmp         ecx, eq_doubleExponentMaskHigh
        jz short    d2l_NaNorInfinity

d2l_MinOrMaxInt:
        test        eax, eq_doubleSignMaskHigh     ; MAX_LONG if +ve, MIN_LONG if -ve
        mov         eax, eq_MaxLongLow
        mov         edx, eq_MaxLongHigh
        jz short    d2l_done
        xor         eax, eax
        mov         edx, eq_MinLongHigh
        jmp short   d2l_done

d2l_NaNorInfinity:
        test        eax, eq_doubleMantissaMaskHigh
        jnz short   d2l_isNaN
        mov         ecx, dword  [esp]
        test        ecx, ecx
        jz short    d2l_MinOrMaxInt                ; double is +INF or -INF

d2l_isNaN:
        xor         eax, eax                       ; double is a NaN
        xor         edx, edx

d2l_done:
        add         esp, 8
        pop         ecx
        retn


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
_floatToInt:
        push        edx
        mov         eax, dword [esp+8]             ; binary representation of float
        mov         edx, eax
        and         edx, eq_singleExponentMask
        cmp         edx, eq_singleExponentMask
        jnz short   f2i_MinOrMaxInt
        test        eax, eq_singleMantissaMask
        jz short    f2i_MinOrMaxInt                ; float is +INF or -INF
        xor         eax, eax                       ; float is a NaN
        jmp short   f2i_done

f2i_MinOrMaxInt:
        test        eax, eq_singleSignMask         ; MAX_INT if +ve, MIN_INT if -ve
        mov         eax, eq_MaxInteger
        jz short    f2i_done
        mov         eax, eq_MinInteger

f2i_done:
        pop         edx
        retn


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
_floatToLong:; proc near
        push        ecx
        sub         esp, 4
        fst         dword [esp]
        mov         eax, dword [esp]              ; binary representation of float
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
        retn

%endif ; TR_HOST_32BIT


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

_X87floatRemainder:
%ifdef NO_X87_UNDER_WIN64
        int 3
%else
        push        rax
        fld         dword [rsp+12]  ; load divisor (ST1)
        fld         dword [rsp+8]   ; load dividend (ST0)
fremloop:
        fprem
        fstsw       ax
        test        ax, 0400h
        jnz short   fremloop
        fstp        st1
        pop         rax
        ret         8
%endif

        align 16
_X87doubleRemainder:
%ifdef NO_X87_UNDER_WIN64
        int 3
%else
        push        rax
        fld         qword [rsp+16]  ; load divisor (ST1)
        fld         qword [rsp+8]   ; load dividend (ST0)
dremloop:
        fprem
        fstsw       ax
        test        ax, 0400h
        jnz short   dremloop
        fstp        st1
        pop         rax
        ret         16
%endif


; The assumption is that this thunk method would be called from a 32-bit runtime only.
; Otherwise, the stack offsets will have to change to accomodate the correct size of return
; address.
;
        align 16
_SSEfloatRemainderIA32Thunk:
        sub         rsp, 8
        movq        qword [rsp], xmm1
        cvtss2sd    xmm1, dword [rsp +16]     ; dividend
        cvtss2sd    xmm0, dword [rsp +12]     ; divisor
        call        doSSEdoubleRemainder
        cvtsd2ss    xmm0, xmm0
        movq        xmm1, qword [rsp]
        add         rsp, 8
        ret         8


; The assumption is that this thunk method would be called from a 32-bit runtime only.
; Otherwise, the stack offsets will have to change to accomodate the correct size of return
; address.
;
        align 16
_SSEdoubleRemainderIA32Thunk:
        sub         rsp, 8
        movq        qword [rsp], xmm1
        movq        xmm1, qword [rsp +20]     ; dividend
        movq        xmm0, qword [rsp +12]     ; divisor
        call        doSSEdoubleRemainder
        movq        xmm1, qword [rsp]
        add         rsp, 8
        ret         16


        align 16
_SSEfloatRemainder:
        cvtss2sd    xmm0, xmm0
        cvtss2sd    xmm1, xmm1
        call        doSSEdoubleRemainder
        cvtsd2ss    xmm0, xmm0
        ret

        align 16
_SSEdoubleRemainder:

doSSEdoubleRemainder:
%ifdef ASM_J9VM_USE_GOT
        push        rdx
        LoadGOTInto rdx
%endif
        ucomisd xmm0, xmm1                    ; unordered compare, is either operand NaN
        jp short RETURN_NAN                   ; either dividend or divisor was a NaN

        sub     rsp, 8
        movq    qword [rsp], xmm2

        ; TODO: current helper linkage expects the dividend and divisor to be
        ; the opposite order of how this code is written
        ; swap xmm0 and xmm1 until this is fixed

        movq xmm2, xmm0
        movq xmm0, xmm1
        movq xmm1, xmm2

        punpcklqdq xmm1, xmm0                           ; xmm1 = {a, b}
        ; note, the sign of the divisor has no affect on the final result
%ifndef ASM_J9VM_USE_GOT
        andpd xmm1, oword  [rel + ABSMASK]              ; xmm1 = {|a|, |b|}
        movapd xmm2, [rel NULL_INF_MASK]                ; xmm2 = {+inf, 0.0}
%else
        andpd xmm1, QWORD  ABSMASK@GOTOFF[rdx]          ; xmm1 = {|a|, |b|}
        movapd xmm2, QWORD  NULL_INF_MASK@GOTOFF[rdx]   ; xmm2 = {+inf, 0.0}
%endif
        cmppd xmm2, xmm1, 4                             ; compare xmm2 != xmm1, leave mask in xmm1

        ; xmm1 = {(|a| != +inf ? |a| : 0, |b| != 0.0 ? |b| : 0}
        andpd xmm1, xmm2

        ; xmm2 = {|a| != +inf ? 0 : QNaN, |b| != 0.0 ? 0 : QNaN}
%ifndef ASM_J9VM_USE_GOT
        andnpd xmm2, [rel  QNaN] ;qword
%else
        andnpd xmm2, QWORD  QNaN@GOTOFF[rdx]
%endif
        ; xmm1 = {|a| != +inf ? |a| : QNaN, |b| != 0.0 ? |b| : QNaN}
        orpd xmm1, xmm2
        movapd xmm2, xmm1                        ; xmm2 = xmm1
        shufpd xmm2, xmm2, 1                     ; swap elems in xmm2 so |a| (or QNaN) in xmm2

        add     rsp, 8
        ucomisd xmm2, xmm1
        movq    xmm2, qword [rsp -8]             ; for scheduling reasons updating of the SP was done 2 intrs above
%ifndef ASM_J9VM_USE_GOT
        jae short _dblRemain                     ; Do normal algorithm
%else
        jae short JMP_DBLREMAIN                  ; Do normal algorithm
%endif
        jnp short RETURN_DIVIDEND

RETURN_NAN:
%ifndef ASM_J9VM_USE_GOT
        movapd xmm0, [rel QNaN]                  ; xmm0 = QNaN
%else
        movapd xmm0, QWORD  QNaN@GOTOFF[rdx]     ; xmm0 = QNaN
%endif
RETURN_DIVIDEND:                                 ; dividend already in xmm0
%ifdef ASM_J9VM_USE_GOT
        pop     rdx
%endif
        nop
        ret

%ifdef ASM_J9VM_USE_GOT
JMP_DBLREMAIN:
        pop     rdx
        jmp short _dblRemain                     ; Do normal algorithm
%endif
        ret

        align 16
_dblRemain:
        ; Prolog Start
        push rax
        push rbx
        push rcx
%ifdef ASM_J9VM_USE_GOT
        push rdx
        LoadGOTInto rdx
%endif
        sub rsp, 48                              ; rsp = rsp - 48(bytes)
        movq QWORD  [rsp+16], xmm2               ; preserve xmm2
        ; Prolog End
%ifndef ASM_J9VM_USE_GOT
        andpd xmm1, [rel ABSMASK]                ; xmm1 = |divisor|
%else
        andpd xmm1, QWORD  ABSMASK@GOTOFF[rdx]   ; xmm1 = |divisor|
%endif
        movq QWORD  [rsp], xmm0                  ; store dividend on stack
        movq QWORD  [rsp+8], xmm1                ; store |divisor| on stack
        mov eax, DWORD  [rsp+12]                 ; eax = high 32 bits of |divisor|
        and eax, 7ff00000h                       ; eax = exponent of |divisor|
        shr eax, 20
        cmp eax, 2                               ; ppc uses 0x0001 and does bgt L144
        jb SMALL_NUMS                            ; we do < 2 and jb SMALL_NUMS
L144:                                            ; and fall through to the normal case

        ; Anything greater than MAGIC_NUM1 is considered "large"
        ; compare |dividend| to MAGIC_NUM1
%ifndef ASM_J9VM_USE_GOT
        ucomisd xmm1, [rel MAGIC_NUM1]
%else
        ucomisd xmm1, QWORD  MAGIC_NUM1@GOTOFF[rdx]
%endif
        jae LARGE_NUMS                           ; if xmm1 >= 0x7fde42d13077b76

L184:
        movq  xmm2, xmm1                         ; xmm2 = xmm1
        addsd xmm2, xmm1                         ; xmm2 = xmm1 + xmm1 (2 * |divisor|)
        movq  QWORD  [rsp+32], xmm2              ; store xmm2 on the stack
        mov eax, DWORD  [rsp+4]                  ; eax = high 32 bits of dividend

        movq  xmm4, xmm0                         ; xmm4 = xmm0 (dividend)
%ifndef ASM_J9VM_USE_GOT
        andpd xmm4, [rel ABSMASK]                ; xmm4 = |dividend|
%else
        andpd xmm4, QWORD  ABSMASK@GOTOFF[rdx]   ; xmm4 = |dividend|
%endif
        movq QWORD  [rsp], xmm4                  ; store xmm4 on the stack
        mov ebx, eax                             ; ebx = eba
        and ebx, 80000000h                       ; ebx = sign bit of dividend

        ucomisd xmm4, xmm2                       ; compare xmm4 with xmm2
        jna L234                                 ; if xmm4 <= xmm2 goto L234

        movq QWORD  [rsp+40], xmm2               ; store xmm2 on the stack
        mov ecx, DWORD  [rsp+36]                 ; ms 32bits of |dividend| (was xmm4)
        sub eax, ecx                             ; eax = eax - ecx
        and eax, 7ff00000h                       ; get exponent (rlwinm r0,r0,0,1,11)
        mov ecx, DWORD  [rsp+44]
        movq xmm2, QWORD  [rsp]                  ; save xmm2 to stack
        add ecx, eax                             ; ecx += eax
        mov eax, ecx                             ; eax = ecx (2 results)
        add eax, 0fff00000h                      ; addis r0,r4,-16
        mov DWORD  [rsp+44], ecx                 ; save ecx to stack
        movq xmm0, QWORD  [rsp+40]               ; store xmm0 to stack
L1d8:
        ucomisd xmm2, xmm0                       ; moved this instruction down from ppc
        jae short L1e8                           ; if xmm2 >= xmm0 goto L1e8
        mov DWORD  [rsp+44], eax
        movq  xmm2, QWORD  [rsp]
        movq  xmm0, QWORD  [rsp+40]
L1e8:
        movq  xmm4, xmm2                         ; xmm4 = xmm2
        subsd xmm4, xmm0                         ; xmm4 = xmm4 - xmm0
        movq  QWORD  [rsp], xmm4                 ; save xmm4
        movq  xmm2, QWORD  [rsp+32]              ; load xmm2 from stack
        ucomisd xmm4, xmm2                       ; compare xmm4 with xmm2
        jna short L230                           ; if xmm4 <= xmm2 goto L230
        mov eax, DWORD  [rsp+4]                  ; eax = ms 32bits of dividend stack area
        mov ecx, DWORD  [rsp+36]
        movq QWORD  [rsp+40], xmm2               ; store xmm2 on stack
        sub eax, ecx                             ;  (subf r0,r4,r0)
        mov ecx, DWORD  [rsp+44]
        and eax, 7ff00000h                       ; eax = exponent of |divisor|
        add ecx, eax                             ; ecx += eax
        mov eax, ecx
        add eax, 0fff00000h                      ; addis r0,r4,-16
        mov DWORD  [rsp+44], ecx                 ; save ecx to stack
        movq xmm2, QWORD  [rsp]
        movq xmm0, QWORD  [rsp+40]
        jmp short L1d8

L230:
        movq    xmm1, QWORD  [rsp+8]
L234:
        ucomisd xmm4, xmm1
        xorpd   xmm2, xmm2                       ; xmm2 = {0.0, 0.0} (only need scalar part)
        jb short L258
        subsd xmm4, xmm1
        ucomisd xmm4, xmm1
        movq    QWORD  [rsp], xmm4
        jb L258
        subsd xmm4, xmm1
        movq  QWORD  [rsp], xmm4

L258:
        ucomisd xmm2, xmm4
        mov eax, DWORD  [rsp+4]                  ; ms 32bits of dividend
        jne short L280
        or eax, ebx                              ; restore sign of result
        mov DWORD  [rsp+4], eax                  ; put result back on stack
        movq xmm0, QWORD  [rsp]                  ; load result into xmm0

        ; Epilog Start
        movq xmm2, QWORD  [rsp+16]               ; restore xmm2
        add rsp, 48
%ifdef ASM_J9VM_USE_GOT
        pop rdx
%endif
        pop rcx
        pop rbx
        pop rax
        ret                                      ; Epilog End
L280:
        xor eax, ebx
        mov DWORD  [rsp+4], eax
        movq xmm0, QWORD  [rsp]                  ; load result into xmm0

        ; Epilog Start
        movq xmm2, QWORD  [rsp+16]               ; restore xmm2
        add rsp, 48
%ifdef ASM_J9VM_USE_GOT
        pop rdx
%endif
        pop rcx
        pop rbx
        pop rax
        ret                                      ; Epilog End

SMALL_NUMS:
%ifndef ASM_J9VM_USE_GOT
        mulsd xmm1,  [rel SCALEUP_NUM]                  ; xmm1 = |divisor| * 2^54
%else
        mulsd xmm1, QWORD  SCALEUP_NUM@GOTOFF[rdx]      ; xmm1 = |divisor| * 2^54
%endif
        movq  QWORD  [rsp+8], xmm1                      ; store xmm1 on the stack
        call _dblRemain

%ifndef ASM_J9VM_USE_GOT
        mulsd xmm0,  [rel SCALEUP_NUM]                  ; xmm0 = dividend * 2^54
%else
        mulsd xmm0, QWORD  SCALEUP_NUM@GOTOFF[rdx]      ; xmm0 = dividend * 2^54
%endif
        movq  xmm1, QWORD  [rsp+8]                      ; load divisor from the stack
        movq  QWORD  [rsp], xmm0                        ; store dividend on the stack
        call _dblRemain

%ifndef ASM_J9VM_USE_GOT
        mulsd xmm0,  [rel SCALEDOWN_NUM]                ; dividend * 1/2^54
%else
        mulsd xmm0, QWORD  SCALEDOWN_NUM@GOTOFF[rdx]    ; dividend * 1/2^54
%endif

        ; Epilog Start
        movq xmm2, QWORD  [rsp+16]                      ; restore xmm2
        add rsp, 48
%ifdef ASM_J9VM_USE_GOT
        pop rdx
%endif
        pop rcx
        pop rbx
        pop rax
        ret                                             ; Epilog End

LARGE_NUMS:
%ifndef ASM_J9VM_USE_GOT
        mulsd xmm1,  [rel ONEHALF]                      ; xmm1 *= 0.5 (scaledown)
%else
        mulsd xmm1, QWORD  ONEHALF@GOTOFF[rdx]          ; xmm1 *= 0.5 (scaledown)
%endif
        ; store xmm1 in divisor slot on stack
        movq  QWORD  [rsp+8], xmm1
%ifndef ASM_J9VM_USE_GOT
        mulsd xmm0,  [rel ONEHALF]                      ; xmm2 *= 0.5 (scaledown)
%else
        mulsd xmm0, QWORD  ONEHALF@GOTOFF[rdx]          ; xmm2 *= 0.5 (scaledown)
%endif
        movq     QWORD  [rsp], xmm0                     ; store xmm0 in dividend slot on stack
        call _dblRemain

        addsd xmm0, xmm0                                ; xmm0 *= 2 (scaleup)

        ; Epilog Start
        movq     xmm2, QWORD  [rsp+16]                  ; restore xmm2
        add rsp, 48
%ifdef ASM_J9VM_USE_GOT
        pop rdx
%endif
        pop rcx
        pop rbx
        pop rax
        ret                                             ; Epilog End

%ifndef TR_HOST_64BIT
; in:   32-bit slot on stack - double to convert
; out:  edx, eax - resulting long
; uses: eax, ebx, ecx, edx, esi
;
; Note that the double is assumed to have an exponent >= 32.

        align 16
_SSEdouble2LongIA32:
        mov edx, dword [rsp+8]      ; load the double into edx:eax
        mov eax, dword [rsp+4]

        push esi                    ; save esi as private linkage
        push ecx                    ; save ecx as private linkage
        push ebx                    ; save ebx as private linkage

        mov ecx, edx                ; extract the biased exponent E of the double into cx
        shr ecx, 20
        and cx, 07ffh

        mov esi, edx                ; set esi if the double is negative
        shr esi, 31

        and edx, 000fffffh          ; clear out the sign bit and the 11-bit exponent field of the double number
        or edx, 00100000h           ; add the implicit leading 1 to the mantissa of the double

        cmp cx, 1075
        jae SSEd2l_convertLarge     ; if exponent is >= 52, we need to scale up the mantissa

; if E < 1075 (i.e. the unbiased exponent is >= 32 and < 52),
; then 1 or more low bits of the 52-bit mantissa must be truncated,
; since those bits represent the fraction

        neg cx                      ; cl = cx = 52 - exponent
        add cx, 1075

        mov ebx, edx                ; shift the mantissa (edx:eax) right by cl; this removes the fraction from the double
        shr edx, cl
        shr eax, cl

        neg cl                      ; cl = 32 - cl
        add cl, 32
        shl ebx, cl                 ; shift the lower bits of edx into the upper end of eax
        or eax, ebx

        jmp SSEd2l_doneConversion   ; done

SSEd2l_convertLarge:
        cmp cx, 1086
        jae SSEd2l_convertSpecial   ; if exponent is >= 63, the value is too large for a long

; if E < 1086 (i.e. the unbiased exponent is >= 52 and < 63),
; then the long can be computed by shifting the mantissa to the left
; by (exponent - 52).

        sub cx, 1075                ; cl = cx = exponent - 52
        je  SSEd2l_doneConversion   ; if exponent == 52, no shifting is needed

        mov ebx, eax                ; shift the mantissa (edx:eax) left by cl; the result is the desired long
        shl edx, cl
        shl eax, cl

        neg cl                      ; cl = 32 - cl
        add cl, 32
        shr ebx, cl                 ; shift the upper bits eax into the lower end of edx
        or edx, ebx

SSEd2l_doneConversion:
        test esi, esi
        jne SSEd2l_negateLong
        pop ebx                     ; restore ebx as private linkage
        pop ecx                     ; restore ecx as private linkage
        pop esi                     ; restore esi as private linkage
        ret 8

SSEd2l_negateLong:
        not edx
        neg eax
        sbb edx, -1
        pop ebx                     ; restore ebx as private linkage
        pop ecx                     ; restore ecx as private linkage
        pop esi                     ; restore esi as private linkage
        ret 8

SSEd2l_convertSpecial:              ; perform conversions for special numbers (very large values, infinities, NaNs)
        cmp cx, 7ffh                ; if the exponent is all 1's then the double is either an infinity or a NaN
        je SSEd2l_NaNOrInfinity

SSEd2l_maxLong:                     ; if double is simply a very large number (exponent >= 63 and < 1023)
        test esi, esi               ; if the number is positive then return (2^63 - 1), else return -(2^63)
        jne SSEd2l_minLong
        or eax, -1
        mov edx, 7fffffffh
        pop ebx                     ; restore ebx as private linkage
        pop ecx                     ; restore ecx as private linkage
        pop esi                     ; restore esi as private linkage
        ret 8

SSEd2l_minLong:
        xor eax, eax
        mov edx, 80000000h
        pop ebx                     ; restore ebx as private linkage
        pop ecx                     ; restore ecx as private linkage
        pop esi                     ; restore esi as private linkage
        ret 8

SSEd2l_NaNOrInfinity:               ; if the double is either an infinity or Nan
        test eax, eax               ; if the mantissa is 0, the double is an infinity; return a max or min long
        jne SSEd2l_NaN
        cmp edx, 00100000h          ; mantissa is 0, but don't forget we added a leading 1, hence 00100000h
        jne SSEd2l_NaN
        jmp SSEd2l_maxLong

SSEd2l_NaN:                         ; if the number is a NaN, return 0 (note: no exception flags are set if it is a QNaN)
        xor edx, edx
        xor eax, eax
        pop ebx                     ; restore ebx as private linkage
        pop ecx                     ; restore ecx as private linkage
        pop esi                     ; restore esi as private linkage
        ret 8

%endif

_jitFPHelpersEnd:


eq_J9VMThread_heapAlloc      equ J9TR_VMThread_heapAlloc
eq_J9VMThread_heapTop        equ J9TR_VMThread_heapTop
eq_J9VMThread_PrefetchCursor equ J9TR_VMThread_tlhPrefetchFTA

        global _prefetchTLH
        global _newPrefetchTLH

%ifdef ASM_J9VM_GC_TLH_PREFETCH_FTA

        align 16
_prefetchTLH:
        push  rcx

%ifdef TR_HOST_64BIT
        mov   rcx, qword [rbp + eq_J9VMThread_heapAlloc]
%else
        mov   rcx, dword [rbp + eq_J9VMThread_heapAlloc]
%endif
        prefetchnta byte [rcx+256]
        prefetchnta byte [rcx+320]
        prefetchnta byte [rcx+384]
        prefetchnta byte [rcx+448]
        prefetchnta byte [rcx+512]
        prefetchnta byte [rcx+576]
        prefetchnta byte [rcx+640]
        prefetchnta byte [rcx+704]
        prefetchnta byte [rcx+768]

%ifdef TR_HOST_64BIT
        mov   qword [rbp + eq_J9VMThread_PrefetchCursor], 384
%else
        mov   dword [rbp + eq_J9VMThread_PrefetchCursor], 384
%endif
prefetch_done:
        pop   rcx
        ret


; Linkage:
;
; rax = start of object just allocated
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
_newPrefetchTLH:

        push        rcx        ; preserve
        push        rdx        ; preserve
        push        rsi        ; preserve

%ifdef TR_HOST_64BIT
        mov         rdx, qword [rbp + eq_J9VMThread_heapAlloc]       ; rdx = O+S
        mov         ecx, dword [rbp + eq_J9VMThread_PrefetchCursor]  ; rcx = current trigger boundary (B)
%else
        mov         rdx, dword [rbp + eq_J9VMThread_heapAlloc]       ; rdx = O+S
        mov         rcx, dword [rbp + eq_J9VMThread_PrefetchCursor]  ; rcx = current trigger boundary (B)
%endif

        ; A zero value for the trigger boundary indicates that this is a new TLH
        ; that we haven't prefetched from yet.
        ;
        test        rcx, rcx
        jz prefetchFromNewTLH

        lea         rcx, [rcx + eq_prefetchTriggerDistance]     ; rcx = derive prefetch frontier (F)
                                                                ;    where F = B + prefetchTriggerDistance
        lea         rsi, [rcx +64]                              ; rsi = prefetch frontier (F) + 1 cache line
                                                                ;    +1 = start at the next cache line after the frontier

        ; Push the new frontier out even further if we're allocating a large object that
        ; exceeds the current frontier.
        ;
        cmp         rcx, rdx
        cmovl       rcx, rdx                                    ; rcx = max(F, O+S)
        add         rcx, eq_prefetchWindow                      ; rcx = F' = F+eq_prefetchWindow = new prefetch frontier (F')

%ifdef TR_HOST_64BIT
        mov         rdx, qword [rbp + eq_J9VMThread_heapTop]
%else
        mov         rdx, dword [rbp + eq_J9VMThread_heapTop]
%endif

        cmp         rcx, rdx                                    ; F' >= heapTop?
        jae prefetchFinalFrontier

mergePrefetchTLHRoundDown:
        and         rcx, -64                                    ; round down to cache line

mergePrefetchTLH:
      ; Prefetch all lines between the last prefetch frontier (F) and the new one (F').
      ; Prefetch forward to give the data a greater chance of being there when we need it.
      ;
      ; rsi = last frontier (F)
      ; rcx = next frontier (F')

      ; Since interpreter allocations do not advance the prefetch frontier the last frontier
      ; might be behind the allocation pointer, in which case we want to move it forward so
      ; that we don't prefetch data that has already been allocated.
      ;
        cmp         rsi, rax
        cmovl       rsi, rax                                    ; rsi = max(F+64, O)

        sub         rsi, rcx
doPrefetch:
        prefetchnta [rcx+rsi]
        add         rsi, 64
        jle short doPrefetch

        sub         rcx, eq_prefetchTriggerDistance             ; rcx = new trigger boundary (B)

%ifdef TR_HOST_64BIT
        mov         dword [rbp + eq_J9VMThread_PrefetchCursor], ecx
%else
        mov         dword [rbp + eq_J9VMThread_PrefetchCursor], rcx
%endif
        pop         rsi
        pop         rdx
        pop         rcx
        ret

prefetchFinalFrontier:
        mov         rcx, rdx
        jmp mergePrefetchTLH


prefetchFromNewTLH:
      ; We have a new TLH that has not been prefetched.  Assume that the object
      ; just allocated will have already been pulled into the cache.  Begin
      ;
      ; Prefetch from O through O+initialPrefetchWindow
      ; if (O+i <= O+s) prefetch to
      ;
        mov         rsi, rdx                                   ; rsi = A
        lea         rcx, [rdx + eq_initialPrefetchWindow]      ; rcx = A + Wi

%ifdef TR_HOST_64BIT
        mov         rdx, qword [rbp + eq_J9VMThread_heapTop]
%else
        mov         rdx, dword [rbp + eq_J9VMThread_heapTop]
%endif
        cmp         rcx, rdx
        jae mergePrefetchTLH

        jmp mergePrefetchTLHRoundDown

ret


%else  ; ASM_J9VM_GC_TLH_PREFETCH_FTA

      align 16
_prefetchTLH:
        ret

      align 16
_newPrefetchTLH:
        ret

%endif  ; REALTIME

      global   _jitReleaseVMAccess

%ifndef TR_HOST_64BIT

; --------------------------------------------------------------------------------
;
;                                    32-BIT
;
; --------------------------------------------------------------------------------

      global   _clearFPStack

_jitReleaseVMAccess:
                pusha
                push       ebp
                mov        eax, [ebp+J9TR_VMThread_javaVM]
                mov        eax, [eax+J9TR_JavaVMInternalFunctionTable]
%ifdef ASM_J9VM_INTERP_ATOMIC_FREE_JNI
                call       [eax+J9TR_InternalFunctionTableExitVMToJNI]
%else
                call       [eax+J9TR_InternalFunctionTableReleaseVMAccess]
%endif
                add        esp,4
                popa
                retn

	; Wipe the entire FP stack without raising any exceptions.
	;
		align 16
_clearFPStack:
		ffree     st0
		ffree     st1
		ffree     st2
		ffree     st3
		ffree     st4
		ffree     st5
		ffree     st6
		ffree     st7
		retn

%else

; --------------------------------------------------------------------------------
;
;                                    64-BIT
;
; --------------------------------------------------------------------------------

_jitReleaseVMAccess:
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
        movq    qword  [rsp+0],   xmm0
        movq    qword  [rsp+8],   xmm1
        movq    qword  [rsp+16],  xmm2
        movq    qword  [rsp+24],  xmm3
        movq    qword  [rsp+32],  xmm4
        movq    qword  [rsp+40],  xmm5
        movq    qword  [rsp+48],  xmm6
        movq    qword  [rsp+56],  xmm7
        movq    qword  [rsp+64],  xmm8
        movq    qword  [rsp+72],  xmm9
        movq    qword  [rsp+80],  xmm10
        movq    qword  [rsp+88],  xmm11
        movq    qword  [rsp+96],  xmm12
        movq    qword  [rsp+104], xmm13
        movq    qword  [rsp+112], xmm14
        movq    qword  [rsp+120], xmm15

        ; Call vmThread->internalFunctionTable->releaseVMAccess(rbp)
%ifdef WINDOWS
        mov     rcx, rbp
%else
        mov     rdi, rbp
%endif
        mov     rax, qword [rbp+J9TR_VMThread_javaVM]
        mov     rax, qword [rax+J9TR_JavaVMInternalFunctionTable]
%ifdef ASM_J9VM_INTERP_ATOMIC_FREE_JNI
        call    qword  [rax+J9TR_InternalFunctionTableExitVMToJNI]
%else
        call    qword  [rax+J9TR_InternalFunctionTableReleaseVMAccess]
%endif

        ; Restore registers
        ; XMMs
        movq    xmm0,  qword  [rsp+0]
        movq    xmm1,  qword  [rsp+8]
        movq    xmm2,  qword  [rsp+16]
        movq    xmm3,  qword  [rsp+24]
        movq    xmm4,  qword  [rsp+32]
        movq    xmm5,  qword  [rsp+40]
        movq    xmm6,  qword  [rsp+48]
        movq    xmm7,  qword  [rsp+56]
        movq    xmm8,  qword  [rsp+64]
        movq    xmm9,  qword  [rsp+72]
        movq    xmm10, qword  [rsp+80]
        movq    xmm11, qword  [rsp+88]
        movq    xmm12, qword  [rsp+96]
        movq    xmm13, qword  [rsp+104]
        movq    xmm14, qword  [rsp+112]
        movq    xmm15, qword  [rsp+120]
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

%endif ; 64-bit
