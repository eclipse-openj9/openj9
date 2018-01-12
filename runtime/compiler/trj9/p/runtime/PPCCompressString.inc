!! Copyright (c) 2000, 2017 IBM Corp. and others
!!
!! This program and the accompanying materials are made available under
!! the terms of the Eclipse Public License 2.0 which accompanies this
!! distribution and is available at https://www.eclipse.org/legal/epl-2.0/
!! or the Apache License, Version 2.0 which accompanies this distribution and
!! is available at https://www.apache.org/licenses/LICENSE-2.0.
!!
!! This Source Code may also be made available under the following
!! Secondary Licenses when the conditions for such availability set
!! forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
!! General Public License, version 2 with the GNU Classpath
!! Exception [1] and GNU General Public License, version 2 with the
!! OpenJDK Assembly Exception [2].
!!
!! [1] https://www.gnu.org/software/classpath/license.html
!! [2] http://openjdk.java.net/legal/assembly-exception.html
!!
!! SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

! arrayCopy*, wordArrayCopy*, halfWordArrayCopy*, forwardArrayCopy*,
! forwardWordArrayCopy*, forwardHalfWordArrayCopy*:
!       copyByteLength -- r7
!       srcByteAddr    -- r8
!       dstByteAddr    -- r9
!	64-bit kills: r0, r5, r6, r7, r8, r9, r11, cr0
!	32-bit kills: r0, r3, r4, r5, r6, r7, r8, r9, r10, r11, cr0
!	32-bit float additionally kills: fp8, fp9, fp10, fp11
!	float double pair additionally kills: fp8, fp9, fp10, fp11


#include "p/runtime/ppcasmdefines.inc"

#ifdef AIXPPC
	.globl    .__compressString
	.globl    .__compressStringJ
	.globl    .__compressStringNoCheck
	.globl    .__compressStringNoCheckJ
	.globl    .__andORString
	.globl    __compressString{DS}
	.globl    __compressStringJ{DS}
	.globl    __compressStringNoCheck{DS}
	.globl    __compressStringNoCheckJ{DS}
	.globl    __andORString{DS}
!	.globl    .__arrayCopy_dp
!	.globl    __arrayCopy_dp{DS}

#elif defined(LINUXPPC64)
	.globl    FUNC_LABEL(__compressString)
	.type     FUNC_LABEL(__compressString),@function
	.globl    FUNC_LABEL(__compressStringJ)
	.type     FUNC_LABEL(__compressStringJ),@function
	.globl    FUNC_LABEL(__compressStringNoCheck)
	.type     FUNC_LABEL(__compressStringNoCheck),@function
	.globl    FUNC_LABEL(__compressStringNoCheckJ)
	.type     FUNC_LABEL(__compressStringNoCheckJ),@function
	.globl    FUNC_LABEL(__andORString)
	.type     FUNC_LABEL(__andOrString),@function

#elif defined(LINUX)
	.globl    __compressString
	.globl    __compressStringJ
	.globl    __compressStringNocheck
	.globl    __compressStringNocheckJ
	.globl    __andORString
!	.globl    __arrayCopy_dp
#endif


#if defined(LINUXPPC64)
	.section  ".text"
	.align	2
#endif

#ifdef AIXPPC
	.csect	__compressStringJ{PR}
.__compressStringJ:
	.function .__compressStringJ,startproc.__compressStringJ,16,0,(endproc.__compressStringJ-startproc.__compressStringJ)
#elif defined(LINUXPPC64)
FUNC_LABEL(__compressStringJ):
#else
__compressStringJ:
#endif
	startproc.__compressStringJ:
L.__compressStringJ:
	add	r11, r7, r7
	li      r12, 0		!check
	add	r9, r9, r11
	srwi.    r0, r8, 4
	bc	BO_IF, CR0_EQ, L.__residueJ
        mtctr   r0
