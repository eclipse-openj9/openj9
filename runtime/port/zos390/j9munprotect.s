***********************************************************************
* Copyright (c) 1991, 2017 IBM Corp. and others
*
* This program and the accompanying materials are made
* available under the terms of the Eclipse Public License 2.0
* which accompanies this distribution and is available at
* https://www.eclipse.org/legal/epl-2.0/ or the Apache License,
* Version 2.0 which accompanies this distribution and is available
* at https://www.apache.org/licenses/LICENSE-2.0.
*
* This Source Code may also be made available under the following
* Secondary Licenses when the conditions for such availability set
* forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
* General Public License, version 2 with the GNU Classpath
* Exception [1] and GNU General Public License, version 2 with the
* OpenJDK Assembly Exception [2].
*
* [1] https://www.gnu.org/software/classpath/license.html
* [2] http://openjdk.java.net/legal/assembly-exception.html
*
* SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
* GPL-2.0 WITH Classpath-exception-2.0 OR
* LicenseRef-GPL-2.0 WITH Assembly-exception
***********************************************************************

         TITLE 'j9munprotect.s'

R1       EQU      1 * input: starting address of the page
R2       EQU      2 * input: address of the last byte for un-protection
R3       EQU      3 * output: return code
R15      EQU     15 

         AIF ('&SYSPARM' EQ 'BIT64').JMP1
_MUNPROT EDCXPRLG BASEREG=8
         PGSER R,UNPROTECT,A=(R1),EA=(R2)
         LR R3,R15
         EDCXEPLG
         AGO .JMP2

.JMP1     ANOP

_MUNPROT CELQPRLG BASEREG=8
         CELQEPLG

.JMP2    ANOP

         IHAPVT
         LTORG
 
         END

