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
	
      
        %include "jilconsts.inc"

segment .text

        DECLARE_GLOBAL longDivide
        DECLARE_GLOBAL longRemainder
        DECLARE_GLOBAL jitMathHelpersDivideBegin
        DECLARE_GLOBAL jitMathHelpersDivideEnd
        DECLARE_GLOBAL jitMathHelpersRemainderBegin
        DECLARE_GLOBAL jitMathHelpersRemainderEnd
;
      align    16
longRemainder:
jitMathHelpersRemainderBegin:
      ; divides two signed 64-bit numbers and returns the remainder.
      ;
      ; In: [ESP+8]:[ESP+4] = dividend
      ; [ESP+16]:[ESP+12] = divisor
      ;
      ; OUTPUT: EDX:EAX = remainder of division

      push    ebx                                 ; scratch registers
      push    ecx                                 ; ebx, ecx, edi, esi
      push    edi                                 ; esi is preserved later
      
      mov     ecx, dword  [esp+28]             ; divisor-hi
      mov     ebx, dword  [esp+24]             ; divisor-lo
      mov     edx, ecx                            ; check to see if we are 
      or      edx, ebx                            ; going to throw div/0            
      jz      zero_remainder                      ; exception
      mov     edx, dword  [esp+20]             ; dividend-hi
      mov     eax, dword  [esp+16]             ; dividend-lo
                                                        
      cmp     edx, -1                             ; check the exception case MIN_INT/-1
      jz      short high_dividend_neg_one         ; handle that separately

      test    eax, eax                            ; and other simple cases 
      jnz     short perform_remainder             ; when dividing with -1
      cmp     edx, 080000000h
      jnz     short perform_remainder
      mov     eax, ecx
      and     eax, ebx
      not     eax
      test    eax, eax
      mov     eax, 0h
      jnz     short perform_remainder
      jmp     short return_eax
      
high_dividend_neg_one:                        
      cmp     eax, 080000000h           
      jb      short perform_remainder
      cmp     ebx, -1
      jnz     short perform_remainder
      cmp     ecx, -1
      jnz     short perform_remainder
      xor     eax, eax
      
return_eax:
      cdq                                         ; extend the sign
      pop     edi                                 ; restore the scratch registers
      pop     ecx                                 ; edi, ecx, ebx
      pop     ebx
      ret     16
            
perform_remainder:                                ; main remainder entry point
      push    esi                                 ; the VM assumes 3 slots for exception handling
                                                  ; saving the 4th scratch register (esi) now when we
                                                  ; can't throw exception
      
      mov     esi, edx                            ; the sign of the remainder is the sign of the dividend
      sar     esi, 31                             ; save that value in esi
      xor     eax, esi                            ; if dividend is < 0
      xor     edx, esi                            ; compute 1's complement of dividend
      sub     eax, esi                            ; if dividend is < 0
      sbb     edx, esi                            ; compute 2's complement of dividend
      mov     edi, ecx                            ; compute the sign of the divisor
      sar     edi, 31                             ; save that in edi
      xor     ebx, edi                            ; if divisor is < 0
      xor     ecx, edi                            ; compute 1's complement of divisor
      sub     ebx, edi                            ; if divisor is < 0
      sbb     ecx, edi                            ; compute 2's complement of divisor
      
      jnz     long_long_rem                       ; if divisor is greater than 32bit value do long remainder
      
      cmp     edx, ebx                            ; Is this remainder with integer values
      jae     long_int_rem                        ; No, we have long remainder with integer divisor
                                                  ; Fallthrough, Yes, we can live with one division only Int%Int
      div     ebx                                 ; EAX = quotient low
      mov     eax, edx                            ; EAX = remainder low
      mov     edx, ecx                                
      xor     eax, esi                            ; Use the remembered sign in esi
      xor     edx, esi                            ; to set the sign of the remainder
      sub     eax, esi                            ; by computing 1's and 2's complement
      sbb     edx, esi                                
      
      pop     esi                                 ; restore the scratch registers
      pop     edi                                 ; edi, ecx, ebx, esi
      pop     ecx 
      pop     ebx 
      
      ret     16
      
long_int_rem:
      mov     ecx, eax                            ; spill divident low in ecx, we'll do that div later
      mov     eax, edx                            ; setup division of the high word 
      xor     edx, edx                            ; zero extend it into edx
      div     ebx                                 ; eax = quotient high, edx = intermediate result
      mov     eax, ecx                            ; restore divident low, edx to be used from the previous div
      div     ebx                                 ; eax = quotient low, edx = remainder low
      mov     eax, edx                            ; copy the result into eax, and zero out the high result
      xor     edx, edx                            ; remainder high = 0
      jmp     sign_rem                            ; make remainder signed based on sign registers esi, edi

long_long_rem:
                                                  ; test for ugly edge cases that cause overflow
      cmp     edx, ecx
      jbe     test_for_edge_cases

      cmp     eax, 0ffffffffh                     ; test for MAX_LONG divident
      jne     long_long_rem_begin
      cmp     edx, 07fffffffh
      je      very_slow_path                      ; if it's MAX_LONG do the slow loop
      
