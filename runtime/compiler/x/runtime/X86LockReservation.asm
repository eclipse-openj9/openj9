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

include jilconsts.inc
include x/runtime/X86RegisterMap.inc

    public jitMonitorEnterReserved
    public jitMonitorEnterReservedPrimitive
    public jitMonitorEnterPreservingReservation
    public jitMethodMonitorEnterReserved
    public jitMethodMonitorEnterReservedPrimitive
    public jitMethodMonitorEnterPreservingReservation
    public jitMonitorExitReserved
    public jitMonitorExitReservedPrimitive
    public jitMonitorExitPreservingReservation
    public jitMethodMonitorExitPreservingReservation
    public jitMethodMonitorExitReserved
    public jitMethodMonitorExitReservedPrimitive

ifdef WINDOWS
    UseFastCall = 1
else
    ifdef TR_HOST_32BIT
        UseFastCall = 1
    endif
endif

eq_ObjectClassMask            equ -J9TR_RequiredClassAlignment
eq_J9Monitor_IncDecValue      equ 08h
eq_J9Monitor_INFBit           equ 01h
eq_J9Monitor_RESBit           equ 04h
eq_J9Monitor_RESINCBits       equ 0Ch
eq_J9Monitor_FLCINFBits       equ 03h
eq_J9Monitor_RecCountMask     equ 0F8h


ifndef TR_HOST_64BIT

eq_J9Monitor_LockWord         equ 04h
eq_J9Monitor_CountsClearMask  equ 0FFFFFF07h
eq_J9Monitor_CNTFLCClearMask  equ 0FFFFFF05h

else ; ndef  64bit
; this stupidness is required because masm2gas can't handle
; ifdef on definitions

ifdef ASM_J9VM_INTERP_SMALL_MONITOR_SLOT
eq_J9Monitor_LockWord         equ 04h
else
eq_J9Monitor_LockWord         equ 08h
endif

eq_J9Monitor_CountsClearMask  equ 0FFFFFFFFFFFFFF07h
eq_J9Monitor_CNTFLCClearMask  equ 0FFFFFFFFFFFFFF05h

endif ; 64bit

; object <= ObjAddr
; lockword address => _rcx
; lockword value   => _rax
ObtainLockWordHelper macro ObjAddr
    ifdef ASM_J9VM_THR_LOCK_NURSERY
        ifdef ASM_J9VM_INTERP_COMPRESSED_OBJECT_HEADER
            mov  eax, [&ObjAddr + J9TR_J9Object_class] ; receiver class
        else
            mov _rax, [&ObjAddr + J9TR_J9Object_class] ; receiver class
        endif
        and _rax, eq_ObjectClassMask
        mov _rax, [_rax + J9TR_J9Class_lockOffset]     ; offset of lock word in receiver class
        lea _rcx, [&ObjAddr + _rax]                    ; load the address of object lock word
    else
        lea _rcx, [&ObjAddr + eq_J9Monitor_LockWord]   ; load the address of object lock word
    endif
    ifdef ASM_J9VM_INTERP_SMALL_MONITOR_SLOT
        mov  eax, [_rcx]
    else
        mov _rax, [_rcx]
    endif
endm

ObtainLockWord macro
    ifdef UseFastCall
        ObtainLockWordHelper _rdx
    else
        ObtainLockWordHelper _rsi
    endif
endm

; try to obtain the lock
; lockword address <= _rcx
; vmthread         <= _rbp
TryLock macro
    push _rbp
    lea  _rbp, [_rbp + eq_J9Monitor_RESINCBits] ; make thread ID + RES + INC_DEC value

    ; Set _rax to the lock word value that allows a monitor to be reserved (the
    ; reservation bit by default, or 0 for -XlockReservation). This value is
    ; provided to the reserving monent helper as the third argument. Calling
    ; conventions differ...
    ifdef TR_HOST_32BIT
        ; 32-bit always uses fastcall. This is the only argument on the stack.
        ; Stack offsets: +0 saved ebp, +4 return address, +8 argument
        mov eax, dword ptr [esp+8]
    else
        ifdef UseFastCall
            mov rax, r8
        else
            mov rax, rdx
        endif
    endif

    ifdef ASM_J9VM_INTERP_SMALL_MONITOR_SLOT
        lock cmpxchg [_rcx],  ebp               ; try taking the lock
    else
        lock cmpxchg [_rcx], _rbp               ; try taking the lock
    endif
    pop  _rbp
