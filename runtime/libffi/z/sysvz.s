* Copyright (c) 2016, 2019 IBM Corp. and others

         ACONTROL AFPR,FLAG(CONT)

FFISYS   EDCXPRLG DSASIZE=DSASZ,PSECT=ASP,PARMWRDS=7

*FFISYS ARGS:
*r1 <- foreign_function_address (2112)
*r2 <- &ecif (2116)
*r3 <- cif->flags (2120)
*@2124(,4) <- ecif->rvalue
*@2128(,4) <- cif->bytes
*@2132(,4) <- cif->nargs
*@2136(,4) <- cif->arg_types->size

*What: Storing arguments in this routine's
*      local storage for future use
         USING CEEDSAHP,4
         L    R14,0(,R2)             ecif->cif
         L    R14,8(,R14)            cif->arg_types
*Name:    ffi_prep_args_call
*Input:   stack, extended_cif
*Output:  void
*Action:  Is an external call to C-XPLINK routine
*         It saves function's arguments in this
*         routine's local storage
         LA   R1,LSTOR
         LR   R13,R1
         EDCXCALL PREPARGS,WORKREG=10

         LR   R5,R14                 Copy of parameter types
*Reset registers used in code gen
         SR   R0,R0                  GPR counter
         SR   R14,R14                FPR counter
         SR   R7,R7                  Offset in the arg area
         SR   R10,R10                Offset in array of parm types
         SR   R6,R6                  Offset in stored parm values

*Get the cif->nargs from caller's stack
         L    R9,(2112+(((DSASZ+31)/32)*32)+20)(,R4)

*Place arguments passed to the foreign function based on type
ARGLOOP  L    R11,0(R10,R5)          Get pointer to current ffi_type
         LLC  R11,7(R11)             ffi_type->type
         SLL  R11,2                  Find offset in branch tabel
         LA   R15,ATABLE
         L    R15,0(R11,R15)
         BR   R15

*Following code prepares ffi arguments, according to xplink

I        DS   0H                     ffi_type_int
UI32     DS   0H                     ffi_type_uint32
         L    R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump to the next word in arg area
         B    CONT                   Next parameter

UI8      DS   0H                     ffi_type_uint8
SI8      DS   0H                     ffi_type_sint8
         LLC  R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump to the next word in arg area
         B    CONT                   Next parameter

UI16     DS   0H                     ffi_type_uint16
         LLH  R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump to the next word in arg area
         B    CONT                   Next parameter

SI16     DS   0H                     ffi_type_sint16
         LH   R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump to the next word in arg area
         B    CONT                   Next parameter

SI64     DS   0H                     ffi_type_sint64
         L    R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump one word in arg area
         L    R11,4(R6,R13)          Argument value
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump one word in arg area
         AHI  R6,4                   Advance the first 4 bytes of param
         B    CONT                   Next parameter

PTR      DS   0H                     ffi_type_pointer
         L    R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump to the next word in arg area
         B    CONT                   Next parameter

UI64     DS   0H                     ffi_type_uint64
         LLGF R11,0(R6,R13)          Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         ST   R11,0(R7,R15)          Store in next 2 words in arg area
         AHI  R7,4                   Bump one word in arg area
         LLGF R11,4(R6,R13)          Argument value
         ST   R11,0(R7,R15)          Store in next word in arg area
         AHI  R7,4                   Bump one word in arg area
         AHI  R6,4                   Advance the first 4 bytes of param
         B    CONT                   Next parameter

FLT      DS   0H                     ffi_type_float
         LA   R15,FLTS
         LR   R11,R14
         SLL  R11,2                  Pass in fpr or arg area
         L    R15,0(R11,r15)
         CFI  R14,4                  FPRs left to pass parms in?
         BL   INC7L
         LA   R14,4                  Reached max fprs
         B    JL7
INC7L    DS   0H
         AHI  R14,1                  Now we have one less fpr left
         CFI  0,3
         BL   INCGF
         LA   R0,3
         B    JL7
INCGF    DS   0H
         AHI  R0,1
JL7      DS   0H
         BR   R15

