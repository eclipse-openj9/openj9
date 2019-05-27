ifdef(`J9ZOS390',`dnl
define(`ZZ',`**')
',`dnl
define(`ZZ',`##')
')dnl

ZZ Copyright (c) 2000, 2019 IBM Corp. and others
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


changequote([,])dnl

ifdef([J9ZOS390],[dnl

        TITLE 'Math.s'

MATH#START      CSECT
MATH#START      RMODE ANY
ifdef([TR_HOST_64BIT],[dnl
MATH#START      AMODE 64
],[dnl
MATH#START      AMODE 31
])dnl
])dnl

define([MATH_M4],[1])

ZZ ===================================================================
ZZ codert/jilconsts.inc is a j9 assembler include that defines
ZZ offsets of various fields in structs used by jit
ZZ ===================================================================

include([jilconsts.inc])dnl


include([z/runtime/s390_macros.inc])dnl
include([z/runtime/s390_asdef.inc])dnl

ZZ ===================================================================
ZZ codert.dev/codertTOC.inc is a JIT assembler include that defines
ZZ index of various runtime helper functions addressable from
ZZ codertTOC.
ZZ ===================================================================

include([runtime/Helpers.inc])dnl

SETPPA2()

ZZ ===================================================================
ZZ for checking that codertTOC.inc was updated.  Simply return the
ZZ constant.  Called as a C func from C++ code
ZZ ===================================================================
START_FUNC(_getnumRTHelpers,_GNUMRH)
  lhi CRINT,TR_S390numRuntimeHelpers
ifdef([J9ZOS390],[dnl
  b  RETURNOFFSET(CRA)
],[dnl
  br  CRA
])dnl
END_FUNC(_getnumRTHelpers,_GNUMRH,9)

ZZ ===================================================================
ZZ This is a zLinux & zOS helper that stores the STFLE bits
ZZ into the passed memory reference.  Called as a C func
ZZ from C++ code.
ZZ
ZZ The stfle instruction is encoded as 0xb2b00000 in binary, leaving
ZZ it in as such so that we can compile on any platform.
ZZ ===================================================================
START_FUNC(_getSTFLEBits,_GSTFLE)
  LR_GPR   r0,CARG1
ifdef([J9ZOS390],[dnl
  DC HEX(b2b02000)
  b  RETURNOFFSET(CRA)
],[dnl
.long HEX(b2b03000)
  br  CRA
])dnl
END_FUNC(_getSTFLEBits,_GSTFLE,9)

ZZ ===================================================================
ZZ This is a zOS helper function that
ZZ     returns 1 if the task is running in problem state mode, and
ZZ     returns 0 if the task is running in supervisor mode
ZZ ===================================================================
START_FUNC(_isPSWInProblemState,_PROBST)
  XR_GPR CRINT,CRINT
  EPSW CRINT,CARG2
  SRL CRINT,16
  NILL CRINT,HEX(0001)
ifdef([J9ZOS390],[dnl
  b  RETURNOFFSET(CRA)
],[dnl
  br  CRA
])dnl
END_FUNC(_isPSWInProblemState,_PROBST,9)

ZZ  This is a generic helper for performing long Remainder
ZZ
ZZ  The function uses the following linkage:
ZZ
ZZ  Params:
ZZ    Dividend    [QotH|QotL]   [GPR2  | GPR3 ]
ZZ    --------  = ----------- = ---------------
ZZ    Divisor     [DivH|DivL]   [GPR8 | GPR9]
ZZ
ZZ  Return:
ZZ     Remainder =  [GPR2|GPR3]
ZZ
ZZ  Scratch:
ZZ     cntreg   = GPR0
ZZ     basereg  = GPR1
ZZ
ZZ
ZZ Register definition
SETVAL(cntreg,0)
SETVAL(breg,1)
SETVAL(RemH,10)
SETVAL(RemL,11)
SETVAL(QotH,2)
SETVAL(QotL,3)
SETVAL(DivH,8)
SETVAL(DivL,9)
SETVAL(SgnDD,rEP)
SETVAL(SgnDS,1)

ZZ Static data struct
SETVAL(eq_CNSTNEG128,0)

START_FUNC(_longRemainder,_LREM)

ZZ Save sign of dividend and take abs value
    xr   SgnDD,SgnDD
    chi  QotH,0
    jnm  LREM201
    lhi  SgnDD,-1
    lcr  QotH,QotH
    lcr  QotL,QotL
    je   LREM201
    ahi  QotH,-1
LABEL(LREM201)

ZZ Save sign or divisor and take abs value
    xr    SgnDS,SgnDS
    chi  DivH,0
    jnm  LREM301
    lhi  SgnDS,-1
    lcr  DivH,DivH
    lcr  DivL,DivL
    je   LREM301
    ahi  DivH,-1
LABEL(LREM301)


ZZ Check if Divisor < 2**31

    ltr   DivH,DivH        # DivH = 0?
    jnz   LREM2         # no, proceed to case 2
    ltr   DivL,DivL        # DivL < 2**31?
    jm    LREM2         # no, proceed to case 2

ZZ Case 1) Divisor < 2**31

ZZ  QotH < 0? (true iff Divident is 0x8000000000000000)
    ltr   QotH,QotH
    jm    LREMMMax  # Special Case 0x800...00 / 0xff...ff = 0x800...00
    lr    RemH,QotH        # load QotH
    srda  RemH,32
    dr    RemH,DivL        # QotH / DivL
    lr    RemL,QotL        # load QotL
    srda  RemH,16          # v1 = ((QotH%Div) << 32 + QotL) >> 16
    dr    RemH,DivL        # v1 / DivL
    lr    RemL,QotL        # load QotL
    sll   RemL,16          # remove upperhalf of QotL
    srda  RemH,16          # v1" = (v1%Div) << 16 + QotL.low
    dr    RemH,DivL        # v1" / Div
    lr    QotL,RemH        # result
    slr   QotH,QotH
    j     LREMSign

ZZ Special Case: 0x80000000000000 as dividend
LABEL(LREMMMax)
    la    cntreg,1
    clr   DivL,cntreg           # is divisor = 1 or -1?
    jne   LREMMMax2     # no, continue
    slr   QotH,QotH        # Remainder is 0.
    j     LREMExit
LABEL(LREMMMax2)
    bctr  QotH,0           # -(-max+1)
    bctr  QotL,0
    lr    RemH,QotH        # load QotH.
    srda  RemH,32          # QotH / DivL
    dr    RemH,DivL        # QotH / DivL
    lr    RemL,QotL        # load QotL
    srda  RemH,16          # v1 = ((QotH%Div) << 32 + QotL) >> 16
    dr    RemH,DivL        # v1 / DivL
    lr    RemL,QotL        # load QotL
    sll   RemL,16          # remove upperhalf of QotL
    srda  RemH,16          # v1" = (v1%Div) << 16 + QotL.low
    dr    RemH,DivL        # v1" / Div
    slr   QotH,QotH         # results = 0
    la    QotL,1(,RemH)    # Quot%Div + 1
    clr   QotL,DivL        # Quot%Div + 1 < Div?
    jl    LREMSign             # yes, adjust sign
    slr   QotL,QotL        # no, result = 0
    j     LREMExit

ZZ    case 2) Divident < Divisor (result = Dividend)
ZZ    case 3) Divisor >= 2**31, Dividend.w1 = Divisor.w1,
ZZ            Dividend >= Divisor
ZZ            (result=dividend.w2 - divisor.w2)
LABEL(LREM2)
    clr   QotH,DivH         # Dividend.w1 : Divisor.w1
    jh    LREM4          # Dividend.w1 > Divisor.w1, not the case.
    jl    LREMSign       # Result is just Dividend.
    clr   QotL,DivL         # is Dividend < Divisor?
    jl    LREMSign       # yes
    slr   QotH,QotH          # save result = 0
    slr   QotL,DivL         # result = dividend - divisor
    j     LREMSign              # Adjust sign and exit.


