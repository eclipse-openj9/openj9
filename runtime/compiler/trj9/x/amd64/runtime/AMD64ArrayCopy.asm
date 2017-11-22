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

ifdef TR_HOST_64BIT

                include jilconsts.inc

OPTION NOSCOPED
_DATA   segment para 'DATA'
ifdef DEBUG
                public _copyStats
endif
                public multiprocessor

                align   4
multiprocessor:
                db      0               ; 1 if we are on an SMP box, 0 otherwise
ifdef DEBUG
; _copyStats is a collection of arrays of counts. The
; constants are used to reach at each individual array.
;
_copyStats:
statMoveShort   equ     0
                dd      17 dup(0)       ; memmove's of 0 to 16 bytes
statMoveFwd     equ     52
                dd      0               ; forward memmove's of > 16 bytes
statMoveBack    equ     56
                dd      0               ; backward memmove's of > 16 bytes
statCopyShort   equ     60
                dd      25 dup(0)       ; memcpy's of 0 to 24 bytes
statCopyLong    equ     160
                dd      0               ; memcpy's of > 24 bytes
statUnaligned   equ     164
                dd      0               ; copies from unaligned sources to unaligned destinations
statSemiAligned equ     168
                dd      0               ; copies from aligned sources to unaligned destinations
statASCheck     equ     172
                dd      0               ; copies of elements that are array-store checked
statWBCheck     equ     176
                dd      0               ; copies of elements that are write-barrier-store checked
statASWBCheck   equ     180
                dd      0               ; copies of elements that are both array-store and write-barrier-store checked
endif
_DATA   ends


J9TR_ObjectColorBlack equ 03h

  include x/amd64/runtime/AMD64ArrayCopy.inc

_TEXT	segment para 'CODE'

else
                .686p
		assume cs:flat,ds:flat,ss:flat
_DATA           segment para use32 public 'DATA'
AMD64ArrayCopy:
                db      00h
_DATA           ends

endif
                end

