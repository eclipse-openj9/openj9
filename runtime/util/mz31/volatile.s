* Copyright (c) 1991, 2019 IBM Corp. and others
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

        TITLE 'volatile.s'
VOLATILE7BVVC#START      CSECT
VOLATILE7BVVC#START      RMODE ANY
VOLATILE7BVVC#START      AMODE ANY
r0      EQU      0
fp0      EQU      0
r1      EQU      1
fp1      EQU      1
r2      EQU      2
fp2      EQU      2
r3      EQU      3
fp3      EQU      3
r4      EQU      4
fp4      EQU      4
r5      EQU      5
fp5      EQU      5
r6      EQU      6
fp6      EQU      6
r7      EQU      7
fp7      EQU      7
r8      EQU      8
fp8      EQU      8
r9      EQU      9
fp9      EQU      9
r10      EQU      10
fp10      EQU      10
r11      EQU      11
fp11      EQU      11
r12      EQU      12
fp12      EQU      12
r13      EQU      13
fp13      EQU      13
r14      EQU      14
fp14      EQU      14
r15      EQU      15
fp15      EQU      15
SL_BORROW      EQU      B'0100'
SL_NO_BORROW      EQU      B'0011'
AL_CARRY_SET      EQU      B'0011'
AL_CARRY_CLEAR      EQU      B'1100'
ALWAYS      EQU      B'1111'
eq_inline_jni_max_arg_count EQU 32
eq_pointer_size EQU 4
eqS_longVolatileRead EQU 147
eqS_longVolatileWrite EQU 147
eqSR_longVolatileRead EQU 12
eqSR_longVolatileWrite EQU 12
eqSRS_longVolatileRead EQU 48
eqSRS_longVolatileWrite EQU 48
eqSS_longVolatileRead EQU 588
eqSS_longVolatileWrite EQU 588
        EXTRN CEESTART
* PPA2: compile unit block
PPA2   DS    0D                            *
        DC    XL4'03002204'                *  mem-id,mem-subid,mem-defi\
               ned,ctrl-level
        DC   AL4(CEESTART-PPA2)    *  Offset to CEESTART
        DC   AL4(0)                        *  no PPA4
        DC   AL4(0)                        *  no timestamp
        DC   AL4(0)                        *  no primary EP
        DC   B'10000001'                   *  flags: binary FP, XPLINK
        DC   XL3'000000'                   *  reserved
* Prototype: U_64 longVolatileRead(J9VMThread * vmThread, U_64 * srcAdd\
               ress);
* Defined in: #UTIL Args: 2
        DS    0D             * Dword Boundary
EPM#LONGVOLATILEREAD      CSECT
EPM#LONGVOLATILEREAD      RMODE ANY
EPM#LONGVOLATILEREAD      AMODE ANY
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPALONGVOLATILEREAD1-EPM#LONGVOLATILEREAD),AL4(736)  \
                      Offset to PPA1
LONGVOLATILEREAD      DS 0D
        ENTRY LONGVOLATILEREAD
LONGVOLATILEREAD      ALIAS C'longVolatileRead'
LONGVOLATILEREAD      XATTR SCOPE(X),LINKAGE(XPLINK)
*184 slots = 16 fixed + 16 out-args + 1 extra out-arg + 147 locals + 3 \
               extra + 1 alignment
        stm  r4,r15,1312(r4)                      * save volatile regis\
               ters: small frame
        la   r5,2112(r4)                           * set up pointer to \
               stack parms
STK#PPALONGVOLATILEREAD1      EQU (*-LONGVOLATILEREAD)/2
        ahi  r4,-736                            * buy a stack frame
PRLG#PPALONGVOLATILEREAD1      EQU (*-LONGVOLATILEREAD)/2
        st   r1,0(r5)
        st   r2,4(r5)
        bras r1,longVolatileRead_CONSTANTS       * derive base pointer \
               into table
L3    DS    0H
        DS    0F  * ensure that table is full word aligned
L5    DS    0H
longVolatileRead_CONSTANTS    DS    0H
        ahi    r1,L5-L3                          * establish pointer to\
                the table
        lr   r3,r2
        lm   r0,r1,0(r3)
        lr   r3,r1
        lr   r2,r0
        lm   r4,r15,2048(r4)                     * small frame: restore\
               volatile registers (including returnAddress and oldSP)\

        br   r7
