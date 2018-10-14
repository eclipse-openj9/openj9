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
; SPDX-License-Identifier: EPL-2.0 or Apache-2.0 or GPL-2.0 WITH Classpath-exception-2.0 or LicenseRef-GPL-2.0 WITH Assembly-exception

%ifdef TR_HOST_64BIT
%include "jilconsts.inc"

%include "x/amd64/runtime/AMD64CompressString_nasm.inc"



J9TR_ObjectColorBlack equ 03h

    segment .text

%else ; TR_HOST_64BIT

segment .data
IA32ArrayCopy:
        db      00h            

%endif ; TR_HOST_64BIT