FLTR0    DS   0H                     FLOAT passed in fpr0
         LE   FR0,0(R6,R13)
         LER  FR11,FR0
         B    FLTSTR
FLTR2    DS   0H                     FLOAT passed in fpr2
         LE   FR2,0(R6,R13)
         LER  FR11,FR2
         B    FLTSTR
FLTR4    DS   0H                     FLOAT passed in fpr4
         LE   FR4,0(R6,R13)
         LER  FR11,FR4
         B    FLTSTR
FLTR6    DS   0H                     FLOAT passed in fpr6
         LE   FR6,0(R6,R13)
         LER  FR11,FR6
         B    FLTSTR
ARGAFT   DS   0H                     FLOAT in arg area
         LE   FR11,0(R6,R13)         Argument value
FLTSTR   DS   0H
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         STE  FR11,0(R7,R15)         Store in next word in arg area
         AHI  R7,4                   Bump to the next word in arg area
         B    CONT                   Next parameter

D        DS   0H                     ffi_type_double
         LA   R15,FLD
         LR   R11,R14
         SLL  R11,2                  Pass in fpr or arg area
         L    R15,0(R11,R15)
         CFI  R14,4                  FPRs left to pass parms in?
         BL   INC7
         LA   R14,4                  Reached max fprs
         B    J7
INC7     DS   0H
         AHI  R14,1                  Now we have one less fpr left
         CFI  R0,2                   Reached max gprs
         BL   INCGD
         LA   R0,3
         B    J7
INCGD    DS   0H
         AHI  R0,2
J7       DS   0H
         BR   R15

FLDR0    DS   0H                     DOUBLE passed in fpr0
         LD   FR0,0(R6,R13)
         LDR  FR11,FR0
         B    DBLSTR
FLDR2    DS   0H                     DOUBLE passed in fpr2
         LD   FR2,0(R6,R13)
         LDR  FR11,FR2
         B    DBLSTR
FLDR4    DS   0H                     DOUBLE passed in fpr4
         LD   FR4,0(R6,R13)
         LDR  FR11,FR4
         B    DBLSTR
FLDR6    DS   0H                     DOUBLED passed in fpr6
         LD   FR6,0(R6,R13)
         LDR  FR11,FR6
         B    DBLSTR
ARGAFL   DS   0H                     DOUBLE in arg area
         LD   FR11,0(R6,R13)         Argument value
DBLSTR   DS   0H
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         STD  FR11,0(R7,R15)         Store in next 2 words in arg area
         AHI  R7,8                   Bump to next two words in arg area
         AHI  R6,4                   Bump stack extra time
         B    CONT                   Next parameter

LD       DS   0H                     ffi_type_longdouble
         LA   R15,LDD
         SR   R11,R11
         CFI  R14,0                  All linkage fprs are available
         BE   0(R11,R15)             Pass l_DOUBLE in fpr0-fpr2
         LA   R11,8
         CFI  14,4
         BH   0(R11,R15)             Pass l_DOUBLE in arg area
         LA   R11,4
         L    R15,0(R11,R15)         Pass l_DOUBLE in fpr4-fpr6
         BR   R15

DFPR0    DS   0H                     l_DOUBLE passed in fpr0-fpr2
         LD   FR0,0(R6,R13)
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         STD  FR0,0(R7,R15)          Store the first 8bytes in arg area
         AHI  R6,8                   Bump stack to next parameter
         LD   FR2,0(R6,R13)          l_DOUBLE passed in fpr0-fpr2
         STD  FR6,8(R7,R15)
         AHI  R6,4                   Bump stack once here, once at end
         AHI  R7,16                  Advance two slots
         AHI  R14,2                  Now we have two less fprs left
         B    CONT                   Next Parameter
DFPR4    DS   0H                     l_DOUBLE passed in fpr4-fpr6
         LD   FR4,0(R6,R13)
         AHI  R6,8                   Bump stack to next parameter
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         STD  FR4,0(R7,R15)          Store the first 8bytes in arg area
         LD   FR6,0(R6,R13)          l_DOUBLE passed in fpr4-fpr6
         STD  FR6,8(R7,R15)
         AHI  R6,4                   Bump stack once here, once at end
         AHI  R7,16                  Advance two slots
         AHI  R14,2                  Now we have two less fprs left
         B    CONT                   Next parameter
