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
	std     r0,-264(J9SP)              ! Save all registers
        std     r1,-256(J9SP)
        std     r2,-248(J9SP)
        std     r3,-240(J9SP)
        std     r4,-232(J9SP)
        std     r5,-224(J9SP)
        std     r6,-216(J9SP)
        std     r7,-208(J9SP)
        std     r8,-200(J9SP)
        std     r9,-192(J9SP)
        std     r10,-184(J9SP)
        std     r11,-176(J9SP)
        std     r12,-168(J9SP)
        std     r13,-160(J9SP)
        std     r14,-152(J9SP)
        std     r15,-144(J9SP)
        std     r16,-136(J9SP)
        std     r17,-128(J9SP)
        std     r18,-120(J9SP)
        std     r19,-112(J9SP)
        std     r20,-104(J9SP)
        std     r21,-96(J9SP)
        std     r22,-88(J9SP)
        std     r23,-80(J9SP)
        std     r24,-72(J9SP)
        std     r25,-64(J9SP)
        std     r26,-56(J9SP)
        std     r27,-48(J9SP)
        std     r28,-40(J9SP)
        std     r29,-32(J9SP)
        std     r30,-24(J9SP)
        std     r31,-16(J9SP)
#else   
        stmw    r0,-132(J9SP)
#endif
