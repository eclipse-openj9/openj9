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

                .486p
                .xmm
                assume cs:flat,ds:flat,ss:flat

_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

                public  _compressString
                public  _compressStringJ
                public  _compressStringNoCheck
		public  _compressStringNoCheckJ
		public  _andORString
                align   16
;
; A c-style memmove with no assumptions on the element size
; or copy direction required.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_compressStringJ PROC NEAR
		SHR  ECX, 4
		ADD  ESI, EAX
		ADD  ESI, EAX
		MOV  EDX, 0
eightcharsJ:
		MOV  EAX, DWORD PTR [ESI]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, DWORD PTR [ESI+4]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		OR   EDX, EBX
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI], EAX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EAX, DWORD PTR [ESI+12]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		OR   EDX, EBX
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI+4], EAX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, DWORD PTR [ESI+20]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		OR   EDX, EBX
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI+8], EAX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EAX, DWORD PTR [ESI+28]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		OR   EDX, EBX
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI+12], EAX			; write the low 2 bytes of the 4 byte value in the destination


		ADD  EDI, 16
		ADD  ESI, 32		
		LOOP eightcharsJ
		AND  EDX, 0ff80ff80h
		MOV  EAX, EDX
                ret
_compressStringJ endp
;

_compressString PROC NEAR
		SHR  ECX, 3
		ADD  ESI, EAX
		ADD  ESI, EAX
		MOV  EDX, 0
eightchars:
		MOV  EAX, DWORD PTR [ESI]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+4]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI+2], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI+4], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+12]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EDX, EAX				;
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI+6], AX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  EDI, 8
		ADD  ESI, 16		
		LOOP eightchars
		AND  EDX, 0ff80ff80h
		MOV  EAX, EDX
                ret
_compressString endp
;



_andORString    PROC NEAR
		SHR  ECX, 4
		ADD  ESI, EAX
		ADD  ESI, EAX
		MOV  EBX, 0
		MOV  EDX, 0ffffffffh
eightchars2:
		MOV  EAX, DWORD PTR [ESI]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+4]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+12]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+20]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		MOV  EAX, DWORD PTR [ESI+28]  		; load 4 bytes from the source array into EAX (2 chars)
		OR   EBX, EAX
		AND  EDX, EAX
		ADD  ESI, 32		
		LOOP eightchars2

		MOV  EAX, EBX
		SHL  EAX, 16
		OR   EBX, EAX
		SHR  EBX, 16
		
		MOV  EAX, EDX
		SHL  EAX, 16
		AND  EAX, EDX

		OR   EAX, EBX		; AND is in the high word and OR is in the low word.
		MOV  EDX, EAX
                ret
_andORString endp
;


_compressStringNoCheckJ    PROC NEAR
		SHR  ECX, 4
		ADD  ESI, EAX
		ADD  ESI, EAX
eightcharsNoCheckJ:

		MOV  EAX, DWORD PTR [ESI]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, DWORD PTR [ESI+4]  		; load 4 bytes from the source array into EAX (2 chars)
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI], EAX			; write the low 2 bytes of the 4 byte value in the destination
		
		MOV  EAX, DWORD PTR [ESI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, DWORD PTR [ESI+12]  		; load 4 bytes from the source array into EAX (2 chars)
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI+4], EAX			; write the low 2 bytes of the 4 byte value in the destination
		
		MOV  EAX, DWORD PTR [ESI+16]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, DWORD PTR [ESI+20]  		; load 4 bytes from the source array into EAX (2 chars)
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI+8], EAX			; write the low 2 bytes of the 4 byte value in the destination
		
		MOV  EAX, DWORD PTR [ESI+24]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, DWORD PTR [ESI+28]  		; load 4 bytes from the source array into EAX (2 chars)
		SHL  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  DWORD PTR [EDI+12], EAX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  EDI, 16
		ADD  ESI, 32		
		LOOP eightcharsNoCheckJ
                ret
_compressStringNoCheckJ endp
;


_compressStringNoCheck PROC NEAR
		SHR  ECX, 3
		ADD  ESI, EAX
		ADD  ESI, EAX
eightcharsNoCheck:
		MOV  EAX, DWORD PTR [ESI]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+4]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI+2], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+8]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI+4], AX			; write the low 2 bytes of the 4 byte value in the destination

		MOV  EAX, DWORD PTR [ESI+12]  		; load 4 bytes from the source array into EAX (2 chars)
		MOV  EBX, EAX					; copy the loaded value
		SHR  EBX, 8					; shift right by 8
		OR   EAX, EBX					; or the 2 values
		MOV  WORD PTR [EDI+6], AX			; write the low 2 bytes of the 4 byte value in the destination

		ADD  EDI, 8
		ADD  ESI, 16		
		LOOP eightcharsNoCheck
                ret
_compressStringNoCheck endp
;


_TEXT           ends