endm

; We reach the reserved monitor enter code here if the monitor isn't reserved, it's reserved
; by another thread or the reservation count has reached it's maximum value.
; In this helper we first check if the reservation count is about to overflow
; then we try taking the lock and if we succeed we increment the reservation count and we
; are back to mainline code.
MonitorEnterReserved macro
    local fallback,trylock
    ObtainLockWord
    push _rax
    and  _rax, eq_J9Monitor_RecCountMask            ; check if the recursive count has
    xor  _rax, eq_J9Monitor_RecCountMask            ; reached the max value
    pop  _rax
    jz   fallback                                   ; and call VM helper to resolve it
    xor  _rax, _rbp                                 ; mask thread ID
    xor  _rax, eq_J9Monitor_RESBit                  ; mask RES bit
    and  _rax, eq_J9Monitor_CountsClearMask         ; clear the count bits
    jnz  trylock                                    ; if any bit is set we don't have it reserved by the same thread, or not reserved at all
    add  dword ptr [_rcx], eq_J9Monitor_IncDecValue ; add 1 to the reservation count
    ifdef TR_HOST_32BIT
        ret 4
    else
        ret
    endif
  trylock:
    TryLock
    jne  fallback                                   ; call out to VM if we couldn't take the lock, which means it's not reserved for us
    ifdef TR_HOST_32BIT
        ret 4
    else
        ret
    endif
  fallback:
endm

; This code is called when we need to exit reserved montior with couple of possible
; scenarios:
;   - more than one level of recursion
;   - FLC or INF are set
;   - we have illegal monitor state
MonitorExitReserved macro
    local fallback
    ObtainLockWord
    test _rax, eq_J9Monitor_RESBit
    jz   fallback
    test _rax, eq_J9Monitor_FLCINFBits
    jnz  fallback
    test _rax, eq_J9Monitor_RecCountMask
    jz   fallback
    sub  dword ptr [_rcx], eq_J9Monitor_IncDecValue
    ret
  fallback:
endm

; We reach the reserved monitor enter code here if the monitor isn't reserved, it's reserved
; by another thread. We try to reserve it for ourselves and if we succeed we just go into
; main line code.
MonitorEnterReservedPrimitive macro
    local fallback,trylock
    ObtainLockWord
    xor  _rax, _rbp                         ; mask thread ID
    xor  _rax, eq_J9Monitor_RESBit          ; mask RES bit
    and  _rax, eq_J9Monitor_CNTFLCClearMask ; clear the count bits
    jnz  trylock                            ; if any bit is set we don't have it reserved by the same thread, or not reserved at all
    ifdef TR_HOST_32BIT
        ret 4
    else
        ret
    endif
  trylock:
    TryLock
    jne  fallback                           ; call out to VM if we couldn't take the lock, which means it's not reserved for us
    ifdef TR_HOST_32BIT
        ret 4
    else
        ret
    endif
  fallback:
endm

; This code is reachable from reserved primitive exit code and it is
; supposed to make sure we call out to monitor exit but with recursive count
; set to 1 if reservation is on. Namely, primitive reserving monitors don't
; increment and decrement reserving counts because the execution cannot be
; stopped inside them. We can get stopped at the exit in which we have to
; make sure we have count set to at least 1, that is pretend we increment/decrement.
; The state of count=0, RES=1, FLC=1 is invalid and illegal monitor state exception is thrown.
MonitorExitReservedPrimitive macro
    local fallback
    ObtainLockWord
    test _rax, eq_J9Monitor_INFBit                   ; check to see if we have inflated monitor
    jnz  fallback                                    ; monitor inflated call the VM helper
    test _rax, eq_J9Monitor_RESBit                   ; check to see if we have reservation ON
    jz   fallback                                    ; no reserved bit set - go on, call the helper to exit
    test _rax, eq_J9Monitor_RecCountMask             ; check to see if we have any recursive bits on
    jnz  fallback                                    ; yes some recursive count, go back to main line code
    sub  dword ptr [_rcx], eq_J9Monitor_IncDecValue
    ret
  fallback:
endm

