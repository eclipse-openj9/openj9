ZZ Copyright (c) 2000, 2017 IBM Corp. and others
ZZ
ZZ This program and the accompanying materials are made 
ZZ available under the terms of the Eclipse Public License 2.0 
ZZ which accompanies this distribution and is available at 
ZZ https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
ZZ Version 2.0 which accompanies this distribution and is available 
ZZ at https://www.apache.org/licenses/LICENSE-2.0.
ZZ
ZZ This Source Code may also be made available under the following
ZZ Secondary Licenses when the conditions for such availability set
ZZ forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
ZZ General Public License, version 2 with the GNU Classpath 
ZZ Exception [1] and GNU General Public License, version 2 with the
ZZ OpenJDK Assembly Exception [2].
ZZ
ZZ [1] https://www.gnu.org/software/classpath/license.html
ZZ [2] http://openjdk.java.net/legal/assembly-exception.html
ZZ
ZZ SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
ZZ GPL-2.0 WITH Classpath-exception-2.0 OR
ZZ LicenseRef-GPL-2.0 WITH Assembly-exception

ZZ ============================================================
ZZ Macro library for Testarossa 390 JIT Development
ZZ
ZZ This file contains macros that are defined
ZZ differently for zOS compared to zLinux
ZZ ============================================================

define(SETPPA2,
[dnl
ifdef([TR_HOST_64BIT],[dnl
      EXTRN CELQSTRT
ZZ PPA2: compile unit block
PPA2   DS    0D
        DC    XL4'03002204'
        DC   AL4(CELQSTRT-PPA2)
        DC   AL4(0)
        DC   AL4(0)
        DC   AL4(0)
        DC   B'10000001'
        DC   XL3'000000'
],[dnl
])dnl
])dnl


ZZ ============================================================

define(SETVAL,
[dnl
$1 EQU $2])dnl

ZZ ============================================================

define(EQUVAL,
[dnl
$1 EQU $2])dnl

ZZ ============================================================

define(CONST_4BYTE,
[dnl
    DC    XL4'$1'])dnl

ZZ ============================================================

define(CONST_4BYTE_WITH_LABEL,
[dnl
$1  DC XL4'$2'])dnl

ZZ ============================================================

define(MACRO_BEGIN_NOARG,
[dnl
        MACRO
&NAME   $1
])dnl

ZZ ============================================================

define(MACRO_END,
[dnl
        MEND
])dnl

ZZ ============================================================

define(LABEL,
[dnl
$1 DS 0H])dnl

ZZ ============================================================

define(HEX,
[dnl
X'$1'])dnl

ZZ ============================================================

define(START_FUNC,
[dnl
ifdef([TR_HOST_64BIT],[dnl
     DS    0D
Z#$2  CSECT
Z#$2  RMODE ANY
Z#$2  AMODE ANY
     DC    0D'0',XL8'00C300C500C500F1'
     DC    AL4(PPAZ$2-Z#$2),AL4(0)
ZZ   START $1
Z$2  DS 0D
     ENTRY Z$2
Z$2  ALIAS C'$1'
Z$2  XATTR SCOPE(X),LINKAGE(XPLINK)

],[dnl

ZZ   START $1
Z$2  DS 0D
     ENTRY Z$2
Z$2  ALIAS C'$1'
Z$2  XATTR SCOPE(X),LINKAGE(XPLINK)
])dnl
ifdef([TR_HOST_64BIT],[dnl
Z$2      AMODE 64
],[dnl
Z$2      AMODE 31
])dnl
])dnl

ZZ ============================================================

define(END_FUNC,
[dnl
ifdef([TR_HOST_64BIT],[dnl

ZZ  Eyecatcher
ZZ   DC AL4(Z$2)
ZZ   DC XL8'005F004A00490054'
ZZ   DC CL$3'Z$2'
ZZ   END  $1

CODELZ$2      EQU *-Z$2
PPAZ$2      DS    0F
        DC    B'00000010'    
        DC    X'CE'         
        DC    XL2'0FFF'        
        DC    AL4(PPA2-PPAZ$2)       
        DC    B'10000000'       
        DC    B'00000000'  
        DC    B'00000000'     
        DC    B'00000001'     
        DC    AL2(0)                
        DC    AL1(0)               
        DC    XL.4'0',AL.4(0)    
        DC    AL4(CODELZ$2)     
        DC    H'$3'       
        DC    CL$3'Z$2'  
        DC    AL4(Z$2-PPAZ$2) 
],[dnl

ZZ  Eyecatcher
     DC AL4(Z$2)
     DC XL8'005F004A00490054'
     DC CL$3'Z$2'
ZZ   END  $1
])dnl
])dnl

ZZ ============================================================

ZZ Entry point Register
EQUVAL(rEP,15)
EQUVAL(rSB,15)

ZZ System Stack pointer Register
EQUVAL(rSSP,4)


ZZ ============================================================
SETVAL(CARG1,1)
SETVAL(CARG2,2)
SETVAL(CARG3,3)
SETVAL(CARG4,99) ZZ invalid
SETVAL(CARG5,99) ZZ invalid
SETVAL(CEP,6)
SETVAL(CRA,7)
ZZ return integer
SETVAL(CRINT,3)
ifdef([TR_HOST_64BIT],[dnl
SETVAL(RETURNOFFSET,2)
],[dnl
SETVAL(RETURNOFFSET,4)
])dnl