long_long_rem_begin:                              ; the following algorithm is based on the
                                                  ; AMD opteron optimization guide

      sub     esp, 16                             ; make room for four spills
      mov     dword  [esp], eax                ; save dividend low
      mov     dword  [esp+4], ebx              ; save divisor low
      mov     dword  [esp+8], edx              ; save dividend high
      mov     dword  [esp+12], ecx             ; save divisor high
      mov     edi, ecx                            ; spill divisor high
      shr     edx, 1                              ; shift right by 1 both
      rcr     eax, 1                              ; divisor and
      ror     edi, 1                              ; and dividend
      rcr     ebx, 1                     
      bsr     ecx, ecx                            ; ecx = number of remaining shifts
      shrd    ebx, edi, cl                        ; scale down divisor and
      shrd    eax, edx, cl                        ; dividend such that divisor is
      shr     edx, cl                             ; less than 2^32 (that is, fits in EBX)
      rol     edi, 1                              ; restore original divisor high
      div     ebx                                 ; compute quotient
      mov     ebx, dword  [esp]                ; restore dividend low
      mov     ecx, eax                            ; save quotient
      imul    edi, eax                            ; quotient * divisor high word (low only)
      mul     dword  [esp+4]                   ; quotient * divisor low word
      add     edx, edi                            ; edx:eax = quotient * divisor
      sub     ebx, eax                            ; dividend low - (quotient * divisor low)
      mov     ecx, dword  [esp+8]              ; dividend high
      sbb     ecx, edx                            ; subtract divisor * quotient from dividend.
      sbb     eax, eax                            ; remainder < 0 ? 0xffffffff : 0
      mov     edx, dword  [esp+12]             ; divisor high
      and     edx, eax                            ; remainder < 0 ? divisor high : 0
      and     eax, dword  [esp+4]              ; remainder < 0 ? divisor low : 0
      add     eax, ebx                            ; remainder low
      adc     edx, ecx                            ; remainder high
      add     esp, 16                             ; restore stack shape

sign_rem:

      xor     eax, esi                            ; if remainder is < 0,
      xor     edx, esi                            ; compute 1's complement of result.
      sub     eax, esi                            ; if remainder is < 0,
      sbb     edx, esi                            ; compute 2's complement of result.
      
exit_rem_4regs:      
      pop     esi                                 ; restore the scratch registers
      pop     edi                                 ; edi, ecx, ebx, esi
      pop     ecx
      pop     ebx

      ret     16

zero_remainder:
      div     edx                                 ; cause div / 0 exception
      
      pop     edi                                 ; restore the scratch registers
      pop     ecx                                 ; edi, ecx, ebx
      pop     ebx
      ret     16
      
test_for_edge_cases:
                                                  ; divident highword is smaller or equal to divisor highword
      jb      short long_remainder_end            ; we are done very quickly if divident is smaller than divisor
      cmp     eax, ebx                            ; divident highword is the same as divisor highword
      jb      short long_remainder_end            ; check if divident is smaller than divisor using low word
      ja      short long_long_rem_begin           ; divisor is smaller than divident go divide
      xor     edx, edx                            ; divident and divisor are equal
      xor     eax, eax                            ; remainder is 0
long_remainder_end:

      test    esi, esi                            ; divident is smaller than divisor
      jz      short exit_rem_4regs                ; remainder is the divident, just make sure we set the sign right
      neg     eax
      adc     edx, 0
      neg     edx
      jmp     short exit_rem_4regs                ; done making the sign, return


very_slow_path:

      push    ebp                                 ; save some extra registers
      push    esi                                 ; ebp and the two sign spill registers
      push    edi
      
      xor     esi, esi
      xor     ebp, ebp
      mov     edi, ebx
      mov     bl, 40h
      
long_rem_loop:
      shl     eax, 1
      rcl     edx, 1
      rcl     esi, 1
      rcl     ebp, 1
      cmp     ebp, ecx
      ja      short lrem_div_too_big
      jb      short lrem_next_iter
      cmp     esi, edi
      jb      short lrem_next_iter
       
lrem_div_too_big:
      sub     esi, edi
      sbb     ebp, ecx
       
lrem_next_iter:
      dec     bl
      jnz     short long_rem_loop
      mov     eax, esi
      mov     edx, ebp
      
lrem_path_end:
      pop     edi
      pop     esi
      pop     ebp                                 ; restore register state and jump
      jmp     sign_rem                            ; to set the sign
                  
jitMathHelpersRemainderEnd:
ret


;
; longDivide divides two signed 64-bit numbers and delivers the quotient
;
; INPUT: [ESP+8]:[ESP+4] dividend
; [ESP+16]:[ESP+12] divisor
;
; OUTPUT: EDX:EAX quotient of division

       align 16