ZZ Check if 2**31 <= divisor < 2**32
LABEL(LREM4)
    ltr   DivH,DivH         # divisor high bits =?
    jnz   LREM5                 # no, not the case.

ZZ    Case 4) 2**31 <= divisor < 2**32
    lr    RemH,QotH
    lr    RemL,QotL
    srdl  RemH,16+1         # Quotient" = Quotient>>1>>16
    lr    cntreg,DivL
    srl   cntreg,1          # Divisor" = Divisor>>1
    dr    RemH,cntreg       # Quotient"/Divisor"
    lr    DivH,RemL
    sll   DivH,16           # (Quotient"/Divisor")<<16 (< 2**32)
ZZ begin Defect 112396
ZZ  Check if the result of division is >= 2^32
    srl   RemL,16
    chi   RemL,1
    jz    LOverflowEdge
ZZ end Defect 112396
    lr    RemL,QotL         # QotL
    sll   RemL,16-1         # remove upper 15 bits

ZZ  Divisor"" = Divisor"%Quotient"<<16+DivL.low>>1

    srdl  RemH,16
    dr    RemH,cntreg       # Divisor""/Quotient"
    alr   DivH,RemL         # result.w2
    la    cntreg,1
    nr    cntreg,DivL       # Divisor = even?
    jnz   LREMLOdd       # no
    sll   QotL,31       # Quotient - Quotient>>1<<1
    srl   QotL,31       # Quotient - Quotient>>1<<1
    sll   RemH,1             # Quotient>>1%Divisor>>1<<1
    alr   QotL,RemH         # Quotient%Divisor
    slr   QotH,QotH
    j     LREMSign