L.__loopJ:
	lwz     r0, 0(r9)
	lwz     r11, 4(r9)
	lwz     r6, 8(r9)
	lwz     r3, 12(r9)
	or      r12, r12, r0		!method 2 check
	rlwinm  r4, r11, 8, 0xFFFFFFFF  !method 2
	or      r12, r12, r11		!method 2 check
	rlwinm  r5, r3, 8, 0xFFFFFFFF	!method 2
	or      r12, r12, r6		!method 2 check
	or      r4, r4, r0		!method 2
	or      r12, r12, r3		!method 2 check
	or	r5, r5, r6		!method 2
	stw	r4, 0(r10)
	stw     r5, 4(r10)

	lwz     r0, 16(r9)
	lwz     r11,20(r9)
	lwz     r6, 24(r9)
	lwz     r3, 28(r9)
	addi	r9, r9, 32
	or      r12, r12, r0		!method 2 check
	rlwinm  r4, r11, 8, 0xFFFFFFFF	!method 2
	or      r12, r12, r11		!method 2 check
	rlwinm  r5, r3, 8, 0xFFFFFFFF	!method 2
	or      r12, r12, r6		!method 2 check
	or      r4, r4, r0		!method 2
	or      r12, r12, r3		!method 2 check
	or	r5, r5, r6		!method 2
	stw	r4, 8(r10)
	stw     r5, 12(r10)
	addi	r10, r10, 16
	subi    r8, r8, 16
	bdnz	L.__loopJ
L.__residueJ:
	srwi.	r0, r8, 2
	bc	BO_IF, CR0_EQ, L.__residue2J
	mtctr	r0
L.__residueLoopJ:
	lwz	r0, 0(r9)
	lwz     r11, 4(r9)
	addi	r9, r9, 8
	or	r12, r12, r0
	rlwinm  r4, r11, 8, 0xFFFFFFFF	!method 2
	or	r12, r12, r11
	or      r4, r4, r0		!method 2
	stw	r4, 0(r10)
	addi	r10, r10, 4
	subi	r8, r8, 4
	bdnz	L.__residueLoopJ

L.__residue2J:
	srwi.   r0, r8, 0
	bc	BO_IF, CR0_EQ, L.__finishJ
	mtctr	r0
L.__residueLoop2J:
	lhz	r0, 0(r9)
	addi    r9, r9, 2
	or	r12, r12, r0
	stb	r0, 0(r10)
	addi    r10, r10, 1
	bdnz	L.__residueLoop2J

L.__finishJ:
	andi.	r3, r12, 0xFF80          !check
	andis.	r4, r12, 0xFF80		!check
	or	r3, r3, r4		!check
	bclr    BO_IF, CR0_EQ
	endproc.__compressStringJ:


#ifdef AIXPPC
	.csect	__compressString{PR}
.__compressString:
	.function .__compressString,startproc.__compressString,16,0,(endproc.__compressString-startproc.__compressString)
#elif defined(LINUXPPC64)
FUNC_LABEL(__compressString):
#else
__compressString:
#endif

	startproc.__compressString:
L.__compressString:
	add	r11, r7, r7
	li      r12, 0		!check
	add	r9, r9, r11
	srwi.	r0, r8, 4
	bc	BO_IF, CR0_EQ, L.__residue
        mtctr   r0
L.__loop:
	lwz     r0, 0(r9)
	lwz     r11, 4(r9)
	lwz     r6, 8(r9)
	lwz     r3, 12(r9)
	rlwinm	r4, r0, 8, 0xFF000000	!method 1
	or      r12, r12, r0		!method 1 check
	rlwinm	r7, r0, 16, 0xFF0000	!method 1
	or      r12, r12, r11		!method 1 check
	rlwinm	r5, r6, 8, 0xFF000000   !method 1
	or      r0, r12, r6             !method 1 check
	rlwinm	r12, r6, 16, 0xFF0000   !method 1
	rlwimi	r4, r11, 24, 0xFF00	!method 1
	rlwimi	r7, r11, 0, 0xFF	!method 1
	rlwimi  r5, r3, 24, 0xFF00	!method 1
	rlwimi  r12, r3, 0, 0xFF	!method 1
	or	r4, r4, r7		!method 1
	or	r5, r5, r12		!method 1
	stw	r4, 0(r10)
	stw     r5, 4(r10)
	or	r12, r0, r3

	lwz     r0, 16(r9)
	lwz     r11,20(r9)
	lwz     r6, 24(r9)
	lwz     r3, 28(r9)
	addi	r9, r9, 32
	rlwinm	r4, r0, 8, 0xFF000000	!method 1
	or      r12, r12, r0		!method 1 check
	rlwinm	r7, r0, 16, 0xFF0000	!method 1
	or      r12, r12, r11		!method 1 check
	rlwinm	r5, r6, 8, 0xFF000000	!method 1
	or      r0, r12, r6		!method 1 check
	rlwinm	r12, r6, 16, 0xFF0000	!method 1
	rlwimi	r4, r11, 24, 0xFF00	!method 1
	rlwimi	r7, r11, 0, 0xFF	!method 1
	rlwimi  r5, r3, 24, 0xFF00	!method 1
	rlwimi  r12, r3, 0, 0xFF	!method 1
	or	r4, r4, r7		!method 1
	or	r5, r5, r12		!method 1
	stw	r4, 8(r10)
	stw     r5, 12(r10)
	addi	r10, r10, 16
	or	r12, r0, r3
	subi    r8, r8, 16
	bdnz	L.__loop