; We reach this out of line code when we fail to enter monitor with
; flat lock, where the monitor is supposed to preserve existing reservation.
; This enter procedure is different compared to the regular monitor enter
; sequence only that it checks for reservation and enters in reserved manner
; if possible.
MonitorEnterPreservingReservation macro
    local fallback
    ObtainLockWord
    push _rax
    and  _rax, eq_J9Monitor_RecCountMask            ; check if the recursive count has
    xor  _rax, eq_J9Monitor_RecCountMask            ; reached the max value
    pop  _rax
    jz   fallback                                   ; and call VM helper to resolve it
    xor  _rax, _rbp                                 ; mask thread ID
    xor  _rax, eq_J9Monitor_RESBit                  ; mask RES bit
    and  _rax, eq_J9Monitor_CountsClearMask         ; clear the count bits
    jnz  fallback                                   ; if any bit is set we don't have it reserved by the same thread, or not reserved at all
    add  dword ptr [_rcx], eq_J9Monitor_IncDecValue ; add 1 to the reservation count
    ret
  fallback:
endm

; We reach this code when we are in regular monitor and we want to make sure we
; preserve reservation for any monitor that has reserved bit set and we are the
; same thread.
MonitorExitPreservingReservation macro
    local fallback
    ObtainLockWord
    test _rax, eq_J9Monitor_RecCountMask            ; check if the recursive count is greater than 1
    jz   fallback                                   ; branch to VM if 0, weird thing has happened
    xor  _rax, _rbp                                 ; mask thread ID
    xor  _rax, eq_J9Monitor_RESBit                  ; mask RES bit
    and  _rax, eq_J9Monitor_CountsClearMask         ; clear the count bits
    jnz  fallback                                   ; if any bit is set we don't have it reserved by the same thread, or not reserved at all
    sub  dword ptr [_rcx], eq_J9Monitor_IncDecValue
    ret
  fallback:
endm


; template for exported functions
MonitorReservationFunction macro Name, Fallback, Template
    align 16
  ifdef TR_HOST_64BIT
    &Name proc
  else
    &Name proc near
  endif
    &Template
    ifdef UseFastCall
        mov  _rcx, _rbp ; restore the first param - vmthread
    endif
    jmp  &Fallback
    &Name endp
endm

ifdef TR_HOST_64BIT
_TEXT segment para 'CODE'
else
_TEXT segment para use32 public 'CODE'
endif

ifdef TR_HOST_64BIT
    entryFallback equ jitMonitorEntry
    methodEntryFallback equ jitMethodMonitorEntry
else
    entryFallback proc near
        ; jitMonitorEntry won't clean up the extra argument
        pop eax
        mov dword ptr [esp], eax
        jmp jitMonitorEntry
    entryFallback endp

    methodEntryFallback proc near
        ; jitMethodMonitorEntry won't clean up the extra argument
        pop eax
        mov dword ptr [esp], eax
        jmp jitMethodMonitorEntry
    methodEntryFallback endp
endif

ExternHelper jitMonitorEntry
ExternHelper jitMethodMonitorEntry
ExternHelper jitMonitorExit
ExternHelper jitMethodMonitorExit

MonitorReservationFunction jitMonitorEnterReserved,                    entryFallback,         MonitorEnterReserved
MonitorReservationFunction jitMethodMonitorEnterReserved,              methodEntryFallback,   MonitorEnterReserved
MonitorReservationFunction jitMonitorEnterReservedPrimitive,           entryFallback,         MonitorEnterReservedPrimitive
MonitorReservationFunction jitMethodMonitorEnterReservedPrimitive,     methodEntryFallback,   MonitorEnterReservedPrimitive
MonitorReservationFunction jitMonitorEnterPreservingReservation,       jitMonitorEntry,       MonitorEnterPreservingReservation
MonitorReservationFunction jitMethodMonitorEnterPreservingReservation, jitMethodMonitorEntry, MonitorEnterPreservingReservation

MonitorReservationFunction jitMonitorExitReserved,                    jitMonitorExit,       MonitorExitReserved
MonitorReservationFunction jitMethodMonitorExitReserved,              jitMethodMonitorExit, MonitorExitReserved
MonitorReservationFunction jitMonitorExitReservedPrimitive,           jitMonitorExit,       MonitorExitReservedPrimitive
MonitorReservationFunction jitMethodMonitorExitReservedPrimitive,     jitMethodMonitorExit, MonitorExitReservedPrimitive
MonitorReservationFunction jitMonitorExitPreservingReservation,       jitMonitorExit,       MonitorExitPreservingReservation
MonitorReservationFunction jitMethodMonitorExitPreservingReservation, jitMethodMonitorExit, MonitorExitPreservingReservation

_TEXT ends

end
