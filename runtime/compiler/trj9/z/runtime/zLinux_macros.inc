ZZ Copyright (c) 2000, 2017 IBM Corp. and others
ZZ
ZZ This program and the accompanying materials are made 
ZZ available under the terms of the Eclipse Public License 2.0 
ZZ which accompanies this distribution and is available at 
ZZ https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
ZZ Version 2.0 which accompanies this distribution and is available 
ZZ at https://www.apache.org/licenses/LICENSE-2.0.
ZZ
ZZ This Source Code may also be made available under the following
ZZ Secondary Licenses when the conditions for such availability set
ZZ forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
ZZ General Public License, version 2 with the GNU Classpath 
ZZ Exception [1] and GNU General Public License, version 2 with the
ZZ OpenJDK Assembly Exception [2].
ZZ
ZZ [1] https://www.gnu.org/software/classpath/license.html
ZZ [2] http://openjdk.java.net/legal/assembly-exception.html
ZZ
ZZ SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
ZZ GPL-2.0 WITH Classpath-exception-2.0 OR
ZZ LicenseRef-GPL-2.0 WITH Assembly-exception

ZZ ============================================================
ZZ Macro library for Testarossa 390 JIT Development
ZZ
ZZ This file contains macros that are defined
ZZ differently for zLinux compared to zOS
ZZ ============================================================

    .nolist

ZZ ============================================================

define(SETPPA2,
[dnl
])dnl

ZZ ============================================================

define(SETVAL,
[dnl
    .set $1,$2])dnl

ZZ ============================================================

define(EQUVAL,
[dnl
    .equ $1,$2])dnl

ZZ ============================================================

define(CONST_4BYTE,
[dnl
    .long 0x$1])dnl

ZZ ============================================================

define(CONST_4BYTE_WITH_LABEL,
[dnl
$1:
    .long 0x$2])dnl

ZZ ============================================================

define(MACRO_BEGIN,
[dnl
    .macro    $1
])dnl

ZZ ============================================================

define(MACRO_END,
[dnl
    .endm
])dnl

ZZ ============================================================

define(LABEL,
[dnl
$1:])dnl

ZZ ============================================================

define(HEX,
[dnl
0x$1])dnl

ZZ ============================================================

define(START_FUNC,
[dnl
.align 8
       START $1
])dnl

ZZ ============================================================

define(END_FUNC,
[dnl
       END  $1
])dnl

ZZ ============================================================

ZZ This macro should be the first statement in every control
ZZ section. The equivalent of CSECT, but without the label.
ZZ

    .macro    START name
    .align    2

ZZ
ZZ Make the symbol externally visible. All the other names are
ZZ local.
ZZ

    .global   \name

ZZ
ZZ On the face of it, this would have seemed to be the right way to
ZZ introduce a control section. But in practise, no symbol gets
ZZ generated. So I've dropped it in favor of a simple text & label
ZZ sequence. Which gives us what we want.
ZZ
ZZ   .section  \name,"ax",@progbits
ZZ

    .text
    .type   \name,@function
\name:
    .endm


ZZ
ZZ Everything this macro does is currently ignored by the assembler.
ZZ But one day there may be something we need to put in here. This
ZZ macro should be placed at the end of every control section.
ZZ

    .macro    END   name
    .ifndef   \name
    .print    "END does not match START"
    .endif
    .size   \name,.-\name
    .ident    "(c) IBM Corp. 2002. JIT compiler support. \name"
    .endm


ZZ ============================================================

ZZ General Registers
ZZ with Upper case names, because Linux assembler is case-sensitive
EQUVAL(R0,0)
EQUVAL(R1,1)
EQUVAL(R2,2)
EQUVAL(R3,3)
EQUVAL(R4,4)
EQUVAL(R5,5)
EQUVAL(R6,6)
EQUVAL(R7,7)
EQUVAL(R8,8)
EQUVAL(R9,9)
EQUVAL(R10,10)
EQUVAL(R11,11)
EQUVAL(R12,12)
EQUVAL(R13,13)
EQUVAL(R14,14)
EQUVAL(R15,15)


ZZ ============================================================

ZZ Floating-point registers.
ZZ with Upper case names, because Linux assembler is case-sensitive
EQUVAL(F0,0)
EQUVAL(F1,1)
EQUVAL(F2,2)
EQUVAL(F3,3)
EQUVAL(F4,4)
EQUVAL(F5,5)
EQUVAL(F6,6)
EQUVAL(F7,7)
EQUVAL(F8,8)
EQUVAL(F9,9)
EQUVAL(F10,10)
EQUVAL(F11,11)
EQUVAL(F12,12)
EQUVAL(F13,13)
EQUVAL(F14,14)
EQUVAL(F15,15)

ZZ ============================================================

ZZ Entry point Register
EQUVAL(rEP,4)
EQUVAL(rSB,4)

ZZ System Stack pointer Register
EQUVAL(rSSP,15)


ZZ ============================================================
SETVAL(CEP,1)
SETVAL(CARG1,2)
SETVAL(CARG2,3)
SETVAL(CARG3,4)
SETVAL(CARG4,5)
SETVAL(CARG5,6)
SETVAL(CRA,14)
ZZ return integer
SETVAL(CRINT,2)
SETVAL(RETURNOFFSET,0)