LABEL(LREMLOdd)
    slr   QotH,QotH
    nr    QotL,cntreg       # Quotient & 1

ZZ  Divisor""%Quotient"<<1 (= Divisor"%Quotient"<<1

    sll   RemH,1
    alr   QotL,RemH         # Divisor%(Quotient-1)
    slr   QotL,DivH         # Divisor%(Quotient-1)-result > 0?
    brc    2+1,LREMSign         # yes,done
    alr   QotL,DivL         # adjust result
    brc    2+1,LREMSign         # result > 0, done    /*ibm.9819*/
    alr   QotL,DivL         # adjust result       /*ibm.9819*/
    j     LREMSign
ZZ begin Defect 112396
ZZ Materialize the result for two edge cases
LABEL(LOverflowEdge)
    xr    QotH,QotH
    xr    QotL,QotL
    la    cntreg,1
    nr    cntreg,DivL       # Divisor = even?
    jz    LREMExit       # no
    lhi   QotH,-1
    lhi   QotL,-2
    j     LREMExit
ZZ end Defect 112396


ZZ  Case 5:  divisor >= 2**32
LABEL(LREM5)

ZZ begin Defect 112396
ZZ check Special Cases dividend = 0x8000000000000000
ZZ       and divH = 0x1 and divL = {0,1,2,3}
ZZ  QotH < 0? (true iff Divident is 0x8000000000000000)
    ltr   QotH,QotH
    jnm   LREMCont
    chi   DivH,1
    jne   LREMCont
    chi   DivL,0
    je    LREMMMaxEdge0
    chi   DivL,1
    je    LREMMMaxEdge1
    chi   DivL,2
    je    LREMMMaxEdge2
    chi   DivL,3
    je    LREMMMaxEdge3
LABEL(LREMCont)
ZZ end   Defect 112396

    lr    RemH,DivH          # DivH
    la    breg,1             # #shift=1
LABEL(LREMLoop)
    la    breg,1(,breg)      # #shift++
    sra   RemH,1             # DivH>>1
    jnz   LREMLoop       # repeat until DivH = 0
    lr    RemH,DivH
    lr    RemL,DivL
    srdl  RemH,0(breg)       # Divisor" = Divisor>>ZZshift
    lr    cntreg,RemL        # save it
    lr    RemH,QotH
    lr    RemL,QotL
    srdl  RemH,0(breg)       # Quotient" = Quotient>>ZZshift
    dr    RemH,cntreg        # Divisor"/Quotient"
    lr    cntreg,RemL        # save result
    lr    RemL,DivH         # DivH
    mr    RemH,cntreg       # DivH x result
    lr    breg,RemL         # save it (< 2**31)
    ltr   RemL,DivL         # DivL >= 2**31?
    jm    LREMMLow       # yes
    mr    RemH,cntreg       # DivL x result
    ar    RemH,breg         # (Divisor x result).w1
    j     LREMAdjust