L.__residue:
	srwi.	r0, r8, 2
	bc	BO_IF, CR0_EQ, L.__residue2
	mtctr	r0
L.__residueLoop:
	lwz	r0, 0(r9)
	lwz     r11, 4(r9)
	addi	r9, r9, 8
	or      r12, r12, r0		!method 1 check
	rlwinm	r4, r0, 8, 0xFF000000
	rlwinm	r7, r0, 16, 0xFF0000
	rlwimi	r4, r11, 24, 0xFF00	!method 1
	rlwimi	r7, r11, 0, 0xFF	!method 1
	or      r12, r12, r11		!method 1 check
	or	r4, r4, r7		!method 1
	stw	r4, 0(r10)
	addi    r10, r10, 4
	subi	r8, r8, 4
	bdnz	L.__residueLoop

L.__residue2:
	srwi.   r0, r8, 0
	bc	BO_IF, CR0_EQ, L.__finish
	mtctr	r0
L.__residueLoop2:
	lhz	r0, 0(r9)
	addi    r9, r9, 2
	or	r12, r12, r0
	stb	r0, 0(r10)
	addi    r10, r10, 1
	bdnz	L.__residueLoop2

L.__finish:
	andi.	r3, r12, 0xFF80          !check
	andis.	r4, r12, 0xFF80		!check
	or	r3, r3, r4		!check
	bclr    BO_IF, CR0_EQ
	endproc.__compressString:

#ifdef AIXPPC
	.csect	__compressStringNoCheck{PR}
.__compressStringNoCheck:
	.function .__compressStringNoCheck,startproc.__compressStringNoCheck,16,0,(endproc.__compressStringNoCheck-startproc.__compressStringNoCheck)
#elif defined(LINUXPPC64)
FUNC_LABEL(__compressStringNoCheck):
#else
__compressStringNoCheck:
#endif

	startproc.__compressStringNoCheck:
L.__compressStringNoCheck:
	add	r11, r7, r7
	add	r9, r9, r11
	srwi.	r0, r8, 4
	bc	BO_IF, CR0_EQ, L.__residueNoCheck
        mtctr   r0
L.__loopNoCheck:
	lwz     r0, 0(r9)
	lwz     r11, 4(r9)
	lwz     r6, 8(r9)
	lwz     r3, 12(r9)
	rlwinm	r4, r0, 8, 0xFF000000	!method 1
	rlwinm	r7, r0, 16, 0xFF0000	!method 1
	rlwinm	r5, r6, 8, 0xFF000000   !method 1
	rlwinm	r12, r6, 16, 0xFF0000   !method 1
	rlwimi	r4, r11, 24, 0xFF00	!method 1
	rlwimi	r7, r11, 0, 0xFF	!method 1
	rlwimi  r5, r3, 24, 0xFF00	!method 1
	rlwimi  r12, r3, 0, 0xFF	!method 1
	or	r4, r4, r7		!method 1
	or	r5, r5, r12		!method 1
	stw	r4, 0(r10)
	stw     r5, 4(r10)

	lwz     r0, 16(r9)
	lwz     r11,20(r9)
	lwz     r6, 24(r9)
	lwz     r3, 28(r9)
	addi	r9, r9, 32
	rlwinm	r4, r0, 8, 0xFF000000	!method 1
	rlwinm	r7, r0, 16, 0xFF0000	!method 1
	rlwinm	r5, r6, 8, 0xFF000000	!method 1
	rlwinm	r12, r6, 16, 0xFF0000	!method 1
	rlwimi	r4, r11, 24, 0xFF00	!method 1
	rlwimi	r7, r11, 0, 0xFF	!method 1
	rlwimi  r5, r3, 24, 0xFF00	!method 1
	rlwimi  r12, r3, 0, 0xFF	!method 1
	or	r4, r4, r7		!method 1
	or	r5, r5, r12		!method 1
	stw	r4, 8(r10)
	stw     r5, 12(r10)
	addi	r10, r10, 16
	subi    r8, r8, 16
	bdnz	L.__loopNoCheck

L.__residueNoCheck:
	srwi.	r0, r8, 2
	bc	BO_IF, CR0_EQ, L.__residue2NoCheck
	mtctr	r0
