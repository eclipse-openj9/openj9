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
; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

_TEXT           segment para 'CODE'

                public  _compressString
                public  _compressStringJ
                public  _compressStringNoCheck
		public  _compressStringNoCheckJ
		public  _andORString

                align 16


_compressString PROC 
		SHR  RCX, 4
		ADD  RSI, RAX
		ADD  RSI, RAX
		MOV  RDX, 0
eightchars:
		MOV  RAX, QWORD PTR [RSI]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RDX, RAX				;
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+2], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  RAX, QWORD PTR [RSI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RDX, RAX				;
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI+4], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+6], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  RAX, QWORD PTR [RSI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RDX, RAX				;
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI+8], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+10], AX			; write the low 2 bytes of the 4 byte value in the destination


		MOV  RAX, QWORD PTR [RSI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RDX, RAX				;
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI+12], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+14], AX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  RDI, 16
		ADD  RSI, 32		
		LOOP eightchars
                MOV  RAX, RDX
                SHL  RDX, 32
                OR   RAX, RDX
		SHR  RAX, 32
                ret
_compressString endp
;


; A c-style memmove with no assumptions on the element size
; or copy direction required.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_compressStringJ PROC
		SHR  RCX, 4
		ADD  RSI, RAX
		ADD  RSI, RAX
		MOV  RDX, 0
eightcharsJ:
		MOV  RAX, QWORD PTR [RSI]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, QWORD PTR [RSI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RDX, RAX				;
		OR   RDX, RBX
		SHL  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  QWORD PTR [RDI], RAX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  RAX, QWORD PTR [RSI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, QWORD PTR [RSI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RDX, RAX				;
		OR   RDX, RBX
		SHL  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  QWORD PTR [RDI+8], RAX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  RDI, 16
		ADD  RSI, 32		
		LOOP eightcharsJ
                MOV  RAX, RDX
                SHL  RDX, 32
                OR   RAX, RDX
		SHR  RAX, 32
                ret
_compressStringJ endp
;

_andORString    PROC
		SHR  RCX, 4
		ADD  RSI, RAX
		ADD  RSI, RAX
		MOV  RBX, 0
		MOV  RDX, 0ffffffffffffffffh
eightchars2:
		MOV  RAX, QWORD PTR [RSI]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RBX, RAX
		AND  RDX, RAX
		MOV  RAX, QWORD PTR [RSI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RBX, RAX
		AND  RDX, RAX
		MOV  RAX, QWORD PTR [RSI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RBX, RAX
		AND  RDX, RAX
		MOV  RAX, QWORD PTR [RSI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   RBX, RAX
		AND  RDX, RAX
		ADD  RSI, 32		
		LOOP eightchars2

		MOV  RAX, RBX                           ; building the OR
		MOV  RCX, RDX                           ; building the AND
		SHR  RBX, 32                            ; building the OR
		SHR  RDX, 32                            ; building the AND
		OR   EBX, EAX				; building the OR
		AND  EDX, ECX				; building the AND
		MOV  EAX, EBX				; building the OR
		MOV  ECX, EDX				; building the AND
		SHR  EBX, 16				; building the OR
		SHL  EDX, 16				; building the AND
		OR   BX, AX				; building the OR
		AND  EDX, ECX				; building the AND

		MOV  DX, BX
                ret
_andORString endp
;


_compressStringNoCheckJ    PROC
		SHR  RCX, 4
		ADD  RSI, RAX
		ADD  RSI, RAX
eightcharsNoCheckJ:
		MOV  RAX, QWORD PTR [RSI]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, QWORD PTR [RSI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		SHL  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  QWORD PTR [RDI], RAX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  RAX, QWORD PTR [RSI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, QWORD PTR [RSI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		SHL  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  QWORD PTR [RDI+8], RAX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  RDI, 16
		ADD  RSI, 32		
		LOOP eightcharsNoCheckJ
                ret
_compressStringNoCheckJ endp
;


_compressStringNoCheck PROC 
		SHR  RCX, 4
		ADD  RSI, RAX
		ADD  RSI, RAX
eightcharsNoCheck:
		MOV  RAX, QWORD PTR [RSI]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+2], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  RAX, QWORD PTR [RSI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI+4], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+6], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  RAX, QWORD PTR [RSI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI+8], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+10], AX			; write the low 2 bytes of the 4 byte value in the destination


		MOV  RAX, QWORD PTR [RSI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  RBX, RAX					; copy the loaded value
		SHR  RBX, 8					; shift right by 8
		OR   RAX, RBX					; or the 2 values
		MOV  WORD PTR [RDI+12], AX			; write the low 2 bytes of the 4 byte value in the destination
		SHR  RAX, 32
		MOV  WORD PTR [RDI+14], AX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  RDI, 16
		ADD  RSI, 32		
		LOOP eightcharsNoCheck
                ret
_compressStringNoCheck endp
;


_TEXT           ends