LABEL(LREMMLow)
    srl   RemL,1            # DivL>>1
    mr    RemH,cntreg        # (DivL>>1) x result
    sldl  RemH,1
    ar    RemH,breg          # (DivL x result).w1
    la    breg,1
    nr    breg,DivL         # Divisor = odd?
    jz    LREMAdjust     # no
    alr   RemL,cntreg       # (DivL x result).w2
    brc    8+4,LREMAdjust # no carry
    la    RemH,1(,RemH)     # adjust for carry
LABEL(LREMAdjust)
    slr   QotL,RemL         # Quotient-(Divisor x result)
    brc    2+1,LREMCC
    bctr  QotH,0
LABEL(LREMCC)
    slr   QotH,RemH         # remainder < 0?
    jnm   LREMSign       # no
    alr   QotL,DivL         # Quotient-(Divisor x (result-1))
    brc    8+4,LREMCC2
    ahi   QotH,1
LABEL(LREMCC2)
    ar    QotH,DivH

ZZ Fix the sign of the result.
LABEL(LREMSign)
    ltr   SgnDD,SgnDD      # See if divident is negative.
    jz    LREMExit      # no, exit
    lcr   QotH,QotH        # result.w1 = -result.w1
    ltr   QotL,QotL        # result.w2 = 0?
    jz    LREMExit
    bctr  QotH,0           # subtract 1 for carry
    lcr   QotL,QotL        # result.w2 = -result.w2

ZZ begin Defect 112396

    j     LREMExit
ZZ  Special cases
ZZ  dividend = 0x8000000000000000 and
ZZ      divH = 0x2 and divL = {0,1,2,3}
LABEL(LREMMMaxEdge0)
ZZ divL = 0 => rem = 0
    LHI QotH,0
    LHI QotL,0
    j     LREMExit
LABEL(LREMMMaxEdge1)
ZZ divL = 1 => rem = -2147483649  (0xFFFF FFFF 7FFF FFFF)
    LHI QotH,-1
    LHI QotL,-1
    SRL QotL,1
    j     LREMExit
LABEL(LREMMMaxEdge2)
ZZ divL = 2 => rem = -2           (0xFFFF FFFF FFFF FFFE)
    LHI QotH,-1
    LHI QotL,-2
    j     LREMExit
LABEL(LREMMMaxEdge3)
ZZ divL = 3 => rem = -2147483654  (0xFFFF FFFF 7FFF FFFA)
    LHI QotH,-1
    LHI QotL,-12
    SRL QotL,1
    j     LREMExit  #Just in case somebody adds code after this

ZZ end   Defect 112396

ZZ Exit Code.
LABEL(LREMExit)
    br    r14               #return.

    END_FUNC(_longRemainder,_LREM,7)


ZZ  This is a generic helper for performing long division
ZZ
ZZ
ZZ  The function uses the following linkage:
ZZ
ZZ  Params:
ZZ    Dividend    [QotH|QotL]   [GPR2  | GPR3 ]
ZZ    --------  = ----------- = ---------------
ZZ    Divisor     [DivH|DivL]   [GPR8 | GPR9]
ZZ
ZZ  Return:
ZZ     Dividend = [GPR2|GPR3]
ZZ
ZZ  Scratch:
ZZ     cntreg   = GPR0
ZZ     basereg  = GPR1
ZZ
ZZ  Note: Remainder is not calculated in long division routine.
ZZ
ZZ
ZZ Register definition

    START_FUNC(_longDivide,_LDIV)

