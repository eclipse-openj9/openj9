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

         TITLE 'j9csrsi_wrp.s'

* Assembler glue routine to call CSRSI (an AMODE31 service) from an
* AMODE64/31 C program.
* CSRSI is a system information service to request information about
* the machine, LPAR, and VM.  The returned information is mapped by
* DSECTs in CSRSIIDF macro.  SIV1V2V3 DSECT maps the data returned
* when MACHINE, LPAR and VM data are requested.
* See MVS Data Areas Volume 1 (ABEP-DDRCOM).
* Only one parameter is expected on input - address of a structure
* containing packed arguments to CSRSI for SIV1V2V3 DSECT.
*
* Following are the various general registers that would be used
* explicitly for passing parameters, holding entry addresses of
* service routines, and finally as base registers for establishing
* addressability inside control section.
*
R0       EQU   0      Entry address of CSRSI load module (LOAD macro)
R1       EQU   1      Input/Output - address of struct csrsi_v2v3
R2       EQU   2      Base register (USING instruction)
R3       EQU   3      Base register (USING instruction)
R6       EQU   6      Base register (USING instruction)
R8       EQU   8      Base register (XPLINK assembler prolog)
R9       EQU   9      Base register (USING instruction)
R10      EQU   10     Base register (USING instruction)
R15      EQU   15     Entry address of CSRSI service (CALL macro)
*
CVT_ADDR        EQU X'10'   Absolute address of CVT
CSRSI_cvtcsrt   EQU X'220'  Offset of CSRSI_cvtcsrt
CSRSI_addr      EQU X'30'   Offset of CSRSI_addr
CSRSI_cvtdcb    EQU X'74'   Offset of CSRSI_cvtdcb
CSRSI_cvtoslv4  EQU X'4F4'  Offset of CSRSI_cvtoslv4
*
CSRSI_cvtosext  EQU X'08'   Mask for CSRSI_cvtdcb.CSRSI_cvtosext
CSRSI_cvtcsrsi  EQU X'80'   Mask for CSRSI_cvtoslv4.CSRSI_cvtcsrsi
*
CSRSI_NOSERVICE EQU 8       Return code: service not available
*
* SYSPARM should be set either in makefile or on command-line to
* switch between 64-bit and 31-bit XPLINK prolog or epilog.
* Branch based on setting of the 64-bit mode.
*
         AIF   ('&SYSPARM' EQ 'BIT64').JMP1
*
* Generate 31-bit XPLINK assembler prolog code with _J9CSRSI as
* the entry point:
* Number of 4-byte input parameters is 1.
* R8 is the base register for establishing addressability.
* Export this function when link edited into a DLL.
*
_J9CSRSI EDCXPRLG PARMWRDS=1,BASEREG=8,EXPORT=YES
*
* Generate code for z/Architecture level
*
         SYSSTATE ARCHLVL=2
*
* Unconditional branching after 31-bit prolog code.
*
         AGO   .JMP2
*
* Does no operation but allows branching to 64-bit XPLINK
* assembler prolog code.
*
.JMP1    ANOP
*
* Generate assembler prolog for AMODE 64 routine with _J9CSRSI as
* the 64-bit entry marker:
* Number of 8-byte input parameters is 1.
* R8 is the base register for establishing addressability.
* Mark this entry point as an exported DLL function.
*
_J9CSRSI CELQPRLG PARMWRDS=1,BASEREG=8,EXPORT=YES
*
* Generate code for z/Architecture level
*
         SYSSTATE ARCHLVL=2
*
* Set addressing mode to 31-bit.
*
         SAM31
*
* Does no operation but allows code continuation after 31-bit
* XPLINK assembler prolog code.
*
.JMP2    ANOP
_J9CSRSI ALIAS C'j9csrsi_wrp'
*
* Set the value of base registers
*
         LR    R2,R1
         LA    R3,4095(,R1)
         LA    R9,4095(,R3)
         LA    R6,4095(,R9)
         LA    R10,4095(,R6)
*
* Establish addressability with IOARGS as the base address.
*
         USING IOARGS,R2
         USING IOARGS+4095,R3
         USING IOARGS+8190,R9
         USING IOARGS+12285,R6
         USING IOARGS+16380,R10
*
* Assume the service is not available: the call to CSRSI will update
* the return code accordingly.
*
         LA    R0,CSRSI_NOSERVICE
         ST    R0,RETC
*
* Because we use the second technique to call CSRSI described at [1],
* we must verify that the service is available by checking that both
* CVTOSEXT and CVTCSRSI bits are set in the CVT.
*
* [1] https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.1.0
*                 /com.ibm.zos.v2r1.ieaa700/CSRSI_Description.htm
*
         L     R15,CVT_ADDR
*
* check that CSRSI_cvtdcb.CSRSI_cvtosext is set
*
         TM    CSRSI_cvtdcb(R15),CSRSI_cvtosext
         JE    @1LEXIT
*
* check that CSRSI_cvtoslv4.CSRSI_cvtcsrsi is set
*
         TM    CSRSI_cvtoslv4(R15),CSRSI_cvtcsrsi
         JE    @1LEXIT
*
* CALL macro expects R15 contains the entry address of
* the called program.
*
         L     R15,CSRSI_cvtcsrt(R15)
         L     R15,CSRSI_addr(R15)
*
* Initiate executable form of the CALL macro.
* Pass set of addresses to CALL macro as per CSRSI
* requirements:
* (input) request code,
* (input) info area length,
* (input/output) info area, and
* (output) return code.
* Build a 4-byte entry parameter list.
* Construct a remote problem program parameter list
* using the address specified in the MF parameter.
*
         CALL  (15),(REQ,INFOAL,INFOAREA,RETC),PLIST4=YES,MF=(E,CPLIST)
*
         DROP  R2,R3,R9,R6,R10
*
@1LEXIT  DS    0H
*
* Branch based on setting of the 64-bit mode.
*
         AIF   ('&SYSPARM' EQ 'BIT64').JMP3
*
* Generate 31-bit assembler epilog code.
*
         EDCXEPLG
*
* Unconditional branching after 31-bit epilog code.
*
         AGO   .JMP4
*
* Does no operation but allows branching to address
* mode reset instruction in 64-bit mode.
*
.JMP3    ANOP
*
* Switch address mode back to 64-bit.
*
         SAM64
*
* Terminate AMODE 64 routine by generating appropriate
* structures.
*
         CELQEPLG
*
* Does no operation but allows branching to program end.
*
.JMP4    ANOP
*
* Dummy control section for input/output arguments.
*
IOARGS   DSECT
*
* Reserve storage for CSRSI input/output arguments as
* well as space for constructing CALL parameter list.
*
REQ      DS    A                     request id
INFOAL   DS    A                     infoarea length
RETC     DS    A                     return code
INFOAREA DS    CL(X'4040')           infoarea buffer
CPLIST   CALL  ,(,,,),MF=L           4-argument parameter list
*
* End the program assembly.
*
         END
