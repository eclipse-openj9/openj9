*
* Copyright (c) 2021, 2021 IBM Corp. and others
*
* This program and the accompanying materials are made
* available under the terms of the Eclipse Public License 2.0
* which accompanies this distribution and is available at
* https://www.eclipse.org/legal/epl-2.0/ or the Apache License,
* Version 2.0 which accompanies this distribution and is available
* at https://www.apache.org/licenses/LICENSE-2.0.
*
* This Source Code is also Distributed under one or more
* Secondary Licenses, as those terms are defined by the
* Eclipse Public License, v. 2.0: GNU General Public License,
* version 2 with the GNU Classpath Exception [1] and GNU
* General Public License, version 2 with the OpenJDK Assembly
* Exception [2].
*
* [1] https://www.gnu.org/software/classpath/license.html
* [2] http://openjdk.java.net/legal/assembly-exception.html
*
         TITLE 'j9vm31floatstubs'
**********************************************************************
* This file contains a set of stub functions with the sole
* intention of loading and storing float/double from/to FPR0.
* However, since the OS Linkage dictates FPR0 is the incoming
* param and outgoing return floating point register, these
* functions are actually NOPs.  Ideally, we just generate
* inline assembly code from the C code (j9cel4ro64 in this case)
* once build compiler is upgraded.

J9VM_STE CSECT
J9VM_STE AMODE ANY
J9VM_STE RMODE ANY
         EDCPRLG
* This routine is to extract FPR0 as a float
* however, since OS linkage returns FPR0 anyways
* this is a nop routine to trick compiler in
* extracting return value from FPR0.
         EDCEPIL
         END

J9VM_STD CSECT
J9VM_STD AMODE ANY
J9VM_STD RMODE ANY
         EDCPRLG
* This routine is to extract FPR0 as a double
* however, since OS linkage returns FPR0 anyways
* this is a nop routine to trick compiler in
* extracting return value from FPR0.
         EDCEPIL
         END

J9VM_LE CSECT
J9VM_LE AMODE ANY
J9VM_LE RMODE ANY
         EDCPRLG
* This routine is to store FPR0 with a float
* parameter.  However, since OS linkage uses
* FPR0 as the parm, this is a nop routine
* to trick the compiler in doing it for us.
         EDCEPIL
         END

J9VM_LD CSECT
J9VM_LD AMODE ANY
J9VM_LD RMODE ANY
         EDCPRLG
* This routine is to store FPR0 with a double
* parameter.  However, since OS linkage uses
* FPR0 as the parm, this is a nop routine
* to trick the compiler in doing it for us.
         EDCEPIL
         END