ZZ Save sign of dividend and take abs value
    xr   SgnDD,SgnDD
    chi  QotH,0
    jnm  LDIV201
    lhi  SgnDD,-1
    lcr  QotH,QotH
    lcr  QotL,QotL
    je   LDIV201
    ahi  QotH,-1
LABEL(LDIV201)

ZZ Save sign or divisor and take abs value
    xr    SgnDS,SgnDS
    chi  DivH,0
    jnm  LDIV301
    lhi  SgnDS,-1
    xr   SgnDD,SgnDS
    lcr  DivH,DivH
    lcr  DivL,DivL
    je   LDIV301
    ahi  DivH,-1
LABEL(LDIV301)


ZZ Check if Divisor < 2**31

    ltr   DivH,DivH        # DivH = 0?
    jnz   LDIV2         # no, proceed to case 2
    ltr   DivL,DivL        # DivL < 2**31?
    jm    LDIV2         # no, proceed to case 2

ZZ Case 1) Divisor < 2**31

ZZ  QotH < 0? (true iff Divident is 0x8000000000000000)

    ltr   QotH,QotH
    jm    LDIVMMax  # Special Case 0x800...00 / 0xff...ff = 0x800...00
    lr    RemH,QotH        # load QotH
    srda  RemH,32
    dr    RemH,DivL        # QotH / DivL
    lr    QotH,RemL        # save result (high)
    lr    RemL,QotL        # load QotL
    srda  RemH,16          # v1 = ((QotH%Div) << 32 + QotL) >> 16
    dr    RemH,DivL        # v1 / DivL
    sll   RemL,16          # (v1 << Div) << 16 (< 2**32)
    lr    cntreg,RemL      # save result
    lr    RemL,QotL        # load QotL
    lr    QotL,cntreg      # save result (low)
    sll   RemL,16          # remove upperhalf of QotL
    srda  RemH,16          # v1" = (v1%Div) << 16 + QotL.low
    dr    RemH,DivL        # v1" / Div
    alr   QotL,RemL        # save result (low)
    j     LDIVSign

ZZ Special Case: 0x80000000000000 as dividend
LABEL(LDIVMMax)
    la    cntreg,1
    clr   DivL,cntreg           # is divisor = 1 or -1?
    je    LDIVExit      # if so, result is just 0x800000000000000.
    bctr  QotH,0           # -(-max+1)
    bctr  QotL,0
    lr    RemH,QotH        # load QotH.
    srda  RemH,32          # QotH / DivL
    dr    RemH,DivL        # QotH / DivL
    lr    QotH,RemL         # save result (high)
    lr    RemL,QotL        # load QotL
    srda  RemH,16          # v1 = ((QotH%Div) << 32 + QotL) >> 16
    dr    RemH,DivL        # v1 / DivL
    sll   RemL,16          # (v1 << Div) << 16 (< 2**32)
    lr    cntreg,RemL      # save result
    lr    RemL,QotL        # load QotL
    lr    QotL,cntreg      # save result (low)
    sll   RemL,16          # remove upperhalf of QotL
    srda  RemH,16          # v1" = (v1%Div) << 16 + QotL.low
    dr    RemH,DivL        # v1" / Div
    alr   QotL,RemL        # save result low.
    la    RemH,1(,RemH)    # Quot%Div + 1
    clr   RemH,DivL        # Quot%Div + 1 < Div?
    jl    LDIVSign             # yes, adjust sign
    slr   QotL,QotL        # otherwise,result++
    ahi   QotH,1
    j     LDIVSign

ZZ    case 2) Divident < Divisor (result =0)
ZZ    case 3) Divisor >= 2**31, Dividend.w1 = Divisor.w1,
ZZ            Dividend >= Divisor (result=1)

LABEL(LDIV2)
    clr   QotH,DivH         # Dividend.w1 : Divisor.w1
    jh    LDIV4          # Dividend.w1 > Divisor.w1, not the case.
    jl    LDIVZero       # Result = 0, dividend < divisor
    clr   QotL,DivL         # is Dividend < Divisor?
    jl    LDIVZero       # yes
    slr   QotH,QotH          # save result = 0
    la    QotL,1             # result =1
    j     LDIVSign              # Adjust sign and exit.

