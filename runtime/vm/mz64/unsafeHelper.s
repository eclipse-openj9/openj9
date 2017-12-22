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

        TITLE 'unsafeHelper.s'
UNSAFEHELPER7BVVC#START      CSECT
UNSAFEHELPER7BVVC#START      RMODE ANY
UNSAFEHELPER7BVVC#START      AMODE 64
        EXTRN CELQSTRT
* PPA2: compile unit block
PPA2    DS    0D
        DC    XL4'03002204'
        DC   AL4(CELQSTRT-PPA2)
        DC   AL4(0)
        DC   AL4(0)
        DC   AL4(0)
        DC   B'10000001'
        DC   XL3'000000'
* Prototype: void unsafePut64(I_64 *address, I_64 value);
* Args: 2
        DS    0D
EPM#UNSAFEPUT64       CSECT
EPM#UNSAFEPUT64       RMODE ANY
EPM#UNSAFEPUT64       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEPUT641-EPM#UNSAFEPUT64),AL4(0)
UNSAFEPUT64           DS 0D
        ENTRY UNSAFEPUT64
UNSAFEPUT64      ALIAS C'unsafePut64'
UNSAFEPUT64      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        STG      2,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEPUT64      EQU *-UNSAFEPUT64
PPAUNSAFEPUT641       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEPUT641)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(4)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEPUT64)
        DC    H'11'
        DC    CL11'UNSAFEPUT64'
* Prototype: void unsafePut32(I_32 *address, I_32 value);
* Args: 2
        DS    0D
EPM#UNSAFEPUT32       CSECT
EPM#UNSAFEPUT32       RMODE ANY
EPM#UNSAFEPUT32       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEPUT321-EPM#UNSAFEPUT32),AL4(0)
UNSAFEPUT32           DS 0D
        ENTRY UNSAFEPUT32
UNSAFEPUT32      ALIAS C'unsafePut32'
UNSAFEPUT32      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        ST       2,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEPUT32      EQU *-UNSAFEPUT32
PPAUNSAFEPUT321       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEPUT321)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(4)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEPUT32)
        DC    H'11'
        DC    CL11'UNSAFEPUT32'
* Prototype: void unsafePut16(I_16 *address, I_16 value);
* Args: 2
        DS    0D
EPM#UNSAFEPUT16       CSECT
EPM#UNSAFEPUT16       RMODE ANY
EPM#UNSAFEPUT16       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEPUT161-EPM#UNSAFEPUT16),AL4(0)
UNSAFEPUT16           DS 0D
        ENTRY UNSAFEPUT16
UNSAFEPUT16      ALIAS C'unsafePut16'
UNSAFEPUT16      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        STH      2,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEPUT16      EQU *-UNSAFEPUT16
PPAUNSAFEPUT161       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEPUT161)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(4)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEPUT16)
        DC    H'11'
        DC    CL11'UNSAFEPUT16'
* Prototype: void unsafePut8(I_8 *address, I_8 value);
* Args: 2
        DS    0D
EPM#UNSAFEPUT8       CSECT
EPM#UNSAFEPUT8       RMODE ANY
EPM#UNSAFEPUT8       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEPUT81-EPM#UNSAFEPUT8),AL4(0)
UNSAFEPUT8           DS 0D
        ENTRY UNSAFEPUT8
UNSAFEPUT8      ALIAS C'unsafePut8'
UNSAFEPUT8      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        STC      2,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEPUT8      EQU *-UNSAFEPUT8
PPAUNSAFEPUT81       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEPUT81)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(4)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEPUT8)
        DC    H'10'
        DC    CL10'UNSAFEPUT8'
* Prototype: I_64 unsafeGet64(I_64 *address);
* Args: 1
        DS    0D
EPM#UNSAFEGET64       CSECT
EPM#UNSAFEGET64       RMODE ANY
EPM#UNSAFEGET64       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEGET641-EPM#UNSAFEGET64),AL4(0)
UNSAFEGET64           DS 0D
        ENTRY UNSAFEGET64
UNSAFEGET64      ALIAS C'unsafeGet64'
UNSAFEGET64      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        LG       3,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEGET64      EQU *-UNSAFEGET64
PPAUNSAFEGET641       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEGET641)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(2)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEGET64)
        DC    H'11'
        DC    CL11'UNSAFEGET64'
* Prototype: I_32 unsafeGet32(I_32 *address);
* Args: 1
        DS    0D
EPM#UNSAFEGET32       CSECT
EPM#UNSAFEGET32       RMODE ANY
EPM#UNSAFEGET32       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEGET321-EPM#UNSAFEGET32),AL4(0)
UNSAFEGET32           DS 0D
        ENTRY UNSAFEGET32
UNSAFEGET32      ALIAS C'unsafeGet32'
UNSAFEGET32      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        LGF      3,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEGET32      EQU *-UNSAFEGET32
PPAUNSAFEGET321       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEGET321)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(2)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEGET32)
        DC    H'11'
        DC    CL11'UNSAFEGET32'
* Prototype: I_16 unsafeGet16(I_16 *address);
* Args: 1
        DS    0D
EPM#UNSAFEGET16       CSECT
EPM#UNSAFEGET16       RMODE ANY
EPM#UNSAFEGET16       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEGET161-EPM#UNSAFEGET16),AL4(0)
UNSAFEGET16           DS 0D
        ENTRY UNSAFEGET16
UNSAFEGET16      ALIAS C'unsafeGet16'
UNSAFEGET16      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS       0D
        LGH      3,0(,1)
        DS       0H
        B        2(,7)
CODELUNSAFEGET16      EQU *-UNSAFEGET16
PPAUNSAFEGET161       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEGET161)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(2)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEGET16)
        DC    H'11'
        DC    CL11'UNSAFEGET16'
* Prototype: I_8 unsafeGet16(I_8 *address);
* Args: 1
        DS    0D
EPM#UNSAFEGET8       CSECT
EPM#UNSAFEGET8       RMODE ANY
EPM#UNSAFEGET8       AMODE 64
        DC    0D'0',XL8'00C300C500C500F1'
        DC    AL4(PPAUNSAFEGET81-EPM#UNSAFEGET8),AL4(0)
UNSAFEGET8           DS 0D
        ENTRY UNSAFEGET8
UNSAFEGET8      ALIAS C'unsafeGet8'
UNSAFEGET8      XATTR SCOPE(X),LINKAGE(XPLINK)
        DS      0D
        LGB     3,0(,1)
        DS      0H
        B       2(,7)
CODELUNSAFEGET8      EQU *-UNSAFEGET8
PPAUNSAFEGET81       DS    0F
        DC    B'00000010'
        DC    X'CE'
        DC    XL2'0000'
        DC    AL4(PPA2-PPAUNSAFEGET81)
        DC    B'10000000'
        DC    B'00000000'
        DC    B'00000000'
        DC    B'00000001'
        DC    AL2(2)
        DC    AL1(0)
        DC    XL.4'0',AL.4(0)
        DC    AL4(CODELUNSAFEGET8)
        DC    H'10'
        DC    CL10'UNSAFEGET8'
        END
