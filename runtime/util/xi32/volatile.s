# Copyright (c) 1991, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
eq_pointer_size = 4
eqS_longVolatileRead = 16
eqS_longVolatileWrite = 16
eqSR_longVolatileRead = 4
eqSR_longVolatileWrite = 4
eqSRS_longVolatileRead = 16
eqSRS_longVolatileWrite = 16
eqSS_longVolatileRead = 64
eqSS_longVolatileWrite = 64
       	#CODE32 SEGMENT FLAT PUBLIC 'CODE'
       	#assume cs:flat,ds:flat,ss:flat
       	#CODE32 ends
       	#CODE32 SEGMENT FLAT PUBLIC 'CODE'
       	#assume cs:flat,ds:flat,ss:flat
        .globl longVolatileWrite
        .type longVolatileWrite,@function
        .globl longVolatileRead
        .type longVolatileRead,@function
## Prototype: U_64 longVolatileRead(J9VMThread * vmThread, U_64 * srcAddress);
## Defined in: #UTIL Args: 2

        .text
        .align 4
longVolatileRead:
        push %ebp
        mov %esp, %ebp
        push %esi
        push %edi
        push %ebx
        sub $76, %esp
        movl (eqSRS_longVolatileRead+12+4+eqSS_longVolatileRead)(%esp), %ebx
        movl (eqSRS_longVolatileRead+8+12+eqSS_longVolatileRead)(%esp), %ebx
        movq (%ebx),%xmm0
        movd %xmm0,%ecx
        pshufd $225, %xmm0, %xmm1
        movd %xmm1,%ebx
        movl %ebx, %edx
        movl %ecx, %eax
        add $76, %esp
        pop %ebx
        pop %edi
        pop %esi
        pop %ebp
        ret
END_longVolatileRead:
        .size longVolatileRead,END_longVolatileRead - longVolatileRead

## Prototype: void longVolatileWrite(J9VMThread * vmThread, U_64 * destAddress, U_64 * value);
## Defined in: #UTIL Args: 3

        .text
        .align 4
longVolatileWrite:
        push %ebp
        mov %esp, %ebp
        push %esi
        push %edi
        push %ebx
        sub $76, %esp
        movl (eqSS_longVolatileWrite+12+4+eqSRS_longVolatileWrite)(%esp), %ebx
        movl (eqSS_longVolatileWrite+8+12+eqSRS_longVolatileWrite)(%esp), %edx
        movl (eqSS_longVolatileWrite+(2*12)+eqSRS_longVolatileWrite)(%esp), %eax
        movl (%eax), %ecx
        movl 4(%eax), %ebx
        movd %ecx,%xmm0
        movd %ebx,%xmm1
        punpckldq %xmm1, %xmm0
        movq %xmm0,(%edx)
        add $76, %esp
        pop %ebx
        pop %edi
        pop %esi
        pop %ebp
        ret
END_longVolatileWrite:
        .size longVolatileWrite,END_longVolatileWrite - longVolatileWrite

       	#CODE32 ends
        # end of file
