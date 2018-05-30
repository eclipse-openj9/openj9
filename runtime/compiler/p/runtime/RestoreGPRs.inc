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

#ifdef TR_HOST_64BIT
	ld 	r0, 0(J9SP)     
	ld 	r2, 16(J9SP)    
        ld      r3, 24(J9SP)
        ld      r4, 32(J9SP)
        ld      r5, 40(J9SP)
        ld      r6, 48(J9SP)
        ld      r7, 56(J9SP)
        ld      r8, 64(J9SP)
        ld      r9, 72(J9SP)
        ld      r10, 80(J9SP)
        ld      r11, 88(J9SP)
        ld      r12, 96(J9SP)
        ld      r13, 104(J9SP)
        ld      r15, 120(J9SP)
        ld      r16, 128(J9SP)
        ld      r17, 136(J9SP)
        ld      r18, 144(J9SP)
        ld      r19, 152(J9SP)
        ld      r20, 160(J9SP)
        ld      r21, 168(J9SP)
        ld      r22, 176(J9SP)
        ld      r23, 184(J9SP)
        ld      r24, 192(J9SP)
        ld      r25, 200(J9SP)
        ld      r26, 208(J9SP)
        ld      r27, 216(J9SP)
        ld      r28, 224(J9SP)
        ld      r29, 232(J9SP)
        ld      r30, 240(J9SP)
        ld      r31, 248(J9SP)
	ld 	SP, 8(J9SP)                 ! Restore saved SP
        addi    J9SP, J9SP,264
#else
        ori     r3, J9SP,0                  ! Copy J9SP into register not trashed by lmw
        lmw     r4, 16(r3)                  ! Restore gpr4 and up registers
        ori	J9SP, r3, 0		    ! Copy J9SP back
        lwz     r0, 0(r3)                   ! Restore gpr0
        lwz     SP, 4(r3)                   ! Restore saved SP
#if !(defined(LINUXPPC))
        lwz     r2, 8(r3)                   ! Restore saved gpr2
#endif
        lwz     r3, 12(r3)                  ! Restore saved gpr3
        addi	J9SP, J9SP, 132             ! Restore J9SP
#endif