L.__residueLoopNoCheck:
	lwz	r0, 0(r9)
	lwz     r11, 4(r9)
	addi	r9, r9, 8
	rlwinm	r4, r0, 8, 0xFF000000
	rlwinm	r7, r0, 16, 0xFF0000
	rlwimi	r4, r11, 24, 0xFF00	!method 1
	rlwimi	r7, r11, 0, 0xFF	!method 1
	or	r4, r4, r7		!method 1
	stw	r4, 0(r10)
	addi    r10, r10, 4
	subi	r8, r8, 4
	bdnz	L.__residueLoopNoCheck

L.__residue2NoCheck:
	srwi.   r0, r8, 0
	bc	BO_IF, CR0_EQ, L.__finishNoCheck
	mtctr	r0
L.__residueLoop2NoCheck:
	lhz	r0, 0(r9)
	addi    r9, r9, 2
	stb	r0, 0(r10)
	addi    r10, r10, 1
	bdnz	L.__residueLoop2NoCheck

L.__finishNoCheck:
	bclr    BO_IF, CR0_EQ

	endproc.__compressStringNoCheck:

#ifdef AIXPPC
	.csect	__compressStringNoCheckJ{PR}
.__compressStringNoCheckJ:
	.function .__compressStringNoCheckJ,startproc.__compressStringNoCheckJ,16,0,(endproc.__compressStringNoCheckJ-startproc.__compressStringNoCheckJ)
#elif defined(LINUXPPC64)
FUNC_LABEL(__compressStringNoCheckJ):
#else
__compressStringNoCheckJ:
#endif
	startproc.__compressStringNoCheckJ:
L.__compressStringNoCheckJ:
	add	r11, r7, r7
	add	r9, r9, r11
	srwi.    r0, r8, 4
	bc	BO_IF, CR0_EQ, L.__residueNoCheckJ
        mtctr   r0
L.__loopNoCheckJ:
	lwz     r0, 0(r9)
	lwz     r11, 4(r9)
	lwz     r6, 8(r9)
	lwz     r3, 12(r9)
	rlwinm  r4, r11, 8, 0xFFFFFFFF  !method 2
	rlwinm  r5, r3, 8, 0xFFFFFFFF	!method 2
	or      r4, r4, r0		!method 2
	or	r5, r5, r6		!method 2
	stw	r4, 0(r10)
	stw     r5, 4(r10)

	lwz     r0, 16(r9)
	lwz     r11,20(r9)
	lwz     r6, 24(r9)
	lwz     r3, 28(r9)
	addi	r9, r9, 32
	rlwinm  r4, r11, 8, 0xFFFFFFFF	!method 2
	rlwinm  r5, r3, 8, 0xFFFFFFFF	!method 2
	or      r4, r4, r0		!method 2
	or	r5, r5, r6		!method 2
	stw	r4, 8(r10)
	stw     r5, 12(r10)
	addi	r10, r10, 16
	subi    r8, r8, 16
	bdnz	L.__loopNoCheckJ
L.__residueNoCheckJ:
	srwi.	r0, r8, 2
	bc	BO_IF, CR0_EQ, L.__residue2NoCheckJ
	mtctr	r0
L.__residueLoopNoCheckJ:
	lwz	r0, 0(r9)
	lwz     r11, 4(r9)
	addi	r9, r9, 8
	rlwinm  r4, r11, 8, 0xFFFFFFFF	!method 2
	or      r4, r4, r0		!method 2
	stw	r4, 0(r10)
	addi	r10, r10, 4
	subi	r8, r8, 4
	bdnz	L.__residueLoopNoCheckJ

L.__residue2NoCheckJ:
	srwi.   r0, r8, 0
	bc	BO_IF, CR0_EQ, L.__finishNoCheckJ
	mtctr	r0
L.__residueLoop2NoCheckJ:
	lhz	r0, 0(r9)
	addi    r9, r9, 2
	stb	r0, 0(r10)
	addi    r10, r10, 1
	bdnz	L.__residueLoop2NoCheckJ

L.__finishNoCheckJ:
	bclr    BO_IF, CR0_EQ
	endproc.__compressStringNoCheckJ:


#ifdef AIXPPC
	.csect	__andORString{PR}
.__andORString:
	.function .__andORString,startproc.__andORString,16,0,(endproc.__andORString-startproc.__andORString)
#elif defined(LINUXPPC64)
FUNC_LABEL(__andORString):
#else
__andORString:
#endif
	startproc.__andORString:
L.__andORString:
	add	r9, r9, r7
	add	r9, r9, r7
        li      r4, 0
        li      r5, 0
        subi    r5, r5, 1
	srwi.   r0, r8, 4
	bc	BO_IF, CR0_EQ, L.__residueandOR
        mtctr   r0
L.__loopandOR:
	lwz     r0, 0(r9)
	lwz     r3, 4(r9)
	or      r4, r4, r0
	and     r5, r5, r0
	or      r4, r4, r3
	and     r5, r5, r3

	lwz     r0, 8(r9)
	lwz     r3, 12(r9)
	or      r4, r4, r0
	and     r5, r5, r0
	or      r4, r4, r3
	and     r5, r5, r3

	lwz     r0, 16(r9)
	lwz     r3, 20(r9)
	or      r4, r4, r0
	and     r5, r5, r0
	or      r4, r4, r3
	and     r5, r5, r3

	lwz     r0, 24(r9)
	lwz     r3, 28(r9)
	or      r4, r4, r0
	and     r5, r5, r0
	or      r4, r4, r3
	and     r5, r5, r3

	addi	r9, r9, 32
	subi    r8, r8, 16
	bdnz	L.__loopandOR
L.__residueandOR:
	srwi.	r0, r8, 2
	bc	BO_IF, CR0_EQ, L.__residue2andOR
	mtctr	r0
L.__residueLoopandOR:
	lwz     r0, 0(r9)
	lwz     r3, 4(r9)
	or      r4, r4, r0
	and     r5, r5, r0
	or      r4, r4, r3
	and     r5, r5, r3

        addi    r9, r9, 8
	subi	r8, r8, 4
	bdnz	L.__residueLoopandOR

L.__residue2andOR:
	srwi.   r0, r8, 0
	bc	BO_IF, CR0_EQ, L.__finishandOR
	mtctr	r0
L.__residueLoop2andOR:
	lhz	r0, 0(r9)
	or      r4, r4, r0
	and     r5, r5, r0
	bdnz	L.__residueLoop2andOR

L.__finishandOR:
        slwi    r3, r4, 16
        or      r4, r3, r4
        srwi    r4, r4, 16

        slwi    r3, r5, 16
        and     r3, r3, r5         ! AND is in the high word and OR is in the low word.

	or	r3, r3, r4
	bclr    BO_IF, CR0_EQ
	endproc.__andORString:



! .data section
#ifdef AIXPPC
!	.toc
!TOC__shortArrayCopyLabelTable:
!	.tc       __shortArrayCopyLabelTable[TC],__shortArrayCopyLabelTable

	.csect    __compressString{DS}
	ADDR      .__compressString
	ADDR      TOC{TC0}
	ADDR      0x00000000
! End   csect     __compressString{DS}

	.csect    __compressStringJ{DS}
	ADDR      .__compressStringJ
	ADDR      TOC{TC0}
	ADDR      0x00000000
! End   csect     __compressStringJ{DS}

	.csect    __compressStringNoCheck{DS}
	ADDR      .__compressStringNoCheck
	ADDR      TOC{TC0}
	ADDR      0x00000000
! End   csect     __compressStringNoCheck{DS}

	.csect    __compressStringNoCheckJ{DS}
	ADDR      .__compressStringNoCheckJ
	ADDR      TOC{TC0}
	ADDR      0x00000000
! End   csect     __compressStringNoCheckJ{DS}


!	.csect    __arrayCopy_dp{DS}
!	ADDR      .__arrayCopy_dp
!	ADDR      TOC{TC0}
!	ADDR      0x00000000
!! End   csect     __arrayCopy_dp{DS}



#elif defined(LINUXPPC64)

#if !defined(__LITTLE_ENDIAN__)
	.section  ".opd","aw"
	.align    3
	.globl    __compressString
	.size     __compressString,24
__compressString:
	.quad     .__compressString
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    __compressStringJ
	.size     __compressStringJ,24
__compressStringJ:
	.quad     .__compressStringJ
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    __compressStringNoCheck
	.size     __compressStringNoCheck,24
__compressStringNoCheck:
	.quad     .__compressStringNoCheck
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    __compressStringNoCheckJ
	.size     __compressStringNoCheckJ,24
__compressStringNoCheckJ:
	.quad     .__compressStringNoCheckJ
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000

	.globl    __andORString
	.size     __andORString,24
__andORString:
	.quad     .__andORString
	.quad     .TOC.@tocbase
	.long     0x00000000
	.long     0x00000000
#endif

#endif