ZZ  Put result as zero.
LABEL(LDIVZero)
    slr   QotH,QotH         # result = 0
    slr   QotL,QotL
    j     LDIVExit

ZZ Check if 2**31 <= divisor < 2**32
LABEL(LDIV4)
    ltr   DivH,DivH         # divisor high bits =?
    jnz   LDIV5                 # no, not the case.

ZZ    Case 4) 2**31 <= divisor < 2**32
    lr    RemH,QotH
    lr    RemL,QotL
    srdl  RemH,16+1         # Quotient" = Quotient>>1>>16
    lr    cntreg,DivL
    srl   cntreg,1          # Divisor" = Divisor>>1
    dr    RemH,cntreg       # Quotient"/Divisor"
ZZ begin Defect 112396 : Use reg pair Qot to hold result
ZZ                       instead of single reg DivH
    lr    DivH,QotL         # DivH <- QotL
    lr    QotL,RemL         # QotL where DivH
    xr    QotH,QotH
    sldl  QotH,16           # (Quotient"/Divisor")<<16
    lr    RemL,DivH         # original QotL
    sll   RemL,16-1         # remove upper 15 bits

ZZ  # Divisor"" = Divisor"%Quotient"<<16+DivL.low>>1

    srdl  RemH,16
    dr    RemH,cntreg       # Divisor""/Quotient"
    alr   QotL,RemL         # result.w2
    la    cntreg,1
    nr    cntreg,DivL       # Divisor = even?
    jz    LDIVSign          # yes, done
    nr    cntreg,DivH        # Quotient & 1

ZZ  # Divisor""%Quotient"<<1 (= Divisor"%Quotient"<<1)

    sll   RemH,1
    or    RemH,cntreg        # Divisor%(Quotient-1)
    chi   QotH,1
    jz    LAdjustByOne
    clr   RemH,QotL          # Divisor%(Quotient-1) >= result?
    jnl   LDIVSign           # yes, done
LABEL(LAdjustByOne)
    bctr  QotL,0             # adjust result
    chi   QotH,1
    jnz   LAdjustCont
    lhi   QotH,0
LABEL(LAdjustCont)
    alr   RemH,DivL       # adjusted Divisor%(Quotient-1)
    brc    2+1,LDIVSign  # >= result, done
    chi   QotH,1
    jz   LAdjustByOneAgain
    clr   RemH,QotL          # still < result?
    jnl   LDIVSign       # no, done
LABEL(LAdjustByOneAgain)
    bctr  QotL,0             # adjust result
    j     LDIVSign
ZZ end   Defect 112396

ZZ  Case 5)  divisor >= 2**32
LABEL(LDIV5)

ZZ begin Defect 112396
ZZ check Special Cases dividend = 0x8000000000000000
ZZ       and divH = 0x1 and divL = {0,1,2,3}
ZZ  QotH < 0? (true iff Divident is 0x8000000000000000)
    ltr   QotH,QotH
    jnm   LDIVCont
    chi   DivH,1
    jne   LDIVCont
    chi   DivL,0
    je    LDIVMMaxEdge0
    chi   DivL,1
    je    LDIVMMaxEdge1
    chi   DivL,2
    je    LDIVMMaxEdge2
    chi   DivL,3
    je    LDIVMMaxEdge3
LABEL(LDIVCont)
ZZ end   Defect 112396

    lr    RemH,DivH          # DivH
    la    breg,1             # #shift=1