longDivide:
jitMathHelpersDivideBegin:
	mov edx, dword [esp + 12]
	or  edx, dword [esp + 16]
	jz  _zero_divisor
	push ebx 				; save ebx as per private linkage
	push esi 				; save esi as per private linkage
	push ecx 				; save ecx as per private linkage
	push edi 				; save edi as per private linkage
	mov ecx, [esp+32] 			; divisor-hi
	mov ebx, [esp+28] 			; divisor-lo
	mov edx, [esp+24] 			; dividend-hi
	mov eax, [esp+20] 			; dividend-lo

	mov esi, ecx 				;divisor-hi
	xor esi, edx 				;divisor-hi ^ dividend-hi
	sar esi, 31 				;(quotient < 0) ? -1 : 0
	mov edi, edx 				;dividend-hi
	sar edi, 31 				;(dividend < 0) ? -1 : 0
	xor eax, edi 				;if (dividend < 0)
	xor edx, edi 				;compute 1's complement of dividend
	sub eax, edi 				;if (dividend < 0)
	sbb edx, edi 				;compute 2's complement of dividend
	mov edi, ecx 				;divisor-hi
	sar edi, 31 				;(divisor < 0) ? -1 : 0
	xor ebx, edi 				;if (divisor < 0)
	xor ecx, edi 				;compute 1's complement of divisor
	sub ebx, edi 				;if (divisor < 0)
	sbb ecx, edi 				; compute 2's complement of divisor
	jnz _big_divisor 			; divisor > 2^32-1

	cmp edx, ebx 				;only one division needed ? (ecx = 0)
	jae _two_divs 				;need two divisions
	
	div ebx 				;eax = quotient-lo
	mov edx, ecx 				;edx = quotient-hi = 0
	; (quotient in edx:eax)
	xor eax, esi 				;if (quotient < 0)
	xor edx, esi 				;compute 1's complement of result
	sub eax, esi 				;if (quotient < 0)
	sbb edx, esi 				;compute 2's complement of result
	pop edi 				; restore edi as per private linkage
	pop ecx 				; restore ecx as per private linkage
	pop esi 				; restore esi as per private linkage
	pop ebx 				; restore ebx as per private linkage
	ret 16					; return
_two_divs:
	mov ecx, eax 				;save dividend-lo in ecx
	mov eax, edx 				;get dividend-hi
	
	xor edx, edx 				;zero extend it into edx:eax
	div ebx 				;quotient-hi in eax
	xchg eax, ecx 				;ecx = quotient-hi, eax = dividend-lo
	div ebx 				;eax = quotient-lo
	mov edx, ecx 				;edx = quotient-hi
	; (quotient in edx:eax)
	jmp _make_sign 				;make quotient signed

_big_divisor:
	sub esp, 12 				;create three local variables
	mov [esp], eax 				;dividend-lo
	mov [esp+4], ebx 			;divisor-lo
	mov [esp+8], edx 			;dividend-hi
	mov edi, ecx 				;save divisor-hi
	shr edx, 1 				;shift both
	rcr eax, 1 				;divisor and
	ror edi, 1 				;and dividend
	rcr ebx, 1 				;right by 1 bit
	bsr ecx, ecx 				;ecx = number of remaining shifts
	shrd ebx, edi, cl 			;scale down divisor and
	shrd eax, edx, cl 			;dividend such that divisor
	shr edx, cl 				;less than 2^32 (i.e. fits in ebx)
	rol edi, 1 				;restore original divisor-hi
	test eax, eax				;for edge cases
	jz _slow_path				;goto the slow path
	div ebx 				;compute quotient
	mov ebx, [esp] 				;dividend-lo
	mov ecx, eax 				;save quotient
	imul edi, eax 				;quotient * divisor hi-word (low only)
	mul dword  [esp+4] 			;quotient * divisor lo-word
	add edx, edi 				;edx:eax = quotient * divisor
	sub ebx, eax 				;dividend-lo - (quot.*divisor)-lo
	mov eax, ecx 				;get quotient
	mov ecx, [esp+8] 			;dividend-hi
	sbb ecx, edx 				;subtract divisor * quot. from dividend
	sbb eax, 0 				;adjust quotient if remainder negative
	xor edx, edx 				;clear hi-word of quotient
	add esp, 12 				;remove local variables
	
_make_sign:
	xor eax, esi 				;if (quotient < 0)
	xor edx, esi 				;compute 1's complement of result
	sub eax, esi 				;if (quotient < 0)
	sbb edx, esi 				;compute 2's complement of result
	pop edi 				; restore edi as per private linkage
	pop ecx 				; restore ecx as per private linkage
	pop esi 				; restore esi as per private linkage
	pop ebx 				; restore ebx as per private linkage
	ret 16					; return

_zero_divisor:
	div edx

_slow_path:
	add esp, 12 				; remove local variables and adjust the stack
	pop edi 				; restore edi as per private linkage
	pop ecx 				; restore ecx as per private linkage
	pop esi 				; restore esi as per private linkage
	pop ebx 				; restore ebx as per private linkage
	push     ecx
	fstcw    word [esp]
	mov      word [esp+2], 0f7fH
	fldcw    word [esp+2]
	fild     qword [esp+8]
	fild     qword [esp+16]
	fdivp    st1, st0
	fistp    qword [esp+8]
	fclex
	fldcw    word [esp]
	add      esp, 4
	mov      eax, dword [esp+4]
	mov      edx, dword [esp+8]
	ret      16	

jitMathHelpersDivideEnd:
ret