DARGF    DS   0H                     l_DOUBLE in arg area
         LD   FR11,0(R6,R13)         Argument value
         LA   R15,CEEDSAHP_ARGLIST   Start of arg area
         STD  FR11,0(R7,R15)         Store the first 8bytes in arg area
         AHI  R6,8                   Bump stack to next parameter
         LD   FR11,0(R6,R13)         Argument value next 8bytes
         STD  FR11,8(R7,R15)         Store the second 8byte in arg area
         AHI  R7,16                  Advance two slots
         LA   R14,4                  We reached max fprs
         B    CONT                   Next parameter

*If we have spare gprs, pass up to 12bytes
*in GPRs.
*TODO: Store left over struct to argument area
*
STRCT    DS   0H
         L    R11,(2112+(((DSASZ+31)/32)*32)+24)(,R4)
         CFI  R11,12
         BL   STRCT2
         B    CONT
STRCT2   DS   0H
         LA   R15,STRCTS
         LR   R11,R0
         SLL  R11,2
         L    R15,0(R11,R15)
         BR   R15

BYTE4    DS   0H
         L    R1,0(R6,R4)
         AHI  R0,1
         B    CONT

BYTE8    DS   0H
         L    R1,0(R6,R4)
         L    R2,4(R6,R4)
         AHI  R0,2
         AHI  R6,4
         B    CONT

BYTE12   DS   0H
         L    R1,0(R6,R4)
         L    R2,4(R6,R4)
         L    R3,8(R6,R4)
         LA   R0,3
         AHI  R6,8
         B    CONT

CONT     DS   0H                     End of processing curr_param
         AHI  R10,4                  Next parameter type
         AHI  R6,4                   Next parameter value stored
         BCT  R9,ARGLOOP

         LMY  R1,R3,CEEDSAHP_ARGLIST
*Get function address, first argument passed,
*and return type, third argument passed from
*caller's argument area.

         SR   R11,R11
         L    R6,(2112+(((DSASZ+31)/32)*32))(,R4)
         L    R11,(2112+(((DSASZ+31)/32)*32)+8)(,R4)

* Load environment ptr and func ptr
         L    R5,16(,R6)
         L    R6,20(,R6)

*What: Call function with call site descriptor based
*      function return type. Then process the return value
         LA   R15,RTABLE
         SLL  R11,2                  Return type
         L    R15,0(R11,R15)
         BR   R15

RF       DS   0H
         BASR R7,R6
         DC   X'4700',Y((CDESCRF-(*-8))/8)

         L    R7,(2112+(((DSASZ+31)/32)*32)+12)(,R4)
         STE  0,0(,R7)
         B    RET

RD       DS   0H
         BASR R7,R6
         DC   X'4700',Y((CDESCRD-(*-8))/8)

         L    R7,(2112+(((DSASZ+31)/32)*32)+12)(,R4)
         STD  0,0(,R7)
         B    RET

RLD      DS   0H
         BASR 7,6
         DC   X'4700',Y((CDESCRLD-(*-8))/8)
         L    R7,(2112+(((DSASZ+31)/32)*32)+12)(,R4)
         STD  0,0(,R7)
         STD  2,8(,R7)
         B    RET

RI       DS   0H
         BASR R7,R6
         DC   X'4700',Y((CDESCRI-(*-8))/8)
         L    R7,(2112+(((DSASZ+31)/32)*32)+12)(,R4)
         ST   3,0(,R7)
         B    RET

RLL      DS   0H
         BASR R7,R6
         DC   X'4700',Y((CDESCRLL-(*-8))/8)
         L    R7,(2112+(((DSASZ+31)/32)*32)+12)(,R4)
         STM  2,3,0(R7)
         B    RET

* TODO: Struct return actually needs a different call descriptor
RV       DS   0H
RS       DS   0H
         BASR R7,R6
         DC   X'4700',Y((CDESCRV-(*-8))/8)
         B    RET


RET      DS   0H
         EDCXEPLG