LABEL(LDIVLoop)
    la    breg,1(,breg)      # #shift++
    sra   RemH,1             # DivH>>1
    jnz   LDIVLoop       # repeat until DivH = 0
    lr    RemH,DivH
    lr    RemL,DivL
    srdl  RemH,0(breg)       # Divisor" = Divisor>>ZZshift
    lr    cntreg,RemL        # save it
    lr    RemH,QotH
    lr    RemL,QotL
    srdl  RemH,0(breg)       # Quotient" = Quotient>>ZZshift
    dr    RemH,cntreg        # Divisor"/Quotient"
    lr    cntreg,RemL        # save result
    lr    RemL,DivH         # DivH
    mr    RemH,cntreg       # DivH x result
    lr    breg,RemL         # save it (< 2**31)
    ltr   RemL,DivL         # DivL >= 2**31?
    jm    LDIVMLow       # yes
    mr    RemH,cntreg       # DivL x result
    ar    RemH,breg         # (Divisor x result).w1
    j     LDIVAdjust
LABEL(LDIVMLow)
    srl   RemL,1            # DivL>>1
    mr    RemH,cntreg        # (DivL>>1) x result
    sldl  RemH,1
    ar    RemH,breg          # (DivL x result).w1
    la    breg,1
    nr    breg,DivL         # Divisor = odd?
    jz    LDIVAdjust     # no
    alr   RemL,cntreg       # (DivL x result).w2
    brc    8+4,LDIVAdjust # no carry
    la    RemH,1(,RemH)     # adjust for carry
LABEL(LDIVAdjust)
    lr    DivH,cntreg        # result
    clr   QotH,RemH          # Quotient >= result x Divisor?
    jh    LDIVSetR       # yes, done
    jl    LDIVMinus
    clr   QotL,RemL
    jnl   LDIVSetR       # yes, done
LABEL(LDIVMinus)
    bctr  DivH,0             # adjust result
LABEL(LDIVSetR)
    lr    QotL,DivH
    slr   QotH,QotH         # result.w1 = 0

ZZ Fix the sign of the result.
LABEL(LDIVSign)
    ltr   SgnDD,SgnDD      # See if divident is negative.
    jz    LDIVExit      # no, exit
    lcr   QotH,QotH        # result.w1 = -result.w1
    ltr   QotL,QotL        # result.w2 = 0?
    jz    LDIVExit
    bctr  QotH,0           # subtract 1 for carry
    lcr   QotL,QotL        # result.w2 = -result.w2

ZZ begin Defect 112396

    j     LDIVExit
ZZ  Special cases
ZZ  dividend = 0x8000000000000000 and
ZZ      divH = 0x1 and divL = {0,1,2,3}

LABEL(LDIVMMaxEdge0)
ZZ divL = 0 => div = -2147483648  (0x8000 0000)
    LHI QotH,1
    LHI QotL,0
    srdl  QotH,1
    j     LDIVSign
LABEL(LDIVMMaxEdge1)
ZZ divL = 1 => div = -2147483647  (0x7FFF FFFF)
LABEL(LDIVMMaxEdge2)
ZZ divL = 2 => div = -2147483647  ((0x7FFF FFFF)
    LHI QotH,0
    LHI QotL,-1
    srl  QotL,1
    j     LDIVSign
LABEL(LDIVMMaxEdge3)
ZZ divL = 3 => div = -2147483646  (0x7FFF FFFE)
    LHI QotH,0
    LHI QotL,-1
    sll   QotL,2
    srl   QotL,1
    j     LDIVSign

ZZ end   Defect 112396

ZZ Exit Code.
LABEL(LDIVExit)
    br    r14               #return.

    END_FUNC(_longDivide,_LDIV,7)

SETVAL(rdsa,5)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
SETVAL(dsaSize,32*PTR_SIZE)
],[dnl
SETVAL(dsaSize,16*PTR_SIZE)
])dnl

ZZ
ZZ doubleRemainder(x,y)
ZZ Return x mod y in exact arithmetic
ZZ Calls helper routine jitMathHelperDoubleRemainderDouble
ZZ

    START_FUNC(__doubleRemainder,_DREM)

    ST_GPR   r14,PTR_SIZE(,rdsa)
    AHI_GPR  rdsa,-dsaSize
    STM_GPR  r0,r15,0(rdsa)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,64(rdsa)
])dnl

    LR_GPR  r8,rdsa #save dsa in r8
    ldr     f0,f1

LOAD_ADDR_FROM_TOC(r6,TR_S390jitMathHelperDREM)