CODELLONGVOLATILEREAD      EQU *-LONGVOLATILEREAD
PPALONGVOLATILEREAD1      DS    0F
        DC    B'00000010'      * HPL PPA1 Version 3 - X'02'
        DC    X'CE'                  * HPL LE Eyecatch
        DC    XL2'0FFF'           * GPR Mask (defaults to R4-R15) : sma\
               ll frame
        DC    AL4(PPA2-PPALONGVOLATILEREAD1)        * Offset from this \
               PPA1 to PPA2
        DC    B'00000000'       * Flag1 (parmsize not specified, vararg\
               )
        DC    B'00000000'       * Flag2
        DC    B'00000000'       * Flag3
        DC    B'00000001'       * Flag4
        DC    AL2(2)                 * Parameter Length in words
        DC    AL1(PRLG#PPALONGVOLATILEREAD1)                 * Length o\
               f XPL prolog / 2
        DC    XL.4'0',AL.4(STK#PPALONGVOLATILEREAD1)    * Allc Reg / R4\
                Chng Offs
        DC    AL4(CODELLONGVOLATILEREAD)      * Length of HPL appl code\
                in bytes
        DC    H'16'       * Length of Entry Point Name
        DC    CL16'LONGVOLATILEREAD'    * Entry Point Name
* START of call descriptors for LONGVOLATILEREAD
* END of call descriptors for LONGVOLATILEREAD
* Prototype: void longVolatileWrite(J9VMThread * vmThread, U_64 * destA\
               ddress, U_64 * value);
* Defined in: #UTIL Args: 3
        DS    0D             * Dword Boundary
EPM#LONGVOLATILEWRITE      CSECT
EPM#LONGVOLATILEWRITE      RMODE ANY
EPM#LONGVOLATILEWRITE      AMODE ANY
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPALONGVOLATILEWRITE8-EPM#LONGVOLATILEWRITE),AL4(736)\
                        Offset to PPA1
LONGVOLATILEWRITE      DS 0D
        ENTRY LONGVOLATILEWRITE
LONGVOLATILEWRITE      ALIAS C'longVolatileWrite'
LONGVOLATILEWRITE      XATTR SCOPE(X),LINKAGE(XPLINK)
*184 slots = 16 fixed + 16 out-args + 1 extra out-arg + 147 locals + 3 \
               extra + 1 alignment
        stm  r4,r15,1312(r4)                      * save volatile regis\
               ters: small frame
        la   r5,2112(r4)                           * set up pointer to \
               stack parms
STK#PPALONGVOLATILEWRITE8      EQU (*-LONGVOLATILEWRITE)/2
        ahi  r4,-736                            * buy a stack frame
PRLG#PPALONGVOLATILEWRITE8      EQU (*-LONGVOLATILEWRITE)/2
        st   r1,0(r5)
        st   r2,4(r5)
        st   r3,8(r5)
        bras r1,longVolatileWrite_CONSTANTS      * derive base pointer \
               into table
L10    DS    0H
        DS    0F  * ensure that table is full word aligned
L12    DS    0H
longVolatileWrite_CONSTANTS    DS    0H
        ahi    r1,L12-L10                          * establish pointer \
               to the table
        lr   r5,r2
        l    r1,0(r3)
        l    r2,4(r3)
        lr   r0,r1
        lr   r1,r2
        stm  r0,r1,0(r5)
        bcr ALWAYS,r0 # readwrite barrier
        lm   r4,r15,2048(r4)                     * small frame: restore\
               volatile registers (including returnAddress and oldSP)\

        br   r7
CODELLONGVOLATILEWRITE      EQU *-LONGVOLATILEWRITE
PPALONGVOLATILEWRITE8      DS    0F
        DC    B'00000010'      * HPL PPA1 Version 3 - X'02'
        DC    X'CE'                  * HPL LE Eyecatch
        DC    XL2'0FFF'           * GPR Mask (defaults to R4-R15) : sma\
               ll frame
        DC    AL4(PPA2-PPALONGVOLATILEWRITE8)        * Offset from this\
                PPA1 to PPA2
        DC    B'00000000'       * Flag1 (parmsize not specified, vararg\
               )
        DC    B'00000000'       * Flag2
        DC    B'00000000'       * Flag3
        DC    B'00000001'       * Flag4
        DC    AL2(3)                 * Parameter Length in words
        DC    AL1(PRLG#PPALONGVOLATILEWRITE8)                 * Length \
               of XPL prolog / 2
        DC    XL.4'0',AL.4(STK#PPALONGVOLATILEWRITE8)    * Allc Reg / R\
               4 Chng Offs
        DC    AL4(CODELLONGVOLATILEWRITE)      * Length of HPL appl cod\
               e in bytes
        DC    H'17'       * Length of Entry Point Name
        DC    CL17'LONGVOLATILEWRITE'    * Entry Point Name
* START of call descriptors for LONGVOLATILEWRITE
* END of call descriptors for LONGVOLATILEWRITE
        END