ATABLE   DC   A(I)                   Labels for parm types
         DC   A(D)
         DC   A(LD)
         DC   A(UI8)
         DC   A(SI8)
         DC   A(UI16)
         DC   A(SI16)
         DC   A(UI32)
         DC   A(SI64)
         DC   A(PTR)
         DC   A(UI64)
         DC   A(FLT)
         DC   A(STRCT)
         DC   A(0)
         DC   A(I)
         DC   A(D)
FLD      DC   A(FLDR0)               Labels to store DOUBLE in fpr
         DC   A(FLDR2)
         DC   A(FLDR4)
         DC   A(FLDR6)
         DC   A(ARGAFL)
LDD      DC   A(DFPR0)               Labels to store l_DOUBLE in fpr
         DC   A(DFPR4)
         DC   A(DARGF)
FLTS     DC   A(FLTR0)               Labels to store FLOAT in fpr
         DC   A(FLTR2)
         DC   A(FLTR4)
         DC   A(FLTR6)
         DC   A(ARGAFT)
STRCTS   DC   A(BYTE4)
         DC   A(BYTE8)
         DC   A(BYTE12)
RTABLE   DC   A(RV)                  Labels for arg returns
         DC   A(RS)
         DC   A(RF)
         DC   A(RD)
         DC   A(RLD)
         DC   A(RI)
         DC   A(RLL)


* Begin Call Descriptors
CDESCRV  DS   0D                     Call decriptor for void rtrn
         DC   A(FFISYS#C-*)
         DC   BL.3'000',BL.5'00000'
         DC   BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRF  DS   0D                     Call decriptor for float rtrn
         DC   A(FFISYS#C-*)
         DC   BL.3'000',BL.5'01000'
         DC   BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRD  DS   0D                     Call decriptor for dbl rtrn
         DC   A(FFISYS#C-*)
         DC   BL.3'000',BL.5'01001'
         DC   BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRLD DS   0D                     Call decriptor for long dbl rtrn
         DC   A(FFISYS#C-*)
         DC   BL.3'000',BL.5'01010'
         DC   BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRI  DS   0D                     Call decriptor for int rtrn
         DC   A(FFISYS#C-*)
         DC   BL.3'000',BL.5'00001'
         DC   BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
CDESCRLL DS   0D                     Call decriptor for long long rtrn
         DC   A(FFISYS#C-*)
         DC   BL.3'000',BL.5'00010'
         DC   BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'


R0       EQU  0,,,,GR
R1       EQU  1,,,,GR
R2       EQU  2,,,,GR
R3       EQU  3,,,,GR
R4       EQU  4,,,,GR
R5       EQU  5,,,,GR
R6       EQU  6,,,,GR
R7       EQU  7,,,,GR
R8       EQU  8,,,,GR
R9       EQU  9,,,,GR
R10      EQU  10,,,,GR
R11      EQU  11,,,,GR
R12      EQU  12,,,,GR
R13      EQU  13,,,,GR
R14      EQU  14,,,,GR
R15      EQU  15,,,,GR

FR0      EQU  0,,,,FPR
FR1      EQU  1,,,,FPR
FR2      EQU  2,,,,FPR
FR3      EQU  3,,,,FPR
FR4      EQU  4,,,,FPR
FR5      EQU  5,,,,FPR
FR6      EQU  6,,,,FPR
FR7      EQU  7,,,,FPR
FR8      EQU  8,,,,FPR
FR9      EQU  9,,,,FPR
FR10     EQU  10,,,,FPR
FR11     EQU  11,,,,FPR
FR12     EQU  12,,,,FPR
FR13     EQU  13,,,,FPR
FR14     EQU  14,,,,FPR
FR15     EQU  15,,,,FPR

RETVOID  EQU  X'0'
RETSTRT  EQU  X'1'
RETFLOT  EQU  X'2'
RETDBLE  EQU  X'3'
RETINT3  EQU  X'4'
RETINT6  EQU  X'5'


CEEDSAHP CEEDSA SECTYPE=XPLINK
ARGSL    DS   XL800
LSTOR    DS   XL800
DSASZ    EQU  (*-CEEDSAHP_FIXED)
         END  FFISYS