ifdef([J9ZOS390],[dnl
ifdef([TR_HOST_64BIT],[dnl
    STD     f0,2176(r4)
    STD     f2,2184(r4)
    LM_GPR r5,r6,0(r6)
    BASR    r7,r6   # Call jitMathHelperDoubleRemainderDouble
    lr      r0,r0     # Nop for XPLINK return
],[dnl
    STD     f0,2112(r4)
    STD     f2,2120(r4)
    L_GPR r12,J9TR_CAA_save_offset(,r4)
    LM_GPR r5,r6,16(r6)
    BASR    r7,r6   # Call jitMathHelperDoubleRemainderDouble
    DC   X'4700',Y((LCALLDESCDRM-(*-8))/8)   * nop desc
])dnl
],[dnl
    BASR    r14,r6  # Call jitMathHelperDoubleRemainderDouble
])dnl

    LR_GPR  rdsa,r8 #restore dsa from r8
    LM_GPR r0,r15,0(rdsa)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,64(rdsa)
])dnl
    AHI_GPR rdsa,dsaSize
    L_GPR  r14,PTR_SIZE(,rdsa)
    br r14

    END_FUNC(__doubleRemainder,_DREM,7)

ZZ
ZZ floatRemainder(x,y)
ZZ Return x mod y in exact arithmetic
ZZ Calls helper routine jitMathHelperFloatRemainderFloat
ZZ

    START_FUNC(__floatRemainder,_FREM)

    ST_GPR   r14,PTR_SIZE(,rdsa)
    AHI_GPR  rdsa,-dsaSize
    STM_GPR  r0,r15,0(rdsa)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,64(rdsa)
])dnl

    LR_GPR  r8,rdsa
    ler     f0,f1

LOAD_ADDR_FROM_TOC(r6,TR_S390jitMathHelperFREM)

ifdef([J9ZOS390],[dnl
ifdef([TR_HOST_64BIT],[dnl
    ste     f0,2180(r4)
    ste     f2,2188(r4)
ZZ load up the C Environment addr into r5 and Entry point in R6
    LM_GPR r5,r6,0(r6)
    BASR    r7,r6     # Call jitMathHelperFloatRemainderFloat
    lr      r0,r0     # Nop for XPLINK return
],[dnl
    ste     f0,2112(r4)
    ste     f2,2116(r4)
    L_GPR r12,J9TR_CAA_save_offset(,r4)
ZZ load up the C Environment addr into r5 and Entry point in R6
    LM_GPR r5,r6,16(r6)
    BASR    r7,r6     # Call jitMathHelperFloatRemainderFloat
    DC   X'4700',Y((LCALLDESCFRM-(*-8))/8)   * nop desc
])dnl
],[dnl
    BASR    r14,r6    # Call jitMathHelperFloatRemainderFloat
])dnl

    LR_GPR  rdsa,r8
    LM_GPR r0,r15,0(rdsa)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR r0,r15,64(rdsa)
])dnl
    AHI_GPR rdsa,dsaSize
    L_GPR  r14,PTR_SIZE(,rdsa)
    br r14

    END_FUNC(__floatRemainder,_FREM,7)

ifdef([J9ZOS390],[dnl
ifdef([TR_HOST_64BIT],[dnl
ZZ 64bit XPLINK doesn't need call descriptors
],[dnl
ZZ Call Descriptor for call to jitMathHelperFloatRemainderFloat
LCALLDESCFRM      DS    0D           * Dword Boundary
        DC    A(Z_FREM-*)            *
        DC    BL.3'000',BL.5'01000'  * XPLINK Linkage+Returns:single
        DC    BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
ZZ                                   unprototyped call

ZZ Call Descriptor for call to jitMathHelperDoubleRemainderDouble
LCALLDESCDRM      DS    0D           * Dword Boundary
        DC    A(Z_DREM-*)            *
        DC    BL.3'000',BL.5'01001'  * XPLINK Linkage+Returns:double
        DC    BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
ZZ                                   unprototyped call
    END
])dnl
])dnl
