*
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
*
         TITLE 'jobname.s'                                   
*
         AIF ('&SYSPARM' EQ 'BIT64').JMP1
_JOBNAME EDCXPRLG BASEREG=8
         LR    3,1
         AGO .JMP2
.JMP1    ANOP
_JOBNAME CELQPRLG BASEREG=8
         LR    3,1
         SAM31
.JMP2    ANOP
*
         IAZXJSAB READ,JOBNAME=(3)
*
         AIF ('&SYSPARM' EQ 'BIT64').JMP3
         EDCXEPLG
         AGO .JMP4
.JMP3    ANOP
         SAM64
         CELQEPLG
.JMP4    ANOP
*
         IAZJSAB
         IHAPSA
         IHAASCB
         IHAASSB
         IHASTCB
         IKJTCB
*
         END
